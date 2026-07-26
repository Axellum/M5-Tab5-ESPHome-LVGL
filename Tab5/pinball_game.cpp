/**
 * [AI-CONTEXT]
 * @file pinball_game.cpp
 * @role Jeu « Flip Noir » — flipper rétro type borne arcade 70-80's.
 * @architecture_constraint Plein ecran 1280x720. Le YAML ne fournit que 4
 *      conteneurs vides + 3 polices ; tout le reste est construit ici. Les objets
 *      LVGL sont PREALLOUES une seule fois (pool) puis reutilises par show/hide +
 *      move : aucune allocation LVGL dans la boucle de jeu. Persistance NVS via
 *      esphome::global_preferences (aucune dependance Home Assistant).
 * @ai_instruction Hot-path = tick() : pas de std::string, pas de to_string(), pas
 *      de new/delete. Les libelles HUD ne sont reecrits que quand leur valeur change.
 *      Couleurs : uniquement UIColor::PIN_* (jamais d'hex en dur ici).
 *      Table ORIENTATION : paysage 1280x672, drain en bas, plunger a droite.
 */
#include "pinball_game.h"
#include "esphome/core/preferences.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace Pinball {

// ===========================================================================
// 1. Geometrie & reglages de physique
// ===========================================================================

static constexpr int   FW = 1280;   // largeur du terrain
static constexpr int   FH = 672;    // hauteur du terrain (720 - bandeau HUD 48)
static constexpr int   HUD_H = 48;

static constexpr float DT      = 0.0222f;   // pas de la boucle (~45 Hz)
static constexpr int   SUBSTEP = 3;         // sous-pas de collision (anti-tunnelling)
static constexpr float SDT     = DT / SUBSTEP;

// --- Bille ---
static constexpr float BALL_R     = 10.0f;   // rayon de la bille (px)
static constexpr float GRAVITY    = 980.0f;  // gravite table (px/s^2) — vers le bas
static constexpr float FRICTION   = 0.9985f; // friction par sous-pas
static constexpr float MAX_SPEED  = 1400.0f; // vitesse max (px/s)
static constexpr float RESTITUTION = 0.35f;  // rebond murs
static constexpr float BUMPER_FORCE = 680.0f; // impulsion bumper (px/s)
static constexpr float SLING_FORCE  = 520.0f; // impulsion slingshot

// --- Flippers ---
// [AI-CONTEXT] Articulation flipper : chaque flipper est un segment pivotant
// autour d'un point fixe. L'angle au repos est ~30 deg sous l'horizontale,
// l'angle actif est ~-25 deg (rotation rapide vers le haut). La collision
// bille-flipper utilise la vitesse angulaire pour transmettre l'impulsion.
static constexpr float FLIP_LEN     = 95.0f;  // longueur du flipper (px)
static constexpr float FLIP_REST_L  = 0.52f;  // angle repos gauche (rad, + = vers le bas)
static constexpr float FLIP_ACTIVE_L = -0.44f; // angle actif gauche
static constexpr float FLIP_REST_R  = 3.14f - 0.52f;  // miroir droit
static constexpr float FLIP_ACTIVE_R = 3.14f + 0.44f;
static constexpr float FLIP_SPEED   = 14.0f;  // vitesse angulaire (rad/s)
static constexpr float FLIP_IMPULSE = 1.6f;   // multiplicateur d'impulsion flipper

// --- Plunger ---
// [AI-CONTEXT] Le plunger charge tant que le joueur maintient la zone tactile.
// La force est proportionnelle au temps de charge (max ~1.2 s), puis la bille
// est propulsee vers le haut a la release.
static constexpr float PLUNGER_MAX_CHARGE = 1.2f;   // secondes de charge max
static constexpr float PLUNGER_MIN_FORCE  = 350.0f; // impulsion min (px/s)
static constexpr float PLUNGER_MAX_FORCE  = 1100.0f;// impulsion max

// --- Tilt / Nudge IMU ---
// [AI-CONTEXT] Le nudge deplace la table (effet de gravite laterale). Un compteur
// d'abuse incremente a chaque nudge franc ; au-dela du seuil, TILT : flippers
// morts pendant 2.5 s + message. Le tilt se reinitialise au drain.
static constexpr float TILT_DEADZONE  = 0.06f;
static constexpr float TILT_SMOOTH    = 0.30f;
static constexpr float TILT_FORCE     = 400.0f;  // px/s^2 par g d'inclinaison
static constexpr int   TILT_MAX_HITS  = 3;       // nudges avant TILT
static constexpr uint32_t TILT_PENALTY_MS = 2500; // duree flippers morts

// --- Multiball ---
static constexpr int   MB_TARGETS_NEEDED = 3;  // cibles drop pour activer
static constexpr int   MB_MAX_BALLS      = 2;  // billes simultanees max

// --- Score ---
static constexpr int   SCORE_BUMPER   = 1000;
static constexpr int   SCORE_SLING    = 500;
static constexpr int   SCORE_TARGET   = 2500;
static constexpr int   SCORE_SPINNER  = 150;
static constexpr int   SCORE_LANE     = 750;
static constexpr int   SCORE_JACKPOT  = 25000;
static constexpr int   BONUS_BALL_1   = 50000;  // seuil bille bonus 1
static constexpr int   BONUS_BALL_2   = 150000; // seuil bille bonus 2
static constexpr int   MAX_BALLS      = 3;      // billes par partie (base)
static constexpr int   MAX_SIM_BALLS  = 3;      // pool billes simultanees

// --- Modes score ---
static constexpr uint32_t MODE_FRENZY_MS  = 10000; // Bumper Frenzy 10 s
static constexpr uint32_t MODE_MANIA_MS   = 8000;  // Target Mania 8 s

// --- NVS ---
// "PIN1" — bumpe a chaque changement de layout de PinballSave.
static constexpr uint32_t SAVE_MAGIC = 0x50494E31u;  // "PIN1"
static constexpr uint32_t PREF_KEY   = 0x50494E42u;  // cle NVS dediee

// ===========================================================================
// 2. Generateur pseudo-aleatoire (xorshift32)
// ===========================================================================

static uint32_t s_rng = 0xCAFEBABEu;
static inline uint32_t rnd() {
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}
static inline int rnd_range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(rnd() % (uint32_t)(hi - lo + 1));
}

// ===========================================================================
// 3. Types geometriques — table definie en donnees
// ===========================================================================

// Segment de mur (collision bille vs segment).
struct Seg {
    float x1, y1, x2, y2;
    float rest;  // coefficient de restitution (0 = mou, 1 = elastique)
};

// Bumper circulaire (rebond fort + score + flash).
struct Bumper {
    float cx, cy, r;
    int   score;
    uint32_t color;
    uint32_t flash_until;  // esphome::millis() jusqu'auquel le bumper flash
    lv_obj_t* obj;
};

// Cible drop (disparait au toucher, bank de 3 pour multiball).
struct DropTarget {
    float cx, cy, r;
    bool  down;       // true = tombee
    int   score;
    lv_obj_t* obj;
};

// Slingshot (triangle pres des flippers, impulsion laterale).
struct Sling {
    float x1, y1, x2, y2;  // face active (segment)
    float nx, ny;           // normale de rebond (unitaire)
    int   score;
    uint32_t flash_until;
    lv_obj_t* obj;
};

// Rollover / spinner lane (score au passage).
struct Rollover {
    float cx, cy, r;
    int   score;
    bool  lit;        // allume = bonus
    lv_obj_t* obj;
};

// Flipper (segment pivotant).
struct Flipper {
    float px, py;       // pivot (centre de rotation)
    float angle;        // angle courant (rad)
    float target_angle; // angle cible (repos ou actif)
    float rest_angle;
    float active_angle;
    bool  pressed;
    float len;
    lv_obj_t* obj;
};

