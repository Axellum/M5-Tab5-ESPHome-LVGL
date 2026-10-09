/**
 * [AI-CONTEXT]
 * @file tab5_parse.cpp
 * @role Implémentation de tab5_parse.h : logique pure, compilée à l'identique dans le
 *       firmware, dans les tests hôte (tools/test_parse.cpp) et dans le harnais libFuzzer
 *       (tools/fuzz/fuzz_parse.cpp). Chaque fonction est la boucle de l'écran qu'elle
 *       remplace, déplacée telle quelle (lot F de l'audit du 30/09/2026).
 */
#include "tab5_parse.h"

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
