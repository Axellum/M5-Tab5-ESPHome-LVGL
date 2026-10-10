/**
 * [AI-CONTEXT]
 * @file tab5_zone_gauche.cpp
 * @role Zone à gauche de l'horloge au choix (ADR-0051, 10/10/2026, demande d'Axel : « sur
 *       cette zone, j'aimerais qu'on puisse choisir soit le vocal, soit un lecteur audio,
 *       soit un graphique ; très beau, léger »). Quatre contenus :
 *         - « vocal » : le conteneur zone_vocal de tab5-lvgl.yaml (micro, Domo, Discu),
 *           montré ou masqué d'un bloc ; ses widgets, leurs gestes et le masquage de Domo /
 *           Discu sans pipeline de discussion (zone « discussion », tab5_zones.cpp) ne
 *           changent pas ;
 *         - « graphique » : dans la carte zone_graphique, les heures qui viennent
 *           (cal_heures_data, 15 au plus) : courbe lissée des températures à la couleur de
 *           leur moyenne (trait de 5 px, bouts arrondis) sur un dégradé discret de la même
 *           couleur, un point cerclé et sa valeur au plus chaud et au plus froid (la valeur
 *           à la première place libre autour du point : jamais sur la courbe, une barre,
 *           l'autre valeur, ni hors du cadre), barres de pluie en mm (couleurs de la pluie,
 *           toute pluie annoncée visible, rien sans pluie), trois heures sous le pied des
 *           barres, le cumul de pluie à droite. Une trentaine d'objets, créés à la première
 *           apparition (style revu le 10/10/2026, demande d'Axel sur la capture du rendu).
 *         - « lecteur » (lot 2, sur le lecteur de l'ADR-0050) : la carte zone_lecteur
 *           (lecteur_zone.yaml), montrée ou masquée d'ici ; tab5_lecteur.cpp la peint
 *           (mêmes données et commandes que le popup Musique, lecteur_zone_montrer) et
 *           masque la mini-barre « en lecture » tant qu'elle est montrée. Sauté quand HA a
 *           dit qu'aucun lecteur n'est choisi (lecteur_zone_disponible).
 *         - « capteur » (ADR-0054) : la carte zone_suivi (suivi_zone.yaml), montrée ou
 *           masquée d'ici ; tab5_suivi.cpp la peint (le premier capteur de « Tab5 · capteurs
 *           suivis » : nom, valeur, variation, courbe des 24 h sur un dégradé). Elle
 *           partage le tampon du dégradé du graphique (zone_degrade_peindre) : les deux ne
 *           sont jamais montrés ensemble. Sautée quand HA a dit qu'aucun capteur n'est
 *           choisi (suivi_zone_disponible).
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
 *       du fichier, jamais dans une variable locale. De même lv_image_set_src() garde
 *       l'adresse du descripteur du dégradé (s_aire_dsc) et de ses pixels (s_aire_px).
 *       Le dégradé est en ARGB8888 : lv_conf.h de la build n'active pas le dessin des
 *       images A8 (LV_DRAW_SW_SUPPORT_A8 0), lv_canvas ni lv_chart.
 * @ai_instruction Un contenu de plus : sa valeur à la FIN de ZoneGauche et son code
 *       (tab5_parse.h), son cas dans disponible() et zone_gauche_appliquer(), son option
 *       dans le blueprint (codes_gauche, section « Zone à gauche de l'horloge ») ;
 *       tests/test_zone_gauche.py compare. Un texte affiché passe par tr().
 */
#include "tab5_internal.h"
#include "lvgl.h"
#include "lvgl_private.h"  // lv_image_cache_drop() (cache d'images, hors de lvgl.h en 9.5)
#include <esp_heap_caps.h>
#include <algorithm>
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

