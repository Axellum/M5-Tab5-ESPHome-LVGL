/**
 * [AI-CONTEXT]
 * @file tab5_assist.h
 * @role Assistant vocal (tab5_assist.cpp) : état du pipeline, décision du mot de réveil,
 *       popup Assistant.
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

namespace esphome { namespace font { class Font; } }

// =============================================================================
// Popup Assistant vocal (assistant_popup.yaml)
// Affiche la demande (STT) + la réponse écrite du moteur, avec prise en charge
// des tableaux Markdown (alignés en police monospace) et d'une image (online_image).
// Logique centralisée ici (décision 0006 : pas de logique complexe dans le YAML LVGL).
// =============================================================================

// -----------------------------------------------------------------------------
// Pipeline vocal — un état, une couleur d'icône micro, un libellé de statut.
// Remplace les 5 blocs identiques des callbacks voice_assistant: (alors dans
// tab5-hardware.yaml, dans tab5-assist.yaml depuis le lot 8c ; audit du 06/09/2026 §4.1 point 6).
// -----------------------------------------------------------------------------
enum class AssistState : uint8_t {
    IDLE,       // gris   « Prêt »
    LISTENING,  // vert   « Écoute… »
    THINKING,   // orange « Analyse… » (sert aussi d'accusé de réception du volet)
    SPEAKING,   // bleu   « Réponse »
    ERROR,      // rouge  « Erreur »
};

// Icône micro du dashboard + label statut du popup Assistant (nuls acceptés).
void assist_set_pipeline_state(lv_obj_t* icon_mic, lv_obj_t* lbl_status, AssistState st);

// Icône micro seule : retour au gris 2 s après une erreur, interruption,
// accusé de réception « Stop » volet — le label du popup n'est pas touché.
void assist_set_mic_state(lv_obj_t* icon_mic, AssistState st);

// Zone image de la réponse (service tab5_assist_reponse + callbacks online_image).
enum class AssistImage : uint8_t {
    NONE,     // pas d'image : indication et image masquées
    LOADING,  // « Chargement image... », image masquée le temps du téléchargement
    READY,    // image affichée, indication masquée
    ERROR,    // « Image indisponible », image masquée
};
void assist_image_state_ui(lv_obj_t* hint, lv_obj_t* img, AssistImage st);

// Indicateur « Ok Nabu: ON / OFF » du panneau switches (switch tab5_wake_word_active).
void assist_wake_word_indicator_ui(lv_obj_t* lbl, bool on);

// -----------------------------------------------------------------------------
// Décision du mot de réveil (on_wake_word_detected, tab5-assist.yaml) —
// audit §4.1 point 7 : les 5 niveaux d'if/else du YAML deviennent une table.
// Le YAML lit les entrées UNE fois, appelle decide(), puis le script
// tab5_wake_word_dispatch (tab5-assist.yaml) exécute l'action.
// -----------------------------------------------------------------------------
namespace WakeWord {

enum Action : uint8_t {
    ALARM_STOP,        // le réveil sonne : TOUT mot l'arrête, avant tout le reste
    VOLET_STOP,        // « Stop » pendant que le volet bouge : arrêt local + HA
    INTERRUPT_LISTEN,  // réponse en cours (va_stop_armed + audio) : on coupe et on ré-écoute
    START_PIPELINE,    // « Okay Nabu » au repos, wake word actif et HA joignable
    IGNORE_STOP,       // « Stop » sans rien à arrêter
    IGNORE_INACTIVE,   // « Okay Nabu » mais wake word désactivé ou HA injoignable
};

struct Inputs {
    bool alarm_ringing;
    bool is_stop;            // wake_word == "Stop"
    bool volet_en_mouvement;
    bool va_stop_armed;
    bool audio_busy;         // haut-parleur actif, annonce en cours, ou pipeline pas démarré
    bool wake_word_enabled;  // switch tab5_wake_word_active
    bool api_connected;
};

// Table de décision, dans l'ordre de priorité :
//   alarm_ringing                                  → ALARM_STOP
//   is_stop && volet_en_mouvement                  → VOLET_STOP
//   va_stop_armed && audio_busy                    → INTERRUPT_LISTEN (Stop ou Okay Nabu)
//   is_stop                                        → IGNORE_STOP
//   wake_word_enabled && api_connected             → START_PIPELINE
//   sinon                                          → IGNORE_INACTIVE
Action decide(const Inputs& in);
const char* action_name(Action a);  // libellé pour les logs

}  // namespace WakeWord

// Renseigne la bulle "Votre demande" (texte STT normalisé UTF-8).
void assist_set_request(lv_obj_t* lbl_request, const std::string& texte);

// Renseigne la zone "Réponse" : normalise + format_assist_markdown + applique la
// police (nullptr : celle de la date du thème, portée par le style du label) puis le
// texte. Le retour à la ligne LVGL est géré par le YAML.
void assist_set_response(lv_obj_t* lbl_response, const std::string& texte,
    esphome::font::Font* font);

// Police de la réponse pour la taille `assist_text_size` : 2 → L (nullptr : la police
// de la date du thème, style_police_date du label), toute autre valeur → S (dont le 1
// de l'ancien M, essai D8 du 26/09/2026).
esphome::font::Font* assist_font(int size_idx, esphome::font::Font* f_s);

// Applique la taille de police de la réponse (0=S 2=L) SANS perdre le texte déjà
// affiché (relit lv_label_get_text). Met aussi à jour les 2 boutons A- / A+.
void assist_apply_text_size(lv_obj_t* lbl_response, int size_idx,
    esphome::font::Font* f_s, lv_obj_t* btn_s, lv_obj_t* btn_l);