// Bille (etat physique).
struct Ball {
    float x, y, vx, vy;
    bool  active;
    bool  in_plunger;   // dans le couloir du plunger
    lv_obj_t* obj;
};

// ===========================================================================
// 4. Donnees de table — segments, bumpers, cibles, slingshots
// ===========================================================================
// [AI-CONTEXT] La table est un paysage 1280x672. Drain en bas (y=672).
// Plunger a droite (couloir vertical x~1220). Flippers en bas-centre.
// Bumpers dans le tiers superieur. Cibles drop au centre. Lanes en haut.

// --- Murs principaux (contour + guides) ---
static const Seg WALLS[] = {
    // Bord gauche
    {10.0f, 0.0f, 10.0f, 672.0f, RESTITUTION},
    // Bord haut
    {10.0f, 0.0f, 1270.0f, 0.0f, RESTITUTION},
    // Bord droit (avant couloir plunger)
    {1200.0f, 0.0f, 1200.0f, 500.0f, RESTITUTION},
    // Couloir plunger : mur gauche du couloir
    {1210.0f, 100.0f, 1210.0f, 672.0f, RESTITUTION},
    // Couloir plunger : mur droit
    {1270.0f, 0.0f, 1270.0f, 672.0f, RESTITUTION},
    // Guide superieur gauche (arc simplifie en segment)
    {10.0f, 0.0f, 120.0f, 60.0f, RESTITUTION},
    // Guide superieur droit
    {1200.0f, 0.0f, 1100.0f, 60.0f, RESTITUTION},
    // Outlane gauche (mur exterieur)
    {10.0f, 400.0f, 10.0f, 672.0f, RESTITUTION},
    // Guide outlane gauche (separateur)
    {100.0f, 420.0f, 80.0f, 580.0f, 0.2f},
    // Outlane droit (avant couloir)
    {1200.0f, 500.0f, 1140.0f, 580.0f, 0.2f},
    // Inlane gauche (guide vers flipper)
    {80.0f, 580.0f, 200.0f, 620.0f, 0.2f},
    // Inlane droit (guide vers flipper)
    {1140.0f, 580.0f, 1020.0f, 620.0f, 0.2f},
    // Mur sous flipper gauche (drain guard)
    {200.0f, 620.0f, 380.0f, 650.0f, 0.15f},
    // Mur sous flipper droit (drain guard)
    {1020.0f, 620.0f, 840.0f, 650.0f, 0.15f},
    // Rampe haute gauche (simplifiee)
    {150.0f, 100.0f, 350.0f, 80.0f, RESTITUTION},
    // Rampe haute droite
    {850.0f, 80.0f, 1050.0f, 100.0f, RESTITUTION},
    // Separateur couloir plunger (haut, guide la bille vers la table)
    {1200.0f, 100.0f, 1150.0f, 60.0f, RESTITUTION},
};
static constexpr int N_WALLS = (int)(sizeof(WALLS) / sizeof(WALLS[0]));

// --- Bumpers (4, dans le tiers superieur) ---
static constexpr int N_BUMPERS = 4;
static Bumper g_bumpers[N_BUMPERS];  // initialise dans build_table()

// --- Slingshots (2, pres des flippers) ---
static constexpr int N_SLINGS = 2;
static Sling g_slings[N_SLINGS];

// --- Cibles drop (bank de 3, centre) ---
static constexpr int N_TARGETS = 3;
static DropTarget g_targets[N_TARGETS];

// --- Rollovers / spinner (2 lanes hautes) ---
static constexpr int N_ROLLOVERS = 2;
static Rollover g_rollovers[N_ROLLOVERS];

// --- Flippers (2) ---
static Flipper g_flip_l, g_flip_r;

// --- Billes (pool) ---
static Ball g_balls[MAX_SIM_BALLS];
static int  g_active_balls = 0;

// ===========================================================================
// 5. Etat runtime
// ===========================================================================

enum State : uint8_t {
    ST_OFF = 0, ST_HUB, ST_PLAYING, ST_BALL_DRAIN, ST_NEXT_BALL,
    ST_PAUSED, ST_TILT, ST_GAMEOVER, ST_HIGHSCORES, ST_SETTINGS
};

static PinballSave g_save{};
static esphome::ESPPreferenceObject g_pref;
static bool  g_pref_ready = false;

static UI    g_ui{};
static bool  g_built = false;
static State g_state = ST_OFF;
static lv_timer_t* g_timer = nullptr;

// --- IMU / inclinaison ---
static float g_raw_x = 0.0f, g_raw_y = 0.0f;
static float g_tilt_x = 0.0f, g_tilt_y = 0.0f;

// --- Partie en cours ---
static uint32_t g_score = 0;
static int      g_ball_num = 0;       // bille courante (1-based)
static int      g_balls_total = MAX_BALLS;
static int      g_multiplier = 1;
static bool     g_bonus_1 = false;    // bille bonus 50k accordee
static bool     g_bonus_2 = false;    // bille bonus 150k accordee

// --- Plunger ---
static bool  g_plunger_held = false;
static float g_plunger_charge = 0.0f;  // 0..PLUNGER_MAX_CHARGE

// --- Tilt ---
static int      g_tilt_hits = 0;
static bool     g_tilted = false;
static uint32_t g_tilt_until = 0;

// --- Multiball ---
static bool     g_mb_ready = false;   // multiball disponible
static bool     g_mb_active = false;
static int      g_targets_down = 0;

// --- Modes score ---
static uint32_t g_frenzy_until = 0;   // Bumper Frenzy actif jusqu'a
static uint32_t g_mania_until = 0;    // Target Mania actif jusqu'a

// --- Feedback visuel ---
static uint32_t g_drain_flash_until = 0;
static char     g_msg_buf[64];        // buffer pour messages ephemeres
static uint32_t g_msg_until = 0;

// --- Objets LVGL (construits une fois) ---
static lv_obj_t* g_hud_score = nullptr;
static lv_obj_t* g_hud_ball  = nullptr;
static lv_obj_t* g_hud_multi = nullptr;
static lv_obj_t* g_hud_high  = nullptr;
static lv_obj_t* g_hud_tilt  = nullptr;
static lv_obj_t* g_p_title   = nullptr;
static lv_obj_t* g_p_sub     = nullptr;
static lv_obj_t* g_p_body    = nullptr;
static lv_obj_t* g_p_foot    = nullptr;
static constexpr int N_SLOTS = 8;
static lv_obj_t* g_slot[N_SLOTS] = {};
static lv_obj_t* g_slot_t[N_SLOTS] = {};
// Zones tactiles flippers / plunger
static lv_obj_t* g_zone_left  = nullptr;
static lv_obj_t* g_zone_right = nullptr;
static lv_obj_t* g_zone_plunger = nullptr;
static lv_obj_t* g_plunger_bar = nullptr;  // jauge de charge visuelle

// Caches HUD : on ne reecrit un libelle que si sa valeur a change.
static uint32_t g_c_score = 0xFFFFFFFF;
static int      g_c_ball = -1;
static int      g_c_multi = -1;
static bool     g_c_tilt = false;

// ===========================================================================
// 6. Persistance NVS
// ===========================================================================

void persist_load() {
    if (!g_pref_ready) {
        g_pref = esphome::global_preferences->make_preference<PinballSave>(PREF_KEY);
        g_pref_ready = true;
    }
    if (!g_pref.load(&g_save) || g_save.magic != SAVE_MAGIC) {
        g_save = PinballSave{};
        g_save.magic = SAVE_MAGIC;
        g_save.sensitivity = 2;
    }
}

