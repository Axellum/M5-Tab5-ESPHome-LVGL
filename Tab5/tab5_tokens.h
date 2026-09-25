/**
 * [AI-CONTEXT]
 * @file tab5_tokens.h
 * @role Jetons de design partagés : couleurs sémantiques (UIColor), durées et
 *       amplitudes d'animation (UIAnim), délais d'inactivité (UIIdle).
 * @architecture_constraint AUCUNE dépendance (ni ESPHome, ni LVGL) : que des
 *       `constexpr`. C'est ce qui permet aux jeux de l'inclure sans tirer
 *       `tab5_custom.h` — avant le 25/09/2026 (audit, lot 8a), toute retouche du
 *       HMI recompilait les 8 consoles, qui n'utilisaient que 4 couleurs.
 *       `tab5_custom.h` l'inclut : le contrat YAML ne change pas.
 * @ai_instruction Ne JAMAIS recréer des constantes de couleurs ailleurs : ajouter
 *       un jeton ici. Les palettes propres à un jeu restent locales (`<Jeu>::Pal`,
 *       ADR-0014).
 */
#pragma once
#include <cstdint>

// =============================================================================
// Helpers d'animation LVGL (popups, swipe, alertes)
// Réutilisent les patterns lv_anim_t de transition_widgets() (callbacks
// anim_y_cb/anim_opa_cb/anim_x_cb/anim_ty_cb).
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

// Couleurs semantiques centralisees (miroir des tokens YAML color:)
// Utiliser dans les lambdas C++ au lieu des hex bruts
// Palette "Dark Mode Slate" : miroir EXACT des tokens YAML color: (les garder synchro).
namespace UIColor {
    // --- Semantiques HSL vibrantes ---
    static constexpr uint32_t SUCCESS      = 0x34D399;  // emerald-400 (actif, OK)
    static constexpr uint32_t WARNING      = 0xFBBF24;  // amber-400 (attention)
    static constexpr uint32_t ERROR        = 0xFB7185;  // rose-400 (erreur, critique)
    static constexpr uint32_t INFO         = 0x38BDF8;  // sky-400 (info, connecte / aujourd'hui)
    static constexpr uint32_t GOLD         = 0xFCD34D;  // amber-300 (soleil, lune)
    static constexpr uint32_t TEXT_DIM     = 0x94A3B8;  // slate-400 (texte secondaire / repos)
    static constexpr uint32_t INACTIVE     = 0x334155;  // slate-700 (hors ligne / NaN)
    static constexpr uint32_t WARM_PINK    = 0xF472B6;  // pink-400 (temperature interieure chaude)
    // --- Accents "verre" ---
    static constexpr uint32_t ACCENT       = 0x22D3EE;  // cyan-400 (accent primaire / halo)
    static constexpr uint32_t ACCENT_ALT   = 0xA78BFA;  // violet-400 (accent secondaire)
    static constexpr uint32_t GLASS_RIM    = 0x93A3BC;  // Liseré lumineux (arête de verre)
    static constexpr uint32_t EARLY        = 0xFB923C;  // orange-400 (embauche < 9h — distinct de ERROR)
    static constexpr uint32_t PAST         = 0x64748B;  // slate-500 (jour passe, estompe)
    // --- Vigilance Meteo-France : NE PAS modifier (semantique officielle) ---
    static constexpr uint32_t ALERT_YELLOW = 0xFFFF00;  // Vigilance jaune MF
    static constexpr uint32_t ALERT_RED    = 0xFF0000;  // Vigilance rouge MF
    // --- Climatisation (popup grille 3x3, tab5_maj_clim) : valeurs inchangees,
    // seulement nommees pour sortir des hex en dur de tab5-api-logic.yaml ---
    static constexpr uint32_t CLIM_COOL_ACTIVE     = 0x4D94FF;  // Bleu vif
    static constexpr uint32_t CLIM_COOL_INACTIVE   = 0x60748F;  // Bleu grisatre inactif
    static constexpr uint32_t CLIM_HEAT_ACTIVE     = 0xFF4D4D;  // Rouge vif
    static constexpr uint32_t CLIM_HEAT_INACTIVE   = 0x8F6060;  // Rouge grisatre inactif
    static constexpr uint32_t CLIM_OFF_ACTIVE      = 0xFFA500;  // Orange
    static constexpr uint32_t CLIM_OFF_INACTIVE    = 0xB48154;  // Orange grise
    static constexpr uint32_t CLIM_TRACK_INACTIVE  = 0x4A596E;  // Gris (fan/swing/quiet inactifs)
    static constexpr uint32_t CLIM_ECO             = 0x4CD964;  // Vert standard
    // --- Forecast / alertes / pluie (tab5-api-logic.yaml) ---
    static constexpr uint32_t TEXT_PRIMARY         = 0xFFFFFF;  // Blanc labels forecast
    static constexpr uint32_t ALERT_DATE_YELLOW    = 0xFCF3CF;
    static constexpr uint32_t ALERT_DATE_ORANGE    = 0xF8C471;
    static constexpr uint32_t ALERT_DATE_RED       = 0xF1948A;
    static constexpr uint32_t RAIN_LIGHT           = 0x81D4FA;
    static constexpr uint32_t RAIN_MODERATE        = 0x29B6F6;
    static constexpr uint32_t RAIN_HEAVY           = 0x0277BD;
    static constexpr uint32_t RAIN_EXTREME         = 0x01579B;
    // --- Icones meteo / humidite / arc (miroir YAML + algorithmes) ---
    static constexpr uint32_t METEO_CELESTIAL      = 0xFFD700;  // Soleil / lune (IconeMeteo)
    static constexpr uint32_t METEO_PRECIP         = 0x8AB4FF;  // Pluie / neige / grele
    static constexpr uint32_t METEO_THUNDER        = 0xFF6600;  // Orage
    static constexpr uint32_t MOISTURE_NAN         = 0x404552;  // Humidite plante indisponible
    static constexpr uint32_t HUMIDITY_WET         = 0x0000CC;  // Air tres humide
    static constexpr uint32_t TEMP_NAN             = 0xA3A8B5;  // Temperature indisponible
    static constexpr uint32_t TEXT_SOFT            = 0xF1F5F9;  // Miroir color_text
    static constexpr uint32_t ICON_MUTED           = 0x555555;  // Miroir color_icon_muted
    static constexpr uint32_t ARC_TRACK            = 0x2A2D35;  // Miroir color_arc_track
    static constexpr uint32_t MODAL_SCRIM          = 0x05080F;  // Miroir color_modal_scrim
}
