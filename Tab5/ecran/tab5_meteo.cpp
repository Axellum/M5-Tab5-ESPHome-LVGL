/**
 * [AI-CONTEXT]
 * @file tab5_meteo.cpp
 * @role Popup « Météo » (ADR-0043, 09/10/2026, demande d'Axel : « une météo graphique
 *       beaucoup plus visuelle, belle, claire et pratique, en plusieurs parties »). Trois
 *       pages, changées comme celles des Réglages (noms en haut, geste gauche / droite,
 *       instantané) :
 *         - « Aujourd'hui » : carte « Maintenant » (icône, température, condition, minimum
 *           et maximum du jour, pluie prévue sur les heures montrées), puis les heures qui
 *           viennent (cal_heures_data, 15 au plus) : heures, icônes, courbe lissée des
 *           températures (points et valeurs à la couleur de la température), barres de
 *           pluie en mm ; l'heure en cours sur une colonne teintée, « Demain » au passage
 *           de minuit ;
 *         - « 10 jours » : une ligne par jour à partir d'aujourd'hui (cal_jours_data, par
 *           cal_index_for_offset) : nom, icône, minimum, barre du minimum au maximum sur
 *           l'échelle commune des dix jours (dégradé des couleurs de la température),
 *           maximum ; la température du moment en point sur la ligne d'aujourd'hui ;
 *         - « Détails » : la pluie dans l'heure (les 9 barres de la carte centrale, à
 *           l'échelle du temps : 5 min puis 10 min, et sa phrase) ; humidité, indice UV,
 *           probabilités de gel et de neige.
 *       Ouvert par « Aller à l'écran → Météo », un geste de l'accueil (code « meteo »,
 *       ADR-0039) ou un appui long sur une tuile de prévision, hors de son appareil
 *       (forecast_hour_card.yaml, forecast_day_body.yaml).
 * @architecture_constraint Push-only (ADR-0001) et contrat inchangé : rien n'est demandé à
 *       HA, le popup lit ce que les tuiles et la carte centrale ont reçu (cal_heures_data,
 *       cal_jours_data, pluie_barre_niveau(), pluie_phrase_lue()) et garde la météo
 *       actuelle et les probabilités (paramètres de tab5_maj_meteo_actuelle et
 *       tab5_maj_probabilites, réservés jusque-là). Repeint à l'ouverture, au changement
 *       de page, à chaque poussée tant qu'il est affiché (meteo_donnees_changees) et au
 *       changement de thème. Widgets des tracés créés UNE fois, à la première ouverture ;
 *       écritures comparées d'abord (ui_text, ui_hidden, ui_poser…). Aucune couleur en
 *       dur : UIColor (ADR-0029).
 * @ai_warning lv_line_set_points() ne COPIE PAS le tableau de points : il vit ici, au
 *       niveau du fichier (s_courbe_pts, s_niveaux_pts), jamais dans une variable locale.
 *       LVGL 9.5 ne trace de pointillés que sur les lignes horizontales ou verticales
 *       (lv_draw_sw_line.c) : les niveaux de pluie, horizontaux, en ont ; la courbe non.
 * @ai_instruction Un texte affiché passe par tr() (Tab5/lang/*.yaml, gen_i18n.py). Une
 *       page de plus : sa valeur dans MeteoPage (avant METEO_NB_PAGES), son conteneur et son
 *       nom (reglages_onglet.yaml, `prefixe: meteo`) dans meteo_popup.yaml,
 *       leurs lignes dans tab5_meteo_ouvrir (tab5-meteo.yaml) et son cas dans peindre().
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include "lvgl.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <ctime>

MeteoUI g_meteo_ui;

namespace {

// ─── Données gardées : météo actuelle et probabilités (le reste est lu où il est) ──────

struct Actuelle {
    char condition[24] = {};
    float temperature = NAN;
    float humidite = NAN;
};
Actuelle s_actuelle;

struct Probabilites {
    float uv = NAN, gel = NAN, neige = NAN;
};
Probabilites s_probas;

// Page affichée (MeteoPage) : posée par meteo_afficher_page(), jamais ailleurs.
int s_page = METEO_PAGE_JOUR;

// État météo de HA → texte affiché (traduit à l'affichage). Un état absent : rien.
struct NomCondition {
    const char* cond;
    const char* texte;
};
constexpr NomCondition kConditions[] = {
    {"clear-night", tr_noop("Nuit claire")},
    {"cloudy", tr_noop("Nuageux")},
    {"exceptional", tr_noop("Temps exceptionnel")},
    {"fog", tr_noop("Brouillard")},
    {"hail", tr_noop("Grêle")},
    {"lightning", tr_noop("Orage")},
    {"thunder", tr_noop("Orage")},
    {"lightning-rainy", tr_noop("Orage et pluie")},
    {"partlycloudy", tr_noop("Éclaircies")},
    {"partlycloudy-night", tr_noop("Éclaircies")},
    {"partlycloudy_night", tr_noop("Éclaircies")},
    {"pouring", tr_noop("Fortes pluies")},
    {"rainy", tr_noop("Pluie")},
    {"snowy", tr_noop("Neige")},
    {"snowy-rainy", tr_noop("Pluie et neige")},
    {"sunny", tr_noop("Ensoleillé")},
    {"Clear", tr_noop("Ensoleillé")},
    {"windy", tr_noop("Venteux")},
    {"windy-variant", tr_noop("Venteux")},
};

const char* texte_condition(const char* cond) {
    for (const NomCondition& c : kConditions)
        if (std::strcmp(cond, c.cond) == 0) return tr(c.texte);
    return "";
}

// Hauteur d'une ligne de la police d'un libellé : « … » (LV_LABEL_LONG_MODE_DOTS) ne
// se pose qu'à hauteur fixe ; sans elle, le libellé passe à la ligne.
int32_t hauteur_ligne(lv_obj_t* o) {
    const lv_font_t* f = lv_obj_get_style_text_font(o, LV_PART_MAIN);
    return f != nullptr ? lv_font_get_line_height(f) : LV_SIZE_CONTENT;
}

// Les libellés du YAML coupés par « … » (long_mode: DOT) : une ligne de haut.
void une_ligne(lv_obj_t* o) {
    if (o == nullptr) return;
    lv_obj_set_height(o, hauteur_ligne(o));
    lv_label_set_long_mode(o, LV_LABEL_LONG_MODE_DOTS);
}

bool visible() {
    const MeteoUI& u = g_meteo_ui;
    return u.popup != nullptr && !lv_obj_has_flag(u.popup, LV_OBJ_FLAG_HIDDEN);
}

// « 18° », « -3° » (arrondi : jamais « -0° ») ; « --° » sans valeur.
void degres(char* out, size_t n, float t) {
    if (!std::isfinite(t)) snprintf(out, n, "--\xC2\xB0");
    else snprintf(out, n, "%ld\xC2\xB0", lroundf(t));
}

// Pluie en mm comme les tuiles horaires (refresh_hourly_forecast) : une décimale sous
// 10 mm, aucune au-delà.
void millimetres(char* out, size_t n, float mm) {
    snprintf(out, n, mm < 9.95f ? "%.1fmm" : "%.0fmm", mm);
}

// Couleur d'une pluie horaire (mm) : les rôles de la pluie dans l'heure.
uint32_t couleur_pluie_mm(float mm) {
    if (mm < 1.0f) return UIColor.RAIN_LIGHT;
    if (mm < 4.0f) return UIColor.RAIN_MODERATE;
    if (mm < 8.0f) return UIColor.RAIN_HEAVY;
    return UIColor.RAIN_EXTREME;
}

// Couleur d'un niveau de pluie dans l'heure (1 faible à 4 très forte).
uint32_t couleur_niveau(int niveau) {
    switch (niveau) {
        case 1: return UIColor.RAIN_LIGHT;
        case 2: return UIColor.RAIN_MODERATE;
        case 3: return UIColor.RAIN_HEAVY;
        case 4: return UIColor.RAIN_EXTREME;
        default: return UIColor.TEXT_PRIMARY;
    }
}

// Heure locale (tab5_time_source) ; faux tant que l'horloge n'est pas réglée.
bool heure_locale(struct tm& out) {
    const time_t now = tab5_time_source(nullptr);
    if (!tab5_heure_valide(now)) return false;
    localtime_r(&now, &out);
    return true;
}

// ─── Construction : briques ──────────────────────────────────────────────────────────────

// Un texte non cliquable, masqué : police, largeur fixe (texte coupé, jamais sur deux
// lignes) et alignement.
lv_obj_t* texte(lv_obj_t* parent, esphome::font::Font* police, int32_t largeur, lv_text_align_t align) {
    lv_obj_t* l = lv_label_create(parent);
    if (police != nullptr) esphome::lvgl::lv_obj_set_style_text_font(l, police, LV_PART_MAIN);
    lv_label_set_text(l, "");
    lv_obj_set_width(l, largeur);
    lv_label_set_long_mode(l, LV_LABEL_LONG_MODE_CLIP);
    lv_obj_set_style_text_align(l, align, LV_PART_MAIN);
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

// Icône à deux calques (update_meteo_icon) dans sa boîte : le calque 1 passe devant le 2
// (lv_obj_move_to_index), d'où une boîte par icône.
struct Icone {
    lv_obj_t* boite = nullptr;
    lv_obj_t* l1 = nullptr;
    lv_obj_t* l2 = nullptr;
    char cond[24] = {};   // condition peinte ("" : à repeindre)
};

void icone_creer(Icone& i, lv_obj_t* parent, int32_t w, int32_t h) {
    i.boite = lv_obj_create(parent);
    lv_obj_remove_style_all(i.boite);
    lv_obj_set_size(i.boite, w, h);
    lv_obj_remove_flag(i.boite, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(i.boite, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(i.boite, LV_OBJ_FLAG_HIDDEN);
    for (lv_obj_t** l : {&i.l2, &i.l1}) {
        *l = lv_label_create(i.boite);
        lv_label_set_text(*l, "");
        lv_obj_align(*l, LV_ALIGN_CENTER, 0, 0);
        lv_obj_add_flag(*l, LV_OBJ_FLAG_HIDDEN);
    }
}

// Pose l'icône d'une condition, seulement si elle change (update_meteo_icon écrit tout,
// sans comparer) ; polices de 48 et 32 px (40 % de celles des tuiles).
void icone_peindre(Icone& i, const std::string& cond) {
    if (i.l1 == nullptr) return;
    if (std::strncmp(i.cond, cond.c_str(), sizeof(i.cond) - 1) == 0 && i.cond[0] != '\0') return;
    snprintf(i.cond, sizeof(i.cond), "%s", cond.c_str());
    update_meteo_icon(i.l1, i.l2, cond, g_meteo_ui.icone_m, g_meteo_ui.icone_m_petite, 40);
}

// ─── Page « Aujourd'hui » : les heures (meteo_zone_heures, kGraphiqueL × 404) ───────────

constexpr int kHeuresMax = 15;                  // cal_heures_data
constexpr int kLisse = 8;                       // points de courbe par heure
constexpr int32_t kHeureY = 0;                  // heures (roboto_22)
constexpr int32_t kIconeY = 30;                 // icônes, boîtes de 72 × 56
constexpr int32_t kIconeL = 72;
constexpr int32_t kIconeH = 56;
constexpr int32_t kCourbeHaut = 132;            // y des points : du plus chaud…
constexpr int32_t kCourbeBas = 236;             // … au plus froid
constexpr int32_t kValeurDy = 36;               // valeur au-dessus de son point
constexpr int32_t kPoint = 12;
constexpr int32_t kPluieBase = 350;             // pied des barres de pluie
constexpr int32_t kPluieH = 70;                 // barre de la plus forte pluie
constexpr int32_t kPluieTexteY = 356;
constexpr int32_t kZoneHeuresH = 404;
constexpr float kPluieEchelleMin = 2.0f;        // mm : une bruine reste une petite barre

struct Heures {
    lv_obj_t* colonne = nullptr;                // l'heure en cours, teintée
    lv_obj_t* minuit = nullptr;                 // trait au passage de minuit
    lv_obj_t* base = nullptr;                   // pied des barres de pluie
    lv_obj_t* courbe = nullptr;
    lv_obj_t* vide = nullptr;
    lv_obj_t* heure[kHeuresMax] = {};
    Icone icone[kHeuresMax];
    lv_obj_t* barre[kHeuresMax] = {};
    lv_obj_t* pluie[kHeuresMax] = {};
    lv_obj_t* point[kHeuresMax] = {};
    lv_obj_t* valeur[kHeuresMax] = {};
};
Heures s_h;
lv_point_precise_t s_courbe_pts[(kHeuresMax - 1) * kLisse + 1];

// Créneaux lisibles, dans l'ordre depuis le premier : heure « HH:MM » et condition connue
// (HA pousse « 00:00 » / « unknown » pour un créneau qu'il n'a pas).
int heures_lisibles() {
    int n = 0;
    while (n < kHeuresMax && hhmm_minutes(cal_heures_data[n].heure_texte) >= 0 &&
           !cal_heures_data[n].condition.empty() && cal_heures_data[n].condition != "unknown")
        n++;
    return n;
}

void construire_heures() {
    lv_obj_t* z = g_meteo_ui.zone_heures;
    if (z == nullptr || s_h.courbe != nullptr) return;
    esphome::font::Font* p = g_meteo_ui.police;
    // Ordre de création = ordre de dessin.
    s_h.colonne = ui_rectangle(z, LV_OPA_10, 14);
    s_h.minuit = ui_rectangle(z, LV_OPA_40, 0);
    s_h.base = ui_rectangle(z, LV_OPA_40, 0);
    for (int k = 0; k < kHeuresMax; k++) s_h.barre[k] = ui_rectangle(z, LV_OPA_COVER, 4);
    s_h.courbe = ui_ligne(z, 4);
    for (int k = 0; k < kHeuresMax; k++) {
        s_h.point[k] = ui_rectangle(z, LV_OPA_COVER, LV_RADIUS_CIRCLE);
        s_h.heure[k] = texte(z, p, 80, LV_TEXT_ALIGN_CENTER);
        icone_creer(s_h.icone[k], z, kIconeL, kIconeH);
        s_h.valeur[k] = texte(z, p, 80, LV_TEXT_ALIGN_CENTER);
        s_h.pluie[k] = texte(z, p, 80, LV_TEXT_ALIGN_CENTER);
    }
    s_h.vide = texte(z, g_meteo_ui.police_grasse, kGraphiqueL, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(s_h.vide, LV_ALIGN_CENTER, 0, 0);
}

// Courbe lissée et monotone (Fritsch-Carlson) : elle passe par chaque point sans jamais
// dépasser deux valeurs voisines (pas de creux ni de bosse inventés entre deux heures).
int lisser(const float* xs, const float* ys, int n, lv_point_precise_t* out) {
    if (n < 2) return 0;
    float d[kHeuresMax], m[kHeuresMax];
    for (int k = 0; k < n - 1; k++) d[k] = (ys[k + 1] - ys[k]) / (xs[k + 1] - xs[k]);
    m[0] = d[0];
    m[n - 1] = d[n - 2];
    for (int k = 1; k < n - 1; k++) m[k] = (d[k - 1] * d[k] <= 0.0f) ? 0.0f : (d[k - 1] + d[k]) / 2.0f;
    for (int k = 0; k < n - 1; k++) {
        if (d[k] == 0.0f) {
            m[k] = m[k + 1] = 0.0f;
            continue;
        }
        const float a = m[k] / d[k], b = m[k + 1] / d[k], s = a * a + b * b;
        if (s > 9.0f) {
            const float t = 3.0f / std::sqrt(s);
            m[k] = t * a * d[k];
            m[k + 1] = t * b * d[k];
        }
    }
    int np = 0;
    for (int k = 0; k < n - 1; k++) {
        const float hx = xs[k + 1] - xs[k];
        for (int j = 0; j < kLisse; j++) {
            const float t = static_cast<float>(j) / kLisse, t2 = t * t, t3 = t2 * t;
            const float y = (2 * t3 - 3 * t2 + 1) * ys[k] + (t3 - 2 * t2 + t) * hx * m[k] +
                            (-2 * t3 + 3 * t2) * ys[k + 1] + (t3 - t2) * hx * m[k + 1];
            out[np].x = static_cast<lv_value_precise_t>(lroundf(xs[k] + t * hx));
            out[np].y = static_cast<lv_value_precise_t>(lroundf(y));
            np++;
        }
    }
    out[np].x = static_cast<lv_value_precise_t>(lroundf(xs[n - 1]));
    out[np].y = static_cast<lv_value_precise_t>(lroundf(ys[n - 1]));
    return np + 1;
}

void cacher_heures(int depuis) {
    for (int k = depuis; k < kHeuresMax; k++) {
        ui_hidden(s_h.heure[k], true);
        ui_hidden(s_h.icone[k].boite, true);
        ui_hidden(s_h.barre[k], true);
        ui_hidden(s_h.pluie[k], true);
        ui_hidden(s_h.point[k], true);
        ui_hidden(s_h.valeur[k], true);
    }
}

void peindre_heures(int n) {
    if (s_h.courbe == nullptr) return;
    ui_hidden(s_h.vide, n > 0);
    if (n == 0) {
        ui_text(s_h.vide, tr("En attente de Home Assistant"));
        ui_text_color(s_h.vide, UIColor.TEXT_DIM);
        for (lv_obj_t* o : {s_h.colonne, s_h.minuit, s_h.base, s_h.courbe}) ui_hidden(o, true);
        cacher_heures(0);
        ui_text(g_meteo_ui.pluie_titre, "");
        ui_text(g_meteo_ui.pluie_total, "");
        return;
    }
    const float largeur = static_cast<float>(kGraphiqueL) / n;
    const int32_t lib = largeur < 80.0f ? static_cast<int32_t>(largeur) : 80;
    // Échelle des températures (4 degrés au moins) et de la pluie.
    float lo = cal_heures_data[0].temp, hi = lo, mm_max = kPluieEchelleMin, somme = 0.0f;
    for (int k = 0; k < n; k++) {
        const HourForecastData& d = cal_heures_data[k];
        lo = std::fmin(lo, d.temp);
        hi = std::fmax(hi, d.temp);
        mm_max = std::fmax(mm_max, d.pluvio);
        somme += d.pluvio;
    }
    if (hi - lo < 4.0f) {
        const float c = (lo + hi) / 2.0f;
        lo = c - 2.0f;
        hi = c + 2.0f;
    }
    // Heure en cours : la colonne dont l'heure est celle de l'horloge.
    struct tm t{};
    const int maintenant = heure_locale(t) ? t.tm_hour : -1;
    int colonne = -1, minuit = -1;
    float xs[kHeuresMax], ys[kHeuresMax];
    float moyenne = 0.0f;
    char buf[24];
    for (int k = 0; k < n; k++) {
        const HourForecastData& d = cal_heures_data[k];
        const int h = hhmm_minutes(d.heure_texte) / 60;
        if (colonne < 0 && h == maintenant && k < 2) colonne = k;
        if (minuit < 0 && k > 0 && h < hhmm_minutes(cal_heures_data[k - 1].heure_texte) / 60) minuit = k;
        const int32_t cx = static_cast<int32_t>(lroundf((k + 0.5f) * largeur));
        xs[k] = static_cast<float>(cx);
        ys[k] = kCourbeBas - (d.temp - lo) / (hi - lo) * (kCourbeBas - kCourbeHaut);
        moyenne += d.temp / n;
        const uint32_t ct = get_temperature_color(d.temp);
        // Heure : « Demain » à minuit, l'heure en cours en couleur d'accent.
        lv_obj_t* lh = s_h.heure[k];
        ui_text(lh, k == minuit ? tr("Demain") : d.heure_texte.c_str());
        ui_text_color(lh, k == colonne ? UIColor.ACCENT : (k == minuit ? UIColor.TEXT_PRIMARY : UIColor.TEXT_SOFT));
        ui_poser(lh, cx - lib / 2, kHeureY, lib, LV_SIZE_CONTENT);
        icone_peindre(s_h.icone[k], d.condition);
        ui_poser(s_h.icone[k].boite, cx - kIconeL / 2, kIconeY, kIconeL, kIconeH);
        const int32_t py = static_cast<int32_t>(lroundf(ys[k]));
        ui_style_couleur(s_h.point[k], LV_STYLE_BG_COLOR, ct);
        ui_poser(s_h.point[k], cx - kPoint / 2, py - kPoint / 2, kPoint, kPoint);
        degres(buf, sizeof(buf), d.temp);
        ui_text(s_h.valeur[k], buf);
        ui_text_color(s_h.valeur[k], ct);
        ui_poser(s_h.valeur[k], cx - lib / 2, py - kValeurDy, lib, LV_SIZE_CONTENT);
        // Pluie : une barre et sa hauteur d'eau, rien sous 0,05 mm.
        const bool pleut = d.pluvio >= 0.05f;
        if (pleut) {
            int32_t hb = static_cast<int32_t>(lroundf(d.pluvio / mm_max * kPluieH));
            if (hb < 4) hb = 4;
            const int32_t lb = static_cast<int32_t>(largeur * 0.46f);
            ui_style_couleur(s_h.barre[k], LV_STYLE_BG_COLOR, couleur_pluie_mm(d.pluvio));
            ui_poser(s_h.barre[k], cx - lb / 2, kPluieBase - hb, lb, hb);
            millimetres(buf, sizeof(buf), d.pluvio);
            ui_text(s_h.pluie[k], buf);
            ui_poser(s_h.pluie[k], cx - lib / 2, kPluieTexteY, lib, LV_SIZE_CONTENT);
        } else {
            ui_hidden(s_h.barre[k], true);
            ui_hidden(s_h.pluie[k], true);
        }
    }
    cacher_heures(n);
    // Colonne de l'heure en cours, trait de minuit, pied des barres.
    if (colonne >= 0) {
        ui_poser(s_h.colonne, static_cast<int32_t>(colonne * largeur) + 3, 0, static_cast<int32_t>(largeur) - 6,
                 kZoneHeuresH);
    } else {
        ui_hidden(s_h.colonne, true);
    }
    if (minuit > 0) ui_poser(s_h.minuit, static_cast<int32_t>(lroundf(minuit * largeur)) - 1, 0, 2, kZoneHeuresH);
    else ui_hidden(s_h.minuit, true);
    ui_poser(s_h.base, 0, kPluieBase, kGraphiqueL, 1);
    // Courbe : couleur de la température moyenne des heures montrées.
    const int np = lisser(xs, ys, n, s_courbe_pts);
    ui_hidden(s_h.courbe, np < 2);
    if (np >= 2) {
        lv_obj_set_style_line_color(s_h.courbe, lv_color_hex(get_temperature_color(moyenne)), LV_PART_MAIN);
        lv_line_set_points(s_h.courbe, s_courbe_pts, static_cast<uint32_t>(np));
    }
    // Carte « Maintenant » : pluie prévue sur les heures montrées.
    const MeteoUI& u = g_meteo_ui;
    snprintf(buf, sizeof(buf), tr("Pluie sur %d h"), n);
    ui_text(u.pluie_titre, buf);
    if (somme >= 0.05f) {
        snprintf(buf, sizeof(buf), "%.1f mm", somme);
        ui_text(u.pluie_total, buf);
        ui_text_color(u.pluie_total, couleur_pluie_mm(somme));
    } else {
        ui_text(u.pluie_total, tr("Aucune"));
        ui_text_color(u.pluie_total, UIColor.TEXT_PRIMARY);
    }
}

// Couleurs fixes de ce qui est créé ici (le reste suit les valeurs à chaque peinture).
void couleurs_heures() {
    if (s_h.courbe == nullptr) return;
    ui_style_couleur(s_h.colonne, LV_STYLE_BG_COLOR, UIColor.ACCENT);
    ui_style_couleur(s_h.minuit, LV_STYLE_BG_COLOR, UIColor.TEXT_DIM);
    ui_style_couleur(s_h.base, LV_STYLE_BG_COLOR, UIColor.GLASS_RIM);
    for (lv_obj_t* o : s_h.pluie) ui_text_color(o, UIColor.RAIN_VALUE);
    for (lv_obj_t* o : s_h.point) {
        ui_style_num(o, LV_STYLE_BORDER_WIDTH, 2);
        ui_style_couleur(o, LV_STYLE_BORDER_COLOR, UIColor.BG);
        ui_style_num(o, LV_STYLE_BORDER_OPA, LV_OPA_COVER);
    }
}

// L'icône de la carte « Maintenant » est écrite sans comparer : seulement si elle change.
char s_icone_maintenant[24] = {};

// Carte « Maintenant » : la météo actuelle poussée, sinon la première heure.
void peindre_maintenant(int n) {
    const MeteoUI& u = g_meteo_ui;
    const char* cond = s_actuelle.condition;
    // Sans condition poussée (source indisponible), la température gardée ne vaut rien
    // non plus : HA envoie alors 0 (tab5_push.yaml), soit « 0° ». La première heure, sinon.
    float temp = cond[0] != '\0' ? s_actuelle.temperature : NAN;
    if (cond[0] == '\0' && n > 0) cond = cal_heures_data[0].condition.c_str();
    if (!std::isfinite(temp) && n > 0) temp = cal_heures_data[0].temp;
    if (cond[0] != '\0' && std::strncmp(s_icone_maintenant, cond, sizeof(s_icone_maintenant) - 1) != 0) {
        snprintf(s_icone_maintenant, sizeof(s_icone_maintenant), "%s", cond);
        update_meteo_icon(u.icone_l1, u.icone_l2, cond, u.icone, u.icone_petite);
    }
    ui_hidden(u.icone_l1, cond[0] == '\0');
    if (cond[0] == '\0') {
        // Icône cachée : la même condition, plus tard, la réécrit (et remontre son 2e calque).
        ui_hidden(u.icone_l2, true);
        s_icone_maintenant[0] = '\0';
    }
    char buf[48];
    degres(buf, sizeof(buf), temp);
    ui_text(u.temperature, buf);
    ui_text_color(u.temperature, std::isfinite(temp) ? get_temperature_color(temp) : UIColor.TEXT_DIM);
    ui_text(u.condition, cond[0] != '\0' ? texte_condition(cond) : tr("En attente de Home Assistant"));
    // Minimum et maximum d'aujourd'hui (prévision du jour).
    const int j = cal_index_for_offset(0);
    if (j >= 0 && !cal_jours_data[j].condition.empty()) {
        char mn[16], mx[16];
        degres(mn, sizeof(mn), cal_jours_data[j].tmin);
        degres(mx, sizeof(mx), cal_jours_data[j].tmax);
        snprintf(buf, sizeof(buf), tr("Minimum %s · Maximum %s"), mn, mx);
        ui_text(u.min_max, buf);
    } else {
        ui_text(u.min_max, "");
    }
}

// ─── Page « 10 jours » (meteo_zone_jours, kGraphiqueL × 580) ────────────────────────────

constexpr int kJours = 10;
constexpr int32_t kLigneH = 58;
constexpr int32_t kNomX = 12, kNomL = 230;
constexpr int32_t kJourIconeX = 252, kJourIconeL = 80;
constexpr int32_t kMinX = 340, kTempL = 100;
constexpr int32_t kPisteX = 460, kPisteL = 580, kPisteH = 10;
constexpr int32_t kMaxX = 1060;
constexpr int32_t kPointJour = 18;
constexpr int32_t kTexteDy = 10;                // texte de 38 px dans une ligne de 58

struct Jours {
    lv_obj_t* vide = nullptr;
    lv_obj_t* nom[kJours] = {};
    Icone icone[kJours];
    lv_obj_t* tmin[kJours] = {};
    lv_obj_t* piste[kJours] = {};
    lv_obj_t* barre[kJours] = {};
    lv_obj_t* tmax[kJours] = {};
    lv_obj_t* sep[kJours] = {};
    lv_obj_t* point = nullptr;                  // la température du moment, ligne 0
};
Jours s_j;

void construire_jours() {
    lv_obj_t* z = g_meteo_ui.zone_jours;
    if (z == nullptr || s_j.vide != nullptr) return;
    esphome::font::Font* p = g_meteo_ui.police_grasse;
    for (int r = 0; r < kJours; r++) {
        s_j.sep[r] = ui_rectangle(z, LV_OPA_40, 0);
        s_j.nom[r] = texte(z, p, kNomL, LV_TEXT_ALIGN_LEFT);
        lv_label_set_long_mode(s_j.nom[r], LV_LABEL_LONG_MODE_DOTS);
        icone_creer(s_j.icone[r], z, kJourIconeL, kLigneH);
        s_j.tmin[r] = texte(z, p, kTempL, LV_TEXT_ALIGN_RIGHT);
        s_j.piste[r] = ui_rectangle(z, LV_OPA_COVER, LV_RADIUS_CIRCLE);
        s_j.barre[r] = ui_rectangle(z, LV_OPA_COVER, LV_RADIUS_CIRCLE);
        lv_obj_set_style_bg_grad_dir(s_j.barre[r], LV_GRAD_DIR_HOR, LV_PART_MAIN);
        s_j.tmax[r] = texte(z, p, kTempL, LV_TEXT_ALIGN_LEFT);
    }
    s_j.point = ui_rectangle(z, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(s_j.point, 3, LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_j.point, LV_OPA_COVER, LV_PART_MAIN);
    s_j.vide = texte(z, p, kGraphiqueL, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(s_j.vide, LV_ALIGN_CENTER, 0, 0);
}

// Case de cal_jours_data du jour J+o : cal_index_for_offset() ; l'horloge pas encore
// réglée, la case o (les tuiles font de même).
int case_du_jour(int o) {
    const int j = cal_index_for_offset(o);
    if (j >= 0) return j;
    return local_day_number_today() < 0 ? o : -1;
}

void peindre_jours() {
    if (s_j.vide == nullptr) return;
    int cases[kJours], decalage[kJours];
    int n = 0;
    float lo = NAN, hi = NAN;
    for (int o = 0; o < 15 && n < kJours; o++) {
        const int j = case_du_jour(o);
        if (j < 0 || cal_jours_data[j].condition.empty()) continue;
        decalage[n] = o;
        cases[n++] = j;
        lo = std::isnan(lo) ? cal_jours_data[j].tmin : std::fmin(lo, cal_jours_data[j].tmin);
        hi = std::isnan(hi) ? cal_jours_data[j].tmax : std::fmax(hi, cal_jours_data[j].tmax);
    }
    const float actuelle = s_actuelle.temperature;
    if (n > 0 && std::isfinite(actuelle)) {
        lo = std::fmin(lo, actuelle);
        hi = std::fmax(hi, actuelle);
    }
    ui_hidden(s_j.vide, n > 0);
    if (n == 0) {
        ui_text(s_j.vide, tr("En attente de Home Assistant"));
        ui_text_color(s_j.vide, UIColor.TEXT_DIM);
    }
    if (hi - lo < 1.0f) hi = lo + 1.0f;
    const auto x_de = [lo, hi](float t) {
        return kPisteX + static_cast<int32_t>(lroundf((t - lo) / (hi - lo) * kPisteL));
    };
    const bool jour_connu = cal_index_for_offset(0) >= 0;
    char buf[32];
    for (int r = 0; r < kJours; r++) {
        const bool montre = r < n;
        for (lv_obj_t* o : {s_j.nom[r], s_j.tmin[r], s_j.piste[r], s_j.barre[r], s_j.tmax[r], s_j.icone[r].boite})
            if (!montre) ui_hidden(o, true);
        if (!montre || r == n - 1) ui_hidden(s_j.sep[r], true);
        if (!montre) continue;
        const DayForecastData& d = cal_jours_data[cases[r]];
        const int32_t y = r * kLigneH;
        // Nom : « Aujourd'hui », « Demain », puis « Mer 17 » (l'horloge pas encore réglée :
        // le nom poussé par HA).
        const int o = decalage[r];
        if (o == 0 && jour_connu) {
            ui_text(s_j.nom[r], tr("Aujourd'hui"));
        } else if (o == 1 && jour_connu) {
            ui_text(s_j.nom[r], tr("Demain"));
        } else if (jour_connu) {
            const std::string lib = format_short_day_label(o);
            ui_text(s_j.nom[r], lib.c_str());
        } else {
            ui_text(s_j.nom[r], ha_day_name(d.nom_jour));
        }
        ui_text_color(s_j.nom[r], o == 0 && jour_connu ? UIColor.INFO : UIColor.TEXT_PRIMARY);
        ui_poser(s_j.nom[r], kNomX, y + kTexteDy, kNomL, hauteur_ligne(s_j.nom[r]));
        icone_peindre(s_j.icone[r], d.condition);
        ui_poser(s_j.icone[r].boite, kJourIconeX, y, kJourIconeL, kLigneH);
        degres(buf, sizeof(buf), d.tmin);
        ui_text(s_j.tmin[r], buf);
        ui_text_color(s_j.tmin[r], get_temperature_color(d.tmin));
        ui_poser(s_j.tmin[r], kMinX, y + kTexteDy, kTempL, LV_SIZE_CONTENT);
        degres(buf, sizeof(buf), d.tmax);
        ui_text(s_j.tmax[r], buf);
        ui_text_color(s_j.tmax[r], get_temperature_color(d.tmax));
        ui_poser(s_j.tmax[r], kMaxX, y + kTexteDy, kTempL, LV_SIZE_CONTENT);
        const int32_t yb = y + (kLigneH - kPisteH) / 2;
        ui_poser(s_j.piste[r], kPisteX, yb, kPisteL, kPisteH);
        int32_t x0 = x_de(d.tmin), x1 = x_de(d.tmax);
        if (x1 - x0 < kPisteH) {
            const int32_t c = (x0 + x1) / 2;
            x0 = c - kPisteH / 2;
            x1 = c + kPisteH / 2;
        }
        ui_style_couleur(s_j.barre[r], LV_STYLE_BG_COLOR, get_temperature_color(d.tmin));
        ui_style_couleur(s_j.barre[r], LV_STYLE_BG_GRAD_COLOR, get_temperature_color(d.tmax));
        ui_poser(s_j.barre[r], x0, yb, x1 - x0, kPisteH);
        if (r < n - 1) ui_poser(s_j.sep[r], kNomX, y + kLigneH - 1, kGraphiqueL - 2 * kNomX, 1);
    }
    // La température du moment sur la ligne d'aujourd'hui.
    if (n > 0 && jour_connu && decalage[0] == 0 && std::isfinite(actuelle)) {
        ui_style_couleur(s_j.point, LV_STYLE_BG_COLOR, get_temperature_color(actuelle));
        ui_poser(s_j.point, x_de(actuelle) - kPointJour / 2, (kLigneH - kPointJour) / 2, kPointJour, kPointJour);
    } else {
        ui_hidden(s_j.point, true);
    }
}

void couleurs_jours() {
    if (s_j.vide == nullptr) return;
    for (lv_obj_t* o : s_j.sep) ui_style_couleur(o, LV_STYLE_BG_COLOR, UIColor.GLASS_RIM);
    for (lv_obj_t* o : s_j.piste) ui_style_couleur(o, LV_STYLE_BG_COLOR, UIColor.ARC_TRACK);
    ui_style_couleur(s_j.point, LV_STYLE_BORDER_COLOR, UIColor.TEXT_PRIMARY);
}

// ─── Page « Détails » : pluie dans l'heure (meteo_zone_pluie, kGraphiqueL × 290) ────────

constexpr int kBarres = 9;
// Bornes des barres en minutes : 5 min jusqu'à 30, puis 10 min (Météo-France ; les
// adaptateurs des autres fournisseurs suivent le même découpage).
constexpr int kBornes[kBarres + 1] = {0, 5, 10, 15, 20, 25, 30, 40, 50, 60};
constexpr int32_t kNiveauxL = 140;              // libellés des niveaux, à gauche
constexpr int32_t kTempsX0 = 160, kTempsX1 = 1150;
constexpr int32_t kPiedY = 226, kNiveauH = 48;  // niveau 4 en haut, à y 34
constexpr int32_t kAxeY = 244;
constexpr int32_t kAxeL = 140;
constexpr int kAxeMinutes[] = {0, 15, 30, 45, 60};
constexpr int kAxe = 5;
constexpr const char* kNiveaux[4] = {tr_noop("Faible"), tr_noop("Modérée"), tr_noop("Forte"),
                                     tr_noop("Très forte")};

struct Pluie {
    lv_obj_t* vide = nullptr;
    lv_obj_t* niveau[4] = {};                   // lignes pointillées
    lv_obj_t* niveau_nom[4] = {};
    lv_obj_t* barre[kBarres] = {};
    lv_obj_t* axe[kAxe] = {};
};
Pluie s_p;
lv_point_precise_t s_niveaux_pts[4][2];

int32_t x_minute(int m) { return kTempsX0 + (kTempsX1 - kTempsX0) * m / 60; }
int32_t y_niveau(int n) { return kPiedY - n * kNiveauH; }

void construire_pluie() {
    lv_obj_t* z = g_meteo_ui.zone_pluie;
    if (z == nullptr || s_p.vide != nullptr) return;
    esphome::font::Font* p = g_meteo_ui.police;
    for (int n = 0; n < 4; n++) {
        lv_obj_t* l = ui_ligne(z, 1);
        lv_obj_set_style_line_rounded(l, false, LV_PART_MAIN);
        lv_obj_set_style_line_dash_width(l, 6, LV_PART_MAIN);
        lv_obj_set_style_line_dash_gap(l, 6, LV_PART_MAIN);
        lv_obj_set_style_line_opa(l, LV_OPA_50, LV_PART_MAIN);
        s_niveaux_pts[n][0].x = static_cast<lv_value_precise_t>(kTempsX0);
        s_niveaux_pts[n][1].x = static_cast<lv_value_precise_t>(kTempsX1);
        s_niveaux_pts[n][0].y = s_niveaux_pts[n][1].y = static_cast<lv_value_precise_t>(y_niveau(n + 1));
        lv_line_set_points(l, s_niveaux_pts[n], 2);
        lv_obj_remove_flag(l, LV_OBJ_FLAG_HIDDEN);
        s_p.niveau[n] = l;
        lv_obj_t* t = texte(z, p, kNiveauxL, LV_TEXT_ALIGN_RIGHT);
        lv_label_set_text(t, tr(kNiveaux[n]));
        lv_obj_set_pos(t, 0, y_niveau(n + 1) - 13);
        lv_obj_remove_flag(t, LV_OBJ_FLAG_HIDDEN);
        s_p.niveau_nom[n] = t;
    }
    for (int i = 0; i < kBarres; i++) s_p.barre[i] = ui_rectangle(z, LV_OPA_COVER, 6);
    char buf[24];
    for (int a = 0; a < kAxe; a++) {
        const int m = kAxeMinutes[a];
        const bool premier = a == 0, dernier = a == kAxe - 1;
        lv_obj_t* t = texte(z, p, kAxeL, premier ? LV_TEXT_ALIGN_LEFT : (dernier ? LV_TEXT_ALIGN_RIGHT : LV_TEXT_ALIGN_CENTER));
        if (premier) {
            lv_label_set_text(t, tr("Maintenant"));
        } else {
            snprintf(buf, sizeof(buf), tr("%d min"), m);
            lv_label_set_text(t, buf);
        }
        const int32_t x = x_minute(m);
        lv_obj_set_pos(t, premier ? x : (dernier ? x - kAxeL : x - kAxeL / 2), kAxeY);
        lv_obj_remove_flag(t, LV_OBJ_FLAG_HIDDEN);
        s_p.axe[a] = t;
    }
    s_p.vide = texte(z, g_meteo_ui.police_grasse, kGraphiqueL, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(s_p.vide, LV_ALIGN_CENTER, 0, -20);
}

void peindre_pluie() {
    if (s_p.vide == nullptr) return;
    const bool recue = pluie_barre_niveau(0) >= 0;
    ui_hidden(s_p.vide, recue);
    if (!recue) {
        ui_text(s_p.vide, tr("En attente de Home Assistant"));
        ui_text_color(s_p.vide, UIColor.TEXT_DIM);
    }
    for (int i = 0; i < kBarres; i++) {
        const int niveau = pluie_barre_niveau(i);
        const int32_t x0 = x_minute(kBornes[i]) + 4, x1 = x_minute(kBornes[i + 1]) - 4;
        if (niveau <= 0) {
            // Sec : un trait au pied, la place de la barre reste lisible.
            ui_style_couleur(s_p.barre[i], LV_STYLE_BG_COLOR, UIColor.BAR_INACTIVE);
            if (recue) ui_poser(s_p.barre[i], x0, kPiedY - 4, x1 - x0, 4);
            else ui_hidden(s_p.barre[i], true);
            continue;
        }
        const int n = niveau > 4 ? 4 : niveau;
        ui_style_couleur(s_p.barre[i], LV_STYLE_BG_COLOR, couleur_niveau(n));
        ui_poser(s_p.barre[i], x0, y_niveau(n), x1 - x0, n * kNiveauH);
    }
    // La phrase de la carte centrale (« Pluie faible dans 12 mn »), à la couleur de son
    // niveau.
    int niveau = -2;
    const char* phrase = pluie_phrase_lue(niveau);
    const MeteoUI& u = g_meteo_ui;
    ui_text(u.pluie_phrase, phrase);
    ui_text_color(u.pluie_phrase, niveau >= 1 && niveau <= 4 ? couleur_niveau(niveau) : UIColor.TEXT_PRIMARY);
}

void couleurs_pluie() {
    if (s_p.vide == nullptr) return;
    for (lv_obj_t* o : s_p.niveau) lv_obj_set_style_line_color(o, lv_color_hex(UIColor.TEXT_DIM), LV_PART_MAIN);
    for (lv_obj_t* o : s_p.niveau_nom) ui_text_color(o, UIColor.TEXT_DIM);
    for (lv_obj_t* o : s_p.axe) ui_text_color(o, UIColor.TEXT_DIM);
}

// ─── Page « Détails » : humidité, UV, gel, neige (meteo_carte.yaml) ──────────────────────

// Indice UV → niveau de l'OMS et sa couleur (contexte « uv » : « Faible » d'un indice
// n'est pas celui d'une pluie, « Low » / « Light »).
const char* uv_niveau(float uv, uint32_t& couleur) {
    if (uv < 3.0f) {
        couleur = UIColor.SUCCESS;
        return tr_ctx("uv", "Faible");
    }
    if (uv < 6.0f) {
        couleur = UIColor.ALERT_YELLOW;
        return tr_ctx("uv", "Modéré");
    }
    if (uv < 8.0f) {
        couleur = UIColor.ALERT_ORANGE;
        return tr_ctx("uv", "Élevé");
    }
    if (uv < 11.0f) {
        couleur = UIColor.ALERT_RED;
        return tr_ctx("uv", "Très élevé");
    }
    couleur = UIColor.ALERT_RED;
    return tr_ctx("uv", "Extrême");
}

void peindre_carte(int c, const char* valeur, uint32_t couleur, const char* detail) {
    const MeteoUI& u = g_meteo_ui;
    ui_text(u.carte_valeur[c], valeur);
    ui_text_color(u.carte_valeur[c], couleur);
    ui_text(u.carte_detail[c], detail);
}

void peindre_cartes() {
    char buf[16];
    const float h = s_actuelle.humidite;
    if (std::isfinite(h)) {
        snprintf(buf, sizeof(buf), "%ld %%", lroundf(h));
        const char* d = h < 30.0f ? tr("Air sec") : (h <= 60.0f ? tr("Confortable") : tr("Air humide"));
        peindre_carte(METEO_CARTE_HUMIDITE, buf, get_humidity_color(h), d);
    } else {
        peindre_carte(METEO_CARTE_HUMIDITE, "--", UIColor.TEXT_DIM, "");
    }
    const float uv = s_probas.uv;
    if (std::isfinite(uv)) {
        uint32_t c = 0;
        const char* d = uv_niveau(uv, c);
        snprintf(buf, sizeof(buf), "%ld", lroundf(uv));
        peindre_carte(METEO_CARTE_UV, buf, c, d);
    } else {
        peindre_carte(METEO_CARTE_UV, "--", UIColor.TEXT_DIM, "");
    }
    const float probas[2] = {s_probas.gel, s_probas.neige};
    const uint32_t teintes[2] = {UIColor.TEMP_MIN, UIColor.METEO_PRECIP};
    for (int k = 0; k < 2; k++) {
        const float v = probas[k];
        if (std::isfinite(v)) {
            snprintf(buf, sizeof(buf), "%ld %%", lroundf(v));
            peindre_carte(METEO_CARTE_GEL + k, buf, v >= 5.0f ? teintes[k] : UIColor.TEXT_PRIMARY, tr("Probabilité"));
        } else {
            peindre_carte(METEO_CARTE_GEL + k, "--", UIColor.TEXT_DIM, "");
        }
    }
}

// ─── Ensemble ────────────────────────────────────────────────────────────────────────────

void construire() {
    const MeteoUI& u = g_meteo_ui;
    for (lv_obj_t* o : {u.condition, u.min_max, u.pluie_phrase}) une_ligne(o);
    for (lv_obj_t* o : u.carte_detail) une_ligne(o);
    construire_heures();
    construire_jours();
    construire_pluie();
    couleurs_heures();
    couleurs_jours();
    couleurs_pluie();
}

void peindre() {
    switch (s_page) {
        case METEO_PAGE_JOUR: {
            const int n = heures_lisibles();
            peindre_maintenant(n);
            peindre_heures(n);
            break;
        }
        case METEO_PAGE_JOURS:
            peindre_jours();
            break;
        case METEO_PAGE_DETAILS:
            peindre_pluie();
            peindre_cartes();
            break;
        default:
            break;
    }
}

// Geste gauche / droite dans le popup (ui_pages_geste) : page suivante ou précédente, en
// boucle (reglages_page_voisine, tab5_core.cpp : la même règle que les Réglages).
void page_voisine(bool suivante) {
    meteo_afficher_page(reglages_page_voisine(s_page, METEO_NB_PAGES, suivante));
}

}  // namespace

void meteo_ouvrir(int page) {
    MeteoUI& u = g_meteo_ui;
    if (u.popup == nullptr) return;
    if (s_h.courbe == nullptr) {
        construire();
        ui_pages_geste(u.popup, page_voisine);
    }
    meteo_afficher_page(page);
}

void meteo_afficher_page(int page) {
    const MeteoUI& u = g_meteo_ui;
    if (u.popup == nullptr) return;
    if (page < 0 || page >= METEO_NB_PAGES) page = METEO_PAGE_JOUR;
    s_page = page;
    for (int i = 0; i < METEO_NB_PAGES; i++) ui_hidden(u.page[i], i != page);
    ui_choix_peindre(u.onglet, METEO_NB_PAGES, page);
    peindre();
}

void meteo_actuelle_recue(const std::string& condition, float temperature, float humidite) {
    snprintf(s_actuelle.condition, sizeof(s_actuelle.condition), "%s",
             condition == "unknown" || condition == "unavailable" ? "" : condition.c_str());
    s_actuelle.temperature = temperature;
    s_actuelle.humidite = humidite;
    meteo_donnees_changees();
}

void meteo_probabilites_recues(float uv, float gel, float neige) {
    s_probas.uv = uv;
    s_probas.gel = gel;
    s_probas.neige = neige;
    meteo_donnees_changees();
}

bool meteo_tuile_libre(lv_obj_t* bouton_appareil) {
    return bouton_appareil == nullptr || lv_obj_has_flag(bouton_appareil, LV_OBJ_FLAG_HIDDEN);
}

void meteo_donnees_changees() {
    if (visible()) peindre();
}

// Changement de thème (theme_rejouer_ui, tab5_theme.cpp) : couleurs fixes, icônes
// (caches vidés : update_meteo_icon reprend la palette) ; la page seulement si le popup
// est affiché (sinon, son ouverture la repeint).
void meteo_rejouer_theme() {
    if (g_meteo_ui.popup == nullptr || s_h.courbe == nullptr) return;
    couleurs_heures();
    couleurs_jours();
    couleurs_pluie();
    for (Icone& i : s_h.icone) i.cond[0] = '\0';
    for (Icone& i : s_j.icone) i.cond[0] = '\0';
    s_icone_maintenant[0] = '\0';
    if (!visible()) return;
    ui_choix_peindre(g_meteo_ui.onglet, METEO_NB_PAGES, s_page);
    peindre();
}
