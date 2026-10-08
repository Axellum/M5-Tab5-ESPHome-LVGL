/**
 * [AI-CONTEXT]
 * @file tab5_services.h
 * @role Services HA (tab5_services.cpp) : garde anti-rendu des poussées, vigilance, pluie,
 *       bandeau planning, fuseau horaire de HA.
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

// Garde anti-rendu des poussées HA (audit du 25/09/2026, lot 3). HA repousse tout
// au cycle /10 min et à chaque (re)connexion, le plus souvent à l'identique ; or en
// LVGL 9 un setter réécrit et invalide même à valeur égale, et chaque poussée
// repeignait tout le bandeau météo (boucle 66 ms au repos → 144 ms au push).
// Vrai si `payload` est identique au précédent reçu sur ce canal (empreinte
// FNV-1a 32 bits + longueur ; la première réception après boot n'est jamais
// « identique »). Une collision ferait sauter UNE mise à jour, rattrapée au
// changement suivant.
enum class PushChannel : uint8_t {
    JOURS, HEURES_0, HEURES_1, HEURES_2, VIGILANCE, PLUIE, ALERTES_HA, INFO, COUNT
};
bool push_unchanged(PushChannel ch, const char* data, size_t len);
bool push_unchanged(PushChannel ch, const std::string& payload);
// Les variables `string` des actions API arrivent en esphome::StringRef (vue sans
// copie) : passer `payload.c_str(), payload.size()` évite une copie de 2 Ko sur le
// tas pour le cas courant (push identique, rien d'autre à faire).

// -----------------------------------------------------------------------------
// Services HA (tab5-api-logic.yaml) : logique LVGL sortie des lambdas le
// 08/09/2026 (ADR-0006, audit du 06/09 §4.1 point 1). Un service ne fait plus
// que résoudre ses `id()`, poser ses globals et appeler l'une de ces fonctions ;
// tools/check_tab5_code_rules.py (pytest) interdit tout `lv_*` dans le contrat
// (hors lv_obj_has_flag, lecture pure).
// -----------------------------------------------------------------------------

// Volet (tab5_maj_volet_etat) : voir « Pièces et tuiles » plus bas — le volet 3.x est
// la tuile 1 de la pièce 0 du mode héritage (tuiles_heritage_volet).

// Vigilance Météo-France : phrase pluie, date recolorée, 4 slots d'icônes.
struct VigilanceUI {
    lv_obj_t* lbl_phrase;      // lbl_proc_pluie
    lv_obj_t* lbl_pluie_val;   // masqués quand la phrase s'affiche
    lv_obj_t* lbl_pluie_unit;
    lv_obj_t* lbl_date;        // recoloré selon la vigilance globale
    lv_obj_t* slots[4];        // alerte_slot_0..3
};

// payload = 11 champs séparés par « | » : phrase pluie, vigilance globale
// (Vert / Jaune / Orange / Rouge), puis vent, inondation, orages,
// pluie-inondation, neige-verglas, grand froid, vagues-submersion, canicule,
// avalanches. Les 4 premiers phénomènes ≠ Vert remplissent les slots (jaune /
// orange / rouge). Retourne true si au moins un phénomène est actif — à
// passer à central_set_vigilance().
bool parse_and_update_vigilance(const std::string& payload, const VigilanceUI& ui);

// Même chose pour les 9 barres en un appel : payload « idx|intensité;… » (ADR-0003,
// service tab5_maj_pluie_1h_bulk). Retourne has_rain, à passer à central_set_pluie().
bool update_rain_bars_bulk_ui(const std::string& payload, lv_obj_t* const bars[9]);

// Icône « pluie prédictive » de la carte centrale : flocon ambre si la
// probabilité de neige ≥ 5, sinon goutte colorée par l'hygrométrie
// (get_humidity_color). Appelée par tab5_maj_probabilites ET
// tab5_maj_meteo_actuelle : les deux services partagent le même rendu.
void update_rain_predict_icon_ui(lv_obj_t* icon, int neige, float humidite);

// Clim, retour de HA : clim_blueprint_recu() (service tab5_maj_clim), plus bas avec
// les réglages de la clim et le popup (ADR-0026, ADR-0027, tab5_clim.cpp).

void update_planning_text_ui(lv_obj_t* lbl, const std::string& l1, const std::string& l2,
    std::string& plan_ligne_1, std::string& plan_ligne_2);

// Construit les 2 prochaines lignes du bandeau planning depuis cal_jours_data[15]
// (après parse_and_update_jours_bulk) — remplace l'ancien push HA tab5_maj_planning.
void build_planning_lines_from_jours(std::string& out_l1, std::string& out_l2);

// =============================================================================
// Fuseau horaire de Home Assistant (tab5_services.cpp, lot 6b, ADR-0020). HA l'envoie
// avec l'heure (time: platform: homeassistant) mais rien ne le garde : sans ces deux
// fonctions, un démarrage sans HA repartirait avec le fuseau de la compilation.
// =============================================================================
// Démarrage (interval de tab5-sensors-diagnostics.yaml) : remet le dernier fuseau reçu.
// N'agit qu'au premier appel.
void fuseau_restaurer();
// on_time_sync de l'horloge homeassistant : HA a donné l'heure, donc son fuseau.
void fuseau_recu_de_ha();
// Tick minute : range en NVS le fuseau de HA s'il a changé depuis le dernier rangement.
void fuseau_memoriser();
