/**
 * Harnais libFuzzer de la lecture des payloads de Home Assistant (Tab5/socle/tab5_parse.h,
 * lot F de l'audit du 30/09/2026). Le premier octet choisit le parseur
 * ((octet - '0') modulo le nombre de parseurs : « 0… » = le premier, « : » le onzième,
 * le caractère qui suit « 9 »), le reste est le
 * payload, passé comme le firmware le passe (std::string, puis c_str() ou data()/size()).
 * Les graines sont les payloads du fuzz de la tablette virtuelle
 * (tools/sanitizers/fuzz_services.py), écrites par tools/fuzz/graines.py.
 *
 * Build & run (CI, job `fuzz-parseurs` de .github/workflows/sanitizers.yml) :
 *   clang++ -std=c++17 -g -O1 -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all \
 *       -I Tab5/socle tools/fuzz/fuzz_parse.cpp Tab5/socle/tab5_parse.cpp Tab5/socle/tab5_champs.cpp \
 *       Tab5/socle/tab5_core.cpp Tab5/socle/tab5_i18n.cpp -o fuzz_parse
 *   ./fuzz_parse -max_total_time=180 corpus/
 * Témoin positif : -DTAB5_FUZZ_TEMOIN ajoute un comportement indéfini connu (un float hors
 * des bornes de int, comme tools/sanitizers/temoin.cpp) sur un payload « TEMOIN » ; le job
 * le rejoue sur tools/fuzz/temoin.txt et exige un rapport, sinon « 0 rapport » ne prouve rien.
 */
#include "tab5_core.h"
#include "tab5_parse.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

// Globaux que le firmware définit dans tab5_custom.cpp (tab5_core.cpp s'en sert).
DayForecastData cal_jours_data[15];
HourForecastData cal_heures_data[15];
int32_t cal_jours_anchor_day = -1;

