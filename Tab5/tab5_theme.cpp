/**
 * [AI-CONTEXT]
 * @file tab5_theme.cpp
 * @role Thèmes de l'écran (ADR-0029, lot 2) : la palette active (UIColor) d'après les
 *       entités « Thème », « Clair ou sombre » et « Nuit (thème auto) » (tab5-themes.yaml),
 *       et la liste des modules qui repeignent leurs couleurs après une bascule.
 * @architecture_constraint ESPHome 2026.9 crée les styles et les widgets dans main(),
 *       AVANT le setup des composants : un thème gardé en mémoire est donc restauré après
 *       la création de l'écran. tab5_theme_repeindre (tab5-themes.yaml) repeint alors les
 *       styles partagés tout de suite, puis attend theme_ui_pret() pour theme_rejouer_ui() :
 *       ce qu'un module a peint pendant le démarrage, avec l'ancienne palette, est repeint
 *       une fois le démarrage fini. Le même chemin sert à la bascule à chaud.
 * @ai_instruction Un module qui pose une couleur de la palette lui-même (ui_text_color,
 *       lv_obj_set_style_*_color, texte recoloré #RRGGBB, style C++ à lui) doit la
 *       repeindre depuis son dernier état dans une fonction <module>_rejouer_theme(),
 *       appelée ci-dessous ; une couleur lue d'un capteur par une lambda YAML se rejoue
 *       dans tab5_theme_repeindre. Preuve : le rendu hors tablette compare la bascule à
 *       chaud au démarrage à froid dans le même thème (tools/rendu/, job « thèmes »).
 */
#include "tab5_internal.h"
#include "esphome/core/application.h"
#include <cstring>

bool theme_selectionner(int theme, int mode, bool nuit) {
    if (theme < 0 || theme >= THEME_COUNT) theme = 0;
    const bool clair = mode == 1 || (mode == 2 && !nuit);
    const Palette& p = clair ? THEMES[theme].clair : THEMES[theme].sombre;
    // Palette = uniquement des uint32_t : pas de bourrage, memcmp fiable.
    if (std::memcmp(&p, &UIColor, sizeof(Palette)) == 0) return false;
    UIColor = p;
    ESP_LOGI("tab5.theme", "Thème %s, %s", THEMES[theme].nom, clair ? "clair" : "sombre");
    return true;
}

bool theme_ui_pret() { return esphome::App.is_setup_complete(); }

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
