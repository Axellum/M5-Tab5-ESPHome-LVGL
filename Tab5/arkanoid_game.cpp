/**
 * [AI-CONTEXT]
 * @file arkanoid_game.cpp
 * @role Jeu « Arcanoïde » — casse-briques rétro Atari / borne arcade 80's.
 * @architecture_constraint Plein écran 1280x720. Le YAML ne fournit que 4
 *      conteneurs vides + 3 polices ; tout le reste est construit ici. Les objets
 *      LVGL sont PRÉALLOUÉS une seule fois (pool) puis réutilisés par show/hide +
 *      move : aucune allocation LVGL dans la boucle de jeu. Persistance NVS via
 *      esphome::global_preferences (aucune dépendance Home Assistant).
 * @ai_instruction Hot-path = tick() : pas de std::string, pas de to_string(), pas
 *      de new/delete. Les libellés HUD ne sont réécrits que quand leur valeur change.
 *      Couleurs : uniquement UIColor::ARK_* (jamais d'hex en dur ici).
 */
#include "arkanoid_game.h"
#include "esphome/core/preferences.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace Arkanoid {

// ===========================================================================
// 1. Géométrie & réglages de physique
// ===========================================================================

static constexpr int   FW = 1280;   // largeur du terrain
static constexpr int   FH = 672;    // hauteur du terrain (720 - bandeau HUD 48)
static constexpr int   HUD_H = 48;

static constexpr float DT      = 0.0333f;   // pas de la boucle (30 Hz)
static constexpr int   SUBSTEP = 3;         // sous-pas de collision (anti-tunnelling)
static constexpr float SDT     = DT / SUBSTEP;

// --- Raquette ---
static constexpr int   PAD_W_DEFAULT = 160;  // largeur initiale de la raquette
static constexpr int   PAD_W_MIN     = 90;   // largeur min (power-up rétrécir)
static constexpr int   PAD_W_MAX     = 260;  // largeur max (power-up élargir)
static constexpr int   PAD_H         = 18;   // hauteur de la raquette
static constexpr int   PAD_Y         = FH - 40;  // position Y fixe depuis le haut du field
static constexpr float PAD_SPEED_BTN = 780.0f;  // vitesse boutons tactiles (px/s)
static constexpr float PAD_ACCEL_IMU = 2400.0f; // accélération IMU (px/s^2 par g)
static constexpr float PAD_MAX_IMU   = 900.0f;  // vitesse max IMU (px/s)
static constexpr float PAD_FRICTION  = 0.88f;   // friction IMU par frame

// --- Balle ---
static constexpr int   BALL_R       = 9;     // rayon de la balle
static constexpr float BALL_SPEED_0 = 420.0f; // vitesse initiale (px/s)
static constexpr float BALL_SPEED_INC = 18.0f; // incrément par niveau
static constexpr float BALL_SPEED_MAX = 720.0f; // plafond absolu
static constexpr int   MAX_BALLS    = 3;     // multi-balles max

// --- Briques ---
static constexpr int   BRICK_COLS   = 12;    // colonnes de la grille
static constexpr int   BRICK_ROWS   = 10;    // lignes max
static constexpr int   BRICK_GAP    = 3;     // espacement entre briques (px)
static constexpr int   BRICK_MARGIN_X = 40;  // marge latérale
static constexpr int   BRICK_TOP    = 50;    // offset depuis le haut du field
// Dimensions calculées d'une brique individuelle.
static constexpr int   BRICK_W = (FW - 2 * BRICK_MARGIN_X - (BRICK_COLS - 1) * BRICK_GAP) / BRICK_COLS;
static constexpr int   BRICK_H = 28;
static constexpr int   MAX_BRICKS = BRICK_COLS * BRICK_ROWS;  // 120

// --- Power-ups ---
static constexpr int   MAX_POWERUPS = 8;     // pool de power-ups simultanés
static constexpr float PU_FALL_SPEED = 220.0f;  // vitesse de chute (px/s)
static constexpr int   PU_W = 44;
static constexpr int   PU_H = 22;

// --- IMU / inclinaison ---
static constexpr float TILT_DEADZONE = 0.05f;
static constexpr float TILT_SMOOTH   = 0.35f;
static constexpr float TILT_CLAMP    = 0.80f;

// --- Score & combo ---
static constexpr int   COMBO_WINDOW_MS = 1200;  // fenêtre de combo (ms)
static constexpr int   COMBO_MAX       = 8;     // multiplicateur max
static constexpr int   SCORE_BRICK     = 10;    // points par brique normale
static constexpr int   SCORE_TOUGH     = 25;    // points par brique renforcée
static constexpr int   LIVES_START     = 3;

// --- NVS ---
// "ARK1" — bumpe à chaque changement de layout de ArkanoidSave.
static constexpr uint32_t SAVE_MAGIC = 0x41524B31u;
static constexpr uint32_t PREF_KEY   = 0x41524B44u;  // clé NVS dédiée

// ===========================================================================
// 2. Générateur pseudo-aléatoire (xorshift32)
// ===========================================================================

static uint32_t s_rng = 0xDEADBEEFu;
static inline uint32_t rnd() {
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}
static inline int rnd_range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(rnd() % (uint32_t)(hi - lo + 1));
}

// ===========================================================================
// 3. Types de briques & power-ups
// ===========================================================================

enum BrickType : uint8_t {
    BT_EMPTY = 0,   // case vide
    BT_NORMAL,      // 1 coup, couleur par rangée (style Atari)
    BT_TOUGH,       // 2-3 coups, changement de teinte
    BT_INDESTRUCT,  // indestructible (obstacle)
    BT_BONUS        // 1 coup, lâche un power-up
};

enum PowerType : uint8_t {
    PU_NONE = 0,
    PU_EXPAND,      // élargir raquette
    PU_SHRINK,      // rétrécir raquette
    PU_SLOW,        // balle lente
    PU_FAST,        // balle rapide
    PU_MULTI,       // multi-balles (max 3)
    PU_GLUE,        // colle (balle collée, relancer au tap)
    PU_LIFE         // extra vie (rare)
};

// Palette Atari NTSC pour les rangées de briques (8 couleurs cycle).
static const uint32_t ROW_COLORS[8] = {
    0xFF4444,  // rouge
    0xFF8800,  // orange
    0xFFDD00,  // jaune
    0x44DD44,  // vert
    0x44DDDD,  // cyan
    0x4488FF,  // bleu
    0xAA44FF,  // violet
    0xFF44AA   // magenta
};

// ===========================================================================
// 4. Layouts des 8 niveaux
// ===========================================================================
// Encodage : grille BRICK_ROWS x BRICK_COLS, valeurs = BrickType.
// Pour BT_TOUGH, la valeur stockée dans g_bricks_hp indique les PV restants.

