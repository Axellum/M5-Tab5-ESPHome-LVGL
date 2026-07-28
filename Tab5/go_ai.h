/**
 * [AI-CONTEXT]
 * @file go_ai.h
 * @role IA Go Tab — 4 niveaux, recherche time-slicée (jamais bloquante).
 * @architecture_constraint La recherche est bornée par le TEMPS, pas par un
 *      compteur de nœuds : `step(slice_ms)` rend la main au bout de la tranche
 *      demandée, même au milieu d'un candidat, et un budget CPU total garantit
 *      la terminaison. L'ancienne version bornait en nœuds ; à 3 plis un seul
 *      candidat coûtait plus que le budget, l'index de candidat n'avançait
 *      jamais et le Tab « réfléchissait » indéfiniment.
 * @ai_instruction Un coup valide (`best_sq()`) est disponible dès `begin()` :
 *      toute interruption rend au pire le meilleur candidat statique.
 */
#pragma once
#include "go_engine.h"

namespace Go {
namespace Ai {

enum Level : uint8_t {
    LVL_BEGINNER = 0,  // choix pondéré aléatoire (capture / sauvetage / centre)
    LVL_AMATEUR  = 1,  // 1 pli
    LVL_SOLID    = 2,  // 2 plis alpha-bêta
    LVL_EXPERT   = 3   // 3 plis (2 sur grand plateau) + approfondissement itératif
};

enum State : uint8_t { AI_IDLE = 0, AI_THINKING, AI_DONE, AI_ABORT };

// Sentinelle : best_sq() == RESIGN → l'IA abandonne (écart de score trop grand).
static constexpr int RESIGN = -2;

// Prépare la recherche. `seed` décorrèle les parties (départage aléatoire des
// coups de score égal). `komi` doit être celui de la partie (0,5 en handicap).
// Retourne immédiatement pour LVL_BEGINNER.
void begin(const Engine::Pos& root, Level level, uint32_t seed, float komi = 6.5f);

// Fait avancer la recherche d'AU PLUS `slice_ms` millisecondes de CPU.
void step(uint32_t slice_ms);

State state();
bool  ready();       // true quand un coup définitif est disponible
int   best_sq();     // index d'intersection, ou Engine::PASS
int   progress_pct();// 0..100, pour la barre de réflexion
void  abort();

// Nom court du niveau (« Débutant », « Amateur », …) — sans accent, les polices
// du Tab5 les gèrent mais l'UI reste homogène avec le reste des jeux.
const char* level_name(Level lv);

}  // namespace Ai
}  // namespace Go
