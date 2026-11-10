/**
 * [AI-CONTEXT]
 * @file tab5_voitures.cpp
 * @role Popup « Voitures » (ADR-0079, lot 6 « C1 » du plan des voitures) : peint, d'après ce que
 *       pousse tab5_maj_voiture (ADR-0076, lu par voiture_lire(), Tab5/socle/tab5_parse.h), une
 *       grande carte par voiture reçue (voitures_carte.yaml) : le nom, la silhouette (le glyphe
 *       car-side de mdi_font_voiture, teinté par l'état : décision D5 du plan, pas d'image en flash),
 *       l'anneau de la batterie et son pourcentage, l'autonomie, une ligne d'état courte (icône +
 *       « En charge · 7.20 kW ») et le détail dessous (énergie ajoutée, fin de la charge, limite).
 *       Deux voitures : deux cartes de même forme côte à côte ; une seule : une carte pleine
 *       largeur ; aucune : un message court et la raison (voitures_message, voitures_conseil).
 *       Des pages (ADR-0046, ADR-0082) : la table kPages (nom, a_donnees, montrer, peindre) ;
 *       Aperçu est ici, les autres sont dans tab5_voitures_<page>.cpp ; une page sans donnée n'a
 *       pas d'onglet, un popup à moins de deux pages n'en montre aucun (pages_onglets).
 * @architecture_constraint Push-only et events-only (ADR-0001, ADR-0025) : la tablette ne nomme
 *       aucune entité. Le popup lit le MÊME service que la page Flux, tab5_maj_voiture, que
 *       tab5-api-logic.yaml donne aux deux modules : il garde sa propre copie des voitures
 *       (struct VoitureRecue, la même lecture voiture_lire), sans toucher tab5_energie.cpp, où un autre
 *       lot travaille. Une seule copie des voitures (Voiture de tab5_energie.cpp passée dans un
 *       en-tête, comme le disait l'ADR-0076) reste à faire quand ce fichier est libre.
 *       Rien n'est inventé : ce que la voiture ne porte pas (verrou, sentinelle, câble, bande
 *       « Aujourd'hui », pages Charge / Trajets / État) n'est pas dessiné ; ces blocs viennent
 *       avec leurs données (pages Charge, Trajets, État). Voiture endormie ou hors ligne (état 0) : le dernier
 *       état connu, marqué « Hors ligne » (décision D6, jamais de réveil de la voiture).
 *       Popup fermé : une poussée garde la voiture et ne repeint RIEN ; peint à l'ouverture.
 *       Couleurs par les rôles de la palette (ADR-0029), repeintes par voitures_rejouer_theme() ;
 *       les écritures sont comparées d'abord (ui_text, ui_hidden, ui_poser).
 * @ai_warning Le glyphe de la silhouette n'est pas centré dans sa boîte de texte : sa hauteur
 *       visible est celle de sa boîte de dessin (lv_font_get_glyph_dsc), et le label est posé pour
 *       que CETTE boîte tombe où on la veut (y = haut voulu − blanc du haut, calculé comme
 *       lv_draw_label.c : line_height − base_line − box_h − ofs_y). Sans ça, les deux tiers de la
 *       hauteur du label seraient du blanc. Les arcs de l'anneau suivent la recette de
 *       tab5_energie.cpp (lv_arc_set_bg_angles AVANT lv_arc_set_angles).
 * @ai_instruction Un widget de plus = son champ dans VoituresUI (tab5_voitures.h), son id dans
 *       voitures_carte.yaml et sa ligne dans le script tab5_voitures_lier (tab5-voitures.yaml).
 *       Un texte affiché passe par tr(). Un bloc de plus (pastilles, bande « Aujourd'hui ») :
 *       sa hauteur dans la pile de disposer_carte(), caché tant que sa donnée n'existe pas.
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

VoituresUI g_voitures_ui;

namespace {

constexpr const char* kTag = "tab5.voitures";
constexpr uint32_t kGlypheSilhouette = 0xF07AB;   // car-side (mdi_font_voiture)

// Les voitures reçues (struct VoitureRecue, tab5_voitures.h) : ce que voiture_lire() a lu.
VoitureRecue s_v[kVoituresCartes];
static_assert(kVoituresCartes == kVoituresMax, "autant de cartes que de voitures lues (tab5_parse.h)");

// --- Géométrie ----------------------------------------------------------------------------
// Les nombres sont ceux du Tab5 (corps de 1202 × 598), ramenés à la carte (la 7B : 946 × 478,
// hauteur_carte) ; les polices, elles, ne changent pas (roboto_22) ou suivent carte_police_echelle.
constexpr int32_t kPad = 26;                                   // marge intérieure de la carte
constexpr int32_t kAnneauD = hauteur_carte(170);               // diamètre de l'anneau de la batterie
constexpr int32_t kAnneauL = hauteur_carte(16);                // son épaisseur
constexpr int32_t kEcartNom = hauteur_carte(8);                // sous le nom
constexpr int32_t kEcartSilhouette = hauteur_carte(22);        // sous la silhouette
constexpr int32_t kEcartAnneau = hauteur_carte(24);            // sous l'anneau
constexpr int32_t kEcartEtat = hauteur_carte(6);               // sous la ligne d'état, entre les détails
constexpr int32_t kEcartAnneauTexte = 26;                      // de l'anneau à l'autonomie
constexpr int32_t kEcartIcone = 16;                            // de l'icône d'état à son texte
constexpr int32_t kHautMin = 14;                               // marge du haut au plus serrée
constexpr int32_t kCarteSeuleL = kCorpsW;                      // une seule voiture : pleine largeur
constexpr int32_t kCarteDeuxL = (kCorpsW - kCartesEcart) / 2;  // deux voitures : côte à côte
// Une seule voiture : sa colonne de contenu ne dépasse pas la largeur d'une des deux cartes.
constexpr int32_t kColonneMax = kCarteDeuxL - 2 * kPad;

bool s_construit = false;
lv_obj_t* s_piste[kVoituresCartes] = {};    // la piste de l'anneau (MAIN)
lv_obj_t* s_part[kVoituresCartes] = {};     // la part de la batterie (INDICATOR)

bool popup_ouvert() {
    const VoituresUI& u = g_voitures_ui;
    return u.popup != nullptr && !lv_obj_has_flag(u.popup, LV_OBJ_FLAG_HIDDEN);
}

// Hauteur d'une ligne de texte dans la police du label.
int32_t haut(lv_obj_t* l) {
    const lv_font_t* f = l != nullptr ? lv_obj_get_style_text_font(l, LV_PART_MAIN) : nullptr;
    return f != nullptr ? lv_font_get_line_height(f) : 0;
}

// Un arc de dessin (même recette que arc_dessin de tab5_energie.cpp) : ni bouton, ni toucher ;
// MAIN = la piste, INDICATOR = la part.
lv_obj_t* arc_dessin(lv_obj_t* parent, bool piste) {
    lv_obj_t* a = lv_arc_create(parent);
    lv_obj_remove_style_all(a);
    lv_obj_set_size(a, kAnneauD, kAnneauD);
    lv_obj_set_pos(a, 0, 0);
    lv_arc_set_rotation(a, 270);        // 0° en haut, sens des aiguilles
    lv_arc_set_bg_angles(a, 0, 360);    // avant tout lv_arc_set_angles ([AI-WARNING], tab5_energie.cpp)
    for (lv_part_t part : {LV_PART_MAIN, LV_PART_INDICATOR}) {
        lv_obj_set_style_arc_width(a, kAnneauL, part);
        lv_obj_set_style_arc_rounded(a, !piste, part);
    }
    lv_obj_set_style_arc_opa(a, piste ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(a, piste ? LV_OPA_TRANSP : LV_OPA_COVER, LV_PART_INDICATOR);
    lv_obj_remove_flag(a, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_move_to_index(a, 0);         // sous le pourcentage
    return a;
}

// Couleur d'arc d'une partie, écrite si elle change.
void arc_couleur(lv_obj_t* a, lv_part_t part, uint32_t hex) {
    if (a == nullptr) return;
    const lv_color_t c = lv_color_hex(hex);
    lv_style_value_t cur;
    if (lv_obj_get_local_style_prop(a, LV_STYLE_ARC_COLOR, &cur, part) == LV_STYLE_RES_FOUND &&
        lv_color_eq(cur.color, c))
        return;
    lv_obj_set_style_arc_color(a, c, part);
}

// Les arcs des anneaux, à la première ouverture : le conteneur de chaque carte les porte.
void construire() {
    if (s_construit) return;
    const VoituresUI& u = g_voitures_ui;
    for (int c = 0; c < kVoituresCartes; c++) {
        if (u.anneau[c] == nullptr) return;
    }
    for (int c = 0; c < kVoituresCartes; c++) {
        s_piste[c] = arc_dessin(u.anneau[c], true);
        s_part[c] = arc_dessin(u.anneau[c], false);
        lv_obj_move_to_index(s_piste[c], 0);
    }
    s_construit = true;
}

// Icône de l'état (mdi_font_45) : l'éclair en charge, la prise branchée ou débranchée, le sommeil
// d'une voiture sans état. MDI_CODE_TARGETS (tools/check_tab5_code_rules.py) : voitures_etat_icone_*.
const char* glyphe_etat(VoitureEtat e) {
    switch (e) {
        case VoitureEtat::EN_CHARGE: return "\U000F0241";    // flash
        case VoitureEtat::BRANCHEE: return "\U000F06A5";     // power-plug
        case VoitureEtat::DEBRANCHEE: return "\U000F06A6";   // power-plug-off
        default: return "\U000F04B2";                        // sleep
    }
}

// « 1 h 35 » ou « 42 min » (la même écriture que la page Flux).
void duree(char* out, size_t n, float minutes) {
    const long m = std::lround(minutes);
    if (m >= 60) snprintf(out, n, "%ld h %02ld", m / 60, m % 60);
    else snprintf(out, n, "%ld min", m);
}

// Un label posé à (x, y) sur `l` px de large (texte coupé par « … » au-delà).
void poser_texte(lv_obj_t* lbl, const char* txt, int32_t x, int32_t y, int32_t l) {
    texte_ha_coupe(lbl, txt, l);
    ui_poser(lbl, x, y, l, LV_SIZE_CONTENT);
}

// Silhouette : le glyphe est dessiné dans sa boîte (hauteur visible `vis`), qu'on veut en `haut` ;
// le label, lui, commence au-dessus de cette boîte du blanc que la police laisse en haut.
struct Silhouette {
    int32_t visible = 0;   // hauteur de ce qui se voit
    int32_t blanc = 0;     // du haut du label au haut de ce qui se voit
};
Silhouette mesurer_silhouette(lv_obj_t* lbl) {
    Silhouette s;
    const lv_font_t* f = lbl != nullptr ? lv_obj_get_style_text_font(lbl, LV_PART_MAIN) : nullptr;
    if (f == nullptr) return s;
    lv_font_glyph_dsc_t g;
    if (lv_font_get_glyph_dsc(f, &g, kGlypheSilhouette, 0) && g.box_h > 0) {
        s.visible = g.box_h;
        s.blanc = (lv_font_get_line_height(f) - f->base_line) - g.box_h - g.ofs_y;
    } else {
        s.visible = lv_font_get_line_height(f);   // glyphe absent : toute la boîte
        s.blanc = 0;
    }
    if (s.blanc < 0) s.blanc = 0;
    return s;
}

// Une carte : la voiture `v` (nom, silhouette, anneau, autonomie, état, détail), posée dans
// la largeur `w`, à la colonne x de la carte x0. `rang` : 1 ou 2, la couleur du nom.
void peindre_carte(int c, const VoitureRecue& v, int rang, int32_t x0, int32_t w) {
    const VoituresUI& u = g_voitures_ui;
    lv_obj_t* carte = u.carte[c];
    ui_poser(carte, kCorpsX + x0, kCorpsY, w, kCorpsH);

    // Colonne de contenu : la carte moins ses marges, centrée quand elle est plus large qu'une des deux.
    const int32_t col = std::min(w - 2 * kPad, kColonneMax);
    const int32_t x = (w - col) / 2;

    // --- Textes ---------------------------------------------------------------------------
    char t[96], a[48], b[48];
    if (v.nom[0] != '\0') snprintf(t, sizeof(t), "%s", v.nom);
    else snprintf(t, sizeof(t), tr("Voiture %d"), rang);
    const uint32_t c_etat = voiture_couleur_etat(v.etat);

    // Autonomie : « 326 » en grand, l'unité (« km », « mi ») à côté ; « -- » sans valeur.
    char auto_txt[16];
    const bool a_auto = std::isfinite(v.autonomie);
    if (a_auto) snprintf(auto_txt, sizeof(auto_txt), "%.0f", v.autonomie);
    else snprintf(auto_txt, sizeof(auto_txt), "--");

    // État : « En charge · 7.20 kW » (la puissance quand on la connaît), « Branchée », « Débranchée »,
    // « Hors ligne » (une voiture endormie ou sans état : le dernier état connu, D6).
    char etat[64];
    voiture_texte_etat(etat, sizeof(etat), v);
    // Détail : en charge, l'énergie déjà ajoutée et ce qui reste ; branchée, « Pas en charge ».
    char detail[96] = "";
    if (v.etat == VoitureEtat::EN_CHARGE) {
        a[0] = b[0] = '\0';
        if (std::isfinite(v.energie) && energie_formater(a, sizeof(a), v.energie, "kWh")) {
            char ajoute[48];
            snprintf(ajoute, sizeof(ajoute), tr("+%s ajoutés"), a);
            snprintf(a, sizeof(a), "%s", ajoute);
        } else {
            a[0] = '\0';
        }
        if (std::isfinite(v.minutes)) {
            char d[24];
            duree(d, sizeof(d), v.minutes);
            snprintf(b, sizeof(b), tr("Prête dans %s"), d);
        }
        if (a[0] != '\0' && b[0] != '\0') snprintf(detail, sizeof(detail), "%s \xC2\xB7 %s", a, b);
        else snprintf(detail, sizeof(detail), "%s%s", a, b);
    } else if (v.etat == VoitureEtat::BRANCHEE) {
        snprintf(detail, sizeof(detail), "%s", tr("Pas en charge"));
    }
    char limite[32] = "";
    if (std::isfinite(v.limite)) snprintf(limite, sizeof(limite), tr("Limite %d %%"), static_cast<int>(std::lround(v.limite)));

    // --- Pile verticale, centrée dans la carte -----------------------------------------------
    const Silhouette sil = mesurer_silhouette(u.silhouette[c]);
    const int32_t h_nom = haut(u.nom[c]);
    const int32_t h_icone = haut(u.etat_icone[c]);
    const int32_t h_etat = std::max(h_icone, haut(u.etat[c]));
    const int32_t h_detail = haut(u.detail[c]);
    const int32_t h_limite = haut(u.limite[c]);
    const bool a_detail = detail[0] != '\0';
    const bool a_limite = limite[0] != '\0';
    const int32_t total = h_nom + kEcartNom + sil.visible + kEcartSilhouette + kAnneauD + kEcartAnneau + h_etat +
                          (a_detail ? kEcartEtat + h_detail : 0) + (a_limite ? kEcartEtat + h_limite : 0);
    int32_t y = std::max(kHautMin, (kCorpsH - total) / 2);

    // Nom : à la couleur de son rang (comme la page Charges), coupé à la colonne.
    ui_text_color(u.nom[c], rang == 1 ? UIColor.ACCENT : UIColor.ACCENT_ALT);
    poser_texte(u.nom[c], t, x, y, col);
    y += h_nom + kEcartNom;

    // Silhouette : centrée, à la couleur de l'état ; son label est remonté du blanc du haut.
    ui_text_color(u.silhouette[c], c_etat);
    ui_poser(u.silhouette[c], x, y - sil.blanc, col, LV_SIZE_CONTENT);
    y += sil.visible + kEcartSilhouette;

    // Anneau (à gauche) et autonomie (à droite, centrée en hauteur sur l'anneau).
    ui_poser(u.anneau[c], x, y, kAnneauD, kAnneauD);
    const float pct_f = std::isfinite(v.batterie) ? std::min(100.0f, std::max(0.0f, v.batterie)) : 0.0f;
    const bool a_batterie = std::isfinite(v.batterie);
    arc_couleur(s_piste[c], LV_PART_MAIN, UIColor.ARC_TRACK);
    const int32_t fin = static_cast<int32_t>(std::lround(360.0f * pct_f / 100.0f));
    if (!a_batterie || fin < 1) {
        ui_hidden(s_part[c], true);
    } else {
        if (static_cast<int32_t>(lv_arc_get_angle_start(s_part[c])) != 0 ||
            static_cast<int32_t>(lv_arc_get_angle_end(s_part[c])) != fin)
            lv_arc_set_angles(s_part[c], 0, fin);
        arc_couleur(s_part[c], LV_PART_INDICATOR, get_battery_color(v.batterie));
        ui_hidden(s_part[c], false);
    }
    if (a_batterie) snprintf(a, sizeof(a), "%.0f %%", v.batterie);
    else snprintf(a, sizeof(a), "--");
    ui_text(u.pct[c], a);
    ui_text_color(u.pct[c], UIColor.TEXT_PRIMARY);
    ui_poser(u.pct[c], 0, (kAnneauD - haut(u.pct[c])) / 2, kAnneauD, LV_SIZE_CONTENT);

    const int32_t h_auto = haut(u.autonomie[c]);
    const int32_t h_sous = haut(u.sous[c]);
    const int32_t bloc = h_auto + h_sous;
    const int32_t ax = x + kAnneauD + kEcartAnneauTexte;
    const int32_t ay = y + (kAnneauD - bloc) / 2;
    ui_text(u.autonomie[c], auto_txt);
    ui_text_color(u.autonomie[c], UIColor.TEXT_PRIMARY);
    ui_poser(u.autonomie[c], ax, ay, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    // L'unité sur la ligne de base du grand chiffre : à droite de lui, sans dépasser la colonne.
    ui_text(u.unite[c], a_auto ? (v.unite[0] != '\0' ? v.unite : "km") : "");
    ui_text_color(u.unite[c], UIColor.TEXT_SOFT);
    ui_hidden(u.unite[c], !a_auto);   // ui_poser() montre : ne le poser que s'il y a une autonomie
    if (a_auto) {
        const int32_t w_auto = ui_largeur_texte(u.autonomie[c], auto_txt);
        ui_poser(u.unite[c], ax + w_auto + 10, ay + h_auto - haut(u.unite[c]) - 6, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    }
    ui_text(u.sous[c], tr("d'autonomie"));
    ui_text_color(u.sous[c], UIColor.TEXT_DIM);
    ui_poser(u.sous[c], ax, ay + h_auto, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    y += kAnneauD + kEcartAnneau;

    // État : l'icône (à gauche) puis le texte, au même niveau ; le détail et la limite dessous, alignés
    // sur le texte de l'état.
    const int32_t w_icone = ui_largeur_texte(u.etat_icone[c], glyphe_etat(v.etat));
    ui_text(u.etat_icone[c], glyphe_etat(v.etat));
    ui_text_color(u.etat_icone[c], c_etat);
    ui_poser(u.etat_icone[c], x, y + (h_etat - h_icone) / 2, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    const int32_t tx = x + std::max<int32_t>(w_icone, 0) + kEcartIcone;
    const int32_t tl = col - (tx - x);
    ui_text_color(u.etat[c], c_etat);
    poser_texte(u.etat[c], etat, tx, y + (h_etat - haut(u.etat[c])) / 2, tl);
    y += h_etat;
    ui_text_color(u.detail[c], UIColor.TEXT_DIM);
    ui_hidden(u.detail[c], !a_detail);
    if (a_detail) {
        y += kEcartEtat;
        poser_texte(u.detail[c], detail, tx, y, tl);
        y += h_detail;
    }
    ui_text_color(u.limite[c], UIColor.TEXT_DIM);
    ui_hidden(u.limite[c], !a_limite);
    if (a_limite) {
        y += kEcartEtat;
        poser_texte(u.limite[c], limite, tx, y, tl);
    }
}

// Rien de reçu : un message court au milieu du corps, et d'où viennent les voitures.
void peindre_vide(bool vide) {
    const VoituresUI& u = g_voitures_ui;
    ui_hidden(u.message, !vide);
    ui_hidden(u.conseil, !vide);
    if (!vide) return;
    const int32_t hm = haut(u.message);
    const int32_t hc = haut(u.conseil);
    const int32_t y = kCorpsY + (kCorpsH - (hm + 10 + hc)) / 2;
    ui_text(u.message, tr("Aucune voiture reçue"));
    ui_text_color(u.message, UIColor.TEXT_SOFT);
    ui_poser(u.message, kCorpsX, y, kCorpsW, LV_SIZE_CONTENT);
    ui_text(u.conseil, tr("Choisissez-les dans le blueprint, section « Énergie : voitures »."));
    ui_text_color(u.conseil, UIColor.TEXT_DIM);
    ui_poser(u.conseil, kCorpsX, y + hm + 10, kCorpsW, LV_SIZE_CONTENT);
}

// --- Pages (ADR-0046, ADR-0082) ----------------------------------------------------------
// Une page = un nom (l'onglet), « a-t-elle des données ? » (sans, ni onglet ni page), « montre ou
// cache ses widgets » (une page crée les siens en C++ à sa première montre, sur voitures_corps()) et
// « repeins-toi » (popup ouvert, page affichée). Aperçu est ici (les cartes du YAML) ; Charge,
// Trajets et État ont leurs fichiers tab5_voitures_<page>.{h,cpp}.
struct Page {
    const char* nom;                // tr_noop : traduit à l'affichage
    bool (*a_donnees)();
    void (*montrer)(bool visible);
    void (*peindre)();
};

// Une voiture reçue au moins : l'Aperçu a de quoi se montrer. Sans voiture, le popup dit pourquoi
// (message et conseil) et n'a aucune page.
bool apercu_a_donnees() {
    for (const VoitureRecue& v : s_v)
        if (v.presente) return true;
    return false;
}

void apercu_montrer(bool visible) {
    if (!visible)
        for (lv_obj_t* c : g_voitures_ui.carte) ui_hidden(c, true);
}

// Les voitures reçues dans l'ordre des rangs : la seconde seule prend la première carte.
void apercu_peindre() {
    const VoituresUI& u = g_voitures_ui;
    int rang[kVoituresCartes] = {};
    int n = 0;
    for (int r = 0; r < kVoituresCartes; r++)
        if (s_v[r].presente) rang[n++] = r;
    const int32_t w = n == 1 ? kCarteSeuleL : kCarteDeuxL;
    for (int c = 0; c < kVoituresCartes; c++) {
        if (c >= n) {
            ui_hidden(u.carte[c], true);
            continue;
        }
        peindre_carte(c, s_v[rang[c]], rang[c] + 1, c * (kCarteDeuxL + kCartesEcart), w);
    }
}

// L'ordre est celui des onglets : Aperçu, Charge, Trajets, État (une page de plus : sa ligne ici, son
// en-tête dans tab5_custom.h et ses fichiers dans les includes: de tab5-ha-hmi.yaml).
const Page kPages[] = {
    {tr_noop("Aperçu"), apercu_a_donnees, apercu_montrer, apercu_peindre},
    // Des lambdas sans capture, pas les fonctions nues : la règle 12 (CPP-4) ne compte comme appel que
    // `nom(`, une adresse prise dans la table n'en est pas un.
    {tr_noop("Charge"), [] { return voitures_charge_a_donnees(); }, [](bool v) { voitures_charge_montrer(v); },
     [] { voitures_charge_peindre(); }},
};
constexpr int kNbPages = static_cast<int>(sizeof(kPages) / sizeof(kPages[0]));
static_assert(kNbPages <= kVoituresPages, "un onglet YAML par page (voitures_popup.yaml)");

int s_page = 0;                 // la page affichée : un index de kPages
bool s_page_choisie = false;    // le doigt a choisi une page (sinon la première qui a des données)

// Les pages qui ont des données, dans l'ordre : `liste` reçoit leurs index de kPages.
int pages_montrees(int* liste) {
    int n = 0;
    for (int p = 0; p < kNbPages; p++)
        if (kPages[p].a_donnees()) liste[n++] = p;
    return n;
}

int nombre_pages() {
    int liste[kNbPages];
    return pages_montrees(liste);
}

// Le rang de la page affichée parmi les pages montrées (0 sans page).
int page_courante() {
    int liste[kNbPages];
    const int n = pages_montrees(liste);
    for (int i = 0; i < n; i++)
        if (liste[i] == s_page) return i;
    return 0;
}

void peindre() {
    const VoituresUI& u = g_voitures_ui;
    if (!s_construit || u.popup == nullptr) return;
    int liste[kNbPages];
    const int n = pages_montrees(liste);
    // Ouvert sur la première page qui a des données, tant que le doigt n'en a pas choisi une ; une
    // page qui perd ses données rend la main à la première.
    if (n > 0 && (!s_page_choisie || !kPages[s_page].a_donnees())) s_page = liste[0];
    const char* noms[kNbPages] = {};
    int rang = 0;
    for (int i = 0; i < n; i++) {
        noms[i] = tr(kPages[liste[i]].nom);
        if (liste[i] == s_page) rang = i;
    }
    pages_onglets(u.onglet, kVoituresPages, noms, n, rang);
    peindre_vide(n == 0);
    for (int p = 0; p < kNbPages; p++) kPages[p].montrer(n > 0 && p == s_page);
    if (n > 0) kPages[s_page].peindre();
}

void afficher_page(int rang) {
    int liste[kNbPages];
    const int n = pages_montrees(liste);
    if (rang < 0 || rang >= n) rang = 0;
    if (n == 0) return;
    s_page = liste[rang];
    s_page_choisie = true;
    peindre();
    ui_mark_activity();
}

// Geste gauche / droite du popup (tab5_pages.cpp, ADR-0046) ; les onglets suivent.
PagesPopup s_pages{nullptr, nombre_pages, page_courante, afficher_page};

}  // namespace

// ADR-0076 : le payload de tab5_maj_voiture, relu ici pour le popup. Un payload vide, ou sans aucun
// champ, retire la voiture ; un rang inconnu est refusé.
void voitures_recu(const std::string& rang, const std::string& payload) {
    if (payload_trop_long(kTag, payload.size())) return;
    const int k = rang == "1" ? 0 : (rang == "2" ? 1 : -1);
    if (k < 0) {
        payload_refuse(kTag, "voiture : rang inconnu", rang.size());
        return;
    }
    VoitureRecue v;
    VoitureLue lu;
    if (voiture_lire(Champ{payload.data(), payload.size()}, lu)) {
        v.presente = true;
        texte_ha_copier(v.nom, sizeof(v.nom), lu.nom.p, lu.nom.n);
        texte_ha_copier(v.unite, sizeof(v.unite), lu.unite_distance.p, lu.unite_distance.n);
        v.etat = lu.etat;
        v.batterie = lu.batterie;
        v.autonomie = lu.autonomie;
        v.puissance = lu.puissance;
        v.energie = lu.energie;
        v.limite = lu.limite;
        v.minutes = lu.minutes;
        const time_t now = tab5_time_source(nullptr);
        v.recue_le = tab5_heure_valide(now) ? now : 0;
    }
    s_v[k] = v;
    if (popup_ouvert()) peindre();
}

// Les trois cas de l'état de charge : la couleur de l'icône, de la silhouette et du texte (les
// mêmes que les cercles de la page Flux : charge, câble branché, le reste en retrait).
uint32_t voiture_couleur_etat(VoitureEtat e) {
    return e == VoitureEtat::EN_CHARGE ? UIColor.SUCCESS : e == VoitureEtat::BRANCHEE ? UIColor.INFO
                                                                                      : UIColor.TEXT_DIM;
}

// « En charge · 7.20 kW » (la puissance quand on la connaît), « Branchée », « Débranchée », « Hors
// ligne » (une voiture endormie ou sans état : le dernier état connu, jamais de réveil, D6).
void voiture_texte_etat(char* out, size_t n, const VoitureRecue& v) {
    char p[48];
    switch (v.etat) {
        case VoitureEtat::EN_CHARGE:
            if (std::isfinite(v.puissance) && energie_formater(p, sizeof(p), v.puissance, "W"))
                snprintf(out, n, "%s \xC2\xB7 %s", tr("En charge"), p);
            else snprintf(out, n, "%s", tr("En charge"));
            break;
        case VoitureEtat::BRANCHEE: snprintf(out, n, "%s", tr("Branchée")); break;
        case VoitureEtat::DEBRANCHEE: snprintf(out, n, "%s", tr("Débranchée")); break;
        default: snprintf(out, n, "%s", tr("Hors ligne")); break;
    }
}

void voitures_repeindre() {
    if (popup_ouvert()) peindre();
}

const VoitureRecue& voiture_recue(int k) { return s_v[k >= 0 && k < kVoituresCartes ? k : 0]; }

lv_obj_t* voitures_corps() {
    const VoituresUI& u = g_voitures_ui;
    return u.carte[0] != nullptr ? lv_obj_get_parent(u.carte[0]) : nullptr;
}

void voitures_page(int rang) { afficher_page(rang); }

void voitures_ouvrir() {
    const VoituresUI& u = g_voitures_ui;
    if (u.popup == nullptr) return;
    construire();
    // Le glissement gauche / droite des pages (une fois : rappelé, l'ancien rappel est retiré d'abord).
    s_pages.popup = u.popup;
    pages_brancher(&s_pages);
    peindre();
    // À chaque ouverture, comme le popup Énergie : Home Assistant ne pousse que tant que « Écran
    // courant » vaut « Energy » ou « Cars » ; ouvert par « Aller à l'écran » après une longue absence,
    // le popup montrerait sinon les dernières valeurs reçues, périmées. Le script de HA est en mode
    // `restart` : une demande de plus relance sa boucle, rien ne s'empile.
    if (u.demander != nullptr) u.demander();
}

void voitures_rejouer_theme() {
    if (!s_construit || !popup_ouvert()) return;
    peindre();
}
