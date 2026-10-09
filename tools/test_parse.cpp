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
#include <cstdint>
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

// ════════════════════════════════════════════════════════════════════════════
// 4. Pluie
// ════════════════════════════════════════════════════════════════════════════

static void test_pluie_niveau() {
    expect(pluie_niveau("0") == 0 && pluie_niveau("3") == 3 && pluie_niveau("4") == 4, "pluie : chiffres 0 à 4");
    expect(pluie_niveau("5") == 0 && pluie_niveau("-1") == 0 && pluie_niveau("12") == 0 && pluie_niveau("") == 0,
           "pluie : autre chiffre ou vide = 0");
    expect(pluie_niveau("Pluie faible") == 1 && pluie_niveau("Pluie modérée") == 2 && pluie_niveau("Pluie forte") == 3,
           "pluie : libellés Météo-France");
    expect(pluie_niveau("Pluie très forte") == 4 && pluie_niveau("Pluie trés forte") == 4,
           "pluie : « très forte », et la faute d'accent de Météo-France");
    expect(pluie_niveau("Temps sec") == 0 && pluie_niveau("pluie faible") == 0 && pluie_niveau("Pluie faible ") == 0,
           "pluie : comparaison exacte");
}

static void test_pluie_barres() {
    PluieBarre b[kPluieBarresMax];
    int n = pluie_barres_lire("0|Temps sec;1|Pluie faible;2|Pluie modérée;8|4", b);
    expect(n == 4 && b[0].idx == 0 && b[0].niveau == 0 && b[1].niveau == 1 && b[2].niveau == 2 && b[3].idx == 8 &&
               b[3].niveau == 4,
           "pluie : barres dans l'ordre");

    n = pluie_barres_lire(";;3;4|2;9|1;-1|3;x|4;5|", b);
    expect(n == 5 && b[0].idx == 4 && b[1].idx == 9 && b[2].idx == -1 && b[3].idx == 0 && b[4].idx == 5 &&
               b[4].niveau == 0,
           "pluie [figé] : sans « | » sauté, index par atoi (hors bornes rendus, « x » = 0), intensité vide = 0");

    n = pluie_barres_lire("1|a|b", b);
    expect(n == 1 && b[0].niveau == 0, "pluie : intensité = tout après le premier « | »");

    n = pluie_barres_lire("", b);
    expect(n == 0, "pluie : payload vide");

    std::string plein;
    for (int i = 0; i < 128; i++) plein += (i ? ";|" : "|");
    expect(plein.size() == 255 && pluie_barres_lire(plein.c_str(), b) == kPluieBarresMax,
           "pluie : 128 enregistrements tiennent dans 255 octets");

    std::string longue(kPluieMax, ';');
    longue += "1|4";
    n = pluie_barres_lire(longue.c_str(), b);
    expect(n == 0, "pluie : au-delà de 255 octets, ignoré");
}

static void test_pluie_phrase() {
    PluiePhrase p = pluie_phrase_lire("@2,1790000000");
    expect(p.code && p.niveau == 2 && p.debut == 1790000000, "phrase : niveau et début");
    p = pluie_phrase_lire("@0,0");
    expect(p.code && p.niveau == 0 && p.debut == 0, "phrase : temps sec");
    p = pluie_phrase_lire("@-");
    expect(p.code && p.niveau == -2 && p.debut == 0, "phrase : aucune source");
    // HA envoie « @-1,0 » quand il n'a pas de données (packages/tab5_meteo_sources.yaml,
    // tab5_push.yaml) : la tablette le lit comme « @- » (aucune source, phrase vide) et
    // n'affiche jamais « Pas de données ». Signalé avec le lot F, pas corrigé ici.
    p = pluie_phrase_lire("@-1,0");
    expect(p.code && p.niveau == -2 && p.debut == 0, "phrase [figé] : « @-1,0 » lu comme « @- » (aucune source)");
    p = pluie_phrase_lire("@3");
    expect(p.code && p.niveau == 3 && p.debut == 0, "phrase : sans virgule, début 0");
    p = pluie_phrase_lire("@x,y");
    expect(p.code && p.niveau == 0 && p.debut == 0, "phrase [figé] : illisible = 0");
    p = pluie_phrase_lire("@1,99999999999999999999999");
    expect(p.code && p.debut == INT64_MAX, "phrase : début saturé par strtoll");
    p = pluie_phrase_lire("@");
    expect(p.code && p.niveau == 0 && p.debut == 0, "phrase : « @ » seul");
    p = pluie_phrase_lire("Averses dans 12 mn");
    expect(!p.code, "phrase : texte sans « @ »");
    p = pluie_phrase_lire("");
    expect(!p.code, "phrase : vide");
}

