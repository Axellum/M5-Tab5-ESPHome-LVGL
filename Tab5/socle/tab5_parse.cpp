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
    if (strncmp(brut, "@froid:", 7) == 0) return {AlerteTexteCode::FROID, brut + 7, 0};
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

// ─── 10. Popup Caméras (ADR-0049) ───
// Lot « Caméras » (09/10/2026) : la base et l'URL viennent des aides de la section 9.

namespace {
bool champs_egaux(const Champ& a, const Champ& b) {
    return a.n == b.n && (a.n == 0 || std::memcmp(a.p, b.p, a.n) == 0);
}
}  // namespace

int cameras_lire(const Champ& payload, CameraLue cameras[kCamerasMax], int* pieces) {
    if (pieces != nullptr) *pieces = 0;
    if (payload.p == nullptr) return 0;
    const char* p = payload.p;
    const char* fin = p + payload.n;
    int n = 0;
    while (p < fin && n < kCamerasMax) {
        const Champ c = champ_suivant(p, fin, ';');
        const char* q = c.p;
        const char* qf = c.p + c.n;
        const Champ nom = champ_suivant(q, qf, '|');
        const Champ image = champ_suivant(q, qf, '|');
        const Champ piece = champ_suivant(q, qf, '|');
        const Champ hors = champ_suivant(q, qf, '|');
        const uint32_t hors_ligne = champ_entier(hors, 0xFFFFFFFEu, 0);
        // Vide, « ;; » ou « nom| » : rien à montrer. Une caméra hors ligne sans image est
        // gardée : l'écran dit depuis quand.
        if (image.n == 0 && hors_ligne == 0) continue;
        CameraLue& l = cameras[n];
        l = CameraLue{};
        l.nom = nom;
        l.image = image;
        l.piece = piece;
        l.hors_ligne = hors_ligne;
        n++;
    }
    // Rangs des pièces : dans l'ordre de leur première caméra, la pièce vide en dernier.
    int np = 0;
    Champ distinctes[kCamerasMax];
    bool vide = false;
    for (int i = 0; i < n; i++) {
        if (cameras[i].piece.n == 0) {
            vide = true;
            continue;
        }
        int j = 0;
        while (j < np && !champs_egaux(distinctes[j], cameras[i].piece)) j++;
        if (j == np) distinctes[np++] = cameras[i].piece;
        cameras[i].piece_i = static_cast<int8_t>(j);
    }
    for (int i = 0; i < n; i++)
        if (cameras[i].piece.n == 0) cameras[i].piece_i = static_cast<int8_t>(np);
    if (pieces != nullptr) *pieces = np + (vide ? 1 : 0);
    return n;
}

bool camera_url(const Champ& image, const char* base, int largeur, int hauteur, char* out, size_t n) {
    if (!ha_image_url(image, base, out, n)) return false;
    // Proxy des caméras de HA (CameraImageView) : l'image est réduite par HA seulement si
    // largeur ET hauteur sont données.
    if (largeur > 0 && hauteur > 0 && std::strstr(out, "/api/camera_proxy/") != nullptr &&
        std::strstr(out, "width=") == nullptr && std::strstr(out, "height=") == nullptr) {
        char taille[40];
        std::snprintf(taille, sizeof(taille), "%cwidth=%d&height=%d", std::strchr(out, '?') != nullptr ? '&' : '?',
                      largeur, hauteur);
        size_t l = std::strlen(out);
        if (!ajouter(out, n, l, taille)) {
            out[0] = '\0';
            return false;
        }
    }
    return true;
}

// ─── 11. Suivi de capteurs (ADR-0054) ───

