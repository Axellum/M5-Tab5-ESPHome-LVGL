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
 *         - « Bilan » (tab5_maj_energie_bilan, energie_bilan_lire) : par vue (les mêmes que
 *           Production, la même s_vue), production = autoconsommé + vendu et consommation =
 *           autoconsommé + acheté en barres empilées ; taux d'autoconsommation, vendu,
 *           acheté, gains de la vue en cartes.
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
 *       energie_icone_* et energie_flux_icone_*, glyphe_bilan() aux energie_icone_* par
 *       MDI_CODE_TARGETS (règle 9). Une page de plus : sa valeur dans Page, son conteneur
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
// Cartes de la page Bilan (energie_carte_4 à 7).
enum CarteBilan : int { AUTOCONSO = 4,
                        VENDU = 5,
                        ACHETE = 6,
                        GAINS = 7 };
enum Vue : int { HEURES = 0, JOURS = 1, MOIS = 2, NB_VUES = 3 };
constexpr const char* kVues[NB_VUES] = {"heures", "jours", "mois"};
constexpr int kSlots[NB_VUES] = {24, 30, 12};
constexpr int kSlotsMax = 30;
constexpr int kAxeMax = 12;
static_assert(kSlotsMax == kEnergieSlotsMax, "une série de la vue la plus longue");
static_assert(kEnergieCartes == 2 * NB_CARTES, "quatre cartes par page à cartes");

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
constexpr int32_t kCartesSeulesY = 194;     // sans : centrées dans le corps (598 − 210) / 2
constexpr int32_t kMargeTexte = 44;         // 22 px de chaque côté
// Zone des barres (energie_zone, energie_bilan_zone : kGraphiqueL × 286) : maximum en haut,
// repère, barres, axe ; libellés de l'axe de kAxeLibelleL px, texte centré (tab5_geometrie.h).
constexpr int32_t kBordX = 20;              // marge : le premier libellé de l'axe tient entier
constexpr int32_t kBarresHaut = 34;
constexpr int32_t kBarresBas = 252;
constexpr int32_t kAxeY = 258;
// Sous 10 W, le réseau et la batterie sont « au repos », un trait du Flux aussi.
constexpr float kRepos = 10.0f;

// Page Flux : cercles de kFluxD (energie_flux_noeud.yaml), anneau autour de la maison.
constexpr int32_t kFluxD = 190;
constexpr int32_t kFluxR = kFluxD / 2;
constexpr int32_t kFluxTexteL = 156;        // texte du bas d'un cercle (corde de 176 px)
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
// (en bas), sans (les trois descendent pour remplir la page).
constexpr Centre kCentresBatterie[NB_CARTES] = {{625, 105}, {1035, 300}, {215, 300}, {625, 493}};
constexpr Centre kCentresSans[NB_CARTES] = {{625, 150}, {1035, 410}, {215, 410}, {625, 493}};
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

// Page Aujourd'hui : zone de gauche (energie_soleil_zone, 810 × 570), heures sur kSolL px.
constexpr int32_t kSolZoneL = 810;
constexpr int32_t kSolBord = 12;
constexpr int32_t kSolL = kSolZoneL - 2 * kSolBord;
constexpr int kArcPoints = 29;              // points de l'arc, lever et coucher compris
constexpr int32_t kArcPoint = 6;
constexpr int32_t kAstre = 30;              // le soleil
constexpr int32_t kMidiY = 4;               // « Midi 13:40 » au-dessus du sommet
constexpr int32_t kArcHaut = 48;            // sommet de l'arc
constexpr int32_t kHorizonY = 190;
constexpr int32_t kLeverY = 198;            // « Lever 07:12 », « Coucher 20:05 »
constexpr int32_t kHeureL = 180;            // largeur de ces trois libellés
constexpr int32_t kSolMaxY = 232;           // maximum et « Courbe en cours d'apprentissage »
constexpr int32_t kSolHaut = 262;           // haut des barres
constexpr int32_t kSolBas = 496;            // pied des barres
constexpr int32_t kSolAxeY = 502;
constexpr int32_t kSolAxeL = 64;
constexpr int kSolAxe = 8;                  // 00:00, 03:00… 21:00
constexpr int32_t kLegendeY = 540;
constexpr int32_t kLegendePas = 200;
constexpr int32_t kLegendeMarque = 18;
constexpr int kSolLegendes = 4;
constexpr int kLisse = 4;                   // points de la courbe par heure
// Colonne de droite (energie_soleil_infos, 304 × 570).
constexpr int32_t kInfosL = 304;
constexpr int32_t kInfosLigne = 96;         // une rangée « libellé, valeur »
constexpr int32_t kCadreY = 300;
constexpr int32_t kCadreH = 112;
constexpr int32_t kSourceY = 540;

