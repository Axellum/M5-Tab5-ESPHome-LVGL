/**
 * [AI-CONTEXT]
 * @file tab5_tokens.h
 * @role Jetons de design partagés : palette de couleurs de l'interface (Palette,
 *       UIColor = palette active), durées et amplitudes d'animation (UIAnim),
 *       délais d'inactivité (UIIdle).
 * @architecture_constraint AUCUNE dépendance (ni ESPHome, ni LVGL) : des
 *       `constexpr` et une seule variable, `UIColor`. C'est ce qui permet aux jeux
 *       de l'inclure sans tirer `tab5_custom.h` — avant le 25/09/2026 (audit,
 *       lot 8a), toute retouche du HMI recompilait les 8 consoles, qui
 *       n'utilisaient que 4 couleurs. `tab5_custom.h` l'inclut : le contrat YAML
 *       ne change pas.
 * @ai_instruction Ne JAMAIS recréer des constantes de couleurs ailleurs : ajouter
 *       un rôle à `Palette` (et sa valeur à chaque palette). L'interface lit
 *       `UIColor.X` (palette active, thèmes : ADR-0029) ; le YAML passe par les
 *       styles de rôle de tab5-styles.yaml, jamais par une couleur posée sur le
 *       widget. Les jeux lisent `PALETTE_SOMBRE.X` (ils restent sombres) et leurs
 *       palettes propres restent locales (`<Jeu>::Pal`, ADR-0014).
 */
#pragma once
#include <cstdint>

// =============================================================================
// Helpers d'animation LVGL (popups, swipe, alertes)
// Réutilisent les patterns lv_anim_t de transition_widgets() (callbacks
// anim_opa_cb/anim_x_cb/anim_ty_cb).
//
// [28/07/2026] Passe « animations légères » : toutes les durées et amplitudes
// sont regroupées ici (UIAnim) — c'était la seule façon de les régler d'un
// coup. L'écran est en `update_interval: never` : c'est LVGL qui redessine
// depuis la loop ESPHome, donc chaque frame d'animation = un repaint de la
// zone animée (fond verre + dégradé compris). Durée courte = moins de frames
// = moins de charge ET moins de latence perçue. Les amplitudes ont été
// réduites en même temps : un glissement de 84px sur un panneau plein cadre
// coûte le même repaint qu'un de 28px, mais se « traîne » visuellement.
// Popups : ouverture et fermeture instantanées (animate_popup_open/_close),
// d'où l'absence de jeton POPUP_* (retirés le 25/09/2026, jamais lus).
// =============================================================================
namespace UIAnim {
    constexpr uint32_t PANEL_DUR    = 190;  // rotateur central (etait 450)
    constexpr int32_t  PANEL_OFFSET = 28;   // px glissement vertical (etait 84)
    constexpr uint32_t SWIPE_DUR    = 200;  // swipe previsions (etait 350)
    constexpr int32_t  SWIPE_OFFSET = 110;  // px (etait 200)
    constexpr uint32_t ALERT_DUR    = 180;  // entree bandeau alerte (etait 300)
    constexpr int32_t  ALERT_OFFSET = 44;   // px (etait 100)

    // Effet « rouleau » (horloge + icones meteo).
    constexpr uint32_t ROLL_CLOCK   = 240;  // minutes / heures
    constexpr uint32_t ROLL_ICON    = 190;  // icone de prevision
    constexpr int32_t  ROLL_ICON_PX = 22;   // amplitude d'entree de l'icone
    constexpr uint32_t ROLL_STAGGER = 28;   // decalage entre 2 tuiles (effet vague)
}

// =============================================================================
// Retour automatique à l'écran principal (inactivité tactile)
// [28/07/2026, demande Axel] Un popup ou une page météo laissés ouverts
// reviennent seuls au dashboard. Les deux délais sont des délais d'INACTIVITÉ,
// pas des délais depuis l'ouverture : toucher la dalle remet le compteur à
// zéro, donc rien ne se ferme sous les doigts. Le compteur est celui de LVGL
// (lv_display_get_inactive_time), remis à zéro par l'indev à chaque appui —
// et par ui_mark_activity() sur les événements vocaux, sinon une conversation
// mains libres (qui ne touche jamais l'écran) fermerait le popup Assistant.
// =============================================================================
namespace UIIdle {
    constexpr uint32_t POPUP_MS    = 45000;  // popup ouvert -> fermeture
    constexpr uint32_t FORECAST_MS = 25000;  // page météo -> retour panneau principal
}

