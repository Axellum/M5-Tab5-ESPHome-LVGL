/**
 * [AI-CONTEXT]
 * @file marble_game.cpp
 * @role Jeu « Fil d'Or » — roguelite de bille pilote a l'inclinaison (BMI270).
 * @architecture_constraint Plein ecran 1280x720. Le YAML ne fournit que 4
 *      conteneurs vides + 3 polices ; tout le reste est construit ici. Les objets
 *      LVGL sont PREALLOUES a l'ouverture (pool) puis reutilises par show/hide +
 *      move : aucune allocation LVGL dans la boucle de jeu. Jeu ferme, il ne reste
 *      rien : objets detruits et etat rendu (struct Mem, audit du 26/09/2026).
 *      Persistance NVS via esphome::global_preferences (aucune dependance HA).
 * @ai_instruction Hot-path = tick() : pas de std::string, pas de to_string(), pas
 *      de new/delete. Les libelles HUD ne sont reecrits que quand leur valeur change.
 *      Couleurs : uniquement Pal::* (jamais d'hex en dur ici).
 */
#include "marble_game.h"
#include "game_common.h"
#include "esphome/core/preferences.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace Marble {

// ===========================================================================
// 1. Geometrie & reglages de jeu
// ===========================================================================

static constexpr int   FW = 1280;   // largeur du terrain
static constexpr int   FH = 672;    // hauteur du terrain (720 - bandeau HUD 48)
static constexpr int   HUD_H = 48;

// Rayon de reference de la bille. La caracteristique « Finesse » ne peut que le
// REDUIRE (jamais l'augmenter) : le garde-fou check_marble_rooms.py (hors depot)
// prouve la traversabilite a ce rayon maximal, donc sa preuve reste valable
// quelle que soit la progression du joueur.
static constexpr int   BALL_R = 11;
static constexpr float DT      = 0.0333f;      // pas de la boucle (30 Hz)
static constexpr int   SUBSTEP = 3;            // sous-pas de collision (anti-tunnelling)
static constexpr float SDT     = DT / SUBSTEP;

// Inclinaison -> acceleration. Deadzone volontairement basse : la tablette est
// grande, on veut que le moindre geste reponde. Le lissage evite le jitter IMU.
static constexpr float TILT_DEADZONE = 0.045f;
static constexpr float TILT_SMOOTH   = 0.38f;
static constexpr float TILT_CLAMP    = 0.85f;   // au-dela, on plafonne (g)
static constexpr float ACCEL_SCALE   = 1900.0f; // px/s^2 par g
static constexpr float FRICTION      = 0.990f;  // par frame (converti en sous-pas)
static constexpr float MAX_SPEED     = 650.0f;  // px/s
static constexpr float BOUNCE        = 0.42f;   // restitution sur les murs

// Dash : declenche par une inclinaison franche, avec recharge.
static constexpr float DASH_TILT     = 0.62f;
static constexpr uint32_t DASH_CD_MS = 900;
static constexpr float DASH_IMPULSE  = 620.0f;

static constexpr uint32_t INVULN_MS   = 1200;  // apres un degat (mode Normal)
static constexpr uint32_t VIGNETTE_MS = 260;   // flash rouge de degat
static constexpr uint32_t VELVET_MS   = 3000;  // boon « Pas de velours »

static constexpr int MAX_ENT   = 48;  // taille du pool d'entites
static constexpr int MAX_BOONS = 8;   // boons cumulables affiches au HUD
static constexpr int N_ROOMS   = 6;

// "FOR3" — bumpe a chaque changement de layout de MarbleSave :
//   FOR1 = version initiale · FOR2 = difficulte + mode dieu
//   FOR3 = ames, 6 caracteristiques, objets et equipement (systeme Dark Souls)
// Une sauvegarde d'un format anterieur est rejetee et repart a zero.
static constexpr uint32_t SAVE_MAGIC = 0x464F5233u;
static constexpr uint32_t PREF_KEY   = 0x4D41524Bu;  // cle NVS dediee au jeu

// ===========================================================================
// 2. Generateur pseudo-aleatoire (xorshift32) — seed par run
// ===========================================================================

// L'etat du generateur (Mem::rng) vit dans le bloc du jeu ouvert (section 5) :
// rnd() est donc defini apres struct Mem, seule sa declaration est ici.
static inline uint32_t rnd();
// Entier dans [lo, hi] inclus.
static inline int rnd_range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int) (rnd() % (uint32_t) (hi - lo + 1));
}

// ===========================================================================
// 3. Contenu : types d'entites, salles
// ===========================================================================

enum EKind : uint8_t {
    K_WALL = 0, K_SPIKE, K_SAW, K_PIT, K_GLUE, K_BOOST, K_WIND,
    K_ORB, K_HUNTER, K_GOLD, K_SHIELD, K_MAGNET, K_BRAKE, K_DASH, K_RUNE, K_EXIT,
    K_CHEST   // coffre au tresor : ames + chance de decouvrir un objet
};

// Convention de coordonnees :
//  - kinds statiques (murs, pieges fixes, zones, pickups) : (x,y) = coin haut-gauche
//  - kinds mobiles (K_SAW, K_ORB, K_HUNTER)               : (x,y) = centre du trajet
// a / b = amplitude|rayon|direction / periode(ms)|vitesse|force selon le kind.
struct Spec { uint8_t k; int16_t x, y, w, h, a, b; };

struct Room {
    const char* name;
    const char* line;       // punchline d'entree (1 phrase, façon Hades)
    const char* goal;       // objectif affiche au HUD
    int16_t     sx, sy;     // position de depart de la bille
    const Spec* specs;
    uint8_t     n;
    uint8_t     runes;      // runes a collecter avant ouverture du portail
    uint8_t     stars;      // difficulte (1..4), affichee au HUD
};

// --- Salle 1 : Seuil — couloirs larges, sortie visible, un seul piege ---------
static const Spec R1[] = {
    {K_WALL,     0, 210, 980,  18, 0, 0},
    {K_WALL,   300, 440, 980,  18, 0, 0},
    {K_SPIKE,  880, 316,  38,  38, 0, 0},
    {K_GOLD,   606,  92,  26,  26, 0, 0},
    {K_GOLD,   676, 316,  26,  26, 0, 0},
    {K_GOLD,   156, 556,  26,  26, 0, 0},
    {K_EXIT,  1150, 546,  56,  56, 0, 0},
};

// --- Salle 2 : Couloirs — serpentin, scie oscillante, pointes dans les passages
static const Spec R2[] = {
    {K_WALL,   200,   0,  16, 520,   0,    0},
    {K_WALL,   400, 152,  16, 520,   0,    0},
    {K_WALL,   600,   0,  16, 520,   0,    0},
    {K_WALL,   800, 152,  16, 520,   0,    0},
    {K_WALL,  1000,   0,  16, 520,   0,    0},
    {K_SPIKE,  282, 582,  36,  36,   0,    0},
    {K_SPIKE,  482,  52,  36,  36,   0,    0},
    {K_SPIKE,  682, 582,  36,  36,   0,    0},
    {K_SPIKE,  882,  52,  36,  36,   0,    0},
    {K_SAW,    708, 336, 110,  18, 185, 2600},
    {K_CHEST,  470, 480,  40,  32,   0,    0},
    {K_GOLD,    95, 307,  26,  26,   0,    0},
    {K_GOLD,   495, 337,  26,  26,   0,    0},
    {K_GOLD,   895, 337,  26,  26,   0,    0},
    {K_GOLD,  1137, 187,  26,  26,   0,    0},
    {K_EXIT,  1122, 572,  56,  56,   0,    0},
};

// --- Salle 3 : Forge — tapis d'acceleration, or place au contact des pointes ---
static const Spec R3[] = {
    {K_WALL,     0, 224, 820,  16, 0,   0},
    {K_WALL,   460, 448, 820,  16, 0,   0},
    {K_BOOST,  170,  56, 620, 140, 0, 620},   // a=0 : pousse vers la droite
    {K_BOOST,  520, 258, 700, 160, 1, 620},   // a=1 : pousse vers la gauche
    {K_BOOST,  120, 486, 700, 150, 0, 620},
    {K_CHEST,  300, 120,  40,  32, 0,   0},
    {K_SPIKE, 1031, 101,  38,  38, 0,   0},
    {K_GOLD,  1177, 107,  26,  26, 0,   0},
    {K_SPIKE,  231, 319,  38,  38, 0,   0},
    {K_GOLD,   117, 325,  26,  26, 0,   0},
    {K_SPIKE,  961, 541,  38,  38, 0,   0},
    {K_GOLD,   867, 583,  26,  26, 0,   0},
    {K_SHIELD, 605, 581,  30,  30, 0,   0},
    {K_EXIT,  1160, 552,  56,  56, 0,   0},
};

// --- Salle 4 : Sanctuaire — route haute sure vs route basse (trous) mieux dotee
static const Spec R4[] = {
    {K_WALL,   250, 300, 790,  18, 0, 0},   // separateur des deux routes
    {K_WALL,   620,   0,  18, 190, 0, 0},   // route haute : passage par le bas
    {K_WALL,   900, 110,  18, 190, 0, 0},   // route haute : passage par le haut
    {K_GOLD,   547,  77,  26,  26, 0, 0},
    {K_GOLD,   987,  47,  26,  26, 0, 0},
    {K_CHEST,  780,  40,  40,  32, 0, 0},
    {K_CHEST,  560, 600,  40,  32, 0, 0},
    {K_PIT,    400, 400,  96,  96, 0, 0},
    {K_PIT,    630, 520,  96,  96, 0, 0},
    {K_PIT,    860, 390,  96,  96, 0, 0},
    {K_SPIKE,  527, 482,  38,  38, 0, 0},
    {K_SPIKE,  762, 452,  38,  38, 0, 0},
    {K_GOLD,   487, 357,  26,  26, 0, 0},
    {K_GOLD,   740, 630,  26,  26, 0, 0},
    {K_GOLD,   947, 587,  26,  26, 0, 0},
    {K_SHIELD, 627, 357,  30,  30, 0, 0},
    {K_EXIT,  1150, 308,  56,  56, 0, 0},
};

// --- Salle 5 : Nemesis — arene ouverte, deux orbes en orbite + une chasseuse ---
static const Spec R5[] = {
    {K_WALL,    620,   0,  18,  96,   0,    0},
    {K_WALL,    620, 576,  18,  96,   0,    0},
    {K_GLUE,    470,  60, 260, 130,   0,    0},
    {K_GLUE,    470, 482, 260, 130,   0,    0},
    {K_SPIKE,   231, 101,  38,  38,   0,    0},
    {K_SPIKE,   231, 533,  38,  38,   0,    0},
    {K_SPIKE,  1021, 101,  38,  38,   0,    0},
    {K_SPIKE,  1021, 533,  38,  38,   0,    0},
    {K_CHEST,   620, 200,  40,  32,   0,    0},
    {K_ORB,     400, 336,  34,  34, 180, 3000},
    {K_ORB,     890, 336,  34,  34, 180, 3600},
    {K_HUNTER,  640, 120,  30,  30,   0,  105},
    {K_GOLD,    387, 107,  26,  26,   0,    0},
    {K_GOLD,    867, 547,  26,  26,   0,    0},
    {K_GOLD,    627, 323,  26,  26,   0,    0},
    {K_SHIELD, 1085, 321,  30,  30,   0,    0},
    {K_EXIT,   1182, 308,  56,  56,   0,    0},
};

// --- Salle 6 : Trone — 3 runes puis le portail central s'ouvre ---------------
static const Spec R6[] = {
    {K_PIT,     290, 110,  96,  96,   0,    0},
    {K_PIT,     894, 110,  96,  96,   0,    0},
    {K_PIT,     290, 466,  96,  96,   0,    0},
    {K_PIT,     894, 466,  96,  96,   0,    0},
    {K_CHEST,   400,  90,  40,  32,   0,    0},
    {K_SAW,     340, 336,  18, 160, 200, 2400},
    {K_SAW,     940, 336,  18, 160, 200, 2800},
    {K_ORB,     640, 336,  36,  36, 215, 2800},
    {K_ORB,     640, 336,  36,  36, 305, 3800},
    {K_HUNTER,  640,  90,  32,  32,   0,  130},
    {K_RUNE,    145,  82,  30,  30,   0,    0},
    {K_RUNE,   1105,  82,  30,  30,   0,    0},
    {K_RUNE,   1135, 585,  30,  30,   0,    0},
    {K_GOLD,    627, 100,  26,  26,   0,    0},
    {K_GOLD,    147, 585,  26,  26,   0,    0},
    {K_GOLD,    627, 572,  26,  26,   0,    0},
    {K_SHIELD,  625, 180,  30,  30,   0,    0},
    {K_EXIT,    612, 308,  56,  56,   0,    0},
};

#define RN(a) (uint8_t)(sizeof(a) / sizeof((a)[0]))
static const Room ROOMS[N_ROOMS] = {
    {"Seuil",      "Le fil commence ici. Ne le lache pas.",   "Rejoins le portail",   70,  90, R1, RN(R1), 0, 1},
    {"Couloirs",   "Le dedale se resserre. Respire.",         "Rejoins le portail",   90,  90, R2, RN(R2), 0, 2},
    {"Forge",      "Ici tout glisse. Meme les bonnes idees.", "Rejoins le portail",   70, 120, R3, RN(R3), 0, 2},
    {"Sanctuaire", "Deux chemins. Un seul te flattera.",      "Rejoins le portail",   70, 336, R4, RN(R4), 0, 3},
    {"Nemesis",    "Elle t'attendait. Elle attend bien.",     "Rejoins le portail",   60, 336, R5, RN(R5), 0, 4},
    {"Trone",      "Trois runes. Pas une de moins.",          "Reunis les runes",     80, 596, R6, RN(R6), 3, 4},
};
#undef RN

// ===========================================================================
// 4. Boons (progression intra-run) & ameliorations (meta)
// ===========================================================================

enum BoonId : uint8_t {
    BO_CONTROL = 0, BO_HEART, BO_PURSE, BO_MAGNET, BO_BRAKE,
    BO_SPEED, BO_BRONZE, BO_EYE, BO_REVIVE, BO_VELVET, BO_COUNT
};

struct BoonDef { const char* name; const char* desc; uint32_t color; bool unique; };
static const BoonDef BOONS[BO_COUNT] = {
    {"Main d'Ariane",  "Reponse a l'inclinaison +18 %",        Pal::BALL,   false},
    {"Coeur de braise","+1 point de vie, soigne aussitot",     Pal::DANGER, false},
    {"Bourse tressee", "+40 % d'ames ramassees",               Pal::RUNE,   false},
    {"Aimant mineur",  "Attire les bonus alentour",            Pal::MAGNET, true},
    {"Semelles lourdes","Freinage nettement plus mordant",     Pal::BRAKE,  true},
    {"Elan",           "Vitesse maximale +170",                Pal::BOOST,  false},
    {"Peau de bronze", "Un bouclier a chaque nouvelle salle",  Pal::SHIELD, true},
    {"Oeil du dedale", "Le portail pulse et se voit de loin",  Pal::EXIT,   true},
    {"Seconde chance", "Releve une fois dans la run",          Pal::WALL_LIT, true},
    {"Pas de velours", "3 s d'invulnerabilite par salle",      Pal::SLOW,   true},
};

