/**
 * [AI-CONTEXT]
 * @file tab5_registry.h
 * @role Registre UNIQUE des consoles arcade (GameRegistry) et des fenêtres
 *       modales (ModalRegistry). Avant lui, la liste des 8 jeux était recopiée
 *       dans 4 lambdas (fermeture globale, poll IMU, dispatch IMU, text_sensor
 *       « Écran courant ») et la liste des 8 popups dans 3 tables (`kPopups`
 *       ×2, `kScreens`, `kTargetOf`) — c'était la source documentée des oublis
 *       (Trivia manquait dans 5 cartes sur 8 avant `tab5_games_close_all`).
 *
 * @architecture_constraint
 *   - JEUX : la table vit dans le module des jeux, Tab5/jeux/jeux_registre.cpp, en
 *     C++ pur (les namespaces Marble, Arkanoid… sont de simples fonctions, aucun
 *     `id()` ESPHome). Ajouter une 9ᵉ console = UNE ligne dans `kGames`. Le module
 *     est optionnel (ADR-0065, `tab5_jeux`) : sans lui, Tab5/jeux/jeux_absents.cpp
 *     fournit une table vide, et ces fonctions répondent « aucun jeu ».
 *   - MODALES : les `lv_obj_t*` ne sont accessibles que par `id()` dans une
 *     lambda YAML. La liste est donc remplie UNE fois par le script
 *     `tab5_modal_registry_init` (tab5-navigation.yaml), idempotent, que chaque
 *     consommateur appelle avant de lire le registre. Ajouter un 10ᵉ popup =
 *     UNE ligne `ModalRegistry::add(...)` dans ce script (+ son option dans le
 *     select « Aller à l'écran » si on veut l'ouvrir depuis HA : `find()` la
 *     retrouve par son nom, donc les deux libellés doivent être identiques ;
 *     et son script d'ouverture en 4e argument s'il en a un, que `ouvrir()`
 *     lance pour tab5_ecran_ouvrir).
 *   - L'écran de SONNERIE (`alarm_ring_layer`) est enregistré en `LAYER` : il
 *     est NOMMÉ (HA doit savoir que la tablette sonne) mais JAMAIS refermé par
 *     `close_all()` — seuls tab5_alarm_stop / _snooze_now le referment.
 *   - « Neon Apron » est le DERNIER de `kGames` : sa fermeture restaure la
 *     rotation paysage 270 (ADR-0012), elle doit avoir le dernier mot.
 *   - GARAGE (17/10/2026) : un POPUP ou un LAYER inscrit et caché est « garé » loin
 *     sous l'écran (translate_y de kGarageY), remis à sa place dès qu'il se montre.
 *     Jamais un SUBWINDOW : roue_ouvrir() lit les coordonnées du voile de la roue
 *     (roue_actions) avant de le montrer, une roue garée s'ouvrait hors de l'écran.
 *     LVGL 9.5 parcourt, après CHAQUE invalidation, tous les objets qui recouvrent
 *     la zone, cachés compris (blur_walk_cb de lv_obj_pos.c, à la recherche d'un
 *     flou que ce firmware n'a pas) : les ~1 000 objets des popups fermés
 *     rallongeaient chaque invalidation de l'interface. Une racine hors de la zone
 *     est sautée avec tous ses enfants. Le contrôle se fait à LV_EVENT_REFR_START
 *     (avant la mise en page et le dessin : une fenêtre montrée par n'importe quel
 *     chemin est remise en place avant sa première image) ; animate_popup_open()
 *     la remet aussi tout de suite (degarer()), pour le code qui lit ses
 *     coordonnées dans le même tour de boucle. Ne jamais lire les coordonnées
 *     absolues d'une fenêtre cachée : elles sont celles du garage. Inutile avec LVGL 9.6 (parcours
 *     sauté sans flou) : à retirer quand ESPHome l'aura.
 *
 * @ai_instruction Ne JAMAIS recopier une liste de jeux ou de popups dans un
 *       YAML : passer par ces fonctions. Le garde-fou
 *       `tools/check_tab5_registry.py` (joué par pytest) échoue si un
 *       `X::is_open()` réapparaît dans un YAML, si un `*_game.h` n'est pas dans
 *       `kGames`, ou si un popup à carte modale n'est pas enregistré.
 */
