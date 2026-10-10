/**
 * [AI-CONTEXT]
 * @file tab5_piece_climat.h
 * @role Climat de la pièce en mode HA (tab5_piece_climat.cpp, ADR-0040) : la zone des
 *       températures de l'accueil (carte clim, climate_card.yaml) montre la pièce affichée
 *       quand le blueprint lui a donné une température, et revient au salon et à la serre
 *       sinon.
 * @architecture_constraint Une fonction déclarée ici a un appelant hors de son fichier
 *       (règle 12 de tools/check_tab5_code_rules.py, qui lit tab5_custom.h et ses en-têtes).
 * @ai_instruction Une déclaration nouvelle de ce module va ici ; ce module est inclus par
 *       tab5_custom.h et listé sous `includes:` des deux configurations racine
 *       (tab5-ha-hmi.yaml, tab5-rendu-host.yaml).
 */
#pragma once
#include "esphome.h"
#include <cstddef>

// =============================================================================
// Climat de la pièce (ADR-0040, 09/10/2026, demande d'Axel) — tab5_piece_climat.cpp
// =============================================================================
// Le blueprint « Tab5 — emplacements » déclare, pièce par pièce (sections « Pièce 1 » à
// « Pièce 5 »), une sonde de température, une sonde d'humidité et une clim, toutes
// facultatives. Il pousse par tab5_maj_emplacements « pR|température|humidité|clim »
// (champ vide : rien de déclaré ; « nan » : valeur inconnue ; clim « 1 » ou « 0 »), et
// les réglages et l'état de la clim de la pièce par « crpR|… » et « cepR|… » (champs de
// crRT / ceRT, ADR-0027 ; tab5_clim.cpp). Un firmware plus ancien ignore ces clés.
//
// En mode HA (ADR-0023), sur une pièce qui a une température déclarée :
//   - à gauche, un thermomètre (couleur de l'humidité de la pièce) et sa température, à
//     la place de l'icône et de la température du salon ;
//   - à droite, une goutte et l'humidité de la pièce (« 48 % ») si elle est déclarée, à
//     la place de la serre ; rien sinon (la zone tactile de la seconde température reste) ;
//   - la tuile − / + règle la clim de la pièce quand elle en a une (tab5_reglables.cpp).
// Hors du mode HA, ou sur une pièce sans température déclarée : le salon et la serre,
// comme avant. Rien en NVS : le blueprint renvoie tout à chaque connexion.

// emplacements_appliquer (tab5_zones.cpp) : « pR|température|humidité|clim ». Faux si
// la clé n'est pas la sienne : l'entrée suit alors la table des emplacements 3.x.
bool piece_climat_recu(const char* cle, size_t n_cle, const char* reste, size_t n_reste);

// Capteurs du salon et de la serre (tab5-sensors-domotique.yaml) : la valeur est gardée,
// et peinte tout de suite quand la zone montre le salon et la serre.
void accueil_salon_temperature(float x);
void accueil_salon_humidite(float x);
void accueil_serre_temperature(float x);

// Toute la zone des températures, d'après la pièce affichée : zones_apply_ui, entrée et
// sortie du mode HA, changement de pièce (tab5_tuiles.cpp), thème (tab5_theme.cpp).
void accueil_temperatures_ui();

// Appui long sur la température de gauche (droite = faux) ou de droite : la clé du popup
// Température (ADR-0032) — « salon », « serre », ou « pR » pour la pièce affichée, des
// deux côtés quand elle a une humidité (ADR-0047) —, ou nullptr quand il n'y a rien à
// montrer (zone absente, pièce sans humidité à droite).
const char* accueil_historique_cle(bool droite);

// Popup Température (tab5_historique.cpp, ADR-0047) : la pièce R a une température
// déclarée par le blueprint, donc son onglet.
bool piece_climat_a_temperature(int r);

// tab5_reglables.cpp : la pièce affichée a une clim déclarée dont la tablette a reçu
// les réglages (crpR) — la tuile − / + la règle. Renvoie R, ou -1.
int piece_climat_clim();
