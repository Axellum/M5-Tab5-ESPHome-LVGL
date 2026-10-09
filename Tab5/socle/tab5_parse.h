/**
 * [AI-CONTEXT]
 * @file tab5_parse.h
 * @role Lecture des payloads poussés par Home Assistant (lot F de l'audit du 30/09/2026,
 *       §2.4) : ce que chaque service reçoit, découpé et converti en données simples, sans
 *       rien peindre. Le code de l'écran (Tab5/ecran/) appelle ces fonctions puis pose
 *       ses widgets ; la lecture elle-même se teste et se fuzze sur PC
 *       (tools/test_parse.cpp, g++ en CI ; tools/fuzz/fuzz_parse.cpp, libFuzzer).
 *       Une famille par section, dans l'ordre du plan : prévisions, vigilance, alertes et
 *       bandeau info, pluie, puis les suivantes.
 * @architecture_constraint Logique PURE, comme tab5_core et tab5_champs : ni ESPHome ni
 *       LVGL (tests/test_rangement.py). Un refus journalisé (payload_refuse,
 *       payload_trop_long) reste chez l'appelant, avant l'appel.
 * @ai_instruction Extraction NEUTRE : chaque fonction reprend à l'identique la boucle
 *       qu'elle remplace, travers compris (strtok_r qui fusionne les champs vides, atoi
 *       qui lit « 3x » comme 3, tampons de pile coupés à leur taille). tools/test_parse.cpp
 *       fige ces comportements ; en changer un est un changement de contrat, dans une PR
 *       à part, pas un nettoyage.
 */
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "tab5_champs.h"
#include "tab5_core.h"

// ─── 1. Prévisions (tab5_maj_meteo_heures_bulk / tab5_maj_meteo_jours_bulk) ───
// Payloads « idx|heure|condition|temp|pluvio;… » et
// « jour|nom|condition|tmin|tmax|repos|dimanche|passé|heures;… », un enregistrement par
// « ; ». Lus dans un tampon de pile de kPrevisionsMax octets : l'appelant refuse avant un
// payload plus long (payload_trop_long), au-delà la fin serait ignorée.
constexpr size_t kPrevisionsMax = 2048;

// Premier créneau du bloc horaire (atoi du payload : « 5|… » → 5, illisible → 0).
int previsions_premier_creneau(const char* payload);

// Créneaux horaires : enregistrement d'au moins 5 champs (« | », champs vides gardés)
// et d'index 0 à 14 → heures[idx] (heure, condition, température, pluie en mm ; atof, donc
// « abc » = 0). Le reste est ignoré. Les enregistrements vides (« ;; ») sont sautés
// (strtok_r).
void previsions_heures_lire(const char* payload, HourForecastData heures[15]);

// Jours : enregistrement d'au moins 9 champs et de jour 0 à 14 → jours[jour] ; les trois
// drapeaux valent vrai si le champ commence par « 1 ». Le jour 0 date le lot :
// `ancre` = local_day_number_today() au moment de la lecture (-1 si l'heure n'est pas
// réglée), comme cal_jours_anchor_day.
void previsions_jours_lire(const char* payload, DayForecastData jours[15], int32_t& ancre);

// ─── 2. Vigilance (tab5_maj_alerte_meteo_france) ───
// « phrase pluie|globale|vent|inondation|orages|pluie-inondation|neige-verglas|grand froid|
//   vagues-submersion|canicule|avalanches[|brouillard|feux de forêt] » : 11 champs pour
// Météo-France, 13 avec MeteoAlarm (lot 4c, 27/09/2026).
constexpr int kVigilanceChamps = 13;
constexpr int kVigilancePhenomenes = 11;  // champs 2 à 12
constexpr int kVigilanceActivesMax = 4;   // cases d'icônes du bandeau

// Payload découpé en place dans `buf` (1 023 octets lus : l'appelant journalise un payload
// plus long, dont les derniers champs manquent). champs[i] vaut "" au-delà du dernier.
// [figé] strtok_r : des « | » consécutifs comptent pour un, un champ vide décale donc les
// suivants (R6 de l'audit) ; HA envoie toujours « Vert », jamais un champ vide.
struct VigilanceLue {
    char buf[1024];
    const char* champs[kVigilanceChamps];
};
void vigilance_lire(const char* payload, VigilanceLue& v);

