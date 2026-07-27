/**
 * [AI-CONTEXT]
 * @file trivia_game.cpp
 * @role Jeu « Trial Poursuite » — clone Trivial Pursuit rétro-salon : roue
 *      authentique, camemberts 6 parts, question finale au centre.
 * @architecture_constraint Plein écran 1280x720. Le YAML ne fournit que 4
 *      conteneurs vides + 3 polices ; tout le reste est construit ici. Les objets
 *      LVGL sont PRÉALLOUÉS une seule fois (pool) puis réutilisés par show/hide +
 *      set_style : aucune allocation LVGL dans la boucle de jeu. Persistance NVS
 *      via esphome::global_preferences (aucune dépendance Home Assistant).
 * @ai_instruction Hot-path = tick() : pas de std::string, pas de to_string(), pas
 *      de new/delete. Les libellés ne sont réécrits que quand leur valeur change
 *      (set_text_if). Couleurs : uniquement Trivia::Pal::* (jamais d'hex en dur),
 *      hors CAT_COLORS/PAWN_COLORS qui sont des données de jeu.
 *
 *      PLATEAU : ce n'est PAS un simple cercle de 42 cases. C'est la roue de
 *      Trivial Pursuit — couronne de 42 cases, 6 rayons de 5 cases, QG central,
 *      soit 73 nœuds (voir trivia_game.h). Le déplacement se fait sur ce GRAPHE :
 *      on énumère toutes les destinations à exactement N pas sans demi-tour, on
 *      les surligne, et le joueur touche celle qu'il veut. Ne pas « simplifier »
 *      en modulo 42 : on perdrait les rayons, donc la finale.
 */
#include "trivia_game.h"
#include "trivia_questions.h"
#include "esphome/core/preferences.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace Trivia {

// ===========================================================================
// 1. Géométrie
// ===========================================================================

// --- Roue (coordonnées dans le conteneur `board`, 1280x672) ---
static constexpr int CX = 356, CY = 336;      // centre de la roue
static constexpr int R_RING   = 288;          // rayon de la couronne
static constexpr int D_RING   = 38;           // diamètre d'une case de couronne
static constexpr int D_HQ     = 44;           // diamètre d'un QG de catégorie
static constexpr int D_SPOKE  = 34;           // diamètre d'une case de rayon
static constexpr int D_HUB    = 82;           // diamètre du QG central
static const int SPOKE_R[TRIVIA_SPOKE_LEN] = {242, 197, 152, 107, 62};
static constexpr float ANG0 = -1.5707963f;    // nœud 0 à midi

// --- Panneau droit ---
static constexpr int PX = 716, PW = 548;
static constexpr int ROW_Y0 = 230, ROW_H = 54, ROW_STEP = 60;

// --- Cadences ---
static constexpr uint32_t TICK_MS      = 33;
static constexpr uint32_t DICE_SPIN_MS = 700;   // durée de roulement du dé
static constexpr uint32_t DICE_FACE_MS = 70;    // changement de face pendant le roulement
static constexpr uint32_t HOP_MS       = 110;   // durée d'un pas du pion
static constexpr uint32_t REVEAL_OK_MS = 2200;  // affichage du verdict — bonne réponse
static constexpr uint32_t REVEAL_KO_MS = 3400;  // ... mauvaise réponse (lecture de la solution)
static constexpr uint32_t MSG_MS       = 2600;  // message transitoire du bandeau d'état
static constexpr uint32_t SHAKE_CD_MS  = 900;   // anti-rebond de la secousse IMU

static constexpr uint32_t SAVE_MAGIC = 0x54525632u;  // "TRV2"
static constexpr uint32_t PREF_KEY   = 0x54525641u;  // clé NVS dédiée

// ===========================================================================
// 2. Données de jeu (couleurs, libellés, réglages)
// ===========================================================================

// Palette TP classique — ce sont des DONNÉES de jeu, pas du thème : elles ne
// vivent donc pas dans Pal.
static constexpr uint32_t CAT_COLORS[TRIVIA_NCAT] = {
    0x2196F3,  // 0 Géographie        — bleu
    0xEC407A,  // 1 Divertissement    — rose
    0xFFC107,  // 2 Histoire          — jaune
    0x8D6E63,  // 3 Arts & Littérature— marron
    0x4CAF50,  // 4 Sciences & Nature — vert
    0xFF7043   // 5 Sports & Loisirs  — orange
};
static const char* const CAT_NAMES[TRIVIA_NCAT] = {
    "Géographie", "Divertissement", "Histoire",
    "Arts & Littérature", "Sciences & Nature", "Sports & Loisirs"
};
// Version courte pour la légende du panneau (largeur de chip = 172 px).
static const char* const CAT_SHORT[TRIVIA_NCAT] = {
    "Géographie", "Divertis.", "Histoire", "Arts", "Sciences", "Sports"
};

static constexpr uint32_t PAWN_COLORS[TRIVIA_MAX_TEAMS] = {
    0xFF5252, 0x448AFF, 0x69F0AE, 0xFFD740, 0xE040FB, 0x40E0D0
};

static const char* const PRESET_NAMES[12] = {
    "Axel", "Marie", "Lucas", "Emma", "Hugo", "Léa",
    "Nathan", "Chloé", "Louis", "Jade", "Gabriel", "Alice"
};

// Difficulté → masque de bits sur TriviaQuestion::difficulty (0=facile,1=moyen,2=dur).
static const char* const DIFF_NAME[3] = {"Facile", "Normal", "Expert"};
static const char* const DIFF_DESC[3] = {"Questions faciles", "Faciles + moyennes", "Moyennes + difficiles"};
static const uint8_t     DIFF_MASK[3] = {0x01, 0x03, 0x06};

static const uint8_t     TIMER_SEC[4]  = {15, 30, 60, 0};
static const char* const TIMER_NAME[4] = {"15 s", "30 s", "60 s", "Illimité"};

static const char* const RULES_TEXT =
    "Réunissez les 6 parts de camembert, puis rejoignez le centre pour la question finale.\n"
    "\n"
    "Lancez le dé avec le bouton, ou secouez la tablette.\n"
    "Touchez ensuite l'une des cases surlignées : vous choisissez votre direction,\n"
    "mais jamais de demi-tour en cours de déplacement.\n"
    "\n"
    "Bonne réponse : vous rejouez.   Mauvaise réponse : au suivant.\n"
    "\n"
    "Les 6 grandes cases cerclées de blanc sont les QG de catégorie :\n"
    "elles seules rapportent une part de camembert.\n"
    "Une case violette à point blanc = Rejouer, relancez immédiatement le dé.\n"
    "Le centre sans les 6 parts = question de la catégorie de votre choix.\n"
    "Le centre avec les 6 parts = finale : l'équipe suivante choisit la catégorie,\n"
    "et une bonne réponse remporte la partie.";

// ===========================================================================
// 3. Générateur pseudo-aléatoire (xorshift32)
// ===========================================================================
static uint32_t s_rng = 0xDEADBEEFu;
static inline uint32_t rnd() {
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}
static inline int rnd_range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int) (rnd() % (uint32_t) (hi - lo + 1));
}

// ===========================================================================
// 4. Modèle du plateau
// ===========================================================================
enum CellType : uint8_t {
    CELL_CAT = 0,   // case de catégorie ordinaire
    CELL_HQ,        // QG de catégorie — donne une part de camembert
    CELL_ROLL,      // « Rejouer » : relance immédiate
    CELL_HUB        // QG central — question au choix, ou finale
};

struct BoardCell {
    CellType type;
    uint8_t  cat;   // catégorie 0..5 (indifférent pour CELL_ROLL / CELL_HUB)
};

static BoardCell s_cells[TRIVIA_NODES];
static constexpr int HUB_NODE = TRIVIA_RING + TRIVIA_NCAT * TRIVIA_SPOKE_LEN;  // 72

static inline int spoke_node(int k, int j) { return TRIVIA_RING + k * TRIVIA_SPOKE_LEN + j; }

// Construit la topologie une seule fois.
//  Couronne : 1 QG tous les 7 nœuds. Entre deux QG, 6 cases dont 1 « Rejouer ».
//  Les 5 autres portent les 5 catégories AUTRES que celle du QG du segment —
//  comme sur le vrai plateau, où aucun segment ne répète sa propre couleur.
static void build_board() {
    for (int i = 0; i < TRIVIA_RING; i++) {
        int seg = i / 7, p = i % 7;
        if (p == 0)      { s_cells[i] = {CELL_HQ,   (uint8_t) seg}; }
        else if (p == 3) { s_cells[i] = {CELL_ROLL, 0}; }
        else {
            // p ∈ {1,2,4,5,6} → rang 1..5 dans le segment
            int slot = (p < 3) ? p : (p - 1);
            s_cells[i] = {CELL_CAT, (uint8_t) ((seg + slot) % TRIVIA_NCAT)};
        }
    }
    for (int k = 0; k < TRIVIA_NCAT; k++)
        for (int j = 0; j < TRIVIA_SPOKE_LEN; j++)
            s_cells[spoke_node(k, j)] = {CELL_CAT, (uint8_t) ((k + 1 + j) % TRIVIA_NCAT)};
    s_cells[HUB_NODE] = {CELL_HUB, 0};
}

// Voisins d'un nœud. Au plus 6 (le centre). `out` doit tenir 8 entrées.
static int neighbors(int n, int* out) {
    int c = 0;
    if (n < TRIVIA_RING) {
        out[c++] = (n + 1) % TRIVIA_RING;
        out[c++] = (n + TRIVIA_RING - 1) % TRIVIA_RING;
        if (n % 7 == 0) out[c++] = spoke_node(n / 7, 0);
    } else if (n == HUB_NODE) {
        for (int k = 0; k < TRIVIA_NCAT; k++) out[c++] = spoke_node(k, TRIVIA_SPOKE_LEN - 1);
    } else {
        int k = (n - TRIVIA_RING) / TRIVIA_SPOKE_LEN;
        int j = (n - TRIVIA_RING) % TRIVIA_SPOKE_LEN;
        out[c++] = (j == 0) ? (k * 7) : spoke_node(k, j - 1);
        out[c++] = (j == TRIVIA_SPOKE_LEN - 1) ? HUB_NODE : spoke_node(k, j + 1);
    }
    return c;
}

// Position à l'écran du centre d'un nœud (coordonnées `board`).
static void node_pos(int n, int* x, int* y) {
    if (n == HUB_NODE) { *x = CX; *y = CY; return; }
    float ang; float r;
    if (n < TRIVIA_RING) {
        ang = ANG0 + (float) n * 6.2831853f / (float) TRIVIA_RING;
        r   = (float) R_RING;
    } else {
        int k = (n - TRIVIA_RING) / TRIVIA_SPOKE_LEN;
        int j = (n - TRIVIA_RING) % TRIVIA_SPOKE_LEN;
        ang = ANG0 + (float) (k * 7) * 6.2831853f / (float) TRIVIA_RING;
        r   = (float) SPOKE_R[j];
    }
    *x = CX + (int) lroundf(r * cosf(ang));
    *y = CY + (int) lroundf(r * sinf(ang));
}

static inline int node_diam(int n) {
    if (n == HUB_NODE) return D_HUB;
    if (n < TRIVIA_RING) return (n % 7 == 0) ? D_HQ : D_RING;
    return D_SPOKE;
}

// ===========================================================================
// 5. État de partie
// ===========================================================================
enum GState : uint8_t {
    ST_HUB = 0, ST_SETUP, ST_RULES, ST_STATS, ST_SETTINGS, ST_CONFIRM,
    ST_PLAY, ST_CATPICK, ST_QUESTION, ST_REVEAL, ST_PAUSE, ST_VICTORY
};
enum Phase : uint8_t { PH_ROLL = 0, PH_ROLLING, PH_CHOOSE, PH_MOVING };

struct Team {
    char    name[TRIVIA_NAME_LEN];
    uint8_t color_idx;
    uint8_t wedges;   // masque 6 bits
    uint8_t pos;      // nœud 0..72
};

static GState  s_state       = ST_HUB;
static GState  s_return_to   = ST_HUB;   // écran d'où l'on a ouvert règles/réglages
static Phase   s_phase       = PH_ROLL;
static bool    s_in_game     = false;    // une partie est en cours (même hors écran de jeu)
static uint8_t s_n_teams     = 2;
static uint8_t s_cur         = 0;
static uint8_t s_turn        = 1;
static Team    s_teams[TRIVIA_MAX_TEAMS];
static uint32_t s_game_ok = 0, s_game_ko = 0;

// Réglages (miroir RAM de TriviaSave::cfg_*)
static uint8_t s_difficulty = 1;
static uint8_t s_timer_idx  = 1;
static bool    s_shake_on   = true;

// Setup en cours d'édition
static uint8_t s_setup_n = 2;
static Team    s_setup_teams[TRIVIA_MAX_TEAMS];