// ════════════════════════════════════════════════════════════════════════════
// 5. Calendrier
// ════════════════════════════════════════════════════════════════════════════

static void test_calendrier_mois() {
    int y = -1, m = -1;
    expect(calendrier_mois_lire("2026", "9", y, m) && y == 2026 && m == 9, "mois : 2026-9");
    expect(calendrier_mois_lire("2000", "01", y, m) && calendrier_mois_lire("2100", "12", y, m), "mois : bornes comprises");
    expect(!calendrier_mois_lire("1999", "5", y, m) && !calendrier_mois_lire("2101", "5", y, m) &&
               !calendrier_mois_lire("2026", "0", y, m) && !calendrier_mois_lire("2026", "13", y, m),
           "mois : hors bornes refusé");
    expect(!calendrier_mois_lire("", "", y, m) && y == 0 && m == 0, "mois : vide = 0, refusé");
    expect(calendrier_mois_lire("2026abc", "9x", y, m) && y == 2026 && m == 9, "mois [figé] : atoi ignore la fin");
}

static void test_calendrier_champ() {
    const std::string h = "08:00-16:00||14:00-22:00";
    expect(calendrier_champ(h, 0, '|') == "08:00-16:00" && calendrier_champ(h, 1, '|').empty() &&
               calendrier_champ(h, 2, '|') == "14:00-22:00",
           "champ : champs vides comptés");
    expect(calendrier_champ(h, 3, '|').empty() && calendrier_champ(h, 30, '|').empty(), "champ : au-delà = \"\"");
    expect(calendrier_champ("a~b~", 2, '~').empty() && calendrier_champ("a~b~", 1, '~') == "b", "champ : « ~ »");
    expect(calendrier_champ("", 0, '|').empty(), "champ : texte vide");
    expect(calendrier_champ("abc", -1, '|') == "abc", "champ [figé] : index négatif = premier champ");
}

static void test_calendrier_code() {
    const std::string c = "0A1fFF";
    expect(calendrier_code_jour(c, 1) == 0x0A && calendrier_code_jour(c, 2) == 0x1F && calendrier_code_jour(c, 3) == 0xFF,
           "code : hexadécimal, casse libre");
    expect(calendrier_code_jour(c, 4) == 0 && calendrier_code_jour("0", 1) == 0, "code : trop court = 0");
    expect(calendrier_code_jour("zG", 1) == 0 && calendrier_code_jour("1z", 1) == 0x10, "code : non hexadécimal = 0");
    expect(calendrier_code_jour(c, 0) == 0 && calendrier_code_jour(c, -5) == 0, "code : jour < 1 = 0");
}

static void test_calendrier_date() {
    int y = 0, m = 0, d = 0;
    expect(calendrier_date_lire("2026-09-17", y, m, d) && y == 2026 && m == 9 && d == 17, "date : ISO");
    expect(!calendrier_date_lire("", y, m, d) && !calendrier_date_lire("2026-09", y, m, d) &&
               !calendrier_date_lire("x", y, m, d),
           "date : incomplète refusée");
    expect(calendrier_date_lire("2026-13-40", y, m, d) && m == 13 && d == 40, "date [figé] : aucune borne");
    expect(calendrier_date_lire("2026-9-1T08:00", y, m, d) && d == 1, "date : la suite est ignorée");
}