// Niveau d'un champ, comparaison exacte (casse comprise) : « Jaune », « Orange », « Rouge ».
enum class NiveauVigilance : uint8_t {
    AUTRE,
    JAUNE,
    ORANGE,
    ROUGE,
};
NiveauVigilance vigilance_niveau(const char* s);

// Phénomènes actifs, dans l'ordre du payload, au plus kVigilanceActivesMax : un champ
// ni vide, ni « Vert », ni « unknown » (tout autre texte compte, « Jaune » ou pas).
// out[k].phenomene = 0 (vent) à 10 (feux de forêt) ; out[k].niveau pointe dans v.buf.
struct VigilanceActive {
    int phenomene;
    const char* niveau;
};
int vigilance_actives(const VigilanceLue& v, VigilanceActive out[kVigilanceActivesMax]);

// ─── 3. Alertes HA, historique des alertes, bandeau info ───
// Bandeaux (tab5_maj_alertes_ha_bulk) : « @n:N;id|niveau|texte;id|niveau|texte;… »,
// 1 024 octets au plus (l'appelant refuse au-delà). Lu jeton par jeton, découpé en place
// dans le tampon du lecteur ; les pointeurs rendus vivent autant que lui.
constexpr size_t kAlertesHaMax = 1024;
enum class AlerteHaType : uint8_t {
    FIN,     // plus de jeton
    TOTAL,   // « @n:N » : total = atoi(N), alertes à lire en tout chez HA
    ALERTE,  // au moins 3 champs « | » : id, niveau, texte (le 3e s'arrête au « | » suivant)
    AUTRE,   // moins de 3 champs : ignoré par l'écran
};
struct AlerteHaJeton {
    AlerteHaType type;
    int total;
    const char* id;
    const char* niveau;
    const char* texte;
};
class LecteurAlertesHa {
public:
    explicit LecteurAlertesHa(const char* payload);
    // Jeton suivant. [figé] strtok_r : les « ; » consécutifs sont sautés.
    AlerteHaJeton suivant();

private:
    char buf_[kAlertesHaMax + 1];
    char* save_ = nullptr;
    bool premier_ = true;
};

// Libellé codé d'une alerte (lot 4c, 27/09/2026) : « @maj:<titre> », « @indispo:<n> »
// (nombre = atoi), « @vigi:<niveau> » ; tout autre texte se montre tel quel.
enum class AlerteTexteCode : uint8_t {
    TEXTE,
    MAJ,
    INDISPO,
    VIGI,
};
struct AlerteTexteLu {
    AlerteTexteCode code;
    const char* reste;  // après le préfixe (le texte entier pour TEXTE)
    int nombre;         // INDISPO seulement
};
AlerteTexteLu alerte_texte_lire(const char* brut);

// Historique (tab5_maj_alertes_historique) : « apparue|lue|terminée|gravité|libellé;… »,
// libellé = le reste. Epochs : champ_entier() borné à kAlerteEpochMax (2100), 0 sinon
// (vide, illisible, « -1 »). Gravité : 'R' ou 'J' si le champ commence ainsi, 'O' sinon.
// Une entrée sans ses cinq champs, ou avec apparue = 0, est comptée dans `illisibles` et
// sautée. Au plus `max` entrées gardées ; lecture du texte jusqu'au premier zéro.
constexpr uint32_t kAlerteEpochMax = 4102444800u;
struct AlerteHistoriqueLue {
    uint32_t apparue;
    uint32_t lue;       // 0 : pas encore lue
    uint32_t terminee;  // 0 : en cours
    char gravite;       // 'R', 'O' ou 'J'
    Champ texte;        // dans le payload, sans zéro final
};
int alertes_historique_lire(const char* payload, AlerteHistoriqueLue out[], int max, int& illisibles);

