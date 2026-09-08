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
 * @memory_constraint Éviter std::string dans les boucles de parsing ; char* + strtok_r.
 */
#include "tab5_custom.h"
#include "tab5_internal.h"
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <ctime>
#include <cstring>
#include <vector>
#include <map>

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
        return "Alerte M\xC3\xA9t\xC3\xA9o Rouge en cours ! Restez prudent.";
    }
    if (couleur.find("Orange") != std::string::npos) {
        return "Alerte M\xC3\xA9t\xC3\xA9o Orange en cours ! Restez prudent.";
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
        std::string id = tok;
        const char* ws = " \t\r\n";
        size_t deb = id.find_first_not_of(ws);
        if (deb != std::string::npos) {
            id = id.substr(deb, id.find_last_not_of(ws) - deb + 1);
            if (!id.empty()) out.push_back(id);
        }
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


bool cal_is_early_shift(const std::string& heures_hhmm_hhmm) {
    // Convention unique : embauche "tôt" si heure de début < 9 (09:00 n'est PAS tôt).
    if (heures_hhmm_hhmm.size() < 2) return false;
    return atoi(heures_hhmm_hhmm.substr(0, 2).c_str()) < 9;
}

// Date locale a J+jour_offset via l'heure systeme SNTP (timezone Europe/Paris
// configuree dans tab5-sensors-diagnostics.yaml). Normalisation par mktime() a midi
// plutot qu'une addition de 86400 s : immunise contre les bascules heure d'ete/hiver
// (une journee de 23 h ou 25 h decalerait la date d'un jour pres de minuit).
bool local_day_from_offset(int jour_offset, struct tm& out) {
    time_t raw = time(nullptr);
    if (raw <= 0 || jour_offset < 0 || jour_offset >= 15) return false;
    if (localtime_r(&raw, &out) == nullptr) return false;
    out.tm_mday += jour_offset;
    out.tm_hour = 12;
    out.tm_min = 0;
    out.tm_sec = 0;
    out.tm_isdst = -1;
    return mktime(&out) != static_cast<time_t>(-1);
}

// Titre court "Lun 16" pour les pages journalieres 2 et 3 (page_index 1/2).
std::string format_short_day_label(int jour_offset) {
    static const char* days[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
    struct tm t;
    if (!local_day_from_offset(jour_offset, t)) return "";
    char buf[12];
    snprintf(buf, sizeof(buf), "%s %02d", days[t.tm_wday], t.tm_mday);
    return std::string(buf);
}

// Jours et mois en toutes lettres pour les titres de la carte centrale.
// UTF-8 explicite (\xC3\xA9 = e, \xC3\xBB = u circonflexe) — jamais du Latin-1.
// Minuscules : en francais, jours et mois ne prennent pas de majuscule hors debut
// de phrase (le titre commence par "Du ...", qui porte la majuscule).
const char* fr_day_long_utf8(int wday) {
    static const char* days[] = {"dimanche", "lundi", "mardi", "mercredi",
                                 "jeudi", "vendredi", "samedi"};
    if (wday < 0 || wday > 6) return "";
    return days[wday];
}

const char* fr_month_long_utf8(int mois_1_12) {
    static const char* months[] = {
        "janvier", "f\xC3\xA9vrier", "mars", "avril", "mai", "juin", "juillet",
        "ao\xC3\xBBt", "septembre", "octobre", "novembre", "d\xC3\xA9" "cembre"
    };
    if (mois_1_12 < 1 || mois_1_12 > 12) return "";
    return months[mois_1_12 - 1];
}

// "mercredi 5 aout" a J+jour_offset — "1er" pour le premier du mois (le seul
// quantieme ordinal en francais, les autres restent cardinaux : 2, 3, 4...).
std::string format_long_day_label(int jour_offset) {
    struct tm t;
    if (!local_day_from_offset(jour_offset, t)) return "";
    char buf[48];
    if (t.tm_mday == 1) {
        snprintf(buf, sizeof(buf), "%s 1er %s",
                 fr_day_long_utf8(t.tm_wday), fr_month_long_utf8(t.tm_mon + 1));
    } else {
        snprintf(buf, sizeof(buf), "%s %d %s",
                 fr_day_long_utf8(t.tm_wday), t.tm_mday, fr_month_long_utf8(t.tm_mon + 1));
    }
    return std::string(buf);
}


// Recolor LVGL : vrai seulement si markup #RRGGBB (évite faux positifs sur '#' isolé).
bool has_lvgl_recolor_markup(const std::string& t) {
    for (size_t i = 0; i + 7 < t.size(); ++i) {
        if (t[i] != '#') continue;
        bool hex6 = true;
        for (int j = 1; j <= 6; ++j) {
            char c = t[i + static_cast<size_t>(j)];
            if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) {
                hex6 = false;
                break;
            }
        }
        if (hex6) return true;
    }
    return false;
}

void set_label_text_utf8(lv_obj_t* label, const char* text) {
    if (!label || !text) return;
    std::string t(text);
    lv_label_set_recolor(label, has_lvgl_recolor_markup(t));
    lv_label_set_text(label, text);
}

const char* clock_month_short_utf8(int month) {
    static const char* months[] = {
        "Janv", "F\xC3\xA9vr", "Mars", "Avr", "Mai", "Juin", "Juil",
        "Ao\xC3\xBBt", "Sept", "Oct", "Nov", "D\xC3\xA9" "c"
    };
    if (month < 1 || month > 12) return "";
    return months[month - 1];
}
