/**
 * [AI-CONTEXT]
 * @file tab5_core.h
 * @role Logique PURE partagée par le HMI et le réveil : données calendrier /
 *       prévisions poussées par HA, dates locales (J+n, numéro de jour civil),
 *       jours et mois en toutes lettres. Et la présence de la batterie
 *       (PresenceBatterie, décidée par tab5_batterie.h depuis le 08/10/2026), le texte
 *       de la ligne « Batterie » et le calcul de la « Charge CPU » de la console système
 *       (06/10/2026).
 * @architecture_constraint Aucune dépendance ESPHome ni LVGL : ce fichier et
 *       tab5_core.cpp se compilent sur PC (tools/test_alarm_clock.cpp, g++ en CI).
 *       C'est ce qui permet de tester le moteur du réveil sans l'appareil — audit
 *       du 25/09/2026, lot 8b. `tab5_custom.h` l'inclut : le contrat YAML ne change pas.
 * @ai_instruction L'heure courante passe par `tab5_time_source` (par défaut
 *       `time`), jamais par `time(nullptr)` directement : c'est le seul moyen pour
 *       un test de simuler « aujourd'hui », un changement d'heure ou une nuit sans HA.
 *       « L'heure est-elle réglée ? » : tab5_heure_valide(), un seul seuil. Dates
 *       civiles (jour_civil, jours_du_mois) et heures « HH:MM » (hhmm_minutes) : ici,
 *       jamais une copie locale (audit du 07/10/2026, lot L5).
 */
#pragma once
#include <cstddef>
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
// Faux si le début n'est pas une heure « HH:MM » lisible.
bool cal_is_early_shift(const std::string& heures_hhmm_hhmm);

// ─── Dates et heures : les seules copies du projet (audit du 07/10/2026, lot L5) ───
// Avant la synchro SNTP (et sans l'horloge RX8130), l'heure part de 1970 : une heure
// antérieure au 01/01/2020 00:00 UTC n'est pas la vraie. Le seul seuil du projet
// (il y en avait trois : année 2020, 1577836800, 1600000000).
constexpr time_t kHeureValideMin = 1577836800;
bool tab5_heure_valide(time_t t);
// Jours depuis le 01/01/1970 d'une date civile (algorithme days_from_civil de
// H. Hinnant) : arithmétique entière, sans fuseau ni jours de 23 h/25 h.
int32_t jour_civil(int annee, int mois, int jour);
bool annee_bissextile(int annee);
// Jours du mois (1-12) de l'année ; 31 pour un mois hors bornes (le maximum : une boucle
// bornée par lui ne coupe aucun jour réel).
int jours_du_mois(int annee, int mois);
// Minutes depuis minuit d'une heure « HH:MM » (00:00 à 23:59) lue à `s` ; -1 si ce n'en
// est pas une (chiffre manquant, pas de « : », heure ou minute hors bornes, texte court).
int hhmm_minutes(const char* s);
inline int hhmm_minutes(const std::string& s, size_t pos = 0) {
    return pos < s.size() ? hhmm_minutes(s.c_str() + pos) : -1;
}

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
// l'horloge. "" hors bornes. Glyphes couverts par la police de lbl_date (règle 6).
const char* fr_day_short_utf8(int wday);
const char* clock_month_short_utf8(int month);
// Première lettre en majuscule : « dimanche » → « Dimanche » (début de libellé).
// Seulement une première lettre ASCII : une langue dont un jour ou un mois
// commence par une lettre accentuée doit l'écrire déjà en majuscule dans sa
// traduction.
std::string fr_capitalized(const char* s);

// Les mêmes libellés dans la langue de l'écran (tab5_i18n, lot 4 du 27/09/2026) :
// ce sont EUX qu'on affiche. Les fr_* restent la source (clés de traduction dans
// Tab5/lang/*.yaml) et ce que vérifient les tests en français.
const char* day_long_utf8(int wday);
const char* month_long_utf8(int mois_1_12);
const char* day_short_utf8(int wday);
const char* month_short_utf8(int month);

// Nom de jour poussé par HA dans le lot des prévisions (« Auj », « Lun »…), traduit
// au moment de l'affichage : cal_jours_data garde le texte reçu. Un texte inconnu
// passe inchangé : le pointeur rendu est alors celui de `nom`, qui doit vivre
// au moins aussi longtemps que son usage.
const char* ha_day_name(const std::string& nom);

// Titres de jour des pages de prévisions, dans la langue de l'écran : « Lun 16 » et
// « mercredi 5 août » (« 1er » pour le premier du mois) ; en anglais « Mon 16 » et
// « Wednesday, August 5 ». "" si l'heure n'est pas synchronisée.
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

