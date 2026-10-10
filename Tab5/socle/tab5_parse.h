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
 *       qu'elle remplace, travers compris (atoi qui lit « 3x » comme 3, tampons de pile
 *       coupés à leur taille). tools/test_parse.cpp fige ces comportements ; en changer un
 *       est un changement de contrat, dans une PR à part, pas un nettoyage. Cinq défauts
 *       relevés par le lot F ont été corrigés ainsi, à part (PR « fix(parse) ») : phrase
 *       « @-1,0 », champs vides de la vigilance, index et nombres illisibles ou non finis
 *       des prévisions, production solaire coupée à 15 octets. Chacun a son test.
 */
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>

#include "tab5_champs.h"
#include "tab5_core.h"
#include "tab5_geometrie.h"

// ─── 1. Prévisions (tab5_maj_meteo_heures_bulk / tab5_maj_meteo_jours_bulk) ───
// Payloads « idx|heure|condition|temp|pluvio;… » et
// « jour|nom|condition|tmin|tmax|repos|dimanche|passé|heures;… », un enregistrement par
// « ; ». Lus dans un tampon de pile de kPrevisionsMax octets : l'appelant refuse avant un
// payload plus long (payload_trop_long), au-delà la fin serait ignorée.
constexpr size_t kPrevisionsMax = 2048;

// Bornes plausibles d'un nombre de prévision (correctif du lot F) : au-delà, ou non fini
// (« nan », « inf », « 1e99 »), l'enregistrement entier est ignoré et le créneau garde ce
// qu'il avait. Températures en °C comme en °F (records : -89 °C, 134 °F) ; pluie en mm
// sur le créneau, jamais négative.
constexpr double kPrevisionTempMin = -100.0;
constexpr double kPrevisionTempMax = 150.0;
constexpr double kPrevisionPluieMax = 1000.0;

// Premier créneau du bloc horaire (« 5|… » → 5, par strtol comme l'atoi d'avant). -1 si
// aucun chiffre n'est lu (« abc », vide) ou s'il est négatif, 15 au-delà de 14 : l'appelant
// refuse alors le payload (payload_refuse) au lieu de le prendre pour le bloc 0.
int previsions_premier_creneau(const char* payload);

// Créneaux horaires : enregistrement d'au moins 5 champs (« | », champs vides gardés)
// et d'index 0 à 14 → heures[idx] (heure, condition, température, pluie en mm ; atof, donc
// « abc » ou vide = 0, ce que HA envoie pour une valeur absente). Le reste est ignoré. Les
// enregistrements vides (« ;; ») sont sautés (strtok_r) : chaque enregistrement porte son
// index, rien ne se décale.
// Renvoie le nombre d'enregistrements ignorés parce qu'illisibles : index sans chiffre, ou
// nombre non fini ou hors des bornes ci-dessus (rien n'est écrit pour eux ; l'appelant le
// journalise une fois par payload). Un index hors de 0 à 14 reste ignoré sans être compté.
int previsions_heures_lire(const char* payload, HourForecastData heures[15]);

// Jours : enregistrement d'au moins 9 champs et de jour 0 à 14 → jours[jour] ; les trois
// drapeaux valent vrai si le champ commence par « 1 ». Le jour 0 date le lot :
// `ancre` = local_day_number_today() au moment de la lecture (-1 si l'heure n'est pas
// réglée), comme cal_jours_anchor_day. Index et températures lus et refusés comme les
// heures ; même valeur de retour (un jour 0 refusé ne touche pas l'ancre).
int previsions_jours_lire(const char* payload, DayForecastData jours[15], int32_t& ancre);

// ─── 2. Vigilance (tab5_maj_alerte_meteo_france) ───
// « phrase pluie|globale|vent|inondation|orages|pluie-inondation|neige-verglas|grand froid|
//   vagues-submersion|canicule|avalanches[|brouillard|feux de forêt] » : 11 champs pour
// Météo-France, 13 avec MeteoAlarm (lot 4c, 27/09/2026).
constexpr int kVigilanceChamps = 13;
constexpr int kVigilancePhenomenes = 11;  // champs 2 à 12
constexpr int kVigilanceActivesMax = 4;   // cases d'icônes du bandeau

