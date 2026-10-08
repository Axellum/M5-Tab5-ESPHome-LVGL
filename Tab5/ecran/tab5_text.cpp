/**
 * [AI-CONTEXT]
 * @file tab5_text.cpp
 * @role Texte : normalisation UTF-8 des textes HA (Latin-1 / mojibake), store local
 *       des alertes HA rejetées, libellés français des jours/mois, helpers de texte
 *       LVGL (recolor #RRGGBB).
 *       Unité de compilation issue de la scission de tab5_custom.cpp (lot (e) de
 *       l'audit du 06/09/2026, faite le 08/09/2026) : mêmes fonctions, même ordre,
 *       aucune logique modifiée.
 * @regle_absolue Seul point de contact avec l'API LVGL, comme avant : les YAML
 *                n'appellent que des helpers déclarés dans tab5_custom.h. Les
 *                helpers partagés entre unités sont déclarés dans tab5_internal.h.
 * @memory_constraint Pas de std::string dans une boucle de parsing : découper un char* en place.
 *       `split_fields()` (tab5_core.h) garde les champs vides ; `strtok_r` les fusionne.
 */
#include "tab5_custom.h"
#include "tab5_internal.h"
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <ctime>
#include <cstring>
#include <vector>
#include <map>

// --- Payload refusé (lot L5, tab5_internal.h) ---
void payload_refuse(const char* tag, const char* raison, size_t taille) {
    ESP_LOGW(tag, "Payload refusé (%s) : %u octets", raison, static_cast<unsigned>(taille));
}

bool payload_trop_long(const char* tag, size_t taille, size_t max) {
    if (taille <= max) return false;
    ESP_LOGW(tag, "Payload refusé (trop long) : %u octets, plafond %u", static_cast<unsigned>(taille),
             static_cast<unsigned>(max));
    return true;
}

// =============================================================================
// UTF-8 : normalisation des textes HA (Latin-1 / mojibake) avant affichage LVGL
// =============================================================================

static bool is_valid_utf8(const std::string& s) {
    for (size_t i = 0; i < s.size(); ) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80) {
            i++;
            continue;
        }
        size_t need = 0;
        if ((c & 0xE0) == 0xC0) need = 1;
        else if ((c & 0xF0) == 0xE0) need = 2;
        else if ((c & 0xF8) == 0xF0) need = 3;
        else return false;
        if (i + need >= s.size()) return false;
        for (size_t j = 1; j <= need; j++) {
            if ((static_cast<unsigned char>(s[i + j]) & 0xC0) != 0x80) return false;
        }
        i += need + 1;
    }
    return true;
}