namespace {
// 10^d, d de 0 à kSuiviDecimalesMax.
double puissance_dix(int d) {
    double p = 1.0;
    for (int i = 0; i < d; i++) p *= 10.0;
    return p;
}

int borner_decimales(int d) { return d < 0 ? 0 : (d > kSuiviDecimalesMax ? kSuiviDecimalesMax : d); }

// Valeur bornée (±kSuiviValeurMax), NAN sinon.
float suivi_nombre(const Champ& c) {
    const float v = champ_nombre(c, NAN);
    return std::isfinite(v) && std::fabs(v) <= kSuiviValeurMax ? v : NAN;
}

// Décimales de la variation telle qu'elle s'affiche.
int decimales_variation(const SuiviLu& s) { return s.genre == SuiviVariation::POURCENT ? 2 : borner_decimales(s.decimales); }

// La variation arrondie comme à l'écran, en unités de la dernière décimale (0 : stable).
long long variation_arrondie(const SuiviLu& s) {
    if (s.genre == SuiviVariation::AUCUNE || !std::isfinite(s.variation)) return 0;
    return std::llround(static_cast<double>(s.variation) * puissance_dix(decimales_variation(s)));
}

// Un point de la courbe : un à trois chiffres, de 0 à 100. Tout le reste (vide, « 50.5 »,
// « -1 », « x ») : aucune mesure. Pas champ_entier(), qui accepte « 50.5 » (lu 50).
int8_t suivi_point(const Champ& c) {
    if (c.p == nullptr || c.n == 0 || c.n > 3) return kSuiviPointAucun;
    int v = 0;
    for (size_t i = 0; i < c.n; i++) {
        if (!std::isdigit(static_cast<unsigned char>(c.p[i]))) return kSuiviPointAucun;
        v = v * 10 + (c.p[i] - '0');
    }
    return v <= 100 ? static_cast<int8_t>(v) : kSuiviPointAucun;
}
}  // namespace

int suivi_decimales(const Champ& c) {
    if (c.p == nullptr) return 0;
    size_t i = 0;
    while (i < c.n && c.p[i] != '.') i++;
    int d = 0;
    for (i++; i < c.n && std::isdigit(static_cast<unsigned char>(c.p[i])); i++) d++;
    return d;
}

int suivis_lire(const Champ& payload, SuiviLu out[kSuivisMax]) {
    if (payload.p == nullptr) return 0;
    const char* p = payload.p;
    const char* fin = p + payload.n;
    int n = 0;
    while (p < fin && n < kSuivisMax) {
        const Champ e = champ_suivant(p, fin, ';');
        Champ f[6];
        const int k = champs_decouper_reste(e.p, e.n, '|', f, 6);
        if (e.n == 0 || k < 2) continue;
        for (int i = k; i < 6; i++) f[i] = Champ{e.p + e.n, 0};
        SuiviLu& s = out[n];
        s = SuiviLu{};
        s.nom = f[0];
        s.valeur = suivi_nombre(f[1]);
        // Au-delà de kSuiviDecimalesMax chiffres (« 7803.3301 »), la valeur arrondie sans
        // ses zéros de fin (« 7803.33 »).
        const int brutes = suivi_decimales(f[1]);
        s.decimales = borner_decimales(brutes);
        if (brutes > kSuiviDecimalesMax && std::isfinite(s.valeur)) {
            long long m = std::llround(static_cast<double>(s.valeur) * puissance_dix(s.decimales));
            while (s.decimales > 0 && m % 10 == 0) {
                m /= 10;
                s.decimales--;
            }
        }
        s.unite = f[2];
        const float v = suivi_nombre(f[3]);
        if (std::isfinite(v) && champ_est(f[4], "p")) s.genre = SuiviVariation::POURCENT;
        else if (std::isfinite(v) && champ_est(f[4], "a")) s.genre = SuiviVariation::ECART;
        s.variation = s.genre == SuiviVariation::AUCUNE ? NAN : v;
        // Points : « , » entre deux, 0 à 100 ; un vide ou illisible = aucune mesure.
        const char* q = f[5].p;
        const char* qf = f[5].p + f[5].n;
        while (q < qf && s.n < kSuiviPointsMax) {
            s.points[s.n++] = suivi_point(champ_suivant(q, qf, ','));
        }
        n++;
    }
    return n;
}

