/**
 * [AI-CONTEXT]
 * @file tab5_zones.h
 * @role Zones optionnelles, bandeau d'état, boutons du haut, batterie de la tablette,
 *       emplacements de la maison (tab5_zones.cpp).
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

// =============================================================================
// Zones optionnelles (tab5_zones.cpp, lot 5 de l'audit « ouverture », 27/09/2026)
// Une zone dont l'entité n'existe pas dans Home Assistant disparaît, avec ses
// boutons. La tablette ne décide pas seule : une entité créée pendant le démarrage
// de HA n'est transmise qu'à son prochain changement (manager.py de l'intégration
// ESPHome), un silence ne prouve donc rien. Elle demande à HA (esphome.tab5_zones,
// tab5-zones.yaml) et HA répond les zones absentes (action tab5_maj_zones) : depuis le
// lot 6a, le blueprint « Tab5 — emplacements », qui sait quels emplacements sont
// choisis et si leurs entités existent (ADR-0019). Une entité en panne existe :
// sa zone reste affichée (« -- », « Hors ligne »). Sans le package, rien ne
// disparaît. La liste est gardée en NVS (pas de clignotement au démarrage) et une
// zone réapparaît dès sa première donnée.
// =============================================================================
enum class Zone : uint8_t {
    // Emplacement suivi par la tablette (tab5-sensors-domotique.yaml), même ordre que
    // la liste envoyée par le script tab5_zones_demande.
    LUMIERE_1, LUMIERE_2, LUMIERE_3,   // chambre, salon, LEDs (tuiles J2 à J4)
    PC, TV, TELEPHONE, SALON, SERRE,
    POT_1, POT_2, POT_3, POT_4, POT_5,
    // Décidées par HA seul (entités du package, pas de la tablette).
    CLIM, VOLET, PLANNING,
    // Pipeline de discussion (29/09/2026) : absente quand la liste « Tab5 · pipeline de
    // discussion » vaut « Aucun » ; masque les boutons Domo / Discu. Toute nouvelle zone
    // s'ajoute ICI, à la fin : les bits sont gardés en NVS dans cet ordre.
    DISCUSSION,
    COUNT
};
constexpr int kZonesSuivies = static_cast<int>(Zone::CLIM);

// Bandeau d'état (haut gauche) : ses icônes dans l'ordre d'affichage, de gauche à droite.
// Les icônes visibles se suivent au pas de 35 px depuis x = 10 (bandeau_apply_ui,
// tab5_zones.cpp) : une icône masquée ne laisse pas de trou. Une icône de plus :
// 1. sa valeur ici, à sa place dans l'ordre (avant BANDEAU_NB) ;
// 2. son label dans tab5-lvgl.yaml (mdi_font_26, y: 10, x de sa place) et ses glyphes
//    dans mdi_font_26 (tab5-styles.yaml, règle 9) ;
// 3. son pointeur dans le script tab5_zones_apply (tab5-zones.yaml) ;
// 4. si elle peut disparaître, sa condition dans bandeau_masquee() (tab5_zones.cpp).
enum BandeauIcone : uint8_t {
    BANDEAU_PC,         // icon_pc : PC allumé (zone PC)
    BANDEAU_TELEPHONE,  // icon_phone : batterie du téléphone (zone TELEPHONE)
    BANDEAU_WIFI,       // icon_wifi
    BANDEAU_REVEIL,     // icon_alarm_status
    BANDEAU_SOLAIRE,    // icon_solaire : production solaire en % de la crête (clé solaire)
    BANDEAU_BATTERIE,   // icon_batterie : batterie de la tablette, si elle est montée
    BANDEAU_NB
};

// Boutons du haut à droite de l'accueil, de gauche à droite (06/10/2026 : un tap et un
// appui long chacun). Leurs deux gestes se choisissent dans le blueprint
// « Tab5 — emplacements » (section « Horloge et boutons du haut ») : clé « gestes » de
// tab5_maj_emplacements depuis le 09/10/2026 (Geste ci-dessous), et l'ancienne clé
// « appuis|maison|engrenage|manette » (07/10/2026, appuis longs seuls), toujours lue.
enum BoutonHaut : uint8_t {
    BOUTON_MAISON,     // btn_control_ha (tap « auto » : mode HA)
    BOUTON_ENGRENAGE,  // btn_control_console (tap « auto » : Réglages)
    BOUTON_MANETTE,    // btn_control_tv (tap « auto » : Arcade)
    BOUTON_HAUT_NB
};

// Gestes de l'accueil au choix (09/10/2026, lot A, ADR-0039) : l'horloge en trois zones
// tactiles (heures, minutes, date ; ui_components/horloge_zone.yaml) et les trois boutons
// du haut, un tap court et un appui long chacun. Cet ORDRE est celui des 12 champs de la
// clé « gestes|c1|…|c12 » de tab5_maj_emplacements (variable gestes du blueprint) et celui
// de la NVS (SauvegardeGestes, tab5_zones.cpp) : un geste de plus va à la FIN (nouveau
// champ, nouvelle préférence), aucun ne se déplace. tests/test_gestes.py compare.
enum Geste : uint8_t {
    GESTE_HEURES_COURT,     // auto : ligne suivante du panneau Ok Nabu (lot 3 ; « rien » avant)
    GESTE_HEURES_LONG,      // auto : Réveil
    GESTE_MINUTES_COURT,    // auto : appareil suivant de la tuile − / + (ADR-0033)
    GESTE_MINUTES_LONG,     // auto : Réveil
    GESTE_DATE_COURT,       // auto : ligne suivante de la rangée sous l'horloge (ADR-0031)
    GESTE_DATE_LONG,        // auto : Calendrier
    GESTE_MAISON_COURT,     // auto : mode HA (tuiles_mode_ha)
    GESTE_MAISON_LONG,      // auto : la clé appuis, sinon Énergie (avec la production solaire)
    GESTE_ENGRENAGE_COURT,  // auto : Réglages (page Écran)
    GESTE_ENGRENAGE_LONG,   // auto : la clé appuis, sinon la console système
    GESTE_MANETTE_COURT,    // auto : Arcade
    GESTE_MANETTE_LONG,     // auto : la clé appuis, sinon la télécommande TV
    GESTE_NB
};
// Geste d'un bouton du haut (ordre de BoutonHaut) : tap court ou appui long.
constexpr int geste_bouton(BoutonHaut b, bool long_appui) {
    return GESTE_MAISON_COURT + 2 * static_cast<int>(b) + (long_appui ? 1 : 0);
}
// Ce que fait un geste : ouvrir un écran (par tab5_ecran_ouvrir), ou une action de
// l'accueil. Le script tab5_geste (tab5-navigation.yaml) l'exécute.
enum class GesteAction : uint8_t {
    RIEN,
    ECRAN,             // `ecran` = valeur d'Ecran, pour id(tab5_ecran_ouvrir).execute()
    MODE_DOMO,         // bascule du mode HA (tuiles_mode_ha), le tap du bouton maison
    APPAREIL_SUIVANT,  // tuile − / + : appareil suivant (reglables_suivant)
    RANGEE_SUIVANTE,   // rangée sous l'horloge : ligne suivante (rangee_toucher)
    ECOUTE,            // bascule du mot de réveil « Ok Nabu » (switch tab5_wake_word_active)
    NABU_SUIVANTE,     // panneau Ok Nabu : ligne suivante (nabu_suivant, lot 3)
    ROUE,              // roue de navigation (ADR-0042), comme l'appui long de la carte centrale
};
struct GesteCible {
    GesteAction action;
    int ecran;  // valeur d'Ecran pour ECRAN, 0 sinon
};
// Geste (valeur de Geste) → ce qu'il fait maintenant : le choix du blueprint, « auto »
// sinon ; un écran absent de cette maison (ecran_disponible) ne fait rien.
GesteCible geste_cible(int geste);

// Écrans qu'ouvre le script tab5_ecran_ouvrir (tab5-navigation.yaml), routine unique du
// select « Aller à l'écran », des gestes de l'accueil (horloge, boutons du haut) et de la
// roue de navigation (ADR-0042). Les
// valeurs 0 à 17 SONT les index des options du select, dans le même ordre
// (tests/test_appuis.py) ; ARCADE n'est pas une option du select (lancer l'Arcade à
// distance n'a pas d'usage), seulement un choix de geste. Un écran de plus : avant ARCADE
// ici, à la fin du select (ARCADE et NB se décalent : la NVS garde l'index du code dans
// kCodesGestes, tab5_zones.cpp, jamais cette valeur), et son code à la fin de kCodesGestes
// et dans le blueprint ; sa fenêtre et son ouverture : une ligne de
// tab5_modal_registry_init (tab5-navigation.yaml).
// Liste gardée telle quelle par clang-format (une valeur de plus ne doit pas réécrire
// toute la liste, que d'autres lots touchent aussi).
// clang-format off
enum class Ecran : uint8_t {
    AUCUN,       // « — » : position de repos du select ; « rien » pour un appui long
    ACCUEIL,
    ASSISTANT,
    CALENDRIER,
    REVEIL,
    CLIM,
    PLANTES,
    TV,
    CONSOLE,
    ENERGIE,
    REGLAGES,
    ALERTES,
    MAISON,      // popup Maison (ADR-0037) : option du select et choix d'appui long (code « maison »)
    // Roue de navigation (ADR-0042, 09/10/2026) : les popups d'une tuile ouverts sans tuile
    // touchée — Lumières et Volets (une page par pièce, ADR-0046) sur la pièce affichée en
    // mode HA, sinon la première qui en a (tuiles_ecran_ouvrir) ; température de l'accueil,
    // celle du salon d'abord.
    LUMIERES,
    VOLET,
    TEMPERATURE,
    METEO,       // popup Météo (ADR-0043) : option du select et code de geste « meteo »
    CAMERAS,     // popup Caméras (ADR-0049) : option du select et code de geste « cameras »
    ARCADE,
    NB
};
// clang-format on

// Widgets que le masquage touche, posés par le script tab5_zones_apply (tab5-zones.yaml).
struct ZonesUI {
    lv_obj_t* bandeau[BANDEAU_NB] = {};  // bandeau d'état, indexé par BandeauIcone
    // Mini icônes des boutons du haut (06/10/2026), indexées par BoutonHaut
    // (icon_mini_ha, icon_mini_sys, icon_mini_tv) : ce que fait l'appui long du bouton,
    // quand il fait quelque chose (boutons_haut_apply_ui, tab5_zones.cpp). Les trois
    // boutons ne bougent pas : la manette ouvre l'Arcade même sans TV.
    lv_obj_t* mini[BOUTON_HAUT_NB] = {};
    // Icônes centrales des mêmes boutons (icon_ha, icon_engrenage, icon_manette, 09/10/2026) :
    // celle d'origine, ou ce que fait le tap quand le blueprint l'a changé.
    lv_obj_t* icone[BOUTON_HAUT_NB] = {};
    // Les cartes du calque « HA » et le sélecteur du popup lumière suivent les pièces
    // depuis l'ADR-0023 (g_tuiles_ui, tab5_tuiles.cpp).
    lv_obj_t* icon_salon = nullptr;
    lv_obj_t* val_salon = nullptr;
    lv_obj_t* icon_serre = nullptr;    // devient une manette sans capteur de serre
    lv_obj_t* val_serre = nullptr;
    // La ligne des plantes sous l'horloge suit la rangée (g_rangee_ui, ADR-0031).
    lv_obj_t* pot_card[5] = {};        // cartes du popup « Mes Plantes »
    // Mode vocal Domotique / Discussion (zone DISCUSSION) : boutons de l'accueil, du
    // popup assistant, et le titre « Cerveau / LLM » de ce dernier.
    lv_obj_t* btn_domo = nullptr;          // btn_mode_domo
    lv_obj_t* btn_discu = nullptr;         // btn_mode_discu
    lv_obj_t* assist_domo = nullptr;       // btn_assist_pipe_domo
    lv_obj_t* assist_discu = nullptr;      // btn_assist_pipe_discu
    lv_obj_t* assist_cerveau = nullptr;    // lbl_assist_cerveau
};
extern ZonesUI g_zones_ui;

bool zone_absente(Zone z);
// Donnée reçue : la zone réapparaît si elle était masquée. Vrai si l'affichage
// doit changer (le YAML relance alors tab5_zones_apply).
bool zone_vue(Zone z);
// Réponse de HA : clés des zones absentes, séparées par des virgules. Vrai si
// l'affichage doit changer.
bool zones_reponse_ha(const std::string& absentes);
// Applique l'état des zones aux widgets de g_zones_ui et aux tuiles (g_day_slots).
void zones_apply_ui();

// Batterie de la tablette, icône du bandeau d'état (tab5_zones.cpp). L'icône n'est
// visible que si l'interrupteur « Tab5 Batterie montée » est allumé
// (tab5-ha-controls.yaml, éteint par défaut). Allumé : une prise (couleur du texte du
// thème) quand il n'y a pas de batterie (batterie_presence(), tab5_batterie.h : tension
// lue chargeur coupé, 08/10/2026) ;
// sinon le glyphe suit le niveau (et « en charge »), couleur de get_battery_color(),
// la même échelle que le téléphone ; « ? » avant la première lecture. Chaque appel
// garde sa valeur : appelés avant le premier zones_apply_ui() (restauration de
// l'interrupteur au setup), ils ne dessinent rien, et zones_apply_ui() peint ensuite
// l'état gardé.
void batterie_montee_ui(bool montee);   // on_state de l'interrupteur
void batterie_niveau_ui(float niveau);  // % de batterie_niveau, NAN = inconnu
void batterie_charge_ui(bool en_charge);  // batterie_en_charge (CHG_STAT)
// Chaque lecture de l'INA226 (on_raw_value de batterie_tension, V) à l'instant
// `maintenant_ms` (millis()), passée à chargeur_tension() (tab5_batterie.h). Vrai si la
// décision « batterie détectée » vient de changer : le YAML publie alors « Tab5 Batterie
// détectée » et recalcule « Tab5 Batterie ».
bool batterie_tension_ui(float tension, uint32_t maintenant_ms);
// Vrai quand HA pousse la production solaire (clé solaire de tab5_maj_emplacements :
// puissance crête choisie dans le blueprint) : l'appui long du bouton « HA » ouvre alors
// le popup Énergie (choix « auto »), et sa mini icône le signale.
bool solaire_present();
// Écran dont la zone est absente de cette maison (clim, plantes sans aucun pot, TV ;
// Lumières et Volets sans aucune tuile de ce genre, ADR-0046) : sa fenêtre n'aurait rien
// à montrer ni à piloter. Lu par tab5_ecran_ouvrir.
bool ecran_sans_zone(Ecran e);
// Écran qu'un geste peut ouvrir : sa zone est là et, pour Énergie, la production
// solaire est reçue (la condition de l'appui long du bouton « HA » depuis le 06/10/2026).
// Plus strict que le select, qui ouvre Énergie sans production solaire.
bool ecran_disponible(Ecran e);
// Mini icônes des trois boutons : ce que fait leur appui long (glyphe de l'écran ou de
// l'action choisis, celles du 06/10/2026 en « auto », aucune sur l'engrenage), masquées
// s'il ne fait rien ; icônes centrales : ce que fait leur tap s'il n'est pas « auto ».
// Appelée par zones_apply_ui(), à la production solaire reçue ou perdue et au choix reçu.
void boutons_haut_apply_ui();
// Tuile i (0 à 4) de l'accueil : son appareil est-il absent ?
bool zone_tuile_absente(int tuile);
// Nombre de pots présents (0 à 5).
int zones_pots_presents();
// « aucune » ou « clim, pot_4, pot_5 » (capteur « Zones masquées » dans HA).
std::string zones_texte_masquees();
// Demande à HA une fois par connexion : remis à zéro à chaque connexion de HA,
// consommé par la première poussée des prévisions.
void zones_nouvelle_connexion();
bool zones_demande_a_envoyer();

// =============================================================================
// Emplacements de la maison (lot 6a, ADR-0019) : la tablette ne connaît plus aucune
// entité. Le blueprint « Tab5 — emplacements » pousse « clé|état|valeur;… »
// (action tab5_maj_emplacements) ; chaque clé est publiée dans le capteur interne qui
// la porte, dont le on_value met l'écran à jour comme avant. Les commandes repartent
// en événements esphome.tab5_action (script tab5_action, tab5-scripts.yaml).
// =============================================================================
struct EmplacementCible {
    const char* cle;
    esphome::text_sensor::TextSensor* texte;  // état HA tel quel (on, off, home…), ou nullptr
    esphome::sensor::Sensor* valeur;          // nombre affiché, NaN pour « nan » ou illisible, ou nullptr
};
// Applique la chaîne aux capteurs de la table ; une clé inconnue est ignorée. Les clés
// de tuile « tRT » (ADR-0023) vont d'abord aux pièces (tab5_tuiles.cpp).
// Renvoie le nombre d'entrées appliquées.
int emplacements_appliquer(const std::string& payload, const EmplacementCible* cibles, size_t n);
