/**
 * [AI-CONTEXT]
 * @file tab5_geometrie.h
 * @role Géométrie partagée par les popups et les pages construits en C++ (audit du
 *       07/10/2026, lot L5) : l'écran, la carte modale (ADR-0009), le corps des popups à
 *       cartes (Énergie, Température) et le nombre de pièces et de tuiles (ADR-0023).
 *       Avant, chaque fichier recopiait ses valeurs en littéral (1280, 1250, 690…).
 * @architecture_constraint En-tête seul, sans dépendance (ni ESPHome ni LVGL). Le C++ ne
 *       lit pas les substitutions ESPHome : ces valeurs sont aussi écrites dans
 *       Tab5/paquets/tab5-ui-tokens.yaml et les popups YAML ; tests/test_geometrie_partagee.py
 *       tient les deux côtés égaux et vérifie qu'aucun fichier ne les redéfinit.
 * @ai_instruction Inclus seulement par les .cpp qui s'en servent (pas par
 *       tab5_internal.h) ; listé dans les `includes:` des deux configurations pour être
 *       copié dans le build. Une valeur de plus ici = sa vérification dans
 *       tests/test_geometrie_partagee.py.
 * @carte Taille de l'écran par carte (ADR-0061) : TAB5_ECRAN_L, TAB5_ECRAN_H et
 *       TAB5_ROTATION viennent des build_flags de tab5-ha-hmi.yaml et de
 *       tab5-rendu-host.yaml, écrits d'après Tab5/paquets/geometrie-<carte>.yaml. Sans eux
 *       (tests compilés sur PC), les valeurs du Tab5. La carte modale en dérive ; le reste
 *       (corps des popups, graphiques) est encore au pixel pour 1280×720.
 */
#pragma once
#include <cstdint>

#ifndef TAB5_ECRAN_L
#define TAB5_ECRAN_L 1280
#endif
#ifndef TAB5_ECRAN_H
#define TAB5_ECRAN_H 720
#endif
#ifndef TAB5_ROTATION
#define TAB5_ROTATION 270
#endif

// Écran (paysage), celui de la carte.
constexpr int32_t kEcranL = TAB5_ECRAN_L;
constexpr int32_t kEcranH = TAB5_ECRAN_H;

// La mise en page (positions et tailles des widgets) est encore dessinée au pixel pour
// 1280 × 720 (ADR-0061) : une vérification de mise en page au pixel ne vaut que sur un écran
// au moins aussi grand, `static_assert(!kMiseEnPageTab5 || …)`. Toujours vraie sur le Tab5.
constexpr bool kMiseEnPageTab5 = kEcranL >= 1280 && kEcranH >= 720;

// Rotation de repos de LVGL (`lvgl: rotation:`, tab5-styles.yaml) : celle que Neon Apron
// remet en quittant son portrait (pinball_game.cpp).
constexpr int kRotationRepos = TAB5_ROTATION;

// Carte modale (tab5-ui-tokens.yaml : modal_card_w, modal_card_h, modal_body_y) : 15 px
// des bords de l'écran, corps sous la barre de titre de modal_header.yaml.
constexpr int32_t kCarteL = kEcranL - 30;
constexpr int32_t kCarteH = kEcranH - 30;
constexpr int32_t kCorpsY = 72;

// Carte du popup Musique (tab5-ui-tokens.yaml : lecteur_card_w, lecteur_card_h ;
// ADR-0050) : centrée, plus petite que la carte modale, le tableau de bord voilé autour.
constexpr int32_t kLecteurCarteL = 940;
constexpr int32_t kLecteurCarteH = 536;

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
