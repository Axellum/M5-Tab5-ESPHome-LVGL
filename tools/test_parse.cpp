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

// ════════════════════════════════════════════════════════════════════════════
// 2. Vigilance
// ════════════════════════════════════════════════════════════════════════════

static void test_vigilance() {
    VigilanceLue v;
    VigilanceActive a[kVigilanceActivesMax];
    vigilance_lire("@2,179|Jaune|Vert|Orange|Vert|Vert|Vert|Vert|Vert|Rouge|Vert|Vert|Jaune", v);
    expect(std::strcmp(v.champs[0], "@2,179") == 0 && std::strcmp(v.champs[1], "Jaune") == 0,
           "vigilance : phrase pluie et niveau global");
    int n = vigilance_actives(v, a);
    expect(n == 3 && a[0].phenomene == 1 && std::strcmp(a[0].niveau, "Orange") == 0 && a[1].phenomene == 7 &&
               std::strcmp(a[1].niveau, "Rouge") == 0 && a[2].phenomene == 10,
           "vigilance : phénomènes actifs dans l'ordre (inondation, canicule, feux de forêt)");

    vigilance_lire("p|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Jaune", v);
    expect(std::strcmp(v.champs[11], "") == 0 && std::strcmp(v.champs[12], "") == 0,
           "vigilance : 11 champs (Météo-France), les deux derniers vides");
    n = vigilance_actives(v, a);
    expect(n == 1 && a[0].phenomene == 8, "vigilance : avalanches seules");

    vigilance_lire("p|Jaune||Orange|Vert", v);
    expect(std::strcmp(v.champs[2], "Orange") == 0 && std::strcmp(v.champs[3], "Vert") == 0,
           "vigilance [figé] : « || » fusionné, le champ suivant remonte (R6, strtok_r)");

    vigilance_lire("|Rouge|Vert", v);
    expect(std::strcmp(v.champs[0], "Rouge") == 0 && std::strcmp(v.champs[1], "Vert") == 0,
           "vigilance [figé] : phrase vide, tout remonte d'un cran");

    vigilance_lire("p|Rouge|Jaune|Jaune|Jaune|Jaune|Jaune|Jaune", v);
    n = vigilance_actives(v, a);
    expect(n == kVigilanceActivesMax && a[0].phenomene == 0 && a[3].phenomene == 3,
           "vigilance : au plus 4 phénomènes, les premiers");

    vigilance_lire("p|g|unknown|Vert|Foo|jaune", v);
    n = vigilance_actives(v, a);
    expect(n == 2 && a[0].phenomene == 2 && a[1].phenomene == 3 &&
               vigilance_niveau(a[0].niveau) == NiveauVigilance::AUTRE,
           "vigilance : « unknown » et « Vert » sautés, tout autre texte compte");

    vigilance_lire("", v);
    bool vides = true;
    for (int i = 0; i < kVigilanceChamps; i++) vides = vides && v.champs[i][0] == '\0';
    expect(vides && vigilance_actives(v, a) == 0, "vigilance : payload vide");

    // Plus long que le tampon : la fin manque (l'appelant le journalise).
    std::string longue(1020, 'x');
    longue += "|Rouge|Rouge";
    vigilance_lire(longue.c_str(), v);
    expect(std::strlen(v.champs[0]) == 1020 && std::strcmp(v.champs[1], "Ro") == 0 && v.champs[2][0] == '\0',
           "vigilance : coupé à 1 023 octets");

    expect(vigilance_niveau("Jaune") == NiveauVigilance::JAUNE && vigilance_niveau("Orange") == NiveauVigilance::ORANGE &&
               vigilance_niveau("Rouge") == NiveauVigilance::ROUGE,
           "niveau : les trois couleurs");
    expect(vigilance_niveau("rouge") == NiveauVigilance::AUTRE && vigilance_niveau("Rouge ") == NiveauVigilance::AUTRE &&
               vigilance_niveau("") == NiveauVigilance::AUTRE,
           "niveau : comparaison exacte");
}

// ════════════════════════════════════════════════════════════════════════════
// 3. Alertes HA, historique, bandeau info
// ════════════════════════════════════════════════════════════════════════════

static bool alerte_vaut(const AlerteHaJeton& j, const char* id, const char* niveau, const char* texte) {
    return j.type == AlerteHaType::ALERTE && std::strcmp(j.id, id) == 0 && std::strcmp(j.niveau, niveau) == 0 &&
           std::strcmp(j.texte, texte) == 0;
}