void persist_save() {
    if (!g_pref_ready) return;
    g_save.magic = SAVE_MAGIC;
    g_pref.save(&g_save);
    esphome::global_preferences->sync();
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

// Ecrit un libelle seulement si le texte a change (evite des invalidations LVGL).
static void set_text_if(lv_obj_t* l, const char* txt) {
    if (!l) return;
    const char* cur = lv_label_get_text(l);
    if (cur && strcmp(cur, txt) == 0) return;
    lv_label_set_text(l, txt);
}

// ===========================================================================
// 8. Physique — collision bille vs segment / cercle
// ===========================================================================

// [AI-CONTEXT] Schema de collision :
//  - Bille = cercle (x, y, BALL_R)
//  - Murs / guides / slingshots = segments (projection point-segment)
//  - Bumpers / cibles / rollovers = cercles (distance centre-centre)
//  - Flippers = segments mobiles (vitesse angulaire -> impulsion)
//  Chaque sous-pas : deplacement, puis resolution de toutes les collisions.

// Distance point-segment + point le plus proche.
static float point_seg_dist(float px, float py, float x1, float y1, float x2, float y2,
                            float& cx, float& cy) {
    float dx = x2 - x1, dy = y2 - y1;
    float len2 = dx * dx + dy * dy;
    float t = 0.0f;
    if (len2 > 0.0001f) {
        t = ((px - x1) * dx + (py - y1) * dy) / len2;
        t = clampf(t, 0.0f, 1.0f);
    }
    cx = x1 + t * dx;
    cy = y1 + t * dy;
    float ex = px - cx, ey = py - cy;
    return sqrtf(ex * ex + ey * ey);
}

// Collision bille vs segment : repousse et reflete la vitesse.
static bool collide_ball_seg(Ball& b, const Seg& s) {
    float cx, cy;
    float d = point_seg_dist(b.x, b.y, s.x1, s.y1, s.x2, s.y2, cx, cy);
    if (d >= BALL_R || d < 0.001f) return false;

    // Normale de penetration (du segment vers la bille)
    float nx = (b.x - cx) / d;
    float ny = (b.y - cy) / d;

    // Repousser la bille hors du segment
    float pen = BALL_R - d;
    b.x += nx * pen;
    b.y += ny * pen;

    // Refletir la composante normale de la vitesse
    float vn = b.vx * nx + b.vy * ny;
    if (vn < 0.0f) {
        b.vx -= (1.0f + s.rest) * vn * nx;
        b.vy -= (1.0f + s.rest) * vn * ny;
    }
    return true;
}

// Collision bille vs bumper circulaire (rebond fort + score + flash).
static bool collide_ball_bumper(Ball& b, Bumper& bmp) {
    float dx = b.x - bmp.cx, dy = b.y - bmp.cy;
    float dist = sqrtf(dx * dx + dy * dy);
    float min_d = BALL_R + bmp.r;
    if (dist >= min_d || dist < 0.001f) return false;

    float nx = dx / dist, ny = dy / dist;
    // Repousser
    b.x = bmp.cx + nx * min_d;
    b.y = bmp.cy + ny * min_d;
    // Impulsion forte (bumper actif)
    b.vx = nx * BUMPER_FORCE;
    b.vy = ny * BUMPER_FORCE;
    return true;
}

// Collision bille vs slingshot (segment avec normale forcee).
static bool collide_ball_sling(Ball& b, Sling& sl) {
    float cx, cy;
    float d = point_seg_dist(b.x, b.y, sl.x1, sl.y1, sl.x2, sl.y2, cx, cy);
    if (d >= BALL_R + 4.0f || d < 0.001f) return false;

    // Repousser le long de la normale du slingshot
    float pen = (BALL_R + 4.0f) - d;
    b.x += sl.nx * pen;
    b.y += sl.ny * pen;
    b.vx = sl.nx * SLING_FORCE;
    b.vy = sl.ny * SLING_FORCE;
    return true;
}

// Collision bille vs flipper (segment mobile avec vitesse angulaire).
// [AI-CONTEXT] Le flipper est modelise comme un segment pivotant. Quand il est
// en mouvement (angle != target), la vitesse lineaire au point de contact est
// omega x r, ce qui donne une impulsion supplementaire a la bille.
static bool collide_ball_flipper(Ball& b, Flipper& f) {
    // Extremite du flipper
    float ex = f.px + cosf(f.angle) * f.len;
    float ey = f.py + sinf(f.angle) * f.len;

    float cx, cy;
    float d = point_seg_dist(b.x, b.y, f.px, f.py, ex, ey, cx, cy);
    float flip_r = 7.0f;  // epaisseur du flipper
    if (d >= BALL_R + flip_r || d < 0.001f) return false;

    float nx = (b.x - cx) / d;
    float ny = (b.y - cy) / d;

    // Repousser
    float pen = (BALL_R + flip_r) - d;
    b.x += nx * pen;
    b.y += ny * pen;

    // Vitesse lineaire du flipper au point de contact
    float r_contact = sqrtf((cx - f.px) * (cx - f.px) + (cy - f.py) * (cy - f.py));
    float omega = (f.target_angle - f.angle) * FLIP_SPEED;  // approximation
    // Direction tangentielle (perpendiculaire au bras)
    float tx = -(cy - f.py), ty = (cx - f.px);
    float tlen = sqrtf(tx * tx + ty * ty);
    if (tlen > 0.01f) { tx /= tlen; ty /= tlen; }

    float flip_vx = tx * omega * r_contact;
    float flip_vy = ty * omega * r_contact;

    // Impulsion : vitesse relative + reflexion
    float rel_vn = (b.vx - flip_vx) * nx + (b.vy - flip_vy) * ny;
    if (rel_vn < 0.0f) {
        b.vx -= (1.0f + 0.5f) * rel_vn * nx;
        b.vy -= (1.0f + 0.5f) * rel_vn * ny;
    }
    // Ajouter la vitesse du flipper (effet de frappe)
    if (f.pressed) {
        b.vx += flip_vx * FLIP_IMPULSE;
        b.vy += flip_vy * FLIP_IMPULSE;
    }
    return true;
}

// ===========================================================================
// 9. Initialisation de la table (donnees + objets LVGL)
// ===========================================================================

static void build_table() {
    // --- Bumpers : 4, dans le tiers superieur ---
    struct { float x, y, r; uint32_t c; } bd[N_BUMPERS] = {
        {350.0f, 160.0f, 28.0f, UIColor::PIN_RED},
        {600.0f, 120.0f, 32.0f, UIColor::PIN_CYAN},
        {850.0f, 160.0f, 28.0f, UIColor::PIN_ORANGE},
        {600.0f, 260.0f, 26.0f, UIColor::PIN_YELLOW},
    };
    for (int i = 0; i < N_BUMPERS; i++) {
        g_bumpers[i].cx = bd[i].x;
        g_bumpers[i].cy = bd[i].y;
        g_bumpers[i].r  = bd[i].r;
        g_bumpers[i].score = SCORE_BUMPER;
        g_bumpers[i].color = bd[i].c;
        g_bumpers[i].flash_until = 0;
        g_bumpers[i].obj = nullptr;
    }

    // --- Slingshots : 2, de part et d'autre des flippers ---
    // Gauche : face active orientee vers la droite/haut
    g_slings[0].x1 = 250.0f; g_slings[0].y1 = 520.0f;
    g_slings[0].x2 = 350.0f; g_slings[0].y2 = 580.0f;
    g_slings[0].nx = 0.6f;   g_slings[0].ny = -0.8f;
    g_slings[0].score = SCORE_SLING;
    g_slings[0].flash_until = 0;
    g_slings[0].obj = nullptr;
    // Droit : face active orientee vers la gauche/haut
    g_slings[1].x1 = 970.0f; g_slings[1].y1 = 520.0f;
    g_slings[1].x2 = 870.0f; g_slings[1].y2 = 580.0f;
    g_slings[1].nx = -0.6f;  g_slings[1].ny = -0.8f;
    g_slings[1].score = SCORE_SLING;
    g_slings[1].flash_until = 0;
    g_slings[1].obj = nullptr;

    // --- Cibles drop : bank de 3 au centre ---
    struct { float x, y; } td[N_TARGETS] = {
        {520.0f, 360.0f}, {600.0f, 340.0f}, {680.0f, 360.0f}
    };
    for (int i = 0; i < N_TARGETS; i++) {
        g_targets[i].cx = td[i].x;
        g_targets[i].cy = td[i].y;
        g_targets[i].r  = 16.0f;
        g_targets[i].down = false;
        g_targets[i].score = SCORE_TARGET;
        g_targets[i].obj = nullptr;
    }

    // --- Rollovers : 2 lanes hautes ---
    g_rollovers[0].cx = 250.0f; g_rollovers[0].cy = 60.0f;
    g_rollovers[0].r = 18.0f; g_rollovers[0].score = SCORE_LANE;
    g_rollovers[0].lit = true; g_rollovers[0].obj = nullptr;
    g_rollovers[1].cx = 950.0f; g_rollovers[1].cy = 60.0f;
    g_rollovers[1].r = 18.0f; g_rollovers[1].score = SCORE_LANE;
    g_rollovers[1].lit = true; g_rollovers[1].obj = nullptr;

    // --- Flippers ---
    // Flipper gauche : pivot a (430, 600), s'etend vers la droite
    g_flip_l.px = 430.0f; g_flip_l.py = 600.0f;
    g_flip_l.len = FLIP_LEN;
    g_flip_l.rest_angle = FLIP_REST_L;
    g_flip_l.active_angle = FLIP_ACTIVE_L;
    g_flip_l.angle = FLIP_REST_L;
    g_flip_l.target_angle = FLIP_REST_L;
    g_flip_l.pressed = false;
    g_flip_l.obj = nullptr;
    // Flipper droit : pivot a (790, 600), s'etend vers la gauche
    g_flip_r.px = 790.0f; g_flip_r.py = 600.0f;
    g_flip_r.len = FLIP_LEN;
    g_flip_r.rest_angle = FLIP_REST_R;
    g_flip_r.active_angle = FLIP_ACTIVE_R;
    g_flip_r.angle = FLIP_REST_R;
    g_flip_r.target_angle = FLIP_REST_R;
    g_flip_r.pressed = false;
    g_flip_r.obj = nullptr;

    // --- Billes : init vide ---
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        g_balls[i].active = false;
        g_balls[i].in_plunger = false;
        g_balls[i].obj = nullptr;
    }
}

