/**
 * [AI-CONTEXT]
 * @file tab5_clim.h
 * @role Clim (tab5_clim.cpp, ADR-0026, ADR-0027) : réglages de l'appareil, carte de
 *       l'accueil, popup et clim affichée.
 * @architecture_constraint Sorti de tab5_custom.h le 08/10/2026, lignes recopiées telles
 *       quelles : tab5_custom.h l'inclut, les lambdas YAML et les unités `tab5_*.cpp` n'ont
 *       rien à changer. Une fonction déclarée ici a un appelant hors de son fichier (règle 12
 *       de tools/check_tab5_code_rules.py, qui lit tab5_custom.h et ses en-têtes).
 * @ai_instruction Une déclaration nouvelle de ce module va ici ; un module nouveau = un
 *       en-tête de plus, inclus par tab5_custom.h et listé sous `includes:` des deux
 *       configurations racine (tab5-ha-hmi.yaml, tab5-rendu-host.yaml).
 */
#pragma once
#include "esphome.h"
#include <string>

// Affichage optimiste de la cible de la carte clim de l'accueil (boutons − / +) avant
// le retour de HA (tab5_maj_clim). La carte montre toujours la clim du blueprint ; le
// popup a ses propres gestes (clim_popup_*, plus bas).
void update_clim_target_ui(lv_obj_t* lbl_target, lv_obj_t* arc, float target);

// =============================================================================
// Réglages de la clim venus de l'appareil (tab5_clim.cpp, ADR-0026) : clé « climr »
// de tab5_maj_emplacements, « climr|min|max|pas|unité|capacités|nom », que le
// blueprint pousse avant tab5_maj_clim. Bornes et pas des boutons − / + et de l'arc,
// °C ou °F, boutons que l'appareil gère, nom de la clim en titre du popup. Tant que
// rien n'est reçu (blueprint plus ancien), rien ne change : 16-30, pas de 0,5, °C,
// tous les boutons, titre du YAML. Pas de NVS : le blueprint les renvoie à chaque
// connexion.
//
// Clims des tuiles (ADR-0027) : une tuile cli sans l'option m a sa clim à elle, décrite
// par les clés « crRT|… » (mêmes réglages que climr) et « ceRT|consigne|pièce|mode|
// préréglage|ventilation|oscillation » (champs de tab5_maj_clim). Le popup montre la
// « clim affichée » : celle du blueprint (défaut), ou celle de la tuile touchée ; ses
// gestes et ses commandes vont à celle-là. La carte de l'accueil reste celle du
// blueprint, dont l'état est dans ses globals (clim_target_temp, clim_hvac_mode…).
// =============================================================================
// Widgets adaptés, posés par le script tab5_clim_ui (tab5-scripts.yaml), que lance
// tab5_zones_apply à la fin du setup.
struct ClimUI {
    lv_obj_t* popup = nullptr;              // clim_options_popup
    lv_obj_t* consigne_carte = nullptr;     // clim_target (carte de l'accueil)
    lv_obj_t* consigne_popup = nullptr;     // clim_target_popup
    lv_obj_t* arc = nullptr;                // arc_temp_popup
    lv_obj_t* unite = nullptr;              // clim_unite_popup (sous la cible du popup)
    lv_obj_t* piece = nullptr;              // val_temp_int_popup (température de la pièce)
    lv_obj_t* titre = nullptr;              // popup_clim_title
    // Carte MODE (pile flex : un bouton masqué ne laisse pas de trou).
    lv_obj_t* mode_froid = nullptr;         // popup_btn_clim_cool
    lv_obj_t* mode_chaud = nullptr;         // popup_btn_clim_heat
    lv_obj_t* mode_sec = nullptr;           // popup_btn_clim_dry
    lv_obj_t* mode_ventilation = nullptr;   // popup_btn_clim_fan
    // Carte OPTIONS (positions absolues, recalculées : les sections s'empilent).
    lv_obj_t* titre_presets = nullptr;      // clim_titre_presets
    lv_obj_t* rangee_presets = nullptr;     // clim_rangee_presets (rangée flex Éco / Boost)
    lv_obj_t* eco = nullptr;                // popup_btn_clim_eco
    lv_obj_t* boost = nullptr;              // popup_btn_clim_boost
    lv_obj_t* titre_ventilation = nullptr;  // clim_titre_ventilation
    lv_obj_t* silence = nullptr;            // popup_btn_clim_quiet
    lv_obj_t* titre_flux = nullptr;         // clim_titre_flux
    lv_obj_t* oscillation = nullptr;        // popup_btn_clim_swing
    lv_obj_t* brise = nullptr;              // popup_btn_clim_windnice
    // Icônes que clim_recolorer() colore selon l'état de la clim affichée, et le libellé
    // « Chaud » (traduction propre à la clim, clé « clim|Chaud »).
    lv_obj_t* icone_froid = nullptr;        // popup_icon_clim_cool
    lv_obj_t* icone_chaud = nullptr;        // popup_icon_clim_heat
    lv_obj_t* icone_sec = nullptr;          // popup_icon_clim_dry
    lv_obj_t* icone_ventilation = nullptr;  // popup_icon_clim_fan
    lv_obj_t* icone_eteint = nullptr;       // popup_icon_clim_off
    lv_obj_t* icone_eco = nullptr;          // popup_icon_clim_eco
    lv_obj_t* icone_boost = nullptr;        // popup_icon_clim_boost
    lv_obj_t* icone_silence = nullptr;      // popup_icon_clim_quiet
    lv_obj_t* icone_oscillation = nullptr;  // popup_icon_clim_swing
    lv_obj_t* icone_brise = nullptr;        // popup_icon_clim_windnice
    lv_obj_t* libelle_chaud = nullptr;      // popup_lbl_clim_heat
    // État de la clim du blueprint : ses globals, qu'écrivent tab5_maj_clim et les gestes
    // (carte, popup quand il la montre).
    float* consigne_bp = nullptr;           // &id(clim_target_temp)
    std::string* mode_bp = nullptr;         // &id(clim_hvac_mode)
    std::string* preset_bp = nullptr;       // &id(clim_preset_mode)
    std::string* ventilation_bp = nullptr;  // &id(clim_fan_mode)
    std::string* oscillation_bp = nullptr;  // &id(clim_swing_mode)
    // Débounces de la consigne du popup (lambdas sans capture, comme g_tuiles_ui.envoyer) :
    // tab5_debounce_clim_temp (clim du blueprint, emplacement « clim ») et
    // tab5_debounce_clim_tuile (clim d'une tuile, emplacement et valeur pris au geste).
    void (*debounce_blueprint)() = nullptr;
    void (*debounce_tuile)() = nullptr;
};
extern ClimUI g_clim_ui;