#pragma once
#include "esphome.h"
#include <cstdint>

// ─────────────────────────────────────────────────────────────────────────────
// Consoles arcade
// ─────────────────────────────────────────────────────────────────────────────
namespace GameRegistry {

struct Entry {
    const char* name;                        // libellé affiché (sélecteur, HA)
    bool (*is_open)();
    void (*close)();
    void (*on_imu)(float ax, float ay, float az);
    bool imu_fast;                           // pilotage à l'inclinaison → IMU 30 Hz
};

// La table des consoles : définie par le module des jeux (jeux_registre.cpp) ou, sans
// lui, vide (jeux_absents.cpp, kJeux nullptr) ; un seul des deux est compilé
// (tab5-arcade.yaml ou tab5-arcade-absente.yaml).
extern const Entry* const kJeux;
extern const int kNbJeux;

// true si ce firmware a des jeux (module compilé) : l'Arcade est un écran de la tablette.
bool disponibles();

// true si une partie est en cours, quelle que soit la console.
bool any_open();

// Libellé de la console ouverte (« Fil d'Or »…), nullptr si aucune.
const char* open_name();

// Ferme toutes les consoles ouvertes, dans l'ordre de la table (Pinball en
// dernier, il restaure la rotation du dashboard).
void close_all();

// true si la console ouverte pilote à l'inclinaison (poll IMU rapide).
bool any_imu_fast_open();

// Pousse les 3 axes à toutes les consoles (simple stockage côté jeu : chaque
// on_imu() ignore l'échantillon quand sa console est fermée).
void dispatch_imu(float ax, float ay, float az);

}  // namespace GameRegistry

// ─────────────────────────────────────────────────────────────────────────────
// Fenêtres modales
// ─────────────────────────────────────────────────────────────────────────────
namespace ModalRegistry {

enum Kind : uint8_t {
    POPUP,      // fenêtre modale ordinaire : nommée, fondu de fermeture, se
                // referme seule après UIIdle::POPUP_MS d'inactivité
    SUBWINDOW,  // sous-fenêtre interne (confirmations, détail d'un jour) :
                // masquage sec, jamais nommée, refermée avec les popups
    LAYER,      // calque plein cadre nommé mais JAMAIS refermé par le registre
                // (sonnerie du réveil)
};

constexpr int MAX = 32;  // 25 inscrites au 10/10/2026 (Caméras, ADR-0049, et Suivi, ADR-0054, ont dépassé les 24 d'avant ; tests/test_reglages.py) : de la marge

// true dès que tab5_modal_registry_init a rempli la table.
bool ready();

// Ouverture d'un écran quand elle est plus qu'animate_popup_open() de la fenêtre :
// son script d'ouverture (une lambda sans capture du registre suffit).
using Ouvreur = void (*)();

// Enregistre une fenêtre. L'ORDRE d'appel est l'ordre de priorité de
// visible_name() : enregistrer d'abord ce qui recouvre le reste. `ouvreur`
// (08/10/2026) : ce que lance ouvrir() pour cette fenêtre, nullptr par défaut.
void add(lv_obj_t* obj, const char* name, Kind kind, Ouvreur ouvreur = nullptr);

// Lance l'ouvreur enregistré pour cette fenêtre (tab5_ecran_ouvrir,
// tab5-navigation.yaml). false si elle n'en a pas : à l'appelant de l'ouvrir
// par animate_popup_open().
bool ouvrir(lv_obj_t* obj);

// Libellé de la première fenêtre nommée visible (POPUP ou LAYER), nullptr si
// rien ne couvre le dashboard.
const char* visible_name();

// true si au moins un POPUP est visible (les LAYER n'entrent pas en compte :
// la sonnerie ne doit pas déclencher le retour automatique).
bool any_popup_visible();

// Retrouve une fenêtre nommée par son libellé (options du select HA « Aller à
// l'écran »). nullptr si inconnu.
lv_obj_t* find(const char* name);

// Masque les SUBWINDOW puis referme les POPUP visibles (fondu via
// close_popup_if_open). Les LAYER sont laissés tels quels.
void close_all();

// Remet tout de suite à sa place une fenêtre garée (voir GARAGE plus haut) ;
// rien si elle n'est pas inscrite ou pas garée. Appelée par animate_popup_open().
void degarer(lv_obj_t* obj);

}  // namespace ModalRegistry
