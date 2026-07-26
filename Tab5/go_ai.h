/**
 * [AI-CONTEXT]
 * @file go_ai.h
 * @role IA Go Tab — 4 niveaux, recherche time-sliced (jamais bloquante).
 */
#pragma once
#include "go_engine.h"

namespace Go {
namespace Ai {

enum Level : uint8_t {
    LVL_BEGINNER = 0,  // légal aléatoire (préfère captures / near)
    LVL_AMATEUR  = 1,  // 1-ply eval
    LVL_SOLID    = 2,  // 2-ply alpha-bêta candidats
    LVL_EXPERT   = 3   // 3-ply + plus de candidats / budget
};

enum State : uint8_t { AI_IDLE = 0, AI_THINKING, AI_DONE, AI_ABORT };

void begin(const Engine::Pos& root, Level level);
void step();
State state();
bool ready();
int  best_sq();   // index ou Engine::PASS
void abort();

}  // namespace Ai
}  // namespace Go