// --- Niveaux de difficulte -------------------------------------------------
// Agissent sur 5 leviers : PV de depart, vitesse des mobiles, vitesse max de la
// bille, duree d'invulnerabilite apres un degat, et recompense en ames
// (jouer plus dur rapporte davantage — sinon personne ne monterait d'un cran).
enum DiffId : uint8_t { D_CALME = 0, D_NORMAL, D_IMPITOYABLE, D_COUNT };

struct DiffDef {
    const char* name;
    const char* desc;
    int8_t   life_delta;   // ajoute aux PV de depart
    float    hazard_mul;   // > 1 = mobiles plus rapides
    float    speed_mul;    // vitesse max de la bille
    float    gold_mul;     // multiplicateur d'ames
    uint16_t invuln_ms;
    uint32_t color;
};

static const DiffDef DIFFS[D_COUNT] = {
    {"Calme",       "+1 PV, pieges lents, longue invulnerabilite",
      1, 0.75f, 0.92f, 0.80f, 1800, Pal::EXIT},
    {"Normal",      "L'equilibre de reference",
      0, 1.00f, 1.00f, 1.00f, 1200, Pal::BALL},
    {"Impitoyable", "-1 PV, pieges rapides, mais +60 % d'ames",
     -1, 1.35f, 1.10f, 1.60f,  800, Pal::DANGER},
};

// --- Caracteristiques ameliorables (montee de niveau facon Dark Souls) ------
// Une seule monnaie : les ames. Le cout d'un niveau depend du niveau TOTAL du
// personnage, pas de la caracteristique choisie — monter n'importe quoi renchérit
// tout le reste, donc il faut choisir une orientation.
enum StatId : uint8_t { S_VITALITE = 0, S_RESISTANCE, S_FINESSE,
                        S_AGILITE, S_ELAN, S_DECOUVERTE };

struct StatDef { const char* name; const char* desc; uint8_t maxlvl; uint32_t color; };
static const StatDef STATS[MARBLE_NSTATS] = {
    {"Vitalite",   "+1 point de vie par niveau",                  5, Pal::DANGER},
    {"Resistance", "+300 ms d'invulnerabilite ; bouclier des 3",  5, Pal::SHIELD},
    {"Finesse",    "-1 px de rayon : bille plus difficile a toucher", 4, Pal::EXIT},
    {"Agilite",    "+12 % de reponse a l'inclinaison par niveau",  5, Pal::BALL},
    {"Elan",       "+60 de vitesse maximale par niveau",           5, Pal::BOOST},
    {"Decouverte", "+15 % d'ames et +12 % de butin en coffre",     5, Pal::MAGNET},
};

// Cout d'un niveau en fonction du niveau total deja atteint (courbe DS-like).
static uint32_t level_cost(uint32_t total_lvl) {
    return 60u + 14u * total_lvl + total_lvl * total_lvl;
}

// --- Objets (trouves en coffre, laches par les boss, ou achetes) ------------
enum ItemEffect : uint8_t {
    IE_HP = 0, IE_SOULS, IE_SPEED, IE_CONTROL, IE_SHIELD_ROOM,
    IE_EYE, IE_REVIVE, IE_MAGNET, IE_BRAKE, IE_GREED
};

struct ItemDef { const char* name; const char* desc; uint16_t price; uint8_t effect; uint32_t color; };
static const ItemDef ITEMS[] = {
    {"Anneau de fer",      "+1 point de vie",                    180, IE_HP,         Pal::DANGER},
    {"Talisman du filon",  "+25 % d'ames ramassees",             220, IE_SOULS,      Pal::RUNE},
    {"Plume de suie",      "+70 de vitesse maximale",            200, IE_SPEED,      Pal::BOOST},
    {"Gantelet poli",      "+15 % de reponse a l'inclinaison",   200, IE_CONTROL,    Pal::BALL},
    {"Ecaille de bronze",  "Un bouclier a chaque salle",         320, IE_SHIELD_ROOM,Pal::SHIELD},
    {"Oeil de rune",       "Le portail pulse et se voit de loin",140, IE_EYE,        Pal::EXIT},
    {"Pierre de sang",     "Releve une fois par run",            400, IE_REVIVE,     Pal::WALL_LIT},
    {"Aimant du mineur",   "Attire les bonus alentour",          260, IE_MAGNET,     Pal::MAGNET},
    {"Semelle de plomb",   "Freinage nettement plus mordant",    160, IE_BRAKE,      Pal::BRAKE},
    {"Couronne felee",     "+50 % d'ames, mais -1 point de vie", 300, IE_GREED,      Pal::SLOW},
};
static constexpr int N_ITEMS = (int) (sizeof(ITEMS) / sizeof(ITEMS[0]));
// 5 et pas 6 : avec 6 objets par page il fallait 8 lignes (6 + « Page suivante »
// + « Retour »), et la 8e descendait a y=688 alors que le pied de page commence
// a y=672 — les deux se chevauchaient. A 5, une page tient en 7 lignes (fin
// y=620) et les 10 objets se repartissent en 2 pages pleines, sans page bancale.
static constexpr int SHOP_PER_PAGE = 5;

// Punchlines FR — 1 phrase max, jamais de pave.
static const char* DEATH_LINES[] = {
    "Le fil se rompt. Il en reste toujours un bout.",
    "La pierre gagne cette manche.",
    "Tu rouleras encore.",
    "Le dedale te garde un peu plus longtemps.",
    "Chute nette. Reprends ton souffle.",
};
static const char* BOON_LINES[] = {
    "Le dedale consent.",
    "Un serment de plus.",
    "Prends. Tu en auras besoin.",
};

// ===========================================================================
// 5. Etat runtime
// ===========================================================================

enum State : uint8_t { ST_OFF = 0, ST_HUB, ST_SETTINGS, ST_LEVEL, ST_SHOP, ST_EQUIP,
                       ST_STATS, ST_PLAYING, ST_REWARD, ST_PAUSED, ST_GAMEOVER, ST_VICTORY };

struct Ent {
    uint8_t   k;
    bool      alive;      // pickups : false une fois ramasses
    int16_t   x, y, w, h; // rect courant en coordonnees terrain
    int16_t   ox, oy;     // origine (centre de trajet des mobiles)
    int16_t   a, b;
    uint16_t  phase;      // dephasage (ms) tire au sort par le seed
    lv_obj_t* obj;
    lv_obj_t* det;        // calque de detail (arete eclairee / reflet), ENFANT de obj
};

// Ce qui survit à la fermeture — quelques octets : l'état ouvert/fermé (lu par le
// registre jeu fermé), les dernières lectures IMU (dispatch_imu les envoie aussi
// jeu fermé), l'accès NVS (le recréer ferait fuir un backend de préférences par
// ouverture) et le lissage de l'inclinaison, qui continue d'une partie à l'autre
// (seul calibrate() le remet à zéro).
static NvsSlot<MarbleSave> g_nvs(PREF_KEY, SAVE_MAGIC);
static State g_state = ST_OFF;

// --- IMU / inclinaison ---
static float g_raw_x = 0.0f, g_raw_y = 0.0f;   // dernier echantillon brut (g)
static float g_tilt_x = 0.0f, g_tilt_y = 0.0f; // valeur lissee, offset applique

// --- Tailles des pools LVGL (constantes) ---
// Decor de salle : peint SOUS les entites, jamais collisionnable. Recycle d'une
// salle a l'autre exactement comme le pool d'entites.
static constexpr int MAX_DEC = 22;
// Arcs peints (anneau du portail, orbites des arenes). Pool distinct : un
// lv_arc n'est pas un lv_obj rectangulaire, il ne peut pas partager le pool du decor.
static constexpr int MAX_DEC_ARC = 4;
// 7 et pas 8, et c'est une CONTRAINTE de mise en page, pas un chiffre rond :
// les lignes sont a y = 150 + i*68 sur 62 px de haut, et le pied de page occupe
// y = 672..698. La ligne d'index 7 irait de 688 a 750 — elle passerait dessous.
// Fixer le pool a 7 rend l'invariant structurel : aucun ecran ne PEUT en demander
// une 8e. Ecran le plus charge aujourd'hui : Feu de camp (6 caracteristiques +
// Retour = 7) et Marchand (5 objets + 2 boutons = 7).
static constexpr int N_SLOTS = 7;
static_assert(150 + (N_SLOTS - 1) * 68 + 62 <= 720 - 22 - 26,
              "La derniere ligne de menu recouvre le pied de page : revoir "
              "N_SLOTS, le pas de 68 px, ou la position du pied.");

// Tout le reste n'existe que jeu ouvert : créé par open(), rendu par close(),
// avec les objets LVGL qu'il pointe (game_common.h, « Mémoire d'un jeu »).
// Les membres sans initialiseur partent de zéro comme les anciens globaux :
// game_mem_new() fait `new (p) Mem()`, qui met tout à zéro avant les initialiseurs.
struct Mem {
    // Etat du xorshift32 (section 2) : re-seme par start_run avant tout tirage.
    uint32_t rng = 0x1234567u;

    MarbleSave save{};  // relue de la NVS a chaque ouverture, ecrite par close()
    UI    ui{};
    lv_timer_t* timer = nullptr;

    // --- Bille ---
    float bx, by, vx, vy;
    uint32_t invuln_until = 0;
    uint32_t dash_ready_at = 0;

    // --- Entites ---
    Ent ent[MAX_ENT];
    int ent_n = 0;

    // --- Run en cours ---
    int      room = 0;          // index 0..5
    int      life = 3, life_max = 3;
    bool     shield = false;
    int      gold = 0;          // or ramasse dans la run
    int      runes = 0;
    uint32_t run_start_ms = 0;
    uint32_t run_ms = 0;
    // Dernier tick en partie (0 = aucun depuis le début de la run) : un écart de plus
    // de PAUSE_GAP_MS (écran éteint → lvgl.pause) n'entre pas dans le temps de la
    // run ni dans le record (audit du 25/09/2026, lot 5).
    uint32_t run_last_tick_ms = 0;
    uint32_t room_enter_ms = 0;
    bool     run_active = false;
    // Reglages figes au lancement de la run (changer la difficulte en cours de
    // partie n'aurait aucun sens : les Reglages ne sont joignables que depuis le hub).
    const DiffDef* diff = &DIFFS[D_NORMAL];
    uint32_t invuln_ms = INVULN_MS;
    bool     god = false;
    // Rayon effectif (BALL_R - Finesse) fige au lancement de la run.
    int      ball_r = BALL_R;
    // Multiplicateur d'ames issu de Decouverte + objets, et chance de butin en coffre.
    float    soul_mul = 1.0f;
    float    loot_chance = 0.35f;
    // Ecran marchand : page courante + slot d'equipement en cours de modification.
    int      shop_page = 0;
    // Index du bouton « Page suivante » sur la page courante (« Retour » = +1).
    // go_shop() l'ecrit, le gestionnaire de tap le relit : les deux DOIVENT rester
    // d'accord, sinon un tap sur « Retour » declencherait un achat.
    int      shop_rows = 0;

    // --- Effets cumules des boons ---
    uint8_t boons[MAX_BOONS];
    int     boon_n = 0;
    float   ctrl_mul, gold_mul, speed_max, fric;
    float   fric_sub;  // cache : powf(fric, 1/SUBSTEP) — recalculé uniquement quand fric change
    int     magnet_r;
    bool    has_bronze, has_eye, has_velvet;
    bool    revive_left;

    // --- Choix de recompense en cours ---
    uint8_t offer[3];
    int     offer_n = 0;

    // --- Feedback visuel ---
    uint32_t vignette_until = 0;
    int      jitter = 0;
    // Banniere ephemere en haut du terrain (decouverte d'objet, contenu d'un coffre).
    lv_obj_t* toast = nullptr;
    uint32_t  toast_until = 0;

    // --- Objets LVGL (construits a chaque ouverture, detruits a la fermeture) ---
    // Bille en TROIS calques : ombre portee, corps en degrade, reflet speculaire.
    // C'est ce trio (repris de la bille du flipper) qui la fait passer de pastille
    // plate a sphere. Ils se deplacent ensemble — voir ball_place().
    lv_obj_t* ball = nullptr;        // corps (reference historique)
    lv_obj_t* ball_sh = nullptr;     // ombre portee, legerement decalee
    lv_obj_t* ball_gloss = nullptr;  // reflet, en haut a gauche
    // Pools du decor (rectangles, arcs) et nombre d'elements pris par la salle courante.
    lv_obj_t* dec[MAX_DEC] = {};
    int dec_n = 0;
    lv_obj_t* dec_arc[MAX_DEC_ARC] = {};
    int dec_arc_n = 0;
    lv_obj_t* vign[4] = {};          // 4 bandes de bord (flash de degat)
    lv_obj_t* hud_room = nullptr;
    lv_obj_t* hud_life = nullptr;
    lv_obj_t* hud_gold = nullptr;
    lv_obj_t* hud_goal = nullptr;
    lv_obj_t* hud_time = nullptr;
    lv_obj_t* hud_dot[MAX_BOONS] = {};
    lv_obj_t* p_title = nullptr;
    lv_obj_t* p_sub = nullptr;
    lv_obj_t* p_body = nullptr;
    lv_obj_t* p_foot = nullptr;
    lv_obj_t* slot[N_SLOTS] = {};
    lv_obj_t* slot_t[N_SLOTS] = {};
    lv_obj_t* slot_d[N_SLOTS] = {};
    // Liseré d'accent de chaque slot (enfant) : reprend la couleur de l'entree.
    // C'est lui qui fait lire les menus comme des cartes et plus comme des boutons.
    lv_obj_t* slot_a[N_SLOTS] = {};

    // Caches HUD : on ne reecrit un libelle que si sa valeur a change. Ils repartent
    // « inconnus » a chaque ouverture, avec les labels neufs.
    int c_life = -1, c_gold = -1, c_runes = -1, c_sec = -1;
    bool c_shield = false;
    // Cache d'etat du portail : -1 = inconnu, sinon 0/1. Evite de reecrire le style
    // du portail a chaque frame (une ecriture de style = une invalidation LVGL).
    int c_gate = -1;
    // Même principe pour l'opacité de la bille (clignotement d'invulnérabilité) et
    // la pulsation du portail « Oeil du dédale » : -1 = inconnu, sinon la dernière
    // valeur posée. Sans eux, 3 à 4 styles réécrits à chaque tick de 33 ms
    // (audit ressources du 26/09/2026, lot 5).
    int c_ball_opa = -1;
    int c_eye_opa = -1;

    // Brouillons de texte des menus et du HUD (le label copie le texte).
    char hub_sub[168];
    char hub_play_desc[80];
    char hub_eq_desc[80];
    char settings_dtitle[64];
    char settings_gtitle[64];
    char settings_stitle[64];
    char level_sub[128];
    char level_titles[MARBLE_NSTATS][72];
    char level_descs[MARBLE_NSTATS][104];
    char shop_sub[112];
    char shop_titles[SHOP_PER_PAGE][80];
    char shop_descs[SHOP_PER_PAGE][112];
    char equip_sub[96];
    char equip_titles[MARBLE_NSLOTS][80];
    char equip_descs[MARBLE_NSLOTS][112];
    char stats_body[320];
    char stats_best[32];
    char end_body[256];
    char room_buf[96];
    char boss_buf[96];
    char hud_buf[64];
    char chest_buf[96];
};
static Mem* gs = nullptr;

