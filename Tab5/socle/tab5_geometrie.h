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
 *       (tests compilés sur PC), les valeurs du Tab5. La carte modale en dérive, et depuis
 *       le lot 2 du portage ce que les popups ont en commun : corps, graphiques, demi-cartes,
 *       onglets d'en-tête, colonnes du calendrier, lignes des pièces, panneaux de la carte
 *       centrale. La mise en page propre de chaque popup reste au pixel pour 1280×720.
 *       Depuis le lot 3, l'accueil entier : celui du Tab5 ramené à l'écran de la carte
 *       (acc_x, acc_y, kAcc… ; jetons acc_* et hor_* de tab5-ui-tokens.yaml), l'horloge à
 *       l'échelle de sa police (TAB5_POLICE_PCT, police_px).
 *       Expressions : de l'arithmétique entière et au plus un `a ? b : c` par constante
 *       (tests/commun.py les recalcule en Python, carte par carte).
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
#ifndef TAB5_POLICE_PCT
#define TAB5_POLICE_PCT 100
#endif

// Écran (paysage), celui de la carte.
constexpr int32_t kEcranL = TAB5_ECRAN_L;
constexpr int32_t kEcranH = TAB5_ECRAN_H;

// La mise en page propre des popups et des jeux est encore dessinée au pixel pour
// 1280 × 720 (ADR-0061) : une vérification de mise en page au pixel ne vaut que sur un écran
// au moins aussi grand, `static_assert(!kMiseEnPageTab5 || …)`. Toujours vraie sur le Tab5.
constexpr bool kMiseEnPageTab5 = kEcranL >= 1280 && kEcranH >= 720;

// Rotation de repos de LVGL (`lvgl: rotation:`, tab5-styles.yaml) : celle d'une carte montée
// dans le sens de son fabricant.
constexpr int kRotationRepos = TAB5_ROTATION;

// Sens de l'écran (ADR-0069) : une carte montée à l'envers tourne l'image de 180° de plus que
// sa rotation de repos (7B : 180 → 0, plus de rotation à chaque image).
constexpr int rotation_sens(int repos, bool retourne) { return retourne ? (repos + 180) % 360 : repos; }
static_assert(rotation_sens(180, true) == 0 && rotation_sens(270, true) == 90 && rotation_sens(180, false) == 180,
              "rotation_sens : un demi-tour de plus, entre 0 et 359");
// La rotation de repos en vigueur : kRotationRepos, ou le sens choisi dans le select « Sens de
// l'écran » de la carte (la 7B seule, carte-waveshare-7b.yaml ; gardé en NVS), posé par
// dalle_sens_choisir() (tab5_dalle.cpp) dès la bande 800 du démarrage : avant la barre de
// chargement (500), qui la lit, et le setup() de LVGL (400). Un jeu qui tourne LVGL la remet
// en sortant. Une seule valeur pour tout le firmware (fonction inline).
inline int& rotation_repos_courante() {
    static int rotation = kRotationRepos;
    return rotation;
}
inline int rotation_repos() { return rotation_repos_courante(); }
// true tant qu'un jeu tient sa propre rotation (portrait) : un sens choisi pendant ce temps
// est seulement posé, le jeu remet rotation_repos() en sortant.
inline bool& rotation_tenue() {
    static bool tenue = false;
    return tenue;
}

// Carte modale (tab5-ui-tokens.yaml : modal_card_w, modal_card_h, modal_body_y) : 15 px
// des bords de l'écran, corps sous la barre de titre de modal_header.yaml.
constexpr int32_t kCarteL = kEcranL - 30;
constexpr int32_t kCarteH = kEcranH - 30;
constexpr int32_t kCorpsY = 72;

// Carte du popup Musique (tab5-ui-tokens.yaml : lecteur_card_w, lecteur_card_h ;
// ADR-0050) : centrée, plus petite que la carte modale, le tableau de bord voilé autour.
constexpr int32_t kLecteurCarteL = 940;
constexpr int32_t kLecteurCarteH = 536;

