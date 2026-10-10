/**
 * [AI-CONTEXT]
 * @file tab5_parse.cpp
 * @role Implémentation de tab5_parse.h : logique pure, compilée à l'identique dans le
 *       firmware, dans les tests hôte (tools/test_parse.cpp) et dans le harnais libFuzzer
 *       (tools/fuzz/fuzz_parse.cpp). Chaque fonction est la boucle de l'écran qu'elle
 *       remplace, déplacée telle quelle (lot F de l'audit du 30/09/2026), sauf les cinq
 *       défauts corrigés à part ensuite (commentaires « correctif du lot F »).
 */
#include "tab5_parse.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

// ─── 1. Prévisions ───
// Avant : parse_and_update_heures_bulk() et parse_and_update_jours_bulk(),
// Tab5/ecran/tab5_forecast.cpp. Tampon de pile plutôt qu'une copie std::string du payload
// (jusqu'à 2 048 octets, fragmentation de la SRAM).

namespace {
// Index d'un enregistrement de prévisions : strtol, comme l'atoi d'avant (blancs de tête
// sautés, signe lu, la fin ignorée : « 3x » = 3), mais faux si aucun chiffre n'est lu
// (« abc », vide) au lieu de 0, qui écrasait le premier créneau (correctif du lot F).
bool index_prevision(const char* s, long& idx) {
    char* bout = nullptr;
    idx = std::strtol(s, &bout, 10);
    return bout != s;
}

// Nombre d'une prévision : atof comme avant (un champ vide ou illisible vaut 0, ce que HA
// envoie déjà pour une valeur absente : `| float(0)`), refusé s'il n'est pas fini ou hors
// de [bas, haut] (« nan », « inf », « 1e99 », correctif du lot F). Bornes testées sur le
// double, avant la conversion en float.
bool nombre_prevision(const char* s, double bas, double haut, float& out) {
    const double v = std::atof(s);
    if (!std::isfinite(v) || v < bas || v > haut) return false;
    out = static_cast<float>(v);
    return true;
}
}  // namespace

int previsions_premier_creneau(const char* payload) {
    long idx = 0;
    if (!index_prevision(payload, idx) || idx < 0) return -1;
    return idx > 14 ? 15 : static_cast<int>(idx);
}

