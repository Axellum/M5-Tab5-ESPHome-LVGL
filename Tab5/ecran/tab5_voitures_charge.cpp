/**
 * [AI-CONTEXT]
 * @file tab5_voitures_charge.cpp
 * @role Page « Charge » du popup Voitures (ADR-0082, lot 8 « C2 » du plan des voitures), tout en C++,
 *       sur le conteneur du popup (voitures_corps()), aucun id YAML :
 *         - en haut, une carte par voiture qui a des heures de charge aujourd'hui : son nom, sa
 *           batterie en grand et en barre, la limite de charge (un trait), l'état de la charge et,
 *           en charge, l'heure d'arrivée ; la part encore à charger jusqu'à la limite est teintée ;
 *         - au milieu, la frise des 24 heures du jour dessinée avec des rectangles (comme la bande
 *           de la page Aujourd'hui, ADR-0078) : la prévision de production du soleil en barres,
 *           une ligne par voiture dont les plages vertes sont les heures où elle a chargé
 *           (plages_charge : tab5_maj_voiture_jour), le trait de l'heure actuelle, et, pour une
 *           voiture en charge, la fin de charge annoncée par la voiture elle-même (minutes) ;
 *         - en bas, le créneau CONSEILLÉ pour charger au soleil : creneau_solaire() (socle, testé
 *           sur PC) d'après la prévision de la page Aujourd'hui ; la phrase se compose ici avec
 *           tr(), et l'écran dit que c'est une suggestion d'après la prévision, jamais un fait.
 * @architecture_constraint Push-only et events-only (ADR-0001, ADR-0025) : aucun service nouveau. La
 *       page lit les deux que le popup Énergie reçoit déjà (tab5_maj_voiture_jour, ADR-0078, et
 *       tab5_maj_energie_soleil, ADR-0058), que tab5-api-logic.yaml donne aussi à ce fichier : il
 *       garde ses propres copies des séries (ConsoJour, EnergieSoleilLu : ~600 octets), sans toucher
 *       tab5_energie.cpp. Les voitures viennent de tab5_voitures.cpp (voiture_recue). Rien n'est
 *       inventé : l'état « branchée » des heures passées n'existe dans aucune donnée, la frise ne le
 *       dessine pas ; la fin prévue est celle que la voiture annonce ; la limite est celle de la voiture.
 *       Jamais sur la 7B (kChargePage : son corps est trop bas pour la frise, comme la consommation de
 *       la page Aujourd'hui, ADR-0078) : la page n'a alors aucune donnée, donc ni onglet ni widget.
 *       Séries d'un autre jour que le jour local (passé minuit, analyse en échec) : ignorées, la page
 *       disparaît plutôt que de montrer hier comme aujourd'hui. Couleurs par les rôles de la palette
 *       (ADR-0029), réécrites à chaque peinture (voitures_rejouer_theme() repeint) ; toute écriture est
 *       comparée d'abord (ui_text, ui_poser, ui_style_*).
 * @ai_warning Le créneau est une SUGGESTION fondée sur une prévision (une heure sans valeur n'y est
 *       jamais) : ne jamais l'écrire comme une mesure ni le montrer sans sa mention. Les heures de
 *       charge viennent de la série par heure de Home Assistant : l'heure en cours est partielle, la
 *       plage qui la contient est coupée au trait de l'heure actuelle (rien n'est dessiné dans le futur).
 * @ai_instruction Un widget de plus = son champ dans struct Page, sa ligne dans creer() et sa peinture
 *       dans peindre_*(). Un texte affiché passe par tr(). Le glyphe du conseil : glyphe_conseil()
 *       (MDI_CODE_TARGETS, tools/check_tab5_code_rules.py).
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include "tab5_creneau.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

namespace {

constexpr const char* kTag = "tab5.voitures";

// La page n'existe que sur un corps aussi grand que celui du Tab5 (1202 × 598) : la frise ne tient pas
// dans celui de la 7B (946 × 478).
constexpr bool kChargePage = kCorpsW >= kCorpsWTab5 && kCorpsH >= kCorpsHTab5;

// --- Géométrie (px du Tab5, relatifs au corps du popup, 1202 × 598) -------------------------------
constexpr int32_t kPad = 22;                                    // marge intérieure des cartes
constexpr int32_t kDemiCarteL = (kCorpsW - kCartesEcart) / 2;   // deux voitures : côte à côte
constexpr int32_t kResumeH = 184;                               // carte d'une voiture
constexpr int32_t kFriseY = kResumeH + kCartesEcart;
constexpr int32_t kFriseH = 318;
constexpr int32_t kConseilY = kFriseY + kFriseH + kCartesEcart;
constexpr int32_t kConseilH = kCorpsH - kConseilY;
static_assert(kConseilH >= 56 || !kMiseEnPageTab5, "la bande du conseil tient une ligne de 32 px");

constexpr int32_t kBarreH = 22;       // la barre de la batterie
constexpr int32_t kMarqueL = 3;       // le trait de la limite

constexpr int32_t kNomL = 150;                                          // colonne des noms de voiture
constexpr int32_t kTraceX = kPad + kNomL;                               // début des 24 heures
constexpr int32_t kHeureL = (kCorpsW - kPad - kTraceX) / kCreneauHeures;   // 42 px par heure
constexpr int32_t kTraceL = kHeureL * kCreneauHeures;
constexpr int32_t kTitreY = 14;
constexpr int32_t kBulleY = 52;       // l'heure actuelle, au-dessus du tracé
constexpr int32_t kBulleH = 30;
constexpr int32_t kGrilleHaut = kBulleY + kBulleH + 4;
constexpr int32_t kSolHaut = 100;     // hauteur du soleil prévu à son pic
constexpr int32_t kSolBase = kGrilleHaut + kSolHaut + 2;
constexpr int32_t kRangeeY = kSolBase + 12;   // première ligne de voiture
constexpr int32_t kRangeeH = 30;
constexpr int32_t kRangeePas = 38;
constexpr int32_t kGrilleBas = kRangeeY + kRangeePas + kRangeeH + 6;
constexpr int32_t kAxeY = kGrilleBas + 8;
constexpr int32_t kAxes = 8;          // un repère toutes les trois heures : 0 h, 3 h, … 21 h
constexpr int32_t kLegendes = 3;
static_assert(kAxeY + 26 <= kFriseH || !kMiseEnPageTab5, "les heures de l'axe tiennent dans la carte de la frise");

// --- Les widgets ----------------------------------------------------------------------------------
// Une carte de voiture : un verre et ses textes, la barre de la batterie en rectangles.
struct Carte {
    lv_obj_t* fond = nullptr;
    lv_obj_t* nom = nullptr;
    lv_obj_t* etat = nullptr;
    lv_obj_t* pct = nullptr;
    lv_obj_t* detail = nullptr;
    lv_obj_t* limite = nullptr;
    lv_obj_t* piste = nullptr;    // la barre vide
    lv_obj_t* prevu = nullptr;    // la part qu'il reste à charger jusqu'à la limite (en charge)
    lv_obj_t* part = nullptr;     // la batterie
    lv_obj_t* marque = nullptr;   // le trait de la limite
};

struct Page {
    lv_obj_t* racine = nullptr;
    Carte carte[kVoituresCartes];
    // La frise.
    lv_obj_t* frise = nullptr;
    lv_obj_t* titre = nullptr;
    lv_obj_t* leg_marque[kLegendes] = {};
    lv_obj_t* leg_texte[kLegendes] = {};
    lv_obj_t* grille[kAxes] = {};
    lv_obj_t* axe[kAxes] = {};
    lv_obj_t* base = nullptr;
    lv_obj_t* soleil_nom = nullptr;
    lv_obj_t* soleil[kCreneauHeures] = {};
    lv_obj_t* creneau = nullptr;
    lv_obj_t* rangee_nom[kVoituresCartes] = {};
    lv_obj_t* rangee_piste[kVoituresCartes] = {};
    lv_obj_t* rangee_prevue[kVoituresCartes] = {};
    lv_obj_t* plage[kVoituresCartes][kPlagesMax] = {};
    lv_obj_t* repere = nullptr;
    lv_obj_t* bulle = nullptr;
    lv_obj_t* bulle_texte = nullptr;
    // Le conseil.
    lv_obj_t* conseil = nullptr;
    lv_obj_t* conseil_icone = nullptr;
    lv_obj_t* conseil_texte = nullptr;
    lv_obj_t* conseil_note = nullptr;
    // Les polices, lues sur des labels du popup (un label C++ n'a pas d'id pour les prendre).
    const lv_font_t* f22 = nullptr;     // roboto_22
    const lv_font_t* f32 = nullptr;     // roboto_32_b
    const lv_font_t* f55 = nullptr;     // roboto_55_b
    const lv_font_t* ficone = nullptr;  // mdi_font_45
};
Page s_p;

// --- Les données reçues ----------------------------------------------------------------------------
ConsoJour s_conso;                // rang 0 : la maison, 1 et 2 : les voitures (tab5_maj_voiture_jour)
char s_debut[16] = "";            // AAAA-MM-JJ du jour de ces heures
EnergieSoleilLu s_sol;            // la prévision du jour (tab5_maj_energie_soleil)
bool s_sol_recu = false;

// L'instant présent ; 0 tant que l'horloge n'est pas réglée.
time_t instant_valide() {
    const time_t now = tab5_time_source(nullptr);
    return tab5_heure_valide(now) ? now : 0;
}

// Heure locale en minutes depuis minuit ; -1 tant que l'horloge n'est pas réglée.
int minutes_maintenant() {
    const time_t now = instant_valide();
    if (now == 0) return -1;
    struct tm t{};
    localtime_r(&now, &t);
    return t.tm_hour * 60 + t.tm_min;
}

// Les minutes qu'il reste à une voiture en charge : ce qu'elle annonçait quand la poussée est arrivée,
// moins le temps passé depuis (VoitureRecue::recue_le). NAN si elle ne charge pas, n'annonce rien (0 ou
// moins) ou si l'annonce est dépassée : une poussée ancienne ne se présente pas comme une fin de charge à
// venir. Horloge pas réglée à la poussée ou maintenant : l'annonce telle quelle, comme l'Aperçu.
float minutes_restantes(const VoitureRecue& v) {
    if (v.etat != VoitureEtat::EN_CHARGE || !std::isfinite(v.minutes) || v.minutes <= 0.0f) return NAN;
    const time_t now = instant_valide();
    if (now == 0 || v.recue_le == 0) return v.minutes;
    const float ecoule = std::max(0.0f, static_cast<float>(now - v.recue_le) / 60.0f);
    const float reste = v.minutes - ecoule;
    return reste > 0.0f ? reste : NAN;
}

// Le jour local « AAAA-MM-JJ » ; faux tant que l'horloge n'est pas réglée.
bool jour_local(char* out, size_t n) {
    const time_t now = instant_valide();
    if (now == 0) return false;
    struct tm t{};
    localtime_r(&now, &t);
    std::snprintf(out, n, "%04d-%02d-%02d", t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
    return true;
}

// La série du rang (1 ou 2) vaut pour aujourd'hui et a au moins une heure connue (un 0 est une donnée).
// Une série d'un autre jour (passé minuit, analyse en échec) ne vaut pas : hier ne passe pas pour
// aujourd'hui. Horloge non réglée : rien à comparer, la série est prise.
bool serie_utile(int rang) {
    if (rang < 1 || rang >= kConsoLignes || !s_conso.recue[rang]) return false;
    char auj[sizeof(s_debut)];
    if (jour_local(auj, sizeof(auj)) && std::strcmp(auj, s_debut) != 0) return false;
    for (int h = 0; h < kEnergieHeures; h++)
        if (std::isfinite(s_conso.kwh[rang][h])) return true;
    return false;
}

// Une voiture de la page : son rang (1 ou 2) et ce qu'on en sait.
struct Ligne {
    int rang = 0;
    const VoitureRecue* v = nullptr;
};

// Les voitures reçues qui ont des heures aujourd'hui, dans l'ordre des rangs (la seconde seule prend la
// première place, comme sur l'Aperçu).
int lignes(Ligne* out) {
    int n = 0;
    for (int k = 0; k < kVoituresCartes; k++) {
        const VoitureRecue& v = voiture_recue(k);
        if (v.presente && serie_utile(k + 1)) {
            out[n].rang = k + 1;
            out[n].v = &v;
            n++;
        }
    }
    return n;
}

// La prévision du soleil a au moins une heure : le créneau a de quoi se calculer.
bool prevision_connue() {
    if (!s_sol_recu) return false;
    for (int h = 0; h < s_sol.n_prevu; h++)
        if (std::isfinite(s_sol.prevu[h])) return true;
    return false;
}

// --- Briques --------------------------------------------------------------------------------------
int32_t haut(const lv_font_t* f) { return f != nullptr ? lv_font_get_line_height(f) : 0; }

// Un texte non cliquable, masqué ; sa largeur est posée à chaque peinture (ui_poser), son texte coupé
// plutôt que renvoyé à la ligne.
lv_obj_t* libelle(lv_obj_t* parent, const lv_font_t* f, lv_text_align_t align = LV_TEXT_ALIGN_LEFT) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, f, LV_PART_MAIN);
    lv_obj_set_style_text_align(l, align, LV_PART_MAIN);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
    lv_label_set_text(l, "");
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

// Un verre : la carte de verre des autres popups (style_glass_card : dégradé, arête lumineuse), faite
// ici en C++ ; ses couleurs suivent le thème (verre_couleurs).
lv_obj_t* verre_creer(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(o, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_main_stop(o, 25, LV_PART_MAIN);
    lv_obj_set_style_bg_grad_stop(o, 240, LV_PART_MAIN);
    lv_obj_set_style_border_width(o, 2, LV_PART_MAIN);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER * 55 / 100, LV_PART_MAIN);
    lv_obj_set_style_radius(o, 20, LV_PART_MAIN);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    return o;
}

void verre_couleurs(lv_obj_t* o) {
    ui_style_couleur(o, LV_STYLE_BG_COLOR, UIColor.GLASS_HI);
    ui_style_couleur(o, LV_STYLE_BG_GRAD_COLOR, UIColor.GLASS_LO);
    ui_style_couleur(o, LV_STYLE_BORDER_COLOR, UIColor.GLASS_RIM);
}

// Bordure d'un rectangle, écrite si elle change.
void contour(lv_obj_t* o, uint32_t couleur, int32_t largeur, lv_opa_t opa) {
    ui_style_couleur(o, LV_STYLE_BORDER_COLOR, couleur);
    ui_style_num(o, LV_STYLE_BORDER_WIDTH, largeur);
    ui_style_num(o, LV_STYLE_BORDER_OPA, opa);
}

// Un label posé à (x, y) sur `l` px de large (texte coupé par « … » au-delà).
void poser_texte(lv_obj_t* lbl, const char* txt, int32_t x, int32_t y, int32_t l) {
    texte_ha_coupe(lbl, txt, l);
    ui_poser(lbl, x, y, l, LV_SIZE_CONTENT);
}

// « 1 h 35 » ou « 42 min » : la durée d'une charge quand l'horloge n'est pas réglée.
void duree(char* out, size_t n, float minutes) {
    const long m = std::lround(minutes);
    if (m >= 60) std::snprintf(out, n, "%ld h %02ld", m / 60, m % 60);
    else std::snprintf(out, n, "%ld min", m);
}

// Le soleil du conseil (weather-sunny, mdi_font_45) : MDI_CODE_TARGETS (voitures_etat_icone_*, la même
// police que celle du label créé ici).
const char* glyphe_conseil() {
    return "\U000F0599";    // weather-sunny
}

// --- Construction (une fois, à la première montre) ---------------------------------------------------
void creer() {
    const VoituresUI& u = g_voitures_ui;
    lv_obj_t* parent = voitures_corps();
    if (s_p.racine != nullptr || parent == nullptr || u.sous[0] == nullptr || u.nom[0] == nullptr ||
        u.autonomie[0] == nullptr || u.etat_icone[0] == nullptr)
        return;
    s_p.f22 = lv_obj_get_style_text_font(u.sous[0], LV_PART_MAIN);
    s_p.f32 = lv_obj_get_style_text_font(u.nom[0], LV_PART_MAIN);
    s_p.f55 = lv_obj_get_style_text_font(u.autonomie[0], LV_PART_MAIN);
    s_p.ficone = lv_obj_get_style_text_font(u.etat_icone[0], LV_PART_MAIN);

    // Le conteneur de la page : le corps du popup. Cliquable (le glissement des pages le traverse).
    lv_obj_t* r = lv_obj_create(parent);
    lv_obj_remove_style_all(r);
    lv_obj_set_pos(r, kCorpsX, kCorpsY);
    lv_obj_set_size(r, kCorpsW, kCorpsH);
    lv_obj_remove_flag(r, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(r, LV_OBJ_FLAG_HIDDEN);
    s_p.racine = r;

    for (Carte& k : s_p.carte) {
        k.fond = verre_creer(r);
        k.nom = libelle(k.fond, s_p.f32);
        k.etat = libelle(k.fond, s_p.f22, LV_TEXT_ALIGN_RIGHT);
        k.pct = libelle(k.fond, s_p.f55);
        k.detail = libelle(k.fond, s_p.f22, LV_TEXT_ALIGN_RIGHT);
        k.piste = ui_rectangle(k.fond, LV_OPA_COVER, kBarreH / 2);
        k.prevu = ui_rectangle(k.fond, LV_OPA_40, kBarreH / 2);
        k.part = ui_rectangle(k.fond, LV_OPA_COVER, kBarreH / 2);
        k.marque = ui_rectangle(k.fond, LV_OPA_COVER, 1);
        k.limite = libelle(k.fond, s_p.f22);
    }

    lv_obj_t* f = verre_creer(r);
    s_p.frise = f;
    s_p.titre = libelle(f, s_p.f22);
    static const lv_opa_t kLegendeOpa[kLegendes] = {LV_OPA_COVER, LV_OPA_40, LV_OPA_20};
    for (int i = 0; i < kLegendes; i++) {
        s_p.leg_marque[i] = ui_rectangle(f, kLegendeOpa[i], 5);
        s_p.leg_texte[i] = libelle(f, s_p.f22);
    }
    for (int i = 0; i < kAxes; i++) {
        s_p.grille[i] = ui_rectangle(f, LV_OPA_30, 0);
        s_p.axe[i] = libelle(f, s_p.f22, LV_TEXT_ALIGN_CENTER);
    }
    s_p.base = ui_rectangle(f, LV_OPA_50, 0);
    s_p.soleil_nom = libelle(f, s_p.f22);
    for (lv_obj_t*& s : s_p.soleil) s = ui_rectangle(f, LV_OPA_40, 4);
    for (int c = 0; c < kVoituresCartes; c++) {
        s_p.rangee_nom[c] = libelle(f, s_p.f22);
        s_p.rangee_piste[c] = ui_rectangle(f, LV_OPA_COVER, kRangeeH / 2);
        s_p.rangee_prevue[c] = ui_rectangle(f, LV_OPA_40, kRangeeH / 2);
        for (lv_obj_t*& p : s_p.plage[c]) p = ui_rectangle(f, LV_OPA_COVER, kRangeeH / 2);
    }
    // Le cadre du créneau après les lignes : leurs pistes opaques ne coupent pas ses côtés (son fond, à
    // 10 %, teinte à peine ce qu'il recouvre).
    s_p.creneau = ui_rectangle(f, LV_OPA_10, 12);
    s_p.repere = ui_rectangle(f, LV_OPA_COVER, 1);
    s_p.bulle = ui_rectangle(f, LV_OPA_20, kBulleH / 2);
    s_p.bulle_texte = libelle(s_p.bulle, s_p.f22, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(s_p.bulle_texte, LV_ALIGN_CENTER, 0, 0);

    s_p.conseil = ui_rectangle(r, LV_OPA_10, 18);
    s_p.conseil_icone = libelle(s_p.conseil, s_p.ficone);
    s_p.conseil_texte = libelle(s_p.conseil, s_p.f32);
    s_p.conseil_note = libelle(s_p.conseil, s_p.f22, LV_TEXT_ALIGN_RIGHT);
}

// --- Peinture ---------------------------------------------------------------------------------------
// Une carte : la voiture `li` posée à x, sur `w` px de large.
void peindre_carte(int c, const Ligne& li, int32_t x, int32_t w) {
    Carte& k = s_p.carte[c];
    const VoitureRecue& v = *li.v;
    verre_couleurs(k.fond);
    ui_poser(k.fond, x, 0, w, kResumeH);
    const int32_t col = w - 2 * kPad;
    const int32_t h32 = haut(s_p.f32);
    const int32_t h22 = haut(s_p.f22);
    const int32_t h55 = haut(s_p.f55);
    const uint32_t c_etat = voiture_couleur_etat(v.etat);

    // Ligne 1 : le nom (à la couleur de son rang, comme l'Aperçu) et l'état, à droite.
    char nom[96];
    if (v.nom[0] != '\0') std::snprintf(nom, sizeof(nom), "%s", v.nom);
    else std::snprintf(nom, sizeof(nom), tr("Voiture %d"), li.rang);
    ui_text_color(k.nom, li.rang == 1 ? UIColor.ACCENT : UIColor.ACCENT_ALT);
    const int32_t wn = std::min(ui_largeur_texte(k.nom, nom) + 2, col * 55 / 100);
    const int32_t y1 = 10;
    poser_texte(k.nom, nom, kPad, y1, wn);
    char etat[64];
    voiture_texte_etat(etat, sizeof(etat), v);
    ui_text_color(k.etat, c_etat);
    const int32_t we = col - wn - 16;
    poser_texte(k.etat, etat, kPad + col - we, y1 + (h32 - h22) / 2, we);

    // Ligne 2 : la batterie en grand, à gauche ; à droite, la fin de la charge (l'heure d'arrivée, d'après
    // les minutes que la voiture annonce) ou « Pas de charge en cours ».
    const int32_t y2 = y1 + h32 - 2;
    char pct[16];
    if (std::isfinite(v.batterie)) std::snprintf(pct, sizeof(pct), "%.0f %%", v.batterie);
    else std::snprintf(pct, sizeof(pct), "--");
    ui_text(k.pct, pct);
    ui_text_color(k.pct, UIColor.TEXT_PRIMARY);
    ui_poser(k.pct, kPad, y2, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    char det[64] = "";
    const float reste = minutes_restantes(v);
    if (std::isfinite(reste)) {
        // L'heure d'arrivée seulement si c'est encore aujourd'hui ; sinon (ou sans horloge) la durée.
        const int maint = minutes_maintenant();
        const int arrivee = maint >= 0 ? maint + static_cast<int>(std::lround(reste)) : -1;
        if (arrivee >= 0 && arrivee < 24 * 60) {
            std::snprintf(det, sizeof(det), tr("Prête à %d h %02d"), arrivee / 60, arrivee % 60);
        } else {
            char d[24];
            duree(d, sizeof(d), reste);
            std::snprintf(det, sizeof(det), tr("Prête dans %s"), d);
        }
    } else if (v.etat == VoitureEtat::BRANCHEE) {
        std::snprintf(det, sizeof(det), "%s", tr("Pas de charge en cours"));
    }
    const bool a_det = det[0] != '\0';
    ui_text_color(k.detail, v.etat == VoitureEtat::EN_CHARGE ? UIColor.TEXT_PRIMARY : UIColor.TEXT_SOFT);
    ui_hidden(k.detail, !a_det);
    if (a_det) {
        const int32_t wd = col - ui_largeur_texte(k.pct, pct) - 16;
        poser_texte(k.detail, det, kPad + col - wd, y2 + (h55 - h22) / 2, wd);
    }

    // La barre de la batterie, le trait de la limite et ce qu'il reste à charger jusqu'à elle.
    const int32_t yb = y2 + h55 + 8;
    ui_style_couleur(k.piste, LV_STYLE_BG_COLOR, UIColor.ARC_TRACK);
    ui_poser(k.piste, kPad, yb, col, kBarreH);
    const bool a_batterie = std::isfinite(v.batterie);
    const float pct_f = a_batterie ? std::min(100.0f, std::max(0.0f, v.batterie)) : 0.0f;
    const int32_t wf = static_cast<int32_t>(std::lround(col * pct_f / 100.0f));
    ui_hidden(k.part, !a_batterie || wf < 1);
    if (a_batterie && wf >= 1) {
        ui_style_couleur(k.part, LV_STYLE_BG_COLOR, get_battery_color(v.batterie));
        ui_poser(k.part, kPad, yb, wf, kBarreH);
    }
    const bool a_limite = std::isfinite(v.limite);
    const float lim_f = a_limite ? std::min(100.0f, std::max(0.0f, v.limite)) : 0.0f;
    const int32_t xl = static_cast<int32_t>(std::lround(col * lim_f / 100.0f));
    const bool reste = a_limite && a_batterie && v.etat == VoitureEtat::EN_CHARGE && lim_f > pct_f;
    ui_hidden(k.prevu, !reste);
    if (reste) {
        ui_style_couleur(k.prevu, LV_STYLE_BG_COLOR, UIColor.SUCCESS);
        ui_poser(k.prevu, kPad, yb, xl, kBarreH);
    }
    ui_hidden(k.marque, !a_limite);
    ui_hidden(k.limite, !a_limite);
    if (a_limite) {
        ui_style_couleur(k.marque, LV_STYLE_BG_COLOR, UIColor.TEXT_PRIMARY);
        ui_poser(k.marque, kPad + xl - kMarqueL / 2, yb - 6, kMarqueL, kBarreH + 12);
        char lim[32];
        std::snprintf(lim, sizeof(lim), tr("Limite %d %%"), static_cast<int>(std::lround(v.limite)));
        const int32_t wt = ui_largeur_texte(k.limite, lim);
        const int32_t xt = std::min(std::max(kPad + xl - wt / 2, kPad), kPad + col - wt);
        ui_text_color(k.limite, UIColor.TEXT_DIM);
        poser_texte(k.limite, lim, xt, yb + kBarreH + 8, wt + 4);
    }
}

// La frise des 24 heures du jour.
void peindre_frise(const Ligne* l, int n, int maint, const CreneauSolaire& cr) {
    Page& p = s_p;
    verre_couleurs(p.frise);
    ui_poser(p.frise, 0, kFriseY, kCorpsW, kFriseH);
    const int32_t h22 = haut(p.f22);
    const int32_t nowx = maint >= 0 ? kTraceX + static_cast<int32_t>(static_cast<int64_t>(maint) * kTraceL / (24 * 60)) : -1;

    // Titre.
    char titre[64];
    std::snprintf(titre, sizeof(titre), "%s", tr("Aujourd'hui, heure par heure"));
    texte_majuscules(titre);
    ui_text(p.titre, titre);
    ui_text_color(p.titre, UIColor.TEXT_SOFT);
    ui_poser(p.titre, kPad, kTitreY, LV_SIZE_CONTENT, LV_SIZE_CONTENT);

    // Les voitures : leurs lignes, plus bas ; ici d'abord ce qui en dit la légende.
    bool a_prevue = false;
    for (int c = 0; c < kVoituresCartes; c++) {
        const bool ligne = c < n;
        ui_hidden(p.rangee_nom[c], !ligne);
        ui_hidden(p.rangee_piste[c], !ligne);
        if (!ligne) {
            ui_hidden(p.rangee_prevue[c], true);
            for (lv_obj_t* pl : p.plage[c]) ui_hidden(pl, true);
            continue;
        }
        const int32_t ry = kRangeeY + c * kRangeePas;
        const VoitureRecue& v = *l[c].v;
        char nom[96];
        if (v.nom[0] != '\0') std::snprintf(nom, sizeof(nom), "%s", v.nom);
        else std::snprintf(nom, sizeof(nom), tr("Voiture %d"), l[c].rang);
        ui_text_color(p.rangee_nom[c], l[c].rang == 1 ? UIColor.ACCENT : UIColor.ACCENT_ALT);
        poser_texte(p.rangee_nom[c], nom, kPad, ry + (kRangeeH - h22) / 2, kNomL - 10);
        ui_style_couleur(p.rangee_piste[c], LV_STYLE_BG_COLOR, UIColor.ARC_TRACK);
        ui_poser(p.rangee_piste[c], kTraceX, ry, kTraceL, kRangeeH);

        // Les heures où la voiture a chargé ; la plage qui contient l'heure en cours s'arrête au trait
        // de l'heure actuelle : l'heure est partielle, rien n'est dessiné dans le futur.
        Plage pl[kPlagesMax];
        const int np = plages_charge(s_conso.kwh[l[c].rang], kEnergieHeures, pl, kPlagesMax);
        for (int i = 0; i < kPlagesMax; i++) {
            int32_t x0 = 0;
            int32_t x1 = 0;
            if (i < np) {
                x0 = kTraceX + pl[i].debut * kHeureL;
                x1 = kTraceX + pl[i].fin * kHeureL;
                if (maint >= 0 && pl[i].debut <= maint / 60 && maint / 60 < pl[i].fin) x1 = std::min(x1, nowx);
            }
            const bool montre = i < np && x1 - x0 >= 4;
            ui_hidden(p.plage[c][i], !montre);
            if (montre) {
                ui_style_couleur(p.plage[c][i], LV_STYLE_BG_COLOR, UIColor.SUCCESS);
                ui_poser(p.plage[c][i], x0, ry, x1 - x0, kRangeeH);
            }
        }
        // La fin de la charge que la voiture annonce : de maintenant à maintenant + minutes.
        bool prevue = false;
        const float reste = minutes_restantes(v);
        if (std::isfinite(reste) && nowx >= 0) {
            const int32_t x1 = std::min<int32_t>(nowx + static_cast<int32_t>(std::lround(reste * kTraceL / (24.0f * 60.0f))),
                                                 kTraceX + kTraceL);
            if (x1 - nowx >= 4) {
                prevue = true;
                ui_style_couleur(p.rangee_prevue[c], LV_STYLE_BG_COLOR, UIColor.SUCCESS);
                ui_poser(p.rangee_prevue[c], nowx, ry, x1 - nowx, kRangeeH);
            }
        }
        ui_hidden(p.rangee_prevue[c], !prevue);
        a_prevue = a_prevue || prevue;
    }

    // La légende, à droite du titre : « En charge », « Fin prévue » (s'il y en a une), « Créneau conseillé ».
    const char* mots[kLegendes] = {tr("En charge"), tr("Fin prévue"), tr("Créneau conseillé")};
    const bool montre_leg[kLegendes] = {true, a_prevue, cr.valide()};
    int32_t xd = kCorpsW - kPad;
    for (int i = kLegendes - 1; i >= 0; i--) {
        ui_hidden(p.leg_marque[i], !montre_leg[i]);
        ui_hidden(p.leg_texte[i], !montre_leg[i]);
        if (!montre_leg[i]) continue;
        const int32_t wt = ui_largeur_texte(p.leg_texte[i], mots[i]) + 4;
        const int32_t y = kTitreY + 2;
        ui_text_color(p.leg_texte[i], UIColor.TEXT_SOFT);
        poser_texte(p.leg_texte[i], mots[i], xd - wt, y, wt);
        ui_style_couleur(p.leg_marque[i], LV_STYLE_BG_COLOR, i == 2 ? UIColor.GOLD : UIColor.SUCCESS);
        if (i == 2) contour(p.leg_marque[i], UIColor.GOLD, 2, LV_OPA_COVER);
        ui_poser(p.leg_marque[i], xd - wt - 8 - 16, y + (h22 - 16) / 2, 16, 16);
        xd -= wt + 8 + 16 + 26;
    }

    // La grille et l'axe : un repère toutes les trois heures.
    for (int i = 0; i < kAxes; i++) {
        const int32_t x = kTraceX + i * 3 * kHeureL;
        ui_style_couleur(p.grille[i], LV_STYLE_BG_COLOR, UIColor.GLASS_RIM);
        ui_poser(p.grille[i], x, kGrilleHaut, 1, kGrilleBas - kGrilleHaut);
        char h[16];
        std::snprintf(h, sizeof(h), tr("%d h"), i * 3);
        ui_text(p.axe[i], h);
        ui_text_color(p.axe[i], UIColor.TEXT_DIM);
        ui_poser(p.axe[i], x - 42, kAxeY, 84, LV_SIZE_CONTENT);
    }
    ui_style_couleur(p.base, LV_STYLE_BG_COLOR, UIColor.GLASS_RIM);
    ui_poser(p.base, kTraceX, kSolBase, kTraceL, 1);

    // Le soleil prévu, une barre par heure, à l'échelle de son pic.
    float maxi = 0.0f;
    const bool a_sol = prevision_connue();
    if (a_sol)
        for (int h = 0; h < s_sol.n_prevu; h++)
            if (std::isfinite(s_sol.prevu[h]) && s_sol.prevu[h] > maxi) maxi = s_sol.prevu[h];
    for (int h = 0; h < kCreneauHeures; h++) {
        const bool a_h = a_sol && maxi > 0.0f && h < s_sol.n_prevu && std::isfinite(s_sol.prevu[h]);
        ui_hidden(p.soleil[h], !a_h);
        if (!a_h) continue;
        const int32_t hb = std::max<int32_t>(2, static_cast<int32_t>(std::lround(s_sol.prevu[h] / maxi * kSolHaut)));
        ui_style_couleur(p.soleil[h], LV_STYLE_BG_COLOR, UIColor.GOLD);
        ui_poser(p.soleil[h], kTraceX + h * kHeureL + 4, kSolBase - hb, kHeureL - 8, hb);
    }
    ui_hidden(p.soleil_nom, !a_sol || maxi <= 0.0f);
    if (a_sol && maxi > 0.0f) {
        ui_text_color(p.soleil_nom, UIColor.GOLD);
        // « Prévision », jamais « Soleil » seul : ces barres sont ce que la prévision promet, y compris
        // pour les heures déjà passées, pas une production mesurée.
        poser_texte(p.soleil_nom, tr("Prévision"), kPad, kSolBase - h22 - 4, kNomL - 10);
    }

    // Le créneau conseillé : un cadre doré sur les heures, du haut du soleil au bas des lignes.
    ui_hidden(p.creneau, !cr.valide());
    if (cr.valide()) {
        const int32_t x0 = kTraceX + cr.debut * kHeureL;
        const int32_t x1 = kTraceX + cr.fin * kHeureL;
        ui_style_couleur(p.creneau, LV_STYLE_BG_COLOR, UIColor.GOLD);
        contour(p.creneau, UIColor.GOLD, 2, LV_OPA_COVER);
        ui_poser(p.creneau, x0, kGrilleHaut - 2, x1 - x0, kGrilleBas - kGrilleHaut + 4);
    }

    // L'heure actuelle : un trait et, au-dessus, l'heure dans une bulle (horloge réglée seulement).
    ui_hidden(p.repere, maint < 0);
    ui_hidden(p.bulle, maint < 0);
    if (maint >= 0) {
        ui_style_couleur(p.repere, LV_STYLE_BG_COLOR, UIColor.TEXT_PRIMARY);
        ui_poser(p.repere, nowx - 1, kBulleY + kBulleH, 2, kGrilleBas - kBulleY - kBulleH);
        char h[24];
        std::snprintf(h, sizeof(h), tr("%d h %02d"), maint / 60, maint % 60);
        ui_text(p.bulle_texte, h);
        ui_text_color(p.bulle_texte, UIColor.INFO);
        ui_hidden(p.bulle_texte, false);
        const int32_t wb = ui_largeur_texte(p.bulle_texte, h) + 28;
        const int32_t xb = std::min(std::max(nowx - wb / 2, kTraceX - 20), kCorpsW - kPad - wb);
        ui_style_couleur(p.bulle, LV_STYLE_BG_COLOR, UIColor.INFO);
        contour(p.bulle, UIColor.INFO, 2, LV_OPA_COVER);
        ui_poser(p.bulle, xb, kBulleY, wb, kBulleH);
    }
}

// Le conseil : le créneau CONSEILLÉ et, toujours, la mention que c'est une suggestion d'après la prévision.
void peindre_conseil(const CreneauSolaire& cr, bool prevision) {
    Page& p = s_p;
    const bool ok = cr.valide();
    const uint32_t c = ok ? UIColor.GOLD : UIColor.TEXT_DIM;
    ui_style_couleur(p.conseil, LV_STYLE_BG_COLOR, c);
    contour(p.conseil, ok ? UIColor.GOLD : UIColor.ARC_TRACK, 2, LV_OPA_COVER);
    ui_poser(p.conseil, 0, kConseilY, kCorpsW, kConseilH);

    char txt[96];
    const char* note = "";
    if (!prevision) {
        std::snprintf(txt, sizeof(txt), "%s", tr("Pas de prévision du soleil reçue"));
    } else if (ok) {
        if (cr.en_cours) std::snprintf(txt, sizeof(txt), tr("Créneau conseillé : jusqu'à %d h"), cr.fin);
        else std::snprintf(txt, sizeof(txt), tr("Créneau conseillé : %d h – %d h"), cr.debut, cr.fin);
        note = tr("Suggestion d'après la prévision du soleil");
    } else {
        std::snprintf(txt, sizeof(txt), "%s", tr("Pas de créneau conseillé aujourd'hui"));
        note = tr("Suggestion d'après la prévision du soleil");
    }

    const int32_t h_icone = haut(p.ficone);
    ui_text(p.conseil_icone, glyphe_conseil());
    ui_text_color(p.conseil_icone, c);
    ui_poser(p.conseil_icone, kPad, (kConseilH - h_icone) / 2 - 2, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    const int32_t xt = kPad + ui_largeur_texte(p.conseil_icone, glyphe_conseil()) + 16;
    ui_text_color(p.conseil_texte, c);
    const int32_t wt = ui_largeur_texte(p.conseil_texte, txt) + 4;
    poser_texte(p.conseil_texte, txt, xt, (kConseilH - haut(p.f32)) / 2 - 2, wt);

    const bool a_note = note[0] != '\0';
    ui_hidden(p.conseil_note, !a_note);
    if (a_note) {
        const int32_t reste = kCorpsW - kPad - (xt + wt + 24);
        ui_text_color(p.conseil_note, UIColor.TEXT_DIM);
        poser_texte(p.conseil_note, note, kCorpsW - kPad - std::max<int32_t>(reste, 0), (kConseilH - haut(p.f22)) / 2 - 2,
                    std::max<int32_t>(reste, 0));
    }
}

}  // namespace

// --- API publique -----------------------------------------------------------------------------------
bool voitures_charge_a_donnees() {
    if (!kChargePage) return false;
    Ligne l[kVoituresCartes];
    return lignes(l) > 0;
}

void voitures_charge_montrer(bool visible) {
    if (!kChargePage) return;
    if (visible) creer();
    ui_hidden(s_p.racine, !visible);
}

void voitures_charge_peindre() {
    if (!kChargePage || s_p.racine == nullptr) return;
    Ligne l[kVoituresCartes];
    const int n = lignes(l);
    if (n == 0) return;
    const int maint = minutes_maintenant();
    const bool prevision = prevision_connue();
    const CreneauSolaire cr = prevision ? creneau_solaire(s_sol.prevu, s_sol.n_prevu, maint) : CreneauSolaire{};
    const int32_t w = n == 1 ? kCorpsW : kDemiCarteL;
    for (int c = 0; c < kVoituresCartes; c++) {
        if (c < n) peindre_carte(c, l[c], c * (kDemiCarteL + kCartesEcart), w);
        else ui_hidden(s_p.carte[c].fond, true);
    }
    peindre_frise(l, n, maint, cr);
    peindre_conseil(cr, prevision);
}

// ADR-0078 : la série d'un rang, 24 kWh par heure (conso_jour_lire, tab5_parse.h). Un autre jour (début)
// repart de zéro : les heures d'hier ne se mêlent pas à celles d'aujourd'hui. Un rang inconnu est refusé.
void voitures_charge_jour(const std::string& rang, const std::string& debut, const std::string& valeurs) {
    if (!kChargePage) return;   // 7B : la page n'existe pas, rien à garder
    if (payload_trop_long(kTag, valeurs.size())) return;
    ConsoJour lu;
    if (!conso_jour_lire(Champ{rang.data(), rang.size()}, Champ{valeurs.data(), valeurs.size()}, lu)) {
        payload_refuse(kTag, "voiture_jour : rang inconnu", rang.size());
        return;
    }
    const int r = rang[0] - '0';
    char jour[sizeof(s_debut)] = {};
    std::snprintf(jour, sizeof(jour), "%.*s", static_cast<int>(sizeof(jour) - 1), debut.c_str());
    if (std::strcmp(jour, s_debut) != 0) {
        s_conso = ConsoJour{};
        std::memcpy(s_debut, jour, sizeof(s_debut));
    }
    s_conso.recue[r] = lu.recue[r];
    std::memcpy(s_conso.kwh[r], lu.kwh[r], sizeof(s_conso.kwh[r]));
    voitures_repeindre();
}

// ADR-0058 : la prévision du soleil (energie_soleil_lire, tab5_parse.h), dont le créneau se calcule.
void voitures_charge_soleil(const std::string& payload) {
    if (!kChargePage) return;
    if (payload_trop_long(kTag, payload.size())) return;
    s_sol = EnergieSoleilLu{};
    s_sol_recu = energie_soleil_lire(Champ{payload.data(), payload.size()}, s_sol);
    voitures_repeindre();
}