// ─── Nombres reçus de Home Assistant (audit du 30/09/2026, lot A) ───
// Convertir en entier un flottant hors des bornes de `int` (« inf », « 1e30 »…) est un
// comportement indéfini en C++ : UBSan l'a relevé sur la tablette virtuelle pour la
// consigne et les bornes de la clim, la luminosité d'une lampe et l'humidité (sur la
// tablette la conversion sature, sur PC elle donne INT_MIN et LVGL déborde ensuite).
// `v` s'il est fini, NAN sinon : un « inf » reçu est traité comme une valeur inconnue.
float tab5_fini_ou_nan(float v);
// Partie entière de `v` (troncature, comme un cast) bornée à [bas, haut] ; `defaut`
// si `v` n'est pas fini. Toute valeur de HA convertie en entier passe par ici.
int tab5_float_vers_int(float v, int bas, int haut, int defaut);
// Luminosité 0-255 d'une lampe allumée, en % (CPP-3, audit du 07/10/2026) : la seule
// formule de la carte, du popup lumière (arc compris) et de la roue — arrondie (127 → 50),
// bornée à 1..100 : une lampe allumée n'affiche jamais 0 %. Inconnue (NaN, infini) : -1.
int lum_pct(float v);

// ─── Batterie de la tablette : montée ou pas ? (discussion #278) ───
// Décidée par tab5_batterie.h (08/10/2026) : tension lue chargeur COUPÉ. Jusqu'à la
// 3.7.0, une lecture sous 6,0 V dans les 10 dernières minutes, chargeur allumé ; il
// chargeait alors dans le vide, sans batterie, avec un léger souffle.
enum class PresenceBatterie : uint8_t {
    INCONNUE,  // pas encore de lecture chargeur coupé (30 premières secondes)
    ABSENTE,   // chargeur coupé, la tension lue est sous 3,0 V (1,9 V mesurés)
    PRESENTE,  // chargeur coupé, au moins 3,0 V : une batterie, même vide
};

// ─── Console système : lignes « Batterie » et « Charge CPU » (discussion #278, 06/10/2026) ───
// Texte de la ligne « Batterie » : « Non montée » si l'interrupteur « Tab5 Batterie
// montée » est éteint, « Sur USB » sans batterie détectée, « -- » avant la première
// lecture, sinon « 78% · 7.62 V » (niveau puis tension, « -- » pour une valeur
// inconnue) ; sur batterie, la consommation à la place de la tension : « 78% · 3.1 W »
// (`puissance_w` fini, 08/10/2026). Jamais de pourcentage sans batterie détectée : sans
// elle, le 8,39 V du chargeur donnerait 100 %. Pure, testée par tools/test_alarm_clock.cpp.
void batterie_texte_console(char* buf, size_t n, bool montee, PresenceBatterie presence,
                            float niveau, float tension, float puissance_w);

// ─── Réglages en quatre pages (08/10/2026, demande d'Axel) ───
// Page affichée après un geste dans le popup Réglages (`nb` pages) : vers la gauche la
// suivante, vers la droite la précédente, en boucle aux deux bouts comme les prévisions
// (qui bouclent aussi, forecast_page_suivante dans tab5_central.cpp). Page hors de
// 0..nb-1 : la première. Pure, testée par tools/test_alarm_clock.cpp.
int reglages_page_voisine(int page, int nb, bool gauche);

// Page Batterie des Réglages, ligne « État » : « Mesure en cours » avant la première
// décision (30 premières secondes), « Pas de batterie détectée », « Sur batterie » (la
// tablette tourne sur elle : courant de décharge lu, tab5_economie.h), « En charge »
// (CHG_STAT), sinon « Sur USB » (batterie pleine, ou en pause par la limite de 80 %).
// Texte traduit. Pure, testée par tools/test_alarm_clock.cpp.
const char* batterie_etat_texte(PresenceBatterie presence, bool en_charge, bool sur_batterie);

// Page Batterie : une valeur mesurée, « 78 % », « 7.62 V » ou « 3.1 W » (même écriture que
// la console), ou « -- » si elle est inconnue ou sans batterie détectée (le chargeur seul
// lit 4,2 à 8,4 V : jamais de niveau, de tension ni de consommation sans elle). Pure,
// testée par tools/test_alarm_clock.cpp.
enum class MesureBatterie : uint8_t { NIVEAU, TENSION, CONSOMMATION };
void batterie_valeur_texte(char* buf, size_t n, PresenceBatterie presence, float valeur, MesureBatterie mesure);

// Charge d'un cœur, en % entier (0 à 100), sur une fenêtre de `duree_us` µs, d'après le
// compteur de temps de sa tâche inactive (FreeRTOS, en µs) lu au début et à la fin.
// Compteur de 32 bits qui reboucle en 71 min : seule la différence compte. -1 si la
// durée est nulle. Pure, testée par tools/test_alarm_clock.cpp.
int cpu_charge_pct(uint32_t inactif_avant, uint32_t inactif_apres, uint32_t duree_us);
