/**
 * [AI-CONTEXT]
 * @file tab5_tuiles_cases.cpp
 * @role Cases des tuiles coupées (mode HA et pages de pièce de la météo, qui dessinent sur le
 *       même calque ; ADR-0066, lot B, 18/10/2026) : au-delà de cinq
 *       éléments, les dernières places de la pièce sont coupées en deux (haut / bas) puis en
 *       quatre (Tab5/socle/tab5_disposition.h). Une case = un élément, sa propre cible :
 *         - une moitié (230 × 135 sur le Tab5), façon carte « tile » de HA couchée : l'icône
 *           de 45 px à gauche, le nom en 22 px et la valeur en 32 px à droite (en 22 px si elle
 *           ne tient pas, coupée avec « … » au-delà) ;
 *         - un quart (113 × 135) : l'icône de 32 px, la valeur en 22 px dessous, et le nom
 *           sous elle s'il tient en hauteur (coupé avec « … » en largeur).
 *       Pas de pastille : un élément montre son état par la couleur de son icône, celle de la
 *       pastille d'une carte (vue_def) ; la valeur prend la couleur de la ligne d'état.
 *       Appui court et appui long : ceux d'une tuile entière du même type (tuile_appui_piece :
 *       kGestes, options, confirmation, roue ancrée sur l'icône de la case) ; un appui au
 *       bout d'un glissement n'ouvre rien (ui_appui_glisse), le glissement reste celui des
 *       pièces.
 * @architecture_constraint Objets créés ici, en C++ (pas de lv_* dans un YAML), sur le calque
 *       du mode HA (g_tuiles_ui.calque_ha), à la première coupe de leur place seulement : une
 *       maison de cinq éléments par pièce n'en crée aucun. Fond et bordure : le style des
 *       cartes (style_meteo_card_page, partagé, qu'un thème redessine) ; couleurs du texte et
 *       des icônes : la palette active (UIColor), repeintes par tuiles_appliquer_ui (thème,
 *       définitions) et peindre_tuile (état). Géométrie : celle des cartes
 *       (kAccTuileL × kAccTuileH à y kAccTuileY, tab5_geometrie.h) découpée par
 *       disposition::case_rect, marges du Tab5 ramenées à l'écran de la carte : la Waveshare
 *       7B suit. Écritures comparées d'abord (ui_text, ui_x…), la mise en page fixe d'une case
 *       seulement quand la coupe de sa place change.
 * @ai_instruction Les icônes sont celles de la palette des tuiles (tuile_icone, par vue_def),
 *       présentes en mdi_font_45 et mdi_font_32 (tools/gen_tuiles_icones.py) ; le mode
 *       héritage (pièce 0 des emplacements 3.x, cinq tuiles au plus) n'est jamais coupé.
 *       Un texte affiché vient de vue_def (déjà traduit) ou de HA (nom, filtré aux glyphes).
 */
#include "tab5_tuiles_priv.h"
#include "tab5_disposition.h"
#include "tab5_geometrie.h"
#include "lvgl.h"
#include <cstdint>

namespace tuiles {

namespace {

using disposition::Coupe;

// Mise en page d'une case, valeurs du Tab5 ramenées à l'écran de la carte.
constexpr int32_t kMargeMoitie = acc_x(14);  // bords gauche et droit d'une moitié
constexpr int32_t kEcartIcone = acc_x(10);   // entre l'icône d'une moitié et ses textes
constexpr int32_t kMargeQuart = acc_x(6);    // bords d'un quart
constexpr int32_t kInterligne = acc_y(2);    // entre deux lignes de texte
constexpr int32_t kSousIcone = acc_y(4);     // entre l'icône d'un quart et sa valeur

struct Case {
    lv_obj_t* cadre = nullptr;   // fond de carte, la cible du toucher
    lv_obj_t* icone = nullptr;
    lv_obj_t* valeur = nullptr;  // la ligne d'état de la carte (« 21.4 °C », « Allumé »…)
    lv_obj_t* nom = nullptr;
};

struct PlaceCases {
    Case c[disposition::kCasesMax];
    Coupe coupe = Coupe::VIDE;  // coupe de la dernière mise en page
    bool creee = false;
};
PlaceCases s_places[kPlacesTuiles];

const lv_font_t* police_lv(esphome::font::Font* f) { return f != nullptr ? f->get_lv_font() : nullptr; }

int32_t hauteur(esphome::font::Font* f, int32_t defaut) {
    const lv_font_t* lf = police_lv(f);
    return lf != nullptr ? lv_font_get_line_height(lf) : defaut;
}

int32_t largeur(const char* txt, esphome::font::Font* f) {
    const lv_font_t* lf = police_lv(f);
    if (txt == nullptr || txt[0] == '\0' || lf == nullptr) return 0;
    lv_point_t pt;
    lv_text_get_size(&pt, txt, lf, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return pt.x;
}

// Police d'un libellé, écrite seulement si elle change (ui_police écrit toujours, et un
// lv_obj_set_style_* invalide même à valeur égale) : la valeur d'une moitié passe de 32 à
// 22 px selon sa longueur, repeinte à chaque état.
void police(lv_obj_t* o, esphome::font::Font* f) {
    if (o == nullptr || f == nullptr) return;
    lv_style_value_t v;
    if (lv_obj_get_local_style_prop(o, LV_STYLE_TEXT_FONT, &v, LV_PART_MAIN) == LV_STYLE_RES_FOUND &&
        v.ptr == police_lv(f))
        return;
    ui_police(o, f);
}

void case_appui(int p, int k, bool long_appui);

// Toucher d'une case : appui court ou long, jamais au bout d'un glissement.
void rappel(lv_event_t* ev) {
    if (ui_appui_glisse()) return;
    const int id = static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(ev)));
    case_appui(id / disposition::kCasesMax, id % disposition::kCasesMax, lv_event_get_code(ev) == LV_EVENT_LONG_PRESSED);
}

lv_obj_t* libelle(lv_obj_t* parent) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, "");
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    return l;
}

