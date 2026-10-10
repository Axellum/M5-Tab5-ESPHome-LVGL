/**
 * [AI-CONTEXT]
 * @file tab5_custom.h
 * @role Contrat C++ du HMI : déclarations appelées par les lambdas YAML, structures
 *       des widgets, état partagé. En-tête parapluie de la couche `tab5_*.cpp` : depuis
 *       le 08/10/2026, les déclarations vivent dans un en-tête par module (tab5_forecast.h
 *       … tab5_theme.h, inclus ci-dessous dans l'ordre de leurs dépendances) ; les lambdas
 *       et les unités n'incluent toujours que lui.
 * @architecture_constraint Les jetons (UIColor, UIAnim, UIIdle) vivent dans
 *       `tab5_tokens.h`, inclus ici : les lambdas les voient comme avant, et les
 *       jeux peuvent n'inclure que les jetons (25/09/2026, audit lot 8a).
 * @ai_instruction Ne JAMAIS recréer des constantes de couleurs ailleurs : ajouter
 *       un jeton dans `tab5_tokens.h`. Une déclaration nouvelle va dans l'en-tête de son
 *       module, pas ici ; un module nouveau = un en-tête de plus, inclus ici et listé sous
 *       `includes:` des deux configurations racine.
 */
#pragma once
#include "esphome.h"
#include "tab5_tokens.h"
#include "tab5_core.h"
#include "tab5_batterie.h"  // batterie et chargeur (08/10/2026) : présence, niveau, consommation
#include "tab5_i18n.h"
#include <initializer_list>
#include <string>
#include <vector>

// Un en-tête par module (08/10/2026), dans l'ordre de leurs dépendances.
#include "tab5_forecast.h"
#include "tab5_services.h"
#include "tab5_anim.h"
#include "tab5_text.h"
#include "tab5_central.h"
#include "tab5_cards.h"
#include "tab5_console.h"
#include "tab5_clim.h"
#include "tab5_assist.h"
#include "tab5_calendar.h"
#include "tab5_journal.h"
#include "tab5_zones.h"
#include "tab5_tuiles.h"
#include "tab5_rangee.h"
#include "tab5_reglables.h"
#include "tab5_roue.h"
#include "tab5_energie.h"
#include "tab5_reglages.h"
#include "tab5_historique.h"
#include "tab5_meteo.h"
#include "tab5_zone_gauche.h"
#include "tab5_lecteur.h"
#include "tab5_cameras.h"
#include "tab5_suivi.h"
#include "tab5_froid.h"
#include "tab5_telecommande.h"
#include "tab5_alertes.h"
#include "tab5_maison.h"
#include "tab5_piece_climat.h"
#include "tab5_theme.h"

// Données calendrier/prévisions, dates locales, jours et mois en toutes lettres :
// logique PURE, déclarée dans tab5_core.h (compilable et testable sur PC).
namespace esphome { namespace font { class Font; } }

// Le jeu de bille vit desormais dans marble_game.h / marble_game.cpp
// (namespace Marble). L'ancien prototype `namespace Game` a ete retire.

// UIColor (couleurs sémantiques) : voir tab5_tokens.h.
