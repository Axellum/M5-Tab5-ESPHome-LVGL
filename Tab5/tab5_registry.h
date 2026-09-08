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
 *   - JEUX : la table vit dans tab5_registry.cpp, en C++ pur (les namespaces
 *     Marble, Arkanoid… sont de simples fonctions, aucun `id()` ESPHome).
 *     Ajouter une 9ᵉ console = UNE ligne dans `kGames`.
 *   - MODALES : les `lv_obj_t*` ne sont accessibles que par `id()` dans une
 *     lambda YAML. La liste est donc remplie UNE fois par le script
 *     `tab5_modal_registry_init` (tab5-scripts.yaml), idempotent, que chaque
 *     consommateur appelle avant de lire le registre. Ajouter un 10ᵉ popup =
 *     UNE ligne `ModalRegistry::add(...)` dans ce script (+ son option dans le
 *     select « Aller à l'écran » si on veut l'ouvrir depuis HA : `find()` la
 *     retrouve par son nom, donc les deux libellés doivent être identiques).
 *   - L'écran de SONNERIE (`alarm_ring_layer`) est enregistré en `LAYER` : il
 *     est NOMMÉ (HA doit savoir que la tablette sonne) mais JAMAIS refermé par
 *     `close_all()` — seuls tab5_alarm_stop / _snooze_now le referment.
 *   - « Neon Apron » est le DERNIER de `kGames` : sa fermeture restaure la
 *     rotation paysage 270 (ADR-0012), elle doit avoir le dernier mot.
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

int count();
const Entry& at(int i);

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

constexpr int MAX = 16;

// true dès que tab5_modal_registry_init a rempli la table.
bool ready();

// Enregistre une fenêtre. L'ORDRE d'appel est l'ordre de priorité de
// visible_name() : enregistrer d'abord ce qui recouvre le reste.
void add(lv_obj_t* obj, const char* name, Kind kind);

int count();

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

}  // namespace ModalRegistry