// =============================================================================
// Palette de l'interface (thèmes, ADR-0029)
// -----------------------------------------------------------------------------
// Un champ par RÔLE de couleur : c'est la seule source des couleurs de
// l'interface. `UIColor` est la palette active ; le C++ et les lambdas lisent
// `UIColor.X`, les styles de rôle du YAML (tab5-styles.yaml) aussi, quand LVGL
// les crée. Une palette de plus = une instance de plus, avec TOUS les champs
// (tests/test_themes.py vérifie qu'aucun n'est oublié : un champ omis vaudrait
// 0x000000 sans un mot du compilateur).
// Deux rôles de même valeur restent deux rôles (INFO et TEMP_MIN, INACTIVE et
// BAR_INACTIVE) : un autre thème peut les séparer.
// =============================================================================
struct Palette {
    // --- Fonds et verre ---
    uint32_t BG;               // fond de l'application (pages)
    uint32_t GLASS_HI;         // verre opaque : haut du dégradé (reflet)
    uint32_t GLASS_LO;         // verre opaque : bas du dégradé (ombre interne)
    uint32_t GLASS_HI_PAGE;    // verre pré-mélangé sur BG (essai D7), haut
    uint32_t GLASS_LO_PAGE;    // verre pré-mélangé sur BG, bas
    uint32_t GLASS_HI_MODAL;   // verre pré-mélangé sur MODAL_SCRIM, haut
    uint32_t GLASS_LO_MODAL;   // verre pré-mélangé sur MODAL_SCRIM, bas
    uint32_t GLASS_RIM;        // liseré lumineux (arête de verre)
    uint32_t MODAL_SCRIM;      // voile des popups
    uint32_t CONSOLE_BG;       // fond des zones de la console système
    uint32_t ARC_TRACK;        // piste des arcs et barres (clim, lumière, console)
    // --- Texte ---
    uint32_t TEXT_SOFT;        // texte courant (thème des labels)
    uint32_t TEXT_PRIMARY;     // blanc pur : prévisions, icônes météo
    uint32_t TEXT_DIM;         // texte secondaire / repos
    uint32_t CONSOLE_LABEL;    // libellés de la console système
    uint32_t CONSOLE_VALUE;    // valeurs numériques de la console
    uint32_t ICON_MUTED;       // icône désactivée / placeholder
    uint32_t TEXT_ON_ACCENT;   // texte et icône posés sur l'accent plein (« Tester », « Parler », « OK »)
    // --- Sémantiques ---
    uint32_t SUCCESS;          // actif, OK
    uint32_t WARNING;          // attention
    uint32_t ERROR;            // erreur, critique
    uint32_t INFO;             // info, connecté, aujourd'hui
    uint32_t GOLD;             // soleil, lune, réveil
    uint32_t INACTIVE;         // hors ligne, NaN
    uint32_t BAR_INACTIVE;     // barre / pastille éteinte
    uint32_t WARM_PINK;        // température intérieure chaude, anniversaires
    uint32_t ACCENT;           // accent primaire / halo
    uint32_t ACCENT_ALT;       // accent secondaire
    uint32_t EARLY;            // embauche < 9 h (distinct de ERROR)
    uint32_t PAST;             // jour passé, estompé
    uint32_t TEMP_MAX;         // température maximale (chaud)
    uint32_t TEMP_MIN;         // température minimale (froid)
    uint32_t RAIN_VALUE;       // valeur de la prévision de pluie
    // --- Vigilance Météo-France : jaune et rouge officiels, identiques dans tous les thèmes ---
    uint32_t ALERT_YELLOW;
    uint32_t ALERT_ORANGE;     // icône orange du bandeau (sur fond clair : un orange vif en pastille)
    uint32_t ALERT_RED;
    uint32_t ALERT_DATE_YELLOW;
    uint32_t ALERT_DATE_ORANGE;
    uint32_t ALERT_DATE_RED;
    // --- Climatisation (popup grille 3x3, tab5_maj_clim) ---
    uint32_t CLIM_COOL_ACTIVE;
    uint32_t CLIM_COOL_INACTIVE;
    uint32_t CLIM_HEAT_ACTIVE;
    uint32_t CLIM_HEAT_INACTIVE;
    uint32_t CLIM_OFF_ACTIVE;
    uint32_t CLIM_OFF_INACTIVE;
    uint32_t CLIM_TRACK_INACTIVE;  // fan / swing / quiet inactifs
    uint32_t CLIM_ECO;
    // --- Pluie, icônes météo, humidité ---
    uint32_t RAIN_LIGHT;
    uint32_t RAIN_MODERATE;
    uint32_t RAIN_HEAVY;
    uint32_t RAIN_EXTREME;
    uint32_t METEO_CELESTIAL;  // soleil / lune des icônes
    uint32_t METEO_CLOUD;      // nuage, brouillard, vent des icônes
    uint32_t METEO_PRECIP;     // pluie / neige / grêle
    uint32_t METEO_THUNDER;    // orage
    uint32_t MOISTURE_NAN;     // humidité d'une plante indisponible
    uint32_t HUMIDITY_WET;     // air très humide (80 % et plus : fin du dégradé d'humidité)
    uint32_t TEMP_NAN;         // température indisponible
    // --- Dégradés de température et d'humidité : couleur aux points d'ancrage ---
    // get_temperature_color() / get_humidity_color() (tab5_forecast.cpp) passent de
    // l'un à l'autre par paliers (2 °C, 3 %). Sur un fond clair, le blanc de 14 °C
    // serait invisible : chaque thème donne les siens.
    uint32_t TEMP_GRAD_M12;    // −12 °C et en dessous
    uint32_t TEMP_GRAD_0_NEG;  // 0 °C, fin de la montée depuis −12 °C
    uint32_t TEMP_GRAD_0_POS;  // juste au-dessus de 0 °C, début de la montée vers 14 °C
    uint32_t TEMP_GRAD_14;     // 14 °C (confort)
    uint32_t TEMP_GRAD_24;     // 24 °C
    uint32_t TEMP_GRAD_35;     // 35 °C et au-dessus
    uint32_t HUM_GRAD_14;      // 14 % et en dessous (air très sec)
    uint32_t HUM_GRAD_22;      // 22 %
    uint32_t HUM_GRAD_30;      // 30 % (confort ; vers HUMIDITY_WET à 80 %)
};

