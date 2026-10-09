/**
 * [AI-CONTEXT]
 * @file draughts_engine.h
 * @role Moteur PUR du jeu « Dames Tab », partagé par l'interface (draughts_game.cpp)
 *      et l'IA (draughts_ai.cpp) : encodage du plateau, coups, règles.
 * @architecture_constraint Aucun en-tête d'ESPHome ni de LVGL ici ni dans
 *      draughts_engine.cpp : tools/test_draughts_engine.cpp les compile sur PC sans
 *      rien d'autre (job `python` de la CI). DraughtsSave (draughts_game.h) stocke
 *      les pièces avec les valeurs de Piece : ne pas les renuméroter sans changer
 *      SAVE_MAGIC (draughts_game.cpp).
 */
#pragma once
#include <cstdint>

namespace Draughts {
namespace Engine {

// Déclarations déplacées telles quelles de draughts_game.h, avec leurs alignements
// (voir draughts_engine.cpp) : hors du hook clang-format.
// clang-format off
static constexpr int MAX_N     = 10;
static constexpr int MAX_SQ    = MAX_N * MAX_N;
static constexpr int MAX_MOVES = 96;
static constexpr int MAX_CAPS  = 20;
static constexpr int MAX_PATH  = 24;
// Nulle quand aucun pion n'a bougé et rien n'a été pris pendant N coups de chaque camp :
// 25 en international (FFJD/FMJD), 40 en anglais (WCDF). Compté en demi-coups.
static constexpr int DRAW_PLIES_INTL = 50;
static constexpr int DRAW_PLIES_ENG  = 80;
// Fins de partie réduites (FMJD, international seulement) : une dame seule contre
// au plus deux pièces dont une dame → nulle après 5 coups de chaque camp ; contre
// trois pièces dont une dame → après 16. Compté en demi-coups depuis la dernière
// prise ou promotion (le matériel ne change qu'à ces moments-là).
static constexpr int ENDGAME_PLIES_SMALL = 10;
static constexpr int ENDGAME_PLIES_THREE = 32;

enum Variant : uint8_t { VAR_INTL10 = 0, VAR_ENG8 = 1 };
enum Side    : uint8_t { SIDE_WHITE = 0, SIDE_BLACK = 1 };
enum Piece   : uint8_t {
    EMPTY = 0,
    W_MAN = 1, W_KING = 2,
    B_MAN = 3, B_KING = 4
};

inline bool is_white(uint8_t p) { return p == W_MAN || p == W_KING; }
inline bool is_black(uint8_t p) { return p == B_MAN || p == B_KING; }
inline bool is_king(uint8_t p)  { return p == W_KING || p == B_KING; }
inline bool is_man(uint8_t p)   { return p == W_MAN || p == B_MAN; }
inline Side piece_side(uint8_t p) { return is_white(p) ? SIDE_WHITE : SIDE_BLACK; }
inline bool is_dark_sq(int r, int c) { return ((r + c) & 1) == 1; }

struct Pos {
    uint8_t sq[MAX_SQ];
    uint8_t n;           // 8 ou 10
    uint8_t side;        // Side
    uint8_t must_from;   // 255 = libre
    uint8_t variant;     // Variant
    uint8_t no_progress; // demi-coups sans prise ni déplacement de pion
    uint8_t eg_limit;    // fin de partie réduite : seuil en demi-coups, 0 = hors cas
    uint8_t eg_plies;    // demi-coups joués depuis l'entrée dans ce cas
};

// Coup légal complet (rafle = chemin + capturées).
// Pièces capturées retirées en FIN de rafle (règle internationale standard).
struct Move {
    uint8_t from;
    uint8_t to;
    uint8_t n_caps;
    uint8_t caps[MAX_CAPS];   // indices cases capturées (ordre)
    uint8_t n_path;
    uint8_t path[MAX_PATH];   // atterrissages successifs (dernier = to)
    uint8_t promote;          // 1 si promotion en fin de coup
};

void pos_init(Pos& p, Variant v);
int  gen_moves(const Pos& p, Move* out, int max_out);
void apply_move(Pos& p, const Move& m);
// Recalcule eg_limit d'après le matériel (et remet eg_plies à 0). apply_move()
// l'appelle à chaque prise ou promotion ; à appeler aussi après avoir posé une
// position à la main (reprise d'une partie sauvegardée).
void refresh_endgame(Pos& p);
int  eval_material(const Pos& p);          // +blancs, −noirs (pion=100, dame=300)
// Matériel + mobilité légère. `scratch` (MAX_MOVES coups) reçoit la génération qui
// sert à compter la mobilité : 4,7 Ko que l'IA fournit hors de la pile.
int  eval_full(const Pos& p, Move* scratch);
int  count_pieces(const Pos& p, Side s);
bool has_legal_move(const Pos& p);         // sans liste : 1 coup trouvé suffit
// winner: 0 blancs, 1 noirs, 2 nulle ; retourne true si terminée
bool is_terminal(const Pos& p, int* winner);
// clang-format on

}  // namespace Engine
}  // namespace Draughts
