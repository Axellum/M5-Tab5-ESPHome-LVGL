/**
 * [AI-CONTEXT]
 * @file jeux_absents.cpp
 * @role Le module des jeux absent (ADR-0065) : une table de consoles vide pour le
 *       registre (tab5_registry.cpp). Compilé par tab5-arcade-absente.yaml à la
 *       place de jeux_registre.cpp, sur une carte sans IMU (la Waveshare 7B) ou
 *       avec `tab5_jeux: non`. GameRegistry::disponibles() vaut alors false :
 *       l'Arcade n'est plus un écran de la tablette (ecran_sans_zone, tab5_zones.cpp).
 * @ai_instruction Rien d'autre ici : ce fichier ne nomme aucun jeu.
 */
#include "tab5_registry.h"

namespace GameRegistry {

const Entry* const kJeux = nullptr;
const int kNbJeux = 0;

}  // namespace GameRegistry
