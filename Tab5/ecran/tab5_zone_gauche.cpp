/**
 * [AI-CONTEXT]
 * @file tab5_zone_gauche.cpp
 * @role Zone à gauche de l'horloge au choix (ADR-0051, 10/10/2026, demande d'Axel : « sur
 *       cette zone, j'aimerais qu'on puisse choisir soit le vocal, soit un lecteur audio,
 *       soit un graphique ; très beau, léger »). Deux contenus aujourd'hui :
 *         - « vocal » : le conteneur zone_vocal de tab5-lvgl.yaml (micro, Domo, Discu),
 *           montré ou masqué d'un bloc ; ses widgets, leurs gestes et le masquage de Domo /
 *           Discu sans pipeline de discussion (zone « discussion », tab5_zones.cpp) ne
 *           changent pas ;
 *         - « graphique » : dans la carte zone_graphique, les heures qui viennent
 *           (cal_heures_data, 15 au plus) : courbe lissée des températures à la couleur de
 *           leur moyenne, un point et sa valeur au plus chaud et au plus froid, barres de
 *           pluie en mm (couleurs de la pluie), trois heures sous le pied des barres, le
 *           cumul de pluie à droite. Une trentaine d'objets, créés à la première
 *           apparition.
 *       « lecteur » (le lecteur audio compact, lot 2 sur le lecteur de l'ADR-0050) est lu,
 *       gardé et sauté tant que disponible() ne le connaît pas.
 *       Ce qui est montré : le contenu courant (NVS), sinon celui de départ, sinon le vocal.
 *       Le blueprint choisit le départ et les contenus du cycle (clé « gauche »,
 *       zone_gauche_lire dans Tab5/socle/tab5_parse.cpp) ; un nouveau départ s'affiche tout
 *       de suite. Changement instantané, sans animation (préférence d'Axel).
 * @architecture_constraint Push-only (ADR-0001), contrat inchangé : le graphique lit les
 *       prévisions horaires déjà poussées, rien n'est demandé à HA. Repeint seulement à
 *       l'arrivée de nouvelles prévisions (zone_gauche_donnees_changees, tab5_forecast.cpp)
 *       ou au changement de thème (zone_gauche_rejouer_theme), et seulement s'il est
 *       affiché (sinon marqué « sale », repeint à sa prochaine apparition). Écritures
 *       comparées d'abord (ui_text, ui_poser…). Aucune couleur en dur : UIColor (ADR-0029).
 * @ai_warning lv_line_set_points() ne COPIE PAS le tableau de points : s_pts vit au niveau
 *       du fichier, jamais dans une variable locale.
 * @ai_instruction Un contenu de plus : sa valeur à la FIN de ZoneGauche et son code
 *       (tab5_parse.h), son cas dans disponible() et zone_gauche_appliquer(), son option
 *       dans le blueprint (codes_gauche, section « Zone à gauche de l'horloge ») ;
 *       tests/test_zone_gauche.py compare. Un texte affiché passe par tr().
 */
#include "tab5_internal.h"
#include "lvgl.h"
#include <cmath>
#include <cstdio>
#include <cstring>

ZoneGaucheUI g_zone_gauche_ui;