static std::string latin1_to_utf8(const std::string& in) {
    std::string out;
    out.reserve(in.size() * 2);
    for (unsigned char c : in) {
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else {
            uint32_t cp = c;
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }
    return out;
}

static std::string fix_utf8_mojibake(std::string s) {
    struct Rep { const char* bad; const char* good; };
    static const Rep reps[] = {
        {"\xC3\x83\xC2\xA9", "\xC3\xA9"},  // Ã© -> é
        {"\xC3\x83\xC2\xA8", "\xC3\xA8"},  // Ã¨ -> è
        {"\xC3\x83\xC2\xAA", "\xC3\xAA"},  // Ãª -> ê
        {"\xC3\x83\xC2\xA0", "\xC3\xA0"},  // Ã  -> à
        {"\xC3\x83\xC2\xB4", "\xC3\xB4"},  // Ã´ -> ô
        {"\xC3\x83\xC2\xBB", "\xC3\xBB"},  // Ã» -> û
        {"\xC3\x83\xC2\xA7", "\xC3\xA7"},  // Ã§ -> ç
    };
    for (const auto& r : reps) {
        const size_t bad_len = strlen(r.bad);
        const size_t good_len = strlen(r.good);
        size_t pos = 0;
        while ((pos = s.find(r.bad, pos)) != std::string::npos) {
            s.replace(pos, bad_len, r.good);
            pos += good_len;
        }
    }
    return s;
}

std::string normalize_text_utf8(const std::string& in) {
    if (in.empty()) return in;
    std::string t = is_valid_utf8(in) ? in : latin1_to_utf8(in);
    return fix_utf8_mojibake(std::move(t));
}

const char* vigilance_alert_banner_utf8(const std::string& couleur) {
    if (couleur.find("Rouge") != std::string::npos) {
        return tr("Alerte M\xC3\xA9t\xC3\xA9o Rouge en cours ! Restez prudent.");
    }
    if (couleur.find("Orange") != std::string::npos) {
        return tr("Alerte M\xC3\xA9t\xC3\xA9o Orange en cours ! Restez prudent.");
    }
    return nullptr;
}

static std::vector<std::string> tab5_dismiss_split_ids(const std::string& store) {
    std::vector<std::string> out;
    char buf[513];
    strncpy(buf, store.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* saveptr = nullptr;
    char* tok = strtok_r(buf, "|", &saveptr);
    while (tok != nullptr) {
        const std::string id = trim_ws(tok);
        if (!id.empty()) out.push_back(id);
        tok = strtok_r(nullptr, "|", &saveptr);
    }
    return out;
}

void tab5_dismiss_local_add(std::string& store, const std::string& id) {
    if (id.empty()) return;
    if (tab5_dismiss_local_has(store, id)) return;
    if (!store.empty()) store += "|";
    store += id;
    if (store.length() > 512) store = store.substr(0, 512);
}

bool tab5_dismiss_local_has(const std::string& store, const std::string& id) {
    if (id.empty()) return false;
    for (const auto& cur : tab5_dismiss_split_ids(store)) {
        if (cur == id) return true;
    }
    return false;
}

void tab5_dismiss_local_prune(std::string& store, const std::vector<std::string>& ids_seen) {
    auto ids = tab5_dismiss_split_ids(store);
    store.clear();
    for (const auto& id : ids) {
        bool seen = false;
        for (const auto& s : ids_seen) {
            if (s == id) { seen = true; break; }
        }
        if (seen) tab5_dismiss_local_add(store, id);
    }
}


// cal_is_early_shift, dates locales (local_day_from_offset, local_day_number_today),
// jours/mois en toutes lettres et titres de jour : tab5_core.cpp (logique pure).

// Recolor LVGL : vrai seulement si markup #RRGGBB (évite faux positifs sur '#' isolé).
// Comme avant (lot L10, 08/10/2026) : le '#' suivi de six chiffres hexadécimaux et d'au
// moins un caractère encore ; lu en place, sans copie.
bool has_lvgl_recolor_markup(const char* t) {
    if (t == nullptr) return false;
    for (const char* p = strchr(t, '#'); p != nullptr; p = strchr(p + 1, '#')) {
        bool hex6 = true;
        for (int j = 1; j <= 6; ++j) {
            const char c = p[j];  // le zéro final arrête la boucle avant de dépasser
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                hex6 = false;
                break;
            }
        }
        if (hex6 && p[7] != '\0') return true;
    }
    return false;
}

// PERF-6 (audit du 07/10/2026) : ni copie en std::string, ni écriture à texte égal.
// lv_label_set_recolor() revient déjà tout seul si le drapeau ne change pas (lv_label.c).
void set_label_text_utf8(lv_obj_t* label, const char* text) {
    if (!label || !text) return;
    lv_label_set_recolor(label, has_lvgl_recolor_markup(text));
    ui_text(label, text);
}

// clock_month_short_utf8() : tab5_core.cpp, avec tous les autres noms de jours et
// de mois (lot 8d).

// =============================================================================
// Traduction des textes posés par le YAML (lot 4, 27/09/2026)
// =============================================================================
// Les labels du YAML gardent leur texte français (`text: "Calendrier"`) : c'est la
// clé de tab5_i18n. Un seul passage en fin de setup (on_boot -100, AVANT la
// première image, autorisé par Axel le 27/09/2026) les remplace par leur
// traduction. En français, rien : retour immédiat. Les textes posés ensuite par le
// C++ passent eux-mêmes par tr().

static void i18n_apply_tree(lv_obj_t* obj) {
    if (obj == nullptr) return;
    if (lv_obj_check_type(obj, &lv_label_class)) {
        const char* actuel = lv_label_get_text(obj);
        const char* traduit = tr(actuel);
        if (traduit != actuel) lv_label_set_text(obj, traduit);
    }
    const uint32_t n = lv_obj_get_child_count(obj);
    for (uint32_t i = 0; i < n; i++) {
        i18n_apply_tree(lv_obj_get_child(obj, i));
    }
}

void i18n_apply_boot(std::initializer_list<lv_obj_t*> racines) {
    if (i18n_language() == 0) return;
    for (lv_obj_t* r : racines) i18n_apply_tree(r);
}