// ===========================================================================
// 10. Construction de l'UI LVGL (une seule fois)
// ===========================================================================

static void zone_event_cb(lv_event_t* e);
static void hud_event_cb(lv_event_t* e);
static void slot_event_cb(lv_event_t* e);

static void build_ui() {
    if (g_built) return;

    lv_obj_t* field = g_ui.field;
    lv_obj_t* hud   = g_ui.hud;
    lv_obj_t* panel = g_ui.panel;

    // --- HUD : Score | Bille x/3 | Multi | High | TILT ---
    g_hud_score = mk_label(hud, g_ui.f_mid, UIColor::PIN_YELLOW);
    lv_obj_align(g_hud_score, LV_ALIGN_LEFT_MID, 16, 0);

    g_hud_ball = mk_label(hud, g_ui.f_small, UIColor::PIN_WHITE);
    lv_obj_align(g_hud_ball, LV_ALIGN_LEFT_MID, 280, 0);

    g_hud_multi = mk_label(hud, g_ui.f_small, UIColor::PIN_CYAN);
    lv_obj_align(g_hud_multi, LV_ALIGN_CENTER, 0, 0);

    g_hud_high = mk_label(hud, g_ui.f_small, UIColor::PIN_ORANGE);
    lv_obj_align(g_hud_high, LV_ALIGN_RIGHT_MID, -120, 0);

    g_hud_tilt = mk_label(hud, g_ui.f_small, UIColor::PIN_RED);
    lv_obj_align(g_hud_tilt, LV_ALIGN_RIGHT_MID, -16, 0);
    lv_label_set_text(g_hud_tilt, "TILT");
    show(g_hud_tilt, false);

    // --- Murs : rendus comme des rectangles fins ---
    for (int i = 0; i < N_WALLS; i++) {
        const Seg& s = WALLS[i];
        lv_obj_t* w = mk_rect(field);
        float dx = s.x2 - s.x1, dy = s.y2 - s.y1;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 1.0f) continue;
        // Epaisseur visuelle du mur
        float thick = 6.0f;
        // Position = centre du segment
        float cx = (s.x1 + s.x2) * 0.5f;
        float cy = (s.y1 + s.y2) * 0.5f;
        lv_obj_set_size(w, (int)len, (int)thick);
        lv_obj_set_pos(w, (int)(cx - len * 0.5f), (int)(cy - thick * 0.5f));
        // Rotation pour les segments non horizontaux
        float ang = atan2f(dy, dx) * 180.0f / 3.14159265f;
        if (ang != 0.0f) {
            lv_obj_set_style_transform_rotation(w, (int)(ang * 10), LV_PART_MAIN);
            lv_obj_set_style_transform_pivot_x(w, (int)(len * 0.5f), LV_PART_MAIN);
            lv_obj_set_style_transform_pivot_y(w, (int)(thick * 0.5f), LV_PART_MAIN);
        }
        set_bg(w, UIColor::PIN_WALL, LV_OPA_COVER);
    }

    // --- Bumpers : cercles colores ---
    for (int i = 0; i < N_BUMPERS; i++) {
        Bumper& bmp = g_bumpers[i];
        lv_obj_t* o = mk_rect(field);
        int d = (int)(bmp.r * 2);
        lv_obj_set_size(o, d, d);
        lv_obj_set_pos(o, (int)(bmp.cx - bmp.r), (int)(bmp.cy - bmp.r));
        lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(o, bmp.color, LV_OPA_COVER);
        set_border(o, UIColor::PIN_WHITE, 2, LV_OPA_50);
        bmp.obj = o;
    }

    // --- Slingshots : triangles simplifies en rectangles angules ---
    for (int i = 0; i < N_SLINGS; i++) {
        Sling& sl = g_slings[i];
        lv_obj_t* o = mk_rect(field);
        float dx = sl.x2 - sl.x1, dy = sl.y2 - sl.y1;
        float len = sqrtf(dx * dx + dy * dy);
        lv_obj_set_size(o, (int)len, 10);
        lv_obj_set_pos(o, (int)sl.x1, (int)(sl.y1 - 5));
        float ang = atan2f(dy, dx) * 180.0f / 3.14159265f;
        lv_obj_set_style_transform_rotation(o, (int)(ang * 10), LV_PART_MAIN);
        set_bg(o, UIColor::PIN_ORANGE, LV_OPA_COVER);
        sl.obj = o;
    }

    // --- Cibles drop : petits rectangles ---
    for (int i = 0; i < N_TARGETS; i++) {
        DropTarget& t = g_targets[i];
        lv_obj_t* o = mk_rect(field);
        lv_obj_set_size(o, 28, 12);
        lv_obj_set_pos(o, (int)(t.cx - 14), (int)(t.cy - 6));
        set_bg(o, UIColor::PIN_YELLOW, LV_OPA_COVER);
        set_border(o, UIColor::PIN_WHITE, 1, LV_OPA_70);
        t.obj = o;
    }

    // --- Rollovers : cercles ---
    for (int i = 0; i < N_ROLLOVERS; i++) {
        Rollover& r = g_rollovers[i];
        lv_obj_t* o = mk_rect(field);
        int d = (int)(r.r * 2);
        lv_obj_set_size(o, d, d);
        lv_obj_set_pos(o, (int)(r.cx - r.r), (int)(r.cy - r.r));
        lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(o, UIColor::PIN_CYAN, LV_OPA_50);
        set_border(o, UIColor::PIN_CYAN, 2, LV_OPA_COVER);
        r.obj = o;
    }

    // --- Flippers : rectangles pivotants ---
    auto mk_flip = [&](Flipper& f) {
        lv_obj_t* o = mk_rect(field);
        lv_obj_set_size(o, (int)f.len, 14);
        lv_obj_set_pos(o, (int)f.px, (int)(f.py - 7));
        lv_obj_set_style_radius(o, 7, LV_PART_MAIN);
        set_bg(o, UIColor::PIN_WHITE, LV_OPA_COVER);
        f.obj = o;
    };
    mk_flip(g_flip_l);
    mk_flip(g_flip_r);

    // --- Billes : cercles blancs ---
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        lv_obj_t* o = mk_rect(field);
        int d = (int)(BALL_R * 2);
        lv_obj_set_size(o, d, d);
        lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(o, UIColor::PIN_BALL, LV_OPA_COVER);
        set_border(o, 0xFFFFFF, 1, LV_OPA_30);
        show(o, false);
        g_balls[i].obj = o;
    }

    // --- Jauge plunger (barre verticale a droite) ---
    g_plunger_bar = mk_rect(field);
    lv_obj_set_size(g_plunger_bar, 12, 100);
    lv_obj_set_pos(g_plunger_bar, 1235, 550);
    set_bg(g_plunger_bar, UIColor::PIN_RED, LV_OPA_COVER);
    show(g_plunger_bar, false);

    // --- Zones tactiles : flippers gauche/droit + plunger ---
    // Zone gauche (moitie gauche du field, en bas)
    g_zone_left = lv_obj_create(field);
    lv_obj_remove_style_all(g_zone_left);
    lv_obj_set_size(g_zone_left, 400, 300);
    lv_obj_set_pos(g_zone_left, 0, 372);
    lv_obj_set_style_bg_opa(g_zone_left, LV_OPA_0, LV_PART_MAIN);
    lv_obj_add_flag(g_zone_left, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_zone_left, zone_event_cb, LV_EVENT_ALL, (void*)0);

    // Zone droite (moitie droite, en bas)
    g_zone_right = lv_obj_create(field);
    lv_obj_remove_style_all(g_zone_right);
    lv_obj_set_size(g_zone_right, 400, 300);
    lv_obj_set_pos(g_zone_right, 800, 372);
    lv_obj_set_style_bg_opa(g_zone_right, LV_OPA_0, LV_PART_MAIN);
    lv_obj_add_flag(g_zone_right, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_zone_right, zone_event_cb, LV_EVENT_ALL, (void*)1);

    // Zone plunger (coin bas-droit)
    g_zone_plunger = lv_obj_create(field);
    lv_obj_remove_style_all(g_zone_plunger);
    lv_obj_set_size(g_zone_plunger, 120, 300);
    lv_obj_set_pos(g_zone_plunger, 1160, 372);
    lv_obj_set_style_bg_opa(g_zone_plunger, LV_OPA_0, LV_PART_MAIN);
    lv_obj_add_flag(g_zone_plunger, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_zone_plunger, zone_event_cb, LV_EVENT_ALL, (void*)2);

    // --- Panel (menus) : titre, sous-titre, corps, pied ---
    g_p_title = mk_label(panel, g_ui.f_big, UIColor::PIN_YELLOW);
    lv_obj_align(g_p_title, LV_ALIGN_TOP_MID, 0, 60);

    g_p_sub = mk_label(panel, g_ui.f_mid, UIColor::PIN_WHITE);
    lv_obj_align(g_p_sub, LV_ALIGN_TOP_MID, 0, 120);

    g_p_body = mk_label(panel, g_ui.f_small, UIColor::PIN_WHITE);
    lv_obj_align(g_p_body, LV_ALIGN_CENTER, 0, 0);

    g_p_foot = mk_label(panel, g_ui.f_small, UIColor::PIN_ORANGE);
    lv_obj_align(g_p_foot, LV_ALIGN_BOTTOM_MID, 0, -30);

    // Slots de menu (8 boutons)
    for (int i = 0; i < N_SLOTS; i++) {
        lv_obj_t* s = lv_obj_create(panel);
        lv_obj_remove_style_all(s);
        lv_obj_set_size(s, 320, 52);
        lv_obj_set_pos(s, 480, 180 + i * 62);
        lv_obj_set_style_radius(s, 8, LV_PART_MAIN);
        set_bg(s, UIColor::PIN_BTN, LV_OPA_COVER);
        set_border(s, UIColor::PIN_CYAN, 1, LV_OPA_40);
        lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(s, slot_event_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        g_slot[i] = s;

        lv_obj_t* t = mk_label(s, g_ui.f_small, UIColor::PIN_WHITE);
        lv_obj_center(t);
        g_slot_t[i] = t;
    }

    g_built = true;
}