static void test_calendrier_jour() {
    {
        LecteurJourCalendrier l("travail|Travail 08:00 – 16:00;rdv|Dentiste 14:30");
        CalJourLigne a = l.suivante();
        expect(a.type == CalJourType::LIGNE && std::strcmp(a.genre, "travail") == 0 &&
                   std::strcmp(a.texte, "Travail 08:00 – 16:00") == 0,
               "jour : première ligne");
        CalJourLigne b = l.suivante();
        expect(b.type == CalJourType::LIGNE && std::strcmp(b.genre, "rdv") == 0, "jour : seconde ligne");
        expect(l.suivante().type == CalJourType::FIN, "jour : fin");
    }
    {
        LecteurJourCalendrier l(";;sans;vide|;|x;a|b|c");
        expect(l.suivante().type == CalJourType::AUTRE, "jour [figé] : « ;; » sauté, sans « | » = AUTRE");
        expect(l.suivante().type == CalJourType::AUTRE, "jour : texte vide = AUTRE");
        CalJourLigne x = l.suivante();
        expect(x.type == CalJourType::LIGNE && x.genre[0] == '\0' && std::strcmp(x.texte, "x") == 0,
               "jour : genre vide gardé");
        CalJourLigne c = l.suivante();
        expect(c.type == CalJourType::LIGNE && std::strcmp(c.texte, "b|c") == 0, "jour : texte = le reste");
        expect(l.suivante().type == CalJourType::FIN, "jour : fin après");
    }
    {
        std::string longue = "rdv|" + std::string(kCalendrierJourMax, 'x');
        LecteurJourCalendrier l(longue.c_str());
        CalJourLigne a = l.suivante();
        expect(a.type == CalJourType::LIGNE && std::strlen(a.texte) == kCalendrierJourMax - 1 - 4,
               "jour : coupé à 1 023 octets");
    }
    {
        LecteurJourCalendrier l("");
        expect(l.suivante().type == CalJourType::FIN, "jour : vide");
    }
}

// ════════════════════════════════════════════════════════════════════════════
// 6. Emplacements et zones
// ════════════════════════════════════════════════════════════════════════════

static bool champ_vaut(const Champ& c, const char* s) {
    return c.n == std::strlen(s) && std::memcmp(c.p, s, c.n) == 0;
}

static void test_emplacements() {
    {
        const std::string p = "a|1;b;;c|2|3";
        size_t debut = 0;
        EmplacementLu e;
        expect(emplacement_suivant(p, debut, e) && e.a_cle && champ_vaut(e.cle, "a") && champ_vaut(e.reste, "1"),
               "emplacements : clé|reste");
        expect(emplacement_suivant(p, debut, e) && !e.a_cle && e.cle.n == 0 && e.reste.n == 0,
               "emplacements : sans « | », sans clé");
        expect(emplacement_suivant(p, debut, e) && !e.a_cle, "emplacements : « ;; » rendu vide (pas sauté)");
        expect(emplacement_suivant(p, debut, e) && champ_vaut(e.cle, "c") && champ_vaut(e.reste, "2|3"),
               "emplacements : le reste garde ses « | »");
        expect(!emplacement_suivant(p, debut, e), "emplacements : fin");
    }
    {
        const std::string p = "a;b|1";  // le « | » est dans l'enregistrement suivant
        size_t debut = 0;
        EmplacementLu e;
        expect(emplacement_suivant(p, debut, e) && !e.a_cle, "emplacements : « | » d'après ignoré");
        expect(emplacement_suivant(p, debut, e) && champ_vaut(e.cle, "b"), "emplacements : puis b");
    }
    {
        const std::string p = "a|1;";
        size_t debut = 0;
        EmplacementLu e;
        expect(emplacement_suivant(p, debut, e) && !emplacement_suivant(p, debut, e),
               "emplacements : « ; » final sans enregistrement vide");
        const std::string vide;
        debut = 0;
        expect(!emplacement_suivant(vide, debut, e), "emplacements : payload vide");
    }
    {
        const std::string p("a|x\0y;b|2", 9);  // zéro au milieu : lu sur la longueur
        size_t debut = 0;
        EmplacementLu e;
        expect(emplacement_suivant(p, debut, e) && e.reste.n == 3, "emplacements : zéro gardé dans le reste");
        expect(emplacement_suivant(p, debut, e) && champ_vaut(e.cle, "b"), "emplacements : la suite est lue");
    }
}

