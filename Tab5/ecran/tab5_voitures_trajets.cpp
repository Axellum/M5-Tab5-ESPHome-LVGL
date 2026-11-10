/**
 * [AI-CONTEXT]
 * @file tab5_voitures_trajets.cpp
 * @role Page « Trajets » du popup Voitures (ADR-0083, lot 9 « C3 » du plan des voitures) : une carte
 *       de verre par voiture dont l'odomètre est choisi — le nom, le compteur, les kilomètres du mois
 *       en grand, le kWh / 100 km ESTIMÉ à côté, puis sept barres de kilomètres (un jour chacune,
 *       aujourd'hui à droite) et, sous elles, sept barres de kWh reçus par la voiture le même jour.
 *       D'après ce que pousse tab5_maj_voiture_trajets (lu par trajets_lire, Tab5/socle/tab5_parse.h).
 * @architecture_constraint Push-only et events-only (ADR-0001, ADR-0025) : la tablette ne nomme aucune
 *       entité. Les widgets sont créés ici, en C++, sur voitures_corps() à la première montre : aucun id
 *       YAML. Les polices sont celles des widgets de l'Aperçu (nom : roboto_32_b, autonomie :
 *       roboto_55_b, « d'autonomie » : roboto_22) : aucune ligne de plus dans tab5-voitures.yaml.
 *       Un kWh / 100 km n'est PAS une consommation mesurée : c'est l'énergie chargée ce mois-ci divisée
 *       par les kilomètres du mois, pertes de charge et climatisation comprises ; l'écran le dit
 *       (« estimé ») et n'emploie jamais le mot « consommation ». Un jour sans relevé n'est pas un jour
 *       à 0 km : pas de barre ; un 0 connu est un petit trait gris. Rien n'est inventé.
 *       Le kWh reçu par jour (rangée du bas) n'est dessiné que sur une carte aussi haute que celle du
 *       Tab5 (kAvecKwh, comme la consommation de la page Aujourd'hui, ADR-0078) : la 7B est trop basse.
 *       Couleurs par les rôles de la palette (ADR-0029), repeintes par voitures_rejouer_theme() ; les
 *       écritures sont comparées d'abord (ui_text, ui_hidden, ui_poser, ui_style_couleur).
 * @ai_instruction Un widget de plus = son champ dans struct Colonne et sa ligne dans creer(), sa
 *       place dans peindre_colonne(). Un texte affiché passe par tr(). Un jour de plus ou de moins est
 *       un changement de contrat (kTrajetsJours, tab5_parse.h ; fenetre_trajets de tools/voitures_charges.py).
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

constexpr const char* kTag = "tab5.voitures_trajets";
constexpr int kVoit = kVoituresCartes;   // une colonne par voiture
constexpr int kJ = kTrajetsJours;        // sept jours

TrajetsLu s_t[kVoit];   // ce que HA a poussé, par voiture (rang - 1)

// --- Géométrie ----------------------------------------------------------------------------
// Les nombres sont ceux du Tab5 (corps de 1202 × 598), ramenés à la carte par hauteur_carte() (la 7B :
// 946 × 478) ; les polices, elles, ne changent pas.
constexpr int32_t kPad = 26;                                    // marge intérieure de la carte
constexpr int32_t kHautMin = 14;                                // marge du haut au plus serrée
constexpr int32_t kCarteSeuleL = kCorpsW;                       // une seule voiture : pleine largeur
constexpr int32_t kCarteDeuxL = (kCorpsW - kCartesEcart) / 2;   // deux voitures : côte à côte
constexpr int32_t kColonneMax = kCarteDeuxL - 2 * kPad;         // la colonne de contenu d'une voiture seule
constexpr int32_t kEcartNom = hauteur_carte(12);                // sous le nom
constexpr int32_t kEcartBloc = hauteur_carte(20);               // entre les blocs (nombres, km, kWh)
constexpr int32_t kEcartTitre = hauteur_carte(4);               // sous le titre d'une rangée de barres
constexpr int32_t kEcartJour = hauteur_carte(6);                // sous les barres, avant les jours
constexpr int32_t kBarresKmH = hauteur_carte(120);              // hauteur de la plus haute barre de km
constexpr int32_t kBarresKwhH = hauteur_carte(70);              // … de la plus haute barre de kWh
constexpr int32_t kTrait = 4;                                   // un zéro connu : un trait, pas une barre
constexpr int32_t kEcartInconnu = 16;                           // entre le nom et le compteur
// La rangée des kWh reçus : une carte aussi haute que celle du Tab5 seulement.
constexpr bool kAvecKwh = kCorpsH >= kCorpsHTab5;

// Les widgets d'une carte (une par voiture montrée). Créés une fois, repeints à chaque peindre().
struct Colonne {
    lv_obj_t* fond = nullptr;         // la carte de verre
    lv_obj_t* nom = nullptr;          // « Tesla 2023 »
    lv_obj_t* compteur = nullptr;     // « Compteur 41 280 km »
    lv_obj_t* km = nullptr;           // « 600 » (roboto_55_b)
    lv_obj_t* km_unite = nullptr;     // « km »
    lv_obj_t* km_sous = nullptr;      // « ce mois-ci »
    lv_obj_t* conso = nullptr;        // « 18.6 »
    lv_obj_t* conso_sous = nullptr;   // « kWh / 100 km · estimé »
    lv_obj_t* titre_km = nullptr;     // « Km par jour »
    lv_obj_t* titre_kwh = nullptr;    // « kWh chargés par jour »
    lv_obj_t* vide = nullptr;         // « Pas encore de relevé du compteur »
    lv_obj_t* barre_km[kJ] = {};
    lv_obj_t* val_km[kJ] = {};
    lv_obj_t* jour[kJ] = {};          // « Lun 9 »
    lv_obj_t* barre_kwh[kJ] = {};
    lv_obj_t* val_kwh[kJ] = {};
};
Colonne s_c[kVoit];
bool s_cree = false;

// Les trois polices de la page, lues sur les widgets de l'Aperçu (voitures_carte.yaml).
const lv_font_t* s_police_grande = nullptr;   // roboto_55_b
const lv_font_t* s_police_grasse = nullptr;   // roboto_32_b
const lv_font_t* s_police = nullptr;          // roboto_22

// Hauteur d'une ligne de texte dans la police du label.
int32_t haut(lv_obj_t* l) {
    const lv_font_t* f = l != nullptr ? lv_obj_get_style_text_font(l, LV_PART_MAIN) : nullptr;
    return f != nullptr ? lv_font_get_line_height(f) : 0;
}

// Un texte non cliquable, masqué ; largeur fixe (> 0 : texte coupé) ou au contenu.
lv_obj_t* libelle(lv_obj_t* parent, const lv_font_t* police, int32_t largeur, lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
    lv_obj_t* l = lv_label_create(parent);
    if (police != nullptr) lv_obj_set_style_text_font(l, police, LV_PART_MAIN);
    lv_label_set_text(l, "");
    if (largeur > 0) {
        lv_obj_set_width(l, largeur);
        lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
    }
    lv_obj_set_style_text_align(l, align, LV_PART_MAIN);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

// La carte de verre : le style partagé style_glass_card lui-même (tab5-styles.yaml), pas une copie, pour que
// ses formes (rayon, bords, opacité) et ses couleurs suivent le thème comme celles de l'Aperçu.
lv_obj_t* carte_verre(lv_obj_t* parent, lv_style_t* style) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_add_style(o, style, LV_PART_MAIN);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    return o;
}

// Les widgets de chaque carte, à la première montre : le conteneur de la page est la carte modale du
// popup, les polices sont celles de l'Aperçu. Sans eux (popup pas encore lié), rien n'est créé.
void creer() {
    if (s_cree) return;
    const VoituresUI& u = g_voitures_ui;
    lv_obj_t* corps = voitures_corps();
    if (corps == nullptr || u.style_carte == nullptr || u.nom[0] == nullptr || u.autonomie[0] == nullptr || u.sous[0] == nullptr)
        return;
    s_police_grasse = lv_obj_get_style_text_font(u.nom[0], LV_PART_MAIN);
    s_police_grande = lv_obj_get_style_text_font(u.autonomie[0], LV_PART_MAIN);
    s_police = lv_obj_get_style_text_font(u.sous[0], LV_PART_MAIN);
    for (Colonne& o : s_c) {
        o.fond = carte_verre(corps, u.style_carte);
        o.nom = libelle(o.fond, s_police_grasse, 0);
        o.compteur = libelle(o.fond, s_police, 0);
        o.km = libelle(o.fond, s_police_grande, 0);
        o.km_unite = libelle(o.fond, s_police_grasse, 0);
        o.km_sous = libelle(o.fond, s_police, 0);
        o.conso = libelle(o.fond, s_police_grande, 0);
        o.conso_sous = libelle(o.fond, s_police, 0);
        o.titre_km = libelle(o.fond, s_police, 0);
        o.titre_kwh = libelle(o.fond, s_police, 0);
        o.vide = libelle(o.fond, s_police, 0);
        for (int i = 0; i < kJ; i++) {
            o.barre_km[i] = ui_rectangle(o.fond, LV_OPA_COVER, 8);
            o.val_km[i] = libelle(o.fond, s_police, 1, LV_TEXT_ALIGN_CENTER);
            o.jour[i] = libelle(o.fond, s_police, 1, LV_TEXT_ALIGN_CENTER);
            o.barre_kwh[i] = ui_rectangle(o.fond, LV_OPA_COVER, 8);
            o.val_kwh[i] = libelle(o.fond, s_police, 1, LV_TEXT_ALIGN_CENTER);
        }
    }
    s_cree = true;
}

// --- Textes --------------------------------------------------------------------------------

// « 41280 » → « 41 280 » (une espace sépare les milliers : les polices n'ont pas l'espace fine).
void milliers(char* out, size_t n, float v) {
    char brut[24];
    snprintf(brut, sizeof(brut), "%ld", std::lround(v));
    const size_t l = strlen(brut);
    size_t j = 0;
    for (size_t i = 0; i < l && j + 2 < n; i++) {
        if (i > 0 && brut[i] != '-' && (l - i) % 3 == 0 && brut[i - 1] != '-') out[j++] = ' ';
        out[j++] = brut[i];
    }
    out[j] = '\0';
}

// Le jour du mois d'aujourd'hui sur l'horloge de la tablette (-1 tant qu'elle n'est pas réglée) : le jour
// de la barre qu'on met en avant. La fenêtre de HA finit aujourd'hui, sauf juste après minuit tant que
// HA n'a pas repoussé : la comparaison tient compte du vrai jour, pas du rang de la barre.
int jour_du_mois_aujourdhui() {
    const time_t now = tab5_time_source(nullptr);
    if (!tab5_heure_valide(now)) return -1;
    struct tm t{};
    localtime_r(&now, &t);
    return t.tm_mday;
}

// Vrai si la voiture a au moins un relevé lisible : le compteur, les kilomètres du mois, le kWh / 100 km, ou
// un jour (kilomètres ou kWh). Sinon la carte dit seulement qu'il n'y a pas encore de relevé.
bool a_releves(const TrajetsLu& t) {
    if (std::isfinite(t.compteur) || std::isfinite(t.km_mois) || std::isfinite(t.kwh_100km)) return true;
    for (int i = 0; i < kJ; i++)
        if (std::isfinite(t.km[i]) || std::isfinite(t.kwh[i])) return true;
    return false;
}

// « kWh / 100 km · estimé » : sur une ligne, ou sur deux (le séparateur « · » devient un saut de ligne) quand
// la ligne entière ne tient pas dans `place` (la 7B, deux voitures). Renvoie le nombre de lignes ; `larg` reçoit
// la largeur du bloc (la plus longue ligne), dans la police du label.
int sous_conso(lv_obj_t* lbl, char* out, size_t n, int32_t place, int32_t& larg) {
    snprintf(out, n, "%s", tr("kWh / 100 km · estimé"));
    larg = ui_largeur_texte(lbl, out);
    if (larg <= place) return 1;
    char* sep = strstr(out, " \xC2\xB7 ");   // espace, point médian (2 octets), espace : 4 octets
    if (sep == nullptr) return 1;
    memmove(sep + 1, sep + 4, strlen(sep + 4) + 1);   // « a · b » devient « a », un octet, « b »
    sep[0] = '\0';
    larg = std::max(ui_largeur_texte(lbl, out), ui_largeur_texte(lbl, sep + 1));   // chaque ligne seule
    sep[0] = '\n';
    return 2;
}

// La plus grande valeur connue d'une série (0 sans valeur).
float maximum(const float* v) {
    float m = 0.0f;
    for (int i = 0; i < kJ; i++)
        if (std::isfinite(v[i]) && v[i] > m) m = v[i];
    return m;
}

// Une rangée de sept barres : `barres` et `valeurs` (labels au-dessus), posées dans la colonne [x, x + col[,
// la ligne de base à y_base, la plus haute à `hmax` de haut. Une valeur inconnue : rien ; un zéro connu : un
// trait de la couleur de la piste, sans nombre ; sinon une barre et son nombre (`decimales` chiffres après
// la virgule, « 112 » en km, « 7.4 » en kWh).
void peindre_barres(lv_obj_t* const* barres, lv_obj_t* const* valeurs, const float* v, int32_t x, int32_t col,
                    int32_t y_base, int32_t hmax, uint32_t couleur, int decimales) {
    const int32_t pas = col / kJ;
    const int32_t bw = pas * 2 / 3;
    const float vmax = maximum(v);
    for (int i = 0; i < kJ; i++) {
        const int32_t bx = x + i * pas + (pas - bw) / 2;
        if (!std::isfinite(v[i])) {
            ui_hidden(barres[i], true);
            ui_hidden(valeurs[i], true);
            continue;
        }
        // Le texte d'abord : une valeur qui s'écrit « 0 » ou « 0.0 » est un zéro (un trait, pas de nombre), et
        // 99.96 s'écrit « 100 », pas « 100.0 ».
        const bool une_decimale = decimales > 0 && std::round(v[i] * 10.0f) / 10.0f < 100.0f;
        char txt[16];
        snprintf(txt, sizeof(txt), une_decimale ? "%.1f" : "%.0f", v[i]);
        const bool zero = v[i] <= 0.0f || strcmp(txt, une_decimale ? "0.0" : "0") == 0;
        int32_t h = kTrait;
        if (!zero && vmax > 0.0f) h = std::max<int32_t>(kTrait, std::lround(v[i] / vmax * hmax));
        ui_style_couleur(barres[i], LV_STYLE_BG_COLOR, zero ? UIColor.ARC_TRACK : couleur);
        ui_poser(barres[i], bx, y_base - h, bw, h);
        if (zero) {
            ui_hidden(valeurs[i], true);
            continue;
        }
        ui_text(valeurs[i], txt);
        ui_text_color(valeurs[i], v[i] >= vmax ? UIColor.TEXT_PRIMARY : UIColor.TEXT_SOFT);
        ui_poser(valeurs[i], x + i * pas, y_base - h - haut(valeurs[i]) - 2, pas, LV_SIZE_CONTENT);
    }
}

// La carte `c` : la voiture de rang `k` + 1, posée dans la largeur `w`, à la colonne x0 du corps.
void peindre_colonne(int c, int k, int32_t x0, int32_t w) {
    Colonne& o = s_c[c];
    const TrajetsLu& t = s_t[k];
    ui_poser(o.fond, kCorpsX + x0, kCorpsY, w, kCorpsH);

    const int32_t col = std::min(w - 2 * kPad, kColonneMax);
    const int32_t x = (w - col) / 2;
    const bool releves = a_releves(t);

    // --- Textes de l'en-tête : le nom (couleur de son rang) et le compteur à droite -------------
    char nom[96], txt[64], num[32];
    const VoitureRecue& vr = voiture_recue(k);
    if (vr.nom[0] != '\0') snprintf(nom, sizeof(nom), "%s", vr.nom);
    else snprintf(nom, sizeof(nom), tr("Voiture %d"), k + 1);
    const bool a_compteur = std::isfinite(t.compteur);
    if (a_compteur) {
        milliers(num, sizeof(num), t.compteur);
        snprintf(txt, sizeof(txt), tr("Compteur %s km"), num);
    }
    const int32_t h_nom = haut(o.nom);
    const int32_t h_compteur = haut(o.compteur);
    const int32_t h_ligne = std::max(h_nom, h_compteur);
    const int32_t h_grand = haut(o.km);
    const int32_t h_sous = haut(o.km_sous);
    const int32_t h_titre = haut(o.titre_km);
    const int32_t h_val = haut(o.val_km[0]);
    const int32_t h_jours = 2 * haut(o.jour[0]);   // le jour de la semaine, puis son numéro : deux lignes
    // « kWh / 100 km · estimé » à droite des kilomètres : une ligne si elle tient, sinon deux.
    char sous_c[64];
    int32_t l_sous_c = 0;
    const int lignes_sous_c = sous_conso(o.conso_sous, sous_c, sizeof(sous_c), col / 2, l_sous_c);
    // Le « grand » nombre a de l'air sous ses chiffres : sa ligne et celle de dessous se recouvrent un peu.
    const int32_t recouvre = 4;

    // --- La pile verticale, centrée dans la carte ---------------------------------------------
    int32_t total = h_ligne;
    if (releves) {
        total += kEcartNom + h_grand + lignes_sous_c * h_sous - recouvre + kEcartBloc;
        total += h_titre + kEcartTitre + h_val + kBarresKmH + kEcartJour + h_jours;
        if (kAvecKwh) total += kEcartBloc + h_titre + kEcartTitre + h_val + kBarresKwhH;
    } else {
        total += kEcartBloc + haut(o.vide);
    }
    int32_t y = std::max(kHautMin, (kCorpsH - total) / 2);

    const uint32_t c_rang = k == 0 ? UIColor.ACCENT : UIColor.ACCENT_ALT;
    // Nom à gauche, compteur à droite sur la même ligne ; le nom est coupé avant de toucher le compteur.
    int32_t l_compteur = 0;
    ui_text_color(o.compteur, UIColor.TEXT_DIM);
    if (a_compteur) {
        ui_text(o.compteur, txt);
        l_compteur = ui_largeur_texte(o.compteur, txt);
        ui_poser(o.compteur, x + col - l_compteur, y + (h_ligne - h_compteur) / 2, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    } else {
        ui_hidden(o.compteur, true);
    }
    ui_text_color(o.nom, c_rang);
    texte_ha_coupe(o.nom, nom, col - (a_compteur ? l_compteur + kEcartInconnu : 0));
    ui_poser(o.nom, x, y + (h_ligne - h_nom) / 2, col - (a_compteur ? l_compteur + kEcartInconnu : 0), LV_SIZE_CONTENT);
    y += h_ligne;

    // --- Rien à montrer : la voiture a son odomètre choisi mais aucun relevé n'est lisible -------
    ui_hidden(o.vide, releves);
    if (!releves) {
        for (lv_obj_t* l : {o.km, o.km_unite, o.km_sous, o.conso, o.conso_sous, o.titre_km, o.titre_kwh}) ui_hidden(l, true);
        for (int i = 0; i < kJ; i++)
            for (lv_obj_t* l : {o.barre_km[i], o.val_km[i], o.jour[i], o.barre_kwh[i], o.val_kwh[i]}) ui_hidden(l, true);
        y += kEcartBloc;
        ui_text(o.vide, tr("Pas encore de relevé du compteur"));
        ui_text_color(o.vide, UIColor.TEXT_DIM);
        ui_poser(o.vide, x, y, col, LV_SIZE_CONTENT);
        return;
    }

    // --- Les kilomètres du mois et le kWh / 100 km estimé, côte à côte ------------------------------
    y += kEcartNom;
    if (std::isfinite(t.km_mois)) milliers(num, sizeof(num), t.km_mois);
    else snprintf(num, sizeof(num), "--");
    ui_text(o.km, num);
    ui_text_color(o.km, UIColor.TEXT_PRIMARY);
    ui_poser(o.km, x, y, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    // L'unité sur la ligne de base du grand chiffre, à sa droite (comme l'autonomie de l'Aperçu).
    const bool a_km = std::isfinite(t.km_mois);
    ui_text(o.km_unite, a_km ? "km" : "");
    ui_text_color(o.km_unite, UIColor.TEXT_SOFT);
    ui_hidden(o.km_unite, !a_km);
    if (a_km) {
        const int32_t w_km = ui_largeur_texte(o.km, num);
        ui_poser(o.km_unite, x + w_km + 10, y + h_grand - haut(o.km_unite) - 6, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    }
    ui_text(o.km_sous, tr("ce mois-ci"));
    ui_text_color(o.km_sous, UIColor.TEXT_DIM);
    ui_poser(o.km_sous, x, y + h_grand - recouvre, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    if (std::isfinite(t.kwh_100km)) snprintf(num, sizeof(num), "%.1f", t.kwh_100km);
    else snprintf(num, sizeof(num), "--");
    ui_text(o.conso, num);
    ui_text_color(o.conso, UIColor.TEXT_PRIMARY);
    // Le bloc du kWh / 100 km est calé sur le bord droit de la colonne (comme le compteur au-dessus), à la
    // largeur de son texte : il n'est jamais coupé, et laisse aux kilomètres tout le reste de la ligne.
    const int32_t x_conso = std::max(x, x + col - std::max(ui_largeur_texte(o.conso, num), l_sous_c));
    ui_poser(o.conso, x_conso, y, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    // « estimé » : l'énergie chargée divisée par les kilomètres, pas une consommation mesurée.
    ui_text(o.conso_sous, sous_c);
    ui_text_color(o.conso_sous, UIColor.TEXT_DIM);
    ui_poser(o.conso_sous, x_conso, y + h_grand - recouvre, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    y += h_grand + lignes_sous_c * h_sous - recouvre + kEcartBloc;

    // --- Les kilomètres des sept derniers jours -----------------------------------------------
    ui_text(o.titre_km, tr("Km par jour"));
    ui_text_color(o.titre_km, UIColor.TEXT_SOFT);
    ui_poser(o.titre_km, x, y, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    y += h_titre + kEcartTitre + h_val;
    peindre_barres(o.barre_km, o.val_km, t.km, x, col, y + kBarresKmH, kBarresKmH, UIColor.GOLD, 0);
    y += kBarresKmH + kEcartJour;
    const int32_t pas = col / kJ;
    const int aujourdhui = jour_du_mois_aujourdhui();
    for (int i = 0; i < kJ; i++) {
        int mjour = 0, sjour = 0;
        if (!trajets_jour(t, i, mjour, sjour)) {
            ui_hidden(o.jour[i], true);
            continue;
        }
        snprintf(txt, sizeof(txt), "%s\n%d", day_short_utf8(sjour), mjour);   // le jour, puis son numéro
        ui_text(o.jour[i], txt);
        ui_text_color(o.jour[i], mjour == aujourdhui ? UIColor.INFO : UIColor.TEXT_DIM);
        ui_poser(o.jour[i], x + i * pas, y, pas, LV_SIZE_CONTENT);
    }
    y += h_jours;

    // --- Les kWh reçus par la voiture ces jours-là (le Tab5 seulement) ------------------------------
    if (!kAvecKwh) {
        ui_hidden(o.titre_kwh, true);
        for (int i = 0; i < kJ; i++) {
            ui_hidden(o.barre_kwh[i], true);
            ui_hidden(o.val_kwh[i], true);
        }
        return;
    }
    y += kEcartBloc;
    ui_text(o.titre_kwh, tr("kWh chargés par jour"));
    ui_text_color(o.titre_kwh, UIColor.TEXT_SOFT);
    ui_poser(o.titre_kwh, x, y, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    y += h_titre + kEcartTitre + h_val;
    peindre_barres(o.barre_kwh, o.val_kwh, t.kwh, x, col, y + kBarresKwhH, kBarresKwhH, c_rang, 1);
}

}  // namespace

bool voitures_trajets_a_donnees() {
    for (const TrajetsLu& t : s_t)
        if (t.presente) return true;
    return false;
}

void voitures_trajets_montrer(bool visible) {
    if (visible) creer();
    if (!s_cree || visible) return;   // visible : voitures_trajets_peindre() montre ce qu'il faut
    for (const Colonne& o : s_c) ui_hidden(o.fond, true);
}

// Les voitures qui ont des trajets, dans l'ordre des rangs : la seconde seule prend la première carte
// (comme l'Aperçu) ; deux voitures : deux cartes côte à côte, une seule : une carte pleine largeur.
void voitures_trajets_peindre() {
    creer();
    if (!s_cree) return;
    int rang[kVoit] = {};
    int n = 0;
    for (int r = 0; r < kVoit; r++)
        if (s_t[r].presente) rang[n++] = r;
    const int32_t w = n == 1 ? kCarteSeuleL : kCarteDeuxL;
    for (int c = 0; c < kVoit; c++) {
        if (c >= n) {
            ui_hidden(s_c[c].fond, true);
            continue;
        }
        peindre_colonne(c, rang[c], c * (kCarteDeuxL + kCartesEcart), w);
    }
}

// ADR-0083 : le payload de tab5_maj_voiture_trajets. Un payload vide retire la voiture de la page ; un rang
// inconnu est refusé.
void voitures_trajets_recu(const std::string& rang, const std::string& payload) {
    if (payload_trop_long(kTag, payload.size())) return;
    const int k = rang == "1" ? 0 : (rang == "2" ? 1 : -1);
    if (k < 0) {
        payload_refuse(kTag, "trajets : rang inconnu", rang.size());
        return;
    }
    TrajetsLu t;
    trajets_lire(Champ{payload.data(), payload.size()}, t);
    s_t[k] = t;
    voitures_rejouer_theme();   // repeint le popup s'il est ouvert (ses pages changent peut-être)
}
