/**
 * [AI-CONTEXT]
 * @file lode_game.h
 * @role Jeu « Coureur d'Or » — clone de Lode Runner (Broderbund, 1983) pour le Tab5.
 * @architecture_constraint Flux PLEIN ECRAN 1280x720, exception assumee au chrome
 *      modal v4 (ADR-0009) : pas de modal_scrim / modal_header / carte 1250x690 /
 *      croix — la sortie se fait par le hub (« Quitter »). Le YAML
 *      (ui_components/lode_game.yaml) ne declare QUE 5 conteneurs vides ; tout le
 *      contenu (HUD, damier, acteurs, pad tactile, menus) est construit en C++.
 *      Aucune dependance Home Assistant : scores et progression vivent en NVS via
 *      esphome::global_preferences.
 * @ai_instruction Ne PAS remettre de logique de jeu dans le YAML. Ne PAS appeler
 *      le tick manuellement : il est pilote par un lv_timer cree a l'ouverture et
 *      detruit a la fermeture (zero tick gameplay quand le jeu est ferme). Le
 *      module est volontairement AUTONOME (palette comprise) pour ne toucher ni
 *      tab5_custom.h ni tab5-styles.yaml — voir Lode::Pal ci-dessous.
 */
#pragma once
#include "esphome.h"
#include "tab5_custom.h"

namespace esphome { namespace font { class Font; } }

// ---------------------------------------------------------------------------
// Sauvegarde inter-sessions (NVS).
// DOIT rester trivially copyable : ESPPreferences fait un memcpy de sizeof(T).
// Toute modification de ce layout doit s'accompagner d'un bump de LODE_SAVE_MAGIC
// (une sauvegarde au mauvais format est alors rejetee et repart a zero).
// ---------------------------------------------------------------------------
#define LODE_MAX_SCORES 10
#define LODE_N_LEVELS   10
#define LODE_N_SPEEDS    5   // paliers de vitesse de deplacement (cf. Lode::SPEEDS)

// Une entree du classement local. `flags` bit 0 = partie hors concours
// (mode debug/invincible) : elle reste affichee mais ne prend pas la tete.
struct LodeScoreEntry {
    uint32_t score;      // score final
    uint32_t stamp;      // epoch SNTP au moment du score (0 = horloge indisponible)
    uint8_t  level;      // niveau atteint (1..LODE_N_LEVELS)
    uint8_t  ctrl_mode;  // 0 = boutons, 1 = inclinaison, 2 = mixte
    uint8_t  flags;      // bit 0 = hors classement
    uint8_t  speed;      // palier de vitesse joue (0..LODE_N_SPEEDS-1) : le bonus de
                         // temps depend du rythme, on le trace pour comparer
};

struct LodeSave {
    uint32_t       magic;                      // LODE_SAVE_MAGIC — sinon reset usine
    LodeScoreEntry scores[LODE_MAX_SCORES];    // top 10, trie decroissant
    uint32_t       best;                       // meilleur score (miroir de scores[0])
    uint32_t       plays;                      // nombre de parties lancees
    uint8_t        score_count;                // entrees valides (0..LODE_MAX_SCORES)
    uint8_t        unlocked;                   // plus haut niveau debloque (1..N)
    uint8_t        ctrl_mode;                  // 0 = boutons, 1 = inclinaison, 2 = mixte
    uint8_t        sensitivity;                // sensibilite d'inclinaison (0..4, defaut 2)
    uint8_t        assist;                     // 1 = mode entrainement (vies infinies, hors concours)
    // Palier de vitesse (0..LODE_N_SPEEDS-1). Occupe l'octet jusqu'ici reserve :
    // le layout est INCHANGE, donc pas de bump de magic et les scores/progressions
    // deja en NVS survivent. Une sauvegarde anterieure y porte 0 = « Normale »,
    // c'est-a-dire exactement le rythme d'avant l'ajout du reglage.
    uint8_t        speed;
    int16_t        cal_x;                      // offset de calibration, en milli-g
    int16_t        cal_y;
};