namespace {

void heures(const std::string& p) {
    previsions_premier_creneau(p.c_str());
    previsions_heures_lire(p.c_str(), cal_heures_data);
}

void jours(const std::string& p) { previsions_jours_lire(p.c_str(), cal_jours_data, cal_jours_anchor_day); }

void vigilance(const std::string& p) {
    VigilanceLue v;
    vigilance_lire(p.c_str(), v);
    // Le premier champ est la phrase pluie (update_rain_phrase_ui, tab5_central.cpp).
    pluie_phrase_lire(std::string(v.champs[0]));
    vigilance_niveau(v.champs[1]);
    VigilanceActive a[kVigilanceActivesMax];
    const int n = vigilance_actives(v, a);
    for (int i = 0; i < n; i++) vigilance_niveau(a[i].niveau);
}

void alertes_ha(const std::string& p) {
    LecteurAlertesHa l(p.c_str());
    // Borné comme l'écran (4 bandeaux) puis au-delà : le lecteur doit finir seul.
    for (AlerteHaJeton j = l.suivant(); j.type != AlerteHaType::FIN; j = l.suivant()) {
        if (j.type == AlerteHaType::ALERTE) alerte_texte_lire(j.texte);
    }
}

void historique(const std::string& p) {
    AlerteHistoriqueLue e[20];
    int illisibles = 0;
    const int n = alertes_historique_lire(p.c_str(), e, 20, illisibles);
    for (int i = 0; i < n; i++) {
        const std::string texte(e[i].texte.p, e[i].texte.n);
        alerte_texte_lire(texte.c_str());
    }
}

void pluie(const std::string& p) {
    PluieBarre b[kPluieBarresMax];
    pluie_barres_lire(p.c_str(), b);
    pluie_niveau(p);
}

// Mois : le payload sert de codes, d'heures (« | ») et de détails (« ~ »), comme les trois
// variables du service ; tous les jours sont lus, comme le rendu de la grille.
void calendrier_mois(const std::string& p) {
    int y = 0, m = 0;
    calendrier_mois_lire(p, p, y, m);
    for (int day = 1; day <= 31; day++) {
        calendrier_code_jour(p, day);
        calendrier_champ(p, day - 1, '|');
        calendrier_champ(p, day - 1, '~');
    }
}

void calendrier_jour(const std::string& p) {
    int y = 0, m = 0, d = 0;
    calendrier_date_lire(p.c_str(), y, m, d);
    LecteurJourCalendrier l(p.c_str());
    int lignes = 0;
    for (CalJourLigne c = l.suivante(); c.type != CalJourType::FIN; c = l.suivante()) {
        if (c.type == CalJourType::LIGNE) lignes += (std::strcmp(c.genre, "travail") == 0) ? 1 : 0;
    }
    (void) lignes;
}

// Emplacements : chaque reste est lu comme la table 3.x (état|valeur, nombre), comme la
// production solaire et comme les clés de la clim (climr/crRT, ceRT), quelle que soit sa
// clé : le firmware les reçoit toutes par ce service. Le climat d'une pièce (« pR ») est
// lu avec sa vraie clé ; la zone à gauche de l'horloge (« gauche », ADR-0051) sur tout reste.
void emplacements(const std::string& p) {
    size_t debut = 0;
    EmplacementLu e;
    while (emplacement_suivant(p, debut, e)) {
        if (!e.a_cle) continue;
        solaire_pourcent(e.reste.p, e.reste.n);
        Champ etat, valeur;
        emplacement_etat_valeur(e.reste, etat, valeur);
        emplacement_nombre(std::string(valeur.p, valeur.n));
        ClimReglages r;
        Champ nom;
        if (clim_reglages_lire(e.reste.p, e.reste.n, r, nom) == 6) {
            const std::string copie(nom.p, nom.n);  // ce que texte_ha_copier recevrait
            (void) copie;
        }
        ClimEtat c;
        clim_etat_lire(e.reste.p, e.reste.n, c);
        PieceClimatLu pc;
        piece_climat_lire(e.cle, e.reste, pc);
        zone_gauche_lire(e.reste.p, e.reste.n);  // « gauche|… » (ADR-0051)
    }
}

// Popup Température : les trois variables du service (entete, mesures, previsions)
// séparées par un saut de ligne dans le payload (tools/fuzz/graines.py) ; une variable
// absente vaut "". Chaque champ est aussi lu comme une humidité.
void temperature(const std::string& p) {
    static HistoriqueSerie s;  // ~2 Ko : hors de la pile, comme le bloc PSRAM de l'écran
    std::string v[3];
    size_t debut = 0;
    for (int i = 0; i < 3; i++) {
        const size_t saut = i < 2 ? p.find('\n', debut) : std::string::npos;
        v[i] = p.substr(debut, saut == std::string::npos ? std::string::npos : saut - debut);
        if (saut == std::string::npos) break;
        debut = saut + 1;  // saut < p.size() : debut <= p.size()
    }
    const Champ nom = historique_lire(Champ{v[0].data(), v[0].size()}, Champ{v[1].data(), v[1].size()},
                                      Champ{v[2].data(), v[2].size()}, s);
    const std::string copie(nom.p, nom.n);  // ce que texte_ha_copier recevrait
    (void) copie;
    humidite_lire(Champ{p.data(), p.size()});
}

void info(const std::string& p) {
    // L'écran ne lit que le texte après « @ha| » (compose_info_code, tab5_central.cpp).
    InfoCodeLu lu;
    info_code_lire(p.rfind("@ha|", 0) == 0 ? p.c_str() + 4 : p.c_str(), lu);
}

// Un parseur par entrée ; l'ordre fixe le premier octet des graines (tools/fuzz/graines.py).
using Parseur = void (*)(const std::string&);
constexpr Parseur kParseurs[] = {
    heures,  // '0' tab5_maj_previsions_heures_bulk
    jours,      // '1' tab5_maj_previsions_jours_bulk
    vigilance,   // '2' tab5_maj_alerte_meteo_france
    alertes_ha,  // '3' tab5_maj_alertes_ha_bulk
    historique,  // '4' tab5_maj_alertes_historique
    info,        // '5' tab5_maj_info_texte
    pluie,            // '6' tab5_maj_pluie_1h_bulk
    calendrier_mois,  // '7' tab5_maj_calendrier_mois
    calendrier_jour,  // '8' tab5_maj_calendrier_jour
    emplacements,     // '9' tab5_maj_emplacements
    temperature,      // ':' tab5_maj_historique
};
constexpr size_t kNbParseurs = sizeof(kParseurs) / sizeof(kParseurs[0]);

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    if (size == 0) return 0;
    const size_t choix = static_cast<uint8_t>(data[0] - '0') % kNbParseurs;
    const std::string payload(reinterpret_cast<const char*>(data + 1), size - 1);
#ifdef TAB5_FUZZ_TEMOIN
    if (payload.compare(0, 6, "TEMOIN") == 0) {
        volatile float f = 1e30f;
        volatile int i = static_cast<int>(f);
        (void) i;
    }
#endif
    kParseurs[choix](payload);
    return 0;
}
