/**
 * Tests hôte de la lecture des payloads de Home Assistant (Tab5/socle/tab5_parse.h/.cpp,
 * lot F de l'audit du 30/09/2026), sans ESPHome ni LVGL : cas normaux, champs vides,
 * payloads tronqués, valeurs extrêmes, « nan » et « inf ».
 *
 * Extraction NEUTRE : ces tests figent le comportement de la boucle d'origine, travers
 * compris. Un cas marqué « [figé] » décrit un comportement discutable gardé tel quel
 * (atoi qui lit « 3x » comme 3, « 0x10 » lu par strtof…) : le changer est un changement
 * de contrat, dans une PR à part.
 * Un cas marqué « [corrigé] » affirme le bon comportement d'un des cinq défauts relevés
 * par le lot F et corrigés à part : phrase « @-1,0 », champs vides de la vigilance, index
 * illisible et nombres non finis des prévisions, production solaire coupée à 15 octets.
 * test_payloads_ha() garde, pour ces mêmes lecteurs, les sorties d'avant sur les payloads
 * tels que HA les envoie.
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

    // Index illisible : avant le correctif, atoi le lisait comme 0 et le créneau 0 était
    // écrasé. Désormais rien n'est écrit et l'enregistrement est compté (journalisé une fois
    // par l'appelant) ; les enregistrements lisibles du même payload passent.
    vider(h);
    h[0].heure_texte = "garde";
    int ign = previsions_heures_lire("abc|zz|sunny|1|2;|yy|sunny|1|2;1|11h|rainy|3|0", h);
    expect(h[0].heure_texte == "garde" && h[0].condition.empty() && ign == 2,
           "heures [corrigé] : index illisible ou vide = enregistrement ignoré, créneau 0 intact");
    expect(h[1].heure_texte == "11h" && h[1].temp == 3.0f, "heures [corrigé] : la suite du payload est lue");

    vider(h);
    ign = previsions_heures_lire("3x|a|b|1|2; 4|c|d|5|6", h);
    expect(h[3].heure_texte == "a" && h[4].heure_texte == "c" && ign == 0,
           "heures [figé] : index lu comme atoi le lisait (« 3x » = 3, blanc de tête sauté)");

    vider(h);
    ign = previsions_heures_lire("6|a|b|abc|", h);
    expect(h[6].heure_texte == "a" && h[6].temp == 0.0f && h[6].pluvio == 0.0f && ign == 0,
           "heures [figé] : nombre illisible ou vide = 0 (atof ; HA envoie 0 pour une valeur absente)");

    // Nombres non finis ou hors des bornes plausibles : enregistrement ignoré, le créneau
    // garde sa valeur d'avant (pas de « nan° » ni d'infini à l'écran).
    vider(h);
    previsions_heures_lire("7|10h|sunny|12|0.5;8|11h|sunny|13|0", h);
    ign = previsions_heures_lire("7|x|rainy|nan|1;8|y|rainy|2|inf", h);
    expect(h[7].heure_texte == "10h" && h[7].temp == 12.0f && h[7].pluvio == 0.5f && h[8].heure_texte == "11h" &&
               h[8].pluvio == 0.0f && ign == 2,
           "heures [corrigé] : « nan » et « inf » refusés, créneau inchangé");

    vider(h);
    ign = previsions_heures_lire("8|a|b|1e99|0;9|a|b|1|-1e99;10|a|b|-inf|0", h);
    expect(h[8].heure_texte.empty() && h[9].heure_texte.empty() && h[10].heure_texte.empty() && ign == 3,
           "heures [corrigé] : hors des float (« 1e99 ») et « -inf » refusés");

    vider(h);
    ign = previsions_heures_lire("0|a|b|-100|0;1|a|b|150|1000;2|a|b|-100.5|0;3|a|b|150.5|0;4|a|b|1|1000.5;5|a|b|1|-0.1",
                                 h);
    expect(h[0].temp == -100.0f && h[1].temp == 150.0f && h[1].pluvio == 1000.0f && h[2].heure_texte.empty() &&
               h[3].heure_texte.empty() && h[4].heure_texte.empty() && h[5].heure_texte.empty() && ign == 4,
           "heures [corrigé] : bornes -100..150 (°C ou °F) et pluie 0..1000 comprises, au-delà refusé");

    vider(h);
    expect(previsions_heures_lire("15|a|b|1|2;-1|a|b|nan|2", h) == 0,
           "heures : index hors de 0 à 14 ignoré sans être compté (comme avant)");

    vider(h);
    previsions_heures_lire("", h);
    expect(h[0].heure_texte.empty(), "heures : payload vide sans effet");

    // Plus long que le tampon (l'appelant le refuse avant) : la fin est ignorée.
    std::string long_payload(kPrevisionsMax - 4, 'x');
    long_payload += ";9|a|b|1|2";
    vider(h);
    previsions_heures_lire(long_payload.c_str(), h);
    expect(h[9].heure_texte.empty(), "heures : au-delà de kPrevisionsMax, ignoré");

    expect(previsions_premier_creneau("5|10h|a|1|2") == 5 && previsions_premier_creneau("0|x") == 0 &&
               previsions_premier_creneau("14|") == 14,
           "premier créneau : lu comme atoi le lisait");
    expect(previsions_premier_creneau("") < 0 && previsions_premier_creneau("x") < 0 &&
               previsions_premier_creneau("|10h") < 0 && previsions_premier_creneau("-3|") < 0 &&
               previsions_premier_creneau("15|") >= 15 && previsions_premier_creneau("99999999999999999999|") >= 15,
           "premier créneau [corrigé] : illisible = refusé par l'appelant (plus le bloc 0), hors bornes aussi");
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
    d[5].nom_jour = "garde";
    d[5].tmin = 4.0f;
    int ign = previsions_jours_lire("5|Sam|a|inf|-inf|0|0|0|h;6|Dim|a|nan|3|0|1|0|;7|Lun|a|1|1e99|0|0|0|", d, ancre);
    expect(d[5].nom_jour == "garde" && d[5].tmin == 4.0f && d[6].nom_jour.empty() && d[7].nom_jour.empty() && ign == 3,
           "jours [corrigé] : « inf », « nan », « 1e99 » refusés, jour inchangé");

    vider(d);
    ancre = -7;
    g_now = 1790000000;
    ign = previsions_jours_lire("x|Auj|a|1|2|0|0|0|;0|Auj|a|nan|2|0|0|0|;2|Mer|a|-3.5|151|0|0|0|", d, ancre);
    expect(d[0].nom_jour.empty() && d[2].nom_jour.empty() && ancre == -7 && ign == 3,
           "jours [corrigé] : index illisible ignoré (plus le jour 0), jour 0 refusé sans toucher l'ancre, "
           "température au-delà de 150 refusée");
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
    expect(std::strcmp(v.champs[2], "") == 0 && std::strcmp(v.champs[3], "Orange") == 0 &&
               std::strcmp(v.champs[4], "Vert") == 0,
           "vigilance [corrigé] : « || » garde le champ vide, les suivants restent à leur place (R6)");
    n = vigilance_actives(v, a);
    expect(n == 1 && a[0].phenomene == 1, "vigilance [corrigé] : l'orange reste sur l'inondation, pas sur le vent");

    vigilance_lire("|Rouge|Vert", v);
    expect(std::strcmp(v.champs[0], "") == 0 && std::strcmp(v.champs[1], "Rouge") == 0 &&
               std::strcmp(v.champs[2], "Vert") == 0,
           "vigilance [corrigé] : phrase vide, rien ne remonte");

    vigilance_lire("p|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Jaune|Rouge|Rouge", v);
    expect(std::strcmp(v.champs[12], "Jaune") == 0, "vigilance : au-delà de 13 champs, le 13e s'arrête au « | »");

    vigilance_lire("p|Vert|", v);
    expect(std::strcmp(v.champs[1], "Vert") == 0 && v.champs[2][0] == '\0' && v.champs[3][0] == '\0',
           "vigilance : « | » final, champ vide puis rien");

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
        expect(l.suivant().type == AlerteHaType::AUTRE, "bandeaux : « ;; » sauté, un champ = AUTRE");
        expect(l.suivant().type == AlerteHaType::AUTRE, "bandeaux : deux champs = AUTRE");
        expect(alerte_vaut(l.suivant(), "id", "n", "t"), "bandeaux : le texte s'arrête au « | » suivant");
        expect(alerte_vaut(l.suivant(), "id2", "", ""), "bandeaux : niveau et texte vides gardés");
        expect(l.suivant().type == AlerteHaType::FIN, "bandeaux : fin");
    }
    {
        // Un enregistrement vide ne décale rien (défaut 2 du lot F, faux positif ici) : chaque
        // alerte porte son id, et ses champs vides restent à leur place (split_fields).
        LecteurAlertesHa l("a|Rouge|x;;b||y");
        expect(alerte_vaut(l.suivant(), "a", "Rouge", "x") && alerte_vaut(l.suivant(), "b", "", "y") &&
                   l.suivant().type == AlerteHaType::FIN,
               "bandeaux : « ;; » et niveau vide, aucun décalage");
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

    // « ;; » sauté sans décalage (défaut 2 du lot F, faux positif ici) : chaque barre porte
    // son index, et une intensité vide reste vide.
    n = pluie_barres_lire("0|1;;1|;;2|3", b);
    expect(n == 3 && b[0].idx == 0 && b[0].niveau == 1 && b[1].idx == 1 && b[1].niveau == 0 && b[2].idx == 2 &&
               b[2].niveau == 3,
           "pluie : enregistrement ou intensité vides, aucun décalage");

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
    // tab5_push.yaml, déjà ainsi dans la 3.7.0) : niveau -1, « Pas de données » à l'écran
    // (rain_phrase_render). Avant le correctif, lu comme « @- » (aucune source, phrase vide).
    p = pluie_phrase_lire("@-1,0");
    expect(p.code && p.niveau == -1 && p.debut == 0, "phrase [corrigé] : « @-1,0 » = pas de données (niveau -1)");
    p = pluie_phrase_lire("@-1");
    expect(p.code && p.niveau == -1 && p.debut == 0, "phrase [corrigé] : « @-1 » sans virgule = pas de données");
    p = pluie_phrase_lire("@-x");
    expect(p.code && p.niveau == -2, "phrase : « @- » suivi d'autre chose qu'un chiffre = aucune source");
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
        expect(l.suivante().type == CalJourType::AUTRE, "jour : « ;; » sauté, sans « | » = AUTRE");
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
    {
        // « ;; » sauté sans décalage (défaut 2 du lot F, faux positif ici) : chaque ligne
        // porte son genre, l'écran ne compte que les LIGNE.
        LecteurJourCalendrier l("rdv|A;;ferie|B");
        CalJourLigne a = l.suivante();
        CalJourLigne b = l.suivante();
        expect(a.type == CalJourType::LIGNE && std::strcmp(a.genre, "rdv") == 0 && std::strcmp(a.texte, "A") == 0 &&
                   b.type == CalJourType::LIGNE && std::strcmp(b.genre, "ferie") == 0 && std::strcmp(b.texte, "B") == 0 &&
                   l.suivante().type == CalJourType::FIN,
               "jour : « ;; », aucun décalage");
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
    expect(lire("000000000000000099") == 99.0f, "solaire [corrigé] : plus coupé à 15 octets (« …99 » lu)");
    expect(lire("1234567890123456789") == 100.0f, "solaire : long mais lisible = lu puis borné");
    const std::string limite = "45" + std::string(kChampNombreMax - 2, '0');  // 31 octets
    expect(lire(limite.c_str()) == 100.0f && std::isnan(lire((limite + "0").c_str())),
           "solaire [corrigé] : au-delà de 31 octets refusé (NAN), pas coupé");
}

static bool piece(const char* cle, const char* reste, PieceClimatLu& lu) {
    return piece_climat_lire(Champ{cle, std::strlen(cle)}, Champ{reste, std::strlen(reste)}, lu);
}

static void test_piece_climat() {
    {
        PieceClimatLu lu;
        expect(piece("p2", "21.5|48|1", lu) && lu.piece == 2, "pièce : clé p2");
        expect(lu.temperature && lu.t == 21.5f && lu.humidite && lu.h == 48.0f && lu.clim,
               "pièce : température, humidité, clim");
    }
    {
        PieceClimatLu lu;
        expect(piece("p0", "19||0", lu) && lu.temperature && !lu.humidite && std::isnan(lu.h) && !lu.clim,
               "pièce : humidité vide = pas de sonde, clim 0");
    }
    {
        PieceClimatLu lu;
        expect(piece("p4", "nan|abc|", lu) && lu.temperature && std::isnan(lu.t) && lu.humidite &&
                   std::isnan(lu.h) && !lu.clim,
               "pièce : « nan » ou illisible = sonde déclarée, valeur inconnue");
    }
    {
        PieceClimatLu lu;
        expect(piece("p1", "inf|-inf|1", lu) && std::isnan(lu.t) && std::isnan(lu.h) && lu.clim,
               "pièce : non fini = inconnu");
    }
    {
        PieceClimatLu lu;
        expect(piece("p3", "", lu) && !lu.temperature && !lu.humidite && !lu.clim && std::isnan(lu.t),
               "pièce : rien de déclaré (reste vide)");
        expect(piece("p3", "20", lu) && lu.temperature && !lu.humidite && !lu.clim,
               "pièce : champs absents = non déclarés");
        expect(piece("p3", "||1x", lu) && !lu.temperature && !lu.clim, "pièce : clim exactement « 1 »");
    }
    {
        PieceClimatLu lu;
        lu.piece = 9;
        expect(!piece("p5", "20|50|1", lu) && lu.piece == 9, "pièce : p5 hors des cinq pièces, rien changé");
        expect(!piece("pa", "20", lu) && !piece("p", "20", lu) && !piece("p00", "20", lu) &&
                   !piece("r0", "20", lu) && lu.piece == 9,
               "pièce : autre clé refusée");
    }
}

static ZoneGaucheLu gauche(const char* s) { return zone_gauche_lire(s, std::strlen(s)); }

static void test_zone_gauche() {
    constexpr uint8_t V = zone_gauche_bit(ZoneGauche::VOCAL);
    constexpr uint8_t G = zone_gauche_bit(ZoneGauche::GRAPHIQUE);
    constexpr uint8_t L = zone_gauche_bit(ZoneGauche::LECTEUR);
    {
        const ZoneGaucheLu z;
        expect(z.defaut == ZoneGauche::VOCAL && z.cycle == (V | G), "zone gauche : sans la clé, vocal puis graphique");
    }
    {
        const ZoneGaucheLu z = gauche("graphique|vocal|graphique");
        expect(z.defaut == ZoneGauche::GRAPHIQUE && z.cycle == (V | G), "zone gauche : graphique au départ, deux au tap");
    }
    {
        const ZoneGaucheLu z = gauche("vocal");
        expect(z.defaut == ZoneGauche::VOCAL && z.cycle == V, "zone gauche : rien au tap = le départ seul");
    }
    {
        const ZoneGaucheLu z = gauche("graphique|vocal");
        expect(z.cycle == (V | G), "zone gauche : le départ fait toujours partie du cycle");
    }
    {
        const ZoneGaucheLu z = gauche("");
        expect(z.defaut == ZoneGauche::VOCAL && z.cycle == V, "zone gauche : clé vide = le vocal seul");
    }
    {
        const ZoneGaucheLu z = gauche("radio|graphique|camera");
        expect(z.defaut == ZoneGauche::VOCAL && z.cycle == (V | G),
               "zone gauche : départ inconnu = vocal, code inconnu ignoré");
    }
    {
        const ZoneGaucheLu z = gauche("lecteur|graphique");
        expect(z.defaut == ZoneGauche::LECTEUR && z.cycle == (L | G), "zone gauche : lecteur lu et gardé (lot 2)");
    }
    {
        constexpr uint8_t C = zone_gauche_bit(ZoneGauche::CAPTEUR);
        const ZoneGaucheLu z = gauche("capteur|vocal|lecteur");
        expect(z.defaut == ZoneGauche::CAPTEUR && z.cycle == (C | V | L), "zone gauche : capteur lu et gardé (ADR-0054)");
    }
    {
        const ZoneGaucheLu z = gauche("Vocal|GRAPHIQUE| graphique|graphiques");
        expect(z.defaut == ZoneGauche::VOCAL && z.cycle == V, "zone gauche : codes exacts seulement");
    }
    {
        // Au-delà de kZoneGaucheChampsMax champs, la fin est ignorée.
        const ZoneGaucheLu z = gauche("vocal|vocal|vocal|vocal|vocal|vocal|vocal|vocal|graphique");
        expect(z.cycle == V, "zone gauche : champs au-delà du maximum ignorés");
    }
    {
        const ZoneGaucheLu z = zone_gauche_lire("graphique|vocalXYZ", 15);
        expect(z.defaut == ZoneGauche::GRAPHIQUE && z.cycle == (V | G), "zone gauche : lu sur n octets");
    }
    {
        const ZoneGaucheLu z = gauche("|||");
        expect(z.defaut == ZoneGauche::VOCAL && z.cycle == V, "zone gauche : champs vides");
    }
}

// ════════════════════════════════════════════════════════════════════════════
// 7. Clim
// ════════════════════════════════════════════════════════════════════════════

static int reglages(const char* s, ClimReglages& r, Champ& nom) {
    return clim_reglages_lire(s, std::strlen(s), r, nom);
}

static void test_clim_reglages() {
    {
        ClimReglages r;
        Champ nom;
        expect(reglages("17|31|1|°C|chdfebqsw|Salon", r, nom) == 6, "clim : six champs");
        expect(r.min == 17.0f && r.max == 31.0f && r.pas == 1.0f && !r.fahrenheit && r.recu,
               "clim : bornes, pas, °C");
        expect(std::strcmp(r.capacites, "chdfebqsw") == 0 && champ_vaut(nom, "Salon"), "clim : capacités et nom");
    }
    {
        ClimReglages r;
        std::strcpy(r.nom, "Ancien");
        Champ nom;
        expect(reglages("60|86|1|°F|cq", r, nom) == 5 && r.fahrenheit && nom.n == 0 && r.nom[0] == '\0',
               "clim : cinq champs, °F, nom vidé");
    }
    {
        ClimReglages r;
        Champ nom;
        expect(reglages("17|31|1|°C", r, nom) == 4 && !r.recu && r.min == 16.0f, "clim : moins de 5 champs, rien");
        expect(reglages("", r, nom) == 1 && !r.recu, "clim : vide, rien");
    }
    {
        ClimReglages r;
        Champ nom;
        reglages("16|30|0.5|°C|c|Salon|bis", r, nom);
        expect(champ_vaut(nom, "Salon|bis"), "clim : le nom prend tout le reste");
    }
    {
        ClimReglages r;
        Champ nom;
        reglages("-1e30|30|0.5|°C|c", r, nom);
        expect(r.min == 16.0f && r.max == 30.0f, "clim : borne hors plage ignorée (lot A)");
        reglages("30|16|0.5|°C|c", r, nom);
        expect(r.min == 16.0f && r.max == 30.0f, "clim : min ≥ max ignoré");
        reglages("nan|inf|nan|°C|c", r, nom);
        expect(r.min == 16.0f && r.max == 30.0f && r.pas == 0.5f, "clim : nan/inf = valeurs d'avant");
        reglages("-100|200|10|°C|c", r, nom);
        expect(r.min == -100.0f && r.max == 200.0f && r.pas == 10.0f, "clim : bornes et pas extrêmes acceptés");
        reglages("16|30|0|°C|c", r, nom);
        expect(r.pas == 10.0f, "clim : pas nul ignoré");
        reglages("16|30|11|°C|c", r, nom);
        expect(r.pas == 10.0f, "clim : pas > 10 ignoré");
        reglages("||||", r, nom);
        expect(r.min == 16.0f && r.max == 30.0f && r.pas == 10.0f && !r.fahrenheit && r.capacites[0] == '\0',
               "clim : champs vides = valeurs d'avant, aucune capacité");
    }
    {
        ClimReglages r;
        Champ nom;
        reglages("16|30|0.5|°C|C-h1d", r, nom);
        expect(std::strcmp(r.capacites, "hd") == 0, "clim : seules les minuscules sont gardées");
        reglages("16|30|0.5|°C|abcdefghijklmnopqrstuvwxyz", r, nom);
        expect(std::strlen(r.capacites) == 15, "clim : capacités bornées à 15 lettres");
    }
}

static void test_clim_etat() {
    auto lire = [](const char* s, ClimEtat& e) { clim_etat_lire(s, std::strlen(s), e); };
    {
        ClimEtat e;
        lire("21.5|20.8|cool|boost|auto|off", e);
        expect(e.consigne == 21.5f && e.piece == 20.8f && e.mode == "cool" && e.preset == "boost" &&
                   e.ventilation == "auto" && e.oscillation == "off",
               "clim état : six champs");
        lire("nan|x|heat", e);
        expect(std::isnan(e.consigne) && std::isnan(e.piece) && e.mode == "heat" && e.preset.empty() &&
                   e.oscillation.empty(),
               "clim état : nombres illisibles inconnus, modes absents vidés");
        lire("", e);
        expect(std::isnan(e.consigne) && e.mode.empty(), "clim état : vide");
        lire("inf|1e99", e);
        expect(std::isnan(e.consigne) && std::isnan(e.piece), "clim état : non fini = inconnu");
    }
    {
        ClimEtat e;
        lire("1|2|abcdefghijklmnopqrst|b|c|d|e", e);
        expect(e.mode.size() == kModeMax, "clim état : mode borné à 15 octets");
        expect(e.oscillation == "d|e", "clim état : le dernier champ prend le reste");
    }
}

// ════════════════════════════════════════════════════════════════════════════
// 8. Popup Température (tab5_maj_historique ; humidité : ADR-0047)
// ════════════════════════════════════════════════════════════════════════════

static Champ c_(const char* s) { return Champ{s, std::strlen(s)}; }

static Champ histo(const char* entete, const char* mesures, const char* prev, HistoriqueSerie& s) {
    return historique_lire(c_(entete), c_(mesures), c_(prev), s);
}

static void test_humidite_lire() {
    expect(humidite_lire(c_("48")) == 48 && humidite_lire(c_("47.6")) == 48 && humidite_lire(c_("0")) == 0 &&
               humidite_lire(c_("100")) == 100,
           "humidité : % arrondi");
    expect(humidite_lire(c_("")) == kHumiditeAucune && humidite_lire(c_("nan")) == kHumiditeAucune &&
               humidite_lire(c_("abc")) == kHumiditeAucune && humidite_lire(c_("inf")) == kHumiditeAucune,
           "humidité : vide, nan, illisible, non finie = aucune");
    expect(humidite_lire(c_("-1")) == kHumiditeAucune && humidite_lire(c_("100.4")) == kHumiditeAucune &&
               humidite_lire(c_("1e30")) == kHumiditeAucune,
           "humidité : hors de 0 à 100 = aucune");
}

static void test_historique() {
    static HistoriqueSerie s;  // ~2 Ko : hors de la pile
    {
        // Ce que pousse le package sans sonde d'humidité (6 champs) : l'écran d'avant.
        const Champ nom = histo("Serre|2026-06-15T07:00|60|1485|18.2|0", "17.1,16.8,17.5;16.9,16.6,17.2;;16.5,16.2,16.8",
                                "1500,19.4;1560,20.8,18.0,22.5;1620,22.1", s);
        expect(s.recue && champ_vaut(nom, "Serre") && s.nom[0] == '\0', "historique : nom rendu, pas copié");
        expect(s.debut_jour == jour_civil(2026, 6, 15) && s.debut_min == 420 && s.pas == 60 && s.maintenant == 1485 &&
                   s.actuel == 18.2f && !s.exterieur,
               "historique : en-tête");
        expect(!s.humidite && s.h_actuelle == kHumiditeAucune, "historique : 6 champs = pas d'humidité");
        expect(s.n == 4 && s.m[0].moy == 17.1f && s.m[0].mn == 16.8f && s.m[0].mx == 17.5f && std::isnan(s.m[2].moy) &&
                   std::isnan(s.m[2].mn) && s.m[3].mx == 16.8f && s.m[0].h_moy == kHumiditeAucune,
               "historique : créneaux, un vide");
        expect(s.np == 3 && s.p[0].minute == 1500 && s.p[0].moy == 19.4f && std::isnan(s.p[0].mn) &&
                   s.p[1].mn == 18.0f && s.p[1].mx == 22.5f && s.p[2].minute == 1620,
               "historique : prévision, mini et maxi d'un jour");
    }
    {
        // Avec une sonde d'humidité (ADR-0047) : 7e champ, trois champs de plus par créneau.
        histo("Bureau|2026-06-15T07:00|60|1485|22.8|0|45", "21.0,20.5,21.6,52,48,57;,,,55,50,61;19.9,19.5,20.4;", "", s);
        expect(s.humidite && s.h_actuelle == 45, "historique : humidité déclarée et actuelle");
        expect(s.n == 3 && s.m[0].h_moy == 52 && s.m[0].h_mn == 48 && s.m[0].h_mx == 57, "historique : humidité d'un créneau");
        expect(std::isnan(s.m[1].moy) && s.m[1].h_moy == 55 && s.m[1].h_mx == 61,
               "historique : humidité sans température dans un créneau");
        expect(s.m[2].moy == 19.9f && s.m[2].h_moy == kHumiditeAucune, "historique : température sans humidité");
        histo("Bureau|2026-06-15T07:00|60|1485|22.8|0|nan", "", "", s);
        expect(s.humidite && s.h_actuelle == kHumiditeAucune && s.n == 0, "historique : « nan » = déclarée, inconnue");
        histo("Bureau|2026-06-15T07:00|60|1485|22.8|0|", "", "", s);
        expect(!s.humidite, "historique : 7e champ vide = pas de sonde");
        histo("Bureau|2026-06-15T07:00|60|1485|22.8|1|150|x", "21,20,22,-5,101,abc", "", s);
        expect(s.exterieur && s.humidite && s.h_actuelle == kHumiditeAucune && s.m[0].h_moy == kHumiditeAucune &&
                   s.m[0].h_mn == kHumiditeAucune && s.m[0].h_mx == kHumiditeAucune && s.m[0].moy == 21.0f,
               "historique : humidités hors de 0 à 100 = aucune, la suite ignorée");
    }
    {
        // Bornes : date, pas, minutes, températures.
        histo("|2026-13-40T25:00|0|-5|1e30|0", "1e30,-2000,5", "", s);
        expect(s.debut_jour == jour_civil(2000, 1, 1) && s.debut_min == 0, "historique : date illisible = 2000-01-01 00:00");
        expect(s.pas == 60 && s.maintenant == 0 && std::isnan(s.actuel), "historique : pas, minute, actuel hors bornes");
        expect(std::isnan(s.m[0].moy) && std::isnan(s.m[0].mn) && s.m[0].mx == 5.0f, "historique : températures bornées");
        histo("x|2026-06-15T07:00|1441|abc|nan|0", "", "", s);
        expect(s.pas == 60 && s.maintenant == 0, "historique : pas > 1440, minute illisible");
        histo("x|2026-06-15T07:00|1440|1000000|20|0", "", "", s);
        expect(s.pas == 1440 && s.maintenant == 1000000, "historique : bornes hautes acceptées");
        histo("", "", "", s);
        expect(s.recue && s.n == 0 && s.np == 0 && s.pas == 60 && !s.humidite, "historique : tout vide");
    }
    {
        // Prévision hors de l'ordre ou illisible : sautée.
        histo("x|2026-06-15T07:00|60|10|20|0", "", "100,1;90,2;abc,3;-5,4;200,5;200,6;300", s);
        expect(s.np == 3 && s.p[0].minute == 100 && s.p[1].minute == 200 && s.p[1].moy == 5.0f && s.p[2].minute == 300 &&
                   std::isnan(s.p[2].moy),
               "historique : prévision hors de l'ordre ou illisible sautée");
    }
    {
        // Plafonds : 64 créneaux, 48 points.
        std::string m, p;
        for (int i = 0; i < 70; i++) m += "20,19,21;";
        for (int i = 0; i < 60; i++) p += std::to_string(100 + i) + ",20;";
        histo("x|2026-06-15T07:00|60|10|20|0", m.c_str(), p.c_str(), s);
        expect(s.n == kHistoriqueMesuresMax && s.np == kHistoriquePrevMax, "historique : 64 créneaux, 48 points au plus");
        // Remise à neuf : rien de la série d'avant ne reste.
        histo("y|2026-06-15T07:00|60|10|20|0", "", "", s);
        expect(s.n == 0 && s.np == 0 && std::isnan(s.m[0].moy), "historique : série remise à neuf");
    }
}

// ════════════════════════════════════════════════════════════════════════════
// 9. Lecteur de musique (ADR-0050)
// ════════════════════════════════════════════════════════════════════════════

static Champ ch(const char* s) { return Champ{s, s ? std::strlen(s) : 0}; }
static bool vaut(const Champ& c, const char* s) { return champ_est(c, s); }

static void test_lecteurs_lire() {
    LecteurListeLu l[kLecteursMax];
    int n = lecteurs_lire(ch("Salon|tv;Cuisine|speaker;Ampli|receiver;Tablette|"), l);
    expect(n == 4 && vaut(l[0].nom, "Salon") && l[0].genre == LecteurGenre::TV && l[1].genre == LecteurGenre::ENCEINTE &&
               l[2].genre == LecteurGenre::AMPLI && vaut(l[3].nom, "Tablette") && l[3].genre == LecteurGenre::AUTRE,
           "lecteurs : quatre lecteurs et leurs genres");
    n = lecteurs_lire(ch(";;A|tv;;B;"), l);
    expect(n == 2 && vaut(l[0].nom, "A") && vaut(l[1].nom, "B") && l[1].genre == LecteurGenre::AUTRE,
           "lecteurs : « ;; » sautés, genre absent = autre");
    n = lecteurs_lire(ch("a;b;c;d;e;f;g;h"), l);
    expect(n == kLecteursMax && vaut(l[5].nom, "f"), "lecteurs : kLecteursMax au plus");
    n = lecteurs_lire(ch("|tv"), l);
    expect(n == 1 && l[0].nom.n == 0 && l[0].genre == LecteurGenre::TV, "lecteurs : nom vide gardé");
    expect(lecteurs_lire(ch(""), l) == 0 && lecteurs_lire(Champ{nullptr, 0}, l) == 0, "lecteurs : vide = aucun");
    expect(lecteur_genre_lire(ch("TV")) == LecteurGenre::AUTRE, "lecteurs : genre sensible à la casse");
}

static void test_lecteur_etat() {
    LecteurEtatLu e;
    const char* p =
        "1|Salon|tv|playing|Bohemian Rhapsody|Queen|A Night at the Opera|Spotify|83.5|354|42|0|1|all|lspnvmar|"
        "/api/media_player_proxy/media_player.salon?token=abc&cache=12";
    expect(lecteur_etat_lire(ch(p), e), "état : lu");
    expect(e.actif == 1 && vaut(e.nom, "Salon") && e.genre == LecteurGenre::TV && e.etat == LecteurEtat::LECTURE,
           "état : index, nom, genre, état");
    expect(vaut(e.titre, "Bohemian Rhapsody") && vaut(e.artiste, "Queen") && vaut(e.album, "A Night at the Opera") &&
               vaut(e.app, "Spotify"),
           "état : textes");
    expect(e.position == 83.5f && e.duree == 354.0f && e.volume == 42 && e.muet == 0 && e.aleatoire == 1 &&
               e.repetition == LecteurRepetition::TOUT,
           "état : nombres et drapeaux");
    expect(e.fonctions == (LECTEUR_F_LECTURE | LECTEUR_F_POSITION | LECTEUR_F_PRECEDENT | LECTEUR_F_SUIVANT |
                           LECTEUR_F_VOLUME | LECTEUR_F_MUET | LECTEUR_F_ALEATOIRE | LECTEUR_F_REPETITION),
           "état : fonctions");
    expect(vaut(e.image, "/api/media_player_proxy/media_player.salon?token=abc&cache=12"), "état : image");

    // Image en dernier : elle prend le reste, « | » compris.
    lecteur_etat_lire(ch("0|A||paused||||||||||||http://x/a|b"), e);
    expect(e.etat == LecteurEtat::PAUSE && vaut(e.image, "http://x/a|b"), "état : image avec « | »");

    // Inconnus : « - », vides, illisibles, hors bornes, non finis.
    lecteur_etat_lire(ch("-1|T||idle|||||-|-|nan|-|-|-||"), e);
    expect(e.actif == -1 && e.etat == LecteurEtat::INACTIF && std::isnan(e.position) && std::isnan(e.duree) &&
               e.volume == -1 && e.muet == -1 && e.aleatoire == -1 && e.repetition == LecteurRepetition::INCONNUE &&
               e.fonctions == 0 && e.image.n == 0,
           "état : champs inconnus");
    lecteur_etat_lire(ch("9|T||on|||||-5|1e99|101|2|x|ONE|zz"), e);
    expect(e.actif == -1 && e.etat == LecteurEtat::INACTIF && std::isnan(e.position) && std::isnan(e.duree) &&
               e.volume == -1 && e.muet == -1 && e.aleatoire == -1 && e.repetition == LecteurRepetition::INCONNUE &&
               e.fonctions == 0,
           "état : hors bornes = inconnu");
    lecteur_etat_lire(ch("2|T||playing|||||10|0|100|1|0|one|o"), e);
    expect(e.actif == 2 && std::isnan(e.duree) && e.position == 10.0f && e.volume == 100 && e.muet == 1 &&
               e.aleatoire == 0 && e.repetition == LecteurRepetition::UNE && e.fonctions == LECTEUR_F_ALLUMER,
           "état : durée 0 = direct, bornes atteintes");
    lecteur_etat_lire(ch("0|T||playing|||||||49.6"), e);
    expect(e.volume == 50, "état : volume arrondi");

    // Payload court : moins de quatre champs, rien.
    expect(!lecteur_etat_lire(ch("0|T|tv"), e) && e.etat == LecteurEtat::AUCUN, "état : trois champs refusés");
    expect(!lecteur_etat_lire(ch(""), e) && e.etat == LecteurEtat::AUCUN, "état : vide = aucun lecteur");
    expect(lecteur_etat_lire(ch("0|T||off"), e) && e.etat == LecteurEtat::ETEINT && e.titre.n == 0 && e.image.n == 0,
           "état : quatre champs suffisent");

    // États de HA.
    expect(lecteur_etat_code(ch("buffering")) == LecteurEtat::CHARGEMENT &&
               lecteur_etat_code(ch("standby")) == LecteurEtat::VEILLE &&
               lecteur_etat_code(ch("unavailable")) == LecteurEtat::INDISPONIBLE &&
               lecteur_etat_code(ch("unknown")) == LecteurEtat::INDISPONIBLE &&
               lecteur_etat_code(ch("")) == LecteurEtat::INDISPONIBLE,
           "état : codes de HA");
    expect(lecteur_fonctions_lire(ch("llx")) == LECTEUR_F_LECTURE && lecteur_fonctions_lire(Champ{nullptr, 3}) == 0,
           "état : lettres répétées ou inconnues");
}

static void test_lecteur_position() {
    LecteurEtatLu e;
    lecteur_etat_lire(ch("0|T||playing|||||100|200"), e);
    expect(lecteur_position(e, 5.0f) == 105.0f, "position : avancée en lecture");
    expect(lecteur_position(e, 500.0f) == 200.0f, "position : jamais au-delà de la durée");
    expect(lecteur_position(e, -3.0f) == 100.0f && lecteur_position(e, NAN) == 100.0f, "position : écoulé faux ignoré");
    lecteur_etat_lire(ch("0|T||paused|||||100|200"), e);
    expect(lecteur_position(e, 5.0f) == 100.0f, "position : figée en pause");
    lecteur_etat_lire(ch("0|T||playing|||||100|"), e);
    expect(lecteur_position(e, 5.0f) == 105.0f, "position : direct, sans borne de durée");
    lecteur_etat_lire(ch("0|T||playing|||||-|200"), e);
    expect(std::isnan(lecteur_position(e, 5.0f)), "position : inconnue");
    lecteur_etat_lire(ch("0|T||playing|||||999999|"), e);
    expect(lecteur_position(e, 1e30f) == kLecteurDureeMax, "position : bornée");
}

static void test_lecteur_temps() {
    char b[16];
    expect(lecteur_temps_texte(0.0f, b, sizeof(b)) && std::strcmp(b, "0:00") == 0, "temps : 0:00");
    expect(lecteur_temps_texte(83.9f, b, sizeof(b)) && std::strcmp(b, "1:23") == 0, "temps : 1:23");
    expect(lecteur_temps_texte(3599.0f, b, sizeof(b)) && std::strcmp(b, "59:59") == 0, "temps : 59:59");
    expect(lecteur_temps_texte(3725.0f, b, sizeof(b)) && std::strcmp(b, "1:02:05") == 0, "temps : 1:02:05");
    expect(lecteur_temps_texte(NAN, b, sizeof(b)) && std::strcmp(b, "-:--") == 0, "temps : inconnu");
    expect(lecteur_temps_texte(-1.0f, b, sizeof(b)) && std::strcmp(b, "-:--") == 0, "temps : négatif");
    expect(lecteur_temps_texte(2e6f, b, sizeof(b)) && std::strcmp(b, "-:--") == 0, "temps : hors bornes");
    expect(lecteur_temps_texte(kLecteurDureeMax, b, sizeof(b)) && std::strcmp(b, "277:46:40") == 0, "temps : maximum");
    char petit[4];
    expect(!lecteur_temps_texte(83.0f, petit, sizeof(petit)) && petit[0] == '\0', "temps : tampon trop petit");
    expect(!lecteur_temps_texte(83.0f, nullptr, 0), "temps : pas de tampon");
}

static void test_ha_base_depuis_hote() {
    char b[64];
    expect(ha_base_depuis_hote("192.0.2.10", b, sizeof(b)) && std::strcmp(b, "http://192.0.2.10:8123") == 0,
           "base : IPv4");
    expect(ha_base_depuis_hote("fd00::1", b, sizeof(b)) && std::strcmp(b, "http://[fd00::1]:8123") == 0, "base : IPv6");
    expect(!ha_base_depuis_hote("", b, sizeof(b)) && b[0] == '\0', "base : adresse vide refusée");
    expect(!ha_base_depuis_hote(nullptr, b, sizeof(b)) && b[0] == '\0', "base : adresse nulle refusée");
    expect(!ha_base_depuis_hote("fe80::1%eth0", b, sizeof(b)) && b[0] == '\0', "base : zone IPv6 refusée");
    expect(!ha_base_depuis_hote("ha.local/x", b, sizeof(b)), "base : nom ou chemin refusé");
    char petit[10];
    expect(!ha_base_depuis_hote("192.0.2.10", petit, sizeof(petit)) && petit[0] == '\0', "base : tampon trop petit");
    expect(!ha_base_depuis_hote("1111:2222:3333:4444:5555:6666:7777:8888:9999:a", b, sizeof(b)),
           "base : adresse trop longue");
}

static void test_ha_image_url() {
    char u[kLecteurUrlMax];
    auto url = [&](const char* image, const char* base) { return ha_image_url(ch(image), base, u, sizeof(u)); };
    expect(url("/api/media_player_proxy/media_player.x?token=a", "http://192.0.2.10:8123") &&
               std::strcmp(u, "http://192.0.2.10:8123/api/media_player_proxy/media_player.x?token=a") == 0,
           "url : chemin relatif");
    expect(url("api/x", "http://h:8123/") && std::strcmp(u, "http://h:8123/api/x") == 0, "url : « / » ajouté et retiré");
    expect(url("https://i.scdn.co/image/ab67", nullptr) && std::strcmp(u, "https://i.scdn.co/image/ab67") == 0,
           "url : URL complète sans base");
    expect(!url("/api/x", "") && u[0] == '\0', "url : base vide refusée");
    expect(!url("/api/x", "ftp://h") && u[0] == '\0', "url : base sans http refusée");
    expect(!url("", "http://h") && !url(nullptr, "http://h"), "url : image vide refusée");
    expect(!url("/a b", "http://h") && u[0] == '\0', "url : espace refusé");
    expect(!url("/a\nb", "http://h"), "url : saut de ligne refusé");
    const std::string zero("/a\0b", 4);
    expect(!ha_image_url(Champ{zero.data(), zero.size()}, "http://h", u, sizeof(u)), "url : zéro refusé");
    char petit[12];
    expect(!ha_image_url(ch("/abcdef"), "http://h", petit, sizeof(petit)) && petit[0] == '\0',
           "url : tampon trop petit");
}

// ════════════════════════════════════════════════════════════════════════════
// 10. Popup Caméras (tab5_maj_cameras, ADR-0049)
// ════════════════════════════════════════════════════════════════════════════

static void test_cameras_lire() {
    CameraLue c[kCamerasMax];
    auto lire = [&c](const std::string& s) { return cameras_lire(Champ{s.data(), s.size()}, c); };
    expect(lire("") == 0, "caméras : payload vide = aucune");
    const std::string deux = "Entrée|/api/camera_proxy/camera.entree?token=abc;Jardin|/api/camera_proxy/camera.jardin?token=def";
    expect(lire(deux) == 2 && champ_vaut(c[0].nom, "Entrée") &&
               champ_vaut(c[0].image, "/api/camera_proxy/camera.entree?token=abc") && champ_vaut(c[1].nom, "Jardin"),
           "caméras : deux caméras dans l'ordre");
    expect(lire(";;Garage|;|/img.jpg;Cour|/cour.jpg;") == 2 && c[0].nom.n == 0 && champ_vaut(c[0].image, "/img.jpg") &&
               champ_vaut(c[1].nom, "Cour"),
           "caméras : sans image sautée, nom vide gardé");
    expect(lire("Seule") == 0, "caméras : un nom sans image = aucune");
    expect(lire("A|/a|Salon|0|champ en trop") == 1 && champ_vaut(c[0].image, "/a") && champ_vaut(c[0].piece, "Salon") &&
               c[0].hors_ligne == 0,
           "caméras : champ en trop ignoré");
    std::string vingt;
    for (int i = 0; i < 20; i++) vingt += "C" + std::to_string(i) + "|/c" + std::to_string(i) + ";";
    expect(lire(vingt) == kCamerasMax && champ_vaut(c[kCamerasMax - 1].nom, "C15"), "caméras : 16 au plus");
}

// ADR-0056 : la pièce (area_name) et « hors ligne depuis » ; un blueprint d'avant
// (« nom|image ») reste lisible.
static void test_cameras_pieces() {
    CameraLue c[kCamerasMax];
    int np = -1;
    auto lire = [&c, &np](const std::string& s) { return cameras_lire(Champ{s.data(), s.size()}, c, &np); };
    expect(lire("") == 0 && np == 0, "pièces : aucune caméra, aucune pièce");
    expect(lire("A|/a;B|/b") == 2 && np == 1 && c[0].piece.n == 0 && c[1].piece_i == 0 && c[0].hors_ligne == 0,
           "pièces : blueprint d'avant, une seule pièce (vide)");
    expect(lire("A|/a|Jardin;B|/b|Entrée;C|/c|Jardin;D|/d") == 4 && np == 3 && c[0].piece_i == 0 &&
               c[1].piece_i == 1 && c[2].piece_i == 0 && c[3].piece_i == 2 && champ_vaut(c[1].piece, "Entrée"),
           "pièces : ordre de la première caméra, la pièce vide en dernier");
    expect(lire("D|/d|;A|/a|Jardin") == 2 && np == 2 && c[0].piece_i == 1 && c[1].piece_i == 0,
           "pièces : la pièce vide en dernier même si sa caméra est la première");
    expect(lire("A|/a|Jardin;B|/b|Jardin") == 2 && np == 1, "pièces : toutes dans la même pièce = une");
    expect(lire("A|/a|jardin;B|/b|Jardin") == 2 && np == 2, "pièces : comparées à l'octet près (casse)");
    expect(lire("Porte||Entrée|1760000000;Cour|/c|Cour|") == 2 && c[0].image.n == 0 &&
               c[0].hors_ligne == 1760000000u && c[1].hors_ligne == 0,
           "hors ligne : gardée sans image, horodatage lu ; vide = en ligne");
    expect(lire("A|/a||abc;B|/b||-5;C|/c||99999999999") == 3 && c[0].hors_ligne == 0 && c[1].hors_ligne == 0 &&
               c[2].hors_ligne == 0,
           "hors ligne : illisible, négatif ou trop grand = en ligne");
    expect(lire("A||Salon|0") == 0, "hors ligne : 0 sans image = sautée");
}

static void test_camera_url() {
    char u[kCameraUrlMax];
    auto url = [&u](const char* image, const char* base, int l = 960, int h = 540) {
        return camera_url(Champ{image, image ? std::strlen(image) : 0}, base, l, h, u, sizeof(u));
    };
    expect(url("/api/camera_proxy/camera.entree?token=abc", "http://192.0.2.10:8123") &&
               std::strcmp(u, "http://192.0.2.10:8123/api/camera_proxy/camera.entree?token=abc&width=960&height=540") ==
                   0,
           "url : chemin du proxy, taille ajoutée avec &");
    expect(url("/api/camera_proxy/camera.x", "https://ha.maison:8123/") &&
               std::strcmp(u, "https://ha.maison:8123/api/camera_proxy/camera.x?width=960&height=540") == 0,
           "url : « / » final de la base retiré, taille ajoutée avec ?");
    expect(url("local/porte.jpg", "http://h:8123") && std::strcmp(u, "http://h:8123/local/porte.jpg") == 0,
           "url : « / » ajouté, pas de taille hors du proxy");
    expect(url("http://cam.lan/snap.jpg", "") && std::strcmp(u, "http://cam.lan/snap.jpg") == 0,
           "url : URL complète telle quelle, sans base");
    expect(url("/api/camera_proxy/camera.x?width=640&height=360", "http://h:8123") &&
               std::strcmp(u, "http://h:8123/api/camera_proxy/camera.x?width=640&height=360") == 0,
           "url : taille déjà donnée gardée");
    expect(url("/api/camera_proxy/camera.x", "http://h:8123", 0, 540) &&
               std::strcmp(u, "http://h:8123/api/camera_proxy/camera.x") == 0,
           "url : sans largeur, pas de taille");
    expect(!url("/api/camera_proxy/camera.x", "") && u[0] == '\0', "url : chemin sans base refusé");
    expect(!url("/api/camera_proxy/camera.x", nullptr), "url : base nulle refusée");
    expect(!url("/api/camera_proxy/camera.x", "ftp://h"), "url : base hors http refusée");
    expect(!url("", "http://h:8123") && !url(nullptr, "http://h:8123"), "url : image vide refusée");
    expect(!url("/a b.jpg", "http://h:8123") && u[0] == '\0', "url : espace refusé");
    expect(!url("/a.jpg\r\nX: y", "http://h:8123"), "url : saut de ligne refusé");
    const std::string zero("/a\0b", 4);
    expect(!camera_url(Champ{zero.data(), zero.size()}, "http://h:8123", 960, 540, u, sizeof(u)), "url : zéro refusé");
    std::string longue = "/";
    longue += std::string(kCameraUrlMax, 'a');
    expect(!url(longue.c_str(), "http://h:8123") && u[0] == '\0', "url : trop longue, vidée");
    char petit[8];
    expect(!camera_url(Champ{"/a", 2}, "http://h", 1, 1, petit, sizeof(petit)) && petit[0] == '\0',
           "url : tampon trop petit, vidé");
}

// ════════════════════════════════════════════════════════════════════════════
// Payloads tels que HA les envoie : mêmes sorties qu'avant les correctifs du lot F
// ════════════════════════════════════════════════════════════════════════════

static void test_payloads_ha() {
    // Heures (packages/tab5_push.yaml, section 8 ; identique dans la 3.7.0) :
    // « i|HH:00|condition|temp|pluvio; » × 5, nombres écrits par Jinja après `| float(0)`,
    // créneau sans prévision = « i|00:00|unknown|0|0; ».
    HourForecastData h[15];
    vider(h);
    const char* heures =
        "5|14:00|partlycloudy|21.5|0.0;6|15:00|rainy|-3.0|1.2;7|16:00|clear-night|0.30000000000000004|1e-05;"
        "8|00:00|unknown|0|0;9|18:00|sunny|104.0|0.0;";
    int ign = previsions_heures_lire(heures, h);
    expect(ign == 0 && previsions_premier_creneau(heures) == 5, "HA heures : rien d'ignoré, bloc 5");
    expect(h[5].heure_texte == "14:00" && h[5].condition == "partlycloudy" && h[5].temp == 21.5f && h[5].pluvio == 0.0f &&
               h[6].temp == -3.0f && h[6].pluvio == static_cast<float>(1.2) &&
               h[7].temp == static_cast<float>(0.30000000000000004) && h[7].pluvio == static_cast<float>(1e-05) &&
               h[8].heure_texte == "00:00" && h[8].condition == "unknown" && h[8].temp == 0.0f && h[9].temp == 104.0f,
           "HA heures : mêmes valeurs qu'avec atoi/atof");

    // Jours (section 5) : « i|nom|condition|tmin|tmax|repos|dimanche|passé|heures; » × 15.
    DayForecastData d[15];
    vider(d);
    int32_t ancre = -7;
    g_now = 1790000000;
    ign = previsions_jours_lire(
        "0|Auj|rainy|8.5|15.0|0|0|0|08:00-16:00;1|Mar|sunny|-2.0|4.0|1|0|0|;14|Dim|unknown|0|0|1|1|1|;", d, ancre);
    expect(ign == 0 && ancre == local_day_number_today() && d[0].nom_jour == "Auj" && d[0].tmin == 8.5f &&
               d[0].tmax == 15.0f && d[0].heures_ouverture == "08:00-16:00" && d[1].tmin == -2.0f && d[1].est_repos &&
               d[14].condition == "unknown" && d[14].est_dimanche && d[14].est_passe && d[14].tmax == 0.0f,
           "HA jours : mêmes valeurs qu'avec atoi/atof, ancre posée");

    // Vigilance (script tab5_push_alertes) : code de pluie, globale, 11 phénomènes, jamais vides.
    VigilanceLue v;
    VigilanceActive a[kVigilanceActivesMax];
    vigilance_lire("@2,1790000600|Orange|Vert|Orange|Jaune|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Vert", v);
    const int n = vigilance_actives(v, a);
    expect(std::strcmp(v.champs[0], "@2,1790000600") == 0 && vigilance_niveau(v.champs[1]) == NiveauVigilance::ORANGE &&
               n == 2 && a[0].phenomene == 1 && a[1].phenomene == 2 && std::strcmp(v.champs[12], "Vert") == 0,
           "HA vigilance : 13 champs à leur place");
    vigilance_lire("@-1,0|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Vert|Vert", v);
    expect(std::strcmp(v.champs[0], "@-1,0") == 0 && v.champs[11][0] == '\0' && vigilance_actives(v, a) == 0,
           "HA vigilance : forme Météo-France (11 champs)");

    // Codes de pluie (packages/tab5_meteo_sources.yaml) autres que « @-1,0 » : inchangés.
    PluiePhrase p = pluie_phrase_lire("@-");
    expect(p.code && p.niveau == -2 && p.debut == 0, "HA pluie : « @- » = aucune source");
    p = pluie_phrase_lire("@0,0");
    expect(p.code && p.niveau == 0 && p.debut == 0, "HA pluie : « @0,0 » = temps sec");
    p = pluie_phrase_lire("@5,1790000600");
    expect(p.code && p.niveau == 5 && p.debut == 1790000600, "HA pluie : « @5,début »");

    // Production solaire (blueprint, solaire_pourcent) : entier de 0 à 100, ou « nan ».
    auto sol = [](const char* s) { return solaire_pourcent(s, std::strlen(s)); };
    expect(sol("0") == 0.0f && sol("50") == 50.0f && sol("100") == 100.0f && std::isnan(sol("nan")),
           "HA solaire : 0, 50, 100, nan");
}

// ════════════════════════════════════════════════════════════════════════════
// 10. Suivi de capteurs (ADR-0054)
// ════════════════════════════════════════════════════════════════════════════

static void test_suivis_lire() {
    SuiviLu s[kSuivisMax];
    int n = suivis_lire(ch("Tesla|382.7|USD|2.05|p|0,10,,100;CAC 40|7803.3301|EUR|-0.95|p|50"), s);
    expect(n == 2 && vaut(s[0].nom, "Tesla") && vaut(s[0].unite, "USD") && s[0].valeur == 382.7f &&
               s[0].decimales == 1 && s[0].genre == SuiviVariation::POURCENT && s[0].variation == 2.05f,
           "suivi : nom, valeur, unité, variation du jour");
    expect(s[0].n == 4 && s[0].points[0] == 0 && s[0].points[1] == 10 && s[0].points[2] == kSuiviPointAucun &&
               s[0].points[3] == 100,
           "suivi : points, un vide = aucune mesure");
    expect(s[1].decimales == 2 && s[1].n == 1 && s[1].points[0] == 50,
           "suivi : « 7803.3301 » montré à 3 décimales sans zéro de fin (2)");
    n = suivis_lire(ch("Serre|21.4|°C|1.5|a|"), s);
    expect(n == 1 && s[0].genre == SuiviVariation::ECART && s[0].n == 0, "suivi : écart, sans courbe");
    n = suivis_lire(ch("A|unknown||nan|p|x,101,-1,50.5,7"), s);
    expect(n == 1 && std::isnan(s[0].valeur) && s[0].genre == SuiviVariation::AUCUNE && std::isnan(s[0].variation) &&
               s[0].n == 5 && s[0].points[0] == kSuiviPointAucun && s[0].points[1] == kSuiviPointAucun &&
               s[0].points[2] == kSuiviPointAucun && s[0].points[3] == kSuiviPointAucun && s[0].points[4] == 7,
           "suivi : valeur et variation illisibles, points hors de 0 à 100 = aucune mesure");
    n = suivis_lire(ch("A|1|u|3|z|1;B|1e30|u|1e30|a|"), s);
    expect(n == 2 && s[0].genre == SuiviVariation::AUCUNE && std::isnan(s[1].valeur) &&
               s[1].genre == SuiviVariation::AUCUNE,
           "suivi : genre inconnu = aucune variation, au-delà de kSuiviValeurMax = inconnue");
    n = suivis_lire(ch(";;A|1;;B;C|2|u|1|p|1|2|3"), s);
    expect(n == 2 && vaut(s[0].nom, "A") && vaut(s[1].nom, "C") && s[1].n == 1,
           "suivi : « ;; » et un seul champ sautés, le dernier champ prend le reste");
    n = suivis_lire(ch("a|1;b|1;c|1;d|1;e|1;f|1;g|1"), s);
    expect(n == kSuivisMax && vaut(s[5].nom, "f"), "suivi : kSuivisMax au plus");
    std::string points;
    for (int i = 0; i < 40; i++) points += (i ? ",9" : "9");
    const std::string long_ = "A|1||||" + points;  // nom, valeur, trois champs vides, points
    n = suivis_lire(ch(long_.c_str()), s);
    expect(n == 1 && s[0].n == kSuiviPointsMax && s[0].points[kSuiviPointsMax - 1] == 9,
           "suivi : kSuiviPointsMax points au plus");
    expect(suivis_lire(ch(""), s) == 0 && suivis_lire(Champ{nullptr, 0}, s) == 0, "suivi : vide = aucun");
    expect(suivi_decimales(ch("12")) == 0 && suivi_decimales(ch("0.125")) == 3 && suivi_decimales(ch("1.5 °C")) == 1 &&
               suivi_decimales(ch("")) == 0 && suivi_decimales(Champ{nullptr, 0}) == 0,
           "suivi : décimales écrites par HA");
}

static void test_suivi_textes() {
    char t[24];
    expect(suivi_nombre_texte(382.7f, 1, t, sizeof(t)) && std::strcmp(t, "382.7") == 0, "suivi : 382.7");
    expect(suivi_nombre_texte(7803.33f, 2, t, sizeof(t)) && std::strcmp(t, "7803.33") == 0, "suivi : 7803.33");
    expect(suivi_nombre_texte(612.0f, 0, t, sizeof(t)) && std::strcmp(t, "612") == 0, "suivi : entier");
    expect(suivi_nombre_texte(NAN, 2, t, sizeof(t)) && std::strcmp(t, "--") == 0, "suivi : inconnue = --");
    expect(suivi_nombre_texte(1.0f, 9, t, sizeof(t)) && std::strcmp(t, "1.000") == 0, "suivi : décimales bornées");
    expect(!suivi_nombre_texte(7803.33f, 2, t, 4) && t[0] == '\0', "suivi : tampon trop petit = vide");
    expect(suivi_nombre_texte(-0.0001f, 2, t, sizeof(t)) && std::strcmp(t, "0.00") == 0, "suivi : jamais -0.00");
    expect(suivi_nombre_texte(-0.4f, 0, t, sizeof(t)) && std::strcmp(t, "0") == 0, "suivi : jamais -0");
    expect(suivi_nombre_texte(-0.006f, 2, t, sizeof(t)) && std::strcmp(t, "-0.01") == 0, "suivi : -0.01 garde son signe");
    expect(suivi_nombre_texte(-12.5f, 1, t, sizeof(t)) && std::strcmp(t, "-12.5") == 0, "suivi : négatif");
    expect(suivi_nombre_texte(33554432.0f, 2, t, sizeof(t)) && std::strcmp(t, "33554432") == 0,
           "suivi : au-delà de 2^24, sans décimale");
    SuiviLu s;
    s.genre = SuiviVariation::POURCENT;
    s.variation = 2.05f;
    expect(suivi_variation_texte(s, t, sizeof(t)) && std::strcmp(t, "+2.05 %") == 0 && suivi_sens(s) == 1,
           "suivi : hausse du jour");
    s.variation = -0.95f;
    expect(suivi_variation_texte(s, t, sizeof(t)) && std::strcmp(t, "-0.95 %") == 0 && suivi_sens(s) == -1,
           "suivi : baisse du jour");
    s.variation = -0.001f;
    expect(suivi_variation_texte(s, t, sizeof(t)) && std::strcmp(t, "0.00 %") == 0 && suivi_sens(s) == 0,
           "suivi : arrondie à zéro, ni signe ni sens");
    s.genre = SuiviVariation::ECART;
    s.decimales = 1;
    s.variation = 1.25f;
    expect(suivi_variation_texte(s, t, sizeof(t)) && std::strcmp(t, "+1.3") == 0 && suivi_sens(s) == 1,
           "suivi : écart aux décimales de la valeur");
    s.genre = SuiviVariation::AUCUNE;
    expect(suivi_variation_texte(s, t, sizeof(t)) && t[0] == '\0' && suivi_sens(s) == 0, "suivi : aucune variation");
    s.genre = SuiviVariation::POURCENT;
    s.variation = 1.0e12f;
    expect(!suivi_variation_texte(s, t, 8) && t[0] == '\0', "suivi : variation trop longue = vide");
}

// ════════════════════════════════════════════════════════════════════════════
// 11. Froid : réfrigérateurs et congélateurs (ADR-0055)
// ════════════════════════════════════════════════════════════════════════════

static void test_froid_lire() {
    FroidLu a[kFroidMax];
    int n = froid_lire(ch("Frigo cuisine|f|9.1|2|porte|1791381720|2.1|9.4|0|5|3.2,3.4,,4.1,9.1|chaud|1791300000|42|8.7;"
                          "Congélateur|c|-19.5|0|ok|0|-21|-18.2||-18|-20,-19.5|||"),
                       a);
    expect(n == 2 && vaut(a[0].nom, "Frigo cuisine") && a[0].type == FroidType::FRIGO && a[0].valeur == 9.1f &&
               a[0].niveau == 2 && a[0].cause == FroidCause::PORTE && a[0].depuis == 1791381720u,
           "froid : nom, type, valeur, niveau, cause, depuis");
    expect(a[0].min == 2.1f && a[0].max == 9.4f && a[0].bas == 0.0f && a[0].haut == 5.0f, "froid : min, max, norme");
    expect(a[0].n == 5 && a[0].points[0] == 3.2f && std::isnan(a[0].points[2]) && a[0].points[4] == 9.1f,
           "froid : points en °C, un vide = heure sans mesure");
    expect(a[0].dernier.cause == FroidCause::CHAUD && a[0].dernier.debut == 1791300000u &&
               a[0].dernier.duree_min == 42 && a[0].dernier.max == 8.7f,
           "froid : dernier incident");
    expect(a[1].type == FroidType::CONGELATEUR && std::isnan(a[1].bas) && a[1].haut == -18.0f &&
               a[1].dernier.cause == FroidCause::OK && a[1].dernier.debut == 0 && std::isnan(a[1].dernier.max),
           "froid : congélateur sans limite basse ni incident");
    n = froid_lire(ch("A|f||1|indispo|1791381720|||||3.2,4.1,"), a);
    expect(n == 1 && a[0].n == 3 && a[0].points[1] == 4.1f && std::isnan(a[0].points[2]),
           "froid : « , » final = valeur actuelle sans mesure, gardée comme dernier point");
    n = froid_lire(ch("A|f|1|0|ok|0|||||,"), a);
    expect(n == 1 && a[0].n == 2 && std::isnan(a[0].points[0]) && std::isnan(a[0].points[1]),
           "froid : « , » seul = deux heures sans mesure");
    n = froid_lire(ch("A|f||1|indispo|1791381720"), a);
    expect(n == 1 && std::isnan(a[0].valeur) && a[0].cause == FroidCause::INDISPO && a[0].n == 0 &&
               std::isnan(a[0].min) && std::isnan(a[0].haut),
           "froid : capteur muet, champs absents");
    n = froid_lire(ch("A|x|1;B;;C|c|nan|7|zzz|-1|1e30|-200|abc|81|x,90,-80.5|porte|-5|99999999|nan"), a);
    expect(n == 1 && vaut(a[0].nom, "C") && std::isnan(a[0].valeur) && a[0].niveau == 2 &&
               a[0].cause == FroidCause::OK && a[0].depuis == 0 && std::isnan(a[0].min) && std::isnan(a[0].max) &&
               std::isnan(a[0].bas) && std::isnan(a[0].haut),
           "froid : type inconnu et un seul champ sautés, niveau borné à 2, valeurs hors bornes inconnues");
    expect(std::isnan(a[0].points[0]) && std::isnan(a[0].points[1]), "froid : points illisibles ou hors bornes");
    expect(a[0].n == 3 && std::isnan(a[0].points[2]) && a[0].dernier.cause == FroidCause::PORTE &&
               a[0].dernier.debut == 0 && a[0].dernier.duree_min == 0 && std::isnan(a[0].dernier.max),
           "froid : incident aux champs illisibles");
    n = froid_lire(ch("A|f|-1|-1"), a);
    expect(n == 1 && a[0].niveau == 0 && a[0].valeur == -1.0f, "froid : niveau « -1 » = 0");
    n = froid_lire(ch("a|f;b|f;c|c;d|c;e|f"), a);
    expect(n == kFroidMax && vaut(a[3].nom, "d"), "froid : kFroidMax au plus");
    std::string points;
    for (int i = 0; i < 40; i++) points += (i ? ",4" : "4");
    const std::string long_ = "A|f|4|0|ok|0|||||" + points;
    n = froid_lire(ch(long_.c_str()), a);
    expect(n == 1 && a[0].n == kFroidPointsMax && a[0].points[kFroidPointsMax - 1] == 4.0f,
           "froid : kFroidPointsMax points au plus");
    expect(froid_lire(ch(""), a) == 0 && froid_lire(Champ{nullptr, 0}, a) == 0, "froid : vide = aucun appareil");
    expect(froid_cause(ch("chaud")) == FroidCause::CHAUD && froid_cause(ch("froid")) == FroidCause::FROID &&
               froid_cause(ch("porte")) == FroidCause::PORTE && froid_cause(ch("indispo")) == FroidCause::INDISPO &&
               froid_cause(ch("ok")) == FroidCause::OK && froid_cause(ch("Porte")) == FroidCause::OK &&
               froid_cause(ch("")) == FroidCause::OK,
           "froid : causes exactes, sinon ok");
}

static void test_froid_textes() {
    char t[16];
    expect(froid_temperature_texte(9.1f, t, sizeof(t)) && std::strcmp(t, "9.1") == 0, "froid : 9.1");
    expect(froid_temperature_texte(-18.0f, t, sizeof(t)) && std::strcmp(t, "-18.0") == 0, "froid : -18.0");
    expect(froid_temperature_texte(-0.04f, t, sizeof(t)) && std::strcmp(t, "0.0") == 0, "froid : jamais -0.0");
    expect(froid_temperature_texte(4.96f, t, sizeof(t)) && std::strcmp(t, "5.0") == 0, "froid : arrondi au dixième");
    expect(froid_temperature_texte(NAN, t, sizeof(t)) && std::strcmp(t, "--") == 0, "froid : inconnue = --");
    expect(froid_temperature_texte(500.0f, t, sizeof(t)) && std::strcmp(t, "--") == 0, "froid : hors bornes = --");
    expect(!froid_temperature_texte(-18.0f, t, 4) && t[0] == '\0', "froid : tampon trop petit = vide");

    FroidLu a;
    float bas = 0, haut = 0;
    expect(!froid_echelle(a, bas, haut), "froid : rien à tracer");
    a.bas = 0.0f;
    a.haut = 5.0f;
    a.valeur = 9.0f;
    a.n = 3;
    a.points[0] = 3.0f;
    a.points[1] = NAN;
    a.points[2] = 4.0f;
    expect(froid_echelle(a, bas, haut) && bas == -1.0f && haut == 10.0f, "froid : échelle des points, valeur et norme");
    FroidLu c;
    c.haut = -18.0f;
    c.valeur = -18.5f;
    expect(froid_echelle(c, bas, haut) && haut - bas == kFroidEchelleMin && bas < -19.5f && haut > -17.0f,
           "froid : échelle de kFroidEchelleMin au moins, centrée");

    FroidLu l[3];
    l[0].niveau = 1;
    l[1].niveau = 2;
    expect(froid_niveau_max(l, 3) == 2 && froid_niveau_max(l, 1) == 1 && froid_niveau_max(l, 0) == 0 &&
               froid_niveau_max(nullptr, 2) == 0,
           "froid : niveau le plus grave");

    AlerteTexteLu lu = alerte_texte_lire("@froid:porte:1:9.1:Frigo : cuisine");
    expect(lu.code == AlerteTexteCode::FROID, "libellé : @froid");
    FroidAlerteLu f;
    expect(froid_alerte_lire(lu.reste, f) && f.cause == FroidCause::PORTE && f.niveau == 1 && f.valeur == 9.1f &&
               std::strcmp(f.nom, "Frigo : cuisine") == 0,
           "froid : libellé codé, le nom prend le reste");
    expect(froid_alerte_lire("indispo:1::Congélateur", f) && f.cause == FroidCause::INDISPO && std::isnan(f.valeur) &&
               std::strcmp(f.nom, "Congélateur") == 0,
           "froid : libellé sans valeur");
    expect(!froid_alerte_lire("chaud:2:9", f) && !froid_alerte_lire(nullptr, f) && f.nom[0] == '\0',
           "froid : libellé incomplet refusé");
}

// Télécommandes (ADR-0056) : « telecommandes|écran|nom|… », paires complètes, 4 au plus.
static int telecommandes(const char* s, TelecommandeLue t[kTelecommandesMax]) {
    return telecommandes_lire(s, std::strlen(s), t);
}

static bool nom_est(const TelecommandeLue& t, const char* mot) { return champ_est(t.nom, mot); }

static void test_telecommandes() {
    TelecommandeLue t[kTelecommandesMax];
    expect(telecommandes("", t) == 0 && telecommandes_lire(nullptr, 3, t) == 0, "télécommandes : vide = aucune");
    expect(telecommandes("tv|TV Samsung|boitier|Apple TV|boitier|Freebox Player", t) == 3 &&
               t[0].ecran == TelecommandeEcran::TV && nom_est(t[0], "TV Samsung") &&
               t[1].ecran == TelecommandeEcran::BOITIER && nom_est(t[1], "Apple TV") &&
               t[2].ecran == TelecommandeEcran::BOITIER && nom_est(t[2], "Freebox Player"),
           "télécommandes : trois, dans l'ordre");
    expect(telecommandes("boitier|A|tv|B|boitier|C|tv|D|boitier|E", t) == kTelecommandesMax && nom_est(t[3], "D"),
           "télécommandes : quatre au plus, la fin ignorée");
    expect(telecommandes("magnetoscope|X", t) == 1 && t[0].ecran == TelecommandeEcran::TV,
           "télécommandes : écran inconnu = tv");
    expect(telecommandes("boitier|A|tv", t) == 1 && telecommandes("boitier", t) == 0,
           "télécommandes : paires complètes seulement");
    expect(telecommandes("boitier|", t) == 1 && t[0].nom.n == 0, "télécommandes : nom vide gardé vide");
    expect(std::strcmp(telecommande_emplacement(0), "tv") == 0 && std::strcmp(telecommande_emplacement(1), "tv1") == 0 &&
               std::strcmp(telecommande_emplacement(3), "tv3") == 0 &&
               std::strcmp(telecommande_emplacement(4), "tv") == 0 &&
               std::strcmp(telecommande_emplacement(-1), "tv") == 0,
           "télécommandes : emplacements tv, tv1… ; hors bornes = tv");
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
    test_piece_climat();
    test_zone_gauche();
    test_clim_reglages();
    test_clim_etat();
    test_humidite_lire();
    test_historique();
    test_lecteurs_lire();
    test_lecteur_etat();
    test_lecteur_position();
    test_lecteur_temps();
    test_ha_base_depuis_hote();
    test_ha_image_url();
    test_cameras_lire();
    test_cameras_pieces();
    test_camera_url();
    test_suivis_lire();
    test_suivi_textes();
    test_froid_lire();
    test_froid_textes();
    test_telecommandes();
    test_payloads_ha();

    std::printf("=== %s (%d OK, %d FAIL) ===\n", g_fail ? "FAILED" : "ALL PASSED", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
