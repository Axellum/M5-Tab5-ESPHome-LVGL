/**
 * [AI-CONTEXT]
 * @file tab5_cards.h
 * @role Cartes (tab5_cards.cpp) : température, plantes et pots, icônes d'état du bandeau.
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

// Met a jour un label de temperature (texte + couleur gradient). Factorise
// depuis temp_serre/temp_salon (tab5-sensors-domotique.yaml, Phase 3, #T164).
void update_temp_ui(lv_obj_t* label, float x);

// Structure pour les 4 slots UI d'humidite plantes (triés dynamiquement) : l'icône
// seule depuis le 05/10/2026 (plus de libellé « Pot X » / « Moy: » dessous).
struct MoistureSlotUI {
    lv_obj_t* icon_lbl;
};

// Tri dynamique : prend 5 valeurs, affiche les 2 plus secs + médiane + plus humide
// icons_utf8[5] = codes MDI pour chaque capteur, slots[4] = widgets LVGL de destination.
// Seuls les pots présents comptent (zones POT_1 à POT_5) : jusqu'à 4, chacun a son
// emplacement ; les emplacements en trop sont masqués. Les entrées sont gardées pour
// moisture_slots_refresh(), que zones_apply_ui() rejoue quand un pot apparaît.
void sort_and_update_moisture_slots(float values[5], const char* icons_utf8[5],
    MoistureSlotUI slots[4]);
void moisture_slots_refresh();

// Couleur batterie par niveau (échelle icône téléphone du bandeau, réutilisée
// par la ligne Batterie du popup détails pots).
uint32_t get_battery_color(float x);

// Icônes d'état du bandeau et des cartes (règle 2 : les sensors n'appellent pas
// LVGL). set_icon_color_ui pose une couleur calculée (batterie, humidité) ;
// set_icon_active_ui choisit entre deux tokens UIColor selon un booléen (API HA,
// Wi-Fi, TV). Tolèrent un widget nullptr (valeur reçue avant le layout).
void set_icon_color_ui(lv_obj_t* icon, uint32_t color);
void set_icon_active_ui(lv_obj_t* icon, bool active, uint32_t color_on, uint32_t color_off);

// PC (text_sensor pc_status) : icône du bandeau d'état. La carte du mode HA est la
// tuile 0 de la pièce 0 en mode héritage (tuiles_heritage_pc). Ne fait rien sans icon_pc.
void update_pc_status_ui(bool active, lv_obj_t* icon_pc);

// Popup détails pots (appui long sur les slots pots) : 5 cartes FIXES, carte N =
// capteur moisture_N (pas de tri dynamique, contrairement au dashboard).
struct PotDetailUI {
    lv_obj_t* icon_lbl;    // icône plante (couleur = humidité)
    lv_obj_t* moist_lbl;   // grande valeur % humidité
    lv_obj_t* status_lbl;  // OK / Bientôt sec / À arroser / Hors ligne
};

// Humidité + statut des 5 cartes — appelé par pots_humidite_maj() à chaque mise à
// jour d'un des 5 capteurs.
void update_pots_popup_moisture_ui(const float values[5], PotDetailUI cards[5]);

// Mise à jour d'un des 5 capteurs d'humidité (script tab5_pots_maj,
// tab5-sensors-domotique.yaml ; 08/10/2026, avant une lambda recopiée 5 fois par une
// ancre YAML) : zone de chaque pot dont le capteur a publié (publies[i], NaN compris :
// il existe dans HA), les 4 emplacements triés de la ligne des plantes avec l'icône de
// chaque pot, les 5 cartes du popup « Mes Plantes ». true si une zone de pot vient
// d'apparaître : l'appelant relance tab5_zones_apply. Dans tab5_rangee.cpp.
bool pots_humidite_maj(const bool publies[5], const float vals[5], MoistureSlotUI slots[4],
                       PotDetailUI cards[5]);

// Une métrique secondaire d'une carte pot (texte + couleur). L'humidité passe par
// update_pots_popup_moisture_ui, pas par cet enum.
enum class PotMetric { CONDUCTIVITY, ILLUMINANCE, TEMPERATURE, BATTERY };
void update_pot_metric_ui(lv_obj_t* value_lbl, float x, PotMetric metric);