// Ce que l'écran sait montrer. Le lecteur compact (lot 2) : sauté quand HA a dit
// qu'aucun lecteur n'est choisi (sa liste « Tab5 · lecteurs de musique » est vide) ; avant
// toute poussée, il se montre (« En attente de Home Assistant »). Le capteur suivi
// (ADR-0054) de même avec la liste « Tab5 · capteurs suivis ».
bool disponible(ZoneGauche z) {
    switch (z) {
        case ZoneGauche::VOCAL:
        case ZoneGauche::GRAPHIQUE: return true;
        case ZoneGauche::LECTEUR: return g_zone_gauche_ui.lecteur != nullptr && lecteur_zone_disponible();
        case ZoneGauche::CAPTEUR: return g_zone_gauche_ui.capteur != nullptr && suivi_zone_disponible();
        default: return false;
    }
}

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
constexpr int32_t kPluieH = 26;             // barre de la plus forte pluie
constexpr int32_t kPluieMin = 8;            // barre d'une pluie à peine mesurée : visible
constexpr int32_t kHeurePied = 30;          // heures et cumul de pluie, à tant du bas
constexpr int32_t kTrait = 5;               // épaisseur de la courbe (bouts arrondis)
constexpr int32_t kPoint = 12;              // repère du plus chaud et du plus froid
constexpr int32_t kAnneau = 3;              // son anneau, à la couleur du fond
constexpr int32_t kEcartPoint = 3;          // entre un point et sa valeur
constexpr int32_t kEcartCourbe = 3;         // entre une valeur et le trait de la courbe
// Encre d'un chiffre dans la boîte de sa ligne (Roboto 22 : la boîte garde l'air des
// accents au-dessus et des jambages au-dessous) : ce que la courbe ne doit pas toucher.
constexpr int32_t kEncreHaut = 4;
constexpr int32_t kEncreBas = 5;
constexpr float kPluieEchelleMin = 2.0f;    // mm : une bruine reste une petite barre
constexpr float kPluieSeuil = 0.05f;        // mm : en dessous, pas de cumul écrit
constexpr float kEcartMin = 4.0f;           // degrés : une journée stable reste à plat
constexpr int kReperes = 2;                 // plus chaud, plus froid
constexpr int kLibellesHeures = 3;
// Remplissage sous la courbe : une image ARGB8888 calculée ici (lv_canvas et lv_chart
// sont désactivés), de l'opacité kAireOpa sous le trait à 0 au pied des barres. Taille
// au plus : la courbe entre les marges (2 px d'arrondi), du haut de la courbe au pied des
// barres, dans la carte zone_graphique de 405 × 184 (tab5-lvgl.yaml) ; 156 Kio, pris une
// fois en PSRAM au premier dessin.
constexpr lv_opa_t kAireOpa = 84;           // ≈ 33 %
constexpr int32_t kZoneL = 405, kZoneH = 184;
constexpr int32_t kAireLMax = kZoneL - 2 * kMarge + 2;
constexpr int32_t kAireHMax = kZoneH - kCourbeHaut - kBasePied;

struct Graphique {
    lv_obj_t* base = nullptr;
    lv_obj_t* aire = nullptr;   // remplissage dégradé sous la courbe (lv_image)
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
// Tampon du dégradé, partagé par les contenus de la zone (le graphique et le capteur suivi,
// ADR-0054 : jamais montrés ensemble). Une seule image le montre à la fois (s_aire_image) ;
// celle qui le reprend vide l'autre et prévient son contenu (s_aire_perdu), qui se marque
// « sale » et repeint son dégradé à sa prochaine apparition.
uint8_t* s_aire_px = nullptr;  // pixels du remplissage (kAireLMax × kAireHMax × 4 octets)
lv_image_dsc_t s_aire_dsc;     // vit avec l'image : lv_image_set_src() ne le copie pas
bool s_aire_trace = false;     // dégradé impossible déjà signalé (une trace, pas une par peinture)
lv_obj_t* s_aire_image = nullptr;
void (*s_aire_perdu)() = nullptr;
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
        ui_style_num(o, LV_STYLE_BORDER_WIDTH, kAnneau);
        ui_style_couleur(o, LV_STYLE_BORDER_COLOR, UIColor.BG);
        ui_style_num(o, LV_STYLE_BORDER_OPA, LV_OPA_COVER);
    }
}

