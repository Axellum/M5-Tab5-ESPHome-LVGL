/**
 * [AI-CONTEXT]
 * @file draughts_ai.h
 * @role IA embarquée « Dames Tab » — 4 niveaux, recherche time-sliced.
 * @architecture_constraint ai_step() doit rester < ~25 ms (budget nœuds).
 *      Jamais de boucle bloquante LVGL. Débutant = aléatoire pondéré ;
 *      Amateur/Confirmé/Expert = alpha-bêta + iterative deepening.
 * @ai_instruction Utiliser uniquement Engine::gen_moves / apply_move / eval_*.
 *      Ne pas mélanger les variantes : la Pos.variant pilote les règles.
 */
#pragma once
#include "draughts_game.h"

namespace Draughts {
namespace Ai {

enum Level : uint8_t {
    LVL_BEGINNER = 0,  // coups légaux aléatoires (préfère prises)
    LVL_AMATEUR  = 1,  // profondeur 1–2 + eval
    LVL_SOLID    = 2,  // profondeur 2–3 + alpha-bêta
    LVL_EXPERT   = 3   // profondeur 3–4 + quiescence prises
};

enum State : uint8_t {
    AI_IDLE = 0,
    AI_THINKING,
    AI_DONE,
    AI_ABORT
};

// Démarre une recherche pour le côté au trait de `root`.
void begin(const Engine::Pos& root, Level level);

// Avance la recherche d'un quantum (~budget nœuds). Appeler depuis le timer.
void step();

State state();
bool  ready();                 // true si AI_DONE
const Engine::Move& best();    // coup choisi (valide si ready)
void  abort();                 // annule (undo / fermeture)

}  // namespace Ai
}  // namespace Draughts