namespace Lode {

// Palette retro (micro 8-bit / borne d'arcade). Volontairement LOCALE au module :
// tab5_custom.h et tab5-styles.yaml sont des fichiers partages du HMI, on evite de
// les toucher pour un sous-module de jeu. L'esprit de la regle 1 du README (pas de
// couleur en dur dans un YAML ou une lambda) est respecte : ce sont des constantes
// nommees, referencees exclusivement par lode_game.cpp.
namespace Pal {
static constexpr uint32_t VOID_BG    = 0x000000;  // hors damier — noir franc, pas de degrade
static constexpr uint32_t FLOOR_BG   = 0x0A0A12;  // fond du damier
static constexpr uint32_t HUD_BG     = 0x101018;  // bandeau HUD
static constexpr uint32_t BRICK      = 0x9E3B1F;  // brique creusable
static constexpr uint32_t BRICK_HI   = 0xD46A3A;  // mortier / arete eclairee
static constexpr uint32_t SOLID      = 0x5A5A66;  // beton indestructible
static constexpr uint32_t SOLID_HI   = 0x8A8A99;  // arete du beton
static constexpr uint32_t LADDER     = 0x2FD4D4;  // echelle
static constexpr uint32_t BAR        = 0xE8C93A;  // barre de suspension
static constexpr uint32_t GOLD       = 0xFFC61A;  // lingot
static constexpr uint32_t GOLD_HI    = 0xFFF0A0;  // eclat du lingot
static constexpr uint32_t RUNNER     = 0xFF4FD8;  // joueur (magenta borne d'arcade)
static constexpr uint32_t RUNNER_HD  = 0xFFFFFF;  // tete du joueur
static constexpr uint32_t RUNNER_DIG = 0xFFFFFF;  // flash de creusement
static constexpr uint32_t GUARD      = 0xE03030;  // garde
static constexpr uint32_t GUARD_HD   = 0xFFB0B0;  // tete du garde
static constexpr uint32_t GUARD_AU   = 0x3FCF5A;  // garde porteur d'un lingot
static constexpr uint32_t EXIT_LAD   = 0xEFEFEF;  // echelle de sortie (active)
static constexpr uint32_t PAD_FILL   = 0x2A2A3A;  // fond des zones tactiles
static constexpr uint32_t PAD_EDGE   = 0x7A7A9A;  // liseré des zones tactiles
static constexpr uint32_t TXT        = 0xF1F5F9;  // texte principal
static constexpr uint32_t TXT_DIM    = 0x8595AD;  // texte secondaire
static constexpr uint32_t DANGER     = 0xFF3B3B;  // mort / avertissement
}  // namespace Pal

// Pointeurs LVGL + polices fournis par le YAML au moment de l'ouverture.
// Les 5 conteneurs sont declares dans ui_components/lode_game.yaml ; les polices
// viennent de tab5-styles.yaml (on ne peut pas faire `id(...)` hors lambda).
struct UI {
    lv_obj_t* root  = nullptr;  // page LVGL plein ecran 1280x720
    lv_obj_t* field = nullptr;  // damier 1280x672 (sous le HUD)
    lv_obj_t* hud   = nullptr;  // bandeau compact 1280x48
    lv_obj_t* panel = nullptr;  // calque menus (hub / niveaux / classement / pause / fin)
    lv_obj_t* pad   = nullptr;  // calque des zones tactiles (D-pad + creuser)
    esphome::lvgl::LvglComponent* lvgl = nullptr;  // pour navigation pages
    size_t home_idx = 0;        // index de la page de retour (page_arcade = 1)
    const esphome::font::Font* f_small = nullptr;  // roboto_22
    const esphome::font::Font* f_mid   = nullptr;  // roboto_32_b
    const esphome::font::Font* f_big   = nullptr;  // roboto_45_b
    // Horodatage SNTP (epoch UTC) au moment de l'ouverture, 0 si l'horloge n'est
    // pas synchronisee. Sert a dater les scores sans dependre de HA a l'execution.
    uint32_t epoch = 0;
};

// Directions logiques (miroir des zones tactiles et de l'inclinaison).
enum Dir : uint8_t { D_NONE = 0, D_LEFT, D_RIGHT, D_UP, D_DOWN };

// Ouvre le jeu sur le hub (construit l'UI au premier appel, la reutilise ensuite)
// et demarre le lv_timer de gameplay. Idempotent.
void open(const UI& ui);

// Ferme le jeu : arrete le timer, enregistre la partie en cours si besoin,
// sauvegarde en NVS et masque l'overlay. Idempotent (sans effet si deja ferme).
void close();

// True tant que l'overlay est visible (utilise pour router les evenements IMU
// et pour passer le BMI270 en cadence rapide).
bool is_open();

// Alimente le filtre d'inclinaison. Appele par les capteurs BMI270
// (tab5-imu.yaml) — ne fait que stocker, aucun calcul lourd ici.
void on_imu(float ax, float ay, float az);

// Maintien d'une direction (zones tactiles). `pressed` = doigt pose / relache.
void on_dir(uint8_t dir, bool pressed);

// Creusement (tap) : `right` = creuse la brique en bas a droite, sinon bas a gauche.
void on_dig(bool right);

// Prend l'inclinaison courante comme reference « tablette a plat ».
// Accessible depuis le hub (Reglages) et l'ecran de pause.
void calibrate();

// Ecrit immediatement la sauvegarde en NVS (appele aux moments cles).
void persist_save();

// Recharge la sauvegarde depuis la NVS (appele au premier open()).
void persist_load();

}  // namespace Lode