void construire() {
    lv_obj_t* z = g_zone_gauche_ui.graphique;
    if (z == nullptr || s_g.courbe != nullptr) return;
    // Ordre de création = ordre de dessin : pied, remplissage, barres, courbe, points, textes.
    s_g.base = ui_rectangle(z, LV_OPA_40, 0);
    s_g.aire = lv_image_create(z);
    lv_obj_remove_flag(s_g.aire, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_g.aire, LV_OBJ_FLAG_HIDDEN);
    for (lv_obj_t*& b : s_g.barre) b = ui_rectangle(z, LV_OPA_COVER, 3);
    s_g.courbe = ui_ligne(z, kTrait);
    for (lv_obj_t*& p : s_g.point) p = ui_rectangle(z, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    for (lv_obj_t*& v : s_g.valeur) v = libelle(z);
    for (lv_obj_t*& h : s_g.heure) h = libelle(z);
    s_g.pluie = libelle(z);
    s_g.vide = libelle(z);
    couleurs();
    s_g.sale = true;
}

void cacher_tout() {
    for (lv_obj_t* o : {s_g.base, s_g.aire, s_g.courbe, s_g.pluie}) ui_hidden(o, true);
    for (lv_obj_t* o : s_g.barre) ui_hidden(o, true);
    for (lv_obj_t* o : s_g.point) ui_hidden(o, true);
    for (lv_obj_t* o : s_g.valeur) ui_hidden(o, true);
    for (lv_obj_t* o : s_g.heure) ui_hidden(o, true);
}

// ─── Remplissage sous la courbe ───

// y de la courbe `pts` en x (interpolé entre deux points), NAN hors de la courbe.
float courbe_y_pts(const lv_point_precise_t* pts, int np, float x) {
    if (np < 2 || x < pts[0].x || x > pts[np - 1].x) return NAN;
    int k = 0;
    while (k < np - 2 && pts[k + 1].x < x) k++;
    const float xa = pts[k].x, xb = pts[k + 1].x;
    const float t = xb > xa ? (x - xa) / (xb - xa) : 0.0f;
    return pts[k].y + (pts[k + 1].y - pts[k].y) * t;
}

// La courbe du graphique (s_pts).
float courbe_y(float x, int np) { return courbe_y_pts(s_pts, np, x); }

// Le graphique a perdu le tampon du dégradé (le capteur suivi l'a repris) : à repeindre.
void graphique_perdu() { s_g.sale = true; }

}  // namespace

