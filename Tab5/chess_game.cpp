/**
 * [AI-CONTEXT]
 * @file chess_game.cpp
 * @role Jeu « Roi Noir » — UI LVGL, machine a etats, persistance NVS.
 * @architecture_constraint Plein ecran 1280x720. Le YAML ne fournit que 4
 *      conteneurs vides + 3 polices ; tout le reste est construit ici. Les objets
 *      LVGL sont PREALLOUES une seule fois (64 cases, 64 pastilles de piece, 28
 *      marqueurs de coup) puis reutilises par show/hide + set_style : aucune
 *      allocation LVGL en cours de partie. Le MODELE (chess_ai.cpp) est totalement
 *      separe du RENDU : cette unite de compilation ne connait des echecs que ce
 *      que chess_ai.h expose.
 * @ai_instruction Hot-path = tick_cb() : pas de std::string, pas de new/delete.
 *      Les libelles ne sont reecrits que quand leur valeur change (set_text_if).
 *      Couleurs : uniquement Chess::Pal::* (jamais d'hex en dur ici).
 *
 *      PIECES : vraies figurines Unicode via la police dediee chess_pieces_80
 *      (Tab5/ChessPieces.ttf, sous-ensemble de 12 glyphes). Rendu en DEUX
 *      CALQUES superposes, comme lichess ou chess.com :
 *        - calque « corps »   = glyphe PLEIN   U+265A..265F, ivoire ou anthracite
 *        - calque « contour » = glyphe CREUX   U+2654..2659, anthracite
 *      Le contour n'est affiche que pour les pieces BLANCHES : sans lui, une
 *      piece ivoire serait invisible sur une case creme. Les 12 glyphes ont des
 *      boites identiques dans cette police (verifie), la superposition est donc
 *      pixel-parfaite. Ne PAS revenir a des lettres : les polices roboto_* du
 *      projet n'embarquent que du Latin-1, mais ChessPieces.ttf est justement la
 *      pour ca.
 */
#include "chess_game.h"
#include "chess_ai.h"
#include "esphome/core/preferences.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace Chess {

// ===========================================================================
// 1. Geometrie
// ===========================================================================

static constexpr int HUD_H    = 40;
static constexpr int CELL     = 84;
static constexpr int BOARD_SZ = CELL * 8;          // 672
static constexpr int BOARD_X  = 20;
static constexpr int BOARD_Y  = 44;
static constexpr int PANEL_X  = 708;
static constexpr int PANEL_Y  = 44;
static constexpr int PANEL_W  = 552;
static constexpr int PANEL_H  = 672;

// Recentrage optique du glyphe dans sa case. La boite d'un label LVGL fait la
// hauteur de LIGNE de la police (94 px a la taille 80), alors que l'encre des
// figurines n'occupe que 72x60 px : centrer la boite laisse l'encre 1 px trop
// haut. Valeur mesuree sur ChessPieces.ttf via les metriques PIL (le meme
// rasteriseur que celui d'ESPHome) — voir README, section « Roi Noir ».
static constexpr int PIECE_DY = 1;
static constexpr int MAX_DOTS = 28;                // max de coups depuis une case (dame)
static constexpr int N_SLOTS  = 8;                 // entrees de menu preallouees
static constexpr int MOVE_ROWS = 14;               // lignes de la liste de coups

static constexpr uint32_t TICK_MS   = 33;
static constexpr uint32_t ANIM_MS   = 150;         // deplacement d'une piece
static constexpr uint32_t HINT_SHOW_MS = 2500;     // duree d'affichage d'un indice
static constexpr uint32_t HINT_CD_MS   = 8000;     // recharge de l'indice
static constexpr uint32_t SHAKE_CD_MS  = 900;      // anti-rebond de la secousse IMU

// "RNR1" — bumpe a chaque changement de layout de ChessSave.
static constexpr uint32_t SAVE_MAGIC = 0x524E5231u;
static constexpr uint32_t PREF_KEY   = 0x43484553u;  // "CHES" — cle NVS dediee

// Cadences du mode demo (Tab vs Tab) : lent / normal / rapide.
static const uint32_t DEMO_DELAY[3] = {1100, 550, 160};

// Pendules proposees. 0 = sans limite. L'increment Fischer n'existe que sur la
// derniere entree (15 min + 10 s par coup).
struct ClockOpt { const char* name; uint32_t base_ms; uint32_t inc_ms; };
static const ClockOpt CLOCKS[4] = {
    {"Sans limite",     0,          0},
    {"Blitz 5 min",     5u * 60000, 0},
    {"Rapide 10 min",  10u * 60000, 0},
    {"15 min + 10 s",  15u * 60000, 10000},
};

static const char* const MODE_NAME[3] = {"Joueur contre Tab", "Joueur contre joueur", "Tab contre Tab"};
static const char* const DEMO_NAME[3] = {"Lente", "Normale", "Rapide"};

// ===========================================================================
// 2. Etats
// ===========================================================================

enum GState : uint8_t {
    ST_OFF = 0, ST_HUB, ST_SETUP, ST_SETTINGS, ST_STATS, ST_CONFIRM,
    ST_PLAY, ST_PROMO, ST_PAUSE, ST_OVER,
};

enum Result : uint8_t { RES_WHITE = 0, RES_BLACK = 1, RES_DRAW = 2 };

// Etats de surbrillance d'une case (l'ordre vaut priorite decroissante).
enum Hl : uint8_t { HL_NONE = 0, HL_LAST, HL_CHECK, HL_SEL, HL_HINT };

static GState g_state = ST_OFF;

// ===========================================================================
// 3. Etat de partie
// ===========================================================================

static Position g_pos;
static Move     g_hist_move[MAX_HIST];
static Undo     g_hist_undo[MAX_HIST];
static uint64_t g_hist_hash[MAX_HIST + 1];
static char     g_hist_san[MAX_HIST][12];
static uint16_t g_hist_num[MAX_HIST];    // numero de coup au moment du demi-coup
static uint8_t  g_hist_side[MAX_HIST];   // trait au moment du demi-coup
static int      g_nply = 0;

static uint8_t  g_mode       = 0;        // 0 = vs Tab, 1 = hotseat, 2 = demo
static uint8_t  g_human      = WHITE;    // couleur de l'humain (mode 0)
static uint8_t  g_level      = 2;
static bool     g_running    = false;    // partie en cours (pendules actives)
static bool     g_flip       = false;    // plateau vu du cote noir
static uint32_t g_clock[2]   = {0, 0};
static uint32_t g_inc_ms     = 0;
static bool     g_clock_on   = false;
static uint32_t g_clock_last = 0;
static uint32_t g_game_t0    = 0;

static uint8_t g_sel_sq    = NO_SQ;      // case selectionnee
static uint8_t g_last_from = NO_SQ;      // dernier coup joue (encadrement)
static uint8_t g_last_to   = NO_SQ;
static uint8_t g_check_sq  = NO_SQ;      // roi en echec

static Move g_all_legal[MAX_MOVES];      // coups legaux de la position courante
static int  g_nall = 0;
static Move g_from_legal[MAX_DOTS];      // coups partant de g_sel_sq
static int  g_nfrom = 0;

static uint8_t g_promo_from = NO_SQ, g_promo_to = NO_SQ;

// Fin de partie
static uint8_t     g_result = RES_DRAW;
static const char* g_reason = "";

// Indice
static uint8_t  g_hint_from = NO_SQ, g_hint_to = NO_SQ;
static uint32_t g_hint_until = 0, g_hint_ready_at = 0;

// IA
static bool     g_ai_think   = false;
static uint32_t g_ai_next_at = 0;        // temporisation avant de lancer la reflexion

// Animation de deplacement
static bool     g_anim_on   = false;
static uint32_t g_anim_t0   = 0;
static int      g_anim_x0 = 0, g_anim_y0 = 0, g_anim_x1 = 0, g_anim_y1 = 0;
static int      g_anim_hide = -1;        // index de case dont la piece est masquee

// IMU
static float    g_shake_mag  = 0.0f;
static uint32_t g_shake_last = 0;

// Ecran de confirmation generique (reutilise pour « effacer les stats »).
static uint8_t g_confirm_kind = 0;

// D'ou l'ecran de reglages a ete ouvert (hub ou pause) — pour y revenir.
static GState g_settings_from = ST_HUB;

// Message transitoire affiche a droite du HUD (refus de nulle, indice en
// recharge...). Prioritaire sur « ECHEC ! » pendant sa duree de vie.
static char     g_msg[48] = "";
static uint32_t g_msg_until = 0;

// ===========================================================================
// 4. Objets LVGL (pool preallouee)
// ===========================================================================

static UI  g_ui;
static bool g_built = false;
static lv_timer_t* g_timer = nullptr;

static lv_obj_t* g_menu = nullptr;       // calque plein ecran des menus (cree en C++)
static lv_obj_t* g_cell[64]  = {};
static lv_obj_t* g_body[64]  = {};       // calque « corps »   (glyphe plein)
static lv_obj_t* g_edge[64]  = {};       // calque « contour » (pieces blanches)
static lv_obj_t* g_dot[MAX_DOTS] = {};
static lv_obj_t* g_coord_f[8] = {};      // lettres de colonne
static lv_obj_t* g_coord_r[8] = {};      // chiffres de rangee
static lv_obj_t* g_anim = nullptr;       // conteneur de la piece en vol
static lv_obj_t* g_anim_body = nullptr;
static lv_obj_t* g_anim_edge = nullptr;

// HUD
static lv_obj_t* g_h_turn = nullptr;
static lv_obj_t* g_h_level = nullptr;
static lv_obj_t* g_h_clock_w = nullptr;
static lv_obj_t* g_h_clock_b = nullptr;
static lv_obj_t* g_h_eval = nullptr;
static lv_obj_t* g_h_status = nullptr;

// Panneau lateral
static lv_obj_t* g_p_title = nullptr;
static lv_obj_t* g_ml_num[MOVE_ROWS] = {};
static lv_obj_t* g_ml_w[MOVE_ROWS]   = {};
static lv_obj_t* g_ml_b[MOVE_ROWS]   = {};
static lv_obj_t* g_think_lbl = nullptr;
static lv_obj_t* g_think_bar = nullptr;
static lv_obj_t* g_think_fill = nullptr;
static lv_obj_t* g_pbtn[3] = {};
static lv_obj_t* g_pbtn_lbl[3] = {};

