/**
 * [AI-CONTEXT]
 * @file tab5_theme.cpp
 * @role Thèmes de l'écran (ADR-0029, lots 2 et 3) : la palette active (UIColor, et
 *       UIBandeau / UIHorloge pour le bandeau central et l'horloge) d'après les entités
 *       « Thème », « Clair ou sombre » et « Nuit (thème auto) » (tab5-themes.yaml), les
 *       formes et les polices d'affichage du thème, et la liste des modules qui
 *       repeignent leurs couleurs après une bascule.
 * @architecture_constraint ESPHome 2026.9 crée les styles et les widgets dans main(),
 *       AVANT le setup des composants : un thème gardé en mémoire est donc restauré après
 *       la création de l'écran. tab5_theme_repeindre (tab5-themes.yaml) repeint alors les
 *       styles partagés tout de suite (couleurs, formes, polices), puis attend
 *       theme_ui_pret() pour theme_rejouer_ui() : ce qu'un module a peint pendant le
 *       démarrage, avec l'ancienne palette, est repeint une fois le démarrage fini. Le
 *       même chemin sert à la bascule à chaud.
 * @ai_instruction Un module qui pose une couleur de la palette lui-même (ui_text_color,
 *       lv_obj_set_style_*_color, texte recoloré #RRGGBB, style C++ à lui) doit la
 *       repeindre depuis son dernier état dans une fonction <module>_rejouer_theme(),
 *       appelée ci-dessous ; une couleur lue d'un capteur par une lambda YAML se rejoue
 *       dans tab5_theme_repeindre. Dans le bandeau central, lire UIBandeau ; dans
 *       l'horloge, UIHorloge (un thème clair peut les garder sombres). Preuve : le rendu
 *       hors tablette compare la bascule à chaud au démarrage à froid dans le même thème
 *       (tools/rendu/, job « thèmes »).
 *       Les tables entre `// >>> formes` et `// <<< formes` sont écrites par
 *       tools/gen_themes.py (`--check` échoue en CI si elles sont périmées).
 */
#include "tab5_internal.h"
#include "esphome/core/application.h"

// Thème et mode actifs. L'écran naît dans l'état compilé de tab5-styles.yaml : le
// premier thème, en sombre (le générateur refuse formes et polices sur ce thème).
static int s_theme = 0;
static bool s_clair = false;

bool theme_selectionner(int theme, int mode, bool nuit) {
    if (theme < 0 || theme >= THEME_COUNT) theme = 0;
    const bool clair = mode == 1 || (mode == 2 && !nuit);
    if (theme == s_theme && clair == s_clair) return false;
    const Theme& t = THEMES[theme];
    const Palette& p = clair ? t.clair : t.sombre;
    UIColor = p;
    // Zones qu'un thème clair garde sombres : elles prennent la palette sombre du thème.
    UIBandeau = clair && t.bandeau_sombre ? t.sombre : p;
    UIHorloge = clair && t.horloge_sombre ? t.sombre : p;
    s_theme = theme;
    s_clair = clair;
    ESP_LOGI("tab5.theme", "Thème %s, %s", t.nom, clair ? "clair" : "sombre");
    return true;
}

bool theme_ui_pret() { return esphome::App.is_setup_complete(); }

// --- Formes (rayons, bordures, dégradés, ombres) ------------------------------
// Une propriété d'un style : nombre (rayon, largeur, côtés, opacité…), couleur
// littérale, rôle de la palette du style, ou retirée (le style ne la portait pas).
enum : uint8_t { FORME_NOMBRE, FORME_COULEUR, FORME_ROLE, FORME_RETIRE };
struct Forme {
    uint8_t style;         // index dans le tableau passé à theme_formes()
    lv_style_prop_t prop;  // LV_STYLE_*
    uint8_t type;          // FORME_*
    int32_t valeur;        // nombre, 0xRRGGBB, ou index dans kRolesFormes
};
// Polices d'un thème : index des polices de l'heure, de la date et des titres, puis la
// géométrie de l'horloge (y des labels des rouleaux, position du « : »).
struct PolicesTheme {
    uint8_t horloge, date, titre;
    int8_t y;
    int16_t x_deux_points;
    int16_t y_deux_points;
};

