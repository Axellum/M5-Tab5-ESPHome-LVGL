/**
 * [AI-CONTEXT]
 * @file tab5_champs.h
 * @role Lecture bornée des payloads poussés par Home Assistant (audit du 07/10/2026,
 *       lot L5) : découpe en champs sans copie ni écriture, nombre d'un champ, entier
 *       non signé d'un champ, et le plafond commun de taille d'un payload. Remplace les
 *       six variantes recopiées dans tab5_energie, tab5_historique, tab5_tuiles,
 *       tab5_reglables, tab5_clim (alors dans tab5_cards) et tab5_alertes.
 * @architecture_constraint Logique PURE, comme tab5_core : ni ESPHome ni LVGL. Compilée
 *       aussi sur PC par tools/test_tab5_socle.cpp (g++ en CI, job `python`). La
 *       journalisation d'un rejet (ESP_LOGW) vit hors d'ici : payload_refuse() et
 *       payload_trop_long(), tab5_internal.h.
 * @ai_instruction Avant d'écrire une boucle strchr / strtof sur un payload, passer par
 *       ces fonctions. Un champ ne se modifie pas : (début, longueur) dans le texte reçu.
 *       Un payload découpé EN PLACE (strtok_r, buffer de pile) a son outil à part :
 *       split_fields() de tab5_core.h.
 */
#pragma once
#include <cstddef>
#include <cstdint>

// Un champ d'un payload : [p, p + n) dans le texte reçu, sans zéro final.
struct Champ {
    const char* p;
    size_t n;
};

// Champ suivant de [p, fin) jusqu'à `sep` (exclu). Avance `p` juste après le séparateur,
// ou jusqu'à `fin` s'il n'y en a plus. Un champ vide (« a||b ») compte.
Champ champ_suivant(const char*& p, const char* fin, char sep);

// Découpe [s, s + n) sur `sep`, au plus `max` champs (vides compris : « a||b » en donne
// trois, « » en donne un, vide). Renvoie leur nombre.
// champs_decouper : au-delà de `max` champs, le reste est ignoré (le dernier champ gardé
// s'arrête au séparateur suivant).
// champs_decouper_reste : le dernier champ prend tout le reste, séparateurs compris (un
// libellé où HA aurait laissé un « | »).
int champs_decouper(const char* s, size_t n, char sep, Champ* out, int max);
int champs_decouper_reste(const char* s, size_t n, char sep, Champ* out, int max);

// Le champ vaut exactement `mot` (comparaison de longueur et d'octets).
bool champ_est(const Champ& c, const char* mot);

// Longueur maximale d'un nombre lu dans un champ (« -1234.5678 », « 1.5e-3 », ou un
// flottant de HA écrit en entier : « 21.299999999999997 »). Au-delà, le champ n'est pas
// un nombre : il n'est pas tronqué, il vaut le défaut.
constexpr size_t kChampNombreMax = 31;

// Nombre (point décimal, strtof) d'un champ ; `defaut` s'il est vide, plus long que
// kChampNombreMax, illisible (« unknown ») ou non fini (« nan », « inf », « 1e99 »).
// Des caractères après le nombre sont ignorés (« 21.5 °C » → 21.5), comme strtof.
float champ_nombre(const char* p, size_t n, float defaut);
inline float champ_nombre(const Champ& c, float defaut) { return champ_nombre(c.p, c.n, defaut); }

// Entier décimal non signé d'un champ ; `defaut` s'il est vide, plus long que 15
// caractères, illisible ou au-dessus de `max` (« -1 » se lit comme un très grand nombre,
// donc au-dessus).
uint32_t champ_entier(const char* p, size_t n, uint32_t max, uint32_t defaut);
inline uint32_t champ_entier(const Champ& c, uint32_t max, uint32_t defaut) {
    return champ_entier(c.p, c.n, max, defaut);
}

// Plafond commun d'un payload reçu par un service HA qui n'a pas de tampon à lui (16 Ko,
// bien au-delà des payloads réels, de quelques Ko au plus) : au-delà, c'est un payload
// faux, refusé en bloc et journalisé (payload_trop_long, tab5_internal.h) ; l'écran garde
// l'état d'avant. Les services à tampon fixe gardent leur plafond (prévisions 2048,
// alertes HA 1024, pluie 255 octets).
constexpr size_t kPayloadMax = 16 * 1024;