// Les quatre cases de la place `p`, masquées, sur le calque du mode HA (une fois).
void creer(int p) {
    PlaceCases& pc = s_places[p];
    const TuilesUI& u = g_tuiles_ui;
    if (pc.creee || u.calque_ha == nullptr) return;
    pc.creee = true;
    for (int k = 0; k < disposition::kCasesMax; k++) {
        Case& c = pc.c[k];
        c.cadre = lv_obj_create(u.calque_ha);
        if (u.style_case != nullptr) lv_obj_add_style(c.cadre, u.style_case, LV_PART_MAIN);
        lv_obj_remove_flag(c.cadre, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_remove_flag(c.cadre, LV_OBJ_FLAG_CLICK_FOCUSABLE);  // pas d'état « focus » après un appui
        lv_obj_set_scrollbar_mode(c.cadre, LV_SCROLLBAR_MODE_OFF);
        lv_obj_add_flag(c.cadre, LV_OBJ_FLAG_HIDDEN);
        void* id = reinterpret_cast<void*>(static_cast<intptr_t>(p * disposition::kCasesMax + k));
        lv_obj_add_event_cb(c.cadre, rappel, LV_EVENT_SHORT_CLICKED, id);
        lv_obj_add_event_cb(c.cadre, rappel, LV_EVENT_LONG_PRESSED, id);
        c.icone = libelle(c.cadre);
        c.nom = libelle(c.cadre);
        c.valeur = libelle(c.cadre);
    }
}

// Mise en page fixe d'une case pour sa coupe : polices de l'icône et du nom, alignements,
// largeurs. Appelée quand la coupe de la place change (lv_obj_set_style_* invalide même à
// valeur égale).
void mettre_en_page(const Case& c, Coupe coupe, int32_t w) {
    const TuilesUI& u = g_tuiles_ui;
    const bool moitie = coupe == Coupe::MOITIES;
    police(c.icone, moitie ? u.police_icone_45 : u.police_icone_32);
    police(c.nom, u.police_petite);
    const lv_text_align_t a = moitie ? LV_TEXT_ALIGN_LEFT : LV_TEXT_ALIGN_CENTER;
    for (lv_obj_t* l : {c.icone, c.nom, c.valeur}) lv_obj_set_style_text_align(l, a, LV_PART_MAIN);
    if (moitie) {
        const int32_t x_texte = kMargeMoitie + hauteur(u.police_icone_45, 45) + kEcartIcone;
        lv_obj_set_width(c.icone, LV_SIZE_CONTENT);
        ui_x(c.icone, kMargeMoitie);
        for (lv_obj_t* l : {c.nom, c.valeur}) {
            lv_obj_set_width(l, w - x_texte - kMargeMoitie);
            ui_x(l, x_texte);
        }
        return;
    }
    for (lv_obj_t* l : {c.icone, c.nom, c.valeur}) {
        lv_obj_set_width(l, w - 2 * kMargeQuart);
        ui_x(l, kMargeQuart);
    }
}

// L'élément tRT dans la case `c` de `w` × `h` px, coupée comme `coupe` (vue_def : les mots,
// l'icône et les couleurs de sa carte).
void peindre(const Case& c, Coupe coupe, int32_t w, int32_t h, int r, int t) {
    const TuilesUI& u = g_tuiles_ui;
    Vue v;
    vue(r, t, v);
    if (v.icone_carte != nullptr) ui_text(c.icone, v.icone_carte);
    ui_text_color(c.icone, v.couleur_carte);
    ui_text_color(c.nom, UIColor.TEXT_SOFT);
    ui_text_color(c.valeur, v.couleur_ligne);
    if (coupe == Coupe::MOITIES) {
        // L'icône à gauche, au milieu ; à droite le nom puis la valeur, centrés ensemble. La
        // valeur en 32 px si elle tient, sinon en 22 px, coupée au-delà.
        const int32_t hi = hauteur(u.police_icone_45, 45);
        const int32_t l_texte = w - (kMargeMoitie + hi + kEcartIcone) - kMargeMoitie;
        esphome::font::Font* fv = largeur(v.ligne, u.police_valeur) <= l_texte ? u.police_valeur : u.police_petite;
        police(c.valeur, fv);
        const int32_t hn = hauteur(u.police_petite, 26), hv = hauteur(fv, 26);
        const int32_t y = (h - (hn + kInterligne + hv)) / 2;
        ui_y(c.icone, (h - hi) / 2);
        ui_y(c.nom, y);
        ui_y(c.valeur, y + hn + kInterligne);
        ui_hidden(c.nom, false);
        ui_texte_coupe(c.nom, v.nom, l_texte);
        ui_texte_coupe(c.valeur, v.ligne, l_texte);
        return;
    }
    // Un quart : l'icône, la valeur, et le nom s'il tient en hauteur ; le tout centré.
    police(c.valeur, u.police_petite);
    const int32_t hi = hauteur(u.police_icone_32, 32), ht = hauteur(u.police_petite, 26);
    const int32_t sans_nom = hi + kSousIcone + ht;
    const bool avec_nom = sans_nom + kInterligne + ht + 2 * kMargeQuart <= h;
    const int32_t y = (h - (avec_nom ? sans_nom + kInterligne + ht : sans_nom)) / 2;
    const int32_t l_texte = w - 2 * kMargeQuart;
    ui_y(c.icone, y);
    ui_y(c.valeur, y + hi + kSousIcone);
    ui_y(c.nom, y + sans_nom + kInterligne);
    ui_hidden(c.nom, !avec_nom);
    if (avec_nom) ui_texte_coupe(c.nom, v.nom, l_texte);
    ui_texte_coupe(c.valeur, v.ligne, l_texte);
}

disposition::Rect rect_case(Coupe coupe, int k) {
    return disposition::case_rect(coupe, k, kAccTuileL, kAccTuileH, kAccCaseEcart);
}

// Toucher d'une case : l'élément de cette case dans la pièce affichée (mode HA, ou page de pièce
// de la météo, ADR-0066 lot A : le même calque), par le
// chemin d'une tuile entière (même commande, même confirmation, même roue, même popup).
void case_appui(int p, int k, bool long_appui) {
    if (!cartes_affichees() || p < 0 || p >= kPlacesTuiles || k < 0 || k >= disposition::kCasesMax) return;
    const int r = piece_courante();
    const disposition::Place& pl = disposition_piece(r, false).place[p];
    if (k >= disposition::cases_de(pl.coupe) || pl.coupe == Coupe::ENTIERE || pl.element[k] < 0) return;
    tuile_appui_piece(r, pl.element[k], long_appui);
}

}  // namespace