// Payload découpé en place dans `buf` (1 023 octets lus : l'appelant journalise un payload
// plus long, dont les derniers champs manquent). champs[i] vaut "" au-delà du dernier.
// Champs vides gardés à leur place (split_fields) : « p||Orange » laisse le champ 1 vide
// et « Orange » au champ 2. Jusqu'au correctif du lot F, strtok_r les fusionnait et un
// champ vide décalait les suivants (R6 de l'audit) ; HA envoie toujours « Vert ».
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
    // Jeton suivant. strtok_r : les « ; » consécutifs sont sautés ; sans effet sur la
    // suite (un jeton vide serait AUTRE, que l'écran ignore, et chaque alerte porte son id).
    // Les champs d'une alerte, eux, gardent leurs vides (split_fields).
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
// atoi (hors de 0 à 8 compris : c'est l'écran qui les ignore). strtok_r : « ;; » sauté,
// sans décalage (chaque enregistrement porte son index ; l'intensité vide est gardée).
// Au plus kPluieBarresMax enregistrements tiennent dans 255 octets (« |;|;… »).
constexpr size_t kPluieMax = 255;
constexpr int kPluieBarresMax = 128;
struct PluieBarre {
    int idx;
    int niveau;
};
int pluie_barres_lire(const char* payload, PluieBarre out[kPluieBarresMax]);

// Phrase pluie (1er champ de la vigilance, lot 4c) : « @niveau,début » (niveau -1 à 5 par
// atoi, début = epoch UTC par strtoll après la première virgule, 0 sans virgule), « @- »
// sans chiffre après le « - » (aucune source : niveau -2, début 0), ou un texte sans « @ »
// montré tel quel (code = false, niveau et début non lus). « @-1,0 » (pas de données, ce
// que HA envoie sans relevé) donne le niveau -1, plus « aucune source » (correctif du lot F).
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
// strtok_r : « ;; » sauté, sans décalage (chaque ligne porte son genre ; l'écran ne compte
// que les LIGNE). Une ligne sans « | » ou au texte vide est AUTRE.
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

// Production solaire « solaire|pourcentage » : champ_nombre (strtof sur n octets), NAN si
// vide, illisible, non fini ou plus long que kChampNombreMax (31 octets : refusé, plus
// coupé à 15 depuis le correctif du lot F), sinon borné à 0..100.
float solaire_pourcent(const char* valeur, size_t n);

// Climat d'une pièce « pR|température|humidité|clim » (ADR-0040, 09/10/2026) : R de 0 à
// kPieces - 1 (tab5_geometrie.h). Pour chaque mesure, champ vide ou absent = aucune sonde
// déclarée ; « nan » ou illisible = sonde déclarée, valeur inconnue (NAN). clim : le
// quatrième champ vaut exactement « 1 ». Faux, et `out` intact, si la clé n'est pas « pR ».
struct PieceClimatLu {
    int piece = -1;
    bool temperature = false;  // une sonde de température est déclarée
    float t = NAN;
    bool humidite = false;  // une sonde d'humidité est déclarée
    float h = NAN;
    bool clim = false;
};
bool piece_climat_lire(const Champ& cle, const Champ& reste, PieceClimatLu& out);

