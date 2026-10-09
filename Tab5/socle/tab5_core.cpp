/**
 * [AI-CONTEXT]
 * @file tab5_core.cpp
 * @role Implémentation de tab5_core.h : logique pure, compilée à l'identique dans
 *       le firmware et dans les tests hôte (tools/test_alarm_clock.cpp).
 *       Déplacé tel quel de tab5_text.cpp le 25/09/2026 (audit, lot 8b) ; seule
 *       différence, `time(nullptr)` passe par `tab5_time_source`.
 */
#include "tab5_core.h"
#include "tab5_i18n.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

time_t (*tab5_time_source)(time_t*) = time;

bool cal_is_early_shift(const std::string& heures_hhmm_hhmm) {
    // Convention unique : embauche "tôt" si heure de début < 9 (09:00 n'est PAS tôt).
    const int debut = hhmm_minutes(heures_hhmm_hhmm);
    return debut >= 0 && debut < 9 * 60;
}

// ─── Dates et heures (lot L5 de l'audit du 07/10/2026) ───

bool tab5_heure_valide(time_t t) { return t >= kHeureValideMin; }

// Algorithme days_from_civil de H. Hinnant.
int32_t jour_civil(int annee, int mois, int jour) {
    annee -= (mois <= 2) ? 1 : 0;
    const int era = (annee >= 0 ? annee : annee - 399) / 400;
    const int yoe = annee - era * 400;                                          // [0, 399]
    const int doy = (153 * (mois > 2 ? mois - 3 : mois + 9) + 2) / 5 + jour - 1;  // [0, 365]
    const int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;                      // [0, 146096]
    return era * 146097 + doe - 719468;
}

bool annee_bissextile(int annee) { return (annee % 4 == 0 && annee % 100 != 0) || annee % 400 == 0; }