bool suivi_nombre_texte(float v, int decimales, char* out, size_t n) {
    if (out == nullptr || n == 0) return false;
    int r = 0;
    if (!std::isfinite(v) || std::fabs(v) > kSuiviValeurMax) {
        r = std::snprintf(out, n, "--");
    } else {
        // Au-delà de 2^24, le float n'a plus de décimales justes : aucune n'est écrite.
        const int d = std::fabs(v) >= kSuiviValeurExacteMax ? 0 : borner_decimales(decimales);
        r = std::snprintf(out, n, "%.*f", d, static_cast<double>(v));
    }
    if (r < 0 || static_cast<size_t>(r) >= n) {
        out[0] = '\0';
        return false;
    }
    // « -0.00 » (une petite valeur négative arrondie à zéro) : le signe retiré.
    if (out[0] == '-' && std::strspn(out + 1, "0.") == std::strlen(out + 1)) {
        std::memmove(out, out + 1, std::strlen(out));
    }
    return true;
}

bool suivi_variation_texte(const SuiviLu& s, char* out, size_t n) {
    if (out == nullptr || n == 0) return false;
    out[0] = '\0';
    if (s.genre == SuiviVariation::AUCUNE || !std::isfinite(s.variation)) return true;
    const int d = decimales_variation(s);
    const long long m = variation_arrondie(s);
    // Sur l'arrondi : jamais « -0.00 » ; la valeur absolue écrite à part, le signe devant.
    const double a = static_cast<double>(m < 0 ? -m : m) / puissance_dix(d);
    const char* signe = m > 0 ? "+" : (m < 0 ? "-" : "");
    const int r = std::snprintf(out, n, "%s%.*f%s", signe, d, a, s.genre == SuiviVariation::POURCENT ? " %" : "");
    if (r < 0 || static_cast<size_t>(r) >= n) {
        out[0] = '\0';
        return false;
    }
    return true;
}

int suivi_sens(const SuiviLu& s) {
    const long long m = variation_arrondie(s);
    return m > 0 ? 1 : (m < 0 ? -1 : 0);
}

// ─── 12. Froid : réfrigérateurs et congélateurs (ADR-0055) ───

namespace {
constexpr int kFroidChamps = 15;
constexpr uint32_t kFroidDureeMax = 10u * 366u * 24u * 60u;  // dix ans en minutes

// Température bornée (±kFroidTempMax), NAN sinon (vide, illisible, non finie).
float froid_temperature(const Champ& c) {
    const float v = champ_nombre(c, NAN);
    return std::isfinite(v) && std::fabs(v) <= kFroidTempMax ? v : NAN;
}

uint8_t froid_niveau(const Champ& c) {
    // « -1 » se lit comme un très grand nombre : au-dessus du plafond, donc 0.
    const uint32_t n = champ_entier(c, 0xFFFFFFFEu, 0);
    return static_cast<uint8_t>(n > 2 ? 2 : n);
}
}  // namespace

FroidCause froid_cause(const Champ& c) {
    if (champ_est(c, "chaud")) return FroidCause::CHAUD;
    if (champ_est(c, "froid")) return FroidCause::FROID;
    if (champ_est(c, "porte")) return FroidCause::PORTE;
    if (champ_est(c, "indispo")) return FroidCause::INDISPO;
    return FroidCause::OK;
}