// Les palettes des thèmes : Tab5/themes/<thème>.yaml, un mode sombre et un mode clair
// chacun, écrits ici en C++ par tools/gen_themes.py (THEMES[], THEME_COUNT) entre les
// deux marques ci-dessous : NE PAS MODIFIER À LA MAIN (`--check` échoue en CI). Le
// premier thème, en sombre, est la palette d'origine de l'interface.
struct Theme {
    const char* nom;  // option du select « Thème » (Home Assistant la lit : jamais traduite)
    // En mode clair, le bandeau central / l'horloge gardent la palette sombre du thème
    // (`zones_sombres:`) : UIBandeau / UIHorloge (ci-dessous).
    bool bandeau_sombre;
    bool horloge_sombre;
    Palette sombre;
    Palette clair;
};
// >>> themes (généré par tools/gen_themes.py depuis Tab5/themes/, ne pas éditer)
inline constexpr Theme THEMES[] = {
    {"Ardoise", false, false,  // Tab5/themes/ardoise.yaml
     {  // sombre
      .BG                  = 0x0B1120,
      .GLASS_HI            = 0x2C3A52,
      .GLASS_LO            = 0x131C2C,
      .GLASS_HI_PAGE       = 0x1E293D,
      .GLASS_LO_PAGE       = 0x101727,
      .GLASS_HI_MODAL      = 0x27344A,
      .GLASS_LO_MODAL      = 0x111A28,
      .GLASS_RIM           = 0x93A3BC,
      .MODAL_SCRIM         = 0x05080F,
      .CONSOLE_BG          = 0x0A0E16,
      .ARC_TRACK           = 0x2A2D35,
      .TEXT_SOFT           = 0xF1F5F9,
      .TEXT_PRIMARY        = 0xFFFFFF,
      .TEXT_DIM            = 0x94A3B8,
      .CONSOLE_LABEL       = 0x8595AD,
      .CONSOLE_VALUE       = 0xFFFFFF,
      .ICON_MUTED          = 0x555555,
      .TEXT_ON_ACCENT      = 0xF1F5F9,
      .SUCCESS             = 0x34D399,
      .WARNING             = 0xFBBF24,
      .ERROR               = 0xFB7185,
      .INFO                = 0x38BDF8,
      .GOLD                = 0xFCD34D,
      .INACTIVE            = 0x334155,
      .BAR_INACTIVE        = 0x334155,
      .WARM_PINK           = 0xF472B6,
      .ACCENT              = 0x22D3EE,
      .ACCENT_ALT          = 0xA78BFA,
      .EARLY               = 0xFB923C,
      .PAST                = 0x64748B,
      .TEMP_MAX            = 0xF87171,
      .TEMP_MIN            = 0x38BDF8,
      .RAIN_VALUE          = 0xFB923C,
      .ALERT_YELLOW        = 0xFFFF00,
      .ALERT_ORANGE        = 0xFBBF24,
      .ALERT_RED           = 0xFF0000,
      .ALERT_DATE_YELLOW   = 0xFCF3CF,
      .ALERT_DATE_ORANGE   = 0xF8C471,
      .ALERT_DATE_RED      = 0xF1948A,
      .CLIM_COOL_ACTIVE    = 0x4D94FF,
      .CLIM_COOL_INACTIVE  = 0x60748F,
      .CLIM_HEAT_ACTIVE    = 0xFF4D4D,
      .CLIM_HEAT_INACTIVE  = 0x8F6060,
      .CLIM_OFF_ACTIVE     = 0xFFA500,
      .CLIM_OFF_INACTIVE   = 0xB48154,
      .CLIM_TRACK_INACTIVE = 0x4A596E,
      .CLIM_ECO            = 0x4CD964,
      .RAIN_LIGHT          = 0x81D4FA,
      .RAIN_MODERATE       = 0x29B6F6,
      .RAIN_HEAVY          = 0x0277BD,
      .RAIN_EXTREME        = 0x01579B,
      .METEO_CELESTIAL     = 0xFFD700,
      .METEO_CLOUD         = 0xFFFFFF,
      .METEO_PRECIP        = 0x8AB4FF,
      .METEO_THUNDER       = 0xFF6600,
      .MOISTURE_NAN        = 0x404552,
      .HUMIDITY_WET        = 0x0000CC,
      .TEMP_NAN            = 0xA3A8B5,
      .TEMP_GRAD_M12       = 0xFF0000,
      .TEMP_GRAD_0_NEG     = 0xFF00FF,
      .TEMP_GRAD_0_POS     = 0x0000FF,
      .TEMP_GRAD_14        = 0xFFFFFF,
      .TEMP_GRAD_24        = 0xFF00FF,
      .TEMP_GRAD_35        = 0xFF0000,
      .HUM_GRAD_14         = 0xFF0000,
      .HUM_GRAD_22         = 0xFFFF00,
      .HUM_GRAD_30         = 0xFFFFFF,
     },
     {  // clair
      .BG                  = 0xE8ECF1,
      .GLASS_HI            = 0xFFFFFF,
      .GLASS_LO            = 0xF5F7FA,
      .GLASS_HI_PAGE       = 0xFFFFFF,
      .GLASS_LO_PAGE       = 0xF3F5F8,
      .GLASS_HI_MODAL      = 0xF6F8FA,
      .GLASS_LO_MODAL      = 0xEAEEF3,
      .GLASS_RIM           = 0x8D99AA,
      .MODAL_SCRIM         = 0xC5CDD8,
      .CONSOLE_BG          = 0x0A0E16,
      .ARC_TRACK           = 0xD3DAE3,
      .TEXT_SOFT           = 0x1E2733,
      .TEXT_PRIMARY        = 0x0E141B,
      .TEXT_DIM            = 0x4F5A69,
      .CONSOLE_LABEL       = 0x8595AD,
      .CONSOLE_VALUE       = 0xFFFFFF,
      .ICON_MUTED          = 0x98A1AD,
      .TEXT_ON_ACCENT      = 0xFFFFFF,
      .SUCCESS             = 0x12785A,
      .WARNING             = 0x965A00,
      .ERROR               = 0xBB2C3E,
      .INFO                = 0x1D5DC2,
      .GOLD                = 0x8A6600,
      .INACTIVE            = 0x9AA4B1,
      .BAR_INACTIVE        = 0xC3CBD5,
      .WARM_PINK           = 0xB0306F,
      .ACCENT              = 0x0E6E85,
      .ACCENT_ALT          = 0x6548BE,
      .EARLY               = 0xB4500B,
      .PAST                = 0x6F7988,
      .TEMP_MAX            = 0xBE1E2D,
      .TEMP_MIN            = 0x1D5DC2,
      .RAIN_VALUE          = 0xB4500B,
      .ALERT_YELLOW        = 0xFFFF00,
      .ALERT_ORANGE        = 0xFF9F1A,
      .ALERT_RED           = 0xFF0000,
      .ALERT_DATE_YELLOW   = 0x8F6C00,
      .ALERT_DATE_ORANGE   = 0xB05600,
      .ALERT_DATE_RED      = 0xB8283A,
      .CLIM_COOL_ACTIVE    = 0x1D62D4,
      .CLIM_COOL_INACTIVE  = 0x5F7290,
      .CLIM_HEAT_ACTIVE    = 0xC92F2F,
      .CLIM_HEAT_INACTIVE  = 0x9E6767,
      .CLIM_OFF_ACTIVE     = 0xAF6200,
      .CLIM_OFF_INACTIVE   = 0x8E6A48,
      .CLIM_TRACK_INACTIVE = 0x6F7988,
      .CLIM_ECO            = 0x1C8240,
      .RAIN_LIGHT          = 0x5AA8DA,
      .RAIN_MODERATE       = 0x2C82C9,
      .RAIN_HEAVY          = 0x1A5FA6,
      .RAIN_EXTREME        = 0x0E3F78,
      .METEO_CELESTIAL     = 0xD66A00,
      .METEO_CLOUD         = 0x7A879A,
      .METEO_PRECIP        = 0x2C6AD3,
      .METEO_THUNDER       = 0xC94A00,
      .MOISTURE_NAN        = 0x6F7988,
      .HUMIDITY_WET        = 0x1E4FC2,
      .TEMP_NAN            = 0x6F7988,
      .TEMP_GRAD_M12       = 0xA3202B,
      .TEMP_GRAD_0_NEG     = 0x7B3FB8,
      .TEMP_GRAD_0_POS     = 0x1D5BD6,
      .TEMP_GRAD_14        = 0x2A3443,
      .TEMP_GRAD_24        = 0xB9480E,
      .TEMP_GRAD_35        = 0xBE1E2D,
      .HUM_GRAD_14         = 0xC22626,
      .HUM_GRAD_22         = 0x9C6400,
      .HUM_GRAD_30         = 0x2A3443,
     }},
};
inline constexpr int THEME_COUNT = static_cast<int>(sizeof(THEMES) / sizeof(THEMES[0]));
// <<< themes