int jours_du_mois(int annee, int mois) {
    static constexpr int kJours[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (mois < 1 || mois > 12) return 31;
    return (mois == 2 && annee_bissextile(annee)) ? 29 : kJours[mois - 1];
}

int hhmm_minutes(const char* s) {
    // Évaluation dans l'ordre : on ne lit jamais au-delà du zéro final d'un texte court.
    auto chiffre = [](char c) { return c >= '0' && c <= '9'; };
    if (s == nullptr || !chiffre(s[0]) || !chiffre(s[1]) || s[2] != ':' || !chiffre(s[3]) || !chiffre(s[4]))
        return -1;
    const int h = (s[0] - '0') * 10 + (s[1] - '0');
    const int m = (s[3] - '0') * 10 + (s[4] - '0');
    return (h > 23 || m > 59) ? -1 : h * 60 + m;
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

// Numéro du jour civil (jour_civil) de la date LOCALE : aucune dépendance aux jours de
// 23 h/25 h — contrairement à epoch / 86400.
int32_t local_day_number_today() {
    const time_t raw = tab5_time_source(nullptr);
    struct tm t;
    // Avant la synchro SNTP l'horloge part de 1970 : pas encore la vraie date.
    if (!tab5_heure_valide(raw) || localtime_r(&raw, &t) == nullptr) return -1;
    return jour_civil(t.tm_year + 1900, t.tm_mon + 1, t.tm_mday);
}

// Case de cal_jours_data[] qui correspond à J+offset (offset compté depuis
// AUJOURD'HUI), -1 si ce jour n'est pas couvert. Les données sont datées par
// cal_jours_anchor_day (jour local du dernier push) : HA muet depuis hier soir,
// aujourd'hui est la case 1 et non la case 0 — sans ce recalage, le réveil
// appliquait le planning de la veille (embauche ratée ou sonnerie un jour de repos,
// audit du 25/09/2026, §2.2) et le planning de la carte centrale l'affichait sous
// « Auj. ». Partagée par alarm_clock.cpp et tab5_services.cpp. Au-delà de 14 jours sans push, plus rien n'est couvert.
int cal_index_for_offset(int offset) {
  if (offset < 0 || cal_jours_anchor_day < 0) return -1;
  const int32_t today = local_day_number_today();
  if (today < 0) return -1;
  const int32_t idx = static_cast<int32_t>(offset) + (today - cal_jours_anchor_day);
  return (idx >= 0 && idx < 15) ? static_cast<int>(idx) : -1;
}

// ─── Noms de jours et de mois : les SEULES tables du projet (lot 8d, 25/09/2026) ───
// Avant, sept tables recopiées dans cinq fichiers (horloge, services, calendrier…)
// pouvaient diverger d'orthographe. Chaque appelant garde son format (« Dim »,
// « Dim. », « Dimanche », « dimanche ») en partant d'ici.

// [AI-WARNING] fr_day_short_utf8() et clock_month_short_utf8() sont affichés sous
// l'horloge (lbl_date, roboto_45_b) : la police doit contenir chaque caractère de
// ces libellés, sinon la lettre sort vide. Règle 6 de tools/check_tab5_code_rules.py.
const char* fr_day_short_utf8(int wday) {
    static const char* days[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
    if (wday < 0 || wday > 6) return "";
    return days[wday];
}

const char* clock_month_short_utf8(int month) {
    static const char* months[] = {
        "Janv", "F\xC3\xA9vr", "Mars", "Avr", "Mai", "Juin", "Juil",
        "Ao\xC3\xBBt", "Sept", "Oct", "Nov", "D\xC3\xA9" "c"
    };
    if (month < 1 || month > 12) return "";
    return months[month - 1];
}

std::string fr_capitalized(const char* s) {
    std::string r = (s != nullptr) ? s : "";
    // Tous les jours et mois commencent par une lettre ASCII (« août », « décembre ») :
    // mettre en majuscule le premier OCTET suffit, sans table Unicode.
    if (!r.empty() && r[0] >= 'a' && r[0] <= 'z') r[0] = static_cast<char>(r[0] - 'a' + 'A');
    return r;
}

// Libellés affichés = libellés français passés par tr() (clés de Tab5/lang/*.yaml).
const char* day_short_utf8(int wday) { return tr(fr_day_short_utf8(wday)); }
const char* month_short_utf8(int month) { return tr(clock_month_short_utf8(month)); }
const char* day_long_utf8(int wday) { return tr(fr_day_long_utf8(wday)); }
const char* month_long_utf8(int mois_1_12) { return tr(fr_month_long_utf8(mois_1_12)); }

// « Auj » (sans point) est le seul nom que HA envoie hors de fr_day_short_utf8().
const char* ha_day_name(const std::string& nom) {
    if (nom == "Auj") return tr("Auj");
    return tr(nom.c_str());
}

// Titre court "Lun 16" pour les pages journalieres 2 et 3 (page_index 1/2).
std::string format_short_day_label(int jour_offset) {
    struct tm t;
    if (!local_day_from_offset(jour_offset, t)) return "";
    char buf[24];
    snprintf(buf, sizeof(buf), "%s %02d", day_short_utf8(t.tm_wday), t.tm_mday);
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
// L'ordre des mots est un modele traduisible : « {jour}, {mois} {quantieme} » en
// anglais, et « 1er » se traduit à part (« 1 » en anglais).
std::string format_long_day_label(int jour_offset) {
    struct tm t;
    if (!local_day_from_offset(jour_offset, t)) return "";
    const std::string quantieme = (t.tm_mday == 1) ? std::string(tr("1er")) : std::to_string(t.tm_mday);
    return tr_fill("{jour} {quantieme} {mois}", {{"jour", day_long_utf8(t.tm_wday)},
                                                {"quantieme", quantieme},
                                                {"mois", month_long_utf8(t.tm_mon + 1)}});
}

// ─── Découpe de texte : reprend à l'identique les copies qu'elle remplace
// (rognage de tab5_central.cpp, boucles strchr des payloads bulk). ───

std::string trim_ws(const std::string& s) {
    const char* ws = " \t\r\n";
    const size_t deb = s.find_first_not_of(ws);
    if (deb == std::string::npos) return "";
    return s.substr(deb, s.find_last_not_of(ws) - deb + 1);
}

int split_fields(char* s, char sep, char* out[], int max) {
    int n = 0;
    char* p = s;
    while (n < max) {
        out[n++] = p;
        char* next = strchr(p, sep);
        if (next == nullptr) break;
        *next = '\0';
        p = next + 1;
    }
    return n;
}

// ─── Nombres reçus de Home Assistant (lot A de l'audit du 30/09/2026) ───

float tab5_fini_ou_nan(float v) { return std::isfinite(v) ? v : NAN; }

int tab5_float_vers_int(float v, int bas, int haut, int defaut) {
    if (!std::isfinite(v)) return defaut;
    if (v <= static_cast<float>(bas)) return bas;
    if (v >= static_cast<float>(haut)) return haut;
    return static_cast<int>(v);
}

int lum_pct(float v) {
    if (!std::isfinite(v)) return -1;
    const float b = v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v);
    const int pct = static_cast<int>(std::lround(b * 100.0f / 255.0f));
    return pct < 1 ? 1 : (pct > 100 ? 100 : pct);
}

// ─── Console système : « Batterie » et « Charge CPU » (discussion #278, 06/10/2026) ───

void batterie_texte_console(char* buf, size_t n, bool montee, PresenceBatterie presence,
                            float niveau, float tension, float puissance_w) {
    if (buf == nullptr || n == 0) return;
    if (!montee) {
        snprintf(buf, n, "%s", tr("Non montée"));
        return;
    }
    if (presence == PresenceBatterie::ABSENTE) {
        snprintf(buf, n, "%s", tr("Sur USB"));
        return;
    }
    const bool a_niveau = presence == PresenceBatterie::PRESENTE && std::isfinite(niveau);
    const bool a_tension = presence == PresenceBatterie::PRESENTE && std::isfinite(tension);
    if (presence == PresenceBatterie::PRESENTE && std::isfinite(puissance_w)) {
        if (a_niveau) snprintf(buf, n, "%.0f%% \xC2\xB7 %.1f W", niveau, puissance_w);
        else snprintf(buf, n, "-- \xC2\xB7 %.1f W", puissance_w);
    } else if (!a_niveau && !a_tension) {
        snprintf(buf, n, "--");
    } else if (!a_tension) {
        snprintf(buf, n, "%.0f%%", niveau);
    } else if (!a_niveau) {
        snprintf(buf, n, "-- \xC2\xB7 %.2f V", tension);
    } else {
        snprintf(buf, n, "%.0f%% \xC2\xB7 %.2f V", niveau, tension);
    }
}

// ─── Réglages en quatre pages (08/10/2026) ───

int reglages_page_voisine(int page, int nb, bool gauche) {
    if (nb <= 0) return 0;
    if (page < 0 || page >= nb) return 0;
    return gauche ? (page + 1) % nb : (page + nb - 1) % nb;
}

// ─── Rouleaux du popup Réveil (09/10/2026) ───

namespace {
// Nombre de pas de bas à haut (au moins un : bas lui-même) ; pas ≤ 0 compte comme 1.
int rouleau_pas_nombre(int bas, int haut, int pas) { return haut < bas ? 1 : (haut - bas) / pas + 1; }
// Place de `extra` : juste après le pas qui le précède.
int rouleau_place_extra(int bas, int pas, int extra) { return (extra - bas) / pas + 1; }
}  // namespace

int rouleau_extra(int bas, int haut, int pas, int valeur) {
    if (pas <= 0) pas = 1;
    if (valeur < bas || valeur > haut) return -1;
    return (valeur - bas) % pas != 0 ? valeur : -1;
}

int rouleau_nombre(int bas, int haut, int pas, int extra) {
    if (pas <= 0) pas = 1;
    return rouleau_pas_nombre(bas, haut, pas) + (rouleau_extra(bas, haut, pas, extra) >= 0 ? 1 : 0);
}

int rouleau_valeur(int bas, int haut, int pas, int extra, int index) {
    if (pas <= 0) pas = 1;
    if (index < 0 || index >= rouleau_nombre(bas, haut, pas, extra)) return -1;
    if (rouleau_extra(bas, haut, pas, extra) >= 0) {
        const int k = rouleau_place_extra(bas, pas, extra);
        if (index == k) return extra;
        if (index > k) index--;
    }
    return bas + index * pas;
}

int rouleau_index(int bas, int haut, int pas, int extra, int valeur) {
    if (pas <= 0) pas = 1;
    if (haut < bas) haut = bas;
    const bool avec_extra = rouleau_extra(bas, haut, pas, extra) >= 0;
    if (avec_extra && valeur == extra) return rouleau_place_extra(bas, pas, extra);
    if (valeur < bas) valeur = bas;
    if (valeur > haut) valeur = haut;
    int i = (valeur - bas + pas / 2) / pas;
    const int n = rouleau_pas_nombre(bas, haut, pas);
    if (i >= n) i = n - 1;
    if (avec_extra && i >= rouleau_place_extra(bas, pas, extra)) i++;
    return i;
}

const char* batterie_etat_texte(PresenceBatterie presence, bool en_charge, bool sur_batterie) {
    switch (presence) {
        case PresenceBatterie::INCONNUE: return tr("Mesure en cours");
        case PresenceBatterie::ABSENTE: return tr("Pas de batterie détectée");
        default: break;
    }
    if (sur_batterie) return tr("Sur batterie");
    if (en_charge) return tr("En charge");
    return tr("Sur USB");
}

void batterie_valeur_texte(char* buf, size_t n, PresenceBatterie presence, float valeur, MesureBatterie mesure) {
    if (buf == nullptr || n == 0) return;
    if (presence != PresenceBatterie::PRESENTE || !std::isfinite(valeur)) {
        snprintf(buf, n, "--");
        return;
    }
    switch (mesure) {
        case MesureBatterie::NIVEAU: snprintf(buf, n, "%.0f %%", valeur); break;
        case MesureBatterie::TENSION: snprintf(buf, n, "%.2f V", valeur); break;
        default: snprintf(buf, n, "%.1f W", valeur); break;
    }
}

int cpu_charge_pct(uint32_t inactif_avant, uint32_t inactif_apres, uint32_t duree_us) {
    if (duree_us == 0) return -1;
    const uint32_t inactif = inactif_apres - inactif_avant;  // non signé : rebouclage compris
    if (inactif >= duree_us) return 0;
    const uint64_t occupe = static_cast<uint64_t>(duree_us - inactif);
    return static_cast<int>((occupe * 100u + duree_us / 2u) / duree_us);
}
