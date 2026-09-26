/**
 * [AI-CONTEXT]
 * @file tab5_i18n.cpp
 * @role Recherche des traductions (voir tab5_i18n.h). Tables générées dans
 *       tab5_i18n_data.h : clés triées par octets (contexte puis texte), une table
 *       par langue alignée sur les clés, nullptr = pas de traduction.
 * @architecture_constraint PUR (compilé aussi sur PC par tools/test_alarm_clock.cpp).
 */
#include "tab5_i18n.h"
#include "tab5_i18n_data.h"
#include <cstring>

static uint8_t s_lang = 0;

uint8_t i18n_language() { return s_lang; }

void i18n_set_language(uint8_t lang) { s_lang = (lang < kI18nLangCount) ? lang : 0; }

size_t i18n_language_count() { return kI18nLangCount; }

const char* i18n_language_name(size_t lang) {
    return (lang < kI18nLangCount) ? kI18nLangNames[lang] : "";
}

const char* i18n_language_code(size_t lang) {
    return (lang < kI18nLangCount) ? kI18nLangCodes[lang] : "";
}

const char* tr_ctx(const char* ctx, const char* fr) {
    if (s_lang == 0 || fr == nullptr || kI18nKeyCount == 0) return fr;
    if (ctx == nullptr) ctx = "";
    const char* const* table = kI18nTables[s_lang];
    int lo = 0, hi = static_cast<int>(kI18nKeyCount) - 1;
    while (lo <= hi) {
        const int mid = (lo + hi) / 2;
        int c = strcmp(ctx, kI18nCtx[mid]);
        if (c == 0) c = strcmp(fr, kI18nKeys[mid]);
        if (c == 0) return (table[mid] != nullptr) ? table[mid] : fr;
        if (c < 0) hi = mid - 1;
        else lo = mid + 1;
    }
    return fr;
}

const char* tr(const char* fr) { return tr_ctx("", fr); }

std::string tr_fill(const char* pattern_fr,
                    std::initializer_list<std::pair<const char*, std::string>> vars) {
    const std::string pattern = tr(pattern_fr);
    std::string out;
    out.reserve(pattern.size() + 32);
    size_t i = 0;
    while (i < pattern.size()) {
        if (pattern[i] == '{') {
            const size_t fin = pattern.find('}', i);
            if (fin != std::string::npos) {
                const std::string nom = pattern.substr(i + 1, fin - i - 1);
                bool trouve = false;
                for (const auto& v : vars) {
                    if (nom == v.first) {
                        out += v.second;
                        trouve = true;
                        break;
                    }
                }
                if (trouve) {
                    i = fin + 1;
                    continue;
                }
            }
        }
        out += pattern[i++];
    }
    return out;
}