void cases_peindre_place(int p, const disposition::Place& pl, int32_t x, int r) {
    if (p < 0 || p >= kPlacesTuiles) return;
    creer(p);
    PlaceCases& pc = s_places[p];
    if (!pc.creee) return;
    const bool nouvelle = pc.coupe != pl.coupe;
    pc.coupe = pl.coupe;
    const int n = disposition::cases_de(pl.coupe);
    for (int k = 0; k < disposition::kCasesMax; k++) {
        const Case& c = pc.c[k];
        const bool montree = k < n && pl.element[k] >= 0;
        ui_hidden(c.cadre, !montree);
        if (k >= n) continue;
        const disposition::Rect rc = rect_case(pl.coupe, k);
        // Toutes les cases de la coupe, même vides : le dernier quart d'une pièce de 19
        // éléments se remplit à 20 (ou dans une autre pièce de même coupe) sans que la
        // coupe change, et il doit déjà avoir ses polices et ses alignements.
        if (nouvelle) mettre_en_page(c, pl.coupe, rc.w);
        if (!montree) continue;
        ui_x(c.cadre, x + rc.x);
        ui_y(c.cadre, kAccTuileY + rc.y);
        lv_obj_set_size(c.cadre, rc.w, rc.h);  // LVGL 9.5.0 compare lui-même (lv_obj_pos.c)
        peindre(c, pl.coupe, rc.w, rc.h, r, pl.element[k]);
    }
}

void cases_masquer(int p) {
    if (p < 0 || p >= kPlacesTuiles || !s_places[p].creee) return;
    for (const Case& c : s_places[p].c) ui_hidden(c.cadre, true);
}

void case_peindre(int p, int k, int r, int t) {
    if (p < 0 || p >= kPlacesTuiles || k < 0 || k >= disposition::kCasesMax) return;
    const PlaceCases& pc = s_places[p];
    if (!pc.creee || k >= disposition::cases_de(pc.coupe) || pc.coupe == Coupe::ENTIERE) return;
    const disposition::Rect rc = rect_case(pc.coupe, k);
    peindre(pc.c[k], pc.coupe, rc.w, rc.h, r, t);
}

lv_obj_t* case_ancre(int p, int k) {
    if (p < 0 || p >= kPlacesTuiles || k < 0 || k >= disposition::kCasesMax || !s_places[p].creee) return nullptr;
    return s_places[p].c[k].icone;
}

}  // namespace tuiles