// Bandeau info codé (tab5_maj_info_texte, lot 4c) : après « @ha| »,
// « nb MAJ|titre|nb erreurs|nb indispo|jaune 0/1|vigilance ». Copié dans `buf` (255
// octets lus), découpé par split_fields (champs vides gardés) ; un champ absent vaut "",
// donc 0. Nombres : atoi.
struct InfoCodeLu {
    char buf[256];
    int nb_maj;
    const char* titre;
    int nb_err;
    int nb_indispo;
    bool jaune;
    const char* vigi;
};
void info_code_lire(const char* apres_prefixe, InfoCodeLu& out);

// ─── 4. Pluie (tab5_maj_pluie_1h_bulk, phrase de la vigilance) ───
// Niveau 0 à 4 d'une barre : « 0 » à « 4 » (adaptateurs des autres fournisseurs, lot 4c),
// ou le libellé Météo-France (« Pluie faible » 1, « Pluie modérée » 2, « Pluie forte » 3,
// « Pluie très forte » ou « Pluie trés forte » 4) ; tout autre texte : 0 (barre vide).
int pluie_niveau(const std::string& intensite);

// Barres « idx|intensité;idx|intensité;… » : 255 octets lus (l'appelant refuse au-delà).
// Un enregistrement sans « | » est sauté, les autres sont rendus dans l'ordre, index par
// atoi (hors de 0 à 8 compris : c'est l'écran qui les ignore). [figé] strtok_r : « ;; »
// sauté. Au plus kPluieBarresMax enregistrements tiennent dans 255 octets (« |;|;… »).
constexpr size_t kPluieMax = 255;
constexpr int kPluieBarresMax = 128;
struct PluieBarre {
    int idx;
    int niveau;
};
int pluie_barres_lire(const char* payload, PluieBarre out[kPluieBarresMax]);

// Phrase pluie (1er champ de la vigilance, lot 4c) : « @niveau,début » (niveau -1 à 5 par
// atoi, début = epoch UTC par strtoll après la première virgule, 0 sans virgule), « @- »
// (aucune source : niveau -2, début 0), ou un texte sans « @ » montré tel quel
// (code = false, niveau et début non lus).
struct PluiePhrase {
    bool code;
    int niveau;
    int64_t debut;
};
PluiePhrase pluie_phrase_lire(const std::string& phrase);

// ─── 5. Calendrier (tab5_maj_calendrier_mois, tab5_maj_calendrier_jour) ───
// Mois poussé : année et mois par atoi ; vrai s'ils sont dans 2000..2100 et 1..12.
bool calendrier_mois_lire(const std::string& annee, const std::string& mois, int& y, int& m);

// idx-ième champ (0 = le premier) de `s` séparé par `sep`, champs vides compris ; "" s'il
// n'y en a pas autant. Heures du mois (« | ») et détails embarqués (« ~ »).
std::string calendrier_champ(const std::string& s, int idx, char sep);

// Code du jour `day` (1-31) : deux chiffres hexadécimaux (casse libre) à (day - 1) × 2 ;
// 0 si `codes` est trop court ou si day < 1. Un caractère non hexadécimal vaut 0.
// Bits : CAL_BIT_*.
int calendrier_code_jour(const std::string& codes, int day);

// Date « AAAA-MM-JJ » (sscanf « %d-%d-%d ») ; faux si les trois nombres n'y sont pas.
// Aucune borne : « 2026-13-40 » est lu tel quel.
bool calendrier_date_lire(const char* date_iso, int& y, int& m, int& d);

// Détail du jour « type|texte;type|texte;… » (script HA tab5_calendrier_jour) : 1 023
// octets lus (l'appelant journalise au-delà), découpé en place dans le lecteur.
// [figé] strtok_r : « ;; » sauté. Une ligne sans « | » ou au texte vide est AUTRE.
constexpr size_t kCalendrierJourMax = 1024;
enum class CalJourType : uint8_t {
    FIN,
    LIGNE,
    AUTRE,
};
struct CalJourLigne {
    CalJourType type;
    const char* genre;  // « travail », « ferie », « vacances », « rdv », « anniv », « fete »…
    const char* texte;  // le reste après le premier « | »
};
class LecteurJourCalendrier {
public:
    explicit LecteurJourCalendrier(const char* payload);
    CalJourLigne suivante();

private:
    char buf_[kCalendrierJourMax];
    char* save_ = nullptr;
    bool premier_ = true;
};

