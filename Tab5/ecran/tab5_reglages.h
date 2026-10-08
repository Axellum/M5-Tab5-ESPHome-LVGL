/**
 * [AI-CONTEXT]
 * @file tab5_reglages.h
 * @role Popup Réglages de la tablette (tab5_reglages.cpp).
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

// =============================================================================
// Réglages de la tablette (06/10/2026, demande d'Axel) — tab5_reglages.cpp
// =============================================================================
// Popup « Réglages » (reglages_popup.yaml), ouvert par un tap sur le bouton central du
// haut (engrenage ; son appui long ouvre la console système) ou par « Aller à l'écran →
// Réglages ». Les réglages de l'écran qu'on veut changer sans passer par Home Assistant :
// luminosité, extinction auto, rallumage par « Okay Nabu » et par une tape ; thème,
// clair ou sombre, nuit du mode Auto, langue. Chacun reste l'entité exposée à HA : le
// popup ne garde rien, il écrit l'entité (script tab5_reglages_choisir) et se repeint
// depuis elle (tab5_reglages_sync_ui, lancé par chaque entité quand elle change).
//
// Réglage d'un bouton à choix (reglages_choix_btn.yaml, paramètre `reglage` du script
// tab5_reglages_choisir ; `valeur` = index de l'option, 1/0 pour Oui/Non, ±1 pour le
// thème). Les valeurs sont écrites en nombres dans le YAML : ne pas les renuméroter.
enum ReglageId : int {
    REGLAGE_EXTINCTION = 0,   // select « Tab5 Extinction auto de l'écran » (index)
    REGLAGE_OKAY_NABU = 1,    // interrupteur « Tab5 Rallumer l'écran à Okay Nabu »
    REGLAGE_TAPE = 2,         // interrupteur « Tab5 Tap-to-Wake »
    REGLAGE_THEME = 3,        // select « Thème » : −1 précédent, +1 suivant (en boucle)
    REGLAGE_MODE = 4,         // select « Clair ou sombre » (index)
    REGLAGE_NUIT = 5,         // interrupteur « Nuit (thème auto) »
    REGLAGE_LANGUE = 6,       // select « Langue » : demande confirmation (redémarrage)
};

// Boutons à choix : autant que d'options (Oui/Non : 0 = Oui, 1 = Non). Langues : une
// pastille par langue de Tab5/lang/, nom natif écrit par reglages_preparer().
constexpr int REGLAGES_NB_EXTINCTION = 6;
constexpr int REGLAGES_NB_MODES = 3;
constexpr int REGLAGES_NB_LANGUES = 7;

// Widgets posés par le script tab5_reglages_ouvrir (tab5-reglages.yaml) à la première
// ouverture : id() n'existe que dans une lambda YAML.
struct ReglagesUI {
    lv_obj_t* popup = nullptr;                        // reglages_popup
    lv_obj_t* lum_slider = nullptr;                   // reglages_lum_slider
    lv_obj_t* lum_valeur = nullptr;                   // reglages_lum_valeur (« 80 % »)
    lv_obj_t* extinction[REGLAGES_NB_EXTINCTION] = {};
    lv_obj_t* okay_nabu[2] = {};
    lv_obj_t* tape[2] = {};
    lv_obj_t* theme_nom = nullptr;                    // reglages_theme_nom
    lv_obj_t* mode[REGLAGES_NB_MODES] = {};
    lv_obj_t* nuit[2] = {};
    lv_obj_t* langue[REGLAGES_NB_LANGUES] = {};
    lv_obj_t* confirmation = nullptr;                 // reglages_confirmation
    lv_obj_t* confirmation_texte = nullptr;           // reglages_confirmation_texte
};
extern ReglagesUI g_reglages_ui;

// État des entités, lu par tab5_reglages_sync_ui (index des selects, interrupteurs,
// luminosité du rétroéclairage en %).
struct ReglagesEtat {
    int luminosite = 100;
    int extinction = 0;
    bool okay_nabu = true;
    bool tape = true;
    int theme = 0;
    int mode = 0;
    bool nuit = false;
    int langue = 0;
};

// Noms natifs des langues sur leurs pastilles (une fois, à la première ouverture).
void reglages_preparer();
// Repeint le popup (choix en couleur d'accent). Sans effet avant la première ouverture.
void reglages_peindre(const ReglagesEtat& e);
// Valeur affichée à côté du curseur de luminosité, pendant qu'on le glisse.
void reglages_luminosite_ui(int pourcent);
// Pastille d'une langue : ouvre la confirmation (la tablette redémarre pour changer de
// langue). Rien pour la langue en cours.
void reglages_langue_demander(int langue);
// « Confirmer » : referme la confirmation et rend la langue choisie (−1 : aucune).
int reglages_langue_confirmer();
// « Annuler », voile ou croix : referme la confirmation.
void reglages_confirmation_fermer();