// Dé
static uint8_t  s_dice_val   = 0;
static uint8_t  s_dice_shown = 1;
static bool     s_dice_spin  = false;
static uint32_t s_dice_t0    = 0;

// Déplacement
static bool     s_reach[TRIVIA_NODES];
static uint8_t  s_path[8];
static int      s_path_n     = 0;
static int      s_hop        = 0;
static uint32_t s_hop_t0     = 0;

// Question
static const TriviaQuestion* s_q = nullptr;
static uint8_t  s_slot_of[4];        // slot d'affichage → 0=bonne réponse, 1..3=leurres
static uint8_t  s_correct_slot = 0;
static uint8_t  s_picked_slot  = 0xFF;
static bool     s_correct      = false;
static bool     s_won_wedge    = false;
static bool     s_is_final     = false;
static uint8_t  s_q_cat        = 0;
static uint32_t s_q_t0         = 0;
static uint32_t s_reveal_until = 0;

// Sacs de questions (tirage sans remise par catégorie)
static uint8_t s_bag_pos[TRIVIA_NCAT];
static uint8_t s_bag[TRIVIA_NCAT][128];

// Message transitoire du bandeau d'état
static char     s_msg[80] = "";
static uint32_t s_msg_until = 0;

// Écran de confirmation générique
enum ConfirmKind : uint8_t { CFM_ABANDON = 0, CFM_WIPE_STATS };
static ConfirmKind s_confirm = CFM_ABANDON;

// IMU
static uint32_t s_shake_last = 0;

// Vainqueur
static uint8_t s_winner = 0;

// ===========================================================================
// 6. Persistance NVS
// ===========================================================================
static TriviaSave s_save;
static esphome::ESPPreferenceObject s_pref;
static bool s_pref_ready = false;

// Copie nom -> nom. Les deux tampons font exactement TRIVIA_NAME_LEN, donc la
// copie est complete par construction (snprintf ferait hurler -Wformat-truncation).
static inline void copy_name(char* dst, const char* src) {
    memcpy(dst, src, TRIVIA_NAME_LEN);
    dst[TRIVIA_NAME_LEN - 1] = '\0';
}

static void roster_defaults(TriviaTeamSave* r) {
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        snprintf(r[i].name, sizeof(r[i].name), "%s", PRESET_NAMES[i]);
        r[i].color_idx = (uint8_t) i;
        r[i].wedges = 0;
        r[i].pos = 0;
        r[i]._pad = 0;
    }
}

static void save_defaults() {
    memset(&s_save, 0, sizeof(s_save));
    s_save.magic          = SAVE_MAGIC;
    s_save.cfg_difficulty = 1;
    s_save.cfg_timer      = 1;
    s_save.cfg_shake      = 1;
    s_save.cfg_nteams     = 2;
    roster_defaults(s_save.roster);
}

void persist_load() {
    if (!s_pref_ready) {
        s_pref = esphome::global_preferences->make_preference<TriviaSave>(PREF_KEY);
        s_pref_ready = true;
    }
    if (!s_pref.load(&s_save) || s_save.magic != SAVE_MAGIC) save_defaults();
    // Garde-fous : une NVS corrompue ne doit pas indexer hors tableau.
    if (s_save.cfg_difficulty > 2) s_save.cfg_difficulty = 1;
    if (s_save.cfg_timer > 3)      s_save.cfg_timer = 1;
    if (s_save.cfg_nteams < 1 || s_save.cfg_nteams > TRIVIA_MAX_TEAMS) s_save.cfg_nteams = 2;
    if (s_save.n_teams > TRIVIA_MAX_TEAMS) s_save.n_teams = 0;
    if (s_save.n_teams > 0 && s_save.current_team >= s_save.n_teams) s_save.current_team = 0;
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        s_save.roster[i].name[sizeof(s_save.roster[i].name) - 1] = '\0';
        s_save.teams[i].name[sizeof(s_save.teams[i].name) - 1] = '\0';
        if (s_save.roster[i].color_idx >= TRIVIA_MAX_TEAMS) s_save.roster[i].color_idx = (uint8_t) i;
        if (s_save.teams[i].color_idx >= TRIVIA_MAX_TEAMS)  s_save.teams[i].color_idx = (uint8_t) i;
        if (s_save.teams[i].pos >= TRIVIA_NODES) s_save.teams[i].pos = 0;
        s_save.teams[i].wedges &= 0x3F;
    }
    if (s_save.roster[0].name[0] == '\0') roster_defaults(s_save.roster);

    s_difficulty = s_save.cfg_difficulty;
    s_timer_idx  = s_save.cfg_timer;
    s_shake_on   = (s_save.cfg_shake != 0);
    s_setup_n    = s_save.cfg_nteams;
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        copy_name(s_setup_teams[i].name, s_save.roster[i].name);
        s_setup_teams[i].color_idx = s_save.roster[i].color_idx;
        s_setup_teams[i].wedges = 0;
        s_setup_teams[i].pos = 0;
    }
}

void persist_save() {
    if (!s_pref_ready) persist_load();
    s_save.magic          = SAVE_MAGIC;
    s_save.cfg_difficulty = s_difficulty;
    s_save.cfg_timer      = s_timer_idx;
    s_save.cfg_shake      = s_shake_on ? 1 : 0;
    s_save.cfg_nteams     = s_setup_n;
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        copy_name(s_save.roster[i].name, s_setup_teams[i].name);
        s_save.roster[i].color_idx = s_setup_teams[i].color_idx;
    }
    // Partie en cours
    s_save.n_teams     = s_in_game ? s_n_teams : 0;
    s_save.current_team = s_cur;
    s_save.turn_num    = s_turn;
    s_save.rng_seed    = s_rng;
    s_save.game_ok     = s_game_ok;
    s_save.game_ko     = s_game_ko;
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        copy_name(s_save.teams[i].name, s_teams[i].name);
        s_save.teams[i].color_idx = s_teams[i].color_idx;
        s_save.teams[i].wedges    = s_teams[i].wedges;
        s_save.teams[i].pos       = s_teams[i].pos;
    }
    s_pref.save(&s_save);
    esphome::global_preferences->sync();
}

// ===========================================================================
// 7. Objets LVGL (pool préallouée)
// ===========================================================================
static constexpr int N_SLOTS   = 26;   // entrées de menu génériques
static constexpr int N_FREE    = 8;    // intertitres libres des écrans de menu

static UI   g_ui;
static bool g_built = false;
static bool g_open  = false;
static lv_timer_t* g_timer = nullptr;

// Plateau
static lv_obj_t* g_cell[TRIVIA_NODES] = {};
static lv_obj_t* g_roll_dot[TRIVIA_NCAT] = {};
static lv_obj_t* g_hub_pie[TRIVIA_NCAT] = {};
static lv_obj_t* g_hub_rim = nullptr;
static lv_obj_t* g_pawn[TRIVIA_MAX_TEAMS] = {};

// Panneau droit
static lv_obj_t* g_dice_face = nullptr;
static lv_obj_t* g_pip[7] = {};
static lv_obj_t* g_roll_btn = nullptr;
static lv_obj_t* g_roll_lbl = nullptr;
static lv_obj_t* g_roll_hint = nullptr;
static lv_obj_t* g_status_lbl = nullptr;
static lv_obj_t* g_row[TRIVIA_MAX_TEAMS] = {};
static lv_obj_t* g_row_chip[TRIVIA_MAX_TEAMS] = {};
static lv_obj_t* g_row_name[TRIVIA_MAX_TEAMS] = {};
static lv_obj_t* g_row_cnt[TRIVIA_MAX_TEAMS] = {};
static lv_obj_t* g_row_pie[TRIVIA_MAX_TEAMS][TRIVIA_NCAT] = {};
static lv_obj_t* g_row_rim[TRIVIA_MAX_TEAMS] = {};

// HUD
static lv_obj_t* g_h_turn = nullptr;
static lv_obj_t* g_h_team = nullptr;
static lv_obj_t* g_h_clock = nullptr;
static lv_obj_t* g_menu_btn = nullptr;

// Calque question
static lv_obj_t* g_qlayer = nullptr;
static lv_obj_t* g_qcard = nullptr;
static lv_obj_t* g_qban = nullptr;
static lv_obj_t* g_qban_fix = nullptr;   // redresse les coins bas du bandeau
static lv_obj_t* g_qban_cat = nullptr;
static lv_obj_t* g_qban_side = nullptr;
static lv_obj_t* g_qbar = nullptr;
static lv_obj_t* g_qbar_fill = nullptr;
static lv_obj_t* g_qtext = nullptr;
static lv_obj_t* g_ans[4] = {};
static lv_obj_t* g_ans_lbl[4] = {};
static lv_obj_t* g_qfeed = nullptr;

// Calque menus (= g_ui.panel)
static lv_obj_t* g_m_title = nullptr;
static lv_obj_t* g_m_sub   = nullptr;
static lv_obj_t* g_m_body  = nullptr;
static lv_obj_t* g_m_foot  = nullptr;
static lv_obj_t* g_slot[N_SLOTS]   = {};
static lv_obj_t* g_slot_t[N_SLOTS] = {};
static lv_obj_t* g_slot_d[N_SLOTS] = {};
static lv_obj_t* g_free[N_FREE]    = {};
static lv_obj_t* g_bar[TRIVIA_NCAT] = {};
static lv_obj_t* g_bar_fill[TRIVIA_NCAT] = {};
static lv_obj_t* g_big_pie[TRIVIA_NCAT] = {};
static lv_obj_t* g_big_rim = nullptr;

// Caches de rendu. Le tick tourne à 30 Hz : sans eux, on réécrirait ~60 styles
// LVGL par image (6 pions, 6 fiches, 36 arcs de camembert, 4 boutons de réponse)
// et chaque écriture invalide sa zone — la roue serait redessinée en continu.
// Règle : on ne restyle un objet que si sa SIGNATURE a changé.
static uint8_t g_drawn_hl[TRIVIA_NODES];
static int     g_c_pawn_cur = -1;
static int     g_c_pawn_x[TRIVIA_MAX_TEAMS], g_c_pawn_y[TRIVIA_MAX_TEAMS];
static int     g_c_pawn_vis[TRIVIA_MAX_TEAMS];
static int     g_c_row_sig[TRIVIA_MAX_TEAMS];
static int     g_c_roll  = -1;
static int     g_c_layers = -1;
static int     g_c_hud_team = -1;
static int     g_c_clock_warn = -1;
static uint8_t g_c_dice  = 0xFF;
static const TriviaQuestion* g_c_q = nullptr;
static int     g_c_qsig  = -1;
static char    g_fmt[192];

static void reset_caches() {
    memset(g_drawn_hl, 0xFF, sizeof(g_drawn_hl));
    g_c_pawn_cur = -1;
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        g_c_pawn_x[i] = g_c_pawn_y[i] = 0x7FFFFFFF;
        g_c_pawn_vis[i] = -1;
        g_c_row_sig[i] = -1;
    }
    g_c_roll = -1;
    g_c_layers = -1;
    g_c_hud_team = -1;
    g_c_clock_warn = -1;
    g_c_dice = 0xFF;
    g_c_q = nullptr;
    g_c_qsig = -1;
}

// ===========================================================================
// 8. Helpers LVGL
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

static inline void set_bg(lv_obj_t* o, uint32_t c, lv_opa_t opa = LV_OPA_COVER) {
    lv_obj_set_style_bg_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, opa, LV_PART_MAIN);
}

static inline void set_border(lv_obj_t* o, uint32_t c, int w, lv_opa_t opa = LV_OPA_COVER) {
    lv_obj_set_style_border_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, w, LV_PART_MAIN);
    lv_obj_set_style_border_opa(o, opa, LV_PART_MAIN);
}

static inline void set_color(lv_obj_t* o, uint32_t c) {
    lv_obj_set_style_text_color(o, lv_color_hex(c), LV_PART_MAIN);
}

static inline void set_pressed_bg(lv_obj_t* o, uint32_t c) {
    lv_obj_set_style_bg_color(o, lv_color_hex(c),
                              (lv_style_selector_t) LV_PART_MAIN |
                              (lv_style_selector_t) LV_STATE_PRESSED);
}

// N'écrit que si le texte change : évite de reconstruire le layout LVGL à 30 Hz.
static void set_text_if(lv_obj_t* l, const char* t) {
    if (!l || !t) return;
    const char* cur = lv_label_get_text(l);
    if (cur && strcmp(cur, t) == 0) return;
    lv_label_set_text(l, t);
}

