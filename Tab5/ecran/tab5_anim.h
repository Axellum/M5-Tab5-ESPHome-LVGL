/**
 * [AI-CONTEXT]
 * @file tab5_anim.h
 * @role Animations et activité (tab5_anim.cpp) : inactivité et retour à l'accueil, tape de
 *       l'IMU (tab5_registry.cpp), popups, horloge à rouleau, appui des boutons verre.
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
#include "tab5_economie.h"  // ChoixAnimations

// UIAnim (durées/amplitudes d'animation) et UIIdle (retour à l'accueil par
// inactivité) : voir tab5_tokens.h.

// Millisecondes écoulées depuis la dernière activité (appui tactile ou
// ui_mark_activity()).
uint32_t ui_idle_ms();

// Remet le compteur d'inactivité à zéro sans qu'il y ait eu de toucher.
// À appeler sur toute activité « invisible » qui doit garder l'écran en place
// (événements du pipeline vocal, ouverture programmée d'un popup).
void ui_mark_activity();

// Retour automatique à l'accueil, un tick (interval 1 s de tab5-scripts.yaml ;
// 08/10/2026, audit YML-7). Ferme ce qui doit l'être (sous-fenêtres, popups) et dit au
// YAML ce qu'il lui reste à faire avec ses id() : QUITTER_ARCADE (revenir à page_main),
// POPUPS_FERMES (oublier l'état du popup Assistant), PREVISIONS (remettre la page
// principale des prévisions). Dans tab5_anim.cpp.
enum class RetourAuto : uint8_t { RIEN, QUITTER_ARCADE, POPUPS_FERMES, PREVISIONS };
RetourAuto retour_auto_tick(bool sur_page_arcade);

// IMU hors jeux (tab5-imu.yaml ; 08/10/2026, audit YML-7 : avant, deux `static` de
// lambda), dans tab5_registry.cpp. imu_tape_franche : (ax, ay, az) est une tape franche
// (> 2,5 g) au moins 500 ms après la dernière retenue (tap-to-wake). imu_cadence_a_changer :
// cadence de lecture voulue — 33 ms pour un jeu `imu_fast`, 100 ms pour un autre jeu ou
// pour écouter la tape (ecoute_tape : écran éteint et tap-to-wake activé), 0 sinon (plus
// de lecture) — ou -1 si elle n'a pas changé depuis l'appel précédent.
bool imu_tape_franche(float ax, float ay, float az, uint32_t maintenant_ms);
int32_t imu_cadence_a_changer(bool ecoute_tape);

// Ouverture d'un popup : affichage instantané (pas de fondu) ET passage au
// premier plan de son parent — ne pas le refaire après l'appel.
void animate_popup_open(lv_obj_t* card);

// Fermeture d'un popup : masquage instantané (LV_OBJ_FLAG_HIDDEN).
void animate_popup_close(lv_obj_t* card);

// Fondu croisé pur (sans glissement) entre deux calques plein cadre —
// bascule prévisions <-> switches HA (bouton « HA »). Le calque sortant est
// masqué à la fin. Durée UIAnim::SWIPE_DUR.
void animate_crossfade_layers(lv_obj_t* out_layer, lv_obj_t* in_layer);

// Suppression temporaire du rouleau d'icônes : mis à true pendant un
// changement de calque (le calque glisse déjà, un rouleau en plus = bruit).
extern bool g_forecast_roll_suppress;

// Niveau des animations de ce fichier (09/10/2026, ADR-0045), décidé par
// economie_decider() (tab5_economie.h : le select « Tab5 Animations », « Aucune » imposé
// par le mode économie actif) et posé par le script tab5_economie_appliquer :
// COMPLETES = toutes ; ESSENTIELLES = seule la rotation de la carte centrale et de la
// rangée sous l'horloge (transition_widgets) ; AUCUNE = chaque transition pose
// directement son état final. Les jeux gardent les leurs.
void animations_niveau(ChoixAnimations niveau);

// =============================================================================
// Horloge à rouleau — un rouleau PAR CHIFFRE (H H : M M)
// Chaque chiffre est un conteneur qui rogne (LVGL clippe les enfants au
// parent) contenant 2 labels : celui affiché et celui qui arrive. Au
// changement, les deux glissent d'une hauteur de boîte vers le haut — l'ancien
// sort, le nouveau entre.
// Découpage par chiffre et non par nombre : à 22 → 23 seule l'unité des
// minutes tourne, la dizaine ne bouge pas. C'est aussi 2× moins de surface
// repeinte par frame qu'un rouleau à deux chiffres.
// Repose sur des chiffres tabulaires (même avance pour 0-9, vrai pour Roboto :
// 75 px en 130 gras) — sinon les chiffres danseraient horizontalement.
// La géométrie (cadres, position des labels et du « : ») est posée dans
// tab5-lvgl.yaml seulement, vérifiée par tests/test_horloge.py ; la course du
// rouleau est la hauteur du cadre, lue dans son style.
// =============================================================================
struct ClockDigitRoller {
    lv_obj_t* wrap = nullptr;
    lv_obj_t* lbl[2] = {nullptr, nullptr};
    uint8_t   cur = 0;      // index du label actuellement affiché (0/1)
    char      shown = 0;    // chiffre peint ('0'..'9'), 0 = jamais peint
};

struct ClockRollerCtx {
    ClockDigitRoller d[4];    // HH:MM -> d[0] d[1] : d[2] d[3]
};
extern ClockRollerCtx g_clock_roller;

// --- 1D : Micro-interactions boutons verre ---

// Parcourt l'arbre LVGL depuis root et applique setup_button_press_animation()
// a tout objet clickable avec radius 18 (caracteristique du style_clim_btn verre).
// Appele une fois au boot via un interval one-shot (apres layout LVGL).
void apply_pressed_scale_to_tree(lv_obj_t* root);
// Marque (LV_OBJ_FLAG_USER_1) les boutons verre de l'état compilé, avant qu'un thème
// change leur rayon : apply_pressed_scale_to_tree les retient (tab5-themes.yaml).
void boutons_verre_marquer(lv_obj_t* root);

// Surbrillance bordure bouton (actif = couleur + active_width px, 2 par défaut ;
// inactif = GLASS_RIM + 1px). Le sélecteur du popup lumière passe 3 px.
void highlight_button_border(lv_obj_t* btn, bool active, uint32_t color, int32_t active_width = 2);

// L'heure passe par g_clock_roller (plus de label lbl_time unique) : seul le
// groupe qui change roule. La date reste un label simple.
void update_clock_date_ui(lv_obj_t* lbl_date,
    int hour, int minute, int day_of_week, int day_of_month, int month);
