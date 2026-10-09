/**
 * Tests hôte de la lecture des payloads de Home Assistant (Tab5/socle/tab5_parse.h/.cpp,
 * lot F de l'audit du 30/09/2026), sans ESPHome ni LVGL : cas normaux, champs vides,
 * payloads tronqués, valeurs extrêmes, « nan » et « inf ».
 *
 * Extraction NEUTRE : ces tests figent le comportement de la boucle d'origine, travers
 * compris. Un cas marqué « [figé] » décrit un comportement discutable gardé tel quel
 * (strtok_r qui fusionne les champs vides, atoi qui lit un index illisible comme 0, « inf »
 * accepté…) : le changer est un changement de contrat, dans une PR à part.
 *
 * Build & run (CI, job `python` de .github/workflows/esphome-tab5.yml) :
 *   g++ -std=c++17 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=all \
 *       -I Tab5/socle -o test_parse tools/test_parse.cpp Tab5/socle/tab5_parse.cpp \
 *       Tab5/socle/tab5_champs.cpp Tab5/socle/tab5_core.cpp Tab5/socle/tab5_i18n.cpp
 *   ./test_parse
 * Le poste de dev n'a qu'un cross-compilateur RISC-V ; vérif locale possible :
 *   riscv32-esp-elf-g++ -std=c++17 -fsyntax-only -Wall -Wextra -I Tab5/socle tools/test_parse.cpp
 */
#include "tab5_core.h"
#include "tab5_parse.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

// Globaux que le firmware définit dans tab5_custom.cpp (tab5_core.cpp s'en sert).
DayForecastData cal_jours_data[15];
HourForecastData cal_heures_data[15];
int32_t cal_jours_anchor_day = -1;

// ── Horloge simulée ─────────────────────────────────────────────────────────
static time_t g_now = 0;
static time_t fake_time(time_t* out) {
    if (out != nullptr) *out = g_now;
    return g_now;
}

// ── Assertions ──────────────────────────────────────────────────────────────
static int g_fail = 0;
static int g_ok = 0;

static void expect(bool cond, const char* msg) {
    if (cond) {
        g_ok++;
    } else {
        g_fail++;
        std::printf("FAIL : %s\n", msg);
    }
}

// ════════════════════════════════════════════════════════════════════════════
// 1. Prévisions
// ════════════════════════════════════════════════════════════════════════════

static void vider(HourForecastData h[15]) {
    for (int i = 0; i < 15; i++) h[i] = HourForecastData{};
}

static void vider(DayForecastData d[15]) {
    for (int i = 0; i < 15; i++) d[i] = DayForecastData{};
}

static void test_previsions_heures() {
    HourForecastData h[15];
    vider(h);
    previsions_heures_lire("0|10h|sunny|21.5|0.2;1|11h|rainy|19|1.5", h);
    expect(h[0].heure_texte == "10h" && h[0].condition == "sunny" && h[0].temp == 21.5f &&
               std::fabs(h[0].pluvio - 0.2f) < 1e-6f,
           "heures : premier créneau");
    expect(h[1].heure_texte == "11h" && h[1].condition == "rainy" && h[1].temp == 19.0f && h[1].pluvio == 1.5f,
           "heures : second créneau");

    vider(h);
    previsions_heures_lire("2||cloudy|5|0", h);
    expect(h[2].heure_texte.empty() && h[2].condition == "cloudy" && h[2].temp == 5.0f,
           "heures : champ vide gardé (split_fields)");

    vider(h);
    previsions_heures_lire(";;3|a|b|1|2;;", h);
    expect(h[3].heure_texte == "a" && h[3].pluvio == 2.0f, "heures : enregistrements vides sautés");

    vider(h);
    previsions_heures_lire("4|a|b|1", h);
    expect(h[4].heure_texte.empty() && h[4].condition.empty(), "heures : moins de 5 champs ignoré");

    vider(h);
    previsions_heures_lire("5|a|b|1|2|x|y|z", h);
    expect(h[5].heure_texte == "a" && h[5].pluvio == 2.0f, "heures : champs en trop ignorés");

    vider(h);
    previsions_heures_lire("15|a|b|1|2;-1|a|b|1|2;99999999999|a|b|1|2", h);
    bool rien = true;
    for (int i = 0; i < 15; i++) rien = rien && h[i].heure_texte.empty();
    expect(rien, "heures : index hors de 0 à 14 ignoré");

    vider(h);
    previsions_heures_lire("abc|zz|sunny|1|2", h);
    expect(h[0].heure_texte == "zz", "heures [figé] : index illisible lu comme 0 (atoi)");

    vider(h);
    previsions_heures_lire("6|a|b|abc|", h);
    expect(h[6].temp == 0.0f && h[6].pluvio == 0.0f, "heures [figé] : nombre illisible ou vide = 0 (atof)");

    vider(h);
    previsions_heures_lire("7|a|b|nan|inf", h);
    expect(std::isnan(h[7].temp) && std::isinf(h[7].pluvio), "heures [figé] : « nan » et « inf » passent (atof)");

    vider(h);
    previsions_heures_lire("8|a|b|1e99|-1e99", h);
    expect(std::isinf(h[8].temp) && std::isinf(h[8].pluvio) && h[8].pluvio < 0,
           "heures [figé] : hors des float = infini");

    vider(h);
    previsions_heures_lire("", h);
    expect(h[0].heure_texte.empty(), "heures : payload vide sans effet");

    // Plus long que le tampon (l'appelant le refuse avant) : la fin est ignorée.
    std::string long_payload(kPrevisionsMax - 4, 'x');
    long_payload += ";9|a|b|1|2";
    vider(h);
    previsions_heures_lire(long_payload.c_str(), h);
    expect(h[9].heure_texte.empty(), "heures : au-delà de kPrevisionsMax, ignoré");

    expect(previsions_premier_creneau("5|10h|a|1|2") == 5 && previsions_premier_creneau("") == 0 &&
               previsions_premier_creneau("x") == 0 && previsions_premier_creneau("-3|") == -3,
           "premier créneau : atoi");
}

