/**
 * [AI-CONTEXT]
 * @file pinball_game.cpp
 * @role Jeu « Neon Apron » — flipper portrait. Table unique, soignee, 100 % local.
 *
 * ===========================================================================
 * [AI-CONTEXT] ORIENTATION — a lire avant de toucher quoi que ce soit
 * ===========================================================================
 * Le dashboard vit en PAYSAGE 1280x720 (`lvgl: rotation: 270` dans
 * tab5-styles.yaml). Un flipper couche sur le cote ne ressemble a rien : ce jeu
 * bascule donc l'ecran en PORTRAIT 720x1280 le temps d'une partie.
 *
 * COMMENT : `UI::lvgl->set_rotation(0)` a l'ouverture, `set_rotation(270)` a la
 * fermeture. C'est une API publique d'ESPHome 2026.7.x
 * (LvglComponent::set_rotation, lvgl_esphome.cpp) qui recalcule la resolution
 * du display LVGL et invalide l'ecran actif.
 *
 * POURQUOI CA MARCHE ICI :
 *  - `rotation:` est present dans la config YAML, donc ESPHome a compile le
 *    support de rotation (rotation_type_ = ROTATION_SOFTWARE, buffer dedie +
 *    client PPA sur ESP32-P4). Sans cette cle, set_rotation() ne ferait qu'un
 *    ESP_LOGW et ne changerait rien. NE PAS retirer `rotation: 270`.
 *  - Le TACTILE suit tout seul : LVTouchListener applique
 *    LvglComponent::rotate_coordinates() qui lit la rotation COURANTE a chaque
 *    lecture d'indev. Aucune recalibration a faire, la calibration native
 *    720x1280 de tab5-hardware.yaml reste la bonne dans les deux sens.
 *  - Les conteneurs de ui_components/pinball_game.yaml sont deja declares en
 *    720x1280. Tant que le jeu est ferme ils sont HIDDEN, et LVGL ignore les
 *    enfants HIDDEN dans le calcul de scroll : aucun debordement parasite sur
 *    le dashboard paysage.
 *
 * EFFET DE BORD HEUREUX : a rotation 0, le chemin de flush d'ESPHome tombe dans
 * le `default:` et fait `dst = ptr` — AUCUNE rotation logicielle, aucun passage
 * PPA. Le flipper coute donc MOINS cher a afficher que le dashboard paysage,
 * qui lui paie une rotation 270 a chaque flush.
 *
 * @ai_instruction Si un jour l'ecran reste a l'envers apres une fermeture
 *     anormale, le coupable est un chemin de sortie qui ne passe pas par
 *     Pinball::close(). Toutes les sorties DOIVENT passer par close().
 *
 * ===========================================================================
 * [AI-CONTEXT] MAPPING IMU EN PORTRAIT
 * ===========================================================================
 * marble_game.cpp et arkanoid_game.cpp (verifies sur la dalle) utilisent en
 * PAYSAGE : ecran_x = -imu_y, ecran_y = +imu_x.
 * Or a rotation 270 les coordonnees LVGL derivent du panneau natif par
 * lvgl_x = natif_h - natif_y - 1 et lvgl_y = natif_x (rotate_coordinates()).
 * En composant les deux : imu_x est aligne sur natif_x, imu_y sur natif_y.
 * A rotation 0, LVGL = natif, donc en PORTRAIT :
 *
 *     ecran_x = +imu_x        ecran_y = +imu_y
 *
 * (et l'inverse des deux si le joueur a choisi l'ecran retourne, rotation 180).
 * C'est le seul endroit du fichier ou ce mapping est ecrit — tout le reste
 * passe par nudge_axes().
 *
 * ===========================================================================
 * [AI-WARNING] RENDU — ce qu'il ne faut PAS refaire
 * ===========================================================================
 * L'ancien « Flip Noir » (supprime en 697e2e9) dessinait ses murs, ses
 * slingshots et ses flippers avec `lv_obj_set_style_transform_rotation()`
 * recalcule a chaque frame. Resultat : laid (rectangles pivotes creneles) et
 * cher (LVGL repasse par un buffer ARGB8888 des qu'une transformation existe).
 *
 * Ici :
 *  - la table est construite UNE fois (section 10) : arcs `lv_arc` pour l'arche,
 *    polylignes `lv_line` a boite englobante serree pour les rails et les
 *    guides, rectangles arrondis + degrades verticaux pour le reste ;
 *  - seuls la bille, les flippers, les flashs et le plunger bougent (section 11) ;
 *  - ZERO transform_rotation, ZERO allocation, ZERO std::string dans le tick.
 *
 * Les polylignes passent par mk_poly() qui place le widget sur la boite
 * englobante des points : un `lv_line` cree en (0,0) de la taille du terrain
 * serait redessine a chaque deplacement de bille, ce qui annulerait tout
 * l'interet du rendu statique.
 */
#include "pinball_game.h"
#include "esphome/core/preferences.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