// Page Bilan : légende en haut à droite de la zone, kBilanLegendeL par entrée.
constexpr int32_t kBilanLegendeL = 200;
constexpr int kBilanLegendes = 3;

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
    bool montre = false;   // vente ou achat choisi
    int annee = 2000, mois = 1, jour = 1;
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

// Bilan : par créneau, la production (autoconsommé, vendu) et la consommation
// (autoconsommé, acheté), empilées.
struct BilanUI {
    lv_obj_t* repere = nullptr;
    lv_obj_t* maximum = nullptr;
    lv_obj_t* vide = nullptr;
    lv_obj_t* prod_auto[kSlotsMax] = {};
    lv_obj_t* prod_vendu[kSlotsMax] = {};
    lv_obj_t* cons_auto[kSlotsMax] = {};
    lv_obj_t* cons_achat[kSlotsMax] = {};
    lv_obj_t* axe[kAxeMax] = {};
    lv_obj_t* marque[kBilanLegendes] = {};
    lv_obj_t* legende[kBilanLegendes] = {};
};
BilanUI s_bil;

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

// Un arc de dessin (anneau de la maison) : ni bouton, ni toucher, ni marge ; MAIN = la piste
// (montrée par `piste`), INDICATOR = une part.
lv_obj_t* arc_dessin(lv_obj_t* parent, bool piste) {
    lv_obj_t* a = lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, kAnneauD, kAnneauD);
    lv_arc_set_rotation(a, 270);        // 0° en haut, sens des aiguilles
    lv_arc_set_bg_angles(a, 0, 360);    // avant tout lv_arc_set_angles ([AI-WARNING])
    for (lv_part_t part : {LV_PART_MAIN, LV_PART_INDICATOR}) {
        lv_obj_set_style_arc_width(a, kAnneauL, part);
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

// Icône d'une carte de la page Bilan (mdi_font_45).
const char* glyphe_bilan(int carte) {
    switch (carte) {
        case AUTOCONSO: return "\U000F1903";                                 // home-lightning-bolt
        case VENDU: return "\U000F192C";                                     // transmission-tower-export
        case ACHETE: return "\U000F192D";                                    // transmission-tower-import
        default: return "\U000F1007";                                        // piggy-bank
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
    char ligne[48] = "";
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
    } else if (i.jour.choisi) {
        snprintf(r.ligne, sizeof(r.ligne), "%s", tr("Produit aujourd'hui"));
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
        } else {
            char x[24];
            puissance(x, sizeof(x), std::fabs(p));
            snprintf(r.ligne, sizeof(r.ligne), "%s %s", r.etat > 0 ? tr("Charge") : tr("Décharge"), x);
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
    const Centre* c = batterie ? kCentresBatterie : kCentresSans;
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

// Un cercle : bord à la couleur de sa source, icône à celle de son état (comme sa carte).
void peindre_noeud(int c, const Vu& v, uint32_t bord) {
    const EnergieUI& u = g_energie_ui;
    lv_obj_t* o = u.flux_noeud[c];
    ui_hidden(o, false);
    contour(o, bord, 3, LV_OPA_COVER);
    ui_text(u.flux_valeur[c], v.valeur);
    ui_text_color(u.flux_valeur[c], v.couleur_valeur);
    ui_text(u.flux_icone[c], glyphe_carte(c, v.etat));
    ui_text_color(u.flux_icone[c], v.couleur);
    texte_ha_coupe(u.flux_texte[c], v.ligne, kFluxTexteL);
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
        snprintf(m.ligne, sizeof(m.ligne), tr("%d %% solaire"),
                 static_cast<int>(std::lround(100.0f * f.solaire_maison / entre)));
    else
        snprintf(m.ligne, sizeof(m.ligne), "%s", tr("Consommation"));
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

// --- Page Bilan --------------------------------------------------------------------------

void peindre_bilan() {
    const EnergieUI& u = g_energie_ui;
    for (int v = 0; v < NB_VUES; v++) highlight_button_border(u.bilan_vue_btn[v], v == s_vue, UIColor.ACCENT);
    if (s_bil.repere == nullptr) return;
    const Bilan& b = s_bilans[s_vue];
    const Serie& prod = s_series[s_vue];
    const int nb = kSlots[s_vue];
    float produit[kSlotsMax];
    for (int k = 0; k < kSlotsMax; k++) produit[k] = (prod.recue && k < prod.n) ? prod.v[k] : NAN;
    const EnergieBilanTotaux t = energie_bilan_totaux(produit, nb, b.lu);
    ui_text(u.bilan_titre, tr(kPeriodes[s_vue]));

    // Cartes : autoconsommé toujours ; vendu, acheté avec leur compteur ; gains avec un prix.
    bool montre[NB_CARTES] = {true, b.lu.vente_choisie, b.lu.achat_choisi, b.devise[0] != '\0' && b.lu.gain_choisi};
    const int32_t largeur = cartes_placer(AUTOCONSO, montre, kCartesY);
    char v[24], l1[48], l2[48], x[24];
    // Bilan de la vue pas encore reçu : « -- » et rien dessous (sans vente ni achat lus,
    // le taux vaudrait 100 %).
    if (b.recu && std::isfinite(t.taux))
        snprintf(v, sizeof(v), "%d %%", static_cast<int>(std::lround(t.taux * 100.0f)));
    else snprintf(v, sizeof(v), "--");
    l1[0] = '\0';
    l2[0] = '\0';
    if (b.recu) {
        kwh(x, sizeof(x), t.produit);
        snprintf(l1, sizeof(l1), "%s %s", tr("Produit"), x);
    }
    if (b.recu && std::isfinite(t.consomme)) {
        kwh(x, sizeof(x), t.consomme);
        snprintf(l2, sizeof(l2), "%s %s", tr("Consommé"), x);
    }
    peindre_carte(AUTOCONSO, v, UIColor.TEXT_PRIMARY, glyphe_bilan(AUTOCONSO), UIColor.GOLD, l1, l2, largeur);
    if (montre[1]) {
        kwh(v, sizeof(v), t.vendu);
        l2[0] = '\0';
        if (std::isfinite(t.vendu) && std::isfinite(t.produit) && t.produit > 0.0f)
            snprintf(l2, sizeof(l2), tr("%d %% de la production"),
                     static_cast<int>(std::lround(std::fmin(t.vendu / t.produit, 1.0f) * 100.0f)));
        peindre_carte(VENDU, v, UIColor.TEXT_PRIMARY, glyphe_bilan(VENDU), UIColor.SUCCESS, tr("Vers le réseau"), l2,
                      largeur);
    }
    if (montre[2]) {
        kwh(v, sizeof(v), t.achete);
        l2[0] = '\0';
        if (std::isfinite(t.achete) && std::isfinite(t.consomme) && t.consomme > 0.0f)
            snprintf(l2, sizeof(l2), tr("%d %% de la consommation"),
                     static_cast<int>(std::lround(std::fmin(t.achete / t.consomme, 1.0f) * 100.0f)));
        peindre_carte(ACHETE, v, UIColor.TEXT_PRIMARY, glyphe_bilan(ACHETE), UIColor.WARNING, tr("Depuis le réseau"), l2,
                      largeur);
    }
    if (montre[3]) {
        if (std::isfinite(t.gain)) snprintf(v, sizeof(v), "%.2f %s", t.gain, b.devise);
        else snprintf(v, sizeof(v), "--");
        peindre_carte(GAINS, v, UIColor.TEXT_PRIMARY, glyphe_bilan(GAINS), UIColor.SUCCESS, tr(kPeriodes[s_vue]), "",
                      largeur);
    }

    // Barres : par créneau, production (autoconsommé + vendu) à gauche, consommation
    // (autoconsommé + acheté) à droite, à la même échelle.
    EnergieBilanCreneau c[kSlotsMax];
    float maximum = 0.0f;
    for (int k = 0; k < nb; k++) {
        c[k] = energie_bilan_creneau(produit[k], b.lu, k);
        if (std::isfinite(c[k].produit)) maximum = std::fmax(maximum, c[k].produit);
        if (std::isfinite(c[k].consomme)) maximum = std::fmax(maximum, c[k].consomme);
    }
    const bool vide = !b.recu || maximum <= 0.0f;
    ui_hidden(s_bil.vide, !vide);
    if (vide) ui_text(s_bil.vide, b.recu ? tr("Aucun historique") : tr("En attente de Home Assistant"));
    ui_hidden(s_bil.repere, vide);
    ui_hidden(s_bil.maximum, vide);
    if (!vide) {
        energie_formater(x, sizeof(x), maximum, "kWh");
        ui_text(s_bil.maximum, x);
    }
    const int32_t pas = (kGraphiqueL - 2 * kBordX) / nb;
    int32_t l = pas * 34 / 100;
    if (l < 4) l = 4;
    const int32_t pleine = kBarresBas - kBarresHaut;
    // Une pile de deux : `bas` en bas, `haut` posé dessus ; rien sous un dixième de pixel.
    auto pile = [pleine, maximum](lv_obj_t* o_bas, lv_obj_t* o_haut, int32_t x, float bas, float haut, int32_t lp) {
        const bool b_ok = std::isfinite(bas) && bas > 0.0f, h_ok = std::isfinite(haut) && haut > 0.0f;
        const int32_t hb = b_ok ? hauteur_de(bas, maximum, pleine) : 0;
        ui_hidden(o_bas, !b_ok);
        if (b_ok) ui_poser(o_bas, x, kBarresBas - hb, lp, hb);
        ui_hidden(o_haut, !h_ok);
        if (h_ok) {
            const int32_t hh = hauteur_de(haut, maximum, pleine);
            ui_poser(o_haut, x, kBarresBas - hb - hh, lp, hh);
        }
    };
    for (int k = 0; k < kSlotsMax; k++) {
        if (vide || k >= nb) {
            for (lv_obj_t* o : {s_bil.prod_auto[k], s_bil.prod_vendu[k], s_bil.cons_auto[k], s_bil.cons_achat[k]})
                ui_hidden(o, true);
            continue;
        }
        const int32_t centre = kBordX + k * pas + pas / 2;
        // Vendu affiché = produit − autoconsommé : la pile fait toujours la production.
        const float vendu = std::isfinite(c[k].produit) && std::isfinite(c[k].autoconsomme)
                                ? c[k].produit - c[k].autoconsomme
                                : NAN;
        pile(s_bil.prod_auto[k], s_bil.prod_vendu[k], centre - l - 1, c[k].autoconsomme, vendu, l);
        const float achete = std::isfinite(c[k].consomme) ? c[k].consomme - (std::isfinite(c[k].autoconsomme)
                                                                                 ? c[k].autoconsomme
                                                                                 : 0.0f)
                                                          : NAN;
        pile(s_bil.cons_auto[k], s_bil.cons_achat[k], centre + 1, std::isfinite(c[k].consomme) ? c[k].autoconsomme : NAN,
             achete, l);
    }
    // Légende : autoconsommé, puis vendu et acheté s'ils ont leur compteur, collée à droite.
    const bool legende[kBilanLegendes] = {true, b.lu.vente_choisie, b.lu.achat_choisi};
    static const char* const kLegendes[kBilanLegendes] = {tr_noop("Autoconsommé"), tr_noop("Vendu"), tr_noop("Acheté")};
    int nl = 0;
    for (bool m : legende) nl += m ? 1 : 0;
    int rang = 0;
    for (int i = 0; i < kBilanLegendes; i++) {
        ui_hidden(s_bil.marque[i], !legende[i]);
        ui_hidden(s_bil.legende[i], !legende[i]);
        if (!legende[i]) continue;
        const int32_t x0 = kGraphiqueL - kBordX - (nl - rang) * kBilanLegendeL;
        ui_poser(s_bil.marque[i], x0, 5, kLegendeMarque, kLegendeMarque);
        texte_ha_coupe(s_bil.legende[i], tr(kLegendes[i]), kBilanLegendeL - kLegendeMarque - 16);
        ui_x(s_bil.legende[i], x0 + kLegendeMarque + 8);
        ui_y(s_bil.legende[i], 0);
        rang++;
    }
    Serie axe;
    axe.recue = b.recu;
    axe.annee = b.annee;
    axe.mois = b.mois;
    axe.jour = b.jour;
    axe.n = nb;
    peindre_axe(s_bil.axe, axe, nb, pas);
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
    // Bilan.
    ui_style_couleur(s_bil.repere, LV_STYLE_BG_COLOR, UIColor.GLASS_RIM);
    for (lv_obj_t* l : {s_bil.maximum, s_bil.vide}) ui_text_color(l, UIColor.TEXT_DIM);
    for (lv_obj_t* l : s_bil.axe) ui_text_color(l, UIColor.TEXT_DIM);
    for (int k = 0; k < kSlotsMax; k++) {
        ui_style_couleur(s_bil.prod_auto[k], LV_STYLE_BG_COLOR, UIColor.GOLD);
        ui_style_couleur(s_bil.prod_vendu[k], LV_STYLE_BG_COLOR, UIColor.SUCCESS);
        ui_style_couleur(s_bil.cons_auto[k], LV_STYLE_BG_COLOR, UIColor.GOLD);
        ui_style_couleur(s_bil.cons_achat[k], LV_STYLE_BG_COLOR, UIColor.WARNING);
    }
    const uint32_t legendes[kBilanLegendes] = {UIColor.GOLD, UIColor.SUCCESS, UIColor.WARNING};
    for (int i = 0; i < kBilanLegendes; i++) {
        ui_style_couleur(s_bil.marque[i], LV_STYLE_BG_COLOR, legendes[i]);
        ui_text_color(s_bil.legende[i], UIColor.TEXT_SOFT);
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

// Bilan : noms des cartes, repère, piles, légende, axe.
void construire_bilan() {
    EnergieUI& u = g_energie_ui;
    lv_obj_t* z = u.bilan_zone;
    if (z == nullptr) return;
    ui_text(u.nom[AUTOCONSO], tr("AUTOCONSOMMÉ"));
    ui_text(u.nom[VENDU], tr("VENDU"));
    ui_text(u.nom[ACHETE], tr("ACHETÉ"));
    ui_text(u.nom[GAINS], tr("GAINS"));
    s_bil.repere = ui_rectangle(z, LV_OPA_40, 0);
    ui_poser(s_bil.repere, kBordX, kBarresHaut, kGraphiqueL - 2 * kBordX, 1);
    ui_hidden(s_bil.repere, true);
    s_bil.maximum = libelle(z, u.police, 0);
    lv_obj_set_pos(s_bil.maximum, kBordX, 0);
    s_bil.vide = libelle(z, u.police, 0);
    lv_obj_align(s_bil.vide, LV_ALIGN_CENTER, 0, 0);
    for (int k = 0; k < kSlotsMax; k++) {
        s_bil.prod_auto[k] = ui_rectangle(z, LV_OPA_COVER, 2);
        s_bil.prod_vendu[k] = ui_rectangle(z, LV_OPA_COVER, 2);
        s_bil.cons_auto[k] = ui_rectangle(z, LV_OPA_COVER, 2);
        s_bil.cons_achat[k] = ui_rectangle(z, LV_OPA_COVER, 2);
    }
    for (int i = 0; i < kBilanLegendes; i++) {
        s_bil.marque[i] = ui_rectangle(z, LV_OPA_COVER, 4);
        s_bil.legende[i] = libelle(z, u.police, 0);
    }
    for (int k = 0; k < kAxeMax; k++) {
        s_bil.axe[k] = libelle(z, u.police, kAxeLibelleL, LV_TEXT_ALIGN_CENTER);
        lv_obj_set_y(s_bil.axe[k], kAxeY);
    }
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

// ADR-0058 : page « Bilan » (lecture : energie_bilan_lire, tab5_parse.h). Date bornée comme
// celle de l'historique (libellés de l'axe des jours).
void energie_bilan(const std::string& vue, const std::string& debut, const std::string& payload) {
    const int v = vue_de(vue);
    if (v < 0) {
        payload_refuse("tab5.energie", "bilan : vue inconnue", vue.size());
        return;
    }
    if (payload_trop_long("tab5.energie", payload.size())) return;
    Bilan& b = s_bilans[v];
    b = Bilan{};
    b.recu = true;
    if (std::sscanf(debut.c_str(), "%d-%d-%d", &b.annee, &b.mois, &b.jour) != 3 || b.annee < 1970 ||
        b.annee > 2200 || b.mois < 1 || b.mois > 12 || b.jour < 1 || b.jour > 31) {
        b.annee = 2000;
        b.mois = 1;
        b.jour = 1;
    }
    b.montre = energie_bilan_lire(Champ{payload.data(), payload.size()}, kSlots[v], b.lu);
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