// ===========================================================================
// 11. Logique de jeu — score, drain, multiball, tilt, modes
// ===========================================================================

static void add_score(int pts) {
    uint32_t now = esphome::millis();
    int mul = g_multiplier;
    if (now < g_frenzy_until) mul *= 2;
    if (now < g_mania_until)  mul *= 2;
    g_score += (uint32_t)(pts * mul);
    if (!g_bonus_1 && g_score >= BONUS_BALL_1) {
        g_bonus_1 = true; g_balls_total++;
        snprintf(g_msg_buf, sizeof(g_msg_buf), "BILLE BONUS !");
        g_msg_until = now + 2000;
    }
    if (!g_bonus_2 && g_score >= BONUS_BALL_2) {
        g_bonus_2 = true; g_balls_total++;
        snprintf(g_msg_buf, sizeof(g_msg_buf), "2e BILLE BONUS !");
        g_msg_until = now + 2000;
    }
}

static void reset_targets() {
    g_targets_down = 0;
    for (int i = 0; i < N_TARGETS; i++) {
        g_targets[i].down = false;
        if (g_targets[i].obj) show(g_targets[i].obj, true);
    }
}

static void launch_ball_in_plunger() {
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        if (!g_balls[i].active) {
            g_balls[i].active = true;
            g_balls[i].in_plunger = true;
            g_balls[i].x = 1240.0f; g_balls[i].y = 620.0f;
            g_balls[i].vx = 0.0f; g_balls[i].vy = 0.0f;
            if (g_balls[i].obj) show(g_balls[i].obj, true);
            g_active_balls++;
            return;
        }
    }
}

