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

// Palette LOCALE du jeu (ex-`UIColor::MARBLE_*` de tab5_custom.h, deplacee ici le
// 08/09/2026, lot (f) de l'audit : meme convention que Lode::Pal, Go::Pal, Chess::Pal,
// Trivia::Pal et Draughts::Pal — un sous-module de jeu ne touche pas aux fichiers
// partages du HMI, ADR-0014). Les tokens « miroir » de tab5-styles.yaml (fond, sol,
// HUD) restent alignes a la main : verifier les deux quand une valeur change.
namespace Pal {
// Palette dediee laiton/sarcelle : volontairement distincte du dashboard
// (et du cliche « purple glow ») pour que le jeu se lise comme un autre monde.
// Utilisee uniquement par le namespace Marble — ne pas melanger avec le HMI.
static constexpr uint32_t VOID     = 0x080C14;  // fond hors terrain
static constexpr uint32_t FLOOR    = 0x111A28;  // sol jouable
static constexpr uint32_t WALL     = 0x3B4A63;  // murs / obstacles
static constexpr uint32_t WALL_LIT = 0x63789B;  // arete eclairee des murs
static constexpr uint32_t HUD_BG   = 0x0C1220;  // bandeau HUD
static constexpr uint32_t BALL     = 0xE8B44A;  // bille — skin 0 (or)
static constexpr uint32_t BALL_ALT = 0xD9E4F5;  // bille — skin 1 (argent)
static constexpr uint32_t BALL_CU  = 0xE2725B;  // bille — skin 2 (cuivre)
static constexpr uint32_t EXIT     = 0x2BB3A3;  // portail de sortie actif
static constexpr uint32_t EXIT_OFF = 0x1E4A47;  // portail verrouille (runes manquantes)
static constexpr uint32_t DANGER   = 0xE05252;  // pieges mortels (spikes, scies, orbes)
static constexpr uint32_t PIT      = 0x03060C;  // trou / vide
static constexpr uint32_t SLOW     = 0x7C5CBF;  // glu / zone lente
static constexpr uint32_t BOOST    = 0xF2853F;  // zone d'acceleration
static constexpr uint32_t WIND     = 0x4E88C7;  // courant lateral
static constexpr uint32_t SHIELD   = 0x5AD1E8;  // pickup bouclier
static constexpr uint32_t MAGNET   = 0xB68CE8;  // pickup aimant
static constexpr uint32_t BRAKE    = 0x8FBF6A;  // pickup frein
static constexpr uint32_t DASH     = 0xF2C14E;  // pickup dash
static constexpr uint32_t RUNE     = 0xF7E08A;  // rune / cle d'objectif
static constexpr uint32_t BRASS_CHEST = 0x9A6B2F;  // coffre au tresor (laiton)
// --- Paires de degrade : c'est d'ICI que vient le volume ---------------------
// [AI-CONTEXT] Meme recette que la table du flipper (Pinball::Pal) : chaque piece a
// un ton HAUT (face eclairee, vers le haut de l'ecran) et un ton BAS (face a
// l'ombre). Un `set_grad(obj, HI, LO)` remplace un aplat et ne coute AUCUN
// objet LVGL supplementaire — une seule passe de dessin.
// @ai_instruction N'ajoute pas une couleur seule : ajoute une paire, sinon la
//      piece redeviendra plate au milieu des autres.
static constexpr uint32_t FLOOR_HI = 0x18243A;  // sol, haut du degrade
static constexpr uint32_t FLOOR_LO = 0x090E18;  // sol, bas du degrade
static constexpr uint32_t SLAB     = 0x16202F;  // dalles peintes au sol (decor)
static constexpr uint32_t WALL_HI  = 0x4C5E7C;  // pierre, face eclairee
static constexpr uint32_t WALL_LO  = 0x202A3B;  // pierre, face a l'ombre
static constexpr uint32_t WALL_EDGE= 0x92A7CA;  // arete vive au sommet du mur
static constexpr uint32_t DANGER_HI= 0xFF8A7A;  // pointe / lame, arete eclairee
static constexpr uint32_t DANGER_LO= 0x71171C;  // pointe / lame, base sombre
static constexpr uint32_t BALL_SH  = 0x03060C;  // ombre portee de la bille
static constexpr uint32_t BALL_HI  = 0xFFF0CE;  // reflet speculaire — skin or
static constexpr uint32_t BALL_ALT_HI = 0xFFFFFF;  // reflet — skin argent
static constexpr uint32_t BALL_CU_HI  = 0xFFD3C2;  // reflet — skin cuivre
static constexpr uint32_t PIT_RIM  = 0x2C3648;  // margelle du trou (rebord eclaire)
static constexpr uint32_t EXIT_HI  = 0x6FF0DC;  // coeur du portail ouvert
static constexpr uint32_t RUNE_LO  = 0xA8863A;  // or / rune, bas du degrade
static constexpr uint32_t CHEST_LO = 0x543813;  // coffre, bas du degrade
static constexpr uint32_t EMBER    = 0xD8873A;  // braise des torches (decor)
}  // namespace Pal

// Pointeurs LVGL + polices fournis par le YAML au moment de l'ouverture.
// Les 4 conteneurs sont declares dans ui_components/marble_game.yaml ; les
// polices viennent de tab5-styles.yaml (on ne peut pas faire `id(...)` hors lambda).
struct UI {
    lv_obj_t* root  = nullptr;  // page LVGL plein ecran 1280x720
    lv_obj_t* field = nullptr;  // aire de jeu 1280x672 (sous le HUD)
    lv_obj_t* hud   = nullptr;  // bandeau compact 1280x48
    lv_obj_t* panel = nullptr;  // calque menus (hub / recompense / pause / fin)
    esphome::lvgl::LvglComponent* lvgl = nullptr;  // pour navigation pages
    size_t home_idx = 0;        // index de la page de retour (page_arcade = 1)
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
