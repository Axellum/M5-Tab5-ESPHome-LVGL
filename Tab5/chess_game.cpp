/**
 * [AI-CONTEXT]
 * @file chess_game.cpp
 * @role Jeu « Roi Noir » — UI LVGL, machine a etats, persistance NVS.
 * @architecture_constraint Plein ecran 1280x720. Le YAML ne fournit que 4
 *      conteneurs vides + 3 polices ; tout le reste est construit ici. Les objets
 *      LVGL sont PREALLOUES a l'ouverture (64 cases, 64 pastilles de piece, 28
 *      marqueurs de coup) puis reutilises par show/hide + set_style : aucune
 *      allocation LVGL en cours de partie. Jeu ferme : objets detruits, interface
 *      rendue ; seule une partie EN COURS garde son etat (section 3). Le MODELE (chess_ai.cpp) est totalement
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
#include "game_common.h"
#include "chess_ai.h"
#include "esphome/core/preferences.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "esp_attr.h"
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
// rasteriseur que celui d'ESPHome) — voir docs/arcade.md, section « Roi Noir ».
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
// Memoire (audit du 26/09/2026, lot 4, choix d'Axel) : jeu ferme, rien ne reste
// reserve SAUF une partie en cours, gardee telle quelle pour la reprendre a
// l'identique (position, historique, pendules, coups legaux : « Annuler » et la
// triple repetition voient toute la partie). Elle tient dans deux blocs : Play
// (lu a chaque tick, RAM interne) et PlayHist (~10 Ko, lu une fois par coup,
// PSRAM). L'interface et ses brouillons (Mem, section 4) sont rendus a chaque
// fermeture. Survivent en plus, hors blocs : g_state, le filtre IMU et g_nvs.

// Sauvegarde différée de la partie en cours (même principe que le Go : drapeau +
// au plus une écriture NVS toutes les 15 s). Avant le 25/09/2026, la partie n'était
// écrite qu'à la fermeture de la console : un reboot (OTA, watchdog API, coupure)
// la perdait, ou proposait une partie plus ancienne encore marquée reprenable —
// alors que le README promet la reprise après redémarrage.
static constexpr uint32_t RESUME_SAVE_MIN_MS = 15000;

// Historique : lu et ecrit une fois par coup joue, jamais par la recherche.
struct PlayHist {
    Move     hist_move[MAX_HIST];
    Undo     hist_undo[MAX_HIST];
    uint64_t hist_hash[MAX_HIST + 1];
    char     hist_san[MAX_HIST][12];
    uint16_t hist_num[MAX_HIST];    // numero de coup au moment du demi-coup
    uint8_t  hist_side[MAX_HIST];   // trait au moment du demi-coup
};

struct Play {
    Position pos;
    int      nply = 0;

    uint8_t  mode       = 0;        // 0 = vs Tab, 1 = hotseat, 2 = demo
    uint8_t  human      = WHITE;    // couleur de l'humain (mode 0)
    uint8_t  level      = 2;
    bool     running    = false;    // partie en cours (pendules actives)
    bool     flip       = false;    // plateau vu du cote noir
    uint32_t clock[2]   = {0, 0};
    uint32_t inc_ms     = 0;
    bool     clock_on   = false;
    uint32_t game_t0    = 0;
    bool     resume_dirty     = false;
    uint32_t resume_saved_at  = 0;

    uint8_t sel_sq    = NO_SQ;      // case selectionnee
    uint8_t last_from = NO_SQ;      // dernier coup joue (encadrement)
    uint8_t last_to   = NO_SQ;
    uint8_t check_sq  = NO_SQ;      // roi en echec

    Move all_legal[MAX_MOVES];      // coups legaux de la position courante
    int  nall = 0;
    Move from_legal[MAX_DOTS];      // coups partant de sel_sq
    int  nfrom = 0;

    // Indice
    uint8_t  hint_from = NO_SQ, hint_to = NO_SQ;
    uint32_t hint_until = 0, hint_ready_at = 0;

    // IA
    uint32_t ai_next_at = 0;        // temporisation avant de lancer la reflexion

    // Message transitoire affiche a droite du HUD (refus de nulle, indice en
    // recharge...). Prioritaire sur « ECHEC ! » pendant sa duree de vie.
    char     msg[48] = "";
    uint32_t msg_until = 0;
};
static Play*     gp = nullptr;
static PlayHist* gh = nullptr;

// IMU (survit : filtre et anti-rebond de la secousse, quelques octets)
static float    g_shake_mag  = 0.0f;
static uint32_t g_shake_last = 0;

// ===========================================================================
// 4. Objets LVGL et etat d'ecran (a chaque ouverture)
// ===========================================================================

// Ligne de la liste des coups (brouillon de refresh_movelist).
struct MoveRow { uint16_t num; char w[12]; char b[12]; };

struct Mem {
    ChessSave save;                 // relue de la NVS a chaque ouverture
    UI  ui;
    lv_timer_t* timer = nullptr;

    // Etat d'ecran : repart de ces valeurs a chaque ouverture (close() remettait
    // deja l'IA et l'animation a l'arret ; la piece masquee d'une animation
    // interrompue ne reste plus cachee a la reouverture).
    uint32_t clock_last = 0;
    uint8_t promo_from = NO_SQ, promo_to = NO_SQ;
    // Fin de partie
    uint8_t     result = RES_DRAW;
    const char* reason = "";
    bool     ai_think   = false;
    // Animation de deplacement
    bool     anim_on   = false;
    uint32_t anim_t0   = 0;
    int      anim_x0 = 0, anim_y0 = 0, anim_x1 = 0, anim_y1 = 0;
    int      anim_hide = -1;        // index de case dont la piece est masquee
    // Ecran de confirmation generique (reutilise pour « effacer les stats »).
    uint8_t confirm_kind = 0;
    // D'ou l'ecran de reglages a ete ouvert (hub ou pause) — pour y revenir.
    GState settings_from = ST_HUB;

    lv_obj_t* menu = nullptr;       // calque plein ecran des menus (cree en C++)
    lv_obj_t* cell[64]  = {};
    lv_obj_t* body[64]  = {};       // calque « corps »   (glyphe plein)
    lv_obj_t* edge[64]  = {};       // calque « contour » (pieces blanches)
    lv_obj_t* dot[MAX_DOTS] = {};
    lv_obj_t* coord_f[8] = {};      // lettres de colonne
    lv_obj_t* coord_r[8] = {};      // chiffres de rangee
    lv_obj_t* anim = nullptr;       // conteneur de la piece en vol
    lv_obj_t* anim_body = nullptr;
    lv_obj_t* anim_edge = nullptr;

    // HUD
    lv_obj_t* h_turn = nullptr;
    lv_obj_t* h_level = nullptr;
    lv_obj_t* h_clock_w = nullptr;
    lv_obj_t* h_clock_b = nullptr;
    lv_obj_t* h_eval = nullptr;
    lv_obj_t* h_status = nullptr;

    // Panneau lateral
    lv_obj_t* p_title = nullptr;
    lv_obj_t* ml_num[MOVE_ROWS] = {};
    lv_obj_t* ml_w[MOVE_ROWS]   = {};
    lv_obj_t* ml_b[MOVE_ROWS]   = {};
    lv_obj_t* think_lbl = nullptr;
    lv_obj_t* think_bar = nullptr;
    lv_obj_t* think_fill = nullptr;
    lv_obj_t* pbtn[3] = {};
    lv_obj_t* pbtn_lbl[3] = {};

    // Menus
    lv_obj_t* m_title = nullptr;
    lv_obj_t* m_sub   = nullptr;
    lv_obj_t* m_body  = nullptr;
    lv_obj_t* m_foot  = nullptr;
    lv_obj_t* slot[N_SLOTS]   = {};
    lv_obj_t* slot_t[N_SLOTS] = {};
    lv_obj_t* slot_d[N_SLOTS] = {};
    lv_obj_t* promo_name[4]   = {};   // libelles sous les tuiles de promotion

    // Caches de rendu : une case n'est restylee que si son contenu a change.
    uint8_t drawn_pc[64];
    uint8_t drawn_hl[64];
    int     c_eval = 0x7FFFFFFF;
    int     c_clock[2] = {-1, -1};

    MoveRow movelist_rows[MOVE_ROWS];
};
static Mem* gs = nullptr;

// ===========================================================================
// 5. Persistance NVS
// ===========================================================================

static NvsSlot<ChessSave> g_nvs(PREF_KEY, SAVE_MAGIC);

static void save_defaults() {
    memset(&gs->save, 0, sizeof(gs->save));
    gs->save.magic      = SAVE_MAGIC;
    gs->save.elo        = 1000;
    gs->save.level      = 2;        // Fou : le niveau « par defaut jouable »
    gs->save.mode       = 0;
    gs->save.human_side = 0;
    gs->save.clock_opt  = 0;
    gs->save.gestures   = 1;
    gs->save.rule50     = 1;
    gs->save.show_eval  = 1;
    gs->save.demo_speed = 1;
}

void persist_load() {
    if (!gs) return;  // la sauvegarde n'est en memoire que jeu ouvert
    if (!g_nvs.load(gs->save)) save_defaults();
    if (gs->save.level >= CHESS_NLEVELS) gs->save.level = 2;
    if (gs->save.mode > 2) gs->save.mode = 0;
    if (gs->save.clock_opt > 3) gs->save.clock_opt = 0;
    if (gs->save.demo_speed > 2) gs->save.demo_speed = 1;
    if (gs->save.elo < 100 || gs->save.elo > 3000) gs->save.elo = 1000;
}

void persist_save() {
    if (!gs || !g_nvs.ready()) return;
    g_nvs.save(gs->save);
}

// ===========================================================================
// 6. Helpers LVGL
// ===========================================================================







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
    const int col = gp->flip ? (7 - f) : f;
    const int row = gp->flip ? r : (7 - r);
    return row * 8 + col;
}

static inline uint8_t sq_of_cell(int i) {
    const int row = i >> 3, col = i & 7;
    const int f = gp->flip ? (7 - col) : col;
    const int r = gp->flip ? row : (7 - row);
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
// 9. Construction de l'UI (a chaque ouverture)
// ===========================================================================

static void cell_event_cb(lv_event_t* e);
static void slot_event_cb(lv_event_t* e);
static void pbtn_event_cb(lv_event_t* e);
static void hud_event_cb(lv_event_t* e);

static void build_ui() {
    // --- Fonds des conteneurs fournis par le YAML (aucune couleur dans le YAML) --
    set_bg(gs->ui.root,  Pal::VOID_BG,  LV_OPA_COVER);
    set_bg(gs->ui.hud,   Pal::HUD_BG,   LV_OPA_COVER);
    set_bg(gs->ui.panel, Pal::PANEL_BG, LV_OPA_COVER);
    lv_obj_set_style_radius(gs->ui.panel, 12, LV_PART_MAIN);
    set_bg(gs->ui.board, Pal::SQ_DARK, LV_OPA_COVER);

    // Liseré sombre autour du plateau : le damier creme/vert « flotte » sinon
    // sur le fond anthracite, et l'oeil ne trouve pas le bord du a1.
    set_border(gs->ui.board, Pal::BOARD_EDGE, 3, LV_OPA_COVER);

    // --- 64 cases, chacune portant ses deux calques de piece --------------
    for (int i = 0; i < 64; i++) {
        gs->cell[i] = mk_rect(gs->ui.board);
        lv_obj_set_size(gs->cell[i], CELL, CELL);
        lv_obj_set_pos(gs->cell[i], (i & 7) * CELL, (i >> 3) * CELL);
        lv_obj_add_flag(gs->cell[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(gs->cell[i], cell_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);

        // Corps puis contour : l'ordre de creation EST l'ordre de dessin, le
        // contour doit passer par-dessus le corps ivoire.
        gs->body[i] = mk_label(gs->cell[i], gs->ui.f_piece, Pal::PC_B_FILL);
        lv_obj_align(gs->body[i], LV_ALIGN_CENTER, 0, PIECE_DY);
        lv_obj_add_flag(gs->body[i], LV_OBJ_FLAG_HIDDEN);

        gs->edge[i] = mk_label(gs->cell[i], gs->ui.f_piece, Pal::PC_EDGE);
        lv_obj_align(gs->edge[i], LV_ALIGN_CENTER, 0, PIECE_DY);
        lv_obj_add_flag(gs->edge[i], LV_OBJ_FLAG_HIDDEN);
    }

    // --- Coordonnees (a-h / 1-8) facon livre d'echecs ---------------------
    // Posees DANS les cases de bord et teintees de la couleur de la case
    // opposee : lisibles sans voler de place au damier.
    for (int i = 0; i < 8; i++) {
        gs->coord_f[i] = mk_label(gs->ui.board, gs->ui.f_small, Pal::TXT_MUTED);
        lv_obj_set_pos(gs->coord_f[i], i * CELL + CELL - 18, BOARD_SZ - 27);
        gs->coord_r[i] = mk_label(gs->ui.board, gs->ui.f_small, Pal::TXT_MUTED);
        lv_obj_set_pos(gs->coord_r[i], 6, i * CELL + 3);
        // La rangee du bas est la ligne 7, la colonne de gauche la colonne 0 :
        // la parite de la case ne depend pas de l'orientation du plateau, ces
        // couleurs sont donc posees une fois pour toutes.
        set_color(gs->coord_f[i], ((7 + i) & 1) == 0 ? Pal::SQ_DARK : Pal::SQ_LIGHT);
        set_color(gs->coord_r[i], (i & 1) == 0 ? Pal::SQ_DARK : Pal::SQ_LIGHT);
    }

    // --- Marqueurs de coups legaux (au-dessus des cases) -------------------
    for (int i = 0; i < MAX_DOTS; i++) {
        gs->dot[i] = mk_rect(gs->ui.board);
        lv_obj_set_style_radius(gs->dot[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_add_flag(gs->dot[i], LV_OBJ_FLAG_HIDDEN);
    }

    // --- Piece en vol (creee en dernier = dessinee au-dessus de tout) ------
    // Conteneur transparent de la taille d'une case : il porte les deux memes
    // calques que les cases, et c'est LUI qu'on deplace pendant l'animation.
    gs->anim = mk_rect(gs->ui.board);
    lv_obj_set_size(gs->anim, CELL, CELL);
    lv_obj_set_style_bg_opa(gs->anim, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_add_flag(gs->anim, LV_OBJ_FLAG_HIDDEN);
    gs->anim_body = mk_label(gs->anim, gs->ui.f_piece, Pal::PC_B_FILL);
    lv_obj_align(gs->anim_body, LV_ALIGN_CENTER, 0, PIECE_DY);
    gs->anim_edge = mk_label(gs->anim, gs->ui.f_piece, Pal::PC_EDGE);
    lv_obj_align(gs->anim_edge, LV_ALIGN_CENTER, 0, PIECE_DY);

    // --- HUD ---------------------------------------------------------------
    gs->h_turn = mk_label(gs->ui.hud, gs->ui.f_small, Pal::TXT);
    lv_obj_align(gs->h_turn, LV_ALIGN_LEFT_MID, 16, 0);
    gs->h_level = mk_label(gs->ui.hud, gs->ui.f_small, Pal::TXT_DIM);
    lv_obj_align(gs->h_level, LV_ALIGN_LEFT_MID, 300, 0);
    gs->h_clock_w = mk_label(gs->ui.hud, gs->ui.f_small, Pal::TXT);
    lv_obj_align(gs->h_clock_w, LV_ALIGN_LEFT_MID, 560, 0);
    gs->h_clock_b = mk_label(gs->ui.hud, gs->ui.f_small, Pal::TXT);
    lv_obj_align(gs->h_clock_b, LV_ALIGN_LEFT_MID, 700, 0);
    gs->h_eval = mk_label(gs->ui.hud, gs->ui.f_small, Pal::TXT_DIM);
    lv_obj_align(gs->h_eval, LV_ALIGN_LEFT_MID, 860, 0);
    gs->h_status = mk_label(gs->ui.hud, gs->ui.f_small, Pal::DANGER);
    lv_obj_align(gs->h_status, LV_ALIGN_RIGHT_MID, -16, 0);

    lv_obj_add_flag(gs->ui.hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gs->ui.hud, hud_event_cb, LV_EVENT_CLICKED, nullptr);

    // --- Panneau lateral ---------------------------------------------------
    gs->p_title = mk_label(gs->ui.panel, gs->ui.f_small, Pal::ACCENT);
    lv_obj_set_pos(gs->p_title, 24, 18);
    set_text_if(gs->p_title, "PARTIE");

    for (int i = 0; i < MOVE_ROWS; i++) {
        const int y = 66 + i * 32;
        gs->ml_num[i] = mk_label(gs->ui.panel, gs->ui.f_small, Pal::TXT_MUTED);
        lv_obj_set_pos(gs->ml_num[i], 24, y);
        gs->ml_w[i] = mk_label(gs->ui.panel, gs->ui.f_small, Pal::TXT);
        lv_obj_set_pos(gs->ml_w[i], 96, y);
        gs->ml_b[i] = mk_label(gs->ui.panel, gs->ui.f_small, Pal::TXT_DIM);
        lv_obj_set_pos(gs->ml_b[i], 300, y);
    }

    gs->think_lbl = mk_label(gs->ui.panel, gs->ui.f_small, Pal::THINK);
    lv_obj_set_pos(gs->think_lbl, 24, 524);
    gs->think_bar = mk_rect(gs->ui.panel);
    lv_obj_set_size(gs->think_bar, 504, 8);
    lv_obj_set_pos(gs->think_bar, 24, 556);
    lv_obj_set_style_radius(gs->think_bar, 4, LV_PART_MAIN);
    set_bg(gs->think_bar, Pal::BTN_BG, LV_OPA_COVER);
    gs->think_fill = mk_rect(gs->think_bar);
    lv_obj_set_size(gs->think_fill, 0, 8);
    lv_obj_set_pos(gs->think_fill, 0, 0);
    lv_obj_set_style_radius(gs->think_fill, 4, LV_PART_MAIN);
    set_bg(gs->think_fill, Pal::THINK, LV_OPA_COVER);
    show(gs->think_lbl, false);
    show(gs->think_bar, false);

    static const char* const BTN[3] = {"Annuler", "Indice", "Menu"};
    for (int i = 0; i < 3; i++) {
        gs->pbtn[i] = mk_rect(gs->ui.panel);
        lv_obj_set_size(gs->pbtn[i], 168, 64);
        lv_obj_set_pos(gs->pbtn[i], 12 + i * 180, 590);
        lv_obj_set_style_radius(gs->pbtn[i], 12, LV_PART_MAIN);
        set_bg(gs->pbtn[i], Pal::BTN_BG, LV_OPA_COVER);
        set_border(gs->pbtn[i], Pal::ACCENT, 2, LV_OPA_40);
        lv_obj_add_flag(gs->pbtn[i], LV_OBJ_FLAG_CLICKABLE);
        // Retour tactile. Casts explicites : combiner lv_part_t et lv_state_t
        // directement est deprecie en C++20 (-Wdeprecated-enum-enum-conversion).
        lv_obj_set_style_bg_color(gs->pbtn[i], lv_color_hex(Pal::BTN_BG_ON),
                                  (lv_style_selector_t) LV_PART_MAIN |
                                  (lv_style_selector_t) LV_STATE_PRESSED);
        lv_obj_add_event_cb(gs->pbtn[i], pbtn_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        gs->pbtn_lbl[i] = mk_label(gs->pbtn[i], gs->ui.f_small, Pal::TXT);
        lv_obj_align(gs->pbtn_lbl[i], LV_ALIGN_CENTER, 0, 0);
        set_text_if(gs->pbtn_lbl[i], BTN[i]);
    }

    // --- Calque des menus, cree ICI (le YAML n'a pas a le connaitre) --------
    gs->menu = mk_rect(gs->ui.root);
    lv_obj_set_size(gs->menu, 1280, 720);
    lv_obj_set_pos(gs->menu, 0, 0);
    set_bg(gs->menu, Pal::VOID_BG, LV_OPA_COVER);
    lv_obj_add_flag(gs->menu, LV_OBJ_FLAG_CLICKABLE);   // absorbe les taps vers le plateau
    lv_obj_add_flag(gs->menu, LV_OBJ_FLAG_HIDDEN);

    gs->m_title = mk_label(gs->menu, gs->ui.f_big, Pal::ACCENT);
    lv_obj_align(gs->m_title, LV_ALIGN_TOP_MID, 0, 46);
    gs->m_sub = mk_label(gs->menu, gs->ui.f_small, Pal::TXT_DIM);
    lv_obj_align(gs->m_sub, LV_ALIGN_TOP_MID, 0, 106);
    gs->m_body = mk_label(gs->menu, gs->ui.f_small, Pal::TXT);
    lv_obj_set_width(gs->m_body, 900);
    lv_obj_set_style_text_align(gs->m_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(gs->m_body, LV_ALIGN_TOP_MID, 0, 148);
    gs->m_foot = mk_label(gs->menu, gs->ui.f_small, Pal::TXT_MUTED);
    lv_obj_align(gs->m_foot, LV_ALIGN_BOTTOM_MID, 0, -20);

    for (int i = 0; i < N_SLOTS; i++) {
        gs->slot[i] = mk_rect(gs->menu);
        lv_obj_add_flag(gs->slot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(gs->slot[i], 14, LV_PART_MAIN);
        set_bg(gs->slot[i], Pal::BTN_BG, LV_OPA_COVER);
        lv_obj_set_style_bg_color(gs->slot[i], lv_color_hex(Pal::BTN_BG_ON),
                                  (lv_style_selector_t) LV_PART_MAIN |
                                  (lv_style_selector_t) LV_STATE_PRESSED);
        lv_obj_add_event_cb(gs->slot[i], slot_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        gs->slot_t[i] = mk_label(gs->slot[i], gs->ui.f_mid, Pal::TXT);
        gs->slot_d[i] = mk_label(gs->slot[i], gs->ui.f_small, Pal::TXT_DIM);
        lv_obj_add_flag(gs->slot[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Libelles des 4 tuiles de promotion : centres SOUS chaque tuile, donc
    // alignes avec elles (un seul libelle en pied de page ne pourrait pas l'etre
    // avec une police proportionnelle).
    static const char* const PROMO_NAME[4] = {"Dame", "Tour", "Fou", "Cavalier"};
    for (int i = 0; i < 4; i++) {
        gs->promo_name[i] = mk_label(gs->menu, gs->ui.f_small, Pal::TXT_DIM);
        lv_obj_set_width(gs->promo_name[i], 168);
        lv_obj_set_style_text_align(gs->promo_name[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(gs->promo_name[i], LV_ALIGN_TOP_LEFT, 220 + i * 220, 484);
        set_text_if(gs->promo_name[i], PROMO_NAME[i]);
        lv_obj_add_flag(gs->promo_name[i], LV_OBJ_FLAG_HIDDEN);
    }

    memset(gs->drawn_pc, 0xFF, sizeof(gs->drawn_pc));
    memset(gs->drawn_hl, 0xFF, sizeof(gs->drawn_hl));
}

// Les libelles de promotion ne doivent apparaitre que sur cet ecran.
static void promo_names(bool v) {
    for (int i = 0; i < 4; i++) show(gs->promo_name[i], v);
}

// Remet une entree de menu dans son habillage par defaut. Necessaire parce que
// l'ecran de promotion detourne les memes objets en tuiles d'echiquier (fond
// creme, police des figurines) : sans ce reset, le menu suivant en heriterait.
static void slot_reset(int i) {
    set_bg(gs->slot[i], Pal::BTN_BG, LV_OPA_COVER);
    esphome::lvgl::lv_obj_set_style_text_font(gs->slot_t[i], gs->ui.f_mid, LV_PART_MAIN);
    esphome::lvgl::lv_obj_set_style_text_font(gs->slot_d[i], gs->ui.f_small, LV_PART_MAIN);
    set_color(gs->slot_d[i], Pal::TXT_DIM);
    show(gs->slot_d[i], true);
}

// --- Mise en page des entrees de menu --------------------------------------
static void slot_list(int i, const char* title, const char* desc, uint32_t col, bool on) {
    if (i < 0 || i >= N_SLOTS) return;
    slot_reset(i);
    lv_obj_set_size(gs->slot[i], 720, 62);
    lv_obj_align(gs->slot[i], LV_ALIGN_TOP_MID, 0, 210 + i * 70);
    lv_obj_set_width(gs->slot_t[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(gs->slot_t[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(gs->slot_d[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(gs->slot_d[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(gs->slot_t[i], LV_ALIGN_LEFT_MID, 24, (desc && desc[0]) ? -13 : 0);
    lv_obj_align(gs->slot_d[i], LV_ALIGN_LEFT_MID, 24, 15);
    set_color(gs->slot_t[i], on ? col : Pal::TXT_MUTED);
    set_text_if(gs->slot_t[i], title);
    set_text_if(gs->slot_d[i], desc ? desc : "");
    set_border(gs->slot[i], on ? col : Pal::TXT_MUTED, 2, LV_OPA_50);
    show(gs->slot[i], true);
}

// Tuile de promotion : une vraie case d'echiquier portant la figurine, rendue
// EXACTEMENT comme sur le plateau (corps plein + contour pour les blancs). On
// detourne gs->slot_t comme calque « corps » et gs->slot_d comme calque « contour ».
static void slot_promo(int i, uint8_t type, bool white) {
    if (i < 0 || i >= N_SLOTS) return;
    lv_obj_set_size(gs->slot[i], 168, 168);
    lv_obj_align(gs->slot[i], LV_ALIGN_TOP_LEFT, 220 + i * 220, 300);
    lv_obj_set_style_radius(gs->slot[i], 10, LV_PART_MAIN);
    // Alternance creme / vert : les 4 tuiles ressemblent a une rangee du damier.
    set_bg(gs->slot[i], (i & 1) ? Pal::SQ_DARK : Pal::SQ_LIGHT, LV_OPA_COVER);
    set_border(gs->slot[i], Pal::ACCENT, 3, LV_OPA_80);

    char g[4];
    esphome::lvgl::lv_obj_set_style_text_font(gs->slot_t[i], gs->ui.f_piece, LV_PART_MAIN);
    lv_obj_set_width(gs->slot_t[i], LV_SIZE_CONTENT);
    lv_obj_align(gs->slot_t[i], LV_ALIGN_CENTER, 0, PIECE_DY);
    piece_utf8(type, false, g);
    set_text_if(gs->slot_t[i], g);
    set_color(gs->slot_t[i], white ? Pal::PC_W_FILL : Pal::PC_B_FILL);

    esphome::lvgl::lv_obj_set_style_text_font(gs->slot_d[i], gs->ui.f_piece, LV_PART_MAIN);
    lv_obj_set_width(gs->slot_d[i], LV_SIZE_CONTENT);
    lv_obj_align(gs->slot_d[i], LV_ALIGN_CENTER, 0, PIECE_DY);
    if (white) {
        piece_utf8(type, true, g);
        set_text_if(gs->slot_d[i], g);
        set_color(gs->slot_d[i], Pal::PC_EDGE);
    }
    show(gs->slot_d[i], white);
    show(gs->slot[i], true);
}

static void slots_hide_from(int n) {
    for (int i = n; i < N_SLOTS; i++) show(gs->slot[i], false);
    promo_names(false);          // par defaut : masques, show_promo() les rallume
}

static void menu_on(bool v, lv_opa_t opa = LV_OPA_COVER) {
    if (!gs->menu) return;
    set_bg(gs->menu, Pal::VOID_BG, opa);
    show(gs->menu, v);
    if (v) lv_obj_move_foreground(gs->menu);
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
    if (gp->hint_until && (sq == gp->hint_from || sq == gp->hint_to)) return HL_HINT;
    if (gp->sel_sq != NO_SQ && sq == gp->sel_sq) return HL_SEL;
    if (gp->check_sq != NO_SQ && sq == gp->check_sq) return HL_CHECK;
    if (gp->last_from != NO_SQ && (sq == gp->last_from || sq == gp->last_to)) return HL_LAST;
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
    uint8_t pc = gp->pos.board[sq];
    if (i == gs->anim_hide) pc = EMPTY;          // piece en vol : masquee a l'arrivee
    const uint8_t hl = hl_of_cell(i);

    if (force || hl != gs->drawn_hl[i]) {
        set_bg(gs->cell[i], cell_color(i, hl), LV_OPA_COVER);
        gs->drawn_hl[i] = hl;
    }
    if (!force && pc == gs->drawn_pc[i]) return;
    gs->drawn_pc[i] = pc;

    if (!pc) { show(gs->body[i], false); show(gs->edge[i], false); return; }

    const bool white = (pc & COLOR_MASK) == WHITE;
    char g[4];
    piece_utf8(pc, false, g);                 // corps : toujours le glyphe plein
    set_text_if(gs->body[i], g);
    set_color(gs->body[i], white ? Pal::PC_W_FILL : Pal::PC_B_FILL);
    show(gs->body[i], true);

    // Contour uniquement sur les pieces claires : sans lui, un corps ivoire
    // disparaitrait sur une case creme.
    if (white) {
        piece_utf8(pc, true, g);
        set_text_if(gs->edge[i], g);
        show(gs->edge[i], true);
    } else {
        show(gs->edge[i], false);
    }
}

// Marqueurs des destinations legales depuis la case selectionnee.
static void draw_dots() {
    int n = 0;
    if (gp->sel_sq != NO_SQ) {
        for (int i = 0; i < gp->nfrom && n < MAX_DOTS; i++) {
            const uint8_t to = gp->from_legal[i].to;
            // Les 4 promotions partagent la meme case d'arrivee : un seul marqueur.
            bool dup = false;
            for (int j = 0; j < i; j++) if (gp->from_legal[j].to == to) { dup = true; break; }
            if (dup) continue;
            const int c = cell_of_sq(to);
            const bool cap = (gp->from_legal[i].flags & MF_CAPTURE) != 0;
            const int sz = cap ? 74 : 26;
            lv_obj_set_size(gs->dot[n], sz, sz);
            lv_obj_set_pos(gs->dot[n], (c & 7) * CELL + (CELL - sz) / 2,
                                     (c >> 3) * CELL + (CELL - sz) / 2);
            if (cap) {
                // Capture : anneau autour de la piece (pas de pastille pleine,
                // sinon on ne verrait plus quelle piece on va prendre).
                set_bg(gs->dot[n], Pal::DOT_CAPTURE, LV_OPA_TRANSP);
                set_border(gs->dot[n], Pal::DOT_CAPTURE, 5, LV_OPA_90);
            } else {
                set_bg(gs->dot[n], Pal::DOT_MOVE, LV_OPA_70);
                set_border(gs->dot[n], Pal::DOT_MOVE, 0, LV_OPA_TRANSP);
            }
            show(gs->dot[n], true);
            n++;
        }
    }
    for (int i = n; i < MAX_DOTS; i++) show(gs->dot[i], false);
}

static void draw_coords() {
    for (int i = 0; i < 8; i++) {
        char c[2];
        c[1] = 0;
        c[0] = (char)('a' + (gp->flip ? (7 - i) : i));
        set_text_if(gs->coord_f[i], c);
        c[0] = (char)('1' + (gp->flip ? i : (7 - i)));
        set_text_if(gs->coord_r[i], c);
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
    strncpy(gp->msg, t, sizeof(gp->msg) - 1);
    gp->msg[sizeof(gp->msg) - 1] = 0;
    gp->msg_until = esphome::millis() + ms;
}

static void fmt_clock(uint32_t ms, char* out, int cap) {
    const uint32_t s = ms / 1000;
    if (s >= 3600) snprintf(out, cap, "%u:%02u:%02u", (unsigned)(s / 3600), (unsigned)((s / 60) % 60), (unsigned)(s % 60));
    else           snprintf(out, cap, "%u:%02u", (unsigned)(s / 60), (unsigned)(s % 60));
}

static void update_hud(bool force) {
    char buf[48];

    // Trait
    const bool white_turn = (gp->pos.side == WHITE);
    if (gp->mode == 1) snprintf(buf, sizeof(buf), "Trait aux %s", white_turn ? "Blancs" : "Noirs");
    else if (gp->mode == 2) snprintf(buf, sizeof(buf), "Demo — %s", white_turn ? "Blancs" : "Noirs");
    else snprintf(buf, sizeof(buf), "Trait : %s", (gp->pos.side == gp->human) ? "a vous" : "au Tab");
    set_text_if(gs->h_turn, buf);
    set_color(gs->h_turn, (gp->mode == 0 && gp->pos.side == gp->human) ? Pal::ACCENT : Pal::TXT);

    // Niveau d'IA (sans objet en hotseat)
    if (gp->mode == 1) snprintf(buf, sizeof(buf), "Duel local");
    else snprintf(buf, sizeof(buf), "Niveau : %s", AI_LEVELS[gp->level].name);
    set_text_if(gs->h_level, buf);

    // Pendules
    if (gp->clock_on) {
        for (int c = 0; c < 2; c++) {
            const int sec = (int)(gp->clock[c] / 1000);
            if (force || sec != gs->c_clock[c]) {
                gs->c_clock[c] = sec;
                char t[16], line[32];
                fmt_clock(gp->clock[c], t, sizeof(t));
                snprintf(line, sizeof(line), "%s %s", c == 0 ? "B" : "N", t);
                set_text_if(c == 0 ? gs->h_clock_w : gs->h_clock_b, line);
                lv_obj_t* lab = (c == 0) ? gs->h_clock_w : gs->h_clock_b;
                const bool active = gp->running && (cidx(gp->pos.side) == c);
                set_color(lab, gp->clock[c] < 30000 ? Pal::DANGER : (active ? Pal::TXT : Pal::TXT_DIM));
            }
        }
    } else if (force) {
        set_text_if(gs->h_clock_w, "");
        set_text_if(gs->h_clock_b, "");
    }

    // Evaluation approximative (statique, du point de vue des blancs).
    if (gs->save.show_eval) {
        int e = eval(gp->pos);
        if (gp->pos.side == BLACK) e = -e;
        if (force || e / 5 != gs->c_eval / 5) {          // pas de clignotement pour 4 centiemes
            gs->c_eval = e;
            snprintf(buf, sizeof(buf), "Eval %+.1f", e / 100.0f);
            set_text_if(gs->h_eval, buf);
            set_color(gs->h_eval, e > 80 ? Pal::GOOD : (e < -80 ? Pal::DANGER : Pal::TXT_DIM));
        }
    } else if (force) {
        set_text_if(gs->h_eval, "");
    }

    // Statut : message transitoire, sinon echec, sinon reflexion, sinon rien.
    const char* st = "";
    uint32_t stc = Pal::DANGER;
    if (gp->msg_until && (int32_t)(esphome::millis() - gp->msg_until) < 0) { st = gp->msg; stc = Pal::TXT_DIM; }
    else if (gp->check_sq != NO_SQ) { st = "ECHEC !"; stc = Pal::DANGER; }
    else if (gs->ai_think)          { st = "Le Tab reflechit"; stc = Pal::THINK; }
    set_text_if(gs->h_status, st);
    set_color(gs->h_status, stc);
}

static void refresh_movelist() {
    auto& rows = gs->movelist_rows;
    int nrows = 0;

    for (int i = 0; i < gp->nply; i++) {
        const uint16_t num = gh->hist_num[i];
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
        char* dst = (gh->hist_side[i] == WHITE) ? rows[nrows - 1].w : rows[nrows - 1].b;
        strncpy(dst, gh->hist_san[i], 11);
        dst[11] = 0;
    }

    char buf[16];
    for (int i = 0; i < MOVE_ROWS; i++) {
        if (i < nrows) {
            snprintf(buf, sizeof(buf), "%u.", (unsigned) rows[i].num);
            set_text_if(gs->ml_num[i], buf);
            set_text_if(gs->ml_w[i], rows[i].w[0] ? rows[i].w : "...");
            set_text_if(gs->ml_b[i], rows[i].b);
        } else {
            set_text_if(gs->ml_num[i], "");
            set_text_if(gs->ml_w[i], "");
            set_text_if(gs->ml_b[i], "");
        }
    }
}

// ===========================================================================
// 12. Ecrans
// ===========================================================================

static void go_hub() {
    g_state = ST_HUB;
    gs->ai_think = false;
    menu_on(true);
    set_text_if(gs->m_title, "ROI NOIR");
    set_color(gs->m_title, Pal::ACCENT);
    set_text_if(gs->m_sub, "Echiquier du Tab — regles FIDE, IA embarquee, 100 % local");
    char body[128];
    snprintf(body, sizeof(body), "Classement local : %u Elo   ·   %u parties jouees",
             (unsigned) gs->save.elo, (unsigned) gs->save.games);
    set_text_if(gs->m_body, body);
    set_text_if(gs->m_foot, "Toucher le bandeau du haut pendant la partie ouvre le menu de pause.");

    int i = 0;
    slot_list(i++, "Nouvelle partie", "Choix du mode, de la couleur, du niveau et de la pendule", Pal::ACCENT, true);
    if (gs->save.resume_valid) {
        char d[96];
        snprintf(d, sizeof(d), "%s · niveau %s · %u demi-coups joues",
                 MODE_NAME[gs->save.r_mode < 3 ? gs->save.r_mode : 0],
                 AI_LEVELS[gs->save.r_level < CHESS_NLEVELS ? gs->save.r_level : 0].name,
                 (unsigned) gs->save.r_plies);
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
    set_text_if(gs->m_title, "NOUVELLE PARTIE");
    set_color(gs->m_title, Pal::ACCENT);
    set_text_if(gs->m_sub, "Toucher une ligne pour changer sa valeur");
    set_text_if(gs->m_body, "");
    set_text_if(gs->m_foot, "");

    char d[96];
    int i = 0;
    slot_list(i++, "Mode de jeu", MODE_NAME[gs->save.mode], Pal::ACCENT, true);

    if (gs->save.mode == 0) {
        snprintf(d, sizeof(d), "%s — vous ouvrez %s",
                 gs->save.human_side == 0 ? "Blancs" : "Noirs",
                 gs->save.human_side == 0 ? "la partie" : "en second");
        slot_list(i++, "Votre couleur", d, Pal::TXT, true);
    } else {
        slot_list(i++, "Votre couleur", "Sans objet dans ce mode", Pal::TXT_MUTED, false);
    }

    if (gs->save.mode == 1) {
        slot_list(i++, "Niveau du Tab", "Sans objet : duel entre deux humains", Pal::TXT_MUTED, false);
    } else {
        snprintf(d, sizeof(d), "%s — %s", AI_LEVELS[gs->save.level].name, AI_LEVELS[gs->save.level].desc);
        slot_list(i++, "Niveau du Tab", d, Pal::TXT, true);
    }

    slot_list(i++, "Pendule", CLOCKS[gs->save.clock_opt].name, Pal::TXT, true);
    slot_list(i++, "Commencer", "Lance la partie avec ces reglages", Pal::GOOD, true);
    slot_list(i++, "Retour", "Revenir au menu principal", Pal::TXT_DIM, true);
    slots_hide_from(i);
}

static void go_settings() {
    g_state = ST_SETTINGS;
    menu_on(true);
    set_text_if(gs->m_title, "REGLAGES");
    set_color(gs->m_title, Pal::ACCENT);
    set_text_if(gs->m_sub, "Conserves en NVS, valables pour toutes les parties");
    set_text_if(gs->m_body, "");
    set_text_if(gs->m_foot, "");

    int i = 0;
    slot_list(i++, "Gestes IMU",
              gs->save.gestures ? "Actives — une secousse franche demande un indice"
                              : "Desactives — l'inclinaison est ignoree",
              gs->save.gestures ? Pal::GOOD : Pal::TXT_MUTED, true);
    slot_list(i++, "Regle des 50 coups",
              gs->save.rule50 ? "Nulle automatique apres 50 coups sans prise ni pion"
                            : "Desactivee — la partie continue",
              gs->save.rule50 ? Pal::GOOD : Pal::TXT_MUTED, true);
    slot_list(i++, "Evaluation au HUD",
              gs->save.show_eval ? "Affichee — estimation en pions, indicative"
                               : "Masquee",
              gs->save.show_eval ? Pal::GOOD : Pal::TXT_MUTED, true);
    char d[64];
    snprintf(d, sizeof(d), "%s — cadence du mode Tab contre Tab", DEMO_NAME[gs->save.demo_speed]);
    slot_list(i++, "Vitesse de demo", d, Pal::TXT, true);
    slot_list(i++, "Effacer les statistiques", "Remet a zero le bilan et le classement local", Pal::DANGER, true);
    slot_list(i++, "Retour", "Revenir au menu principal", Pal::TXT_DIM, true);
    slots_hide_from(i);
}

static void go_stats() {
    g_state = ST_STATS;
    menu_on(true);
    set_text_if(gs->m_title, "STATISTIQUES");
    set_color(gs->m_title, Pal::ACCENT);

    char sub[96];
    snprintf(sub, sizeof(sub), "Classement local : %u Elo   ·   %u parties contre le Tab",
             (unsigned) gs->save.elo, (unsigned) gs->save.games);
    set_text_if(gs->m_sub, sub);

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
               (unsigned) gs->save.wins[l], (unsigned) gs->save.draws[l],
               (unsigned) gs->save.losses[l]);
    }
    append("\nPlus longue partie : %u demi-coups\n", (unsigned) gs->save.longest_plies);
    append("Temps de jeu cumule : %u min", (unsigned)(gs->save.total_ms / 60000u));
    lv_obj_set_style_text_align(gs->m_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    set_text_if(gs->m_body, body);
    set_text_if(gs->m_foot, "");

    slot_list(0, "Retour", "Revenir au menu principal", Pal::TXT_DIM, true);
    lv_obj_align(gs->slot[0], LV_ALIGN_BOTTOM_MID, 0, -80);
    slots_hide_from(1);
}

static void show_confirm(uint8_t kind, const char* title, const char* question) {
    gs->confirm_kind = kind;
    g_state = ST_CONFIRM;
    menu_on(true);
    set_text_if(gs->m_title, title);
    set_color(gs->m_title, Pal::ACCENT);
    set_text_if(gs->m_sub, question);
    set_text_if(gs->m_body, "");
    set_text_if(gs->m_foot, "");
    slot_list(0, "Confirmer", "", Pal::DANGER, true);
    slot_list(1, "Annuler", "", Pal::TXT_DIM, true);
    slots_hide_from(2);
}

static void show_promo() {
    g_state = ST_PROMO;
    // 82 % d'opacite : on garde le plateau visible derriere, c'est utile pour
    // decider entre dame et cavalier.
    menu_on(true, (lv_opa_t) 209);
    set_text_if(gs->m_title, "PROMOTION");
    set_color(gs->m_title, Pal::ACCENT);
    set_text_if(gs->m_sub, "Le pion atteint la derniere rangee — choisir la piece");
    set_text_if(gs->m_body, "");
    set_text_if(gs->m_foot, "");

    // La couleur promue est celle du camp AU TRAIT (le pion n'a pas encore bouge).
    const bool white = (gp->pos.side == WHITE);
    static const uint8_t PROMO_T[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
    for (int i = 0; i < 4; i++) slot_promo(i, PROMO_T[i], white);
    slots_hide_from(4);
    promo_names(true);
}

static void show_pause() {
    g_state = ST_PAUSE;
    menu_on(true, (lv_opa_t) 235);
    set_text_if(gs->m_title, "PAUSE");
    set_color(gs->m_title, Pal::ACCENT);
    set_text_if(gs->m_sub, "La pendule est arretee");
    set_text_if(gs->m_body, "");
    set_text_if(gs->m_foot, "");
    int i = 0;
    slot_list(i++, "Reprendre", "Retour a la partie", Pal::GOOD, true);
    slot_list(i++, "Annuler le dernier coup", gp->mode == 0 ? "Annule votre coup et la reponse du Tab"
                                                          : "Annule le dernier demi-coup",
              gp->nply > 0 ? Pal::TXT : Pal::TXT_MUTED, gp->nply > 0);
    slot_list(i++, "Proposer nulle", gp->mode == 0 ? "Le Tab accepte s'il n'est pas mieux"
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
    if (gs->result != RES_DRAW) {
        if (gp->mode == 0) {
            const bool human_won = (gs->result == RES_WHITE) ? (gp->human == WHITE) : (gp->human == BLACK);
            t = human_won ? "VICTOIRE" : "DEFAITE";
            col = human_won ? Pal::GOOD : Pal::DANGER;
        } else {
            t = (gs->result == RES_WHITE) ? "LES BLANCS GAGNENT" : "LES NOIRS GAGNENT";
            col = Pal::ACCENT;
        }
    }
    set_text_if(gs->m_title, t);
    set_color(gs->m_title, col);
    set_text_if(gs->m_sub, gs->reason);

    char body[160];
    if (gp->mode == 0)
        snprintf(body, sizeof(body), "%d demi-coups  ·  niveau %s  ·  classement local : %u Elo",
                 gp->nply, AI_LEVELS[gp->level].name, (unsigned) gs->save.elo);
    else
        snprintf(body, sizeof(body), "%d demi-coups joues", gp->nply);
    set_text_if(gs->m_body, body);
    set_text_if(gs->m_foot, "");

    int i = 0;
    slot_list(i++, "Rejouer", "Meme mode, memes reglages", Pal::ACCENT, true);
    slot_list(i++, "Menu principal", "", Pal::TXT_DIM, true);
    slots_hide_from(i);
}

// ===========================================================================
// 13. Logique de partie
// ===========================================================================

static inline bool side_is_ai() {
    if (gp->mode == 2) return true;
    if (gp->mode == 1) return false;
    return gp->pos.side != gp->human;
}

static void recompute_legal() {
    gp->nall = gen_legal(gp->pos, gp->all_legal);
    gp->check_sq = in_check(gp->pos, gp->pos.side) ? gp->pos.king_sq[cidx(gp->pos.side)] : NO_SQ;
}

// Nombre d'occurrences de la position courante dans l'historique.
// [FR] La clef Zobrist n'encode ni le compteur des 50 coups ni le numero de coup :
// deux positions identiques (meme trait, memes droits de roque, meme case de
// prise en passant exploitable) ont donc la meme clef, ce qui est exactement la
// definition FIDE de la repetition.
static int repetition_count() {
    const uint64_t h = gh->hist_hash[gp->nply];
    int n = 0;
    for (int i = 0; i <= gp->nply; i++) if (gh->hist_hash[i] == h) n++;
    return n;
}

static void update_elo(float score) {
    const int opp = (int) AI_LEVELS[gp->level].elo;
    const float expected = 1.0f / (1.0f + powf(10.0f, (float)(opp - (int) gs->save.elo) / 400.0f));
    int e = (int) gs->save.elo + (int) lroundf(24.0f * (score - expected));
    if (e < 100) e = 100;
    if (e > 3000) e = 3000;
    gs->save.elo = (uint32_t) e;
}

static void end_game(uint8_t res, const char* reason) {
    gp->running = false;
    gs->ai_think = false;
    gs->result = res;
    gs->reason = reason;

    // Une animation en vol au moment du mat laisserait une case vide derriere le
    // menu : on la termine immediatement.
    if (gs->anim_on) {
        gs->anim_on = false;
        gs->anim_hide = -1;
        show(gs->anim, false);
        draw_all();
    }

    if ((uint32_t) gp->nply > gs->save.longest_plies) gs->save.longest_plies = (uint32_t) gp->nply;
    if (gp->game_t0) gs->save.total_ms += esphome::millis() - gp->game_t0;
    gp->game_t0 = 0;

    // Seules les parties CONTRE LE TAB alimentent le bilan et le classement.
    if (gp->mode == 0 && gp->level < CHESS_NLEVELS) {
        gs->save.games++;
        if (res == RES_DRAW) { gs->save.draws[gp->level]++; update_elo(0.5f); }
        else {
            const bool human_won = (res == RES_WHITE) ? (gp->human == WHITE) : (gp->human == BLACK);
            if (human_won) { gs->save.wins[gp->level]++;   update_elo(1.0f); }
            else           { gs->save.losses[gp->level]++; update_elo(0.0f); }
        }
    }
    gs->save.resume_valid = 0;      // la partie est finie : plus rien a reprendre
    persist_save();
    show_over();
}

static void check_game_end() {
    recompute_legal();
    if (gp->nall == 0) {
        if (gp->check_sq != NO_SQ) end_game(gp->pos.side == WHITE ? RES_BLACK : RES_WHITE, "Echec et mat");
        else                     end_game(RES_DRAW, "Pat — le roi n'est pas en echec mais aucun coup n'est jouable");
        return;
    }
    if (insufficient_material(gp->pos)) { end_game(RES_DRAW, "Materiel insuffisant pour mater"); return; }
    if (gs->save.rule50 && gp->pos.halfmove >= 100) {
        end_game(RES_DRAW, "Regle des 50 coups"); return;
    }
    if (repetition_count() >= 3) { end_game(RES_DRAW, "Triple repetition de la position"); return; }
}

// Applique un coup au modele puis declenche le rendu et l'animation.
static void play_move(const Move& m) {
    if (gp->nply >= MAX_HIST) {          // garde-fou : historique plein
        end_game(RES_DRAW, "Partie interrompue : historique plein");
        return;
    }
    move_to_san(gp->pos, m, gh->hist_san[gp->nply], 12);
    gh->hist_num[gp->nply]  = gp->pos.fullmove;
    gh->hist_side[gp->nply] = gp->pos.side;

    const int from_cell = cell_of_sq(m.from);
    const int to_cell   = cell_of_sq(m.to);
    const uint8_t mover = gp->pos.side;

    if (!make(gp->pos, m, gh->hist_undo[gp->nply])) return;   // ne doit jamais arriver
    gh->hist_move[gp->nply] = m;
    gp->nply++;
    gh->hist_hash[gp->nply] = hash_of(gp->pos);
    gp->resume_dirty = true;   // écrit en NVS par tick_cb, au plus toutes les 15 s

    // Increment Fischer credite au joueur qui vient de jouer.
    if (gp->clock_on && gp->inc_ms) gp->clock[cidx(mover)] += gp->inc_ms;

    gp->last_from = m.from;
    gp->last_to   = m.to;
    gp->sel_sq    = NO_SQ;
    gp->nfrom     = 0;
    gp->hint_until = 0;

    // Animation : la pastille d'arrivee est masquee, une pastille libre vole de
    // la case de depart vers la case d'arrivee.
    const uint8_t shown = gp->pos.board[m.to];
    if (shown) {
        const bool white = (shown & COLOR_MASK) == WHITE;
        char g[4];
        piece_utf8(shown, false, g);
        set_text_if(gs->anim_body, g);
        set_color(gs->anim_body, white ? Pal::PC_W_FILL : Pal::PC_B_FILL);
        if (white) { piece_utf8(shown, true, g); set_text_if(gs->anim_edge, g); }
        show(gs->anim_edge, white);
        // Le conteneur fait exactement une case : ses coordonnees sont celles
        // de la case, sans recentrage a calculer.
        gs->anim_x0 = (from_cell & 7) * CELL;
        gs->anim_y0 = (from_cell >> 3) * CELL;
        gs->anim_x1 = (to_cell & 7) * CELL;
        gs->anim_y1 = (to_cell >> 3) * CELL;
        lv_obj_set_pos(gs->anim, gs->anim_x0, gs->anim_y0);
        show(gs->anim, true);
        lv_obj_move_foreground(gs->anim);
        gs->anim_hide = to_cell;
        gs->anim_on = true;
        gs->anim_t0 = esphome::millis();
    }

    draw_all();
    refresh_movelist();
    update_hud(true);
    check_game_end();
}

static void undo_move() {
    if (g_state != ST_PLAY && g_state != ST_PAUSE) return;
    if (gp->nply <= 0) return;

    // Si le Tab reflechit, on annule sa reflexion et on remonte d'un demi-coup
    // (celui du joueur) : le trait revient a l'humain.
    int steps = 1;
    if (gs->ai_think) { gs->ai_think = false; }
    else if (gp->mode == 0 && gp->nply >= 2) steps = 2;

    for (int k = 0; k < steps && gp->nply > 0; k++) {
        gp->nply--;
        unmake(gp->pos, gh->hist_move[gp->nply], gh->hist_undo[gp->nply]);
    }
    if (gp->nply > 0) { gp->last_from = gh->hist_move[gp->nply - 1].from; gp->last_to = gh->hist_move[gp->nply - 1].to; }
    else            { gp->last_from = NO_SQ; gp->last_to = NO_SQ; }

    gp->sel_sq = NO_SQ;
    gp->nfrom = 0;
    gp->hint_until = 0;
    gs->anim_on = false;
    gs->anim_hide = -1;
    show(gs->anim, false);
    gp->running = true;
    gp->ai_next_at = 0;
    recompute_legal();
    memset(gs->drawn_pc, 0xFF, sizeof(gs->drawn_pc));
    draw_all();
    refresh_movelist();
    update_hud(true);
}

static void start_new_game() {
    gp->mode  = gs->save.mode;
    gp->level = gs->save.level;
    gp->human = (gs->save.human_side == 0) ? WHITE : BLACK;

    set_start(gp->pos);
    gp->nply = 0;
    gh->hist_hash[0] = hash_of(gp->pos);

    // Plateau oriente cote joueur : en hotseat et en demo on garde les blancs
    // en bas (convention), en solo on tourne si l'humain a les noirs.
    gp->flip = (gp->mode == 0 && gp->human == BLACK);

    gp->clock_on = (gs->save.clock_opt != 0);
    gp->clock[0] = gp->clock[1] = CLOCKS[gs->save.clock_opt].base_ms;
    gp->inc_ms   = CLOCKS[gs->save.clock_opt].inc_ms;
    gs->clock_last = esphome::millis();
    gp->game_t0 = esphome::millis();

    gp->sel_sq = gp->last_from = gp->last_to = NO_SQ;
    gp->nfrom = 0;
    gp->hint_until = 0;
    gp->hint_ready_at = 0;
    gs->ai_think = false;
    gp->ai_next_at = 0;
    gs->anim_on = false;
    gs->anim_hide = -1;
    show(gs->anim, false);
    gp->running = true;

    g_state = ST_PLAY;
    menu_on(false);
    recompute_legal();
    memset(gs->drawn_pc, 0xFF, sizeof(gs->drawn_pc));
    memset(gs->drawn_hl, 0xFF, sizeof(gs->drawn_hl));
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
    if (!gs->save.resume_valid) return false;
    Position p;
    if (!set_fen(p, gs->save.r_fen)) { gs->save.resume_valid = 0; return false; }
    gp->pos   = p;
    gp->mode  = (gs->save.r_mode < 3) ? gs->save.r_mode : 0;
    gp->level = (gs->save.r_level < CHESS_NLEVELS) ? gs->save.r_level : 2;
    gp->human = (gs->save.r_human == 0) ? WHITE : BLACK;

    gp->nply = 0;
    gh->hist_hash[0] = hash_of(gp->pos);
    gp->flip = (gp->mode == 0 && gp->human == BLACK);

    gp->clock[0] = gs->save.r_clock_w;
    gp->clock[1] = gs->save.r_clock_b;
    gp->clock_on = (gp->clock[0] != 0 || gp->clock[1] != 0);
    gp->inc_ms   = CLOCKS[gs->save.clock_opt].inc_ms;
    gs->clock_last = esphome::millis();
    gp->game_t0 = esphome::millis();

    gp->sel_sq = gp->last_from = gp->last_to = NO_SQ;
    gp->nfrom = 0;
    gp->hint_until = 0;
    gs->ai_think = false;
    gp->ai_next_at = 0;
    gs->anim_on = false;
    gs->anim_hide = -1;
    show(gs->anim, false);
    gp->running = true;

    g_state = ST_PLAY;
    menu_on(false);
    recompute_legal();
    memset(gs->drawn_pc, 0xFF, sizeof(gs->drawn_pc));
    memset(gs->drawn_hl, 0xFF, sizeof(gs->drawn_hl));
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
    if (!gp->running) return;
    get_fen(gp->pos, gs->save.r_fen, CHESS_FEN_CAP);
    gs->save.r_level   = gp->level;
    gs->save.r_mode    = gp->mode;
    gs->save.r_human   = (gp->human == WHITE) ? 0 : 1;
    gs->save.r_clock_w = gp->clock_on ? gp->clock[0] : 0;
    gs->save.r_clock_b = gp->clock_on ? gp->clock[1] : 0;
    gs->save.r_plies   = (uint16_t) gp->nply;
    gs->save.resume_valid = 1;
}

// Proposition de nulle. Contre le Tab, il accepte s'il n'est pas mieux : une
// recherche courte (profondeur 2, 30 ms max) donne son avis.
static void offer_draw() {
    if (gp->mode == 1) { end_game(RES_DRAW, "Nulle par accord entre les joueurs"); return; }
    if (gp->mode == 2) { end_game(RES_DRAW, "Nulle declaree en mode demo"); return; }
    int sc = 0;
    search_quick(gp->pos, 2, 30, &sc);
    // `sc` est du point de vue du trait. Si c'est au Tab de jouer, un score
    // positif signifie qu'il est mieux : il refuse.
    const int tab_score = (gp->pos.side == gp->human) ? -sc : sc;
    if (tab_score <= 40) end_game(RES_DRAW, "Le Tab accepte la nulle");
    else {
        g_state = ST_PLAY;
        menu_on(false);
        gs->clock_last = esphome::millis();
        toast("Le Tab refuse la nulle", 3000);
        update_hud(true);
    }
}

// ===========================================================================
// 14. Pilotage de l'IA
// ===========================================================================

static void ai_begin() {
    if (!gp->running || gs->ai_think) return;
    if (gp->nall <= 0) return;
    gs->ai_think = true;
    search_start(gp->pos, gp->level, esphome::millis() ^ (uint32_t)(gp->nply * 2654435761u));
    show(gs->think_lbl, true);
    show(gs->think_bar, true);
}

void ai_step() {
    if (!gs || !gs->ai_think) return;
    if (!search_step()) {
        // Progression : temps CPU consomme / budget du niveau, plus la profondeur
        // reellement atteinte (c'est l'information honnete, pas une estimation).
        const uint32_t bud = search_budget_ms() ? search_budget_ms() : 1;
        uint32_t pct = search_cpu_ms() * 100u / bud;
        if (pct > 100u) pct = 100u;
        lv_obj_set_width(gs->think_fill, (int)(504u * pct / 100u));
        char b[64];
        snprintf(b, sizeof(b), "Le Tab reflechit — profondeur %d, %u k noeuds",
                 search_depth_done(), (unsigned)(search_nodes() / 1000u));
        set_text_if(gs->think_lbl, b);
        return;
    }

    gs->ai_think = false;
    show(gs->think_lbl, false);
    show(gs->think_bar, false);
    const Move m = search_best();
    if (move_null(m)) { check_game_end(); return; }
    play_move(m);
}

static void do_hint() {
    if (g_state != ST_PLAY || !gp->running || gs->ai_think || gs->anim_on) return;
    const uint32_t now = esphome::millis();
    if (gp->hint_ready_at && (int32_t)(now - gp->hint_ready_at) < 0) {
        toast("Indice en recharge", 1500);
        return;
    }
    int sc = 0;
    const Move m = search_quick(gp->pos, 2, 30, &sc);
    if (move_null(m)) return;
    gp->hint_from = m.from;
    gp->hint_to   = m.to;
    gp->hint_until = now + HINT_SHOW_MS;
    gp->hint_ready_at = now + HINT_CD_MS;
    draw_all();
}

// ===========================================================================
// 15. Evenements
// ===========================================================================

static void select_square(uint8_t sq) {
    gp->sel_sq = sq;
    gp->nfrom = 0;
    for (int i = 0; i < gp->nall && gp->nfrom < MAX_DOTS; i++)
        if (gp->all_legal[i].from == sq) gp->from_legal[gp->nfrom++] = gp->all_legal[i];
    if (gp->nfrom == 0) gp->sel_sq = NO_SQ;
    draw_all();
}

// Tente de jouer gp->sel_sq -> to. Retourne true si la case etait une destination.
static bool try_move(uint8_t to) {
    int found = -1, count = 0;
    for (int i = 0; i < gp->nfrom; i++)
        if (gp->from_legal[i].to == to) { if (found < 0) found = i; count++; }
    if (found < 0) return false;

    // 4 coups pour la meme case d'arrivee = promotion : il faut demander la piece.
    if (count > 1 && (gp->from_legal[found].flags & MF_PROMO)) {
        gs->promo_from = gp->sel_sq;
        gs->promo_to   = to;
        show_promo();
        return true;
    }
    play_move(gp->from_legal[found]);
    return true;
}

static void cell_event_cb(lv_event_t* e) {
    if (g_state != ST_PLAY || !gp->running) return;
    if (gs->anim_on || gs->ai_think) return;
    if (gp->mode == 2) return;                       // demo : plateau en lecture seule
    if (gp->mode == 0 && gp->pos.side != gp->human) return;

    const int i = (int) (intptr_t) lv_event_get_user_data(e);
    const uint8_t sq = sq_of_cell(i);
    const uint8_t pc = gp->pos.board[sq];

    if (gp->sel_sq != NO_SQ) {
        if (sq == gp->sel_sq) { gp->sel_sq = NO_SQ; gp->nfrom = 0; draw_all(); return; }
        if (try_move(sq)) return;
    }
    // Selection (ou re-selection) d'une piece du joueur au trait.
    if (pc && (pc & COLOR_MASK) == gp->pos.side) select_square(sq);
    else if (gp->sel_sq != NO_SQ) { gp->sel_sq = NO_SQ; gp->nfrom = 0; draw_all(); }
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
            const int resume = gs->save.resume_valid ? 1 : 0;
            if (i == 0) { go_setup(); }
            else if (resume && i == 1) {
                if (gp->running) {
                    // Partie encore en memoire (meme session) : on revient dessus
                    // sans repasser par le FEN, ce qui preserve l'historique des
                    // coups (et donc « Annuler » et la triple repetition).
                    g_state = ST_PLAY;
                    menu_on(false);
                    gs->clock_last = esphome::millis();
                    memset(gs->drawn_pc, 0xFF, sizeof(gs->drawn_pc));
                    memset(gs->drawn_hl, 0xFF, sizeof(gs->drawn_hl));
                    draw_all();
                    refresh_movelist();
                    update_hud(true);
                } else if (!resume_game()) {
                    go_hub();
                }
            }
            else if (i == 1 + resume) { go_stats(); }
            else if (i == 2 + resume) { gs->settings_from = ST_HUB; go_settings(); }
            else if (i == 3 + resume) { close(); }
            break;
        }

        case ST_SETUP:
            if (i == 0) { gs->save.mode = (uint8_t)((gs->save.mode + 1) % 3); go_setup(); }
            else if (i == 1) { if (gs->save.mode == 0) { gs->save.human_side ^= 1; go_setup(); } }
            else if (i == 2) { if (gs->save.mode != 1) { gs->save.level = (uint8_t)((gs->save.level + 1) % CHESS_NLEVELS); go_setup(); } }
            else if (i == 3) { gs->save.clock_opt = (uint8_t)((gs->save.clock_opt + 1) % 4); go_setup(); }
            else if (i == 4) { persist_save(); start_new_game(); }
            else if (i == 5) { go_hub(); }
            break;

        case ST_SETTINGS:
            if (i == 0) { gs->save.gestures ^= 1; go_settings(); }
            else if (i == 1) { gs->save.rule50 ^= 1; go_settings(); }
            else if (i == 2) { gs->save.show_eval ^= 1; gs->c_eval = 0x7FFFFFFF; go_settings(); }
            else if (i == 3) { gs->save.demo_speed = (uint8_t)((gs->save.demo_speed + 1) % 3); go_settings(); }
            else if (i == 4) { show_confirm(1, "EFFACER LES STATISTIQUES", "Bilan, records et classement local seront remis a zero."); }
            else if (i == 5) {
                persist_save();
                if (gs->settings_from == ST_PAUSE) show_pause(); else go_hub();
            }
            break;

        case ST_STATS:
            if (i == 0) go_hub();
            break;

        case ST_CONFIRM:
            if (i == 0 && gs->confirm_kind == 1) {
                const uint8_t lv = gs->save.level, md = gs->save.mode, hs = gs->save.human_side;
                const uint8_t ck = gs->save.clock_opt, ge = gs->save.gestures, r5 = gs->save.rule50;
                const uint8_t se = gs->save.show_eval, ds = gs->save.demo_speed;
                save_defaults();                 // remet aussi l'Elo a 1000
                gs->save.level = lv; gs->save.mode = md; gs->save.human_side = hs;
                gs->save.clock_opt = ck; gs->save.gestures = ge; gs->save.rule50 = r5;
                gs->save.show_eval = se; gs->save.demo_speed = ds;
                persist_save();
                go_settings();
            } else {
                go_settings();
            }
            break;

        case ST_PROMO: {
            static const uint8_t PROMO[4] = {QUEEN, ROOK, BISHOP, KNIGHT};
            if (i < 0 || i > 3) break;
            for (int k = 0; k < gp->nfrom; k++) {
                const Move& m = gp->from_legal[k];
                if (m.from == gs->promo_from && m.to == gs->promo_to && m.promo == PROMO[i]) {
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
            if (i == 0) { g_state = ST_PLAY; menu_on(false); gs->clock_last = esphome::millis(); update_hud(true); }
            else if (i == 1) { if (gp->nply > 0) { g_state = ST_PLAY; menu_on(false); gs->clock_last = esphome::millis(); undo_move(); } }
            else if (i == 2) { offer_draw(); }
            else if (i == 3) {
                const uint8_t loser = (gp->mode == 0) ? gp->human : gp->pos.side;
                end_game(loser == WHITE ? RES_BLACK : RES_WHITE, "Abandon");
            }
            else if (i == 4) { gs->settings_from = ST_PAUSE; go_settings(); }
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
    uint32_t dt = now - gs->clock_last;
    gs->clock_last = now;
    // Plus de PAUSE_GAP_MS sans tick : écran éteint (lvgl.pause) ou boucle
    // bloquée. La pendule ne débite pas ce temps au camp au trait — avant, un
    // joueur immobile dont l'écran s'éteignait perdait au temps (25/09/2026).
    if (dt > PAUSE_GAP_MS) dt = 0;
    if (!gp->clock_on || !gp->running || g_state != ST_PLAY) return;
    const int s = cidx(gp->pos.side);
    if (gp->clock[s] <= dt) {
        gp->clock[s] = 0;
        // Chute de pendule. Si l'adversaire ne peut plus mater, c'est nulle.
        if (insufficient_material(gp->pos)) end_game(RES_DRAW, "Temps ecoule — materiel insuffisant pour mater");
        else end_game(gp->pos.side == WHITE ? RES_BLACK : RES_WHITE, "Temps ecoule");
        return;
    }
    gp->clock[s] -= dt;
}

static void tick_cb(lv_timer_t*) {
    const uint32_t now = esphome::millis();

    if (g_state != ST_PLAY) {
        // Hors partie, la pendule ne coule pas : on garde juste la reference a jour
        // pour ne pas accumuler un saut au retour.
        gs->clock_last = now;
        return;
    }

    update_clocks(now);
    if (g_state != ST_PLAY) return;      // update_clocks a pu terminer la partie

    // Animation de deplacement (lerp adouci).
    if (gs->anim_on) {
        const uint32_t dt = now - gs->anim_t0;
        if (dt >= ANIM_MS) {
            gs->anim_on = false;
            gs->anim_hide = -1;
            show(gs->anim, false);
            draw_cell(cell_of_sq(gp->last_to), true);
        } else {
            float t = (float) dt / (float) ANIM_MS;
            t = t * t * (3.0f - 2.0f * t);          // smoothstep
            lv_obj_set_pos(gs->anim,
                           gs->anim_x0 + (int)((gs->anim_x1 - gs->anim_x0) * t),
                           gs->anim_y0 + (int)((gs->anim_y1 - gs->anim_y0) * t));
        }
    }

    // Sauvegarde différée de la partie en cours (cf. RESUME_SAVE_MIN_MS).
    if (gp->resume_dirty && gp->running && (now - gp->resume_saved_at) >= RESUME_SAVE_MIN_MS) {
        store_running_game();
        persist_save();
        gp->resume_dirty = false;
        gp->resume_saved_at = now;
    }

    // Extinction de l'indice.
    if (gp->hint_until && (int32_t)(now - gp->hint_until) >= 0) {
        gp->hint_until = 0;
        gp->hint_from = gp->hint_to = NO_SQ;
        draw_all();
    }

    // Tour du Tab : une tranche de reflexion par tick, jamais plus.
    if (gp->running && !gs->anim_on) {
        if (gs->ai_think) {
            ai_step();
        } else if (side_is_ai()) {
            if (!gp->ai_next_at) {
                // Temporisation : immediate contre un humain, reglable en demo.
                gp->ai_next_at = now + (gp->mode == 2 ? DEMO_DELAY[gs->save.demo_speed] : 120u);
            } else if ((int32_t)(now - gp->ai_next_at) >= 0) {
                gp->ai_next_at = 0;
                ai_begin();
            }
        } else {
            gp->ai_next_at = 0;
        }
    }

    update_hud(false);
}

// ===========================================================================
// 17. API publique
// ===========================================================================

void on_imu(float ax, float ay, float az) {
    if (g_state == ST_OFF || !gs->save.gestures) return;
    if (ax != ax || ay != ay || az != az) return;             // garde NaN
    const float mag = sqrtf(ax * ax + ay * ay + az * az);
    // Filtre passe-bas leger : evite qu'une seule lecture bruitee declenche.
    g_shake_mag += (mag - g_shake_mag) * 0.5f;
    if (shake_fire(g_shake_mag, 1.9f, g_shake_last, esphome::millis(), SHAKE_CD_MS)) do_hint();
}

bool is_open() { return g_state != ST_OFF; }

void perft_log(int depth) {
    perft_selftest(depth);
    if (g_state == ST_OFF) search_release();  // jeu ferme : rien ne reste reserve
}

void open(const UI& ui) {
    if (g_state != ST_OFF) return;
    if (!ui.root || !ui.hud || !ui.board || !ui.panel) return;
    // Partie suspendue : ses deux blocs survivent a la fermeture tant qu'elle est
    // en cours ; sinon ils sont recrees ici, vides (position neutre plus bas).
    const bool fresh = (gp == nullptr);
    if (fresh) {
        gp = game_mem_new<Play>(MemPref::Internal);
        gh = game_mem_new<PlayHist>(MemPref::Psram);
    }
    gs = game_mem_new<Mem>(MemPref::Internal);
    if (!gp || !gh || !gs) {
        ESP_LOGW("chess", "%u + %u + %u o introuvables : jeu non ouvert", (unsigned) sizeof(Play),
                 (unsigned) sizeof(PlayHist), (unsigned) sizeof(Mem));
        game_mem_free(gs);
        if (fresh) { game_mem_free(gh); game_mem_free(gp); }
        if (ui.lvgl) ui.lvgl->show_page(ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
        return;
    }
    gs->ui = ui;

    persist_load();
    build_ui();

    // Position neutre affichee derriere le hub tant qu'aucune partie n'a demarre.
    if (gp->nply == 0 && !gp->running) {
        set_start(gp->pos);
        gh->hist_hash[0] = hash_of(gp->pos);
        recompute_legal();
    }

    // La page LVGL est déjà active (navigation via lvgl.page.show dans le YAML).
    memset(gs->drawn_pc, 0xFF, sizeof(gs->drawn_pc));
    memset(gs->drawn_hl, 0xFF, sizeof(gs->drawn_hl));
    draw_all();
    refresh_movelist();
    update_hud(true);

    gs->clock_last = esphome::millis();
    go_hub();

    if (!gs->timer) gs->timer = lv_timer_create(tick_cb, TICK_MS, nullptr);
}

void close() {
    if (g_state == ST_OFF) return;

    // Une partie en cours est sauvegardee pour pouvoir etre reprise au prochain
    // lancement (position + pendules), y compris apres un reboot.
    store_running_game();
    if (gp->game_t0) { gs->save.total_ms += esphome::millis() - gp->game_t0; gp->game_t0 = 0; }
    persist_save();

    if (gs->timer) { lv_timer_delete(gs->timer); gs->timer = nullptr; }
    gs->ai_think = false;
    search_release();
    gs->anim_on = false;
    // Navigation retour vers le sélecteur arcade (page LVGL).
    if (gs->ui.lvgl) gs->ui.lvgl->show_page(gs->ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
    g_state = ST_OFF;

    // Rien ne reste reserve : le callback pose sur le HUD YAML (il se doublerait a
    // la reouverture), les objets LVGL (dont le menu, enfant direct de root, et le
    // slot dont le callback nous appelle peut-etre), l'interface ; la partie
    // seulement si elle est finie — sinon on la reprend telle quelle.
    lv_obj_remove_event_cb(gs->ui.hud, hud_event_cb);
    ui_destroy(gs->ui.root, {gs->ui.hud, gs->ui.board, gs->ui.panel});
    game_mem_free(gs);
    if (!gp->running) { game_mem_free(gh); game_mem_free(gp); }
}

}  // namespace Chess