int froid_lire(const Champ& payload, FroidLu out[kFroidMax]) {
    if (payload.p == nullptr) return 0;
    const char* p = payload.p;
    const char* fin = p + payload.n;
    int n = 0;
    while (p < fin && n < kFroidMax) {
        const Champ e = champ_suivant(p, fin, ';');
        Champ f[kFroidChamps];
        const int k = champs_decouper(e.p, e.n, '|', f, kFroidChamps);
        if (e.n == 0 || k < 2) continue;
        for (int i = k; i < kFroidChamps; i++) f[i] = Champ{e.p + e.n, 0};
        FroidType type;
        if (champ_est(f[1], "f")) type = FroidType::FRIGO;
        else if (champ_est(f[1], "c")) type = FroidType::CONGELATEUR;
        else continue;
        FroidLu& a = out[n];
        a = FroidLu{};
        a.nom = f[0];
        a.type = type;
        a.valeur = froid_temperature(f[2]);
        a.niveau = froid_niveau(f[3]);
        a.cause = froid_cause(f[4]);
        a.depuis = champ_entier(f[5], kAlerteEpochMax, 0);
        a.min = froid_temperature(f[6]);
        a.max = froid_temperature(f[7]);
        a.bas = froid_temperature(f[8]);
        a.haut = froid_temperature(f[9]);
        // Points : « , » entre deux, en °C ; un vide ou illisible = heure sans mesure. Un
        // « , » final compte : la valeur actuelle vide (capteur muet) reste le dernier point, à
        // sa place sur l'axe du temps (sinon la courbe s'étire d'une heure).
        const char* q = f[10].p;
        const char* qf = f[10].p + f[10].n;
        bool encore = f[10].n > 0;
        while (encore && a.n < kFroidPointsMax) {
            const Champ c = champ_suivant(q, qf, ',');
            encore = c.p + c.n < qf;  // un « , » suivait : un champ de plus, vide compris
            a.points[a.n++] = froid_temperature(c);
        }
        a.dernier.cause = froid_cause(f[11]);
        if (a.dernier.cause != FroidCause::OK) {
            a.dernier.debut = champ_entier(f[12], kAlerteEpochMax, 0);
            a.dernier.duree_min = champ_entier(f[13], kFroidDureeMax, 0);
            a.dernier.max = froid_temperature(f[14]);
        }
        n++;
    }
    return n;
}

bool froid_temperature_texte(float v, char* out, size_t n) {
    if (out == nullptr || n == 0) return false;
    int r = 0;
    if (!std::isfinite(v) || std::fabs(v) > kFroidTempMax) {
        r = std::snprintf(out, n, "--");
    } else {
        // Arrondie au dixième d'abord : « -0.04 » s'écrit « 0.0 », jamais « -0.0 ».
        double x = std::round(static_cast<double>(v) * 10.0) / 10.0;
        if (x == 0.0) x = 0.0;
        r = std::snprintf(out, n, "%.1f", x);
    }
    if (r < 0 || static_cast<size_t>(r) >= n) {
        out[0] = '\0';
        return false;
    }
    return true;
}

bool froid_echelle(const FroidLu& a, float& bas, float& haut) {
    float lo = INFINITY;
    float hi = -INFINITY;
    auto prendre = [&](float v) {
        if (!std::isfinite(v)) return;
        lo = std::min(lo, v);
        hi = std::max(hi, v);
    };
    for (int i = 0; i < a.n && i < kFroidPointsMax; i++) prendre(a.points[i]);
    prendre(a.valeur);
    prendre(a.bas);
    prendre(a.haut);
    if (!std::isfinite(lo) || !std::isfinite(hi)) return false;
    // Un degré d'air au-dessus et au-dessous, puis kFroidEchelleMin au moins, centré.
    lo -= 1.0f;
    hi += 1.0f;
    if (hi - lo < kFroidEchelleMin) {
        const float c = (lo + hi) / 2.0f;
        lo = c - kFroidEchelleMin / 2.0f;
        hi = c + kFroidEchelleMin / 2.0f;
    }
    bas = lo;
    haut = hi;
    return true;
}

uint8_t froid_niveau_max(const FroidLu* a, int n) {
    uint8_t m = 0;
    for (int i = 0; a != nullptr && i < n; i++) m = std::max(m, a[i].niveau);
    return m;
}

bool froid_alerte_lire(const char* reste, FroidAlerteLu& out) {
    out = FroidAlerteLu{};
    if (reste == nullptr) return false;
    const char* c1 = std::strchr(reste, ':');
    const char* c2 = c1 != nullptr ? std::strchr(c1 + 1, ':') : nullptr;
    const char* c3 = c2 != nullptr ? std::strchr(c2 + 1, ':') : nullptr;
    if (c3 == nullptr) return false;
    out.cause = froid_cause(Champ{reste, static_cast<size_t>(c1 - reste)});
    out.niveau = froid_niveau(Champ{c1 + 1, static_cast<size_t>(c2 - c1 - 1)});
    out.valeur = froid_temperature(Champ{c2 + 1, static_cast<size_t>(c3 - c2 - 1)});
    out.nom = c3 + 1;
    return true;
}

