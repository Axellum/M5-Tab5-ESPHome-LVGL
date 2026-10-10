/**
 * [AI-CONTEXT]
 * @file tab5_serveur_ia.h
 * @role Serveur IA (tab5_serveur_ia.cpp, ADR-0059) : le popup plein écran « Serveur IA »,
 *       tableau de bord d'un serveur de LLM local (LM Studio, Ollama, llama.cpp, vLLM…)
 *       d'après ce que Home Assistant pousse par tab5_maj_serveur_ia.
 * @architecture_constraint tab5_custom.h l'inclut : les lambdas YAML et les unités
 *       `tab5_*.cpp` le voient. Une fonction déclarée ici a un appelant hors de son fichier
 *       (règle 12 de tools/check_tab5_code_rules.py) ; ce que seule la roue de navigation
 *       appelle (serveur_ia_disponible) est dans tab5_internal.h.
 * @ai_instruction Un widget de plus = son champ dans ServeurIaUI et sa ligne dans le script
 *       tab5_serveur_ia_lier (Tab5/paquets/tab5-serveur-ia.yaml).
 */
#pragma once
#include "esphome.h"
#include <string>

// =============================================================================
// Serveur IA : un serveur de LLM local (ADR-0059, 10/10/2026) — tab5_serveur_ia.cpp
// =============================================================================
// Home Assistant (packages/tab5_serveur_ia.yaml) lit les capteurs choisis dans les listes
// « Tab5 · serveur IA, … », assemble un payload et le pousse au changement, deux secondes
// au moins entre deux poussées. La tablette montre :
//   - un bandeau : pastille en ligne / hors ligne, nom du serveur, modèle chargé (« … ») ;
//   - à gauche, les tokens par seconde : grand chiffre, courbe des 24 dernières poussées
//     (gardée ici, environ la dernière minute quand le serveur travaille), requêtes en
//     cours et en file ;
//   - à droite, quatre cartes : VRAM (utilisée, totale, barre), GPU (température colorée
//     par le niveau que HA calcule : la tablette ne code aucun seuil), RAM (% et barre),
//     puissance (W).
// Une valeur inconnue s'écrit « — », jamais un zéro inventé. Popup fermé : rien n'est
// repeint, la poussée ne fait que garder les valeurs et le point de la courbe.
// Dans le bandeau, à droite (ADR-0060) : jusqu'à trois boutons d'action — décharger le
// modèle, réveiller le PC, redémarrer le service —, montrés, grisés ou absents selon ce
// que HA pousse (14e champ). Décharger et redémarrer demandent un second tap dans les
// 4 s (« Confirmer ? ») ; réveiller part au premier. Un tap envoie l'événement
// esphome.tab5_serveur_ia_action (champ `action` = code, jamais un texte libre) ; HA
// choisit quoi faire et sur quoi (packages/tab5_llm.yaml).
constexpr int kServeurIaCartes = 4;  // VRAM, GPU, RAM, puissance
constexpr int kServeurIaBoutons = 3;  // = kServeurIaActions (tab5_parse.h), vérifié par le .cpp

// Widgets posés par le script tab5_serveur_ia_lier (tab5-serveur-ia.yaml) : id() n'existe
// que dans une lambda YAML.
struct ServeurIaUI {
    lv_obj_t* popup = nullptr;                    // serveur_ia_popup
    lv_obj_t* attente = nullptr;                  // serveur_ia_attente (avant la première poussée, aucun capteur)
    lv_obj_t* conseil = nullptr;                  // serveur_ia_conseil
    lv_obj_t* bandeau = nullptr;                  // serveur_ia_bandeau (carte de verre)
    lv_obj_t* etat = nullptr;                     // serveur_ia_etat (« En ligne »…)
    lv_obj_t* nom = nullptr;                      // serveur_ia_nom
    lv_obj_t* modele = nullptr;                   // serveur_ia_modele
    lv_obj_t* tps_carte = nullptr;                // serveur_ia_tps_carte
    lv_obj_t* tps_icone = nullptr;                // serveur_ia_tps_icone
    lv_obj_t* tps_titre = nullptr;                // serveur_ia_tps_titre
    lv_obj_t* tps_valeur = nullptr;               // serveur_ia_tps_valeur (roboto_55_b)
    lv_obj_t* tps_unite = nullptr;                // serveur_ia_tps_unite
    lv_obj_t* requetes = nullptr;                 // serveur_ia_requetes
    lv_obj_t* carte[kServeurIaCartes] = {};       // serveur_ia_carte_N (serveur_ia_carte.yaml)
    lv_obj_t* icone[kServeurIaCartes] = {};       // serveur_ia_icone_N (mdi_font_32)
    lv_obj_t* titre[kServeurIaCartes] = {};       // serveur_ia_titre_N
    lv_obj_t* valeur[kServeurIaCartes] = {};      // serveur_ia_valeur_N (police de la date)
    lv_obj_t* detail[kServeurIaCartes] = {};      // serveur_ia_detail_N
    lv_obj_t* action[kServeurIaBoutons] = {};        // serveur_ia_action_N (serveur_ia_action.yaml)
    lv_obj_t* action_icone[kServeurIaBoutons] = {};  // serveur_ia_action_icone_N (mdi_font_32)
    lv_obj_t* action_texte[kServeurIaBoutons] = {};  // serveur_ia_action_texte_N
    // Événement esphome.tab5_serveur_ia_action (script tab5_serveur_ia_evenement) : le code.
    void (*envoyer)(const char* action) = nullptr;
};
extern ServeurIaUI g_serveur_ia_ui;

// Action tab5_maj_serveur_ia : le serveur (tab5_parse.h, section 14). Garde les valeurs et
// ajoute un point à la courbe ; repeint le popup seulement s'il est ouvert.
void serveur_ia_recu(const std::string& payload);
// Script tab5_serveur_ia_ouvrir : peint le popup (dernier état reçu) avant son ouverture.
void serveur_ia_ouvrir();
// Bouton d'action `i` touché (serveur_ia_action.yaml, ADR-0060) : rien s'il est absent,
// grisé ou si une action est partie il y a moins de 5 s ; décharger et redémarrer : le
// premier tap arme « Confirmer ? » (4 s), le second envoie ; réveiller : envoie.
void serveur_ia_action_touchee(int i);
// Thèmes (ADR-0029) : pastille, valeurs, barres et courbe. Appelé par theme_rejouer_ui()
// (tab5_theme.cpp).
void serveur_ia_rejouer_theme();