// Une part de camembert = un arc de 60° dont l'épaisseur vaut le rayon, ce qui
// le rend plein jusqu'au centre. Le fond et le bouton de l'arc sont neutralisés :
// on ne veut qu'un secteur coloré, pas un widget interactif.
static lv_obj_t* mk_wedge(lv_obj_t* parent, int d, int idx) {
    lv_obj_t* a = lv_arc_create(parent);
    lv_obj_set_size(a, d, d);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(a, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(a, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);      // pas d'anneau de fond
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);       // pas de poignée
    lv_obj_set_style_pad_all(a, 0, LV_PART_KNOB);
    lv_obj_set_style_arc_width(a, d / 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, false, LV_PART_INDICATOR);
    lv_arc_set_mode(a, LV_ARC_MODE_NORMAL);
    lv_arc_set_rotation(a, 270);                                   // part 0 à midi
    lv_arc_set_bg_angles(a, idx * 60, (idx + 1) * 60);
    lv_arc_set_angles(a, idx * 60, (idx + 1) * 60);
    lv_obj_set_style_arc_color(a, lv_color_hex(CAT_COLORS[idx]), LV_PART_INDICATOR);
    return a;
}

// Repositionne / redimensionne un camembert et peint ses parts selon le masque.
static void pie_set(lv_obj_t* const* wedges, lv_obj_t* rim, int x, int y, int d, uint8_t mask) {
    for (int c = 0; c < TRIVIA_NCAT; c++) {
        lv_obj_set_pos(wedges[c], x, y);
        lv_obj_set_size(wedges[c], d, d);
        lv_obj_set_style_arc_width(wedges[c], d / 2, LV_PART_INDICATOR);
        lv_obj_set_style_arc_opa(wedges[c],
            (mask & (1 << c)) ? (lv_opa_t) LV_OPA_COVER : (lv_opa_t) 45,
            LV_PART_INDICATOR);
    }
    if (rim) {
        lv_obj_set_pos(rim, x - 2, y - 2);
        lv_obj_set_size(rim, d + 4, d + 4);
        lv_obj_set_style_radius(rim, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    }
}

static void pie_show(lv_obj_t* const* wedges, lv_obj_t* rim, bool v) {
    for (int c = 0; c < TRIVIA_NCAT; c++) show(wedges[c], v);
    show(rim, v);
}

// ===========================================================================
// 9. Callbacks (déclarés ici, définis après la logique de jeu)
// ===========================================================================
static void cell_cb(lv_event_t* e);
static void roll_cb(lv_event_t* e);
static void menu_cb(lv_event_t* e);
static void slot_cb(lv_event_t* e);
static void answer_cb(lv_event_t* e);
static void qlayer_cb(lv_event_t* e);

// ===========================================================================
// 10. Construction de l'UI (une seule fois)
// ===========================================================================
static void build_board_ui() {
    lv_obj_t* b = g_ui.board;

    // Disque décoratif : donne son assise à la roue (sinon 73 pastilles flottent).
    lv_obj_t* deco = mk_rect(b);
    lv_obj_set_size(deco, 2 * (R_RING + 30), 2 * (R_RING + 30));
    lv_obj_set_pos(deco, CX - R_RING - 30, CY - R_RING - 30);
    lv_obj_set_style_radius(deco, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    set_bg(deco, Pal::RING_DECO, LV_OPA_COVER);
    set_border(deco, Pal::CARD_EDGE, 2, LV_OPA_70);

    lv_obj_t* halo = mk_rect(b);
    lv_obj_set_size(halo, 2 * (SPOKE_R[0] + 24), 2 * (SPOKE_R[0] + 24));
    lv_obj_set_pos(halo, CX - SPOKE_R[0] - 24, CY - SPOKE_R[0] - 24);
    lv_obj_set_style_radius(halo, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    set_bg(halo, Pal::FLOOR_BG, LV_OPA_COVER);
    set_border(halo, Pal::CARD_EDGE, 2, LV_OPA_50);

    // 73 cases
    int roll_i = 0;
    for (int n = 0; n < TRIVIA_NODES; n++) {
        int cx, cy, d = node_diam(n);
        node_pos(n, &cx, &cy);
        lv_obj_t* c = lv_obj_create(b);
        lv_obj_remove_style_all(c);
        lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(c, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_size(c, d, d);
        lv_obj_set_pos(c, cx - d / 2, cy - d / 2);
        lv_obj_set_style_radius(c, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_add_event_cb(c, cell_cb, LV_EVENT_CLICKED, (void*) (intptr_t) n);
        g_cell[n] = c;

        // Point blanc central : marqueur des cases « Rejouer ».
        if (s_cells[n].type == CELL_ROLL && roll_i < TRIVIA_NCAT) {
            lv_obj_t* dot = mk_rect(c);
            lv_obj_set_size(dot, 12, 12);
            lv_obj_align(dot, LV_ALIGN_CENTER, 0, 0);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            set_bg(dot, Pal::TXT, LV_OPA_COVER);
            g_roll_dot[roll_i++] = dot;
        }
    }

    // Camembert décoratif du QG central : c'est l'emblème du jeu.
    for (int c = 0; c < TRIVIA_NCAT; c++)
        g_hub_pie[c] = mk_wedge(g_ui.board, D_HUB - 16, c);
    g_hub_rim = mk_rect(g_ui.board);
    lv_obj_set_style_radius(g_hub_rim, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_hub_rim, LV_OPA_TRANSP, LV_PART_MAIN);
    set_border(g_hub_rim, Pal::HUB_EDGE, 3, LV_OPA_90);
    pie_set(g_hub_pie, g_hub_rim, CX - (D_HUB - 16) / 2, CY - (D_HUB - 16) / 2, D_HUB - 16, 0x3F);

    // Pions (créés après les cases : ils doivent passer devant)
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        lv_obj_t* p = mk_rect(b);
        lv_obj_set_size(p, 22, 22);
        lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(p, PAWN_COLORS[i], LV_OPA_COVER);
        set_border(p, Pal::CELL_EDGE, 2, LV_OPA_COVER);
        show(p, false);
        g_pawn[i] = p;
    }
}

static void build_panel_ui() {
    lv_obj_t* b = g_ui.board;

    // --- Carte du dé ---
    lv_obj_t* dcard = mk_rect(b);
    lv_obj_set_pos(dcard, PX, 8);
    lv_obj_set_size(dcard, PW, 150);
    lv_obj_set_style_radius(dcard, 18, LV_PART_MAIN);
    set_bg(dcard, Pal::CARD_BG, LV_OPA_COVER);
    set_border(dcard, Pal::CARD_EDGE, 2, LV_OPA_COVER);

    g_dice_face = mk_rect(b);
    lv_obj_set_pos(g_dice_face, 740, 27);
    lv_obj_set_size(g_dice_face, 112, 112);
    lv_obj_set_style_radius(g_dice_face, 20, LV_PART_MAIN);
    set_bg(g_dice_face, Pal::DICE_BG, LV_OPA_COVER);
    set_border(g_dice_face, Pal::CARD_EDGE, 2, LV_OPA_COVER);
    // 7 points : 3 colonnes × 3 lignes, la ligne du milieu n'ayant que 3 positions.
    static const int PIP_X[7] = {22, 90, 22, 56, 90, 22, 90};
    static const int PIP_Y[7] = {22, 22, 56, 56, 56, 90, 90};
    for (int i = 0; i < 7; i++) {
        lv_obj_t* p = mk_rect(g_dice_face);
        lv_obj_set_size(p, 20, 20);
        lv_obj_set_pos(p, PIP_X[i] - 10, PIP_Y[i] - 10);
        lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(p, Pal::DICE_PIP, LV_OPA_COVER);
        g_pip[i] = p;
    }

    g_roll_btn = mk_rect(b);
    lv_obj_add_flag(g_roll_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(g_roll_btn, 872, 30);
    lv_obj_set_size(g_roll_btn, 368, 76);
    lv_obj_set_style_radius(g_roll_btn, 16, LV_PART_MAIN);
    set_bg(g_roll_btn, Pal::ACCENT, LV_OPA_COVER);
    set_pressed_bg(g_roll_btn, Pal::BTN_BG_ON);
    lv_obj_add_event_cb(g_roll_btn, roll_cb, LV_EVENT_CLICKED, nullptr);
    g_roll_lbl = mk_label(g_roll_btn, g_ui.f_mid, Pal::VOID_BG);
    lv_obj_align(g_roll_lbl, LV_ALIGN_CENTER, 0, 0);
    set_text_if(g_roll_lbl, "LANCER LE DÉ");

    g_roll_hint = mk_label(b, g_ui.f_small, Pal::TXT_MUTED);
    lv_obj_set_width(g_roll_hint, 368);
    lv_obj_set_style_text_align(g_roll_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(g_roll_hint, 872, 114);

    // --- Bandeau d'état ---
    lv_obj_t* scard = mk_rect(b);
    lv_obj_set_pos(scard, PX, 166);
    lv_obj_set_size(scard, PW, 56);
    lv_obj_set_style_radius(scard, 14, LV_PART_MAIN);
    set_bg(scard, Pal::BTN_BG, LV_OPA_COVER);
    set_border(scard, Pal::BTN_EDGE, 2, LV_OPA_COVER);
    // Enfant de la carte et centré : si un message déborde sur deux lignes, il
    // grandit symétriquement au lieu de mordre sur les fiches d'équipe.
    g_status_lbl = mk_label(scard, g_ui.f_small, Pal::TXT);
    lv_obj_set_width(g_status_lbl, PW - 32);
    lv_obj_set_style_text_align(g_status_lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(g_status_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_align(g_status_lbl, LV_ALIGN_CENTER, 0, 0);

    // --- Fiches équipes ---
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        int ry = ROW_Y0 + i * ROW_STEP;
        lv_obj_t* r = mk_rect(b);
        lv_obj_set_pos(r, PX, ry);
        lv_obj_set_size(r, PW, ROW_H);
        lv_obj_set_style_radius(r, 14, LV_PART_MAIN);
        set_bg(r, Pal::CARD_BG, LV_OPA_COVER);
        set_border(r, Pal::CARD_EDGE, 2, LV_OPA_COVER);
        g_row[i] = r;

        g_row_chip[i] = mk_rect(b);
        lv_obj_set_size(g_row_chip[i], 20, 20);
        lv_obj_set_pos(g_row_chip[i], PX + 18, ry + 17);
        lv_obj_set_style_radius(g_row_chip[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(g_row_chip[i], PAWN_COLORS[i], LV_OPA_COVER);

        g_row_name[i] = mk_label(b, g_ui.f_small, Pal::TXT);
        lv_obj_set_pos(g_row_name[i], PX + 48, ry + 15);

        g_row_cnt[i] = mk_label(b, g_ui.f_small, Pal::TXT_DIM);
        lv_obj_set_width(g_row_cnt[i], 72);
        lv_obj_set_style_text_align(g_row_cnt[i], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        lv_obj_set_pos(g_row_cnt[i], 1104, ry + 15);

        for (int c = 0; c < TRIVIA_NCAT; c++) g_row_pie[i][c] = mk_wedge(b, 44, c);
        g_row_rim[i] = mk_rect(b);
        lv_obj_set_style_bg_opa(g_row_rim[i], LV_OPA_TRANSP, LV_PART_MAIN);
        set_border(g_row_rim[i], Pal::CARD_EDGE, 2, LV_OPA_COVER);
        pie_set(g_row_pie[i], g_row_rim[i], 1192, ry + 5, 44, 0);
    }

    // --- Légende des catégories ---
    lv_obj_t* lcard = mk_rect(b);
    lv_obj_set_pos(lcard, PX, 594);
    lv_obj_set_size(lcard, PW, 66);
    lv_obj_set_style_radius(lcard, 14, LV_PART_MAIN);
    set_bg(lcard, Pal::CARD_BG, LV_OPA_COVER);
    set_border(lcard, Pal::CARD_EDGE, 2, LV_OPA_COVER);
    for (int c = 0; c < TRIVIA_NCAT; c++) {
        int col = c % 3, row = c / 3;
        int lx = PX + 16 + col * 172, ly = 604 + row * 28;
        lv_obj_t* dot = mk_rect(b);
        lv_obj_set_size(dot, 12, 12);
        lv_obj_set_pos(dot, lx, ly + 7);
        lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(dot, CAT_COLORS[c], LV_OPA_COVER);
        lv_obj_t* l = mk_label(b, g_ui.f_small, Pal::TXT_DIM);
        lv_obj_set_pos(l, lx + 20, ly);
        set_text_if(l, CAT_SHORT[c]);
    }
}

static void build_hud_ui() {
    lv_obj_t* h = g_ui.hud;

    lv_obj_t* t = mk_label(h, g_ui.f_small, Pal::ACCENT);
    lv_obj_set_pos(t, 18, 12);
    set_text_if(t, "TRIAL POURSUITE");

    g_h_turn = mk_label(h, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_set_pos(g_h_turn, 250, 12);

    g_h_team = mk_label(h, g_ui.f_small, Pal::TXT);
    lv_obj_set_pos(g_h_team, 396, 12);

    g_h_clock = mk_label(h, g_ui.f_small, Pal::ACCENT);
    lv_obj_set_width(g_h_clock, 220);
    lv_obj_set_style_text_align(g_h_clock, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_set_pos(g_h_clock, 906, 12);

    g_menu_btn = mk_rect(h);
    lv_obj_add_flag(g_menu_btn, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_pos(g_menu_btn, 1146, 6);
    lv_obj_set_size(g_menu_btn, 118, 36);
    lv_obj_set_style_radius(g_menu_btn, 10, LV_PART_MAIN);
    set_bg(g_menu_btn, Pal::BTN_BG, LV_OPA_COVER);
    set_pressed_bg(g_menu_btn, Pal::BTN_BG_ON);
    set_border(g_menu_btn, Pal::BTN_EDGE, 2, LV_OPA_COVER);
    lv_obj_add_event_cb(g_menu_btn, menu_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t* ml = mk_label(g_menu_btn, g_ui.f_small, Pal::TXT);
    lv_obj_align(ml, LV_ALIGN_CENTER, 0, 0);
    set_text_if(ml, "Menu");
}

static void build_question_ui() {
    // Calque plein écran : voile sombre + carte. Il absorbe les taps (« continuer »
    // pendant le verdict) — la carte, elle, n'est pas cliquable pour que le tap
    // traverse jusqu'au calque.
    g_qlayer = mk_rect(g_ui.root);
    lv_obj_set_pos(g_qlayer, 0, 0);
    lv_obj_set_size(g_qlayer, 1280, 720);
    set_bg(g_qlayer, Pal::VOID_BG, (lv_opa_t) 220);
    lv_obj_add_flag(g_qlayer, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_qlayer, qlayer_cb, LV_EVENT_CLICKED, nullptr);
    show(g_qlayer, false);

    g_qcard = mk_rect(g_qlayer);
    lv_obj_set_pos(g_qcard, 60, 60);
    lv_obj_set_size(g_qcard, 1160, 600);
    lv_obj_set_style_radius(g_qcard, 22, LV_PART_MAIN);
    set_bg(g_qcard, Pal::FLOOR_BG, LV_OPA_COVER);
    set_border(g_qcard, Pal::CARD_EDGE, 3, LV_OPA_COVER);

    g_qban = mk_rect(g_qcard);
    lv_obj_set_pos(g_qban, 0, 0);
    lv_obj_set_size(g_qban, 1160, 74);
    lv_obj_set_style_radius(g_qban, 22, LV_PART_MAIN);
    // Le bandeau reprend le rayon de 22 px de la carte pour épouser ses coins
    // hauts ; ce cache carré, de la MEME couleur, redresse les deux coups bas.
    g_qban_fix = mk_rect(g_qcard);
    lv_obj_set_pos(g_qban_fix, 0, 52);
    lv_obj_set_size(g_qban_fix, 1160, 22);
    lv_obj_set_style_radius(g_qban_fix, 0, LV_PART_MAIN);

    g_qban_cat = mk_label(g_qban, g_ui.f_mid, Pal::VOID_BG);
    lv_obj_align(g_qban_cat, LV_ALIGN_LEFT_MID, 32, 0);
    g_qban_side = mk_label(g_qban, g_ui.f_small, Pal::VOID_BG);
    lv_obj_align(g_qban_side, LV_ALIGN_RIGHT_MID, -32, 0);

    g_qbar = mk_rect(g_qcard);
    lv_obj_set_pos(g_qbar, 0, 74);
    lv_obj_set_size(g_qbar, 1160, 8);
    set_bg(g_qbar, Pal::BTN_BG, LV_OPA_COVER);
    g_qbar_fill = mk_rect(g_qbar);
    lv_obj_set_pos(g_qbar_fill, 0, 0);
    lv_obj_set_size(g_qbar_fill, 1160, 8);
    set_bg(g_qbar_fill, Pal::GOOD, LV_OPA_COVER);

    g_qtext = mk_label(g_qcard, g_ui.f_mid, Pal::TXT);
    lv_obj_set_width(g_qtext, 1064);
    lv_obj_set_style_text_align(g_qtext, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(g_qtext, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(g_qtext, 48, 110);

    for (int i = 0; i < 4; i++) {
        int bx = 48 + (i % 2) * 548;
        int by = 272 + (i / 2) * 124;
        g_ans[i] = mk_rect(g_qcard);
        lv_obj_add_flag(g_ans[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_pos(g_ans[i], bx, by);
        lv_obj_set_size(g_ans[i], 516, 112);
        lv_obj_set_style_radius(g_ans[i], 16, LV_PART_MAIN);
        set_bg(g_ans[i], Pal::BTN_BG, LV_OPA_COVER);
        set_pressed_bg(g_ans[i], Pal::BTN_BG_ON);
        set_border(g_ans[i], Pal::BTN_EDGE, 2, LV_OPA_COVER);
        lv_obj_add_event_cb(g_ans[i], answer_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        g_ans_lbl[i] = mk_label(g_ans[i], g_ui.f_mid, Pal::TXT);
        lv_obj_set_width(g_ans_lbl[i], 476);
        lv_obj_set_style_text_align(g_ans_lbl[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_label_set_long_mode(g_ans_lbl[i], LV_LABEL_LONG_WRAP);
        lv_obj_align(g_ans_lbl[i], LV_ALIGN_CENTER, 0, 0);
    }

    g_qfeed = mk_label(g_qcard, g_ui.f_mid, Pal::TXT);
    lv_obj_set_width(g_qfeed, 1064);
    lv_obj_set_style_text_align(g_qfeed, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(g_qfeed, LV_LABEL_LONG_WRAP);
    lv_obj_set_pos(g_qfeed, 48, 522);
}

static void build_menu_ui() {
    lv_obj_t* p = g_ui.panel;
    set_bg(p, Pal::VOID_BG, (lv_opa_t) 240);
    lv_obj_add_flag(p, LV_OBJ_FLAG_CLICKABLE);   // absorbe les taps vers le plateau
    lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
    show(p, false);

    g_m_title = mk_label(p, g_ui.f_big, Pal::ACCENT);
    lv_obj_align(g_m_title, LV_ALIGN_TOP_MID, 0, 28);
    g_m_sub = mk_label(p, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_align(g_m_sub, LV_ALIGN_TOP_MID, 0, 86);
    g_m_body = mk_label(p, g_ui.f_small, Pal::TXT);
    lv_obj_set_width(g_m_body, 1100);
    lv_obj_set_style_text_align(g_m_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(g_m_body, LV_LABEL_LONG_WRAP);
    lv_obj_align(g_m_body, LV_ALIGN_TOP_MID, 0, 130);
    g_m_foot = mk_label(p, g_ui.f_small, Pal::TXT_MUTED);
    lv_obj_align(g_m_foot, LV_ALIGN_BOTTOM_MID, 0, -16);

    for (int i = 0; i < N_SLOTS; i++) {
        g_slot[i] = mk_rect(p);
        lv_obj_add_flag(g_slot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(g_slot[i], 14, LV_PART_MAIN);
        set_bg(g_slot[i], Pal::BTN_BG, LV_OPA_COVER);
        set_pressed_bg(g_slot[i], Pal::BTN_BG_ON);
        lv_obj_add_event_cb(g_slot[i], slot_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        g_slot_t[i] = mk_label(g_slot[i], g_ui.f_mid, Pal::TXT);
        g_slot_d[i] = mk_label(g_slot[i], g_ui.f_small, Pal::TXT_DIM);
        show(g_slot[i], false);
    }
    for (int i = 0; i < N_FREE; i++) {
        g_free[i] = mk_label(p, g_ui.f_small, Pal::ACCENT);
        show(g_free[i], false);
    }
    for (int c = 0; c < TRIVIA_NCAT; c++) {
        g_bar[c] = mk_rect(p);
        lv_obj_set_size(g_bar[c], 480, 16);
        lv_obj_set_style_radius(g_bar[c], 8, LV_PART_MAIN);
        set_bg(g_bar[c], Pal::BTN_BG, LV_OPA_COVER);
        g_bar_fill[c] = mk_rect(g_bar[c]);
        lv_obj_set_pos(g_bar_fill[c], 0, 0);
        lv_obj_set_size(g_bar_fill[c], 1, 16);
        lv_obj_set_style_radius(g_bar_fill[c], 8, LV_PART_MAIN);
        set_bg(g_bar_fill[c], CAT_COLORS[c], LV_OPA_COVER);
        show(g_bar[c], false);
    }
    for (int c = 0; c < TRIVIA_NCAT; c++) g_big_pie[c] = mk_wedge(p, 120, c);
    g_big_rim = mk_rect(p);
    lv_obj_set_style_bg_opa(g_big_rim, LV_OPA_TRANSP, LV_PART_MAIN);
    set_border(g_big_rim, Pal::ACCENT, 3, LV_OPA_90);
    pie_show(g_big_pie, g_big_rim, false);
}

static void build_ui() {
    if (g_built) return;
    build_board();
    set_bg(g_ui.root,  Pal::VOID_BG);
    set_bg(g_ui.hud,   Pal::HUD_BG);
    set_bg(g_ui.board, Pal::FLOOR_BG);
    build_board_ui();
    build_panel_ui();
    build_hud_ui();
    build_question_ui();
    build_menu_ui();
    reset_caches();
    g_built = true;
}

// ===========================================================================
// 11. Mise en page générique des entrées de menu
// ===========================================================================
static void slot_set(int i, int x, int y, int w, int h,
                     const char* title, const char* desc = nullptr,
                     uint32_t bg = Pal::BTN_BG, uint32_t border = Pal::BTN_EDGE,
                     uint32_t tcol = Pal::TXT, bool clickable = true, bool left = false) {
    if (i < 0 || i >= N_SLOTS) return;
    lv_obj_t* s = g_slot[i];
    lv_obj_set_pos(s, x, y);
    lv_obj_set_size(s, w, h);
    // Rayon remis à plat : l'écran de composition détourne certains slots en
    // pastilles rondes, l'écran suivant en hériterait sinon.
    lv_obj_set_style_radius(s, 14, LV_PART_MAIN);
    set_bg(s, bg, LV_OPA_COVER);
    set_pressed_bg(s, clickable ? Pal::BTN_BG_ON : bg);
    set_border(s, border, 2, LV_OPA_COVER);
    if (clickable) lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
    else           lv_obj_clear_flag(s, LV_OBJ_FLAG_CLICKABLE);

    bool has_desc = (desc && desc[0]);
    // Le titre passe en petite police sur les puces basses (chips, ± , légendes).
    esphome::lvgl::lv_obj_set_style_text_font(g_slot_t[i],
        (h >= 56 && !has_desc) || h >= 62 ? g_ui.f_mid : g_ui.f_small, LV_PART_MAIN);
    set_color(g_slot_t[i], tcol);
    set_text_if(g_slot_t[i], title ? title : "");
    set_text_if(g_slot_d[i], has_desc ? desc : "");
    show(g_slot_d[i], has_desc);

    if (left) {
        lv_obj_align(g_slot_t[i], LV_ALIGN_LEFT_MID, 20, has_desc ? -13 : 0);
        lv_obj_align(g_slot_d[i], LV_ALIGN_LEFT_MID, 20, 15);
    } else {
        lv_obj_align(g_slot_t[i], LV_ALIGN_CENTER, 0, has_desc ? -13 : 0);
        lv_obj_align(g_slot_d[i], LV_ALIGN_CENTER, 0, 15);
    }
    show(s, true);
}

// Entrée de liste centrée, gabarit commun au hub / à la pause / à la victoire.
static void slot_list(int i, int rank, const char* title, const char* desc,
                      bool enabled = true, uint32_t tcol = Pal::TXT) {
    slot_set(i, 290, 240 + rank * 72, 700, 64, title, desc,
             Pal::BTN_BG, enabled ? Pal::BTN_EDGE : Pal::CARD_EDGE,
             enabled ? tcol : Pal::TXT_MUTED, enabled);
}

// Puce de réglage : allumée = liseré + texte à l'accent.
static void slot_opt(int i, int x, int y, int w, int h, const char* title,
                     const char* desc, bool on) {
    slot_set(i, x, y, w, h, title, desc,
             on ? Pal::BTN_BG_ON : Pal::BTN_BG,
             on ? Pal::ACCENT : Pal::BTN_EDGE,
             on ? Pal::ACCENT : Pal::TXT_DIM, true);
}

static void slots_hide_from(int n) {
    for (int i = n; i < N_SLOTS; i++) show(g_slot[i], false);
}
static void free_lbl(int i, int x, int y, const char* txt, uint32_t col = Pal::ACCENT) {
    if (i < 0 || i >= N_FREE) return;
    lv_obj_set_pos(g_free[i], x, y);
    set_color(g_free[i], col);
    set_text_if(g_free[i], txt);
    show(g_free[i], true);
}
static void free_hide_from(int n) {
    for (int i = n; i < N_FREE; i++) show(g_free[i], false);
}
static void bars_hide() {
    for (int c = 0; c < TRIVIA_NCAT; c++) show(g_bar[c], false);
}
// Titre + sous-titre : le hub les descend pour laisser place à son camembert.
static void menu_head(int title_y, int sub_y) {
    lv_obj_align(g_m_title, LV_ALIGN_TOP_MID, 0, title_y);
    lv_obj_align(g_m_sub,   LV_ALIGN_TOP_MID, 0, sub_y);
}

// ===========================================================================
// 12. Tirage des questions
// ===========================================================================
static void init_bags() {
    for (int c = 0; c < TRIVIA_NCAT; c++) {
        uint8_t n = TRIVIA_BANK_SIZES[c];
        if (n > (uint8_t) sizeof(s_bag[c])) n = (uint8_t) sizeof(s_bag[c]);
        for (int i = 0; i < n; i++) s_bag[c][i] = (uint8_t) i;
        for (int i = n - 1; i > 0; i--) {          // Fisher-Yates
            int j = rnd_range(0, i);
            uint8_t t = s_bag[c][i]; s_bag[c][i] = s_bag[c][j]; s_bag[c][j] = t;
        }
        s_bag_pos[c] = 0;
    }
}

// Tire la prochaine question de la catégorie qui satisfait le masque de
// difficulté, en avançant le curseur du sac (sans remise). Repasse à zéro quand
// le sac est épuisé.
static const TriviaQuestion* draw_question(uint8_t cat, uint8_t mask) {
    uint8_t n = TRIVIA_BANK_SIZES[cat];
    if (n > (uint8_t) sizeof(s_bag[cat])) n = (uint8_t) sizeof(s_bag[cat]);
    const TriviaQuestion* bank = TRIVIA_BANKS[cat];
    for (int k = 0; k < n; k++) {
        uint8_t idx = s_bag[cat][(s_bag_pos[cat] + k) % n];
        if ((mask >> bank[idx].difficulty) & 1) {
            s_bag_pos[cat] = (uint8_t) ((s_bag_pos[cat] + k + 1) % n);
            return &bank[idx];
        }
    }
    // Masque trop restrictif pour cette banque : on retombe sur n'importe quelle question.
    uint8_t idx = s_bag[cat][s_bag_pos[cat] % n];
    s_bag_pos[cat] = (uint8_t) ((s_bag_pos[cat] + 1) % n);
    return &bank[idx];
}

static void prepare_question(uint8_t cat) {
    s_q_cat = cat;
    s_q = draw_question(cat, DIFF_MASK[s_difficulty]);
    for (int i = 0; i < 4; i++) s_slot_of[i] = (uint8_t) i;
    for (int i = 3; i > 0; i--) {
        int j = rnd_range(0, i);
        uint8_t t = s_slot_of[i]; s_slot_of[i] = s_slot_of[j]; s_slot_of[j] = t;
    }
    s_correct_slot = 0;
    for (int i = 0; i < 4; i++) if (s_slot_of[i] == 0) { s_correct_slot = (uint8_t) i; break; }
    s_picked_slot = 0xFF;
    s_correct = false;
    s_won_wedge = false;
    s_q_t0 = esphome::millis();
    s_state = ST_QUESTION;
}

static const char* choice_text(int slot) {
    if (!s_q) return "";
    switch (s_slot_of[slot]) {
        case 0:  return s_q->a;
        case 1:  return s_q->w1;
        case 2:  return s_q->w2;
        default: return s_q->w3;
    }
}

// ===========================================================================
// 13. Logique de partie
// ===========================================================================
static void msg(const char* m) {
    snprintf(s_msg, sizeof(s_msg), "%s", m);
    s_msg_until = esphome::millis() + MSG_MS;
}

static inline bool all_wedges(int t) { return (s_teams[t].wedges & 0x3F) == 0x3F; }
static inline int  n_wedges(int t) {
    int n = 0; for (int c = 0; c < TRIVIA_NCAT; c++) if (s_teams[t].wedges & (1 << c)) n++;
    return n;
}

static void clear_reach() {
    memset(s_reach, 0, sizeof(s_reach));
}

// Énumère toutes les cases atteignables en EXACTEMENT `left` pas, sans demi-tour.
static void dfs_reach(int node, int prev, int left) {
    if (left == 0) { s_reach[node] = true; return; }
    int nb[8];
    int c = neighbors(node, nb);
    for (int i = 0; i < c; i++) {
        if (nb[i] == prev) continue;
        dfs_reach(nb[i], node, left - 1);
    }
}

// Reconstruit UN chemin de `left` pas menant à `dest` (pour l'animation du pion).
static bool build_path(int node, int prev, int left, int dest, int depth) {
    if (left == 0) {
        if (node != dest) return false;
        s_path_n = depth + 1;
        return true;
    }
    int nb[8];
    int c = neighbors(node, nb);
    for (int i = 0; i < c; i++) {
        if (nb[i] == prev) continue;
        s_path[depth + 1] = (uint8_t) nb[i];
        if (build_path(nb[i], node, left - 1, dest, depth + 1)) return true;
    }
    return false;
}

static void next_team() {
    s_cur = (uint8_t) ((s_cur + 1) % s_n_teams);
    if (s_cur == 0) s_turn++;
}

static void begin_roll_phase() {
    s_phase = PH_ROLL;
    clear_reach();
}

static void roll_dice() {
    if (s_state != ST_PLAY || s_phase != PH_ROLL) return;
    s_dice_spin = true;
    s_dice_t0 = esphome::millis();
    s_dice_val = (uint8_t) rnd_range(1, 6);
    s_phase = PH_ROLLING;
}

static void victory(uint8_t team) {
    s_winner = team;
    s_save.stats.games_played++;
    if (s_save.stats.best_turns == 0 || s_turn < s_save.stats.best_turns)
        s_save.stats.best_turns = s_turn;
    s_in_game = false;
    s_state = ST_VICTORY;
    persist_save();
}

// Le pion vient d'arriver : on décide de la suite.
static void resolve_cell() {
    const BoardCell& c = s_cells[s_teams[s_cur].pos];
    switch (c.type) {
        case CELL_ROLL:
            msg("Case Rejouer — relancez le dé !");
            s_state = ST_PLAY;
            begin_roll_phase();
            break;
        case CELL_HUB:
            s_is_final = all_wedges(s_cur);
            s_state = ST_CATPICK;
            break;
        case CELL_HQ:
        case CELL_CAT:
        default:
            s_is_final = false;
            prepare_question(c.cat);
            break;
    }
}

static void start_move(int dest) {
    s_path[0] = s_teams[s_cur].pos;
    s_path_n = 1;
    if (!build_path(s_teams[s_cur].pos, -1, s_dice_val, dest, 0)) {
        // Ne devrait pas arriver : dest sort de dfs_reach avec le même dé.
        s_path[1] = (uint8_t) dest;
        s_path_n = 2;
    }
    s_hop = 0;
    s_hop_t0 = esphome::millis();
    s_phase = PH_MOVING;
    clear_reach();
}

static void finish_move() {
    s_teams[s_cur].pos = s_path[s_path_n - 1];
    resolve_cell();
}

// Verdict d'une réponse. slot 0xFF = temps écoulé.
static void answer(uint8_t slot) {
    if (s_state != ST_QUESTION || !s_q) return;
    s_picked_slot = slot;
    s_correct = (slot == s_correct_slot);
    s_won_wedge = false;

    if (s_correct) {
        s_save.stats.q_ok++; s_save.stats.cat_ok[s_q_cat]++; s_game_ok++;
        if (!s_is_final) {
            const BoardCell& c = s_cells[s_teams[s_cur].pos];
            if (c.type == CELL_HQ && !(s_teams[s_cur].wedges & (1 << c.cat))) {
                s_teams[s_cur].wedges |= (uint8_t) (1 << c.cat);
                s_save.stats.wedges_won++;
                s_won_wedge = true;
            }
        }
    } else {
        s_save.stats.q_ko++; s_save.stats.cat_ko[s_q_cat]++; s_game_ko++;
    }
    s_state = ST_REVEAL;
    s_reveal_until = esphome::millis() + (s_correct ? REVEAL_OK_MS : REVEAL_KO_MS);
}

static void end_reveal() {
    if (s_is_final) {
        s_is_final = false;
        if (s_correct) { victory(s_cur); return; }
        msg("Finale manquée — il faudra revenir au centre.");
        next_team();
    } else if (s_correct) {
        if (s_won_wedge) msg("Part gagnée ! Vous rejouez.");
        else             msg("Bonne réponse — vous rejouez.");
    } else {
        next_team();
    }
    s_state = ST_PLAY;
    begin_roll_phase();
}

static void start_game(uint8_t n, const Team* roster) {
    s_n_teams = (n < 1) ? 1 : (n > TRIVIA_MAX_TEAMS ? TRIVIA_MAX_TEAMS : n);
    for (int i = 0; i < s_n_teams; i++) {
        copy_name(s_teams[i].name, roster[i].name);
        s_teams[i].color_idx = roster[i].color_idx;
        s_teams[i].wedges = 0;
        s_teams[i].pos = HUB_NODE;   // départ au centre, comme au vrai TP
    }
    s_cur = 0;
    s_turn = 1;
    s_game_ok = s_game_ko = 0;
    s_is_final = false;
    s_rng ^= esphome::millis() * 2654435761u;
    if (s_rng == 0) s_rng = 0xDEADBEEFu;
    init_bags();
    s_in_game = true;
    s_state = ST_PLAY;
    begin_roll_phase();
    reset_caches();
    msg("Que la partie commence !");
}

static bool resume_game() {
    if (s_save.n_teams == 0) return false;
    s_n_teams = s_save.n_teams;
    s_cur     = s_save.current_team;
    s_turn    = s_save.turn_num ? s_save.turn_num : 1;
    s_rng     = s_save.rng_seed ? s_save.rng_seed : 0xDEADBEEFu;
    s_game_ok = s_save.game_ok;
    s_game_ko = s_save.game_ko;
    for (int i = 0; i < s_n_teams; i++) {
        copy_name(s_teams[i].name, s_save.teams[i].name);
        if (s_teams[i].name[0] == '\0')
            snprintf(s_teams[i].name, sizeof(s_teams[i].name), "%s", PRESET_NAMES[i]);
        s_teams[i].color_idx = s_save.teams[i].color_idx;
        s_teams[i].wedges    = s_save.teams[i].wedges;
        s_teams[i].pos       = s_save.teams[i].pos;
    }
    init_bags();      // les sacs ne sont pas persistés : on remélange
    s_is_final = false;
    s_in_game = true;
    s_state = ST_PLAY;
    begin_roll_phase();
    reset_caches();
    return true;
}

// ===========================================================================
// 14. Écrans de menu
// ===========================================================================
static void render_hub() {
    menu_head(138, 196);
    set_text_if(g_m_title, "TRIAL POURSUITE");
    set_text_if(g_m_sub, "Le quiz rétro-salon — 720 questions, 6 catégories");
    set_text_if(g_m_body, "");
    set_text_if(g_m_foot, "");
    pie_set(g_big_pie, g_big_rim, 640 - 55, 20, 110, 0x3F);
    pie_show(g_big_pie, g_big_rim, true);

    slot_list(0, 0, "Nouvelle partie", "1 à 6 équipes");
    if (s_in_game || s_save.n_teams > 0) {
        uint8_t n = s_in_game ? s_n_teams : s_save.n_teams;
        uint8_t t = s_in_game ? s_turn : s_save.turn_num;
        snprintf(g_fmt, sizeof(g_fmt), "%u équipes · tour %u", (unsigned) n, (unsigned) t);
        slot_list(1, 1, "Reprendre la partie", g_fmt);
    } else {
        slot_list(1, 1, "Reprendre la partie", "Aucune partie sauvegardée", false);
    }
    uint32_t tot = s_save.stats.q_ok + s_save.stats.q_ko;
    if (tot > 0) snprintf(g_fmt, sizeof(g_fmt), "%u parties · %u %% de réussite",
                          (unsigned) s_save.stats.games_played,
                          (unsigned) (s_save.stats.q_ok * 100 / tot));
    else snprintf(g_fmt, sizeof(g_fmt), "Aucune question jouée");
    slot_list(2, 2, "Statistiques", g_fmt);
    slot_list(3, 3, "Règles du jeu", "Comment gagner ses 6 parts");
    snprintf(g_fmt, sizeof(g_fmt), "%s · %s", DIFF_NAME[s_difficulty], TIMER_NAME[s_timer_idx]);
    slot_list(4, 4, "Réglages", g_fmt);
    slot_list(5, 5, "Quitter", "Retour au Tab", true, Pal::BAD);
    slots_hide_from(6);
    free_hide_from(0);
    bars_hide();
}

static void render_setup() {
    menu_head(28, 86);
    set_text_if(g_m_title, "NOUVELLE PARTIE");
    set_text_if(g_m_sub, "Composez les équipes, puis réglez les questions");
    set_text_if(g_m_body, "");
    set_text_if(g_m_foot, "Touchez un nom pour le changer, la pastille pour la couleur");
    pie_show(g_big_pie, g_big_rim, false);
    bars_hide();

    free_lbl(0, 72, 126, "ÉQUIPES");
    snprintf(g_fmt, sizeof(g_fmt), "%u", (unsigned) s_setup_n);
    free_lbl(1, 556, 126, g_fmt, Pal::TXT);

    // Slots 0..11 : 6 lignes d'équipe (pastille couleur + nom)
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        int y = 170 + i * 62;
        bool on = (i < s_setup_n);
        slot_set(2 * i, 72, y, 56, 56, "", nullptr,
                 on ? PAWN_COLORS[s_setup_teams[i].color_idx] : Pal::BTN_BG,
                 on ? Pal::TXT : Pal::CARD_EDGE, Pal::TXT, on);
        lv_obj_set_style_radius(g_slot[2 * i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        if (on) {
            slot_set(2 * i + 1, 140, y, 496, 56, s_setup_teams[i].name, nullptr,
                     Pal::BTN_BG, Pal::BTN_EDGE, Pal::TXT, true, true);
        } else {
            slot_set(2 * i + 1, 140, y, 496, 56,
                     (i == s_setup_n) ? "+ Ajouter une équipe" : "—", nullptr,
                     Pal::BTN_BG, Pal::CARD_EDGE,
                     (i == s_setup_n) ? Pal::TXT_DIM : Pal::TXT_MUTED, (i == s_setup_n), true);
        }
    }
    slot_set(24, 486, 118, 46, 40, "−", nullptr, Pal::BTN_BG, Pal::BTN_EDGE,
             s_setup_n > 1 ? Pal::TXT : Pal::TXT_MUTED, s_setup_n > 1);
    slot_set(25, 590, 118, 46, 40, "+", nullptr, Pal::BTN_BG, Pal::BTN_EDGE,
             s_setup_n < TRIVIA_MAX_TEAMS ? Pal::TXT : Pal::TXT_MUTED,
             s_setup_n < TRIVIA_MAX_TEAMS);

    free_lbl(2, 680, 126, "DIFFICULTÉ");
    for (int i = 0; i < 3; i++)
        slot_opt(12 + i, 680 + i * 180, 162, 168, 64, DIFF_NAME[i], nullptr, s_difficulty == i);
    free_lbl(3, 680, 246, "TEMPS DE RÉPONSE");
    for (int i = 0; i < 4; i++)
        slot_opt(15 + i, 680 + i * 135, 284, 123, 64, TIMER_NAME[i], nullptr, s_timer_idx == i);
    free_lbl(4, 680, 368, "SECOUSSE = LANCER LE DÉ");
    slot_opt(19, 680, 406, 258, 64, "Activée", nullptr, s_shake_on);
    slot_opt(20, 950, 406, 258, 64, "Désactivée", nullptr, !s_shake_on);

    slot_set(21, 680, 506, 528, 80, "COMMENCER LA PARTIE", nullptr,
             Pal::ACCENT, Pal::ACCENT, Pal::VOID_BG, true);
    slot_set(22, 680, 600, 528, 60, "Retour au menu");
    show(g_slot[23], false);
    free_hide_from(5);
}

static void render_rules() {
    menu_head(28, 86);
    set_text_if(g_m_title, "RÈGLES DU JEU");
    set_text_if(g_m_sub, "");
    set_text_if(g_m_body, RULES_TEXT);
    set_text_if(g_m_foot, "");
    pie_show(g_big_pie, g_big_rim, false);
    bars_hide();
    free_hide_from(0);
    slot_set(0, 440, 604, 400, 60, "Retour");
    slots_hide_from(1);
}

static void render_stats() {
    menu_head(28, 86);
    set_text_if(g_m_title, "STATISTIQUES");
    uint32_t tot = s_save.stats.q_ok + s_save.stats.q_ko;
    if (tot > 0) snprintf(g_fmt, sizeof(g_fmt),
                          "%u parties · %u questions · %u %% de réussite · %u parts gagnées",
                          (unsigned) s_save.stats.games_played, (unsigned) tot,
                          (unsigned) (s_save.stats.q_ok * 100 / tot),
                          (unsigned) s_save.stats.wedges_won);
    else snprintf(g_fmt, sizeof(g_fmt), "Aucune question jouée pour le moment");
    set_text_if(g_m_sub, g_fmt);
    if (s_save.stats.best_turns > 0) {
        snprintf(g_fmt, sizeof(g_fmt), "Victoire la plus rapide : %u tours",
                 (unsigned) s_save.stats.best_turns);
        set_text_if(g_m_foot, g_fmt);
    } else set_text_if(g_m_foot, "");
    set_text_if(g_m_body, "");
    pie_show(g_big_pie, g_big_rim, false);
    free_hide_from(0);

    for (int c = 0; c < TRIVIA_NCAT; c++) {
        int y = 140 + c * 66;
        uint32_t ok = s_save.stats.cat_ok[c], ko = s_save.stats.cat_ko[c];
        uint32_t n = ok + ko;
        int pct = n ? (int) (ok * 100 / n) : 0;
        snprintf(g_fmt, sizeof(g_fmt), "%u / %u  ·  %d %%", (unsigned) ok, (unsigned) n, pct);
        slot_set(c, 200, y, 880, 56, CAT_NAMES[c], nullptr,
                 Pal::CARD_BG, Pal::CARD_EDGE, CAT_COLORS[c], false, true);
        set_text_if(g_slot_d[c], "");
        show(g_slot_d[c], false);
        lv_obj_set_pos(g_bar[c], 560, y + 20);
        lv_obj_set_size(g_bar_fill[c], n ? (1 + 479 * pct / 100) : 1, 16);
        show(g_bar[c], true);
        // Le pourcentage est écrit dans le libellé de droite du bandeau d'état
        // du slot : ici on réutilise g_free pour ne pas empiler un 4e objet/ligne.
        free_lbl(c, 1090, y + 16, g_fmt, Pal::TXT_DIM);
    }
    free_hide_from(TRIVIA_NCAT);
    slot_set(6, 340, 556, 600, 56, "Effacer les statistiques", nullptr,
             Pal::BTN_BG, Pal::BTN_EDGE, Pal::BAD);
    slot_set(7, 340, 622, 600, 56, "Retour");
    slots_hide_from(8);
}

static void render_settings() {
    menu_head(28, 86);
    set_text_if(g_m_title, "RÉGLAGES");
    set_text_if(g_m_sub, "Ces réglages s'appliquent à la prochaine question");
    set_text_if(g_m_body, "");
    set_text_if(g_m_foot, "");
    pie_show(g_big_pie, g_big_rim, false);
    bars_hide();

    free_lbl(0, 376, 152, "DIFFICULTÉ");
    for (int i = 0; i < 3; i++)
        slot_opt(i, 376 + i * 180, 190, 168, 64, DIFF_NAME[i], nullptr, s_difficulty == i);
    free_lbl(1, 376, 262, DIFF_DESC[s_difficulty], Pal::TXT_DIM);
    free_lbl(2, 376, 300, "TEMPS DE RÉPONSE");
    for (int i = 0; i < 4; i++)
        slot_opt(3 + i, 376 + i * 135, 338, 123, 64, TIMER_NAME[i], nullptr, s_timer_idx == i);
    free_lbl(3, 376, 420, "SECOUSSE = LANCER LE DÉ");
    slot_opt(7, 376, 458, 258, 64, "Activée", nullptr, s_shake_on);
    slot_opt(8, 646, 458, 258, 64, "Désactivée", nullptr, !s_shake_on);
    slot_set(9, 376, 546, 528, 56, "Effacer les statistiques", nullptr,
             Pal::BTN_BG, Pal::BTN_EDGE, Pal::BAD);
    slot_set(10, 376, 612, 528, 56, "Retour");
    slots_hide_from(11);
    free_hide_from(4);
}

static void render_pause() {
    menu_head(28, 86);
    set_text_if(g_m_title, "PAUSE");
    snprintf(g_fmt, sizeof(g_fmt), "Tour %u · au tour de %s", (unsigned) s_turn, s_teams[s_cur].name);
    set_text_if(g_m_sub, g_fmt);
    set_text_if(g_m_body, "");
    set_text_if(g_m_foot, "");
    pie_show(g_big_pie, g_big_rim, false);
    bars_hide();
    free_hide_from(0);
    slot_list(0, 0, "Reprendre la partie", nullptr);
    slot_list(1, 1, "Règles du jeu", nullptr);
    slot_list(2, 2, "Réglages", nullptr);
    slot_list(3, 3, "Abandonner la partie", "Retour au menu principal", true, Pal::BAD);
    slot_list(4, 4, "Quitter le jeu", "La partie sera reprise plus tard", true, Pal::BAD);
    slots_hide_from(5);
}

static void render_confirm() {
    menu_head(28, 86);
    set_text_if(g_m_title, "CONFIRMER");
    set_text_if(g_m_sub, "");
    set_text_if(g_m_body, s_confirm == CFM_ABANDON
        ? "Abandonner la partie en cours ?\nLes parts gagnées seront perdues."
        : "Effacer toutes les statistiques ?\nCette action est définitive.");
    set_text_if(g_m_foot, "");
    pie_show(g_big_pie, g_big_rim, false);
    bars_hide();
    free_hide_from(0);
    slot_set(0, 300, 380, 320, 76, "Confirmer", nullptr, Pal::BTN_BG, Pal::BAD, Pal::BAD);
    slot_set(1, 660, 380, 320, 76, "Annuler");
    slots_hide_from(2);
}

static void render_catpick() {
    menu_head(28, 86);
    set_text_if(g_m_title, s_is_final ? "QUESTION FINALE" : "CATÉGORIE AU CHOIX");
    if (s_is_final) {
        if (s_n_teams > 1) {
            int chooser = (s_cur + 1) % s_n_teams;
            snprintf(g_fmt, sizeof(g_fmt), "%s a ses 6 parts — %s choisit la catégorie",
                     s_teams[s_cur].name, s_teams[chooser].name);
        } else {
            snprintf(g_fmt, sizeof(g_fmt), "%s a ses 6 parts — choisissez votre catégorie finale",
                     s_teams[s_cur].name);
        }
    } else {
        snprintf(g_fmt, sizeof(g_fmt), "%s est au centre : choisissez une catégorie",
                 s_teams[s_cur].name);
    }
    set_text_if(g_m_sub, g_fmt);
    set_text_if(g_m_body, "");
    set_text_if(g_m_foot, s_is_final ? "Bonne réponse = victoire" : "");
    pie_show(g_big_pie, g_big_rim, false);
    bars_hide();
    free_hide_from(0);
    for (int c = 0; c < TRIVIA_NCAT; c++) {
        int col = c % 3, row = c / 3;
        slot_set(c, 100 + col * 360, 200 + row * 180, 340, 150, CAT_NAMES[c], nullptr,
                 CAT_COLORS[c], CAT_COLORS[c], Pal::VOID_BG, true);
    }
    slots_hide_from(TRIVIA_NCAT);
}

static void render_victory() {
    menu_head(28, 86);
    set_text_if(g_m_title, "VICTOIRE !");
    snprintf(g_fmt, sizeof(g_fmt), "%s remporte la partie", s_teams[s_winner].name);
    set_text_if(g_m_sub, g_fmt);
    uint32_t n = s_game_ok + s_game_ko;
    snprintf(g_fmt, sizeof(g_fmt), "Partie bouclée en %u tours · %u bonnes réponses sur %u",
             (unsigned) s_turn, (unsigned) s_game_ok, (unsigned) n);
    set_text_if(g_m_body, g_fmt);
    set_text_if(g_m_foot, "");
    pie_set(g_big_pie, g_big_rim, 640 - 70, 200, 140, 0x3F);
    pie_show(g_big_pie, g_big_rim, true);
    bars_hide();
    free_hide_from(0);
    slot_set(0, 290, 380, 700, 64, "Rejouer avec les mêmes équipes", nullptr);
    slot_set(1, 290, 456, 700, 64, "Nouvelle partie", nullptr);
    slot_set(2, 290, 532, 700, 64, "Retour au menu", nullptr);
    slots_hide_from(3);
}

// Un seul point d'entrée : on repeint l'écran de menu correspondant à l'état.
static void render_menu() {
    switch (s_state) {
        case ST_HUB:      render_hub();      break;
        case ST_SETUP:    render_setup();    break;
        case ST_RULES:    render_rules();    break;
        case ST_STATS:    render_stats();    break;
        case ST_SETTINGS: render_settings(); break;
        case ST_CONFIRM:  render_confirm();  break;
        case ST_PAUSE:    render_pause();    break;
        case ST_CATPICK:  render_catpick();  break;
        case ST_VICTORY:  render_victory();  break;
        default: break;
    }
}

static inline bool menu_state(GState s) {
    return s == ST_HUB || s == ST_SETUP || s == ST_RULES || s == ST_STATS ||
           s == ST_SETTINGS || s == ST_CONFIRM || s == ST_PAUSE ||
           s == ST_CATPICK || s == ST_VICTORY;
}

// Change d'écran et repeint immédiatement (les écrans de menu ne sont pas
// redessinés à chaque tick : ils ne changent que sur action).
static void go(GState s) {
    s_state = s;
    if (menu_state(s)) render_menu();
}

// ===========================================================================
// 15. Rendu du plateau, du panneau et du HUD
// ===========================================================================
static void paint_cell(int n, uint8_t hl) {
    if (g_drawn_hl[n] == hl) return;
    g_drawn_hl[n] = hl;
    lv_obj_t* c = g_cell[n];
    const BoardCell& bc = s_cells[n];
    uint32_t bg;
    switch (bc.type) {
        case CELL_ROLL: bg = Pal::ROLL_BG; break;
        case CELL_HUB:  bg = Pal::FLOOR_BG; break;
        default:        bg = CAT_COLORS[bc.cat]; break;
    }
    lv_obj_set_style_bg_color(c, lv_color_hex(bg), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(c, (bc.type == CELL_CAT) ? (lv_opa_t) 225 : (lv_opa_t) LV_OPA_COVER,
                            LV_PART_MAIN);
    if (hl) {
        set_border(c, Pal::TXT, 4, LV_OPA_COVER);
    } else if (bc.type == CELL_HQ) {
        set_border(c, Pal::HQ_EDGE, 3, LV_OPA_90);
    } else if (bc.type == CELL_HUB) {
        set_border(c, Pal::HUB_EDGE, 3, LV_OPA_60);
    } else {
        set_border(c, Pal::CELL_EDGE, 2, LV_OPA_60);
    }
}

static void render_board() {
    bool choosing = (s_state == ST_PLAY && s_phase == PH_CHOOSE);
    for (int n = 0; n < TRIVIA_NODES; n++)
        paint_cell(n, (choosing && s_reach[n]) ? 1 : 0);

    // Pulsation des cases atteignables : seule l'opacité du liseré bouge.
    if (choosing) {
        uint32_t ph = esphome::millis() % 1000;
        lv_opa_t o = (lv_opa_t) (150 + 105 * (0.5f + 0.5f * cosf((float) ph * 0.0062831853f)));
        for (int n = 0; n < TRIVIA_NODES; n++)
            if (s_reach[n]) lv_obj_set_style_border_opa(g_cell[n], o, LV_PART_MAIN);
    }

    // Pions. Ceux d'une même case sont décalés en quinconce pour rester lisibles.
    bool cur_changed = (g_c_pawn_cur != (int) s_cur);
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        bool vis = (i < s_n_teams && s_in_game);
        if (g_c_pawn_vis[i] != (int) vis) { g_c_pawn_vis[i] = (int) vis; show(g_pawn[i], vis); }
        if (!vis) continue;
        int px, py;
        if (i == s_cur && s_phase == PH_MOVING && s_path_n > 1) {
            int a = s_path[s_hop], b = s_path[s_hop + 1 < s_path_n ? s_hop + 1 : s_hop];
            int ax, ay, bx, by;
            node_pos(a, &ax, &ay);
            node_pos(b, &bx, &by);
            uint32_t el = esphome::millis() - s_hop_t0;
            float t = (el >= HOP_MS) ? 1.0f : (float) el / (float) HOP_MS;
            px = ax + (int) ((bx - ax) * t);
            py = ay + (int) ((by - ay) * t);
        } else {
            node_pos(s_teams[i].pos, &px, &py);
        }
        int ox = (i % 3) * 13 - 13, oy = (i / 3) * 15 - 7;
        px += ox - 11; py += oy - 11;
        if (g_c_pawn_x[i] != px || g_c_pawn_y[i] != py) {
            g_c_pawn_x[i] = px; g_c_pawn_y[i] = py;
            lv_obj_set_pos(g_pawn[i], px, py);
        }
        if (cur_changed)
            set_border(g_pawn[i], (i == s_cur) ? Pal::TXT : Pal::CELL_EDGE,
                       (i == s_cur) ? 3 : 2, LV_OPA_COVER);
    }
    if (cur_changed) g_c_pawn_cur = (int) s_cur;
}

// Faces du dé : bit i = point i allumé.
// Points : 0=HG 1=HD 2=MG 3=centre 4=MD 5=BG 6=BD.
static void render_dice() {
    static const uint8_t FACE[7] = {0x00, 0x08, 0x41, 0x49, 0x63, 0x6B, 0x77};
    uint8_t v = s_dice_shown;
    if (v < 1 || v > 6) v = 1;
    if (g_c_dice == v) return;
    g_c_dice = v;
    uint8_t m = FACE[v];
    for (int i = 0; i < 7; i++) show(g_pip[i], (m >> i) & 1);
}

static void render_panel() {
    // Bouton « lancer »
    int can_roll = (s_state == ST_PLAY && s_phase == PH_ROLL) ? 1 : 0;
    if (g_c_roll != can_roll) {
        g_c_roll = can_roll;
        set_bg(g_roll_btn, can_roll ? Pal::ACCENT : Pal::BTN_BG, LV_OPA_COVER);
        set_color(g_roll_lbl, can_roll ? Pal::VOID_BG : Pal::TXT_MUTED);
    }
    set_text_if(g_roll_lbl, s_phase == PH_ROLLING ? "…" : "LANCER LE DÉ");
    set_text_if(g_roll_hint, s_shake_on ? "ou secouez la tablette" : "");

    // Bandeau d'état
    const char* st;
    if (s_msg[0] && esphome::millis() < s_msg_until) {
        st = s_msg;
    } else if (!s_in_game) {
        st = "Aucune partie en cours";
    } else {
        switch (s_phase) {
            case PH_ROLLING: st = "Le dé roule…"; break;
            case PH_MOVING:  st = "Déplacement…"; break;
            case PH_CHOOSE:
                snprintf(g_fmt, sizeof(g_fmt), "Dé : %u — touchez une case surlignée",
                         (unsigned) s_dice_val);
                st = g_fmt;
                break;
            default:
                snprintf(g_fmt, sizeof(g_fmt), "À %s de lancer le dé", s_teams[s_cur].name);
                st = g_fmt;
                break;
        }
    }
    set_text_if(g_status_lbl, st);

    // Fiches équipes — signature = visible | actif | parts | couleur du pion.
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        bool on = (i < s_n_teams && s_in_game);
        int sig = !on ? 0
                      : (1 << 20) | ((i == s_cur) ? (1 << 16) : 0) |
                        (s_teams[i].wedges << 8) | s_teams[i].color_idx;
        if (g_c_row_sig[i] == sig) continue;
        g_c_row_sig[i] = sig;
        if (!on) {
            show(g_row[i], false);
            show(g_row_chip[i], false);
            show(g_row_name[i], false);
            show(g_row_cnt[i], false);
            pie_show(g_row_pie[i], g_row_rim[i], false);
            continue;
        }
        bool active = (i == s_cur);
        show(g_row[i], true);
        show(g_row_chip[i], true);
        show(g_row_name[i], true);
        show(g_row_cnt[i], true);
        pie_show(g_row_pie[i], g_row_rim[i], true);
        set_bg(g_row[i], active ? Pal::BTN_BG_ON : Pal::CARD_BG, LV_OPA_COVER);
        set_border(g_row[i], active ? Pal::ACCENT : Pal::CARD_EDGE, 2, LV_OPA_COVER);
        set_bg(g_row_chip[i], PAWN_COLORS[s_teams[i].color_idx], LV_OPA_COVER);
        set_color(g_row_name[i], active ? Pal::ACCENT : Pal::TXT);
        set_text_if(g_row_name[i], s_teams[i].name);
        int nw = n_wedges(i);
        snprintf(g_fmt, sizeof(g_fmt), "%d/6", nw);
        set_text_if(g_row_cnt[i], g_fmt);
        set_color(g_row_cnt[i], nw == TRIVIA_NCAT ? Pal::GOOD : Pal::TXT_DIM);
        pie_set(g_row_pie[i], g_row_rim[i], 1192, ROW_Y0 + i * ROW_STEP + 5, 44, s_teams[i].wedges);
    }
}

static void render_hud() {
    if (s_in_game) {
        snprintf(g_fmt, sizeof(g_fmt), "Tour %u", (unsigned) s_turn);
        set_text_if(g_h_turn, g_fmt);
        snprintf(g_fmt, sizeof(g_fmt), "Équipe : %s", s_teams[s_cur].name);
        set_text_if(g_h_team, g_fmt);
        int hsig = ((int) s_cur << 8) | s_teams[s_cur].color_idx;
        if (g_c_hud_team != hsig) {
            g_c_hud_team = hsig;
            set_color(g_h_team, PAWN_COLORS[s_teams[s_cur].color_idx]);
        }
    } else {
        set_text_if(g_h_turn, "");
        set_text_if(g_h_team, "");
    }
    if (s_state == ST_QUESTION && TIMER_SEC[s_timer_idx] > 0) {
        uint32_t total = (uint32_t) TIMER_SEC[s_timer_idx] * 1000;
        uint32_t el = esphome::millis() - s_q_t0;
        int left = (el >= total) ? 0 : (int) ((total - el + 999) / 1000);
        snprintf(g_fmt, sizeof(g_fmt), "%d s", left);
        set_text_if(g_h_clock, g_fmt);
        int warn = (left <= 5) ? 1 : 0;
        if (g_c_clock_warn != warn) {
            g_c_clock_warn = warn;
            set_color(g_h_clock, warn ? Pal::BAD : Pal::ACCENT);
        }
    } else {
        set_text_if(g_h_clock, "");
        g_c_clock_warn = -1;
    }
}

static void render_question() {
    if (!s_q) return;
    bool reveal = (s_state == ST_REVEAL);

    // Chronomètre : la seule chose qui bouge réellement à chaque image.
    uint8_t tsec = TIMER_SEC[s_timer_idx];
    if (tsec > 0 && !reveal) {
        uint32_t total = (uint32_t) tsec * 1000;
        uint32_t el = esphome::millis() - s_q_t0;
        int pct = (el >= total) ? 0 : (int) (100 - el * 100 / total);
        lv_obj_set_size(g_qbar_fill, (pct > 0) ? (1160 * pct / 100) : 1, 8);
        set_bg(g_qbar_fill, pct > 50 ? Pal::GOOD : (pct > 20 ? Pal::ACCENT : Pal::BAD), LV_OPA_COVER);
    }
    show(g_qbar, tsec > 0);

    // Le reste de la carte ne se repeint qu'au changement de question ou de verdict.
    int sig = (reveal ? 0x10000 : 0) | ((int) s_picked_slot << 8) | (int) s_correct_slot;
    if (g_c_q == s_q && g_c_qsig == sig) return;
    g_c_q = s_q;
    g_c_qsig = sig;

    uint32_t col = CAT_COLORS[s_q_cat];
    set_bg(g_qban, col, LV_OPA_COVER);
    set_bg(g_qban_fix, col, LV_OPA_COVER);
    if (s_is_final) snprintf(g_fmt, sizeof(g_fmt), "FINALE — %s", CAT_NAMES[s_q_cat]);
    else            snprintf(g_fmt, sizeof(g_fmt), "%s", CAT_NAMES[s_q_cat]);
    set_text_if(g_qban_cat, g_fmt);
    static const char* const DLBL[3] = {"Facile", "Moyen", "Difficile"};
    snprintf(g_fmt, sizeof(g_fmt), "%s  ·  %s", s_teams[s_cur].name,
             DLBL[s_q->difficulty < 3 ? s_q->difficulty : 0]);
    set_text_if(g_qban_side, g_fmt);
    set_text_if(g_qtext, s_q->q);

    for (int i = 0; i < 4; i++) {
        set_text_if(g_ans_lbl[i], choice_text(i));
        if (!reveal) {
            set_bg(g_ans[i], Pal::BTN_BG, LV_OPA_COVER);
            set_border(g_ans[i], Pal::BTN_EDGE, 2, LV_OPA_COVER);
            set_color(g_ans_lbl[i], Pal::TXT);
        } else if (i == s_correct_slot) {
            set_bg(g_ans[i], Pal::GOOD, LV_OPA_COVER);
            set_border(g_ans[i], Pal::TXT, 3, LV_OPA_COVER);
            set_color(g_ans_lbl[i], Pal::VOID_BG);
        } else if (i == s_picked_slot) {
            set_bg(g_ans[i], Pal::BAD, LV_OPA_COVER);
            set_border(g_ans[i], Pal::TXT, 3, LV_OPA_COVER);
            set_color(g_ans_lbl[i], Pal::TXT);
        } else {
            set_bg(g_ans[i], Pal::BTN_BG, (lv_opa_t) 120);
            set_border(g_ans[i], Pal::BTN_EDGE, 2, (lv_opa_t) 90);
            set_color(g_ans_lbl[i], Pal::TXT_MUTED);
        }
    }

    if (!reveal) {
        set_text_if(g_qfeed, "");
    } else if (s_correct) {
        if (s_is_final)        snprintf(g_fmt, sizeof(g_fmt), "Exact — %s remporte la partie !", s_teams[s_cur].name);
        else if (s_won_wedge)  snprintf(g_fmt, sizeof(g_fmt), "Bravo ! Part « %s » gagnée — vous rejouez.", CAT_NAMES[s_q_cat]);
        else                   snprintf(g_fmt, sizeof(g_fmt), "Bonne réponse — vous rejouez.");
        set_color(g_qfeed, Pal::GOOD);
        set_text_if(g_qfeed, g_fmt);
    } else {
        if (s_picked_slot == 0xFF) snprintf(g_fmt, sizeof(g_fmt), "Temps écoulé — la réponse était : %s", s_q->a);
        else                       snprintf(g_fmt, sizeof(g_fmt), "Raté — la réponse était : %s", s_q->a);
        set_color(g_qfeed, Pal::BAD);
        set_text_if(g_qfeed, g_fmt);
    }
}

// Aiguillage des calques : un seul visible à la fois, toujours au premier plan.
// Le passage au premier plan réordonne les enfants de root : on ne le refait
// qu'au changement de calque, pas à chaque image.
static void render_layers() {
    int q  = (s_state == ST_QUESTION || s_state == ST_REVEAL) ? 1 : 0;
    int mn = menu_state(s_state) ? 1 : 0;
    int sig = (q << 1) | mn;
    if (g_c_layers == sig) return;
    g_c_layers = sig;
    show(g_qlayer, q != 0);
    show(g_ui.panel, mn != 0);
    if (q)       lv_obj_move_foreground(g_qlayer);
    else if (mn) lv_obj_move_foreground(g_ui.panel);
}

// ===========================================================================
// 16. Tick (lv_timer ~30 Hz)
// ===========================================================================
static void tick(lv_timer_t* t) {
    (void) t;
    if (!g_open) return;
    uint32_t now = esphome::millis();

    // --- Dé ---
    if (s_dice_spin) {
        uint32_t el = now - s_dice_t0;
        if (el < DICE_SPIN_MS) {
            s_dice_shown = (uint8_t) (1 + ((el / DICE_FACE_MS) % 6));
        } else {
            s_dice_shown = s_dice_val;
            s_dice_spin = false;
            clear_reach();
            dfs_reach(s_teams[s_cur].pos, -1, s_dice_val);
            s_reach[s_teams[s_cur].pos] = false;   // rester sur place n'est pas un coup
            int only = -1, cnt = 0;
            for (int n = 0; n < TRIVIA_NODES; n++) if (s_reach[n]) { cnt++; only = n; }
            s_phase = PH_CHOOSE;
            if (cnt == 0) {                        // filet de sécurité : jamais de blocage
                msg("Aucun déplacement possible — au suivant.");
                next_team();
                begin_roll_phase();
            } else if (cnt == 1) {
                start_move(only);                  // pas de choix : on part tout de suite
            }
        }
    }

    // --- Déplacement du pion ---
    if (s_phase == PH_MOVING && (now - s_hop_t0) >= HOP_MS) {
        s_hop++;
        s_hop_t0 = now;
        if (s_hop >= s_path_n - 1) {
            s_phase = PH_ROLL;
            finish_move();
            if (menu_state(s_state)) render_menu();
        }
    }

    // --- Chronomètre de la question ---
    if (s_state == ST_QUESTION && TIMER_SEC[s_timer_idx] > 0) {
        if ((now - s_q_t0) >= (uint32_t) TIMER_SEC[s_timer_idx] * 1000) answer(0xFF);
    }
    // --- Fin du verdict ---
    if (s_state == ST_REVEAL && now >= s_reveal_until) {
        end_reveal();
        if (menu_state(s_state)) render_menu();
    }

    // --- Rendu ---
    render_layers();
    render_hud();
    render_dice();
    render_panel();
    render_board();
    if (s_state == ST_QUESTION || s_state == ST_REVEAL) render_question();
}

// ===========================================================================
// 17. Callbacks
// ===========================================================================
static void cell_cb(lv_event_t* e) {
    int n = (int) (intptr_t) lv_event_get_user_data(e);
    if (s_state != ST_PLAY || s_phase != PH_CHOOSE) return;
    if (n < 0 || n >= TRIVIA_NODES || !s_reach[n]) return;
    start_move(n);
}

static void roll_cb(lv_event_t* e) {
    (void) e;
    roll_dice();
}

static void menu_cb(lv_event_t* e) {
    (void) e;
    // Pas de fuite en pleine question, ni pendant que le dé roule ou que le pion
    // se déplace : le tick continuerait d'avancer la partie sous le menu.
    if (s_state != ST_PLAY) return;
    if (s_phase != PH_ROLL && s_phase != PH_CHOOSE) return;
    go(ST_PAUSE);
}

static void answer_cb(lv_event_t* e) {
    int slot = (int) (intptr_t) lv_event_get_user_data(e);
    if (s_state != ST_QUESTION) return;
    answer((uint8_t) slot);
}

// Tap n'importe où sur le voile pendant le verdict = passer à la suite.
static void qlayer_cb(lv_event_t* e) {
    (void) e;
    if (s_state != ST_REVEAL) return;
    end_reveal();
    if (menu_state(s_state)) render_menu();
}

static void wipe_stats() {
    memset(&s_save.stats, 0, sizeof(s_save.stats));
    persist_save();
}

static void slot_cb(lv_event_t* e) {
    int i = (int) (intptr_t) lv_event_get_user_data(e);

    switch (s_state) {
        // ------------------------------------------------------------------
        case ST_HUB:
            switch (i) {
                case 0: s_setup_n = s_setup_n ? s_setup_n : 2; go(ST_SETUP); break;
                case 1:
                    if (s_in_game) { s_state = ST_PLAY; begin_roll_phase(); }
                    else if (resume_game()) { msg("Partie reprise."); }
                    break;
                case 2: go(ST_STATS); break;
                case 3: s_return_to = ST_HUB; go(ST_RULES); break;
                case 4: s_return_to = ST_HUB; go(ST_SETTINGS); break;
                case 5: close(); break;
            }
            return;

        // ------------------------------------------------------------------
        case ST_SETUP:
            if (i >= 0 && i < 12) {
                int team = i / 2;
                if (i % 2 == 0) {                       // pastille : couleur suivante libre
                    if (team >= s_setup_n) return;
                    for (int k = 1; k <= TRIVIA_MAX_TEAMS; k++) {
                        uint8_t c = (uint8_t) ((s_setup_teams[team].color_idx + k) % TRIVIA_MAX_TEAMS);
                        bool taken = false;
                        for (int o = 0; o < s_setup_n; o++)
                            if (o != team && s_setup_teams[o].color_idx == c) { taken = true; break; }
                        if (!taken) { s_setup_teams[team].color_idx = c; break; }
                    }
                } else if (team < s_setup_n) {          // nom : prénom suivant libre
                    int cur = 0;
                    for (int k = 0; k < 12; k++)
                        if (strcmp(s_setup_teams[team].name, PRESET_NAMES[k]) == 0) { cur = k; break; }
                    for (int k = 1; k <= 12; k++) {
                        const char* cand = PRESET_NAMES[(cur + k) % 12];
                        bool taken = false;
                        for (int o = 0; o < s_setup_n; o++)
                            if (o != team && strcmp(s_setup_teams[o].name, cand) == 0) { taken = true; break; }
                        if (!taken) {
                            snprintf(s_setup_teams[team].name, sizeof(s_setup_teams[team].name), "%s", cand);
                            break;
                        }
                    }
                } else if (team == s_setup_n) {         // « + Ajouter une équipe »
                    if (s_setup_n < TRIVIA_MAX_TEAMS) s_setup_n++;
                }
            } else if (i >= 12 && i <= 14) {
                s_difficulty = (uint8_t) (i - 12);
            } else if (i >= 15 && i <= 18) {
                s_timer_idx = (uint8_t) (i - 15);
            } else if (i == 19) { s_shake_on = true;
            } else if (i == 20) { s_shake_on = false;
            } else if (i == 21) {
                persist_save();
                start_game(s_setup_n, s_setup_teams);
                return;
            } else if (i == 22) {
                persist_save();
                go(ST_HUB);
                return;
            } else if (i == 24) { if (s_setup_n > 1) s_setup_n--;
            } else if (i == 25) { if (s_setup_n < TRIVIA_MAX_TEAMS) s_setup_n++; }
            render_setup();
            return;

        // ------------------------------------------------------------------
        case ST_RULES:
            go(s_return_to);
            return;

        // ------------------------------------------------------------------
        case ST_STATS:
            if (i == 6)      { s_confirm = CFM_WIPE_STATS; s_return_to = ST_STATS; go(ST_CONFIRM); }
            else if (i == 7) { go(ST_HUB); }
            return;

        // ------------------------------------------------------------------
        case ST_SETTINGS:
            if (i >= 0 && i <= 2)      s_difficulty = (uint8_t) i;
            else if (i >= 3 && i <= 6) s_timer_idx = (uint8_t) (i - 3);
            else if (i == 7)  s_shake_on = true;
            else if (i == 8)  s_shake_on = false;
            else if (i == 9)  { s_confirm = CFM_WIPE_STATS; go(ST_CONFIRM); return; }
            else if (i == 10) { persist_save(); go(s_return_to); return; }
            render_settings();
            return;

        // ------------------------------------------------------------------
        case ST_CONFIRM:
            if (i == 0) {
                if (s_confirm == CFM_ABANDON) {
                    s_in_game = false;
                    persist_save();
                    go(ST_HUB);
                } else {
                    wipe_stats();
                    go(s_return_to == ST_STATS ? ST_STATS : ST_SETTINGS);
                }
            } else {
                go(s_confirm == CFM_ABANDON ? ST_PAUSE
                                            : (s_return_to == ST_STATS ? ST_STATS : ST_SETTINGS));
            }
            return;

        // ------------------------------------------------------------------
        case ST_PAUSE:
            switch (i) {
                case 0: s_state = ST_PLAY; break;
                case 1: s_return_to = ST_PAUSE; go(ST_RULES); break;
                case 2: s_return_to = ST_PAUSE; go(ST_SETTINGS); break;
                case 3: s_confirm = CFM_ABANDON; go(ST_CONFIRM); break;
                case 4: close(); break;
            }
            return;

        // ------------------------------------------------------------------
        case ST_CATPICK:
            if (i >= 0 && i < TRIVIA_NCAT) prepare_question((uint8_t) i);
            return;

        // ------------------------------------------------------------------
        case ST_VICTORY:
            if (i == 0) {
                Team roster[TRIVIA_MAX_TEAMS];
                for (int k = 0; k < s_n_teams; k++) roster[k] = s_teams[k];
                start_game(s_n_teams, roster);
            } else if (i == 1) {
                go(ST_SETUP);
            } else {
                go(ST_HUB);
            }
            return;

        default:
            return;
    }
}

// ===========================================================================
// 18. IMU — secousse = lancer le dé
// ===========================================================================
void on_imu(float ax, float ay, float az) {
    if (!g_open || !s_shake_on) return;
    if (s_state != ST_PLAY || s_phase != PH_ROLL) return;
    float mag = sqrtf(ax * ax + ay * ay + az * az);
    uint32_t now = esphome::millis();
    if (mag > 2.2f && (now - s_shake_last) > SHAKE_CD_MS) {
        s_shake_last = now;
        roll_dice();
    }
}

// ===========================================================================
// 19. API publique
// ===========================================================================
void open(const UI& ui) {
    g_ui = ui;
    if (!s_pref_ready) persist_load();
    build_ui();

    g_open = true;
    reset_caches();
    // Une partie déjà en cours en RAM reprend directement ; sinon on ouvre le hub.
    if (s_in_game) { s_state = ST_PLAY; begin_roll_phase(); }
    else            go(ST_HUB);
    render_layers();

    lv_obj_clear_flag(g_ui.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(g_ui.root);
    if (!g_timer) g_timer = lv_timer_create(tick, TICK_MS, nullptr);
}

void close() {
    if (!g_open) return;
    g_open = false;
    persist_save();
    if (g_timer) { lv_timer_delete(g_timer); g_timer = nullptr; }
    if (g_ui.root) lv_obj_add_flag(g_ui.root, LV_OBJ_FLAG_HIDDEN);
}

bool is_open() { return g_open; }

}  // namespace Trivia