// Menus
static lv_obj_t* g_m_title = nullptr;
static lv_obj_t* g_m_sub   = nullptr;
static lv_obj_t* g_m_body  = nullptr;
static lv_obj_t* g_m_foot  = nullptr;
static lv_obj_t* g_slot[N_SLOTS]   = {};
static lv_obj_t* g_slot_t[N_SLOTS] = {};
static lv_obj_t* g_slot_d[N_SLOTS] = {};
static lv_obj_t* g_promo_name[4]   = {};   // libelles sous les tuiles de promotion

// Caches de rendu : une case n'est restylee que si son contenu a change.
static uint8_t g_drawn_pc[64];
static uint8_t g_drawn_hl[64];
static int     g_c_eval = 0x7FFFFFFF;
static int     g_c_clock[2] = {-1, -1};

// ===========================================================================
// 5. Persistance NVS
// ===========================================================================

static ChessSave g_save;
static esphome::ESPPreferenceObject g_pref;
static bool g_pref_ready = false;

static void save_defaults() {
    memset(&g_save, 0, sizeof(g_save));
    g_save.magic      = SAVE_MAGIC;
    g_save.elo        = 1000;
    g_save.level      = 2;        // Fou : le niveau « par defaut jouable »
    g_save.mode       = 0;
    g_save.human_side = 0;
    g_save.clock_opt  = 0;
    g_save.gestures   = 1;
    g_save.rule50     = 1;
    g_save.show_eval  = 1;
    g_save.demo_speed = 1;
}

void persist_load() {
    if (!g_pref_ready) {
        g_pref = esphome::global_preferences->make_preference<ChessSave>(PREF_KEY);
        g_pref_ready = true;
    }
    if (!g_pref.load(&g_save) || g_save.magic != SAVE_MAGIC) save_defaults();
    if (g_save.level >= CHESS_NLEVELS) g_save.level = 2;
    if (g_save.mode > 2) g_save.mode = 0;
    if (g_save.clock_opt > 3) g_save.clock_opt = 0;
    if (g_save.demo_speed > 2) g_save.demo_speed = 1;
    if (g_save.elo < 100 || g_save.elo > 3000) g_save.elo = 1000;
}

void persist_save() {
    if (!g_pref_ready) return;
    g_save.magic = SAVE_MAGIC;
    g_pref.save(&g_save);
    esphome::global_preferences->sync();
}

// ===========================================================================
// 6. Helpers LVGL
// ===========================================================================

static lv_obj_t* mk_rect(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    return o;
}

static lv_obj_t* mk_label(lv_obj_t* parent, const esphome::font::Font* f, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_remove_style_all(l);
    if (f) esphome::lvgl::lv_obj_set_style_text_font(l, f, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_label_set_text(l, "");
    return l;
}

static inline void show(lv_obj_t* o, bool v) {
    if (!o) return;
    if (v) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else   lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static inline void set_bg(lv_obj_t* o, uint32_t c, lv_opa_t opa) {
    lv_obj_set_style_bg_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, opa, LV_PART_MAIN);
}

static inline void set_border(lv_obj_t* o, uint32_t c, int w, lv_opa_t opa) {
    lv_obj_set_style_border_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, w, LV_PART_MAIN);
    lv_obj_set_style_border_opa(o, opa, LV_PART_MAIN);
}

// Ecrit un libelle seulement si le texte a change (evite les invalidations LVGL).
static void set_text_if(lv_obj_t* l, const char* txt) {
    if (!l) return;
    const char* cur = lv_label_get_text(l);
    if (cur && strcmp(cur, txt) == 0) return;
    lv_label_set_text(l, txt);
}

static inline void set_color(lv_obj_t* l, uint32_t c) {
    if (l) lv_obj_set_style_text_color(l, lv_color_hex(c), LV_PART_MAIN);
}

// ===========================================================================
// 7. Correspondance case <-> position a l'ecran
// ===========================================================================
// Le pool de 64 cases est indexe par POSITION A L'ECRAN (0 = coin haut-gauche).
// Retourner le plateau ne deplace donc aucun objet : seule la correspondance
// change, et un redessin complet suffit.

static inline int cell_of_sq(uint8_t sq) {
    const int f = sq & 7, r = sq >> 4;
    const int col = g_flip ? (7 - f) : f;
    const int row = g_flip ? r : (7 - r);
    return row * 8 + col;
}

static inline uint8_t sq_of_cell(int i) {
    const int row = i >> 3, col = i & 7;
    const int f = g_flip ? (7 - col) : col;
    const int r = g_flip ? row : (7 - row);
    return (uint8_t)((r << 4) | f);
}

static inline bool cell_is_light(int i) {
    const int row = i >> 3, col = i & 7;
    return ((row + col) & 1) == 0;
}

// ===========================================================================
// 8. Declarations avancees
// ===========================================================================

static void go_hub();
static void go_setup();
static void go_settings();
static void go_stats();
static void show_pause();
static void show_promo();
static void show_over();
static void draw_all();
static void refresh_movelist();
static void update_hud(bool force);
static void check_game_end();
static void recompute_legal();
static void start_new_game();
static void do_hint();
static void piece_utf8(uint8_t type, bool outline, char* out);

// ===========================================================================
// 9. Construction de l'UI (une seule fois)
// ===========================================================================

static void cell_event_cb(lv_event_t* e);
static void slot_event_cb(lv_event_t* e);
static void pbtn_event_cb(lv_event_t* e);
static void hud_event_cb(lv_event_t* e);