// Corps des popups (tab5-ui-tokens.yaml : corps_w, corps_h, graphique_l, demi_w, demi_x) :
// x 24..1226 et y 72..670 sur le Tab5 (20 px d'air sous le corps), cartes séparées de 16 px ;
// zone de graphique d'une carte pleine largeur (1166 px, 18 px de marge de chaque côté) et
// largeur d'un libellé de son axe, texte centré ; deux demi-cartes côte à côte (589 px à
// x 24 et 637, 24 px entre elles : Réglages, Réveil, console). Tout suit la carte modale
// (lot 2 du portage 1024×600, ADR-0061).
constexpr int32_t kCorpsX = 24;
constexpr int32_t kCorpsW = kCarteL - 2 * kCorpsX;
constexpr int32_t kCorpsH = kCarteH - kCorpsY - 20;
constexpr int32_t kCartesEcart = 16;
constexpr int32_t kGraphiqueL = kCorpsW - 2 * 18;
constexpr int32_t kAxeLibelleL = 120;
constexpr int32_t kDemiL = (kCorpsW - 24) / 2;
constexpr int32_t kDemiX = kCorpsX + kDemiL + 24;

// Mise en page propre de chaque popup (lot 4 du portage 1024×600, ADR-0061) : dessinée au
// pixel pour le corps du Tab5 (1202 × 598), elle est ramenée au corps de la carte — une
// longueur horizontale par largeur_carte(), verticale par hauteur_carte(), le côté d'un
// carré ou d'un cercle par taille_carte() (la plus petite des deux) —, arrondie en dessous
// (une valeur négative : l'opposée de celle de sa valeur absolue). Le YAML fait le même
// calcul en Jinja : `${ 386 * (corps_w | int) // 1202 }`, `${ 456 * (corps_h | int) // 598 }`
// (tab5-ui-tokens.yaml). Sur le Tab5, chaque valeur reste la même, au pixel.
constexpr int32_t kCorpsWTab5 = 1202;
constexpr int32_t kCorpsHTab5 = 598;
constexpr int32_t largeur_carte(int32_t v) { return v < 0 ? -largeur_carte(-v) : v * kCorpsW / kCorpsWTab5; }
constexpr int32_t hauteur_carte(int32_t v) { return v < 0 ? -hauteur_carte(-v) : v * kCorpsH / kCorpsHTab5; }
constexpr int32_t taille_carte(int32_t v) { return largeur_carte(v) < hauteur_carte(v) ? largeur_carte(v) : hauteur_carte(v); }

// Onglets d'en-tête (noms des pages, pages_onglet.yaml et reglages_onglet.yaml, ADR-0046) :
// sur la ligne du titre, jusqu'à 8 px avant la croix (modal_header.yaml : 80 px à 10 px du
// bord). Leur mise en page est celle du Tab5 (x 318 à 1148 ; le Réveil commence à 196) ;
// sur une carte plus étroite, places et largeurs sont réduites du rapport des deux zones
// (de 196 à la croix), à partir de la croix : onglet_x(), onglet_echelle(). Le YAML fait le
// même calcul (onglets_fin, onglets_zone de tab5-ui-tokens.yaml). Sur le Tab5 : inchangés.
constexpr int32_t kOngletsFin = kCarteL - 102;
constexpr int32_t kOngletsFinTab5 = 1148;
constexpr int32_t kOngletsDebutTab5 = 196;
constexpr int32_t kOngletsZoneTab5 = kOngletsFinTab5 - kOngletsDebutTab5;

// Une longueur de la mise en page des onglets du Tab5, ramenée à la carte (arrondie).
constexpr int32_t onglet_echelle(int32_t v) {
    return kOngletsFin - kOngletsDebutTab5 >= kOngletsZoneTab5
               ? v
               : (v * (kOngletsFin - kOngletsDebutTab5) + kOngletsZoneTab5 / 2) / kOngletsZoneTab5;
}
// Le x d'un onglet posé à `x_tab5` sur le Tab5, sur la carte : sa distance à la croix réduite.
constexpr int32_t onglet_x(int32_t x_tab5) { return kOngletsFin - onglet_echelle(kOngletsFinTab5 - x_tab5); }