// >>> formes (généré par tools/gen_themes.py depuis Tab5/themes/ et tab5-styles.yaml, ne pas éditer)
// Styles qu'un thème redessine, dans l'ordre du tableau que passe tab5_theme_repeindre
// (tab5-themes.yaml), et la palette où leurs rôles se lisent.
static constexpr int kStylesFormes = 0;
static const Palette* const kPaletteStyle[] = {nullptr};
static constexpr uint32_t Palette::* kRolesFormes[] = {nullptr};
// État compilé (tab5-styles.yaml) de chaque propriété qu'un thème change, reposé avant
// les formes du thème choisi.
static constexpr int kNbFormesDefaut = 0;
static constexpr Forme kFormesDefaut[] = {
    {0, 0, FORME_RETIRE, 0},  // (fin)
};
// Formes de chaque thème, mode sombre puis clair : kFormes[kFormesDebut[2 t + clair] ..
// kFormesDebut[2 t + clair + 1][.
static constexpr uint16_t kFormesDebut[] = {0, 0, 0};
static constexpr Forme kFormes[] = {
    {0, 0, FORME_RETIRE, 0},  // (fin)
};
// Polices de chaque thème : index dans le tableau `polices` que passe
// tab5_theme_repeindre (0-2 = les Roboto compilées), puis la géométrie de l'horloge
// (y des labels des rouleaux, position du « : »), tools/police_theme.py.
static constexpr int kNbPolices = 3;
static constexpr PolicesTheme kPolices[] = {
    {0, 1, 2, -23, 181, 10},  // ardoise
};
// <<< formes

static void poser_forme(lv_style_t* const styles[], const Forme& f) {
    lv_style_t* st = styles[f.style];
    if (f.type == FORME_RETIRE) {
        lv_style_remove_prop(st, f.prop);
        return;
    }
    lv_style_value_t v{};
    if (f.type == FORME_NOMBRE) {
        v.num = f.valeur;
    } else {
        const uint32_t rgb = f.type == FORME_ROLE ? kPaletteStyle[f.style]->*kRolesFormes[f.valeur]
                                                  : static_cast<uint32_t>(f.valeur);
        v.color = lv_color_hex(rgb);
    }
    lv_style_set_prop(st, f.prop, v);
}

void theme_formes(lv_style_t* const styles[], int n) {
    if (n != kStylesFormes) {
        ESP_LOGE("tab5.theme", "theme_formes : %d styles reçus, %d attendus (tables périmées)", n, kStylesFormes);
        return;
    }
    // L'état compilé d'abord (un thème précédent a pu changer une propriété que celui-ci
    // ne touche pas), puis les formes du thème.
    for (int i = 0; i < kNbFormesDefaut; i++) poser_forme(styles, kFormesDefaut[i]);
    const int rangee = s_theme * 2 + (s_clair ? 1 : 0);
    for (int i = kFormesDebut[rangee]; i < kFormesDebut[rangee + 1]; i++) poser_forme(styles, kFormes[i]);
    for (int i = 0; i < n; i++) lv_obj_report_style_change(styles[i]);
}

// --- Polices d'affichage ------------------------------------------------------
void theme_polices(lv_style_t* st_horloge, lv_style_t* st_date, lv_style_t* st_titre,
    esphome::font::Font* const polices[], int n, lv_obj_t* const horloge[9]) {
    if (n != kNbPolices) {
        ESP_LOGE("tab5.theme", "theme_polices : %d polices reçues, %d attendues (tables périmées)", n, kNbPolices);
        return;
    }
    const PolicesTheme& p = kPolices[s_theme];
    const uint8_t index[3] = {p.horloge, p.date, p.titre};
    lv_style_t* const styles[3] = {st_horloge, st_date, st_titre};
    for (int role = 0; role < 3; role++) {
        esphome::font::Font* police = polices[index[role]];
        // Un glyphe absent d'une police de thème est dessiné par la Roboto du même rôle.
        if (index[role] >= 3) {
            const_cast<lv_font_t*>(police->get_lv_font())->fallback = polices[role]->get_lv_font();
        }
        lv_style_set_text_font(styles[role], police->get_lv_font());
        lv_obj_report_style_change(styles[role]);
    }
    // Rouleaux : l'encre des chiffres centrée dans le cadre de 75 × 104 ; « : » centré
    // entre les heures et les minutes (tools/police_theme.py, tests/test_polices_themes.py).
    for (int i = 0; i < 8; i++) {
        if (horloge[i] != nullptr) lv_obj_set_y(horloge[i], p.y);
    }
    if (horloge[8] != nullptr) lv_obj_set_pos(horloge[8], p.x_deux_points, p.y_deux_points);
}

void theme_console_libelles(lv_obj_t* lbl_theme, lv_obj_t* lbl_mode, int theme, int mode) {
    // Mêmes valeurs, dans le même ordre, que les options du select « Clair ou sombre ».
    static const char* const kModes[] = {tr_noop("Sombre"), tr_noop("Clair"), tr_noop("Auto")};
    if (theme < 0 || theme >= THEME_COUNT) theme = 0;
    if (mode < 0 || mode > 2) mode = 0;
    if (lbl_theme != nullptr) lv_label_set_text(lbl_theme, THEMES[theme].nom);
    if (lbl_mode != nullptr) lv_label_set_text(lbl_mode, tr(kModes[mode]));
}

void theme_rejouer_ui() {
    central_rejouer_theme();
    vigilance_rejouer();
    rain_bars_rejouer();
    rain_predict_rejouer();
    tuiles_rejouer_theme();
    cartes_rejouer_theme();
    energie_rejouer_theme();
    zones_rejouer_theme();
    assist_rejouer_theme();
    cal_detail_rejouer();
}
