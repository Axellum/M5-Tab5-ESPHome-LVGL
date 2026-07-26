/**
 * [AI-CONTEXT]
 * @file marble_game.h
 * @role Jeu « Fil d'Or » — roguelite de bille pilote a l'inclinaison (BMI270).
 * @architecture_constraint Flux PLEIN ECRAN 1280x720, exception assumee au chrome
 *      modal v4 (ADR-0009) : pas de carte 1250x690 ni de barre de titre de 52 px,
 *      le playfield doit dominer. Le YAML (ui_components/marble_game.yaml) ne
 *      declare QUE 4 conteneurs vides ; tout le contenu (HUD, menus, entites) est
 *      construit en C++ ici. Aucune dependance Home Assistant : la meta-progression
 *      est persistee en NVS via esphome::global_preferences.
 * @ai_instruction Ne PAS remettre de logique de jeu dans le YAML. Ne PAS appeler
 *      Marble::tick() manuellement : il est pilote par un lv_timer cree a l'ouverture
 *      et detruit a la fermeture (zero tick gameplay quand le jeu est ferme).
 */
#pragma once
#include "esphome.h"
#include "tab5_custom.h"

namespace esphome { namespace font { class Font; } }

// ---------------------------------------------------------------------------
// Sauvegarde meta inter-sessions (NVS).
// DOIT rester trivially copyable : ESPPreferences fait un memcpy de sizeof(T).
// Toute modification de ce layout doit s'accompagner d'un bump de MARBLE_SAVE_MAGIC
// (une sauvegarde au mauvais format est alors rejetee et repart a zero).
// ---------------------------------------------------------------------------
// Nombre de caracteristiques ameliorables (voir STATS[] dans marble_game.cpp).
#define MARBLE_NSTATS 6
// Emplacements d'objets equipes simultanement (facon anneaux de Dark Souls).
#define MARBLE_NSLOTS 2

struct MarbleSave {
    uint32_t magic;      // MARBLE_SAVE_MAGIC — sinon reset usine
    uint32_t souls;      // ames : monnaie unique (montee de niveau + marchand)
    uint32_t runs;       // nombre de runs lancees
    uint32_t wins;       // nombre de victoires (salle 6 nettoyee)
    uint32_t best_ms;    // meilleur temps de victoire en ms (0 = aucune)
    uint32_t items;      // masque de bits des objets possedes (32 max)
    uint8_t  deepest;    // salle la plus profonde atteinte (1..6)
    uint8_t  st[MARBLE_NSTATS];      // niveau de chaque caracteristique
    uint8_t  equip[MARBLE_NSLOTS];   // 0 = vide, sinon (index objet + 1)
    uint8_t  skin;       // teinte de la bille            (0..2)
    uint8_t  difficulty; // 0 = Calme, 1 = Normal, 2 = Impitoyable
    uint8_t  god;        // 1 = mode dieu (invulnerable, hors classement)
    int16_t  cal_x;      // offset de calibration, en milli-g
    int16_t  cal_y;
};

namespace Marble {

// Pointeurs LVGL + polices fournis par le YAML au moment de l'ouverture.
// Les 4 conteneurs sont declares dans ui_components/marble_game.yaml ; les
// polices viennent de tab5-styles.yaml (on ne peut pas faire `id(...)` hors lambda).
struct UI {
    lv_obj_t* root  = nullptr;  // overlay plein ecran 1280x720
    lv_obj_t* field = nullptr;  // aire de jeu 1280x672 (sous le HUD)
    lv_obj_t* hud   = nullptr;  // bandeau compact 1280x48
    lv_obj_t* panel = nullptr;  // calque menus (hub / recompense / pause / fin)
    const esphome::font::Font* f_small = nullptr;  // roboto_22
    const esphome::font::Font* f_mid   = nullptr;  // roboto_32_b
    const esphome::font::Font* f_big   = nullptr;  // roboto_45_b
};

// Ouvre le jeu sur le hub (construit l'UI au premier appel, la reutilise ensuite)
// et demarre le lv_timer de gameplay. Idempotent.
void open(const UI& ui);

// Ferme le jeu : arrete le timer, banque la run en cours si besoin, sauvegarde
// en NVS et masque l'overlay. Idempotent (sans effet si deja ferme).
void close();

// True tant que l'overlay est visible (utilise pour router les evenements).
bool is_open();

// Alimente le filtre d'inclinaison. Appele par les capteurs BMI270
// (tab5-imu.yaml) — ne fait que stocker, aucun calcul lourd ici.
void on_imu(float ax, float ay, float az);

// Prend l'inclinaison courante comme reference « tablette a plat ».
// Accessible depuis le hub et l'ecran de pause.
void calibrate();

// Ecrit immediatement la sauvegarde meta en NVS (appele aux moments cles).
void persist_save();

// Recharge la sauvegarde meta depuis la NVS (appele au premier open()).
void persist_load();

}  // namespace Marble
