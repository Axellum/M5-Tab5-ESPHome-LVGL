/**
 * [AI-CONTEXT]
 * @file tab5_registry.cpp
 * @role Implémentation des deux registres (voir tab5_registry.h). C'est ICI, et
 *       nulle part ailleurs, que la liste des consoles est écrite.
 * @ai_instruction Une nouvelle console = une ligne dans `kGames`. Garder
 *       « Neon Apron » en dernier (ADR-0012).
 */
#include "tab5_registry.h"
#include "tab5_custom.h"   // close_popup_if_open()
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cstring>

#include "marble_game.h"
#include "arkanoid_game.h"
#include "lode_game.h"
#include "go_game.h"
#include "trivia_game.h"
#include "draughts_game.h"
#include "chess_game.h"
#include "pinball_game.h"

static const char* const TAG = "TAB5";

// =============================================================================
// Consoles arcade
// =============================================================================
namespace GameRegistry {

// Ordre = ordre de fermeture ET ordre de priorité du libellé HA.
// `imu_fast` : « Roi Noir », « Dames Tab » et « Go Tab » n'utilisent l'IMU que
// pour détecter une secousse franche (demande d'indice), fiable à 10 Hz —
// inutile de payer 30 Hz pendant une partie qui peut durer une demi-heure.
// « Neon Apron » est en cadence rapide : un nudge est une secousse de ~150 ms,
// à 10 Hz on n'en verrait qu'un échantillon sur deux.
static const Entry kGames[] = {
    {"Fil d'Or",        Marble::is_open,   Marble::close,   Marble::on_imu,   true},
    {"Arcanoïde",       Arkanoid::is_open, Arkanoid::close, Arkanoid::on_imu, true},
    {"Coureur d'Or",    Lode::is_open,     Lode::close,     Lode::on_imu,     true},
    {"Go Tab",          Go::is_open,       Go::close,       Go::on_imu,       false},
    {"Trial Poursuite", Trivia::is_open,   Trivia::close,   Trivia::on_imu,   true},
    {"Dames Tab",       Draughts::is_open, Draughts::close, Draughts::on_imu, false},
    {"Roi Noir",        Chess::is_open,    Chess::close,    Chess::on_imu,    false},
    // EN DERNIER : sa fermeture restaure la rotation 270 du dashboard. Si un
    // jour une autre console touche à l'orientation, elle devra fermer après,
    // pour que le dernier mot revienne au paysage (ADR-0012).
    {"Neon Apron",      Pinball::is_open,  Pinball::close,  Pinball::on_imu,  true},
};
static constexpr int kNbGames = sizeof(kGames) / sizeof(kGames[0]);

int count() { return kNbGames; }
const Entry& at(int i) { return kGames[i]; }

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
// Fenêtres modales
// =============================================================================
namespace ModalRegistry {

struct Slot {
    lv_obj_t* obj;
    const char* name;
    Kind kind;
};

static Slot g_slots[MAX];
static int g_nb = 0;

bool ready() { return g_nb > 0; }
int count() { return g_nb; }

void add(lv_obj_t* obj, const char* name, Kind kind) {
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
    g_slots[g_nb++] = Slot{obj, name, kind};
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
