/**
 * [AI-CONTEXT]
 * @file lode_game.cpp
 * @role Jeu « Coureur d'Or » — clone de Lode Runner pour le Tab5 (ESP32-P4 / LVGL).
 * @architecture_constraint Plein ecran 1280x720. Le YAML ne fournit que 5
 *      conteneurs vides + 3 polices ; tout le reste est construit ici. Les objets
 *      LVGL sont PREALLOUES une seule fois (pool) puis reutilises par show/hide +
 *      move : aucune allocation LVGL dans la boucle de jeu. Persistance NVS via
 *      esphome::global_preferences (aucune dependance Home Assistant).
 * @ai_instruction Hot-path = tick_cb() : pas de std::string, pas de to_string(),
 *      pas de new/delete. Les libelles HUD ne sont reecrits que quand leur valeur
 *      change. Couleurs : uniquement Lode::Pal::* (jamais d'hex en dur ici).
 *      Les chaines affichees restent SANS accent (convention du projet, cf.
 *      marble_game.cpp) pour ne dependre d'aucun encodage source.
 *
 * MODELE DE DEPLACEMENT (identique pour le joueur et les gardes) :
 *      Chaque acteur est verrouille sur la grille. A l'arret (`sub == 0`) il prend
 *      UNE decision et s'engage sur un pas d'une case ; `sub` progresse ensuite de
 *      `speed` px par tick jusqu'a TILE, puis la cellule change. C'est le
 *      fonctionnement du Lode Runner d'origine : pas de platformer flottant.
 *      Le garde-fou scripts/check_lode_levels.py rejoue EXACTEMENT ces regles
 *      (passable / supported / can_step / arete de creusement) sur les 10 maps.
 */
#include "lode_game.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <ctime>

namespace Lode {

// ===========================================================================
// 1. Geometrie & reglages
// ===========================================================================

static constexpr int GRID_W = 30;    // 30 x 42 = 1260 px
static constexpr int GRID_H = 16;    // 16 x 42 =  672 px (exactement la hauteur utile)
static constexpr int TILE   = 42;
static constexpr int FW = 1280, FH = 672, HUD_H = 48;
static constexpr int OX = (FW - GRID_W * TILE) / 2;   // 10 px de marge de chaque cote
static constexpr int OY = 0;

static constexpr int TICK_MS     = 33;   // ~30 Hz
static constexpr int RUN_SPEED   = 6;    // 42/6  = 7 ticks par case (~4,3 cases/s)
static constexpr int CLIMB_SPEED = 6;
static constexpr int FALL_SPEED  = 14;   // 42/14 = 3 ticks par case (chute franche)

static constexpr uint32_t DIG_MS       = 420;   // duree de l'animation de creusement
static constexpr uint32_t HOLE_MS      = 5400;  // duree de vie d'un trou
static constexpr uint32_t HOLE_WARN_MS = 1400;  // clignotement avant refermeture
static constexpr uint32_t TRAP_MS      = 2600;  // temps qu'un garde reste coince
static constexpr uint32_t RESPAWN_MS   = 1600;  // delai avant reapparition d'un garde
static constexpr uint32_t DEATH_MS     = 900;   // gel apres la mort du joueur
static constexpr uint32_t TOAST_MS     = 1600;

static constexpr int START_LIVES = 5;
static constexpr int MAX_GUARDS  = 4;
static constexpr int MAX_GOLD    = 40;
static constexpr int MAX_TILEOBJ = 340;  // verifie par check_lode_levels.py
static constexpr int MAX_HOLES   = 24;

static constexpr int SC_GOLD  = 250;   // lingot ramasse
static constexpr int SC_TRAP  = 75;    // garde detruit dans un trou qui se referme
static constexpr int SC_CLEAR = 1500;  // niveau termine

// Inclinaison : seuils de bascule en 4 directions discretes.
static constexpr float TILT_SMOOTH = 0.35f;
static constexpr float DEAD_BASE   = 0.130f;   // reduit par la sensibilite

// "LOD1" — bumpe a chaque changement de layout de LodeSave.
static constexpr uint32_t SAVE_MAGIC = 0x4C4F4431u;
// Cle NVS dediee, distincte de celle de marble_game.cpp (0x4D41524B).
static constexpr uint32_t PREF_KEY   = 0x4C4F4445u;

// ===========================================================================
// 2. Generateur pseudo-aleatoire (xorshift32)
// ===========================================================================

static uint32_t s_rng = 0x9E3779B9u;
static inline uint32_t rnd() {
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}
static inline int rnd_range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int) (rnd() % (uint32_t) (hi - lo + 1));
}
static inline int iabs(int v) { return v < 0 ? -v : v; }

// ===========================================================================
// 3. Tuiles & niveaux
// ===========================================================================
//
// LEGENDE DES MAPS (une ligne = une rangee, 30 colonnes ; les espaces de fin
// peuvent etre omis, ils sont completes au chargement) :
//
//   ' '  vide
//   '#'  brique      — creusable, se referme apres HOLE_MS
//   '@'  beton       — indestructible
//   'H'  echelle     — monter / descendre, fait aussi office de sol
//   '-'  barre       — on s'y suspend, on se deplace dessous, on peut lacher
//   '$'  lingot d'or — a ramasser (la case reste vide pour le deplacement)
//   'P'  depart du joueur (une seule occurrence)
//   'G'  depart d'un garde (4 au maximum)
//   'S'  echelle de sortie — case VIDE tant qu'il reste de l'or, echelle ensuite ;
//        elle doit atteindre la rangee 0 (la sortie = atteindre le haut de l'ecran)
//
enum Tile : uint8_t { T_EMPTY = 0, T_BRICK, T_SOLID, T_LADDER, T_BAR, T_EXIT };

// --- Niveau 1 : Premier filon ---
static const char* const MAP1[GRID_H] = {
    "                          S",
    "                          S",
    "                          S",
    "     $             $      S",
    "   #####H#######H#####    S",
    "        H       H         S",
    "        H       H         S",
    "   $    H   G   H    $    S",
    "   #####H#######H#######  S",
    "        H       H         S",
    "        H       H         S",
    " $      H  P    H      $  S",
    "########H#######H#########S###",
    "        H       H         S",
    "  $     H       H      $  S",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
};
// --- Niveau 2 : Escalier ---
static const char* const MAP2[GRID_H] = {
    "                            S",
    "     $  H                   S",
    "   #####H                   S",
    "        H     $             S",
    "  ######H##########         S",
    "        H         H         S",
    "  $     H    G    H    $    S",
    "  ######H#########H#####    S",
    "        H         H         S",
    "   $    H         H   $     S",
    "  ######H#########H#####    S",
    "        H         H         S",
    "   $    H    P    H---------S",
    "########H#########H#      ##S#",
    "  $     H         H   $ $   S",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
};
// --- Niveau 3 : Le pendu ---
static const char* const MAP3[GRID_H] = {
    " S",
    " S    $           $",
    " S  ######H#######H######",
    " S        H       H",
    " S -------H-------H------",
    " S        H       H",
    " S$       H   G   H     $",
    " S########H#######H######",
    " S        H       H",
    " S--------H-------H-----",
    " S        H       H",
    " S$       H   G   H     $",
    "#S########H#######H###########",
    " S        H       H",
    " S $      H   P   H    $",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
};
// --- Niveau 4 : Le puits ---
static const char* const MAP4[GRID_H] = {
    "                             S",
    "  H                    H     S",
    "  H  $   P        $    H     S",
    "  H####################H     S",
    "  H        G           H     S",
    "  H####################H     S",
    "  H   $        $       H     S",
    "  H####################H     S",
    "  H   $  ########   G  H     S",
    "  ######################     S",
    "        $      $             S",
    "  ######################     S",
    "     $      $       $        S",
    "  ######################     S",
    "  H                    H     S",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
};
// --- Niveau 5 : Le peigne ---
static const char* const MAP5[GRID_H] = {
    "S",
    "S   $    $    $    $    $",
    "S ###H#####H#####H#####H###",
    "S    H     H     H     H",
    "S    H     H     H     H",
    "S ###H#####H#####H#####H###",
    "S $  H G   H   G H     H$",
    "S ##H#####H#####H#####H###",
    "S   H     H     H     H",
    "S   H     H     H     H",
    "S ##H#####H#####H#####H###",
    "S  $H  $  H  $  H  $  H $",
    "S ##H#########H###########",
    "S   H         H",
    "S   H     P   H        $",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
};
// --- Niveau 6 : Faux chemin ---
static const char* const MAP6[GRID_H] = {
    "              S",
    "  $        $  S     $",
    "  ####H@@@@###S@@@@@#H##",
    "      H       S      H",
    "  ----H----   S -----H----",
    "      H       S      H",
    "  $   H    G  S G    H   $",
    "  ####H####@@@S@#####H####",
    "      H       S      H",
    "  $   H  $    S   $  H   $",
    "  @@@@H@@@@@##S@@@@@@H@@@@",
    "      H       S      H",
    "   $  H    P  S      H  $",
    "######H#######S######H########",
    "      H     $ S      H",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
};
// --- Niveau 7 : Le nid ---
static const char* const MAP7[GRID_H] = {
    "  S",
    "  S  H              H",
    "  S##H##############H###",
    "  S  H              H",
    "  S--H---$$$$$$--- -H---",
    "  S  H  ########    H",
    "  S  H  #$$$$$$#    H  $",
    "  S##H##########H###H###",
    "  S  H          H   H",
    "  S  H   G  G   H   H  $",
    "  S##H##########H###H###",
    "  S  H          H   H",
    "  S  H     P    H   H  $",
    "##S##H##########H###H#######",
    "  S  H          H   H   $",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
};
// --- Niveau 8 : Cathedrale ---
static const char* const MAP8[GRID_H] = {
    "                           S",
    "   $   H            H   $  S",
    "  #####H############H##### S",
    "       H            H      S",
    "  -----H---    -----H----- S",
    "       H            H      S",
    "  $ G  H  $      $  H  G $ S",
    "  #####H####@@@@####H##### S",
    "       H            H      S",
    "  -----H------------H----- S",
    "       H            H      S",
    "  $    H   $    $   H    $ S",
    "#######H############H######S",
    "       H            H      S",
    "   $   H     P      H   $  S",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
};
// --- Niveau 9 : Dedale ---
static const char* const MAP9[GRID_H] = {
    "S",
    "S $  H   $   H   $   H  $",
    "S ###H###H###H###H###H###",
    "S    H   H   H   H   H",
    "S ---H---H---H---H---H---",
    "S    H   H   H   H   H",
    "S $  H G H   H G H   H  $",
    "S ###H###H###H###H###H###",
    "S    H   H   H   H   H",
    "S $  H   H $ H   H $ H  $",
    "S ###H###H###H###H###H###",
    "S    H   H   H   H   H",
    "S $  H G H P H G H   H  $",
    "S####H###H###H###H###H#####",
    "S    H   H   H   H   H  $",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
};
// --- Niveau 10 : Coffre-fort ---
static const char* const MAP10[GRID_H] = {
    "                            S",
    "  H  $      @@@@      $  H  S",
    "  H#####@@@@@@@@@@@@#####H  S",
    "  H                      H  S",
    "  H--------    ----------H  S",
    "  H                      H  S",
    "  H  $  ############  $  H  S",
    "  H#####@@@@@@@@@@@@#####H  S",
    "  H   G              G   H  S",
    "  #########H################S#",
    "     $   $ H    $   $       S",
    "  #########H################S#",
    "  H   G    HP        G   H  S",
    "  H########H##########@@@H##S#",
    "  H   $    H         $   H  S",
    "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
};

