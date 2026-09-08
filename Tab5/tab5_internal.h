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
#include <string>

// --- tab5_text.cpp ---
// Normalise un texte venu de HA (Latin-1 / mojibake) en UTF-8 valide pour LVGL.
std::string normalize_text_utf8(const std::string& in);
// Bandeau de vigilance selon la couleur Météo-France (« Vert », « Orange »…).
const char* vigilance_alert_banner_utf8(const std::string& couleur);
// Libellés de jour relatifs à aujourd'hui (offset en jours) : « Mer 09 » / « mercredi 9 septembre ».
std::string format_short_day_label(int jour_offset);
std::string format_long_day_label(int jour_offset);
// Vrai seulement si le texte contient un markup recolor LVGL #RRGGBB (évite les faux positifs sur un '#' isolé).
bool has_lvgl_recolor_markup(const std::string& t);
// Pose un texte sur un label en activant le recolor LVGL seulement s'il contient du #RRGGBB.
void set_label_text_utf8(lv_obj_t* label, const char* text);
// Mois abrégé en français (1-12), UTF-8.
const char* clock_month_short_utf8(int month);
