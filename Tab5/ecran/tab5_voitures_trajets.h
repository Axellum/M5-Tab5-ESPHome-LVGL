/**
 * [AI-CONTEXT]
 * @file tab5_voitures_trajets.h
 * @role Page « Trajets » du popup Voitures (tab5_voitures_trajets.cpp, ADR-0083) : par voiture dont
 *       l'odomètre est choisi dans le blueprint, ses kilomètres du mois et son kWh / 100 km ESTIMÉ, les
 *       kilomètres de chacun des sept derniers jours et les kWh qu'elle a reçus ces jours-là. D'après
 *       ce que Home Assistant pousse par tab5_maj_voiture_trajets (lu par trajets_lire,
 *       Tab5/socle/tab5_parse.h).
 * @architecture_constraint tab5_custom.h l'inclut (via tab5_voitures.h) : les lambdas YAML et les
 *       unités `tab5_*.cpp` le voient. Une fonction déclarée ici a un appelant hors de son fichier
 *       (règle 12 de tools/check_tab5_code_rules.py) : les trois premières, la table kPages de
 *       tab5_voitures.cpp ; la dernière, l'action de tab5-api-logic.yaml. La page n'apparaît que si une
 *       voiture a un payload non vide (une voiture sans odomètre choisi n'a pas de colonne : ADR-0046).
 * @ai_instruction La page crée ses widgets en C++ (sur voitures_corps()) à sa première montre : un
 *       widget de plus = son champ dans struct Colonne du .cpp et sa ligne dans creer().
 */
#pragma once
#include "esphome.h"
#include <string>

// La page apparaît seulement avec des données (ADR-0046) : au moins une voiture dont HA a poussé le
// texte des trajets (celle dont l'odomètre est choisi).
bool voitures_trajets_a_donnees();
// Crée les widgets au premier appel avec `visible` vrai ; les montre ou les cache ensuite.
void voitures_trajets_montrer(bool visible);
// Repeint la page (popup ouvert, page affichée) : les écritures sont comparées d'abord.
void voitures_trajets_peindre();

// Action tab5_maj_voiture_trajets (ADR-0083) : rang (« 1 » ou « 2 », la voiture de tab5_maj_voiture) et le
// payload de six champs (trajets_lire, tab5_parse.h). Garde la voiture ; repeint le popup seulement
// s'il est ouvert. Un payload vide retire la voiture de la page.
void voitures_trajets_recu(const std::string& rang, const std::string& payload);
