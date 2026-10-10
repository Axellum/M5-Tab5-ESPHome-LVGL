/**
 * [AI-CONTEXT]
 * @file tab5_suivi.cpp
 * @role Capteurs suivis (ADR-0053, 10/10/2026, demande d'Axel : « le graphique d'un capteur
 *       numérique choisi dans HA », à gauche de l'horloge, et un popup avec tous ceux qu'on
 *       suit). Peint, d'après ce que pousse tab5_maj_suivi (lu par suivis_lire(),
 *       Tab5/socle/tab5_parse.h, section 11) :
 *         - le popup « Suivi » (suivi_popup.yaml) : une carte par capteur (suivi_carte.yaml,
 *           six au plus), en une rangée jusqu'à trois capteurs, deux au-delà (deux colonnes
 *           pour quatre), la dernière rangée centrée ; chaque carte : nom, valeur et unité,
 *           flèche de tendance et variation colorées, période (« Aujourd'hui » pour un
 *           change_pct, « 24 h » pour un écart sur la courbe), courbe des 24 h lissée et
 *           son dernier point ;
 *         - la carte de la zone à gauche de l'horloge (suivi_zone.yaml, contenu « capteur »
 *           de l'ADR-0051) : le PREMIER capteur de la liste, sa courbe sur le dégradé de la
 *           zone (tampon partagé avec le graphique, zone_degrade_peindre). Un tap ouvre le
 *           popup (choix le plus simple : pas de défilement des capteurs dans la zone).
 * @architecture_constraint Push-only et events-only (ADR-0001, ADR-0025) : rien n'est
 *       demandé à HA, ni à l'ouverture ni au tap ; aucune entité nommée. Hausse SUCCESS,
 *       baisse ERROR, stable TEXT_DIM (courbe ACCENT) : rôles de la palette (ADR-0029),
 *       aucune couleur en dur ; repeint par suivi_rejouer_theme(). La carte de la zone n'est
 *       peinte que montrée (s_zone_montre, posé par zone_gauche_appliquer() via
 *       suivi_zone_montrer), sinon marquée « sale ». Courbes : lv_line (lv_chart et
 *       lv_canvas ne sont pas compilés), créées à la première peinture ; écritures
 *       comparées d'abord (ui_text, ui_poser…).
 * @ai_warning lv_line_set_points() ne COPIE PAS les points : s_pts et s_zone_pts vivent au
 *       niveau du fichier. Glyphes de tendance posés d'ici (glyphe_tendance), dans
 *       mdi_font_32 : MDI_CODE_TARGETS de tools/check_tab5_code_rules.py (règle 9).
 * @ai_instruction Un widget de plus = son champ dans SuiviUI (tab5_suivi.h) et sa ligne
 *       dans le script tab5_suivi_lier (tab5-suivi.yaml). Un texte affiché passe par tr().
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include <cmath>
#include <cstdio>
#include <cstring>

SuiviUI g_suivi_ui;

namespace {

static_assert(kSuiviCartes == kSuivisMax, "une carte par capteur de la liste");
static_assert(kSuiviPointsMax <= kCourbeLissePoints, "ui_courbe_lisse ne trace pas plus de points");

constexpr const char* kTag = "tab5.suivi";
constexpr size_t kNomMax = 48;
constexpr size_t kUniteMax = 24;
constexpr int kLisse = 6;  // points de courbe entre deux points poussés
constexpr int kCourbeMax = (kSuiviPointsMax - 1) * kLisse + 1;
// Popup : grille dans le corps de la carte modale (x kCorpsX, y kCorpsY..kCorpsBas).
constexpr int32_t kCorpsBas = kCarteH - 20;
constexpr int32_t kColonnesMax = 3;
// Une carte du popup : marges, nom, valeur, rangée de la variation, courbe.
constexpr int32_t kMarge = 22;
constexpr int32_t kNomY = 16;
constexpr int32_t kValeurY = 46;
constexpr int32_t kEcartUnite = 8;
constexpr int32_t kFlecheL = 34;          // flèche de tendance (mdi_font_32) et son air
constexpr int32_t kEcartPeriode = 12;
constexpr int32_t kCourbeEcart = 18;      // sous la rangée de la variation
constexpr int32_t kCourbeBas = 26;        // le point le plus bas, à tant du bas
constexpr int32_t kTrait = 4;
// Zone à gauche de l'horloge (405 × 184, suivi_zone.yaml) : nom et variation en haut,
// valeur dessous, courbe en bas sur le dégradé de la zone (jusqu'au pied kZonePied).
constexpr int32_t kZoneMarge = 22;        // le cadre Capsule est très arrondi
constexpr int32_t kZoneNomY = 12;
constexpr int32_t kZoneValeurY = 34;
constexpr int32_t kZoneCourbeEcart = 10;  // sous la valeur
constexpr int32_t kZoneCourbeBas = 22;    // le point le plus bas, à tant du bas
constexpr int32_t kZonePied = 12;         // pied du dégradé, à tant du bas
constexpr int32_t kZoneTrait = 5;
constexpr int32_t kPoint = 10;            // dernier point de la courbe
constexpr int32_t kAnneau = 3;            // son anneau, à la couleur du fond

// Un capteur reçu : ses textes copiés (le payload ne vit pas).
struct Capteur {
    SuiviLu lu;  // Champ vides : seuls les textes ci-dessous sont lus
    char nom[kNomMax + 1] = "";
    char unite[kUniteMax + 1] = "";
};

// Une courbe : sa ligne, son dernier point (créés à la première peinture).
struct Courbe {
    lv_obj_t* ligne = nullptr;
    lv_obj_t* point = nullptr;
};

Capteur s_c[kSuivisMax];
int s_n = 0;
bool s_recu = false;          // une poussée de HA est arrivée depuis le démarrage
Courbe s_courbe[kSuiviCartes];
lv_point_precise_t s_pts[kSuiviCartes][kCourbeMax];
Courbe s_zone_courbe;
lv_obj_t* s_zone_aire = nullptr;  // dégradé sous la courbe de la zone (lv_image)
lv_point_precise_t s_zone_pts[kCourbeMax];
bool s_zone_montre = false;   // la carte est le contenu de la zone gauche
bool s_zone_sale = true;      // à repeindre à sa prochaine apparition

const SuiviUI& ui() { return g_suivi_ui; }

bool popup_ouvert() { return ui().popup != nullptr && !lv_obj_has_flag(ui().popup, LV_OBJ_FLAG_HIDDEN); }

// Flèche de tendance (mdi_font_32) : trending-up, trending-down, trending-neutral.
const char* glyphe_tendance(int sens) {
    if (sens > 0) return "\U000F0535";
    if (sens < 0) return "\U000F0533";
    return "\U000F0534";
}

uint32_t couleur_texte(int sens) { return sens > 0 ? UIColor.SUCCESS : (sens < 0 ? UIColor.ERROR : UIColor.TEXT_DIM); }
uint32_t couleur_courbe(int sens) { return sens > 0 ? UIColor.SUCCESS : (sens < 0 ? UIColor.ERROR : UIColor.ACCENT); }

int32_t largeur_texte(lv_obj_t* l, const char* t) {
    if (l == nullptr || t == nullptr || t[0] == '\0') return 0;
    lv_point_t sz;
    lv_text_get_size(&sz, t, lv_obj_get_style_text_font(l, LV_PART_MAIN), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return sz.x;
}

int32_t hauteur_ligne(lv_obj_t* l) {
    const lv_font_t* f = l != nullptr ? lv_obj_get_style_text_font(l, LV_PART_MAIN) : nullptr;
    return f != nullptr ? lv_font_get_line_height(f) : 26;
}

void poser(lv_obj_t* o, int32_t x, int32_t y) {
    ui_x(o, x);
    ui_y(o, y);
    ui_hidden(o, false);
}

// Taille utile d'une carte : sa largeur ou sa hauteur moins bordure et padding (LVGL 9
// place les enfants dans la bordure et le padding).
int32_t utile(lv_obj_t* c, bool largeur) {
    const int32_t b = lv_obj_get_style_border_width(c, LV_PART_MAIN);
    if (largeur) {
        return lv_obj_get_style_width(c, LV_PART_MAIN) - 2 * b - lv_obj_get_style_pad_left(c, LV_PART_MAIN) -
               lv_obj_get_style_pad_right(c, LV_PART_MAIN);
    }
    return lv_obj_get_style_height(c, LV_PART_MAIN) - 2 * b - lv_obj_get_style_pad_top(c, LV_PART_MAIN) -
           lv_obj_get_style_pad_bottom(c, LV_PART_MAIN);
}

void creer_courbe(Courbe& c, lv_obj_t* parent, int32_t trait) {
    if (c.ligne != nullptr || parent == nullptr) return;
    c.ligne = ui_ligne(parent, trait);
    c.point = ui_rectangle(parent, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    ui_style_num(c.point, LV_STYLE_BORDER_WIDTH, kAnneau);
    ui_style_num(c.point, LV_STYLE_BORDER_OPA, LV_OPA_COVER);
}

void cacher_courbe(const Courbe& c) {
    ui_hidden(c.ligne, true);
    ui_hidden(c.point, true);
}

// La courbe de `s` entre x0 et x1, du point le plus haut y_haut au plus bas y_bas, dans
// `pts` (qui vit avec la ligne). Les créneaux sans mesure laissent un trou dans l'axe du
// temps, la courbe passe par les points voisins. Renvoie le nombre de points tracés (0 :
// moins de deux mesures, courbe masquée).
int tracer(const Capteur& s, const Courbe& c, lv_point_precise_t* pts, int32_t x0, int32_t x1, int32_t y_haut,
           int32_t y_bas) {
    float xs[kSuiviPointsMax], ys[kSuiviPointsMax];
    int m = 0;
    const int n = s.lu.n;
    for (int k = 0; k < n && n >= 2; k++) {
        if (s.lu.points[k] == kSuiviPointAucun) continue;
        xs[m] = static_cast<float>(x0) + static_cast<float>(x1 - x0) * static_cast<float>(k) / static_cast<float>(n - 1);
        ys[m] = static_cast<float>(y_bas) - static_cast<float>(s.lu.points[k]) / 100.0f * static_cast<float>(y_bas - y_haut);
        m++;
    }
    const int np = ui_courbe_lisse(xs, ys, m, kLisse, pts);
    if (np < 2) {
        cacher_courbe(c);
        return 0;
    }
    const int sens = suivi_sens(s.lu);
    ui_style_couleur(c.ligne, LV_STYLE_LINE_COLOR, couleur_courbe(sens));
    lv_line_set_points(c.ligne, pts, static_cast<uint32_t>(np));
    ui_hidden(c.ligne, false);
    ui_style_couleur(c.point, LV_STYLE_BG_COLOR, couleur_courbe(sens));
    ui_style_couleur(c.point, LV_STYLE_BORDER_COLOR, UIColor.BG);
    const int32_t px = static_cast<int32_t>(lroundf(xs[m - 1])), py = static_cast<int32_t>(lroundf(ys[m - 1]));
    ui_poser(c.point, px - kPoint / 2, py - kPoint / 2, kPoint, kPoint);
    return np;
}

// Valeur, unité, flèche, variation et période d'un capteur (textes et couleurs).
void textes(const Capteur& s, char* valeur, size_t nv, char* variation, size_t nvar) {
    suivi_nombre_texte(s.lu.valeur, s.lu.decimales, valeur, nv);
    suivi_variation_texte(s.lu, variation, nvar);
    // Écart : dans l'unité du capteur (« +1.2 °C ») ; un pourcentage a déjà son « % ».
    if (variation[0] != '\0' && s.lu.genre == SuiviVariation::ECART && s.unite[0] != '\0') {
        const size_t l = std::strlen(variation);
        snprintf(variation + l, nvar - l, " %s", s.unite);
    }
}

// ─── Popup ───

void peindre_carte(int i, int32_t x, int32_t y, int32_t w, int32_t h) {
    const SuiviUI& u = ui();
    lv_obj_t* carte = u.carte[i];
    if (carte == nullptr) return;
    ui_poser(carte, x, y, w, h);
    const Capteur& s = s_c[i];
    const int32_t cw = utile(carte, true), ch = utile(carte, false);
    char valeur[32], variation[48];
    textes(s, valeur, sizeof(valeur), variation, sizeof(variation));
    poser(u.nom[i], kMarge, kNomY);
    texte_ha_coupe(u.nom[i], s.nom[0] != '\0' ? s.nom : tr("Capteur"), cw - 2 * kMarge);
    ui_text(u.valeur[i], valeur);
    poser(u.valeur[i], kMarge, kValeurY);
    const int32_t lh_v = hauteur_ligne(u.valeur[i]);
    const int32_t lh_u = hauteur_ligne(u.unite[i]);
    const int32_t x_unite = kMarge + largeur_texte(u.valeur[i], valeur) + kEcartUnite;
    ui_hidden(u.unite[i], s.unite[0] == '\0' || !std::isfinite(s.lu.valeur));
    if (s.unite[0] != '\0' && std::isfinite(s.lu.valeur)) {
        ui_x(u.unite[i], x_unite);
        ui_y(u.unite[i], kValeurY + lh_v - lh_u - 6);
        texte_ha_coupe(u.unite[i], s.unite, cw - kMarge - x_unite);
    }
    // Variation : flèche, texte coloré, période en gris.
    const int sens = suivi_sens(s.lu);
    const bool a_variation = variation[0] != '\0';
    const int32_t y_rangee = kValeurY + lh_v + 6;
    const int32_t lh_var = hauteur_ligne(u.variation[i]);
    const int32_t lh_fleche = hauteur_ligne(u.fleche[i]);
    ui_hidden(u.fleche[i], !a_variation);
    ui_hidden(u.variation[i], !a_variation);
    ui_hidden(u.periode[i], !a_variation);
    if (a_variation) {
        ui_text(u.fleche[i], glyphe_tendance(sens));
        ui_text_color(u.fleche[i], couleur_texte(sens));
        poser(u.fleche[i], kMarge - 4, y_rangee);
        ui_text(u.variation[i], variation);
        ui_text_color(u.variation[i], couleur_texte(sens));
        const int32_t y_texte = y_rangee + (lh_fleche - lh_var) / 2;
        poser(u.variation[i], kMarge + kFlecheL, y_texte);
        const int32_t x_periode = kMarge + kFlecheL + largeur_texte(u.variation[i], variation) + kEcartPeriode;
        ui_text(u.periode[i], s.lu.genre == SuiviVariation::POURCENT ? tr("Aujourd'hui") : tr("24 h"));
        poser(u.periode[i], x_periode, y_texte);
    }
    // Courbe : sous la rangée de la variation, jusqu'en bas de la carte.
    creer_courbe(s_courbe[i], carte, kTrait);
    const int32_t y_haut = y_rangee + lh_fleche + kCourbeEcart;
    const int32_t y_bas = ch - kCourbeBas;
    if (y_bas - y_haut < 16) {
        cacher_courbe(s_courbe[i]);
        return;
    }
    tracer(s, s_courbe[i], s_pts[i], kMarge, cw - kMarge, y_haut, y_bas);
}

void peindre_popup() {
    const SuiviUI& u = ui();
    if (u.popup == nullptr) return;
    // Sans capteur : la phrase (en attente, ou aucun choisi et où le choisir).
    const bool vide = !s_recu || s_n == 0;
    ui_hidden(u.attente, !vide);
    ui_hidden(u.conseil, !(s_recu && s_n == 0));
    if (vide) ui_text(u.attente, s_recu ? tr("Aucun capteur suivi") : tr("En attente de Home Assistant"));
    if (s_recu && s_n == 0) ui_text(u.conseil, tr("Choisissez vos capteurs dans Home Assistant : « Tab5 · capteurs suivis »."));
    for (int i = s_n; i < kSuiviCartes; i++) ui_hidden(u.carte[i], true);
    if (vide) return;
    // Grille : une rangée jusqu'à trois capteurs, deux au-delà ; deux colonnes pour quatre.
    const int colonnes = s_n <= kColonnesMax ? s_n : (s_n == 4 ? 2 : kColonnesMax);
    const int rangees = (s_n + colonnes - 1) / colonnes;
    const int32_t w = (kCorpsW - (colonnes - 1) * kCartesEcart) / colonnes;
    const int32_t h = (kCorpsBas - kCorpsY - (rangees - 1) * kCartesEcart) / rangees;
    for (int i = 0; i < s_n; i++) {
        const int r = i / colonnes, c = i % colonnes;
        // La dernière rangée, incomplète, centrée.
        const int dans_rangee = r == rangees - 1 ? s_n - r * colonnes : colonnes;
        const int32_t largeur_rangee = dans_rangee * w + (dans_rangee - 1) * kCartesEcart;
        const int32_t x = kCorpsX + (kCorpsW - largeur_rangee) / 2 + c * (w + kCartesEcart);
        peindre_carte(i, x, kCorpsY + r * (h + kCartesEcart), w, h);
    }
}

// ─── Carte de la zone à gauche de l'horloge ───

void zone_perdue() { s_zone_sale = true; }  // le graphique a repris le tampon du dégradé

void peindre_zone() {
    const SuiviUI& u = ui();
    if (u.zone == nullptr) return;
    if (!s_zone_montre) {
        s_zone_sale = true;  // caché : repeint à sa prochaine apparition
        return;
    }
    s_zone_sale = false;
    if (s_zone_aire == nullptr) {
        // Ordre de création = ordre de dessin : le dégradé sous la courbe et son point.
        s_zone_aire = lv_image_create(u.zone);
        lv_obj_remove_flag(s_zone_aire, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_flag(s_zone_aire, LV_OBJ_FLAG_HIDDEN);
        creer_courbe(s_zone_courbe, u.zone, kZoneTrait);
    }
    const int32_t w = utile(u.zone, true), h = utile(u.zone, false);
    const bool vide = !s_recu || s_n == 0;
    ui_hidden(u.zone_attente, !vide);
    for (lv_obj_t* o : {u.zone_nom, u.zone_valeur, u.zone_unite, u.zone_fleche, u.zone_variation})
        if (vide) ui_hidden(o, true);
    if (vide) {
        ui_text(u.zone_attente, s_recu ? tr("Aucun capteur suivi") : tr("En attente de Home Assistant"));
        cacher_courbe(s_zone_courbe);
        ui_hidden(s_zone_aire, true);
        return;
    }
    const Capteur& s = s_c[0];
    char valeur[32], variation[48];
    textes(s, valeur, sizeof(valeur), variation, sizeof(variation));
    // En haut : la variation à droite (flèche et texte), le nom dans la place qui reste.
    const int sens = suivi_sens(s.lu);
    const bool a_variation = variation[0] != '\0';
    const int32_t lh_var = hauteur_ligne(u.zone_variation);
    int32_t x_var = w - kZoneMarge;
    ui_hidden(u.zone_fleche, !a_variation);
    ui_hidden(u.zone_variation, !a_variation);
    if (a_variation) {
        ui_text(u.zone_variation, variation);
        ui_text_color(u.zone_variation, couleur_texte(sens));
        x_var -= largeur_texte(u.zone_variation, variation);
        poser(u.zone_variation, x_var, kZoneNomY);
        ui_text(u.zone_fleche, glyphe_tendance(sens));
        ui_text_color(u.zone_fleche, couleur_texte(sens));
        x_var -= kFlecheL;
        poser(u.zone_fleche, x_var, kZoneNomY + (lh_var - hauteur_ligne(u.zone_fleche)) / 2);
    }
    poser(u.zone_nom, kZoneMarge, kZoneNomY);
    texte_ha_coupe(u.zone_nom, s.nom[0] != '\0' ? s.nom : tr("Capteur"), x_var - 12 - kZoneMarge);
    // Valeur et unité.
    ui_text(u.zone_valeur, valeur);
    poser(u.zone_valeur, kZoneMarge, kZoneValeurY);
    const int32_t lh_v = hauteur_ligne(u.zone_valeur);
    const int32_t x_unite = kZoneMarge + largeur_texte(u.zone_valeur, valeur) + kEcartUnite;
    ui_hidden(u.zone_unite, s.unite[0] == '\0' || !std::isfinite(s.lu.valeur));
    if (s.unite[0] != '\0' && std::isfinite(s.lu.valeur)) {
        ui_x(u.zone_unite, x_unite);
        ui_y(u.zone_unite, kZoneValeurY + lh_v - hauteur_ligne(u.zone_unite) - 6);
        texte_ha_coupe(u.zone_unite, s.unite, w - kZoneMarge - x_unite);
    }
    // Courbe et dégradé, sous la valeur jusqu'en bas.
    const int32_t y_haut = kZoneValeurY + lh_v + kZoneCourbeEcart;
    const int32_t y_bas = h - kZoneCourbeBas, base = h - kZonePied;
    const int np = y_bas - y_haut >= 16 ? tracer(s, s_zone_courbe, s_zone_pts, kZoneMarge, w - kZoneMarge, y_haut, y_bas) : 0;
    if (np < 2) {
        cacher_courbe(s_zone_courbe);
        ui_hidden(s_zone_aire, true);
        return;
    }
    zone_degrade_peindre(s_zone_aire, s_zone_pts, np, couleur_courbe(sens), y_haut, base, zone_perdue);
}

void peindre() {
    if (popup_ouvert()) peindre_popup();
    peindre_zone();
}

}  // namespace

// ─── API (tab5_suivi.h, tab5_internal.h) ───────────────────────────────────────────────

void suivi_recu(const std::string& payload) {
    if (payload_trop_long(kTag, payload.size())) return;
    const bool zone_avant = suivi_zone_disponible();
    // Lu à part : un payload non vide mais illisible garde l'état précédent ; seul un
    // payload vide dit « aucun capteur suivi ».
    SuiviLu lu[kSuivisMax];
    const int n = suivis_lire(Champ{payload.data(), payload.size()}, lu);
    if (n == 0 && !payload.empty()) {
        payload_refuse(kTag, "aucun capteur lisible", payload.size());
        return;
    }
    s_n = n;
    for (int i = 0; i < s_n; i++) {
        texte_ha_copier(s_c[i].nom, sizeof(s_c[i].nom), lu[i].nom.p, lu[i].nom.n);
        texte_ha_copier(s_c[i].unite, sizeof(s_c[i].unite), lu[i].unite.p, lu[i].unite.n);
        lu[i].nom = lu[i].unite = Champ{nullptr, 0};
        s_c[i].lu = lu[i];
    }
    s_recu = true;
    s_zone_sale = true;
    peindre();
    // Liste vidée ou remplie dans HA : la zone gauche saute le capteur, ou le retrouve.
    if (suivi_zone_disponible() != zone_avant) zone_gauche_appliquer();
}

void suivi_ouvrir() { peindre_popup(); }

void suivi_rejouer_theme() {
    if (ui().popup == nullptr && ui().zone == nullptr) return;
    s_zone_sale = true;
    peindre();
}

void suivi_zone_montrer(bool montre) {
    if (montre == s_zone_montre && !(montre && s_zone_sale)) return;
    s_zone_montre = montre;
    peindre_zone();
}

bool suivi_zone_disponible() { return !(s_recu && s_n == 0); }