namespace {

constexpr int kNb = static_cast<int>(ZoneGauche::NB);

// ─── Choix courant (NVS) ───────────────────────────────────────────────────────────────

constexpr uint32_t kMagic = 0x5A474131;    // « ZGA1 »
constexpr uint32_t kPrefKey = 0x7A676175;  // « zgau »
// Une taille de plus = un nouveau magic.
struct Sauvegarde {
    uint32_t magic;
    uint8_t courant;
    uint8_t defaut;
    uint8_t cycle;
};

struct Etat {
    bool charge = false;
    ZoneGauche courant = kZoneGaucheDefaut;
    ZoneGauche defaut = kZoneGaucheDefaut;
    uint8_t cycle = kZoneGaucheCycleDefaut;
    esphome::ESPPreferenceObject pref;
};
Etat s_etat;

// Ce que l'écran sait montrer. LECTEUR : lot 2.
bool disponible(ZoneGauche z) { return z == ZoneGauche::VOCAL || z == ZoneGauche::GRAPHIQUE; }

bool proposee(ZoneGauche z) { return (s_etat.cycle & zone_gauche_bit(z)) != 0 && disponible(z); }

const char* code(ZoneGauche z) {
    const int i = static_cast<int>(z);
    return (i >= 0 && i < kNb) ? kZoneGaucheCodes[i] : "?";
}

void charger() {
    if (s_etat.charge) return;
    s_etat.charge = true;
    s_etat.pref = esphome::global_preferences->make_preference<Sauvegarde>(kPrefKey);
    Sauvegarde s{};
    if (!s_etat.pref.load(&s) || s.magic != kMagic || s.courant >= kNb || s.defaut >= kNb) return;
    s_etat.courant = static_cast<ZoneGauche>(s.courant);
    s_etat.defaut = static_cast<ZoneGauche>(s.defaut);
    s_etat.cycle = static_cast<uint8_t>((s.cycle & ((1u << kNb) - 1u)) | zone_gauche_bit(s_etat.defaut));
}

void sauver() {
    Sauvegarde s;
    std::memset(&s, 0, sizeof(s));  // bourrage compris : rien d'indéterminé en NVS
    s.magic = kMagic;
    s.courant = static_cast<uint8_t>(s_etat.courant);
    s.defaut = static_cast<uint8_t>(s_etat.defaut);
    s.cycle = s_etat.cycle;
    s_etat.pref.save(&s);
}

// Le contenu montré : le courant, sinon le départ, sinon le vocal (toujours disponible).
ZoneGauche montree() {
    if (disponible(s_etat.courant)) return s_etat.courant;
    if (disponible(s_etat.defaut)) return s_etat.defaut;
    return ZoneGauche::VOCAL;
}

// ─── Graphique : les heures qui viennent ───────────────────────────────────────────────
// Coordonnées dans la carte zone_graphique, bordure non comprise (LVGL 9 place les enfants
// dans la bordure et le padding). Marges de 22 px à gauche et à droite : le cadre du thème
// peut être très arrondi (Capsule).

constexpr int kHeuresMax = 15;              // cal_heures_data
constexpr int kLisse = 6;                   // points de courbe entre deux heures
constexpr int32_t kMarge = 22;
constexpr int32_t kCourbeHaut = 40;         // y du plus chaud
constexpr int32_t kCourbeBasPied = 80;      // le plus froid, à tant du bas
constexpr int32_t kBasePied = 34;           // pied des barres de pluie, à tant du bas
constexpr int32_t kPluieH = 24;             // barre de la plus forte pluie
constexpr int32_t kHeurePied = 30;          // heures et cumul de pluie, à tant du bas
constexpr int32_t kPoint = 10;
constexpr int32_t kEcartPoint = 2;          // entre un point et sa valeur
constexpr float kPluieEchelleMin = 2.0f;    // mm : une bruine reste une petite barre
constexpr float kPluieSeuil = 0.05f;        // mm : en dessous, pas de barre
constexpr float kEcartMin = 4.0f;           // degrés : une journée stable reste à plat
constexpr int kReperes = 2;                 // plus chaud, plus froid
constexpr int kLibellesHeures = 3;

struct Graphique {
    lv_obj_t* base = nullptr;
    lv_obj_t* courbe = nullptr;
    lv_obj_t* barre[kHeuresMax] = {};
    lv_obj_t* point[kReperes] = {};
    lv_obj_t* valeur[kReperes] = {};
    lv_obj_t* heure[kLibellesHeures] = {};
    lv_obj_t* pluie = nullptr;
    lv_obj_t* vide = nullptr;
    bool sale = true;  // à repeindre (données ou thème changés pendant qu'il était caché)
};
Graphique s_g;
lv_point_precise_t s_pts[(kHeuresMax - 1) * kLisse + 1];
static_assert(kHeuresMax <= kCourbeLissePoints, "ui_courbe_lisse ne trace pas plus de points");

bool graphique_visible() {
    lv_obj_t* z = g_zone_gauche_ui.graphique;
    return z != nullptr && s_g.courbe != nullptr && !lv_obj_has_flag(z, LV_OBJ_FLAG_HIDDEN);
}

// Un texte non cliquable, masqué, à la police des heures, de la largeur de son texte.
lv_obj_t* libelle(lv_obj_t* parent) {
    lv_obj_t* l = lv_label_create(parent);
    if (g_zone_gauche_ui.police != nullptr)
        esphome::lvgl::lv_obj_set_style_text_font(l, g_zone_gauche_ui.police, LV_PART_MAIN);
    lv_label_set_text(l, "");
    lv_obj_remove_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

int32_t hauteur_ligne(lv_obj_t* l) {
    const lv_font_t* f = lv_obj_get_style_text_font(l, LV_PART_MAIN);
    return f != nullptr ? lv_font_get_line_height(f) : 26;
}

// Pose `txt` centré sur cx (au plus près, entre gauche et droite), en y, et le montre.
void poser_texte(lv_obj_t* l, const char* txt, int32_t cx, int32_t y, int32_t gauche, int32_t droite) {
    ui_text(l, txt);
    lv_point_t t;
    lv_text_get_size(&t, txt, lv_obj_get_style_text_font(l, LV_PART_MAIN), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    int32_t x = cx - t.x / 2;
    if (x + t.x > droite) x = droite - t.x;
    if (x < gauche) x = gauche;
    ui_poser(l, x, y, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
}

// « 18° », « -3° » (arrondi : jamais « -0° »).
void degres(char* out, size_t n, float t) { snprintf(out, n, "%ld\xC2\xB0", lroundf(t)); }

// Taille utile de la carte : sa largeur ou sa hauteur moins bordure et padding.
int32_t utile(bool largeur) {
    lv_obj_t* z = g_zone_gauche_ui.graphique;
    const int32_t b = lv_obj_get_style_border_width(z, LV_PART_MAIN);
    if (largeur) {
        return lv_obj_get_style_width(z, LV_PART_MAIN) - 2 * b - lv_obj_get_style_pad_left(z, LV_PART_MAIN) -
               lv_obj_get_style_pad_right(z, LV_PART_MAIN);
    }
    return lv_obj_get_style_height(z, LV_PART_MAIN) - 2 * b - lv_obj_get_style_pad_top(z, LV_PART_MAIN) -
           lv_obj_get_style_pad_bottom(z, LV_PART_MAIN);
}

// Couleurs fixes de ce qui est créé ici (le reste suit les valeurs à chaque peinture).
void couleurs() {
    ui_style_couleur(s_g.base, LV_STYLE_BG_COLOR, UIColor.GLASS_RIM);
    for (lv_obj_t* o : s_g.heure) ui_text_color(o, UIColor.TEXT_SOFT);
    ui_text_color(s_g.pluie, UIColor.RAIN_VALUE);
    ui_text_color(s_g.vide, UIColor.TEXT_DIM);
    for (lv_obj_t* o : s_g.point) {
        ui_style_num(o, LV_STYLE_BORDER_WIDTH, 2);
        ui_style_couleur(o, LV_STYLE_BORDER_COLOR, UIColor.BG);
        ui_style_num(o, LV_STYLE_BORDER_OPA, LV_OPA_COVER);
    }
}

void construire() {
    lv_obj_t* z = g_zone_gauche_ui.graphique;
    if (z == nullptr || s_g.courbe != nullptr) return;
    // Ordre de création = ordre de dessin : pied et barres, courbe, points, textes.
    s_g.base = ui_rectangle(z, LV_OPA_40, 0);
    for (lv_obj_t*& b : s_g.barre) b = ui_rectangle(z, LV_OPA_COVER, 3);
    s_g.courbe = ui_ligne(z, 3);
    for (lv_obj_t*& p : s_g.point) p = ui_rectangle(z, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    for (lv_obj_t*& v : s_g.valeur) v = libelle(z);
    for (lv_obj_t*& h : s_g.heure) h = libelle(z);
    s_g.pluie = libelle(z);
    s_g.vide = libelle(z);
    couleurs();
    s_g.sale = true;
}

void cacher_tout() {
    for (lv_obj_t* o : {s_g.base, s_g.courbe, s_g.pluie}) ui_hidden(o, true);
    for (lv_obj_t* o : s_g.barre) ui_hidden(o, true);
    for (lv_obj_t* o : s_g.point) ui_hidden(o, true);
    for (lv_obj_t* o : s_g.valeur) ui_hidden(o, true);
    for (lv_obj_t* o : s_g.heure) ui_hidden(o, true);
}

// Le repère i (0 le plus chaud, 1 le plus froid) sur le point (cx, py) : la valeur au-dessus
// du point, ou au-dessous s'il est en dessous et qu'elle tient avant la barre de pluie de
// sa colonne (`plafond`, son haut).
void repere(int i, float t, int32_t cx, int32_t py, bool dessous, int32_t plafond, int32_t w) {
    const uint32_t c = get_temperature_color(t);
    ui_style_couleur(s_g.point[i], LV_STYLE_BG_COLOR, c);
    ui_poser(s_g.point[i], cx - kPoint / 2, py - kPoint / 2, kPoint, kPoint);
    char buf[16];
    degres(buf, sizeof(buf), t);
    const int32_t lh = hauteur_ligne(s_g.valeur[i]);
    int32_t y = py + kPoint / 2 + kEcartPoint;
    if (!dessous || y + lh > plafond) y = py - kPoint / 2 - kEcartPoint - lh;
    ui_text_color(s_g.valeur[i], c);
    poser_texte(s_g.valeur[i], buf, cx, y, kMarge / 2, w - kMarge / 2);
}

void peindre() {
    if (s_g.courbe == nullptr) return;
    s_g.sale = false;
    const int n = previsions_heures_lisibles();
    const int32_t w = utile(true), h = utile(false);
    ui_hidden(s_g.vide, n >= 2);
    if (n < 2) {
        cacher_tout();
        poser_texte(s_g.vide, tr("En attente de Home Assistant"), w / 2, (h - hauteur_ligne(s_g.vide)) / 2, 0, w);
        return;
    }
    // Échelles : températures (kEcartMin au moins), pluie (kPluieEchelleMin au moins).
    float lo = cal_heures_data[0].temp, hi = lo, mm_max = kPluieEchelleMin, somme = 0.0f, moyenne = 0.0f;
    int k_chaud = 0, k_froid = 0;
    for (int k = 0; k < n; k++) {
        const HourForecastData& d = cal_heures_data[k];
        if (d.temp > hi) {
            hi = d.temp;
            k_chaud = k;
        }
        if (d.temp < lo) {
            lo = d.temp;
            k_froid = k;
        }
        mm_max = std::fmax(mm_max, d.pluvio);
        somme += d.pluvio;
        moyenne += d.temp / n;
    }
    const bool plat = hi - lo < 0.5f;  // un seul repère : les deux diraient la même valeur
    float bas_t = lo, haut_t = hi;
    if (haut_t - bas_t < kEcartMin) {
        const float c = (lo + hi) / 2.0f;
        bas_t = c - kEcartMin / 2.0f;
        haut_t = c + kEcartMin / 2.0f;
    }
    const int32_t y_haut = kCourbeHaut, y_bas = h - kCourbeBasPied, base = h - kBasePied;
    const float pas = static_cast<float>(w - 2 * kMarge) / n;
    float xs[kHeuresMax], ys[kHeuresMax];
    int32_t haut_barre[kHeuresMax];
    for (int k = 0; k < n; k++) {
        const HourForecastData& d = cal_heures_data[k];
        xs[k] = kMarge + (k + 0.5f) * pas;
        ys[k] = y_bas - (d.temp - bas_t) / (haut_t - bas_t) * (y_bas - y_haut);
        haut_barre[k] = base;
        if (d.pluvio < kPluieSeuil) {
            ui_hidden(s_g.barre[k], true);
            continue;
        }
        int32_t hb = static_cast<int32_t>(lroundf(d.pluvio / mm_max * kPluieH));
        if (hb < 3) hb = 3;
        int32_t lb = static_cast<int32_t>(pas * 0.5f);
        if (lb < 4) lb = 4;
        haut_barre[k] = base - hb;
        ui_style_couleur(s_g.barre[k], LV_STYLE_BG_COLOR, couleur_pluie_mm(d.pluvio));
        ui_poser(s_g.barre[k], static_cast<int32_t>(lroundf(xs[k])) - lb / 2, base - hb, lb, hb);
    }
    for (int k = n; k < kHeuresMax; k++) ui_hidden(s_g.barre[k], true);
    ui_poser(s_g.base, kMarge, base, w - 2 * kMarge, 1);
    // Courbe : couleur de la température moyenne des heures montrées.
    const int np = ui_courbe_lisse(xs, ys, n, kLisse, s_pts);
    ui_hidden(s_g.courbe, np < 2);
    if (np >= 2) {
        ui_style_couleur(s_g.courbe, LV_STYLE_LINE_COLOR, get_temperature_color(moyenne));
        lv_line_set_points(s_g.courbe, s_pts, static_cast<uint32_t>(np));
    }
    // Repères : le plus chaud au-dessus de son point, le plus froid au-dessous.
    auto px = [&](int k) { return static_cast<int32_t>(lroundf(xs[k])); };
    auto py = [&](int k) { return static_cast<int32_t>(lroundf(ys[k])); };
    repere(0, hi, px(k_chaud), py(k_chaud), false, base, w);
    if (plat) {
        ui_hidden(s_g.point[1], true);
        ui_hidden(s_g.valeur[1], true);
    } else {
        repere(1, lo, px(k_froid), py(k_froid), true, haut_barre[k_froid] - 2, w);
    }
    // Heures : maintenant, puis tous les 4 créneaux (le tiers sous 12 créneaux) ; le cumul
    // de pluie à droite, sur la même ligne.
    const int pas_heures = n >= 12 ? 4 : (n / kLibellesHeures > 0 ? n / kLibellesHeures : 1);
    const int32_t y_heure = h - kHeurePied;
    for (int i = 0; i < kLibellesHeures; i++) {
        const int k = i * pas_heures;
        if (k >= n) {
            ui_hidden(s_g.heure[i], true);
            continue;
        }
        poser_texte(s_g.heure[i], cal_heures_data[k].heure_texte.c_str(), px(k), y_heure, kMarge / 2, w - kMarge);
    }
    if (somme >= kPluieSeuil) {
        char buf[16];
        snprintf(buf, sizeof(buf), "%.1f mm", somme);
        poser_texte(s_g.pluie, buf, w, y_heure, kMarge, w - kMarge);
    } else {
        ui_hidden(s_g.pluie, true);
    }
}

}  // namespace

// ─── API (tab5_zone_gauche.h, tab5_internal.h) ─────────────────────────────────────────

void zone_gauche_appliquer() {
    charger();
    const ZoneGaucheUI& u = g_zone_gauche_ui;
    const ZoneGauche z = montree();
    ui_hidden(u.vocal, z != ZoneGauche::VOCAL);
    ui_hidden(u.graphique, z != ZoneGauche::GRAPHIQUE);
    if (z != ZoneGauche::GRAPHIQUE || u.graphique == nullptr) return;
    construire();
    if (s_g.sale) peindre();
}

void zone_gauche_suivante() {
    charger();
    const int actuelle = static_cast<int>(montree());
    for (int i = 1; i < kNb; i++) {
        const ZoneGauche z = static_cast<ZoneGauche>((actuelle + i) % kNb);
        if (!proposee(z)) continue;
        s_etat.courant = z;
        sauver();
        ESP_LOGI("tab5.gauche", "Zone gauche : %s", code(z));
        zone_gauche_appliquer();
        return;
    }
}

void zone_gauche_recu(const char* valeur, size_t n) {
    charger();
    const ZoneGaucheLu lu = zone_gauche_lire(valeur, n);  // Tab5/socle/tab5_parse.cpp
    if (lu.defaut == s_etat.defaut && lu.cycle == s_etat.cycle) return;  // poussée à chaque connexion
    const bool nouveau_depart = lu.defaut != s_etat.defaut;
    s_etat.defaut = lu.defaut;
    s_etat.cycle = lu.cycle;
    if (nouveau_depart || !proposee(s_etat.courant)) s_etat.courant = s_etat.defaut;
    sauver();
    ESP_LOGI("tab5.gauche", "Zone gauche : départ %s, cycle 0x%02X, montrée %s", code(s_etat.defaut), s_etat.cycle,
             code(montree()));
    zone_gauche_appliquer();
}

void zone_gauche_donnees_changees() {
    s_g.sale = true;
    if (graphique_visible()) peindre();
}

// Changement de thème (theme_rejouer_ui, tab5_theme.cpp) : couleurs fixes, puis la
// peinture (couleurs des valeurs) si le graphique est affiché, à sa prochaine apparition
// sinon.
void zone_gauche_rejouer_theme() {
    if (s_g.courbe == nullptr) return;
    couleurs();
    s_g.sale = true;
    if (graphique_visible()) peindre();
}
