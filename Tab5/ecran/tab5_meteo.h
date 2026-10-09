/**
 * [AI-CONTEXT]
 * @file tab5_meteo.h
 * @role Popup Météo (tab5_meteo.cpp, ADR-0043).
 * @architecture_constraint tab5_custom.h l'inclut : les lambdas YAML et les unités
 *       `tab5_*.cpp` le voient. Une fonction déclarée ici a un appelant hors de son fichier
 *       (règle 12 de tools/check_tab5_code_rules.py, qui lit tab5_custom.h et ses en-têtes) ;
 *       ce que seules les autres unités C++ appellent est dans tab5_internal.h.
 * @ai_instruction Une déclaration nouvelle de ce module va ici ; un module nouveau = un
 *       en-tête de plus, inclus par tab5_custom.h et listé sous `includes:` des deux
 *       configurations racine (tab5-ha-hmi.yaml, tab5-rendu-host.yaml).
 */
#pragma once
#include "esphome.h"
#include <string>

namespace esphome {
namespace font {
class Font;
}
}

// =============================================================================
// Météo graphique (ADR-0043, 09/10/2026, demande d'Axel) — tab5_meteo.cpp
// =============================================================================
// Popup « Météo » (meteo_popup.yaml) en trois pages, comme les Réglages : leurs noms en
// haut, à côté du titre, celle affichée en couleur d'accent ; un geste gauche / droite
// dans le popup ou un tap sur un nom change de page, sans animation.
//   - « Aujourd'hui » : le temps du moment (icône, température, condition, minimum et
//     maximum du jour, pluie prévue), puis les heures qui viennent (15 au plus) en
//     courbe des températures, icônes au-dessus, barres de pluie dessous, l'heure en
//     cours marquée ;
//   - « 10 jours » : une ligne par jour, minimum et maximum en barre sur une échelle
//     commune (dégradé des couleurs de la température), icône ; la température du
//     moment en point sur la ligne d'aujourd'hui ;
//   - « Détails » : la pluie dans l'heure (9 barres, la phrase de la carte centrale),
//     puis l'humidité, l'indice UV, et les probabilités de gel et de neige.
// Rien de nouveau n'est demandé à Home Assistant : ce sont les prévisions, la pluie, la
// météo actuelle et les probabilités qu'il pousse déjà (contrat inchangé).
//
// Pages, dans l'ordre des noms en haut (reglages_onglet.yaml, `page` en nombre dans le
// YAML) : ne pas les renuméroter.
enum MeteoPage : int {
    METEO_PAGE_JOUR = 0,
    METEO_PAGE_JOURS = 1,
    METEO_PAGE_DETAILS = 2,
    METEO_NB_PAGES = 3,
};
// Cartes du bas de la page Détails (meteo_carte.yaml, `n` en nombre).
enum MeteoCarte : int {
    METEO_CARTE_HUMIDITE = 0,
    METEO_CARTE_UV = 1,
    METEO_CARTE_GEL = 2,
    METEO_CARTE_NEIGE = 3,
    METEO_NB_CARTES = 4,
};

// Widgets posés par le script tab5_meteo_ouvrir (tab5-meteo.yaml) à la première
// ouverture : id() n'existe que dans une lambda YAML. Les tracés (heures, jours, pluie)
// sont construits en C++ dans leurs zones, à la première ouverture.
struct MeteoUI {
    lv_obj_t* popup = nullptr;                          // meteo_popup
    lv_obj_t* page[METEO_NB_PAGES] = {};                // meteo_page_jour, _jours, _details
    lv_obj_t* onglet[METEO_NB_PAGES] = {};              // meteo_onglet_jour, _jours, _details
    // Page Aujourd'hui, carte « Maintenant ».
    lv_obj_t* icone_l1 = nullptr;                       // meteo_maintenant_l1 (font_meteo_card)
    lv_obj_t* icone_l2 = nullptr;                       // meteo_maintenant_l2
    lv_obj_t* temperature = nullptr;                    // meteo_maintenant_temperature (roboto_55_b)
    lv_obj_t* condition = nullptr;                      // meteo_maintenant_condition
    lv_obj_t* min_max = nullptr;                        // meteo_maintenant_min_max
    lv_obj_t* pluie_titre = nullptr;                    // meteo_pluie_titre (« Pluie sur 15 h »)
    lv_obj_t* pluie_total = nullptr;                    // meteo_pluie_total
    lv_obj_t* zone_heures = nullptr;                    // meteo_zone_heures (1166 × 404)
    // Page 10 jours.
    lv_obj_t* zone_jours = nullptr;                     // meteo_zone_jours (1166 × 580)
    // Page Détails.
    lv_obj_t* pluie_phrase = nullptr;                   // meteo_pluie_phrase
    lv_obj_t* zone_pluie = nullptr;                     // meteo_zone_pluie (1166 × 290)
    lv_obj_t* carte_valeur[METEO_NB_CARTES] = {};       // meteo_carte_valeur_N
    lv_obj_t* carte_detail[METEO_NB_CARTES] = {};       // meteo_carte_detail_N
    // Polices des tracés : libellés, valeurs, icônes des heures et des jours.
    esphome::font::Font* police = nullptr;              // roboto_22
    esphome::font::Font* police_grasse = nullptr;       // roboto_32_b
    esphome::font::Font* icone = nullptr;               // font_meteo_card (120 px)
    esphome::font::Font* icone_petite = nullptr;        // font_meteo_card_small (80 px)
    esphome::font::Font* icone_m = nullptr;             // font_meteo_48
    esphome::font::Font* icone_m_petite = nullptr;      // font_meteo_32
};
extern MeteoUI g_meteo_ui;

// Construit les tracés (première fois) et montre une page (MeteoPage), peinte. Le script
// tab5_meteo_ouvrir ouvre ensuite le popup (animate_popup_open). Sans effet tant que
// g_meteo_ui.popup est nul.
void meteo_ouvrir(int page);
// Montre une page (MeteoPage) : les autres masquées, son nom en couleur d'accent, peinte.
// Un nom en haut (reglages_onglet.yaml) et le geste gauche / droite du popup.
void meteo_afficher_page(int page);
// Action tab5_maj_meteo_actuelle : condition (état météo de HA), température (°C, NAN si
// illisible), humidité (%, NAN si illisible). Gardées ; le popup affiché se repeint.
void meteo_actuelle_recue(const std::string& condition, float temperature, float humidite);
// Action tab5_maj_probabilites : indice UV, probabilités de gel et de neige (%), NAN si
// illisibles. Gardés ; le popup affiché se repeint.
void meteo_probabilites_recues(float uv, float gel, float neige);
// Appui long sur le corps d'une tuile de prévision (forecast_hour_card.yaml,
// forecast_day_body.yaml) : vrai si la tuile n'a pas d'appareil (son bouton, masqué par
// peindre_epaules, tab5_tuiles.cpp) ; le geste ouvre alors le popup Météo.
bool meteo_tuile_libre(lv_obj_t* bouton_appareil);