// ─── 13. Télécommandes (ADR-0056) ───
// Nouveau, écrit ici d'emblée comme zone_gauche_lire.

int telecommandes_lire(const char* valeur, size_t n, TelecommandeLue out[kTelecommandesMax]) {
    if (valeur == nullptr || n == 0) return 0;
    Champ f[2 * kTelecommandesMax] = {};
    const int k = champs_decouper(valeur, n, '|', f, 2 * kTelecommandesMax);
    const int nb = k / 2;  // paires complètes seulement
    for (int i = 0; i < nb; i++) {
        TelecommandeLue t;
        for (int e = 0; e < static_cast<int>(TelecommandeEcran::NB); e++)
            if (champ_est(f[2 * i], kTelecommandeEcranCodes[e])) t.ecran = static_cast<TelecommandeEcran>(e);
        t.nom = f[2 * i + 1];
        out[i] = t;
    }
    return nb;
}

const char* telecommande_emplacement(int i) {
    static constexpr const char* kCles[kTelecommandesMax] = {"tv", "tv1", "tv2", "tv3"};
    return (i >= 0 && i < kTelecommandesMax) ? kCles[i] : kCles[0];
}

// ─── 14. Serveur IA (ADR-0059) ───
// Nouveau, écrit ici d'emblée.

namespace {
constexpr int kServeurIaChamps = 14;

// Nombre borné à [bas, haut], NAN sinon (vide, illisible, non fini, hors bornes).
float serveur_ia_borne(const Champ& c, float bas, float haut) {
    const float v = champ_nombre(c, NAN);
    return std::isfinite(v) && v >= bas && v <= haut ? v : NAN;
}

// Requêtes : un entier de 0 à kServeurIaRequetesMax, -1 inconnu. Un nombre écrit en
// flottant par HA (« 2.0 ») est lu 2 ; « -1 », vide ou illisible : inconnu.
int32_t serveur_ia_requetes(const Champ& c) {
    const uint32_t v = champ_entier(c, kServeurIaRequetesMax, 0xFFFFFFFFu);
    return v == 0xFFFFFFFFu ? -1 : static_cast<int32_t>(v);
}

// Actions (ADR-0060) : « decharger,-reveiller » → bits montrés et actifs. Un code inconnu,
// vide ou en double ne gêne pas (montré une fois, actif s'il l'est une fois).
void serveur_ia_actions(const Champ& c, uint8_t& montrees, uint8_t& actives) {
    montrees = actives = 0;
    if (c.p == nullptr) return;
    const char* p = c.p;
    const char* fin = c.p + c.n;
    while (p < fin) {
        Champ code = champ_suivant(p, fin, ',');
        const bool grise = code.n > 0 && code.p[0] == '-';
        if (grise) code = Champ{code.p + 1, code.n - 1};
        for (int i = 0; i < kServeurIaActions; i++) {
            if (!champ_est(code, kServeurIaActionCodes[i])) continue;
            montrees |= static_cast<uint8_t>(1u << i);
            if (!grise) actives |= static_cast<uint8_t>(1u << i);
        }
    }
}
}  // namespace

