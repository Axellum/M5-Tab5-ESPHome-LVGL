/**
 * [AI-CONTEXT]
 * @file tab5_registry.cpp
 * @role Implémentation des deux registres (voir tab5_registry.h). La liste des
 *       consoles n'est pas ici : elle est dans le module des jeux
 *       (Tab5/jeux/jeux_registre.cpp, `kGames`), optionnel (ADR-0065).
 * @ai_instruction Une nouvelle console = une ligne dans `kGames`
 *       (jeux_registre.cpp). Garder « Neon Apron » en dernier (ADR-0012).
 */
#include "tab5_registry.h"
#include "tab5_internal.h"   // close_popup_if_open()
#include "tab5_custom.h"   // close_popup_if_open()
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cmath>
#include <cstring>

static const char* const TAG = "tab5.registry";

// =============================================================================
// Consoles arcade
// =============================================================================
namespace GameRegistry {

// La table (kJeux, kNbJeux) est celle du module des jeux, ou vide sans lui
// (tab5_registry.h). Ordre = ordre de fermeture ET ordre de priorité du libellé HA.

bool disponibles() { return kNbJeux > 0; }

bool any_open() {
    for (int i = 0; i < kNbJeux; i++)
        if (kJeux[i].is_open()) return true;
    return false;
}

const char* open_name() {
    for (int i = 0; i < kNbJeux; i++)
        if (kJeux[i].is_open()) return kJeux[i].name;
    return nullptr;
}

void close_all() {
    for (int i = 0; i < kNbJeux; i++)
        if (kJeux[i].is_open()) kJeux[i].close();
}

bool any_imu_fast_open() {
    for (int i = 0; i < kNbJeux; i++)
        if (kJeux[i].imu_fast && kJeux[i].is_open()) return true;
    return false;
}

void dispatch_imu(float ax, float ay, float az) {
    for (int i = 0; i < kNbJeux; i++) kJeux[i].on_imu(ax, ay, az);
}

}  // namespace GameRegistry

// =============================================================================
// IMU hors jeux (tab5-imu.yaml ; 08/10/2026, audit YML-7 : avant, deux `static` de
// lambda YAML)
// =============================================================================
namespace {
uint32_t s_derniere_tape_ms = 0;  // anti-rebond du tap-to-wake
uint32_t s_cadence_imu_ms = 100;  // update_interval du composant (tab5-imu.yaml)
}  // namespace

bool imu_tape_franche(float ax, float ay, float az, uint32_t maintenant_ms) {
    // Au repos la norme vaut ~1 g ; une tape franche sur la dalle produit un pic
    // > 2,5 g. Anti-rebond de 500 ms.
    const float norme = sqrtf(ax * ax + ay * ay + az * az);
    if (norme <= 2.5f || (maintenant_ms - s_derniere_tape_ms) <= 500) return false;
    s_derniere_tape_ms = maintenant_ms;
    return true;
}

int32_t imu_cadence_a_changer(bool ecoute_tape) {
    // Quelles consoles justifient 30 Hz : le drapeau `imu_fast` de kGames (jeux_registre.cpp).
    // « Roi Noir », « Dames » et « Go Tab » l'ont à false : ils n'utilisent l'IMU que
    // pour une secousse franche (demande d'indice), fiable à 10 Hz — inutile de payer
    // 30 Hz pendant une partie qui peut durer une demi-heure. « Neon Apron » l'a à
    // true : un nudge est une secousse de ~150 ms, à 10 Hz on n'en verrait qu'un
    // échantillon sur deux et le passe-haut raterait la moitié des coups de hanche.
    // Écran éteint : on écoute le tap-to-wake (s'il est activé). Un jeu ouvert
    // (« Dames », « Go », « Trial Poursuite ») lit une secousse à 10 Hz.
    uint32_t voulue = 0;  // 0 = pas de lecture (écran allumé, aucun jeu)
    if (GameRegistry::any_imu_fast_open()) voulue = 33;
    else if (GameRegistry::any_open()) voulue = 100;
    else if (ecoute_tape) voulue = 100;
    if (voulue == s_cadence_imu_ms) return -1;
    s_cadence_imu_ms = voulue;
    return static_cast<int32_t>(voulue);
}