static void build_ui() {
    if (g_built) return;

    // --- Fonds des conteneurs fournis par le YAML (aucune couleur dans le YAML) --
    set_bg(g_ui.root,  Pal::VOID_BG,  LV_OPA_COVER);
    set_bg(g_ui.hud,   Pal::HUD_BG,   LV_OPA_COVER);
    set_bg(g_ui.panel, Pal::PANEL_BG, LV_OPA_COVER);
    lv_obj_set_style_radius(g_ui.panel, 12, LV_PART_MAIN);
    set_bg(g_ui.board, Pal::SQ_DARK, LV_OPA_COVER);

    // Liseré sombre autour du plateau : le damier creme/vert « flotte » sinon
    // sur le fond anthracite, et l'oeil ne trouve pas le bord du a1.
    set_border(g_ui.board, Pal::BOARD_EDGE, 3, LV_OPA_COVER);

    // --- 64 cases, chacune portant ses deux calques de piece --------------
    for (int i = 0; i < 64; i++) {
        g_cell[i] = mk_rect(g_ui.board);
        lv_obj_set_size(g_cell[i], CELL, CELL);
        lv_obj_set_pos(g_cell[i], (i & 7) * CELL, (i >> 3) * CELL);
        lv_obj_add_flag(g_cell[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(g_cell[i], cell_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);

        // Corps puis contour : l'ordre de creation EST l'ordre de dessin, le
        // contour doit passer par-dessus le corps ivoire.
        g_body[i] = mk_label(g_cell[i], g_ui.f_piece, Pal::PC_B_FILL);
        lv_obj_align(g_body[i], LV_ALIGN_CENTER, 0, PIECE_DY);
        lv_obj_add_flag(g_body[i], LV_OBJ_FLAG_HIDDEN);

        g_edge[i] = mk_label(g_cell[i], g_ui.f_piece, Pal::PC_EDGE);
        lv_obj_align(g_edge[i], LV_ALIGN_CENTER, 0, PIECE_DY);
        lv_obj_add_flag(g_edge[i], LV_OBJ_FLAG_HIDDEN);
    }

    // --- Coordonnees (a-h / 1-8) facon livre d'echecs ---------------------
    // Posees DANS les cases de bord et teintees de la couleur de la case
    // opposee : lisibles sans voler de place au damier.
    for (int i = 0; i < 8; i++) {
        g_coord_f[i] = mk_label(g_ui.board, g_ui.f_small, Pal::TXT_MUTED);
        lv_obj_set_pos(g_coord_f[i], i * CELL + CELL - 18, BOARD_SZ - 27);
        g_coord_r[i] = mk_label(g_ui.board, g_ui.f_small, Pal::TXT_MUTED);
        lv_obj_set_pos(g_coord_r[i], 6, i * CELL + 3);
        // La rangee du bas est la ligne 7, la colonne de gauche la colonne 0 :
        // la parite de la case ne depend pas de l'orientation du plateau, ces
        // couleurs sont donc posees une fois pour toutes.
        set_color(g_coord_f[i], ((7 + i) & 1) == 0 ? Pal::SQ_DARK : Pal::SQ_LIGHT);
        set_color(g_coord_r[i], (i & 1) == 0 ? Pal::SQ_DARK : Pal::SQ_LIGHT);
    }

    // --- Marqueurs de coups legaux (au-dessus des cases) -------------------
    for (int i = 0; i < MAX_DOTS; i++) {
        g_dot[i] = mk_rect(g_ui.board);
        lv_obj_set_style_radius(g_dot[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_add_flag(g_dot[i], LV_OBJ_FLAG_HIDDEN);
    }

    // --- Piece en vol (creee en dernier = dessinee au-dessus de tout) ------
    // Conteneur transparent de la taille d'une case : il porte les deux memes
    // calques que les cases, et c'est LUI qu'on deplace pendant l'animation.
    g_anim = mk_rect(g_ui.board);
    lv_obj_set_size(g_anim, CELL, CELL);
    lv_obj_set_style_bg_opa(g_anim, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(g_anim, LV_OBJ_FLAG_HIDDEN);
    g_anim_body = mk_label(g_anim, g_ui.f_piece, Pal::PC_B_FILL);
    lv_obj_align(g_anim_body, LV_ALIGN_CENTER, 0, PIECE_DY);
    g_anim_edge = mk_label(g_anim, g_ui.f_piece, Pal::PC_EDGE);
    lv_obj_align(g_anim_edge, LV_ALIGN_CENTER, 0, PIECE_DY);

    // --- HUD ---------------------------------------------------------------
    g_h_turn = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT);
    lv_obj_align(g_h_turn, LV_ALIGN_LEFT_MID, 16, 0);
    g_h_level = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_align(g_h_level, LV_ALIGN_LEFT_MID, 300, 0);
    g_h_clock_w = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT);
    lv_obj_align(g_h_clock_w, LV_ALIGN_LEFT_MID, 560, 0);
    g_h_clock_b = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT);
    lv_obj_align(g_h_clock_b, LV_ALIGN_LEFT_MID, 700, 0);
    g_h_eval = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_align(g_h_eval, LV_ALIGN_LEFT_MID, 860, 0);
    g_h_status = mk_label(g_ui.hud, g_ui.f_small, Pal::DANGER);
    lv_obj_align(g_h_status, LV_ALIGN_RIGHT_MID, -16, 0);

    lv_obj_add_flag(g_ui.hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.hud, hud_event_cb, LV_EVENT_CLICKED, nullptr);

    // --- Panneau lateral ---------------------------------------------------
    g_p_title = mk_label(g_ui.panel, g_ui.f_small, Pal::ACCENT);
    lv_obj_set_pos(g_p_title, 24, 18);
    set_text_if(g_p_title, "PARTIE");

    for (int i = 0; i < MOVE_ROWS; i++) {
        const int y = 66 + i * 32;
        g_ml_num[i] = mk_label(g_ui.panel, g_ui.f_small, Pal::TXT_MUTED);
        lv_obj_set_pos(g_ml_num[i], 24, y);
        g_ml_w[i] = mk_label(g_ui.panel, g_ui.f_small, Pal::TXT);
        lv_obj_set_pos(g_ml_w[i], 96, y);
        g_ml_b[i] = mk_label(g_ui.panel, g_ui.f_small, Pal::TXT_DIM);
        lv_obj_set_pos(g_ml_b[i], 300, y);
    }

    g_think_lbl = mk_label(g_ui.panel, g_ui.f_small, Pal::THINK);
    lv_obj_set_pos(g_think_lbl, 24, 524);
    g_think_bar = mk_rect(g_ui.panel);
    lv_obj_set_size(g_think_bar, 504, 8);
    lv_obj_set_pos(g_think_bar, 24, 556);
    lv_obj_set_style_radius(g_think_bar, 4, LV_PART_MAIN);
    set_bg(g_think_bar, Pal::BTN_BG, LV_OPA_COVER);
    g_think_fill = mk_rect(g_think_bar);
    lv_obj_set_size(g_think_fill, 0, 8);
    lv_obj_set_pos(g_think_fill, 0, 0);
    lv_obj_set_style_radius(g_think_fill, 4, LV_PART_MAIN);
    set_bg(g_think_fill, Pal::THINK, LV_OPA_COVER);
    show(g_think_lbl, false);
    show(g_think_bar, false);

    static const char* const BTN[3] = {"Annuler", "Indice", "Menu"};
    for (int i = 0; i < 3; i++) {
        g_pbtn[i] = mk_rect(g_ui.panel);
        lv_obj_set_size(g_pbtn[i], 168, 64);
        lv_obj_set_pos(g_pbtn[i], 12 + i * 180, 590);
        lv_obj_set_style_radius(g_pbtn[i], 12, LV_PART_MAIN);
        set_bg(g_pbtn[i], Pal::BTN_BG, LV_OPA_COVER);
        set_border(g_pbtn[i], Pal::ACCENT, 2, LV_OPA_40);
        lv_obj_add_flag(g_pbtn[i], LV_OBJ_FLAG_CLICKABLE);
        // Retour tactile. Casts explicites : combiner lv_part_t et lv_state_t
        // directement est deprecie en C++20 (-Wdeprecated-enum-enum-conversion).
        lv_obj_set_style_bg_color(g_pbtn[i], lv_color_hex(Pal::BTN_BG_ON),
                                  (lv_style_selector_t) LV_PART_MAIN |
                                  (lv_style_selector_t) LV_STATE_PRESSED);
        lv_obj_add_event_cb(g_pbtn[i], pbtn_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        g_pbtn_lbl[i] = mk_label(g_pbtn[i], g_ui.f_small, Pal::TXT);
        lv_obj_align(g_pbtn_lbl[i], LV_ALIGN_CENTER, 0, 0);
        set_text_if(g_pbtn_lbl[i], BTN[i]);
    }

    // --- Calque des menus, cree ICI (le YAML n'a pas a le connaitre) --------
    g_menu = mk_rect(g_ui.root);
    lv_obj_set_size(g_menu, 1280, 720);
    lv_obj_set_pos(g_menu, 0, 0);
    set_bg(g_menu, Pal::VOID_BG, LV_OPA_COVER);
    lv_obj_add_flag(g_menu, LV_OBJ_FLAG_CLICKABLE);   // absorbe les taps vers le plateau
    lv_obj_add_flag(g_menu, LV_OBJ_FLAG_HIDDEN);

    g_m_title = mk_label(g_menu, g_ui.f_big, Pal::ACCENT);
    lv_obj_align(g_m_title, LV_ALIGN_TOP_MID, 0, 46);
    g_m_sub = mk_label(g_menu, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_align(g_m_sub, LV_ALIGN_TOP_MID, 0, 106);
    g_m_body = mk_label(g_menu, g_ui.f_small, Pal::TXT);
    lv_obj_set_width(g_m_body, 900);
    lv_obj_set_style_text_align(g_m_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g_m_body, LV_ALIGN_TOP_MID, 0, 148);
    g_m_foot = mk_label(g_menu, g_ui.f_small, Pal::TXT_MUTED);
    lv_obj_align(g_m_foot, LV_ALIGN_BOTTOM_MID, 0, -20);

    for (int i = 0; i < N_SLOTS; i++) {
        g_slot[i] = mk_rect(g_menu);
        lv_obj_add_flag(g_slot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(g_slot[i], 14, LV_PART_MAIN);
        set_bg(g_slot[i], Pal::BTN_BG, LV_OPA_COVER);
        lv_obj_set_style_bg_color(g_slot[i], lv_color_hex(Pal::BTN_BG_ON),
                                  (lv_style_selector_t) LV_PART_MAIN |
                                  (lv_style_selector_t) LV_STATE_PRESSED);
        lv_obj_add_event_cb(g_slot[i], slot_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        g_slot_t[i] = mk_label(g_slot[i], g_ui.f_mid, Pal::TXT);
        g_slot_d[i] = mk_label(g_slot[i], g_ui.f_small, Pal::TXT_DIM);
        lv_obj_add_flag(g_slot[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Libelles des 4 tuiles de promotion : centres SOUS chaque tuile, donc
    // alignes avec elles (un seul libelle en pied de page ne pourrait pas l'etre
    // avec une police proportionnelle).
    static const char* const PROMO_NAME[4] = {"Dame", "Tour", "Fou", "Cavalier"};
    for (int i = 0; i < 4; i++) {
        g_promo_name[i] = mk_label(g_menu, g_ui.f_small, Pal::TXT_DIM);
        lv_obj_set_width(g_promo_name[i], 168);
        lv_obj_set_style_text_align(g_promo_name[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(g_promo_name[i], LV_ALIGN_TOP_LEFT, 220 + i * 220, 484);
        set_text_if(g_promo_name[i], PROMO_NAME[i]);
        lv_obj_add_flag(g_promo_name[i], LV_OBJ_FLAG_HIDDEN);
    }

    memset(g_drawn_pc, 0xFF, sizeof(g_drawn_pc));
    memset(g_drawn_hl, 0xFF, sizeof(g_drawn_hl));
    g_built = true;
}

// Les libelles de promotion ne doivent apparaitre que sur cet ecran.
static void promo_names(bool v) {
    for (int i = 0; i < 4; i++) show(g_promo_name[i], v);
}

// Remet une entree de menu dans son habillage par defaut. Necessaire parce que
// l'ecran de promotion detourne les memes objets en tuiles d'echiquier (fond
// creme, police des figurines) : sans ce reset, le menu suivant en heriterait.
static void slot_reset(int i) {
    set_bg(g_slot[i], Pal::BTN_BG, LV_OPA_COVER);
    esphome::lvgl::lv_obj_set_style_text_font(g_slot_t[i], g_ui.f_mid, LV_PART_MAIN);
    esphome::lvgl::lv_obj_set_style_text_font(g_slot_d[i], g_ui.f_small, LV_PART_MAIN);
    set_color(g_slot_d[i], Pal::TXT_DIM);
    show(g_slot_d[i], true);
}

// --- Mise en page des entrees de menu --------------------------------------
static void slot_list(int i, const char* title, const char* desc, uint32_t col, bool on) {
    if (i < 0 || i >= N_SLOTS) return;
    slot_reset(i);
    lv_obj_set_size(g_slot[i], 720, 62);
    lv_obj_align(g_slot[i], LV_ALIGN_TOP_MID, 0, 210 + i * 70);
    lv_obj_set_width(g_slot_t[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(g_slot_t[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(g_slot_d[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(g_slot_d[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(g_slot_t[i], LV_ALIGN_LEFT_MID, 24, (desc && desc[0]) ? -13 : 0);
    lv_obj_align(g_slot_d[i], LV_ALIGN_LEFT_MID, 24, 15);
    set_color(g_slot_t[i], on ? col : Pal::TXT_MUTED);
    set_text_if(g_slot_t[i], title);
    set_text_if(g_slot_d[i], desc ? desc : "");
    set_border(g_slot[i], on ? col : Pal::TXT_MUTED, 2, LV_OPA_50);
    show(g_slot[i], true);
}

// Tuile de promotion : une vraie case d'echiquier portant la figurine, rendue
// EXACTEMENT comme sur le plateau (corps plein + contour pour les blancs). On
// detourne g_slot_t comme calque « corps » et g_slot_d comme calque « contour ».
static void slot_promo(int i, uint8_t type, bool white) {
    if (i < 0 || i >= N_SLOTS) return;
    lv_obj_set_size(g_slot[i], 168, 168);
    lv_obj_align(g_slot[i], LV_ALIGN_TOP_LEFT, 220 + i * 220, 300);
    lv_obj_set_style_radius(g_slot[i], 10, LV_PART_MAIN);
    // Alternance creme / vert : les 4 tuiles ressemblent a une rangee du damier.
    set_bg(g_slot[i], (i & 1) ? Pal::SQ_DARK : Pal::SQ_LIGHT, LV_OPA_COVER);
    set_border(g_slot[i], Pal::ACCENT, 3, LV_OPA_80);

    char g[4];
    esphome::lvgl::lv_obj_set_style_text_font(g_slot_t[i], g_ui.f_piece, LV_PART_MAIN);
    lv_obj_set_width(g_slot_t[i], LV_SIZE_CONTENT);
    lv_obj_align(g_slot_t[i], LV_ALIGN_CENTER, 0, PIECE_DY);
    piece_utf8(type, false, g);
    set_text_if(g_slot_t[i], g);
    set_color(g_slot_t[i], white ? Pal::PC_W_FILL : Pal::PC_B_FILL);

    esphome::lvgl::lv_obj_set_style_text_font(g_slot_d[i], g_ui.f_piece, LV_PART_MAIN);
    lv_obj_set_width(g_slot_d[i], LV_SIZE_CONTENT);
    lv_obj_align(g_slot_d[i], LV_ALIGN_CENTER, 0, PIECE_DY);
    if (white) {
        piece_utf8(type, true, g);
        set_text_if(g_slot_d[i], g);
        set_color(g_slot_d[i], Pal::PC_EDGE);
    }
    show(g_slot_d[i], white);
    show(g_slot[i], true);
}

static void slots_hide_from(int n) {
    for (int i = n; i < N_SLOTS; i++) show(g_slot[i], false);
    promo_names(false);          // par defaut : masques, show_promo() les rallume
}

static void menu_on(bool v, lv_opa_t opa = LV_OPA_COVER) {
    if (!g_menu) return;
    set_bg(g_menu, Pal::VOID_BG, opa);
    show(g_menu, v);
    if (v) lv_obj_move_foreground(g_menu);
}

// ===========================================================================
// 10. Rendu du plateau
// ===========================================================================

// [FR] Encode la figurine d'une piece en UTF-8, dans `out` (>= 4 octets).
// Le bloc Unicode « Chess Symbols » range les pieces roi -> pion :
//   U+2654..2659 = versions CREUSES (dites « blanches »)
//   U+265A..265F = versions PLEINES (dites « noires »)
// Notre enum va dans l'autre sens (PAWN=1 ... KING=6), d'ou l'index 6 - type.
// On n'utilise PAS la semantique blanc/noir d'Unicode : le corps est toujours le
// glyphe PLEIN (colore en ivoire ou en anthracite), et le glyphe CREUX sert de
// contour pour les pieces claires.
static void piece_utf8(uint8_t type, bool outline, char* out) {
    const int idx = 6 - (int)(type & TYPE_MASK);      // roi = 0 ... pion = 5
    const uint32_t cp = (outline ? 0x2654u : 0x265Au) + (uint32_t) idx;
    out[0] = (char)(0xE0u | (cp >> 12));
    out[1] = (char)(0x80u | ((cp >> 6) & 0x3Fu));
    out[2] = (char)(0x80u | (cp & 0x3Fu));
    out[3] = 0;
}

static uint8_t hl_of_cell(int i) {
    const uint8_t sq = sq_of_cell(i);
    if (g_hint_until && (sq == g_hint_from || sq == g_hint_to)) return HL_HINT;
    if (g_sel_sq != NO_SQ && sq == g_sel_sq) return HL_SEL;
    if (g_check_sq != NO_SQ && sq == g_check_sq) return HL_CHECK;
    if (g_last_from != NO_SQ && (sq == g_last_from || sq == g_last_to)) return HL_LAST;
    return HL_NONE;
}

static uint32_t cell_color(int i, uint8_t hl) {
    const bool light = cell_is_light(i);
    switch (hl) {
        case HL_HINT:  return light ? Pal::SQ_L_HINT : Pal::SQ_D_HINT;
        case HL_SEL:   return light ? Pal::SQ_L_SEL  : Pal::SQ_D_SEL;
        case HL_CHECK: return light ? Pal::SQ_L_CHK  : Pal::SQ_D_CHK;
        case HL_LAST:  return light ? Pal::SQ_L_LAST : Pal::SQ_D_LAST;
        default:       return light ? Pal::SQ_LIGHT  : Pal::SQ_DARK;
    }
}

static void draw_cell(int i, bool force) {
    const uint8_t sq = sq_of_cell(i);
    uint8_t pc = g_pos.board[sq];
    if (i == g_anim_hide) pc = EMPTY;          // piece en vol : masquee a l'arrivee
    const uint8_t hl = hl_of_cell(i);

    if (force || hl != g_drawn_hl[i]) {
        set_bg(g_cell[i], cell_color(i, hl), LV_OPA_COVER);
        g_drawn_hl[i] = hl;
    }
    if (!force && pc == g_drawn_pc[i]) return;
    g_drawn_pc[i] = pc;

    if (!pc) { show(g_body[i], false); show(g_edge[i], false); return; }

    const bool white = (pc & COLOR_MASK) == WHITE;
    char g[4];
    piece_utf8(pc, false, g);                 // corps : toujours le glyphe plein
    set_text_if(g_body[i], g);
    set_color(g_body[i], white ? Pal::PC_W_FILL : Pal::PC_B_FILL);
    show(g_body[i], true);

    // Contour uniquement sur les pieces claires : sans lui, un corps ivoire
    // disparaitrait sur une case creme.
    if (white) {
        piece_utf8(pc, true, g);
        set_text_if(g_edge[i], g);
        show(g_edge[i], true);
    } else {
        show(g_edge[i], false);
    }
}

// Marqueurs des destinations legales depuis la case selectionnee.
static void draw_dots() {
    int n = 0;
    if (g_sel_sq != NO_SQ) {
        for (int i = 0; i < g_nfrom && n < MAX_DOTS; i++) {
            const uint8_t to = g_from_legal[i].to;
            // Les 4 promotions partagent la meme case d'arrivee : un seul marqueur.
            bool dup = false;
            for (int j = 0; j < i; j++) if (g_from_legal[j].to == to) { dup = true; break; }
            if (dup) continue;
            const int c = cell_of_sq(to);
            const bool cap = (g_from_legal[i].flags & MF_CAPTURE) != 0;
            const int sz = cap ? 74 : 26;
            lv_obj_set_size(g_dot[n], sz, sz);
            lv_obj_set_pos(g_dot[n], (c & 7) * CELL + (CELL - sz) / 2,
                                     (c >> 3) * CELL + (CELL - sz) / 2);
            if (cap) {
                // Capture : anneau autour de la piece (pas de pastille pleine,
                // sinon on ne verrait plus quelle piece on va prendre).
                set_bg(g_dot[n], Pal::DOT_CAPTURE, LV_OPA_TRANSP);
                set_border(g_dot[n], Pal::DOT_CAPTURE, 5, LV_OPA_90);
            } else {
                set_bg(g_dot[n], Pal::DOT_MOVE, LV_OPA_70);
                set_border(g_dot[n], Pal::DOT_MOVE, 0, LV_OPA_TRANSP);
            }
            show(g_dot[n], true);
            n++;
        }
    }
    for (int i = n; i < MAX_DOTS; i++) show(g_dot[i], false);
}

static void draw_coords() {
    for (int i = 0; i < 8; i++) {
        char c[2];
        c[1] = 0;
        c[0] = (char)('a' + (g_flip ? (7 - i) : i));
        set_text_if(g_coord_f[i], c);
        c[0] = (char)('1' + (g_flip ? i : (7 - i)));
        set_text_if(g_coord_r[i], c);
    }
}

static void draw_all() {
    for (int i = 0; i < 64; i++) draw_cell(i, false);
    draw_dots();
    draw_coords();
}

// ===========================================================================
// 11. HUD et liste de coups
// ===========================================================================

// Message court dans la zone de statut du HUD (2,5 s par defaut).
static void toast(const char* t, uint32_t ms = 2500) {
    strncpy(g_msg, t, sizeof(g_msg) - 1);
    g_msg[sizeof(g_msg) - 1] = 0;
    g_msg_until = esphome::millis() + ms;
}

static void fmt_clock(uint32_t ms, char* out, int cap) {
    const uint32_t s = ms / 1000;
    if (s >= 3600) snprintf(out, cap, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));
    else           snprintf(out, cap, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}

static void update_hud(bool force) {
    char buf[48];

    // Trait
    const bool white_turn = (g_pos.side == WHITE);
    if (g_mode == 1) snprintf(buf, sizeof(buf), "Trait aux %s", white_turn ? "Blancs" : "Noirs");
    else if (g_mode == 2) snprintf(buf, sizeof(buf), "Demo — %s", white_turn ? "Blancs" : "Noirs");
    else snprintf(buf, sizeof(buf), "Trait : %s", (g_pos.side == g_human) ? "a vous" : "au Tab");
    set_text_if(g_h_turn, buf);
    set_color(g_h_turn, (g_mode == 0 && g_pos.side == g_human) ? Pal::ACCENT : Pal::TXT);

    // Niveau d'IA (sans objet en hotseat)
    if (g_mode == 1) snprintf(buf, sizeof(buf), "Duel local");
    else snprintf(buf, sizeof(buf), "Niveau : %s", AI_LEVELS[g_level].name);
    set_text_if(g_h_level, buf);

    // Pendules
    if (g_clock_on) {
        for (int c = 0; c < 2; c++) {
            const int sec = (int)(g_clock[c] / 1000);
            if (force || sec != g_c_clock[c]) {
                g_c_clock[c] = sec;
                char t[16], line[32];
                fmt_clock(g_clock[c], t, sizeof(t));
                snprintf(line, sizeof(line), "%s %s", c == 0 ? "B" : "N", t);
                set_text_if(c == 0 ? g_h_clock_w : g_h_clock_b, line);
                lv_obj_t* lab = (c == 0) ? g_h_clock_w : g_h_clock_b;
                const bool active = g_running && (cidx(g_pos.side) == c);
                set_color(lab, g_clock[c] < 30000 ? Pal::DANGER : (active ? Pal::TXT : Pal::TXT_DIM));
            }
        }
    } else if (force) {
        set_text_if(g_h_clock_w, "");
        set_text_if(g_h_clock_b, "");
    }

    // Evaluation approximative (statique, du point de vue des blancs).
    if (g_save.show_eval) {
        int e = eval(g_pos);
        if (g_pos.side == BLACK) e = -e;
        if (force || e / 5 != g_c_eval / 5) {          // pas de clignotement pour 4 centiemes
            g_c_eval = e;
            snprintf(buf, sizeof(buf), "Eval %+.1f", e / 100.0f);
            set_text_if(g_h_eval, buf);
            set_color(g_h_eval, e > 80 ? Pal::GOOD : (e < -80 ? Pal::DANGER : Pal::TXT_DIM));
        }
    } else if (force) {
        set_text_if(g_h_eval, "");
    }

    // Statut : message transitoire, sinon echec, sinon reflexion, sinon rien.
    const char* st = "";
    uint32_t stc = Pal::DANGER;
    if (g_msg_until && (int32_t)(esphome::millis() - g_msg_until) < 0) { st = g_msg; stc = Pal::TXT_DIM; }
    else if (g_check_sq != NO_SQ) { st = "ECHEC !"; stc = Pal::DANGER; }
    else if (g_ai_think)          { st = "Le Tab reflechit"; stc = Pal::THINK; }
    set_text_if(g_h_status, st);
    set_color(g_h_status, stc);
}

static void refresh_movelist() {
    struct Row { uint16_t num; char w[12]; char b[12]; };
    static Row rows[MOVE_ROWS];
    int nrows = 0;

    for (int i = 0; i < g_nply; i++) {
        const uint16_t num = g_hist_num[i];
        if (nrows == 0 || rows[nrows - 1].num != num) {
            if (nrows == MOVE_ROWS) {           // decalage : on garde les dernieres
                for (int k = 1; k < MOVE_ROWS; k++) rows[k - 1] = rows[k];
                nrows--;
            }
            rows[nrows].num = num;
            rows[nrows].w[0] = 0;
            rows[nrows].b[0] = 0;
            nrows++;
        }
        char* dst = (g_hist_side[i] == WHITE) ? rows[nrows - 1].w : rows[nrows - 1].b;
        strncpy(dst, g_hist_san[i], 11);
        dst[11] = 0;
    }

    char buf[16];
    for (int i = 0; i < MOVE_ROWS; i++) {
        if (i < nrows) {
            snprintf(buf, sizeof(buf), "%u.", (unsigned) rows[i].num);
            set_text_if(g_ml_num[i], buf);
            set_text_if(g_ml_w[i], rows[i].w[0] ? rows[i].w : "...");
            set_text_if(g_ml_b[i], rows[i].b);
        } else {
            set_text_if(g_ml_num[i], "");
            set_text_if(g_ml_w[i], "");
            set_text_if(g_ml_b[i], "");
        }
    }
}

// ===========================================================================
// 12. Ecrans
// ===========================================================================

static void go_hub() {
    g_state = ST_HUB;
    g_ai_think = false;
    menu_on(true);
    set_text_if(g_m_title, "ROI NOIR");
    set_color(g_m_title, Pal::ACCENT);
    set_text_if(g_m_sub, "Echiquier du Tab — regles FIDE, IA embarquee, 100 % local");
    char body[128];
    snprintf(body, sizeof(body), "Classement local : %u Elo   ·   %u parties jouees",
             (unsigned) g_save.elo, (unsigned) g_save.games);
    set_text_if(g_m_body, body);
    set_text_if(g_m_foot, "Toucher le bandeau du haut pendant la partie ouvre le menu de pause.");

    int i = 0;
    slot_list(i++, "Nouvelle partie", "Choix du mode, de la couleur, du niveau et de la pendule", Pal::ACCENT, true);
    if (g_save.resume_valid) {
        char d[96];
        snprintf(d, sizeof(d), "%s · niveau %s · %u demi-coups joues",
                 MODE_NAME[g_save.r_mode < 3 ? g_save.r_mode : 0],
                 AI_LEVELS[g_save.r_level < CHESS_NLEVELS ? g_save.r_level : 0].name,
                 (unsigned) g_save.r_plies);
        slot_list(i++, "Reprendre la partie", d, Pal::GOOD, true);
    }
    slot_list(i++, "Statistiques", "Bilan par niveau, records, classement local", Pal::TXT, true);
    slot_list(i++, "Reglages", "Gestes, regle des 50 coups, evaluation, vitesse de demo", Pal::TXT, true);
    slot_list(i++, "Quitter", "Retour au tableau de bord", Pal::TXT_DIM, true);
    slots_hide_from(i);
}

static void go_setup() {
    g_state = ST_SETUP;
    menu_on(true);
    set_text_if(g_m_title, "NOUVELLE PARTIE");
    set_color(g_m_title, Pal::ACCENT);
    set_text_if(g_m_sub, "Toucher une ligne pour changer sa valeur");
    set_text_if(g_m_body, "");
    set_text_if(g_m_foot, "");

    char d[96];
    int i = 0;
    slot_list(i++, "Mode de jeu", MODE_NAME[g_save.mode], Pal::ACCENT, true);

    if (g_save.mode == 0) {
        snprintf(d, sizeof(d), "%s — vous ouvrez %s",
                 g_save.human_side == 0 ? "Blancs" : "Noirs",
                 g_save.human_side == 0 ? "la partie" : "en second");
        slot_list(i++, "Votre couleur", d, Pal::TXT, true);
    } else {
        slot_list(i++, "Votre couleur", "Sans objet dans ce mode", Pal::TXT_MUTED, false);
    }

    if (g_save.mode == 1) {
        slot_list(i++, "Niveau du Tab", "Sans objet : duel entre deux humains", Pal::TXT_MUTED, false);
    } else {
        snprintf(d, sizeof(d), "%s — %s", AI_LEVELS[g_save.level].name, AI_LEVELS[g_save.level].desc);
        slot_list(i++, "Niveau du Tab", d, Pal::TXT, true);
    }

    slot_list(i++, "Pendule", CLOCKS[g_save.clock_opt].name, Pal::TXT, true);
    slot_list(i++, "Commencer", "Lance la partie avec ces reglages", Pal::GOOD, true);
    slot_list(i++, "Retour", "Revenir au menu principal", Pal::TXT_DIM, true);
    slots_hide_from(i);
}

static void go_settings() {
    g_state = ST_SETTINGS;
    menu_on(true);
    set_text_if(g_m_title, "REGLAGES");
    set_color(g_m_title, Pal::ACCENT);
    set_text_if(g_m_sub, "Conserves en NVS, valables pour toutes les parties");
    set_text_if(g_m_body, "");
    set_text_if(g_m_foot, "");

    int i = 0;
    slot_list(i++, "Gestes IMU",
              g_save.gestures ? "Actives — une secousse franche demande un indice"
                              : "Desactives — l'inclinaison est ignoree",
              g_save.gestures ? Pal::GOOD : Pal::TXT_MUTED, true);
    slot_list(i++, "Regle des 50 coups",
              g_save.rule50 ? "Nulle automatique apres 50 coups sans prise ni pion"
                            : "Desactivee — la partie continue",
              g_save.rule50 ? Pal::GOOD : Pal::TXT_MUTED, true);
    slot_list(i++, "Evaluation au HUD",
              g_save.show_eval ? "Affichee — estimation en pions, indicative"
                               : "Masquee",
              g_save.show_eval ? Pal::GOOD : Pal::TXT_MUTED, true);
    char d[64];
    snprintf(d, sizeof(d), "%s — cadence du mode Tab contre Tab", DEMO_NAME[g_save.demo_speed]);
    slot_list(i++, "Vitesse de demo", d, Pal::TXT, true);
    slot_list(i++, "Effacer les statistiques", "Remet a zero le bilan et le classement local", Pal::DANGER, true);
    slot_list(i++, "Retour", "Revenir au menu principal", Pal::TXT_DIM, true);
    slots_hide_from(i);
}

static void go_stats() {
    g_state = ST_STATS;
    menu_on(true);
    set_text_if(g_m_title, "STATISTIQUES");
    set_color(g_m_title, Pal::ACCENT);

    char sub[96];
    snprintf(sub, sizeof(sub), "Classement local : %u Elo   ·   %u parties contre le Tab",
             (unsigned) g_save.elo, (unsigned) g_save.games);
    set_text_if(g_m_sub, sub);

    // Bilan par niveau, en une seule etiquette multi-lignes.
    // [FR] snprintf renvoie la longueur QU'IL AURAIT ECRITE : cumuler son
    // retour sans borne ferait deborder `k` au-dela du tampon, et le
    // `sizeof(body) - k` suivant repasserait en arithmetique non signee (donc
    // une taille enorme). D'ou le clamp systematique via append().
    char body[512];
    int k = 0;
    auto append = [&](const char* fmt, auto... args) {
        if (k >= (int) sizeof(body) - 1) return;
        const int n = snprintf(body + k, sizeof(body) - (size_t) k, fmt, args...);
        if (n < 0) return;
        k = (n >= (int) sizeof(body) - k) ? (int) sizeof(body) - 1 : k + n;
    };
    append("%s", "Niveau            V / N / D\n");
    for (int l = 0; l < CHESS_NLEVELS; l++) {
        append("%-10s   %u / %u / %u\n", AI_LEVELS[l].name,
               (unsigned) g_save.wins[l], (unsigned) g_save.draws[l],
               (unsigned) g_save.losses[l]);
    }
    append("\nPlus longue partie : %u demi-coups\n", (unsigned) g_save.longest_plies);
    append("Temps de jeu cumule : %u min", (unsigned)(g_save.total_ms / 60000u));
    lv_obj_set_style_text_align(g_m_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    set_text_if(g_m_body, body);
    set_text_if(g_m_foot, "");

    slot_list(0, "Retour", "Revenir au menu principal", Pal::TXT_DIM, true);
    lv_obj_align(g_slot[0], LV_ALIGN_BOTTOM_MID, 0, -80);
    slots_hide_from(1);
}

static void show_confirm(uint8_t kind, const char* title, const char* question) {
    g_confirm_kind = kind;
    g_state = ST_CONFIRM;
    menu_on(true);
    set_text_if(g_m_title, title);
    set_color(g_m_title, Pal::ACCENT);
    set_text_if(g_m_sub, question);
    set_text_if(g_m_body, "");
    set_text_if(g_m_foot, "");
    slot_list(0, "Confirmer", "", Pal::DANGER, true);
    slot_list(1, "Annuler", "", Pal::TXT_DIM, true);
    slots_hide_from(2);
}

static void show_promo() {
    g_state = ST_PROMO;
    // 82 % d'opacite : on garde le plateau visible derriere, c'est utile pour
    // decider entre dame et cavalier.
    menu_on(true, (lv_opa_t) 209);
    set_text_if(g_m_title, "PROMOTION");
    set_color(g_m_title, Pal::ACCENT);
    set_text_if(g_m_sub, "Le pion atteint la derniere rangee — choisir la piece");
    set_text_if(g_m_body, "");
    set_text_if(g_m_foot, "");

    // La couleur promue est celle du camp AU TRAIT (le pion n'a pas encore bouge).
    const bool white = (g_pos.side == WHITE);
    static const uint8_t PROMO_T[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
    for (int i = 0; i < 4; i++) slot_promo(i, PROMO_T[i], white);
    slots_hide_from(4);
    promo_names(true);
}

static void show_pause() {
    g_state = ST_PAUSE;
    menu_on(true, (lv_opa_t) 235);
    set_text_if(g_m_title, "PAUSE");
    set_color(g_m_title, Pal::ACCENT);
    set_text_if(g_m_sub, "La pendule est arretee");
    set_text_if(g_m_body, "");
    set_text_if(g_m_foot, "");
    int i = 0;
    slot_list(i++, "Reprendre", "Retour a la partie", Pal::GOOD, true);
    slot_list(i++, "Annuler le dernier coup", g_mode == 0 ? "Annule votre coup et la reponse du Tab"
                                                          : "Annule le dernier demi-coup",
              g_nply > 0 ? Pal::TXT : Pal::TXT_MUTED, g_nply > 0);
    slot_list(i++, "Proposer nulle", g_mode == 0 ? "Le Tab accepte s'il n'est pas mieux"
                                                 : "Accord entre les deux joueurs", Pal::TXT, true);
    slot_list(i++, "Abandonner", "La partie est perdue et comptabilisee", Pal::DANGER, true);
    slot_list(i++, "Reglages", "Gestes, regles, affichage", Pal::TXT_DIM, true);
    slot_list(i++, "Quitter le jeu", "La partie en cours est sauvegardee", Pal::TXT_DIM, true);
    slots_hide_from(i);
}

static void show_over() {
    g_state = ST_OVER;
    menu_on(true);
    const char* t = "PARTIE NULLE";
    uint32_t col = Pal::TXT;
    if (g_result != RES_DRAW) {
        if (g_mode == 0) {
            const bool human_won = (g_result == RES_WHITE) ? (g_human == WHITE) : (g_human == BLACK);
            t = human_won ? "VICTOIRE" : "DEFAITE";
            col = human_won ? Pal::GOOD : Pal::DANGER;
        } else {
            t = (g_result == RES_WHITE) ? "LES BLANCS GAGNENT" : "LES NOIRS GAGNENT";
            col = Pal::ACCENT;
        }
    }
    set_text_if(g_m_title, t);
    set_color(g_m_title, col);
    set_text_if(g_m_sub, g_reason);

    char body[160];
    if (g_mode == 0)
        snprintf(body, sizeof(body), "%d demi-coups  ·  niveau %s  ·  classement local : %u Elo",
                 g_nply, AI_LEVELS[g_level].name, (unsigned) g_save.elo);
    else
        snprintf(body, sizeof(body), "%d demi-coups joues", g_nply);
    set_text_if(g_m_body, body);
    set_text_if(g_m_foot, "");

    int i = 0;
    slot_list(i++, "Rejouer", "Meme mode, memes reglages", Pal::ACCENT, true);
    slot_list(i++, "Menu principal", "", Pal::TXT_DIM, true);
    slots_hide_from(i);
}

// ===========================================================================
// 13. Logique de partie
// ===========================================================================

static inline bool side_is_ai() {
    if (g_mode == 2) return true;
    if (g_mode == 1) return false;
    return g_pos.side != g_human;
}

static void recompute_legal() {
    g_nall = gen_legal(g_pos, g_all_legal);
    g_check_sq = in_check(g_pos, g_pos.side) ? g_pos.king_sq[cidx(g_pos.side)] : NO_SQ;
}

// Nombre d'occurrences de la position courante dans l'historique.
// [FR] La clef Zobrist n'encode ni le compteur des 50 coups ni le numero de coup :
// deux positions identiques (meme trait, memes droits de roque, meme case de
// prise en passant exploitable) ont donc la meme clef, ce qui est exactement la
// definition FIDE de la repetition.
static int repetition_count() {
    const uint64_t h = g_hist_hash[g_nply];
    int n = 0;
    for (int i = 0; i <= g_nply; i++) if (g_hist_hash[i] == h) n++;
    return n;
}

static void update_elo(float score) {
    const int opp = (int) AI_LEVELS[g_level].elo;
    const float expected = 1.0f / (1.0f + powf(10.0f, (float)(opp - (int) g_save.elo) / 400.0f));
    int e = (int) g_save.elo + (int) lroundf(24.0f * (score - expected));
    if (e < 100) e = 100;
    if (e > 3000) e = 3000;
    g_save.elo = (uint32_t) e;
}

static void end_game(uint8_t res, const char* reason) {
    g_running = false;
    g_ai_think = false;
    g_result = res;
    g_reason = reason;

    // Une animation en vol au moment du mat laisserait une case vide derriere le
    // menu : on la termine immediatement.
    if (g_anim_on) {
        g_anim_on = false;
        g_anim_hide = -1;
        show(g_anim, false);
        draw_all();
    }

    if ((uint32_t) g_nply > g_save.longest_plies) g_save.longest_plies = (uint32_t) g_nply;
    if (g_game_t0) g_save.total_ms += esphome::millis() - g_game_t0;
    g_game_t0 = 0;

    // Seules les parties CONTRE LE TAB alimentent le bilan et le classement.
    if (g_mode == 0 && g_level < CHESS_NLEVELS) {
        g_save.games++;
        if (res == RES_DRAW) { g_save.draws[g_level]++; update_elo(0.5f); }
        else {
            const bool human_won = (res == RES_WHITE) ? (g_human == WHITE) : (g_human == BLACK);
            if (human_won) { g_save.wins[g_level]++;   update_elo(1.0f); }
            else           { g_save.losses[g_level]++; update_elo(0.0f); }
        }
    }
    g_save.resume_valid = 0;      // la partie est finie : plus rien a reprendre
    persist_save();
    show_over();
}

static void check_game_end() {
    recompute_legal();
    if (g_nall == 0) {
        if (g_check_sq != NO_SQ) end_game(g_pos.side == WHITE ? RES_BLACK : RES_WHITE, "Echec et mat");
        else                     end_game(RES_DRAW, "Pat — le roi n'est pas en echec mais aucun coup n'est jouable");
        return;
    }
    if (insufficient_material(g_pos)) { end_game(RES_DRAW, "Materiel insuffisant pour mater"); return; }
    if (g_save.rule50 && g_pos.halfmove >= 100) {
        end_game(RES_DRAW, "Regle des 50 coups"); return;
    }
    if (repetition_count() >= 3) { end_game(RES_DRAW, "Triple repetition de la position"); return; }
}

// Applique un coup au modele puis declenche le rendu et l'animation.
static void play_move(const Move& m) {
    if (g_nply >= MAX_HIST) {          // garde-fou : historique plein
        end_game(RES_DRAW, "Partie interrompue : historique plein");
        return;
    }
    move_to_san(g_pos, m, g_hist_san[g_nply], 12);
    g_hist_num[g_nply]  = g_pos.fullmove;
    g_hist_side[g_nply] = g_pos.side;

    const int from_cell = cell_of_sq(m.from);
    const int to_cell   = cell_of_sq(m.to);
    const uint8_t mover = g_pos.side;

    if (!make(g_pos, m, g_hist_undo[g_nply])) return;   // ne doit jamais arriver
    g_hist_move[g_nply] = m;
    g_nply++;
    g_hist_hash[g_nply] = hash_of(g_pos);

    // Increment Fischer credite au joueur qui vient de jouer.
    if (g_clock_on && g_inc_ms) g_clock[cidx(mover)] += g_inc_ms;

    g_last_from = m.from;
    g_last_to   = m.to;
    g_sel_sq    = NO_SQ;
    g_nfrom     = 0;
    g_hint_until = 0;

    // Animation : la pastille d'arrivee est masquee, une pastille libre vole de
    // la case de depart vers la case d'arrivee.
    const uint8_t shown = g_pos.board[m.to];
    if (shown) {
        const bool white = (shown & COLOR_MASK) == WHITE;
        char g[4];
        piece_utf8(shown, false, g);
        set_text_if(g_anim_body, g);
        set_color(g_anim_body, white ? Pal::PC_W_FILL : Pal::PC_B_FILL);
        if (white) { piece_utf8(shown, true, g); set_text_if(g_anim_edge, g); }
        show(g_anim_edge, white);
        // Le conteneur fait exactement une case : ses coordonnees sont celles
        // de la case, sans recentrage a calculer.
        g_anim_x0 = (from_cell & 7) * CELL;
        g_anim_y0 = (from_cell >> 3) * CELL;
        g_anim_x1 = (to_cell & 7) * CELL;
        g_anim_y1 = (to_cell >> 3) * CELL;
        lv_obj_set_pos(g_anim, g_anim_x0, g_anim_y0);
        show(g_anim, true);
        lv_obj_move_foreground(g_anim);
        g_anim_hide = to_cell;
        g_anim_on = true;
        g_anim_t0 = esphome::millis();
    }

    draw_all();
    refresh_movelist();
    update_hud(true);
    check_game_end();
}

static void undo_move() {
    if (g_state != ST_PLAY && g_state != ST_PAUSE) return;
    if (g_nply <= 0) return;

    // Si le Tab reflechit, on annule sa reflexion et on remonte d'un demi-coup
    // (celui du joueur) : le trait revient a l'humain.
    int steps = 1;
    if (g_ai_think) { g_ai_think = false; }
    else if (g_mode == 0 && g_nply >= 2) steps = 2;

    for (int k = 0; k < steps && g_nply > 0; k++) {
        g_nply--;
        unmake(g_pos, g_hist_move[g_nply], g_hist_undo[g_nply]);
    }
    if (g_nply > 0) { g_last_from = g_hist_move[g_nply - 1].from; g_last_to = g_hist_move[g_nply - 1].to; }
    else            { g_last_from = NO_SQ; g_last_to = NO_SQ; }

    g_sel_sq = NO_SQ;
    g_nfrom = 0;
    g_hint_until = 0;
    g_anim_on = false;
    g_anim_hide = -1;
    show(g_anim, false);
    g_running = true;
    g_ai_next_at = 0;
    recompute_legal();
    memset(g_drawn_pc, 0xFF, sizeof(g_drawn_pc));
    draw_all();
    refresh_movelist();
    update_hud(true);
}

static void start_new_game() {
    g_mode  = g_save.mode;
    g_level = g_save.level;
    g_human = (g_save.human_side == 0) ? WHITE : BLACK;

    set_start(g_pos);
    g_nply = 0;
    g_hist_hash[0] = hash_of(g_pos);

    // Plateau oriente cote joueur : en hotseat et en demo on garde les blancs
    // en bas (convention), en solo on tourne si l'humain a les noirs.
    g_flip = (g_mode == 0 && g_human == BLACK);

    g_clock_on = (g_save.clock_opt != 0);
    g_clock[0] = g_clock[1] = CLOCKS[g_save.clock_opt].base_ms;
    g_inc_ms   = CLOCKS[g_save.clock_opt].inc_ms;
    g_clock_last = esphome::millis();
    g_game_t0 = esphome::millis();

    g_sel_sq = g_last_from = g_last_to = NO_SQ;
    g_nfrom = 0;
    g_hint_until = 0;
    g_hint_ready_at = 0;
    g_ai_think = false;
    g_ai_next_at = 0;
    g_anim_on = false;
    g_anim_hide = -1;
    show(g_anim, false);
    g_running = true;

    g_state = ST_PLAY;
    menu_on(false);
    recompute_legal();
    memset(g_drawn_pc, 0xFF, sizeof(g_drawn_pc));
    memset(g_drawn_hl, 0xFF, sizeof(g_drawn_hl));
    draw_all();
    refresh_movelist();
    update_hud(true);
}

// Reprise d'une partie sauvegardee (FEN + pendules).
// [AI-CONTEXT] Limitation ASSUMEE : seule la POSITION est restauree, pas
// l'historique des coups. Consequence : « Annuler » et le compteur de triple
// repetition repartent de zero apres une reprise. C'est le compromis choisi pour
// garder ChessSave sous 200 octets en NVS.
static bool resume_game() {
    if (!g_save.resume_valid) return false;
    Position p;
    if (!set_fen(p, g_save.r_fen)) { g_save.resume_valid = 0; return false; }
    g_pos   = p;
    g_mode  = (g_save.r_mode < 3) ? g_save.r_mode : 0;
    g_level = (g_save.r_level < CHESS_NLEVELS) ? g_save.r_level : 2;
    g_human = (g_save.r_human == 0) ? WHITE : BLACK;

    g_nply = 0;
    g_hist_hash[0] = hash_of(g_pos);
    g_flip = (g_mode == 0 && g_human == BLACK);

    g_clock[0] = g_save.r_clock_w;
    g_clock[1] = g_save.r_clock_b;
    g_clock_on = (g_clock[0] != 0 || g_clock[1] != 0);
    g_inc_ms   = CLOCKS[g_save.clock_opt].inc_ms;
    g_clock_last = esphome::millis();
    g_game_t0 = esphome::millis();

    g_sel_sq = g_last_from = g_last_to = NO_SQ;
    g_nfrom = 0;
    g_hint_until = 0;
    g_ai_think = false;
    g_ai_next_at = 0;
    g_anim_on = false;
    g_anim_hide = -1;
    show(g_anim, false);
    g_running = true;

    g_state = ST_PLAY;
    menu_on(false);
    recompute_legal();
    memset(g_drawn_pc, 0xFF, sizeof(g_drawn_pc));
    memset(g_drawn_hl, 0xFF, sizeof(g_drawn_hl));
    draw_all();
    refresh_movelist();
    update_hud(true);
    return true;
}

// Sauvegarde de la partie en cours (appelee a la fermeture du jeu).
// [FR] Si aucune partie n'est en cours, on ne TOUCHE PAS a la sauvegarde
// existante : quitter depuis le hub ne doit pas effacer la partie que l'on
// comptait reprendre. C'est end_game() qui remet resume_valid a 0.
static void store_running_game() {
    if (!g_running) return;
    get_fen(g_pos, g_save.r_fen, CHESS_FEN_CAP);
    g_save.r_level   = g_level;
    g_save.r_mode    = g_mode;
    g_save.r_human   = (g_human == WHITE) ? 0 : 1;
    g_save.r_clock_w = g_clock_on ? g_clock[0] : 0;
    g_save.r_clock_b = g_clock_on ? g_clock[1] : 0;
    g_save.r_plies   = (uint16_t) g_nply;
    g_save.resume_valid = 1;
}

// Proposition de nulle. Contre le Tab, il accepte s'il n'est pas mieux : une
// recherche courte (profondeur 2, 30 ms max) donne son avis.
static void offer_draw() {
    if (g_mode == 1) { end_game(RES_DRAW, "Nulle par accord entre les joueurs"); return; }
    if (g_mode == 2) { end_game(RES_DRAW, "Nulle declaree en mode demo"); return; }
    int sc = 0;
    search_quick(g_pos, 2, 30, &sc);
    // `sc` est du point de vue du trait. Si c'est au Tab de jouer, un score
    // positif signifie qu'il est mieux : il refuse.
    const int tab_score = (g_pos.side == g_human) ? -sc : sc;
    if (tab_score <= 40) end_game(RES_DRAW, "Le Tab accepte la nulle");
    else {
        g_state = ST_PLAY;
        menu_on(false);
        g_clock_last = esphome::millis();
        toast("Le Tab refuse la nulle", 3000);
        update_hud(true);
    }
}

// ===========================================================================
// 14. Pilotage de l'IA
// ===========================================================================

static void ai_begin() {
    if (!g_running || g_ai_think) return;
    if (g_nall <= 0) return;
    g_ai_think = true;
    search_start(g_pos, g_level, esphome::millis() ^ (uint32_t)(g_nply * 2654435761u));
    show(g_think_lbl, true);
    show(g_think_bar, true);
}

void ai_step() {
    if (!g_ai_think) return;
    if (!search_step()) {
        // Progression : temps CPU consomme / budget du niveau, plus la profondeur
        // reellement atteinte (c'est l'information honnete, pas une estimation).
        const uint32_t bud = search_budget_ms() ? search_budget_ms() : 1;
        uint32_t pct = search_cpu_ms() * 100u / bud;
        if (pct > 100u) pct = 100u;
        lv_obj_set_width(g_think_fill, (int)(504u * pct / 100u));
        char b[64];
        snprintf(b, sizeof(b), "Le Tab reflechit — profondeur %d, %u k noeuds",
                 search_depth_done(), (unsigned)(search_nodes() / 1000u));
        set_text_if(g_think_lbl, b);
        return;
    }

    g_ai_think = false;
    show(g_think_lbl, false);
    show(g_think_bar, false);
    const Move m = search_best();
    if (move_null(m)) { check_game_end(); return; }
    play_move(m);
}

static void do_hint() {
    if (g_state != ST_PLAY || !g_running || g_ai_think || g_anim_on) return;
    const uint32_t now = esphome::millis();
    if (g_hint_ready_at && (int32_t)(now - g_hint_ready_at) < 0) {
        toast("Indice en recharge", 1500);
        return;
    }
    int sc = 0;
    const Move m = search_quick(g_pos, 2, 30, &sc);
    if (move_null(m)) return;
    g_hint_from = m.from;
    g_hint_to   = m.to;
    g_hint_until = now + HINT_SHOW_MS;
    g_hint_ready_at = now + HINT_CD_MS;
    draw_all();
}

// ===========================================================================
// 15. Evenements
// ===========================================================================

static void select_square(uint8_t sq) {
    g_sel_sq = sq;
    g_nfrom = 0;
    for (int i = 0; i < g_nall && g_nfrom < MAX_DOTS; i++)
        if (g_all_legal[i].from == sq) g_from_legal[g_nfrom++] = g_all_legal[i];
    if (g_nfrom == 0) g_sel_sq = NO_SQ;
    draw_all();
}

// Tente de jouer g_sel_sq -> to. Retourne true si la case etait une destination.
static bool try_move(uint8_t to) {
    int found = -1, count = 0;
    for (int i = 0; i < g_nfrom; i++)
        if (g_from_legal[i].to == to) { if (found < 0) found = i; count++; }
    if (found < 0) return false;

    // 4 coups pour la meme case d'arrivee = promotion : il faut demander la piece.
    if (count > 1 && (g_from_legal[found].flags & MF_PROMO)) {
        g_promo_from = g_sel_sq;
        g_promo_to   = to;
        show_promo();
        return true;
    }
    play_move(g_from_legal[found]);
    return true;
}

static void cell_event_cb(lv_event_t* e) {
    if (g_state != ST_PLAY || !g_running) return;
    if (g_anim_on || g_ai_think) return;
    if (g_mode == 2) return;                       // demo : plateau en lecture seule
    if (g_mode == 0 && g_pos.side != g_human) return;

    const int i = (int) (intptr_t) lv_event_get_user_data(e);
    const uint8_t sq = sq_of_cell(i);
    const uint8_t pc = g_pos.board[sq];

    if (g_sel_sq != NO_SQ) {
        if (sq == g_sel_sq) { g_sel_sq = NO_SQ; g_nfrom = 0; draw_all(); return; }
        if (try_move(sq)) return;
    }
    // Selection (ou re-selection) d'une piece du joueur au trait.
    if (pc && (pc & COLOR_MASK) == g_pos.side) select_square(sq);
    else if (g_sel_sq != NO_SQ) { g_sel_sq = NO_SQ; g_nfrom = 0; draw_all(); }
}

static void pbtn_event_cb(lv_event_t* e) {
    const int id = (int) (intptr_t) lv_event_get_user_data(e);
    if (g_state != ST_PLAY) return;
    switch (id) {
        case 0: undo_move(); break;
        case 1: do_hint(); break;
        case 2: show_pause(); break;
        default: break;
    }
}

static void hud_event_cb(lv_event_t*) {
    if (g_state == ST_PLAY) show_pause();
}

static void slot_event_cb(lv_event_t* e) {
    const int i = (int) (intptr_t) lv_event_get_user_data(e);

    switch (g_state) {
        case ST_HUB: {
            // La ligne « Reprendre » n'existe que s'il y a une partie sauvegardee :
            // on decale les index suivants en consequence.
            const int resume = g_save.resume_valid ? 1 : 0;
            if (i == 0) { go_setup(); }
            else if (resume && i == 1) {
                if (g_running) {
                    // Partie encore en memoire (meme session) : on revient dessus
                    // sans repasser par le FEN, ce qui preserve l'historique des
                    // coups (et donc « Annuler » et la triple repetition).
                    g_state = ST_PLAY;
                    menu_on(false);
                    g_clock_last = esphome::millis();
                    memset(g_drawn_pc, 0xFF, sizeof(g_drawn_pc));
                    memset(g_drawn_hl, 0xFF, sizeof(g_drawn_hl));
                    draw_all();
                    refresh_movelist();
                    update_hud(true);
                } else if (!resume_game()) {
                    go_hub();
                }
            }
            else if (i == 1 + resume) { go_stats(); }
            else if (i == 2 + resume) { g_settings_from = ST_HUB; go_settings(); }
            else if (i == 3 + resume) { close(); }
            break;
        }

        case ST_SETUP:
            if (i == 0) { g_save.mode = (uint8_t)((g_save.mode + 1) % 3); go_setup(); }
            else if (i == 1) { if (g_save.mode == 0) { g_save.human_side ^= 1; go_setup(); } }
            else if (i == 2) { if (g_save.mode != 1) { g_save.level = (uint8_t)((g_save.level + 1) % CHESS_NLEVELS); go_setup(); } }
            else if (i == 3) { g_save.clock_opt = (uint8_t)((g_save.clock_opt + 1) % 4); go_setup(); }
            else if (i == 4) { persist_save(); start_new_game(); }
            else if (i == 5) { go_hub(); }
            break;

        case ST_SETTINGS:
            if (i == 0) { g_save.gestures ^= 1; go_settings(); }
            else if (i == 1) { g_save.rule50 ^= 1; go_settings(); }
            else if (i == 2) { g_save.show_eval ^= 1; g_c_eval = 0x7FFFFFFF; go_settings(); }
            else if (i == 3) { g_save.demo_speed = (uint8_t)((g_save.demo_speed + 1) % 3); go_settings(); }
            else if (i == 4) { show_confirm(1, "EFFACER LES STATISTIQUES", "Bilan, records et classement local seront remis a zero."); }
            else if (i == 5) {
                persist_save();
                if (g_settings_from == ST_PAUSE) show_pause(); else go_hub();
            }
            break;

        case ST_STATS:
            if (i == 0) go_hub();
            break;

        case ST_CONFIRM:
            if (i == 0 && g_confirm_kind == 1) {
                const uint8_t lv = g_save.level, md = g_save.mode, hs = g_save.human_side;
                const uint8_t ck = g_save.clock_opt, ge = g_save.gestures, r5 = g_save.rule50;
                const uint8_t se = g_save.show_eval, ds = g_save.demo_speed;
                save_defaults();                 // remet aussi l'Elo a 1000
                g_save.level = lv; g_save.mode = md; g_save.human_side = hs;
                g_save.clock_opt = ck; g_save.gestures = ge; g_save.rule50 = r5;
                g_save.show_eval = se; g_save.demo_speed = ds;
                persist_save();
                go_settings();
            } else {
                go_settings();
            }
            break;

        case ST_PROMO: {
            static const uint8_t PROMO[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
            if (i < 0 || i > 3) break;
            for (int k = 0; k < g_nfrom; k++) {
                const Move& m = g_from_legal[k];
                if (m.from == g_promo_from && m.to == g_promo_to && m.promo == PROMO[i]) {
                    g_state = ST_PLAY;
                    menu_on(false);
                    play_move(m);
                    return;
                }
            }
            g_state = ST_PLAY;              // securite : coup introuvable
            menu_on(false);
            draw_all();
            break;
        }

        case ST_PAUSE:
            if (i == 0) { g_state = ST_PLAY; menu_on(false); g_clock_last = esphome::millis(); update_hud(true); }
            else if (i == 1) { if (g_nply > 0) { g_state = ST_PLAY; menu_on(false); g_clock_last = esphome::millis(); undo_move(); } }
            else if (i == 2) { offer_draw(); }
            else if (i == 3) {
                const uint8_t loser = (g_mode == 0) ? g_human : g_pos.side;
                end_game(loser == WHITE ? RES_BLACK : RES_WHITE, "Abandon");
            }
            else if (i == 4) { g_settings_from = ST_PAUSE; go_settings(); }
            else if (i == 5) { close(); }
            break;

        case ST_OVER:
            if (i == 0) start_new_game();
            else if (i == 1) go_hub();
            break;

        default: break;
    }
}

// ===========================================================================
// 16. Boucle de jeu
// ===========================================================================

static void update_clocks(uint32_t now) {
    const uint32_t dt = now - g_clock_last;
    g_clock_last = now;
    if (!g_clock_on || !g_running || g_state != ST_PLAY) return;
    const int s = cidx(g_pos.side);
    if (g_clock[s] <= dt) {
        g_clock[s] = 0;
        // Chute de pendule. Si l'adversaire ne peut plus mater, c'est nulle.
        if (insufficient_material(g_pos)) end_game(RES_DRAW, "Temps ecoule — materiel insuffisant pour mater");
        else end_game(g_pos.side == WHITE ? RES_BLACK : RES_WHITE, "Temps ecoule");
        return;
    }
    g_clock[s] -= dt;
}

static void tick_cb(lv_timer_t*) {
    const uint32_t now = esphome::millis();

    if (g_state != ST_PLAY) {
        // Hors partie, la pendule ne coule pas : on garde juste la reference a jour
        // pour ne pas accumuler un saut au retour.
        g_clock_last = now;
        return;
    }

    update_clocks(now);
    if (g_state != ST_PLAY) return;      // update_clocks a pu terminer la partie

    // Animation de deplacement (lerp adouci).
    if (g_anim_on) {
        const uint32_t dt = now - g_anim_t0;
        if (dt >= ANIM_MS) {
            g_anim_on = false;
            g_anim_hide = -1;
            show(g_anim, false);
            draw_cell(cell_of_sq(g_last_to), true);
        } else {
            float t = (float) dt / (float) ANIM_MS;
            t = t * t * (3.0f - 2.0f * t);          // smoothstep
            lv_obj_set_pos(g_anim,
                           g_anim_x0 + (int)((g_anim_x1 - g_anim_x0) * t),
                           g_anim_y0 + (int)((g_anim_y1 - g_anim_y0) * t));
        }
    }

    // Extinction de l'indice.
    if (g_hint_until && (int32_t)(now - g_hint_until) >= 0) {
        g_hint_until = 0;
        g_hint_from = g_hint_to = NO_SQ;
        draw_all();
    }

    // Tour du Tab : une tranche de reflexion par tick, jamais plus.
    if (g_running && !g_anim_on) {
        if (g_ai_think) {
            ai_step();
        } else if (side_is_ai()) {
            if (!g_ai_next_at) {
                // Temporisation : immediate contre un humain, reglable en demo.
                g_ai_next_at = now + (g_mode == 2 ? DEMO_DELAY[g_save.demo_speed] : 120u);
            } else if ((int32_t)(now - g_ai_next_at) >= 0) {
                g_ai_next_at = 0;
                ai_begin();
            }
        } else {
            g_ai_next_at = 0;
        }
    }

    update_hud(false);
}

// ===========================================================================
// 17. API publique
// ===========================================================================

void on_imu(float ax, float ay, float az) {
    if (g_state == ST_OFF || !g_save.gestures) return;
    if (ax != ax || ay != ay || az != az) return;             // garde NaN
    const float mag = sqrtf(ax * ax + ay * ay + az * az);
    // Filtre passe-bas leger : evite qu'une seule lecture bruitee declenche.
    g_shake_mag += (mag - g_shake_mag) * 0.5f;
    const uint32_t now = esphome::millis();
    if (g_shake_mag > 1.9f && (now - g_shake_last) > SHAKE_CD_MS) {
        g_shake_last = now;
        do_hint();
    }
}

bool is_open() { return g_state != ST_OFF; }

void perft_log(int depth) { perft_selftest(depth); }

void open(const UI& ui) {
    if (g_state != ST_OFF) return;
    if (!ui.root || !ui.hud || !ui.board || !ui.panel) return;
    g_ui = ui;

    init();                 // tables du moteur (Zobrist, masques de roque)
    persist_load();
    build_ui();

    // Position neutre affichee derriere le hub tant qu'aucune partie n'a demarre.
    if (g_nply == 0 && !g_running) {
        set_start(g_pos);
        g_hist_hash[0] = hash_of(g_pos);
        recompute_legal();
    }

    // La page LVGL est déjà active (navigation via lvgl.page.show dans le YAML).
    memset(g_drawn_pc, 0xFF, sizeof(g_drawn_pc));
    memset(g_drawn_hl, 0xFF, sizeof(g_drawn_hl));
    draw_all();
    refresh_movelist();
    update_hud(true);

    g_clock_last = esphome::millis();
    go_hub();

    if (!g_timer) g_timer = lv_timer_create(tick_cb, TICK_MS, nullptr);
}

void close() {
    if (g_state == ST_OFF) return;

    // Une partie en cours est sauvegardee pour pouvoir etre reprise au prochain
    // lancement (position + pendules), y compris apres un reboot.
    store_running_game();
    if (g_game_t0) { g_save.total_ms += esphome::millis() - g_game_t0; g_game_t0 = 0; }
    persist_save();

    if (g_timer) { lv_timer_delete(g_timer); g_timer = nullptr; }
    g_ai_think = false;
    g_anim_on = false;
    // Navigation retour vers le sélecteur arcade (page LVGL).
    if (g_ui.lvgl) g_ui.lvgl->show_page(g_ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
    g_state = ST_OFF;
}

}  // namespace Chess
