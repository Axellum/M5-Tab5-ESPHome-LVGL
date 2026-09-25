/**
 * [AI-CONTEXT]
 * @file tab5_core.cpp
 * @role Implémentation de tab5_core.h : logique pure, compilée à l'identique dans
 *       le firmware et dans les tests hôte (tools/test_alarm_clock.cpp).
 *       Déplacé tel quel de tab5_text.cpp le 25/09/2026 (audit, lot 8b) ; seule
 *       différence, `time(nullptr)` passe par `tab5_time_source`.
 */
#include "tab5_core.h"

#include <cstdio>
#include <cstdlib>

time_t (*tab5_time_source)(time_t*) = time;

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
    time_t raw = tab5_time_source(nullptr);
    if (raw <= 0 || jour_offset < 0 || jour_offset >= 15) return false;
    if (localtime_r(&raw, &out) == nullptr) return false;
    out.tm_mday += jour_offset;
    out.tm_hour = 12;
    out.tm_min = 0;
    out.tm_sec = 0;
    out.tm_isdst = -1;
    return mktime(&out) != static_cast<time_t>(-1);
}

// Jours depuis le 01/01/1970 pour une date civile (algorithme days_from_civil de
// H. Hinnant) : arithmétique entière sur (année, mois, jour), aucune dépendance au
// fuseau ni aux jours de 23 h/25 h — contrairement à epoch / 86400.
static int32_t days_from_civil(int y, int m, int d) {
    y -= (m <= 2) ? 1 : 0;
    const int era = (y >= 0 ? y : y - 399) / 400;
    const int yoe = y - era * 400;                                    // [0, 399]
    const int doy = (153 * (m > 2 ? m - 3 : m + 9) + 2) / 5 + d - 1;  // [0, 365]
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;            // [0, 146096]
    return era * 146097 + doe - 719468;
}

int32_t local_day_number_today() {
    const time_t raw = tab5_time_source(nullptr);
    struct tm t;
    if (raw <= 0 || localtime_r(&raw, &t) == nullptr) return -1;
    // Avant la synchro SNTP l'horloge part de 1970 : une date antérieure à 2020 ne
    // peut pas être la vraie (même seuil qu'ESPTime::is_valid, année ≥ 2019).
    if (t.tm_year + 1900 < 2020) return -1;
    return days_from_civil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
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
