/**
 * Harnais libFuzzer de la lecture des payloads de Home Assistant (Tab5/socle/tab5_parse.h,
 * lot F de l'audit du 30/09/2026). Le premier octet choisit le parseur
 * ((octet - '0') modulo le nombre de parseurs : « 0… » = le premier), le reste est le
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