static void test_alertes_ha() {
    {
        LecteurAlertesHa l("@n:6;u#1|Rouge|@maj:HA Core;i#2|Orange|@indispo:3");
        const AlerteHaJeton t = l.suivant();
        expect(t.type == AlerteHaType::TOTAL && t.total == 6, "bandeaux : en-tête « @n:6 »");
        expect(alerte_vaut(l.suivant(), "u#1", "Rouge", "@maj:HA Core"), "bandeaux : première alerte");
        expect(alerte_vaut(l.suivant(), "i#2", "Orange", "@indispo:3"), "bandeaux : seconde alerte");
        expect(l.suivant().type == AlerteHaType::FIN && l.suivant().type == AlerteHaType::FIN,
               "bandeaux : fin, et fin encore après");
    }
    {
        LecteurAlertesHa l(";;x;a|b;id|n|t|reste;id2||");
        expect(l.suivant().type == AlerteHaType::AUTRE, "bandeaux [figé] : « ;; » sauté, un champ = AUTRE");
        expect(l.suivant().type == AlerteHaType::AUTRE, "bandeaux : deux champs = AUTRE");
        expect(alerte_vaut(l.suivant(), "id", "n", "t"), "bandeaux : le texte s'arrête au « | » suivant");
        expect(alerte_vaut(l.suivant(), "id2", "", ""), "bandeaux : niveau et texte vides gardés");
        expect(l.suivant().type == AlerteHaType::FIN, "bandeaux : fin");
    }
    {
        LecteurAlertesHa l("@n:abc;@n:;@n:-2");
        const int a = l.suivant().total, b = l.suivant().total, c = l.suivant().total;
        expect(a == 0 && b == 0 && c == -2, "bandeaux [figé] : total = atoi (illisible 0, négatif gardé)");
    }
    {
        LecteurAlertesHa l("");
        expect(l.suivant().type == AlerteHaType::FIN, "bandeaux : payload vide");
    }
    {
        std::string longue = "id|n|" + std::string(kAlertesHaMax, 'x') + ";id2|n|t";
        LecteurAlertesHa l(longue.c_str());
        const AlerteHaJeton j = l.suivant();
        expect(j.type == AlerteHaType::ALERTE && std::strlen(j.texte) == kAlertesHaMax - 5 &&
                   l.suivant().type == AlerteHaType::FIN,
               "bandeaux : coupé à kAlertesHaMax octets");
    }
}

static void test_alerte_texte() {
    AlerteTexteLu t = alerte_texte_lire("@maj:Home Assistant Core");
    expect(t.code == AlerteTexteCode::MAJ && std::strcmp(t.reste, "Home Assistant Core") == 0, "libellé : @maj");
    t = alerte_texte_lire("@indispo:12");
    expect(t.code == AlerteTexteCode::INDISPO && t.nombre == 12, "libellé : @indispo");
    t = alerte_texte_lire("@indispo:x");
    expect(t.code == AlerteTexteCode::INDISPO && t.nombre == 0, "libellé [figé] : @indispo illisible = 0 (atoi)");
    t = alerte_texte_lire("@vigi:Orange");
    expect(t.code == AlerteTexteCode::VIGI && vigilance_niveau(t.reste) == NiveauVigilance::ORANGE, "libellé : @vigi");
    t = alerte_texte_lire("Capteur en panne");
    expect(t.code == AlerteTexteCode::TEXTE && std::strcmp(t.reste, "Capteur en panne") == 0, "libellé : texte libre");
    t = alerte_texte_lire("@MAJ:x");
    expect(t.code == AlerteTexteCode::TEXTE, "libellé : préfixe sensible à la casse");
    t = alerte_texte_lire("");
    expect(t.code == AlerteTexteCode::TEXTE && t.reste[0] == '\0', "libellé : vide");
}

static bool texte_vaut(const Champ& c, const char* attendu) {
    return c.n == std::strlen(attendu) && std::memcmp(c.p, attendu, c.n) == 0;
}

