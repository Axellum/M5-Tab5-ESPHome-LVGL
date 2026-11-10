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
#include "tab5_parse.h"
#include <ctime>
#include <string>

// =============================================================================
// Voitures : le popup « Voitures » (ADR-0079, lot 6 du plan des voitures) — tab5_voitures.cpp
// =============================================================================
// Une page seulement, « Aperçu » : une carte par voiture reçue (deux au plus, côte à côte ; une
// seule voiture : une carte pleine largeur). Chaque carte : le nom, la silhouette (un grand glyphe
// MDI, à la couleur de l'état), l'anneau de la batterie et son pourcentage, l'autonomie, une ligne
// d'état courte (« En charge · 7.20 kW ») et le détail (énergie ajoutée, fin de la charge, limite).
// Les pages Charge, Trajets et État s'y ajoutent (ADR-0046, ADR-0082) : une page sans donnée n'a pas
// d'onglet, et le popup n'a pas d'onglets tant qu'il n'a qu'une page. Ouvert en appuyant sur un
// cercle de voiture de la page Flux du popup Énergie, ou par « Aller à l'écran → Cars ».
constexpr int kVoituresCartes = 2;
// Pages du popup, dans l'ordre des onglets : Aperçu, Charge, Trajets, État (table kPages de
// tab5_voitures.cpp ; une page de plus = une ligne là et un onglet de plus dans voitures_popup.yaml).
constexpr int kVoituresPages = 4;

// Widgets posés par le script tab5_voitures_lier (tab5-voitures.yaml) : id() n'existe que dans
// une lambda YAML. Un champ [n] = la carte n (voitures_carte.yaml) ; une carte porte la voiture
// reçue au rang n + 1 ou, sans la première, la seconde en premier.
struct VoituresUI {
    lv_obj_t* popup = nullptr;                       // voitures_popup
    lv_obj_t* message = nullptr;                     // voitures_message : « Aucune voiture reçue »
    lv_obj_t* conseil = nullptr;                     // voitures_conseil : où la choisir dans Home Assistant
    lv_obj_t* onglet[kVoituresPages] = {};           // voitures_onglet_N (pages_onglet.yaml : noms des pages)
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
    // Événement esphome.tab5_energie (script tab5_energie_demande, lambda sans capture) : demande à
    // Home Assistant de pousser les voitures quand le popup s'ouvre sans en avoir reçu.
    void (*demander)() = nullptr;
    // style_glass_card (tab5-styles.yaml) : les cartes que les pages Charge et Trajets créent en C++ le
    // prennent, pour suivre le rayon, le bord, le fond et les couleurs de chaque thème comme les cartes
    // de l'Aperçu.
    lv_style_t* style_carte = nullptr;
};
extern VoituresUI g_voitures_ui;

// Une voiture reçue : ce que voiture_lire() a lu (tab5_parse.h), le nom et l'unité copiés (le payload
// ne vit pas plus loin). Gardée par voitures_recu() ; lue par les pages du popup (voiture_recue).
constexpr size_t kVoitureNomMax = 48;
constexpr size_t kVoitureUniteMax = 8;
struct VoitureRecue {
    bool presente = false;
    char nom[kVoitureNomMax] = {};
    char unite[kVoitureUniteMax] = {};
    VoitureEtat etat = VoitureEtat::INCONNUE;
    float batterie = NAN, autonomie = NAN, puissance = NAN, energie = NAN, limite = NAN, minutes = NAN;
    // L'instant de la poussée (0 : horloge pas réglée) : `minutes` valait ce nombre à cet instant, une page
    // qui en tire une heure d'arrivée compte le temps passé depuis.
    time_t recue_le = 0;
};
// La voiture de rang k + 1 (k = 0 ou 1) ; une voiture absente a presente = faux. k hors de 0..1 : la première.
const VoitureRecue& voiture_recue(int k);

// L'état d'une voiture, le même sur toutes les pages : sa couleur (charge en vert, câble branché en
// bleu, le reste en retrait) et son texte court (« En charge · 7.20 kW », « Branchée », « Débranchée »,
// « Hors ligne »).
uint32_t voiture_couleur_etat(VoitureEtat e);
void voiture_texte_etat(char* out, size_t n, const VoitureRecue& v);

// Repeint le popup s'il est ouvert : appelé par une page quand ses données arrivent (sans quoi elles
// attendraient la poussée suivante d'une voiture).
void voitures_repeindre();

// Le conteneur des pages : la carte modale du popup, où chaque page crée ses widgets en C++ (aucun
// id YAML de plus). nullptr tant que le popup n'est pas lié.
lv_obj_t* voitures_corps();

// Un onglet touché (pages_onglet.yaml, `choisir`) : `rang` parmi les pages qui ont des données.
void voitures_page(int rang);

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
