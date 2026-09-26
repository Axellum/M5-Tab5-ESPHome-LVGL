/**
 * [AI-CONTEXT]
 * @file tab5_internal.h
 * @role Helpers partagés ENTRE les unités C++ issues de la scission de tab5_custom.cpp
 *       (lot (e), 08/09/2026). Ils étaient `static` dans le fichier unique ; ils ne font
 *       pas partie du contrat avec les YAML (qui n'incluent que tab5_custom.h).
 * @ai_instruction N'ajouter ici qu'un helper appelé depuis au moins deux unités. Un helper
 *                 propre à une unité reste `static` dans son .cpp.
 */
#pragma once
#include "tab5_custom.h"
#include <cstring>
#include <string>

// --- Écritures conditionnelles (audit du 26/09/2026, lot 3) ---
// En LVGL 9.5, lv_label_set_text() libère et réalloue le texte puis invalide le label
// même à texte identique (lv_label.c), et tout lv_obj_set_style_*() invalide l'objet
// (lv_obj_style.c) : un repaint pour rien à chaque capteur, interval ou push inchangé.
// Ces deux helpers comparent d'abord (mêmes gardes nulles que les appels remplacés).
inline void ui_text(lv_obj_t* label, const char* txt) {
    if (label == nullptr || txt == nullptr) return;
    const char* cur = lv_label_get_text(label);
    if (cur != nullptr && strcmp(cur, txt) == 0) return;
    lv_label_set_text(label, txt);
}
// Couleur de texte locale (partie principale, état par défaut, comme les
// lv_obj_set_style_text_color(o, …, LV_PART_MAIN) qu'il remplace).
inline void ui_text_color(lv_obj_t* obj, uint32_t hex) {
    if (obj == nullptr) return;
    const lv_color_t want = lv_color_hex(hex);
    lv_style_value_t cur;
    if (lv_obj_get_local_style_prop(obj, LV_STYLE_TEXT_COLOR, &cur, LV_PART_MAIN) == LV_STYLE_RES_FOUND &&
        lv_color_eq(cur.color, want)) {
        return;
    }
    lv_obj_set_style_text_color(obj, want, LV_PART_MAIN);
}

// --- tab5_text.cpp ---
// Normalise un texte venu de HA (Latin-1 / mojibake) en UTF-8 valide pour LVGL.
std::string normalize_text_utf8(const std::string& in);
// Bandeau de vigilance selon la couleur Météo-France (« Vert », « Orange »…).
const char* vigilance_alert_banner_utf8(const std::string& couleur);
// Libellés de jour relatifs à aujourd'hui (offset en jours) : « Mer 09 » / « mercredi 9 septembre ».
// format_short_day_label / format_long_day_label : tab5_core.h (logique pure).
// Vrai seulement si le texte contient un markup recolor LVGL #RRGGBB (évite les faux positifs sur un '#' isolé).
bool has_lvgl_recolor_markup(const std::string& t);
// Pose un texte sur un label en activant le recolor LVGL seulement s'il contient du #RRGGBB.
void set_label_text_utf8(lv_obj_t* label, const char* text);
// clock_month_short_utf8() : tab5_core.h.

// --- Sortis de tab5_custom.h le 25/09/2026 (audit, lot 8d) : appelés entre unités
// C++ mais jamais depuis un YAML — ils ne font pas partie du contrat.

void transition_widgets(lv_obj_t* out_obj, lv_obj_t* in_obj);

// Ferme un popup UNIQUEMENT s'il est réellement affiché et qu'aucun fondu n'est
// déjà en cours dessus. Renvoie true si une fermeture a été lancée.
// Le garde-fou sur l'animation évite un clignotement : animate_popup_close()
// repart de LV_OPA_COVER, la rejouer sur un popup à moitié effacé le
// rallumerait d'un coup avant de le refaire disparaître.
bool close_popup_if_open(lv_obj_t* card);

// Glissement horizontal + fondu croisé entre deux layers (swipe prévisions).
// dir = LV_DIR_LEFT (in arrive de la droite, out part à gauche) ou
//       LV_DIR_RIGHT (in arrive de la gauche, out part à droite).
// Durée UIAnim::SWIPE_DUR. Dérivée de transition_widgets() mais en horizontal.
void animate_swipe_horizontal(lv_obj_t* out_layer, lv_obj_t* in_layer, lv_dir_t dir);

// Slide-in depuis la droite + fondu pour un bandeau d'alerte qui entre
// dans le rotateur central (alertes HA, alertes Météo-France).
// Durée UIAnim::ALERT_DUR, ease_out.
void animate_alert_enter(lv_obj_t* alert_wrap);

// « Rouleau » d'icône météo : la nouvelle icône monte depuis le bas en
// apparaissant (translate_y relatif à l'offset de base posé par
// update_meteo_icon(), donc compatible avec les icônes composées l1+l2).
// delay_ms permet d'échelonner les 5 tuiles (effet vague).
void animate_icon_roll_in(lv_obj_t* l1, lv_obj_t* l2, uint32_t delay_ms);

uint32_t get_temperature_color(float t);

bool tab5_dismiss_local_has(const std::string& store, const std::string& id);

void tab5_dismiss_local_prune(std::string& store, const std::vector<std::string>& ids_seen);

void update_rain_phrase_ui(lv_obj_t* lbl, const std::string& phrase);
