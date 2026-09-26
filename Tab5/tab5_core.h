/**
 * [AI-CONTEXT]
 * @file tab5_core.h
 * @role Logique PURE partagée par le HMI et le réveil : données calendrier /
 *       prévisions poussées par HA, dates locales (J+n, numéro de jour civil),
 *       jours et mois en toutes lettres.
 * @architecture_constraint Aucune dépendance ESPHome ni LVGL : ce fichier et
 *       tab5_core.cpp se compilent sur PC (tools/test_alarm_clock.cpp, g++ en CI).
 *       C'est ce qui permet de tester le moteur du réveil sans l'appareil — audit
 *       du 25/09/2026, lot 8b. `tab5_custom.h` l'inclut : le contrat YAML ne change pas.
 * @ai_instruction L'heure courante passe par `tab5_time_source` (par défaut
 *       `time`), jamais par `time(nullptr)` directement : c'est le seul moyen pour
 *       un test de simuler « aujourd'hui », un changement d'heure ou une nuit sans HA.
 */
#pragma once
#include <cstdint>
#include <ctime>
#include <string>

// Source de l'heure courante des fonctions de dates ci-dessous. Le firmware garde
// `time` (horloge SNTP) ; un test la remplace pour fixer « maintenant ».
extern time_t (*tab5_time_source)(time_t*);

struct DayForecastData {
    std::string nom_jour;
    std::string condition;
    float tmin = 0.0f;
    float tmax = 0.0f;
    bool est_repos = false;
    bool est_dimanche = false;
    bool est_passe = false;
    std::string heures_ouverture;
};

struct HourForecastData {
    std::string heure_texte;
    std::string condition;
    float temp = 0.0f;
    float pluvio = 0.0f;
};

extern DayForecastData cal_jours_data[15];
extern HourForecastData cal_heures_data[15];

// Jour local (numéro de jour civil, cf. local_day_number_today) auquel correspond
// cal_jours_data[0], posé par parse_and_update_jours_bulk() au moment du push ;
// -1 tant qu'aucun push n'a été reçu avec l'heure synchronisée. Sans lui, la case 0
// était « aujourd'hui » quel que soit l'âge des données : HA muet depuis minuit, le
// réveil appliquait le planning de la veille (audit du 25/09/2026, §2.2).
extern int32_t cal_jours_anchor_day;

// Embauche "tôt" = heure de début < 9h (même seuil partout : tuiles, popup, bandeau).
bool cal_is_early_shift(const std::string& heures_hhmm_hhmm);

// Date locale à J+jour_offset (0-14) via l'heure système SNTP, normalisée à midi
// par mktime() : immunisé contre les bascules heure d'été/hiver (une journée de
// 23 h ou 25 h décalerait la date d'un jour près de minuit). Renvoie false si
// l'heure n'est pas encore synchronisée ou si l'offset est hors bornes.
// Partagé avec alarm_clock.cpp (calcul de la prochaine sonnerie) — c'était un
// `static` de tab5_custom.cpp jusqu'au 05/08/2026 : le réveil DOIT utiliser
// exactement la même arithmétique de dates que les tuiles météo, sinon les deux
// divergent d'un jour deux fois par an.
bool local_day_from_offset(int jour_offset, struct tm& out);

// Numéro du jour civil local d'aujourd'hui (jours depuis le 01/01/1970 dans le
// calendrier local, pas une division d'epoch : insensible aux jours de 23 h/25 h).
// -1 si l'heure SNTP n'est pas encore synchronisée. Deux valeurs se soustraient
// pour obtenir un écart en jours (cf. cal_jours_anchor_day).
int32_t local_day_number_today();

// Case de cal_jours_data[] qui correspond à J+offset (offset compté depuis AUJOURD'HUI),
// recalée par cal_jours_anchor_day ; -1 si ce jour n'est pas couvert, si aucun lot daté
// n'a été reçu ou si l'heure n'est pas synchronisée. Ne JAMAIS indexer cal_jours_data[]
// par un décalage depuis aujourd'hui sans passer par elle.
int cal_index_for_offset(int offset);

// Jours et mois en toutes lettres, UTF-8, minuscules (en français ils ne
// prennent pas de majuscule hors début de phrase). wday : 0 = dimanche.
// Partagés avec alarm_clock.cpp (« demain, mercredi 6 août ») — une seule table
// pour tout le projet, sinon deux orthographes finissent par diverger.
const char* fr_day_long_utf8(int wday);
const char* fr_month_long_utf8(int mois_1_12);
// « Dim » … « Sam » (wday : 0 = dimanche) et « Janv » … « Déc » (1-12) : la date sous
// l'horloge. "" hors bornes. Glyphes couverts par roboto_45 (règle 6).
const char* fr_day_short_utf8(int wday);
const char* clock_month_short_utf8(int month);
// Première lettre en majuscule : « dimanche » → « Dimanche » (début de libellé).
std::string fr_capitalized(const char* s);

// Titres de jour des pages de prévisions : « Lun 16 » et « mercredi 5 août »
// (« 1er » pour le premier du mois). "" si l'heure n'est pas synchronisée.
std::string format_short_day_label(int jour_offset);
std::string format_long_day_label(int jour_offset);

// ─── Découpe de texte (audit du 26/09/2026, lot 7.1) ───
// Copie de `s` sans les espaces, tabulations, CR et LF de début et de fin ; ""
// s'il n'y a que cela.
std::string trim_ws(const std::string& s);
// Découpe EN PLACE `s` sur `sep` : chaque séparateur rencontré devient '\0' et
// out[0..n-1] pointent sur les champs (vides compris, contrairement à strtok_r).
// Au plus `max` champs : le séparateur qui suit le dernier est lui aussi coupé,
// le reste de la chaîne est ignoré. Renvoie n (0 si max <= 0).
int split_fields(char* s, char sep, char* out[], int max);