static void drain_ball(int idx) {
    g_balls[idx].active = false;
    g_balls[idx].in_plunger = false;
    if (g_balls[idx].obj) show(g_balls[idx].obj, false);
    g_active_balls--;
    g_drain_flash_until = esphome::millis() + 400;
    if (g_active_balls <= 0) {
        g_active_balls = 0; g_mb_active = false;
        g_tilt_hits = 0; g_tilted = false;
        uint8_t balls_played = (uint8_t)g_ball_num;  // avant incrément
        g_ball_num++;
        if (g_ball_num > g_balls_total) {
            g_state = ST_GAMEOVER;
            g_save.career_games++;
            if (g_score > 0 && (g_save.score_count < PIN_MAX_SCORES ||
                g_score > g_save.scores[g_save.score_count - 1].score)) {
                int pos = g_save.score_count < PIN_MAX_SCORES ?
                          g_save.score_count : PIN_MAX_SCORES - 1;
                g_save.scores[pos].score = g_score;
                g_save.scores[pos].ctrl_mode = g_save.ctrl_mode;
                g_save.scores[pos].balls = balls_played;
                g_save.scores[pos].timestamp = esphome::millis() / 1000;
                if (g_save.score_count < PIN_MAX_SCORES) g_save.score_count++;
                for (int i = 0; i < g_save.score_count - 1; i++)
                    for (int j = i + 1; j < g_save.score_count; j++)
                        if (g_save.scores[j].score > g_save.scores[i].score) {
                            PinScoreEntry tmp = g_save.scores[i];
                            g_save.scores[i] = g_save.scores[j];
                            g_save.scores[j] = tmp;
                        }
            }
            persist_save();
        } else {
            g_state = ST_NEXT_BALL;
        }
    }
}

static void start_multiball() {
    if (g_mb_active) return;
    g_mb_active = true;
    g_save.career_multiballs++;
    snprintf(g_msg_buf, sizeof(g_msg_buf), "MULTIBALL !");
    g_msg_until = esphome::millis() + 2500;
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        if (!g_balls[i].active) {
            g_balls[i].active = true;
            g_balls[i].in_plunger = false;
            g_balls[i].x = 1100.0f; g_balls[i].y = 200.0f;
            g_balls[i].vx = -200.0f; g_balls[i].vy = -100.0f;
            if (g_balls[i].obj) show(g_balls[i].obj, true);
            g_active_balls++;
            break;
        }
    }
}

static void trigger_tilt() {
    if (g_tilted) return;
    g_tilted = true;
    g_tilt_until = esphome::millis() + TILT_PENALTY_MS;
    g_save.career_tilts++;
    snprintf(g_msg_buf, sizeof(g_msg_buf), "TILT !");
    g_msg_until = g_tilt_until;
}

static void start_game() {
    g_score = 0; g_ball_num = 1; g_balls_total = MAX_BALLS;
    g_multiplier = 1; g_bonus_1 = false; g_bonus_2 = false;
    g_tilt_hits = 0; g_tilted = false;
    g_mb_ready = false; g_mb_active = false;
    g_frenzy_until = 0; g_mania_until = 0;
    g_active_balls = 0; g_plunger_charge = 0.0f; g_plunger_held = false;
    reset_targets();
    for (int i = 0; i < N_BUMPERS; i++) g_bumpers[i].flash_until = 0;
    for (int i = 0; i < N_SLINGS; i++) g_slings[i].flash_until = 0;
    for (int i = 0; i < N_ROLLOVERS; i++) {
        g_rollovers[i].lit = true;
        if (g_rollovers[i].obj) set_bg(g_rollovers[i].obj, UIColor::PIN_CYAN, LV_OPA_50);
    }
    launch_ball_in_plunger();
    g_state = ST_PLAYING;
    show(g_ui.panel, false);
}

// ===========================================================================
// 12. Callbacks evenements
// ===========================================================================

// Declarations forward des menus (definies en section 13).
static void go_hub();
static void go_settings();
static void go_highscores();

static void zone_event_cb(lv_event_t* e) {
    if (g_state != ST_PLAYING) return;
    int zone = (int)(intptr_t)lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (zone == 0) {
        if (code == LV_EVENT_PRESSED) g_flip_l.pressed = true;
        if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) g_flip_l.pressed = false;
    } else if (zone == 1) {
        if (code == LV_EVENT_PRESSED) g_flip_r.pressed = true;
        if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) g_flip_r.pressed = false;
    } else if (zone == 2) {
        if (code == LV_EVENT_PRESSED) { g_plunger_held = true; g_plunger_charge = 0.0f; }
        if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
            if (g_plunger_held) {
                g_plunger_held = false;
                for (int i = 0; i < MAX_SIM_BALLS; i++) {
                    if (g_balls[i].active && g_balls[i].in_plunger) {
                        float ratio = clampf(g_plunger_charge / PLUNGER_MAX_CHARGE, 0.0f, 1.0f);
                        float force = PLUNGER_MIN_FORCE + ratio * (PLUNGER_MAX_FORCE - PLUNGER_MIN_FORCE);
                        g_balls[i].in_plunger = false;
                        g_balls[i].vy = -force;
                        g_balls[i].vx = -30.0f;
                    }
                }
                g_plunger_charge = 0.0f;
            }
        }
    }
}

static void hud_event_cb(lv_event_t* e) {
    (void)e;
    if (g_state == ST_PLAYING) g_state = ST_PAUSED;
    else if (g_state == ST_PAUSED) { g_state = ST_PLAYING; show(g_ui.panel, false); }
}

static void slot_event_cb(lv_event_t* e) {
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    // Routage par index de slot (pas par texte du label, fragile)
    if (g_state == ST_SETTINGS) {
        switch (idx) {
            case 0: g_save.ctrl_mode = (g_save.ctrl_mode == 0) ? 1 : 0; persist_save(); go_settings(); return;
            case 1: if (g_save.sensitivity < 4) g_save.sensitivity++; persist_save(); go_settings(); return;
            case 2: if (g_save.sensitivity > 0) g_save.sensitivity--; persist_save(); go_settings(); return;
            case 3: calibrate(); return;
            case 4: g_save.muted = !g_save.muted; persist_save(); go_settings(); return;
            case 5: g_state = ST_HUB; return;
        }
        return;
    }
    if (g_state == ST_PAUSED) {
        switch (idx) {
            case 0: g_state = ST_PLAYING; show(g_ui.panel, false); return;  // Reprendre
            case 1: start_game(); return;  // Nouvelle partie
            case 2: calibrate(); return;  // Calibrer IMU
            case 3: close(); return;  // Quitter
        }
        return;
    }
    if (g_state == ST_GAMEOVER) {
        switch (idx) {
            case 0: start_game(); return;  // Nouvelle partie
            case 1: g_state = ST_HIGHSCORES; return;  // Classement
            case 2: g_state = ST_HUB; return;  // Retour
        }
        return;
    }
    if (g_state == ST_HIGHSCORES) {
        // Indices dynamiques : scores puis "Effacer" (next) et "Retour" (next+1)
        int next = (g_save.score_count < N_SLOTS - 2) ? g_save.score_count : N_SLOTS - 2;
        if (idx == next) {
            g_save.score_count = 0;
            memset(g_save.scores, 0, sizeof(g_save.scores));
            persist_save(); g_state = ST_HUB;
        } else if (idx == next + 1) {
            g_state = ST_HUB;
        }
        return;
    }
    // ST_HUB
    switch (idx) {
        case 0: start_game(); break;  // Jouer
        case 1: g_state = ST_HIGHSCORES; break;  // Classement
        case 2: g_state = ST_SETTINGS; break;  // Reglages
        case 3: close(); break;  // Quitter
    }
}

// ===========================================================================
// 13. Menus
// ===========================================================================

static void set_slot(int i, const char* txt) {
    if (i < 0 || i >= N_SLOTS) return;
    if (txt) { show(g_slot[i], true); lv_label_set_text(g_slot_t[i], txt); }
    else show(g_slot[i], false);
}
static void clear_slots() { for (int i = 0; i < N_SLOTS; i++) show(g_slot[i], false); }

static void go_hub() {
    g_state = ST_HUB;
    show(g_ui.panel, true); clear_slots();
    set_text_if(g_p_title, "FLIP NOIR");
    set_text_if(g_p_sub, "Flipper retro — M5Stack Tab5");
    set_text_if(g_p_body, "");
    set_text_if(g_p_foot, "Hold bords = flippers | Coin droit = plunger");
    set_slot(0, "Jouer"); set_slot(1, "Classement");
    set_slot(2, "Reglages"); set_slot(3, "Quitter");
}