static void test_alertes_historique() {
    AlerteHistoriqueLue e[20];
    int ill = -1;
    int n = alertes_historique_lire(
        "1791381720|1791382200|0|Rouge|@maj:Home Assistant Core;1791370000|0|1791375400|Orange|@vigi:Orange", e, 20, ill);
    expect(n == 2 && ill == 0, "historique : deux entrées");
    expect(e[0].apparue == 1791381720u && e[0].lue == 1791382200u && e[0].terminee == 0 && e[0].gravite == 'R' &&
               texte_vaut(e[0].texte, "@maj:Home Assistant Core"),
           "historique : première entrée");
    expect(e[1].lue == 0 && e[1].terminee == 1791375400u && e[1].gravite == 'O', "historique : seconde entrée");

    n = alertes_historique_lire("1|0|0|Jaune|a;2|0|0||b;3|0|0|x|c;4|0|0|jaune|d", e, 20, ill);
    expect(n == 4 && e[0].gravite == 'J' && e[1].gravite == 'O' && e[2].gravite == 'O' && e[3].gravite == 'O',
           "historique : gravité R/J à la première lettre, sinon O");

    n = alertes_historique_lire("1|-1|4102444801|R|a;4102444800|abc||R|b", e, 20, ill);
    expect(n == 2 && e[0].lue == 0 && e[0].terminee == 0 && e[1].apparue == kAlerteEpochMax && e[1].lue == 0,
           "historique : epoch « -1 », au-delà de 2100 ou illisible = 0 ; 2100 compris");

    n = alertes_historique_lire("0|0|0|R|a;|0|0|R|b;x|0|0|R|c;1|0|0|R", e, 20, ill);
    expect(n == 0 && ill == 4, "historique : apparue 0, vide, illisible, quatre champs = illisibles");

    n = alertes_historique_lire(";;1|0|0|R|a;", e, 20, ill);
    expect(n == 1 && ill == 2, "historique : entrées vides illisibles, « ; » final sans entrée");

    n = alertes_historique_lire("1|0|0|R|a|b|c", e, 20, ill);
    expect(n == 1 && texte_vaut(e[0].texte, "a|b|c"), "historique : le libellé prend le reste");

    std::string beaucoup;
    for (int i = 1; i <= 25; i++) beaucoup += std::to_string(i) + "|0|0|R|t;";
    beaucoup = "x;" + beaucoup;
    n = alertes_historique_lire(beaucoup.c_str(), e, 20, ill);
    expect(n == 20 && e[19].apparue == 20 && ill == 1, "historique : au plus `max` entrées, la lecture s'arrête là");

    n = alertes_historique_lire("", e, 20, ill);
    expect(n == 0 && ill == 0, "historique : payload vide");
}

static void test_info_code() {
    InfoCodeLu lu;
    info_code_lire("1|Home Assistant Core|0|0|0|", lu);
    expect(lu.nb_maj == 1 && std::strcmp(lu.titre, "Home Assistant Core") == 0 && lu.nb_err == 0 &&
               lu.nb_indispo == 0 && !lu.jaune && lu.vigi[0] == '\0',
           "info : une MAJ");
    info_code_lire("2||3|4|1|rouge", lu);
    expect(lu.nb_maj == 2 && lu.titre[0] == '\0' && lu.nb_err == 3 && lu.nb_indispo == 4 && lu.jaune &&
               std::strcmp(lu.vigi, "rouge") == 0,
           "info : tous les compteurs, titre vide gardé");
    info_code_lire("", lu);
    expect(lu.nb_maj == 0 && lu.titre[0] == '\0' && lu.vigi[0] == '\0' && !lu.jaune, "info : vide");
    info_code_lire("5", lu);
    expect(lu.nb_maj == 5 && lu.nb_err == 0 && lu.vigi[0] == '\0', "info : champs absents = \"\" (0)");
    info_code_lire("1|t|0|0|0|orange|en trop", lu);
    expect(std::strcmp(lu.vigi, "orange") == 0, "info : champs en trop ignorés");
    info_code_lire("x|t|y|z|2|", lu);
    expect(lu.nb_maj == 0 && lu.nb_err == 0 && lu.jaune, "info [figé] : nombres par atoi, « 2 » = jaune");
    std::string longue = "1|" + std::string(300, 'T');
    info_code_lire(longue.c_str(), lu);
    expect(std::strlen(lu.titre) == 253, "info : copié dans 255 octets");
}

int main() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // Europe/Paris, comme le firmware
    tzset();
    tab5_time_source = fake_time;

    test_previsions_heures();
    test_previsions_jours();
    test_vigilance();
    test_alertes_ha();
    test_alerte_texte();
    test_alertes_historique();
    test_info_code();

    std::printf("=== %s (%d OK, %d FAIL) ===\n", g_fail ? "FAILED" : "ALL PASSED", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
