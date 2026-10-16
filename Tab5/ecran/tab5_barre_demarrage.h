/**
 * [AI-CONTEXT]
 * @file tab5_barre_demarrage.h
 * @role Barre de chargement du démarrage (tab5_barre_demarrage.cpp, 16/10/2026).
 * @architecture_constraint Appelée seulement par l'on_boot de tab5-ha-hmi.yaml (accord
 *       d'Axel du 16/10/2026), pendant setup(), avant la première image de LVGL. Dessine
 *       sur la dalle sans LVGL (Display::draw_pixels_at), d'où la fermeture obligatoire
 *       avant la fin de setup() : après, la dalle est à LVGL seul.
 * @ai_instruction Une déclaration nouvelle de ce module va ici ; tab5_custom.h inclut cet
 *       en-tête.
 */
#pragma once
#include "esphome.h"

// on_boot 500 (la dalle vient d'être initialisée) : fond de la palette, barre vide, et une
// tâche qui la fait avancer. false si la dalle manque ou a échoué, et hors tablette (rendu) :
// rien n'est dessiné, le rétroéclairage reste à la lumière.
bool barre_demarrage_ouvrir(esphome::display::Display* ecran);
// on_boot 399 puis 249 : étape suivante de la barre (barre::kJalons, Tab5/socle/tab5_barre.h).
void barre_demarrage_etape(int etape);
// on_boot -100 (fin de setup()) : arrête la tâche, barre pleine. La première image de LVGL
// la recouvre à la boucle suivante.
void barre_demarrage_fermer();
