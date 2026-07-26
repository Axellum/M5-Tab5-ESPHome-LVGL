/**
 * [AI-CONTEXT]
 * @file chess_game.h
 * @role Jeu « Roi Noir » — echiquier complet (regles FIDE) avec IA embarquee.
 * @architecture_constraint Flux PLEIN ECRAN 1280x720, exception assumee au chrome
 *      modal v4 (ADR-0009) : pas de modal_scrim / modal_header / carte 1250x690 /
 *      croix — la sortie se fait par le hub (« Quitter »). Le YAML
 *      (ui_components/chess_game.yaml) ne declare QUE 4 conteneurs vides ; tout le
 *      contenu (plateau, pieces, HUD, panneau, menus) est construit en C++.
 *      Aucune dependance Home Assistant, aucun reseau, aucune table de finale :
 *      100 % local, options / statistiques / partie en cours en NVS.
 * @ai_instruction Le moteur d'echecs vit dans chess_ai.h/.cpp (aucun LVGL). Ici on
 *      ne fait que du rendu et de la machine a etats. Ne PAS appeler ai_step() en
 *      boucle : il est pilote par le lv_timer cree a l'ouverture et detruit a la
 *      fermeture (zero tick quand le jeu est ferme). Le module est AUTONOME
 *      (palette comprise, voir Chess::Pal) : ni tab5_custom.h ni tab5-styles.yaml
 *      ne sont modifies.
 */
#pragma once
#include "esphome.h"
#include "tab5_custom.h"

namespace esphome { namespace font { class Font; } }

// ---------------------------------------------------------------------------
// Sauvegarde inter-sessions (NVS).
// DOIT rester trivially copyable : ESPPreferences fait un memcpy de sizeof(T).
// Toute modification de ce layout doit s'accompagner d'un bump de CHESS_SAVE_MAGIC
// (une sauvegarde au mauvais format est alors rejetee et repart a zero).
// ---------------------------------------------------------------------------
#define CHESS_NLEVELS 5
#define CHESS_FEN_CAP 92

struct ChessSave {
    uint32_t magic;                    // CHESS_SAVE_MAGIC — sinon reset usine
    uint32_t elo;                      // classement local fictif (depart : 1000)
    uint32_t games;                    // parties terminees contre le Tab
    uint32_t longest_plies;            // plus longue partie, en demi-coups
    uint32_t total_ms;                 // temps de jeu cumule
    uint16_t wins[CHESS_NLEVELS];      // victoires par niveau d'IA
    uint16_t losses[CHESS_NLEVELS];    // defaites par niveau
    uint16_t draws[CHESS_NLEVELS];     // nulles par niveau

    // --- Reglages ---------------------------------------------------------
    uint8_t level;        // niveau d'IA (0..CHESS_NLEVELS-1)
    uint8_t mode;         // 0 = joueur vs Tab, 1 = joueur vs joueur, 2 = Tab vs Tab
    uint8_t human_side;   // 0 = l'humain joue les blancs, 1 = les noirs
    uint8_t clock_opt;    // 0 = sans limite, 1 = 5 min, 2 = 10 min, 3 = 15 min + 10 s
    uint8_t gestures;     // 1 = secousse IMU -> indice
    uint8_t rule50;       // 1 = nulle automatique a la regle des 50 coups
    uint8_t show_eval;    // 1 = evaluation approximative affichee au HUD
    uint8_t demo_speed;   // vitesse du mode Tab vs Tab (0 = lent, 1 = normal, 2 = rapide)

    // --- Partie en cours (reprise apres reboot) ---------------------------
    uint8_t  resume_valid;             // 1 = r_fen exploitable
    uint8_t  r_level, r_mode, r_human;
    uint32_t r_clock_w, r_clock_b;     // pendules restantes, en ms
    uint16_t r_plies;                  // demi-coups joues (statistique uniquement)
    char     r_fen[CHESS_FEN_CAP];     // position courante
};

