/**
 * [AI-CONTEXT]
 * @file tab5_voitures.h
 * @role Popup « Voitures » (tab5_voitures.cpp, ADR-0079) : les voitures électriques de la maison,
 *       une grande carte chacune (silhouette, anneau de la batterie, autonomie, état de la
 *       charge), d'après ce que Home Assistant pousse par tab5_maj_voiture (ADR-0076).
 * @architecture_constraint tab5_custom.h l'inclut : les lambdas YAML et les unités
 *       `tab5_*.cpp` le voient. Une fonction déclarée ici a un appelant hors de son fichier
 *       (règle 12 de tools/check_tab5_code_rules.py).
 * @ai_instruction Un widget de plus = son champ dans VoituresUI et sa ligne dans le script
 *       tab5_voitures_lier (Tab5/paquets/tab5-voitures.yaml), son id dans voitures_carte.yaml.
 */
#pragma once
#include "esphome.h"
#include <string>

// =============================================================================
// Voitures : le popup « Voitures » (ADR-0079, lot 6 du plan des voitures) — tab5_voitures.cpp
// =============================================================================
// Quatre pages au plus (ADR-0079, ADR-0081), dans l'ordre des onglets ; une page n'apparaît que si elle
// a des données et le popup n'a pas d'onglets sous deux pages (ADR-0046) :
//   - « Aperçu » (toujours là : sans voiture, elle dit « Aucune voiture reçue ») : une carte par voiture
//     reçue (deux au plus, côte à côte ; une seule voiture : une carte pleine largeur). Chaque carte :
//     le nom, la silhouette (un grand glyphe MDI, à la couleur de l'état), l'anneau de la batterie et
//     son pourcentage, l'autonomie, une ligne d'état courte (« En charge · 7.20 kW ») et le détail
//     (énergie ajoutée, fin de la charge, limite) ;
//   - « Charge » et « Trajets » : lots 8 et 9 du plan des voitures ;
//   - « État » (tab5_voitures_etat.cpp) : pneus, températures, verrou, Sentinelle, logiciel.
// Une page de plus = son numéro ici, son conteneur dans voitures_popup.yaml et son entrée dans la table
// kPages de tab5_voitures.cpp. Ouvert en appuyant sur un cercle de voiture de la page Flux du popup
// Énergie, ou par « Aller à l'écran → Cars ».
constexpr int kVoituresCartes = 2;
constexpr int kVoituresPages = 4;
constexpr int kPageApercu = 0;
constexpr int kPageCharge = 1;
constexpr int kPageTrajets = 2;
constexpr int kPageEtat = 3;

// Widgets posés par le script tab5_voitures_lier (tab5-voitures.yaml) : id() n'existe que dans
// une lambda YAML. Un champ [n] = la carte n (voitures_carte.yaml) ; une carte porte la voiture
// reçue au rang n + 1 ou, sans la première, la seconde en premier.
struct VoituresUI {
    lv_obj_t* popup = nullptr;                       // voitures_popup
    lv_obj_t* message = nullptr;                     // voitures_message : « Aucune voiture reçue »
    lv_obj_t* conseil = nullptr;                     // voitures_conseil : où la choisir dans Home Assistant
    lv_obj_t* carte[kVoituresCartes] = {};           // voitures_carte_N (carte de verre)
    lv_obj_t* nom[kVoituresCartes] = {};             // voitures_nom_N
    lv_obj_t* silhouette[kVoituresCartes] = {};      // voitures_silhouette_N (grand glyphe MDI)
    lv_obj_t* anneau[kVoituresCartes] = {};          // voitures_anneau_N (conteneur : les arcs y sont créés en C++)
    lv_obj_t* pct[kVoituresCartes] = {};             // voitures_pct_N : « 68 % » au centre de l'anneau
    lv_obj_t* autonomie[kVoituresCartes] = {};       // voitures_autonomie_N : « 326 » (roboto_55_b)
    lv_obj_t* unite[kVoituresCartes] = {};           // voitures_unite_N : « km », « mi »
    lv_obj_t* sous[kVoituresCartes] = {};            // voitures_sous_N : « d'autonomie »
    lv_obj_t* etat_icone[kVoituresCartes] = {};      // voitures_etat_icone_N (mdi_font_45)
    lv_obj_t* etat[kVoituresCartes] = {};            // voitures_etat_N : « En charge · 7.20 kW »
    lv_obj_t* detail[kVoituresCartes] = {};          // voitures_detail_N : énergie ajoutée, fin de charge
    lv_obj_t* limite[kVoituresCartes] = {};          // voitures_limite_N : « Limite 80 % »
    // Pages (ADR-0081) : les noms (pages_onglet.yaml, voitures_onglet_N) et le conteneur plein corps de
    // chaque page ; page[kPageApercu] reste nul (l'Aperçu garde ses cartes, ci-dessus).
    lv_obj_t* onglet[kVoituresPages] = {};           // voitures_onglet_N
    lv_obj_t* page[kVoituresPages] = {};             // voitures_page_etat, voitures_page_…
    // Événement esphome.tab5_energie (script tab5_energie_demande, lambda sans capture) : demande à
    // Home Assistant de pousser les voitures quand le popup s'ouvre sans en avoir reçu.
    void (*demander)() = nullptr;
};
extern VoituresUI g_voitures_ui;

// Action tab5_maj_voiture (ADR-0076), aussi lue ici : rang (« 1 » ou « 2 ») et le payload de dix
// champs (voiture_lire, tab5_parse.h). Garde la voiture ; repeint le popup seulement s'il est
// ouvert. Un payload vide retire la voiture.
void voitures_recu(const std::string& rang, const std::string& payload);
// Script tab5_voitures_ouvrir : peint le popup (dernier état reçu) avant son ouverture, et demande
// les voitures à Home Assistant si aucune n'est connue.
void voitures_ouvrir();
// Thèmes (ADR-0029) : couleurs des cartes, anneaux et silhouettes du popup s'il est ouvert.
// Appelé par theme_rejouer_ui() (tab5_theme.cpp).
void voitures_rejouer_theme();
// Les onglets (pages_onglet.yaml, `choisir: voitures_page`) : montre la page de rang `rang` parmi les
// pages montrées (comme energie_page).
void voitures_page(int rang);

// La page « État » (ADR-0081) : son code est dans son fichier.
#include "tab5_voitures_etat.h"