struct LevelDef { const char* name; const char* const* rows; };
static const LevelDef LEVELS[LODE_N_LEVELS] = {
    {"Premier filon", MAP1},  {"Escalier",    MAP2},
    {"Le pendu",      MAP3},  {"Le puits",    MAP4},
    {"Le peigne",     MAP5},  {"Faux chemin", MAP6},
    {"Le nid",        MAP7},  {"Cathedrale",  MAP8},
    {"Dedale",        MAP9},  {"Coffre-fort", MAP10},
};

// ===========================================================================
// 4. Etat
// ===========================================================================

enum State : uint8_t {
    ST_OFF = 0, ST_HUB, ST_LEVELS, ST_SETTINGS, ST_SCORES, ST_CONFIRM,
    ST_PLAYING, ST_PAUSED, ST_DYING, ST_CLEAR, ST_GAMEOVER
};

enum ASt : uint8_t { A_IDLE = 0, A_FALL, A_DIG, A_TRAP, A_OUT, A_GONE };

struct Actor {
    int8_t   cx, cy;      // cellule courante
    int8_t   mdx, mdy;    // direction du pas engage (0,0 = a l'arret)
    int16_t  sub;         // avancement dans le pas, 0..TILE
    int8_t   face;        // -1 / +1 : orientation visuelle
    uint8_t  st;
    bool     carry;       // porte un lingot (gardes uniquement)
    int8_t   digd;        // sens du creusement en cours
    uint32_t until;       // fin de creusement / de piege / de respawn
    lv_obj_t* body;
    lv_obj_t* head;
};

struct Hole { int8_t x, y; uint8_t blink; uint32_t at; };

static LodeSave g_save{};
static esphome::ESPPreferenceObject g_pref;
static bool  g_pref_ready = false;

static UI    g_ui{};
static bool  g_built = false;
static State g_state = ST_OFF;
static lv_timer_t* g_timer = nullptr;
static uint32_t g_tick = 0;

// --- Horloge (pour dater les scores sans dependre de HA a l'execution) ---
static uint32_t g_epoch0 = 0, g_epoch_ms = 0;

// --- Grille ---
static uint8_t g_tile[GRID_H][GRID_W];
static bool    g_goldmap[GRID_H][GRID_W];
static int16_t g_cellobj[GRID_H * GRID_W];   // cellule -> index tuile (-1 = aucun)
static int16_t g_goldobj[GRID_H * GRID_W];   // cellule -> index lingot (-1 = aucun)
static Hole    g_hole[MAX_HOLES];
static int     g_hole_n = 0;
static bool    g_exit_on = false;

// --- Acteurs ---
static Actor g_p{};
static Actor g_gd[MAX_GUARDS];
static int   g_guard_n = 0;

// --- Champ de distance (BFS inverse depuis le joueur) ---
static uint8_t  g_dist[GRID_H][GRID_W];
static uint16_t g_bfsq[GRID_H * GRID_W];

// --- Partie en cours ---
static int      g_level = 0;         // index 0..9
static int      g_lives = START_LIVES;
static uint32_t g_score = 0;
static int      g_gold_left = 0;     // lingots que le joueur n'a pas encore ramasses
static uint32_t g_level_start = 0;
static bool     g_run_active = false;
static bool     g_offrank = false;   // partie hors classement (mode entrainement)
static uint32_t g_die_until = 0;

// --- Entrees ---
static float   g_raw_x = 0.0f, g_raw_y = 0.0f;
static float   g_tilt_x = 0.0f, g_tilt_y = 0.0f;
static uint8_t g_btn_dir = D_NONE;
static uint8_t g_imu_dir = D_NONE;
// Intention de creusement mise en tampon : un appui recu pendant qu'un pas est
// en cours (7 ticks) serait sinon perdu, ce qui rend le bouton « mou ».
// Elle est consommee des que le joueur retombe aligne sur une case, et expire
// au bout de DIG_BUFFER_MS pour ne jamais declencher un creusement fantome.
static int8_t   g_dig_want = 0;
static uint32_t g_dig_want_until = 0;
static constexpr uint32_t DIG_BUFFER_MS = 350;

// --- Objets LVGL (construits une fois) ---
static lv_obj_t* g_tobj[MAX_TILEOBJ] = {};
static int       g_tobj_n = 0;
static lv_obj_t* g_gobj[MAX_GOLD] = {};
static int       g_gobj_n = 0;
static lv_obj_t* g_padbtn[6] = {};        // 0..3 = gauche/droite/haut/bas, 4/5 = creuser G/D
static lv_obj_t* g_flash = nullptr;       // voile de mort
static lv_obj_t* g_toast = nullptr;
static uint32_t  g_toast_until = 0;

static lv_obj_t* g_hud_score = nullptr;
static lv_obj_t* g_hud_lives = nullptr;
static lv_obj_t* g_hud_level = nullptr;
static lv_obj_t* g_hud_gold  = nullptr;
static lv_obj_t* g_hud_best  = nullptr;

static lv_obj_t* g_p_title = nullptr;
static lv_obj_t* g_p_sub   = nullptr;
static lv_obj_t* g_p_body  = nullptr;
static lv_obj_t* g_p_foot  = nullptr;
static constexpr int N_SLOTS = 12;
static lv_obj_t* g_slot[N_SLOTS]   = {};
static lv_obj_t* g_slot_t[N_SLOTS] = {};
static lv_obj_t* g_slot_d[N_SLOTS] = {};

// Caches HUD : on ne reecrit un libelle que si sa valeur a change.
static int32_t g_c_score = -1, g_c_lives = -1, g_c_level = -1,
               g_c_gold = -1, g_c_best = -1;

static const char* const CTRL_NAME[3] = {"Boutons", "Inclinaison", "Mixte"};

static void go_hub();
static void go_levels();
static void go_settings();
static void go_scores();
static void show_pause();
static void start_level(int idx, bool new_run);
static void load_level(int idx);
static void rebuild_dist();
static void open_exit();

// ===========================================================================
// 5. Persistance NVS
// ===========================================================================

void persist_load() {
    if (!g_pref_ready) {
        g_pref = esphome::global_preferences->make_preference<LodeSave>(PREF_KEY);
        g_pref_ready = true;
    }
    if (!g_pref.load(&g_save) || g_save.magic != SAVE_MAGIC) {
        g_save = LodeSave{};
        g_save.magic = SAVE_MAGIC;
        g_save.unlocked = 1;
        g_save.ctrl_mode = 0;      // boutons par defaut
        g_save.sensitivity = 2;
    }
    if (g_save.unlocked < 1) g_save.unlocked = 1;
    if (g_save.unlocked > LODE_N_LEVELS) g_save.unlocked = LODE_N_LEVELS;
    if (g_save.ctrl_mode > 2) g_save.ctrl_mode = 0;
    if (g_save.sensitivity > 4) g_save.sensitivity = 2;
    if (g_save.score_count > LODE_MAX_SCORES) g_save.score_count = 0;
}

void persist_save() {
    if (!g_pref_ready) return;
    g_save.magic = SAVE_MAGIC;
    g_pref.save(&g_save);
    esphome::global_preferences->sync();
}

// Horodatage courant : base SNTP passee a l'ouverture + temps ecoule depuis.
static uint32_t now_epoch() {
    if (!g_epoch0) return 0;
    return g_epoch0 + (lv_tick_get() - g_epoch_ms) / 1000u;
}