// Zone à gauche de l'horloge au choix (ADR-0051, 10/10/2026) : « gauche|défaut|c1|c2|… ».
// Contenus, dans l'ORDRE du cycle et de la NVS (Tab5/ecran/tab5_zone_gauche.cpp) : une
// valeur de plus va à la FIN, aucune ne se déplace. Codes lus par le blueprint
// (codes_gauche) : ni traduits ni renommés sans lui (tests/test_zone_gauche.py).
// LECTEUR : le lecteur de musique compact (lot 2, sur le lecteur de l'ADR-0050), sauté
// quand HA a dit qu'aucun lecteur n'est choisi (tab5_zone_gauche.cpp, disponible()).
enum class ZoneGauche : uint8_t {
    VOCAL,      // le micro et les boutons Domo / Discu (l'écran d'avant)
    GRAPHIQUE,  // les prévisions des heures qui viennent en courbe et barres de pluie
    LECTEUR,    // le lecteur de musique compact (lot 2)
    NB
};
constexpr const char* kZoneGaucheCodes[static_cast<int>(ZoneGauche::NB)] = {"vocal", "graphique", "lecteur"};
constexpr uint8_t zone_gauche_bit(ZoneGauche z) { return static_cast<uint8_t>(1u << static_cast<int>(z)); }
// Sans la clé (blueprint plus ancien) : le vocal au départ, le vocal et le graphique au tap.
constexpr ZoneGauche kZoneGaucheDefaut = ZoneGauche::VOCAL;
constexpr uint8_t kZoneGaucheCycleDefaut = zone_gauche_bit(ZoneGauche::VOCAL) | zone_gauche_bit(ZoneGauche::GRAPHIQUE);
struct ZoneGaucheLu {
    ZoneGauche defaut = kZoneGaucheDefaut;
    uint8_t cycle = kZoneGaucheCycleDefaut;  // un bit par contenu proposé au tap (zone_gauche_bit)
};
// Premier champ : le contenu au départ (vide ou inconnu : le vocal). Les suivants : les
// contenus proposés au tap, dans n'importe quel ordre, codes inconnus ignorés (blueprint
// plus récent). Le contenu de départ en fait toujours partie : sans autre champ, le tap ne
// change rien. Au-delà de kZoneGaucheChampsMax champs, la fin est ignorée.
constexpr int kZoneGaucheChampsMax = 8;
ZoneGaucheLu zone_gauche_lire(const char* valeur, size_t n);

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

// ─── 8. Popup Température (tab5_maj_historique, ADR-0032 ; humidité : ADR-0047) ───
// Venu de historique_recu() (Tab5/ecran/tab5_historique.cpp) le 09/10/2026, boucle
// recopiée telle quelle, puis l'humidité ajoutée (lot « climat des pièces en graphique ») :
//   entete     « nom|debut|pas|maintenant|actuel|exterieur[|humidité] »
//   mesures    « moy,min,max[,h_moy,h_min,h_max] » par créneau, séparés par « ; »
//   previsions « minute,moy[,min,max] » séparés par « ; », dans l'ordre du temps
// Le septième champ de l'en-tête dit qu'une sonde d'humidité est déclarée (vide ou absent :
// aucune, l'écran d'avant ; « nan » : déclarée, valeur inconnue) ; les trois champs
// d'humidité d'un créneau suivent ceux de la température (un firmware plus ancien lit les
// trois premiers et ignore le reste : aucun changement de contrat).
constexpr int kHistoriqueMesuresMax = 64;   // 24 + 1, 56 + 1, 30 + 1 créneaux
constexpr int kHistoriquePrevMax = 48;      // 72 h d'heures au plus, ou 7 jours
// Bornes de lecture : au-delà, c'est un payload faux, pas une mesure. Elles gardent aussi
// les conversions en entier et les calculs de minutes sans débordement (le fuzz des
// sanitizers envoie 1e30). La vue 30 jours et ses 7 jours de prévision vont jusqu'à
// 54 000 minutes.
constexpr float kHistoriqueMinutesMax = 1.0e6f;
constexpr float kHistoriquePasMax = 1440.0f;  // un créneau d'un jour au plus
constexpr float kHistoriqueTempMax = 1000.0f;
// Humidité en % entier (HA l'arrondit) ; 0xFF : pas de mesure.
constexpr uint8_t kHumiditeAucune = 0xFF;

struct HistoriquePoint {
    float moy = NAN, mn = NAN, mx = NAN;
    uint8_t h_moy = kHumiditeAucune, h_mn = kHumiditeAucune, h_mx = kHumiditeAucune;
};

struct HistoriquePrev {
    int32_t minute = 0;  // depuis le début du premier créneau
    float moy = NAN, mn = NAN, mx = NAN;
};