static void test_previsions_jours() {
    DayForecastData d[15];
    vider(d);
    int32_t ancre = -7;
    g_now = 1790000000;  // 2026-09-21, heure réglée
    previsions_jours_lire("1|Mar|rainy|8.5|15|1|0|1|08:00-16:00", d, ancre);
    expect(d[1].nom_jour == "Mar" && d[1].condition == "rainy" && d[1].tmin == 8.5f && d[1].tmax == 15.0f,
           "jours : champs texte et nombres");
    expect(d[1].est_repos && !d[1].est_dimanche && d[1].est_passe && d[1].heures_ouverture == "08:00-16:00",
           "jours : drapeaux et heures");
    expect(ancre == -7, "jours : l'ancre ne bouge que pour le jour 0");

    previsions_jours_lire("0|Auj|sunny|1|2|0|0|0|", d, ancre);
    expect(ancre == local_day_number_today() && ancre > 20000, "jours : le jour 0 date le lot");
    expect(d[0].heures_ouverture.empty(), "jours : heures vides gardées");

    g_now = 0;  // heure pas réglée
    previsions_jours_lire("0|Auj|sunny|1|2|0|0|0|", d, ancre);
    expect(ancre == -1, "jours : heure pas réglée, ancre -1");

    vider(d);
    previsions_jours_lire("2|Mer|a|1|2||10|1x|", d, ancre);
    expect(!d[2].est_repos && d[2].est_dimanche && d[2].est_passe,
           "jours : drapeau = premier caractère « 1 » (vide = faux)");

    vider(d);
    previsions_jours_lire("3|Jeu|a|1|2|0|0|0", d, ancre);
    expect(d[3].nom_jour.empty(), "jours : moins de 9 champs ignoré");

    vider(d);
    previsions_jours_lire("4|Ven|a|1|2|0|0|0|h|x|y", d, ancre);
    expect(d[4].nom_jour == "Ven" && d[4].heures_ouverture == "h", "jours : champs en trop ignorés");

    vider(d);
    previsions_jours_lire("15|a|b|1|2|0|0|0|h;-1|a|b|1|2|0|0|0|h", d, ancre);
    bool rien = true;
    for (int i = 0; i < 15; i++) rien = rien && d[i].nom_jour.empty();
    expect(rien, "jours : jour hors de 0 à 14 ignoré");

    vider(d);
    previsions_jours_lire("5|Sam|a|inf|-inf|0|0|0|h", d, ancre);
    expect(std::isinf(d[5].tmin) && std::isinf(d[5].tmax), "jours [figé] : « inf » passe (atof)");
}

int main() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // Europe/Paris, comme le firmware
    tzset();
    tab5_time_source = fake_time;

    test_previsions_heures();
    test_previsions_jours();

    std::printf("=== %s (%d OK, %d FAIL) ===\n", g_fail ? "FAILED" : "ALL PASSED", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
