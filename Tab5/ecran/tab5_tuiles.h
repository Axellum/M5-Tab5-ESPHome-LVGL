/**
 * [AI-CONTEXT]
 * @file tab5_tuiles.h
 * @role Pièces et tuiles (tab5_tuiles.cpp, tab5_tuiles_popups.cpp, tab5_tuiles_roue.cpp,
 *       ADR-0023) : définitions, widgets, appuis, popups.
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
#include "tab5_geometrie.h"
#include <string>

namespace esphome { namespace font { class Font; } }

// =============================================================================
// Pièces et tuiles génériques (tab5_tuiles.cpp, ADR-0023) : chaque page du bas est une
// pièce de cinq appareils au plus, décrits par Home Assistant. Pièce R ↔ page : R0 = 2
// (accueil), R1 = 3, R2 = 4, R3 = 1, R4 = 0 ; tuile T = position visuelle (0 = gauche).
// Tant qu'aucune définition n'est arrivée (drapeau en NVS), la pièce 0 est construite
// depuis les emplacements 3.x (mode héritage).
// =============================================================================
// Action tab5_maj_tuiles : instantané complet « pR|nom;tRT|type|icône|options|complément|
// nom;… » (ce qui n'est pas listé est vide). Gardé en NVS s'il change. Vrai si changé.
bool tuiles_definir(const std::string& payload);

// Widgets des pièces, posés par le script tab5_tuiles_ui (tab5-tuiles.yaml), que
// tab5_zones_apply lance à la fin du setup, avant la première image : id() n'existe que
// dans les lambdas YAML, et l'on_boot n'est pas à nous (tab5-ha-hmi.yaml).
struct TuilesUI {
    // Navigation : calques du bas, pastilles, titre de la carte centrale, bouton « HA ».
    lv_obj_t* calque_jours = nullptr;     // layer_forecast_daily
    lv_obj_t* calque_heures = nullptr;    // layer_forecast_hourly
    lv_obj_t* calque_ha = nullptr;        // layer_switches
    lv_obj_t* pastilles[5] = {};          // pbar_0 … pbar_4
    lv_obj_t* titre_cadre = nullptr;      // page_title_wrapper
    lv_obj_t* titre = nullptr;            // lbl_page_title
    lv_obj_t* bouton_ha = nullptr;        // btn_control_ha
    lv_obj_t* icone_ha = nullptr;         // icon_ha
    esphome::font::Font* police_meteo = nullptr;         // font_meteo_card
    esphome::font::Font* police_meteo_petite = nullptr;  // font_meteo_card_small
    // Mode météo : épaules (icône à gauche, ampoule ou flèche à droite) et bouton
    // invisible de chaque tuile, par position visuelle T (0 = gauche). Journalières :
    // mêmes objets sur les pages 2 à 4 ; horaires : l'objet h(4−T).
    lv_obj_t* jour_g[5] = {};
    lv_obj_t* jour_d[5] = {};
    lv_obj_t* jour_bouton[5] = {};
    lv_obj_t* jour_sens = nullptr;        // btn_j1_dir : sens du volet 3.x (mode héritage)
    lv_obj_t* heure_g[5] = {};
    lv_obj_t* heure_d[5] = {};
    lv_obj_t* heure_bouton[5] = {};
    // Libellés des onglets de titre (leur parent devient cliquable : sens d'un volet).
    lv_obj_t* jour_titre[5] = {};          // j{T}_day
    lv_obj_t* heure_titre[5] = {};         // h(4−T)_time
    // Cartes du mode HA (switches_card.yaml) : carte T = tuile T de la pièce courante.
    // Depuis le 06/10/2026, façon carte « tile » de HA : l'icône dans une pastille ronde
    // de la couleur de son état (carte_pastille, sw_pastille_N), le nom, l'état dessous.
    lv_obj_t* carte[5] = {};
    lv_obj_t* carte_pastille[5] = {};
    lv_obj_t* carte_icone[5] = {};
    lv_obj_t* carte_nom[5] = {};
    lv_obj_t* carte_etat[5] = {};
    // Popup qu'une tuile ouvre (climatisation : celle du blueprint avec l'option m, sinon
    // celle de la tuile, ADR-0027). La télécommande s'ouvre par telecommande_ouvrir()
    // (tab5_telecommande.h, ADR-0056), sur la page de la TV.
    lv_obj_t* popup_clim = nullptr;
    // Popups Lumières et Volets, une page par pièce (ADR-0046) : les lignes de la pièce
    // affichée (piece_ligne.yaml, 5 au plus) et les noms des pièces (pages_onglet.yaml).
    // Popup Lumières (light_popup.yaml).
    lv_obj_t* lum_popup = nullptr;        // light_options_popup
    lv_obj_t* lum_ligne[kTuiles] = {};    // btn_light_sel_N
    lv_obj_t* lum_onglet[kPieces] = {};   // lum_onglet_N
    lv_obj_t* lum_arc = nullptr;          // arc_light_brightness
    lv_obj_t* lum_pct = nullptr;          // lbl_light_brightness_val
    std::string* lum_cle = nullptr;       // &id(current_light_slot) : cible des commandes
    // Popup Volets (volet_popup.yaml, 05/10/2026) : le volet choisi, dessiné, à droite.
    lv_obj_t* vol_popup = nullptr;        // volet_popup
    lv_obj_t* vol_ligne[kTuiles] = {};    // volet_ligne_N
    lv_obj_t* vol_onglet[kPieces] = {};   // vol_onglet_N
    lv_obj_t* vol_nom = nullptr;          // volet_nom : le nom du volet choisi
    lv_obj_t* vol_position = nullptr;     // volet_position : rangée « 45 % »
    lv_obj_t* vol_nombre = nullptr;       // volet_nombre : chiffres (police de l'horloge)
    lv_obj_t* vol_etat = nullptr;         // volet_etat : l'état en mots
    lv_obj_t* vol_cadre = nullptr;        // volet_cadre : le volet dessiné, zone de glissement
    lv_obj_t* vol_tablier = nullptr;      // volet_tablier : les lames, glissent dans la fenêtre
    // Popup d'un appareil (appareil_popup.yaml, 06/10/2026) : ouvert par l'appui long d'une
    // tuile int, act, ou med sans l'option t.
    lv_obj_t* app_popup = nullptr;        // appareil_popup
    lv_obj_t* app_titre = nullptr;        // appareil_popup_titre
    lv_obj_t* app_pastille = nullptr;     // appareil_pastille : cercle de la couleur de l'état
    lv_obj_t* app_icone = nullptr;        // appareil_icone : palette des tuiles (mdi_font_70)
    lv_obj_t* app_etat = nullptr;         // appareil_etat : l'état en mots
    lv_obj_t* app_piece = nullptr;        // appareil_piece : « Pièce : … »
    lv_obj_t* app_options = nullptr;      // appareil_options : options de la tuile
    lv_obj_t* app_remplissage = nullptr;  // appareil_remplissage : dans le grand bouton
    lv_obj_t* app_commande_icone = nullptr;  // appareil_commande_icone (mdi_font_45)
    lv_obj_t* app_action = nullptr;       // appareil_action : ce que fera l'appui
    // Volet 3.x (mode héritage) : sens de la prochaine commande.
    bool* volet_sens = nullptr;           // &id(volet_target_open)
    // Commandes, posées par le script (lambdas sans capture) : événement
    // esphome.tab5_action (script tab5_action) et tap du volet 3.x (tab5_volet_tap).
    void (*envoyer)(const char* emplacement, const char* action, const char* valeur) = nullptr;
    void (*volet_tap)() = nullptr;
    // Option e (ADR-0028) : ouvre le popup Énergie (script tab5_energie_ouvrir).
    void (*energie_ouvrir)() = nullptr;
    // Appui long d'une med sans option t (ADR-0050) : le popup Musique sur le lecteur de la
    // tuile, `cle` = sa clé « tRT » (script tab5_lecteur_ouvrir).
    void (*lecteur_ouvrir)(const char* cle) = nullptr;
};
extern TuilesUI g_tuiles_ui;

// Appui sur la tuile T de la pièce de la page courante (tuile météo ou carte du mode
// HA) : commande selon le type et les options (tableau de l'ADR-0023), popup, ou rien.
void tuile_appui(int tuile, bool long_appui);

// Toucher du titre de la tuile T : bascule le sens d'un volet (flèche, puis appui).
void tuile_titre_appui(int tuile);
// Rend cliquables les onglets de titre (une fois, depuis tab5_tuiles_ui).
void tuiles_brancher_titres();

// Mode HA (bouton « HA », « Aller à l'écran → Accueil ») : cartes de la pièce de la page
// courante, ou de la plus proche qui a des appareils ; titre de la pièce dans la carte
// centrale ; en sortant, la météo de la page courante.
void tuiles_mode_ha(bool actif);
// Roue de navigation (ADR-0042, tab5_roue_navigation.cpp) : le mode HA sur la pièce R du
// blueprint (sa page, ses cartes, le titre de la carte centrale). Faux si elle n'a aucun
// appareil.
bool tuiles_aller_piece(int r);
// Écrans Lumières et Volet (Ecran::LUMIERES, Ecran::VOLET, ADR-0042) : une tuile en ouvre-t-elle
// le popup (ecran_sans_zone, tab5_zones.cpp) ; l'ouvrir (registre, tab5-navigation.yaml) —
// la pièce de la page affichée d'abord, puis les pièces dans l'ordre du blueprint.
bool tuiles_ecran_disponible(Ecran e);
void tuiles_ecran_ouvrir(Ecran e);

// Interrupteur « Tab5 Appareils sur la météo » (tab5-ha-controls.yaml, 05/10/2026) :
// éteint, les prévisions du mode météo ne montrent plus les appareils des pièces (ni
// épaules, ni bouton d'action, ni sens du volet par le titre) ; le mode HA ne change
// pas. Repeint tout de suite ; appelé au setup (restauration), il ne dessine rien.
void tuiles_appareils_meteo(bool montres);

// Mode héritage (blueprint 3.x) : les emplacements 3.x forment la pièce 0 — PC/TV
// (tuile 0), volet (1), lumiere_1..3 (2-4). Appelés par leurs capteurs
// (tab5-sensors-domotique.yaml) et par tab5_maj_volet_etat, quel que soit le mode.
void tuiles_heritage_pc(bool actif);
void tuiles_heritage_tv(bool actif);
void tuiles_heritage_lumiere(int i, bool allumee);
// Luminosité 0-255 (NaN éteinte) de lumiere_1..3 : l'arc du popup s'il la montre.
void tuiles_heritage_luminosite(int i, float luminosite);
// Renvoie vrai si le volet est en mouvement (volet_en_mouvement).
bool tuiles_heritage_volet(const std::string& etat_physique);
// Bouton btn_j1_dir (haut de la tuile du volet, mode héritage) : inverse le sens de la
// prochaine commande (volet_target_open) et repeint la flèche.
void tuiles_heritage_volet_sens();

// Popups à pages par pièce (ADR-0046, 09/10/2026) : Lumières (light_popup.yaml) et Volets
// (volet_popup.yaml), une page par pièce qui a des lumières (des volets pilotables : vol
// sans l'option r ni k). Ouverts par l'appui long d'une tuile (sa pièce, cette tuile), par
// « Aller à l'écran », la roue de navigation et les gestes de l'accueil (Ecran::LUMIERES,
// Ecran::VOLET, tuiles_ecran_ouvrir : la pièce affichée, sinon la première qui en a ; rien
// sans aucune). Leurs lignes
// (piece_ligne.yaml) : `idx` = rang dans la page.
//   - *_choisir : toucher d'une ligne, elle devient celle que pilotent la partie droite ;
//   - *_ligne_appui : toucher de sa pastille (le toucher de la tuile) ou appui long (sa
//     roue d'actions rapides, sinon le chemin de la tuile) ;
//   - *_page : tap sur le nom d'une pièce (pages_onglet.yaml) ; le glissement passe par
//     la même fonction (tab5_pages.cpp).
void popup_lumiere_page(int page);
void popup_lumiere_ligne_appui(int idx, bool long_appui);
void popup_volet_choisir(int idx);
void popup_volet_page(int page);
void popup_volet_ligne_appui(int idx, bool long_appui);
// Boutons Ouvrir / Stop / Fermer : la commande `action` (ouvrir, arreter, fermer) au
// volet choisi, comme son appui. Tout ouvrir / Tout fermer : à chaque volet de la page.
void popup_volet_commande(const char* action);
void popup_volets_tout(const char* action);
// Construit les lames du volet dessiné et branche les événements de son cadre (glisser :
// le dessin et le nombre suivent ; relâcher : « position » part), puis le glissement de
// pièce en pièce des deux popups. Une fois, depuis tab5_tuiles_ui.
void tuiles_brancher_popup_volet();

// Popup d'un appareil (appareil_popup.yaml, ouvert par l'appui long d'une tuile int, act,
// ou med sans l'option t, 06/10/2026, discussion #278). Son grand bouton : exactement le
// toucher de la tuile du popup (même commande, même confirmation avec l'option k).
void popup_appareil_appui();

// Popup Lumières : ses lignes sont les lumières de la pièce affichée, dans l'ordre des
// tuiles. Choisit la ligne `idx` : surbrillance, arc, et current_light_slot = clé de la
// tuile (tRT, ou lumiere_N en mode héritage).
void popup_lumiere_choisir(int idx);
// « Tout éteindre » : pR / eteindre (toutes les lumières de la pièce affichée), lumieres /
// eteindre en mode héritage.
void popup_lumiere_tout_eteindre();
// Couleur montrée d'une teinte de lampe (color_name : « warmwhite », « gold »…) : la
// seule liste, pour les pastilles du popup lumière (light_white_btn.yaml,
// light_color_preset_btn.yaml) et de la roue (UI-8). Nom inconnu : UIColor.TEXT_DIM.
uint32_t lampe_teinte(const char* nom);