static void fmt_stamp(uint32_t st, char* out, size_t n) {
    if (!st) { snprintf(out, n, "  --  "); return; }
    time_t t = (time_t) st;
    struct tm tmv;
    localtime_r(&t, &tmv);
    snprintf(out, n, "%02d/%02d %02d:%02d", tmv.tm_mday, tmv.tm_mon + 1,
             tmv.tm_hour, tmv.tm_min);
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

static void set_text_if(lv_obj_t* l, const char* txt) {
    if (!l) return;
    const char* cur = lv_label_get_text(l);
    if (cur && strcmp(cur, txt) == 0) return;
    lv_label_set_text(l, txt);
}

static void toast(const char* msg) {
    if (!g_toast) return;
    set_text_if(g_toast, msg);
    show(g_toast, true);
    g_toast_until = lv_tick_get() + TOAST_MS;
}

// Effets sonores : crochet volontairement neutre. Le Tab5 n'expose pas de
// generateur de bip cote C++ (le media_player joue des flux, pas des tonalites) ;
// brancher un vrai SFX demanderait un composant audio dedie. On garde le point
// d'appel pour ne pas avoir a re-instrumenter le gameplay plus tard.
static inline void sfx(int /*id*/) {}

// ===========================================================================
// 7. Regles de grille — MIROIR EXACT de scripts/check_lode_levels.py
// ===========================================================================

static inline bool in_grid(int x, int y) {
    return x >= 0 && x < GRID_W && y >= 0 && y < GRID_H;
}

// Hors carte = beton : le monde est ferme, personne ne sort par les bords.
static inline uint8_t tile_at(int x, int y) {
    return in_grid(x, y) ? g_tile[y][x] : (uint8_t) T_SOLID;
}

static inline bool is_ladder(int x, int y) {
    uint8_t t = tile_at(x, y);
    return t == T_LADDER || (t == T_EXIT && g_exit_on);
}

static inline bool passable(int x, int y) {
    uint8_t t = tile_at(x, y);
    return t != T_BRICK && t != T_SOLID;
}

// Un garde coince dans un trou fait office de plancher : on lui court dessus.
static bool trapped_guard_at(int x, int y) {
    for (int i = 0; i < g_guard_n; i++) {
        const Actor& g = g_gd[i];
        if (g.st == A_TRAP && g.cx == x && g.cy == y) return true;
    }
    return false;
}

// Un acteur tient-il en place sur cette case ?
static bool supported(int x, int y) {
    if (is_ladder(x, y)) return true;
    if (tile_at(x, y) == T_BAR) return true;      // suspendu a une barre
    uint8_t b = tile_at(x, y + 1);
    if (b == T_BRICK || b == T_SOLID || b == T_LADDER) return true;
    if (b == T_EXIT && g_exit_on) return true;
    if (trapped_guard_at(x, y + 1)) return true;
    return false;
}

// Un acteur peut-il passer de (fx,fy) a la case voisine (tx,ty) ?
static bool can_step(int fx, int fy, int tx, int ty) {
    if (!passable(tx, ty)) return false;
    if (ty == fy) return supported(fx, fy);       // lateral : il faut un appui
    if (ty == fy - 1) return is_ladder(fx, fy);   // monter : seulement sur une echelle
    return true;                                  // descendre : echelle, lachage ou chute
}

static int hole_index(int x, int y) {
    for (int i = 0; i < g_hole_n; i++)
        if (g_hole[i].x == x && g_hole[i].y == y) return i;
    return -1;
}

// ===========================================================================
// 8. Rendu des acteurs
// ===========================================================================

static inline int actor_px(const Actor& a) { return OX + a.cx * TILE + a.mdx * a.sub; }
static inline int actor_py(const Actor& a) { return OY + a.cy * TILE + a.mdy * a.sub; }

static void draw_actor(Actor& a, uint32_t body_col, uint32_t head_col) {
    if (!a.body) return;
    if (a.st == A_GONE) { show(a.body, false); show(a.head, false); return; }
    int px = actor_px(a), py = actor_py(a);
    // Animation 2 images : la silhouette s'affine une frame sur deux en marche.
    bool moving = (a.mdx != 0 || a.mdy != 0);
    int bw = (moving && ((g_tick / 4) & 1)) ? 20 : 26;
    lv_obj_set_size(a.body, bw, 24);
    lv_obj_set_pos(a.body, px + (TILE - bw) / 2, py + 17);
    lv_obj_set_pos(a.head, px + 13, py + 3);
    set_bg(a.body, body_col, LV_OPA_COVER);
    set_bg(a.head, head_col, LV_OPA_COVER);
    show(a.body, true);
    show(a.head, true);
}

// ===========================================================================
// 9. Construction de l'UI (une seule fois)
// ===========================================================================

static void slot_event_cb(lv_event_t* e);
static void pad_event_cb(lv_event_t* e);
static void hud_event_cb(lv_event_t* e);

static lv_obj_t* mk_pad_btn(int id, int x, int y, int w, int h) {
    lv_obj_t* o = mk_rect(g_ui.pad);
    lv_obj_set_pos(o, x, y);
    lv_obj_set_size(o, w, h);
    lv_obj_set_style_radius(o, 12, LV_PART_MAIN);
    set_bg(o, Pal::PAD_FILL, LV_OPA_50);
    set_border(o, Pal::PAD_EDGE, 2, LV_OPA_70);
    lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
    // Retour tactile : le fond s'eclaircit tant que le doigt est pose.
    // Casts explicites : combiner lv_part_t et lv_state_t directement est
    // deprecie en C++20 (-Wdeprecated-enum-enum-conversion).
    lv_obj_set_style_bg_color(o, lv_color_hex(Pal::PAD_EDGE),
                              (lv_style_selector_t) LV_PART_MAIN |
                              (lv_style_selector_t) LV_STATE_PRESSED);
    void* ud = (void*) (intptr_t) id;
    if (id < 4) {
        lv_obj_add_event_cb(o, pad_event_cb, LV_EVENT_PRESSED, ud);
        lv_obj_add_event_cb(o, pad_event_cb, LV_EVENT_RELEASED, ud);
        lv_obj_add_event_cb(o, pad_event_cb, LV_EVENT_PRESS_LOST, ud);
    } else {
        lv_obj_add_event_cb(o, pad_event_cb, LV_EVENT_CLICKED, ud);
    }
    return o;
}

static void build_ui() {
    if (g_built) return;

    set_bg(g_ui.root,  Pal::VOID_BG,  LV_OPA_COVER);
    set_bg(g_ui.field, Pal::FLOOR_BG, LV_OPA_COVER);
    set_bg(g_ui.hud,   Pal::HUD_BG,   LV_OPA_COVER);
    set_bg(g_ui.panel, Pal::VOID_BG,  (lv_opa_t) 245);
    // Le calque des zones tactiles est transparent et non cliquable lui-meme :
    // seuls ses enfants (D-pad, boutons creuser) captent les evenements.
    lv_obj_set_style_bg_opa(g_ui.pad, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_clear_flag(g_ui.pad, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(g_ui.pad, LV_OBJ_FLAG_SCROLLABLE);

    // --- Pool de tuiles : cree une fois, recycle a chaque niveau ---
    for (int i = 0; i < MAX_TILEOBJ; i++) {
        g_tobj[i] = mk_rect(g_ui.field);
        lv_obj_add_flag(g_tobj[i], LV_OBJ_FLAG_HIDDEN);
    }
    // --- Pool de lingots (dessines APRES les tuiles => au-dessus) ---
    for (int i = 0; i < MAX_GOLD; i++) {
        g_gobj[i] = mk_rect(g_ui.field);
        lv_obj_set_size(g_gobj[i], 22, 22);
        lv_obj_set_style_radius(g_gobj[i], 6, LV_PART_MAIN);
        set_bg(g_gobj[i], Pal::GOLD, LV_OPA_COVER);
        set_border(g_gobj[i], Pal::GOLD_HI, 2, LV_OPA_COVER);
        lv_obj_add_flag(g_gobj[i], LV_OBJ_FLAG_HIDDEN);
    }
    // --- Acteurs : corps + tete (rendu retro 2 blocs, pas de sprite) ---
    g_p.body = mk_rect(g_ui.field);
    g_p.head = mk_rect(g_ui.field);
    lv_obj_set_size(g_p.head, 16, 14);
    for (int i = 0; i < MAX_GUARDS; i++) {
        g_gd[i].body = mk_rect(g_ui.field);
        g_gd[i].head = mk_rect(g_ui.field);
        lv_obj_set_size(g_gd[i].head, 16, 14);
        show(g_gd[i].body, false); show(g_gd[i].head, false);
    }
    show(g_p.body, false); show(g_p.head, false);

    // --- Voile de mort (flash bref, pas de secousse plein ecran) ---
    g_flash = mk_rect(g_ui.field);
    lv_obj_set_pos(g_flash, 0, 0);
    lv_obj_set_size(g_flash, FW, FH);
    set_bg(g_flash, Pal::DANGER, (lv_opa_t) 90);
    show(g_flash, false);

    g_toast = mk_label(g_ui.field, g_ui.f_mid, Pal::GOLD);
    lv_obj_align(g_toast, LV_ALIGN_TOP_MID, 0, 16);
    show(g_toast, false);

    // --- HUD : bande compacte de 48 px, jamais plus ---
    g_hud_score = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT);
    lv_obj_align(g_hud_score, LV_ALIGN_LEFT_MID, 18, 0);
    g_hud_lives = mk_label(g_ui.hud, g_ui.f_small, Pal::RUNNER);
    lv_obj_align(g_hud_lives, LV_ALIGN_LEFT_MID, 260, 0);
    g_hud_level = mk_label(g_ui.hud, g_ui.f_small, Pal::LADDER);
    lv_obj_align(g_hud_level, LV_ALIGN_LEFT_MID, 420, 0);
    g_hud_gold = mk_label(g_ui.hud, g_ui.f_small, Pal::GOLD);
    lv_obj_align(g_hud_gold, LV_ALIGN_LEFT_MID, 700, 0);
    g_hud_best = mk_label(g_ui.hud, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_align(g_hud_best, LV_ALIGN_RIGHT_MID, -18, 0);

    lv_obj_add_flag(g_ui.hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_ui.hud, hud_event_cb, LV_EVENT_CLICKED, nullptr);

    // --- Zones tactiles : D-pad en croix a gauche, creusement a droite --------
    // Le Tab5 n'a pas de bouton physique : ce sont des zones LVGL semi-opaques.
    g_padbtn[0] = mk_pad_btn(0,  16, 484,  92,  92);   // gauche
    g_padbtn[1] = mk_pad_btn(1, 204, 484,  92,  92);   // droite
    g_padbtn[2] = mk_pad_btn(2, 110, 392,  92,  92);   // haut
    g_padbtn[3] = mk_pad_btn(3, 110, 576,  92,  92);   // bas
    g_padbtn[4] = mk_pad_btn(4, 884, 560, 170,  96);   // creuser a gauche
    g_padbtn[5] = mk_pad_btn(5,1064, 560, 170,  96);   // creuser a droite

    static const char* const PAD_TXT[6] = {"<", ">", "^", "v", "<", ">"};
    for (int i = 0; i < 6; i++) {
        lv_obj_t* l = mk_label(g_padbtn[i], g_ui.f_big, Pal::TXT);
        lv_label_set_text(l, PAD_TXT[i]);
        lv_obj_align(l, LV_ALIGN_CENTER, 0, i >= 4 ? -14 : 0);
    }
    for (int i = 4; i < 6; i++) {
        lv_obj_t* l = mk_label(g_padbtn[i], g_ui.f_small, Pal::BRICK_HI);
        lv_label_set_text(l, "CREUSER");
        lv_obj_align(l, LV_ALIGN_CENTER, 0, 26);
    }

    // --- Panneau de menus ----------------------------------------------------
    g_p_title = mk_label(g_ui.panel, g_ui.f_big, Pal::GOLD);
    lv_obj_align(g_p_title, LV_ALIGN_TOP_MID, 0, 42);
    g_p_sub = mk_label(g_ui.panel, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_align(g_p_sub, LV_ALIGN_TOP_MID, 0, 104);
    g_p_body = mk_label(g_ui.panel, g_ui.f_small, Pal::TXT);
    lv_obj_set_width(g_p_body, 1000);
    lv_obj_set_style_text_align(g_p_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(g_p_body, LV_ALIGN_TOP_MID, 0, 148);
    g_p_foot = mk_label(g_ui.panel, g_ui.f_small, Pal::TXT_DIM);
    lv_obj_align(g_p_foot, LV_ALIGN_BOTTOM_MID, 0, -20);

    for (int i = 0; i < N_SLOTS; i++) {
        g_slot[i] = mk_rect(g_ui.panel);
        lv_obj_add_flag(g_slot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(g_slot[i], 8, LV_PART_MAIN);
        set_bg(g_slot[i], Pal::FLOOR_BG, LV_OPA_COVER);
        lv_obj_set_style_bg_color(g_slot[i], lv_color_hex(Pal::PAD_FILL),
                                  (lv_style_selector_t) LV_PART_MAIN |
                                  (lv_style_selector_t) LV_STATE_PRESSED);
        lv_obj_add_event_cb(g_slot[i], slot_event_cb, LV_EVENT_CLICKED,
                            (void*) (intptr_t) i);
        g_slot_t[i] = mk_label(g_slot[i], g_ui.f_mid, Pal::TXT);
        g_slot_d[i] = mk_label(g_slot[i], g_ui.f_small, Pal::TXT_DIM);
        show(g_slot[i], false);
    }

    g_built = true;
}

// --- Mise en page des entrees de menu ---------------------------------------
static void slot_list(int i, const char* title, const char* desc, uint32_t col, bool on) {
    lv_obj_set_size(g_slot[i], 720, 62);
    lv_obj_align(g_slot[i], LV_ALIGN_TOP_MID, 0, 196 + i * 70);
    lv_obj_set_width(g_slot_t[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(g_slot_t[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(g_slot_d[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(g_slot_d[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(g_slot_t[i], LV_ALIGN_LEFT_MID, 22, desc && desc[0] ? -13 : 0);
    lv_obj_align(g_slot_d[i], LV_ALIGN_LEFT_MID, 22, 15);
    lv_obj_set_style_text_color(g_slot_t[i], lv_color_hex(on ? col : Pal::TXT_DIM), LV_PART_MAIN);
    set_text_if(g_slot_t[i], title);
    set_text_if(g_slot_d[i], desc ? desc : "");
    set_border(g_slot[i], on ? col : Pal::TXT_DIM, 2, LV_OPA_50);
    show(g_slot[i], true);
}

// Grille 2 colonnes x 5 lignes : selection de niveau.
static void slot_grid(int i, const char* title, const char* desc, uint32_t col, bool on) {
    int c = i / 5, r = i % 5;
    lv_obj_set_size(g_slot[i], 460, 72);
    lv_obj_align(g_slot[i], LV_ALIGN_TOP_LEFT, 148 + c * 500, 200 + r * 84);
    lv_obj_set_width(g_slot_t[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(g_slot_t[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(g_slot_d[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(g_slot_d[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(g_slot_t[i], LV_ALIGN_LEFT_MID, 22, -13);
    lv_obj_align(g_slot_d[i], LV_ALIGN_LEFT_MID, 22, 15);
    lv_obj_set_style_text_color(g_slot_t[i], lv_color_hex(on ? col : Pal::TXT_DIM), LV_PART_MAIN);
    set_text_if(g_slot_t[i], title);
    set_text_if(g_slot_d[i], desc ? desc : "");
    set_border(g_slot[i], on ? col : Pal::TXT_DIM, 2, LV_OPA_50);
    show(g_slot[i], true);
}

static void slots_hide_from(int n) {
    for (int i = n; i < N_SLOTS; i++) show(g_slot[i], false);
}

static void panel_on(bool v) {
    show(g_ui.panel, v);
    if (v) lv_obj_move_foreground(g_ui.panel);
}

// Corps de panneau centre (etat par defaut). Seul le classement passe a gauche.
static inline void body_center() {
    lv_obj_set_style_text_align(g_p_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
}

// Le pad ne s'affiche qu'en jeu ; le D-pad disparait en mode inclinaison pure.
static void pad_sync() {
    bool play = (g_state == ST_PLAYING);
    show(g_ui.pad, play);
    if (play) lv_obj_move_foreground(g_ui.pad);
    // Masquer le pad sous le doigt ne genere pas toujours un RELEASED : on purge
    // la direction maintenue, sinon le joueur repartirait tout seul a la reprise.
    else { g_btn_dir = D_NONE; g_dig_want = 0; }
    bool dpad = play && (g_save.ctrl_mode != 1);
    for (int i = 0; i < 4; i++) show(g_padbtn[i], dpad);
    for (int i = 4; i < 6; i++) show(g_padbtn[i], play);
}

// ===========================================================================
// 10. Chargement d'un niveau
// ===========================================================================

// Reconstruit les objets LVGL du damier. Les runs horizontaux de beton sont
// fusionnes en un seul objet (le bord bas d'une carte = 1 objet au lieu de 30).
static void build_tiles() {
    for (int i = 0; i < g_tobj_n; i++) show(g_tobj[i], false);
    g_tobj_n = 0;
    memset(g_cellobj, 0xFF, sizeof(g_cellobj));

    for (int y = 0; y < GRID_H; y++) {
        int x = 0;
        while (x < GRID_W) {
            uint8_t t = g_tile[y][x];
            if (t == T_EMPTY) { x++; continue; }
            int run = 1;
            if (t == T_SOLID)
                while (x + run < GRID_W && g_tile[y][x + run] == T_SOLID) run++;

            if (g_tobj_n >= MAX_TILEOBJ) {
                // Ne devrait jamais arriver : check_lode_levels.py plafonne a
                // MAX_TILEOBJ. On sort proprement plutot que de deborder du pool.
                ESP_LOGW("lode", "pool de tuiles sature (niveau %d)", g_level + 1);
                return;
            }
            lv_obj_t* o = g_tobj[g_tobj_n];
            int px = OX + x * TILE, py = OY + y * TILE;

            switch (t) {
                case T_BRICK:
                    lv_obj_set_pos(o, px, py);
                    lv_obj_set_size(o, TILE, TILE);
                    lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
                    set_bg(o, Pal::BRICK, LV_OPA_COVER);
                    set_border(o, Pal::BRICK_HI, 1, (lv_opa_t) 150);
                    break;
                case T_SOLID:
                    lv_obj_set_pos(o, px, py);
                    lv_obj_set_size(o, TILE * run, TILE);
                    lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
                    set_bg(o, Pal::SOLID, LV_OPA_COVER);
                    set_border(o, Pal::SOLID_HI, 1, (lv_opa_t) 120);
                    break;
                case T_LADDER:
                case T_EXIT: {
                    uint32_t c = (t == T_LADDER) ? Pal::LADDER : Pal::EXIT_LAD;
                    lv_obj_set_pos(o, px + 4, py);
                    lv_obj_set_size(o, TILE - 8, TILE);
                    lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
                    set_bg(o, c, (lv_opa_t) 45);
                    set_border(o, c, 4, LV_OPA_COVER);   // rails + barreau = 1 objet
                    break;
                }
                case T_BAR:
                    lv_obj_set_pos(o, px, py + 5);
                    lv_obj_set_size(o, TILE, 7);
                    lv_obj_set_style_radius(o, 3, LV_PART_MAIN);
                    set_bg(o, Pal::BAR, LV_OPA_COVER);
                    set_border(o, Pal::BAR, 0, LV_OPA_TRANSP);
                    break;
                default: break;
            }
            // Seules les tuiles susceptibles de changer d'etat ont besoin d'un
            // lien cellule -> objet (brique creusee, echelle de sortie activee).
            if (t != T_SOLID) g_cellobj[y * GRID_W + x] = (int16_t) g_tobj_n;
            show(o, t != T_EXIT || g_exit_on);
            g_tobj_n++;
            x += run;
        }
    }
}

static void place_actor(Actor& a, int cx, int cy) {
    a.cx = (int8_t) cx; a.cy = (int8_t) cy;
    a.mdx = a.mdy = 0; a.sub = 0;
    a.st = A_IDLE; a.carry = false; a.digd = 0; a.until = 0;
    a.face = 1;
}

static void load_level(int idx) {
    g_level = idx;
    const char* const* rows = LEVELS[idx].rows;

    memset(g_tile, 0, sizeof(g_tile));
    memset(g_goldmap, 0, sizeof(g_goldmap));
    memset(g_goldobj, 0xFF, sizeof(g_goldobj));
    g_hole_n = 0;
    g_exit_on = false;
    g_gold_left = 0;
    g_guard_n = 0;
    g_gobj_n = 0;
    g_btn_dir = D_NONE;
    g_imu_dir = D_NONE;
    g_dig_want = 0;

    int px = 1, py = 1;
    for (int y = 0; y < GRID_H; y++) {
        const char* r = rows[y];
        int len = (int) strlen(r);
        for (int x = 0; x < GRID_W; x++) {
            char c = (x < len) ? r[x] : ' ';   // lignes courtes = completees par du vide
            uint8_t t = T_EMPTY;
            switch (c) {
                case '#': t = T_BRICK;  break;
                case '@': t = T_SOLID;  break;
                case 'H': t = T_LADDER; break;
                case '-': t = T_BAR;    break;
                case 'S': t = T_EXIT;   break;
                case '$':
                    if (g_gold_left < MAX_GOLD) {
                        g_goldmap[y][x] = true;
                        g_gold_left++;
                    }
                    break;
                case 'P': px = x; py = y; break;
                case 'G':
                    if (g_guard_n < MAX_GUARDS) {
                        place_actor(g_gd[g_guard_n], x, y);
                        g_guard_n++;
                    }
                    break;
                default: break;
            }
            g_tile[y][x] = t;
        }
    }

    place_actor(g_p, px, py);
    build_tiles();

    // Lingots : un objet par lingot, positionne une fois pour toutes.
    for (int i = 0; i < MAX_GOLD; i++) show(g_gobj[i], false);
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++) {
            if (!g_goldmap[y][x] || g_gobj_n >= MAX_GOLD) continue;
            lv_obj_t* o = g_gobj[g_gobj_n];
            lv_obj_set_pos(o, OX + x * TILE + (TILE - 22) / 2,
                              OY + y * TILE + (TILE - 22) / 2);
            show(o, true);
            g_goldobj[y * GRID_W + x] = (int16_t) g_gobj_n;
            g_gobj_n++;
        }

    for (int i = g_guard_n; i < MAX_GUARDS; i++) {
        g_gd[i].st = A_GONE;
        show(g_gd[i].body, false);
        show(g_gd[i].head, false);
    }

    // Carte sans or (cas theorique, refuse par le garde-fou) : sortie d'emblee.
    if (g_gold_left <= 0) open_exit();

    g_level_start = lv_tick_get();
    show(g_flash, false);
    rebuild_dist();
}

// ===========================================================================
// 11. Or, trous, sortie
// ===========================================================================

static void open_exit() {
    if (g_exit_on) return;
    g_exit_on = true;
    for (int y = 0; y < GRID_H; y++)
        for (int x = 0; x < GRID_W; x++) {
            if (g_tile[y][x] != T_EXIT) continue;
            int16_t oi = g_cellobj[y * GRID_W + x];
            if (oi >= 0) show(g_tobj[oi], true);
        }
    toast("Sortie ouverte ! Grimpe tout en haut.");
    sfx(3);
}

// Rend visible un lingot sur une case en recyclant un objet libre du pool
// (un lingot ramasse ou vole en libere toujours un).
static void spawn_gold_obj(int x, int y) {
    g_goldmap[y][x] = true;
    if (g_goldobj[y * GRID_W + x] >= 0) return;
    for (int i = 0; i < MAX_GOLD; i++) {
        if (!lv_obj_has_flag(g_gobj[i], LV_OBJ_FLAG_HIDDEN)) continue;
        lv_obj_set_pos(g_gobj[i], OX + x * TILE + (TILE - 22) / 2,
                                  OY + y * TILE + (TILE - 22) / 2);
        show(g_gobj[i], true);
        g_goldobj[y * GRID_W + x] = (int16_t) i;
        return;
    }
}

// Depose un lingot lache par un garde. On tente d'abord les cases voisines, puis
// TOUTE la grille : un lingot perdu rendrait le niveau infinissable.
static void drop_gold(int cx, int cy) {
    static const int ORDER[5][2] = {{0, -1}, {0, 0}, {-1, 0}, {1, 0}, {0, 1}};
    for (int k = 0; k < 5; k++) {
        int x = cx + ORDER[k][0], y = cy + ORDER[k][1];
        if (!in_grid(x, y) || !passable(x, y) || g_goldmap[y][x]) continue;
        spawn_gold_obj(x, y);
        return;
    }
    for (int y = GRID_H - 1; y >= 0; y--)
        for (int x = 0; x < GRID_W; x++) {
            if (!passable(x, y) || g_goldmap[y][x] || !supported(x, y)) continue;
            spawn_gold_obj(x, y);
            return;
        }
}

static void finish_dig(Actor& a) {
    int tx = a.cx + a.digd, ty = a.cy + 1;
    if (!in_grid(tx, ty) || g_tile[ty][tx] != T_BRICK) return;
    if (g_hole_n >= MAX_HOLES) return;
    g_tile[ty][tx] = T_EMPTY;
    int16_t oi = g_cellobj[ty * GRID_W + tx];
    if (oi >= 0) show(g_tobj[oi], false);
    g_hole[g_hole_n].x = (int8_t) tx;
    g_hole[g_hole_n].y = (int8_t) ty;
    g_hole[g_hole_n].blink = 1;
    g_hole[g_hole_n].at = lv_tick_get() + HOLE_MS;
    g_hole_n++;
    sfx(1);
}

static void kill_guard(Actor& g, uint32_t now) {
    if (g.carry) { drop_gold(g.cx, g.cy); g.carry = false; }
    g.st = A_GONE;
    g.until = now + RESPAWN_MS;
    g.mdx = g.mdy = 0; g.sub = 0;
    show(g.body, false); show(g.head, false);
    g_score += SC_TRAP;
}

static void player_die();

// Refermeture des trous + clignotement d'avertissement.
static void update_holes(uint32_t now) {
    for (int i = 0; i < g_hole_n; ) {
        Hole& h = g_hole[i];
        int16_t oi = g_cellobj[h.y * GRID_W + h.x];
        if (now >= h.at) {
            g_tile[h.y][h.x] = T_BRICK;
            if (oi >= 0) show(g_tobj[oi], true);
            // Qui se trouve dans la brique au moment ou elle se referme ?
            for (int k = 0; k < g_guard_n; k++)
                if (g_gd[k].st != A_GONE && g_gd[k].cx == h.x && g_gd[k].cy == h.y)
                    kill_guard(g_gd[k], now);
            bool player_inside = (g_p.cx == h.x && g_p.cy == h.y);
            g_hole[i] = g_hole[--g_hole_n];
            if (player_inside) { player_die(); return; }
            continue;
        }
        if (oi >= 0 && h.at - now < HOLE_WARN_MS) {
            uint8_t b = (uint8_t) ((now / 110) & 1);
            if (b != h.blink) { h.blink = b; show(g_tobj[oi], b != 0); }
        }
        i++;
    }
}

// ===========================================================================
// 12. Creusement
// ===========================================================================

static void try_dig(int d) {
    Actor& a = g_p;
    if (g_state != ST_PLAYING) return;
    if (a.st != A_IDLE || a.mdx || a.mdy || a.sub) return;   // ni en l'air, ni en mouvement
    // On ne creuse que debout sur un vrai sol : ni suspendu a une barre,
    // ni accroche a une echelle (regle d'origine de Lode Runner).
    if (is_ladder(a.cx, a.cy) || tile_at(a.cx, a.cy) == T_BAR) return;
    uint8_t below = tile_at(a.cx, a.cy + 1);
    if (below != T_BRICK && below != T_SOLID) return;

    int tx = a.cx + d, ty = a.cy + 1;
    if (!in_grid(tx, ty)) return;
    if (g_tile[ty][tx] != T_BRICK) return;    // seul le brique se creuse
    if (!passable(tx, a.cy)) return;          // rien ne doit boucher le passage au-dessus
    if (g_goldmap[ty][tx]) return;            // pas de lingot enterre dans cette version
    if (g_hole_n >= MAX_HOLES) return;
    for (int k = 0; k < g_guard_n; k++)       // ni de garde deja sur la cible
        if (g_gd[k].st != A_GONE && g_gd[k].cx == tx && g_gd[k].cy == ty) return;

    a.face = (int8_t) d;
    a.digd = (int8_t) d;
    a.st = A_DIG;
    a.until = lv_tick_get() + DIG_MS;
}

// ===========================================================================
// 13. Deplacement du joueur
// ===========================================================================

static inline void advance(Actor& a, int spd) {
    a.sub += (int16_t) spd;
    if (a.sub >= TILE) {
        a.cx = (int8_t) (a.cx + a.mdx);
        a.cy = (int8_t) (a.cy + a.mdy);
        a.sub = 0; a.mdx = 0; a.mdy = 0;
    }
}

static uint8_t current_dir() {
    uint8_t m = g_save.ctrl_mode;
    if (m == 0) return g_btn_dir;                     // boutons seuls
    if (m == 1) return g_imu_dir;                     // inclinaison seule
    return g_btn_dir != D_NONE ? g_btn_dir : g_imu_dir;  // mixte : le doigt prime
}

static void level_clear();

static void player_enter_cell() {
    Actor& a = g_p;
    if (g_goldmap[a.cy][a.cx]) {
        g_goldmap[a.cy][a.cx] = false;
        int16_t gi = g_goldobj[a.cy * GRID_W + a.cx];
        if (gi >= 0) { show(g_gobj[gi], false); g_goldobj[a.cy * GRID_W + a.cx] = -1; }
        g_gold_left--;
        g_score += SC_GOLD;
        sfx(2);
        if (g_gold_left <= 0) open_exit();
    }
    if (g_exit_on && a.cy == 0) level_clear();
}

static void player_step(uint32_t now) {
    Actor& a = g_p;

    if (a.st == A_DIG) {
        if (now >= a.until) { finish_dig(a); a.st = A_IDLE; }
        return;
    }

    // 1) Un pas engage se termine avant toute nouvelle decision.
    if (a.mdx || a.mdy) {
        advance(a, a.st == A_FALL ? FALL_SPEED : RUN_SPEED);
        if (a.mdx || a.mdy) return;
        player_enter_cell();
        if (g_state != ST_PLAYING) return;    // la case d'arrivee a pu finir le niveau
    }

    // 2) Gravite : rien sous les pieds => on tombe, sans controle lateral.
    if (!supported(a.cx, a.cy)) {
        a.st = A_FALL;
        if (passable(a.cx, a.cy + 1)) a.mdy = 1;
        return;
    }
    a.st = A_IDLE;

    // 3) Creusement demande pendant le pas precedent : on le consomme ici.
    if (g_dig_want) {
        int8_t d = g_dig_want;
        if (now >= g_dig_want_until) { g_dig_want = 0; }
        else {
            g_dig_want = 0;
            try_dig(d);
            if (a.st == A_DIG) return;   // creusement engage : pas d'autre action
        }
    }

    // 4) Entree du joueur.
    switch (current_dir()) {
        case D_UP:
            if (is_ladder(a.cx, a.cy) && passable(a.cx, a.cy - 1)) a.mdy = -1;
            break;
        case D_DOWN:
            if (passable(a.cx, a.cy + 1)) {
                a.mdy = 1;
                // Descendre hors echelle (lacher une barre, sauter dans un trou)
                // se fait a la vitesse de chute.
                if (!is_ladder(a.cx, a.cy + 1)) a.st = A_FALL;
            }
            break;
        case D_LEFT:
        case D_RIGHT: {
            int dx = (current_dir() == D_LEFT) ? -1 : 1;
            a.face = (int8_t) dx;
            if (passable(a.cx + dx, a.cy)) a.mdx = (int8_t) dx;
            break;
        }
        default: break;
    }
}

// ===========================================================================
// 14. IA des gardes
// ===========================================================================

// BFS INVERSE depuis le joueur : g_dist[y][x] = nombre de pas pour ALLER de
// (x,y) jusqu'au joueur. On explore donc les predecesseurs (can_step(n -> c)).
static void rebuild_dist() {
    memset(g_dist, 0xFF, sizeof(g_dist));
    int head = 0, tail = 0;
    if (!in_grid(g_p.cx, g_p.cy)) return;
    g_dist[g_p.cy][g_p.cx] = 0;
    g_bfsq[tail++] = (uint16_t) (g_p.cy * GRID_W + g_p.cx);

    static const int DX[4] = {-1, 1, 0, 0};
    static const int DY[4] = {0, 0, -1, 1};
    while (head < tail) {
        uint16_t c = g_bfsq[head++];
        int x = c % GRID_W, y = c / GRID_W;
        uint8_t d = g_dist[y][x];
        if (d >= 254) continue;
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!in_grid(nx, ny) || g_dist[ny][nx] != 255) continue;
            if (!can_step(nx, ny, x, y)) continue;
            g_dist[ny][nx] = (uint8_t) (d + 1);
            g_bfsq[tail++] = (uint16_t) (ny * GRID_W + nx);
        }
    }
}

static bool guard_cell_taken(const Actor& self, int x, int y) {
    for (int i = 0; i < g_guard_n; i++) {
        const Actor& g = g_gd[i];
        if (&g == &self || g.st == A_GONE) continue;
        if (g.cx == x && g.cy == y) return true;
        if ((g.mdx || g.mdy) && g.cx + g.mdx == x && g.cy + g.mdy == y) return true;
    }
    return false;
}

// Descente du gradient de distance. En cas d'egalite ou de joueur injoignable,
// on retombe sur une heuristique de rapprochement horizontal.
static void guard_choose(Actor& a) {
    static const int DX[4] = {-1, 1, 0, 0};
    static const int DY[4] = {0, 0, -1, 1};
    uint8_t here = g_dist[a.cy][a.cx];
    uint8_t best = 255;
    int bx = 0, by = 0;

    for (int k = 0; k < 4; k++) {
        int tx = a.cx + DX[k], ty = a.cy + DY[k];
        if (!in_grid(tx, ty)) continue;
        if (!can_step(a.cx, a.cy, tx, ty)) continue;
        if (guard_cell_taken(a, tx, ty)) continue;
        uint8_t d = g_dist[ty][tx];
        if (d < best) { best = d; bx = DX[k]; by = DY[k]; }
    }

    if (best != 255 && (here == 255 || best < here)) {
        a.mdx = (int8_t) bx; a.mdy = (int8_t) by;
        if (bx) a.face = (int8_t) bx;
        return;
    }
    // Joueur injoignable : on se rapproche lateralement pour rester menacant.
    int want = (g_p.cx > a.cx) ? 1 : (g_p.cx < a.cx ? -1 : 0);
    if (want && can_step(a.cx, a.cy, a.cx + want, a.cy) &&
        !guard_cell_taken(a, a.cx + want, a.cy)) {
        a.mdx = (int8_t) want; a.face = (int8_t) want;
    }
}

static void guard_respawn(Actor& a) {
    for (int tries = 0; tries < 60; tries++) {
        int x = rnd_range(0, GRID_W - 1);
        int y = rnd_range(0, 3);
        if (!passable(x, y) || guard_cell_taken(a, x, y)) continue;
        if (iabs(x - g_p.cx) < 3 && iabs(y - g_p.cy) < 3) continue;
        place_actor(a, x, y);
        return;
    }
    place_actor(a, GRID_W / 2, 0);
}

static void guard_enter_cell(Actor& a) {
    // Un garde ramasse un lingot au passage (un seul a la fois), et le relache
    // plus tard : le joueur doit alors le piegier pour recuperer son or.
    if (!a.carry && g_goldmap[a.cy][a.cx] && rnd_range(0, 99) < 45) {
        a.carry = true;
        g_goldmap[a.cy][a.cx] = false;
        int16_t gi = g_goldobj[a.cy * GRID_W + a.cx];
        if (gi >= 0) { show(g_gobj[gi], false); g_goldobj[a.cy * GRID_W + a.cx] = -1; }
    } else if (a.carry && rnd_range(0, 99) < 4 && !g_goldmap[a.cy][a.cx]) {
        a.carry = false;
        drop_gold(a.cx, a.cy);
    }
}

static void guard_step(Actor& a, int idx, uint32_t now) {
    if (a.st == A_GONE) {
        if (now >= a.until) guard_respawn(a);
        return;
    }
    if (a.st == A_TRAP) {
        if (now < a.until) return;
        // Sortie du trou : exemption a la regle d'echelle, le garde se hisse.
        if (passable(a.cx, a.cy - 1)) { a.mdy = -1; a.sub = 0; a.st = A_OUT; }
        else a.until = now + 400;
        return;
    }

    if (a.mdx || a.mdy) {
        if (a.st == A_FALL) {
            advance(a, FALL_SPEED);
        } else {
            // Les gardes sautent 1 tick sur 5 : legerement plus lents que le joueur.
            if (((g_tick + (uint32_t) idx) % 5) == 0) return;
            advance(a, CLIMB_SPEED);
        }
        if (a.mdx || a.mdy) return;
        guard_enter_cell(a);
    }

    if (!supported(a.cx, a.cy)) {
        a.st = A_FALL;
        if (passable(a.cx, a.cy + 1)) a.mdy = 1;
        return;
    }
    // Arrive au fond d'un trou ouvert : piege.
    if (hole_index(a.cx, a.cy) >= 0) {
        a.st = A_TRAP;
        a.until = now + TRAP_MS;
        a.mdx = a.mdy = 0; a.sub = 0;
        if (a.carry) { a.carry = false; drop_gold(a.cx, a.cy - 1); }
        return;
    }
    a.st = A_IDLE;
    guard_choose(a);
}

// ===========================================================================
// 15. Vies, fin de niveau, classement
// ===========================================================================

static void record_score() {
    if (!g_run_active) return;
    g_run_active = false;
    if (g_score == 0) return;

    LodeScoreEntry e{};
    e.score = g_score;
    e.stamp = now_epoch();
    e.level = (uint8_t) (g_level + 1);
    e.ctrl_mode = g_save.ctrl_mode;
    e.flags = g_offrank ? 1 : 0;

    int n = g_save.score_count;
    int pos = n;
    for (int i = 0; i < n; i++) {
        if (e.score > g_save.scores[i].score) { pos = i; break; }
    }
    if (pos >= LODE_MAX_SCORES) return;
    for (int i = (n < LODE_MAX_SCORES ? n : LODE_MAX_SCORES - 1); i > pos; i--)
        g_save.scores[i] = g_save.scores[i - 1];
    g_save.scores[pos] = e;
    if (n < LODE_MAX_SCORES) g_save.score_count = (uint8_t) (n + 1);

    if (!g_offrank && g_score > g_save.best) g_save.best = g_score;
    persist_save();
}

static void show_clear() {
    g_state = ST_CLEAR;
    pad_sync();
    bool last = (g_level + 1 >= LODE_N_LEVELS);
    set_text_if(g_p_title, last ? "TOUS LES NIVEAUX !" : "NIVEAU TERMINE");
    char sub[96];
    snprintf(sub, sizeof(sub), "Niveau %d - %s", g_level + 1, LEVELS[g_level].name);
    set_text_if(g_p_sub, sub);
    char body[160];
    snprintf(body, sizeof(body), "Score : %lu     Vies : %d",
             (unsigned long) g_score, g_lives);
    body_center();
    set_text_if(g_p_body, body);
    set_text_if(g_p_foot, "");
    slot_list(0, last ? "Voir le classement" : "Niveau suivant", nullptr, Pal::LADDER, true);
    slot_list(1, "Retour au hub", nullptr, Pal::TXT, true);
    slots_hide_from(2);
    panel_on(true);
}

static void show_gameover() {
    record_score();
    g_state = ST_GAMEOVER;
    pad_sync();
    set_text_if(g_p_title, "PARTIE TERMINEE");
    char sub[96];
    snprintf(sub, sizeof(sub), "Niveau %d - %s", g_level + 1, LEVELS[g_level].name);
    set_text_if(g_p_sub, sub);
    char body[160];
    snprintf(body, sizeof(body), "Score : %lu%s",
             (unsigned long) g_score, g_offrank ? "   (hors classement)" : "");
    body_center();
    set_text_if(g_p_body, body);
    set_text_if(g_p_foot, "");
    slot_list(0, "Rejouer", nullptr, Pal::GOLD, true);
    slot_list(1, "Classement", nullptr, Pal::LADDER, true);
    slot_list(2, "Retour au hub", nullptr, Pal::TXT, true);
    slots_hide_from(3);
    panel_on(true);
}

static void level_clear() {
    // Bonus de temps : recompense la vitesse sans jamais devenir negatif.
    uint32_t secs = (lv_tick_get() - g_level_start) / 1000u;
    uint32_t bonus = (secs < 240u) ? (240u - secs) * 12u : 0u;
    g_score += SC_CLEAR + bonus;
    if (g_level + 1 < LODE_N_LEVELS && g_save.unlocked < g_level + 2) {
        g_save.unlocked = (uint8_t) (g_level + 2);
        persist_save();
    }
    if (g_level + 1 >= LODE_N_LEVELS) record_score();
    sfx(4);
    show_clear();
}

static void player_die() {
    if (g_state != ST_PLAYING) return;
    sfx(5);
    show(g_flash, true);
    lv_obj_move_foreground(g_flash);
    g_state = ST_DYING;
    g_die_until = lv_tick_get() + DEATH_MS;
    pad_sync();
}

static void after_death() {
    show(g_flash, false);
    if (!g_save.assist) g_lives--;
    if (g_lives <= 0) { show_gameover(); return; }
    g_state = ST_PLAYING;
    load_level(g_level);
    pad_sync();
}

static void start_level(int idx, bool new_run) {
    if (idx < 0) idx = 0;
    if (idx >= LODE_N_LEVELS) idx = LODE_N_LEVELS - 1;
    if (new_run) {
        g_lives = START_LIVES;
        g_score = 0;
        g_run_active = true;
        g_offrank = (g_save.assist != 0);
        g_save.plays++;
    }
    s_rng ^= lv_tick_get() * 2654435761u + 1u;
    load_level(idx);
    g_state = ST_PLAYING;
    panel_on(false);
    pad_sync();
    char t[96];
    snprintf(t, sizeof(t), "Niveau %d - %s", idx + 1, LEVELS[idx].name);
    toast(t);
}

// ===========================================================================
// 16. Boucle de jeu
// ===========================================================================

static void update_imu_dir() {
    float ox = g_save.cal_x / 1000.0f, oy = g_save.cal_y / 1000.0f;
    float tx = g_raw_x - ox, ty = g_raw_y - oy;
    g_tilt_x += (tx - g_tilt_x) * TILT_SMOOTH;
    g_tilt_y += (ty - g_tilt_y) * TILT_SMOOTH;

    // Rotation ecran 270 deg (meme convention que marble_game.cpp) :
    // l'axe Y physique pilote X a l'ecran, l'axe X physique pilote Y.
    float ax = -g_tilt_y, ay = g_tilt_x;
    float dead = DEAD_BASE - g_save.sensitivity * 0.014f;
    float keep = dead * 0.6f;                 // hysteresis : evite le papillonnement

    uint8_t d = D_NONE;
    if (fabsf(ax) >= fabsf(ay)) {
        if (fabsf(ax) > dead) d = (ax < 0) ? D_LEFT : D_RIGHT;
    } else {
        if (fabsf(ay) > dead) d = (ay < 0) ? D_UP : D_DOWN;
    }
    if (d == D_NONE && g_imu_dir != D_NONE) {
        float v = (g_imu_dir == D_LEFT || g_imu_dir == D_RIGHT) ? fabsf(ax) : fabsf(ay);
        if (v > keep) d = g_imu_dir;
    }
    g_imu_dir = d;
}

static void update_hud() {
    char buf[64];
    if ((int32_t) g_score != g_c_score) {
        g_c_score = (int32_t) g_score;
        snprintf(buf, sizeof(buf), "Score %lu", (unsigned long) g_score);
        set_text_if(g_hud_score, buf);
    }
    if (g_lives != g_c_lives) {
        g_c_lives = g_lives;
        if (g_save.assist) set_text_if(g_hud_lives, "Vies  oo");
        else { snprintf(buf, sizeof(buf), "Vies  %d", g_lives); set_text_if(g_hud_lives, buf); }
    }
    if (g_level != g_c_level) {
        g_c_level = g_level;
        snprintf(buf, sizeof(buf), "Niveau %d/%d  %s",
                 g_level + 1, LODE_N_LEVELS, LEVELS[g_level].name);
        set_text_if(g_hud_level, buf);
    }
    if (g_gold_left != g_c_gold) {
        g_c_gold = g_gold_left;
        if (g_gold_left > 0) snprintf(buf, sizeof(buf), "Or restant %d", g_gold_left);
        else                 snprintf(buf, sizeof(buf), "SORTIE OUVERTE");
        set_text_if(g_hud_gold, buf);
    }
    if ((int32_t) g_save.best != g_c_best) {
        g_c_best = (int32_t) g_save.best;
        snprintf(buf, sizeof(buf), "Record %lu", (unsigned long) g_save.best);
        set_text_if(g_hud_best, buf);
    }
}

static void tick_cb(lv_timer_t*) {
    uint32_t now = lv_tick_get();

    if (g_toast_until && now >= g_toast_until) { show(g_toast, false); g_toast_until = 0; }

    if (g_state == ST_DYING) {
        if (now >= g_die_until) after_death();
        return;
    }
    if (g_state != ST_PLAYING) return;

    g_tick++;

    if (g_save.ctrl_mode != 0) update_imu_dir();

    update_holes(now);
    if (g_state != ST_PLAYING) return;   // update_holes a pu tuer le joueur

    player_step(now);
    if (g_state != ST_PLAYING) return;

    // Le champ de distance est recalcule 1 tick sur 4 (~7,5 Hz) : largement
    // assez reactif pour une poursuite, 4x moins cher qu'a chaque frame.
    if ((g_tick & 3) == 0) rebuild_dist();

    for (int i = 0; i < g_guard_n; i++) {
        if (g_gd[i].st == A_GONE && g_gd[i].until == 0) continue;
        guard_step(g_gd[i], i, now);
    }

    // Contact garde / joueur. Un garde PIEGE ne tue pas : on lui marche dessus.
    int ppx = actor_px(g_p), ppy = actor_py(g_p);
    for (int i = 0; i < g_guard_n; i++) {
        const Actor& g = g_gd[i];
        if (g.st == A_GONE || g.st == A_TRAP) continue;
        if (iabs(actor_px(g) - ppx) < TILE * 3 / 5 &&
            iabs(actor_py(g) - ppy) < TILE * 3 / 5) { player_die(); return; }
    }

    // --- Rendu ---
    uint32_t pcol = (g_p.st == A_DIG && ((now / 90) & 1)) ? Pal::RUNNER_DIG : Pal::RUNNER;
    draw_actor(g_p, pcol, Pal::RUNNER_HD);
    for (int i = 0; i < g_guard_n; i++) {
        Actor& g = g_gd[i];
        uint32_t c = g.carry ? Pal::GUARD_AU : Pal::GUARD;
        if (g.st == A_TRAP) c = Pal::SOLID;      // coince : silhouette eteinte
        draw_actor(g, c, Pal::GUARD_HD);
    }
    update_hud();
}

// ===========================================================================
// 17. Ecrans
// ===========================================================================

static void go_hub() {
    g_state = ST_HUB;
    pad_sync();
    set_text_if(g_p_title, "COUREUR D'OR");
    set_text_if(g_p_sub, "Ramasse tout l'or, echappe aux gardes, grimpe en haut.");
    set_text_if(g_p_body, "");
    set_text_if(g_p_foot, "Pendant une partie : touche le bandeau du haut pour mettre en pause.");

    char d0[96], d1[64], d2[64], d3[64];
    int startlvl = g_save.unlocked;
    snprintf(d0, sizeof(d0), "Niveau %d - %s", startlvl, LEVELS[startlvl - 1].name);
    snprintf(d1, sizeof(d1), "%d/%d debloques", g_save.unlocked, LODE_N_LEVELS);
    snprintf(d2, sizeof(d2), "Meilleur score : %lu", (unsigned long) g_save.best);
    snprintf(d3, sizeof(d3), "Controle : %s", CTRL_NAME[g_save.ctrl_mode]);

    slot_list(0, "Jouer",      d0, Pal::GOLD,   true);
    slot_list(1, "Niveaux",    d1, Pal::LADDER, true);
    slot_list(2, "Classement", d2, Pal::BAR,    true);
    slot_list(3, "Reglages",   d3, Pal::TXT,    true);
    slot_list(4, "Quitter",    "Retour au tableau de bord", Pal::DANGER, true);
    slots_hide_from(5);
    panel_on(true);
}

static void go_levels() {
    g_state = ST_LEVELS;
    pad_sync();
    set_text_if(g_p_title, "NIVEAUX");
    set_text_if(g_p_sub, "Un niveau se debloque en terminant le precedent.");
    set_text_if(g_p_body, "");
    set_text_if(g_p_foot, "Un niveau termine debloque le suivant, definitivement.");
    static char names[LODE_N_LEVELS][40];
    for (int i = 0; i < LODE_N_LEVELS; i++) {
        bool on = (i < g_save.unlocked);
        snprintf(names[i], sizeof(names[i]), "%d. %s", i + 1, LEVELS[i].name);
        slot_grid(i, names[i], on ? "Jouable" : "Verrouille",
                  on ? Pal::LADDER : Pal::TXT_DIM, on);
    }
    slot_list(10, "Retour", nullptr, Pal::TXT, true);
    lv_obj_set_size(g_slot[10], 300, 58);
    lv_obj_align(g_slot[10], LV_ALIGN_BOTTOM_MID, 0, -56);
    slots_hide_from(11);
    panel_on(true);
}

static void go_settings() {
    g_state = ST_SETTINGS;
    pad_sync();
    set_text_if(g_p_title, "REGLAGES");
    set_text_if(g_p_sub, "Le Tab5 n'a pas de croix physique : ce sont des zones tactiles.");
    set_text_if(g_p_body, "");
    set_text_if(g_p_foot,
        "Un seul point de contact a la fois : en mode Boutons, on creuse a l'arret. "
        "Le mode Mixte libere le doigt pour creuser en marchant.");

    static const char* const CTRL_DESC[3] = {
        "D-pad + 2 boutons creuser",
        "Inclinaison 4 directions + 2 boutons creuser",
        "Inclinaison ET D-pad, le doigt prime",
    };
    char d1[64], d3[64];
    snprintf(d1, sizeof(d1), "%d / 5 (plus haut = plus sensible)", g_save.sensitivity + 1);
    snprintf(d3, sizeof(d3), "%s - hors classement", g_save.assist ? "Active" : "Desactive");

    char d0[96];
    snprintf(d0, sizeof(d0), "%s : %s", CTRL_NAME[g_save.ctrl_mode], CTRL_DESC[g_save.ctrl_mode]);
    slot_list(0, "Controle",           d0, Pal::LADDER, true);
    slot_list(1, "Sensibilite",        d1, Pal::BAR, g_save.ctrl_mode != 0);
    slot_list(2, "Calibrer a plat",    "Pose la tablette PUIS appuie", Pal::GOLD, true);
    slot_list(3, "Mode entrainement",  d3, Pal::TXT, true);
    slot_list(4, "Effacer scores et progression", "Irreversible", Pal::DANGER, true);
    slot_list(5, "Retour",             nullptr, Pal::TXT, true);
    slots_hide_from(6);
    panel_on(true);
}

static void go_scores() {
    g_state = ST_SCORES;
    pad_sync();
    set_text_if(g_p_title, "CLASSEMENT");
    set_text_if(g_p_sub, "Top 10 local - conserve en NVS, survit aux reboots et aux OTA.");
    set_text_if(g_p_foot, "");

    static char body[900];
    int off = 0;
    if (g_save.score_count == 0) {
        snprintf(body, sizeof(body), "Aucun score enregistre pour l'instant.");
    } else {
        for (int i = 0; i < g_save.score_count && off < (int) sizeof(body) - 90; i++) {
            const LodeScoreEntry& e = g_save.scores[i];
            char when[24];
            fmt_stamp(e.stamp, when, sizeof(when));
            int w = snprintf(body + off, sizeof(body) - off,
                             "%2d.  %7lu   niv %2d   %-11s  %s%s\n",
                             i + 1, (unsigned long) e.score, e.level,
                             CTRL_NAME[e.ctrl_mode < 3 ? e.ctrl_mode : 0], when,
                             (e.flags & 1) ? "   H.C." : "");
            // snprintf renvoie la longueur VOULUE : on borne pour ne jamais
            // pousser `off` au-dela du tampon en cas de troncature.
            if (w < 0) break;
            off += w;
            if (off >= (int) sizeof(body)) { off = (int) sizeof(body) - 1; break; }
        }
    }
    // Le classement est une colonne : aligne a gauche, contrairement aux autres
    // ecrans dont le corps est une phrase centree.
    lv_obj_set_style_text_align(g_p_body, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    set_text_if(g_p_body, body);
    slot_list(0, "Effacer les scores", "Demande confirmation", Pal::DANGER, true);
    lv_obj_align(g_slot[0], LV_ALIGN_BOTTOM_MID, 0, -136);
    slot_list(1, "Retour", nullptr, Pal::TXT, true);
    lv_obj_align(g_slot[1], LV_ALIGN_BOTTOM_MID, 0, -60);
    slots_hide_from(2);
    panel_on(true);
}

static void go_confirm() {
    g_state = ST_CONFIRM;
    pad_sync();
    set_text_if(g_p_title, "TOUT EFFACER ?");
    set_text_if(g_p_sub, "Scores, meilleur score ET progression des niveaux.");
    body_center();
    set_text_if(g_p_body, "Cette action est irreversible.");
    set_text_if(g_p_foot, "");
    slot_list(0, "Oui, tout effacer", nullptr, Pal::DANGER, true);
    slot_list(1, "Annuler",           nullptr, Pal::TXT, true);
    slots_hide_from(2);
    panel_on(true);
}

static void show_pause() {
    g_state = ST_PAUSED;
    pad_sync();
    set_text_if(g_p_title, "PAUSE");
    char sub[96];
    snprintf(sub, sizeof(sub), "Niveau %d - %s", g_level + 1, LEVELS[g_level].name);
    set_text_if(g_p_sub, sub);
    char body[160];
    snprintf(body, sizeof(body), "Score : %lu     Vies : %d     Or restant : %d",
             (unsigned long) g_score, g_lives, g_gold_left);
    body_center();
    set_text_if(g_p_body, body);
    set_text_if(g_p_foot, "");
    slot_list(0, "Reprendre",            nullptr, Pal::LADDER, true);
    slot_list(1, "Recalibrer a plat",    "Pose la tablette PUIS appuie", Pal::GOLD,
              g_save.ctrl_mode != 0);
    slot_list(2, "Relancer le niveau",   "Sans perdre de vie", Pal::BAR, true);
    slot_list(3, "Quitter la partie",    "Le score est enregistre", Pal::DANGER, true);
    slots_hide_from(4);
    panel_on(true);
}

// ===========================================================================
// 18. Evenements
// ===========================================================================

static void hud_event_cb(lv_event_t*) {
    if (g_state == ST_PLAYING) show_pause();
}

static void pad_event_cb(lv_event_t* e) {
    int id = (int) (intptr_t) lv_event_get_user_data(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (id >= 4) {
        if (code == LV_EVENT_CLICKED) on_dig(id == 5);
        return;
    }
    static const uint8_t MAP[4] = {D_LEFT, D_RIGHT, D_UP, D_DOWN};
    on_dir(MAP[id], code == LV_EVENT_PRESSED);
}

static void slot_event_cb(lv_event_t* e) {
    int i = (int) (intptr_t) lv_event_get_user_data(e);

    switch (g_state) {
        case ST_HUB:
            if (i == 0)      start_level(g_save.unlocked - 1, true);
            else if (i == 1) go_levels();
            else if (i == 2) go_scores();
            else if (i == 3) go_settings();
            else if (i == 4) close();
            break;

        case ST_LEVELS:
            if (i == 10) { go_hub(); break; }
            if (i < LODE_N_LEVELS && i < g_save.unlocked) start_level(i, true);
            break;

        case ST_SETTINGS:
            if (i == 0) {
                g_save.ctrl_mode = (uint8_t) ((g_save.ctrl_mode + 1) % 3);
                persist_save(); go_settings();
            } else if (i == 1) {
                g_save.sensitivity = (uint8_t) ((g_save.sensitivity + 1) % 5);
                persist_save(); go_settings();
            } else if (i == 2) {
                calibrate();
                set_text_if(g_p_sub, "Calibration prise.");
            } else if (i == 3) {
                g_save.assist = g_save.assist ? 0 : 1;
                persist_save(); go_settings();
            } else if (i == 4) {
                go_confirm();
            } else if (i == 5) {
                go_hub();
            }
            break;

        case ST_SCORES:
            if (i == 0) go_confirm(); else go_hub();
            break;

        case ST_CONFIRM:
            if (i == 0) {
                memset(g_save.scores, 0, sizeof(g_save.scores));
                g_save.score_count = 0;
                g_save.best = 0;
                g_save.unlocked = 1;
                persist_save();
                g_c_best = -1;
                go_hub();
            } else {
                go_hub();
            }
            break;

        case ST_PAUSED:
            if (i == 0) { g_state = ST_PLAYING; panel_on(false); pad_sync(); }
            else if (i == 1) { calibrate(); set_text_if(g_p_sub, "Calibration prise."); }
            else if (i == 2) { g_state = ST_PLAYING; panel_on(false); load_level(g_level); pad_sync(); }
            else if (i == 3) { show_gameover(); }
            break;

        case ST_CLEAR:
            if (i == 0) {
                if (g_level + 1 >= LODE_N_LEVELS) { record_score(); go_scores(); }
                else start_level(g_level + 1, false);
            } else {
                record_score();
                go_hub();
            }
            break;

        case ST_GAMEOVER:
            if (i == 0)      start_level(0, true);
            else if (i == 1) go_scores();
            else             go_hub();
            break;

        default: break;
    }
}

// ===========================================================================
// 19. API publique
// ===========================================================================

void on_imu(float ax, float ay, float /*az*/) {
    if (ax != ax || ay != ay) return;   // garde NaN
    g_raw_x = ax;
    g_raw_y = ay;
}

void on_dir(uint8_t dir, bool pressed) {
    if (pressed) g_btn_dir = dir;
    else if (g_btn_dir == dir) g_btn_dir = D_NONE;
}

void on_dig(bool right) {
    if (g_state != ST_PLAYING) return;
    g_dig_want = right ? 1 : -1;
    g_dig_want_until = lv_tick_get() + DIG_BUFFER_MS;
}

void calibrate() {
    g_save.cal_x = (int16_t) (g_raw_x * 1000.0f);
    g_save.cal_y = (int16_t) (g_raw_y * 1000.0f);
    g_tilt_x = 0; g_tilt_y = 0;
    g_imu_dir = D_NONE;
    persist_save();
}

bool is_open() { return g_state != ST_OFF; }

void open(const UI& ui) {
    if (g_state != ST_OFF) return;
    if (!ui.root || !ui.field || !ui.hud || !ui.panel || !ui.pad) return;
    g_ui = ui;

    g_epoch0 = ui.epoch;
    g_epoch_ms = lv_tick_get();

    persist_load();
    build_ui();

    show(g_ui.root, true);
    lv_obj_move_foreground(g_ui.root);

    g_run_active = false;
    g_btn_dir = D_NONE;
    g_imu_dir = D_NONE;
    g_c_score = g_c_lives = g_c_level = g_c_gold = g_c_best = -1;
    show(g_p.body, false); show(g_p.head, false);
    for (int i = 0; i < MAX_GUARDS; i++) { show(g_gd[i].body, false); show(g_gd[i].head, false); }

    go_hub();

    if (!g_timer) g_timer = lv_timer_create(tick_cb, TICK_MS, nullptr);
}

void close() {
    if (g_state == ST_OFF) return;

    // Une partie abandonnee en cours est quand meme enregistree au classement.
    record_score();
    persist_save();

    if (g_timer) { lv_timer_delete(g_timer); g_timer = nullptr; }
    show(g_ui.pad, false);
    show(g_ui.root, false);
    g_btn_dir = D_NONE;
    g_imu_dir = D_NONE;
    g_state = ST_OFF;
}

}  // namespace Lode