// =============================================================================
// Fenêtres modales
// =============================================================================
namespace ModalRegistry {

struct Slot {
    lv_obj_t* obj;
    const char* name;
    Kind kind;
    Ouvreur ouvreur;
    bool garee;  // cachée et décalée de kGarageY (voir GARAGE, tab5_registry.h)
};

static Slot g_slots[MAX];
static int g_nb = 0;

// Assez loin pour qu'aucune fenêtre garée, ombre comprise, ne touche l'écran, quelles
// que soient la carte et l'orientation.
constexpr int32_t kGarageY = 10000;

// Coordonnées justes tout de suite (lv_obj_refr_pos : la racine et ses enfants déplacés,
// la nouvelle zone invalidée), pas à la prochaine mise en page : le code qui lit la carte
// d'un popup à LV_EVENT_REFR_START, avant elle, la trouve déjà à sa place.
static void garer(Slot& s, bool garee) {
    if (garee)
        lv_obj_set_style_translate_y(s.obj, kGarageY, LV_PART_MAIN);
    else
        lv_obj_remove_local_style_prop(s.obj, LV_STYLE_TRANSLATE_Y, LV_PART_MAIN);
    lv_obj_refr_pos(s.obj);
    s.garee = garee;
}

// À chaque rafraîchissement, AVANT la mise en page et le dessin (lv_refr.c) : gare ce
// qui vient d'être caché, remet ce qui vient d'être montré, quel que soit le chemin.
// Les SUBWINDOW jamais : roue_ouvrir() place la roue d'après les coordonnées de son voile
// (roue_actions) pendant qu'il est encore caché (tab5_roue.cpp), et les autres vivent
// dans un popup déjà garé quand il est fermé.
static void garage_rafraichir(lv_event_t*) {
    for (int i = 0; i < g_nb; i++) {
        Slot& s = g_slots[i];
        if (s.obj == nullptr || s.kind == SUBWINDOW) continue;
        const bool cachee = lv_obj_has_flag(s.obj, LV_OBJ_FLAG_HIDDEN);
        if (cachee != s.garee) garer(s, cachee);
    }
}

void degarer(lv_obj_t* obj) {
    if (obj == nullptr) return;
    for (int i = 0; i < g_nb; i++) {
        Slot& s = g_slots[i];
        if (s.obj == obj && s.garee) garer(s, false);
    }
}

bool ready() { return g_nb > 0; }

void add(lv_obj_t* obj, const char* name, Kind kind, Ouvreur ouvreur) {
    if (g_nb == 0) {
        lv_display_t* d = lv_display_get_default();
        if (d != nullptr) lv_display_add_event_cb(d, garage_rafraichir, LV_EVENT_REFR_START, nullptr);
    }
    if (g_nb >= MAX) {
        ESP_LOGE(TAG, "ModalRegistry plein (%d) : '%s' ignore", MAX, name ? name : "?");
        return;
    }
    if (obj == nullptr) {
        // Un id() non résolu ne compile pas ; un pointeur nul ici serait donc
        // un widget pas encore construit — on garde la ligne, les lecteurs
        // testent le pointeur.
        ESP_LOGW(TAG, "ModalRegistry : '%s' enregistre sans widget", name ? name : "?");
    }
    g_slots[g_nb++] = Slot{obj, name, kind, ouvreur, false};
}

bool ouvrir(lv_obj_t* obj) {
    if (obj == nullptr) return false;
    for (int i = 0; i < g_nb; i++) {
        const Slot& s = g_slots[i];
        if (s.obj != obj) continue;
        if (s.ouvreur == nullptr) return false;
        s.ouvreur();
        return true;
    }
    return false;
}

static inline bool visible(const Slot& s) {
    return s.obj != nullptr && !lv_obj_has_flag(s.obj, LV_OBJ_FLAG_HIDDEN);
}

const char* visible_name() {
    for (int i = 0; i < g_nb; i++) {
        const Slot& s = g_slots[i];
        if (s.kind == SUBWINDOW || s.name == nullptr) continue;
        if (visible(s)) return s.name;
    }
    return nullptr;
}

bool any_popup_visible() {
    for (int i = 0; i < g_nb; i++) {
        const Slot& s = g_slots[i];
        if (s.kind == POPUP && visible(s)) return true;
    }
    return false;
}

lv_obj_t* find(const char* name) {
    if (name == nullptr) return nullptr;
    for (int i = 0; i < g_nb; i++) {
        const Slot& s = g_slots[i];
        if (s.kind == SUBWINDOW || s.name == nullptr) continue;
        if (std::strcmp(s.name, name) == 0) return s.obj;
    }
    return nullptr;
}

void close_all() {
    // Sous-fenêtres d'abord : masquage sec, elles n'ont pas de fondu propre et
    // disparaissent avec la carte qui les porte (mêmes gestes que les croix de
    // calendar_popup.yaml / console_sys.yaml).
    for (int i = 0; i < g_nb; i++) {
        const Slot& s = g_slots[i];
        if (s.kind == SUBWINDOW && s.obj != nullptr) lv_obj_add_flag(s.obj, LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < g_nb; i++) {
        const Slot& s = g_slots[i];
        if (s.kind == POPUP) close_popup_if_open(s.obj);
    }
}

}  // namespace ModalRegistry
