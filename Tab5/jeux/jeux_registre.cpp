/**
 * [AI-CONTEXT]
 * @file jeux_registre.cpp
 * @role La liste des consoles (GameRegistry::kGames) : C'EST ICI, et nulle part
 *       ailleurs, qu'elle est écrite. Partie du module des jeux (ADR-0065),
 *       compilée par tab5-arcade.yaml seulement ; sans le module,
 *       jeux_absents.cpp donne une table vide au registre (tab5_registry.cpp).
 * @ai_instruction Une nouvelle console = une ligne dans `kGames`. Garder
 *       « Neon Apron » en dernier (ADR-0012). tools/check_tab5_registry.py lit
 *       cette table (chaque `*_game.h` y figure, et réciproquement).
 */
#include "tab5_registry.h"

#include "marble_game.h"
#include "arkanoid_game.h"
#include "lode_game.h"
#include "go_game.h"
#include "trivia_game.h"
#include "draughts_game.h"
#include "chess_game.h"
#include "pinball_game.h"

namespace GameRegistry {

// Ordre = ordre de fermeture ET ordre de priorité du libellé HA.
// `imu_fast` : « Roi Noir », « Dames Tab », « Go Tab » et « Trial Poursuite »
// n'utilisent l'IMU que pour détecter une secousse franche (indice, lancer du dé ;
// Trial Poursuite passé à false le 26/09/2026, audit lot 3), fiable à 10 Hz —
// inutile de payer 30 Hz pendant une partie qui peut durer une demi-heure.
// « Neon Apron » est en cadence rapide : un nudge est une secousse de ~150 ms,
// à 10 Hz on n'en verrait qu'un échantillon sur deux.
// clang-format off
static const Entry kGames[] = {
    {"Fil d'Or",        Marble::is_open,   Marble::close,   Marble::on_imu,   true},
    {"Arcanoïde",       Arkanoid::is_open, Arkanoid::close, Arkanoid::on_imu, true},
    {"Coureur d'Or",    Lode::is_open,     Lode::close,     Lode::on_imu,     true},
    {"Go Tab",          Go::is_open,       Go::close,       Go::on_imu,       false},
    {"Trial Poursuite", Trivia::is_open,   Trivia::close,   Trivia::on_imu,   false},
    {"Dames Tab",       Draughts::is_open, Draughts::close, Draughts::on_imu, false},
    {"Roi Noir",        Chess::is_open,    Chess::close,    Chess::on_imu,    false},
    // EN DERNIER : sa fermeture restaure la rotation 270 du dashboard. Si un
    // jour une autre console touche à l'orientation, elle devra fermer après,
    // pour que le dernier mot revienne au paysage (ADR-0012).
    {"Neon Apron",      Pinball::is_open,  Pinball::close,  Pinball::on_imu,  true},
};
// clang-format on

const Entry* const kJeux = kGames;
const int kNbJeux = sizeof(kGames) / sizeof(kGames[0]);

}  // namespace GameRegistry
