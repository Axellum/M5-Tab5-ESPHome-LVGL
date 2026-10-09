/**
 * [AI-CONTEXT]
 * @file tab5_parse.cpp
 * @role Implémentation de tab5_parse.h : logique pure, compilée à l'identique dans le
 *       firmware, dans les tests hôte (tools/test_parse.cpp) et dans le harnais libFuzzer
 *       (tools/fuzz/fuzz_parse.cpp). Chaque fonction est la boucle de l'écran qu'elle
 *       remplace, déplacée telle quelle (lot F de l'audit du 30/09/2026).
 */
#include "tab5_parse.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ─── 1. Prévisions ───
// Avant : parse_and_update_heures_bulk() et parse_and_update_jours_bulk(),
// Tab5/ecran/tab5_forecast.cpp. Tampon de pile plutôt qu'une copie std::string du payload
// (jusqu'à 2 048 octets, fragmentation de la SRAM).

int previsions_premier_creneau(const char* payload) { return std::atoi(payload); }

void previsions_heures_lire(const char* payload, HourForecastData heures[15]) {
    char buf[kPrevisionsMax + 1];
    strncpy(buf, payload, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* saveptr1 = nullptr;

    char* token = strtok_r(buf, ";", &saveptr1);
    while (token != nullptr) {
        char* parts[6];
        const int num_parts = split_fields(token, '|', parts, 6);

        if (num_parts >= 5) {
            int idx = std::atoi(parts[0]);
            if (idx >= 0 && idx < 15) {
                heures[idx].heure_texte = parts[1];
                heures[idx].condition = parts[2];
                heures[idx].temp = std::atof(parts[3]);
                heures[idx].pluvio = std::atof(parts[4]);
            }
        }
        token = strtok_r(nullptr, ";", &saveptr1);
    }
}

void previsions_jours_lire(const char* payload, DayForecastData jours[15], int32_t& ancre) {
    char buf[kPrevisionsMax + 1];
    strncpy(buf, payload, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    char* saveptr1 = nullptr;

    char* token = strtok_r(buf, ";", &saveptr1);
    while (token != nullptr) {
        // Découper chaque token par '|' — in-place, pas de std::vector
        char* parts[10];  // 9 champs attendus + marge
        const int num_parts = split_fields(token, '|', parts, 10);

        if (num_parts >= 9) {
            int jour = std::atoi(parts[0]);
            if (jour >= 0 && jour < 15) {
                jours[jour].nom_jour = parts[1];
                jours[jour].condition = parts[2];
                jours[jour].tmin = std::atof(parts[3]);
                jours[jour].tmax = std::atof(parts[4]);
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
            }
        }
        token = strtok_r(nullptr, ";", &saveptr1);
    }
}

// ─── 2. Vigilance ───
// Avant : le début de parse_and_update_vigilance(), Tab5/ecran/tab5_services.cpp.

void vigilance_lire(const char* payload, VigilanceLue& v) {
    // 1024 (était 512) : la phrase de vigilance peut être longue, un payload
    // complet dépassait parfois 512 et tronquait les derniers champs (#T165).
    strncpy(v.buf, payload, sizeof(v.buf) - 1);
    v.buf[sizeof(v.buf) - 1] = '\0';
    // strtok_r saute les champs vides consécutifs ("||"), comme l'ancien lambda :
    // un champ vide décalerait les suivants. Contrat HA inchangé — HA envoie
    // toujours "Vert" plutôt qu'une chaîne vide.
    char* saveptr = nullptr;
    for (int i = 0; i < kVigilanceChamps; i++) {
        char* tok = strtok_r(i == 0 ? v.buf : nullptr, "|", &saveptr);
        v.champs[i] = tok ? tok : "";
    }
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
    if (phrase.size() >= 2 && phrase[1] == '-') {
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
    char tampon[16];
    const size_t l = n < sizeof(tampon) - 1 ? n : sizeof(tampon) - 1;
    memcpy(tampon, valeur, l);
    tampon[l] = '\0';
    char* bout = nullptr;
    float v = strtof(tampon, &bout);
    if (l == 0 || bout == tampon) v = NAN;
    v = tab5_fini_ou_nan(v);
    if (!std::isnan(v)) v = v < 0.0f ? 0.0f : (v > 100.0f ? 100.0f : v);
    return v;
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