// Le dégradé sous la courbe `pts` dans `image`, de kAireOpa sous le trait (bord lissé au
// pixel) à 0 au pied `base` ; sa hauteur est celle de la courbe la plus haute possible
// (y_haut) : une courbe basse a un remplissage plus pâle. Calculé à chaque peinture
// (nouvelles données ou thème : quelques dizaines de milliers de pixels). Faux, et l'image
// masquée, quand il ne peut pas être peint (PSRAM refusée, hors du tampon).
bool zone_degrade_peindre(lv_obj_t* image, const lv_point_precise_t* pts, int np, uint32_t couleur, int32_t y_haut,
                          int32_t base, void (*perdu)()) {
    if (image == nullptr || pts == nullptr || np < 2) {
        ui_hidden(image, true);
        return false;
    }
    // Le tampon passe à une autre image : la précédente est vidée et son contenu prévenu.
    if (s_aire_image != nullptr && s_aire_image != image) {
        lv_image_set_src(s_aire_image, nullptr);
        ui_hidden(s_aire_image, true);
        if (s_aire_perdu != nullptr) s_aire_perdu();
    }
    s_aire_image = image;
    s_aire_perdu = perdu;
    const int32_t x0 = static_cast<int32_t>(std::floor(pts[0].x));
    const int32_t x1 = static_cast<int32_t>(std::ceil(pts[np - 1].x));
    const int32_t aw = x1 - x0 + 1, ah = base - y_haut;
    // PSRAM seulement : 156 Kio pris à la mémoire interne priveraient le Wi-Fi et lwIP ;
    // sans PSRAM libre, la courbe reste sans dégradé (une trace, une fois).
    if (s_aire_px == nullptr && aw > 1 && ah > 1) {
        const size_t octets = static_cast<size_t>(kAireLMax) * kAireHMax * 4;
        s_aire_px = static_cast<uint8_t*>(heap_caps_malloc(octets, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    }
    if (s_aire_px == nullptr || aw <= 1 || ah <= 1 || aw > kAireLMax || ah > kAireHMax) {
        if (!s_aire_trace) {
            ESP_LOGW("tab5.gauche", "Dégradé de la zone gauche non peint (%s, %ld × %ld)",
                     s_aire_px == nullptr ? "PSRAM refusée" : "hors du tampon", static_cast<long>(aw),
                     static_cast<long>(ah));
            s_aire_trace = true;
        }
        ui_hidden(image, true);
        return false;
    }
    const uint8_t r = (couleur >> 16) & 0xFF, g = (couleur >> 8) & 0xFF, b = couleur & 0xFF;
    for (int32_t i = 0; i < aw; i++) {
        float yc = courbe_y_pts(pts, np, static_cast<float>(x0 + i) + 0.5f);
        if (std::isnan(yc)) yc = static_cast<float>(base);
        yc -= static_cast<float>(y_haut);
        for (int32_t j = 0; j < ah; j++) {
            // Part du pixel sous la courbe (0 au-dessus, 1 dessous), puis le fondu vers le pied.
            float part = static_cast<float>(j + 1) - yc;
            part = part < 0.0f ? 0.0f : (part > 1.0f ? 1.0f : part);
            const float a = part * kAireOpa * static_cast<float>(ah - j) / static_cast<float>(ah);
            uint8_t* p = s_aire_px + (static_cast<size_t>(j) * aw + i) * 4;
            p[0] = b;  // ARGB8888 non prémultiplié : B, G, R, A en mémoire
            p[1] = g;
            p[2] = r;
            p[3] = static_cast<uint8_t>(lroundf(a));
        }
    }
    std::memset(&s_aire_dsc, 0, sizeof(s_aire_dsc));
    s_aire_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_aire_dsc.header.cf = LV_COLOR_FORMAT_ARGB8888;
    s_aire_dsc.header.w = static_cast<uint32_t>(aw);
    s_aire_dsc.header.h = static_cast<uint32_t>(ah);
    s_aire_dsc.header.stride = static_cast<uint32_t>(aw) * 4;
    s_aire_dsc.data_size = static_cast<uint32_t>(aw) * static_cast<uint32_t>(ah) * 4;
    s_aire_dsc.data = s_aire_px;
    // Mêmes adresses, pixels neufs : rien de l'image d'avant ne doit rester en cache.
    lv_image_cache_drop(&s_aire_dsc);
    lv_image_set_src(image, nullptr);
    lv_image_set_src(image, &s_aire_dsc);
    ui_x(image, x0);
    ui_y(image, y_haut);
    ui_hidden(image, false);
    return true;
}

namespace {

// ─── Repères du plus chaud et du plus froid ───

struct Boite {
    int32_t x0, y0, x1, y1;  // x1, y1 exclus
};

bool chevauche(const Boite& a, const Boite& b) { return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1; }

// La courbe (son trait et kEcartCourbe d'air) passe-t-elle dans la boîte ?
bool touche_courbe(const Boite& o, int np) {
    const float demi = kTrait / 2.0f + kEcartCourbe;
    for (int32_t x = o.x0; x <= o.x1; x += 2) {
        const float y = courbe_y(static_cast<float>(x), np);
        if (!std::isnan(y) && y + demi > o.y0 && y - demi < o.y1) return true;
    }
    return false;
}

// Ce qu'une valeur ne doit pas couvrir : la courbe, les deux points, les barres de pluie
// montrées, l'autre valeur ; et le cadre : entre les marges, sous le haut de la carte,
// au-dessus du pied des barres (les heures sont dessous).
struct Obstacles {
    int np = 0;
    int32_t gauche = 0, droite = 0, haut = 0, bas = 0;
    Boite point[kReperes] = {};
    int nb_points = 0;
    Boite barre[kHeuresMax] = {};
    int nb_barres = 0;
    Boite valeur = {};
    bool a_valeur = false;
};

bool place_libre(const Boite& encre, const Obstacles& ob) {
    if (touche_courbe(encre, ob.np)) return false;
    for (int i = 0; i < ob.nb_points; i++)
        if (chevauche(encre, ob.point[i])) return false;
    for (int i = 0; i < ob.nb_barres; i++)
        if (chevauche(encre, ob.barre[i])) return false;
    return !(ob.a_valeur && chevauche(encre, ob.valeur));
}

// Le repère i (0 le plus chaud, 1 le plus froid) sur le point (cx, cy) ; sa valeur à la
// première place libre autour du point : au-dessus d'abord pour le plus chaud, au-dessous
// pour le plus froid, puis en diagonale, sur les côtés, de l'autre côté. Aucune libre :
// la première qui tient dans le cadre. Renvoie la boîte d'encre de la valeur.
Boite repere(int i, float t, int32_t cx, int32_t cy, bool dessous, const Obstacles& ob) {
    const uint32_t c = get_temperature_color(t);
    ui_style_couleur(s_g.point[i], LV_STYLE_BG_COLOR, c);
    ui_poser(s_g.point[i], cx - kPoint / 2, cy - kPoint / 2, kPoint, kPoint);
    char buf[16];
    degres(buf, sizeof(buf), t);
    lv_obj_t* l = s_g.valeur[i];
    ui_text_color(l, c);
    ui_text(l, buf);
    lv_point_t sz;
    lv_text_get_size(&sz, buf, lv_obj_get_style_text_font(l, LV_PART_MAIN), 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    const int32_t tw = sz.x, lh = hauteur_ligne(l);
    const int32_t e = kPoint / 2 + kEcartPoint;
    // Huit places (x, y du coin de la boîte de ligne).
    const int32_t dessus = cy - e - lh + kEncreBas, sous = cy + e - kEncreHaut, milieu = cy - lh / 2;
    const int32_t centre = cx - tw / 2, droite = cx + e, gauche = cx - e - tw;
    const int32_t places_dessus[8][2] = {{centre, dessus}, {droite, dessus}, {gauche, dessus}, {cx + e + 2, milieu}, {cx - e - 2 - tw, milieu}, {centre, sous}, {droite, sous}, {gauche, sous}};
    const int32_t places_sous[8][2] = {{centre, sous}, {droite, sous}, {gauche, sous}, {cx + e + 2, milieu}, {cx - e - 2 - tw, milieu}, {centre, dessus}, {droite, dessus}, {gauche, dessus}};
    const int32_t (*places)[2] = dessous ? places_sous : places_dessus;
    int32_t choix_x = 0, choix_y = 0;
    bool trouve = false, repli = false;
    for (int k = 0; k < 8 && !trouve; k++) {
        int32_t x = places[k][0];
        const int32_t y = places[k][1];
        if (x + tw > ob.droite) x = ob.droite - tw;
        if (x < ob.gauche) x = ob.gauche;
        const Boite encre{x, y + kEncreHaut, x + tw, y + lh - kEncreBas};
        if (encre.y0 < ob.haut || encre.y1 > ob.bas) continue;
        if (!repli) {
            choix_x = x;
            choix_y = y;
            repli = true;
        }
        if (place_libre(encre, ob)) {
            choix_x = x;
            choix_y = y;
            trouve = true;
        }
    }
    if (!repli) {  // aucune place dans le cadre (carte minuscule) : au-dessus, bornée
        choix_x = centre < ob.gauche ? ob.gauche : centre;
        choix_y = ob.haut - kEncreHaut;
    }
    ui_poser(l, choix_x, choix_y, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    return Boite{choix_x, choix_y + kEncreHaut, choix_x + tw, choix_y + lh - kEncreBas};
}

void peindre() {
    if (s_g.courbe == nullptr) return;
    s_g.sale = false;
    // Borné par le kHeuresMax de ce fichier (xs, ys, s_pts, barres), égal aujourd'hui à
    // celui de tab5_meteo.cpp.
    const int n = std::min(previsions_heures_lisibles(), kHeuresMax);
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
    Obstacles ob;
    ob.gauche = kMarge / 2;
    ob.droite = w - kMarge / 2;
    ob.haut = 2;
    ob.bas = base - 2;
    // Barres de pluie : toute pluie annoncée se voit (kPluieMin), la plus forte monte à
    // kPluieH ; racine carrée de la quantité, pour qu'une petite pluie à côté d'une averse
    // ne retombe pas à la hauteur minimale. Aucune barre sans pluie.
    for (int k = 0; k < n; k++) {
        const HourForecastData& d = cal_heures_data[k];
        xs[k] = kMarge + (k + 0.5f) * pas;
        ys[k] = y_bas - (d.temp - bas_t) / (haut_t - bas_t) * (y_bas - y_haut);
        if (!(d.pluvio > 0.0f)) {
            ui_hidden(s_g.barre[k], true);
            continue;
        }
        const float part = std::sqrt(std::fmin(d.pluvio / mm_max, 1.0f));
        const int32_t hb = kPluieMin + static_cast<int32_t>(lroundf(part * (kPluieH - kPluieMin)));
        int32_t lb = static_cast<int32_t>(pas * 0.55f);
        if (lb < 6) lb = 6;
        const int32_t bx = static_cast<int32_t>(lroundf(xs[k])) - lb / 2;
        ui_style_couleur(s_g.barre[k], LV_STYLE_BG_COLOR, couleur_pluie_mm(d.pluvio));
        ui_poser(s_g.barre[k], bx, base - hb, lb, hb);
        ob.barre[ob.nb_barres++] = Boite{bx - 2, base - hb - 2, bx + lb + 2, base};
    }
    for (int k = n; k < kHeuresMax; k++) ui_hidden(s_g.barre[k], true);
    ui_poser(s_g.base, kMarge, base, w - 2 * kMarge, 1);
    // Courbe et remplissage : couleur de la température moyenne des heures montrées.
    const uint32_t couleur = get_temperature_color(moyenne);
    const int np = ui_courbe_lisse(xs, ys, n, kLisse, s_pts);
    ob.np = np;
    ui_hidden(s_g.courbe, np < 2);
    if (np >= 2) {
        ui_style_couleur(s_g.courbe, LV_STYLE_LINE_COLOR, couleur);
        lv_line_set_points(s_g.courbe, s_pts, static_cast<uint32_t>(np));
        zone_degrade_peindre(s_g.aire, s_pts, np, couleur, y_haut, base, graphique_perdu);
    } else {
        ui_hidden(s_g.aire, true);
    }
    // Repères : les deux points d'abord (aucune valeur ne les couvre), puis la valeur du
    // plus chaud (au-dessus de préférence), puis celle du plus froid (au-dessous), qui
    // évite aussi la première.
    auto px = [&](int k) { return static_cast<int32_t>(lroundf(xs[k])); };
    auto py = [&](int k) { return static_cast<int32_t>(lroundf(ys[k])); };
    const int32_t m = kPoint / 2 + 1;
    ob.point[ob.nb_points++] = Boite{px(k_chaud) - m, py(k_chaud) - m, px(k_chaud) + m, py(k_chaud) + m};
    if (!plat) ob.point[ob.nb_points++] = Boite{px(k_froid) - m, py(k_froid) - m, px(k_froid) + m, py(k_froid) + m};
    ob.valeur = repere(0, hi, px(k_chaud), py(k_chaud), false, ob);
    ob.a_valeur = true;
    if (plat) {
        ui_hidden(s_g.point[1], true);
        ui_hidden(s_g.valeur[1], true);
    } else {
        repere(1, lo, px(k_froid), py(k_froid), true, ob);
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
    ui_hidden(u.lecteur, z != ZoneGauche::LECTEUR);
    ui_hidden(u.capteur, z != ZoneGauche::CAPTEUR);
    lecteur_zone_montrer(z == ZoneGauche::LECTEUR);  // et la mini-barre, masquée ou rendue
    suivi_zone_montrer(z == ZoneGauche::CAPTEUR);    // repeint s'il a changé caché
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