// Tirage du xorshift32 (declare en section 2) : son etat vit dans le bloc.
static inline uint32_t rnd() {
    return xorshift32_next(gs->rng);
}
static inline void apply_fric() { gs->fric_sub = powf(gs->fric, 1.0f / SUBSTEP); }

static void go_hub();
static void go_settings();
static void go_level();
static void go_shop();
static void go_equip();

// Niveau total = somme des caracteristiques (pilote le cout du prochain point).
static uint32_t total_level() {
    uint32_t s = 0;
    for (int i = 0; i < MARBLE_NSTATS; i++) s += gs->save.st[i];
    return s;
}
static void show_reward();
static void load_room(int idx);
static void end_run(bool victory);

// ===========================================================================
// 6. Persistance NVS
// ===========================================================================

void persist_load() {
    if (!gs) return;  // la sauvegarde n'est en memoire que jeu ouvert
    if (!g_nvs.load(gs->save)) {
        gs->save = MarbleSave{};          // remise a zero complete
        gs->save.magic = SAVE_MAGIC;
    }
}

void persist_save() {
    if (!gs || !g_nvs.ready()) return;
    g_nvs.save(gs->save);
}

// ===========================================================================
// 7. Helpers LVGL
// ===========================================================================







// Ecrit un libelle seulement si le texte a change (evite des invalidations LVGL).
// Degrade vertical : c'est lui qui donne du volume aux pieces sans coûter
// d'objet supplementaire (une seule passe de dessin LVGL). Meme recette que la
// table du flipper — voir les paires MARBLE_*_HI / *_LO dans tab5_custom.h.
static inline void set_grad(lv_obj_t* o, uint32_t hi, uint32_t lo,
                            lv_grad_dir_t dir = LV_GRAD_DIR_VER) {
    if (!o) return;
    lv_obj_set_style_bg_color(o, lv_color_hex(hi), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(o, lv_color_hex(lo), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(o, dir, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
}

// Arc decoratif (portail, orbites peintes). Le knob et l'anneau de fond sont
// neutralises : on ne veut qu'un trait courbe, pas un widget interactif.
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

// Pose un enfant de detail dans une piece (arete eclairee, reflet, coeur sombre).
// [AI-CONTEXT] Etre ENFANT est le point important : LVGL le deplace et le masque
// avec son parent, donc le code de mouvement des mobiles (scies, orbes,
// chasseuse) n'a AUCUNE ligne a ajouter — un seul lv_obj_set_pos suffit toujours.
// Corollaire : le detail est clippe a la boite du parent, donc pas de halo
// debordant ici (les lueurs larges sont peintes dans le decor, cf. build_decor).
static inline void detail(lv_obj_t* d, int x, int y, int w, int h, int radius) {
    lv_obj_set_size(d, w, h);
    lv_obj_set_pos(d, x, y);
    lv_obj_set_style_radius(d, radius, LV_PART_MAIN);
    lv_obj_clear_flag(d, LV_OBJ_FLAG_HIDDEN);
}


// ===========================================================================
// 8. Construction de l'UI (a chaque ouverture ; close() la detruit)
// ===========================================================================

static void slot_event_cb(lv_event_t* e);

// Assombrit une couleur (pct = % de luminosite conservee). Sert a fabriquer le
// ton BAS d'un degrade a partir du ton haut : chaque pastille de bonus gagne du
// volume sans qu'on ait a inventer un token « version sombre » par pickup.
static inline uint32_t shade(uint32_t c, int pct) {
    const uint32_t r = ((c >> 16) & 0xFF) * (uint32_t) pct / 100u;
    const uint32_t g = ((c >> 8)  & 0xFF) * (uint32_t) pct / 100u;
    const uint32_t b = ( c        & 0xFF) * (uint32_t) pct / 100u;
    return (r << 16) | (g << 8) | b;
}

// --- Bille : 3 calques pilotes ensemble -------------------------------------
// [AI-CONTEXT] Tout le code de jeu passe par ces quatre fonctions et JAMAIS par
// gs->ball directement : oublier l'ombre ou le reflet les laisserait au dernier
// endroit visite, ce qui se voit immediatement a l'ecran.

// Rayon effectif (la caracteristique Finesse peut le reduire en cours de run).
static void ball_resize(int r) {
    const int d = r * 2;
    lv_obj_set_size(gs->ball_sh, d + 4, d + 4);
    lv_obj_set_size(gs->ball, d, d);
    // Reflet : ~30 % du diametre, jamais moins de 4 px (sinon il disparait a la
    // Finesse maximale, ou la bille ne fait plus que 14 px).
    int gl = d * 3 / 10;
    if (gl < 4) gl = 4;
    lv_obj_set_size(gs->ball_gloss, gl, gl);
}

// Applique le skin choisi au corps ET au reflet. Le corps est un degrade
// clair->sombre : c'est lui qui fait la sphere, le reflet ne fait que la vernir.
static void ball_apply_skin() {
    const uint32_t body  = (gs->save.skin == 1) ? Pal::BALL_ALT
                         : (gs->save.skin == 2) ? Pal::BALL_CU
                                                : Pal::BALL;
    const uint32_t gloss = (gs->save.skin == 1) ? Pal::BALL_ALT_HI
                         : (gs->save.skin == 2) ? Pal::BALL_CU_HI
                                                : Pal::BALL_HI;
    set_grad(gs->ball, gloss, shade(body, 42));
    set_bg(gs->ball_gloss, gloss, 225);
    gs->c_ball_opa = -1;   // le reflet vient d'être reposé à 225 : ball_set_opa réécrira
}

static void ball_show(bool v) {
    show(gs->ball_sh, v);
    show(gs->ball, v);
    show(gs->ball_gloss, v);
}

// (left, top) = coin haut-gauche du corps, comme l'ancien lv_obj_set_pos direct.
static void ball_place(int left, int top) {
    const int d = lv_obj_get_width(gs->ball);
    lv_obj_set_pos(gs->ball_sh, left - 2 + 3, top - 2 + 4);   // ombre decalee bas-droite
    lv_obj_set_pos(gs->ball, left, top);
    lv_obj_set_pos(gs->ball_gloss, left + d * 22 / 100, top + d * 16 / 100);
}

// Clignotement d'invulnerabilite : le trio s'efface ensemble.
static void ball_set_opa(lv_opa_t opa) {
    if (gs->c_ball_opa == (int) opa) return;
    gs->c_ball_opa = (int) opa;
    lv_obj_set_style_bg_opa(gs->ball, opa, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(gs->ball_gloss, opa == LV_OPA_COVER ? 225 : opa, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(gs->ball_sh, opa == LV_OPA_COVER ? 150 : opa, LV_PART_MAIN);
}

static void build_ui() {
    // --- Sol : degrade vertical au lieu d'un aplat -------------------------
    // Le haut plus clair simule la lumiere qui tombe du fond du donjon, le bas
    // sombre fait ressortir la bille. Une seule passe de dessin, zero objet.
    set_grad(gs->ui.field, Pal::FLOOR_HI, Pal::FLOOR_LO);
    set_grad(gs->ui.hud, Pal::HUD_BG, Pal::VOID);
    // Filet de laiton sous le HUD : separe le bandeau du terrain sans lui voler
    // de hauteur (le bandeau doit rester a 48 px, cf. marble_game.yaml). Non
    // memorise : close() le detruit en vidant le HUD.
    lv_obj_t* hud_line = mk_rect(gs->ui.hud);
    lv_obj_set_size(hud_line, FW, 2);
    lv_obj_set_pos(hud_line, 0, HUD_H - 2);
    set_bg(hud_line, Pal::BRASS_CHEST, 160);
    // Calque de menus : degrade sombre. On REPOSE l'opacite apres set_grad, qui
    // force LV_OPA_COVER — sans ca le panneau deviendrait opaque et on perdrait
    // la lecture du terrain en arriere-plan (choix d'origine du YAML, 96 %).
    set_grad(gs->ui.panel, Pal::FLOOR_LO, Pal::VOID);
    lv_obj_set_style_bg_opa(gs->ui.panel, 245, LV_PART_MAIN);

    // --- Pool de decor : cree AVANT les entites => dessine DERRIERE ---------
    // [AI-WARNING] L'ordre de creation EST l'ordre d'empilement dans LVGL. Si tu
    // deplaces ce bloc apres le pool d'entites, le decor peindra par-dessus les
    // murs et la salle deviendra illisible.
    for (int i = 0; i < MAX_DEC; i++) {
        gs->dec[i] = mk_rect(gs->ui.field);
        lv_obj_add_flag(gs->dec[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = 0; i < MAX_DEC_ARC; i++) {
        gs->dec_arc[i] = mk_arc(gs->ui.field, 0, 0, 10, 0, 360, 2,
                                Pal::EXIT, LV_OPA_TRANSP);
        lv_obj_add_flag(gs->dec_arc[i], LV_OBJ_FLAG_HIDDEN);
    }

    // --- Pool d'entites : cree a l'ouverture, recycle a chaque salle ---
    for (int i = 0; i < MAX_ENT; i++) {
        gs->ent[i].obj = mk_rect(gs->ui.field);
        lv_obj_add_flag(gs->ent[i].obj, LV_OBJ_FLAG_HIDDEN);
        // Calque de detail, enfant : suit son parent sans une ligne de code.
        gs->ent[i].det = mk_rect(gs->ent[i].obj);
        lv_obj_add_flag(gs->ent[i].det, LV_OBJ_FLAG_HIDDEN);
    }

    // --- Bille en 3 calques (creee apres le pool => dessinee au-dessus) -----
    // Ordre de creation = ordre d'empilement : ombre, puis corps, puis reflet.
    gs->ball_sh = mk_rect(gs->ui.field);
    lv_obj_set_style_radius(gs->ball_sh, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    set_bg(gs->ball_sh, Pal::BALL_SH, 150);
    lv_obj_add_flag(gs->ball_sh, LV_OBJ_FLAG_HIDDEN);

    gs->ball = mk_rect(gs->ui.field);
    lv_obj_set_style_radius(gs->ball, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_add_flag(gs->ball, LV_OBJ_FLAG_HIDDEN);

    gs->ball_gloss = mk_rect(gs->ui.field);
    lv_obj_set_style_radius(gs->ball_gloss, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    lv_obj_add_flag(gs->ball_gloss, LV_OBJ_FLAG_HIDDEN);
    ball_resize(BALL_R);
    ball_apply_skin();

    // --- Banniere de decouverte (coffres, butin de boss) ---
    gs->toast = mk_label(gs->ui.field, gs->ui.f_mid, Pal::RUNE);
    lv_obj_align(gs->toast, LV_ALIGN_TOP_MID, 0, 18);
    lv_obj_add_flag(gs->toast, LV_OBJ_FLAG_HIDDEN);

    // --- Vignette de degat : 4 bandes fines (invalide peu de pixels) ---
    const int VB = 7;
    for (int i = 0; i < 4; i++) {
        gs->vign[i] = mk_rect(gs->ui.field);
        set_bg(gs->vign[i], Pal::DANGER, LV_OPA_COVER);
        lv_obj_add_flag(gs->vign[i], LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_set_pos(gs->vign[0], 0, 0);        lv_obj_set_size(gs->vign[0], FW, VB);
    lv_obj_set_pos(gs->vign[1], 0, FH - VB);  lv_obj_set_size(gs->vign[1], FW, VB);
    lv_obj_set_pos(gs->vign[2], 0, 0);        lv_obj_set_size(gs->vign[2], VB, FH);
    lv_obj_set_pos(gs->vign[3], FW - VB, 0);  lv_obj_set_size(gs->vign[3], VB, FH);

    // --- HUD : bande compacte de 48 px, jamais plus ---
    // [AI-CONTEXT] Les abscisses ci-dessous ne sont PAS choisies a l'oeil : chaque
    // colonne reserve la largeur de son libelle le plus long, mesuree dans les
    // metriques reelles de roboto_22 (police generee par ESPHome), + 22 px de
    // gouttiere. Les anciennes valeurs (18/300/452/610) faisaient deborder
    // « Salle 4/6 - Sanctuaire *** [ DIEU ] » (348 px) sur les PV, et
    // « PV 12/12  +BOUCLIER » (217 px) sur l'or.
    //   salle    x= 18  reserve 352  (max mesure 348)
    //   PV       x=392  reserve 236  (max mesure 217)
    //   or       x=650  reserve 100  (max mesure  93)
    //   objectif x=772  reserve 172  (max mesure 164) -> fin 944
    //   1re pastille de boon a x=1016 : 72 px de marge.
    // @ai_instruction Rallonger un de ces libelles impose de refaire l'addition.
    gs->hud_room = mk_label(gs->ui.hud, gs->ui.f_small, Pal::BALL);
    lv_obj_align(gs->hud_room, LV_ALIGN_LEFT_MID, 18, 0);
    gs->hud_life = mk_label(gs->ui.hud, gs->ui.f_small, Pal::DANGER);
    lv_obj_align(gs->hud_life, LV_ALIGN_LEFT_MID, 392, 0);
    gs->hud_gold = mk_label(gs->ui.hud, gs->ui.f_small, Pal::RUNE);
    lv_obj_align(gs->hud_gold, LV_ALIGN_LEFT_MID, 650, 0);
    gs->hud_goal = mk_label(gs->ui.hud, gs->ui.f_small, Pal::EXIT);
    lv_obj_align(gs->hud_goal, LV_ALIGN_LEFT_MID, 772, 0);
    gs->hud_time = mk_label(gs->ui.hud, gs->ui.f_small, UIColor::TEXT_DIM);
    lv_obj_align(gs->hud_time, LV_ALIGN_RIGHT_MID, -18, 0);

    // Pastilles de boons actifs (compact : une pastille coloree par boon)
    for (int i = 0; i < MAX_BOONS; i++) {
        gs->hud_dot[i] = mk_rect(gs->ui.hud);
        lv_obj_set_size(gs->hud_dot[i], 16, 16);
        lv_obj_set_style_radius(gs->hud_dot[i], LV_RADIUS_CIRCLE, LV_PART_MAIN);
        lv_obj_align(gs->hud_dot[i], LV_ALIGN_RIGHT_MID, -110 + i * 22 - (MAX_BOONS - 1) * 22, 0);
        lv_obj_add_flag(gs->hud_dot[i], LV_OBJ_FLAG_HIDDEN);
    }

    // --- Panneau de menus (hub / recompense / pause / fin) ---
    gs->p_title = mk_label(gs->ui.panel, gs->ui.f_big, Pal::BALL);
    lv_obj_align(gs->p_title, LV_ALIGN_TOP_MID, 0, 56);
    gs->p_sub = mk_label(gs->ui.panel, gs->ui.f_small, UIColor::TEXT_DIM);
    lv_obj_align(gs->p_sub, LV_ALIGN_TOP_MID, 0, 122);
    gs->p_body = mk_label(gs->ui.panel, gs->ui.f_small, UIColor::TEXT_SOFT);
    lv_obj_set_width(gs->p_body, 900);
    lv_obj_set_style_text_align(gs->p_body, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(gs->p_body, LV_ALIGN_TOP_MID, 0, 170);
    gs->p_foot = mk_label(gs->ui.panel, gs->ui.f_small, UIColor::TEXT_DIM);
    lv_obj_align(gs->p_foot, LV_ALIGN_BOTTOM_MID, 0, -22);

    for (int i = 0; i < N_SLOTS; i++) {
        gs->slot[i] = mk_rect(gs->ui.panel);
        lv_obj_add_flag(gs->slot[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_radius(gs->slot[i], 14, LV_PART_MAIN);
        set_grad(gs->slot[i], Pal::FLOOR_HI, Pal::FLOOR_LO);
        // Retour tactile : le fond s'eclaircit tant que le doigt est pose.
        // Casts explicites : combiner lv_part_t et lv_state_t directement est
        // deprecie en C++20 (-Wdeprecated-enum-enum-conversion).
        lv_obj_set_style_bg_color(gs->slot[i], lv_color_hex(Pal::WALL),
                                  (lv_style_selector_t) LV_PART_MAIN |
                                  (lv_style_selector_t) LV_STATE_PRESSED);
        lv_obj_add_event_cb(gs->slot[i], slot_event_cb, LV_EVENT_CLICKED,
                            (void*) (intptr_t) i);
        // Cree AVANT les labels : le liseré doit rester derriere le texte.
        gs->slot_a[i] = mk_rect(gs->slot[i]);
        lv_obj_add_flag(gs->slot_a[i], LV_OBJ_FLAG_HIDDEN);
        gs->slot_t[i] = mk_label(gs->slot[i], gs->ui.f_mid, UIColor::TEXT_SOFT);
        gs->slot_d[i] = mk_label(gs->slot[i], gs->ui.f_small, UIColor::TEXT_DIM);
        lv_obj_add_flag(gs->slot[i], LV_OBJ_FLAG_HIDDEN);
    }
}

// --- Mise en page des slots -------------------------------------------------
// Liste verticale (menus) : 6 entrees tiennent dans 720 px.
static void slot_list(int i, const char* title, const char* desc, uint32_t col, bool on) {
    // Garde de bornes : le pool est volontairement serre (N_SLOTS = 7, cale sur
    // la place disponible au-dessus du pied de page). Un index hors pool serait
    // un debordement de tableau, pas juste une ligne mal placee.
    if (i < 0 || i >= N_SLOTS) return;
    lv_obj_set_size(gs->slot[i], 680, 62);
    lv_obj_align(gs->slot[i], LV_ALIGN_TOP_MID, 0, 150 + i * 68);
    // Les memes labels servent en mode carte (largeur fixe + texte centre) :
    // on remet explicitement la mise en forme « liste », sinon un passage par
    // l'ecran de recompense laisserait les libelles centres sur 320 px.
    lv_obj_set_width(gs->slot_t[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(gs->slot_t[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_width(gs->slot_d[i], LV_SIZE_CONTENT);
    lv_obj_set_style_text_align(gs->slot_d[i], LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_align(gs->slot_t[i], LV_ALIGN_LEFT_MID, 22, desc && desc[0] ? -13 : 0);
    lv_obj_align(gs->slot_d[i], LV_ALIGN_LEFT_MID, 22, 15);
    lv_obj_set_style_text_color(gs->slot_t[i], lv_color_hex(on ? col : UIColor::INACTIVE), LV_PART_MAIN);
    set_text_if(gs->slot_t[i], title);
    set_text_if(gs->slot_d[i], desc ? desc : "");
    set_border(gs->slot[i], on ? col : UIColor::INACTIVE, 2, LV_OPA_50);
    // Liseré vertical a gauche. [AI-WARNING] LVGL ne clippe PAS les enfants sur
    // le rayon du parent (clip_corner est off) et dessine la bordure AVANT eux :
    // un liseré pose en x=0 chevaucherait les 2 px de bordure et depasserait du
    // coin arrondi (rayon 14). D'ou x=4 et une hauteur centree hors des arrondis :
    // 62 - 2*14 = 34 px utiles, donc y=14..48.
    detail(gs->slot_a[i], 4, 14, 5, 34, 3);
    set_bg(gs->slot_a[i], on ? col : UIColor::INACTIVE, on ? LV_OPA_COVER : LV_OPA_40);
    show(gs->slot[i], true);
}

// Cartes cote a cote (choix de boon, facon Hades).
static void slot_card(int i, const char* title, const char* desc, uint32_t col) {
    if (i < 0 || i >= N_SLOTS) return;
    lv_obj_set_size(gs->slot[i], 370, 300);
    lv_obj_align(gs->slot[i], LV_ALIGN_TOP_LEFT, 85 + i * 385, 250);
    lv_obj_set_width(gs->slot_t[i], 320);
    lv_obj_set_style_text_align(gs->slot_t[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(gs->slot_t[i], LV_ALIGN_TOP_MID, 0, 48);
    lv_obj_set_width(gs->slot_d[i], 320);
    lv_obj_set_style_text_align(gs->slot_d[i], LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(gs->slot_d[i], LV_ALIGN_TOP_MID, 0, 130);
    lv_obj_set_style_text_color(gs->slot_t[i], lv_color_hex(col), LV_PART_MAIN);
    set_text_if(gs->slot_t[i], title);
    set_text_if(gs->slot_d[i], desc);
    set_border(gs->slot[i], col, 3, LV_OPA_80);
    // En mode carte le liseré passe en banniere haute : la carte se lit comme
    // une carte de boon, pas comme une ligne de menu tournee de 90 deg.
    // Meme contrainte que slot_list : rentre de 20 px pour rester a l'interieur
    // du rayon 14 et ne pas manger les 3 px de bordure.
    detail(gs->slot_a[i], 20, 6, 330, 5, 3);
    set_bg(gs->slot_a[i], col, LV_OPA_COVER);
    show(gs->slot[i], true);
}

static void slots_hide_from(int n) {
    for (int i = n; i < N_SLOTS; i++) show(gs->slot[i], false);
}

// ===========================================================================
// 9. Ecrans
// ===========================================================================

static void panel_on(bool v) {
    show(gs->ui.panel, v);
    if (v) lv_obj_move_foreground(gs->ui.panel);
}

static void go_hub() {
    g_state = ST_HUB;
    panel_on(true);
    ball_show(false);
    if (gs->save.difficulty >= D_COUNT) gs->save.difficulty = D_NORMAL;
    const DiffDef& d = DIFFS[gs->save.difficulty];

    int owned_n = 0;
    for (int i = 0; i < N_ITEMS; i++) if (gs->save.items & (1u << i)) owned_n++;

    auto& sub = gs->hub_sub;
    snprintf(sub, sizeof(sub),
             "Niveau %u   -   %u ames   -   salle %u/6   -   %s%s",
             (unsigned) total_level(), (unsigned) gs->save.souls,
             (unsigned) gs->save.deepest, d.name,
             gs->save.god ? "   -   MODE DIEU" : "");
    auto& play_desc = gs->hub_play_desc;
    snprintf(play_desc, sizeof(play_desc), "6 salles. 2 a 5 minutes.  Difficulte : %s", d.name);
    auto& eq_desc = gs->hub_eq_desc;
    snprintf(eq_desc, sizeof(eq_desc), "%d objet(s) trouve(s) sur %d", owned_n, N_ITEMS);

    set_text_if(gs->p_title, "FIL D'OR");
    set_text_if(gs->p_sub, sub);
    set_text_if(gs->p_body, "");
    set_text_if(gs->p_foot, "Incline la tablette pour guider la bille. L'ecran tactile ne sert qu'aux menus.");
    slot_list(0, "Lancer une run", play_desc, Pal::BALL, true);
    slot_list(1, "Feu de camp", "Depenser les ames en caracteristiques", Pal::DANGER, true);
    slot_list(2, "Marchand", "Acheter et revendre des objets", Pal::RUNE, true);
    slot_list(3, "Equipement", eq_desc, Pal::MAGNET, true);
    slot_list(4, "Reglages", "Difficulte, mode dieu, teinte, calibration", Pal::BOOST, true);
    slot_list(5, "Statistiques", "Runs, victoires, records", Pal::EXIT, true);
    slot_list(6, "Quitter", "Retour au tableau de bord", UIColor::TEXT_DIM, true);
    slots_hide_from(7);
}

static void go_settings() {
    g_state = ST_SETTINGS;
    panel_on(true);
    ball_show(false);
    if (gs->save.difficulty >= D_COUNT) gs->save.difficulty = D_NORMAL;
    const DiffDef& d = DIFFS[gs->save.difficulty];

    auto& dtitle = gs->settings_dtitle;
    snprintf(dtitle, sizeof(dtitle), "Difficulte : %s", d.name);
    auto& gtitle = gs->settings_gtitle;
    snprintf(gtitle, sizeof(gtitle), "Mode dieu : %s", gs->save.god ? "ACTIF" : "inactif");

    set_text_if(gs->p_title, "Reglages");
    set_text_if(gs->p_sub, "Ces reglages s'appliquent au lancement de la prochaine run.");
    set_text_if(gs->p_body, "");
    set_text_if(gs->p_foot, "Le mode dieu rend invulnerable : la run reste jouable mais ne rapporte "
                            "aucun fragment et n'entre pas dans les statistiques.");
    slot_list(0, dtitle, d.desc, d.color, true);
    slot_list(1, gtitle,
              gs->save.god ? "Invulnerable - hors concours" : "Jouer sans jamais mourir",
              gs->save.god ? Pal::MAGNET : UIColor::TEXT_DIM, true);
    static const char* SKINS[3] = {"Or", "Argent", "Cuivre"};
    auto& stitle = gs->settings_stitle;
    snprintf(stitle, sizeof(stitle), "Teinte de la bille : %s",
             SKINS[gs->save.skin < 3 ? gs->save.skin : 0]);
    slot_list(2, stitle, "Purement cosmetique", Pal::BALL, true);
    slot_list(3, "Calibrer a plat", "Pose la tablette et appuie", Pal::BOOST, true);
    slot_list(4, "Retour", "", UIColor::TEXT_DIM, true);
    slots_hide_from(5);
}

static void go_level() {
    g_state = ST_LEVEL;
    panel_on(true);
    ball_show(false);
    uint32_t lvl = total_level();
    uint32_t cost = level_cost(lvl);

    auto& sub = gs->level_sub;
    snprintf(sub, sizeof(sub),
             "Niveau %u   -   %u ames   -   prochain point : %u ames",
             (unsigned) lvl, (unsigned) gs->save.souls, (unsigned) cost);
    set_text_if(gs->p_title, "Feu de camp");
    set_text_if(gs->p_sub, sub);
    set_text_if(gs->p_body, "");
    set_text_if(gs->p_foot, "Le cout depend du niveau TOTAL : monter une caracteristique "
                            "rencherit toutes les autres. Il faut choisir une orientation.");

    auto& titles = gs->level_titles;
    auto& descs = gs->level_descs;
    for (int i = 0; i < MARBLE_NSTATS; i++) {
        const StatDef& s = STATS[i];
        bool maxed = gs->save.st[i] >= s.maxlvl;
        snprintf(titles[i], sizeof(titles[i]), "%s  %u/%u", s.name,
                 (unsigned) gs->save.st[i], (unsigned) s.maxlvl);
        if (maxed) snprintf(descs[i], sizeof(descs[i]), "%s  -  au maximum", s.desc);
        else       snprintf(descs[i], sizeof(descs[i]), "%s  -  %u ames", s.desc, (unsigned) cost);
        slot_list(i, titles[i], descs[i],
                  maxed ? Pal::EXIT : s.color,
                  !maxed && gs->save.souls >= cost);
    }
    slot_list(MARBLE_NSTATS, "Retour", "", UIColor::TEXT_DIM, true);
    slots_hide_from(MARBLE_NSTATS + 1);
}

static void go_shop() {
    g_state = ST_SHOP;
    panel_on(true);
    ball_show(false);
    int pages = (N_ITEMS + SHOP_PER_PAGE - 1) / SHOP_PER_PAGE;
    if (gs->shop_page >= pages) gs->shop_page = 0;
    int base = gs->shop_page * SHOP_PER_PAGE;
    int n = N_ITEMS - base;
    if (n > SHOP_PER_PAGE) n = SHOP_PER_PAGE;

    auto& sub = gs->shop_sub;
    snprintf(sub, sizeof(sub), "%u ames   -   page %d/%d",
             (unsigned) gs->save.souls, gs->shop_page + 1, pages);
    set_text_if(gs->p_title, "Marchand");
    set_text_if(gs->p_sub, sub);
    set_text_if(gs->p_body, "");
    set_text_if(gs->p_foot, "Appuyer sur un objet possede le revend a la moitie de son prix. "
                            "Un objet revendu est aussi retire de l'equipement.");

    auto& titles = gs->shop_titles;
    auto& descs = gs->shop_descs;
    for (int i = 0; i < n; i++) {
        int id = base + i;
        const ItemDef& it = ITEMS[id];
        bool owned = (gs->save.items & (1u << id)) != 0;
        snprintf(titles[i], sizeof(titles[i]), "%s%s", it.name, owned ? "  (possede)" : "");
        if (owned) snprintf(descs[i], sizeof(descs[i]), "%s  -  revendre : %u ames",
                            it.desc, (unsigned) (it.price / 2));
        else       snprintf(descs[i], sizeof(descs[i]), "%s  -  %u ames",
                            it.desc, (unsigned) it.price);
        slot_list(i, titles[i], descs[i], it.color,
                  owned || gs->save.souls >= it.price);
    }
    // [AI-WARNING] La navigation se place APRES le dernier objet de la page, pas
    // a un index fixe. Avec des index fixes (SHOP_PER_PAGE, +1), une page
    // incomplete laissait les slots intermediaires ni remplis ni masques : ils
    // gardaient le texte de l'ecran precedent — des lignes fantomes cliquables.
    // gs->shop_rows memorise le nombre d'objets affiches pour que le gestionnaire
    // de tap retrouve les memes index.
    gs->shop_rows = n;
    slot_list(n, "Page suivante", "", Pal::BOOST, pages > 1);
    slot_list(n + 1, "Retour", "", UIColor::TEXT_DIM, true);
    slots_hide_from(n + 2);
}

static void go_equip() {
    g_state = ST_EQUIP;
    panel_on(true);
    ball_show(false);
    int owned_n = 0;
    for (int i = 0; i < N_ITEMS; i++) if (gs->save.items & (1u << i)) owned_n++;

    auto& sub = gs->equip_sub;
    snprintf(sub, sizeof(sub), "%d objet(s) en votre possession   -   %d emplacement(s)",
             owned_n, MARBLE_NSLOTS);
    set_text_if(gs->p_title, "Equipement");
    set_text_if(gs->p_sub, sub);
    set_text_if(gs->p_body, "");
    set_text_if(gs->p_foot, owned_n ? "Appuyer sur un emplacement le fait passer a l'objet suivant."
                                    : "Aucun objet : ouvrez des coffres, battez les boss, ou passez chez le marchand.");

    auto& titles = gs->equip_titles;
    auto& descs = gs->equip_descs;
    for (int s = 0; s < MARBLE_NSLOTS; s++) {
        uint8_t e = gs->save.equip[s];
        if (e == 0 || e > N_ITEMS || !(gs->save.items & (1u << (e - 1)))) {
            snprintf(titles[s], sizeof(titles[s]), "Emplacement %d : vide", s + 1);
            snprintf(descs[s], sizeof(descs[s]), "Aucun effet actif");
            slot_list(s, titles[s], descs[s], UIColor::TEXT_DIM, owned_n > 0);
        } else {
            const ItemDef& it = ITEMS[e - 1];
            snprintf(titles[s], sizeof(titles[s]), "Emplacement %d : %s", s + 1, it.name);
            snprintf(descs[s], sizeof(descs[s]), "%s", it.desc);
            slot_list(s, titles[s], descs[s], it.color, true);
        }
    }
    slot_list(MARBLE_NSLOTS, "Retour", "", UIColor::TEXT_DIM, true);
    slots_hide_from(MARBLE_NSLOTS + 1);
}

static void go_stats() {
    g_state = ST_STATS;
    panel_on(true);
    auto& body = gs->stats_body;
    unsigned bs = gs->save.best_ms / 1000;
    int owned_n = 0;
    for (int i = 0; i < N_ITEMS; i++) if (gs->save.items & (1u << i)) owned_n++;
    auto& best = gs->stats_best;
    if (gs->save.best_ms == 0) snprintf(best, sizeof(best), "aucun");
    else snprintf(best, sizeof(best), "%u:%02u", bs / 60, bs % 60);
    snprintf(body, sizeof(body),
             "Runs lancees : %u\nVictoires : %u\nSalle la plus profonde : %u/6\n"
             "Meilleur temps : %s\nNiveau total : %u\nAmes disponibles : %u\n"
             "Objets decouverts : %d/%d",
             (unsigned) gs->save.runs, (unsigned) gs->save.wins,
             (unsigned) gs->save.deepest, best, (unsigned) total_level(),
             (unsigned) gs->save.souls, owned_n, N_ITEMS);
    set_text_if(gs->p_title, "Statistiques");
    set_text_if(gs->p_sub, "");
    set_text_if(gs->p_body, body);
    set_text_if(gs->p_foot, "Les runs jouees en mode dieu ne sont pas comptabilisees ici.");
    slot_list(0, "Retour", "", UIColor::TEXT_DIM, true);
    // Le bouton retour est place sous le bloc de texte.
    lv_obj_align(gs->slot[0], LV_ALIGN_BOTTOM_MID, 0, -90);
    slots_hide_from(1);
}

// Applique un boon a l'etat de la run.
static void apply_boon(uint8_t id) {
    if (gs->boon_n < MAX_BOONS) gs->boons[gs->boon_n++] = id;
    switch (id) {
        case BO_CONTROL: gs->ctrl_mul += 0.18f; break;
        case BO_HEART:   gs->life_max++; gs->life++; break;
        case BO_PURSE:   gs->gold_mul += 0.40f; break;
        case BO_MAGNET:  gs->magnet_r = 130; break;
        case BO_BRAKE:   gs->fric -= 0.006f; apply_fric(); break;      // friction plus forte
        case BO_SPEED:   gs->speed_max += 170.0f; break;
        case BO_BRONZE:  gs->has_bronze = true; gs->shield = true; break;
        case BO_EYE:     gs->has_eye = true; break;
        case BO_REVIVE:  gs->revive_left = true; break;
        case BO_VELVET:  gs->has_velvet = true; break;
        default: break;
    }
}

static bool boon_owned(uint8_t id) {
    for (int i = 0; i < gs->boon_n; i++) if (gs->boons[i] == id) return true;
    return false;
}

static void show_reward() {
    g_state = ST_REWARD;
    panel_on(true);
    ball_show(false);

    // Tirage de 3 boons distincts ; les boons « uniques » deja pris sont exclus.
    uint8_t pool[BO_COUNT];
    int np = 0;
    for (uint8_t i = 0; i < BO_COUNT; i++) {
        if (BOONS[i].unique && boon_owned(i)) continue;
        pool[np++] = i;
    }
    gs->offer_n = 0;
    for (int k = 0; k < 3 && np > 0; k++) {
        int p = rnd_range(0, np - 1);
        gs->offer[gs->offer_n++] = pool[p];
        pool[p] = pool[--np];
    }

    set_text_if(gs->p_title, "Le dedale offre");
    set_text_if(gs->p_sub, BOON_LINES[rnd_range(0, 2)]);
    set_text_if(gs->p_body, "");
    set_text_if(gs->p_foot, "Un seul choix. Il te suivra jusqu'a la fin de la run.");
    for (int i = 0; i < gs->offer_n; i++) {
        const BoonDef& b = BOONS[gs->offer[i]];
        slot_card(i, b.name, b.desc, b.color);
    }
    slots_hide_from(gs->offer_n);
}

static void show_end(bool victory) {
    g_state = victory ? ST_VICTORY : ST_GAMEOVER;
    panel_on(true);
    ball_show(false);

    unsigned s = gs->run_ms / 1000;
    auto& body = gs->end_body;
    if (gs->god) {
        snprintf(body, sizeof(body),
                 "Salles franchies : %d/6\nTemps : %u:%02u\nMode dieu — run hors concours",
                 victory ? 6 : gs->room, s / 60, s % 60);
    } else {
        snprintf(body, sizeof(body),
                 "Salles franchies : %d/6\nTemps : %u:%02u\nDifficulte : %s\nAmes rapportees : %d",
                 victory ? 6 : gs->room, s / 60, s % 60, gs->diff->name, gs->gold);
    }
    set_text_if(gs->p_title, victory ? "Le fil tient" : "Fin de la run");
    set_text_if(gs->p_sub, victory ? "Tu sors du dedale. Il te laisse partir."
                                   : DEATH_LINES[rnd_range(0, 4)]);
    set_text_if(gs->p_body, body);
    set_text_if(gs->p_foot, gs->god ? "Aucune ame creditee : le mode dieu ne compte pas."
                                    : "Les ames sont deja mises de cote.");
    slot_list(0, "Relancer une run", "", Pal::BALL, true);
    slot_list(1, "Retour au hub", "", UIColor::TEXT_DIM, true);
    lv_obj_align(gs->slot[0], LV_ALIGN_BOTTOM_MID, 0, -180);
    lv_obj_align(gs->slot[1], LV_ALIGN_BOTTOM_MID, 0, -100);
    slots_hide_from(2);
}

static void show_pause() {
    g_state = ST_PAUSED;
    panel_on(true);
    set_text_if(gs->p_title, "Pause");
    set_text_if(gs->p_sub, "Le dedale patiente.");
    set_text_if(gs->p_body, "");
    set_text_if(gs->p_foot, "");
    slot_list(0, "Reprendre", "", Pal::BALL, true);
    slot_list(1, "Recalibrer a plat", "Pose la tablette avant d'appuyer", Pal::BOOST, true);
    slot_list(2, "Abandonner la run", "Les ames sont conservees", Pal::DANGER, true);
    slots_hide_from(3);
}

// ===========================================================================
// 10. Chargement d'une salle
// ===========================================================================

// Un pickup ne doit jamais atterrir dans un mur ni au-dessus d'un trou :
// on annule alors le decalage de seed et on garde la position d'origine.
static bool overlaps_solid(int x, int y, int w, int h, int upto) {
    for (int i = 0; i < upto; i++) {
        const Ent& e = gs->ent[i];
        if (e.k != K_WALL && e.k != K_PIT) continue;
        if (x < e.x + e.w && x + w > e.x && y < e.y + e.h && y + h > e.y) return true;
    }
    return false;
}

// Le segment [x0,y0]->[x1,y1] est-il franchissable par une bille de rayon
// BALL_R sans traverser un mur ? Sert a garantir que le decalage de seed d'un
// pickup reste dans la MEME zone accessible : « ne pas etre dans un mur » ne
// suffit pas, un bonus pourrait sinon sauter de l'autre cote d'une paroi fine
// et devenir inatteignable. Un segment libre => connexite prouvee.
static bool segment_clear(int x0, int y0, int x1, int y1, int upto) {
    const int STEPS = 12;
    for (int s = 0; s <= STEPS; s++) {
        float t = (float) s / STEPS;
        float px = x0 + (x1 - x0) * t;
        float py = y0 + (y1 - y0) * t;
        for (int i = 0; i < upto; i++) {
            const Ent& e = gs->ent[i];
            if (e.k != K_WALL && e.k != K_PIT) continue;
            // Distance point/rectangle < BALL_R => la bille toucherait l'obstacle.
            float cx = clampf(px, e.x, (float) (e.x + e.w));
            float cy = clampf(py, e.y, (float) (e.y + e.h));
            float dx = px - cx, dy = py - cy;
            if (dx * dx + dy * dy < (float) (BALL_R * BALL_R)) return false;
        }
    }
    return true;
}

// Pastille ronde de bonus : degrade du ton vif vers son ombre + reflet. Les 6
// pickups partagent cette recette, seule la teinte change — c'est ce qui les
// fait lire comme une meme famille d'objets et pas comme 6 gommettes.
static void style_pickup(Ent& e, uint32_t col, uint32_t rim, int rim_w) {
    lv_obj_t* o = e.obj;
    set_grad(o, col, shade(col, 42));
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    if (rim_w > 0) set_border(o, rim, rim_w, LV_OPA_70);
    int g = e.w * 3 / 10;
    if (g < 3) g = 3;
    detail(e.det, e.w * 22 / 100, e.h * 16 / 100, g, g, LV_RADIUS_CIRCLE);
    set_bg(e.det, UIColor::TEXT_PRIMARY, 150);
}

// Portail de sortie. Sorti de style_entity parce que son etat change EN COURS
// de partie (il s'ouvre quand toutes les runes sont prises) : les deux appelants
// doivent produire exactement le meme rendu, sinon le portail changerait
// d'aspect au moment ou il s'ouvre pour une raison sans rapport.
static void style_exit(Ent& e, bool open_gate) {
    lv_obj_t* o = e.obj;
    const uint32_t c = open_gate ? Pal::EXIT : Pal::EXIT_OFF;
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    set_grad(o, open_gate ? Pal::EXIT_HI : c, shade(c, 30));
    lv_obj_set_style_bg_opa(o, open_gate ? 70 : 26, LV_PART_MAIN);
    set_border(o, c, 5, open_gate ? LV_OPA_COVER : LV_OPA_50);
    gs->c_eye_opa = -1;   // bordure reposée : la pulsation de l'Oeil repart d'un état connu
    // Coeur lumineux : un second anneau interieur donne la profondeur du puits.
    const int m = e.w / 5;
    detail(e.det, m, m, e.w - 2 * m, e.h - 2 * m, LV_RADIUS_CIRCLE);
    set_bg(e.det, open_gate ? Pal::EXIT_HI : c, open_gate ? 90 : 30);
}

static void style_entity(Ent& e) {
    lv_obj_t* o = e.obj;
    lv_obj_set_size(o, e.w, e.h);
    lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
    set_border(o, Pal::WALL_LIT, 0, LV_OPA_TRANSP);
    // Le pool est recycle : le detail est masque par defaut, chaque cas le
    // rallume s'il en veut un. Sans ca, une piece heriterait du detail de la
    // piece qui occupait le meme slot dans la salle precedente.
    lv_obj_add_flag(e.det, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_border_width(e.det, 0, LV_PART_MAIN);

    switch (e.k) {
        case K_WALL:
            // Bloc de pierre eclaire par le haut : degrade + arete vive au sommet.
            // C'est le changement le plus visible de la salle — les murs occupent
            // l'essentiel de l'ecran.
            set_grad(o, Pal::WALL_HI, Pal::WALL_LO);
            lv_obj_set_style_radius(o, 4, LV_PART_MAIN);
            set_border(o, shade(Pal::WALL_LO, 60), 1, LV_OPA_80);
            // y=2 : la bordure occupe la ligne 0-1, l'arete se pose juste apres
            // sans la manger. Garde sur la largeur : certaines cloisons sont fines.
            if (e.w > 8) {
                detail(e.det, 3, 2, e.w - 6, 2, 1);
                set_bg(e.det, Pal::WALL_EDGE, 190);
            }
            break;
        case K_SPIKE:
            // Lame : base sombre, arete claire. Le contour noir la detache du sol.
            set_grad(o, Pal::DANGER_HI, Pal::DANGER_LO);
            lv_obj_set_style_radius(o, 3, LV_PART_MAIN);
            set_border(o, Pal::VOID, 2, LV_OPA_80);
            // Le contour noir fait 2 px : l'arete demarre a y=3 pour le laisser
            // entier, sinon la lame semble ouverte sur le haut.
            if (e.w > 10) {
                detail(e.det, 4, 3, e.w - 8, 2, 1);
                set_bg(e.det, Pal::DANGER_HI, 230);
            }
            break;
        case K_SAW:
            // Barre de scie : degrade + gorge sombre en creux au centre (biseau).
            set_grad(o, Pal::DANGER_HI, Pal::DANGER_LO);
            lv_obj_set_style_radius(o, 6, LV_PART_MAIN);
            set_border(o, shade(Pal::DANGER_LO, 70), 1, LV_OPA_80);
            if (e.w > 10 && e.h > 10) {
                detail(e.det, 4, 4, e.w - 8, e.h - 8, 4);
                set_bg(e.det, Pal::DANGER_LO, 120);
            }
            break;
        case K_PIT:
            // Trou : margelle claire (bordure) + puits qui s'assombrit vers le bas
            // + disque noir en creux. Le vide doit se lire comme une profondeur.
            set_grad(o, Pal::PIT, 0x000000);
            lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            set_border(o, Pal::PIT_RIM, 3, LV_OPA_90);
            detail(e.det, e.w / 5, e.h / 5, e.w - 2 * (e.w / 5), e.h - 2 * (e.h / 5),
                   LV_RADIUS_CIRCLE);
            set_bg(e.det, 0x000000, 170);
            break;
        case K_GLUE:
        case K_BOOST:
        case K_WIND: {
            // Zones d'effet : nappes translucides. Le degre d'opacite est ce qui
            // les distingue d'un obstacle plein — ne pas le monter.
            const uint32_t c = (e.k == K_GLUE)  ? Pal::SLOW
                             : (e.k == K_BOOST) ? Pal::BOOST
                                                : Pal::WIND;
            const lv_opa_t a = (e.k == K_GLUE) ? 66 : (e.k == K_BOOST ? 52 : 46);
            set_grad(o, c, shade(c, 25));
            lv_obj_set_style_bg_opa(o, a, LV_PART_MAIN);
            lv_obj_set_style_radius(o, 10, LV_PART_MAIN);
            set_border(o, c, 2, LV_OPA_60);
            // Liseré clair au ras du haut : donne une surface a la nappe.
            detail(e.det, 6, 3, e.w - 12 > 0 ? e.w - 12 : 1, 2, 1);
            set_bg(e.det, c, 120);
            break;
        }
        case K_ORB:
            style_pickup(e, Pal::DANGER, 0, 0);
            set_bg(e.det, Pal::DANGER_HI, 200);
            break;
        case K_HUNTER:
            style_pickup(e, Pal::DANGER, Pal::RUNE, 3);
            set_border(o, Pal::RUNE, 3, LV_OPA_COVER);
            set_bg(e.det, Pal::DANGER_HI, 210);
            break;
        case K_GOLD:
            set_grad(o, Pal::RUNE, Pal::RUNE_LO);
            lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
            {
                int g = e.w * 3 / 10; if (g < 3) g = 3;
                detail(e.det, e.w * 20 / 100, e.h * 15 / 100, g, g, LV_RADIUS_CIRCLE);
                set_bg(e.det, Pal::BALL_HI, 190);
            }
            break;
        case K_SHIELD: style_pickup(e, Pal::SHIELD, UIColor::TEXT_PRIMARY, 2); break;
        case K_MAGNET: style_pickup(e, Pal::MAGNET, 0, 0); break;
        case K_BRAKE:  style_pickup(e, Pal::BRAKE,  0, 0); break;
        case K_DASH:   style_pickup(e, Pal::DASH,   0, 0); break;
        case K_RUNE:
            // Carre a coins doux : se distingue au premier coup d'oeil des pickups ronds.
            set_grad(o, Pal::RUNE, Pal::RUNE_LO);
            lv_obj_set_style_radius(o, 5, LV_PART_MAIN);
            set_border(o, UIColor::TEXT_PRIMARY, 3, LV_OPA_90);
            detail(e.det, 4, 3, e.w - 8 > 0 ? e.w - 8 : 1, 2, 1);
            set_bg(e.det, Pal::BALL_HI, 200);
            break;
        case K_CHEST:
            // Coffre : rectangle trapu cercle d'or, volontairement different
            // des pastilles rondes de bonus — on doit le reperer de loin.
            // La ferrure horizontale est ce qui le fait lire comme un coffre.
            set_grad(o, Pal::BRASS_CHEST, Pal::CHEST_LO);
            lv_obj_set_style_radius(o, 6, LV_PART_MAIN);
            set_border(o, Pal::RUNE, 3, LV_OPA_COVER);
            // Rentree de 3 px de chaque cote : la bordure d'or du coffre fait
            // 3 px, une ferrure pleine largeur la recouvrirait aux deux bouts.
            if (e.w > 8) {
                detail(e.det, 3, e.h / 2 - 2, e.w - 6, 4, 0);
                set_bg(e.det, Pal::RUNE, 170);
            }
            break;
        case K_EXIT:
            style_exit(e, false);   // etat reel pose par le tick des la 1re frame
            break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Decor de salle : peinture au sol, JAMAIS de collision.
// [AI-CONTEXT] Meme principe que la serigraphie sous le verre d'un flipper : la
// bille passe par-dessus, la physique ignore tout ce qui est cree ici. C'est
// donc le seul endroit du jeu ou on peut ajouter du visuel sans repasser par
// check_marble_rooms.py (hors depot ; il ne lit que les Spec des salles).
// @ai_instruction Si tu ajoutes un element, prends-le dans le pool (dec_next /
//      dec_arc) et ne depasse pas MAX_DEC / MAX_DEC_ARC : le pool est alloue a
//      l'ouverture du jeu, aucune allocation ne doit avoir lieu en cours de partie.
// ---------------------------------------------------------------------------
static lv_obj_t* dec_next() {
    if (gs->dec_n >= MAX_DEC) return nullptr;
    lv_obj_t* o = gs->dec[gs->dec_n++];
    lv_obj_set_style_border_width(o, 0, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 0, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_NONE, LV_PART_MAIN);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    return o;
}

// Rayon maximal d'un disque centre en (cx,cy) qui tient ENTIEREMENT dans le
// terrain. [AI-CONTEXT] LVGL clippe au conteneur : un disque trop grand ne
// deborde pas, il est tranche net — et un arc de cercle coupe par une droite se
// voit immediatement. Les departs de salle et plusieurs portails sont a moins de
// 100 px d'un bord, donc les rayons nominaux ci-dessous DOIVENT etre rabotes.
static inline int fit_radius(int cx, int cy, int r) {
    int m = cx;
    if (cy < m) m = cy;
    if (FW - cx < m) m = FW - cx;
    if (FH - cy < m) m = FH - cy;
    return r < m ? r : (m > 0 ? m : 0);
}

// Nappe de lumiere : disque large et tres transparent. Designe un lieu (le
// depart, le portail, une torche) sans ajouter une ligne de HUD.
// Le rayon demande est un MAXIMUM : il est reduit si le bord est trop proche.
static void dec_light(int cx, int cy, int r, uint32_t col, lv_opa_t opa) {
    r = fit_radius(cx, cy, r);
    if (r < 4) return;
    lv_obj_t* o = dec_next();
    if (!o) return;
    lv_obj_set_size(o, r * 2, r * 2);
    lv_obj_set_pos(o, cx - r, cy - r);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, LV_PART_MAIN);
    set_bg(o, col, opa);
}

// Meme regle que dec_light : le rayon demande est un maximum. Un anneau tranche
// par le bord du terrain est bien plus laid qu'un anneau un peu plus petit.
static void dec_arc(int cx, int cy, int r, int a0, int a1, int w,
                    uint32_t col, lv_opa_t opa) {
    r = fit_radius(cx, cy, r) - (w + 1) / 2;   // l'epaisseur du trait compte aussi
    if (r < 8) return;
    if (gs->dec_arc_n >= MAX_DEC_ARC) return;
    lv_obj_t* a = gs->dec_arc[gs->dec_arc_n++];
    lv_obj_set_style_arc_width(a, w, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(a, lv_color_hex(col), LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(a, opa, LV_PART_INDICATOR);
    lv_arc_set_bg_angles(a, a0, a1);
    lv_arc_set_angles(a, a0, a1);
    const int d = r * 2 + w;
    lv_obj_set_size(a, d, d);
    lv_obj_set_pos(a, cx - d / 2, cy - d / 2);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_HIDDEN);
}

static void build_decor(int idx) {
    for (int i = 0; i < gs->dec_n; i++)     show(gs->dec[i], false);
    for (int i = 0; i < gs->dec_arc_n; i++) show(gs->dec_arc[i], false);
    gs->dec_n = 0;
    gs->dec_arc_n = 0;
    const Room& r = ROOMS[idx];

    // Joints de dalles : 3 lignes a peine visibles. Elles donnent une echelle au
    // sol — sans elles la bille semble flotter dans le vide.
    for (int i = 0; i < 3; i++) {
        lv_obj_t* o = dec_next();
        if (!o) break;
        lv_obj_set_size(o, FW, 2);
        lv_obj_set_pos(o, 0, 168 + i * 168);
        set_bg(o, Pal::SLAB, 110);
    }

    // Torches murales : une braise vive dans un halo large. Les positions
    // nominales sont fixes, mais une salle peut y avoir plante une cloison —
    // une braise enterree sous un mur ne se voit pas et fait tache la ou elle
    // depasse. On decale donc lateralement, et on renonce si rien n'est libre.
    static const int TORCH[4][2] = {{210, 24}, {1070, 24}, {210, FH - 24}, {1070, FH - 24}};
    static const int SHIFT[5] = {0, -70, 70, -140, 140};
    for (int t = 0; t < 4; t++) {
        for (int s = 0; s < 5; s++) {
            const int tx = TORCH[t][0] + SHIFT[s], ty = TORCH[t][1];
            if (tx < 30 || tx > FW - 30) continue;
            bool blocked = false;
            for (int i = 0; i < r.n && !blocked; i++) {
                const Spec& sp = r.specs[i];
                if (sp.k != K_WALL) continue;
                // Marge de 8 px : une braise collee a un mur se lit mal aussi.
                blocked = (tx >= sp.x - 8 && tx <= sp.x + sp.w + 8 &&
                           ty >= sp.y - 8 && ty <= sp.y + sp.h + 8);
            }
            if (blocked) continue;
            dec_light(tx, ty, 46, Pal::EMBER, 20);
            dec_light(tx, ty, 6,  Pal::EMBER, 210);
            break;
        }
    }

    // Nappe chaude au point de depart : le joueur voit d'ou il part.
    dec_light(r.sx, r.sy, 84, Pal::EMBER, 22);

    // Anneau du portail : deux arcs peints autour de la sortie. C'est le repere
    // le plus utile de la salle, il merite d'etre lisible de loin.
    for (int i = 0; i < r.n; i++) {
        if (r.specs[i].k != K_EXIT) continue;
        const int ex = r.specs[i].x + r.specs[i].w / 2;
        const int ey = r.specs[i].y + r.specs[i].h / 2;
        dec_light(ex, ey, 96, Pal::EXIT, 20);
        dec_arc(ex, ey, 62, 0, 360, 3, Pal::EXIT, 70);
        dec_arc(ex, ey, 78, 210, 330, 2, Pal::EXIT, 45);
        break;
    }

    // Arenes de boss (Nemesis, Trone) : orbite peinte au centre. Elle annonce la
    // trajectoire des orbes avant meme qu'ils ne bougent.
    if (idx >= 4) {
        dec_arc(FW / 2, FH / 2, 196, 0, 360, 2, Pal::DANGER, 34);
        dec_light(FW / 2, FH / 2, 150, Pal::DANGER, 12);
    }
}

static void load_room(int idx) {
    const Room& r = ROOMS[idx];
    gs->ent_n = 0;
    build_decor(idx);

    for (int i = 0; i < r.n && gs->ent_n < MAX_ENT; i++) {
        const Spec& s = r.specs[i];
        Ent& e = gs->ent[gs->ent_n];
        e.k = s.k;
        e.alive = true;
        e.w = s.w; e.h = s.h;
        e.a = s.a; e.b = s.b;
        // Difficulte : les mobiles vont plus ou moins vite. Pour les scies et
        // les orbes, `b` est une PERIODE => on la divise pour accelerer ;
        // pour la chasseuse, `b` est une vitesse => on la multiplie.
        if (s.k == K_SAW || s.k == K_ORB) {
            int nb = (int) (s.b / gs->diff->hazard_mul);
            if (nb < 300) nb = 300;          // garde-fou : jamais injouable
            e.b = (int16_t) nb;
        } else if (s.k == K_HUNTER) {
            e.b = (int16_t) (s.b * gs->diff->hazard_mul);
        }
        e.phase = (uint16_t) rnd_range(0, e.b > 0 ? e.b - 1 : 0);

        if (s.k == K_SAW || s.k == K_ORB || s.k == K_HUNTER) {
            // Mobiles : (x,y) est le centre du trajet.
            e.ox = s.x; e.oy = s.y;
            e.x = (int16_t) (s.x - s.w / 2);
            e.y = (int16_t) (s.y - s.h / 2);
        } else {
            e.x = s.x; e.y = s.y;
            e.ox = s.x; e.oy = s.y;
            // Variante de seed : les pickups bougent un peu d'une run a l'autre,
            // uniquement si la nouvelle position reste hors des murs.
            if (s.k == K_GOLD || s.k == K_SHIELD || s.k == K_MAGNET ||
                s.k == K_BRAKE || s.k == K_DASH) {
                int jx = e.x + rnd_range(-28, 28);
                int jy = e.y + rnd_range(-28, 28);
                // Trois conditions : dans le terrain, pas dans un obstacle, et
                // relie en ligne droite a la position d'origine (connexite).
                if (jx >= 8 && jy >= 8 && jx + e.w <= FW - 8 && jy + e.h <= FH - 8 &&
                    !overlaps_solid(jx, jy, e.w, e.h, gs->ent_n) &&
                    segment_clear(e.x + e.w / 2, e.y + e.h / 2,
                                  jx + e.w / 2, jy + e.h / 2, gs->ent_n)) {
                    e.x = (int16_t) jx; e.y = (int16_t) jy;
                }
            }
        }

        style_entity(e);
        lv_obj_set_pos(e.obj, e.x, e.y);
        show(e.obj, true);
        gs->ent_n++;
    }
    for (int i = gs->ent_n; i < MAX_ENT; i++) show(gs->ent[i].obj, false);

    // Placement de la bille + reinitialisation de la physique.
    gs->bx = r.sx; gs->by = r.sy;
    gs->vx = 0; gs->vy = 0;
    gs->runes = 0;
    gs->room_enter_ms = lv_tick_get();
    if (gs->has_bronze) gs->shield = true;
    gs->invuln_until = gs->has_velvet ? gs->room_enter_ms + VELVET_MS : 0;

    ball_place((int) gs->bx - gs->ball_r, (int) gs->by - gs->ball_r);
    ball_show(true);
    // Remise au premier plan dans l'ordre d'empilement voulu : ombre, corps,
    // reflet. Les remonter dans le desordre mettrait l'ombre PAR-DESSUS la bille.
    lv_obj_move_foreground(gs->ball_sh);
    lv_obj_move_foreground(gs->ball);
    lv_obj_move_foreground(gs->ball_gloss);
    for (int i = 0; i < 4; i++) lv_obj_move_foreground(gs->vign[i]);

    // Nom + numero de salle : ecrits une seule fois par salle.
    auto& rbuf = gs->room_buf;
    snprintf(rbuf, sizeof(rbuf), "Salle %d/%d  -  %s  %.*s%s", idx + 1, N_ROOMS, r.name,
             (int) r.stars, "****", gs->god ? "   [ DIEU ]" : "");
    set_text_if(gs->hud_room, rbuf);
    lv_obj_set_style_text_color(gs->hud_room,
        lv_color_hex(gs->god ? Pal::MAGNET : Pal::BALL), LV_PART_MAIN);

    // Reset des caches HUD pour forcer un repaint complet du bandeau.
    gs->c_life = -1; gs->c_gold = -1; gs->c_runes = -1; gs->c_sec = -1; gs->c_gate = -1;
    gs->c_eye_opa = -1;
}

static void start_run() {
    gs->rng = lv_tick_get() ^ 0x9E3779B9u ^ (gs->save.runs * 2654435761u);
    if (gs->rng == 0) gs->rng = 0x1234567u;

    // Fige les reglages pour toute la duree de la run.
    if (gs->save.difficulty >= D_COUNT) gs->save.difficulty = D_NORMAL;
    gs->diff = &DIFFS[gs->save.difficulty];
    gs->god = (gs->save.god != 0);

    // --- Caracteristiques ---
    const uint8_t* st = gs->save.st;
    gs->invuln_ms = gs->diff->invuln_ms + 300u * st[S_RESISTANCE];
    gs->ball_r = BALL_R - (int) st[S_FINESSE];
    if (gs->ball_r < 5) gs->ball_r = 5;
    gs->life_max = 3 + (int) st[S_VITALITE] + gs->diff->life_delta;
    gs->ctrl_mul = 1.0f + 0.12f * st[S_AGILITE];
    gs->speed_max = (MAX_SPEED + 60.0f * st[S_ELAN]) * gs->diff->speed_mul;
    gs->soul_mul = 1.0f + 0.15f * st[S_DECOUVERTE];
    gs->loot_chance = 0.35f + 0.12f * st[S_DECOUVERTE];

    gs->shield = false;
    gs->gold = 0;
    gs->boon_n = 0;
    gs->gold_mul = gs->diff->gold_mul;
    gs->fric = FRICTION;
    apply_fric();
    gs->magnet_r = 0;
    // Resistance 3+ : un bouclier offert a chaque salle (comme « Peau de bronze »).
    gs->has_bronze = (st[S_RESISTANCE] >= 3);
    gs->has_eye = false; gs->has_velvet = false;
    gs->revive_left = false;

    // --- Objets equipes ---
    for (int s = 0; s < MARBLE_NSLOTS; s++) {
        uint8_t e = gs->save.equip[s];
        if (e == 0 || e > N_ITEMS) continue;
        switch (ITEMS[e - 1].effect) {
            case IE_HP:          gs->life_max++; break;
            case IE_SOULS:       gs->soul_mul += 0.25f; break;
            case IE_SPEED:       gs->speed_max += 70.0f; break;
            case IE_CONTROL:     gs->ctrl_mul += 0.15f; break;
            case IE_SHIELD_ROOM: gs->has_bronze = true; break;
            case IE_EYE:         gs->has_eye = true; break;
            case IE_REVIVE:      gs->revive_left = true; break;
            case IE_MAGNET:      gs->magnet_r = 130; break;
            case IE_BRAKE:       gs->fric -= 0.005f; apply_fric(); break;
            case IE_GREED:       gs->soul_mul += 0.50f; gs->life_max--; break;
            default: break;
        }
    }

    if (gs->life_max < 1) gs->life_max = 1;
    gs->life = gs->life_max;
    ball_resize(gs->ball_r);
    gs->room = 0;
    gs->run_start_ms = lv_tick_get();
    gs->run_ms = 0;
    gs->run_last_tick_ms = 0;
    gs->run_active = true;

    // Une run en mode dieu ne compte pas : ni au compteur, ni au classement.
    if (!gs->god) { gs->save.runs++; persist_save(); }

    load_room(0);
    g_state = ST_PLAYING;
    panel_on(false);
}

static void end_run(bool victory) {
    if (!gs->run_active) { show_end(victory); return; }
    gs->run_active = false;

    // Mode dieu : rien n'est credite ni enregistre — l'invulnerabilite viderait
    // la meta-progression de son sens. La run reste jouable, juste hors concours.
    if (gs->god) { gs->gold = 0; show_end(victory); return; }

    // Banque des ames : butin de la run x difficulte x Decouverte/objets x boon Bourse.
    uint32_t earned = (uint32_t) (gs->gold * gs->gold_mul * gs->soul_mul);
    gs->save.souls += earned;
    gs->gold = (int) earned;   // affiche le montant reellement credite

    uint8_t reached = (uint8_t) (victory ? 6 : (gs->room + 1));
    if (reached > gs->save.deepest) gs->save.deepest = reached;
    if (victory) {
        gs->save.wins++;
        if (gs->save.best_ms == 0 || gs->run_ms < gs->save.best_ms) gs->save.best_ms = gs->run_ms;
    }
    persist_save();
    show_end(victory);
}

// ===========================================================================
// 11. Physique & boucle de jeu
// ===========================================================================

// Resolution cercle vs rectangle : repousse la bille et reflechit la vitesse.
static void resolve_wall(const Ent& e) {
    float cx = clampf(gs->bx, e.x, (float) (e.x + e.w));
    float cy = clampf(gs->by, e.y, (float) (e.y + e.h));
    float dx = gs->bx - cx, dy = gs->by - cy;
    float d2 = dx * dx + dy * dy;
    if (d2 >= (float) (gs->ball_r * gs->ball_r)) return;

    float d = sqrtf(d2);
    if (d < 0.001f) {
        // Centre exactement sur l'arete : on ressort par l'axe le moins enfonce.
        float left = gs->bx - e.x, right = (e.x + e.w) - gs->bx;
        float top = gs->by - e.y, bot = (e.y + e.h) - gs->by;
        float m = left; float nx = -1, ny = 0;
        if (right < m) { m = right; nx = 1; ny = 0; }
        if (top < m)   { m = top;   nx = 0; ny = -1; }
        if (bot < m)   {            nx = 0; ny = 1; }
        gs->bx = cx + nx * gs->ball_r;
        gs->by = cy + ny * gs->ball_r;
        if (nx != 0) gs->vx = -gs->vx * BOUNCE; else gs->vy = -gs->vy * BOUNCE;
        return;
    }
    float nx = dx / d, ny = dy / d;
    gs->bx = cx + nx * gs->ball_r;
    gs->by = cy + ny * gs->ball_r;
    float dot = gs->vx * nx + gs->vy * ny;
    if (dot < 0) {
        gs->vx -= (1.0f + BOUNCE) * dot * nx;
        gs->vy -= (1.0f + BOUNCE) * dot * ny;
    }
}

static bool circle_hits(const Ent& e) {
    float cx = clampf(gs->bx, e.x, (float) (e.x + e.w));
    float cy = clampf(gs->by, e.y, (float) (e.y + e.h));
    float dx = gs->bx - cx, dy = gs->by - cy;
    return dx * dx + dy * dy < (float) (gs->ball_r * gs->ball_r);
}

static bool inside_zone(const Ent& e) {
    return gs->bx > e.x && gs->bx < e.x + e.w && gs->by > e.y && gs->by < e.y + e.h;
}

static void flash_damage() {
    gs->vignette_until = lv_tick_get() + VIGNETTE_MS;
    gs->jitter = 5;
    for (int i = 0; i < 4; i++) show(gs->vign[i], true);
}

// Retourne true si la run est terminee.
static bool take_damage(bool from_pit) {
    uint32_t now = lv_tick_get();
    if (now < gs->invuln_until) return false;

    // Mode dieu : aucun degat. On replace quand meme la bille si elle est
    // tombee dans un trou, sinon elle resterait coincee dans le vide.
    if (gs->god) {
        if (from_pit) {
            gs->bx = ROOMS[gs->room].sx; gs->by = ROOMS[gs->room].sy;
            gs->vx = gs->vy = 0;
        }
        return false;
    }

    if (gs->shield) {
        gs->shield = false;
        gs->invuln_until = now + gs->invuln_ms;
        flash_damage();
        if (from_pit) {
            gs->bx = ROOMS[gs->room].sx; gs->by = ROOMS[gs->room].sy;
            gs->vx = gs->vy = 0;
        }
        return false;
    }

    gs->life--;
    gs->invuln_until = now + INVULN_MS;
    flash_damage();

    if (gs->life <= 0) {
        if (gs->revive_left) {
            gs->revive_left = false;
            gs->life = 1;
            gs->bx = ROOMS[gs->room].sx; gs->by = ROOMS[gs->room].sy;
            gs->vx = gs->vy = 0;
            gs->invuln_until = now + gs->invuln_ms * 2;
            return false;
        }
        end_run(false);
        return true;
    }

    // Repart du depart de la salle : plus lisible qu'un knockback aleatoire.
    gs->bx = ROOMS[gs->room].sx; gs->by = ROOMS[gs->room].sy;
    gs->vx = gs->vy = 0;
    return false;
}

// Affiche une banniere ephemere en haut du terrain (2,5 s).
static void toast(const char* txt, uint32_t color) {
    if (!gs->toast) return;
    set_text_if(gs->toast, txt);
    lv_obj_set_style_text_color(gs->toast, lv_color_hex(color), LV_PART_MAIN);
    show(gs->toast, true);
    lv_obj_move_foreground(gs->toast);
    gs->toast_until = lv_tick_get() + 2500;
}

// Accorde un objet non encore possede. Retourne son index, ou -1 si la
// collection est deja complete (le butin est alors converti en ames).
static int grant_random_item() {
    uint8_t pool[N_ITEMS];
    int np = 0;
    for (int i = 0; i < N_ITEMS; i++)
        if (!(gs->save.items & (1u << i))) pool[np++] = (uint8_t) i;
    if (np == 0) return -1;
    int pick = pool[rnd_range(0, np - 1)];
    gs->save.items |= (1u << pick);
    persist_save();
    return pick;
}

// Butin garanti apres un boss (salles 5 et 6).
static void boss_reward(const char* who) {
    auto& buf = gs->boss_buf;
    int it = grant_random_item();
    if (it >= 0) {
        snprintf(buf, sizeof(buf), "%s cede : %s", who, ITEMS[it].name);
        toast(buf, ITEMS[it].color);
    } else {
        gs->gold += 120;
        snprintf(buf, sizeof(buf), "%s cede 120 ames", who);
        toast(buf, Pal::RUNE);
    }
}

static void next_room() {
    // Recompense apres les salles 2 et 4 (index 1 et 3), comme prevu au design.
    int done = gs->room + 1;
    // Butin de boss : Nemesis (salle 5) et le Trone (salle 6) laissent un objet.
    if (done == 5) boss_reward("Nemesis");
    if (done >= N_ROOMS) { boss_reward("Le Trone"); end_run(true); return; }
    gs->room = done;
    load_room(gs->room);
    if (done == 2 || done == 4) show_reward();
}

static void update_hud() {
    auto& buf = gs->hud_buf;

    if (gs->c_life != gs->life || gs->c_shield != gs->shield) {
        gs->c_life = gs->life; gs->c_shield = gs->shield;
        if (gs->god) {
            // Afficher des PV en mode dieu serait mensonger : rien ne les entame.
            snprintf(buf, sizeof(buf), "PV invulnerable");
        } else {
            snprintf(buf, sizeof(buf), "PV %d/%d%s", gs->life, gs->life_max,
                     gs->shield ? "  +BOUCLIER" : "");
        }
        set_text_if(gs->hud_life, buf);
        lv_obj_set_style_text_color(gs->hud_life,
            lv_color_hex(gs->god ? Pal::MAGNET
                                 : (gs->shield ? Pal::SHIELD : Pal::DANGER)),
            LV_PART_MAIN);
    }
    if (gs->c_gold != gs->gold) {
        gs->c_gold = gs->gold;
        snprintf(buf, sizeof(buf), "Or %d", gs->gold);
        set_text_if(gs->hud_gold, buf);
    }
    const Room& r = ROOMS[gs->room];
    if (gs->c_runes != gs->runes) {
        gs->c_runes = gs->runes;
        if (r.runes > 0) {
            if (gs->runes >= r.runes) snprintf(buf, sizeof(buf), "Portail ouvert !");
            else snprintf(buf, sizeof(buf), "Runes %d/%u", gs->runes, (unsigned) r.runes);
        } else {
            snprintf(buf, sizeof(buf), "%s", r.goal);
        }
        set_text_if(gs->hud_goal, buf);
    }
    // `unsigned` explicite : uint32_t est `long unsigned int` sur RISC-V, ce qui
    // ne correspond pas a %u (-Wformat).
    unsigned sec = (unsigned) (gs->run_ms / 1000);
    if ((int) sec != gs->c_sec) {
        gs->c_sec = (int) sec;
        snprintf(buf, sizeof(buf), "%u:%02u", sec / 60, sec % 60);
        set_text_if(gs->hud_time, buf);
    }
}

static void tick_cb(lv_timer_t*) {
    if (g_state != ST_PLAYING) return;
    uint32_t now = lv_tick_get();
    if (gs->run_last_tick_ms != 0 && (now - gs->run_last_tick_ms) > PAUSE_GAP_MS) {
        gs->run_start_ms += now - gs->run_last_tick_ms;  // lv_tick_get suit millis()
    }
    gs->run_last_tick_ms = now;
    gs->run_ms = now - gs->run_start_ms;

    // --- Inclinaison : offset de calibration, lissage, zone morte -----------
    tilt_smooth(g_tilt_x, g_tilt_y, g_raw_x, g_raw_y, gs->save.cal_x, gs->save.cal_y, TILT_SMOOTH);

    // « Main sure » elargit legerement la zone morte pour un pilotage plus calme.
    float dead = TILT_DEADZONE + (gs->save.difficulty == D_CALME ? 0.015f : 0.0f);
    // Rotation ecran 270 deg : l'axe Y physique pilote X a l'ecran, et X pilote Y.
    float ax = -g_tilt_y, ay = g_tilt_x;
    float mag_sq = ax * ax + ay * ay;
    if (mag_sq < dead * dead) { ax = 0; ay = 0; mag_sq = 0; }
    else {
        float mag = sqrtf(mag_sq);
        float k = (mag - dead) / mag;      // deadzone radiale (pas de marche d'escalier)
        ax *= k; ay *= k;
        float m2_sq = ax * ax + ay * ay;
        if (m2_sq > TILT_CLAMP * TILT_CLAMP) {
            float m2 = sqrtf(m2_sq);
            ax = ax / m2 * TILT_CLAMP; ay = ay / m2 * TILT_CLAMP;
        }
    }

    // --- Dash : inclinaison franche, avec recharge ---------------------------
    if (mag_sq > DASH_TILT * DASH_TILT && now >= gs->dash_ready_at) {
        gs->dash_ready_at = now + DASH_CD_MS;
        float n = sqrtf(ax * ax + ay * ay);
        if (n > 0.001f) { gs->vx += ax / n * DASH_IMPULSE; gs->vy += ay / n * DASH_IMPULSE; }
    }

    float acc_x = ax * ACCEL_SCALE * gs->ctrl_mul;
    float acc_y = ay * ACCEL_SCALE * gs->ctrl_mul;

    // --- Zones (glu / acceleration / vent) : lues une fois par frame ---------
    float zone_ax = 0, zone_ay = 0, zone_damp = 1.0f;
    for (int i = 0; i < gs->ent_n; i++) {
        const Ent& e = gs->ent[i];
        if (!e.alive) continue;
        if (e.k == K_GLUE) { if (inside_zone(e)) zone_damp = 0.90f; }
        else if (e.k == K_BOOST || e.k == K_WIND) {
            if (inside_zone(e)) {
                float f = (float) e.b;
                switch (e.a) {
                    case 0: zone_ax += f; break;
                    case 1: zone_ax -= f; break;
                    case 2: zone_ay += f; break;
                    default: zone_ay -= f; break;
                }
            }
        }
    }
    acc_x += zone_ax; acc_y += zone_ay;

    // --- Integration en sous-pas (collisions robustes a grande vitesse) -----
    for (int s = 0; s < SUBSTEP; s++) {
        gs->vx += acc_x * SDT;
        gs->vy += acc_y * SDT;
        gs->vx *= gs->fric_sub * zone_damp;
        gs->vy *= gs->fric_sub * zone_damp;

        float sp_sq = gs->vx * gs->vx + gs->vy * gs->vy;
        if (sp_sq > gs->speed_max * gs->speed_max) {
            float sp = sqrtf(sp_sq);
            gs->vx = gs->vx / sp * gs->speed_max; gs->vy = gs->vy / sp * gs->speed_max;
        }

        gs->bx += gs->vx * SDT;
        gs->by += gs->vy * SDT;

        // Bords du terrain
        if (gs->bx < gs->ball_r)      { gs->bx = gs->ball_r;      gs->vx = -gs->vx * BOUNCE; }
        if (gs->bx > FW - gs->ball_r) { gs->bx = FW - gs->ball_r; gs->vx = -gs->vx * BOUNCE; }
        if (gs->by < gs->ball_r)      { gs->by = gs->ball_r;      gs->vy = -gs->vy * BOUNCE; }
        if (gs->by > FH - gs->ball_r) { gs->by = FH - gs->ball_r; gs->vy = -gs->vy * BOUNCE; }

        for (int i = 0; i < gs->ent_n; i++) {
            if (gs->ent[i].k == K_WALL) resolve_wall(gs->ent[i]);
        }
    }

    // --- Mobiles : position + rendu -----------------------------------------
    for (int i = 0; i < gs->ent_n; i++) {
        Ent& e = gs->ent[i];
        if (!e.alive) continue;
        // Garde-fou : une periode nulle ferait un modulo/division par zero (UB)
        // si une future salle oubliait de renseigner `b`.
        if ((e.k == K_SAW || e.k == K_ORB) && e.b <= 0) continue;
        if (e.k == K_SAW) {
            float t = (float) ((now + e.phase) % (uint32_t) e.b) / (float) e.b;
            float off = sinf(t * 6.28318f) * e.a;
            if (e.w >= e.h) {   // barre horizontale : oscille verticalement
                e.x = (int16_t) (e.ox - e.w / 2);
                e.y = (int16_t) (e.oy - e.h / 2 + off);
            } else {            // barre verticale : oscille horizontalement
                e.x = (int16_t) (e.ox - e.w / 2 + off);
                e.y = (int16_t) (e.oy - e.h / 2);
            }
            lv_obj_set_pos(e.obj, e.x, e.y);
        } else if (e.k == K_ORB) {
            float t = (float) ((now + e.phase) % (uint32_t) e.b) / (float) e.b;
            float an = t * 6.28318f;
            e.x = (int16_t) (e.ox + cosf(an) * e.a - e.w / 2);
            e.y = (int16_t) (e.oy + sinf(an) * e.a - e.h / 2);
            lv_obj_set_pos(e.obj, e.x, e.y);
        } else if (e.k == K_HUNTER) {
            float hx = e.x + e.w * 0.5f, hy = e.y + e.h * 0.5f;
            float dx = gs->bx - hx, dy = gs->by - hy;
            float d = sqrtf(dx * dx + dy * dy);
            if (d > 1.0f) {
                float step = e.b * DT;
                hx += dx / d * step; hy += dy / d * step;
                e.x = (int16_t) (hx - e.w * 0.5f);
                e.y = (int16_t) (hy - e.h * 0.5f);
                lv_obj_set_pos(e.obj, e.x, e.y);
            }
        }
    }

    // --- Aimant : attire les pickups vers la bille ---------------------------
    if (gs->magnet_r > 0) {
        for (int i = 0; i < gs->ent_n; i++) {
            Ent& e = gs->ent[i];
            if (!e.alive) continue;
            if (e.k != K_GOLD && e.k != K_SHIELD && e.k != K_RUNE) continue;
            float ex = e.x + e.w * 0.5f, ey = e.y + e.h * 0.5f;
            float dx = gs->bx - ex, dy = gs->by - ey;
            float d = sqrtf(dx * dx + dy * dy);
            if (d > 4.0f && d < gs->magnet_r) {
                float step = 190.0f * DT;
                e.x = (int16_t) (ex + dx / d * step - e.w * 0.5f);
                e.y = (int16_t) (ey + dy / d * step - e.h * 0.5f);
                lv_obj_set_pos(e.obj, e.x, e.y);
            }
        }
    }

    // --- Collisions logiques : pickups, pieges, sortie ------------------------
    const Room& room = ROOMS[gs->room];
    for (int i = 0; i < gs->ent_n; i++) {
        Ent& e = gs->ent[i];
        if (!e.alive) continue;
        switch (e.k) {
            case K_GOLD:
                if (circle_hits(e)) { e.alive = false; show(e.obj, false); gs->gold += 10; }
                break;
            case K_SHIELD:
                if (circle_hits(e)) { e.alive = false; show(e.obj, false); gs->shield = true; }
                break;
            case K_MAGNET:
                if (circle_hits(e)) { e.alive = false; show(e.obj, false); gs->magnet_r = 130; }
                break;
            case K_BRAKE:
                if (circle_hits(e)) { e.alive = false; show(e.obj, false); gs->fric -= 0.004f; apply_fric(); }
                break;
            case K_DASH:
                if (circle_hits(e)) { e.alive = false; show(e.obj, false); gs->dash_ready_at = 0; }
                break;
            case K_RUNE:
                if (circle_hits(e)) { e.alive = false; show(e.obj, false); gs->runes++; }
                break;
            case K_CHEST:
                if (circle_hits(e)) {
                    e.alive = false; show(e.obj, false);
                    auto& cbuf = gs->chest_buf;
                    int bonus = rnd_range(25, 60);
                    gs->gold += bonus;
                    // Jet de butin, module par la caracteristique Decouverte.
                    if ((int) (rnd() % 1000u) < (int) (gs->loot_chance * 1000.0f)) {
                        int it = grant_random_item();
                        if (it >= 0) {
                            snprintf(cbuf, sizeof(cbuf), "Coffre : %s !", ITEMS[it].name);
                            toast(cbuf, ITEMS[it].color);
                            break;
                        }
                    }
                    snprintf(cbuf, sizeof(cbuf), "Coffre : %d ames", bonus);
                    toast(cbuf, Pal::RUNE);
                }
                break;
            case K_SPIKE: case K_SAW: case K_ORB: case K_HUNTER:
                if (circle_hits(e) && take_damage(false)) return;
                break;
            case K_PIT:
                if (circle_hits(e) && take_damage(true)) return;
                break;
            case K_EXIT: {
                bool open_gate = (gs->runes >= room.runes);
                // Le portail verrouille reste visible mais eteint : l'objectif se lit.
                // Le style n'est reecrit qu'au changement d'etat (sinon on
                // invaliderait le portail a chaque frame pour rien).
                if (gs->c_gate != (int) open_gate) {
                    gs->c_gate = (int) open_gate;
                    style_exit(e, open_gate);
                }
                // « Oeil du dedale » : le portail ouvert pulse doucement.
                if (open_gate && gs->has_eye) {
                    const int opa = (now / 300) & 1 ? LV_OPA_COVER : LV_OPA_50;
                    if (gs->c_eye_opa != opa) {
                        gs->c_eye_opa = opa;
                        lv_obj_set_style_border_opa(e.obj, (lv_opa_t) opa, LV_PART_MAIN);
                    }
                }
                if (open_gate && circle_hits(e)) { next_room(); return; }
                break;
            }
            default: break;
        }
    }

    // --- Rendu de la bille (+ tremblement court apres un degat) --------------
    int jx = 0, jy = 0;
    if (gs->jitter > 0) { gs->jitter--; jx = rnd_range(-4, 4); jy = rnd_range(-4, 4); }
    ball_place((int) gs->bx - gs->ball_r + jx, (int) gs->by - gs->ball_r + jy);

    // Invulnerabilite : la bille clignote (feedback sans cout de rendu).
    bool inv = now < gs->invuln_until;
    ball_set_opa((inv && ((now / 100) & 1)) ? LV_OPA_40 : LV_OPA_COVER);

    if (gs->vignette_until && now >= gs->vignette_until) {
        gs->vignette_until = 0;
        for (int i = 0; i < 4; i++) show(gs->vign[i], false);
    }
    if (gs->toast_until && now >= gs->toast_until) {
        gs->toast_until = 0;
        show(gs->toast, false);
    }

    update_hud();
}

// ===========================================================================
// 12. Interactions tactiles (menus uniquement)
// ===========================================================================

// Depense un point de niveau dans une caracteristique.
static void buy_stat(int i) {
    if (i < 0 || i >= MARBLE_NSTATS) return;
    if (gs->save.st[i] >= STATS[i].maxlvl) return;
    uint32_t cost = level_cost(total_level());
    if (gs->save.souls < cost) return;
    gs->save.souls -= cost;
    gs->save.st[i]++;
    persist_save();
    go_level();   // rafraichit cout / niveaux / solvabilite
}

// Achat, ou revente a moitie prix si l'objet est deja possede.
static void shop_action(int id) {
    if (id < 0 || id >= N_ITEMS) return;
    const ItemDef& it = ITEMS[id];
    uint32_t bit = 1u << id;
    if (gs->save.items & bit) {
        gs->save.items &= ~bit;
        gs->save.souls += it.price / 2;
        // Un objet vendu ne peut pas rester equipe.
        for (int s = 0; s < MARBLE_NSLOTS; s++)
            if (gs->save.equip[s] == (uint8_t) (id + 1)) gs->save.equip[s] = 0;
    } else {
        if (gs->save.souls < it.price) return;
        gs->save.souls -= it.price;
        gs->save.items |= bit;
    }
    persist_save();
    go_shop();
}

// Fait passer un emplacement a l'objet possede suivant (ou au vide).
// Un meme objet ne peut pas occuper les deux emplacements.
static void equip_cycle(int s) {
    if (s < 0 || s >= MARBLE_NSLOTS) return;
    uint8_t cur = gs->save.equip[s];
    for (int step = 1; step <= N_ITEMS + 1; step++) {
        int next = (cur + step) % (N_ITEMS + 1);
        if (next == 0) { gs->save.equip[s] = 0; break; }
        if (!(gs->save.items & (1u << (next - 1)))) continue;
        bool dup = false;
        for (int o = 0; o < MARBLE_NSLOTS; o++)
            if (o != s && gs->save.equip[o] == (uint8_t) next) dup = true;
        if (dup) continue;
        gs->save.equip[s] = (uint8_t) next;
        break;
    }
    persist_save();
    go_equip();
}

static void slot_event_cb(lv_event_t* e) {
    int i = (int) (intptr_t) lv_event_get_user_data(e);

    switch (g_state) {
        case ST_HUB:
            if (i == 0) start_run();
            else if (i == 1) go_level();
            else if (i == 2) { gs->shop_page = 0; go_shop(); }
            else if (i == 3) go_equip();
            else if (i == 4) go_settings();
            else if (i == 5) go_stats();
            else if (i == 6) close();
            break;

        case ST_LEVEL:
            if (i == MARBLE_NSTATS) go_hub(); else buy_stat(i);
            break;

        case ST_SHOP:
            // Index dynamiques : ils suivent le nombre d'objets reellement
            // affiches (gs->shop_rows), comme dans go_shop().
            if (i == gs->shop_rows) {
                int pages = (N_ITEMS + SHOP_PER_PAGE - 1) / SHOP_PER_PAGE;
                gs->shop_page = (gs->shop_page + 1) % (pages > 0 ? pages : 1);
                go_shop();
            } else if (i == gs->shop_rows + 1) {
                go_hub();
            } else {
                shop_action(gs->shop_page * SHOP_PER_PAGE + i);
            }
            break;

        case ST_EQUIP:
            if (i == MARBLE_NSLOTS) go_hub(); else equip_cycle(i);
            break;

        case ST_SETTINGS:
            if (i == 0) {
                gs->save.difficulty = (uint8_t) ((gs->save.difficulty + 1) % D_COUNT);
                persist_save();
                go_settings();
            } else if (i == 1) {
                gs->save.god = gs->save.god ? 0 : 1;
                persist_save();
                go_settings();
            } else if (i == 2) {
                gs->save.skin = (uint8_t) ((gs->save.skin + 1) % 3);
                persist_save();
                ball_apply_skin();
                go_settings();
            } else if (i == 3) {
                calibrate();
                set_text_if(gs->p_foot, "Calibration prise. La tablette est desormais « a plat ».");
            } else {
                go_hub();
            }
            break;

        case ST_STATS:
            go_hub();
            break;

        case ST_REWARD:
            if (i < gs->offer_n) {
                apply_boon(gs->offer[i]);
                // Pastille HUD du boon fraichement acquis.
                int b = gs->boon_n - 1;
                if (b >= 0 && b < MAX_BOONS) {
                    set_bg(gs->hud_dot[b], BOONS[gs->offer[i]].color, LV_OPA_COVER);
                    show(gs->hud_dot[b], true);
                }
                g_state = ST_PLAYING;
                panel_on(false);
                ball_show(true);
                gs->c_life = -1;   // PV/bouclier ont pu changer
            }
            break;

        case ST_PAUSED:
            if (i == 0) { g_state = ST_PLAYING; panel_on(false); }
            else if (i == 1) { calibrate(); set_text_if(gs->p_sub, "Calibration prise."); }
            else if (i == 2) end_run(false);
            break;

        case ST_GAMEOVER:
        case ST_VICTORY:
            if (i == 0) start_run(); else go_hub();
            break;

        default: break;
    }
}

// Appui sur le bandeau HUD pendant une partie => pause.
static void hud_event_cb(lv_event_t*) {
    if (g_state == ST_PLAYING) show_pause();
}

// ===========================================================================
// 13. API publique
// ===========================================================================

void on_imu(float ax, float ay, float /*az*/) {
    if (ax != ax || ay != ay) return;   // garde NaN
    g_raw_x = ax;
    g_raw_y = ay;
}

void calibrate() {
    if (!gs) return;  // la calibration est rangee dans la sauvegarde (jeu ouvert)
    tilt_calibrate(gs->save.cal_x, gs->save.cal_y, g_raw_x, g_raw_y);
    g_tilt_x = 0; g_tilt_y = 0;
    persist_save();
}

bool is_open() { return g_state != ST_OFF; }

void open(const UI& ui) {
    if (g_state != ST_OFF) return;
    if (!ui.root || !ui.field || !ui.hud || !ui.panel) return;
    gs = game_mem_new<Mem>(MemPref::Internal);
    if (!gs) {
        ESP_LOGW("marble", "%u o introuvables : jeu non ouvert", (unsigned) sizeof(Mem));
        if (ui.lvgl) ui.lvgl->show_page(ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
        return;
    }
    gs->ui = ui;

    persist_load();
    build_ui();

    // Teinte de la bille selon le cosmetique debloque (corps + reflet).
    ball_apply_skin();

    for (int i = 0; i < MAX_BOONS; i++) show(gs->hud_dot[i], false);
    // La page LVGL est déjà active (navigation via lvgl.page.show dans le YAML).

    gs->run_active = false;
    go_hub();

    // La pause se declenche en touchant le bandeau HUD (pas de croix a l'ecran :
    // le jeu est un flux plein cadre, on ne remet pas le chrome modal).
    lv_obj_add_flag(gs->ui.hud, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(gs->ui.hud, hud_event_cb, LV_EVENT_CLICKED, nullptr);

    if (!gs->timer) gs->timer = lv_timer_create(tick_cb, 33, nullptr);
}

void close() {
    if (g_state == ST_OFF) return;

    // Une run abandonnee en cours conserve quand meme ses ames
    // (sauf en mode dieu, hors concours).
    if (gs->run_active && !gs->god) {
        gs->run_active = false;
        gs->save.souls += (uint32_t) (gs->gold * gs->gold_mul * gs->soul_mul);
        uint8_t reached = (uint8_t) (gs->room + 1);
        if (reached > gs->save.deepest) gs->save.deepest = reached;
    }
    gs->run_active = false;
    persist_save();

    if (gs->timer) { lv_timer_delete(gs->timer); gs->timer = nullptr; }
    if (gs->ui.hud) lv_obj_remove_event_cb(gs->ui.hud, hud_event_cb);
    // Navigation retour vers le sélecteur arcade (page LVGL).
    if (gs->ui.lvgl) gs->ui.lvgl->show_page(gs->ui.home_idx, LV_SCREEN_LOAD_ANIM_NONE, 0);
    g_state = ST_OFF;

    // Rien ne reste reserve : les objets LVGL du jeu (dont le slot « Quitter » dont
    // le callback nous appelle), puis le bloc qui les pointait. Le callback du HUD
    // (conteneur YAML) est retire plus haut ; le jeu ne cree rien directement dans
    // root, d'ou nullptr.
    ui_destroy(nullptr, {gs->ui.field, gs->ui.hud, gs->ui.panel});
    game_mem_free(gs);
}

}  // namespace Marble