bool serveur_ia_lire(const Champ& payload, ServeurIaLu& out) {
    out = ServeurIaLu{};
    if (payload.p == nullptr) return false;
    const char* p = payload.p;
    const char* fin = p + payload.n;
    while (p < fin) {
        const Champ e = champ_suivant(p, fin, ';');
        if (e.n == 0) continue;  // « ;; » : sauté
        Champ f[kServeurIaChamps];
        const int k = champs_decouper(e.p, e.n, '|', f, kServeurIaChamps);
        for (int i = k; i < kServeurIaChamps; i++) f[i] = Champ{e.p + e.n, 0};
        out.nom = f[0];
        out.en_ligne = champ_est(f[1], "1") ? 1 : (champ_est(f[1], "0") ? 0 : -1);
        out.modele = f[2];
        out.tps = serveur_ia_borne(f[3], 0.0f, kServeurIaTpsMax);
        out.en_cours = serveur_ia_requetes(f[4]);
        out.file = serveur_ia_requetes(f[5]);
        out.vram = serveur_ia_borne(f[6], 0.0f, kServeurIaVramMax);
        out.vram_total = serveur_ia_borne(f[7], 0.0f, kServeurIaVramMax);
        out.vram_pct = serveur_ia_borne(f[8], 0.0f, 100.0f);
        out.temperature = serveur_ia_borne(f[9], -kServeurIaTempMax, kServeurIaTempMax);
        // Vide, illisible ou « -1 » (au-dessus de max) : inconnu, jamais « normale ».
        const uint32_t niveau = champ_entier(f[10], 0xFFFFFFFEu, 0xFFFFFFFFu);
        out.niveau = niveau == 0xFFFFFFFFu ? kServeurIaNiveauInconnu : static_cast<uint8_t>(niveau > 2 ? 2 : niveau);
        out.ram = serveur_ia_borne(f[11], 0.0f, 100.0f);
        out.puissance = serveur_ia_borne(f[12], 0.0f, kServeurIaPuissanceMax);
        serveur_ia_actions(f[13], out.actions, out.actives);
        return true;  // un serveur : les enregistrements suivants sont ignorés
    }
    return false;
}

bool serveur_ia_nombre_texte(float v, int decimales, char* out, size_t n) {
    if (out == nullptr || n == 0) return false;
    int r = 0;
    if (!std::isfinite(v)) {
        r = std::snprintf(out, n, "—");
    } else {
        const int d = decimales < 0 ? 0 : (decimales > 3 ? 3 : decimales);
        double p = 1.0;
        for (int i = 0; i < d; i++) p *= 10.0;
        // Arrondi d'abord : « -0.04 » s'écrit « 0.0 », jamais « -0.0 ».
        double x = std::round(static_cast<double>(v) * p) / p;
        if (x == 0.0) x = 0.0;
        r = std::snprintf(out, n, "%.*f", d, x);
    }
    if (r < 0 || static_cast<size_t>(r) >= n) {
        out[0] = '\0';
        return false;
    }
    return true;
}

void ServeurIaCourbe::ajouter(float x) {
    if (n < 0 || n > kServeurIaPoints) n = 0;
    if (n == kServeurIaPoints) {
        std::memmove(v, v + 1, sizeof(float) * (kServeurIaPoints - 1));
        n--;
    }
    v[n++] = x;
}

bool ServeurIaCourbe::haut(float& h) const {
    float m = -INFINITY;
    for (int i = 0; i < n && i < kServeurIaPoints; i++)
        if (std::isfinite(v[i])) m = std::max(m, v[i]);
    if (!std::isfinite(m)) return false;
    h = std::max(m * 1.1f, 1.0f);
    return true;
}

// ─── 15. Énergie : soleil, prévision et bilan (ADR-0058) ───
// Nouveau, écrit ici d'emblée.