// Palette sombre du thème d'origine : les jeux la lisent directement, ils restent
// sombres quel que soit le thème (ADR-0014).
inline constexpr Palette PALETTE_SOMBRE = THEMES[0].sombre;

// Palette active de l'interface : `lv_color_hex(UIColor.TEXT_DIM)`. Initialisée
// à la compilation (aucun ordre d'initialisation statique à craindre), puis
// remplacée par le thème choisi (theme_selectionner(), tab5_theme.cpp) avant que
// LVGL crée ses styles. Une table `constexpr` qui doit suivre le thème garde un
// pointeur de membre (`&Palette::TEXT_PRIMARY`, lu par `UIColor.*champ`), pas une
// valeur.
inline Palette UIColor = PALETTE_SOMBRE;

// Palettes du bandeau central et de l'horloge (thèmes, lot 3) : égales à UIColor, sauf
// en mode clair d'un thème qui garde ces zones sombres (`zones_sombres:` de
// Tab5/themes/<thème>.yaml) — elles prennent alors sa palette sombre. Ce qui est peint
// dans ces zones les lit à la place d'UIColor : styles style_bandeau_* et
// style_horloge_* (tab5-styles.yaml), et le C++ du bandeau (vigilance, pluie, planning,
// alertes, réponse vocale).
inline Palette UIBandeau = PALETTE_SOMBRE;
inline Palette UIHorloge = PALETTE_SOMBRE;

// Fond clair ? (luminosité perçue du fond, 0,299 R + 0,587 G + 0,114 B, au-dessus de
// la moitié). Pour ce qui n'est pas une couleur : la pastille sous les icônes de
// vigilance (un jaune pur ne se lit pas sur du blanc).
constexpr bool palette_claire(const Palette& p) {
    return 299u * ((p.BG >> 16) & 0xFF) + 587u * ((p.BG >> 8) & 0xFF) + 114u * (p.BG & 0xFF) > 127500u;
}
