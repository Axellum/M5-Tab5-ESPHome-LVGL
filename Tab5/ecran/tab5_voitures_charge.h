/**
 * [AI-CONTEXT]
 * @file tab5_voitures_charge.h
 * @role Page « Charge » du popup Voitures (tab5_voitures_charge.cpp, ADR-0082) : une carte par voiture
 *       (batterie, limite, état de la charge), la frise des 24 heures du jour (par voiture, les
 *       heures où elle a chargé ; en dessous, la prévision du soleil) et un créneau CONSEILLÉ pour
 *       charger au soleil, calculé ici d'après la prévision (creneau_solaire, tab5_creneau.h).
 * @architecture_constraint tab5_custom.h l'inclut : les lambdas YAML et les unités `tab5_*.cpp` le
 *       voient. Une fonction déclarée ici a un appelant hors de son fichier (règle 12 de
 *       tools/check_tab5_code_rules.py) : les trois premières, la table kPages de tab5_voitures.cpp ;
 *       les deux dernières, les actions de tab5-api-logic.yaml. Aucun nouveau service : la page lit
 *       ceux que la page Aujourd'hui du popup Énergie reçoit déjà (ADR-0078, ADR-0058).
 * @ai_instruction La page crée ses widgets en C++ (sur voitures_corps()) à sa première montre : un
 *       widget de plus = son champ dans la struct Page du .cpp et sa ligne dans creer().
 */
#pragma once
#include "esphome.h"
#include <string>

// La page apparaît seulement avec des données (ADR-0046) : une voiture reçue qui a au moins une heure
// de consommation connue pour le jour d'aujourd'hui (tab5_maj_voiture_jour). Jamais sur la 7B (sa
// page est trop basse, comme la consommation de la page Aujourd'hui, ADR-0078).
bool voitures_charge_a_donnees();
// Crée les widgets au premier appel avec `visible` vrai ; les montre ou les cache ensuite.
void voitures_charge_montrer(bool visible);
// Repeint la page (popup ouvert, page affichée) : les écritures sont comparées d'abord.
void voitures_charge_peindre();

// Action tab5_maj_voiture_jour (ADR-0078), aussi lue ici : rang (« 0 » la maison, « 1 » et « 2 » les
// voitures), début (AAAA-MM-JJ) et 24 kWh par heure (conso_jour_lire, tab5_parse.h). Garde les séries ;
// repeint le popup seulement s'il est ouvert.
void voitures_charge_jour(const std::string& rang, const std::string& debut, const std::string& valeurs);
// Action tab5_maj_energie_soleil (ADR-0058), aussi lue ici : la prévision de chaque heure du jour
// (energie_soleil_lire, tab5_parse.h), dont le créneau conseillé se calcule.
void voitures_charge_soleil(const std::string& payload);