namespace {
constexpr int kSoleilChamps = 10;
constexpr int kBilanChamps = 4;

// « HH:MM » strict sur un champ (qui n'a pas de zéro final) : 5 octets exactement.
int energie_hhmm(const Champ& c) {
    if (c.p == nullptr || c.n != 5) return -1;
    char t[6];
    std::memcpy(t, c.p, 5);
    t[5] = '\0';
    return hhmm_minutes(t);
}

// Heure entière 0..24 d'un champ, -1 sinon.
int energie_heure(const Champ& c) {
    const uint32_t h = champ_entier(c, 24, 0xFFFFFFFFu);
    return h == 0xFFFFFFFFu ? -1 : static_cast<int>(h);
}

// Énergie d'un champ en kWh : NAN si vide, illisible, négative ou au-delà de kEnergieKwhMax.
float energie_kwh(const Champ& c) {
    const float v = champ_nombre(c, NAN);
    return std::isfinite(v) && v >= 0.0f && v <= kEnergieKwhMax ? v : NAN;
}

// Puissance d'un instantané (W) : NAN si non finie ou au-delà de ±kEnergieWMax.
float energie_w(float v) { return std::isfinite(v) && std::fabs(v) <= kEnergieWMax ? v : NAN; }

float somme_si(float s, float v) { return std::isfinite(v) ? (std::isfinite(s) ? s + v : v) : s; }

// Liste « ; » : la boucle d'energie_historique (champ vide compté, le dernier aussi).
// signe : une valeur négative est gardée (un gain), sinon elle vaut NAN (une énergie).
int liste_nombres(const Champ& c, float* out, int max, bool signe) {
    if (c.p == nullptr || c.n == 0 || out == nullptr || max <= 0) return 0;
    const char* p = c.p;
    const char* fin = c.p + c.n;
    int n = 0;
    bool apres_sep = false;
    while ((p < fin || apres_sep) && n < max) {
        const Champ v = champ_suivant(p, fin, ';');
        apres_sep = p > v.p + v.n;  // le champ s'est terminé sur un « ; »
        const float x = champ_nombre(v, NAN);
        const bool ok = std::isfinite(x) && std::fabs(x) <= kEnergieKwhMax && (signe || x >= 0.0f);
        out[n++] = ok ? x : NAN;
    }
    return n;
}
}  // namespace

int energie_kwh_liste(const Champ& c, float* out, int max) { return liste_nombres(c, out, max, false); }

bool energie_soleil_lire(const Champ& payload, EnergieSoleilLu& out) {
    out = EnergieSoleilLu{};
    if (payload.p == nullptr) return false;
    Champ f[kSoleilChamps];
    const int k = champs_decouper(payload.p, payload.n, '|', f, kSoleilChamps);
    for (int i = k; i < kSoleilChamps; i++) f[i] = Champ{payload.p + payload.n, 0};
    out.lever = energie_hhmm(f[0]);
    out.midi = energie_hhmm(f[1]);
    out.coucher = energie_hhmm(f[2]);
    if (out.lever < 0 || out.coucher <= out.lever) out.lever = out.midi = out.coucher = -1;
    if (out.midi <= out.lever || out.midi >= out.coucher) out.midi = -1;
    out.prevu_jour_choisi = f[3].n > 0;
    out.prevu_jour = energie_kwh(f[3]);
    out.prevu_demain_choisi = f[4].n > 0;
    out.prevu_demain = energie_kwh(f[4]);
    if (champ_est(f[5], "a")) out.source = EnergieSource::APPRISE;
    else if (champ_est(f[5], "e")) out.source = EnergieSource::EXTERNE;
    out.creneau_debut = energie_heure(f[6]);
    out.creneau_fin = energie_heure(f[7]);
    if (out.creneau_debut < 0 || out.creneau_fin <= out.creneau_debut) out.creneau_debut = out.creneau_fin = -1;
    out.n_prevu = energie_kwh_liste(f[8], out.prevu, kEnergieHeures);
    out.n_clair = energie_kwh_liste(f[9], out.clair, kEnergieHeures);
    return out.lever >= 0 || out.n_prevu > 0 || out.n_clair > 0;
}

bool energie_bilan_lire(const Champ& payload, int slots, EnergieBilanLu& out) {
    out = EnergieBilanLu{};
    if (payload.p == nullptr) return false;
    if (slots > kEnergieSlotsMax) slots = kEnergieSlotsMax;
    if (slots < 0) slots = 0;
    Champ f[kBilanChamps];
    const int k = champs_decouper(payload.p, payload.n, '|', f, kBilanChamps);
    for (int i = k; i < kBilanChamps; i++) f[i] = Champ{payload.p + payload.n, 0};
    if (f[0].n <= kEnergieDeviseMax) out.devise = f[0];
    out.vente_choisie = f[1].n > 0;
    out.achat_choisi = f[2].n > 0;
    out.gain_choisi = f[3].n > 0;
    out.n_vente = energie_kwh_liste(f[1], out.vente, slots);
    out.n_achat = energie_kwh_liste(f[2], out.achat, slots);
    // Le gain peut être négatif (prix de revente négatif) : borné en valeur absolue.
    out.n_gain = liste_nombres(f[3], out.gain, slots, true);
    return out.vente_choisie || out.achat_choisi;
}