// Boutons − / + de la carte de l'accueil (clim du blueprint) : un pas de plus (sens > 0)
// ou de moins, borné aux limites de l'appareil. NaN reste NaN (consigne inconnue).
float clim_consigne_suivante(float t, int sens);
// Consigne envoyée à HA (scripts tab5_debounce_clim_temp et _tuile) : deux décimales au
// plus, sans zéro final (« 21.5 », « 72 », « 21.25 ») ; le blueprint la relit en nombre.
std::string clim_consigne_texte(float t);

// Bascules du popup et coloration (clim_recolorer) : un mode « actif » sous tous les
// noms que lui donnent les appareils, les mêmes que les listes du blueprint (clim_eco,
// clim_silence, clim_oscillation ; tests/test_clim.py compare). La bascule envoie alors
// none / auto / stop, sinon away / quiet / swing : le blueprint traduit vers le mode que
// l'appareil connaît.
bool clim_eco_actif(const std::string& preset);
bool clim_silence_actif(const std::string& fan);
bool clim_oscillation_actif(const std::string& swing);
// Bouton de préréglage `bouton` (away = Éco, boost) : actif sur ce préréglage.
bool clim_preset_actif(const std::string& preset, const char* bouton);

// Retour de HA pour la clim du blueprint (tab5_maj_clim, après ses globals) : cible de la
// carte, et du popup (cible, arc, température de la pièce) s'il montre cette clim ; puis
// les couleurs.
void clim_blueprint_recu(float consigne, float piece);
// Couleurs : la cible de la carte d'après le mode de la clim du blueprint, la cible et
// les icônes du popup d'après la clim affichée.
void clim_recolorer();

// Clim affichée par le popup (ADR-0027). Celle du blueprint : carte de l'accueil, tuile
// avec l'option m, « Aller à l'écran → Climatisation », et à la fermeture (croix, voile).
// Celle d'une tuile : clim_afficher_tuile() (tab5_internal.h), à l'appui de la tuile.
void clim_afficher_blueprint();
// Emplacement des commandes du popup : « clim » (clim du blueprint) ou « tRT ».
const char* clim_affichee_cle();
// Gestes du popup, sur la clim affichée : affichage optimiste et couleurs (l'envoi suit,
// dans le YAML, avec clim_affichee_cle() et la valeur ci-dessous). La consigne part par
// l'un des deux débounces de ClimUI.
void clim_popup_consigne(float t);            // arc
void clim_popup_pas(int sens);                // − / + (consigne inconnue : rien)
void clim_popup_mode(const char* mode);       // Froid, Chaud, Sec, Ventilation, Éteint (off)
void clim_popup_preset(const char* bouton);   // Éco (away), Boost : le bouton ou none
void clim_popup_silence();                    // quiet ↔ auto
void clim_popup_oscillation();                // swing ↔ stop
void clim_popup_brise();                      // windnice ↔ stop
const std::string& clim_affichee_preset();
const std::string& clim_affichee_ventilation();
const std::string& clim_affichee_oscillation();
// Consigne d'une clim de tuile en attente (script tab5_debounce_clim_tuile) : emplacement
// et valeur pris au geste, pour qu'elle parte à sa clim même si le popup est déjà fermé.
const char* clim_tuile_attente_cle();
std::string clim_tuile_attente_texte();