namespace Chess {

// Palette retro « bois / feutre vert ». Volontairement LOCALE au module :
// tab5_custom.h et tab5-styles.yaml sont des fichiers partages du HMI, on evite
// de les toucher pour un sous-module de jeu (meme choix que Lode::Pal).
namespace Pal {
static constexpr uint32_t VOID_BG     = 0x0B0E14;  // hors plateau
static constexpr uint32_t HUD_BG      = 0x121722;  // bandeau HUD
static constexpr uint32_t PANEL_BG    = 0x0F141E;  // panneau lateral
static constexpr uint32_t SQ_LIGHT    = 0xE9E2C8;  // case claire (creme)
static constexpr uint32_t SQ_DARK     = 0x6E8B5A;  // case sombre (vert feutre)
static constexpr uint32_t BOARD_EDGE  = 0x2A3428;  // liseré du damier
static constexpr uint32_t SQ_L_LAST   = 0xE0D98C;  // case claire — dernier coup
static constexpr uint32_t SQ_D_LAST   = 0x8B9C4A;  // case sombre — dernier coup
static constexpr uint32_t SQ_L_SEL    = 0x9BE3E8;  // case claire — selection
static constexpr uint32_t SQ_D_SEL    = 0x4FA9B4;  // case sombre — selection
static constexpr uint32_t SQ_L_CHK    = 0xF0A0A0;  // case claire — roi en echec
static constexpr uint32_t SQ_D_CHK    = 0xB05050;  // case sombre — roi en echec
static constexpr uint32_t SQ_L_HINT   = 0xC9B6E8;  // case claire — indice
static constexpr uint32_t SQ_D_HINT   = 0x8A6FB0;  // case sombre — indice
// Figurines : deux calques seulement (voir l'en-tete de chess_game.cpp).
// PC_EDGE DOIT rester sombre : c'est le contour qui detache une piece blanche
// d'une case creme. Un liseré clair la rendrait invisible.
static constexpr uint32_t PC_W_FILL   = 0xF7F3E3;  // corps des pieces blanches (ivoire)
static constexpr uint32_t PC_B_FILL   = 0x232B36;  // corps des pieces noires (anthracite)
static constexpr uint32_t PC_EDGE     = 0x1B2430;  // contour des pieces blanches
static constexpr uint32_t DOT_MOVE    = 0x3FBFD4;  // pastille de coup legal
static constexpr uint32_t DOT_CAPTURE = 0xE8A33F;  // pastille de capture possible
static constexpr uint32_t TXT         = 0xF1F5F9;  // texte principal
static constexpr uint32_t TXT_DIM     = 0x8595AD;  // texte secondaire
static constexpr uint32_t TXT_MUTED   = 0x4A5568;  // texte desactive
static constexpr uint32_t ACCENT      = 0xE8B44A;  // accent or (titres, boutons)
static constexpr uint32_t GOOD        = 0x4ADE80;  // avantage / victoire
static constexpr uint32_t DANGER      = 0xE05252;  // echec / defaite / abandon
static constexpr uint32_t THINK       = 0x7C9CD6;  // « le Tab reflechit »
static constexpr uint32_t BTN_BG      = 0x1B2331;  // fond de bouton
static constexpr uint32_t BTN_BG_ON   = 0x2A3648;  // fond de bouton presse
}  // namespace Pal

// Pointeurs LVGL + polices fournis par le YAML au moment de l'ouverture.
// Les 4 conteneurs sont declares dans ui_components/chess_game.yaml ; les polices
// viennent de tab5-styles.yaml (on ne peut pas faire `id(...)` hors lambda).
// Le calque des menus, lui, est cree en C++ (enfant de root) : il n'a aucune
// raison d'exister dans le YAML puisqu'il est entierement peuple par le C++.
struct UI {
    lv_obj_t* root  = nullptr;  // overlay plein ecran 1280x720
    lv_obj_t* hud   = nullptr;  // bandeau superieur 1280x40
    lv_obj_t* board = nullptr;  // plateau 672x672 (8 cases de 84 px)
    lv_obj_t* panel = nullptr;  // panneau lateral 552x672 (coups + boutons)
    const esphome::font::Font* f_small = nullptr;  // roboto_22
    const esphome::font::Font* f_mid   = nullptr;  // roboto_32_b
    const esphome::font::Font* f_big   = nullptr;  // roboto_45_b
    // Pieces d'echecs : chess_pieces_80 (sous-ensemble Unicode U+2654-265F,
    // Tab5/ChessPieces.ttf). Voir PC_* dans chess_game.cpp pour la technique de
    // rendu en deux calques (corps plein + contour superpose).
    const esphome::font::Font* f_piece = nullptr;
};

// Ouvre le jeu sur le hub (construit l'UI au premier appel, la reutilise ensuite)
// et demarre le lv_timer. Idempotent.
void open(const UI& ui);

// Ferme le jeu : arrete le timer, sauvegarde la partie en cours (FEN + pendules)
// et les statistiques en NVS, masque l'overlay. Idempotent.
void close();

// True tant que l'overlay est visible (routage des evenements IMU et cadence
// rapide du BMI270).
bool is_open();

// Fait avancer la reflexion du Tab d'UNE tranche (~18 ms de CPU).
// [AI-CONTEXT] Appele par le lv_timer interne : ne PAS l'appeler en boucle
// depuis une lambda ESPHome, ce serait exactement le gel qu'on cherche a eviter.
void ai_step();

// Alimente le detecteur de secousse (BMI270). Appele par tab5-imu.yaml — ne fait
// que du filtrage scalaire, aucun calcul lourd. Une secousse franche = indice.
void on_imu(float ax, float ay, float az);

// Ecrit / recharge la sauvegarde NVS (appelees aux moments cles).
void persist_save();
void persist_load();

// Lance perft(1..depth) sur la position initiale et journalise le resultat.
// Utilitaire de validation du generateur de coups, appelable depuis un lambda
// ESPHome en phase de debug (voir README, section « Echecs »).
void perft_log(int depth);

}  // namespace Chess
