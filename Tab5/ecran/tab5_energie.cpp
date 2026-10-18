/**
 * [AI-CONTEXT]
 * @file tab5_energie.cpp
 * @role Popup « Énergie » (ADR-0028, 04/10/2026, discussion #278 ; pages : ADR-0058,
 *       10/10/2026) : une installation solaire en quatre pages, celles qui ont des données
 *       seulement (brique commune des popups à pages, tab5_pages.cpp, ADR-0046), ouvert sur
 *       la première qui en a :
 *         - « Flux » (instantané, tab5_maj_energie, avec un capteur solaire) : solaire,
 *           maison, réseau, batterie en cercles (energie_flux_noeud.yaml) reliés par des
 *           traits dont l'épaisseur suit la puissance (partage : energie_flux_calculer,
 *           tab5_parse.h), anneau solaire / batterie / réseau autour de la maison. Statique.
 *         - « Aujourd'hui » (tab5_maj_energie_soleil, energie_soleil_lire) : arc du soleil
 *           de son lever à son coucher, le soleil à sa place à l'heure locale ; les 24 heures
 *           (production réelle de la série des heures en barres pleines, prévision en barres
 *           contour, courbe « ciel clair », bande du meilleur créneau) ; à droite produit,
 *           prévu, demain, meilleur créneau, source de la prévision.
 *         - « Production » (ADR-0028, inchangée) : quatre cartes (solaire, maison, réseau,
 *           batterie ; une carte sans capteur disparaît) et l'historique de la production
 *           en barres (tab5_maj_energie_historique : 24 heures, 30 jours, 12 mois).
 *         - « Bilan » (tab5_maj_energie_bilan, energie_bilan_lire ; en kWh, ADR-0062) : à
 *           gauche la « batterie réseau » du mois civil (net = produit − consommé depuis
 *           le 1er, jauge centrée sur 0, fin du mois au rythme des 7 derniers jours,
 *           produit, consommé, économies avec un prix, part de la consommation couverte
 *           par le soleil) ; à droite, par vue (mêmes boutons que Production, même s_vue :
 *           heures du jour, jours du mois civil, mois de l'année civile), le net de chaque
 *           créneau en barres (surplus en haut, déficit en bas), le cumul en courbe
 *           lissée, le dernier créneau connu pointé, la fin du mois projetée en points.
 *       Home Assistant n'envoie rien tant que le popup est fermé : l'ouverture et chaque
 *       changement de vue émettent esphome.tab5_energie (vue), auquel le blueprint répond
 *       en lançant script.tab5_energie (packages/tab5_energie.yaml), qui pousse
 *       l'instantané tant que « Écran courant » vaut « Énergie ».
 * @architecture_constraint Aucune donnée gardée en NVS : tout repart de HA à l'ouverture
 *       (la RAM garde la dernière poussée, pour ouvrir tout de suite sur la bonne page).
 *       Écritures comparées d'abord (ui_text, ui_hidden, ui_poser, ui_style_* : LVGL 9.5
 *       invalide même à valeur égale). Tout ce que dessine ce fichier (barres, traits,
 *       arcs, points, libellés) est créé UNE fois, à la première ouverture (construire),
 *       jamais pendant une poussée. Changement de page INSTANTANÉ (préférence d'Axel) :
 *       conteneurs masqués / montrés, aucune animation, même sur la page Flux (coût sur
 *       batterie, goût de l'auteur, ADR-0058). Couleurs : palette active (UIColor.X),
 *       rejouées par energie_rejouer_theme (ADR-0029).
 * @ai_warning [AI-WARNING] lv_line_set_points() ne COPIE PAS le tableau de points : ceux des
 *       traits et de la courbe vivent ici, au niveau du fichier (s_traits_pts, s_courbe_pts),
 *       jamais dans une variable locale. LVGL 9.5 ne trace ni bordure ni ligne oblique en
 *       pointillés : l'arc du soleil est fait de points (des ronds), les barres de
 *       prévision ont un contour plein. lv_arc_set_bg_angles() recalcule l'indicateur
 *       depuis la valeur de l'arc : il n'est appelé qu'à la construction, avant tout
 *       lv_arc_set_angles().
 * @ai_instruction Le format des actions est un contrat avec packages/tab5_energie.yaml,
 *       la démo (tools/demo/) et le rendu (tools/rendu/) : tests/test_energie.py les lit
 *       tous ; la lecture des deux actions de l'ADR-0058 est dans tab5_parse.* (testée et
 *       fuzzée). Un texte affiché passe par tr(). Une icône de plus = son glyphe dans
 *       mdi_font_45 (tab5-styles.yaml) ; glyphe_carte() est rattachée aux labels
 *       energie_icone_* et energie_flux_icone_* par MDI_CODE_TARGETS (règle 9). Les calculs
 *       du Bilan (cumul, mois, échelles) sont dans tab5_parse.*, testés sur PC
 *       (tools/test_parse.cpp). Une page de plus : sa valeur dans Page, son conteneur
 *       dans energie_popup.yaml (et EnergieUI::page), son nom dans kNomsPages, sa
 *       condition dans page_a_donnees() et son cas dans peindre().
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include "tab5_parse.h"
#include "lvgl.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

EnergieUI g_energie_ui;

namespace {

enum Carte : int { SOLAIRE = 0, MAISON = 1, RESEAU = 2, BATTERIE = 3, NB_CARTES = 4 };
enum Vue : int { HEURES = 0, JOURS = 1, MOIS = 2, NB_VUES = 3 };
constexpr const char* kVues[NB_VUES] = {"heures", "jours", "mois"};
// Vues du Bilan poussées par un HA de l'ADR-0062 : les heures du jour, le mois civil (du 1er
// à la fin du mois), l'année civile (janvier à décembre). Un HA d'avant pousse kVues : les
// fenêtres glissantes de l'historique (un firmware d'avant refuse ces noms au lieu de
// montrer des créneaux décalés).
constexpr const char* kVuesBilan[NB_VUES] = {"heures", "mois_civil", "annee_civile"};
constexpr int kSlots[NB_VUES] = {24, 30, 12};
constexpr int kSlotsMax = 31;   // les jours d'un mois civil (Bilan)
constexpr int kAxeMax = 12;
static_assert(kSlotsMax == kEnergieSlotsMax, "une série de la vue la plus longue");
static_assert(kEnergieCartes == NB_CARTES, "quatre cartes, page Production");
static_assert(static_cast<int>(EnergieVue::JOURS) == JOURS && static_cast<int>(EnergieVue::MOIS) == MOIS,
              "mêmes vues que tab5_parse.h");

// Pages (ADR-0058), dans l'ordre des onglets ; Production n'a besoin de rien (elle dit
// « En attente de Home Assistant »), elle est toujours là.
enum Page : int { P_FLUX = 0,
                  P_SOLEIL = 1,
                  P_PRODUCTION = 2,
                  P_BILAN = 3,
                  NB_PAGES = 4 };
static_assert(NB_PAGES == kEnergiePages, "un conteneur par page (EnergieUI::page)");
constexpr const char* kNomsPages[NB_PAGES] = {tr_noop("Flux"), tr_noop("Aujourd'hui"), tr_noop("Production"),
                                              tr_noop("Bilan")};
constexpr const char* kPeriodes[NB_VUES] = {tr_noop("Aujourd'hui"), tr_noop("30 derniers jours"),
                                            tr_noop("12 derniers mois")};

// Géométrie (energie_popup.yaml) : une page = un conteneur de 1250 × 598 à y 72 de la
// carte ; corps x 24..1226 (kCorpsX, kCorpsW, kCartesEcart : tab5_geometrie.h).
constexpr int32_t kCartesY = 0;             // avec le graphique
// Sur une autre carte (lot 4 du portage 1024×600), hauteurs et y ramenés au corps
// (hauteur_carte), comme energie_popup.yaml et energie_carte.yaml (tests/test_energie.py).
constexpr int32_t kCarteH210 = hauteur_carte(210);   // hauteur des cartes (210 sur le Tab5)
constexpr int32_t kCartesSeulesY = (kCorpsH - kCarteH210) / 2;  // sans : centrées dans le corps (194)
constexpr int32_t kMargeTexte = 44;         // 22 px de chaque côté
// Zone des barres (energie_zone, energie_bilan_zone : kGraphiqueL × kZoneH, 286 sur le Tab5) :
// maximum en haut, repère, barres, axe ; libellés de l'axe de kAxeLibelleL px, texte centré
// (tab5_geometrie.h). La carte du graphique prend le corps sous les cartes, la zone le reste.
constexpr int32_t kZoneH = kCorpsH - kCarteH210 - 16 - hauteur_carte(72) - 14;
constexpr int32_t kBordX = 20;              // marge : le premier libellé de l'axe tient entier
constexpr int32_t kBarresHaut = 34;
constexpr int32_t kBarresBas = kZoneH - 34;   // 252
constexpr int32_t kAxeY = kZoneH - 28;        // 258
// Sous 10 W, le réseau et la batterie sont « au repos », un trait du Flux aussi.
constexpr float kRepos = 10.0f;

// Page Flux : cercles de kFluxD (energie_flux_noeud.yaml), anneau autour de la maison.
constexpr int32_t kFluxD = 200;
constexpr int32_t kFluxR = kFluxD / 2;
// Deux lignes sous la valeur : le texte (corde de 192 px à son bas) et le détail (168 px).
// Mesurés en Roboto 22 le 10/10/2026 dans les 7 langues : « Geen uitwisseling » 175 px,
// « Aujourd'hui » 112 px ; le détail est un nombre (« 1.47 kWh », « 400 W »).
constexpr int32_t kFluxTexteL = 180;
constexpr int32_t kFluxDetailL = 156;
constexpr int32_t kAnneauEcart = 5;         // entre le cercle et l'anneau
constexpr int32_t kAnneauL = 10;            // épaisseur de l'anneau
constexpr int32_t kAnneauD = 2 * (kFluxR + kAnneauEcart + kAnneauL);
constexpr int32_t kTraitJeu = 8;            // entre un trait et le bord d'un cercle
constexpr int32_t kTraitMin = 2;            // trait sans flux
constexpr int32_t kTraitMax = 16;
constexpr float kTraitPlein = 6000.0f;      // W : le trait le plus épais
struct Centre {
    int32_t x, y;
};
// Centres des cercles dans la page (1250 × 598), dans l'ordre des cartes : avec la batterie
// (en bas), sans (les trois descendent pour remplir la page). Avec la batterie : 10 px
// au-dessus du solaire, 43 px sous la batterie (le bas du verre d'un cercle a la couleur
// du bas de la carte : rien ne doit frôler le bord de la page).
constexpr Centre kCentresBatterie[NB_CARTES] = {{625, 110}, {1035, 285}, {215, 285}, {625, 455}};
constexpr Centre kCentresSans[NB_CARTES] = {{625, 150}, {1035, 410}, {215, 410}, {625, 493}};
// Ces centres sont ceux du Tab5. Sur une autre carte (lot 4 du portage), un centre est ramené
// à sa page : x depuis le milieu de la carte (largeur_carte), y entre 110 (le haut, gardé) et
// le bas de la page moins 143 (les 43 px sous la batterie et son rayon) ; les cercles gardent
// leur taille (polices fixes), la disposition est la même, resserrée. Sur le Tab5 : identité.
constexpr int32_t kFluxHautY = 110;
constexpr int32_t kFluxBasMarge = 143;
constexpr int32_t kFluxMilieuTab5 = (kCorpsWTab5 + 2 * kCorpsX) / 2;   // 625
constexpr Centre centre_carte(Centre t) {
    return {kCarteL / 2 + largeur_carte(t.x - kFluxMilieuTab5),
            kFluxHautY + (t.y - kFluxHautY) * (kCorpsH - kFluxBasMarge - kFluxHautY) /
                             (kCorpsHTab5 - kFluxBasMarge - kFluxHautY)};
}
enum Trait : int { T_SOL_MAISON,
                   T_SOL_RESEAU,
                   T_RESEAU_MAISON,
                   T_SOL_BATT,
                   T_BATT_MAISON,
                   T_RESEAU_BATT,
                   NB_TRAITS };
constexpr int kTraitBouts[NB_TRAITS][2] = {{SOLAIRE, MAISON}, {SOLAIRE, RESEAU}, {RESEAU, MAISON}, {SOLAIRE, BATTERIE}, {BATTERIE, MAISON}, {RESEAU, BATTERIE}};
// Anneau de la maison : la piste, puis les parts solaire, batterie, réseau.
enum Anneau : int { A_PISTE,
                    A_SOLAIRE,
                    A_BATTERIE,
                    A_RESEAU,
                    NB_ANNEAUX };

// Page Aujourd'hui : zone de gauche (energie_soleil_zone, 810 × 570 sur le Tab5), heures sur
// kSolL px. Sur une autre carte (lot 4 du portage) : colonnes ramenées au corps
// (largeur_carte), le haut (arc) ramené (hauteur_carte), le bas (axe, légende) posé depuis le
// bas de la zone ; les libellés gardent leur taille (energie_popup.yaml, même calcul).
constexpr int32_t kSolZoneL = largeur_carte(846) - 36;
constexpr int32_t kSolZoneH = kCorpsH - 28;
// Bords de la zone des heures : 12 px sur le Tab5. « 00:00 » (57 px en roboto_22, qui ne
// rétrécit pas), centré sous 00:30 sur kSolAxeL, perd la gauche de son premier zéro si 00:30
// tombe avant x 28 : sur une zone plus étroite, le bord grandit d'autant (16 px sur la 7B,
// où ce zéro était coupé à moitié, rendu du 18/10/2026).
constexpr int32_t kSolBordMin = 12;
constexpr int32_t kSolBordLibelle = (28 * 1440 - 30 * kSolZoneL + 1379) / 1380;
constexpr int32_t kSolBord = kSolBordLibelle > kSolBordMin ? kSolBordLibelle : kSolBordMin;
constexpr int32_t kSolL = kSolZoneL - 2 * kSolBord;
constexpr int kArcPoints = 29;              // points de l'arc, lever et coucher compris
constexpr int32_t kArcPoint = 6;
constexpr int32_t kAstre = 30;              // le soleil
constexpr int32_t kMidiY = 4;               // « Midi 13:40 » au-dessus du sommet
constexpr int32_t kArcHaut = hauteur_carte(48);    // sommet de l'arc
constexpr int32_t kHorizonY = hauteur_carte(190);
constexpr int32_t kLeverY = kHorizonY + 8;         // « Lever 07:12 », « Coucher 20:05 » (198)
constexpr int32_t kHeureL = 180;            // largeur de ces trois libellés
constexpr int32_t kSolMaxY = kLeverY + 34;  // maximum et « Courbe en cours d'apprentissage » (232)
constexpr int32_t kSolHaut = kSolMaxY + 30; // haut des barres (262)
constexpr int32_t kLegendeY = kSolZoneH - 30;   // 540
constexpr int32_t kSolAxeY = kLegendeY - 38;    // 502
constexpr int32_t kSolBas = kSolAxeY - 6;       // pied des barres (496)
constexpr int32_t kSolAxeL = 64;
constexpr int kSolAxe = 8;                  // 00:00, 03:00… 21:00
constexpr int32_t kLegendePas = largeur_carte(200);
constexpr int32_t kLegendeMarque = 18;
constexpr int kSolLegendes = 4;
constexpr int kLisse = 4;                   // points de la courbe par heure
// Colonne de droite (energie_soleil_infos, 304 × 570 sur le Tab5 : le reste du corps).
constexpr int32_t kInfosL = kCorpsW - largeur_carte(862) - 36;
constexpr int32_t kInfosLigne = hauteur_carte(96);  // une rangée « libellé, valeur »
constexpr int32_t kCadreY = hauteur_carte(300);
constexpr int32_t kCadreH = hauteur_carte(112);
constexpr int32_t kSourceY = kSolZoneH - 30;       // 540

// Page Bilan (ADR-0062). Carte de gauche (energie_bilan_heros, 416 × 594 dans son bord) :
// texte de kHerosX à kHerosX + kHerosW ; mesures en Roboto 700 le 13/10/2026 (fonttools) :
// « +888.8 » 172 px en 55, « kWh » 63 px et « 8888 kWh » 144 px en 32 ; titre du graphique :
// « 30 derniers jours » 244 px en 32 (place : 248 px avant les boutons de vue).
// Sur une autre carte (18/10/2026, la page n'était pas mise en page pour la 7B : bas de la
// carte de gauche et du graphique coupés, titre sous les boutons, axe de droite et légende
// hors de la carte) : la carte de gauche garde ses 420 px de large (ses phrases en roboto_22,
// qui ne rétrécit pas, y tiennent), ses hauteurs sont ramenées au corps (hauteur_carte) et ce
// qui suit une police ramenée l'est comme elle (police_px : pas des lignes, anneau) ; le
// graphique prend le reste de la largeur, son bas (note, légende, axe) posé depuis le bas de
// sa zone. Sur le Tab5, chaque valeur reste la même.
constexpr int32_t kHerosX = 24;
constexpr int32_t kHerosW = 372;
constexpr int32_t kHerosTitreY = hauteur_carte(22);
constexpr int32_t kHerosGrandY = hauteur_carte(50);  // net du mois (police_grande)
constexpr int32_t kHerosUniteDy = police_px(21);     // « kWh » sur la même ligne de base
constexpr int32_t kHerosSousY = hauteur_carte(124);
constexpr int32_t kJaugeY = hauteur_carte(168);
constexpr int32_t kJaugeL = 330;
constexpr int32_t kJaugeH = hauteur_carte(80);
constexpr int32_t kJaugeBord = 3;
constexpr int32_t kJaugeJeu = 8;            // entre le bord et le remplissage
constexpr int32_t kJaugeBoutL = 12;         // le « + » de la pile, à droite
constexpr int32_t kJaugeBoutH = 30;
constexpr int32_t kJaugeDedans = kJaugeBord + kJaugeJeu;
constexpr int32_t kJaugeDemi = (kJaugeL - 2 * kJaugeDedans) / 2;
constexpr int32_t kJaugeZeroX = kHerosX + kJaugeDedans + kJaugeDemi;
constexpr int32_t kEchelleY = hauteur_carte(254);
constexpr int32_t kFinY = hauteur_carte(288);  // « Fin du mois (…) » à gauche, valeur à droite
constexpr int32_t kFinEcart = 8;            // entre les deux (la valeur prend sa largeur :
                                            // « ~ +37 » 83 px, le titre français 272 px)
constexpr int32_t kSep1Y = hauteur_carte(336);
constexpr int32_t kLigneY = hauteur_carte(346);  // produit, consommé, économies
constexpr int32_t kLignePas = police_px(38);
constexpr int32_t kLigneValeurL = 190;
constexpr int32_t kAnneauBilanD = police_px(92);
constexpr int32_t kAnneauBilanL = 12;
constexpr int kHerosLignes = 3;
// Graphique (energie_bilan_zone, kBilZoneL × 530 sous le titre et les boutons) : barres et
// courbe de kBilX0 à kBilX1 et de kBilY0 à kBilY1 ; graduations du net à gauche, du cumul
// à droite ; axe, légende, puis la note de l'estimation.
// Zone : energie_bilan_zone (corps_w − 438 × corps_h − 64 ramené − 4, energie_popup.yaml),
// 764 × 530 sur le Tab5, 508 × 423 sur la 7B. Trois légendes sur une ligne si elles y
// tiennent (240 px chacune), sinon la troisième (le cumul) sur une seconde ligne.
constexpr int kBilanLegendes = 3;
constexpr int32_t kBilZoneL = kCorpsW - 438;
constexpr int32_t kBilZoneH = kCorpsH - hauteur_carte(64) - 4;
constexpr int32_t kBilUniteY = 4;           // « kWh / jour », « cumul »
constexpr int32_t kBilX0 = 60;
constexpr int32_t kBilX1 = kBilZoneL - 68;  // 696 ; les graduations du cumul à droite
constexpr int32_t kBilY0 = 48;
constexpr int32_t kBilAxeL = 64;
constexpr int32_t kBilLegendeX = 20;
constexpr int32_t kBilLegendePas = 240;
constexpr int kBilLegendesLigne = kBilLegendeX + kBilanLegendes * kBilLegendePas <= kBilZoneL ? kBilanLegendes : 2;
constexpr int32_t kBilLegendeRangee = 32;   // de la première ligne de légendes à la seconde
constexpr int32_t kBilLegendeEnPlus = kBilLegendesLigne < kBilanLegendes ? kBilLegendeRangee : 0;
constexpr int32_t kBilNoteY = kBilZoneH - 42;                         // 488
constexpr int32_t kBilLegendeY = kBilNoteY - 36 - kBilLegendeEnPlus;  // 452
constexpr int32_t kBilAxeY = kBilLegendeY - 44;                       // 408
constexpr int32_t kBilY1 = kBilAxeY - 8;                              // 400
constexpr int32_t kBilNoteL = kBilZoneL - 2 * kBilLegendeX < 600 ? kBilZoneL - 2 * kBilLegendeX : 600;
constexpr int32_t kBulleL = 150;            // la bulle du dernier créneau connu
constexpr int32_t kBulleH = 40;
constexpr int32_t kPointD = 18;             // son point sur la courbe
constexpr int32_t kProjD = 6;               // un point de la projection
constexpr int32_t kFinL = 300;              // « ~ +37 fin du mois »
// Boutons de vue de la carte du graphique (vue_btn.yaml, jeton bil_vue_l) : 150 px sur le
// Tab5, 10 px entre eux, le dernier à 18 px du bord ; plus étroits sur une carte où le titre
// n'aurait plus 184 px (« Aujourd'hui »). Le titre va jusqu'à 10 px du premier (248 sur le Tab5).
constexpr int32_t kBilCarteL = kBilZoneL + 4;
constexpr int32_t kBilVueLMax = (kBilCarteL - 18 - 2 * 10 - 184) / 3;
constexpr int32_t kBilVueL = kBilVueLMax < 150 ? kBilVueLMax : 150;
constexpr int32_t kBilTitreL = kBilCarteL - 18 - 3 * kBilVueL - 2 * 10 - 22 - 10;
constexpr int kGrille = 8;                  // graduations de chaque axe, au plus

struct Mesure {
    bool choisi = false;
    float v = NAN;
};

struct Instant {
    bool recu = false;
    Mesure solaire, maison, reseau, batterie, batterie_puissance, batterie_temperature, jour;
    char unite_temperature[8] = {};
};

struct Serie {
    bool recue = false;
    int annee = 0, mois = 0, jour = 0;
    int n = 0;
    float v[kSlotsMax] = {};
};

// Bilan d'une vue (tab5_maj_energie_bilan) : la devise copiée, le reste tel que lu.
struct Bilan {
    bool recu = false;
    bool montre = false;      // vente ou achat choisi, ou production et consommation
    bool calendaire = false;  // vue civile (ADR-0062) ; faux : fenêtre glissante d'un HA
                              // d'avant (« jours », « mois »)
    int annee = 2000, mois = 1, jour = 1;
    int n = 0;                // créneaux de la vue (24, 28 à 31, 30 ou 12)
    char devise[kEnergieDeviseMax + 1] = {};
    EnergieBilanLu lu;
};

Instant s_i;
Serie s_series[NB_VUES];
Bilan s_bilans[NB_VUES];
EnergieSoleilLu s_soleil;
bool s_soleil_montre = false;
int s_vue = HEURES;
int s_page = P_PRODUCTION;
bool s_page_choisie = false;  // la page a été choisie au doigt depuis l'ouverture

// Production : barres et axe.
lv_obj_t* s_barres[kSlotsMax] = {};
lv_obj_t* s_axe[kAxeMax] = {};
lv_obj_t* s_repere = nullptr;
lv_obj_t* s_maximum = nullptr;
lv_obj_t* s_vide = nullptr;                 // « Aucun historique » au milieu de la zone

// Flux.
struct Flux {
    lv_obj_t* trait[NB_TRAITS] = {};
    lv_obj_t* anneau[NB_ANNEAUX] = {};
    int disposition = -1;                   // 1 avec la batterie, 0 sans, -1 pas encore posé
};
Flux s_flux;
lv_point_precise_t s_traits_pts[NB_TRAITS][2];

// Aujourd'hui.
struct Soleil {
    lv_obj_t* bande = nullptr;
    lv_obj_t* repere = nullptr;
    lv_obj_t* barre[kEnergieHeures] = {};
    lv_obj_t* prevu[kEnergieHeures] = {};
    lv_obj_t* courbe = nullptr;
    lv_obj_t* maintenant = nullptr;
    lv_obj_t* point[kArcPoints] = {};
    lv_obj_t* horizon = nullptr;
    lv_obj_t* astre = nullptr;
    lv_obj_t* lever = nullptr;
    lv_obj_t* midi = nullptr;
    lv_obj_t* coucher = nullptr;
    lv_obj_t* sans_soleil = nullptr;
    lv_obj_t* maximum = nullptr;
    lv_obj_t* apprentissage = nullptr;
    lv_obj_t* vide = nullptr;
    lv_obj_t* axe[kSolAxe] = {};
    lv_obj_t* marque[kSolLegendes] = {};
    lv_obj_t* legende[kSolLegendes] = {};
    // Colonne de droite : produit, prévu, demain ; cadre du meilleur créneau ; source.
    lv_obj_t* titre[3] = {};
    lv_obj_t* valeur[3] = {};
    lv_obj_t* cadre = nullptr;
    lv_obj_t* cadre_titre = nullptr;
    lv_obj_t* cadre_valeur = nullptr;
    lv_obj_t* source = nullptr;
};
Soleil s_sol;
lv_point_precise_t s_courbe_pts[(kEnergieHeures - 1) * kLisse + 1];
// Ce qui a donné s_courbe_pts : la courbe n'est recalculée (et repassée à LVGL, qui
// invalide la ligne) que si le ciel clair reçu ou l'échelle ont changé.
struct CourbeCle {
    int n = -1;                       // -1 : jamais calculée
    float maximum = 0.0f;
    float clair[kEnergieHeures] = {};
    int points = 0;                   // nombre de points de s_courbe_pts
};
CourbeCle s_courbe_cle;

// Bilan (ADR-0062) : la carte « Batterie réseau » et le graphique du net.
struct BilanUI {
    // Carte de gauche.
    lv_obj_t* titre = nullptr;            // « BATTERIE RÉSEAU · OCTOBRE »
    lv_obj_t* grand = nullptr;            // « +23.4 »
    lv_obj_t* grand_unite = nullptr;      // « kWh », après le nombre
    lv_obj_t* sous = nullptr;             // « d'avance sur le réseau ce mois-ci »
    lv_obj_t* jauge = nullptr;
    lv_obj_t* jauge_bout = nullptr;
    lv_obj_t* jauge_proj = nullptr;       // la fin du mois projetée, estompée
    lv_obj_t* jauge_plein = nullptr;      // le net du mois
    lv_obj_t* jauge_zero = nullptr;
    lv_obj_t* echelle[3] = {};            // « -40 », « 0 », « +40 kWh »
    lv_obj_t* fin_titre = nullptr;
    lv_obj_t* fin_valeur = nullptr;
    lv_obj_t* sep[2] = {};
    lv_obj_t* ligne_titre[kHerosLignes] = {};   // produit, consommé, économies
    lv_obj_t* ligne_valeur[kHerosLignes] = {};
    lv_obj_t* anneau_piste = nullptr;
    lv_obj_t* anneau = nullptr;
    lv_obj_t* anneau_pct = nullptr;
    lv_obj_t* anneau_texte = nullptr;
    // Graphique.
    lv_obj_t* unite = nullptr;            // « kWh / jour »
    lv_obj_t* cumul_titre = nullptr;      // « cumul », au-dessus de l'axe de droite
    lv_obj_t* vide = nullptr;
    lv_obj_t* grille[kGrille] = {};
    lv_obj_t* grad_net[kGrille] = {};
    lv_obj_t* grad_cumul[kGrille] = {};
    lv_obj_t* zero = nullptr;
    lv_obj_t* barre[kSlotsMax] = {};
    lv_obj_t* courbe = nullptr;
    lv_obj_t* proj[kSlotsMax] = {};
    lv_obj_t* point = nullptr;
    lv_obj_t* bulle = nullptr;
    lv_obj_t* bulle_texte = nullptr;
    lv_obj_t* fin = nullptr;              // « ~ +37 fin du mois »
    lv_obj_t* axe[kAxeMax] = {};
    lv_obj_t* marque[kBilanLegendes] = {};
    lv_obj_t* legende[kBilanLegendes] = {};
    lv_obj_t* note = nullptr;             // « achat / vente estimés heure par heure »
};
BilanUI s_bil;
// Points de la courbe du cumul (lv_line_set_points ne copie pas : [AI-WARNING] de l'en-tête),
// le point de départ à 0 compris ; repassés à LVGL seulement s'ils changent.
constexpr int kCumulPoints = (kCourbeLissePoints - 1) * kLisse + 1;
static_assert(kSlotsMax + 1 <= kCourbeLissePoints, "le départ et un point par créneau");
lv_point_precise_t s_cumul_pts[kCumulPoints];
int s_cumul_n = 0;

// --- Lecture des payloads ---------------------------------------------------------------

// Champs et nombres : champ_suivant() et champ_nombre() (tab5_champs.h ; NAN s'il ne se
// lit pas : « nan », « unknown », vide).
Mesure lire_mesure(const Champ& c) {
    Mesure m;
    m.choisi = c.n > 0;
    m.v = champ_nombre(c, NAN);
    return m;
}

int vue_de(const std::string& nom) {
    for (int v = 0; v < NB_VUES; v++)
        if (nom == kVues[v]) return v;
    return -1;
}

// Dates de l'axe des jours : jours_du_mois() (tab5_core.h).

// --- Pages ------------------------------------------------------------------------------

bool visible() {
    const EnergieUI& u = g_energie_ui;
    return u.popup != nullptr && !lv_obj_has_flag(u.popup, LV_OBJ_FLAG_HIDDEN);
}

bool page_a_donnees(int p) {
    switch (p) {
        case P_FLUX: return s_i.recu && s_i.solaire.choisi;
        case P_SOLEIL: return s_soleil_montre;
        case P_BILAN:
            for (const Bilan& b : s_bilans)
                if (b.recu && b.montre) return true;
            return false;
        default: return true;
    }
}

// Pages montrées, dans l'ordre ; renvoie leur nombre (Production toujours : au moins 1).
int pages_montrees(int liste[NB_PAGES]) {
    int n = 0;
    for (int p = 0; p < NB_PAGES; p++)
        if (page_a_donnees(p)) liste[n++] = p;
    return n;
}

// Première page qui a des données (ADR-0058) : Flux, puis Aujourd'hui, sinon Production.
int premiere_page() {
    for (int p = 0; p < NB_PAGES; p++)
        if (page_a_donnees(p)) return p;
    return P_PRODUCTION;
}

int nombre_pages() {
    int liste[NB_PAGES];
    return pages_montrees(liste);
}

int page_courante() {
    int liste[NB_PAGES];
    const int n = pages_montrees(liste);
    for (int i = 0; i < n; i++)
        if (liste[i] == s_page) return i;
    return 0;
}

// --- Dessin : briques --------------------------------------------------------------------

// Heure locale en minutes depuis minuit (tab5_time_source) ; -1 tant que l'horloge n'est pas
// réglée.
int minutes_maintenant() {
    const time_t now = tab5_time_source(nullptr);
    if (!tab5_heure_valide(now)) return -1;
    struct tm t{};
    localtime_r(&now, &t);
    return t.tm_hour * 60 + t.tm_min;
}

// Un texte non cliquable, masqué ; largeur fixe (> 0 : texte coupé, aligné) ou au contenu.
lv_obj_t* libelle(lv_obj_t* parent, const esphome::font::Font* police, int32_t largeur,
                  lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
    lv_obj_t* l = lv_label_create(parent);
    if (police != nullptr) esphome::lvgl::lv_obj_set_style_text_font(l, police, LV_PART_MAIN);
    lv_label_set_text(l, "");
    if (largeur > 0) {
        lv_obj_set_width(l, largeur);
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_style_text_align(l, align, LV_PART_MAIN);
    }
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

// Un arc de dessin (anneau de la maison, du Bilan) : ni bouton, ni toucher, ni marge ;
// MAIN = la piste (montrée par `piste`), INDICATOR = une part.
lv_obj_t* arc_dessin(lv_obj_t* parent, bool piste, int32_t diametre = kAnneauD, int32_t epaisseur = kAnneauL) {
    lv_obj_t* a = lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, diametre, diametre);
    lv_arc_set_rotation(a, 270);        // 0° en haut, sens des aiguilles
    lv_arc_set_bg_angles(a, 0, 360);    // avant tout lv_arc_set_angles ([AI-WARNING])
    for (lv_part_t part : {LV_PART_MAIN, LV_PART_INDICATOR}) {
        lv_obj_set_style_arc_width(a, epaisseur, part);
        lv_obj_set_style_arc_rounded(a, false, part);
    }
    lv_obj_set_style_arc_opa(a, piste ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, piste ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(a, LV_OBJ_FLAG_HIDDEN);
    return a;
}

// Couleur d'arc d'une partie (MAIN : la piste, INDICATOR : la part), écrite si elle change.
void arc_couleur(lv_obj_t* a, lv_part_t part, uint32_t hex) {
    if (a == nullptr) return;
    const lv_color_t c = lv_color_hex(hex);
    lv_style_value_t cur;
    if (lv_obj_get_local_style_prop(a, LV_STYLE_ARC_COLOR, &cur, part) == LV_STYLE_RES_FOUND &&
        lv_color_eq(cur.color, c))
        return;
    lv_obj_set_style_arc_color(a, c, part);
}

// Bordure d'un rectangle (contour d'une barre de prévision, cadre), écrite si elle change.
void contour(lv_obj_t* o, uint32_t couleur, int32_t largeur, lv_opa_t opa) {
    ui_style_couleur(o, LV_STYLE_BORDER_COLOR, couleur);
    ui_style_num(o, LV_STYLE_BORDER_WIDTH, largeur);
    ui_style_num(o, LV_STYLE_BORDER_OPA, opa);
}

// Hauteur en px d'une valeur sur une échelle (2 px au moins : la donnée existe).
int32_t hauteur_de(float v, float maximum, int32_t pleine) {
    int32_t h = maximum > 0.0f ? static_cast<int32_t>(std::lround(v / maximum * pleine)) : 0;
    return h < 2 ? 2 : h;
}

// Icône d'une carte (mdi_font_45). etat : réseau 1 achat, -1 vente, 0 rien ; batterie
// 1 charge, -1 décharge, 0 repos.
const char* glyphe_carte(int carte, int etat) {
    switch (carte) {
        case SOLAIRE: return "\U000F0A72";                                   // solar-power
        case MAISON: return "\U000F1903";                                    // home-lightning-bolt
        case RESEAU:
            if (etat > 0) return "\U000F192D";                               // transmission-tower-import
            if (etat < 0) return "\U000F192C";                               // transmission-tower-export
            return "\U000F0D3E";                                             // transmission-tower
        default:
            if (etat > 0) return "\U000F17E0";                               // battery-arrow-up
            if (etat < 0) return "\U000F17DE";                               // battery-arrow-down
            return "\U000F0079";                                             // battery
    }
}

void puissance(char* out, size_t n, float w) {
    if (std::isnan(w)) snprintf(out, n, "--");
    else energie_formater(out, n, w, "W");
}

void kwh(char* out, size_t n, float v) {
    if (!std::isfinite(v)) snprintf(out, n, "--");
    else energie_formater(out, n, v, "kWh");
}

void peindre_carte(int c, const char* valeur, uint32_t couleur_valeur, const char* glyphe, uint32_t couleur_icone,
                   const char* l1, const char* l2, int32_t largeur) {
    const EnergieUI& u = g_energie_ui;
    ui_text(u.valeur[c], valeur);
    ui_text_color(u.valeur[c], couleur_valeur);
    ui_text(u.icone[c], glyphe);
    ui_text_color(u.icone[c], couleur_icone);
    texte_ha_coupe(u.ligne1[c], l1, largeur - kMargeTexte);
    texte_ha_coupe(u.ligne2[c], l2, largeur - kMargeTexte);
}

// Cartes c0..c0+3 montrées, réparties sur toute la largeur du corps, dans l'ordre, à la
// hauteur y. Renvoie leur largeur (0 : aucune).
int32_t cartes_placer(int c0, const bool montre[NB_CARTES], int32_t y) {
    const EnergieUI& u = g_energie_ui;
    int n = 0;
    for (int c = 0; c < NB_CARTES; c++) n += montre[c] ? 1 : 0;
    if (n == 0) {
        for (int c = 0; c < NB_CARTES; c++) ui_hidden(u.carte[c0 + c], true);
        return 0;
    }
    const int32_t largeur = (kCorpsW - (n - 1) * kCartesEcart) / n;
    int k = 0;
    for (int c = 0; c < NB_CARTES; c++) {
        lv_obj_t* o = u.carte[c0 + c];
        ui_hidden(o, !montre[c]);
        if (!montre[c] || o == nullptr) continue;
        if (lv_obj_get_style_width(o, LV_PART_MAIN) != largeur) lv_obj_set_width(o, largeur);
        ui_x(o, kCorpsX + k * (largeur + kCartesEcart));
        ui_y(o, y);
        k++;
    }
    return largeur;
}

// Ce que montre une carte de l'instantané, partagé par la page Production (cartes) et la
// page Flux (cercles) : valeur, son état (glyphe_carte), la couleur de l'icône, sa ligne.
struct Vu {
    char valeur[24] = "--";
    uint32_t couleur_valeur = 0;
    int etat = 0;
    uint32_t couleur = 0;
    char ligne[48] = "";    // carte (Production) : une ligne
    char cercle[48] = "";   // cercle (Flux) : le texte, puis…
    char detail[24] = "";   // … son nombre dessous (« Aujourd'hui » / « 1.47 kWh »)
};

// Solaire : sa puissance (sinon la production du jour), et la production du jour.
Vu vu_solaire() {
    const Instant& i = s_i;
    Vu r;
    char x[24];
    if (i.solaire.choisi) puissance(r.valeur, sizeof(r.valeur), i.solaire.v);
    else if (!std::isnan(i.jour.v)) energie_formater(r.valeur, sizeof(r.valeur), i.jour.v, "kWh");
    if (i.jour.choisi && i.solaire.choisi) {
        kwh(x, sizeof(x), i.jour.v);
        snprintf(r.ligne, sizeof(r.ligne), "%s %s", tr("Aujourd'hui"), x);
        snprintf(r.cercle, sizeof(r.cercle), "%s", tr("Aujourd'hui"));
        snprintf(r.detail, sizeof(r.detail), "%s", x);
    } else if (i.jour.choisi) {
        snprintf(r.ligne, sizeof(r.ligne), "%s", tr("Produit aujourd'hui"));
        snprintf(r.cercle, sizeof(r.cercle), "%s", tr("Aujourd'hui"));
    }
    const bool produit = !std::isnan(i.solaire.v) && i.solaire.v >= kRepos;
    r.couleur_valeur = UIColor.TEXT_PRIMARY;
    r.couleur = produit ? UIColor.GOLD : UIColor.TEXT_DIM;
    return r;
}

// Réseau : + achat (import), − vente (export) ; la valeur sans son signe.
Vu vu_reseau() {
    Vu r;
    const float v = s_i.reseau.v;
    r.etat = std::isnan(v) || std::fabs(v) < kRepos ? 0 : (v > 0 ? 1 : -1);
    puissance(r.valeur, sizeof(r.valeur), std::isnan(v) ? NAN : std::fabs(v));
    const char* sens = r.etat > 0 ? tr("Depuis le réseau") : r.etat < 0 ? tr("Vers le réseau")
                                                                        : tr("Aucun échange");
    snprintf(r.ligne, sizeof(r.ligne), "%s", std::isnan(v) ? "" : sens);
    snprintf(r.cercle, sizeof(r.cercle), "%s", r.ligne);
    r.couleur_valeur = UIColor.TEXT_PRIMARY;
    r.couleur = r.etat > 0 ? UIColor.WARNING : r.etat < 0 ? UIColor.SUCCESS
                                                          : UIColor.TEXT_DIM;
    return r;
}

// Batterie : son niveau (sinon sa puissance), charge / décharge.
Vu vu_batterie() {
    const Instant& i = s_i;
    Vu r;
    const float p = i.batterie_puissance.v;
    r.etat = std::isnan(p) || std::fabs(p) < kRepos ? 0 : (p > 0 ? 1 : -1);
    r.couleur_valeur = UIColor.TEXT_PRIMARY;
    if (i.batterie.choisi) {
        if (!std::isnan(i.batterie.v)) snprintf(r.valeur, sizeof(r.valeur), "%.0f %%", i.batterie.v);
        r.couleur_valeur = get_battery_color(i.batterie.v);
    } else {
        puissance(r.valeur, sizeof(r.valeur), std::isnan(p) ? NAN : std::fabs(p));
    }
    if (i.batterie_puissance.choisi && !std::isnan(p)) {
        if (r.etat == 0) {
            snprintf(r.ligne, sizeof(r.ligne), "%s", tr("Au repos"));
            snprintf(r.cercle, sizeof(r.cercle), "%s", r.ligne);
        } else {
            char x[24];
            puissance(x, sizeof(x), std::fabs(p));
            const char* sens = r.etat > 0 ? tr("Charge") : tr("Décharge");
            snprintf(r.ligne, sizeof(r.ligne), "%s %s", sens, x);
            snprintf(r.cercle, sizeof(r.cercle), "%s", sens);
            snprintf(r.detail, sizeof(r.detail), "%s", x);
        }
    }
    r.couleur = r.etat > 0 ? UIColor.SUCCESS : r.etat < 0 ? UIColor.WARNING
                                                          : get_battery_color(i.batterie.v);
    return r;
}

// --- Page Production ---------------------------------------------------------------------

void peindre_instant() {
    const EnergieUI& u = g_energie_ui;
    const Instant& i = s_i;
    bool montre[NB_CARTES];
    montre[SOLAIRE] = i.solaire.choisi || i.jour.choisi;
    montre[MAISON] = i.maison.choisi;
    montre[RESEAU] = i.reseau.choisi;
    montre[BATTERIE] = i.batterie.choisi || i.batterie_puissance.choisi;
    int n = 0;
    for (bool m : montre) n += m ? 1 : 0;
    const bool graphique = i.jour.choisi;
    ui_hidden(u.graphique, !graphique);
    // Message central : avant le premier instantané, ou HA a répondu sans aucun capteur
    // (section « Énergie » du blueprint vide).
    ui_hidden(u.attente, i.recu && n > 0);
    ui_text(u.attente, i.recu ? tr("Aucun capteur d'énergie choisi") : tr("En attente de Home Assistant"));
    const int32_t largeur = cartes_placer(SOLAIRE, montre, graphique ? kCartesY : kCartesSeulesY);
    if (largeur == 0) return;

    if (montre[SOLAIRE]) {
        const Vu v = vu_solaire();
        peindre_carte(SOLAIRE, v.valeur, v.couleur_valeur, glyphe_carte(SOLAIRE, 0), v.couleur, v.ligne, "", largeur);
    }
    char v[24], x[24], l2[48];
    if (montre[MAISON]) {
        puissance(v, sizeof(v), i.maison.v);
        peindre_carte(MAISON, v, UIColor.TEXT_PRIMARY, glyphe_carte(MAISON, 0), UIColor.INFO, tr("Consommation"), "",
                      largeur);
    }
    if (montre[RESEAU]) {
        const Vu r = vu_reseau();
        peindre_carte(RESEAU, r.valeur, r.couleur_valeur, glyphe_carte(RESEAU, r.etat), r.couleur, r.ligne, "", largeur);
    }
    // Batterie : et sa température.
    if (montre[BATTERIE]) {
        const Vu b = vu_batterie();
        l2[0] = '\0';
        if (i.batterie_temperature.choisi) {
            if (std::isnan(i.batterie_temperature.v)) snprintf(x, sizeof(x), "--");
            else snprintf(x, sizeof(x), "%.1f %s", i.batterie_temperature.v, i.unite_temperature);
            snprintf(l2, sizeof(l2), "%s %s", tr("Température"), x);
        }
        peindre_carte(BATTERIE, b.valeur, b.couleur_valeur, glyphe_carte(BATTERIE, b.etat), b.couleur, b.ligne, l2,
                      largeur);
    }
}

// Titre du graphique : la période et son total.
void peindre_titre(const Serie& s) {
    char total[24], buf[64];
    float somme = 0.0f;
    bool une = false;
    for (int k = 0; k < s.n; k++)
        if (!std::isnan(s.v[k])) {
            somme += s.v[k];
            une = true;
        }
    if (s_vue == HEURES && !std::isnan(s_i.jour.v)) {
        somme = s_i.jour.v;   // le total du jour, au plus près (le dernier partiel compris)
        une = true;
    }
    if (une) energie_formater(total, sizeof(total), somme, "kWh");
    else snprintf(total, sizeof(total), "--");
    snprintf(buf, sizeof(buf), "%s \xC2\xB7 %s", tr(kPeriodes[s_vue]), total);
    ui_text(g_energie_ui.titre, buf);
}

// Libellé de l'axe sous la barre k : heures 00:00, 03:00… ; jours : le quantième tous les
// cinq jours en finissant par aujourd'hui ; mois : leur nom court.
bool libelle_axe(const Serie& s, int k, char* out, size_t n) {
    switch (s_vue) {
        case HEURES:
            if (k % 3 != 0) return false;
            snprintf(out, n, "%02d:00", k);
            return true;
        case JOURS: {
            if ((s.n - 1 - k) % 5 != 0) return false;
            int a = s.annee, m = s.mois, j = s.jour + k;
            while (m >= 1 && m <= 12 && j > jours_du_mois(a, m)) {
                j -= jours_du_mois(a, m);
                if (++m > 12) {
                    m = 1;
                    a++;
                }
            }
            snprintf(out, n, "%d", j);
            return true;
        }
        default: {
            const int m = ((s.mois - 1 + k) % 12 + 12) % 12 + 1;
            snprintf(out, n, "%s", month_short_utf8(m));
            return true;
        }
    }
}

// Libellés de l'axe d'une zone de barres (Production, Bilan) : centrés sous leur barre.
void peindre_axe(lv_obj_t* const* axe, const Serie& s, int nb, int32_t pas) {
    char buf[24];
    int a = 0;
    for (int k = 0; k < nb && a < kAxeMax; k++) {
        if (!s.recue || !libelle_axe(s, k, buf, sizeof(buf))) continue;
        lv_obj_t* l = axe[a++];
        ui_text(l, buf);
        ui_hidden(l, false);
        // Centré sous sa barre (largeur fixe kAxeLibelleL, texte centré dedans).
        ui_x(l, kBordX + k * pas + pas / 2 - kAxeLibelleL / 2);
    }
    for (; a < kAxeMax; a++) ui_hidden(axe[a], true);
}

void peindre_graphique() {
    EnergieUI& u = g_energie_ui;
    for (int v = 0; v < NB_VUES; v++) highlight_button_border(u.vue_btn[v], v == s_vue, UIColor.ACCENT);
    if (s_barres[0] == nullptr) return;
    const Serie& s = s_series[s_vue];
    peindre_titre(s);
    // La dernière barre qui a une valeur : l'heure, le jour ou le mois en cours.
    int courante = -1;
    float maximum = 0.0f;
    for (int k = 0; k < s.n; k++)
        if (!std::isnan(s.v[k])) {
            courante = k;
            if (s.v[k] > maximum) maximum = s.v[k];
        }
    const bool vide = !s.recue || courante < 0;
    ui_hidden(s_repere, vide);
    ui_hidden(s_maximum, vide);
    // Pas de barres : le message dit pourquoi.
    ui_hidden(s_vide, !vide);
    if (vide) ui_text(s_vide, s.recue ? tr("Aucun historique") : tr("En attente de Home Assistant"));
    char buf[24];
    if (!vide) {
        energie_formater(buf, sizeof(buf), maximum, "kWh");
        ui_text(s_maximum, buf);
    }
    const int nb = s.n > 0 ? s.n : kSlots[s_vue];
    const int32_t pas = (kGraphiqueL - 2 * kBordX) / nb;
    const int32_t largeur = pas * 7 / 10;
    for (int k = 0; k < kSlotsMax; k++) {
        lv_obj_t* b = s_barres[k];
        const bool montre = !vide && k < s.n && !std::isnan(s.v[k]);
        ui_hidden(b, !montre);
        if (!montre) continue;
        const int32_t h = hauteur_de(s.v[k], maximum, kBarresBas - kBarresHaut);
        ui_poser(b, kBordX + k * pas + (pas - largeur) / 2, kBarresBas - h, largeur, h);
        ui_style_couleur(b, LV_STYLE_BG_COLOR, k == courante ? UIColor.ACCENT : UIColor.GOLD);
    }
    peindre_axe(s_axe, s, nb, pas);
}

// --- Page Flux ---------------------------------------------------------------------------

// Épaisseur d'un trait : kTraitMin sans flux, puis en racine de la puissance jusqu'à
// kTraitMax à kTraitPlein (100 W se voit, 6 kW reste lisible).
int32_t epaisseur(float w) {
    if (!(w >= kRepos)) return kTraitMin;
    const float r = std::sqrt(std::fmin(w / kTraitPlein, 1.0f));
    return 3 + static_cast<int32_t>(std::lround(r * (kTraitMax - 3)));
}

// Rayon occupé autour d'un centre : le cercle (et l'anneau de la maison), plus le jeu.
float rayon_occupe(int c) {
    return static_cast<float>(c == MAISON ? kFluxR + kAnneauEcart + kAnneauL + kTraitJeu : kFluxR + kTraitJeu);
}

// Pose les cercles, les traits (d'un bord à l'autre) et l'anneau pour une disposition.
void flux_disposer(bool batterie) {
    const EnergieUI& u = g_energie_ui;
    const int d = batterie ? 1 : 0;
    if (s_flux.disposition == d) return;
    s_flux.disposition = d;
    const Centre* t = batterie ? kCentresBatterie : kCentresSans;
    Centre c[NB_CARTES];
    for (int n = 0; n < NB_CARTES; n++) c[n] = centre_carte(t[n]);
    for (int n = 0; n < NB_CARTES; n++) {
        ui_x(u.flux_noeud[n], c[n].x - kFluxR);
        ui_y(u.flux_noeud[n], c[n].y - kFluxR);
    }
    for (int t = 0; t < NB_TRAITS; t++) {
        const int a = kTraitBouts[t][0], b = kTraitBouts[t][1];
        const float dx = static_cast<float>(c[b].x - c[a].x), dy = static_cast<float>(c[b].y - c[a].y);
        const float l = std::sqrt(dx * dx + dy * dy);
        const float ux = l > 0.0f ? dx / l : 0.0f, uy = l > 0.0f ? dy / l : 0.0f;
        const float ra = rayon_occupe(a), rb = rayon_occupe(b);
        s_traits_pts[t][0].x = static_cast<lv_value_precise_t>(lroundf(c[a].x + ux * ra));
        s_traits_pts[t][0].y = static_cast<lv_value_precise_t>(lroundf(c[a].y + uy * ra));
        s_traits_pts[t][1].x = static_cast<lv_value_precise_t>(lroundf(c[b].x - ux * rb));
        s_traits_pts[t][1].y = static_cast<lv_value_precise_t>(lroundf(c[b].y - uy * rb));
        lv_line_set_points(s_flux.trait[t], s_traits_pts[t], 2);
    }
    for (lv_obj_t* a : s_flux.anneau) {
        ui_x(a, c[MAISON].x - kAnneauD / 2);
        ui_y(a, c[MAISON].y - kAnneauD / 2);
    }
}

// Un cercle : bord à la couleur de sa source sur tout le tour (un thème à relief ne pose
// la bordure du verre partagé que sur deux côtés ; sans le tour entier, le bas du cercle se
// confond avec la carte), icône à la couleur de son état (comme sa carte).
void peindre_noeud(int c, const Vu& v, uint32_t bord) {
    const EnergieUI& u = g_energie_ui;
    lv_obj_t* o = u.flux_noeud[c];
    ui_hidden(o, false);
    contour(o, bord, 3, LV_OPA_COVER);
    ui_style_num(o, LV_STYLE_BORDER_SIDE, LV_BORDER_SIDE_FULL);
    ui_text(u.flux_valeur[c], v.valeur);
    ui_text_color(u.flux_valeur[c], v.couleur_valeur);
    ui_text(u.flux_icone[c], glyphe_carte(c, v.etat));
    ui_text_color(u.flux_icone[c], v.couleur);
    texte_ha_coupe(u.flux_texte[c], v.cercle, kFluxTexteL);
    texte_ha_coupe(u.flux_detail[c], v.detail, kFluxDetailL);
}

void peindre_trait(int t, bool montre, float w, uint32_t couleur) {
    lv_obj_t* o = s_flux.trait[t];
    ui_hidden(o, !montre);
    if (!montre) return;
    const bool passe = w >= kRepos;
    ui_style_num(o, LV_STYLE_LINE_WIDTH, epaisseur(w));
    ui_style_couleur(o, LV_STYLE_LINE_COLOR, passe ? couleur : UIColor.ARC_TRACK);
}

// Une part de l'anneau, de `debut` à `fin` degrés (0 en haut) ; cachée sous un degré.
// Anneau plein (0, 360) : LVGL 9.5 garde 360 tel quel (lv_arc_set_end_angle ne retranche
// 360 qu'au-delà) et lv_draw_sw_arc trace un anneau entier quand fin = début + 360 ; la
// relecture par lv_arc_get_angle_* rend donc bien le couple posé (vérifié le 10/10/2026
// dans lv_arc.c et lv_draw_sw_arc.c de LVGL 9.5.0).
void peindre_part(lv_obj_t* a, int32_t debut, int32_t fin, uint32_t couleur) {
    if (fin - debut < 1) {
        ui_hidden(a, true);
        return;
    }
    if (static_cast<int32_t>(lv_arc_get_angle_start(a)) != debut || static_cast<int32_t>(lv_arc_get_angle_end(a)) != fin)
        lv_arc_set_angles(a, debut, fin);
    arc_couleur(a, LV_PART_INDICATOR, couleur);
    ui_hidden(a, false);
}

void peindre_flux() {
    const EnergieUI& u = g_energie_ui;
    if (s_flux.trait[0] == nullptr) return;
    const Instant& i = s_i;
    const bool batterie = i.batterie.choisi || i.batterie_puissance.choisi;
    const bool reseau = i.reseau.choisi;
    flux_disposer(batterie);
    const EnergieFlux f = energie_flux_calculer(i.solaire.v, i.maison.v, i.reseau.v, i.batterie_puissance.v);

    peindre_noeud(SOLAIRE, vu_solaire(), UIColor.GOLD);
    // Maison : sa consommation (mesurée, sinon ce qui y entre) et la part du solaire.
    Vu m;
    puissance(m.valeur, sizeof(m.valeur), f.maison);
    m.couleur_valeur = UIColor.TEXT_PRIMARY;
    m.couleur = UIColor.INFO;
    const float entre = f.solaire_maison + f.batterie_maison + f.reseau_maison;
    if (entre >= kRepos)
        snprintf(m.cercle, sizeof(m.cercle), tr("%d %% solaire"),
                 static_cast<int>(std::lround(100.0f * f.solaire_maison / entre)));
    else
        snprintf(m.cercle, sizeof(m.cercle), "%s", tr("Consommation"));
    peindre_noeud(MAISON, m, UIColor.INFO);
    // Réseau : bord de l'achat, ou de la vente quand il en reçoit.
    if (reseau) {
        const Vu r = vu_reseau();
        peindre_noeud(RESEAU, r, r.etat < 0 ? UIColor.SUCCESS : UIColor.WARNING);
    } else {
        ui_hidden(u.flux_noeud[RESEAU], true);
    }
    if (batterie) {
        peindre_noeud(BATTERIE, vu_batterie(), UIColor.ACCENT_ALT);
    } else {
        ui_hidden(u.flux_noeud[BATTERIE], true);
    }

    // Traits : la couleur de la source (solaire, achat ; vente ; batterie), l'épaisseur du flux.
    peindre_trait(T_SOL_MAISON, true, f.solaire_maison, UIColor.GOLD);
    peindre_trait(T_SOL_RESEAU, reseau, f.solaire_reseau, UIColor.SUCCESS);
    peindre_trait(T_RESEAU_MAISON, reseau, f.reseau_maison, UIColor.WARNING);
    peindre_trait(T_SOL_BATT, batterie, f.solaire_batterie, UIColor.GOLD);
    peindre_trait(T_BATT_MAISON, batterie, f.batterie_maison, UIColor.ACCENT_ALT);
    const bool achat_batterie = f.reseau_batterie >= f.batterie_reseau;
    peindre_trait(T_RESEAU_BATT, reseau && batterie, achat_batterie ? f.reseau_batterie : f.batterie_reseau,
                  achat_batterie ? UIColor.WARNING : UIColor.SUCCESS);

    // Anneau de la maison : parts solaire, batterie, réseau de ce qui y entre.
    ui_hidden(s_flux.anneau[A_PISTE], false);
    arc_couleur(s_flux.anneau[A_PISTE], LV_PART_MAIN, UIColor.ARC_TRACK);
    if (entre >= kRepos) {
        const int32_t a1 = static_cast<int32_t>(std::lround(360.0f * f.solaire_maison / entre));
        const int32_t a2 = a1 + static_cast<int32_t>(std::lround(360.0f * f.batterie_maison / entre));
        peindre_part(s_flux.anneau[A_SOLAIRE], 0, a1, UIColor.GOLD);
        peindre_part(s_flux.anneau[A_BATTERIE], a1, a2 > 360 ? 360 : a2, UIColor.ACCENT_ALT);
        peindre_part(s_flux.anneau[A_RESEAU], a2 > 360 ? 360 : a2, 360, UIColor.WARNING);
    } else {
        for (int a = A_SOLAIRE; a < NB_ANNEAUX; a++) ui_hidden(s_flux.anneau[a], true);
    }
}

// --- Page Aujourd'hui --------------------------------------------------------------------

// x dans la zone de gauche d'une heure locale en minutes (0..1440).
int32_t sol_x(float minutes) { return kSolBord + static_cast<int32_t>(std::lround(minutes * kSolL / 1440.0f)); }

// Un libellé de largeur `l` centré sur x, gardé dans la zone.
void centrer(lv_obj_t* o, int32_t x, int32_t y, int32_t l) {
    int32_t g = x - l / 2;
    if (g < 0) g = 0;
    if (g > kSolZoneL - l) g = kSolZoneL - l;
    ui_poser(o, g, y, l, LV_SIZE_CONTENT);
}

void heure_texte(char* out, size_t n, const char* nom, int minutes) {
    snprintf(out, n, "%s %02d:%02d", nom, minutes / 60, minutes % 60);
}

void peindre_arc(const EnergieSoleilLu& s, int maintenant) {
    Soleil& o = s_sol;
    const bool arc = s.lever >= 0;
    ui_hidden(o.sans_soleil, arc);
    if (!arc) ui_text(o.sans_soleil, tr("Lever et coucher du soleil inconnus"));
    ui_hidden(o.horizon, !arc);
    if (arc) ui_poser(o.horizon, 0, kHorizonY, kSolZoneL, 1);
    const float duree = static_cast<float>(s.coucher - s.lever);
    const float pi = 3.14159265f;
    for (int j = 0; j < kArcPoints; j++) {
        if (!arc) {
            ui_hidden(o.point[j], true);
            continue;
        }
        const float f = static_cast<float>(j) / (kArcPoints - 1);
        const int32_t x = sol_x(s.lever + f * duree);
        const int32_t y = kHorizonY - static_cast<int32_t>(std::lround((kHorizonY - kArcHaut) * std::sin(pi * f)));
        ui_poser(o.point[j], x - kArcPoint / 2, y - kArcPoint / 2, kArcPoint, kArcPoint);
    }
    // Le soleil à sa place entre son lever et son coucher ; la nuit, rien.
    const bool jour = arc && maintenant >= s.lever && maintenant <= s.coucher;
    ui_hidden(o.astre, !jour);
    if (jour) {
        const float f = (maintenant - s.lever) / duree;
        const int32_t x = sol_x(static_cast<float>(maintenant));
        const int32_t y = kHorizonY - static_cast<int32_t>(std::lround((kHorizonY - kArcHaut) * std::sin(pi * f)));
        ui_poser(o.astre, x - kAstre / 2, y - kAstre / 2, kAstre, kAstre);
    }
    char buf[48];
    ui_hidden(o.lever, !arc);
    ui_hidden(o.coucher, !arc);
    if (arc) {
        heure_texte(buf, sizeof(buf), tr("Lever"), s.lever);
        ui_text(o.lever, buf);
        centrer(o.lever, sol_x(static_cast<float>(s.lever)), kLeverY, kHeureL);
        heure_texte(buf, sizeof(buf), tr("Coucher"), s.coucher);
        ui_text(o.coucher, buf);
        centrer(o.coucher, sol_x(static_cast<float>(s.coucher)), kLeverY, kHeureL);
    }
    ui_hidden(o.midi, s.midi < 0);
    if (s.midi >= 0) {
        heure_texte(buf, sizeof(buf), tr("Midi"), s.midi);
        ui_text(o.midi, buf);
        centrer(o.midi, sol_x(static_cast<float>(s.midi)), kMidiY, kHeureL);
    }
}

void peindre_heures_soleil(const EnergieSoleilLu& s, int maintenant) {
    Soleil& o = s_sol;
    const Serie& h = s_series[HEURES];
    float maximum = 0.0f;
    bool donnees = false;
    auto prendre = [&maximum, &donnees](float v) {
        if (!std::isfinite(v)) return;
        donnees = true;
        if (v > maximum) maximum = v;
    };
    for (int k = 0; k < h.n && k < kEnergieHeures; k++) prendre(h.v[k]);
    for (int k = 0; k < s.n_prevu; k++) prendre(s.prevu[k]);
    for (int k = 0; k < s.n_clair; k++) prendre(s.clair[k]);
    if (maximum < 0.01f) maximum = 0.01f;
    const int32_t pleine = kSolBas - kSolHaut;
    ui_hidden(o.vide, donnees);
    if (!donnees) ui_text(o.vide, tr("Aucune production ni prévision"));
    ui_hidden(o.repere, !donnees);
    ui_hidden(o.maximum, !donnees);
    char buf[48];
    if (donnees) {
        ui_poser(o.repere, kSolBord, kSolHaut, kSolL, 1);
        energie_formater(buf, sizeof(buf), maximum, "kWh");
        ui_text(o.maximum, buf);
    }
    // La courbe se dit apprise ou non (ADR-0058 : vide tant que le recorder n'a pas assez
    // de jours de soleil).
    ui_hidden(o.apprentissage, s.n_clair > 0);
    if (s.n_clair == 0) ui_text(o.apprentissage, tr("Courbe en cours d'apprentissage"));
    // Bande du meilleur créneau, sous les barres.
    const bool creneau = s.creneau_debut >= 0;
    ui_hidden(o.bande, !creneau);
    if (creneau) {
        const int32_t x0 = sol_x(s.creneau_debut * 60.0f), x1 = sol_x(s.creneau_fin * 60.0f);
        ui_poser(o.bande, x0, kSolHaut - 6, x1 - x0, pleine + 6);
    }
    const float pas = static_cast<float>(kSolL) / kEnergieHeures;
    const int32_t l = static_cast<int32_t>(pas * 0.62f);
    for (int k = 0; k < kEnergieHeures; k++) {
        const int32_t cx = sol_x(k * 60.0f + 30.0f);
        const float p = (h.recue && k < h.n) ? h.v[k] : NAN;
        ui_hidden(o.barre[k], !std::isfinite(p));
        if (std::isfinite(p)) {
            const int32_t hp = hauteur_de(p, maximum, pleine);
            ui_poser(o.barre[k], cx - l / 2, kSolBas - hp, l, hp);
        }
        const float q = k < s.n_prevu ? s.prevu[k] : NAN;
        ui_hidden(o.prevu[k], !std::isfinite(q));
        if (std::isfinite(q)) {
            const int32_t hq = hauteur_de(q, maximum, pleine);
            ui_poser(o.prevu[k], cx - l / 2, kSolBas - hq, l, hq);
        }
    }
    // Courbe du ciel clair : recalculée seulement si ses valeurs ou l'échelle ont changé.
    CourbeCle& cle = s_courbe_cle;
    const size_t octets = sizeof(float) * static_cast<size_t>(s.n_clair);
    if (cle.n != s.n_clair || cle.maximum != maximum || std::memcmp(cle.clair, s.clair, octets) != 0) {
        cle.n = s.n_clair;
        cle.maximum = maximum;
        std::memcpy(cle.clair, s.clair, octets);
        cle.points = 0;
        if (s.n_clair >= 2) {
            float xs[kEnergieHeures], ys[kEnergieHeures];
            for (int k = 0; k < s.n_clair; k++) {
                xs[k] = static_cast<float>(sol_x(k * 60.0f + 30.0f));
                const float c = std::isfinite(s.clair[k]) ? s.clair[k] : 0.0f;
                ys[k] = kSolBas - c / maximum * pleine;
            }
            cle.points = ui_courbe_lisse(xs, ys, s.n_clair, kLisse, s_courbe_pts);
            if (cle.points >= 2) lv_line_set_points(o.courbe, s_courbe_pts, static_cast<uint32_t>(cle.points));
        }
    }
    ui_hidden(o.courbe, cle.points < 2);
    // L'heure qu'il est : un trait sur les heures.
    ui_hidden(o.maintenant, maintenant < 0);
    if (maintenant >= 0) ui_poser(o.maintenant, sol_x(static_cast<float>(maintenant)) - 1, kSolHaut, 2, pleine);
    for (int a = 0; a < kSolAxe; a++) {
        snprintf(buf, sizeof(buf), "%02d:00", a * 3);
        ui_text(o.axe[a], buf);
        ui_poser(o.axe[a], sol_x(a * 180.0f + 30.0f) - kSolAxeL / 2, kSolAxeY, kSolAxeL, LV_SIZE_CONTENT);
    }
}

void peindre_infos_soleil(const EnergieSoleilLu& s) {
    Soleil& o = s_sol;
    const Serie& h = s_series[HEURES];
    // Produit : le compteur du jour, sinon la somme des heures reçues.
    float produit = s_i.jour.choisi ? s_i.jour.v : NAN;
    if (!std::isfinite(produit) && h.recue)
        for (int k = 0; k < h.n; k++)
            if (std::isfinite(h.v[k])) produit = std::isfinite(produit) ? produit + h.v[k] : h.v[k];
    const float valeurs[3] = {produit, s.prevu_jour, s.prevu_demain};
    // Prévision non choisie (champ vide) : sa ligne disparaît et les suivantes remontent ;
    // choisie sans valeur (« nan ») : « -- ».
    const bool montre[3] = {true, s.prevu_jour_choisi, s.prevu_demain_choisi};
    char buf[48];
    int rang = 0;
    for (int r = 0; r < 3; r++) {
        ui_hidden(o.titre[r], !montre[r]);
        ui_hidden(o.valeur[r], !montre[r]);
        if (!montre[r]) continue;
        ui_y(o.titre[r], rang * kInfosLigne);
        ui_y(o.valeur[r], rang * kInfosLigne + 30);
        kwh(buf, sizeof(buf), valeurs[r]);
        ui_text(o.valeur[r], buf);
        rang++;
    }
    if (s.creneau_debut >= 0) {
        snprintf(buf, sizeof(buf), tr("%d h – %d h"), s.creneau_debut, s.creneau_fin);
        ui_text(o.cadre_valeur, buf);
        ui_text_color(o.cadre_valeur, UIColor.GOLD);
    } else {
        ui_text(o.cadre_valeur, tr("Aucun"));
        ui_text_color(o.cadre_valeur, UIColor.TEXT_DIM);
    }
    // La source, traduite : jamais le code de HA.
    const char* source = s.source == EnergieSource::APPRISE   ? tr("Prévision : météo")
                         : s.source == EnergieSource::EXTERNE ? tr("Prévision : externe")
                                                              : "";
    texte_ha_coupe(o.source, source, kInfosL);
}

void peindre_soleil() {
    if (s_sol.courbe == nullptr) return;
    const int maintenant = minutes_maintenant();
    peindre_arc(s_soleil, maintenant);
    peindre_heures_soleil(s_soleil, maintenant);
    peindre_infos_soleil(s_soleil);
}

// --- Page Bilan (ADR-0062) ----------------------------------------------------------------

// Largeur d'un texte dans la police de son label (l'unité posée après le grand nombre, les
// libellés de l'axe qui se chevaucheraient).
int32_t largeur_texte(lv_obj_t* l, const char* t) {
    if (l == nullptr || t == nullptr || t[0] == '\0') return 0;
    lv_point_t sz;
    lv_text_get_size(&sz, t, lv_obj_get_style_text_font(l, LV_PART_MAIN), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return sz.x;
}

// Production du créneau k d'un bilan : son champ prod (HA de l'ADR-0062), sinon la série de
// l'historique de la même vue (HA d'avant : les mêmes fenêtres glissantes). Jamais pour une
// vue civile des jours ou des mois : ses créneaux ne sont pas ceux de l'historique.
float bilan_produit(const Bilan& b, int v, int k) {
    if (b.lu.prod_choisie) return k < b.lu.n_prod ? b.lu.prod[k] : NAN;
    if (b.calendaire && v != HEURES) return NAN;
    const Serie& s = s_series[v];
    return (s.recue && k < s.n) ? s.v[k] : NAN;
}

// Créneaux d'une vue du bilan ; renvoie leur nombre (0 : pas encore reçu).
int bilan_creneaux(int v, EnergieBilanCreneau* c) {
    const Bilan& b = s_bilans[v];
    if (!b.recu) return 0;
    for (int k = 0; k < b.n && k < kSlotsMax; k++) c[k] = energie_bilan_creneau(bilan_produit(b, v, k), b.lu, k);
    return b.n < kSlotsMax ? b.n : kSlotsMax;
}

// Un nombre de kWh signé : « +23.4 », « -3.0 » (une décimale sous 100, aucune au-delà, ou
// `decimales`) ; « 0.0 » sans signe ; « -- » s'il est inconnu.
void signe_kwh(char* out, size_t n, float v, int decimales = -1) {
    if (!std::isfinite(v)) {
        snprintf(out, n, "--");
        return;
    }
    const int d = decimales >= 0 ? decimales : (std::fabs(v) < 100.0f ? 1 : 0);
    if (std::fabs(v) < (d == 0 ? 0.5f : 0.05f)) {
        snprintf(out, n, "%s", d == 0 ? "0" : "0.0");
        return;
    }
    snprintf(out, n, "%+.*f", d, static_cast<double>(v));
}

// Une graduation : « +4 », « -2.5 », « +0.25 », « 0 ».
void graduation(char* out, size_t n, float v) {
    const double x = v;
    if (std::fabs(x) < 1e-6) snprintf(out, n, "0");
    else if (std::fabs(x - std::round(x)) < 1e-3) snprintf(out, n, "%+.0f", x);
    else if (std::fabs(x * 10.0 - std::round(x * 10.0)) < 1e-3) snprintf(out, n, "%+.1f", x);
    else snprintf(out, n, "%+.2f", x);
}

// y d'une valeur dans le graphique : de kBilY0 (`haut`) au zéro yz, puis de yz à kBilY1
// (`bas`, en valeur absolue) ; bornée au cadre.
int32_t bilan_y(float v, float haut, float bas, int32_t yz) {
    if (v >= 0.0f) return yz - static_cast<int32_t>(std::lround(std::fmin(v / haut, 1.0f) * (yz - kBilY0)));
    return yz + static_cast<int32_t>(std::lround(std::fmin(-v / bas, 1.0f) * (kBilY1 - yz)));
}

// Un segment de la jauge depuis le zéro, vers la droite (positif) ou la gauche, à l'échelle
// `e` ; caché sous 2 px.
void jauge_segment(lv_obj_t* r, float v, float e) {
    int32_t l = static_cast<int32_t>(std::lround(std::fabs(v) / e * kJaugeDemi));
    if (l > kJaugeDemi) l = kJaugeDemi;
    if (l < 2) {
        ui_hidden(r, true);
        return;
    }
    ui_poser(r, v >= 0.0f ? kJaugeZeroX : kJaugeZeroX - l, kJaugeY + kJaugeDedans, l, kJaugeH - 2 * kJaugeDedans);
}

// Carte de gauche : le mois civil du dernier jour connu, lu dans le bilan des jours quelle
// que soit la vue (HA le pousse à l'ouverture, ADR-0062).
void peindre_heros() {
    BilanUI& o = s_bil;
    const Bilan& b = s_bilans[JOURS];
    EnergieBilanCreneau c[kSlotsMax];
    const int n = bilan_creneaux(JOURS, c);
    const EnergieMois m = energie_mois(c, n, b.annee, b.mois, b.jour);
    const bool ok = m.ok && std::isfinite(m.net);
    char buf[96], x[32];
    if (ok) snprintf(buf, sizeof(buf), "%s \xC2\xB7 %s", tr("BATTERIE RÉSEAU"), month_long_utf8(m.mois));
    else snprintf(buf, sizeof(buf), "%s", tr("BATTERIE RÉSEAU"));
    texte_majuscules(buf);
    texte_ha_coupe(o.titre, buf, kHerosW);

    // Le net du mois : en vert d'avance, en couleur d'avertissement de retard.
    const uint32_t teinte = ok && m.net < 0.0f ? UIColor.WARNING : UIColor.SUCCESS;
    signe_kwh(x, sizeof(x), ok ? m.net : NAN);
    ui_text(o.grand, x);
    ui_text_color(o.grand, ok ? teinte : UIColor.TEXT_DIM);
    ui_hidden(o.grand_unite, !ok);
    if (ok) {
        ui_text_color(o.grand_unite, teinte);
        ui_x(o.grand_unite, kHerosX + largeur_texte(o.grand, x) + 10);
    }
    const char* sous = !b.recu        ? tr("En attente de Home Assistant")
                       : !ok          ? tr("Aucun historique")
                       : m.net < 0.0f ? tr("de retard sur le réseau ce mois-ci")
                                      : tr("d'avance sur le réseau ce mois-ci");
    texte_ha_coupe(o.sous, sous, kHerosW);

    // Jauge centrée sur 0 : le net, et la fin du mois projetée derrière, estompée ; échelle
    // symétrique arrondie qui les contient tous deux.
    const bool fin = ok && std::isfinite(m.fin);
    for (lv_obj_t* j : {o.jauge, o.jauge_bout, o.jauge_zero, o.echelle[0], o.echelle[1], o.echelle[2]})
        ui_hidden(j, !ok);
    if (ok) {
        const float e = energie_rond(std::fmax(std::fabs(m.net), fin ? std::fabs(m.fin) : 0.0f));
        jauge_segment(o.jauge_plein, m.net, e);
        ui_style_couleur(o.jauge_plein, LV_STYLE_BG_COLOR, teinte);
        if (fin) {
            const uint32_t t_fin = m.fin < 0.0f ? UIColor.WARNING : UIColor.SUCCESS;
            jauge_segment(o.jauge_proj, m.fin, e);
            ui_style_couleur(o.jauge_proj, LV_STYLE_BG_COLOR, t_fin);
            contour(o.jauge_proj, t_fin, 2, LV_OPA_70);
        } else {
            ui_hidden(o.jauge_proj, true);
        }
        graduation(x, sizeof(x), -e);
        ui_text(o.echelle[0], x);
        ui_text(o.echelle[1], "0");
        graduation(x, sizeof(x), e);
        snprintf(buf, sizeof(buf), "%s kWh", x);
        ui_text(o.echelle[2], buf);
    } else {
        ui_hidden(o.jauge_plein, true);
        ui_hidden(o.jauge_proj, true);
    }

    // Fin du mois au rythme des derniers jours (leur nombre réel : moins de 7 en début de mois).
    ui_hidden(o.fin_titre, !fin);
    ui_hidden(o.fin_valeur, !fin);
    if (fin) {
        signe_kwh(x, sizeof(x), m.fin, std::fabs(m.fin) < 10.0f ? 1 : 0);
        snprintf(buf, sizeof(buf), "~ %s", x);
        ui_text(o.fin_valeur, buf);
        ui_text_color(o.fin_valeur, m.fin < 0.0f ? UIColor.WARNING : UIColor.SUCCESS);
        const int32_t lv = largeur_texte(o.fin_valeur, buf);
        ui_x(o.fin_valeur, kHerosX + kHerosW - lv);
        snprintf(buf, sizeof(buf), tr("Fin du mois (rythme %d j)"), m.n_rythme);
        texte_ha_coupe(o.fin_titre, buf, kHerosW - lv - kFinEcart);
    }

    // Produit, consommé ; économies seulement avec un prix (l'argent n'apparaît que là).
    const bool prix = ok && b.devise[0] != '\0' && std::isfinite(m.gain);
    const int lignes = prix ? 3 : 2;
    ui_hidden(o.sep[0], !ok);
    for (int i = 0; i < kHerosLignes; i++) {
        const bool montre = ok && i < lignes;
        ui_hidden(o.ligne_titre[i], !montre);
        ui_hidden(o.ligne_valeur[i], !montre);
    }
    if (ok) {
        kwh(x, sizeof(x), m.produit);
        ui_text(o.ligne_valeur[0], x);
        kwh(x, sizeof(x), m.consomme);
        ui_text(o.ligne_valeur[1], x);
        if (prix) {
            snprintf(buf, sizeof(buf), "%.2f %s", static_cast<double>(m.gain), b.devise);
            ui_text(o.ligne_valeur[2], buf);
            ui_text_color(o.ligne_valeur[2], m.gain < 0.0f ? UIColor.WARNING : UIColor.SUCCESS);
        }
    }

    // Anneau : la part de la consommation couverte par le soleil (autoconsommé / consommé).
    const bool anneau = ok && std::isfinite(m.couverture);
    const int32_t y_sep = kLigneY + lignes * kLignePas + 6;
    ui_hidden(o.sep[1], !anneau);
    ui_hidden(o.anneau_piste, !anneau);
    ui_hidden(o.anneau_pct, !anneau);
    ui_hidden(o.anneau_texte, !anneau);
    if (!anneau) {
        ui_hidden(o.anneau, true);
        return;
    }
    const int32_t ya = y_sep + 14;
    ui_poser(o.sep[1], kHerosX, y_sep, kHerosW, 1);
    ui_y(o.anneau_piste, ya);
    ui_y(o.anneau, ya);
    arc_couleur(o.anneau_piste, LV_PART_MAIN, UIColor.ARC_TRACK);
    peindre_part(o.anneau, 0, static_cast<int32_t>(std::lround(360.0f * m.couverture)), UIColor.GOLD);
    snprintf(x, sizeof(x), "%d %%", static_cast<int>(std::lround(100.0f * m.couverture)));
    ui_text(o.anneau_pct, x);
    ui_y(o.anneau_pct, ya + kAnneauBilanD / 2 - 13);
    ui_text(o.anneau_texte, tr("de ta conso couverte par le soleil"));
    ui_y(o.anneau_texte, ya + kAnneauBilanD / 2 - 26);
}

// Le titre du graphique : la période seule (« Octobre », « Aujourd'hui », « 2026 ») ; une
// fenêtre glissante d'un HA d'avant garde le nom de Production.
void titre_bilan(const Bilan& b, char* out, size_t n) {
    if (!b.recu) snprintf(out, n, "%s", "");
    else if (s_vue == HEURES || !b.calendaire) snprintf(out, n, "%s", tr(kPeriodes[s_vue]));
    else if (s_vue == JOURS) snprintf(out, n, "%s", fr_capitalized(month_long_utf8(b.mois)).c_str());
    else snprintf(out, n, "%d", b.annee);
}

// Libellé de l'axe sous le créneau k ; `regulier` : montré même loin du dernier créneau
// connu (heures 00:00, 03:00… ; jours 1, 6, 11… et le dernier du mois, ou tous les cinq
// en finissant par le dernier dans une fenêtre glissante ; tous les mois).
void axe_bilan(const Bilan& b, int n, int k, char* out, size_t taille, bool& regulier) {
    // Pas d'un créneau : sur un graphique étroit (7B, 18/10/2026), une heure sur six et un
    // mois sur deux, sinon les libellés (roboto_22, qui ne rétrécit pas) se chevauchaient.
    const float pas = n > 0 ? static_cast<float>(kBilX1 - kBilX0) / static_cast<float>(n) : 0.0f;
    switch (s_vue) {
        case HEURES:
            snprintf(out, taille, "%02d:00", k);
            regulier = k % (pas * 3.0f >= 72.0f ? 3 : 6) == 0;
            return;
        case JOURS: {
            int a, m, j;
            energie_jour_plus(b.annee, b.mois, b.jour, k, a, m, j);
            snprintf(out, taille, "%d", j);
            regulier = b.calendaire ? (k % 5 == 0 || k == n - 1) : ((n - 1 - k) % 5 == 0);
            return;
        }
        default:
            snprintf(out, taille, "%s", month_short_utf8(((b.mois - 1 + k) % 12 + 12) % 12 + 1));
            regulier = pas >= 52.0f || k % 2 == 0;
            return;
    }
}

void masquer_graphique_bilan() {
    BilanUI& o = s_bil;
    for (int g = 0; g < kGrille; g++)
        for (lv_obj_t* x : {o.grille[g], o.grad_net[g], o.grad_cumul[g]}) ui_hidden(x, true);
    for (int k = 0; k < kSlotsMax; k++) {
        ui_hidden(o.barre[k], true);
        ui_hidden(o.proj[k], true);
    }
    for (lv_obj_t* x : o.axe) ui_hidden(x, true);
    for (int i = 0; i < kBilanLegendes; i++) {
        ui_hidden(o.marque[i], true);
        ui_hidden(o.legende[i], true);
    }
    for (lv_obj_t* x : {o.zero, o.courbe, o.point, o.bulle, o.fin, o.note, o.unite, o.cumul_titre}) ui_hidden(x, true);
}

// Graphique de la vue : le net de chaque créneau en barres (surplus en haut, déficit en
// bas, l'axe le plus court au quart de l'autre au moins), le cumul en courbe lissée sur son
// axe de droite (même zéro), le dernier créneau connu pointé avec sa bulle, et dans la vue
// des jours d'un mois civil la fin du mois projetée en points au rythme de la carte de
// gauche.
void peindre_graphique_bilan() {
    const EnergieUI& u = g_energie_ui;
    BilanUI& o = s_bil;
    const Bilan& b = s_bilans[s_vue];
    char buf[96], x[32];
    titre_bilan(b, buf, sizeof(buf));
    texte_ha_coupe(u.bilan_titre, buf, kBilTitreL);

    EnergieBilanCreneau c[kSlotsMax];
    const int n = bilan_creneaux(s_vue, c);
    float net[kSlotsMax], cumul[kSlotsMax];
    for (int k = 0; k < n; k++) net[k] = c[k].net;
    const int dernier = n > 0 ? energie_cumul(net, n, static_cast<EnergieVue>(s_vue), b.annee, b.mois, b.jour, cumul) : -1;
    ui_hidden(o.vide, dernier >= 0);
    if (dernier < 0) {
        ui_text(o.vide, b.recu ? tr("Aucun historique") : tr("En attente de Home Assistant"));
        masquer_graphique_bilan();
        return;
    }
    float rythme = NAN;
    if (s_vue == JOURS && b.calendaire && dernier < n - 1) rythme = energie_mois(c, n, b.annee, b.mois, b.jour).rythme;
    const float fin = std::isfinite(rythme) ? cumul[dernier] + rythme * static_cast<float>(n - 1 - dernier) : NAN;
    const EnergieEchelle e = energie_echelle(net, cumul, n, fin);
    const int32_t yz = kBilY0 + static_cast<int32_t>(std::lround((kBilY1 - kBilY0) * e.haut / (e.haut + e.bas)));

    static const char* const kUnites[NB_VUES] = {tr_noop("kWh / heure"), tr_noop("kWh / jour"), tr_noop("kWh / mois")};
    texte_ha_coupe(o.unite, tr(kUnites[s_vue]), 300);
    ui_hidden(o.unite, false);
    ui_hidden(o.cumul_titre, false);

    // Graduations du net (traits et libellés à gauche), puis du cumul (à droite).
    int g = 0;
    for (int s = 1; s >= -1; s -= 2) {
        const float borne = (s > 0 ? e.haut : e.bas) * 1.0001f;
        for (int i = 1; g < kGrille && i * e.pas <= borne; i++, g++) {
            const float v = s * i * e.pas;
            const int32_t y = bilan_y(v, e.haut, e.bas, yz);
            ui_poser(o.grille[g], kBilX0, y, kBilX1 - kBilX0, 1);
            graduation(x, sizeof(x), v);
            ui_text(o.grad_net[g], x);
            ui_poser(o.grad_net[g], 0, y - 13, kBilX0 - 10, LV_SIZE_CONTENT);
        }
    }
    for (; g < kGrille; g++) {
        ui_hidden(o.grille[g], true);
        ui_hidden(o.grad_net[g], true);
    }
    const float cpas = std::fmax(e.cumul_pas, energie_pas(std::fmax(e.cumul_haut, e.cumul_bas) / 4.0f));
    g = 0;
    for (int s = 1; s >= -1; s -= 2) {
        const float borne = (s > 0 ? e.cumul_haut : e.cumul_bas) * 1.0001f;
        for (int i = 1; g < kGrille && i * cpas <= borne; i++, g++) {
            const float v = s * i * cpas;
            graduation(x, sizeof(x), v);
            ui_text(o.grad_cumul[g], x);
            ui_poser(o.grad_cumul[g], kBilX1 + 8, bilan_y(v, e.cumul_haut, e.cumul_bas, yz) - 13, kBilZoneL - kBilX1 - 8,
                     LV_SIZE_CONTENT);
        }
    }
    for (; g < kGrille; g++) ui_hidden(o.grad_cumul[g], true);
    ui_poser(o.zero, kBilX0, yz - 1, kBilX1 - kBilX0, 2);

    // Barres du net.
    const float pas = static_cast<float>(kBilX1 - kBilX0) / static_cast<float>(n);
    auto centre = [pas](int k) { return kBilX0 + static_cast<int32_t>(std::lround((k + 0.5f) * pas)); };
    int32_t l = static_cast<int32_t>(pas * 0.55f);
    if (l < 4) l = 4;
    for (int k = 0; k < kSlotsMax; k++) {
        lv_obj_t* r = o.barre[k];
        const bool montre = k < n && std::isfinite(net[k]);
        ui_hidden(r, !montre);
        if (!montre) continue;
        const int32_t y = bilan_y(net[k], e.haut, e.bas, yz);
        int32_t h = y < yz ? yz - y : y - yz;
        if (h < 2) h = 2;
        ui_poser(r, centre(k) - l / 2, net[k] >= 0.0f ? yz - h : yz, l, h);
        ui_style_couleur(r, LV_STYLE_BG_COLOR, net[k] >= 0.0f ? UIColor.SUCCESS : UIColor.WARNING);
    }

    // Cumul : depuis 0 au début d'une période civile, jusqu'au dernier créneau connu.
    float xs[kCourbeLissePoints], ys[kCourbeLissePoints];
    int np = 0;
    if (b.calendaire) {
        xs[np] = static_cast<float>(kBilX0);
        ys[np++] = static_cast<float>(yz);
    }
    for (int k = 0; k <= dernier && np < kCourbeLissePoints; k++) {
        xs[np] = static_cast<float>(centre(k));
        ys[np++] = static_cast<float>(bilan_y(cumul[k], e.cumul_haut, e.cumul_bas, yz));
    }
    lv_point_precise_t pts[kCumulPoints];
    const int npts = ui_courbe_lisse(xs, ys, np, kLisse, pts);
    if (npts != s_cumul_n || std::memcmp(pts, s_cumul_pts, sizeof(lv_point_precise_t) * static_cast<size_t>(npts)) != 0) {
        std::memcpy(s_cumul_pts, pts, sizeof(lv_point_precise_t) * static_cast<size_t>(npts));
        s_cumul_n = npts;
        if (npts >= 2) lv_line_set_points(o.courbe, s_cumul_pts, static_cast<uint32_t>(npts));
    }
    ui_hidden(o.courbe, s_cumul_n < 2);

    // Le dernier créneau connu : son point sur la courbe, sa bulle au-dessus (en dessous
    // contre le haut du cadre), gardée dans la zone.
    const int32_t px = centre(dernier), py = bilan_y(cumul[dernier], e.cumul_haut, e.cumul_bas, yz);
    ui_poser(o.point, px - kPointD / 2, py - kPointD / 2, kPointD, kPointD);
    signe_kwh(x, sizeof(x), cumul[dernier]);
    snprintf(buf, sizeof(buf), "%s kWh", x);
    ui_text(o.bulle_texte, buf);
    ui_hidden(o.bulle_texte, false);
    int32_t bx = px - kBulleL / 2;
    if (bx < 0) bx = 0;
    if (bx > kBilZoneL - kBulleL) bx = kBilZoneL - kBulleL;
    int32_t by = py - kPointD / 2 - 10 - kBulleH;
    if (by < 0) by = py + kPointD / 2 + 10;
    ui_poser(o.bulle, bx, by, kBulleL, kBulleH);

    // Projection : du lendemain du dernier jour connu à la fin du mois.
    for (int k = 0; k < kSlotsMax; k++) {
        const bool montre = std::isfinite(fin) && k > dernier && k < n;
        ui_hidden(o.proj[k], !montre);
        if (!montre) continue;
        const float v = cumul[dernier] + rythme * static_cast<float>(k - dernier);
        ui_poser(o.proj[k], centre(k) - kProjD / 2, bilan_y(v, e.cumul_haut, e.cumul_bas, yz) - kProjD / 2, kProjD, kProjD);
    }
    ui_hidden(o.fin, !std::isfinite(fin));
    if (std::isfinite(fin)) {
        signe_kwh(x, sizeof(x), fin, std::fabs(fin) < 10.0f ? 1 : 0);
        char approx[40];
        snprintf(approx, sizeof(approx), "~ %s", x);
        snprintf(buf, sizeof(buf), tr("%s fin du mois"), approx);
        ui_text(o.fin, buf);
        const int32_t yf = bilan_y(fin, e.cumul_haut, e.cumul_bas, yz);
        ui_poser(o.fin, kBilX1 - kFinL, yf - 40 >= 0 ? yf - 40 : yf + 12, kFinL, LV_SIZE_CONTENT);
    }

    // Axe : les libellés réguliers, et le dernier créneau connu en couleur d'accent ; un
    // libellé régulier qui le chevaucherait s'efface.
    char courant[24];
    bool regulier = false;
    axe_bilan(b, n, dernier, courant, sizeof(courant), regulier);
    const int32_t l_courant = largeur_texte(o.axe[0], courant);
    int a = 0;
    for (int k = 0; k < n && a < kAxeMax; k++) {
        axe_bilan(b, n, k, x, sizeof(x), regulier);
        const bool est_courant = k == dernier;
        if (!est_courant) {
            if (!regulier) continue;
            const int32_t ecart = std::abs(centre(k) - px);
            if (ecart < (largeur_texte(o.axe[0], x) + l_courant) / 2 + 8) continue;
        }
        lv_obj_t* lbl = o.axe[a++];
        ui_text(lbl, x);
        ui_text_color(lbl, est_courant ? UIColor.ACCENT : UIColor.TEXT_DIM);
        ui_poser(lbl, centre(k) - kBilAxeL / 2, kBilAxeY, kBilAxeL, LV_SIZE_CONTENT);
    }
    for (; a < kAxeMax; a++) ui_hidden(o.axe[a], true);

    // Légende (les noms de la vue), puis la note de l'estimation.
    static const char* const kLegendes[NB_VUES][kBilanLegendes] = {
        {tr_noop("surplus de l'heure"), tr_noop("déficit de l'heure"), tr_noop("cumul du jour")},
        {tr_noop("surplus du jour"), tr_noop("déficit du jour"), tr_noop("cumul du mois")},
        {tr_noop("surplus du mois"), tr_noop("déficit du mois"), tr_noop("cumul de l'année")}};
    for (int i = 0; i < kBilanLegendes; i++) {
        ui_hidden(o.marque[i], false);
        texte_ha_coupe(o.legende[i], tr(kLegendes[s_vue][i]), kBilLegendePas - kLegendeMarque - 16);
        ui_hidden(o.legende[i], false);
    }
    ui_hidden(o.note, !b.lu.estime);
    if (b.lu.estime) texte_ha_coupe(o.note, tr("achat / vente estimés heure par heure"), kBilNoteL);
}

void peindre_bilan() {
    const EnergieUI& u = g_energie_ui;
    for (int v = 0; v < NB_VUES; v++) highlight_button_border(u.bilan_vue_btn[v], v == s_vue, UIColor.ACCENT);
    if (s_bil.courbe == nullptr) return;
    peindre_heros();
    peindre_graphique_bilan();
}

// --- Ensemble -----------------------------------------------------------------------------

void peindre() {
    if (!visible()) return;
    const EnergieUI& u = g_energie_ui;
    // Ouvert sur la première page qui a des données, tant que le doigt n'en a pas choisi
    // une ; une page qui perd ses données rend la main à la première.
    if (!s_page_choisie || !page_a_donnees(s_page)) s_page = premiere_page();
    int liste[NB_PAGES];
    const int n = pages_montrees(liste);
    const char* noms[NB_PAGES] = {};
    int rang = 0;
    for (int i = 0; i < n; i++) {
        noms[i] = tr(kNomsPages[liste[i]]);
        if (liste[i] == s_page) rang = i;
    }
    pages_onglets(u.onglet, kEnergiePages, noms, n, rang);
    for (int p = 0; p < NB_PAGES; p++) ui_hidden(u.page[p], p != s_page);
    switch (s_page) {
        case P_FLUX:
            peindre_flux();
            break;
        case P_SOLEIL:
            peindre_soleil();
            break;
        case P_BILAN:
            peindre_bilan();
            break;
        default:
            peindre_instant();
            peindre_graphique();
            break;
    }
}

void afficher_page(int rang) {
    int liste[NB_PAGES];
    const int n = pages_montrees(liste);
    if (rang < 0 || rang >= n) rang = 0;
    s_page = liste[rang];
    s_page_choisie = true;
    peindre();
    ui_mark_activity();
}

// Geste gauche / droite du popup (tab5_pages.cpp, ADR-0046) ; les onglets suivent.
PagesPopup s_pages{nullptr, nombre_pages, page_courante, afficher_page};

// Couleurs de ce qui est créé ici et ne change qu'avec le thème (le reste suit les valeurs
// à chaque peinture).
void couleurs() {
    ui_style_couleur(s_repere, LV_STYLE_BG_COLOR, UIColor.GLASS_RIM);
    ui_text_color(s_maximum, UIColor.TEXT_DIM);
    ui_text_color(s_vide, UIColor.TEXT_DIM);
    for (lv_obj_t* l : s_axe) ui_text_color(l, UIColor.TEXT_DIM);
    // Aujourd'hui.
    Soleil& o = s_sol;
    ui_style_couleur(o.bande, LV_STYLE_BG_COLOR, UIColor.GOLD);
    ui_style_couleur(o.repere, LV_STYLE_BG_COLOR, UIColor.GLASS_RIM);
    for (lv_obj_t* b : o.barre) ui_style_couleur(b, LV_STYLE_BG_COLOR, UIColor.GOLD);
    for (lv_obj_t* b : o.prevu) {
        ui_style_couleur(b, LV_STYLE_BG_COLOR, UIColor.GOLD);
        contour(b, UIColor.GOLD, 2, LV_OPA_80);
    }
    ui_style_couleur(o.courbe, LV_STYLE_LINE_COLOR, UIColor.TEXT_DIM);
    ui_style_couleur(o.maintenant, LV_STYLE_BG_COLOR, UIColor.ACCENT);
    for (lv_obj_t* p : o.point) ui_style_couleur(p, LV_STYLE_BG_COLOR, UIColor.TEXT_DIM);
    ui_style_couleur(o.horizon, LV_STYLE_BG_COLOR, UIColor.GLASS_RIM);
    ui_style_couleur(o.astre, LV_STYLE_BG_COLOR, UIColor.GOLD);
    for (lv_obj_t* l : {o.lever, o.midi, o.coucher, o.sans_soleil, o.maximum, o.apprentissage, o.vide, o.source,
                        o.cadre_titre})
        ui_text_color(l, UIColor.TEXT_DIM);
    for (lv_obj_t* l : o.axe) ui_text_color(l, UIColor.TEXT_DIM);
    for (lv_obj_t* l : o.legende) ui_text_color(l, UIColor.TEXT_SOFT);
    for (lv_obj_t* l : o.titre) ui_text_color(l, UIColor.TEXT_DIM);
    for (lv_obj_t* l : o.valeur) ui_text_color(l, UIColor.TEXT_PRIMARY);
    ui_style_couleur(o.marque[0], LV_STYLE_BG_COLOR, UIColor.GOLD);
    ui_style_couleur(o.marque[1], LV_STYLE_BG_COLOR, UIColor.GOLD);
    contour(o.marque[1], UIColor.GOLD, 2, LV_OPA_80);
    ui_style_couleur(o.marque[2], LV_STYLE_BG_COLOR, UIColor.TEXT_DIM);
    ui_style_couleur(o.marque[3], LV_STYLE_BG_COLOR, UIColor.GOLD);
    ui_style_couleur(o.cadre, LV_STYLE_BG_COLOR, UIColor.GOLD);
    contour(o.cadre, UIColor.GOLD, 2, LV_OPA_COVER);
    // Bilan : carte de gauche (le net, la jauge et les valeurs suivent leur signe à chaque
    // peinture), puis le graphique (barres : leur signe ; libellés de l'axe : le courant).
    BilanUI& b = s_bil;
    ui_text_color(b.titre, UIColor.ACCENT);
    ui_text_color(b.sous, UIColor.TEXT_PRIMARY);
    ui_style_couleur(b.jauge, LV_STYLE_BG_COLOR, UIColor.BG);
    contour(b.jauge, UIColor.TEXT_DIM, kJaugeBord, LV_OPA_COVER);
    ui_style_couleur(b.jauge_bout, LV_STYLE_BG_COLOR, UIColor.TEXT_DIM);
    ui_style_couleur(b.jauge_zero, LV_STYLE_BG_COLOR, UIColor.TEXT_DIM);
    for (lv_obj_t* l : b.echelle) ui_text_color(l, UIColor.TEXT_DIM);
    ui_text_color(b.fin_titre, UIColor.TEXT_SOFT);
    for (lv_obj_t* s : b.sep) ui_style_couleur(s, LV_STYLE_BG_COLOR, UIColor.GLASS_RIM);
    for (lv_obj_t* l : b.ligne_titre) ui_text_color(l, UIColor.TEXT_SOFT);
    ui_text_color(b.ligne_valeur[0], UIColor.GOLD);
    ui_text_color(b.ligne_valeur[1], UIColor.TEXT_PRIMARY);
    ui_text_color(b.anneau_pct, UIColor.GOLD);
    ui_text_color(b.anneau_texte, UIColor.TEXT_PRIMARY);
    for (lv_obj_t* l : {b.unite, b.vide, b.note}) ui_text_color(l, UIColor.TEXT_DIM);
    ui_text_color(b.cumul_titre, UIColor.ACCENT);
    for (lv_obj_t* l : b.grad_net) ui_text_color(l, UIColor.TEXT_DIM);
    for (lv_obj_t* l : b.grad_cumul) ui_text_color(l, UIColor.ACCENT);
    for (lv_obj_t* g : b.grille) ui_style_couleur(g, LV_STYLE_BG_COLOR, UIColor.GLASS_RIM);
    ui_style_couleur(b.zero, LV_STYLE_BG_COLOR, UIColor.TEXT_DIM);
    ui_style_couleur(b.courbe, LV_STYLE_LINE_COLOR, UIColor.ACCENT);
    for (lv_obj_t* p : b.proj) ui_style_couleur(p, LV_STYLE_BG_COLOR, UIColor.ACCENT);
    ui_style_couleur(b.point, LV_STYLE_BG_COLOR, UIColor.BG);
    contour(b.point, UIColor.ACCENT, 4, LV_OPA_COVER);
    ui_style_couleur(b.bulle, LV_STYLE_BG_COLOR, UIColor.ACCENT);
    ui_text_color(b.bulle_texte, UIColor.TEXT_ON_ACCENT);
    ui_text_color(b.fin, UIColor.ACCENT);
    const uint32_t legendes[kBilanLegendes] = {UIColor.SUCCESS, UIColor.WARNING, UIColor.ACCENT};
    for (int i = 0; i < kBilanLegendes; i++) {
        ui_style_couleur(b.marque[i], LV_STYLE_BG_COLOR, legendes[i]);
        ui_text_color(b.legende[i], UIColor.TEXT_SOFT);
    }
}

// Production : noms des cartes, barres, repère et libellés de l'axe.
void construire_production() {
    EnergieUI& u = g_energie_ui;
    // « RÉSEAU » est aussi le réseau Wi-Fi de la console système (« NETWORK ») : contexte.
    ui_text(u.nom[SOLAIRE], tr("SOLAIRE"));
    ui_text(u.nom[MAISON], tr("MAISON"));
    ui_text(u.nom[RESEAU], tr_ctx("energie", "RÉSEAU"));
    ui_text(u.nom[BATTERIE], tr("BATTERIE"));
    s_repere = ui_rectangle(u.zone, LV_OPA_40, 0);
    ui_poser(s_repere, kBordX, kBarresHaut, kGraphiqueL - 2 * kBordX, 1);
    ui_hidden(s_repere, true);
    s_maximum = libelle(u.zone, u.police, 0);
    lv_obj_set_pos(s_maximum, kBordX, 0);
    s_vide = libelle(u.zone, u.police, 0);
    lv_obj_align(s_vide, LV_ALIGN_CENTER, 0, 0);
    for (int k = 0; k < kSlotsMax; k++) s_barres[k] = ui_rectangle(u.zone, LV_OPA_COVER, 4);
    // Libellés de l'axe, texte centré dans kAxeLibelleL : un tous les trois pas en heures (138 px),
    // cinq en jours (187 px) ; en mois un pas fait 93 px et le nom court (« Janv ») tient.
    for (int k = 0; k < kAxeMax; k++) {
        s_axe[k] = libelle(u.zone, u.police, kAxeLibelleL, LV_TEXT_ALIGN_CENTER);
        lv_obj_set_y(s_axe[k], kAxeY);
    }
}

// Flux : traits puis anneau, dans la zone sous les cercles (ordre de création = de dessin).
void construire_flux() {
    lv_obj_t* z = g_energie_ui.flux_zone;
    if (z == nullptr) return;
    for (lv_obj_t*& t : s_flux.trait) t = ui_ligne(z, kTraitMin);
    for (int a = 0; a < NB_ANNEAUX; a++) s_flux.anneau[a] = arc_dessin(z, a == A_PISTE);
}

// Aujourd'hui : bande, barres, contours, courbe, trait de l'heure, arc, soleil, libellés.
void construire_soleil() {
    const EnergieUI& u = g_energie_ui;
    lv_obj_t* z = u.soleil_zone;
    lv_obj_t* d = u.soleil_infos;
    if (z == nullptr || d == nullptr) return;
    Soleil& o = s_sol;
    o.bande = ui_rectangle(z, LV_OPA_20, 8);
    o.repere = ui_rectangle(z, LV_OPA_40, 0);
    for (lv_obj_t*& b : o.barre) b = ui_rectangle(z, LV_OPA_COVER, 4);
    for (lv_obj_t*& b : o.prevu) b = ui_rectangle(z, LV_OPA_10, 4);
    o.courbe = ui_ligne(z, 3);
    o.maintenant = ui_rectangle(z, LV_OPA_60, 0);
    for (lv_obj_t*& p : o.point) p = ui_rectangle(z, LV_OPA_70, LV_RADIUS_CIRCLE);
    o.horizon = ui_rectangle(z, LV_OPA_60, 0);
    o.astre = ui_rectangle(z, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    o.lever = libelle(z, u.police, kHeureL, LV_TEXT_ALIGN_CENTER);
    o.midi = libelle(z, u.police, kHeureL, LV_TEXT_ALIGN_CENTER);
    o.coucher = libelle(z, u.police, kHeureL, LV_TEXT_ALIGN_CENTER);
    o.sans_soleil = libelle(z, u.police, kSolZoneL, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_pos(o.sans_soleil, 0, (kArcHaut + kHorizonY) / 2 - 12);
    o.maximum = libelle(z, u.police, 0);
    lv_obj_set_pos(o.maximum, kSolBord, kSolMaxY);
    o.apprentissage = libelle(z, u.police, 420, LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(o.apprentissage, kSolZoneL - kSolBord - 420, kSolMaxY);
    o.vide = libelle(z, u.police, kSolZoneL, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_pos(o.vide, 0, (kSolHaut + kSolBas) / 2 - 12);
    for (lv_obj_t*& a : o.axe) a = libelle(z, u.police, kSolAxeL, LV_TEXT_ALIGN_CENTER);
    // Légende : production (plein), prévision (contour), ciel clair (trait), créneau (bande).
    // 166 px par nom : « Meilleur créneau » y tient tout juste en français et pas en
    // allemand (183 px en Roboto 22, mesuré le 10/10/2026), d'où « Créneau » ; le cadre de
    // droite, plus large, garde le nom entier.
    static const char* const kLegendes[kSolLegendes] = {tr_noop("Production"), tr_noop("Prévision"),
                                                        tr_noop("Ciel clair"), tr_noop("Créneau")};
    for (int i = 0; i < kSolLegendes; i++) {
        o.marque[i] = ui_rectangle(z, i == 1 ? LV_OPA_10 : (i == 3 ? LV_OPA_30 : LV_OPA_COVER), i == 2 ? 2 : 4);
        const int32_t x = kSolBord + i * kLegendePas;
        if (i == 2) ui_poser(o.marque[i], x, kLegendeY + 11, kLegendeMarque, 4);
        else ui_poser(o.marque[i], x, kLegendeY + 4, kLegendeMarque, kLegendeMarque);
        o.legende[i] = libelle(z, u.police, 0);
        lv_obj_set_pos(o.legende[i], x + kLegendeMarque + 8, kLegendeY);
        texte_ha_coupe(o.legende[i], tr(kLegendes[i]), kLegendePas - kLegendeMarque - 16);
        ui_hidden(o.legende[i], false);
    }
    // Colonne de droite.
    static const char* const kTitres[3] = {tr_noop("Produit aujourd'hui"), tr_noop("Prévu aujourd'hui"),
                                           tr_noop("Prévu demain")};
    for (int r = 0; r < 3; r++) {
        o.titre[r] = libelle(d, u.police, kInfosL);
        lv_obj_set_pos(o.titre[r], 0, r * kInfosLigne);
        ui_text(o.titre[r], tr(kTitres[r]));
        o.valeur[r] = libelle(d, u.police_grasse, kInfosL);
        lv_obj_set_pos(o.valeur[r], 0, r * kInfosLigne + 30);
    }
    o.cadre = ui_rectangle(d, LV_OPA_10, 14);
    ui_poser(o.cadre, 0, kCadreY, kInfosL, kCadreH);
    o.cadre_titre = libelle(o.cadre, u.police, kInfosL - 32);
    lv_obj_set_pos(o.cadre_titre, 16, 16);
    texte_ha_coupe(o.cadre_titre, tr("Meilleur créneau"), kInfosL - 32);
    ui_hidden(o.cadre_titre, false);
    o.cadre_valeur = libelle(o.cadre, u.police_grasse, kInfosL - 32);
    lv_obj_set_pos(o.cadre_valeur, 16, 52);
    ui_hidden(o.cadre_valeur, false);
    o.source = libelle(d, u.police, 0);
    lv_obj_set_pos(o.source, 0, kSourceY);
    ui_hidden(o.source, false);
}

// Bilan : la carte de gauche (textes, jauge, lignes, anneau), puis le graphique (graduations,
// zéro, barres, courbe, projection, point, bulle, axe, légende, note), dans l'ordre de dessin.
void construire_bilan() {
    EnergieUI& u = g_energie_ui;
    lv_obj_t* h = u.bilan_heros;
    lv_obj_t* z = u.bilan_zone;
    if (h == nullptr || z == nullptr) return;
    BilanUI& o = s_bil;
    const esphome::font::Font* grande = u.police_grande != nullptr ? u.police_grande : u.police_grasse;
    o.titre = libelle(h, u.police, kHerosW);
    lv_obj_set_pos(o.titre, kHerosX, kHerosTitreY);
    o.grand = libelle(h, grande, 0);
    lv_obj_set_pos(o.grand, kHerosX, kHerosGrandY);
    o.grand_unite = libelle(h, u.police_grasse, 0);
    lv_obj_set_pos(o.grand_unite, kHerosX, kHerosGrandY + kHerosUniteDy);
    ui_text(o.grand_unite, "kWh");
    o.sous = libelle(h, u.police, kHerosW);
    lv_obj_set_pos(o.sous, kHerosX, kHerosSousY);
    for (lv_obj_t* l : {o.titre, o.grand, o.sous}) ui_hidden(l, false);
    o.jauge = ui_rectangle(h, LV_OPA_50, 16);
    ui_poser(o.jauge, kHerosX, kJaugeY, kJaugeL, kJaugeH);
    o.jauge_bout = ui_rectangle(h, LV_OPA_COVER, 4);
    ui_poser(o.jauge_bout, kHerosX + kJaugeL + 3, kJaugeY + (kJaugeH - kJaugeBoutH) / 2, kJaugeBoutL, kJaugeBoutH);
    o.jauge_proj = ui_rectangle(h, LV_OPA_30, 8);
    o.jauge_plein = ui_rectangle(h, LV_OPA_COVER, 8);
    o.jauge_zero = ui_rectangle(h, LV_OPA_COVER, 0);
    ui_poser(o.jauge_zero, kJaugeZeroX - 1, kJaugeY + kJaugeBord + 3, 2, kJaugeH - 2 * (kJaugeBord + 3));
    o.echelle[0] = libelle(h, u.police, 120);
    lv_obj_set_pos(o.echelle[0], kHerosX, kEchelleY);
    o.echelle[1] = libelle(h, u.police, 80, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_pos(o.echelle[1], kJaugeZeroX - 40, kEchelleY);
    o.echelle[2] = libelle(h, u.police, 160, LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(o.echelle[2], kHerosX + kJaugeL - 160, kEchelleY);
    o.fin_titre = libelle(h, u.police, 0);
    lv_obj_set_pos(o.fin_titre, kHerosX, kFinY + 6);
    o.fin_valeur = libelle(h, u.police_grasse, 0);
    lv_obj_set_pos(o.fin_valeur, kHerosX, kFinY);
    for (lv_obj_t*& s : o.sep) s = ui_rectangle(h, LV_OPA_40, 0);
    ui_poser(o.sep[0], kHerosX, kSep1Y, kHerosW, 1);
    static const char* const kLignes[kHerosLignes] = {tr_noop("Produit"), tr_noop("Consommé"), tr_noop("Économies")};
    for (int i = 0; i < kHerosLignes; i++) {
        o.ligne_titre[i] = libelle(h, u.police, kHerosW - kLigneValeurL);
        lv_obj_set_pos(o.ligne_titre[i], kHerosX, kLigneY + i * kLignePas + 6);
        texte_ha_coupe(o.ligne_titre[i], tr(kLignes[i]), kHerosW - kLigneValeurL);
        o.ligne_valeur[i] = libelle(h, u.police_grasse, kLigneValeurL, LV_TEXT_ALIGN_RIGHT);
        lv_obj_set_pos(o.ligne_valeur[i], kHerosX + kHerosW - kLigneValeurL, kLigneY + i * kLignePas);
    }
    o.anneau_piste = arc_dessin(h, true, kAnneauBilanD, kAnneauBilanL);
    o.anneau = arc_dessin(h, false, kAnneauBilanD, kAnneauBilanL);
    lv_obj_set_x(o.anneau_piste, kHerosX);
    lv_obj_set_x(o.anneau, kHerosX);
    o.anneau_pct = libelle(h, u.police, kAnneauBilanD, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_x(o.anneau_pct, kHerosX);
    // Deux lignes à droite de l'anneau (le texte passe à la ligne, jamais coupé).
    o.anneau_texte = libelle(h, u.police, kHerosW - kAnneauBilanD - 16);
    lv_label_set_long_mode(o.anneau_texte, LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_x(o.anneau_texte, kHerosX + kAnneauBilanD + 16);

    o.unite = libelle(z, u.police, 300);
    lv_obj_set_pos(o.unite, kBilLegendeX, kBilUniteY);
    o.cumul_titre = libelle(z, u.police, 120, LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(o.cumul_titre, kBilZoneL - 8 - 120, kBilUniteY);
    texte_ha_coupe(o.cumul_titre, tr("cumul"), 120);
    o.vide = libelle(z, u.police, kBilZoneL, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_pos(o.vide, 0, (kBilY0 + kBilY1) / 2 - 13);
    for (int g = 0; g < kGrille; g++) {
        o.grille[g] = ui_rectangle(z, LV_OPA_30, 0);
        o.grad_net[g] = libelle(z, u.police, kBilX0 - 10, LV_TEXT_ALIGN_RIGHT);
        o.grad_cumul[g] = libelle(z, u.police, kBilZoneL - kBilX1 - 8);
    }
    o.zero = ui_rectangle(z, LV_OPA_60, 0);
    for (lv_obj_t*& r : o.barre) r = ui_rectangle(z, LV_OPA_COVER, 4);
    o.courbe = ui_ligne(z, 4);
    for (lv_obj_t*& p : o.proj) p = ui_rectangle(z, LV_OPA_70, LV_RADIUS_CIRCLE);
    o.point = ui_rectangle(z, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    o.bulle = ui_rectangle(z, LV_OPA_COVER, kBulleH / 2);
    o.bulle_texte = libelle(o.bulle, u.police, kBulleL, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(o.bulle_texte, LV_ALIGN_CENTER, 0, 0);
    o.fin = libelle(z, u.police, kFinL, LV_TEXT_ALIGN_RIGHT);
    for (lv_obj_t*& a : o.axe) a = libelle(z, u.police, kBilAxeL, LV_TEXT_ALIGN_CENTER);
    for (int i = 0; i < kBilanLegendes; i++) {
        // Au-delà de kBilLegendesLigne, sur la ligne suivante, depuis la gauche.
        const int col = i % kBilLegendesLigne;
        const int32_t x = kBilLegendeX + col * kBilLegendePas;
        const int32_t y = kBilLegendeY + (i / kBilLegendesLigne) * kBilLegendeRangee;
        o.marque[i] = ui_rectangle(z, LV_OPA_COVER, i == 2 ? 2 : 4);
        if (i == 2) ui_poser(o.marque[i], x, y + 11, kLegendeMarque, 4);
        else ui_poser(o.marque[i], x, y + 4, kLegendeMarque, kLegendeMarque);
        ui_hidden(o.marque[i], true);
        o.legende[i] = libelle(z, u.police, 0);
        lv_obj_set_pos(o.legende[i], x + kLegendeMarque + 8, y);
    }
    o.note = libelle(z, u.police, kBilNoteL, LV_TEXT_ALIGN_RIGHT);
    lv_obj_set_pos(o.note, kBilZoneL - kBilLegendeX - kBilNoteL, kBilNoteY);
}

// Tout ce que dessine ce fichier : une fois, à la première ouverture.
void construire() {
    EnergieUI& u = g_energie_ui;
    if (s_barres[0] != nullptr || u.zone == nullptr) return;
    construire_production();
    construire_flux();
    construire_soleil();
    construire_bilan();
    couleurs();
    s_pages.popup = u.popup;
    pages_brancher(&s_pages);
}

void demander() {
    if (g_energie_ui.demander != nullptr) g_energie_ui.demander(kVues[s_vue]);
}

}  // namespace

bool energie_formater(char* out, size_t n, float v, const char* unite) {
    if (out == nullptr || n == 0 || unite == nullptr) return false;
    float k = 0.0f;
    bool energie = false;
    if (std::strcmp(unite, "W") == 0) k = 1.0f;
    else if (std::strcmp(unite, "kW") == 0) k = 1000.0f;
    else if (std::strcmp(unite, "MW") == 0) k = 1e6f;
    else if (std::strcmp(unite, "Wh") == 0) { k = 0.001f; energie = true; }
    else if (std::strcmp(unite, "kWh") == 0) { k = 1.0f; energie = true; }
    else if (std::strcmp(unite, "MWh") == 0) { k = 1000.0f; energie = true; }
    else return false;
    const float x = v * k;   // W, ou kWh
    const float a = std::fabs(x);
    if (energie) {
        if (a < 10.0f) snprintf(out, n, "%.2f kWh", x);
        else if (a < 100.0f) snprintf(out, n, "%.1f kWh", x);
        else if (a < 1000.0f) snprintf(out, n, "%.0f kWh", x);
        else snprintf(out, n, "%.2f MWh", x / 1000.0f);
    } else {
        if (a < 1000.0f) snprintf(out, n, "%.0f W", x);
        else if (a < 10000.0f) snprintf(out, n, "%.2f kW", x / 1000.0f);
        else if (a < 1e6f) snprintf(out, n, "%.1f kW", x / 1000.0f);
        else snprintf(out, n, "%.2f MW", x / 1e6f);
    }
    return true;
}

void energie_instantane(const std::string& payload) {
    if (payload_trop_long("tab5.energie", payload.size())) return;
    const char* p = payload.data();
    const char* fin = p + payload.size();
    Instant i;
    i.recu = true;
    Mesure* champs[] = {&i.solaire, &i.maison, &i.reseau, &i.batterie, &i.batterie_puissance,
                        &i.batterie_temperature};
    for (Mesure* m : champs) *m = lire_mesure(champ_suivant(p, fin, '|'));
    const Champ unite = champ_suivant(p, fin, '|');
    texte_ha_copier(i.unite_temperature, sizeof(i.unite_temperature), unite.p, unite.n);
    i.jour = lire_mesure(champ_suivant(p, fin, '|'));
    s_i = i;
    peindre();
}

void energie_historique(const std::string& vue, const std::string& debut, const std::string& valeurs) {
    const int v = vue_de(vue);
    if (v < 0) {
        payload_refuse("tab5.energie", "historique : vue inconnue", vue.size());
        return;
    }
    if (payload_trop_long("tab5.energie", valeurs.size())) return;
    Serie s;
    s.recue = true;
    // Date illisible : 1er janvier (seuls les libellés de l'axe s'en servent). Année bornée
    // comme dans l'historique (1970..2200, DO-2, audit du 07/10/2026) : libelle_axe fait
    // `a++`, qui déborderait sur une année proche de INT_MAX.
    if (std::sscanf(debut.c_str(), "%d-%d-%d", &s.annee, &s.mois, &s.jour) != 3 || s.annee < 1970 ||
        s.annee > 2200 || s.mois < 1 || s.mois > 12 || s.jour < 1 || s.jour > 31) {
        s.annee = 2000;
        s.mois = 1;
        s.jour = 1;
    }
    const char* p = valeurs.data();
    const char* fin = p + valeurs.size();
    // Un champ vide compte (pas de donnée : heure à venir…), le dernier aussi : « 3.1;; »
    // donne trois valeurs. Avant (DO-11, audit du 07/10/2026), le champ vide après le
    // dernier « ; » était perdu : 23 barres au lieu de 24 l'après-midi, espacement changé.
    bool apres_sep = false;
    while ((p < fin || apres_sep) && s.n < kSlots[v]) {
        const Champ c = champ_suivant(p, fin, ';');
        apres_sep = p > c.p + c.n;   // le champ s'est terminé sur un « ; »
        s.v[s.n++] = champ_nombre(c, NAN);
    }
    // Une valeur négative (compteur remis à zéro mal compté) n'a pas de barre.
    for (int k = 0; k < s.n; k++)
        if (s.v[k] < 0.0f) s.v[k] = NAN;
    s_series[v] = s;
    // La série des heures sert aussi à la page Aujourd'hui ; celle de la vue, au Bilan.
    if (v == s_vue || v == HEURES) peindre();
}

// ADR-0058 : page « Aujourd'hui » (lecture : energie_soleil_lire, tab5_parse.h).
void energie_soleil(const std::string& payload) {
    if (payload_trop_long("tab5.energie", payload.size())) return;
    s_soleil_montre = energie_soleil_lire(Champ{payload.data(), payload.size()}, s_soleil);
    peindre();
}

// ADR-0058 : page « Bilan » (lecture : energie_bilan_lire, tab5_parse.h). Vues civiles de
// l'ADR-0062 (« heures », « mois_civil » : autant de créneaux que de jours dans le mois,
// « annee_civile ») ou fenêtres glissantes d'un HA d'avant (« jours », « mois »). Date
// bornée comme celle de l'historique (libellés de l'axe des jours).
void energie_bilan(const std::string& vue, const std::string& debut, const std::string& payload) {
    int v = -1;
    bool calendaire = true;
    for (int i = 0; i < NB_VUES && v < 0; i++)
        if (vue == kVuesBilan[i]) v = i;
    if (v < 0) {
        v = vue_de(vue);
        calendaire = false;
    }
    if (v < 0) {
        payload_refuse("tab5.energie", "bilan : vue inconnue", vue.size());
        return;
    }
    if (payload_trop_long("tab5.energie", payload.size())) return;
    Bilan& b = s_bilans[v];
    b = Bilan{};
    b.recu = true;
    b.calendaire = calendaire;
    if (std::sscanf(debut.c_str(), "%d-%d-%d", &b.annee, &b.mois, &b.jour) != 3 || b.annee < 1970 ||
        b.annee > 2200 || b.mois < 1 || b.mois > 12 || b.jour < 1 || b.jour > 31) {
        b.annee = 2000;
        b.mois = 1;
        b.jour = 1;
    }
    b.n = calendaire && v == JOURS ? jours_du_mois(b.annee, b.mois) : kSlots[v];
    b.montre = energie_bilan_lire(Champ{payload.data(), payload.size()}, b.n, b.lu);
    if (b.lu.devise.n > 0) texte_ha_copier(b.devise, sizeof(b.devise), b.lu.devise.p, b.lu.devise.n);
    b.lu.devise = Champ{nullptr, 0};  // pointait dans le payload, qui ne vit pas plus loin
    peindre();
}

void energie_ouvrir() {
    EnergieUI& u = g_energie_ui;
    if (u.popup == nullptr) return;
    construire();
    s_vue = HEURES;
    s_page_choisie = false;
    s_page = premiere_page();
    animate_popup_open(u.popup);
    ui_mark_activity();
    peindre();
    demander();
}

void energie_choisir_vue(int vue) {
    if (vue < 0 || vue >= NB_VUES) return;
    if (vue != s_vue) {
        s_vue = vue;
        peindre();
    }
    demander();
}

void energie_bilan_choisir_vue(int vue) { energie_choisir_vue(vue); }

void energie_page(int rang) { afficher_page(rang); }

// Thèmes (ADR-0029) : ce que construire() a peint une fois (repères, libellés, barres), puis
// le popup s'il est ouvert ; fermé, sa prochaine ouverture repeint le reste.
void energie_rejouer_theme() {
    if (s_barres[0] == nullptr) return;
    couleurs();
    peindre();
}