int previsions_heures_lire(const char* payload, HourForecastData heures[15]) {
    char buf[kPrevisionsMax + 1];
    strncpy(buf, payload, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* saveptr1 = nullptr;
    int ignores = 0;

    char* token = strtok_r(buf, ";", &saveptr1);
    while (token != nullptr) {
        char* parts[6];
        const int num_parts = split_fields(token, '|', parts, 6);

        if (num_parts >= 5) {
            long idx = 0;
            float temp = 0.0f, pluvio = 0.0f;
            if (!index_prevision(parts[0], idx)) {
                ignores++;
            } else if (idx >= 0 && idx < 15) {
                if (nombre_prevision(parts[3], kPrevisionTempMin, kPrevisionTempMax, temp) &&
                    nombre_prevision(parts[4], 0.0, kPrevisionPluieMax, pluvio)) {
                    heures[idx].heure_texte = parts[1];
                    heures[idx].condition = parts[2];
                    heures[idx].temp = temp;
                    heures[idx].pluvio = pluvio;
                } else {
                    ignores++;
                }
            }
        }
        token = strtok_r(nullptr, ";", &saveptr1);
    }
    return ignores;
}

int previsions_jours_lire(const char* payload, DayForecastData jours[15], int32_t& ancre) {
    char buf[kPrevisionsMax + 1];
    strncpy(buf, payload, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* saveptr1 = nullptr;
    int ignores = 0;

    char* token = strtok_r(buf, ";", &saveptr1);
    while (token != nullptr) {
        // Découper chaque token par '|' — in-place, pas de std::vector
        char* parts[10];  // 9 champs attendus + marge
        const int num_parts = split_fields(token, '|', parts, 10);

        if (num_parts >= 9) {
            long jour = 0;
            float tmin = 0.0f, tmax = 0.0f;
            if (!index_prevision(parts[0], jour)) {
                ignores++;
            } else if (jour >= 0 && jour < 15) {
                if (nombre_prevision(parts[3], kPrevisionTempMin, kPrevisionTempMax, tmin) &&
                    nombre_prevision(parts[4], kPrevisionTempMin, kPrevisionTempMax, tmax)) {
                    jours[jour].nom_jour = parts[1];
                    jours[jour].condition = parts[2];
                    jours[jour].tmin = tmin;
                    jours[jour].tmax = tmax;
                    jours[jour].est_repos = (parts[5][0] == '1');
                    jours[jour].est_dimanche = (parts[6][0] == '1');
                    jours[jour].est_passe = (parts[7][0] == '1');
                    jours[jour].heures_ouverture = parts[8];
                    // HA calcule l'index 0 sur SON « aujourd'hui » au moment du push : on
                    // date la case 0 avec le jour local de réception (écart possible
                    // seulement si le push chevauche minuit à la seconde près). Heure pas
                    // encore synchronisée → -1 : le réveil reste sur l'heure fixe jusqu'au
                    // push suivant (cycle /10 min) plutôt que de deviner.
                    if (jour == 0) ancre = local_day_number_today();
                } else {
                    ignores++;
                }
            }
        }
        token = strtok_r(nullptr, ";", &saveptr1);
    }
    return ignores;
}

// ─── 2. Vigilance ───
// Avant : le début de parse_and_update_vigilance(), Tab5/ecran/tab5_services.cpp.

void vigilance_lire(const char* payload, VigilanceLue& v) {
    // 1024 (était 512) : la phrase de vigilance peut être longue, un payload
    // complet dépassait parfois 512 et tronquait les derniers champs (#T165).
    strncpy(v.buf, payload, sizeof(v.buf) - 1);
    v.buf[sizeof(v.buf) - 1] = '\0';
    // split_fields garde les champs vides (« || ») : chaque champ reste à sa place. Avant
    // (strtok_r, jusqu'au lot F), un champ vide faisait remonter les suivants d'un cran
    // (R6 de l'audit du 30/09/2026). HA envoie toujours « Vert », jamais un champ vide :
    // ses payloads se lisent comme avant. Au-delà de 13 champs, le 13e s'arrête au « | »
    // suivant, comme avec strtok_r.
    char* parts[kVigilanceChamps];
    const int n = split_fields(v.buf, '|', parts, kVigilanceChamps);
    for (int i = 0; i < kVigilanceChamps; i++) v.champs[i] = i < n ? parts[i] : "";
}

NiveauVigilance vigilance_niveau(const char* s) {
    if (strcmp(s, "Jaune") == 0) return NiveauVigilance::JAUNE;
    if (strcmp(s, "Orange") == 0) return NiveauVigilance::ORANGE;
    if (strcmp(s, "Rouge") == 0) return NiveauVigilance::ROUGE;
    return NiveauVigilance::AUTRE;
}

int vigilance_actives(const VigilanceLue& v, VigilanceActive out[kVigilanceActivesMax]) {
    int n = 0;
    for (int i = 0; i < kVigilancePhenomenes && n < kVigilanceActivesMax; i++) {
        const char* state = v.champs[2 + i];
        if (strlen(state) == 0 || strcmp(state, "Vert") == 0 || strcmp(state, "unknown") == 0) continue;
        out[n++] = VigilanceActive{i, state};
    }
    return n;
}

// ─── 3. Alertes HA, historique des alertes, bandeau info ───
// Avant : la boucle de parse_and_update_ha_alerts_bulk(), le début de ha_alerte_texte()
// et de compose_info_code() (Tab5/ecran/tab5_central.cpp), la boucle
// d'alertes_historique_recu() (Tab5/ecran/tab5_alertes.cpp).

LecteurAlertesHa::LecteurAlertesHa(const char* payload) {
    strncpy(buf_, payload, sizeof(buf_) - 1);
    buf_[sizeof(buf_) - 1] = '\0';
}

AlerteHaJeton LecteurAlertesHa::suivant() {
    AlerteHaJeton j{AlerteHaType::FIN, 0, nullptr, nullptr, nullptr};
    char* token = strtok_r(premier_ ? buf_ : nullptr, ";", &save_);
    premier_ = false;
    if (token == nullptr) return j;
    // En-tête « @n:N » (alertes du 06/10/2026, lot 3) : HA a N alertes à lire en tout,
    // plus que les 4 bandeaux. Un jeton d'un seul champ : l'ancien firmware l'ignore.
    if (strncmp(token, "@n:", 3) == 0) {
        j.type = AlerteHaType::TOTAL;
        j.total = atoi(token + 3);
        return j;
    }
    char* parts[3];
    const int num_parts = split_fields(token, '|', parts, 3);
    if (num_parts < 3) {
        j.type = AlerteHaType::AUTRE;
        return j;
    }
    j.type = AlerteHaType::ALERTE;
    j.id = parts[0];
    j.niveau = parts[1];
    j.texte = parts[2];
    return j;
}

AlerteTexteLu alerte_texte_lire(const char* brut) {
    if (strncmp(brut, "@maj:", 5) == 0) return {AlerteTexteCode::MAJ, brut + 5, 0};
    if (strncmp(brut, "@indispo:", 9) == 0) return {AlerteTexteCode::INDISPO, brut + 9, atoi(brut + 9)};
    if (strncmp(brut, "@vigi:", 6) == 0) return {AlerteTexteCode::VIGI, brut + 6, 0};
    return {AlerteTexteCode::TEXTE, brut, 0};
}

int alertes_historique_lire(const char* payload, AlerteHistoriqueLue out[], int max, int& illisibles) {
    int nb = 0;
    illisibles = 0;
    const char* p = payload;
    while (*p != '\0' && nb < max) {
        const char* fin = strchr(p, ';');
        const size_t n = fin ? static_cast<size_t>(fin - p) : strlen(p);
        // « apparue|lue|terminée|gravité|libellé » : le libellé est le reste (il ne contient
        // ni « | » ni « ; », HA les remplace) ; une entrée sans ses cinq champs est ignorée.
        Champ f[5] = {};
        if (champs_decouper_reste(p, n, '|', f, 5) == 5) {
            AlerteHistoriqueLue& e = out[nb];
            e.apparue = champ_entier(f[0], kAlerteEpochMax, 0);
            e.lue = champ_entier(f[1], kAlerteEpochMax, 0);
            e.terminee = champ_entier(f[2], kAlerteEpochMax, 0);
            e.gravite = f[3].n > 0 ? f[3].p[0] : 'O';
            if (e.gravite != 'R' && e.gravite != 'J') e.gravite = 'O';
            e.texte = f[4];
            if (e.apparue != 0) nb++;
            else illisibles++;
        } else {
            illisibles++;
        }
        if (fin == nullptr) break;
        p = fin + 1;
    }
    return nb;
}

void info_code_lire(const char* apres_prefixe, InfoCodeLu& out) {
    snprintf(out.buf, sizeof(out.buf), "%s", apres_prefixe);
    char* f[6];
    const int n = split_fields(out.buf, '|', f, 6);
    auto champ = [&](int i) -> const char* { return i < n ? f[i] : ""; };
    out.nb_maj = atoi(champ(0));
    out.titre = champ(1);
    out.nb_err = atoi(champ(2));
    out.nb_indispo = atoi(champ(3));
    out.jaune = atoi(champ(4)) != 0;
    out.vigi = champ(5);
}

// ─── 4. Pluie ───
// Avant : rain_level() et la boucle d'update_rain_bars_bulk_ui() (tab5_services.cpp), le
// décodage d'update_rain_phrase_ui() (tab5_central.cpp).

int pluie_niveau(const std::string& intensite) {
    if (intensite.size() == 1 && intensite[0] >= '0' && intensite[0] <= '4') return intensite[0] - '0';
    if (intensite == "Pluie faible") return 1;
    if (intensite == "Pluie modérée") return 2;
    if (intensite == "Pluie forte") return 3;
    if (intensite == "Pluie très forte" || intensite == "Pluie trés forte") return 4;
    return 0;
}

int pluie_barres_lire(const char* payload, PluieBarre out[kPluieBarresMax]) {
    char buf[kPluieMax + 1];
    strncpy(buf, payload, sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    int n = 0;
    char* save = nullptr;
    for (char* rec = strtok_r(buf, ";", &save); rec != nullptr && n < kPluieBarresMax;
         rec = strtok_r(nullptr, ";", &save)) {
        char* sep = strchr(rec, '|');
        if (sep == nullptr) continue;
        *sep = '\0';
        out[n++] = PluieBarre{atoi(rec), pluie_niveau(std::string(sep + 1))};
    }
    return n;
}

PluiePhrase pluie_phrase_lire(const std::string& phrase) {
    PluiePhrase p{false, 0, 0};
    if (phrase.empty() || phrase[0] != '@') return p;
    p.code = true;
    // « @- » sans chiffre après le « - » : aucune source. « @-1,0 » (pas de données, envoyé
    // par HA) est un niveau négatif, lu par atoi ; avant le correctif du lot F, le « - »
    // seul suffisait et « Pas de données » ne s'affichait jamais.
    const bool chiffre = phrase.size() >= 3 && phrase[2] >= '0' && phrase[2] <= '9';
    if (phrase.size() >= 2 && phrase[1] == '-' && !chiffre) {
        p.niveau = -2;
        p.debut = 0;
    } else {
        p.niveau = atoi(phrase.c_str() + 1);
        const char* virgule = strchr(phrase.c_str(), ',');
        p.debut = virgule ? strtoll(virgule + 1, nullptr, 10) : 0;
    }
    return p;
}

// ─── 5. Calendrier ───
// Avant : cal_store_month_data(), cal_field_delim(), cal_hex_val(), le sscanf de
// cal_show_day_detail_loading() et la boucle de cal_render_day_detail()
// (Tab5/ecran/tab5_calendar.cpp).

bool calendrier_mois_lire(const std::string& annee, const std::string& mois, int& y, int& m) {
    y = atoi(annee.c_str());
    m = atoi(mois.c_str());
    return !(y < 2000 || y > 2100 || m < 1 || m > 12);
}

// n-ième champ d'une chaîne délimitée par un séparateur — champs vides autorisés
// (strtok_r fusionnerait les séparateurs consécutifs, donc parcours manuel).
std::string calendrier_champ(const std::string& s, int idx, char sep) {
    size_t start = 0;
    for (int i = 0; i < idx; i++) {
        const size_t p = s.find(sep, start);
        if (p == std::string::npos) return "";
        start = p + 1;
    }
    size_t end = s.find(sep, start);
    if (end == std::string::npos) end = s.size();
    return s.substr(start, end - start);
}

namespace {
int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}
}  // namespace

int calendrier_code_jour(const std::string& codes, int day) {
    // day < 1 : jamais demandé par l'écran (jours 1 à 31) ; 0 plutôt qu'un index négatif.
    if (day < 1) return 0;
    if ((int) codes.size() >= day * 2) return hex_val(codes[(day - 1) * 2]) * 16 + hex_val(codes[(day - 1) * 2 + 1]);
    return 0;
}

bool calendrier_date_lire(const char* date_iso, int& y, int& m, int& d) {
    return sscanf(date_iso, "%d-%d-%d", &y, &m, &d) == 3;
}

LecteurJourCalendrier::LecteurJourCalendrier(const char* payload) {
    strncpy(buf_, payload, sizeof(buf_) - 1);
    buf_[sizeof(buf_) - 1] = '\0';
}

CalJourLigne LecteurJourCalendrier::suivante() {
    CalJourLigne l{CalJourType::FIN, nullptr, nullptr};
    char* tok = strtok_r(premier_ ? buf_ : nullptr, ";", &save_);
    premier_ = false;
    if (tok == nullptr) return l;
    char* sep = strchr(tok, '|');
    if (sep == nullptr || *(sep + 1) == '\0') {
        l.type = CalJourType::AUTRE;
        return l;
    }
    *sep = '\0';
    l.type = CalJourType::LIGNE;
    l.genre = tok;
    l.texte = sep + 1;
    return l;
}

// ─── 6. Emplacements et zones ───
// Avant : le parcours d'emplacements_appliquer(), le découpage et le strtof de sa table
// 3.x, et la lecture de solaire_recu() (Tab5/ecran/tab5_zones.cpp).

bool emplacement_suivant(const std::string& payload, size_t& debut, EmplacementLu& e) {
    if (debut >= payload.size()) return false;
    size_t fin = payload.find(';', debut);
    if (fin == std::string::npos) fin = payload.size();
    const size_t p1 = payload.find('|', debut);
    e.a_cle = p1 != std::string::npos && p1 < fin;
    if (e.a_cle) {
        e.cle = Champ{payload.data() + debut, p1 - debut};
        e.reste = Champ{payload.data() + p1 + 1, fin - p1 - 1};
    } else {
        e.cle = Champ{payload.data() + debut, 0};
        e.reste = Champ{payload.data() + fin, 0};
    }
    debut = fin + 1;
    return true;
}

void emplacement_etat_valeur(const Champ& reste, Champ& etat, Champ& valeur) {
    const char* fin = reste.p + reste.n;
    const char* p = reste.p;
    etat = champ_suivant(p, fin, '|');
    const bool trois = etat.p + etat.n < fin;  // un second « | » dans le reste
    valeur = trois ? Champ{p, static_cast<size_t>(fin - p)} : Champ{fin, 0};
}

float emplacement_nombre(const std::string& valeur) {
    char* bout = nullptr;
    float v = strtof(valeur.c_str(), &bout);
    if (valeur.empty() || bout == valeur.c_str()) v = NAN;  // « unavailable »…
    return tab5_fini_ou_nan(v);  // « inf » : inconnue aussi (lot A, audit du 30/09)
}

float solaire_pourcent(const char* valeur, size_t n) {
    // champ_nombre (tab5_champs.h) : vide, illisible, non fini ou plus long que
    // kChampNombreMax (31 octets) = NAN. Avant le correctif du lot F, un tampon de 16
    // octets coupait le texte à 15 sans le dire (« 000000000000000099 » valait 0). HA
    // envoie un entier de 0 à 100 ou « nan » (blueprint, solaire_pourcent).
    float v = champ_nombre(valeur, n, NAN);
    if (!std::isnan(v)) v = v < 0.0f ? 0.0f : (v > 100.0f ? 100.0f : v);
    return v;
}

// Nouveau (ADR-0040), pas une extraction : écrit ici d'emblée, l'écran n'en garde que
// l'affichage (Tab5/ecran/tab5_piece_climat.cpp).
namespace {
bool mesure_lire(const Champ& f, float& v) {
    v = f.n == 0 ? NAN : champ_nombre(f, NAN);
    return f.n != 0;
}
}  // namespace

bool piece_climat_lire(const Champ& cle, const Champ& reste, PieceClimatLu& out) {
    if (cle.n != 2 || cle.p[0] != 'p' || cle.p[1] < '0' || cle.p[1] >= '0' + kPieces) return false;
    Champ f[3] = {};
    const int k = champs_decouper(reste.p, reste.n, '|', f, 3);
    out.piece = cle.p[1] - '0';
    out.temperature = k > 0 && mesure_lire(f[0], out.t);
    out.humidite = k > 1 && mesure_lire(f[1], out.h);
    if (!out.temperature) out.t = NAN;
    if (!out.humidite) out.h = NAN;
    out.clim = k > 2 && champ_est(f[2], "1");
    return true;
}

// Nouveau (ADR-0051), écrit ici d'emblée comme piece_climat_lire.
namespace {
int zone_gauche_code(const Champ& f) {
    for (int z = 0; z < static_cast<int>(ZoneGauche::NB); z++)
        if (champ_est(f, kZoneGaucheCodes[z])) return z;
    return -1;
}
}  // namespace

ZoneGaucheLu zone_gauche_lire(const char* valeur, size_t n) {
    ZoneGaucheLu out;
    Champ f[kZoneGaucheChampsMax] = {};
    const int k = champs_decouper(valeur, n, '|', f, kZoneGaucheChampsMax);
    const int d = k > 0 ? zone_gauche_code(f[0]) : -1;
    out.defaut = d >= 0 ? static_cast<ZoneGauche>(d) : kZoneGaucheDefaut;
    out.cycle = zone_gauche_bit(out.defaut);
    for (int i = 1; i < k; i++) {
        const int z = zone_gauche_code(f[i]);
        if (z >= 0) out.cycle |= zone_gauche_bit(static_cast<ZoneGauche>(z));
    }
    return out;
}

// ─── 7. Clim ───
// Avant : lire_reglages() et lire_etat() de Tab5/ecran/tab5_clim.cpp.

int clim_reglages_lire(const char* reste, size_t n, ClimReglages& r, Champ& nom) {
    Champ f[6] = {};
    const int k = champs_decouper_reste(reste, n, '|', f, 6);
    nom = Champ{reste + n, 0};
    if (k < 5) return k;
    const float mn = champ_nombre(f[0], r.min);
    const float mx = champ_nombre(f[1], r.max);
    // Bornes hors de toute clim réelle (« -1e30 ») ignorées : elles deviendraient celles
    // de l'arc (lot A de l'audit du 30/09/2026).
    if (mn < mx && mn >= kClimBorneBasse && mx <= kClimBorneHaute) {
        r.min = mn;
        r.max = mx;
    }
    const float pas = champ_nombre(f[2], r.pas);
    if (pas > 0.0f && pas <= 10.0f) r.pas = pas;
    // « °F » ou « °C » (UTF-8) : la dernière lettre suffit.
    r.fahrenheit = f[3].n > 0 && f[3].p[f[3].n - 1] == 'F';
    size_t j = 0;
    for (size_t i = 0; i < f[4].n && j + 1 < sizeof(r.capacites); i++)
        if (f[4].p[i] >= 'a' && f[4].p[i] <= 'z') r.capacites[j++] = f[4].p[i];
    r.capacites[j] = '\0';
    if (k == 6) nom = f[5];
    else r.nom[0] = '\0';
    r.recu = true;
    return k;
}

void clim_etat_lire(const char* reste, size_t n, ClimEtat& e) {
    Champ f[6] = {};
    const int k = champs_decouper_reste(reste, n, '|', f, 6);
    e.consigne = k > 0 ? champ_nombre(f[0], NAN) : NAN;
    e.piece = k > 1 ? champ_nombre(f[1], NAN) : NAN;
    std::string* modes[4] = {&e.mode, &e.preset, &e.ventilation, &e.oscillation};
    for (int i = 0; i < 4; i++) {
        if (k > 2 + i) modes[i]->assign(f[2 + i].p, std::min(f[2 + i].n, kModeMax));
        else modes[i]->clear();
    }
}

// ─── 8. Popup Température ───
// Avant : historique_recu() de Tab5/ecran/tab5_historique.cpp (lecture recopiée telle
// quelle), puis l'humidité (septième champ de l'en-tête, trois champs de plus par créneau).

namespace {

// Température : NAN si illisible ou hors de ±kHistoriqueTempMax.
float lire_temperature(const Champ& c) {
    const float v = champ_nombre(c, NAN);
    return std::fabs(v) <= kHistoriqueTempMax ? v : NAN;  // fabs(NAN) <= x est faux
}

// Minutes depuis le premier créneau : NAN si illisible, négatif ou au-delà de
// kHistoriqueMinutesMax.
float lire_minutes(const Champ& c) {
    const float v = champ_nombre(c, NAN);
    return v >= 0.0f && v <= kHistoriqueMinutesMax ? v : NAN;
}

}  // namespace

uint8_t humidite_lire(const Champ& c) {
    const float v = champ_nombre(c, NAN);
    if (!(v >= 0.0f && v <= 100.0f)) return kHumiditeAucune;  // NAN compris
    return static_cast<uint8_t>(std::lround(v));
}

Champ historique_lire(const Champ& entete, const Champ& mesures, const Champ& previsions, HistoriqueSerie& s) {
    new (&s) HistoriqueSerie();
    s.recue = true;

    // En-tête « nom|debut|pas|maintenant|actuel|exterieur[|humidité] ».
    const char* p = entete.p;
    const char* fin = p + entete.n;
    const Champ nom = champ_suivant(p, fin, '|');
    const Champ debut = champ_suivant(p, fin, '|');
    char date[24] = {};
    std::memcpy(date, debut.p, debut.n < sizeof(date) - 1 ? debut.n : sizeof(date) - 1);
    int an = 2000, mo = 1, jo = 1, he = 0, mi = 0;
    if (std::sscanf(date, "%d-%d-%d%*c%d:%d", &an, &mo, &jo, &he, &mi) != 5 || an < 1970 || an > 2200 || mo < 1 ||
        mo > 12 || jo < 1 || jo > 31 || he < 0 || he > 23 || mi < 0 || mi > 59) {
        an = 2000;  // illisible : seuls les libellés de l'axe s'en servent
        mo = jo = 1;
        he = mi = 0;
    }
    s.debut_jour = jour_civil(an, mo, jo);  // année bornée à 1970..2200 : tient en 32 bits
    s.debut_min = he * 60 + mi;
    const float pas = lire_minutes(champ_suivant(p, fin, '|'));
    s.pas = std::isnan(pas) || pas < 1.0f || pas > kHistoriquePasMax ? 60 : static_cast<int32_t>(pas);
    const float maintenant = lire_minutes(champ_suivant(p, fin, '|'));
    s.maintenant = std::isnan(maintenant) ? 0 : static_cast<int32_t>(maintenant);
    s.actuel = lire_temperature(champ_suivant(p, fin, '|'));
    s.exterieur = champ_est(champ_suivant(p, fin, '|'), "1");
    const Champ humidite = champ_suivant(p, fin, '|');
    s.humidite = humidite.n > 0;
    s.h_actuelle = humidite_lire(humidite);

    // Créneaux « moy,min,max[,h_moy,h_min,h_max] » séparés par « ; » (vide = pas de donnée).
    p = mesures.p;
    fin = p + mesures.n;
    while (p < fin && s.n < kHistoriqueMesuresMax) {
        const Champ c = champ_suivant(p, fin, ';');
        const char* q = c.p;
        const char* qf = c.p + c.n;
        HistoriquePoint& pt = s.m[s.n++];
        pt.moy = lire_temperature(champ_suivant(q, qf, ','));
        pt.mn = lire_temperature(champ_suivant(q, qf, ','));
        pt.mx = lire_temperature(champ_suivant(q, qf, ','));
        pt.h_moy = humidite_lire(champ_suivant(q, qf, ','));
        pt.h_mn = humidite_lire(champ_suivant(q, qf, ','));
        pt.h_mx = humidite_lire(champ_suivant(q, qf, ','));
    }
    // Points de prévision « minute,moy[,min,max] », dans l'ordre du temps.
    p = previsions.p;
    fin = p + previsions.n;
    while (p < fin && s.np < kHistoriquePrevMax) {
        const Champ c = champ_suivant(p, fin, ';');
        const char* q = c.p;
        const char* qf = c.p + c.n;
        const float minute = lire_minutes(champ_suivant(q, qf, ','));
        if (std::isnan(minute)) continue;
        HistoriquePrev& pv = s.p[s.np];
        pv.minute = static_cast<int32_t>(minute);
        pv.moy = lire_temperature(champ_suivant(q, qf, ','));
        pv.mn = lire_temperature(champ_suivant(q, qf, ','));
        pv.mx = lire_temperature(champ_suivant(q, qf, ','));
        if (s.np > 0 && pv.minute <= s.p[s.np - 1].minute) continue;  // hors de l'ordre : ignoré
        s.np++;
    }
    return nom;
}

// ─── 9. Lecteur de musique (ADR-0050) ───

namespace {
// Ajoute [s, s + m) à out (n octets, zéro final compris) ; faux si ça ne tient pas.
bool ajouter(char* out, size_t n, size_t& l, const char* s, size_t m) {
    if (l + m + 1 > n) return false;
    std::memcpy(out + l, s, m);
    l += m;
    out[l] = '\0';
    return true;
}
bool ajouter(char* out, size_t n, size_t& l, const char* s) { return ajouter(out, n, l, s, std::strlen(s)); }

bool commence_par(const char* p, size_t n, const char* mot) {
    const size_t m = std::strlen(mot);
    return n >= m && std::memcmp(p, mot, m) == 0;
}
bool url_absolue(const char* p, size_t n) {
    return p != nullptr && (commence_par(p, n, "http://") || commence_par(p, n, "https://"));
}

// 1 / 0 / -1 (inconnu) d'un champ « 1 » ou « 0 ».
int8_t drapeau(const Champ& c) {
    if (champ_est(c, "1")) return 1;
    if (champ_est(c, "0")) return 0;
    return -1;
}

// Secondes d'un champ : NAN si vide, illisible, non fini ou hors de 0 à kLecteurDureeMax.
float secondes(const Champ& c) {
    const float v = champ_nombre(c, NAN);
    return (std::isfinite(v) && v >= 0.0f && v <= kLecteurDureeMax) ? v : NAN;
}
}  // namespace

LecteurGenre lecteur_genre_lire(const Champ& c) {
    if (champ_est(c, "tv")) return LecteurGenre::TV;
    if (champ_est(c, "speaker")) return LecteurGenre::ENCEINTE;
    if (champ_est(c, "receiver")) return LecteurGenre::AMPLI;
    return LecteurGenre::AUTRE;
}

int lecteurs_lire(const Champ& payload, LecteurListeLu out[kLecteursMax]) {
    if (payload.p == nullptr) return 0;
    const char* p = payload.p;
    const char* fin = p + payload.n;
    int n = 0;
    while (p < fin && n < kLecteursMax) {
        const Champ c = champ_suivant(p, fin, ';');
        if (c.n == 0) continue;
        const char* q = c.p;
        const char* qf = c.p + c.n;
        out[n].nom = champ_suivant(q, qf, '|');
        out[n].genre = lecteur_genre_lire(champ_suivant(q, qf, '|'));
        n++;
    }
    return n;
}

LecteurEtat lecteur_etat_code(const Champ& c) {
    if (champ_est(c, "playing")) return LecteurEtat::LECTURE;
    if (champ_est(c, "paused")) return LecteurEtat::PAUSE;
    if (champ_est(c, "buffering")) return LecteurEtat::CHARGEMENT;
    if (champ_est(c, "idle") || champ_est(c, "on")) return LecteurEtat::INACTIF;
    if (champ_est(c, "standby")) return LecteurEtat::VEILLE;
    if (champ_est(c, "off")) return LecteurEtat::ETEINT;
    return LecteurEtat::INDISPONIBLE;
}

uint16_t lecteur_fonctions_lire(const Champ& c) {
    uint16_t f = 0;
    for (size_t i = 0; c.p != nullptr && i < c.n; i++) {
        switch (c.p[i]) {
            case 'l': f |= LECTEUR_F_LECTURE; break;
            case 's': f |= LECTEUR_F_POSITION; break;
            case 'v': f |= LECTEUR_F_VOLUME; break;
            case 'm': f |= LECTEUR_F_MUET; break;
            case 'p': f |= LECTEUR_F_PRECEDENT; break;
            case 'n': f |= LECTEUR_F_SUIVANT; break;
            case 'a': f |= LECTEUR_F_ALEATOIRE; break;
            case 'r': f |= LECTEUR_F_REPETITION; break;
            case 'o': f |= LECTEUR_F_ALLUMER; break;
            default: break;
        }
    }
    return f;
}

bool lecteur_etat_lire(const Champ& payload, LecteurEtatLu& out) {
    out = LecteurEtatLu{};
    if (payload.p == nullptr || payload.n == 0) return false;
    Champ f[16];
    const int n = champs_decouper_reste(payload.p, payload.n, '|', f, 16);
    if (n < 4) return false;
    for (int i = n; i < 16; i++) f[i] = Champ{payload.p + payload.n, 0};
    const float actif = champ_nombre(f[0], NAN);
    out.actif = (actif >= 0.0f && actif < static_cast<float>(kLecteursMax)) ? static_cast<int>(actif) : -1;
    out.nom = f[1];
    out.genre = lecteur_genre_lire(f[2]);
    out.etat = lecteur_etat_code(f[3]);
    out.titre = f[4];
    out.artiste = f[5];
    out.album = f[6];
    out.app = f[7];
    out.position = secondes(f[8]);
    out.duree = secondes(f[9]);
    if (out.duree == 0.0f) out.duree = NAN;
    const float v = champ_nombre(f[10], NAN);
    out.volume = (std::isfinite(v) && v >= 0.0f && v <= 100.0f) ? static_cast<int>(std::lround(v)) : -1;
    out.muet = drapeau(f[11]);
    out.aleatoire = drapeau(f[12]);
    if (champ_est(f[13], "off")) out.repetition = LecteurRepetition::NON;
    else if (champ_est(f[13], "all")) out.repetition = LecteurRepetition::TOUT;
    else if (champ_est(f[13], "one")) out.repetition = LecteurRepetition::UNE;
    out.fonctions = lecteur_fonctions_lire(f[14]);
    out.image = f[15];
    return true;
}

float lecteur_position(const LecteurEtatLu& e, float ecoule) {
    if (std::isnan(e.position)) return NAN;
    float p = e.position;
    if (e.etat == LecteurEtat::LECTURE && std::isfinite(ecoule) && ecoule > 0.0f) p += ecoule;
    if (!std::isnan(e.duree) && p > e.duree) p = e.duree;
    return std::min(p, kLecteurDureeMax);
}

bool lecteur_temps_texte(float s, char* out, size_t n) {
    if (out == nullptr || n == 0) return false;
    out[0] = '\0';
    int r = 0;
    if (!std::isfinite(s) || s < 0.0f || s > kLecteurDureeMax) {
        r = std::snprintf(out, n, "-:--");
    } else {
        const long t = static_cast<long>(s);
        const long h = t / 3600;
        const long m = (t / 60) % 60;
        const long sec = t % 60;
        r = h > 0 ? std::snprintf(out, n, "%ld:%02ld:%02ld", h, m, sec) : std::snprintf(out, n, "%ld:%02ld", m, sec);
    }
    if (r < 0 || static_cast<size_t>(r) >= n) {
        out[0] = '\0';
        return false;
    }
    return true;
}

bool ha_base_depuis_hote(const char* hote, char* out, size_t n) {
    if (out == nullptr || n == 0) return false;
    out[0] = '\0';
    if (hote == nullptr || hote[0] == '\0') return false;
    const size_t m = std::strlen(hote);
    if (m > 45) return false;  // INET6_ADDRSTRLEN - 1
    bool ipv6 = false;
    for (size_t i = 0; i < m; i++) {
        const char c = hote[i];
        if (c == ':') ipv6 = true;
        else if (c != '.' && !std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    size_t l = 0;
    const bool ok = ajouter(out, n, l, ipv6 ? "http://[" : "http://") && ajouter(out, n, l, hote, m) &&
                    ajouter(out, n, l, ipv6 ? "]:8123" : ":8123");
    if (!ok) out[0] = '\0';
    return ok;
}

bool ha_image_url(const Champ& image, const char* base, char* out, size_t n) {
    if (out == nullptr || n == 0) return false;
    out[0] = '\0';
    if (image.p == nullptr || image.n == 0) return false;
    size_t l = 0;
    bool ok = true;
    if (!url_absolue(image.p, image.n)) {
        const size_t b = base != nullptr ? std::strlen(base) : 0;
        if (!url_absolue(base, b)) return false;  // base vide comprise
        size_t bl = b;
        while (bl > 0 && base[bl - 1] == '/') bl--;
        ok = ajouter(out, n, l, base, bl) && (image.p[0] == '/' || ajouter(out, n, l, "/"));
    }
    ok = ok && ajouter(out, n, l, image.p, image.n);
    for (size_t i = 0; ok && i < l; i++) {
        const unsigned char c = static_cast<unsigned char>(out[i]);
        if (c <= 0x20 || c == 0x7F) ok = false;  // espace, saut de ligne, zéro : requête HTTP cassée
    }
    if (!ok) out[0] = '\0';
    return ok;
}