// Niveau 1 : Mur plein (classique Breakout)
static const uint8_t LVL1[BRICK_ROWS][BRICK_COLS] = {
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

// Niveau 2 : Pyramide
static const uint8_t LVL2[BRICK_ROWS][BRICK_COLS] = {
    {0,0,0,0,0,1,1,0,0,0,0,0},
    {0,0,0,0,1,1,1,1,0,0,0,0},
    {0,0,0,1,1,1,1,1,1,0,0,0},
    {0,0,1,1,1,1,1,1,1,1,0,0},
    {0,1,1,1,1,1,1,1,1,1,1,0},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

// Niveau 3 : Colonnes
static const uint8_t LVL3[BRICK_ROWS][BRICK_COLS] = {
    {1,0,1,0,1,0,1,0,1,0,1,0},
    {1,0,1,0,1,0,1,0,1,0,1,0},
    {1,0,1,0,1,0,1,0,1,0,1,0},
    {1,0,1,0,1,0,1,0,1,0,1,0},
    {1,0,1,0,1,0,1,0,1,0,1,0},
    {1,0,1,0,1,0,1,0,1,0,1,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

// Niveau 4 : Damier
static const uint8_t LVL4[BRICK_ROWS][BRICK_COLS] = {
    {1,0,1,0,1,0,1,0,1,0,1,0},
    {0,1,0,1,0,1,0,1,0,1,0,1},
    {1,0,1,0,1,0,1,0,1,0,1,0},
    {0,1,0,1,0,1,0,1,0,1,0,1},
    {1,0,1,0,1,0,1,0,1,0,1,0},
    {0,1,0,1,0,1,0,1,0,1,0,1},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

// Niveau 5 : Couloirs (murs indestructibles verticaux)
static const uint8_t LVL5[BRICK_ROWS][BRICK_COLS] = {
    {1,1,3,1,1,3,1,1,3,1,1,3},
    {1,1,0,1,1,0,1,1,0,1,1,0},
    {1,1,3,1,1,3,1,1,3,1,1,3},
    {1,1,0,1,1,0,1,1,0,1,1,0},
    {1,1,3,1,1,3,1,1,3,1,1,3},
    {1,1,0,1,1,0,1,1,0,1,1,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

// Niveau 6 : Forteresse (briques renforcées + bonus au centre)
static const uint8_t LVL6[BRICK_ROWS][BRICK_COLS] = {
    {2,2,2,2,2,2,2,2,2,2,2,2},
    {2,0,0,0,0,0,0,0,0,0,0,2},
    {2,0,4,4,4,4,4,4,4,4,0,2},
    {2,0,4,1,1,1,1,1,1,4,0,2},
    {2,0,4,1,1,1,1,1,1,4,0,2},
    {2,0,4,4,4,4,4,4,4,4,0,2},
    {2,0,0,0,0,0,0,0,0,0,0,2},
    {2,2,2,2,0,0,0,0,2,2,2,2},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

// Niveau 7 : Zigzag
static const uint8_t LVL7[BRICK_ROWS][BRICK_COLS] = {
    {1,1,1,0,0,0,0,0,0,1,1,1},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,0,0,1,1,1,1,0,0,0,0},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {1,1,1,0,0,0,0,0,0,1,1,1},
    {0,0,1,1,1,0,0,1,1,1,0,0},
    {0,0,0,0,1,1,1,1,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

// Niveau 8 : Boss — ligne renforcée + indestructibles + bonus
static const uint8_t LVL8[BRICK_ROWS][BRICK_COLS] = {
    {3,3,3,3,3,3,3,3,3,3,3,3},
    {2,2,2,2,2,2,2,2,2,2,2,2},
    {2,4,2,4,2,4,2,4,2,4,2,4},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {3,0,3,0,3,0,3,0,3,0,3,0},
    {1,1,1,1,1,1,1,1,1,1,1,1},
    {2,4,2,4,2,4,2,4,2,4,2,4},
    {2,2,2,2,2,2,2,2,2,2,2,2},
    {0,0,0,0,0,0,0,0,0,0,0,0},
    {0,0,0,0,0,0,0,0,0,0,0,0},
};

static const uint8_t* LEVELS[8] = {
    (const uint8_t*)LVL1, (const uint8_t*)LVL2, (const uint8_t*)LVL3,
    (const uint8_t*)LVL4, (const uint8_t*)LVL5, (const uint8_t*)LVL6,
    (const uint8_t*)LVL7, (const uint8_t*)LVL8,
};

static const char* LEVEL_NAMES[8] = {
    "Mur plein", "Pyramide", "Colonnes", "Damier",
    "Couloirs", "Forteresse", "Zigzag", "Boss final"
};

// ===========================================================================
// 5. État runtime
// ===========================================================================

enum State : uint8_t {
    ST_OFF = 0, ST_HUB, ST_SETTINGS, ST_PLAYING, ST_PAUSED,
    ST_LEVELCLEAR, ST_GAMEOVER, ST_HIGHSCORES
};

struct Ball {
    float x, y;       // position du centre
    float vx, vy;     // vitesse (px/s)
    bool  active;
    bool  glued;      // collée à la raquette (power-up colle)
};

struct PowerUp {
    float x, y;
    uint8_t type;     // PowerType
    bool  active;
    lv_obj_t* obj;
};

struct Brick {
    uint8_t type;     // BrickType
    uint8_t hp;       // points de vie restants (pour BT_TOUGH)
    uint8_t row;      // rangée d'origine (pour la couleur)
    bool    alive;
    lv_obj_t* obj;
};

static ArkanoidSave g_save{};
static esphome::ESPPreferenceObject g_pref;
static bool  g_pref_ready = false;

static UI    g_ui{};
static bool  g_built = false;
static State g_state = ST_OFF;
static lv_timer_t* g_timer = nullptr;

// --- IMU / inclinaison ---
static float g_raw_x = 0.0f, g_raw_y = 0.0f;
static float g_tilt_x = 0.0f;  // valeur lissée (axe utile pour la raquette)

// --- Entrée boutons tactiles ---
static bool  g_btn_left = false;
static bool  g_btn_right = false;

// --- Raquette ---
static float g_pad_x = 0.0f;   // centre X de la raquette
static float g_pad_vx = 0.0f;  // vitesse (pour IMU)
static int   g_pad_w = PAD_W_DEFAULT;

// --- Balles ---
static Ball  g_balls[MAX_BALLS];
static int   g_ball_count = 0;

// --- Briques ---
static Brick g_bricks[MAX_BRICKS];
static int   g_bricks_alive = 0;  // compteur de briques destructibles vivantes

// --- Power-ups ---
static PowerUp g_powerups[MAX_POWERUPS];

// --- Partie en cours ---
static int      g_level = 0;        // index 0..7
static int      g_lives = LIVES_START;
static int      g_score = 0;
static int      g_combo = 0;
static uint32_t g_last_hit_ms = 0;  // dernier casse (pour combo)
static float    g_ball_speed = BALL_SPEED_0;
static bool     g_glue_active = false;
static bool     g_run_active = false;

// --- Objets LVGL (construits une fois) ---
static lv_obj_t* g_pad_obj = nullptr;
static lv_obj_t* g_ball_obj[MAX_BALLS] = {};
static lv_obj_t* g_hud_score = nullptr;
static lv_obj_t* g_hud_lives = nullptr;
static lv_obj_t* g_hud_level = nullptr;
static lv_obj_t* g_hud_best  = nullptr;
static lv_obj_t* g_hud_ctrl  = nullptr;
static lv_obj_t* g_btn_l = nullptr;   // bouton tactile gauche
static lv_obj_t* g_btn_r = nullptr;   // bouton tactile droite
static lv_obj_t* g_p_title = nullptr;
static lv_obj_t* g_p_sub   = nullptr;
static lv_obj_t* g_p_body  = nullptr;
static lv_obj_t* g_p_foot  = nullptr;
static constexpr int N_SLOTS = 8;
static lv_obj_t* g_slot[N_SLOTS] = {};
static lv_obj_t* g_slot_t[N_SLOTS] = {};
static lv_obj_t* g_slot_d[N_SLOTS] = {};

// Flash de mort (bandes de bord)
static lv_obj_t* g_vign[4] = {};
static uint32_t  g_vignette_until = 0;

// Caches HUD : on ne réécrit un libellé que si sa valeur a changé.
static int g_c_score = -1, g_c_lives = -1, g_c_level = -1, g_c_best = -1;

// ===========================================================================
// 6. Persistance NVS
// ===========================================================================

void persist_load() {
    if (!g_pref_ready) {
        g_pref = esphome::global_preferences->make_preference<ArkanoidSave>(PREF_KEY);
        g_pref_ready = true;
    }
    if (!g_pref.load(&g_save) || g_save.magic != SAVE_MAGIC) {
        g_save = ArkanoidSave{};
        g_save.magic = SAVE_MAGIC;
        g_save.ctrl_mode = 2;      // défaut : les deux
        g_save.sensitivity = 2;    // défaut : médian
    }
}

void persist_save() {
    if (!g_pref_ready) return;
    g_save.magic = SAVE_MAGIC;
    g_pref.save(&g_save);
    esphome::global_preferences->sync();
}

// Insère un score dans le top 10 (tri décroissant). Retourne true si qualifié.
static bool insert_score(uint32_t score, uint8_t level, uint8_t ctrl) {
    if (score == 0) return false;
    ArkScoreEntry entry;
    entry.score = score;
    entry.level = level;
    entry.ctrl_mode = ctrl;
    entry.pad = 0;
    entry.timestamp = (uint32_t)(esphome::millis() / 1000);

    int pos = g_save.score_count;
    for (int i = 0; i < g_save.score_count; i++) {
        if (score > g_save.scores[i].score) { pos = i; break; }
    }
    if (pos >= ARK_MAX_SCORES) return false;

    // Décale les entrées suivantes.
    int end = g_save.score_count < ARK_MAX_SCORES ? g_save.score_count : ARK_MAX_SCORES - 1;
    for (int i = end; i > pos; i--) g_save.scores[i] = g_save.scores[i - 1];
    g_save.scores[pos] = entry;
    if (g_save.score_count < ARK_MAX_SCORES) g_save.score_count++;
    persist_save();
    return true;
}

static uint32_t best_score() {
    return g_save.score_count > 0 ? g_save.scores[0].score : 0;
}

// ===========================================================================
// 7. Helpers LVGL
// ===========================================================================

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

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

static void set_text_if(lv_obj_t* l, const char* txt) {
    if (!l) return;
    const char* cur = lv_label_get_text(l);
    if (cur && strcmp(cur, txt) == 0) return;
    lv_label_set_text(l, txt);
}

// ===========================================================================
// 8. Construction de l'UI (une seule fois)
// ===========================================================================

static void slot_event_cb(lv_event_t* e);
static void btn_left_cb(lv_event_t* e);
static void btn_right_cb(lv_event_t* e);
static void hud_event_cb(lv_event_t* e);
static void field_tap_cb(lv_event_t* e);

static void build_ui() {
    if (g_built) return;

    // --- Pool de briques : 120 rectangles préalloués ---
    for (int i = 0; i < MAX_BRICKS; i++) {
        g_bricks[i].obj = mk_rect(g_ui.field);
        lv_obj_set_size(g_bricks[i].obj, BRICK_W, BRICK_H);
        lv_obj_add_flag(g_bricks[i].obj, LV_OBJ_FLAG_HIDDEN);
    }

    // --- Raquette ---
    g_pad_obj = mk_rect(g_ui.field);
    lv_obj_set_size(g_pad_obj, PAD_W_DEFAULT, PAD_H);
    lv_obj_set_style_radius(g_pad_obj, 4, LV_PART_MAIN);
    set_bg(g_pad_obj, UIColor::ARK_PADDLE, LV_OPA_COVER);
    lv_obj_set_pos(g_pad_obj, FW / 2 - PAD_W_DEFAULT / 2, PAD_Y);

    // --- Balles (pool de 3) ---
    for (int i = 0; i < MAX_BALLS; i++) {
        g_ball_obj[i] = mk_rect(g_ui.field);
        lv_obj_set_size(g_ball_obj[i], BALL_R * 2, BALL_R * 2);
        lv_obj_set_style_radius(g_ball_obj[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(g_ball_obj[i], UIColor::ARK_BALL, LV_OPA_COVER);
        lv_obj_add_flag(g_ball_obj[i], LV_OBJ_FLAG_HIDDEN);
    }

    // --- Power-ups (pool de 8) ---
    for (int i = 0; i < MAX_POWERUPS; i++) {
        g_powerups[i].obj = mk_rect(g_ui.field);
        lv_obj_set_size(g_powerups[i].obj, PU_W, PU_H);
        lv_obj_set_style_radius(g_powerups[i].obj, 4, LV_PART_MAIN);
        lv_obj_add_flag(g_powerups[i].obj, LV_OBJ_FLAG_HIDDEN);
        g_powerups[i].active = false;
    }

    // --- Boutons tactiles latéraux (semi-transparents, coins bas) ---
    g_btn_l = lv_obj_create(g_ui.field);
    lv_obj_remove_style_all(g_btn_l);
    lv_obj_set_size(g_btn_l, 180, 120);
    lv_obj_set_pos(g_btn_l, 0, FH - 120);
    lv_obj_set_style_radius(g_btn_l, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_btn_l, lv_color_hex(UIColor::ARK_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_btn_l, LV_OPA_50, LV_PART_MAIN);
    lv_obj_add_flag(g_btn_l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_btn_l, btn_left_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(g_btn_l, btn_left_cb, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(g_btn_l, btn_left_cb, LV_EVENT_PRESS_LOST, nullptr);
    lv_obj_add_flag(g_btn_l, LV_OBJ_FLAG_HIDDEN);

    g_btn_r = lv_obj_create(g_ui.field);
    lv_obj_remove_style_all(g_btn_r);
    lv_obj_set_size(g_btn_r, 180, 120);
    lv_obj_set_pos(g_btn_r, FW - 180, FH - 120);
    lv_obj_set_style_radius(g_btn_r, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_btn_r, lv_color_hex(UIColor::ARK_BTN), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_btn_r, LV_OPA_50, LV_PART_MAIN);
    lv_obj_add_flag(g_btn_r, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_btn_r, btn_right_cb, LV_EVENT_PRESSED, nullptr);
    lv_obj_add_event_cb(g_btn_r, btn_right_cb, LV_EVENT_RELEASED, nullptr);
    lv_obj_add_event_cb(g_btn_r, btn_right_cb, LV_EVENT_PRESS_LOST, nullptr);
    lv_obj_add_flag(g_btn_r, LV_OBJ_FLAG_HIDDEN);

    // --- Vignette de mort : 4 bandes fines (flash rouge) ---
    const int VB = 6;
    for (int i = 0; i < 4; i++) {
        g_vign[i] = mk_rect(g_ui.field);
        set_bg(g_vign[i], UIColor::ARK_DANGER, LV_OPA_COVER);
        lv_obj_add_flag(g_vign[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_pos(g_vign[0], 0, 0);        lv_obj_set_size(g_vign[0], FW, VB);
    lv_obj_set_pos(g_vign[1], 0, FH - VB);  lv_obj_set_size(g_vign[1], FW, VB);
    lv_obj_set_pos(g_vign[2], 0, 0);        lv_obj_set_size(g_vign[2], VB, FH);
    lv_obj_set_pos(g_vign[3], FW - VB, 0);  lv_obj_set_size(g_vign[3], VB, FH);

    // --- HUD : bande compacte de 48 px ---
    g_hud_score = mk_label(g_ui.hud, g_ui.f_small, UIColor::ARK_BALL);
    lv_obj_align(g_hud_score, LV_ALIGN_LEFT_MID, 18, 0);
    g_hud_lives = mk_label(g_ui.hud, g_ui.f_small, UIColor::ARK_DANGER);
    lv_obj_align(g_hud_lives, LV_ALIGN_LEFT_MID, 280, 0);
    g_hud_level = mk_label(g_ui.hud, g_ui.f_small, UIColor::ARK_CYAN);
    lv_obj_align(g_hud_level, LV_ALIGN_LEFT_MID, 450, 0);
    g_hud_best  = mk_label(g_ui.hud, g_ui.f_small, UIColor::TEXT_DIM);
    lv_obj_align(g_hud_best, LV_ALIGN_LEFT_MID, 700, 0);
    g_hud_ctrl  = mk_label(g_ui.hud, g_ui.f_small, UIColor::TEXT_DIM);
    lv_obj_align(g_hud_ctrl, LV_ALIGN_RIGHT_MID, -18, 0);

    // --- Panneau de menus ---
    g_p_title = mk_label(g_ui.panel, g_ui.f_big, UIColor::ARK_BALL);
    lv_obj_align(g_p_title, LV_ALIGN_TOP_MID, 0, 56);
    g_p_sub = mk_label(g_ui.panel, g_ui.f_small, UIColor::TEXT_DIM);
    lv_obj_align(g_p_sub, LV_ALIGN_TOP_MID, 0, 122);
    g_p_body = mk_label(g_ui.panel, g_ui.f_small, UIColor::TEXT_SOFT);
    lv_obj_set_width(g_p_body, 900);
    lv_obj_set_style_text_align(g_p_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g_p_body, LV_ALIGN_TOP_MID, 0, 170);
    g_p_foot = mk_label(g_ui.panel, g_ui.f_small, UIColor::TEXT_DIM);
    lv_obj_align(g_p_foot, LV_ALIGN_BOTTOM_MID, 0, -22);

    for (int i = 0; i < N_SLOTS; i++) {
        g_slot[i] = mk_rect(g_ui.panel);
        lv_obj_add_flag(g_slot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(g_slot[i], 14, LV_PART_MAIN);
        set_bg(g_slot[i], UIColor::ARK_FLOOR, LV_OPA_COVER);
        lv_obj_set_style_bg_color(g_slot[i], lv_color_hex(UIColor::ARK_WALL),
                                  (lv_style_selector_t)LV_PART_MAIN |
                                  (lv_style_selector_t)LV_STATE_PRESSED);
        lv_obj_add_event_cb(g_slot[i], slot_event_cb, LV_EVENT_CLICKED,
                            (void*)(intptr_t)i);
        g_slot_t[i] = mk_label(g_slot[i], g_ui.f_mid, UIColor::TEXT_SOFT);
        g_slot_d[i] = mk_label(g_slot[i], g_ui.f_small, UIColor::TEXT_DIM);
        lv_obj_add_flag(g_slot[i], LV_OBJ_FLAG_HIDDEN);
    }

    g_built = true;
}

// --- Mise en page des slots (liste verticale) ---
static void slot_list(int i, const char* title, const char* desc, uint32_t col, bool on) {
    lv_obj_set_size(g_slot[i], 680, 62);
    lv_obj_align(g_slot[i], LV_ALIGN_TOP_MID, 0, 150 + i * 68);
    lv_obj_set_width(g_slot_t[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(g_slot_t[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(g_slot_d[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(g_slot_d[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(g_slot_t[i], LV_ALIGN_LEFT_MID, 22, desc && desc[0] ? -13 : 0);
    lv_obj_align(g_slot_d[i], LV_ALIGN_LEFT_MID, 22, 15);
    lv_obj_set_style_text_color(g_slot_t[i], lv_color_hex(on ? col : UIColor::INACTIVE), LV_PART_MAIN);
    set_text_if(g_slot_t[i], title);
    set_text_if(g_slot_d[i], desc ? desc : "");
    set_border(g_slot[i], on ? col : UIColor::INACTIVE, 2, LV_OPA_50);
    show(g_slot[i], true);
}

static void slots_hide_from(int n) {
    for (int i = n; i < N_SLOTS; i++) show(g_slot[i], false);
}

// ===========================================================================
// 9. Écrans
// ===========================================================================

static void panel_on(bool v) {
    show(g_ui.panel, v);
    if (v) lv_obj_move_foreground(g_ui.panel);
}

static const char* ctrl_name() {
    switch (g_save.ctrl_mode) {
        case 0: return "Inclinaison";
        case 1: return "Boutons";
        default: return "Les deux";
    }
}

static void go_hub() {
    g_state = ST_HUB;
    panel_on(true);
    show(g_pad_obj, false);
    for (int i = 0; i < MAX_BALLS; i++) show(g_ball_obj[i], false);
    show(g_btn_l, false);
    show(g_btn_r, false);

    static char sub[128];
    snprintf(sub, sizeof(sub), "Meilleur score : %u   -   Controle : %s",
             (unsigned)best_score(), ctrl_name());

    set_text_if(g_p_title, "ARCANOIDE");
    set_text_if(g_p_sub, sub);
    set_text_if(g_p_body, "");
    set_text_if(g_p_foot, "Casse toutes les briques. Ne laisse pas tomber la balle.");
    slot_list(0, "Jouer", "8 niveaux, 3 vies, power-ups", UIColor::ARK_BALL, true);
    slot_list(1, "Classement", "Top 10 local", UIColor::ARK_CYAN, true);
    slot_list(2, "Reglages", "Controle, sensibilite, calibration, SFX", UIColor::ARK_GREEN, true);
    slot_list(3, "Quitter", "Retour au tableau de bord", UIColor::TEXT_DIM, true);
    slots_hide_from(4);
}

static void go_settings() {
    g_state = ST_SETTINGS;
    panel_on(true);

    static char ctrl_title[64];
    snprintf(ctrl_title, sizeof(ctrl_title), "Controle : %s", ctrl_name());
    static char sens_title[64];
    snprintf(sens_title, sizeof(sens_title), "Sensibilite IMU : %d/5",
             (int)g_save.sensitivity + 1);

    set_text_if(g_p_title, "Reglages");
    set_text_if(g_p_sub, "Ces reglages sont sauvegardes automatiquement.");
    set_text_if(g_p_body, "");
    set_text_if(g_p_foot, "");
    slot_list(0, ctrl_title, "Inclinaison / Boutons / Les deux", UIColor::ARK_CYAN, true);
    slot_list(1, sens_title, "Vitesse de reponse a l'inclinaison", UIColor::ARK_GREEN, true);
    slot_list(2, "Calibrer a plat", "Pose la tablette et appuie", UIColor::ARK_ORANGE, true);
    slot_list(3, g_save.muted ? "SFX : coupes" : "SFX : actifs",
              "Bips sonores (casse, mort, niveau)", UIColor::ARK_MAGENTA, true);
    slot_list(4, "Retour", "", UIColor::TEXT_DIM, true);
    slots_hide_from(5);
}

static void go_highscores() {
    g_state = ST_HIGHSCORES;
    panel_on(true);

    static char body[512];
    int off = 0;
    off += snprintf(body + off, sizeof(body) - off, "Rang  Score      Niv  Controle\n");
    for (int i = 0; i < g_save.score_count && i < ARK_MAX_SCORES; i++) {
        const ArkScoreEntry& e = g_save.scores[i];
        const char* cn = (e.ctrl_mode == 0) ? "IMU" : (e.ctrl_mode == 1) ? "Btn" : "Mix";
        off += snprintf(body + off, sizeof(body) - off, " %2d   %7u    %d    %s\n",
                        i + 1, (unsigned)e.score, (int)e.level, cn);
    }
    if (g_save.score_count == 0) {
        off += snprintf(body + off, sizeof(body) - off, "\n  Aucun score enregistre.");
    }

    set_text_if(g_p_title, "Classement");
    set_text_if(g_p_sub, "Top 10 local (NVS)");
    set_text_if(g_p_body, body);
    set_text_if(g_p_foot, "");
    slot_list(0, "Effacer les scores", "Appuie pour confirmer", UIColor::ARK_DANGER, true);
    slot_list(1, "Retour", "", UIColor::TEXT_DIM, true);
    lv_obj_align(g_slot[0], LV_ALIGN_BOTTOM_MID, 0, -160);
    lv_obj_align(g_slot[1], LV_ALIGN_BOTTOM_MID, 0, -80);
    slots_hide_from(2);
}

static void show_pause() {
    g_state = ST_PAUSED;
    panel_on(true);
    set_text_if(g_p_title, "Pause");
    set_text_if(g_p_sub, "Le jeu attend.");
    set_text_if(g_p_body, "");
    set_text_if(g_p_foot, "");
    slot_list(0, "Reprendre", "", UIColor::ARK_BALL, true);
    slot_list(1, "Recalibrer a plat", "Pose la tablette avant d'appuyer", UIColor::ARK_ORANGE, true);
    slot_list(2, "Abandonner", "Le score est enregistre", UIColor::ARK_DANGER, true);
    slots_hide_from(3);
}

static void show_level_clear() {
    g_state = ST_LEVELCLEAR;
    panel_on(true);
    static char body[128];
    snprintf(body, sizeof(body), "Niveau %d — %s\nScore : %d",
             g_level + 1, LEVEL_NAMES[g_level], g_score);
    set_text_if(g_p_title, "Niveau termine !");
    set_text_if(g_p_sub, "");
    set_text_if(g_p_body, body);
    set_text_if(g_p_foot, "");
    slot_list(0, "Niveau suivant", "", UIColor::ARK_GREEN, true);
    slots_hide_from(1);
    lv_obj_align(g_slot[0], LV_ALIGN_BOTTOM_MID, 0, -120);
}

static void show_gameover() {
    g_state = ST_GAMEOVER;
    panel_on(true);
    show(g_pad_obj, false);
    for (int i = 0; i < MAX_BALLS; i++) show(g_ball_obj[i], false);
    show(g_btn_l, false);
    show(g_btn_r, false);

    bool qualified = insert_score((uint32_t)g_score, (uint8_t)(g_level + 1), g_save.ctrl_mode);
    static char body[192];
    snprintf(body, sizeof(body), "Score : %d\nNiveau atteint : %d/8 — %s%s",
             g_score, g_level + 1, LEVEL_NAMES[g_level],
             qualified ? "\n*** Nouveau record ! ***" : "");
    set_text_if(g_p_title, "GAME OVER");
    set_text_if(g_p_sub, "");
    set_text_if(g_p_body, body);
    set_text_if(g_p_foot, "");
    slot_list(0, "Rejouer", "", UIColor::ARK_BALL, true);
    slot_list(1, "Retour au hub", "", UIColor::TEXT_DIM, true);
    lv_obj_align(g_slot[0], LV_ALIGN_BOTTOM_MID, 0, -180);
    lv_obj_align(g_slot[1], LV_ALIGN_BOTTOM_MID, 0, -100);
    slots_hide_from(2);
}

// ===========================================================================
// 10. Chargement d'un niveau
// ===========================================================================

static void load_level(int idx) {
    g_level = idx;
    g_bricks_alive = 0;
    g_glue_active = false;
    g_pad_w = PAD_W_DEFAULT;
    g_ball_speed = BALL_SPEED_0 + BALL_SPEED_INC * idx;
    if (g_ball_speed > BALL_SPEED_MAX) g_ball_speed = BALL_SPEED_MAX;

    const uint8_t* layout = LEVELS[idx];

    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BRICK_COLS; c++) {
            int i = r * BRICK_COLS + c;
            uint8_t val = layout[r * BRICK_COLS + c];
            Brick& b = g_bricks[i];
            b.row = (uint8_t)r;

            if (val == 0) {
                b.type = BT_EMPTY;
                b.alive = false;
                b.hp = 0;
                show(b.obj, false);
                continue;
            }

            b.type = (BrickType)val;
            b.alive = true;
            switch (val) {
                case BT_NORMAL: b.hp = 1; break;
                case BT_TOUGH:  b.hp = 2 + (idx >= 6 ? 1 : 0); break;  // 2 ou 3 PV
                case BT_INDESTRUCT: b.hp = 255; break;
                case BT_BONUS:  b.hp = 1; break;
                default: b.hp = 1; break;
            }

            if (val != BT_INDESTRUCT) g_bricks_alive++;

            // Position et style
            int px = BRICK_MARGIN_X + c * (BRICK_W + BRICK_GAP);
            int py = BRICK_TOP + r * (BRICK_H + BRICK_GAP);
            lv_obj_set_pos(b.obj, px, py);
            lv_obj_set_size(b.obj, BRICK_W, BRICK_H);

            uint32_t col;
            if (val == BT_INDESTRUCT) {
                col = UIColor::ARK_WALL;
                lv_obj_set_style_radius(b.obj, 2, LV_PART_MAIN);
            } else if (val == BT_TOUGH) {
                col = UIColor::ARK_TOUGH;
                lv_obj_set_style_radius(b.obj, 3, LV_PART_MAIN);
            } else if (val == BT_BONUS) {
                col = UIColor::ARK_MAGENTA;
                lv_obj_set_style_radius(b.obj, 3, LV_PART_MAIN);
            } else {
                col = ROW_COLORS[r % 8];
                lv_obj_set_style_radius(b.obj, 3, LV_PART_MAIN);
            }
            set_bg(b.obj, col, LV_OPA_COVER);
            set_border(b.obj, 0x000000, 1, LV_OPA_30);
            show(b.obj, true);
        }
    }

    // Raquette : centrée
    g_pad_x = FW / 2.0f;
    g_pad_vx = 0;
    lv_obj_set_size(g_pad_obj, g_pad_w, PAD_H);
    lv_obj_set_pos(g_pad_obj, (int)(g_pad_x - g_pad_w / 2.0f), PAD_Y);
    show(g_pad_obj, true);

    // Une seule balle, collée à la raquette
    g_ball_count = 1;
    g_balls[0].x = g_pad_x;
    g_balls[0].y = PAD_Y - BALL_R - 2;
    g_balls[0].vx = 0;
    g_balls[0].vy = 0;
    g_balls[0].active = true;
    g_balls[0].glued = true;
    for (int i = 1; i < MAX_BALLS; i++) g_balls[i].active = false;

    lv_obj_set_pos(g_ball_obj[0], (int)(g_balls[0].x - BALL_R), (int)(g_balls[0].y - BALL_R));
    show(g_ball_obj[0], true);
    for (int i = 1; i < MAX_BALLS; i++) show(g_ball_obj[i], false);

    // Power-ups : tous désactivés
    for (int i = 0; i < MAX_POWERUPS; i++) {
        g_powerups[i].active = false;
        show(g_powerups[i].obj, false);
    }

    // Boutons tactiles visibles si mode boutons ou les deux
    bool show_btns = (g_save.ctrl_mode >= 1);
    show(g_btn_l, show_btns);
    show(g_btn_r, show_btns);

    // HUD : reset caches
    g_c_score = -1; g_c_lives = -1; g_c_level = -1; g_c_best = -1;
}

static void start_game() {
    s_rng = (uint32_t)(esphome::millis() ^ 0x9E3779B9u);
    if (s_rng == 0) s_rng = 0xDEADBEEFu;

    g_score = 0;
    g_lives = LIVES_START;
    g_combo = 0;
    g_last_hit_ms = 0;
    g_run_active = true;

    load_level(0);
    g_state = ST_PLAYING;
    panel_on(false);
}

// ===========================================================================
// 11. Physique & collisions
// ===========================================================================

// --- Stubs audio (SFX) -------------------------------------------------------
// Le speaker I2S du Tab5 est piloté par media_player (HA). Pour des bips locaux
// sans dépendance HA, il faudrait un composant `rtttl` ou `speaker` dédié.
// En l'état : stubs no-op clairement marqués, activables ultérieurement.
static inline void sfx_brick_break() { /* STUB : bip court casse brique */ }
static inline void sfx_ball_lost()   { /* STUB : bip grave perte balle */ }
static inline void sfx_level_clear() { /* STUB : arpège victoire niveau */ }
static inline void sfx_powerup()     { /* STUB : bip montant power-up */ }
static inline void sfx_game_over()   { /* STUB : descente game over */ }

// Lance une balle collée (direction : légèrement aléatoire vers le haut).
static void launch_ball(Ball& b) {
    if (!b.glued) return;
    b.glued = false;
    float angle = -1.5708f + (rnd_range(-20, 20) / 100.0f);  // ~-90° ± 11°
    b.vx = g_ball_speed * cosf(angle);
    b.vy = g_ball_speed * sinf(angle);
}

// Spawne un power-up à la position d'une brique cassée.
static void spawn_powerup(float x, float y) {
    // Cherche un slot libre
    int slot = -1;
    for (int i = 0; i < MAX_POWERUPS; i++) {
        if (!g_powerups[i].active) { slot = i; break; }
    }
    if (slot < 0) return;

    // Tirage du type (pondéré : extra vie rare)
    int roll = rnd_range(0, 99);
    uint8_t type;
    if (roll < 20)      type = PU_EXPAND;
    else if (roll < 32) type = PU_SHRINK;
    else if (roll < 47) type = PU_SLOW;
    else if (roll < 57) type = PU_FAST;
    else if (roll < 75) type = PU_MULTI;
    else if (roll < 92) type = PU_GLUE;
    else                type = PU_LIFE;   // 8 % — rare

    PowerUp& pu = g_powerups[slot];
    pu.x = x; pu.y = y;
    pu.type = type;
    pu.active = true;

    // Couleur selon le type
    uint32_t col;
    switch (type) {
        case PU_EXPAND: col = UIColor::ARK_GREEN; break;
        case PU_SHRINK: col = UIColor::ARK_ORANGE; break;
        case PU_SLOW:   col = UIColor::ARK_CYAN; break;
        case PU_FAST:   col = UIColor::ARK_DANGER; break;
        case PU_MULTI:  col = UIColor::ARK_BALL; break;
        case PU_GLUE:   col = UIColor::ARK_MAGENTA; break;
        case PU_LIFE:   col = UIColor::ARK_GREEN; break;
        default:        col = UIColor::TEXT_DIM; break;
    }
    set_bg(pu.obj, col, LV_OPA_COVER);
    set_border(pu.obj, 0xFFFFFF, 1, LV_OPA_60);
    lv_obj_set_pos(pu.obj, (int)(x - PU_W / 2), (int)y);
    show(pu.obj, true);
}

// Applique l'effet d'un power-up ramassé.
static void apply_powerup(uint8_t type) {
    switch (type) {
        case PU_EXPAND:
            g_pad_w = (g_pad_w + 50 <= PAD_W_MAX) ? g_pad_w + 50 : PAD_W_MAX;
            lv_obj_set_size(g_pad_obj, g_pad_w, PAD_H);
            break;
        case PU_SHRINK:
            g_pad_w = (g_pad_w - 40 >= PAD_W_MIN) ? g_pad_w - 40 : PAD_W_MIN;
            lv_obj_set_size(g_pad_obj, g_pad_w, PAD_H);
            break;
        case PU_SLOW:
            g_ball_speed = (g_ball_speed > 280.0f) ? g_ball_speed - 80.0f : 280.0f;
            break;
        case PU_FAST:
            g_ball_speed = (g_ball_speed < BALL_SPEED_MAX) ? g_ball_speed + 60.0f : BALL_SPEED_MAX;
            break;
        case PU_MULTI:
            // Ajoute des balles (max 3) depuis une balle active
            if (g_ball_count < MAX_BALLS) {
                for (int i = 0; i < MAX_BALLS && g_ball_count < MAX_BALLS; i++) {
                    if (g_balls[i].active && !g_balls[i].glued) {
                        // Trouve un slot libre
                        for (int j = 0; j < MAX_BALLS; j++) {
                            if (!g_balls[j].active) {
                                g_balls[j] = g_balls[i];
                                float ang = (rnd_range(-40, 40) / 100.0f);
                                float sp = sqrtf(g_balls[j].vx * g_balls[j].vx +
                                                 g_balls[j].vy * g_balls[j].vy);
                                g_balls[j].vx = sp * sinf(ang);
                                g_balls[j].vy = -sp * cosf(ang);
                                g_balls[j].active = true;
                                show(g_ball_obj[j], true);
                                g_ball_count++;
                                break;
                            }
                        }
                        break;  // une seule duplication
                    }
                }
            }
            break;
        case PU_GLUE:
            g_glue_active = true;
            break;
        case PU_LIFE:
            if (g_lives < 5) g_lives++;
            break;
        default: break;
    }
}

// Flash de mort (bandes rouges)
static void flash_death() {
    g_vignette_until = lv_tick_get() + 300;
    for (int i = 0; i < 4; i++) show(g_vign[i], true);
}

// Perte d'une balle : -1 vie si c'était la dernière.
static void lose_ball(int idx) {
    g_balls[idx].active = false;
    show(g_ball_obj[idx], false);
    g_ball_count--;

    if (g_ball_count <= 0) {
        g_lives--;
        flash_death();
        if (g_lives <= 0) {
            g_run_active = false;
            show_gameover();
            return;
        }
        // Respawn : une balle collée
        g_ball_count = 1;
        g_balls[0].x = g_pad_x;
        g_balls[0].y = PAD_Y - BALL_R - 2;
        g_balls[0].vx = 0; g_balls[0].vy = 0;
        g_balls[0].active = true;
        g_balls[0].glued = true;
        show(g_ball_obj[0], true);
        lv_obj_set_pos(g_ball_obj[0], (int)(g_balls[0].x - BALL_R), (int)(g_balls[0].y - BALL_R));
    }
}

// Collision balle/brique : retourne true si la brique est touchée.
static bool ball_hits_brick(Ball& b, Brick& br, int bx, int by) {
    if (!br.alive || br.type == BT_EMPTY) return false;
    // AABB vs cercle
    float cx = clampf(b.x, (float)bx, (float)(bx + BRICK_W));
    float cy = clampf(b.y, (float)by, (float)(by + BRICK_H));
    float dx = b.x - cx, dy = b.y - cy;
    if (dx * dx + dy * dy >= (float)(BALL_R * BALL_R)) return false;

    // Réflexion : détermine l'axe de pénétration principale
    float overlap_x = BALL_R - fabsf(dx);
    float overlap_y = BALL_R - fabsf(dy);
    if (overlap_x < overlap_y) {
        b.vx = -b.vx;
        b.x += (dx > 0 ? overlap_x : -overlap_x);
    } else {
        b.vy = -b.vy;
        b.y += (dy > 0 ? overlap_y : -overlap_y);
    }

    // Indestructible : pas de dégât
    if (br.type == BT_INDESTRUCT) return true;

    br.hp--;
    if (br.hp == 0) {
        br.alive = false;
        show(br.obj, false);
        g_bricks_alive--;

        // Score + combo
        uint32_t now = lv_tick_get();
        if (now - g_last_hit_ms < COMBO_WINDOW_MS && g_combo < COMBO_MAX) g_combo++;
        else g_combo = 1;
        g_last_hit_ms = now;
        int pts = (br.type == BT_TOUGH) ? SCORE_TOUGH : SCORE_BRICK;
        g_score += pts * g_combo;

        // Bonus : lâche un power-up
        if (br.type == BT_BONUS) {
            int px = bx + BRICK_W / 2;
            int py = by + BRICK_H / 2;
            spawn_powerup((float)px, (float)py);
        }
        // Chance aléatoire de power-up sur brique normale (12 %)
        else if (br.type == BT_NORMAL && rnd_range(0, 99) < 12) {
            int px = bx + BRICK_W / 2;
            int py = by + BRICK_H / 2;
            spawn_powerup((float)px, (float)py);
        }
    } else {
        // Brique renforcée : change de teinte selon les PV restants
        uint32_t col = (br.hp == 1) ? UIColor::ARK_TOUGH_HIT : UIColor::ARK_TOUGH;
        set_bg(br.obj, col, LV_OPA_COVER);
    }
    return true;
}

// ===========================================================================
// 12. Boucle de jeu (tick)
// ===========================================================================

static void update_hud() {
    static char buf[64];
    if (g_c_score != g_score) {
        g_c_score = g_score;
        snprintf(buf, sizeof(buf), "Score %d", g_score);
        set_text_if(g_hud_score, buf);
    }
    if (g_c_lives != g_lives) {
        g_c_lives = g_lives;
        snprintf(buf, sizeof(buf), "Vies %d", g_lives);
        set_text_if(g_hud_lives, buf);
    }
    if (g_c_level != g_level) {
        g_c_level = g_level;
        snprintf(buf, sizeof(buf), "Niv %d/8", g_level + 1);
        set_text_if(g_hud_level, buf);
    }
    uint32_t bs = best_score();
    if ((int)bs != g_c_best) {
        g_c_best = (int)bs;
        snprintf(buf, sizeof(buf), "Best %u", (unsigned)bs);
        set_text_if(g_hud_best, buf);
    }
    // Indicateur contrôle (écrit une fois)
    static char cbuf[32];
    snprintf(cbuf, sizeof(cbuf), "[%s]", ctrl_name());
    set_text_if(g_hud_ctrl, cbuf);
}

static void tick_cb(lv_timer_t*) {
    if (g_state != ST_PLAYING) return;
    uint32_t now = lv_tick_get();

    // --- Entrée raquette ---
    float pad_input = 0.0f;

    // IMU : rotation 270° → X_écran = -tilt_Y
    if (g_save.ctrl_mode == 0 || g_save.ctrl_mode == 2) {
        float oy = g_save.cal_y / 1000.0f;
        float raw = g_raw_y - oy;  // axe Y physique → X écran (inversé)
        g_tilt_x += ((-raw) - g_tilt_x) * TILT_SMOOTH;

        float dead = TILT_DEADZONE;
        float t = g_tilt_x;
        float sens_scale = 0.6f + 0.2f * g_save.sensitivity;  // 0.6..1.4
        if (fabsf(t) < dead) t = 0;
        else t = (t > 0 ? t - dead : t + dead) * sens_scale;
        t = clampf(t, -TILT_CLAMP, TILT_CLAMP);
        pad_input += t;
    }

    // Boutons tactiles
    if (g_save.ctrl_mode == 1 || g_save.ctrl_mode == 2) {
        if (g_btn_left)  pad_input -= 1.0f;
        if (g_btn_right) pad_input += 1.0f;
    }
    pad_input = clampf(pad_input, -1.0f, 1.0f);

    // Déplacement raquette
    if (g_save.ctrl_mode == 0 || (g_save.ctrl_mode == 2 && fabsf(pad_input) > 0.01f)) {
        // Mode IMU : accélération + friction
        g_pad_vx += pad_input * PAD_ACCEL_IMU * DT;
        g_pad_vx *= PAD_FRICTION;
        g_pad_vx = clampf(g_pad_vx, -PAD_MAX_IMU, PAD_MAX_IMU);
        g_pad_x += g_pad_vx * DT;
    }
    if (g_save.ctrl_mode == 1) {
        // Mode boutons purs : vitesse directe (en mode mixte, déjà compté dans pad_input)
        if (g_btn_left || g_btn_right) {
            float dir = (g_btn_right ? 1.0f : 0.0f) - (g_btn_left ? 1.0f : 0.0f);
            g_pad_x += dir * PAD_SPEED_BTN * DT;
        }
    }
    // Clamp raquette dans le terrain
    float half = g_pad_w / 2.0f;
    g_pad_x = clampf(g_pad_x, half, (float)(FW - half));
    lv_obj_set_pos(g_pad_obj, (int)(g_pad_x - half), PAD_Y);

    // --- Balles : sous-pas de collision ---
    for (int bi = 0; bi < MAX_BALLS; bi++) {
        Ball& b = g_balls[bi];
        if (!b.active) continue;

        // Balle collée : suit la raquette
        if (b.glued) {
            b.x = g_pad_x;
            b.y = PAD_Y - BALL_R - 2;
            lv_obj_set_pos(g_ball_obj[bi], (int)(b.x - BALL_R), (int)(b.y - BALL_R));
            continue;
        }

        // Normalise la vitesse à g_ball_speed (évite accélération/décélération parasite)
        float spd = sqrtf(b.vx * b.vx + b.vy * b.vy);
        if (spd > 0.1f) {
            float k = g_ball_speed / spd;
            b.vx *= k; b.vy *= k;
        }

        for (int step = 0; step < SUBSTEP; step++) {
            b.x += b.vx * SDT;
            b.y += b.vy * SDT;

            // Murs latéraux
            if (b.x - BALL_R < 0) { b.x = BALL_R; b.vx = fabsf(b.vx); }
            if (b.x + BALL_R > FW) { b.x = FW - BALL_R; b.vx = -fabsf(b.vx); }
            // Plafond
            if (b.y - BALL_R < 0) { b.y = BALL_R; b.vy = fabsf(b.vy); }

            // Raquette : collision uniquement si la balle descend
            if (b.vy > 0 && b.y + BALL_R >= PAD_Y && b.y + BALL_R <= PAD_Y + PAD_H + 8 &&
                b.x >= g_pad_x - half - BALL_R && b.x <= g_pad_x + half + BALL_R) {
                b.y = PAD_Y - BALL_R;
                // Angle selon le point d'impact (centre = vertical, bord = oblique)
                float rel = (b.x - g_pad_x) / half;  // -1..+1
                rel = clampf(rel, -1.0f, 1.0f);
                float angle = rel * 1.1f;  // ±63°
                b.vx = g_ball_speed * sinf(angle);
                b.vy = -g_ball_speed * cosf(angle);
                // Colle : si actif, la balle se recolle
                if (g_glue_active) {
                    b.glued = true;
                    b.vx = 0; b.vy = 0;
                }
            }

            // Briques
            for (int r = 0; r < BRICK_ROWS; r++) {
                for (int c = 0; c < BRICK_COLS; c++) {
                    int idx = r * BRICK_COLS + c;
                    Brick& br = g_bricks[idx];
                    if (!br.alive) continue;
                    int bx = BRICK_MARGIN_X + c * (BRICK_W + BRICK_GAP);
                    int by = BRICK_TOP + r * (BRICK_H + BRICK_GAP);
                    ball_hits_brick(b, br, bx, by);
                }
            }

            // Perte sous le plancher
            if (b.y - BALL_R > FH) {
                lose_ball(bi);
                break;  // la balle n'est plus active
            }
        }

        // Met à jour la position LVGL
        if (b.active) {
            lv_obj_set_pos(g_ball_obj[bi], (int)(b.x - BALL_R), (int)(b.y - BALL_R));
        }
    }

    // --- Power-ups : chute + ramassage ---
    for (int i = 0; i < MAX_POWERUPS; i++) {
        PowerUp& pu = g_powerups[i];
        if (!pu.active) continue;
        pu.y += PU_FALL_SPEED * DT;
        lv_obj_set_pos(pu.obj, (int)(pu.x - PU_W / 2), (int)pu.y);

        // Ramassage par la raquette
        if (pu.y + PU_H >= PAD_Y && pu.y <= PAD_Y + PAD_H &&
            pu.x + PU_W / 2 >= g_pad_x - half && pu.x - PU_W / 2 <= g_pad_x + half) {
            apply_powerup(pu.type);
            pu.active = false;
            show(pu.obj, false);
        }
        // Sortie écran
        else if (pu.y > FH) {
            pu.active = false;
            show(pu.obj, false);
        }
    }

    // --- Fin de niveau : toutes les briques destructibles cassées ---
    if (g_bricks_alive <= 0 && g_state == ST_PLAYING) {
        if (g_level >= 7) {
            // Dernier niveau terminé = victoire = game over avec score
            g_run_active = false;
            show_gameover();
        } else {
            show_level_clear();
        }
        return;
    }

    // --- Flash de mort : extinction ---
    if (g_vignette_until > 0 && now > g_vignette_until) {
        g_vignette_until = 0;
        for (int i = 0; i < 4; i++) show(g_vign[i], false);
    }

    update_hud();
}

// ===========================================================================
// 13. Interactions tactiles (menus + boutons jeu)
// ===========================================================================

static void btn_left_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) g_btn_left = true;
    else g_btn_left = false;  // RELEASED ou PRESS_LOST
}

static void btn_right_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED) g_btn_right = true;
    else g_btn_right = false;
}

// Tap sur le field pendant le jeu : lance la balle collée / désactive la colle.
static void field_tap_cb(lv_event_t*) {
    if (g_state != ST_PLAYING) return;
    if (g_glue_active) {
        g_glue_active = false;
        // Lance toutes les balles collées
        for (int i = 0; i < MAX_BALLS; i++) {
            if (g_balls[i].active && g_balls[i].glued) launch_ball(g_balls[i]);
        }
    } else {
        for (int i = 0; i < MAX_BALLS; i++) {
            if (g_balls[i].active && g_balls[i].glued) launch_ball(g_balls[i]);
        }
    }
}

static void hud_event_cb(lv_event_t*) {
    if (g_state == ST_PLAYING) show_pause();
}

static void slot_event_cb(lv_event_t* e) {
    int i = (int)(intptr_t)lv_event_get_user_data(e);

    switch (g_state) {
        case ST_HUB:
            if (i == 0) start_game();
            else if (i == 1) go_highscores();
            else if (i == 2) go_settings();
            else if (i == 3) close();
            break;

        case ST_SETTINGS:
            if (i == 0) {
                g_save.ctrl_mode = (uint8_t)((g_save.ctrl_mode + 1) % 3);
                persist_save();
                go_settings();
            } else if (i == 1) {
                g_save.sensitivity = (uint8_t)((g_save.sensitivity + 1) % 5);
                persist_save();
                go_settings();
            } else if (i == 2) {
                calibrate();
                set_text_if(g_p_foot, "Calibration prise. Tablette = plat.");
            } else if (i == 3) {
                g_save.muted = g_save.muted ? 0 : 1;
                persist_save();
                go_settings();
            } else {
                go_hub();
            }
            break;

        case ST_HIGHSCORES:
            if (i == 0) {
                // Effacer les scores
                g_save.score_count = 0;
                memset(g_save.scores, 0, sizeof(g_save.scores));
                persist_save();
                go_highscores();
            } else {
                go_hub();
            }
            break;

        case ST_PAUSED:
            if (i == 0) { g_state = ST_PLAYING; panel_on(false); }
            else if (i == 1) { calibrate(); set_text_if(g_p_sub, "Calibration prise."); }
            else if (i == 2) {
                g_run_active = false;
                insert_score((uint32_t)g_score, (uint8_t)(g_level + 1), g_save.ctrl_mode);
                go_hub();
            }
            break;

        case ST_LEVELCLEAR:
            if (i == 0) {
                load_level(g_level + 1);
                g_state = ST_PLAYING;
                panel_on(false);
            }
            break;

        case ST_GAMEOVER:
            if (i == 0) start_game();
            else go_hub();
            break;

        default: break;
    }
}

// ===========================================================================
// 14. API publique
// ===========================================================================

void on_imu(float ax, float ay, float /*az*/) {
    if (ax != ax || ay != ay) return;  // garde NaN
    g_raw_x = ax;
    g_raw_y = ay;
}

void calibrate() {
    g_save.cal_x = (int16_t)(g_raw_x * 1000.0f);
    g_save.cal_y = (int16_t)(g_raw_y * 1000.0f);
    g_tilt_x = 0;
    persist_save();
}

bool is_open() { return g_state != ST_OFF; }

void open(const UI& ui) {
    if (g_state != ST_OFF) return;
    if (!ui.root || !ui.field || !ui.hud || !ui.panel) return;
    g_ui = ui;

    persist_load();
    build_ui();

    // La page LVGL est déjà active (navigation via lvgl.page.show dans le YAML).

    g_run_active = false;
    go_hub();

    // Pause via tap HUD ; lancement balle via tap field.
    lv_obj_add_flag(g_ui.hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.hud, hud_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_flag(g_ui.field, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.field, field_tap_cb, LV_EVENT_CLICKED, nullptr);

    if (!g_timer) g_timer = lv_timer_create(tick_cb, 33, nullptr);
}

void close() {
    if (g_state == ST_OFF) return;

    // Sauvegarde le score en cours si une partie est active.
    if (g_run_active && g_score > 0) {
        insert_score((uint32_t)g_score, (uint8_t)(g_level + 1), g_save.ctrl_mode);
    }
    g_run_active = false;
    persist_save();

    if (g_timer) { lv_timer_delete(g_timer); g_timer = nullptr; }
    if (g_ui.hud) lv_obj_remove_event_cb(g_ui.hud, hud_event_cb);
    if (g_ui.field) lv_obj_remove_event_cb(g_ui.field, field_tap_cb);
    // Navigation retour vers le sélecteur arcade (page LVGL).
    if (g_ui.lvgl) g_ui.lvgl->show_page(g_ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
    g_state = ST_OFF;
    g_btn_left = false;
    g_btn_right = false;
}

}  // namespace Arkanoid
