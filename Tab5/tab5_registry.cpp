/**
 * [AI-CONTEXT]
 * @file tab5_registry.cpp
 * @role Implémentation des deux registres (voir tab5_registry.h). C'est ICI, et
 *       nulle part ailleurs, que la liste des consoles est écrite.
 * @ai_instruction Une nouvelle console = une ligne dans `kGames`. Garder
 *       « Neon Apron » en dernier (ADR-0012).
 */
#include "tab5_registry.h"
#include "tab5_internal.h"   // close_popup_if_open()
#include "tab5_custom.h"   // close_popup_if_open()
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cmath>
#include <cstring>

#include "marble_game.h"
#include "arkanoid_game.h"
#include "lode_game.h"
#include "go_game.h"
#include "trivia_game.h"
#include "draughts_game.h"
#include "chess_game.h"
#include "pinball_game.h"

static const char* const TAG = "tab5.registry";

// =============================================================================
// Consoles arcade
// =============================================================================
namespace GameRegistry {

// Ordre = ordre de fermeture ET ordre de priorité du libellé HA.
// `imu_fast` : « Roi Noir », « Dames Tab », « Go Tab » et « Trial Poursuite »
// n'utilisent l'IMU que pour détecter une secousse franche (indice, lancer du dé ;
// Trial Poursuite passé à false le 26/09/2026, audit lot 3), fiable à 10 Hz —
// inutile de payer 30 Hz pendant une partie qui peut durer une demi-heure.
// « Neon Apron » est en cadence rapide : un nudge est une secousse de ~150 ms,
// à 10 Hz on n'en verrait qu'un échantillon sur deux.
static const Entry kGames[] = {
    {"Fil d'Or",        Marble::is_open,   Marble::close,   Marble::on_imu,   true},
    {"Arcanoïde",       Arkanoid::is_open, Arkanoid::close, Arkanoid::on_imu, true},
    {"Coureur d'Or",    Lode::is_open,     Lode::close,     Lode::on_imu,     true},
    {"Go Tab",          Go::is_open,       Go::close,       Go::on_imu,       false},
    {"Trial Poursuite", Trivia::is_open,   Trivia::close,   Trivia::on_imu,   false},
    {"Dames Tab",       Draughts::is_open, Draughts::close, Draughts::on_imu, false},
    {"Roi Noir",        Chess::is_open,    Chess::close,    Chess::on_imu,    false},
    // EN DERNIER : sa fermeture restaure la rotation 270 du dashboard. Si un
    // jour une autre console touche à l'orientation, elle devra fermer après,
    // pour que le dernier mot revienne au paysage (ADR-0012).
    {"Neon Apron",      Pinball::is_open,  Pinball::close,  Pinball::on_imu,  true},
};
static constexpr int kNbGames = sizeof(kGames) / sizeof(kGames[0]);

bool any_open() {
    for (const auto& g : kGames) if (g.is_open()) return true;
    return false;
}

const char* open_name() {
    for (const auto& g : kGames) if (g.is_open()) return g.name;
    return nullptr;
}

void close_all() {
    for (const auto& g : kGames) if (g.is_open()) g.close();
}

bool any_imu_fast_open() {
    for (const auto& g : kGames) if (g.imu_fast && g.is_open()) return true;
    return false;
}

void dispatch_imu(float ax, float ay, float az) {
    for (const auto& g : kGames) g.on_imu(ax, ay, az);
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
    // Quelles consoles justifient 30 Hz : le drapeau `imu_fast` de kGames (plus haut).
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
};

static Slot g_slots[MAX];
static int g_nb = 0;

bool ready() { return g_nb > 0; }

void add(lv_obj_t* obj, const char* name, Kind kind, Ouvreur ouvreur) {
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
    g_slots[g_nb++] = Slot{obj, name, kind, ouvreur};
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