static void test_emplacement_etat_valeur() {
    struct Cas {
        const char* reste;
        const char* etat;
        const char* valeur;
    };
    const Cas cas[] = {
        {"on|180", "on", "180"},
        {"on", "on", ""},
        {"on|", "on", ""},
        {"on|1|2", "on", "1|2"},
        {"", "", ""},
        {"|5", "", "5"},
    };
    for (const Cas& c : cas) {
        const Champ reste{c.reste, std::strlen(c.reste)};
        Champ etat, valeur;
        emplacement_etat_valeur(reste, etat, valeur);
        char msg[96];
        std::snprintf(msg, sizeof(msg), "état|valeur : « %s »", c.reste);
        expect(champ_vaut(etat, c.etat) && champ_vaut(valeur, c.valeur), msg);
    }
}

static void test_emplacement_nombre() {
    expect(emplacement_nombre("21.5") == 21.5f, "nombre : 21.5");
    expect(emplacement_nombre("21.5 °C") == 21.5f, "nombre : la fin est ignorée");
    expect(emplacement_nombre("-3") == -3.0f, "nombre : négatif");
    expect(std::isnan(emplacement_nombre("")), "nombre : vide = NAN");
    expect(std::isnan(emplacement_nombre("unavailable")), "nombre : illisible = NAN");
    expect(std::isnan(emplacement_nombre("inf")) && std::isnan(emplacement_nombre("-inf")),
           "nombre : « inf » = NAN (lot A)");
    expect(std::isnan(emplacement_nombre("nan")), "nombre : « nan » = NAN");
    expect(std::isnan(emplacement_nombre("1e99")), "nombre : hors des float = NAN");
    expect(emplacement_nombre("0x10") == 16.0f, "nombre [figé] : l'hexadécimal de strtof est lu");
}

static void test_solaire() {
    auto lire = [](const char* s) { return solaire_pourcent(s, std::strlen(s)); };
    expect(lire("45") == 45.0f, "solaire : 45");
    expect(lire("12.5%") == 12.5f, "solaire : la fin est ignorée");
    expect(lire("150") == 100.0f && lire("-5") == 0.0f, "solaire : borné à 0..100");
    expect(std::isnan(lire("")) && std::isnan(lire("abc")), "solaire : vide ou illisible = NAN");
    expect(std::isnan(lire("inf")) && std::isnan(lire("nan")), "solaire : non fini = NAN");
    expect(solaire_pourcent("45xyz", 2) == 45.0f, "solaire : lu sur n octets, sans zéro final");
    expect(lire("000000000000000099") == 0.0f, "solaire [figé] : coupé à 15 octets (« …99 » perdu)");
    expect(lire("1234567890123456789") == 100.0f, "solaire [figé] : long = coupé puis borné");
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
    test_pluie_niveau();
    test_pluie_barres();
    test_pluie_phrase();
    test_calendrier_mois();
    test_calendrier_champ();
    test_calendrier_code();
    test_calendrier_date();
    test_calendrier_jour();
    test_emplacements();
    test_emplacement_etat_valeur();
    test_emplacement_nombre();
    test_solaire();

    std::printf("=== %s (%d OK, %d FAIL) ===\n", g_fail ? "FAILED" : "ALL PASSED", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