static void go_pause() {
    show(g_ui.panel, true); clear_slots();
    set_text_if(g_p_title, "PAUSE");
    set_text_if(g_p_sub, "");
    snprintf(g_msg_buf, sizeof(g_msg_buf), "Score : %lu", (unsigned long)g_score);
    set_text_if(g_p_body, g_msg_buf);
    set_text_if(g_p_foot, "");
    set_slot(0, "Reprendre"); set_slot(1, "Nouvelle partie");
    set_slot(2, "Calibrer IMU"); set_slot(3, "Quitter");
}

static void go_gameover() {
    show(g_ui.panel, true); clear_slots();
    set_text_if(g_p_title, "GAME OVER");
    snprintf(g_msg_buf, sizeof(g_msg_buf), "Score final : %lu", (unsigned long)g_score);
    set_text_if(g_p_sub, g_msg_buf);
    uint32_t best = g_save.score_count > 0 ? g_save.scores[0].score : 0;
    snprintf(g_msg_buf, sizeof(g_msg_buf), "Meilleur : %lu", (unsigned long)best);
    set_text_if(g_p_body, g_msg_buf);
    set_text_if(g_p_foot, "");
    set_slot(0, "Nouvelle partie"); set_slot(1, "Classement"); set_slot(2, "Retour");
}

static void go_highscores() {
    show(g_ui.panel, true); clear_slots();
    set_text_if(g_p_title, "CLASSEMENT");
    set_text_if(g_p_sub, "Top 10 local");
    set_text_if(g_p_body, ""); set_text_if(g_p_foot, "");
    for (int i = 0; i < N_SLOTS - 2 && i < g_save.score_count; i++) {
        snprintf(g_msg_buf, sizeof(g_msg_buf), "%d. %lu", i + 1,
                 (unsigned long)g_save.scores[i].score);
        set_slot(i, g_msg_buf);
    }
    int next = (g_save.score_count < N_SLOTS - 2) ? g_save.score_count : N_SLOTS - 2;
    set_slot(next, "Effacer scores");
    set_slot(next + 1, "Retour");
}

static void go_settings() {
    show(g_ui.panel, true); clear_slots();
    set_text_if(g_p_title, "REGLAGES");
    const char* ctrl = (g_save.ctrl_mode == 0) ? "Boutons" : "Mixte (IMU)";
    snprintf(g_msg_buf, sizeof(g_msg_buf), "Controle : %s | Sens : %d", ctrl, g_save.sensitivity);
    set_text_if(g_p_sub, g_msg_buf);
    set_text_if(g_p_body, ""); set_text_if(g_p_foot, "");
    set_slot(0, (g_save.ctrl_mode == 0) ? "Mode : Mixte" : "Mode : Boutons");
    set_slot(1, "Sensibilite +"); set_slot(2, "Sensibilite -");
    set_slot(3, "Calibrer IMU");
    set_slot(4, (g_save.muted) ? "SFX : Activer" : "SFX : Couper");
    set_slot(5, "Retour");
}

// ===========================================================================
// 14. Tick — boucle de jeu (~45 Hz via lv_timer)
// ===========================================================================

