/**
 * [AI-CONTEXT]
 * @file tab5_geometrie.h
 * @role Géométrie partagée par les popups et les pages construits en C++ (audit du
 *       07/10/2026, lot L5) : l'écran, la carte modale (ADR-0009), le corps des popups à
 *       cartes (Énergie, Température) et le nombre de pièces et de tuiles (ADR-0023).
 *       Avant, chaque fichier recopiait ses valeurs en littéral (1280, 1250, 690…).
 * @architecture_constraint En-tête seul, sans dépendance (ni ESPHome ni LVGL). Le C++ ne
 *       lit pas les substitutions ESPHome : ces valeurs sont aussi écrites dans
 *       Tab5/tab5-ui-tokens.yaml et les popups YAML ; tests/test_geometrie_partagee.py
 *       tient les deux côtés égaux et vérifie qu'aucun fichier ne les redéfinit.
 * @ai_instruction Inclus seulement par les .cpp qui s'en servent (pas par
 *       tab5_internal.h) ; listé dans les `includes:` des deux configurations pour être
 *       copié dans le build. Une valeur de plus ici = sa vérification dans
 *       tests/test_geometrie_partagee.py.
 */
#pragma once
#include <cstdint>

// Écran (paysage).
constexpr int32_t kEcranL = 1280;
constexpr int32_t kEcranH = 720;

// Carte modale (tab5-ui-tokens.yaml : modal_card_w, modal_card_h, modal_body_y) : 15 px
// des bords de l'écran, corps sous la barre de titre de modal_header.yaml.
constexpr int32_t kCarteL = 1250;
constexpr int32_t kCarteH = 690;
constexpr int32_t kCorpsY = 72;

// Corps des popups à cartes (energie_popup.yaml, historique_popup.yaml) : x 24..1226,
// cartes séparées de 16 px ; zone de graphique de leur carte du bas (1166 px, 18 px de
// marge de chaque côté) et largeur d'un libellé de son axe, texte centré.
constexpr int32_t kCorpsX = 24;
constexpr int32_t kCorpsW = kCarteL - 2 * kCorpsX;
constexpr int32_t kCartesEcart = 16;
constexpr int32_t kGraphiqueL = 1166;
constexpr int32_t kAxeLibelleL = 120;

// Pièces (pages du bas) et tuiles par pièce (ADR-0023) : le blueprint en envoie autant.
constexpr int kPieces = 5;
constexpr int kTuiles = 5;
