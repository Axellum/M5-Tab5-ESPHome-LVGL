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
// Popup « Réglages » (reglages_popup.yaml) en quatre pages depuis le 08/10/2026 (demande
// d'Axel) : Écran, Apparence, Batterie, Système. Leurs noms sont en haut, à côté du titre,
// celle affichée en couleur d'accent ; un geste gauche / droite dans le popup ou un tap
// sur un nom change de page, sans animation. Tap sur l'engrenage ou « Aller à l'écran →
// Réglages » : page Écran ; appui long sur l'engrenage (choix « auto » du blueprint) ou
// « Aller à l'écran → Console système » : page Système, l'ancienne console système.
// Les réglages de l'écran qu'on veut changer sans passer par Home Assistant :
// luminosité, extinction auto, rallumage par « Okay Nabu » et par une tape ; thème,
// clair ou sombre, nuit du mode Auto, langue ; limite de charge, économie d'énergie,
// batterie montée. Chacun reste l'entité exposée à HA : le
// popup ne garde rien, il écrit l'entité (script tab5_reglages_choisir) et se repeint
// depuis elle (tab5_reglages_sync_ui, lancé par chaque entité quand elle change).
//
// Pages, dans l'ordre des noms en haut (reglages_onglet.yaml, `page` en nombre dans le
// YAML) : ne pas les renuméroter.
enum ReglagesPage : int {
    REGLAGES_PAGE_ECRAN = 0,
    REGLAGES_PAGE_APPARENCE = 1,
    REGLAGES_PAGE_BATTERIE = 2,
    REGLAGES_PAGE_SYSTEME = 3,   // la console système (console_sys.yaml)
    REGLAGES_NB_PAGES = 4,
};
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
    // Page Batterie (08/10/2026).
    REGLAGE_LIMITE_CHARGE = 7,    // select « Tab5 Limite de charge » (index)
    REGLAGE_ECONOMIE = 8,         // select « Tab5 Économie d'énergie » (index)
    REGLAGE_BATTERIE_MONTEE = 9,  // interrupteur « Tab5 Batterie montée »
};

// Boutons à choix : autant que d'options (Oui/Non : 0 = Oui, 1 = Non). Langues : une
// pastille par langue de Tab5/lang/, nom natif écrit par reglages_preparer().
constexpr int REGLAGES_NB_EXTINCTION = 6;
constexpr int REGLAGES_NB_MODES = 3;
constexpr int REGLAGES_NB_LANGUES = 7;
constexpr int REGLAGES_NB_LIMITES = 2;   // « 100 % », « 80 % » (LimiteCharge, tab5_batterie.h)
constexpr int REGLAGES_NB_ECONOMIE = 3;  // « Jamais », « Sur batterie », « Toujours »

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
    // Pages (08/10/2026) : leurs conteneurs et leurs noms en haut.
    lv_obj_t* page[REGLAGES_NB_PAGES] = {};           // reglages_page_ecran… reglages_page_systeme
    lv_obj_t* onglet[REGLAGES_NB_PAGES] = {};         // reglages_onglet_ecran… reglages_onglet_systeme
    // Page Batterie : boutons à choix et valeurs lues.
    lv_obj_t* limite[REGLAGES_NB_LIMITES] = {};
    lv_obj_t* economie[REGLAGES_NB_ECONOMIE] = {};
    lv_obj_t* montee[2] = {};
    lv_obj_t* batt_etat = nullptr;                    // reglages_batt_etat (« En charge »…)
    lv_obj_t* batt_niveau = nullptr;                  // reglages_batt_niveau
    lv_obj_t* batt_tension = nullptr;                 // reglages_batt_tension
    lv_obj_t* batt_conso = nullptr;                   // reglages_batt_conso
    // Page Système : ses deux confirmations, refermées quand on quitte la page.
    lv_obj_t* confirm_ha = nullptr;                   // overlay_confirm_ha
    lv_obj_t* confirm_reboot = nullptr;               // overlay_confirm_reboot
    // Une page vient de s'afficher : script tab5_reglages_page (tab5-reglages.yaml), qui
    // remplit la console (ses valeurs viennent d'entités, lisibles seulement en YAML).
    void (*page_montree)(int page) = nullptr;
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
    int limite = 0;
    int economie = 1;
    bool montee = false;
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
// Affiche une page (ReglagesPage) : les autres masquées, son nom en couleur d'accent, les
// confirmations refermées ; la page Batterie se peint, la page Système passe par
// page_montree. Appelée à l'ouverture (tab5_reglages_ouvrir), par un nom en haut et par un
// geste gauche / droite dans le popup (tab5_reglages.cpp).
void reglages_afficher_page(int page);
// Page affichée, popup ouvert : seul cas où la console (REGLAGES_PAGE_SYSTEME) et la page
// Batterie se rafraîchissent (interval 2 s de tab5-sensors-diagnostics.yaml, capteurs à
// 60 s, garde #T222). Faux avant la première ouverture.
bool reglages_page_visible(int page);
// Page Batterie : état, niveau, tension et consommation, d'après le dernier état gardé
// par le C++ (tab5_batterie.cpp, tab5_zones.cpp). Rien si la page n'est pas visible.
void reglages_batterie_peindre();
// Croix et voile : referme les confirmations (langue, page Système) et le popup.
void reglages_fermer();