// Colonnes de la grille du calendrier (cal_grid_build, tab5_calendar.cpp ; en-têtes « Lun »…
// « Dim » de calendar_popup.yaml, jetons cal_col_*) : sept colonnes au pas de 172 px, cases de
// 168, grille centrée dans la carte (x 25 sur le Tab5).
constexpr int32_t kCalColPas = (kCarteL - 46) / 7;
constexpr int32_t kCalColW = kCalColPas - 4;
constexpr int32_t kCalColX0 = (kCarteL - 7 * kCalColPas + 4) / 2;

// Lignes d'une pièce des popups Lumières et Volets (piece_ligne.yaml, ADR-0046 ; jetons
// piece_ligne_*) : cinq au plus, à y 50 de leur carte, au pas de 92 (lignes de 84) sur le
// Tab5 ; au-dessous, les boutons de la pièce (88 px du bas de la carte). Plus courtes si la
// carte est moins haute.
constexpr int32_t kPieceLignePasTab5 = 92;
constexpr int32_t kPieceLignePasMax = (kCorpsH - 88 - 50 + 8) / 5;
constexpr int32_t kPieceLignePas = kPieceLignePasMax < kPieceLignePasTab5 ? kPieceLignePasMax : kPieceLignePasTab5;
constexpr int32_t kPieceLigneH = kPieceLignePas - 8;
// Largeur d'une ligne (jeton piece_ligne_l, lot 4) : la carte de gauche de ces popups (386
// sur le Tab5) ramenée au corps, moins 22 px de marge de chaque côté ; 342 sur le Tab5.
constexpr int32_t kPieceLigneL = largeur_carte(386) - 44;

// ─── Accueil (lot 3 du portage 1024×600, ADR-0061 ; jetons acc_* de tab5-ui-tokens.yaml) ───
// La mise en page du Tab5 ramenée à l'écran de la carte : un x ou une largeur v du Tab5
// devient acc_x(v) = (v × kEcranL + 640) / 1280, un y ou une hauteur acc_y(v) (même calcul
// sur 720), arrondis au plus proche ; sur le Tab5, v. Le YAML fait le même calcul en
// entiers (tests/test_geometrie_partagee.py compare, carte par carte). Valeurs positives.
constexpr int32_t acc_x(int32_t v) { return (v * kEcranL + 640) / 1280; }
constexpr int32_t acc_y(int32_t v) { return (v * kEcranH + 360) / 720; }
// Marge de l'accueil aux bords de l'écran (20 px sur le Tab5 : acc_marge).
constexpr int32_t kAccMarge = (20 * kEcranL + 640) / 1280;

// Panneaux de la carte centrale de l'accueil (central_card, tab5-lvgl.yaml : x 20, y 333,
// 20 px des bords de l'écran ; jetons central_carte_w, central_w, acc_centrale_y) : largeur
// utile des panneaux, 1180 sur le Tab5 (1240 - 2 × 20 de marge - 2 × 1 de bordure,
// arrondi) ; haut de la carte (sous lui, un glissement pagine les prévisions).
constexpr int32_t kCentralCarteL = kEcranL - 2 * kAccMarge;
constexpr int32_t kPanneauCentralL = kCentralCarteL - 60;
constexpr int32_t kAccCentraleY = (333 * kEcranH + 360) / 720;

