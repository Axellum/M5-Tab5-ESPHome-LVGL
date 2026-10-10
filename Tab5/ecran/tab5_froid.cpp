/**
 * [AI-CONTEXT]
 * @file tab5_froid.cpp
 * @role Suivi du froid (ADR-0055, 10/10/2026, demande d'Axel : « un suivi de frigo et
 *       congélateur, une surveillance de la température… une icône qui vient se mettre
 *       dans le coin droit de l'horloge et qui clignote… une pop up de gestion avec courbe
 *       et alertes »). Peint, d'après ce que pousse tab5_maj_froid (lu par froid_lire(),
 *       Tab5/socle/tab5_parse.h, section 12) :
 *         - le popup « Froid » (froid_popup.yaml) : une carte par appareil
 *           (froid_carte.yaml, quatre au plus), une rangée pour un ou deux, deux rangées
 *           de deux au-delà (la dernière centrée) ; chaque carte : icône (réfrigérateur ou
 *           flocon), nom, température colorée par niveau, statut en clair, extrêmes des
 *           24 h et norme, courbe des 24 h avec les lignes de la norme (pointillés), dernier
 *           incident ;
 *         - l'icône « fridge-alert » du coin haut-droit de l'horloge (btn_froid_alerte,
 *           tab5-lvgl.yaml) : montrée tant qu'un appareil est au niveau 2, elle clignote
 *           par un lv_timer de 500 ms créé une fois, mis en pause quand elle est cachée.
 * @architecture_constraint Push-only et events-only (ADR-0001, ADR-0025) : rien n'est
 *       demandé à HA ; aucune entité nommée ; AUCUNE norme ni seuil ici (HA pousse niveau,
 *       cause et norme). Couleurs par les rôles de la palette (ADR-0029) : niveau 0
 *       SUCCESS, 1 WARNING, 2 ERROR, inconnue TEXT_DIM ; courbe ACCENT, norme WARNING ;
 *       repeintes par froid_rejouer_theme(). Clignotement SANS fondu (préférence d'Axel :
 *       transitions instantanées) : seul le glyphe est basculé, le bouton reste touchable.
 *       Courbes : lv_line (lv_chart et lv_canvas ne sont pas compilés), créées à la
 *       première peinture ; écritures comparées d'abord (ui_text, ui_poser…).
 * @ai_warning lv_line_set_points() ne COPIE PAS les points : s_pts, s_haut et s_bas vivent
 *       au niveau du fichier. Glyphes des cartes posés d'ici (glyphe_appareil, mdi_font_45) :
 *       MDI_CODE_TARGETS de tools/check_tab5_code_rules.py (règle 9).
 * @ai_instruction Un widget de plus = son champ dans FroidUI (tab5_froid.h) et sa ligne
 *       dans le script tab5_froid_lier (tab5-froid.yaml). Un texte affiché passe par tr().
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

FroidUI g_froid_ui;

namespace {

static_assert(kFroidCartes == kFroidMax, "une carte par appareil");
static_assert(kFroidPointsMax <= kCourbeLissePoints, "ui_courbe_lisse ne trace pas plus de points");

constexpr const char* kTag = "tab5.froid";
constexpr size_t kNomMax = 48;
constexpr int kLisse = 6;  // points de courbe entre deux points poussés
constexpr int kCourbeMax = (kFroidPointsMax - 1) * kLisse + 1;
constexpr uint32_t kClignoteMs = 500;
// Popup : grille dans le corps de la carte modale (x kCorpsX, y kCorpsY..kCorpsBas).
constexpr int32_t kCorpsBas = kCarteH - 20;
// Une carte : marges, rangée du haut (icône, nom, température), statut, extrêmes, courbe,
// dernier incident en bas.
constexpr int32_t kMarge = 22;
constexpr int32_t kHautY = 10;
constexpr int32_t kIconeL = 56;        // icône (mdi_font_45) et son air
constexpr int32_t kEcart = 6;          // entre deux rangées de texte
constexpr int32_t kCourbeEcart = 14;   // sous les extrêmes, au-dessus du dernier incident
constexpr int32_t kTrait = 4;
constexpr int32_t kTraitNorme = 2;
constexpr int32_t kTiret = 10;         // pointillés de la norme : tiret, puis vide
constexpr int32_t kVide = 8;
constexpr int32_t kPoint = 10;         // dernier point de la courbe
constexpr int32_t kAnneau = 3;         // son anneau, à la couleur du fond


// Une courbe et les deux lignes de la norme (créées à la première peinture).
struct Trace {
    lv_obj_t* ligne = nullptr;
    lv_obj_t* point = nullptr;
    lv_obj_t* haut = nullptr;
    lv_obj_t* bas = nullptr;
};

// Les appareils reçus : leurs noms copiés à part (le payload ne vit pas ; s_lu[i].nom vide).
FroidLu s_lu[kFroidMax];
char s_nom[kFroidMax][kNomMax + 1] = {};
int s_n = 0;
bool s_recu = false;  // une poussée de HA est arrivée depuis le démarrage
Trace s_trace[kFroidCartes];
lv_point_precise_t s_pts[kFroidCartes][kCourbeMax];
lv_point_precise_t s_haut[kFroidCartes][2];
lv_point_precise_t s_bas[kFroidCartes][2];
// Icône de l'horloge : le minuteur du clignotement (créé une fois), s'il tourne, et la
// phase montrée.
lv_timer_t* s_clignote = nullptr;
bool s_clignote_actif = false;
bool s_clignote_visible = true;

const FroidUI& ui() { return g_froid_ui; }

bool popup_ouvert() { return ui().popup != nullptr && !lv_obj_has_flag(ui().popup, LV_OBJ_FLAG_HIDDEN); }

// Icône d'une carte (mdi_font_45) : réfrigérateur ou flocon.
const char* glyphe_appareil(FroidType t) {
    return t == FroidType::CONGELATEUR ? "\U000F0717"  // snowflake
                                       : "\U000F0290"; // fridge
}

uint32_t couleur_niveau(const FroidLu& a) {
    if (!std::isfinite(a.valeur) && a.cause != FroidCause::INDISPO) return UIColor.TEXT_DIM;
    switch (a.niveau) {
        case 0: return UIColor.SUCCESS;
        case 1: return UIColor.WARNING;
        default: return UIColor.ERROR;
    }
}

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

// « 14 h 32 » aujourd'hui, « hier 14 h 32 », « Lun 5 14 h 32 » un autre jour (heure
// locale de la tablette ; avant que l'heure soit réglée, l'heure seule).
void quand_txt(char* out, size_t n, uint32_t epoch) {
    const time_t t = static_cast<time_t>(epoch);
    struct tm lt = {};
    localtime_r(&t, &lt);
    char hm[24];
    snprintf(hm, sizeof(hm), tr("%d h %02d"), lt.tm_hour, lt.tm_min);
    const time_t maintenant = tab5_time_source(nullptr);
    struct tm auj = {};
    localtime_r(&maintenant, &auj);
    const time_t veille_t = maintenant - 24 * 3600;
    struct tm veille = {};
    localtime_r(&veille_t, &veille);
    if (!tab5_heure_valide(maintenant) || (lt.tm_year == auj.tm_year && lt.tm_yday == auj.tm_yday)) {
        snprintf(out, n, "%s", hm);
    } else if (lt.tm_year == veille.tm_year && lt.tm_yday == veille.tm_yday) {
        snprintf(out, n, tr("hier %s"), hm);
    } else {
        snprintf(out, n, "%s %d %s", day_short_utf8(lt.tm_wday), lt.tm_mday, hm);
    }
}

// Statut en clair : « Conforme », « Trop chaud depuis 14 h 32 », « Porte ouverte ? »…
void statut_txt(const FroidLu& a, char* out, size_t n) {
    char depuis[48] = "";
    if (a.depuis > 0) quand_txt(depuis, sizeof(depuis), a.depuis);
    const bool d = depuis[0] != '\0';
    switch (a.cause) {
        case FroidCause::CHAUD:
            if (a.niveau >= 2) snprintf(out, n, d ? tr("Coup de chaud depuis %s") : tr("Coup de chaud"), depuis);
            else snprintf(out, n, d ? tr("Trop chaud depuis %s") : tr("Trop chaud"), depuis);
            return;
        case FroidCause::FROID:
            snprintf(out, n, d ? tr("Trop froid depuis %s") : tr("Trop froid"), depuis);
            return;
        case FroidCause::PORTE:
            snprintf(out, n, "%s", a.niveau >= 2 ? tr("Porte mal fermée") : tr("Porte ouverte ?"));
            return;
        case FroidCause::INDISPO:
            snprintf(out, n, "%s", tr("Capteur muet"));
            return;
        default:
            break;
    }
    if (a.niveau > 0) snprintf(out, n, "%s", tr("À vérifier"));
    else if (!std::isfinite(a.valeur)) snprintf(out, n, "%s", tr("Mesure en attente"));
    else if (std::isfinite(a.haut) && a.valeur > a.haut) snprintf(out, n, "%s", tr("Au-dessus de la norme"));
    else if (std::isfinite(a.bas) && a.valeur < a.bas) snprintf(out, n, "%s", tr("Sous la norme"));
    else snprintf(out, n, "%s", tr("Conforme"));
}

// « Min 2.8 °C · max 9.4 °C · norme 0.0 à 5.0 °C » (congélateur : « norme -18.0 °C au
// plus ») ; les parties inconnues sont omises.
void extremes_txt(const FroidLu& a, char* out, size_t n) {
    out[0] = '\0';
    char lo[16], hi[16], t[64];
    if (std::isfinite(a.min) && std::isfinite(a.max)) {
        froid_temperature_texte(a.min, lo, sizeof(lo));
        froid_temperature_texte(a.max, hi, sizeof(hi));
        snprintf(out, n, tr("Min %s °C · max %s °C"), lo, hi);
    }
    t[0] = '\0';
    if (std::isfinite(a.haut) && std::isfinite(a.bas)) {
        froid_temperature_texte(a.bas, lo, sizeof(lo));
        froid_temperature_texte(a.haut, hi, sizeof(hi));
        snprintf(t, sizeof(t), tr("norme %s à %s °C"), lo, hi);
    } else if (std::isfinite(a.haut)) {
        froid_temperature_texte(a.haut, hi, sizeof(hi));
        snprintf(t, sizeof(t), tr("norme %s °C au plus"), hi);
    }
    if (t[0] != '\0') {
        const size_t l = std::strlen(out);
        snprintf(out + l, n - l, "%s%s", l > 0 ? " · " : "", t);
    }
}

// « Dernier incident : porte ouverte, hier 14 h 32, 12 min, 9.1 °C » ou « Aucun incident ».
void incident_txt(const FroidIncident& i, char* out, size_t n) {
    const char* cause = nullptr;
    switch (i.cause) {
        case FroidCause::CHAUD: cause = tr("trop chaud"); break;
        case FroidCause::FROID: cause = tr("trop froid"); break;
        case FroidCause::PORTE: cause = tr("porte ouverte"); break;
        case FroidCause::INDISPO: cause = tr("capteur muet"); break;
        default: break;
    }
    if (cause == nullptr) {
        snprintf(out, n, "%s", tr("Aucun incident"));
        return;
    }
    char quand[48] = "";
    if (i.debut > 0) quand_txt(quand, sizeof(quand), i.debut);
    char duree[24];
    if (i.duree_min < 60) snprintf(duree, sizeof(duree), tr("%u min"), static_cast<unsigned>(i.duree_min));
    else snprintf(duree, sizeof(duree), tr("%u h %02u"), static_cast<unsigned>(i.duree_min / 60),
                  static_cast<unsigned>(i.duree_min % 60));
    char max[16];
    froid_temperature_texte(i.max, max, sizeof(max));
    int r = snprintf(out, n, tr("Dernier incident : %s"), cause);
    // Chaque partie connue, après une virgule.
    for (const char* p : {static_cast<const char*>(quand), static_cast<const char*>(duree)}) {
        if (r < 0 || static_cast<size_t>(r) >= n || p[0] == '\0') continue;
        r += snprintf(out + r, n - r, ", %s", p);
    }
    if (r >= 0 && static_cast<size_t>(r) < n && std::isfinite(i.max)) snprintf(out + r, n - r, ", %s °C", max);
}

void creer_trace(Trace& t, lv_obj_t* parent) {
    if (t.ligne != nullptr || parent == nullptr) return;
    // Ordre de création = ordre de dessin : la norme sous la courbe.
    t.haut = ui_ligne(parent, kTraitNorme);
    t.bas = ui_ligne(parent, kTraitNorme);
    for (lv_obj_t* l : {t.haut, t.bas}) {
        ui_style_num(l, LV_STYLE_LINE_ROUNDED, 0);
        ui_style_num(l, LV_STYLE_LINE_DASH_WIDTH, kTiret);
        ui_style_num(l, LV_STYLE_LINE_DASH_GAP, kVide);
    }
    t.ligne = ui_ligne(parent, kTrait);
    t.point = ui_rectangle(parent, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    ui_style_num(t.point, LV_STYLE_BORDER_WIDTH, kAnneau);
    ui_style_num(t.point, LV_STYLE_BORDER_OPA, LV_OPA_COVER);
}

void cacher_trace(const Trace& t) {
    for (lv_obj_t* o : {t.ligne, t.point, t.haut, t.bas}) ui_hidden(o, true);
}

// Une ligne de la norme à la température v (pointillés, de x0 à x1), cachée hors de
// l'échelle ou inconnue.
void ligne_norme(lv_obj_t* l, lv_point_precise_t* pts, float v, float bas, float haut, int32_t x0, int32_t x1,
                 int32_t y_haut, int32_t y_bas) {
    if (l == nullptr) return;
    if (!std::isfinite(v) || v < bas || v > haut) {
        ui_hidden(l, true);
        return;
    }
    const float y = static_cast<float>(y_bas) - (v - bas) / (haut - bas) * static_cast<float>(y_bas - y_haut);
    pts[0] = {static_cast<lv_value_precise_t>(x0), static_cast<lv_value_precise_t>(lroundf(y))};
    pts[1] = {static_cast<lv_value_precise_t>(x1), static_cast<lv_value_precise_t>(lroundf(y))};
    ui_style_couleur(l, LV_STYLE_LINE_COLOR, UIColor.WARNING);
    lv_line_set_points(l, pts, 2);
    ui_hidden(l, false);
}

// La courbe de l'appareil i entre x0 et x1, de y_haut à y_bas, avec les lignes de sa norme.
// Les heures sans mesure laissent un trou dans l'axe du temps : la courbe passe par les
// points voisins. Moins de deux mesures : la courbe est cachée, la norme reste.
void tracer(int i, int32_t x0, int32_t x1, int32_t y_haut, int32_t y_bas) {
    const FroidLu& a = s_lu[i];
    const Trace& t = s_trace[i];
    float bas = 0, haut = 0;
    if (!froid_echelle(a, bas, haut)) {
        cacher_trace(t);
        return;
    }
    ligne_norme(t.haut, s_haut[i], a.haut, bas, haut, x0, x1, y_haut, y_bas);
    ligne_norme(t.bas, s_bas[i], a.bas, bas, haut, x0, x1, y_haut, y_bas);
    float xs[kFroidPointsMax], ys[kFroidPointsMax];
    int m = 0;
    const int n = a.n;
    for (int k = 0; k < n && n >= 2; k++) {
        if (!std::isfinite(a.points[k])) continue;
        xs[m] = static_cast<float>(x0) + static_cast<float>(x1 - x0) * static_cast<float>(k) / static_cast<float>(n - 1);
        ys[m] = static_cast<float>(y_bas) - (a.points[k] - bas) / (haut - bas) * static_cast<float>(y_bas - y_haut);
        m++;
    }
    const int np = ui_courbe_lisse(xs, ys, m, kLisse, s_pts[i]);
    if (np < 2) {
        ui_hidden(t.ligne, true);
        ui_hidden(t.point, true);
        return;
    }
    ui_style_couleur(t.ligne, LV_STYLE_LINE_COLOR, UIColor.ACCENT);
    lv_line_set_points(t.ligne, s_pts[i], static_cast<uint32_t>(np));
    ui_hidden(t.ligne, false);
    ui_style_couleur(t.point, LV_STYLE_BG_COLOR, couleur_niveau(a));
    ui_style_couleur(t.point, LV_STYLE_BORDER_COLOR, UIColor.BG);
    const int32_t px = static_cast<int32_t>(lroundf(xs[m - 1])), py = static_cast<int32_t>(lroundf(ys[m - 1]));
    ui_poser(t.point, px - kPoint / 2, py - kPoint / 2, kPoint, kPoint);
}

// ─── Popup ───

void peindre_carte(int i, int32_t x, int32_t y, int32_t w, int32_t h) {
    const FroidUI& u = ui();
    lv_obj_t* carte = u.carte[i];
    if (carte == nullptr) return;
    ui_poser(carte, x, y, w, h);
    const FroidLu& a = s_lu[i];
    const int32_t cw = utile(carte, true), ch = utile(carte, false);
    const uint32_t couleur = couleur_niveau(a);
    // En haut : l'icône, le nom, la température à droite (police de la date).
    ui_text(u.icone[i], glyphe_appareil(a.type));
    ui_text_color(u.icone[i], couleur);
    poser(u.icone[i], kMarge - 4, kHautY);
    char t[24], valeur[32];
    froid_temperature_texte(a.valeur, t, sizeof(t));
    snprintf(valeur, sizeof(valeur), std::isfinite(a.valeur) ? "%s °C" : "%s", t);
    ui_text(u.valeur[i], valeur);
    ui_text_color(u.valeur[i], couleur);
    const int32_t lh_v = hauteur_ligne(u.valeur[i]);
    const int32_t x_valeur = cw - kMarge - largeur_texte(u.valeur[i], valeur);
    poser(u.valeur[i], x_valeur, kHautY);
    const int32_t lh_icone = hauteur_ligne(u.icone[i]);
    const int32_t haut_rangee = lh_v > lh_icone ? lh_v : lh_icone;
    const int32_t x_nom = kMarge - 4 + kIconeL;
    poser(u.nom[i], x_nom, kHautY + (haut_rangee - hauteur_ligne(u.nom[i])) / 2);
    texte_ha_coupe(u.nom[i], s_nom[i][0] != '\0' ? s_nom[i] : (a.type == FroidType::CONGELATEUR ? tr("Congélateur") : tr("Réfrigérateur")),
                   x_valeur - 16 - x_nom);
    // Statut coloré, puis extrêmes et norme.
    char texte[160];
    statut_txt(a, texte, sizeof(texte));
    ui_text_color(u.statut[i], couleur);
    const int32_t y_statut = kHautY + haut_rangee + kEcart;
    poser(u.statut[i], kMarge, y_statut);
    texte_ha_coupe(u.statut[i], texte, cw - 2 * kMarge);
    extremes_txt(a, texte, sizeof(texte));
    const int32_t y_extremes = y_statut + hauteur_ligne(u.statut[i]) + kEcart;
    ui_hidden(u.extremes[i], texte[0] == '\0');
    if (texte[0] != '\0') {
        poser(u.extremes[i], kMarge, y_extremes);
        texte_ha_coupe(u.extremes[i], texte, cw - 2 * kMarge);
    }
    // En bas : le dernier incident.
    incident_txt(a.dernier, texte, sizeof(texte));
    const int32_t lh_i = hauteur_ligne(u.incident[i]);
    const int32_t y_incident = ch - kMarge / 2 - lh_i;
    poser(u.incident[i], kMarge, y_incident);
    texte_ha_coupe(u.incident[i], texte, cw - 2 * kMarge);
    // La courbe entre les deux.
    creer_trace(s_trace[i], carte);
    const int32_t y_haut = y_extremes + hauteur_ligne(u.extremes[i]) + kCourbeEcart;
    const int32_t y_bas = y_incident - kCourbeEcart;
    if (y_bas - y_haut < 24) {
        cacher_trace(s_trace[i]);
        return;
    }
    tracer(i, kMarge, cw - kMarge, y_haut, y_bas);
}

void peindre_popup() {
    const FroidUI& u = ui();
    if (u.popup == nullptr) return;
    // Sans appareil : la phrase (en attente, ou aucun déclaré et où les déclarer).
    const bool vide = !s_recu || s_n == 0;
    ui_hidden(u.attente, !vide);
    ui_hidden(u.conseil, !(s_recu && s_n == 0));
    if (vide) ui_text(u.attente, s_recu ? tr("Aucun appareil déclaré") : tr("En attente de Home Assistant"));
    if (s_recu && s_n == 0) {
        ui_text(u.conseil, tr("Déclarez-les dans Home Assistant : listes « Tab5 · réfrigérateurs » et « Tab5 · congélateurs »."));
    }
    for (int i = s_n; i < kFroidCartes; i++) ui_hidden(u.carte[i], true);
    if (vide) return;
    // Grille : une rangée pour un ou deux appareils, deux rangées de deux au-delà (la
    // dernière, incomplète, centrée) : des cartes assez larges pour leurs textes.
    const int colonnes = s_n == 1 ? 1 : 2;
    const int rangees = (s_n + colonnes - 1) / colonnes;
    const int32_t w = (kCorpsW - (colonnes - 1) * kCartesEcart) / colonnes;
    const int32_t h = (kCorpsBas - kCorpsY - (rangees - 1) * kCartesEcart) / rangees;
    for (int i = 0; i < s_n; i++) {
        const int r = i / colonnes, c = i % colonnes;
        const int dans_rangee = r == rangees - 1 ? s_n - r * colonnes : colonnes;
        const int32_t largeur_rangee = dans_rangee * w + (dans_rangee - 1) * kCartesEcart;
        const int32_t x = kCorpsX + (kCorpsW - largeur_rangee) / 2 + c * (w + kCartesEcart);
        peindre_carte(i, x, kCorpsY + r * (h + kCartesEcart), w, h);
    }
}

// ─── Icône du coin de l'horloge ───

// Popup Froid ouvert (il couvre l'horloge, et dit déjà tout) : l'icône reste allumée,
// sans clignoter derrière lui — et la capture hors tablette du popup ne dépend pas de
// l'instant où elle est prise.
void clignoter(lv_timer_t*) {
    s_clignote_visible = popup_ouvert() || !s_clignote_visible;
    ui_hidden(ui().alerte_icone, !s_clignote_visible);
}

// Montrée (et clignotante) tant qu'un appareil est au niveau 2 ; cachée sinon, minuteur
// en pause. Pas d'acquittement : elle reste tant que dure le niveau grave.
void peindre_alerte() {
    const FroidUI& u = ui();
    if (u.alerte == nullptr) return;
    const bool montre = s_recu && froid_niveau_max(s_lu, s_n) >= 2;
    ui_hidden(u.alerte, !montre);
    if (montre) {
        if (s_clignote == nullptr) s_clignote = lv_timer_create(clignoter, kClignoteMs, nullptr);
        else if (!s_clignote_actif) lv_timer_resume(s_clignote);
        if (!s_clignote_actif) {
            s_clignote_visible = true;
            ui_hidden(u.alerte_icone, false);
        }
        s_clignote_actif = true;
    } else if (s_clignote != nullptr && s_clignote_actif) {
        lv_timer_pause(s_clignote);
        s_clignote_actif = false;
    }
}

}  // namespace

// ─── API (tab5_froid.h, tab5_internal.h) ───────────────────────────────────────────────

void froid_recu(const std::string& payload) {
    if (payload_trop_long(kTag, payload.size())) return;
    // Lu à part : un payload non vide mais illisible garde l'état précédent ; seul un
    // payload vide dit « aucun appareil déclaré ».
    FroidLu lu[kFroidMax];
    const int n = froid_lire(Champ{payload.data(), payload.size()}, lu);
    if (n == 0 && !payload.empty()) {
        payload_refuse(kTag, "aucun appareil lisible", payload.size());
        return;
    }
    s_n = n;
    for (int i = 0; i < s_n; i++) {
        texte_ha_copier(s_nom[i], sizeof(s_nom[i]), lu[i].nom.p, lu[i].nom.n);
        lu[i].nom = Champ{nullptr, 0};
        s_lu[i] = lu[i];
    }
    s_recu = true;
    if (popup_ouvert()) peindre_popup();
    peindre_alerte();
}

void froid_ouvrir() { peindre_popup(); }

void froid_rejouer_theme() {
    if (popup_ouvert()) peindre_popup();
}

bool froid_disponible() { return !(s_recu && s_n == 0); }
