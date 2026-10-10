/**
 * [AI-CONTEXT]
 * @file tab5_telecommande.h
 * @role Popup télécommande à plusieurs télécommandes (tab5_telecommande.cpp, ADR-0056).
 * @architecture_constraint tab5_custom.h l'inclut : les lambdas YAML et les unités
 *       `tab5_*.cpp` le voient. Une fonction déclarée ici a un appelant hors de son fichier
 *       (règle 12 de tools/check_tab5_code_rules.py, qui lit tab5_custom.h et ses en-têtes).
 * @ai_instruction Un widget de plus = son champ dans TelecommandeUI et sa ligne dans le
 *       script tab5_telecommande_ui (Tab5/paquets/tab5-telecommande.yaml).
 */
#pragma once
#include "esphome.h"
#include <string>

// =============================================================================
// Télécommandes (ADR-0056, 10/10/2026, demande d'Axel) — tab5_telecommande.cpp
// =============================================================================
// Le popup télécommande (tv_remote_popup.yaml) montre une page par télécommande choisie
// dans le blueprint « Tab5 — emplacements » (« Télécommande de la TV », puis « Autres
// télécommandes » : quatre au plus). Le blueprint les pousse avec tous les états (clé
// « telecommandes|écran|nom|… » de tab5_maj_emplacements, tab5_parse.h section 13) : leur
// nom (un onglet en haut) et leur disposition (tv : Source, Muet, applications ; boitier :
// Stop, rangée de lecture). La tablette ne nomme aucune entité : chaque touche part dans
// l'événement esphome.tab5_action avec l'emplacement de la page montrée
// (telecommande_cle() : « tv », « tv1 »…), que le blueprint traduit pour la bonne
// télécommande. Sans la clé (blueprint plus ancien) ou avec une seule télécommande : la
// page unique d'avant, ni onglets ni glissement.
constexpr int kTelecommandeOnglets = 4;  // = kTelecommandesMax (tab5_parse.h)

// Widgets posés par le script tab5_telecommande_ui (tab5-telecommande.yaml), lancé par
// tab5_tuiles_ui à la fin du setup : id() n'existe que dans une lambda YAML.
struct TelecommandeUI {
    lv_obj_t* popup = nullptr;                          // tv_remote_popup
    lv_obj_t* titre = nullptr;                          // popup_tv_title
    lv_obj_t* onglet[kTelecommandeOnglets] = {};        // tv_onglet_N (pages_onglet.yaml)
    lv_obj_t* source = nullptr;                         // tv_touche_source (tv)
    lv_obj_t* stop = nullptr;                           // tv_touche_stop (boitier)
    lv_obj_t* muet = nullptr;                           // tv_touche_muet (tv)
    lv_obj_t* volume_icone = nullptr;                   // tv_volume_icone (boitier)
    lv_obj_t* applis = nullptr;                         // tv_rangee_applis (tv)
    lv_obj_t* lecture = nullptr;                        // tv_rangee_lecture (boitier)
};
extern TelecommandeUI g_telecommande_ui;

// Script tab5_telecommande_ui, une fois les widgets posés : le glissement de page en page
// (pages_brancher, tab5_pages.cpp) et la disposition de la page montrée.
void telecommande_brancher();
// Clé « telecommandes » de tab5_maj_emplacements (tab5_zones.cpp) : la liste des
// télécommandes, repeinte sur place si le popup est ouvert.
void telecommandes_recu(const char* valeur, size_t n);
// Ouvre le popup sur la page `page` (0 = la télécommande de la TV), ou sur celle déjà
// montrée (-1) ; une page qui n'existe pas : la première.
void telecommande_ouvrir(int page);
// Tap sur le nom d'une télécommande (pages_onglet.yaml).
void telecommande_page(int page);
// Emplacement de l'événement esphome.tab5_action de la page montrée : « tv », « tv1 »…
std::string telecommande_cle();
// Au moins une télécommande connue de HA : l'écran Télécommande est disponible même sans
// la TV du blueprint (zone TV absente), ecran_sans_zone() (tab5_zones.cpp).
bool telecommandes_connues();
// Thèmes (ADR-0029) : couleur d'accent de l'onglet de la page montrée. Appelé par
// theme_rejouer_ui() (tab5_theme.cpp).
void telecommande_rejouer_theme();