// ─── 6. Emplacements et zones (tab5_maj_emplacements) ───
// « clé|reste;clé|reste;… » parcouru enregistrement par enregistrement, sur toute la
// longueur du payload (zéros compris). `debut` avance après le « ; » ; faux à la fin.
// a_cle : l'enregistrement a un « | » ; sinon cle et reste sont vides et l'écran l'ignore.
// Un enregistrement vide (« ;; ») est rendu, sans clé.
struct EmplacementLu {
    bool a_cle;
    Champ cle;    // avant le premier « | »
    Champ reste;  // après, jusqu'au « ; »
};
bool emplacement_suivant(const std::string& payload, size_t& debut, EmplacementLu& e);

// Emplacement 3.x « clé|état[|valeur] » : reste coupé au premier « | » ; valeur vide
// sans second « | » (le reste après lui, « | » compris, sinon).
void emplacement_etat_valeur(const Champ& reste, Champ& etat, Champ& valeur);

// Valeur numérique d'un emplacement (strtof sur le texte entier) : NAN si vide, illisible
// (« unavailable ») ou non finie (« inf », lot A) ; « 21.5 °C » → 21.5.
float emplacement_nombre(const std::string& valeur);

// Production solaire « solaire|pourcentage » : 15 premiers octets lus (strtof), NAN si
// vide, illisible ou non fini, sinon borné à 0..100. [figé] un texte plus long est coupé
// à 15 octets, pas refusé.
float solaire_pourcent(const char* valeur, size_t n);

// ─── 7. Clim (clés « climr », « crRT » et « ceRT » de tab5_maj_emplacements) ───
// Réglages et état d'une clim (ADR-0026, ADR-0027), venus de Tab5/ecran/tab5_clim.cpp
// tels quels : l'écran garde ses tables (s_clim, s_ct) et ce qu'il en peint.

// Plage plausible des bornes d'une clim, en °C comme en °F : des « min|max » reçus
// au-delà sont ignorés, et l'arc n'en reçoit jamais d'autres (lot A, audit du 30/09/2026).
constexpr int kClimBorneBasse = -100;
constexpr int kClimBorneHaute = 200;

struct ClimReglages {
    float min = 16.0f;
    float max = 30.0f;
    float pas = 0.5f;
    bool fahrenheit = false;
    // Lettres des boutons que l'appareil gère (tableau de l'ADR-0026) : c froid, h chaud,
    // d sec, f ventilation, e Éco, b Boost, q Silence, s Oscillation, w Brise.
    char capacites[16] = "chdfebqsw";
    char nom[49] = "";  // friendly_name de la clim, 48 octets au plus
    bool recu = false;
};

// Modes gardés sur 15 octets au plus : la chaîne reste dans son std::string (petite
// chaîne, sans allocation). Aucun mode de HA n'est plus long.
constexpr size_t kModeMax = 15;

struct ClimEtat {
    float consigne = NAN;  // NaN : inconnue (« -- » ; − / + ne font rien, l'arc la choisit)
    float piece = NAN;     // température de la pièce
    std::string mode;
    std::string preset;
    std::string ventilation;
    std::string oscillation;
};

// Réglages « min|max|pas|unité|capacités|nom » (climr ou crRT, sans la clé) dans `r`,
// dont les valeurs servent de défaut à un nombre illisible. Renvoie le nombre de champs :
// moins de 5, rien n'est changé. Le nom n'est pas copié : `nom` le désigne (6 champs ;
// vide sinon, et r.nom est vidé) et l'écran le copie par texte_ha_copier (glyphes des
// polices, tab5_internal.h).
int clim_reglages_lire(const char* reste, size_t n, ClimReglages& r, Champ& nom);

// État « consigne|pièce|mode|préréglage|ventilation|oscillation » (ceRT, sans la clé) :
// les nombres « nan » ou illisibles sont inconnus, les modes gardés tels quels (bornés).
void clim_etat_lire(const char* reste, size_t n, ClimEtat& e);