// Largeur d'une ligne de zone (rangée sous l'horloge, panneaux Ok Nabu ; acc_ligne_l) :
// celle de l'horloge (401 sur le Tab5), au plus l'intérieur du cadre Ok Nabu (colonne de
// trois boutons et deux gouttières, moins 4).
constexpr int32_t kAccColL = 3 * ((125 * kEcranL + 640) / 1280) + 2 * ((15 * kEcranL + 640) / 1280);
constexpr int32_t kAccHorlogeL = kEcranL - 2 * kAccMarge - 2 * kAccColL - 2 * ((15 * kEcranL + 640) / 1280) + 1;
constexpr int32_t kAccLigneL = kAccHorlogeL < kAccColL - 4 ? kAccHorlogeL : kAccColL - 4;
constexpr int32_t kAccHorlogeH = (210 * kEcranH + 360) / 720;

// Tuiles du bas (prévisions, cartes du mode HA ; acc_tuile_l, acc_tuile_pas, acc_tuile_h,
// acc_tuile_cell_y) : 230 × 275 px au pas de 250, centrées, à y 5 de leur calque ; texte d'une carte du mode HA (206 px, sous sa
// pastille).
constexpr int32_t kAccTuileL = (230 * kEcranL + 640) / 1280;
constexpr int32_t kAccTuilePas = (250 * kEcranL + 640) / 1280;
constexpr int32_t kAccTuileH = (275 * kEcranH + 360) / 720;
constexpr int32_t kAccTuileY = (5 * kEcranH + 360) / 720;  // y des cartes dans leur calque (acc_tuile_cell_y)
constexpr int32_t kAccCarteTexteL = (206 * kEcranL + 640) / 1280;
// Écart entre deux cases d'une tuile coupée en deux ou en quatre (ADR-0066 : moitiés de
// 230 × 135 et quarts de 113 × 135 sur le Tab5, disposition::case_rect).
constexpr int32_t kAccCaseEcart = (4 * kEcranL + 640) / 1280;

// Horloge (clock_tile) : sa géométrie intérieure suit la police des chiffres, réduite de
// carte_police_echelle (TAB5_POLICE_PCT, en pour cent ; jeton police_pct), puis centrée
// dans la tuile (hor_dx, hor_dy : 0 sur le Tab5). police_px(v) : une longueur v de la
// mise en page du Tab5 à cette échelle, arrondie (signe gardé).
constexpr int32_t kPolicePct = TAB5_POLICE_PCT;
constexpr int32_t police_px(int32_t v) { return v >= 0 ? (v * kPolicePct + 50) / 100 : -((-v * kPolicePct + 50) / 100); }
constexpr int32_t kHorlogeDx = (kAccHorlogeL - 2 - (399 * kPolicePct + 50) / 100) / 2;
constexpr int32_t kHorlogeDy = (kAccHorlogeH - (210 * kPolicePct + 50) / 100) / 2;

// Pièces (ADR-0023) : depuis l'ADR-0066 (lot A, 18/10/2026), leur nombre est choisi dans Home
// Assistant (« pn|N » en tête de tab5_maj_tuiles, 1 à kPiecesMax) : les tableaux sont
// dimensionnés sur kPiecesMax, et un blueprint sans « pn » en a kPiecesDefaut, les cinq d'avant.
// La météo garde ses kPagesMeteo pages (0-1 heures, 2-4 jours) ; les pièces 5 et plus y ont
// chacune une page de plus après les jours 10-14.
// Éléments d'une pièce, jusqu'à vingt (ADR-0066, lot B : l'index T de la liste du blueprint,
// clés « tRT », T = 0 … 9 puis a … j) ; places de la bande du bas, cinq tuiles que la
// disposition coupe en deux ou en quatre au-delà de cinq éléments
// (Tab5/socle/tab5_disposition.h). Lignes d'une page des popups à une ligne par élément
// (Lumières, Volets, Maison) : kPlacesTuiles aussi, une pièce de plus de lignes prend plus de
// pages.
constexpr int kPiecesMax = 10;
constexpr int kPiecesDefaut = 5;
constexpr int kPagesMeteo = 5;
constexpr int kTuilesMax = 20;
constexpr int kPlacesTuiles = 5;