struct HistoriqueSerie {
    bool recue = false;
    bool exterieur = false;
    bool humidite = false;            // une sonde d'humidité est déclarée (7e champ non vide)
    char nom[48] = {};                // copié par l'écran (texte_ha_copier), pas par historique_lire
    int64_t debut_jour = 0;           // jours depuis le 1970-01-01 (date locale)
    int32_t debut_min = 0;            // minute du jour du premier créneau
    int32_t pas = 60;                 // minutes par créneau
    int32_t maintenant = 0;           // minutes depuis le début
    float actuel = NAN;
    uint8_t h_actuelle = kHumiditeAucune;
    int n = 0;
    HistoriquePoint m[kHistoriqueMesuresMax];
    int np = 0;
    HistoriquePrev p[kHistoriquePrevMax];
};

// Humidité d'un champ : % arrondi, kHumiditeAucune si vide, illisible, non finie ou hors
// de 0 à 100.
uint8_t humidite_lire(const Champ& c);

// Remet `s` à neuf et la remplit (recue = vrai). Une date illisible vaut le 2000-01-01
// 00:00 (seuls les libellés de l'axe s'en servent), un pas illisible ou hors de 1 à
// kHistoriquePasMax 60, une minute illisible ou hors de 0 à kHistoriqueMinutesMax 0 (en-tête)
// ou un point de prévision sauté, une température hors de ±kHistoriqueTempMax NAN ; un
// point de prévision hors de l'ordre du temps est sauté. Au plus kHistoriqueMesuresMax
// créneaux et kHistoriquePrevMax points. Renvoie le nom (premier champ de l'en-tête), que
// l'écran copie dans s.nom.
Champ historique_lire(const Champ& entete, const Champ& mesures, const Champ& previsions, HistoriqueSerie& s);

// ─── 9. Lecteur de musique (tab5_maj_lecteur, ADR-0050) ───
// Deux variables, poussées par packages/tab5_lecteur.yaml :
//   lecteurs « nom|genre;nom|genre;… » : les lecteurs choisis dans « Tab5 · lecteurs de
//            musique », dans l'ordre de la liste (kLecteursMax au plus) ; genre = la
//            device_class de HA (tv, speaker, receiver, ou vide), jamais montrée telle quelle.
//   etat     « actif|nom|genre|état|titre|artiste|album|app|position|durée|volume|muet|
//            aléatoire|répétition|fonctions|image » : le lecteur montré. Vide : aucun.
//            actif = son index dans `lecteurs`, -1 pour un lecteur hors de la liste (celui
//            d'une tuile med) ; état = celui de HA (playing, paused, idle, on, off,
//            standby, buffering, unavailable, unknown) ; position et durée en secondes
//            (position déjà avancée jusqu'à l'envoi par HA) ; volume 0 à 100 ; muet et
//            aléatoire 1 / 0 ; répétition off / all / one ; un champ inconnu vaut « - » ou
//            rien. fonctions : une lettre par commande offerte (supported_features lu par
//            HA, comme les capacités d'une clim, ADR-0026) ; image : l'entity_picture
//            (chemin relatif à HA) ou une URL complète, en dernier : elle prend le reste.
// HA retire « | » et « ; » des textes.
constexpr int kLecteursMax = 6;
constexpr size_t kLecteurNomMax = 48;     // nom d'un lecteur, copié par l'écran (texte_ha_copier)
constexpr size_t kLecteurTexteMax = 120;  // titre, artiste, album (une ligne coupée par « … »)
constexpr size_t kLecteurUrlMax = 512;    // base + entity_picture (jeton et cache compris)
constexpr float kLecteurDureeMax = 1.0e6f;  // ~11 jours : au-delà, une position fausse

enum class LecteurGenre : uint8_t {
    AUTRE,     // vide ou inconnu : une note de musique
    TV,        // « tv »
    ENCEINTE,  // « speaker »
    AMPLI,     // « receiver »
};
LecteurGenre lecteur_genre_lire(const Champ& c);

struct LecteurListeLu {
    Champ nom;
    LecteurGenre genre;
};
// Au plus kLecteursMax lecteurs, dans l'ordre. Un enregistrement vide (« ;; ») est sauté ;
// un nom vide est gardé (l'écran montre alors « Lecteur n »). Renvoie le nombre lu.
int lecteurs_lire(const Champ& payload, LecteurListeLu out[kLecteursMax]);