EnergieBilanCreneau energie_bilan_creneau(float produit, const EnergieBilanLu& b, int k) {
    EnergieBilanCreneau c;
    if (k < 0 || k >= kEnergieSlotsMax) return c;
    c.produit = std::isfinite(produit) && produit >= 0.0f && produit <= kEnergieKwhMax ? produit : NAN;
    c.vendu = (b.vente_choisie && k < b.n_vente) ? b.vente[k] : NAN;
    c.achete = (b.achat_choisi && k < b.n_achat) ? b.achat[k] : NAN;
    c.gain = (b.gain_choisi && k < b.n_gain) ? b.gain[k] : NAN;
    if (std::isfinite(c.produit)) c.autoconsomme = std::max(c.produit - (std::isfinite(c.vendu) ? c.vendu : 0.0f), 0.0f);
    if (b.achat_choisi && (std::isfinite(c.autoconsomme) || std::isfinite(c.achete)))
        c.consomme = (std::isfinite(c.autoconsomme) ? c.autoconsomme : 0.0f) +
                     (std::isfinite(c.achete) ? c.achete : 0.0f);
    return c;
}

EnergieBilanTotaux energie_bilan_totaux(const float* produit, int n, const EnergieBilanLu& b) {
    EnergieBilanTotaux t;
    if (n > kEnergieSlotsMax) n = kEnergieSlotsMax;
    for (int k = 0; k < n; k++) {
        const EnergieBilanCreneau c = energie_bilan_creneau(produit != nullptr ? produit[k] : NAN, b, k);
        t.produit = somme_si(t.produit, c.produit);
        t.autoconsomme = somme_si(t.autoconsomme, c.autoconsomme);
        t.vendu = somme_si(t.vendu, c.vendu);
        t.achete = somme_si(t.achete, c.achete);
        t.consomme = somme_si(t.consomme, c.consomme);
        t.gain = somme_si(t.gain, c.gain);
    }
    if (std::isfinite(t.produit) && t.produit > 0.0f && std::isfinite(t.autoconsomme))
        t.taux = std::min(t.autoconsomme / t.produit, 1.0f);
    return t;
}

EnergieFlux energie_flux_calculer(float solaire, float maison, float reseau, float batterie) {
    EnergieFlux f;
    solaire = energie_w(solaire);
    maison = energie_w(maison);
    reseau = energie_w(reseau);
    batterie = energie_w(batterie);
    const float s = std::isfinite(solaire) ? std::max(solaire, 0.0f) : 0.0f;
    const float r = std::isfinite(reseau) ? reseau : 0.0f;
    const float b = std::isfinite(batterie) ? batterie : 0.0f;
    const float vente = std::max(-r, 0.0f), achat = std::max(r, 0.0f);
    const float charge = std::max(b, 0.0f), decharge = std::max(-b, 0.0f);
    f.solaire_reseau = std::min(vente, s);
    f.solaire_batterie = std::min(charge, s - f.solaire_reseau);
    f.solaire_maison = std::max(s - f.solaire_reseau - f.solaire_batterie, 0.0f);
    f.reseau_batterie = std::min(std::max(charge - f.solaire_batterie, 0.0f), achat);
    f.reseau_maison = std::max(achat - f.reseau_batterie, 0.0f);
    f.batterie_reseau = std::min(std::max(vente - f.solaire_reseau, 0.0f), decharge);
    f.batterie_maison = std::max(decharge - f.batterie_reseau, 0.0f);
    if (std::isfinite(maison)) f.maison = maison;
    else if (std::isfinite(solaire) || std::isfinite(reseau) || std::isfinite(batterie))
        f.maison = f.solaire_maison + f.reseau_maison + f.batterie_maison;
    return f;
}
