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
// (nombre = atoi), « @vigi:<niveau> », « @froid:<cause>:<niveau>:<valeur>:<nom> »
// (ADR-0055, lu par froid_alerte_lire, section 12) ; tout autre texte se montre tel quel.
enum class AlerteTexteCode : uint8_t {
    TEXTE,
    MAJ,
    INDISPO,
    VIGI,
    FROID,
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
// kPiecesMax - 1 (tab5_geometrie.h, dix pièces depuis l'ADR-0066). Pour chaque mesure, champ vide ou absent = aucune sonde
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
// CAPTEUR : le premier capteur de « Tab5 · capteurs suivis » en courbe (ADR-0054, section
// 11), sauté de même quand HA a dit qu'aucun capteur n'est choisi.
enum class ZoneGauche : uint8_t {
    VOCAL,      // le micro et les boutons Domo / Discu (l'écran d'avant)
    GRAPHIQUE,  // les prévisions des heures qui viennent en courbe et barres de pluie
    LECTEUR,    // le lecteur de musique compact (lot 2)
    CAPTEUR,    // un capteur suivi : nom, valeur, variation, courbe des 24 h (ADR-0054)
    NB
};
constexpr const char* kZoneGaucheCodes[static_cast<int>(ZoneGauche::NB)] = {"vocal", "graphique", "lecteur",
                                                                            "capteur"};
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

// ─── 9 bis. Musique de toute la maison (ADR-0067, sur le lecteur de la section 9) ───
// Deux actions, poussées par packages/tab5_musique.yaml (Music Assistant ou la liste
// « Tab5 · lecteurs de musique ») ; « | » et « ; » des textes remplacés par HA (« / »,
// « , »), « ; » d'une URL écrit %3B.
//   tab5_maj_musique_pieces(pieces, contexte), toujours poussée, même vide :
//     contexte « ma|montre|file|suivant » : ma 1 avec Music Assistant ; montre l'index dans
//              `pieces` du lecteur montré (ADR-0050), -1 absent ; file le nombre de titres
//              de sa file (0 inconnu) ; suivant « titre — artiste » du titre suivant, en
//              dernier : il prend le reste.
//     pieces   kMusiquePiecesMax « nom|genre|état|volume|muet|groupe|fonctions|titre » au
//              plus, séparés par « ; » : genre t TV, e enceinte, a écran / tablette, g
//              groupe, x autre ; état j lecture, p pause, r allumé, e éteint / veille, i
//              indisponible ; volume 0-100 (vide inconnu) ; muet 1 / 0 ; groupe l'index du
//              meneur de son groupe (le sien s'il mène), -1 seul ; fonctions v volume, m
//              muet, o marche, g grouper, s sources, t transfert ; titre en dernier.
//              L'index d'une pièce est son RANG dans `pieces` : c'est lui que l'événement
//              esphome.tab5_musique renvoie, et HA refait la même liste pour le lire.
//   tab5_maj_musique_liste(contexte, elements), en réponse à un événement seulement :
//     contexte « vue|titre|page|suite|retour » : vue fav pl alb art rad pod rec det src
//              rech err (err : titre = la phrase à montrer) ; page depuis 0 ; suite 1 s'il
//              y a une page de plus ; retour 1 pour le bouton retour.
//     elements kMusiqueElementsMax « nom|sous|genre|ref|image » au plus, séparés par « ; » :
//              genre t titre, a album, r artiste, p playlist, d radio, o podcast ou
//              épisode, l livre audio, s source, x autre ; ref une URI de Music Assistant
//              (160 caractères au plus) ou, pour s, le rang de la source ; image une URL
//              http(s), en dernier (elle prend le reste), vide sans pochette.
// Rien en NVS : HA repousse tout à la connexion et à l'ouverture du popup.
constexpr int kMusiquePiecesMax = 8;
constexpr int kMusiqueElementsMax = 10;
// Tailles en octets (zéro final non compris) des textes copiés par l'écran : HA coupe à
// 40, 60 et 48 caractères, deux octets au plus chacun dans les polices (Latin-1).
constexpr size_t kMusiqueNomMax = 80;          // nom d'une pièce, d'un élément, sa 2e ligne
constexpr size_t kMusiqueTitreMax = 120;       // « titre — artiste » d'une pièce, le suivant
constexpr size_t kMusiqueListeTitreMax = 96;   // titre d'une liste, phrase d'erreur
constexpr size_t kMusiqueRefMax = 160;         // URI de Music Assistant (ASCII)

enum class MusiqueGenre : uint8_t {
    AUTRE,     // x, ou inconnu
    TV,        // t
    ENCEINTE,  // e
    ECRAN,     // a : écran, tablette
    GROUPE,    // g : un groupe de lecteurs
};
enum class MusiqueEtat : uint8_t {
    INDISPONIBLE,  // i, ou inconnu
    ETEINT,        // e : éteint ou en veille
    ALLUME,        // r : allumé, rien en lecture
    PAUSE,         // p
    LECTURE,       // j
};
// Lettres de `fonctions` → bits ; une autre lettre est ignorée.
enum : uint8_t {
    MUSIQUE_F_VOLUME = 1u << 0,     // v
    MUSIQUE_F_MUET = 1u << 1,       // m
    MUSIQUE_F_MARCHE = 1u << 2,     // o
    MUSIQUE_F_GROUPE = 1u << 3,     // g
    MUSIQUE_F_SOURCE = 1u << 4,     // s
    MUSIQUE_F_TRANSFERT = 1u << 5,  // t
};
uint8_t musique_fonctions_lire(const Champ& c);

struct MusiquePieceLue {
    int index = -1;  // rang dans `pieces` (enregistrements vides comptés) : celui de l'événement
    Champ nom{nullptr, 0};
    MusiqueGenre genre = MusiqueGenre::AUTRE;
    MusiqueEtat etat = MusiqueEtat::INDISPONIBLE;
    int volume = -1;  // 0 à 100, -1 inconnu
    int8_t muet = -1;  // 1, 0, -1 inconnu
    int groupe = -1;  // index (rang) du meneur de son groupe, -1 seule
    uint8_t fonctions = 0;
    Champ titre{nullptr, 0};
};
struct MusiquePiecesLues {
    bool ma = false;  // Music Assistant chargé dans HA
    int montre = -1;  // index (rang) du lecteur montré, -1 absent
    int file = 0;     // titres de sa file, 0 inconnu
    Champ suivant{nullptr, 0};
    int n = 0;
    MusiquePieceLue pieces[kMusiquePiecesMax];
};
// Lit les deux variables ; renvoie le nombre de pièces (out.n). Un enregistrement vide
// (« ;; », le « ; » final) est sauté mais garde son rang ; au-delà du rang
// kMusiquePiecesMax - 1, la fin est ignorée. Un index (montre, groupe) qui ne désigne
// aucune pièce lue vaut -1 ; un nombre illisible, non fini ou hors bornes vaut inconnu ;
// un contexte vide ou illisible : sans Music Assistant, rien de montré.
int musique_pieces_lire(const Champ& pieces, const Champ& contexte, MusiquePiecesLues& out);
// Pièces du groupe que mène la pièce de rang `index`, elle exceptée (« + n » de l'en-tête) ;
// 0 si elle ne mène pas de groupe.
int musique_membres(const MusiquePiecesLues& p, int index);
// Les pièces de rangs a et b sont-elles dans le même groupe (a != b) ?
bool musique_meme_groupe(const MusiquePiecesLues& p, int a, int b);
// La pièce de rang `index`, nullptr si aucune.
const MusiquePieceLue* musique_piece(const MusiquePiecesLues& p, int index);

enum class MusiqueVue : uint8_t {
    INCONNUE,
    FAVORIS,    // fav
    PLAYLISTS,  // pl
    ALBUMS,     // alb
    ARTISTES,   // art
    RADIOS,     // rad
    PODCASTS,   // pod
    RECENTS,    // rec
    DETAIL,     // det : le contenu d'un artiste, d'un album, d'une playlist, d'un podcast
    SOURCES,    // src : les sources d'une pièce
    RECHERCHE,  // rech (lot suivant)
    ERREUR,     // err : titre = la phrase à montrer
    NB
};
// Codes de l'événement (« parcourir » : vue|page) et de la poussée, lus par HA : jamais
// traduits.
constexpr const char* kMusiqueVueCodes[static_cast<int>(MusiqueVue::NB)] = {
    "", "fav", "pl", "alb", "art", "rad", "pod", "rec", "det", "src", "rech", "err"};
MusiqueVue musique_vue_lire(const Champ& c);
const char* musique_vue_code(MusiqueVue v);
// Les sept catégories de la bibliothèque (FAVORIS à RECENTS) : celles qu'on parcourt.
bool musique_vue_categorie(MusiqueVue v);

enum class MusiqueElementGenre : uint8_t {
    AUTRE,     // x
    TITRE,     // t
    ALBUM,     // a
    ARTISTE,   // r
    PLAYLIST,  // p
    RADIO,     // d
    PODCAST,   // o : podcast ou épisode (l'URI le dit, musique_element_jouable)
    LIVRE,     // l : livre audio
    SOURCE,    // s : une source de la pièce (ref = son rang)
};
struct MusiqueElementLu {
    Champ nom{nullptr, 0};
    Champ sous{nullptr, 0};
    MusiqueElementGenre genre = MusiqueElementGenre::AUTRE;
    Champ ref{nullptr, 0};  // vide si refusée (musique_ref_valide)
    Champ image{nullptr, 0};
};
struct MusiqueListeLue {
    MusiqueVue vue = MusiqueVue::INCONNUE;
    Champ titre{nullptr, 0};
    int page = 0;
    bool suite = false;
    bool retour = false;
    int n = 0;
    MusiqueElementLu elements[kMusiqueElementsMax];
};
// Lit les deux variables ; renvoie le nombre d'éléments (out.n). Un enregistrement vide est
// sauté (la ref de l'élément revient à HA, pas son rang) ; au-delà de kMusiqueElementsMax,
// la fin est ignorée. Une page illisible ou négative vaut 0.
int musique_liste_lire(const Champ& contexte, const Champ& elements, MusiqueListeLue& out);
// Une ref que la tablette peut renvoyer : pour une source, son rang (1 à 3 chiffres) ;
// sinon « schéma://reste » (schéma en a-z, 0-9, « _ », « - » ; reste sans « | », « ; », espace
// ni caractère de contrôle), kMusiqueRefMax octets au plus — la même règle que HA.
bool musique_ref_valide(const Champ& ref, MusiqueElementGenre g);
// Ce que fait le toucher d'un élément : jouer (titre, radio, livre, épisode, autre) ou
// ouvrir (album, artiste, playlist, podcast). Un épisode est un « o » dont l'URI contient
// « episode » (library://podcast_episode/…). Une source : ni l'un ni l'autre.
bool musique_element_jouable(const MusiqueElementLu& e);
bool musique_element_conteneur(const MusiqueElementLu& e);

// ─── 10. Popup Caméras (tab5_maj_cameras, ADR-0049, ADR-0057) ───
// Variable cameras : « nom|image|pièce|hors_ligne;… » dans l'ordre du blueprint :
//   nom        friendly_name (HA remplace « | » et « ; ») ;
//   image      l'attribut entity_picture (« /api/camera_proxy/camera.x?token=… », chemin
//              relatif à Home Assistant) ou une URL complète ;
//   pièce      area_name() de la caméra (ADR-0057), vide sans pièce ; absente chez un
//              blueprint d'avant (« nom|image ») : vide aussi ;
//   hors_ligne horodatage Unix (secondes) depuis lequel HA la dit « unavailable », vide ou
//              0 = en ligne ; absent chez un blueprint d'avant.
// Un firmware d'avant lit « nom|image » et ignore la suite. La base de HA vient de
// ha_base_depuis_hote() (section 9), comme les pochettes du lecteur.
constexpr int kCamerasMax = 16;
constexpr size_t kCameraNomMax = 48;      // copié par l'écran (texte_ha_copier)
constexpr size_t kCameraPieceMax = 48;    // idem
constexpr size_t kCameraImageMax = 256;   // chemin + jeton : ~110 octets chez HA
constexpr size_t kCameraUrlMax = 384;     // base + image + « &width=…&height=… »

struct CameraLue {
    Champ nom{nullptr, 0};
    Champ image{nullptr, 0};
    Champ piece{nullptr, 0};
    uint32_t hors_ligne = 0;   // horodatage Unix, 0 : en ligne
    int8_t piece_i = 0;        // rang de sa pièce parmi les pièces distinctes (cameras_lire)
};

// Au plus kCamerasMax caméras, dans l'ordre. Un enregistrement sans image ni hors_ligne
// (vide, « ;; », « nom| ») est sauté ; un nom vide est gardé (l'écran n'en montre aucun).
// Pièces : chaque caméra reçoit `piece_i`, le rang de sa pièce parmi les pièces distinctes
// (même texte à l'octet près), dans l'ordre de leur première caméra ; la pièce vide (caméra
// sans pièce) vient en dernier. `*pieces` (s'il est donné) reçoit leur nombre, la pièce
// vide comprise (0 sans caméra, 1 si toutes sont dans la même pièce ou sans pièce).
// Renvoie le nombre de caméras lues.
int cameras_lire(const Champ& payload, CameraLue cameras[kCamerasMax], int* pieces = nullptr);

// URL à télécharger : celle de ha_image_url() (section 9) ; pour le proxy des caméras
// (/api/camera_proxy/), « width=L&height=H » ajoutés (« ? » ou « & ») s'ils n'y sont pas et
// que L et H sont positifs : HA réduit alors l'image avant de l'envoyer. Faux (out vidé)
// dans les cas de ha_image_url() ou si la taille ne tient pas dans `n`.
bool camera_url(const Champ& image, const char* base, int largeur, int hauteur, char* out, size_t n);

// ─── 11. Suivi de capteurs (tab5_maj_suivi, ADR-0054) ───
// Une variable, poussée par packages/tab5_suivi.yaml : un enregistrement par capteur de la
// liste « Tab5 · capteurs suivis », dans son ordre, séparés par « ; » (kSuivisMax au plus ;
// vide : aucun capteur choisi) :
//   « nom|valeur|unité|variation|genre|points »
//   nom       friendly_name (HA retire « | » et « ; »), copié par l'écran (texte_ha_copier) ;
//   valeur    l'état du capteur tel que HA l'écrit (« 382.7 », « 7803.3301 ») ; illisible,
//             non fini ou au-delà de ±kSuiviValeurMax : inconnue (« -- ») ;
//   unité     unit_of_measurement, vide sans ;
//   variation et genre : « p » = pourcentage du jour (attribut change_pct de l'entité),
//             « a » = écart depuis le premier point de la courbe, dans l'unité du capteur ;
//             un autre genre, ou une variation illisible : aucune ;
//   points    la courbe des 24 dernières heures, du plus ancien au plus récent, séparés par
//             « , » : chacun de 0 (minimum de la fenêtre) à 100 (maximum), vide = aucune
//             mesure à ce créneau ; kSuiviPointsMax au plus, la fin ignorée.
// Un enregistrement vide (« ;; ») est sauté ; moins de deux champs : sauté aussi.
constexpr int kSuivisMax = 6;
constexpr int kSuiviPointsMax = 32;         // = kCourbeLissePoints (tab5_internal.h)
constexpr int kSuiviDecimalesMax = 3;
constexpr float kSuiviValeurMax = 1.0e12f;  // au-delà, un payload faux, pas une mesure
// La valeur est un float : 24 bits de mantisse, entiers exacts jusqu'à 2^24 (16 777 216).
// Au-delà, les décimales et les derniers chiffres ne sont plus ceux de HA : la valeur
// s'écrit alors sans décimale (suivi_nombre_texte), arrondie à la précision du float.
constexpr float kSuiviValeurExacteMax = 16777216.0f;
constexpr int8_t kSuiviPointAucun = -1;

enum class SuiviVariation : uint8_t {
    AUCUNE,
    POURCENT,  // « p » : pourcentage du jour (change_pct)
    ECART,     // « a » : écart depuis le début de la courbe, dans l'unité du capteur
};

struct SuiviLu {
    Champ nom{nullptr, 0};
    Champ unite{nullptr, 0};
    float valeur = NAN;
    // Décimales à montrer : celles écrites par HA, 0 à kSuiviDecimalesMax ; au-delà, la
    // valeur arrondie à kSuiviDecimalesMax sans ses zéros de fin (« 7803.3301 » : 2).
    int decimales = 0;
    float variation = NAN;
    SuiviVariation genre = SuiviVariation::AUCUNE;
    int n = 0;  // points lus, manquants compris
    int8_t points[kSuiviPointsMax] = {};  // 0 à 100, kSuiviPointAucun : aucune mesure
};
// Renvoie le nombre de capteurs lus (kSuivisMax au plus), dans l'ordre.
int suivis_lire(const Champ& payload, SuiviLu out[kSuivisMax]);

// Chiffres après le point d'un nombre écrit par HA (« 382.7 » : 1, « 1e3 » : 0), sans
// borne ; 0 sans point ou pour un champ vide.
int suivi_decimales(const Champ& c);

// « 382.7 », « 7803.33 » : `decimales` (bornées à 0..kSuiviDecimalesMax) après le point,
// aucune au-delà de ±kSuiviValeurExacteMax ; « 0.00 » sans signe quand elle s'arrondit à
// zéro (jamais « -0.00 ») ; « -- » pour une valeur inconnue, non finie ou au-delà de
// ±kSuiviValeurMax. Faux si `out` est trop petit (le texte est alors vidé).
bool suivi_nombre_texte(float v, int decimales, char* out, size_t n);

// Variation à montrer : « +2.05 % », « -0.95 % » (POURCENT, deux décimales), « +1.2 »
// (ECART, aux décimales de la valeur ; l'écran ajoute l'unité) ; « 0.00 % »
// sans signe quand elle s'arrondit à zéro ; vide sans variation. Faux si `out` est trop
// petit (vidé).
bool suivi_variation_texte(const SuiviLu& s, char* out, size_t n);

// Sens de la variation arrondie comme elle s'affiche : 1 hausse, -1 baisse, 0 stable ou
// inconnue (flèche, couleur de la variation et de la courbe).
int suivi_sens(const SuiviLu& s);

// ─── 12. Froid : réfrigérateurs et congélateurs (tab5_maj_froid, ADR-0055) ───
// Une variable, poussée par packages/tab5_froid.yaml : un enregistrement par appareil des
// listes « Tab5 · réfrigérateurs » et « Tab5 · congélateurs » (kFroidMax au plus, dans
// l'ordre ; vide : aucun appareil déclaré), séparés par « ; », quinze champs « | » :
//   « nom|type|valeur|niveau|cause|depuis|min|max|bas|haut|points|icause|idebut|iduree|imax »
//   nom      friendly_name du capteur (HA retire « | » et « ; »), copié par l'écran ;
//   type     f (réfrigérateur) ou c (congélateur) ; autre : enregistrement sauté ;
//   valeur   température actuelle en °C, vide si le capteur est indisponible ;
//   niveau   0 conforme, 1 attention, 2 grave (au-delà de 2 : 2 ; illisible : 0) ;
//   cause    ok, chaud, froid, porte, indispo (autre : ok) — la détection est dans HA ;
//   depuis   epoch du début de la cause en cours, 0 sans ;
//   min, max la plus basse et la plus haute des dernières 24 h, en °C (vides : inconnues) ;
//   bas, haut la norme de l'appareil en °C, telle que HA la pousse (bas vide : pas de
//            limite basse, le congélateur) : la tablette n'en code aucune ;
//   points   24 moyennes horaires puis la valeur actuelle, en °C, séparées par « , » ;
//            vide = heure sans mesure ; kFroidPointsMax au plus, la fin ignorée ;
//   icause…  le DERNIER incident terminé : sa cause (ok ou vide : aucun), son début
//            (epoch), sa durée en minutes et la température la plus haute atteinte.
// Températures hors de ±kFroidTempMax, illisibles ou non finies : inconnues (NAN).
// Un enregistrement vide (« ;; ») ou de moins de deux champs est sauté.
constexpr int kFroidMax = 4;
constexpr int kFroidPointsMax = 32;  // = kCourbeLissePoints (tab5_internal.h)
constexpr float kFroidTempMax = 80.0f;

enum class FroidType : uint8_t {
    FRIGO,        // « f » : réfrigérateur
    CONGELATEUR,  // « c »
};
enum class FroidCause : uint8_t {
    OK,
    CHAUD,    // trop chaud (moyenne de 15 min), ou coup de chaud au niveau 2
    FROID,    // trop froid (risque de gel, réfrigérateur)
    PORTE,    // montée rapide : porte ouverte ? (1), porte mal fermée (2)
    INDISPO,  // capteur muet depuis plus de 30 min
};
struct FroidIncident {
    FroidCause cause = FroidCause::OK;  // OK : aucun incident
    uint32_t debut = 0;
    uint32_t duree_min = 0;
    float max = NAN;
};
struct FroidLu {
    Champ nom{nullptr, 0};
    FroidType type = FroidType::FRIGO;
    float valeur = NAN;
    uint8_t niveau = 0;
    FroidCause cause = FroidCause::OK;
    uint32_t depuis = 0;
    float min = NAN;
    float max = NAN;
    float bas = NAN;
    float haut = NAN;
    int n = 0;                         // points lus, manquants compris
    float points[kFroidPointsMax] = {};  // NAN : heure sans mesure
    FroidIncident dernier;
};
// Renvoie le nombre d'appareils lus (kFroidMax au plus), dans l'ordre.
int froid_lire(const Champ& payload, FroidLu out[kFroidMax]);

// Cause écrite par HA (« porte ») ; inconnue ou vide : OK.
FroidCause froid_cause(const Champ& c);

// « 9.1 », « -18.0 » (une décimale, jamais « -0.0 ») ; « -- » pour une température
// inconnue. Faux si `out` est trop petit (vidé).
bool froid_temperature_texte(float v, char* out, size_t n);

// Bornes de l'axe vertical de la courbe d'un appareil : ses points, sa valeur et sa norme
// (bas et haut) y tiennent, avec une marge ; au moins kFroidEchelleMin degrés. Faux sans
// rien à tracer (ni point ni norme connus).
constexpr float kFroidEchelleMin = 4.0f;
bool froid_echelle(const FroidLu& a, float& bas, float& haut);

// Niveau le plus grave des appareils lus (0 sans appareil) : 2 allume l'icône qui clignote
// dans le coin de l'horloge.
uint8_t froid_niveau_max(const FroidLu* a, int n);

// Libellé codé d'une alerte « froid » (packages/tab5_froid.yaml → tab5_alertes.jinja) :
// « @froid:cause:niveau:valeur:nom », lu après « @froid: » (AlerteTexteCode::FROID) ; le
// nom est le reste (il peut contenir « : »). Valeur vide : inconnue. Faux si le texte n'a
// pas ses quatre champs (l'écran le montre alors tel quel).
struct FroidAlerteLu {
    FroidCause cause = FroidCause::OK;
    uint8_t niveau = 0;
    float valeur = NAN;
    const char* nom = "";
};
bool froid_alerte_lire(const char* reste, FroidAlerteLu& out);

// ─── 13. Télécommandes (clé « telecommandes » de tab5_maj_emplacements, ADR-0056) ───
// « telecommandes|écran|nom|écran|nom… » : les télécommandes choisies dans le blueprint
// (« Télécommande de la TV », puis « Autres télécommandes »), kTelecommandesMax au plus,
// dans l'ordre. Une page du popup chacune. Écran = la disposition des touches :
//   tv       Source, Muet, rangée d'applications (Netflix… : script tab5_tv_app de HA) ;
//   boitier  Stop, rangée de lecture (précédent, reculer, lecture/pause, avancer, suivant),
//            sans Muet ni applications (Apple TV, Freebox Player par l'intégration Apple TV).
// Un code inconnu (blueprint plus récent) vaut tv, la disposition d'avant l'ADR. Seules les
// paires complètes comptent ; vide (« telecommandes| ») : aucune télécommande connue, le
// popup garde sa page unique d'avant. Les codes sont lus par le blueprint : ni traduits ni
// renommés sans lui (tests/test_telecommandes.py).
constexpr int kTelecommandesMax = 4;
enum class TelecommandeEcran : uint8_t {
    TV,
    BOITIER,
    NB
};
constexpr const char* kTelecommandeEcranCodes[static_cast<int>(TelecommandeEcran::NB)] = {"tv", "boitier"};
struct TelecommandeLue {
    TelecommandeEcran ecran = TelecommandeEcran::TV;
    Champ nom{nullptr, 0};  // copié par l'écran (texte_ha_copier) ; vide : un nom par défaut
};
// Renvoie le nombre de télécommandes lues (0 à kTelecommandesMax).
int telecommandes_lire(const char* valeur, size_t n, TelecommandeLue out[kTelecommandesMax]);

// Emplacement de l'événement esphome.tab5_action de la télécommande `i` : « tv » pour la
// première (l'emplacement d'avant l'ADR : un blueprint plus ancien la commande toujours),
// « tv1 », « tv2 », « tv3 » pour les suivantes ; « tv » hors de 0..kTelecommandesMax - 1.
const char* telecommande_emplacement(int i);

// ─── 14. Serveur IA : un serveur de LLM local (tab5_maj_serveur_ia, ADR-0059) ───
// Une variable, poussée par packages/tab5_serveur_ia.yaml (deux secondes au moins entre
// deux poussées) : UN enregistrement de quatorze champs « | » — le format admet d'autres
// enregistrements après un « ; », que ce firmware ignore (un serveur, ADR-0059) ; vide :
// aucun capteur choisi dans les listes « Tab5 · serveur IA ».
//   « nom|etat|modele|tps|cours|file|vram|vram_total|vram_pct|temp|niveau|ram|puissance|actions »
//   nom        nom de l'appareil du serveur dans HA (vide : l'écran n'en montre pas) ;
//   etat       1 en ligne, 0 hors ligne, autre ou vide : inconnu ;
//   modele     le modèle chargé, tel que HA l'écrit (vide : aucun) ;
//   tps        tokens par seconde ; cours, file : requêtes en cours, en file (entiers) ;
//   vram, vram_total  en Go ; vram_pct  0 à 100 (HA le calcule, ou le lit d'un capteur en %) ;
//   temp       température du GPU en °C ; niveau 0 normale, 1 élevée, 2 critique (les
//              seuils sont dans HA, la tablette n'en code aucun ; au-delà de 2 : 2 ;
//              vide, illisible ou négatif : kServeurIaNiveauInconnu) ;
//   ram        RAM utilisée en % ; puissance  en W ;
//   actions    (ADR-0060, 14e champ) les boutons d'action du popup, codes séparés par
//              « , » parmi kServeurIaActionCodes : « code » = bouton actif, « -code » =
//              bouton grisé (réglé dans HA mais sans objet maintenant : réveiller un
//              serveur en ligne…), code absent = pas de bouton ; un code inconnu est
//              ignoré. Absent (HA d'avant l'ADR-0060) : aucun bouton. Un firmware d'avant
//              ignore ce champ (champs_decouper s'arrête à treize).
// Un champ vide, illisible, non fini ou hors de ses bornes est INCONNU (NAN, ou -1 pour
// les requêtes) : l'écran écrit « — », jamais un zéro inventé.
constexpr size_t kServeurIaMax = 1024;      // payload : un serveur fait ~200 octets
constexpr float kServeurIaTpsMax = 1.0e6f;  // tokens/s au-delà : un payload faux
constexpr uint32_t kServeurIaRequetesMax = 1000000u;
constexpr float kServeurIaVramMax = 1.0e5f;  // Go
constexpr float kServeurIaTempMax = 200.0f;  // °C
constexpr float kServeurIaPuissanceMax = 1.0e5f;  // W
constexpr uint8_t kServeurIaNiveauInconnu = 255;   // niveau que HA n'a pas donné
// Actions sur le serveur (ADR-0060) : index = rang du bouton dans le bandeau et bit de
// ServeurIaLu::actions / actives. Le code est aussi le champ `action` de l'événement
// esphome.tab5_serveur_ia_action (packages/tab5_llm.yaml, script tab5_serveur_ia_action).
constexpr int kServeurIaActions = 3;
constexpr int kServeurIaDecharger = 0;   // décharger le(s) modèle(s) : libère la VRAM
constexpr int kServeurIaReveiller = 1;   // réveiller le PC (Wake-on-LAN)
constexpr int kServeurIaRedemarrer = 2;  // redémarrer le service (script ou bouton choisi dans HA)
constexpr const char* kServeurIaActionCodes[kServeurIaActions] = {"decharger", "reveiller", "redemarrer"};
struct ServeurIaLu {
    Champ nom{nullptr, 0};     // copiés par l'écran (texte_ha_copier)
    Champ modele{nullptr, 0};
    int8_t en_ligne = -1;      // 1, 0, -1 inconnu
    float tps = NAN;
    int32_t en_cours = -1;     // -1 : inconnu
    int32_t file = -1;
    float vram = NAN;
    float vram_total = NAN;
    float vram_pct = NAN;      // 0 à 100
    float temperature = NAN;
    uint8_t niveau = kServeurIaNiveauInconnu;  // de la température : 0, 1, 2, ou inconnu
    float ram = NAN;           // 0 à 100
    float puissance = NAN;
    uint8_t actions = 0;       // bit i (kServeurIaActionCodes[i]) : bouton montré
    uint8_t actives = 0;       // bit i : bouton actif (montré sans ce bit : grisé)
};
// Faux sans enregistrement (payload vide ou « ; » seul) : aucun serveur choisi.
bool serveur_ia_lire(const Champ& payload, ServeurIaLu& out);

// Nombre d'un champ du serveur : `decimales` (0 à 3) après le point ; « — » s'il est
// inconnu (non fini) ; jamais « -0 ». Faux si `out` est trop petit (vidé).
bool serveur_ia_nombre_texte(float v, int decimales, char* out, size_t n);

// La courbe des tokens/s : les kServeurIaPoints dernières valeurs poussées, la plus
// ancienne d'abord (un point par poussée, deux secondes au moins entre deux : environ la
// dernière minute quand le serveur travaille). Une valeur inconnue y entre aussi (un
// trou dans la courbe) ; gardée par l'écran, la tablette ne demande rien.
constexpr int kServeurIaPoints = 24;
struct ServeurIaCourbe {
    float v[kServeurIaPoints] = {};
    int n = 0;
    // Ajoute `x` à la fin ; au-delà de kServeurIaPoints, la plus ancienne sort.
    void ajouter(float x);
    // Haut de l'axe : la plus grande valeur connue, 10 % d'air, au moins 1 ; faux sans
    // valeur connue.
    bool haut(float& h) const;
};

// ─── 15. Énergie : soleil, prévision et bilan (ADR-0058) ───
// Pages « Flux », « Aujourd'hui » et « Bilan » du popup Énergie (tab5_energie.cpp). Les
// codes et l'ordre des champs sont un contrat avec packages/tab5_energie.yaml, la démo et
// le rendu : ni renommés ni réordonnés sans eux.
constexpr int kEnergieHeures = 24;      // créneaux d'une journée, heures locales
// 24 heures, 30 jours glissants ou 12 mois ; 31 : les jours du mois civil du bilan
// (ADR-0062).
constexpr int kEnergieSlotsMax = 31;
// Une énergie de créneau ou de journée au-delà (kWh) ou négative est fausse : inconnue.
constexpr float kEnergieKwhMax = 1e6f;
// Une puissance (W) au-delà est fausse : inconnue (100 MW : bien au-delà d'une maison).
constexpr float kEnergieWMax = 1e8f;

// tab5_maj_energie_soleil :
//   « lever|midi|coucher|prevu_jour|prevu_demain|source|creneau_debut|creneau_fin|prevu|clair »
//   lever, midi, coucher  « HH:MM » locales strictes (hhmm_minutes) ; lever et coucher
//                         illisibles, ou coucher pas après lever : soleil inconnu (les trois
//                         à -1, pas d'arc) ; midi hors de ]lever, coucher[ : inconnu seul ;
//   prevu_jour, _demain   kWh ; champ vide = pas de prévision (non choisie), « nan » ou
//                         illisible = choisie sans valeur ;
//   source                « a » apprise × météo, « e » externe, autre ou vide : aucune ;
//   creneau_debut, _fin   heures entières 0..24, fin exclue ; illisibles, ou fin pas après
//                         début : aucun créneau (les deux à -1) ;
//   prevu, clair          24 valeurs en kWh au plus, « ; » entre deux ; un champ vide (heure
//                         sans valeur) compte et vaut NAN, le dernier aussi (« 1;; » = trois) ;
//                         n = 0 si le champ est vide.
enum class EnergieSource : uint8_t {
    AUCUNE,
    APPRISE,  // « a » : courbe apprise × nuages de la météo (package)
    EXTERNE,  // « e » : courbe apprise mise à l'échelle d'une prévision externe
};
struct EnergieSoleilLu {
    int lever = -1, midi = -1, coucher = -1;  // minutes depuis minuit, -1 inconnue
    bool prevu_jour_choisi = false, prevu_demain_choisi = false;
    float prevu_jour = NAN, prevu_demain = NAN;
    EnergieSource source = EnergieSource::AUCUNE;
    int creneau_debut = -1, creneau_fin = -1;  // heures, fin exclue ; -1 : aucun
    int n_prevu = 0, n_clair = 0;
    float prevu[kEnergieHeures] = {};
    float clair[kEnergieHeures] = {};
};
// Vrai si la page « Aujourd'hui » a de quoi se montrer : le soleil, une prévision par heure
// ou la courbe apprise (ADR-0058).
bool energie_soleil_lire(const Champ& payload, EnergieSoleilLu& out);

// Liste de kWh d'un champ (« ; » entre deux) dans `out`, `max` valeurs au plus : la boucle
// d'energie_historique (tab5_energie.cpp) — un champ vide compte et vaut NAN, le dernier
// aussi ; négative, illisible ou au-delà de kEnergieKwhMax : NAN. Renvoie le nombre lu, 0
// pour un champ vide.
int energie_kwh_liste(const Champ& c, float* out, int max);

// tab5_maj_energie_bilan, variable payload : « devise|vente|achat|gain|prod|conso|estime ».
//   devise   symbole tel quel, 7 octets au plus (au-delà : aucun), copié par l'écran ;
//   vente, achat, gain  listes de energie_kwh_liste (gain : dans la devise, peut être
//            négatif) ; champ vide = compteur ou prix non choisi ;
//   prod, conso  (ADR-0062, ajoutés en fin : un firmware d'avant s'arrête à gain, un HA
//            d'avant ne les envoie pas) production et consommation de la maison par
//            créneau, listes de energie_kwh_liste ; prod vide = la production est la série
//            de tab5_maj_energie_historique de la même vue (HA d'avant), conso vide = pas
//            de compteur de la maison (consommé = autoconsommé + acheté) ;
//   estime   « 1 » : vente et achat sont estimés par HA heure par heure depuis la
//            production et la consommation (aucun compteur d'achat ni de vente) ; autre
//            ou vide : mesurés.
constexpr size_t kEnergieDeviseMax = 7;
struct EnergieBilanLu {
    Champ devise{nullptr, 0};
    bool vente_choisie = false, achat_choisi = false, gain_choisi = false;
    bool prod_choisie = false, conso_choisie = false, estime = false;
    int n_vente = 0, n_achat = 0, n_gain = 0, n_prod = 0, n_conso = 0;
    float vente[kEnergieSlotsMax] = {};
    float achat[kEnergieSlotsMax] = {};
    float gain[kEnergieSlotsMax] = {};
    float prod[kEnergieSlotsMax] = {};
    float conso[kEnergieSlotsMax] = {};
};
// `slots` : 24, 30, 31 ou 12 (la vue) ; au-delà, la fin des listes est ignorée. Vrai si la
// page « Bilan » a de quoi se montrer : vente ou achat choisi, ou production et
// consommation (ADR-0062).
bool energie_bilan_lire(const Champ& payload, int slots, EnergieBilanLu& out);

// Un créneau du bilan, `produit` = la production du créneau (champ prod du bilan, sinon la
// série de tab5_maj_energie_historique de la même vue), NAN inconnue. Autoconsommé =
// produit − vendu (jamais sous 0 ; vente non choisie ou inconnue sur le créneau : 0) ;
// consommé = la consommation mesurée (champ conso), sinon autoconsommé + acheté (NAN sans
// compteur d'achat) ; l'autoconsommé ne dépasse jamais le consommé connu. Net = produit −
// consommé (ADR-0062 : positif, la maison a donné au réseau plus qu'elle n'en a pris).
// Toutes les valeurs NAN si inconnues.
struct EnergieBilanCreneau {
    float produit = NAN, autoconsomme = NAN, vendu = NAN, achete = NAN, consomme = NAN, gain = NAN;
    float net = NAN;
};
EnergieBilanCreneau energie_bilan_creneau(float produit, const EnergieBilanLu& b, int k);

// Date du créneau k d'une vue qui commence le (annee, mois, jour) : `k` jours plus tard
// (vue jours) ; a, m, j inchangés pour k ≤ 0. Mois de 1 à 12 attendu.
void energie_jour_plus(int annee, int mois, int jour, int k, int& a, int& m, int& j);

// Vues du bilan (ADR-0062), dans l'ordre de tab5_energie.cpp.
enum class EnergieVue : uint8_t {
    HEURES = 0,
    JOURS = 1,
    MOIS = 2,
};

// Cumul du net, créneau par créneau (ADR-0062) : la vue des heures cumule la journée, celle
// des jours repart de 0 au 1er du mois, celle des mois en janvier (une fenêtre glissante
// d'un HA d'avant change de mois ou d'année en route). Un net inconnu au milieu garde le
// cumul ; après le dernier net connu, NAN. `cumul` tient `n` valeurs. Renvoie l'indice du
// dernier net connu, -1 sans aucun.
int energie_cumul(const float* net, int n, EnergieVue vue, int annee, int mois, int jour, float* cumul);

// « Batterie réseau » (ADR-0062, compensation mensuelle) : le mois civil du dernier jour
// dont le net est connu, depuis les créneaux d'une vue des jours qui commence le (annee,
// mois, jour). Sommes des valeurs connues des jours de ce mois (NAN sans aucune) ;
// couverture = autoconsommé / consommé (0..1) ; rythme = net moyen des jours connus parmi
// les 7 qui précèdent le dernier (ce jour-là n'est pas fini), n_rythme leur nombre ; fin =
// net + rythme × jours restants après le dernier (NAN sans rythme).
struct EnergieMois {
    bool ok = false;
    int annee = 0, mois = 0, jour = 0, jours_mois = 0;
    float produit = NAN, consomme = NAN, net = NAN, autoconsomme = NAN, gain = NAN;
    float couverture = NAN;
    int n_rythme = 0;
    float rythme = NAN;
    float fin = NAN;
};
EnergieMois energie_mois(const EnergieBilanCreneau* c, int n, int annee, int mois, int jour);

// Nombre « rond » d'une échelle : le plus petit de 1, 1,5, 2, 2,5, 3, 4, 5, 6, 8 × 10^k qui
// atteint v (au moins 0,1 ; v non fini ou ≤ 0 : 1). Pas d'une graduation : le plus petit
// de 1, 2, 2,5, 5 × 10^k qui atteint v (même bornes).
float energie_rond(float v);
float energie_pas(float v);

// Échelles du graphique du bilan (ADR-0062), un zéro commun : barres du net de haut (kWh,
// surplus) à bas (kWh, déficits, en valeur absolue), le côté le plus court au quart de
// l'autre au moins (des déficits petits ont un axe plus court) ; cumul (et projection) de
// cumul_haut à cumul_bas, dans le même rapport. pas, cumul_pas : graduations (≈ 3 et 4 au-
// dessus du zéro). `cumul` et `projection` peuvent être nuls / NAN.
struct EnergieEchelle {
    float haut = 1.0f, bas = 0.25f, pas = 0.5f;
    float cumul_haut = 1.0f, cumul_bas = 0.25f, cumul_pas = 0.25f;
};
EnergieEchelle energie_echelle(const float* net, const float* cumul, int n, float projection);

// Totaux de la vue sur `n` créneaux (sommes des valeurs connues, NAN si aucune) et taux
// d'autoconsommation = autoconsommé / produit (0..1, NAN sans production).
struct EnergieBilanTotaux {
    float produit = NAN, autoconsomme = NAN, vendu = NAN, achete = NAN, consomme = NAN, gain = NAN;
    float taux = NAN;
};
EnergieBilanTotaux energie_bilan_totaux(const float* produit, int n, const EnergieBilanLu& b);

// Flux de l'instantané (page « Flux ») en W, depuis tab5_maj_energie : solaire ≥ 0, maison,
// réseau (+ achat, − vente), batterie (+ charge, − décharge) ; NAN = inconnue (0 pour le
// partage). Le solaire va d'abord à la vente, puis à la charge, le reste à la maison ; la
// charge que le solaire ne couvre pas vient du réseau ; la décharge va d'abord à la vente
// que le solaire ne couvre pas, le reste à la maison. maison = la mesure, sinon la somme de
// ce qui y entre (NAN si rien n'est connu).
struct EnergieFlux {
    float solaire_maison = 0.0f, solaire_reseau = 0.0f, solaire_batterie = 0.0f;
    float reseau_maison = 0.0f, reseau_batterie = 0.0f;
    float batterie_maison = 0.0f, batterie_reseau = 0.0f;
    float maison = NAN;
};
EnergieFlux energie_flux_calculer(float solaire, float maison, float reseau, float batterie);

// ─── 16. Playlists favorites (clé « playlists » de tab5_maj_emplacements, ADR-0068) ───
// « playlists|nom|nom|… » : les NOMS des playlists écrites dans le blueprint (entrée
// « Playlists favorites », une ligne « Nom | lien » chacune), kPlaylistsMax au plus, dans
// l'ordre. Leur rang est la valeur de l'événement esphome.tab5_lecteur (action
// « playlist ») : le blueprint, qui seul garde les liens, retrouve la ligne. Aucun lien ni
// aucune entité ne vient jusqu'à la tablette. Un nom vide garde sa place (l'écran écrit
// « Playlist N ») ; vide (« playlists| ») : aucune playlist, le popup Musique garde sa page
// unique — une seule playlist au nom vide se confond donc avec la liste vide : le
// blueprint écrit « Playlist N » pour un nom absent. Rien en NVS, comme les télécommandes (section 13) : le blueprint repousse la
// liste avec tous les états, à chaque connexion.
constexpr int kPlaylistsMax = 8;
constexpr size_t kPlaylistNomMax = 48;  // octets, zéro final non compris ; l'écran coupe avec « … »
// Renvoie le nombre de playlists lues (0 à kPlaylistsMax) ; out[i] pointe dans `valeur`
// (copié par l'écran, texte_ha_copier). Au-delà de kPlaylistsMax, la fin est ignorée.
int playlists_lire(const char* valeur, size_t n, Champ out[kPlaylistsMax]);