static void tick_cb(lv_timer_t* t) {
    (void)t;
    if (g_state == ST_OFF) return;
    uint32_t now = esphome::millis();

    // Garde-fou transitions : reconstruit l'UI seulement au changement d'état
    static int prev_menu = -1;
    int cur = (int)g_state;
    if (g_state == ST_PAUSED)       { if (prev_menu != cur) { go_pause();     prev_menu = cur; } return; }
    if (g_state == ST_GAMEOVER)     { if (prev_menu != cur) { go_gameover();  prev_menu = cur; } return; }
    if (g_state == ST_HIGHSCORES)   { if (prev_menu != cur) { go_highscores(); prev_menu = cur; } return; }
    if (g_state == ST_SETTINGS)     { if (prev_menu != cur) { go_settings();  prev_menu = cur; } return; }
    if (g_state == ST_HUB)          { prev_menu = cur; return; }
    if (g_state == ST_NEXT_BALL) {
        // Pas de reset_targets : la progression multiball reste entre balles
        g_tilted = false; g_tilt_hits = 0;
        launch_ball_in_plunger();
        g_state = ST_PLAYING;
        prev_menu = -1;  // force rebuild si retour menu
        return;
    }
    if (g_state != ST_PLAYING) return;
    prev_menu = -1;  // reset après retour en jeu

    // --- Charge plunger ---
    if (g_plunger_held) {
        g_plunger_charge += DT;
        if (g_plunger_charge > PLUNGER_MAX_CHARGE) g_plunger_charge = PLUNGER_MAX_CHARGE;
    }

    // --- Tilt IMU (nudge) ---
    if (g_save.ctrl_mode == 1 && !g_tilted) {
        float cal_x = g_save.cal_x / 1000.0f;
        float cal_y = g_save.cal_y / 1000.0f;
        g_tilt_x += ((g_raw_x - cal_x) - g_tilt_x) * TILT_SMOOTH;
        g_tilt_y += ((g_raw_y - cal_y) - g_tilt_y) * TILT_SMOOTH;
        float sens = 0.5f + g_save.sensitivity * 0.25f;
        float eff_x = (fabsf(g_tilt_x) > TILT_DEADZONE) ? g_tilt_x * sens : 0.0f;
        float eff_y = (fabsf(g_tilt_y) > TILT_DEADZONE) ? g_tilt_y * sens : 0.0f;
        if (fabsf(eff_x) > 0.4f || fabsf(eff_y) > 0.4f) {
            static uint32_t last_nudge = 0;
            if (now - last_nudge > 300) {
                last_nudge = now;
                g_tilt_hits++;
                if (g_tilt_hits >= TILT_MAX_HITS) trigger_tilt();
            }
        }
    } else { g_tilt_x = 0.0f; g_tilt_y = 0.0f; }
    if (g_tilted && now >= g_tilt_until) { g_tilted = false; g_tilt_hits = 0; }

    // --- Flippers ---
    auto update_flip = [&](Flipper& f) {
        f.target_angle = (f.pressed && !g_tilted) ? f.active_angle : f.rest_angle;
        float diff = f.target_angle - f.angle;
        float step = FLIP_SPEED * DT;
        if (fabsf(diff) <= step) f.angle = f.target_angle;
        else f.angle += (diff > 0 ? step : -step);
    };
    update_flip(g_flip_l);
    update_flip(g_flip_r);

    // --- Physique billes (sous-pas anti-tunnelling) ---
    for (int bi = 0; bi < MAX_SIM_BALLS; bi++) {
        Ball& b = g_balls[bi];
        if (!b.active || b.in_plunger) continue;
        for (int sub = 0; sub < SUBSTEP; sub++) {
            b.vy += GRAVITY * SDT;
            if (g_save.ctrl_mode == 1 && !g_tilted) {
                b.vx += g_tilt_x * TILT_FORCE * SDT;
                b.vy += g_tilt_y * TILT_FORCE * 0.3f * SDT;
            }
            b.vx *= FRICTION; b.vy *= FRICTION;
            float spd = sqrtf(b.vx * b.vx + b.vy * b.vy);
            if (spd > MAX_SPEED) { b.vx *= MAX_SPEED / spd; b.vy *= MAX_SPEED / spd; }
            b.x += b.vx * SDT;
            b.y += b.vy * SDT;

            for (int wi = 0; wi < N_WALLS; wi++) collide_ball_seg(b, WALLS[wi]);
            for (int i = 0; i < N_BUMPERS; i++) {
                if (collide_ball_bumper(b, g_bumpers[i])) {
                    g_bumpers[i].flash_until = now + 80;
                    add_score(g_bumpers[i].score);
                    if (g_frenzy_until == 0 && rnd_range(1, 20) == 1) {
                        g_frenzy_until = now + MODE_FRENZY_MS;
                        snprintf(g_msg_buf, sizeof(g_msg_buf), "BUMPER FRENZY x2 !");
                        g_msg_until = now + 2000;
                    }
                }
            }
            for (int i = 0; i < N_SLINGS; i++) {
                if (collide_ball_sling(b, g_slings[i])) {
                    g_slings[i].flash_until = now + 80;
                    add_score(g_slings[i].score);
                }
            }
            for (int i = 0; i < N_TARGETS; i++) {
                if (g_targets[i].down) continue;
                float dx = b.x - g_targets[i].cx, dy = b.y - g_targets[i].cy;
                float dist = sqrtf(dx * dx + dy * dy);
                if (dist < BALL_R + g_targets[i].r) {
                    g_targets[i].down = true;
                    if (g_targets[i].obj) show(g_targets[i].obj, false);
                    add_score(g_targets[i].score);
                    g_targets_down++;
                    if (dist > 0.01f) { b.vx = (dx/dist)*200.0f; b.vy = (dy/dist)*200.0f; }
                    if (g_targets_down >= MB_TARGETS_NEEDED && !g_mb_active) {
                        start_multiball(); reset_targets();
                    }
                    if (g_mania_until == 0 && rnd_range(1, 10) == 1) {
                        g_mania_until = now + MODE_MANIA_MS;
                        snprintf(g_msg_buf, sizeof(g_msg_buf), "TARGET MANIA x2 !");
                        g_msg_until = now + 2000;
                    }
                }
            }
            for (int i = 0; i < N_ROLLOVERS; i++) {
                if (!g_rollovers[i].lit) continue;
                float dx = b.x - g_rollovers[i].cx, dy = b.y - g_rollovers[i].cy;
                if (sqrtf(dx*dx+dy*dy) < BALL_R + g_rollovers[i].r) {
                    g_rollovers[i].lit = false;
                    if (g_rollovers[i].obj) set_bg(g_rollovers[i].obj, UIColor::PIN_CYAN, LV_OPA_20);
                    add_score(g_rollovers[i].score);
                }
            }
            collide_ball_flipper(b, g_flip_l);
            collide_ball_flipper(b, g_flip_r);
            if (b.y > FH + BALL_R * 2) { drain_ball(bi); break; }
            if (b.x < BALL_R + 10.0f) { b.x = BALL_R + 10.0f; b.vx = fabsf(b.vx)*0.5f; }
            if (b.x > 1270.0f - BALL_R) { b.x = 1270.0f - BALL_R; b.vx = -fabsf(b.vx)*0.5f; }
            if (b.y < BALL_R) { b.y = BALL_R; b.vy = fabsf(b.vy)*0.5f; }
        }
    }

    // --- Rendu LVGL ---
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        if (g_balls[i].obj && g_balls[i].active)
            lv_obj_set_pos(g_balls[i].obj, (int)(g_balls[i].x - BALL_R), (int)(g_balls[i].y - BALL_R));
    }
    auto render_flip = [&](Flipper& f) {
        if (!f.obj) return;
        int ang = (int)(f.angle * 180.0f / 3.14159265f * 10.0f);
        lv_obj_set_pos(f.obj, (int)f.px, (int)(f.py - 7));
        lv_obj_set_style_transform_rotation(f.obj, ang, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_x(f.obj, 0, LV_PART_MAIN);
        lv_obj_set_style_transform_pivot_y(f.obj, 7, LV_PART_MAIN);
    };
    render_flip(g_flip_l);
    render_flip(g_flip_r);
    for (int i = 0; i < N_BUMPERS; i++) {
        if (!g_bumpers[i].obj) continue;
        set_bg(g_bumpers[i].obj, (now < g_bumpers[i].flash_until) ?
               UIColor::PIN_WHITE : g_bumpers[i].color, LV_OPA_COVER);
    }
    for (int i = 0; i < N_SLINGS; i++) {
        if (!g_slings[i].obj) continue;
        set_bg(g_slings[i].obj, (now < g_slings[i].flash_until) ?
               UIColor::PIN_WHITE : UIColor::PIN_ORANGE, LV_OPA_COVER);
    }
    if (g_plunger_held) {
        show(g_plunger_bar, true);
        int h = (int)(100.0f * g_plunger_charge / PLUNGER_MAX_CHARGE);
        lv_obj_set_size(g_plunger_bar, 12, h > 4 ? h : 4);
        lv_obj_set_pos(g_plunger_bar, 1235, 650 - h);
    } else show(g_plunger_bar, false);

    // --- HUD (on-change only) ---
    if (g_score != g_c_score) {
        g_c_score = g_score;
        snprintf(g_msg_buf, sizeof(g_msg_buf), "%lu", (unsigned long)g_score);
        set_text_if(g_hud_score, g_msg_buf);
    }
    if (g_ball_num != g_c_ball) {
        g_c_ball = g_ball_num;
        snprintf(g_msg_buf, sizeof(g_msg_buf), "Bille %d/%d", g_ball_num, g_balls_total);
        set_text_if(g_hud_ball, g_msg_buf);
    }
    int cur_multi = g_multiplier;
    if (now < g_frenzy_until || now < g_mania_until) cur_multi *= 2;
    if (cur_multi != g_c_multi) {
        g_c_multi = cur_multi;
        snprintf(g_msg_buf, sizeof(g_msg_buf), "x%d", cur_multi);
        set_text_if(g_hud_multi, g_msg_buf);
    }
    if (g_tilted != g_c_tilt) { g_c_tilt = g_tilted; show(g_hud_tilt, g_tilted); }
    static uint32_t last_high = 0xFFFFFFFF;
    uint32_t high = g_save.score_count > 0 ? g_save.scores[0].score : 0;
    if (high != last_high) {
        last_high = high;
        snprintf(g_msg_buf, sizeof(g_msg_buf), "HI:%lu", (unsigned long)high);
        set_text_if(g_hud_high, g_msg_buf);
    }
}

// ===========================================================================
// 15. API publique
// ===========================================================================

void on_imu(float ax, float ay, float az) {
    (void)az;
    // [AI-CONTEXT] Mapping rotation 270 deg : X table = -tilt_Y, Y table = +tilt_X
    g_raw_x = -ay;
    g_raw_y = ax;
}

void calibrate() {
    g_save.cal_x = (int16_t)(g_raw_x * 1000.0f);
    g_save.cal_y = (int16_t)(g_raw_y * 1000.0f);
    g_tilt_x = 0; g_tilt_y = 0;
    persist_save();
}

bool is_open() { return g_state != ST_OFF; }

void open(const UI& ui) {
    if (g_state != ST_OFF) return;
    if (!ui.root || !ui.field || !ui.hud || !ui.panel) return;
    g_ui = ui;
    persist_load();
    build_table();
    build_ui();
    show(g_ui.root, true);
    lv_obj_move_foreground(g_ui.root);
    go_hub();
    lv_obj_add_flag(g_ui.hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.hud, hud_event_cb, LV_EVENT_CLICKED, nullptr);
    if (!g_timer) g_timer = lv_timer_create(tick_cb, 22, nullptr);
}

void close() {
    if (g_state == ST_OFF) return;
    persist_save();
    if (g_timer) { lv_timer_delete(g_timer); g_timer = nullptr; }
    if (g_ui.hud) lv_obj_remove_event_cb(g_ui.hud, hud_event_cb);
    show(g_ui.root, false);
    g_state = ST_OFF;
}

}  // namespace Pinball