namespace Pinball {

// ===========================================================================
// 1. Constantes — geometrie de la table (repere du terrain, 720 x 1140)
// ===========================================================================
// [AI-CONTEXT] Toutes les coordonnees ci-dessous sont LOCALES au conteneur
// `pinball_field` (720 large x 1140 haut, place sous le fronton de 140 px).
// L'ecran complet fait 720x1280. Ne pas melanger les deux reperes.

static constexpr int   SCR_W  = 720;
static constexpr int   SCR_H  = 1280;
static constexpr int   HUD_H  = 140;
static constexpr int   FW     = 720;    // largeur du terrain
static constexpr int   FH     = 1140;   // hauteur du terrain

// --- Arche haute : contrainte circulaire, pas une suite de segments ---------
// Un demi-cercle exact coute UN test de distance par sous-pas au lieu de 18
// tests point-segment, et la bille n'accroche sur aucune facette.
static constexpr float ARCH_CX = 360.0f;
static constexpr float ARCH_CY = 360.0f;
static constexpr float ARCH_R  = 338.0f;

// --- Couloir du plunger (a droite) -----------------------------------------
static constexpr float LANE_X   = 636.0f;   // cloison gauche du couloir
static constexpr float LANE_RX  = 698.0f;   // paroi droite (bord de table)
static constexpr float LANE_BOT = 1108.0f;  // fond du couloir
static constexpr float LANE_MID = 667.0f;   // axe du couloir (repos de la bille)
static constexpr float BALL_REST_Y = 1078.0f;

// --- Flippers ---------------------------------------------------------------
static constexpr float FLIP_PX_L = 200.0f, FLIP_PX_R = 458.0f;
static constexpr float FLIP_PY   = 984.0f;
static constexpr float FLIP_LEN  = 116.0f;
static constexpr float FLIP_REST_L   =  0.46f;              // repos, bras vers le bas
static constexpr float FLIP_ACT_L    = -0.40f;              // frappe
static constexpr float FLIP_REST_R   = 3.14159265f - 0.46f; // miroir
static constexpr float FLIP_ACT_R    = 3.14159265f + 0.40f;
// 18 rad/s = ~48 ms pour les 0,86 rad de course, la valeur d'un vrai flipper.
// Plus rapide, la fenetre de frappe deviendrait trop courte pour une bille
// lancee a pleine vitesse ; plus lent, le battoir semblerait mou.
static constexpr float FLIP_SPEED    = 18.0f;   // rad/s — montee du bras
static constexpr float FLIP_THICK    = 11.0f;   // demi-epaisseur de collision
static constexpr float FLIP_PUNCH    = 1.35f;   // gain d'impulsion a la frappe

// Boites de rendu des flippers (voir mk_poly / section 11) : dimensionnees pour
// contenir tout le balayage + la moitie de l'epaisseur du trait.
static constexpr int FLIP_BOX_X[2] = {184, 337};
static constexpr int FLIP_BOX_Y    = 923;
static constexpr int FLIP_BOX_W    = 140;
static constexpr int FLIP_BOX_H    = 130;

// --- Drain / tablier --------------------------------------------------------
static constexpr float DRAIN_Y  = 1092.0f;  // sous cette ligne, bille perdue
static constexpr int   APRON_Y  = 1078;     // haut du tablier chrome

// ===========================================================================
// 2. Constantes — physique et rythme
// ===========================================================================
// [AI-CONTEXT] La table est simulee « a plat vue de dessus avec une pente » :
// la gravite est CONSTANTE vers le bas de l'ecran, elle ne depend pas de l'IMU.
// C'est volontaire — le joueur peut tenir la tablette droite, inclinee ou posee
// sur un bureau, la table se comporte pareil. L'IMU ne sert QU'AU NUDGE.

static constexpr uint32_t TICK_MS      = 20;    // 50 Hz en partie
static constexpr uint32_t TICK_MENU_MS = 200;   // menus : rien a animer
static constexpr int   SUBSTEP = 4;             // sous-pas anti-tunnelling
static constexpr float DT      = TICK_MS / 1000.0f;
static constexpr float SDT     = DT / SUBSTEP;

static constexpr float BALL_R      = 13.0f;
// [AI-CONTEXT] GRAVITY calibree, pas choisie au hasard : un plateau reel fait
// ~1,20 m pour 1140 px ici (950 px/m) et une table inclinee a 6,5 deg donne une
// acceleration utile de ~1,15 m/s^2, soit ~1090 px/s^2. On arrondit a 1150 :
// la bille traverse le plateau en ~1,3 s, exactement le rythme d'une machine.
// Toute modification de GRAVITY oblige a refaire le calcul de PLUNGER_* ci-dessous.
static constexpr float GRAVITY     = 1150.0f;   // px/s^2 (pente de la table)
static constexpr float FRICTION    = 0.9990f;   // par sous-pas
static constexpr float MAX_SPEED   = 1900.0f;
static constexpr float REST_WALL   = 0.30f;     // rebond des rails
static constexpr float REST_SOFT   = 0.12f;     // rebond des guides mous
static constexpr float BUMPER_KICK = 690.0f;
static constexpr float SLING_KICK  = 620.0f;
static constexpr float TARGET_KICK = 240.0f;

// Demi-epaisseur de collision des rails : la valeur est calee sur l'epaisseur
// DESSINEE (15 px, cf. mk_rail) pour que la bille touche pile le rail visible.
static constexpr float RAIL_HALF = 7.0f;

// [AI-CONTEXT] Course du lanceur, calculee et pas devinee (h = v^2 / 2g depuis
// BALL_REST_Y = 1078) :
//   - PLUNGER_MIN 1300 -> apogee y ~ 343 : la bille sort tout juste du couloir
//     (qui s'ouvre a y = 430) et retombe mollement a droite du plateau. Un tir
//     faible RATE, c'est voulu, sinon la jauge ne servirait a rien.
//   - PLUNGER_MAX 1800 -> depasse le sommet de l'arche (y = 22) avec de la
//     reserve : la bille fait le tour complet et traverse les 3 rollovers du
//     skill shot. C'est le seul tir qui les atteint.
// Baisser GRAVITY ou remonter BALL_REST_Y sans refaire ce calcul rendrait le
// skill shot injouable (la bille calerait sur le flanc droit de l'arche).
static constexpr float PLUNGER_CHARGE_S = 1.10f;  // secondes pour charger a fond
static constexpr float PLUNGER_MIN = 1300.0f;
static constexpr float PLUNGER_MAX = 1800.0f;
static constexpr uint32_t AUTOLAUNCH_MS = 9000;   // securite anti-blocage

// --- Nudge / TILT -----------------------------------------------------------
static constexpr float NUDGE_HP    = 0.12f;   // coupure du passe-haut
static constexpr float NUDGE_GAIN  = 900.0f;  // px/s d'impulsion par g de secousse
static constexpr uint32_t NUDGE_COOLDOWN_MS = 260;
static constexpr int   TILT_HITS   = 3;
static constexpr uint32_t TILT_DECAY_MS   = 2600;  // un cran de compteur s'efface
static constexpr uint32_t TILT_PENALTY_MS = 2500;  // flippers morts

// --- Modes ------------------------------------------------------------------
static constexpr uint32_t FRENZY_MS = 9000;
static constexpr int MAX_SIM_BALLS = 3;
static constexpr int BALLS_PER_GAME = 3;

// --- Score ------------------------------------------------------------------
static constexpr uint32_t SC_BUMPER   = 1200;
static constexpr uint32_t SC_SLING    = 400;
static constexpr uint32_t SC_TARGET   = 3500;
static constexpr uint32_t SC_LANE     = 900;
static constexpr uint32_t SC_SKILL    = 25000;
static constexpr uint32_t SC_BANK     = 12000;
static constexpr uint32_t SC_JACKPOT  = 30000;
static constexpr uint32_t BONUS_BALL_1 = 250000;
static constexpr uint32_t BONUS_BALL_2 = 750000;

// --- Persistance ------------------------------------------------------------
// [AI-WARNING] « PIN2 » et pas « PIN1 » : PIN1 etait le layout de l'ancien Flip
// Noir. Le reutiliser ferait charger les octets d'un autre jeu ici.
static constexpr uint32_t PINBALL_SAVE_MAGIC = 0x50494E32u;  // "PIN2"
static constexpr uint32_t PREF_KEY           = 0x504E4232u;  // cle NVS dediee

// ===========================================================================
// 3. Sons — stubs (meme convention qu'arkanoid_game.cpp)
// ===========================================================================
// [AI-CONTEXT] Aucun bip local n'est emis pour l'instant : le haut-parleur du
// Tab5 est pilote par le pipeline vocal Home Assistant (media_player + micro),
// et lui voler la sortie audio pendant une partie couperait une reponse de
// l'assistant. Ces fonctions sont le point d'accroche unique le jour ou un
// buzzer PWM local (ou un canal mixe) sera disponible.
static inline void sfx_bumper()   { /* STUB : pop court et sec */ }
static inline void sfx_sling()    { /* STUB : claquement */ }
static inline void sfx_target()   { /* STUB : bip aigu */ }
static inline void sfx_bank()     { /* STUB : arpege montant */ }
static inline void sfx_launch()   { /* STUB : ressort */ }
static inline void sfx_drain()    { /* STUB : bip grave */ }
static inline void sfx_tilt()     { /* STUB : buzzer d'alarme */ }
static inline void sfx_multiball(){ /* STUB : fanfare */ }
static inline void sfx_gameover() { /* STUB : descente */ }

// ===========================================================================
// 4. Aleatoire (xorshift32) et petits utilitaires
// ===========================================================================

static uint32_t s_rng = 0x9E3779B9u;
static inline uint32_t rnd() {
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}
// Flottant aleatoire dans [-1, 1].
static inline float rndf() { return (float)(int32_t)(rnd() >> 8) / 8388608.0f - 1.0f; }

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// ===========================================================================
// 5. Types
// ===========================================================================

// Segment de collision. `rest` = coefficient de restitution.
// `one_way != 0` : le segment n'existe que pour une bille qui DESCEND — c'est
// la porte anti-retour en haut du couloir du plunger, exactement comme le
// clapet metallique d'une vraie machine.
struct Seg {
    float x1, y1, x2, y2;
    float rest;
    uint8_t one_way;
};

struct Bumper {
    float cx, cy, r;
    uint32_t color;
    uint32_t flash_until;
    lv_obj_t* flash;
};

struct Target {
    float cx, cy;      // centre de la barre
    float half;        // demi-longueur
    bool  down;
    lv_obj_t* obj;     // cible levee (masquee quand tombee)
    lv_obj_t* lamp;    // insert sous la cible (reste visible)
};

struct Sling {
    float x1, y1, x2, y2;   // face active
    float nx, ny;           // normale (unitaire) de la detente
    uint32_t flash_until;
    lv_obj_t* flash;
};

struct Rollover {
    float cx, cy, r;
    bool  lit;
    lv_obj_t* obj;
};

struct Flipper {
    float px, py;
    float angle, target, rest, active;
    bool  pressed;
    float draw_angle;   // dernier angle effectivement dessine
};

struct Ball {
    float x, y, vx, vy;
    bool  active;
    bool  in_plunger;
    int   dx, dy;       // derniere position dessinee (entiers)
    lv_obj_t* shadow;
    lv_obj_t* body;
    lv_obj_t* gloss;
};

// ===========================================================================
// 6. Donnees de table
// ===========================================================================
// [AI-CONTEXT] Lecture de la table, de haut en bas :
//   - arche haute (demi-cercle) : le tir du plunger monte a droite, contourne
//     par le haut et redescend a gauche. 3 rollovers y sont poses sur la
//     trajectoire (skill shot).
//   - 3 bumpers en triangle dans le tiers haut.
//   - 3 inserts lumineux (FRENZY / MULTI / SKILL) puis la banque de 3 cibles.
//   - 2 slingshots, 2 couloirs de retour (inlane) et 2 couloirs de perte
//     (outlane, sans plancher : la bille y tombe).
//   - 2 flippers, drain au centre, tablier chrome par-dessus.

static const Seg WALLS[] = {
    // --- Bord gauche puis entonnoir de l'outlane gauche ---------------------
    { 22.0f,  360.0f,  22.0f,  900.0f, REST_WALL, 0 },
    { 22.0f,  900.0f,  46.0f, 1010.0f, REST_WALL, 0 },
    { 46.0f, 1010.0f,  64.0f, 1070.0f, REST_WALL, 0 },
    // --- Cloison du couloir du plunger = bord droit du terrain --------------
    {636.0f,  430.0f, 636.0f, 1108.0f, REST_WALL, 0 },
    // --- Entonnoir de l'outlane droite --------------------------------------
    {636.0f,  900.0f, 612.0f, 1010.0f, REST_WALL, 0 },
    {612.0f, 1010.0f, 594.0f, 1070.0f, REST_WALL, 0 },
    // --- Separateurs outlane / inlane (les « posts ») ------------------------
    {126.0f,  850.0f, 110.0f,  928.0f, REST_SOFT, 0 },
    {532.0f,  850.0f, 548.0f,  928.0f, REST_SOFT, 0 },
    // --- Planchers d'inlane : ils deposent la bille sur le flipper -----------
    {108.0f,  930.0f, 196.0f,  962.0f, REST_SOFT, 0 },
    {550.0f,  930.0f, 462.0f,  962.0f, REST_SOFT, 0 },
    // --- Couloir du plunger : paroi droite et fond --------------------------
    {698.0f,  360.0f, 698.0f, 1108.0f, REST_WALL, 0 },
    {636.0f, 1108.0f, 698.0f, 1108.0f, 0.02f,     0 },
    // --- Clapet anti-retour en haut du couloir ------------------------------
    {636.0f,  366.0f, 636.0f,  430.0f, REST_WALL, 1 },
};
static constexpr int N_WALLS = (int)(sizeof(WALLS) / sizeof(WALLS[0]));

static constexpr int N_BUMPERS   = 3;
static constexpr int N_TARGETS   = 3;
static constexpr int N_SLINGS    = 2;
static constexpr int N_ROLLOVERS = 3;

static Bumper   g_bump[N_BUMPERS];
static Target   g_targ[N_TARGETS];
static Sling    g_sling[N_SLINGS];
static Rollover g_roll[N_ROLLOVERS];
static Flipper  g_flip[2];
static Ball     g_balls[MAX_SIM_BALLS];

// ===========================================================================
// 7. Etat runtime
// ===========================================================================

enum State : uint8_t {
    ST_OFF = 0, ST_HUB, ST_SCORES, ST_SETTINGS, ST_PLAYING, ST_PAUSED, ST_GAMEOVER
};

static PinballSave g_save{};
static esphome::ESPPreferenceObject g_pref;
static bool  g_pref_ready = false;

static UI    g_ui{};
static bool  g_built = false;
static State g_state = ST_OFF;
static lv_timer_t* g_timer = nullptr;
static uint32_t g_tick_period = TICK_MENU_MS;

// --- Partie -----------------------------------------------------------------
static uint32_t g_score = 0;
static uint32_t g_ball_score = 0;      // points marques sur la bille en cours
static int      g_ball_num = 0;
static int      g_balls_total = BALLS_PER_GAME;
static int      g_active_balls = 0;
static int      g_multiplier = 1;
static int      g_game_multiballs = 0;
static bool     g_game_tilted = false;
static bool     g_bonus1 = false, g_bonus2 = false;
static int      g_banks_done = 0;      // banques de cibles completees

// --- Plunger ----------------------------------------------------------------
static bool     g_plunger_held = false;
static float    g_plunger_charge = 0.0f;
static uint32_t g_autolaunch_at = 0;
static uint32_t g_serve_at = 0;        // 0 = rien en attente

// --- Skill shot -------------------------------------------------------------
static int      g_skill_lane = -1;     // rollover a viser apres le tir
static uint32_t g_skill_until = 0;

// --- Modes ------------------------------------------------------------------
static uint32_t g_frenzy_until = 0;
static bool     g_mb_active = false;

// --- IMU / nudge / TILT -----------------------------------------------------
static float    g_raw_x = 0.0f, g_raw_y = 0.0f;
static float    g_slow_x = 0.0f, g_slow_y = 0.0f;
static int      g_tilt_hits = 0;
static uint32_t g_tilt_decay_at = 0;
static bool     g_tilted = false;
static uint32_t g_tilt_until = 0;
static uint32_t g_nudge_ok_at = 0;

// --- Feedback ---------------------------------------------------------------
static char     g_toast_buf[48];
static uint32_t g_toast_until = 0;

// --- Objets LVGL statiques --------------------------------------------------
static lv_obj_t* g_hud_ball  = nullptr;
static lv_obj_t* g_hud_dot[BALLS_PER_GAME] = {};
static lv_obj_t* g_hud_score = nullptr;
static lv_obj_t* g_hud_best  = nullptr;
static lv_obj_t* g_hud_badge = nullptr;
static lv_obj_t* g_hud_tilt  = nullptr;
static lv_obj_t* g_pwr_bg    = nullptr;
static lv_obj_t* g_pwr_fill  = nullptr;
static lv_obj_t* g_toast     = nullptr;
static lv_obj_t* g_insert[3] = {};
static lv_obj_t* g_insert_l[3] = {};
static lv_obj_t* g_plunger_rod = nullptr;
static lv_obj_t* g_flip_base[2] = {};
static lv_obj_t* g_flip_edge[2] = {};
static lv_obj_t* g_zone_l = nullptr;
static lv_obj_t* g_zone_r = nullptr;
static lv_obj_t* g_zone_p = nullptr;
static lv_obj_t* g_plunger_hint = nullptr;

// --- Panneau de menus -------------------------------------------------------
static lv_obj_t* g_p_title = nullptr;
static lv_obj_t* g_p_sub   = nullptr;
static lv_obj_t* g_p_body  = nullptr;
static lv_obj_t* g_p_foot  = nullptr;
static constexpr int N_SLOTS = 7;
static lv_obj_t* g_slot[N_SLOTS]   = {};
static lv_obj_t* g_slot_t[N_SLOTS] = {};
static lv_obj_t* g_slot_d[N_SLOTS] = {};
// Pictogramme « tournez la tablette » du hub (3 objets, aucune police MDI a
// enrichir : deux rectangles et un chevron suffisent et restent lisibles).
static lv_obj_t* g_rot_land = nullptr;
static lv_obj_t* g_rot_port = nullptr;
static lv_obj_t* g_rot_arrow = nullptr;

// --- Caches HUD (on ne reecrit un libelle que si sa valeur change) ----------
static uint32_t g_c_score = 0xFFFFFFFFu;
static int      g_c_ball  = -1;
static int      g_c_dots  = -1;
static bool     g_c_tilt  = false;
static char     g_c_badge[24] = {0};

// --- Pool de points pour les polylignes statiques ---------------------------
// [AI-WARNING] lv_line_set_points() ne COPIE PAS le tableau, il n'en garde que
// l'adresse. Les points doivent donc vivre aussi longtemps que le widget :
// d'ou ce pool statique plutot que des tableaux locaux.
static constexpr int MAX_PTS = 96;
static lv_point_precise_t g_pts[MAX_PTS];
static int g_pts_n = 0;
// Points des flippers : un tableau par flipper, partage par les deux traits
// (corps sombre + arete neon) puisque lv_line ne stocke que le pointeur.
static lv_point_precise_t g_flip_pts[2][2];

// Declarations avancees
static void go_hub();
static void go_scores();
static void go_settings();
static void go_pause();
static void go_gameover();
static void tick_period_sync();

// ===========================================================================
// 8. Persistance NVS
// ===========================================================================

void persist_load() {
    if (!g_pref_ready) {
        g_pref = esphome::global_preferences->make_preference<PinballSave>(PREF_KEY);
        g_pref_ready = true;
    }
    if (!g_pref.load(&g_save) || g_save.magic != PINBALL_SAVE_MAGIC) {
        g_save = PinballSave{};
        g_save.magic = PINBALL_SAVE_MAGIC;
        g_save.nudge_sens = 2;
        g_save.sfx = 1;
    }
    if (g_save.nudge_sens > 4) g_save.nudge_sens = 2;
    if (g_save.flip_screen > 1) g_save.flip_screen = 0;
    if (g_save.invert_nudge > 1) g_save.invert_nudge = 0;
}

void persist_save() {
    if (!g_pref_ready) return;
    g_save.magic = PINBALL_SAVE_MAGIC;
    g_pref.save(&g_save);
    esphome::global_preferences->sync();
}

// Insere un score dans le top 10 et renvoie son rang (0 = premier), -1 si hors
// classement. Le tableau est trie par score decroissant.
static int scores_insert(uint32_t sc, int balls, int mb, bool tilted) {
    if (sc == 0) return -1;
    int pos = -1;
    for (int i = 0; i < PINBALL_NSCORES; i++) {
        if (sc > g_save.top[i].score) { pos = i; break; }
    }
    if (pos < 0) return -1;
    for (int i = PINBALL_NSCORES - 1; i > pos; i--) g_save.top[i] = g_save.top[i - 1];
    g_save.top[pos].score        = sc;
    g_save.top[pos].ball_reached = (uint16_t) balls;
    g_save.top[pos].multiballs   = (uint8_t) (mb > 255 ? 255 : mb);
    g_save.top[pos].tilted       = tilted ? 1 : 0;
    return pos;
}

static inline uint32_t best_score() { return g_save.top[0].score; }

// ===========================================================================
// 9. Helpers LVGL
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

// Idempotent A DESSEIN : show() est appelé des dizaines de fois par frame (flashs
// de bumpers, billes, jauges). Poser un flag déjà posé fait quand même repasser
// LVGL par lv_obj_invalidate — a 50 Hz ça salit l'écran pour rien.
static inline void show(lv_obj_t* o, bool v) {
    if (!o) return;
    if (v == !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (v) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else   lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static inline void set_bg(lv_obj_t* o, uint32_t c, lv_opa_t opa) {
    if (!o) return;
    lv_obj_set_style_bg_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, opa, LV_PART_MAIN);
}

// Degrade vertical : c'est lui qui donne du volume aux pieces sans coûter
// d'objet supplementaire (une seule passe de dessin LVGL).
static inline void set_grad(lv_obj_t* o, uint32_t hi, uint32_t lo) {
    if (!o) return;
    lv_obj_set_style_bg_color(o, lv_color_hex(hi), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(o, lv_color_hex(lo), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
}

static inline void set_border(lv_obj_t* o, uint32_t c, int w, lv_opa_t opa) {
    if (!o) return;
    lv_obj_set_style_border_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, w, LV_PART_MAIN);
    lv_obj_set_style_border_opa(o, opa, LV_PART_MAIN);
}

// N'ecrit un libelle que si le texte a change : evite de reconstruire le layout
// LVGL a 50 Hz pour rien.
static void set_text_if(lv_obj_t* l, const char* txt) {
    if (!l || !txt) return;
    const char* cur = lv_label_get_text(l);
    if (cur && strcmp(cur, txt) == 0) return;
    lv_label_set_text(l, txt);
}

// Formate un score avec des espaces fins tous les 3 chiffres (« 1 250 000 »).
// La police roboto_55_b ne contient que les chiffres, l'espace et quelques
// symboles : aucune lettre ne doit passer par ce buffer.
static void fmt_score(char* buf, size_t n, uint32_t v) {
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%lu", (unsigned long) v);
    int len = (int) strlen(tmp);
    size_t o = 0;
    for (int i = 0; i < len && o + 2 < n; i++) {
        if (i > 0 && ((len - i) % 3) == 0) buf[o++] = ' ';
        buf[o++] = tmp[i];
    }
    buf[o] = '\0';
}

// --- Polyligne a boite englobante serree ------------------------------------
// Voir le [AI-WARNING] RENDU en tete de fichier : un lv_line pose en (0,0) a la
// taille du terrain serait redessine a chaque frame. On place donc le widget
// exactement sur ses points et on convertit ceux-ci en coordonnees locales.
static lv_obj_t* mk_poly(lv_obj_t* parent, const float* xy, int n,
                         int width, uint32_t color, lv_opa_t opa) {
    if (n < 2 || g_pts_n + n > MAX_PTS) return nullptr;
    float minx = xy[0], maxx = xy[0], miny = xy[1], maxy = xy[1];
    for (int i = 1; i < n; i++) {
        if (xy[2 * i]     < minx) minx = xy[2 * i];
        if (xy[2 * i]     > maxx) maxx = xy[2 * i];
        if (xy[2 * i + 1] < miny) miny = xy[2 * i + 1];
        if (xy[2 * i + 1] > maxy) maxy = xy[2 * i + 1];
    }
    const int m = width / 2 + 2;          // marge : demi-epaisseur + bout arrondi
    const int ox = (int) minx - m, oy = (int) miny - m;
    lv_point_precise_t* p = &g_pts[g_pts_n];
    for (int i = 0; i < n; i++) {
        p[i].x = (lv_value_precise_t) ((int) xy[2 * i]     - ox);
        p[i].y = (lv_value_precise_t) ((int) xy[2 * i + 1] - oy);
    }
    g_pts_n += n;

    lv_obj_t* o = lv_line_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_line_width(o, width, LV_PART_MAIN);
    lv_obj_set_style_line_color(o, lv_color_hex(color), LV_PART_MAIN);
    lv_obj_set_style_line_opa(o, opa, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(o, true, LV_PART_MAIN);
    lv_line_set_points(o, p, n);
    lv_obj_set_pos(o, ox, oy);
    lv_obj_set_size(o, (int) (maxx - minx) + 2 * m, (int) (maxy - miny) + 2 * m);
    return o;
}

// Un rail = deux passes : un corps epais en acier froid, une arete fine et
// claire par-dessus. C'est ce qui donne le relief sans aucune image.
static void mk_rail(lv_obj_t* parent, const float* xy, int n) {
    mk_poly(parent, xy, n, 15, UIColor::PIN_RAIL, LV_OPA_COVER);
    mk_poly(parent, xy, n, 5,  UIColor::PIN_RAIL_HI, 190);
}

// Arc decoratif ou structurel. Le knob et l'anneau de fond sont neutralises :
// on ne veut qu'un trait courbe, pas un widget interactif (meme recette que
// mk_wedge() dans trivia_game.cpp).
static lv_obj_t* mk_arc(lv_obj_t* parent, float cx, float cy, float r,
                        int a0, int a1, int width, uint32_t color, lv_opa_t opa) {
    lv_obj_t* a = lv_arc_create(parent);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_width(a, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(a, 0, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, LV_OPA_TRANSP, LV_PART_MAIN);   // pas d'anneau de fond
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, LV_PART_KNOB);    // pas de poignee
    lv_obj_set_style_pad_all(a, 0, LV_PART_KNOB);
    lv_obj_set_style_arc_width(a, width, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(a, true, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, lv_color_hex(color), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(a, opa, LV_PART_INDICATOR);
    lv_arc_set_rotation(a, 0);
    lv_arc_set_bg_angles(a, a0, a1);
    lv_arc_set_angles(a, a0, a1);
    int d = (int) (r * 2.0f) + width;
    lv_obj_set_size(a, d, d);
    lv_obj_set_pos(a, (int) (cx - d / 2.0f), (int) (cy - d / 2.0f));
    return a;
}

// ===========================================================================
// 10. Orientation de l'ecran
// ===========================================================================
// [AI-CONTEXT] Voir le bloc ORIENTATION en tete de fichier. `portrait=false`
// remet EXACTEMENT la rotation de repos du dashboard (270), jamais autre chose.

static void screen_portrait(bool portrait) {
    if (!g_ui.lvgl) return;
    int angle = 270;
    if (portrait) angle = g_save.flip_screen ? 180 : 0;
    g_ui.lvgl->set_rotation(angle);
}

// ===========================================================================
// 11. Physique
// ===========================================================================
// [AI-CONTEXT] Schema de collision :
//   - bille                       = cercle (x, y, BALL_R)
//   - rails, guides, planchers    = segments (projection point-segment)
//   - arche haute                 = contrainte circulaire (un seul test)
//   - bumpers, cibles, rollovers  = cercles / capsules
//   - flippers                    = segments mobiles (vitesse au point de contact)
// Chaque sous-pas : integration, puis resolution de toutes les collisions.

static float point_seg_dist(float px, float py, float x1, float y1, float x2, float y2,
                            float& cx, float& cy) {
    const float dx = x2 - x1, dy = y2 - y1;
    const float len2 = dx * dx + dy * dy;
    float t = 0.0f;
    if (len2 > 0.0001f) {
        t = ((px - x1) * dx + (py - y1) * dy) / len2;
        t = clampf(t, 0.0f, 1.0f);
    }
    cx = x1 + t * dx;
    cy = y1 + t * dy;
    const float ex = px - cx, ey = py - cy;
    return sqrtf(ex * ex + ey * ey);
}

// Rebond generique bille / segment epais (capsule de rayon `rad`).
static bool collide_capsule(Ball& b, float x1, float y1, float x2, float y2,
                            float rad, float rest) {
    float cx, cy;
    const float d = point_seg_dist(b.x, b.y, x1, y1, x2, y2, cx, cy);
    if (d >= rad || d < 0.0001f) return false;
    const float nx = (b.x - cx) / d, ny = (b.y - cy) / d;
    const float pen = rad - d;
    b.x += nx * pen;
    b.y += ny * pen;
    const float vn = b.vx * nx + b.vy * ny;
    if (vn < 0.0f) {
        b.vx -= (1.0f + rest) * vn * nx;
        b.vy -= (1.0f + rest) * vn * ny;
    }
    return true;
}

// L'arche : la bille ne peut pas sortir du disque tant qu'elle est au-dessus du
// diametre. Un seul sqrt par sous-pas, et une courbe parfaitement lisse.
static void collide_arch(Ball& b) {
    if (b.y >= ARCH_CY) return;
    const float dx = b.x - ARCH_CX, dy = b.y - ARCH_CY;
    const float d = sqrtf(dx * dx + dy * dy);
    // -RAIL_HALF : le rail est DESSINE centre sur ARCH_R avec 15 px d'epaisseur,
    // la bille doit donc s'arreter sur sa face interieure, pas sur son axe.
    const float lim = ARCH_R - BALL_R - RAIL_HALF;
    if (d <= lim || d < 0.0001f) return;
    const float ux = dx / d, uy = dy / d;
    b.x = ARCH_CX + ux * lim;
    b.y = ARCH_CY + uy * lim;
    const float nx = -ux, ny = -uy;               // normale rentrante
    const float vn = b.vx * nx + b.vy * ny;
    if (vn < 0.0f) {
        b.vx -= (1.0f + REST_WALL) * vn * nx;
        b.vy -= (1.0f + REST_WALL) * vn * ny;
    }
}

// ===========================================================================
// 12. Scoring et modes
// ===========================================================================

static void toast(const char* txt, uint32_t ms) {
    snprintf(g_toast_buf, sizeof(g_toast_buf), "%s", txt);
    set_text_if(g_toast, g_toast_buf);
    show(g_toast, true);
    g_toast_until = lv_tick_get() + ms;
}

static void add_score(uint32_t pts) {
    const uint32_t gain = pts * (uint32_t) g_multiplier;
    g_score += gain;
    g_ball_score += gain;
    if (!g_bonus1 && g_score >= BONUS_BALL_1) {
        g_bonus1 = true; g_balls_total++;
        toast("BILLE BONUS", 1800);
    } else if (!g_bonus2 && g_score >= BONUS_BALL_2) {
        g_bonus2 = true; g_balls_total++;
        toast("BILLE BONUS", 1800);
    }
}

static void start_frenzy() {
    g_frenzy_until = lv_tick_get() + FRENZY_MS;
    toast("BUMPER FRENZY", 1600);
    sfx_bank();
}

// Lance la bille supplementaire du multiball : elle part du couloir et se tire
// toute seule (voir g_autolaunch_at). Une seule bille en plus : deux billes
// suffisent a rendre l'ecran vivant sans le rendre illisible.
static void start_multiball() {
    int slot = -1;
    for (int i = 0; i < MAX_SIM_BALLS; i++) if (!g_balls[i].active) { slot = i; break; }
    if (slot < 0) return;
    Ball& b = g_balls[slot];
    b.x = LANE_MID; b.y = BALL_REST_Y;
    b.vx = 0.0f; b.vy = 0.0f;
    b.active = true; b.in_plunger = true;
    b.dx = b.dy = -9999;    // invalide le cache de rendu, sinon la bille reste invisible
    g_active_balls++;
    g_mb_active = true;
    g_multiplier = 2;
    g_game_multiballs++;
    g_save.multiballs++;
    g_autolaunch_at = lv_tick_get() + 900;
    toast("MULTIBALL", 2000);
    sfx_multiball();
}

// Banque de cibles completee : alterne mode court et multiball, pour que les
// deux recompenses tombent a coup sur au fil d'une partie.
static void bank_complete() {
    add_score(SC_BANK);
    g_banks_done++;
    for (int i = 0; i < N_TARGETS; i++) {
        g_targ[i].down = false;
        show(g_targ[i].obj, true);
    }
    if ((g_banks_done % 2) == 1) start_frenzy();
    else if (!g_mb_active)       start_multiball();
    else                         start_frenzy();
}

// ===========================================================================
// 13. Construction de l'UI — table statique (une seule fois)
// ===========================================================================

static void zone_event_cb(lv_event_t* e);
static void hud_event_cb(lv_event_t* e);
static void slot_event_cb(lv_event_t* e);
static void flipper_points(int s, bool force);

// --- Donnees de placement des pieces animees --------------------------------
static void build_table_data() {
    struct { float x, y, r; uint32_t c; } bd[N_BUMPERS] = {
        {186.0f, 570.0f, 36.0f, UIColor::PIN_CYAN},
        {330.0f, 476.0f, 40.0f, UIColor::PIN_AMBER},
        {474.0f, 570.0f, 36.0f, UIColor::PIN_MAGENTA},
    };
    for (int i = 0; i < N_BUMPERS; i++) {
        g_bump[i].cx = bd[i].x; g_bump[i].cy = bd[i].y; g_bump[i].r = bd[i].r;
        g_bump[i].color = bd[i].c; g_bump[i].flash_until = 0; g_bump[i].flash = nullptr;
    }

    const float tx[N_TARGETS] = {246.0f, 330.0f, 414.0f};
    for (int i = 0; i < N_TARGETS; i++) {
        g_targ[i].cx = tx[i]; g_targ[i].cy = 752.0f; g_targ[i].half = 32.0f;
        g_targ[i].down = false; g_targ[i].obj = nullptr; g_targ[i].lamp = nullptr;
    }

    // Slingshots : la normale est la direction de la detente, pas la normale
    // geometrique du segment — c'est elle qui decide ou part la bille.
    g_sling[0] = {108.0f, 828.0f, 218.0f, 908.0f,  0.609f, -0.793f, 0, nullptr};
    g_sling[1] = {550.0f, 828.0f, 440.0f, 908.0f, -0.609f, -0.793f, 0, nullptr};

    // Rollovers poses sur la trajectoire de l'arche (rayon ARCH_R - 32) :
    // une bille qui longe l'arche les traverse forcement.
    const float rr = ARCH_R - 32.0f;
    const float ang[N_ROLLOVERS] = {2.007f, 1.5708f, 1.134f};   // 115 / 90 / 65 deg
    for (int i = 0; i < N_ROLLOVERS; i++) {
        g_roll[i].cx = ARCH_CX + rr * cosf(ang[i]);
        g_roll[i].cy = ARCH_CY - rr * sinf(ang[i]);
        g_roll[i].r  = 18.0f;
        g_roll[i].lit = true;
        g_roll[i].obj = nullptr;
    }

    g_flip[0] = {FLIP_PX_L, FLIP_PY, FLIP_REST_L, FLIP_REST_L, FLIP_REST_L, FLIP_ACT_L, false, 99.0f};
    g_flip[1] = {FLIP_PX_R, FLIP_PY, FLIP_REST_R, FLIP_REST_R, FLIP_REST_R, FLIP_ACT_R, false, 99.0f};

    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        g_balls[i].active = false;
        g_balls[i].in_plunger = false;
        g_balls[i].dx = g_balls[i].dy = -9999;
    }
}

// --- Decor sous le verre : ce que la bille survole ---------------------------
// [AI-CONTEXT] Sur une vraie table, la bille roule PAR-DESSUS la serigraphie.
// Tout ce qui est dessine ici est donc volontairement non collisionnable et
// tres peu opaque : c'est de la peinture, pas de la geometrie.
static void build_art(lv_obj_t* field) {
    // Sol : degrade vertical du bleu nuit vers le noir. Le haut plus clair
    // « eclaire » l'arche, le bas sombre fait ressortir le tablier chrome.
    set_grad(field, UIColor::PIN_FELT_HI, UIColor::PIN_FELT_LO);

    // Orbites peintes sous l'arche.
    mk_arc(field, ARCH_CX, ARCH_CY, ARCH_R - 44.0f, 182, 358, 3, UIColor::PIN_CYAN, 42);
    mk_arc(field, ARCH_CX, ARCH_CY, ARCH_R - 62.0f, 195, 345, 2, UIColor::PIN_CYAN, 26);

    // Halo peint autour du groupe de bumpers : deux disques tres transparents
    // suffisent a creuser le centre de la table.
    lv_obj_t* halo = mk_rect(field);
    lv_obj_set_size(halo, 420, 300);
    lv_obj_set_pos(halo, 120, 400);
    lv_obj_set_style_radius(halo, 150, LV_PART_MAIN);
    set_bg(halo, UIColor::PIN_CYAN, 16);

    // Liseres de fuite au bas du plateau : ils donnent la pente.
    for (int i = 0; i < 3; i++) {
        lv_obj_t* r = mk_rect(field);
        lv_obj_set_size(r, 520 - i * 90, 2);
        lv_obj_set_pos(r, 100 + i * 45, 1000 + i * 22);
        set_bg(r, UIColor::PIN_RAIL_HI, (lv_opa_t) (26 - i * 7));
    }

    // Couloir du plunger : fond legerement plus clair + fleches de tir.
    lv_obj_t* lane = mk_rect(field);
    lv_obj_set_size(lane, (int) (LANE_RX - LANE_X), (int) (LANE_BOT - 380.0f));
    lv_obj_set_pos(lane, (int) LANE_X, 380);
    set_grad(lane, UIColor::PIN_FELT_HI, UIColor::PIN_VOID);
    lv_obj_set_style_bg_opa(lane, 210, LV_PART_MAIN);
    for (int i = 0; i < 4; i++) {
        lv_obj_t* a = mk_rect(field);
        lv_obj_set_size(a, 26, 3);
        lv_obj_set_pos(a, (int) LANE_MID - 13, 930 - i * 34);
        lv_obj_set_style_radius(a, 2, LV_PART_MAIN);
        set_bg(a, UIColor::PIN_CYAN, (lv_opa_t) (110 - i * 22));
    }

    // Inserts lumineux : FRENZY / MULTI / SKILL. Eteints par defaut, allumes
    // par hud_sync() quand le mode correspondant tourne.
    static const char* INS[3] = {"FRENZY", "MULTI", "SKILL"};
    static const uint32_t INSC[3] = {UIColor::PIN_CYAN, UIColor::PIN_MAGENTA, UIColor::PIN_AMBER};
    for (int i = 0; i < 3; i++) {
        lv_obj_t* o = mk_rect(field);
        lv_obj_set_size(o, 104, 34);
        lv_obj_set_pos(o, 151 + i * 126, 652);
        lv_obj_set_style_radius(o, 8, LV_PART_MAIN);
        set_bg(o, UIColor::PIN_INSERT_OFF, LV_OPA_COVER);
        set_border(o, INSC[i], 2, 90);
        g_insert[i] = o;
        lv_obj_t* l = mk_label(o, g_ui.f_small, INSC[i]);
        lv_obj_set_style_text_opa(l, 130, LV_PART_MAIN);
        lv_obj_set_width(l, 104);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 0);
        lv_label_set_text(l, INS[i]);
        g_insert_l[i] = l;
    }
}

// --- Rails, arche et guides -------------------------------------------------
static void build_rails(lv_obj_t* field) {
    // Arche : deux passes, corps d'acier puis liseré neon interieur.
    mk_arc(field, ARCH_CX, ARCH_CY, ARCH_R, 180, 360, 15, UIColor::PIN_RAIL, LV_OPA_COVER);
    mk_arc(field, ARCH_CX, ARCH_CY, ARCH_R - 9.0f, 181, 359, 4, UIColor::PIN_RAIL_HI, 170);

    // Tous les segments de collision sont doubles par un rail visible, sauf le
    // clapet anti-retour (invisible sur une vraie machine aussi) : on le marque
    // par un simple trait fin pour que le joueur comprenne le sens unique.
    for (int i = 0; i < N_WALLS; i++) {
        const Seg& s = WALLS[i];
        const float xy[4] = {s.x1, s.y1, s.x2, s.y2};
        if (s.one_way) {
            mk_poly(field, xy, 2, 4, UIColor::PIN_RAIL, 130);
            continue;
        }
        mk_rail(field, xy, 2);
    }
}

// --- Pieces marquantes : bumpers, cibles, slingshots, rollovers -------------
static void build_pieces(lv_obj_t* field) {
    // Rollovers : anneau fin, rempli quand la lane est allumee.
    for (int i = 0; i < N_ROLLOVERS; i++) {
        lv_obj_t* o = mk_rect(field);
        const int d = (int) (g_roll[i].r * 2.0f);
        lv_obj_set_size(o, d, d);
        lv_obj_set_pos(o, (int) (g_roll[i].cx - g_roll[i].r), (int) (g_roll[i].cy - g_roll[i].r));
        lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(o, UIColor::PIN_CYAN, 40);
        set_border(o, UIColor::PIN_CYAN, 3, LV_OPA_COVER);
        g_roll[i].obj = o;
    }

    // Cibles drop : une lampe encastree (toujours visible) + la barre qui tombe.
    for (int i = 0; i < N_TARGETS; i++) {
        Target& t = g_targ[i];
        lv_obj_t* lamp = mk_rect(field);
        lv_obj_set_size(lamp, (int) (t.half * 2.0f) + 8, 10);
        lv_obj_set_pos(lamp, (int) (t.cx - t.half) - 4, (int) t.cy + 9);
        lv_obj_set_style_radius(lamp, 5, LV_PART_MAIN);
        set_bg(lamp, UIColor::PIN_AMBER_DIM, LV_OPA_COVER);
        t.lamp = lamp;

        lv_obj_t* o = mk_rect(field);
        lv_obj_set_size(o, (int) (t.half * 2.0f), 22);
        lv_obj_set_pos(o, (int) (t.cx - t.half), (int) t.cy - 11);
        lv_obj_set_style_radius(o, 7, LV_PART_MAIN);
        set_grad(o, UIColor::PIN_AMBER, UIColor::PIN_AMBER_DIM);
        set_border(o, UIColor::PIN_WHITE, 1, 120);
        t.obj = o;
    }

    // Slingshots : triangle ferme (base + arete neon) + un flash superpose.
    for (int i = 0; i < N_SLINGS; i++) {
        Sling& s = g_sling[i];
        // 3e sommet : recule derriere la face, du cote oppose a la normale.
        const float bx = (s.x1 + s.x2) * 0.5f - s.nx * 40.0f;
        const float by = (s.y1 + s.y2) * 0.5f - s.ny * 40.0f;
        const float tri[8] = {s.x1, s.y1, bx, by, s.x2, s.y2, s.x1, s.y1};
        mk_poly(field, tri, 4, 13, UIColor::PIN_RAIL, LV_OPA_COVER);
        const float face[4] = {s.x1, s.y1, s.x2, s.y2};
        mk_poly(field, face, 2, 7, UIColor::PIN_MAGENTA_DIM, LV_OPA_COVER);
        s.flash = mk_poly(field, face, 2, 9, UIColor::PIN_MAGENTA, LV_OPA_COVER);
        show(s.flash, false);
    }

    // Bumpers : socle sombre, corps en degrade, capuchon neon, anneau de flash.
    for (int i = 0; i < N_BUMPERS; i++) {
        Bumper& b = g_bump[i];
        const int r = (int) b.r;

        lv_obj_t* base = mk_rect(field);
        lv_obj_set_size(base, (r + 9) * 2, (r + 9) * 2);
        lv_obj_set_pos(base, (int) b.cx - r - 9, (int) b.cy - r - 9);
        lv_obj_set_style_radius(base, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(base, UIColor::PIN_INSERT_OFF, LV_OPA_COVER);
        set_border(base, b.color, 2, 110);

        lv_obj_t* body = mk_rect(field);
        lv_obj_set_size(body, r * 2, r * 2);
        lv_obj_set_pos(body, (int) b.cx - r, (int) b.cy - r);
        lv_obj_set_style_radius(body, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_grad(body, UIColor::PIN_RAIL, UIColor::PIN_VOID);
        set_border(body, b.color, 3, LV_OPA_COVER);

        lv_obj_t* cap = mk_rect(field);
        lv_obj_set_size(cap, r, r);
        lv_obj_set_pos(cap, (int) b.cx - r / 2, (int) b.cy - r / 2);
        lv_obj_set_style_radius(cap, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(cap, b.color, 70);

        lv_obj_t* fl = mk_rect(field);
        lv_obj_set_size(fl, (r + 18) * 2, (r + 18) * 2);
        lv_obj_set_pos(fl, (int) b.cx - r - 18, (int) b.cy - r - 18);
        lv_obj_set_style_radius(fl, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(fl, b.color, 0);
        set_border(fl, b.color, 4, LV_OPA_COVER);
        show(fl, false);
        b.flash = fl;
    }

    // Plunger : la tige se comprime quand on charge (voir render_dynamic()).
    g_plunger_rod = mk_rect(field);
    lv_obj_set_size(g_plunger_rod, 18, 60);
    lv_obj_set_pos(g_plunger_rod, (int) LANE_MID - 9, 1091);
    lv_obj_set_style_radius(g_plunger_rod, 9, LV_PART_MAIN);
    set_grad(g_plunger_rod, UIColor::PIN_CHROME, UIColor::PIN_RAIL);
}

// --- Billes, flippers, tablier, toast (ordre d'empilement important) --------
static void build_actors(lv_obj_t* field) {
    // Billes : ombre portee, corps en degrade (le volume vient de la), reflet.
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        Ball& b = g_balls[i];
        b.shadow = mk_rect(field);
        lv_obj_set_size(b.shadow, (int) (BALL_R * 2.0f) + 4, (int) (BALL_R * 2.0f) + 4);
        lv_obj_set_style_radius(b.shadow, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(b.shadow, UIColor::PIN_BALL_SH, 150);

        b.body = mk_rect(field);
        lv_obj_set_size(b.body, (int) (BALL_R * 2.0f), (int) (BALL_R * 2.0f));
        lv_obj_set_style_radius(b.body, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_grad(b.body, UIColor::PIN_BALL_HI, UIColor::PIN_RAIL);

        b.gloss = mk_rect(field);
        lv_obj_set_size(b.gloss, 9, 9);
        lv_obj_set_style_radius(b.gloss, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(b.gloss, UIColor::PIN_BALL_HI, 235);

        show(b.shadow, false); show(b.body, false); show(b.gloss, false);
    }

    // Flippers : dessines APRES les billes (la bille roule sous le battoir) et
    // AVANT le tablier. Chaque flipper vit dans une boite serree pour que le
    // balayage n'invalide que ~140x130 px.
    for (int s = 0; s < 2; s++) {
        for (int layer = 0; layer < 2; layer++) {
            lv_obj_t* o = lv_line_create(field);
            lv_obj_remove_style_all(o);
            lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
            lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_set_style_line_rounded(o, true, LV_PART_MAIN);
            if (layer == 0) {
                lv_obj_set_style_line_width(o, 24, LV_PART_MAIN);
                lv_obj_set_style_line_color(o, lv_color_hex(UIColor::PIN_FLIP_BASE), LV_PART_MAIN);
                lv_obj_set_style_line_opa(o, LV_OPA_COVER, LV_PART_MAIN);
                g_flip_base[s] = o;
            } else {
                lv_obj_set_style_line_width(o, 10, LV_PART_MAIN);
                lv_obj_set_style_line_color(o, lv_color_hex(UIColor::PIN_FLIP_EDGE), LV_PART_MAIN);
                lv_obj_set_style_line_opa(o, LV_OPA_COVER, LV_PART_MAIN);
                g_flip_edge[s] = o;
            }
            lv_line_set_points(o, g_flip_pts[s], 2);
            lv_obj_set_pos(o, FLIP_BOX_X[s], FLIP_BOX_Y);
            lv_obj_set_size(o, FLIP_BOX_W, FLIP_BOX_H);
        }
        // Premier trace : sans lui le battoir resterait un point tant qu'aucune
        // partie n'a tourne (g_flip_pts est statique, donc a zero au demarrage).
        flipper_points(s, true);

        // Axe chrome du flipper (statique).
        lv_obj_t* pin = mk_rect(field);
        lv_obj_set_size(pin, 20, 20);
        lv_obj_set_pos(pin, (int) g_flip[s].px - 10, (int) FLIP_PY - 10);
        lv_obj_set_style_radius(pin, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        set_bg(pin, UIColor::PIN_CHROME, LV_OPA_COVER);
    }

    // Tablier chrome : cree APRES les billes pour masquer le drain, exactement
    // comme la tole d'une vraie machine avale la bille perdue.
    lv_obj_t* apron = mk_rect(field);
    lv_obj_set_size(apron, FW, FH - APRON_Y);
    lv_obj_set_pos(apron, 0, APRON_Y);
    set_grad(apron, UIColor::PIN_APRON, UIColor::PIN_VOID);
    lv_obj_t* edge = mk_rect(field);
    lv_obj_set_size(edge, FW, 3);
    lv_obj_set_pos(edge, 0, APRON_Y);
    set_bg(edge, UIColor::PIN_CHROME, 200);
    lv_obj_t* name = mk_label(apron, g_ui.f_small, UIColor::PIN_TEXT_DIM);
    lv_obj_align(name, LV_ALIGN_CENTER, 0, 4);
    lv_label_set_text(name, "N E O N   A P R O N");

    // Consigne de tir, affichee seulement quand une bille attend au plunger.
    g_plunger_hint = mk_label(field, g_ui.f_small, UIColor::PIN_CYAN);
    lv_obj_set_width(g_plunger_hint, 300);
    lv_obj_set_style_text_align(g_plunger_hint, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(g_plunger_hint, 210, 1020);
    lv_label_set_text(g_plunger_hint, "Maintiens ici pour armer,\nrelache pour tirer");
    show(g_plunger_hint, false);

    // Banniere de mode, au centre de l'arche (zone la plus lisible du plateau).
    g_toast = mk_label(field, g_ui.f_big, UIColor::PIN_MODE);
    lv_obj_set_width(g_toast, FW);
    lv_obj_set_style_text_align(g_toast, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_set_pos(g_toast, 0, 300);
    show(g_toast, false);
}

// --- Zones tactiles ---------------------------------------------------------
// [AI-CONTEXT] Filles du TERRAIN (pas de la racine) : le calque des menus est
// un frere declare apres le terrain dans le YAML, il reste donc au-dessus et
// les zones ne volent jamais un appui de menu.
static void build_zones(lv_obj_t* field) {
    struct { lv_obj_t** dst; int x, y, w, h; intptr_t tag; } Z[3] = {
        {&g_zone_l,   0, 200, 290, 940, 0},
        {&g_zone_r, 430, 200, 290, 940, 1},
        {&g_zone_p, 290, 940, 140, 200, 2},
    };
    for (int i = 0; i < 3; i++) {
        lv_obj_t* o = lv_obj_create(field);
        lv_obj_remove_style_all(o);
        lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, LV_PART_MAIN);
        lv_obj_set_pos(o, Z[i].x, Z[i].y);
        lv_obj_set_size(o, Z[i].w, Z[i].h);
        lv_obj_add_event_cb(o, zone_event_cb, LV_EVENT_PRESSED,     (void*) Z[i].tag);
        lv_obj_add_event_cb(o, zone_event_cb, LV_EVENT_RELEASED,    (void*) Z[i].tag);
        lv_obj_add_event_cb(o, zone_event_cb, LV_EVENT_PRESS_LOST,  (void*) Z[i].tag);
        *Z[i].dst = o;
    }
    show(g_zone_p, false);
}

// --- Fronton (DMD) ----------------------------------------------------------
static void build_hud(lv_obj_t* hud) {
    g_hud_ball = mk_label(hud, g_ui.f_led, UIColor::PIN_TEXT_DIM);
    lv_obj_align(g_hud_ball, LV_ALIGN_TOP_LEFT, 24, 12);

    for (int i = 0; i < BALLS_PER_GAME; i++) {
        lv_obj_t* d = mk_rect(hud);
        lv_obj_set_size(d, 20, 20);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_align(d, LV_ALIGN_TOP_LEFT, 24 + i * 28, 48);
        set_bg(d, UIColor::PIN_AMBER, LV_OPA_COVER);
        g_hud_dot[i] = d;
    }

    g_hud_score = mk_label(hud, g_ui.f_score, UIColor::PIN_AMBER);
    lv_obj_set_style_text_align(g_hud_score, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(g_hud_score, LV_ALIGN_TOP_RIGHT, -24, 4);

    g_hud_best = mk_label(hud, g_ui.f_led, UIColor::PIN_TEXT_DIM);
    lv_obj_align(g_hud_best, LV_ALIGN_TOP_LEFT, 24, 86);

    g_hud_badge = mk_label(hud, g_ui.f_mid, UIColor::PIN_MODE);
    lv_obj_set_style_text_align(g_hud_badge, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    lv_obj_align(g_hud_badge, LV_ALIGN_TOP_RIGHT, -24, 84);

    g_hud_tilt = mk_label(hud, g_ui.f_big, UIColor::PIN_DANGER);
    lv_obj_align(g_hud_tilt, LV_ALIGN_TOP_MID, 0, 40);
    lv_label_set_text(g_hud_tilt, "T I L T");
    show(g_hud_tilt, false);

    // Jauge de puissance du plunger : dans le fronton, jamais sur la table
    // (aucune place dans le couloir sans recouvrir la bille).
    g_pwr_bg = mk_rect(hud);
    lv_obj_set_size(g_pwr_bg, 300, 12);
    lv_obj_align(g_pwr_bg, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_set_style_radius(g_pwr_bg, 6, LV_PART_MAIN);
    set_bg(g_pwr_bg, UIColor::PIN_INSERT_OFF, LV_OPA_COVER);
    set_border(g_pwr_bg, UIColor::PIN_CYAN, 1, 120);
    g_pwr_fill = mk_rect(g_pwr_bg);
    lv_obj_set_size(g_pwr_fill, 0, 8);
    lv_obj_align(g_pwr_fill, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_radius(g_pwr_fill, 4, LV_PART_MAIN);
    set_bg(g_pwr_fill, UIColor::PIN_CYAN, LV_OPA_COVER);
    show(g_pwr_bg, false);

    // Filet neon en pied de fronton : separe le DMD de la table.
    lv_obj_t* rule = mk_rect(hud);
    lv_obj_set_size(rule, SCR_W, 3);
    lv_obj_align(rule, LV_ALIGN_BOTTOM_MID, 0, 0);
    set_bg(rule, UIColor::PIN_CYAN, 200);

    lv_obj_add_flag(hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(hud, hud_event_cb, LV_EVENT_CLICKED, nullptr);
}

// --- Calque des menus -------------------------------------------------------
static void build_panel(lv_obj_t* panel) {
    g_p_title = mk_label(panel, g_ui.f_big, UIColor::PIN_AMBER);
    lv_obj_set_width(g_p_title, SCR_W);
    lv_obj_set_style_text_align(g_p_title, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g_p_title, LV_ALIGN_TOP_MID, 0, 96);

    g_p_sub = mk_label(panel, g_ui.f_small, UIColor::PIN_TEXT_DIM);
    lv_obj_set_width(g_p_sub, SCR_W - 80);
    lv_obj_set_style_text_align(g_p_sub, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g_p_sub, LV_ALIGN_TOP_MID, 0, 156);

    g_p_body = mk_label(panel, g_ui.f_led, UIColor::PIN_WHITE);
    lv_obj_set_width(g_p_body, SCR_W - 80);
    lv_obj_set_style_text_align(g_p_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g_p_body, LV_ALIGN_TOP_MID, 0, 210);

    g_p_foot = mk_label(panel, g_ui.f_small, UIColor::PIN_TEXT_DIM);
    lv_obj_set_width(g_p_foot, SCR_W - 60);
    lv_obj_set_style_text_align(g_p_foot, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g_p_foot, LV_ALIGN_BOTTOM_MID, 0, -28);

    // Pictogramme d'orientation : paysage barre -> portrait allume.
    g_rot_land = mk_rect(panel);
    lv_obj_set_size(g_rot_land, 104, 66);
    lv_obj_align(g_rot_land, LV_ALIGN_TOP_MID, -104, 214);
    lv_obj_set_style_radius(g_rot_land, 8, LV_PART_MAIN);
    set_bg(g_rot_land, UIColor::PIN_VOID, LV_OPA_COVER);
    set_border(g_rot_land, UIColor::PIN_TEXT_DIM, 3, 150);

    g_rot_arrow = mk_label(panel, g_ui.f_big, UIColor::PIN_CYAN);
    lv_obj_align(g_rot_arrow, LV_ALIGN_TOP_MID, 0, 222);
    lv_label_set_text(g_rot_arrow, ">");

    g_rot_port = mk_rect(panel);
    lv_obj_set_size(g_rot_port, 66, 104);
    lv_obj_align(g_rot_port, LV_ALIGN_TOP_MID, 100, 196);
    lv_obj_set_style_radius(g_rot_port, 8, LV_PART_MAIN);
    set_bg(g_rot_port, UIColor::PIN_VOID, LV_OPA_COVER);
    set_border(g_rot_port, UIColor::PIN_CYAN, 3, LV_OPA_COVER);

    for (int i = 0; i < N_SLOTS; i++) {
        lv_obj_t* s = mk_rect(panel);
        lv_obj_add_flag(s, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(s, 14, LV_PART_MAIN);
        set_bg(s, UIColor::PIN_FELT_HI, LV_OPA_COVER);
        // Retour tactile : le fond s'eclaircit tant que le doigt est pose.
        // Casts explicites : combiner lv_part_t et lv_state_t directement est
        // deprecie en C++20 (-Wdeprecated-enum-enum-conversion).
        lv_obj_set_style_bg_color(s, lv_color_hex(UIColor::PIN_RAIL),
                                  (lv_style_selector_t) LV_PART_MAIN |
                                  (lv_style_selector_t) LV_STATE_PRESSED);
        lv_obj_add_event_cb(s, slot_event_cb, LV_EVENT_CLICKED, (void*) (intptr_t) i);
        g_slot[i]   = s;
        g_slot_t[i] = mk_label(s, g_ui.f_mid, UIColor::PIN_WHITE);
        g_slot_d[i] = mk_label(s, g_ui.f_small, UIColor::PIN_TEXT_DIM);
        show(s, false);
    }
}

static void build_ui() {
    if (g_built) return;
    g_pts_n = 0;
    build_table_data();
    build_art(g_ui.field);
    build_rails(g_ui.field);
    build_pieces(g_ui.field);
    build_actors(g_ui.field);
    build_zones(g_ui.field);
    build_hud(g_ui.hud);
    build_panel(g_ui.panel);
    g_built = true;
}

// ===========================================================================
// 14. Mise en page des slots de menu
// ===========================================================================

static void slot(int i, const char* title, const char* desc, uint32_t col) {
    if (i < 0 || i >= N_SLOTS) return;
    lv_obj_set_size(g_slot[i], 620, 84);
    lv_obj_align(g_slot[i], LV_ALIGN_TOP_MID, 0, 400 + i * 96);
    lv_obj_align(g_slot_t[i], LV_ALIGN_LEFT_MID, 26, desc && desc[0] ? -15 : 0);
    lv_obj_align(g_slot_d[i], LV_ALIGN_LEFT_MID, 26, 18);
    lv_obj_set_style_text_color(g_slot_t[i], lv_color_hex(col), LV_PART_MAIN);
    set_text_if(g_slot_t[i], title);
    set_text_if(g_slot_d[i], desc ? desc : "");
    set_border(g_slot[i], col, 2, 120);
    show(g_slot[i], true);
}

static void slots_hide_from(int n) {
    for (int i = n; i < N_SLOTS; i++) show(g_slot[i], false);
}

static void panel_on(bool v) {
    show(g_ui.panel, v);
    if (v) lv_obj_move_foreground(g_ui.panel);
}

// Le pictogramme d'orientation occupe la bande y = 196..316 du panneau. Les
// ecrans qui l'affichent doivent donc pousser leur corps de texte SOUS lui —
// d'ou le parametre `body_y`, unique endroit ou cette contrainte est exprimee.
static void rot_hint(bool v, int body_y) {
    show(g_rot_land, v);
    show(g_rot_port, v);
    show(g_rot_arrow, v);
    lv_obj_align(g_p_body, LV_ALIGN_TOP_MID, 0, body_y);
}

// ===========================================================================
// 15. Ecrans
// ===========================================================================

static const char* NUDGE_NAMES[5] = {"Tres douce", "Douce", "Normale", "Vive", "Tres vive"};

static void go_hub() {
    g_state = ST_HUB;
    panel_on(true);
    rot_hint(true, 336);   // sous le pictogramme d'orientation

    static char sub[128];
    snprintf(sub, sizeof(sub), "%lu partie(s) - %lu multiball(s) - %lu tilt(s)",
             (unsigned long) g_save.games, (unsigned long) g_save.multiballs,
             (unsigned long) g_save.tilts);

    static char best[64];
    if (best_score() > 0) {
        char sc[24]; fmt_score(sc, sizeof(sc), best_score());
        snprintf(best, sizeof(best), "Record : %s", sc);
    } else {
        snprintf(best, sizeof(best), "Aucun score enregistre");
    }

    set_text_if(g_p_title, "NEON APRON");
    set_text_if(g_p_sub, sub);
    set_text_if(g_p_body, "Tournez la tablette a la verticale");
    set_text_if(g_p_foot,
        "Zone gauche / zone droite = flippers (maintien). Bas du centre = lanceur.\n"
        "Secouez la tablette pour pousser la bille — trois abus de suite et c'est TILT.");
    slot(0, "Jouer", "3 billes - lanceur en bas de l'ecran", UIColor::PIN_AMBER);
    slot(1, "Classement", best, UIColor::PIN_CYAN);
    slot(2, "Reglages", "Nudge, sens de l'ecran, calibration", UIColor::PIN_MAGENTA);
    slot(3, "Quitter", "Retour au tableau de bord (paysage)", UIColor::PIN_TEXT_DIM);
    slots_hide_from(4);
    tick_period_sync();
}

static void go_scores() {
    g_state = ST_SCORES;
    panel_on(true);
    rot_hint(false, 226);  // 10 lignes de classement : il faut toute la hauteur

    static char body[512];
    size_t o = 0;
    body[0] = '\0';
    bool any = false;
    for (int i = 0; i < PINBALL_NSCORES; i++) {
        if (g_save.top[i].score == 0) continue;
        any = true;
        char sc[24]; fmt_score(sc, sizeof(sc), g_save.top[i].score);
        int w = snprintf(body + o, sizeof(body) - o, "%2d.  %11s   %ub%s%s\n",
                         i + 1, sc, (unsigned) g_save.top[i].ball_reached,
                         g_save.top[i].multiballs ? "  MB" : "",
                         g_save.top[i].tilted ? "  tilt" : "");
        if (w <= 0 || (size_t) w >= sizeof(body) - o) break;
        o += (size_t) w;
    }
    if (!any) snprintf(body, sizeof(body), "Aucun score pour l'instant.\nLance une partie !");

    static char sub[96];
    snprintf(sub, sizeof(sub), "Cumul carriere : %lu points",
             (unsigned long) g_save.total_score);

    set_text_if(g_p_title, "CLASSEMENT");
    set_text_if(g_p_sub, sub);
    set_text_if(g_p_body, body);
    set_text_if(g_p_foot, "b = billes jouees, MB = multiball declenche.");
    slot(0, "Retour", "", UIColor::PIN_TEXT_DIM);
    slots_hide_from(1);
    // Le classement est long : on remonte les slots sous le texte.
    lv_obj_align(g_slot[0], LV_ALIGN_BOTTOM_MID, 0, -110);
    tick_period_sync();
}

static void go_settings() {
    g_state = ST_SETTINGS;
    panel_on(true);
    rot_hint(false, 300);

    static char t0[72], t1[72], t2[72];
    snprintf(t0, sizeof(t0), "Sensibilite du nudge : %s", NUDGE_NAMES[g_save.nudge_sens]);
    snprintf(t1, sizeof(t1), "Sens du nudge : %s", g_save.invert_nudge ? "inverse" : "normal");
    snprintf(t2, sizeof(t2), "Orientation : %s", g_save.flip_screen ? "retournee" : "normale");

    set_text_if(g_p_title, "REGLAGES");
    set_text_if(g_p_sub, "Tout est enregistre et survit au redemarrage.");
    set_text_if(g_p_body, "");
    set_text_if(g_p_foot,
        "Calibre a plat AVANT de jouer : le nudge mesure l'ecart avec cette reference,\n"
        "pas l'inclinaison absolue. Poser la tablette, puis appuyer.");
    slot(0, t0, "Force de la secousse necessaire", UIColor::PIN_CYAN);
    slot(1, t1, "Si la bille part du mauvais cote", UIColor::PIN_CYAN);
    slot(2, t2, "Si l'ecran est a l'envers dans vos mains", UIColor::PIN_MAGENTA);
    slot(3, "Calibrer a plat", "Poser la tablette puis appuyer", UIColor::PIN_AMBER);
    slot(4, "Retour", "", UIColor::PIN_TEXT_DIM);
    slots_hide_from(5);
    tick_period_sync();
}

static void go_pause() {
    g_state = ST_PAUSED;
    panel_on(true);
    rot_hint(false, 300);

    static char sub[96];
    char sc[24]; fmt_score(sc, sizeof(sc), g_score);
    snprintf(sub, sizeof(sub), "Score %s - bille %d / %d", sc, g_ball_num, g_balls_total);

    set_text_if(g_p_title, "PAUSE");
    set_text_if(g_p_sub, sub);
    set_text_if(g_p_body, "");
    set_text_if(g_p_foot, "La partie reprend exactement ou elle s'est arretee.");
    slot(0, "Reprendre", "", UIColor::PIN_AMBER);
    slot(1, "Recalibrer a plat", "Poser la tablette puis appuyer", UIColor::PIN_CYAN);
    slot(2, "Abandonner", "La partie est enregistree telle quelle", UIColor::PIN_MAGENTA);
    slot(3, "Quitter le flipper", "Retour au tableau de bord", UIColor::PIN_TEXT_DIM);
    slots_hide_from(4);
    tick_period_sync();
}

// Cloture la partie : classement, statistiques carriere, ecriture NVS.
static void end_game() {
    g_save.games++;
    g_save.total_score += g_score;
    if (g_ball_score > g_save.best_ball) g_save.best_ball = g_ball_score;
    const int rank = scores_insert(g_score, g_ball_num, g_game_multiballs, g_game_tilted);
    persist_save();

    g_state = ST_GAMEOVER;
    panel_on(true);
    rot_hint(false, 300);
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        g_balls[i].active = false;
        show(g_balls[i].shadow, false);
        show(g_balls[i].body, false);
        show(g_balls[i].gloss, false);
    }
    show(g_zone_p, false);
    show(g_plunger_hint, false);
    show(g_toast, false);
    sfx_gameover();

    static char sub[96];
    char sc[24]; fmt_score(sc, sizeof(sc), g_score);
    snprintf(sub, sizeof(sub), "Score final : %s", sc);

    static char body[160];
    if (rank == 0)      snprintf(body, sizeof(body), "NOUVEAU RECORD !");
    else if (rank > 0)  snprintf(body, sizeof(body), "%de au classement", rank + 1);
    else                snprintf(body, sizeof(body), "Hors du top %d", PINBALL_NSCORES);

    set_text_if(g_p_title, "FIN DE PARTIE");
    set_text_if(g_p_sub, sub);
    set_text_if(g_p_body, body);
    set_text_if(g_p_foot, g_game_tilted ? "Partie marquee TILT." : "");
    slot(0, "Rejouer", "Nouvelle partie, 3 billes", UIColor::PIN_AMBER);
    slot(1, "Classement", "", UIColor::PIN_CYAN);
    slot(2, "Hub", "", UIColor::PIN_TEXT_DIM);
    slots_hide_from(3);
    tick_period_sync();
}

static void go_gameover() { end_game(); }

// ===========================================================================
// 16. Cycle de vie d'une partie
// ===========================================================================

// Pose la bille au repos dans le couloir du plunger et arme le tir.
static void serve_ball() {
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        g_balls[i].active = false;
        g_balls[i].in_plunger = false;
    }
    Ball& b = g_balls[0];
    b.x = LANE_MID; b.y = BALL_REST_Y; b.vx = 0.0f; b.vy = 0.0f;
    b.active = true; b.in_plunger = true;
    // Le cache de rendu porte la position de la bille PRECEDENTE : sans cette
    // invalidation, une bille servie au meme pixel entier ne serait jamais
    // reaffichee (render_balls() saute les positions inchangees).
    b.dx = b.dy = -9999;
    g_active_balls = 1;
    g_multiplier = 1;
    g_mb_active = false;
    g_ball_score = 0;
    g_plunger_charge = 0.0f;
    g_plunger_held = false;
    g_autolaunch_at = lv_tick_get() + AUTOLAUNCH_MS;
    g_serve_at = 0;
    g_tilted = false;
    g_tilt_hits = 0;
    // Skill shot : une lane est tiree au sort, elle seule paie le gros lot.
    g_skill_lane = (int) (rnd() % (uint32_t) N_ROLLOVERS);
    g_skill_until = 0;
    for (int i = 0; i < N_ROLLOVERS; i++) g_roll[i].lit = (i == g_skill_lane);
    show(g_zone_p, true);
    show(g_plunger_hint, true);
}

static void new_game() {
    g_score = 0;
    g_ball_num = 1;
    g_balls_total = BALLS_PER_GAME;
    g_game_multiballs = 0;
    g_game_tilted = false;
    g_bonus1 = g_bonus2 = false;
    g_banks_done = 0;
    g_frenzy_until = 0;
    for (int i = 0; i < N_TARGETS; i++) {
        g_targ[i].down = false;
        show(g_targ[i].obj, true);
    }
    g_c_score = 0xFFFFFFFFu; g_c_ball = -1; g_c_dots = -1; g_c_badge[0] = '\0';
    serve_ball();
    g_state = ST_PLAYING;
    panel_on(false);
    tick_period_sync();
}

static void launch_ball(Ball& b, float power) {
    b.in_plunger = false;
    b.vy = -(PLUNGER_MIN + clampf(power, 0.0f, 1.0f) * (PLUNGER_MAX - PLUNGER_MIN));
    b.vx = rndf() * 12.0f;
    g_plunger_charge = 0.0f;
    g_plunger_held = false;
    g_skill_until = lv_tick_get() + 6000;
    show(g_zone_p, false);
    show(g_plunger_hint, false);
    sfx_launch();
}

// Une bille est tombee. S'il en reste en jeu on continue (multiball) ; sinon on
// passe a la bille suivante, ou on termine la partie.
static void ball_drained(Ball& b) {
    b.active = false;
    show(b.shadow, false); show(b.body, false); show(b.gloss, false);
    g_active_balls--;
    if (g_active_balls > 0) {
        if (g_active_balls == 1 && g_mb_active) {
            g_mb_active = false;
            g_multiplier = 1;
            toast("MULTIBALL TERMINE", 1400);
        }
        return;
    }
    sfx_drain();
    g_mb_active = false;
    g_multiplier = 1;
    show(g_zone_p, false);
    if (g_ball_num >= g_balls_total) {
        end_game();
        return;
    }
    g_ball_num++;
    g_serve_at = lv_tick_get() + 900;   // court temps mort avant la relance
    toast("BILLE PERDUE", 800);
}

// ===========================================================================
// 17. Nudge et TILT
// ===========================================================================

// [AI-CONTEXT] Mapping des axes en portrait — la demonstration complete est en
// tete de fichier. Ici on n'ecrit QUE la conclusion, une seule fois.
static inline void nudge_axes(float& sx, float& sy) {
    sx = g_raw_x - g_save.cal_x / 1000.0f;
    sy = g_raw_y - g_save.cal_y / 1000.0f;
    if (g_save.flip_screen) { sx = -sx; sy = -sy; }
}

static void nudge_update(uint32_t now) {
    float sx, sy;
    nudge_axes(sx, sy);

    // Passe-haut : une inclinaison entretenue (le joueur tient la tablette de
    // travers) glisse dans la composante lente et ne declenche rien. Seule une
    // secousse breve — le vrai coup de hanche — produit du jerk.
    g_slow_x += (sx - g_slow_x) * NUDGE_HP;
    g_slow_y += (sy - g_slow_y) * NUDGE_HP;
    const float jx = sx - g_slow_x, jy = sy - g_slow_y;

    // Compteur de TILT : un cran s'efface toutes les TILT_DECAY_MS.
    if (g_tilt_hits > 0 && now >= g_tilt_decay_at) {
        g_tilt_hits--;
        g_tilt_decay_at = now + TILT_DECAY_MS;
    }
    if (g_tilted && now >= g_tilt_until) g_tilted = false;

    if (now < g_nudge_ok_at) return;
    const float thr = 0.42f - 0.06f * (float) g_save.nudge_sens;   // 0.42 .. 0.18 g
    const float mag = sqrtf(jx * jx + jy * jy);
    if (mag < thr) return;

    g_nudge_ok_at = now + NUDGE_COOLDOWN_MS;

    // La table bouge, la bille garde son inertie : vue de la table, elle part
    // dans le sens OPPOSE a la secousse. `invert_nudge` existe parce que le
    // signe depend de la facon dont le joueur tient la tablette.
    const float s = g_save.invert_nudge ? 1.0f : -1.0f;
    const float gain = NUDGE_GAIN * s;
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        Ball& b = g_balls[i];
        if (!b.active || b.in_plunger) continue;
        b.vx += jx * gain;
        b.vy += jy * gain * 0.55f;   // moins d'effet vertical : plus lisible
    }

    if (g_tilt_hits == 0) g_tilt_decay_at = now + TILT_DECAY_MS;
    g_tilt_hits++;
    if (g_tilt_hits >= TILT_HITS && !g_tilted) {
        g_tilted = true;
        g_game_tilted = true;
        g_tilt_until = now + TILT_PENALTY_MS;
        g_tilt_hits = 0;
        g_save.tilts++;
        toast("TILT", 1600);
        sfx_tilt();
    }
}

// ===========================================================================
// 18. Boucle physique
// ===========================================================================

static void step_flippers(float dt) {
    for (int s = 0; s < 2; s++) {
        Flipper& f = g_flip[s];
        f.target = (f.pressed && !g_tilted) ? f.active : f.rest;
        const float d = f.target - f.angle;
        const float step = FLIP_SPEED * dt;
        if (d > step)       f.angle += step;
        else if (d < -step) f.angle -= step;
        else                f.angle = f.target;
    }
}

// Collision bille / flipper : segment mobile. L'impulsion vient de la vitesse
// lineaire du bras au point de contact (omega x r), pas d'un simple rebond —
// c'est ce qui fait la difference entre « la bille repart » et « la bille est
// frappee ».
static void collide_flipper(Ball& b, const Flipper& f, float dt) {
    const float ex = f.px + cosf(f.angle) * FLIP_LEN;
    const float ey = f.py + sinf(f.angle) * FLIP_LEN;

    float cx, cy;
    const float d = point_seg_dist(b.x, b.y, f.px, f.py, ex, ey, cx, cy);
    const float rad = BALL_R + FLIP_THICK;
    if (d >= rad || d < 0.0001f) return;

    const float nx = (b.x - cx) / d, ny = (b.y - cy) / d;
    b.x += nx * (rad - d);
    b.y += ny * (rad - d);

    // Vitesse angulaire effective sur ce pas, puis vitesse au point de contact.
    float omega = 0.0f;
    if (dt > 0.0001f) {
        const float diff = f.target - f.angle;
        const float step = FLIP_SPEED * dt;
        omega = (fabsf(diff) > step ? (diff > 0 ? step : -step) : diff) / dt;
    }
    const float rx = cx - f.px, ry = cy - f.py;
    const float fvx = -ry * omega, fvy = rx * omega;

    const float rel = (b.vx - fvx) * nx + (b.vy - fvy) * ny;
    if (rel < 0.0f) {
        b.vx -= 1.45f * rel * nx;
        b.vy -= 1.45f * rel * ny;
    }
    if (f.pressed && !g_tilted) {
        b.vx += fvx * FLIP_PUNCH;
        b.vy += fvy * FLIP_PUNCH;
    }
}

static void physics_step(float dt, uint32_t now) {
    // Le bras avance a CHAQUE sous-pas, pas une fois par tick : sinon le battoir
    // resterait fige pendant les 4 sous-pas alors que collide_flipper() annonce
    // une vitesse angulaire non nulle, et une bille rapide pourrait etre frappee
    // par un flipper qui n'a pas bouge.
    step_flippers(dt);

    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        Ball& b = g_balls[i];
        if (!b.active || b.in_plunger) continue;

        b.vy += GRAVITY * dt;
        b.x += b.vx * dt;
        b.y += b.vy * dt;
        b.vx *= FRICTION;
        b.vy *= FRICTION;

        // --- Rails et guides ------------------------------------------------
        for (int w = 0; w < N_WALLS; w++) {
            const Seg& s = WALLS[w];
            if (s.one_way && b.vy <= 0.0f) continue;   // clapet : ouvert en montee
            collide_capsule(b, s.x1, s.y1, s.x2, s.y2, BALL_R + RAIL_HALF, s.rest);
        }
        collide_arch(b);

        // --- Flippers -------------------------------------------------------
        collide_flipper(b, g_flip[0], dt);
        collide_flipper(b, g_flip[1], dt);

        // --- Bumpers --------------------------------------------------------
        for (int k = 0; k < N_BUMPERS; k++) {
            Bumper& bm = g_bump[k];
            const float dx = b.x - bm.cx, dy = b.y - bm.cy;
            const float dist = sqrtf(dx * dx + dy * dy);
            const float mind = BALL_R + bm.r;
            if (dist >= mind || dist < 0.0001f) continue;
            const float nx = dx / dist, ny = dy / dist;
            b.x = bm.cx + nx * mind;
            b.y = bm.cy + ny * mind;
            b.vx = nx * BUMPER_KICK;
            b.vy = ny * BUMPER_KICK;
            bm.flash_until = now + 130;
            const bool frenzy = now < g_frenzy_until;
            add_score(frenzy ? SC_BUMPER * 3 : SC_BUMPER);
            if (g_mb_active) add_score(SC_JACKPOT / 10);
            sfx_bumper();
        }

        // --- Slingshots -----------------------------------------------------
        for (int k = 0; k < N_SLINGS; k++) {
            Sling& sl = g_sling[k];
            float cx, cy;
            const float d = point_seg_dist(b.x, b.y, sl.x1, sl.y1, sl.x2, sl.y2, cx, cy);
            const float rad = BALL_R + 8.0f;
            if (d >= rad || d < 0.0001f) continue;
            b.x = cx + sl.nx * rad;
            b.y = cy + sl.ny * rad;
            b.vx = sl.nx * SLING_KICK + rndf() * 60.0f;
            b.vy = sl.ny * SLING_KICK;
            sl.flash_until = now + 120;
            add_score(SC_SLING);
            sfx_sling();
        }

        // --- Cibles drop ----------------------------------------------------
        for (int k = 0; k < N_TARGETS; k++) {
            Target& t = g_targ[k];
            if (t.down) continue;
            float cx, cy;
            const float d = point_seg_dist(b.x, b.y, t.cx - t.half, t.cy,
                                           t.cx + t.half, t.cy, cx, cy);
            const float rad = BALL_R + 11.0f;
            if (d >= rad || d < 0.0001f) continue;
            const float nx = (b.x - cx) / d, ny = (b.y - cy) / d;
            b.x += nx * (rad - d);
            b.y += ny * (rad - d);
            b.vx += nx * TARGET_KICK;
            b.vy += ny * TARGET_KICK;
            t.down = true;
            show(t.obj, false);
            add_score(SC_TARGET);
            sfx_target();
            int left = 0;
            for (int j = 0; j < N_TARGETS; j++) if (!g_targ[j].down) left++;
            if (left == 0) bank_complete();
        }

        // --- Rollovers (declencheurs, aucune collision) ----------------------
        for (int k = 0; k < N_ROLLOVERS; k++) {
            Rollover& r = g_roll[k];
            if (!r.lit) continue;
            const float dx = b.x - r.cx, dy = b.y - r.cy;
            if (dx * dx + dy * dy > (BALL_R + r.r) * (BALL_R + r.r)) continue;
            r.lit = false;
            if (k == g_skill_lane && now < g_skill_until) {
                add_score(SC_SKILL);
                toast("SKILL SHOT", 1600);
                g_skill_lane = -1;
            } else {
                add_score(SC_LANE);
            }
            // Les 3 lanes eteintes se rallument ensemble : la voie reste vivante.
            int off = 0;
            for (int j = 0; j < N_ROLLOVERS; j++) if (!g_roll[j].lit) off++;
            if (off == N_ROLLOVERS) {
                for (int j = 0; j < N_ROLLOVERS; j++) g_roll[j].lit = true;
                add_score(SC_LANE * 4);
            }
        }

        // --- Vitesse maximale ------------------------------------------------
        const float sp2 = b.vx * b.vx + b.vy * b.vy;
        if (sp2 > MAX_SPEED * MAX_SPEED) {
            const float k = MAX_SPEED / sqrtf(sp2);
            b.vx *= k; b.vy *= k;
        }
    }
}

// ===========================================================================
// 19. Rendu
// ===========================================================================

// Recalcule les 2 points d'un flipper et redemande le trace. `force` sert a la
// construction (les tableaux statiques valent 0,0 : sans ce premier calcul le
// battoir serait un point au coin de sa boite jusqu'au premier tick de partie).
static void flipper_points(int s, bool force) {
    Flipper& f = g_flip[s];
    // Seuil : sous ~0,6 degre le trait ne bougerait d'aucun pixel utile.
    if (!force && fabsf(f.angle - f.draw_angle) < 0.01f) return;
    f.draw_angle = f.angle;
    const float ex = f.px + cosf(f.angle) * FLIP_LEN;
    const float ey = f.py + sinf(f.angle) * FLIP_LEN;
    g_flip_pts[s][0].x = (lv_value_precise_t) ((int) f.px - FLIP_BOX_X[s]);
    g_flip_pts[s][0].y = (lv_value_precise_t) ((int) f.py - FLIP_BOX_Y);
    g_flip_pts[s][1].x = (lv_value_precise_t) ((int) ex - FLIP_BOX_X[s]);
    g_flip_pts[s][1].y = (lv_value_precise_t) ((int) ey - FLIP_BOX_Y);
    // Les deux traits partagent le meme tableau : lv_line n'en garde que
    // l'adresse, il faut donc redemander le trace sur chacun.
    lv_line_set_points(g_flip_base[s], g_flip_pts[s], 2);
    lv_line_set_points(g_flip_edge[s], g_flip_pts[s], 2);
}

static void render_flippers() {
    flipper_points(0, false);
    flipper_points(1, false);
}

static void render_balls() {
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        Ball& b = g_balls[i];
        if (!b.active) continue;
        const int x = (int) b.x, y = (int) b.y;
        if (x == b.dx && y == b.dy) continue;
        b.dx = x; b.dy = y;
        const int r = (int) BALL_R;
        lv_obj_set_pos(b.shadow, x - r - 1, y - r + 3);
        lv_obj_set_pos(b.body,   x - r,     y - r);
        lv_obj_set_pos(b.gloss,  x - 7,     y - 8);
        show(b.shadow, true); show(b.body, true); show(b.gloss, true);
    }
}

// [AI-WARNING] Les lampes (rollovers, inserts) sont repeintes UNIQUEMENT quand
// leur etat change. lv_obj_set_style_*() invalide l'objet a chaque appel, meme
// si la valeur ecrite est identique : sans ces caches, ce sont 12 invalidations
// gratuites par frame, soit 600 par seconde pour des pastilles immobiles.
static int8_t g_c_roll[N_ROLLOVERS] = {-1, -1, -1};
static int8_t g_c_ins[3] = {-1, -1, -1};

static void render_effects(uint32_t now) {
    for (int i = 0; i < N_BUMPERS; i++) {
        show(g_bump[i].flash, now < g_bump[i].flash_until);
    }
    for (int i = 0; i < N_SLINGS; i++) {
        show(g_sling[i].flash, now < g_sling[i].flash_until);
    }
    for (int i = 0; i < N_ROLLOVERS; i++) {
        const int8_t lit = g_roll[i].lit ? 1 : 0;
        if (lit == g_c_roll[i]) continue;
        g_c_roll[i] = lit;
        lv_obj_set_style_bg_opa(g_roll[i].obj, lit ? (lv_opa_t) 150 : (lv_opa_t) 20, LV_PART_MAIN);
    }
    if (g_toast_until && now >= g_toast_until) {
        show(g_toast, false);
        g_toast_until = 0;
    }
    // Inserts : FRENZY / MULTI / SKILL s'allument avec leur mode.
    const bool on[3] = {now < g_frenzy_until, g_mb_active,
                        g_skill_lane >= 0 && now < g_skill_until};
    for (int i = 0; i < 3; i++) {
        const int8_t v = on[i] ? 1 : 0;
        if (v == g_c_ins[i]) continue;
        g_c_ins[i] = v;
        lv_obj_set_style_bg_opa(g_insert[i], v ? (lv_opa_t) 110 : LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_bg_color(g_insert[i],
            lv_color_hex(v ? UIColor::PIN_MODE : UIColor::PIN_INSERT_OFF), LV_PART_MAIN);
        lv_obj_set_style_text_opa(g_insert_l[i], v ? LV_OPA_COVER : (lv_opa_t) 130, LV_PART_MAIN);
    }
}

// La bille en attente est deja dessinee par render_balls() (elle est `active`) :
// ici on ne s'occupe que du ressort, qui se comprime avec la charge.
static void render_plunger() {
    static int c_len = -1;
    const int len = 60 - (int) (g_plunger_charge * 26.0f);
    if (len == c_len) return;
    c_len = len;
    lv_obj_set_size(g_plunger_rod, 18, len);
    lv_obj_set_pos(g_plunger_rod, (int) LANE_MID - 9, 1091 + (60 - len));
}

static void hud_sync(uint32_t now) {
    if (g_score != g_c_score) {
        g_c_score = g_score;
        static char sc[24];
        fmt_score(sc, sizeof(sc), g_score);
        set_text_if(g_hud_score, sc);
    }
    if (g_ball_num != g_c_ball) {
        g_c_ball = g_ball_num;
        static char bl[32];
        snprintf(bl, sizeof(bl), "BILLE %d / %d", g_ball_num, g_balls_total);
        set_text_if(g_hud_ball, bl);
        static char be[32];
        char sc[24]; fmt_score(sc, sizeof(sc), best_score());
        snprintf(be, sizeof(be), "RECORD %s", sc);
        set_text_if(g_hud_best, be);
    }
    // Pastilles : une par bille restante (les billes bonus au-dela de 3 ne sont
    // pas representees, le libelle « BILLE x / y » les annonce deja).
    const int left = g_balls_total - g_ball_num + 1;
    if (left != g_c_dots) {
        g_c_dots = left;
        for (int i = 0; i < BALLS_PER_GAME; i++) {
            set_bg(g_hud_dot[i], i < left ? UIColor::PIN_AMBER : UIColor::PIN_INSERT_OFF,
                   LV_OPA_COVER);
        }
    }
    if (g_tilted != g_c_tilt) {
        g_c_tilt = g_tilted;
        show(g_hud_tilt, g_tilted);
    }

    static char badge[24];
    badge[0] = '\0';
    if (g_mb_active)            snprintf(badge, sizeof(badge), "MULTIBALL x%d", g_multiplier);
    else if (now < g_frenzy_until)
        snprintf(badge, sizeof(badge), "FRENZY %lu", (unsigned long) ((g_frenzy_until - now) / 1000 + 1));
    if (strcmp(badge, g_c_badge) != 0) {
        snprintf(g_c_badge, sizeof(g_c_badge), "%s", badge);
        set_text_if(g_hud_badge, badge);
    }

    const bool charging = g_plunger_held && g_plunger_charge > 0.01f;
    show(g_pwr_bg, charging);
    if (charging) lv_obj_set_size(g_pwr_fill, (int) (296.0f * g_plunger_charge), 8);
}

// ===========================================================================
// 20. Tick
// ===========================================================================

// Le timer tourne vite en partie et lentement dans les menus : un hub statique
// n'a rien a animer (meme motif que go_game.cpp / lode_game.cpp).
static void tick_period_sync() {
    if (!g_timer) return;
    const uint32_t want = (g_state == ST_PLAYING) ? TICK_MS : TICK_MENU_MS;
    if (want == g_tick_period) return;
    g_tick_period = want;
    lv_timer_set_period(g_timer, want);
}

static void tick_cb(lv_timer_t*) {
    if (g_state != ST_PLAYING) return;
    const uint32_t now = lv_tick_get();

    // Relance apres une bille perdue.
    if (g_serve_at && now >= g_serve_at) {
        serve_ball();
        return;
    }
    if (g_serve_at) return;

    nudge_update(now);

    // Charge du plunger + securite anti-blocage.
    bool waiting = false;
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        if (g_balls[i].active && g_balls[i].in_plunger) { waiting = true; break; }
    }
    if (waiting) {
        if (g_plunger_held) {
            g_plunger_charge = clampf(g_plunger_charge + DT / PLUNGER_CHARGE_S, 0.0f, 1.0f);
        }
        if (now >= g_autolaunch_at) {
            for (int i = 0; i < MAX_SIM_BALLS; i++) {
                if (g_balls[i].active && g_balls[i].in_plunger) launch_ball(g_balls[i], 0.85f);
            }
        }
    }

    for (int s = 0; s < SUBSTEP; s++) physics_step(SDT, now);

    // Drain : uniquement cote table. Une bille redescendue dans le couloir se
    // pose sur le fond et se relance (la zone du lanceur se rouvre toute seule).
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        Ball& b = g_balls[i];
        if (!b.active || b.in_plunger) continue;
        // Seuil calcule, pas approxime : posee sur le fond du couloir la bille
        // s'immobilise a LANE_BOT - BALL_R - RAIL_HALF = 1088. Un seuil trop
        // proche de cette valeur (a 1 px pres) laisserait la bille bloquee au
        // fond du couloir sans jamais rouvrir la zone du lanceur.
        if (b.x > LANE_X + 4.0f && b.y > LANE_BOT - BALL_R - RAIL_HALF - 12.0f &&
            fabsf(b.vy) < 55.0f && fabsf(b.vx) < 55.0f) {
            b.in_plunger = true;
            b.x = LANE_MID; b.y = BALL_REST_Y; b.vx = b.vy = 0.0f;
            g_autolaunch_at = now + AUTOLAUNCH_MS;
            show(g_zone_p, true);
            show(g_plunger_hint, true);
            continue;
        }
        if (b.y > DRAIN_Y && b.x < LANE_X) {
            ball_drained(b);
            if (g_state != ST_PLAYING) return;   // end_game() a pu changer d'ecran
        }
    }

    render_flippers();
    render_balls();
    render_plunger();
    render_effects(now);
    hud_sync(now);
}

// ===========================================================================
// 21. Entrees tactiles
// ===========================================================================

static void zone_event_cb(lv_event_t* e) {
    if (g_state != ST_PLAYING) return;
    const int tag = (int) (intptr_t) lv_event_get_user_data(e);
    const lv_event_code_t code = lv_event_get_code(e);
    const bool down = (code == LV_EVENT_PRESSED);

    if (tag == 0 || tag == 1) {
        g_flip[tag].pressed = down;
        return;
    }
    // Plunger : maintien = charge, relachement = tir.
    if (down) {
        g_plunger_held = true;
        g_plunger_charge = 0.0f;
        return;
    }
    if (!g_plunger_held) return;
    g_plunger_held = false;
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        if (g_balls[i].active && g_balls[i].in_plunger) {
            launch_ball(g_balls[i], g_plunger_charge);
            break;
        }
    }
    g_plunger_charge = 0.0f;
}

// Toucher le fronton = pause. Volontairement pas de croix : le jeu est un flux
// plein cadre, on ne remet pas le chrome modal (ADR-0009).
static void hud_event_cb(lv_event_t*) {
    if (g_state == ST_PLAYING) {
        g_flip[0].pressed = g_flip[1].pressed = false;
        go_pause();
    }
}

static void slot_event_cb(lv_event_t* e) {
    const int i = (int) (intptr_t) lv_event_get_user_data(e);

    switch (g_state) {
    case ST_HUB:
        if (i == 0)      new_game();
        else if (i == 1) go_scores();
        else if (i == 2) go_settings();
        else if (i == 3) close();
        break;

    case ST_SCORES:
        if (i == 0) go_hub();
        break;

    case ST_SETTINGS:
        if (i == 0)      { g_save.nudge_sens = (uint8_t) ((g_save.nudge_sens + 1) % 5); persist_save(); go_settings(); }
        else if (i == 1) { g_save.invert_nudge = g_save.invert_nudge ? 0 : 1; persist_save(); go_settings(); }
        else if (i == 2) {
            // Effet immediat : le joueur voit tout de suite si c'est le bon sens.
            g_save.flip_screen = g_save.flip_screen ? 0 : 1;
            persist_save();
            screen_portrait(true);
            go_settings();
        }
        else if (i == 3) { calibrate_flat(); go_settings(); }
        else if (i == 4) go_hub();
        break;

    case ST_PAUSED:
        if (i == 0)      { g_state = ST_PLAYING; panel_on(false); tick_period_sync(); }
        else if (i == 1) calibrate_flat();
        else if (i == 2) end_game();
        else if (i == 3) close();
        break;

    case ST_GAMEOVER:
        if (i == 0)      new_game();
        else if (i == 1) go_scores();
        else if (i == 2) go_hub();
        break;

    default:
        break;
    }
}

// ===========================================================================
// 22. API publique
// ===========================================================================

void on_imu(float ax, float ay, float /*az*/) {
    if (ax != ax || ay != ay) return;   // garde NaN
    g_raw_x = ax;
    g_raw_y = ay;
}

void calibrate_flat() {
    g_save.cal_x = (int16_t) (g_raw_x * 1000.0f);
    g_save.cal_y = (int16_t) (g_raw_y * 1000.0f);
    g_slow_x = g_slow_y = 0.0f;
    g_tilt_hits = 0;
    persist_save();
}

bool is_open() { return g_state != ST_OFF; }

void open(const UI& ui) {
    if (g_state != ST_OFF) return;
    if (!ui.root || !ui.field || !ui.hud || !ui.panel) return;
    g_ui = ui;

    persist_load();

    // Portrait AVANT de construire / afficher : LVGL redimensionne l'ecran actif
    // et invalide tout, autant que ce soit fait une seule fois.
    screen_portrait(true);

    build_ui();

    // La page LVGL est déjà active (navigation via lvgl.page.show dans le YAML).

    // Etat de repos visible derriere le hub : table vide, aucune bille.
    for (int i = 0; i < MAX_SIM_BALLS; i++) {
        g_balls[i].active = false;
        show(g_balls[i].shadow, false);
        show(g_balls[i].body, false);
        show(g_balls[i].gloss, false);
    }
    show(g_zone_p, false);
    show(g_plunger_hint, false);
    show(g_toast, false);
    g_state = ST_HUB;

    if (!g_timer) {
        g_timer = lv_timer_create(tick_cb, TICK_MENU_MS, nullptr);
        g_tick_period = TICK_MENU_MS;
    }
    go_hub();
}

void close() {
    if (g_state == ST_OFF) return;

    // Une partie abandonnee est quand meme enregistree : sinon on pourrait
    // quitter pour effacer un mauvais score, et le classement ne voudrait plus
    // rien dire.
    if (g_state == ST_PLAYING || g_state == ST_PAUSED) {
        g_save.games++;
        g_save.total_score += g_score;
        if (g_ball_score > g_save.best_ball) g_save.best_ball = g_ball_score;
        scores_insert(g_score, g_ball_num, g_game_multiballs, g_game_tilted);
    }
    persist_save();

    if (g_timer) { lv_timer_delete(g_timer); g_timer = nullptr; }
    g_flip[0].pressed = g_flip[1].pressed = false;
    g_plunger_held = false;

    g_state = ST_OFF;

    // Restauration du paysage AVANT de naviguer : le sélecteur arcade
    // s'affiche en orientation correcte.
    screen_portrait(false);
    // Navigation retour vers le sélecteur arcade (page LVGL).
    if (g_ui.lvgl) g_ui.lvgl->show_page(g_ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
}

}  // namespace Pinball
