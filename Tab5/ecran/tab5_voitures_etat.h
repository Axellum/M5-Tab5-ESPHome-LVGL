/**
 * [AI-CONTEXT]
 * @file tab5_voitures_etat.h
 * @role Page « État » du popup Voitures (tab5_voitures_etat.cpp, ADR-0081, lot 7 du plan des
 *       voitures) : par voiture, les quatre pneus sur une voiture vue de dessus (pression de
 *       chacun, verdict « Pneus corrects » ou « Pression basse »), la température de l'habitacle
 *       et de l'extérieur, les portes verrouillées ou non, la Sentinelle, le logiciel à jour ou
 *       non. Les données sont les onze derniers champs de tab5_maj_voiture (voiture_lire,
 *       Tab5/socle/tab5_parse.h) ; une valeur que Home Assistant ne donne pas s'écrit « -- ».
 * @architecture_constraint tab5_voitures.h l'inclut : le popup Voitures (tab5_voitures.cpp) appelle
 *       ces quatre fonctions, une table de pages (kPages) les relie à l'onglet « État ». Le code de
 *       la page est ici, jamais dans tab5_voitures.cpp (trois lots y ajoutent chacun une page).
 *       Une fonction déclarée ici a un appelant hors de son fichier (règle 12 de
 *       tools/check_tab5_code_rules.py).
 * @ai_instruction Une ligne de plus sur la page = son entrée dans `Ligne` et dans le tableau
 *       kLignes (tab5_voitures_etat.cpp), son champ en fin de payload (tab5_parse.h, tests, fuzz,
 *       démo, blueprint). Un texte affiché passe par tr().
 */
#pragma once
#include "esphome.h"

struct VoitureLue;

// Script tab5_voitures_lier (tab5-voitures.yaml) : le conteneur de la page (voitures_page_etat), qui
// porte dans l'ordre les quatre cartes de voitures_etat_haut.yaml / voitures_etat_bas.yaml (haut 0,
// bas 0, haut 1, bas 1) ; leurs libellés et leurs icônes y sont déjà. Le dessin de la voiture est
// créé à la première peinture de la page, pas avant. Un gabarit dont le nombre d'enfants n'est pas
// celui que ce fichier attend est journalisé et la page ne s'affiche pas.
void voitures_etat_lier(lv_obj_t* page);

// tab5_maj_voiture, côté popup (voitures_recu) : garde l'état de la voiture de rang k (0 ou 1) ;
// `lu` nul = la voiture est retirée. Ne repeint rien : le popup le fait s'il est ouvert.
void voitures_etat_recu(int k, const VoitureLue* lu);

// Vrai si une voiture reçue a au moins une donnée d'état connue (une pression, une température,
// le verrou, la Sentinelle ou le logiciel) : sans, la page n'existe pas (pas d'onglet, ADR-0046).
bool voitures_etat_a_donnees();

// Peint la page (le popup la montre et cache l'Aperçu) : une colonne par voiture reçue, deux cartes
// l'une sous l'autre ; une seule voiture : les cartes pleine largeur, contenu centré.
void voitures_etat_peindre();