enum class LecteurEtat : uint8_t {
    AUCUN,          // payload vide : aucun lecteur choisi
    INDISPONIBLE,   // unavailable, unknown, ou un état inconnu
    ETEINT,         // off
    VEILLE,         // standby
    INACTIF,        // idle, on : allumé, rien en lecture
    LECTURE,        // playing
    PAUSE,          // paused
    CHARGEMENT,     // buffering
};
LecteurEtat lecteur_etat_code(const Champ& c);

enum class LecteurRepetition : uint8_t {
    INCONNUE,
    NON,   // off
    TOUT,  // all
    UNE,   // one
};

// Lettres de `fonctions` → bits. l lecture / pause, s position (seek), v volume, m muet,
// p précédent, n suivant, a aléatoire, r répétition, o allumer. Une autre lettre est ignorée.
enum : uint16_t {
    LECTEUR_F_LECTURE = 1u << 0,
    LECTEUR_F_POSITION = 1u << 1,
    LECTEUR_F_VOLUME = 1u << 2,
    LECTEUR_F_MUET = 1u << 3,
    LECTEUR_F_PRECEDENT = 1u << 4,
    LECTEUR_F_SUIVANT = 1u << 5,
    LECTEUR_F_ALEATOIRE = 1u << 6,
    LECTEUR_F_REPETITION = 1u << 7,
    LECTEUR_F_ALLUMER = 1u << 8,
};
uint16_t lecteur_fonctions_lire(const Champ& c);

struct LecteurEtatLu {
    int actif = -1;
    Champ nom{nullptr, 0};
    LecteurGenre genre = LecteurGenre::AUTRE;
    LecteurEtat etat = LecteurEtat::AUCUN;
    Champ titre{nullptr, 0};
    Champ artiste{nullptr, 0};
    Champ album{nullptr, 0};
    Champ app{nullptr, 0};
    float position = NAN;  // secondes, NAN : inconnue (0 à kLecteurDureeMax)
    float duree = NAN;     // secondes, NAN : inconnue ou nulle (un direct)
    int volume = -1;       // 0 à 100, -1 : inconnu
    int8_t muet = -1;      // 1, 0, -1 : inconnu
    int8_t aleatoire = -1;
    LecteurRepetition repetition = LecteurRepetition::INCONNUE;
    uint16_t fonctions = 0;
    Champ image{nullptr, 0};
};
// Faux, et `out` remis à neuf (AUCUN), pour un payload vide ou de moins de quatre champs.
// Un index hors de -1 à kLecteursMax - 1 vaut -1 ; un nombre illisible, non fini ou hors
// de ses bornes vaut inconnu ; une durée de 0 aussi (un direct n'a pas de durée).
bool lecteur_etat_lire(const Champ& payload, LecteurEtatLu& out);

// Position à montrer `ecoule` secondes après la réception : avancée en lecture seulement,
// jamais au-delà de la durée connue ; NAN si la position est inconnue.
float lecteur_position(const LecteurEtatLu& e, float ecoule);

// « m:ss » sous une heure, « h:mm:ss » au-delà ; « -:-- » pour une valeur inconnue,
// négative ou au-delà de kLecteurDureeMax. Faux si `out` est trop petit (8 octets suffisent
// jusqu'à 99 h ; 12 au-delà).
bool lecteur_temps_texte(float s, char* out, size_t n);

// Base « http://hôte:8123 » tirée de l'adresse du client API de Home Assistant (celle que
// l'API ESPHome donne à on_client_connected) : IPv4 telle quelle, IPv6 entre crochets.
// Faux (out vidé) si l'adresse est vide, trop longue ou contient autre chose que des
// chiffres hexadécimaux, « . » et « : » (une zone IPv6 « %eth0 » comprise).
bool ha_base_depuis_hote(const char* hote, char* out, size_t n);

// URL d'une image de HA : `image` telle quelle si elle commence par http:// ou https://,
// sinon `base` (http(s)://…, « / » final retiré) suivie de l'image (« / » ajouté s'il
// manque). Faux (out vidé) si l'image est vide, si une base est nécessaire et qu'elle
// manque ou n'est pas en http(s)://, si l'URL contient un espace ou un caractère de
// contrôle, ou si elle ne tient pas dans `n`.
bool ha_image_url(const Champ& image, const char* base, char* out, size_t n);
