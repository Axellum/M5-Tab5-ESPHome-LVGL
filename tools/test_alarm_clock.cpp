/**
 * Tests hôte du moteur de réveil (Tab5/alarm_clock.cpp + Tab5/tab5_core.cpp),
 * sans ESPHome ni LVGL — audit du 25/09/2026, lot 8b.
 *
 * Le réveil est la seule fonction du Tab5 qui doit marcher sans Home Assistant,
 * et le seul bug grave qu'on y ait trouvé avant ce test (heure fixe à 00:01,
 * 05/08/2026) n'avait été attrapé que par un warning du compilateur. Ici on
 * rejoue les cas qui comptent avec une horloge simulée, fuseau Europe/Paris :
 * heure fixe, jours cochés, sonnerie consommée une seule fois, répétition,
 * arrêt, fenêtre de grâce au démarrage, mode embauche (délai, bornes, repos
 * minimum), calendrier daté par son jour d'ancrage (bug §2.2 de l'audit),
 * changements d'heure, rendez-vous. Aussi les conversions des nombres reçus de HA
 * (tab5_float_vers_int : « inf » ou « 1e30 » → entier borné, lot A de l'audit du 30/09),
 * et la décision « batterie de la tablette montée ou pas » (batterie_lecture, 05/10),
 * puis les lignes « Batterie » et « Charge CPU » de la console système (06/10),
 * et les règles du mode économie d'énergie (tab5_economie.h, header seul, 06/10) :
 * sur batterie au courant de l'INA226, batterie basse, plafond de luminosité.
 *
 * Build & run (CI, job `python`) :
 *   g++ -std=c++17 -O2 -Wall -Wextra -I Tab5 -o test_alarm_clock \
 *       tools/test_alarm_clock.cpp Tab5/alarm_clock.cpp Tab5/tab5_core.cpp
 *   ./test_alarm_clock
 * Le poste de dev n'a qu'un cross-compilateur RISC-V ; vérif locale possible :
 *   riscv32-esp-elf-g++ -std=c++17 -fsyntax-only -I Tab5 tools/test_alarm_clock.cpp
 */
#include "alarm_clock.h"
#include "tab5_core.h"
#include "tab5_economie.h"
#include "tab5_i18n.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

// Globaux que le firmware définit dans tab5_custom.cpp.
DayForecastData cal_jours_data[15];
HourForecastData cal_heures_data[15];
int32_t cal_jours_anchor_day = -1;

// ── Horloge simulée ─────────────────────────────────────────────────────────
static time_t g_now = 0;
static time_t fake_time(time_t* out) {
    if (out != nullptr) *out = g_now;
    return g_now;
}

// Epoch local (Europe/Paris) d'une heure murale.
static time_t at(int y, int mo, int d, int h, int mi, int s = 0) {
    struct tm t;
    std::memset(&t, 0, sizeof(t));
    t.tm_year = y - 1900;
    t.tm_mon = mo - 1;
    t.tm_mday = d;
    t.tm_hour = h;
    t.tm_min = mi;
    t.tm_sec = s;
    t.tm_isdst = -1;
    return mktime(&t);
}

// Fixe « maintenant » pour le moteur ET pour les fonctions de dates.
static time_t now_is(time_t t) {
    g_now = t;
    return t;
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

static void expect_str(const std::string& got, const char* want, const char* msg) {
    if (got == want) {
        g_ok++;
    } else {
        g_fail++;
        std::printf("FAIL : %s\n       obtenu  « %s »\n       attendu « %s »\n", msg, got.c_str(), want);
    }
}

static void expect_ts(time_t got, time_t want, const char* msg) {
    if (got == want) {
        g_ok++;
        return;
    }
    g_fail++;
    char a[32] = "0", b[32] = "0";
    struct tm t;
    if (got != 0 && localtime_r(&got, &t) != nullptr) std::strftime(a, sizeof(a), "%Y-%m-%d %H:%M:%S", &t);
    if (want != 0 && localtime_r(&want, &t) != nullptr) std::strftime(b, sizeof(b), "%Y-%m-%d %H:%M:%S", &t);
    std::printf("FAIL : %s\n       obtenu  %s\n       attendu %s\n", msg, a, b);
}

// ── Remise à zéro entre deux scénarios ──────────────────────────────────────
static void reset() {
    alarm_reset_state();
    rdv_clear();
    g_alarm_cfg = AlarmCfg{};
    for (auto& d : cal_jours_data) d = DayForecastData{};
    cal_jours_anchor_day = -1;
}

static void cfg_fixe(int minute, uint8_t mask) {
    g_alarm_cfg.enabled = true;
    g_alarm_cfg.mode = AlarmMode::FIXE;
    g_alarm_cfg.fixed_min = minute;
    g_alarm_cfg.days_mask = mask;
    alarm_invalidate();
}

// Jour `idx` du calendrier poussé par HA ; heures "" = repos si repos=true.
static void cal_day(int idx, const char* heures, bool repos) {
    cal_jours_data[idx].nom_jour = "J";
    cal_jours_data[idx].heures_ouverture = heures;
    cal_jours_data[idx].est_repos = repos;
}

// Poussée HA reçue « maintenant » : la case 0 est aujourd'hui.
static void cal_anchor_today() {
    cal_jours_anchor_day = local_day_number_today();
    alarm_invalidate();
}

// ════════════════════════════════════════════════════════════════════════════

static void test_dates() {
    now_is(at(2026, 9, 25, 12, 0));
    expect(local_day_number_today() == 20721, "vendredi 25/09/2026 = jour civil 20721");
    now_is(at(2026, 9, 28, 0, 30));
    expect(local_day_number_today() == 20724, "lundi 28/09/2026 00:30 = jour civil 20724");
    now_is(0);
    expect(local_day_number_today() == -1, "horloge pas encore synchronisée : -1");
    now_is(at(2019, 6, 1, 12, 0));
    expect(local_day_number_today() == -1, "année < 2020 = SNTP pas prêt : -1");

    now_is(at(2026, 9, 25, 12, 0));
    struct tm t;
    expect(!local_day_from_offset(15, t), "J+15 hors bornes");
    expect(local_day_from_offset(3, t) && t.tm_wday == 1 && t.tm_mday == 28, "J+3 depuis vendredi = lundi 28");
    expect_str(format_short_day_label(0), "Ven 25", "titre court");
    expect_str(format_long_day_label(0), "vendredi 25 septembre", "titre long");
    now_is(at(2026, 9, 30, 12, 0));
    expect_str(format_long_day_label(1), "jeudi 1er octobre", "« 1er » pour le premier du mois");
    expect(cal_is_early_shift("08:59-16:00") && !cal_is_early_shift("09:00-17:00"), "embauche tôt = avant 09:00");

    // Noms de jours et de mois : les seules tables du projet (lot 8d).
    expect_str(fr_day_short_utf8(0), "Dim", "jour abrégé, 0 = dimanche");
    expect_str(fr_day_short_utf8(6), "Sam", "jour abrégé, 6 = samedi");
    expect_str(fr_day_short_utf8(7), "", "jour abrégé hors bornes");
    expect_str(clock_month_short_utf8(8), "Ao\xC3\xBBt", "mois abrégé d'août (glyphe û)");
    expect_str(clock_month_short_utf8(13), "", "mois abrégé hors bornes");
    expect_str(fr_capitalized(fr_month_long_utf8(12)), "D\xC3\xA9" "cembre", "majuscule initiale d'un mois accentué");
    expect_str(fr_capitalized(fr_day_long_utf8(1)), "Lundi", "majuscule initiale d'un jour");
    expect_str(fr_capitalized(""), "", "majuscule d'une chaîne vide");

    // Case du calendrier pour J+n, recalée sur le jour du lot poussé par HA
    // (partagée par le réveil et le planning de la carte centrale).
    now_is(at(2026, 9, 25, 12, 0));
    cal_jours_anchor_day = -1;
    expect(cal_index_for_offset(0) == -1, "aucun lot daté : pas de case");
    cal_jours_anchor_day = local_day_number_today();           // lot du jour
    expect(cal_index_for_offset(0) == 0 && cal_index_for_offset(3) == 3, "lot du jour : case = décalage");
    expect(cal_index_for_offset(15) == -1 && cal_index_for_offset(-1) == -1, "hors des 15 cases : -1");
    now_is(at(2026, 9, 26, 0, 5));                              // HA muet depuis la veille
    expect(cal_index_for_offset(0) == 1 && cal_index_for_offset(13) == 14, "lot de la veille : décalé d'une case");
    expect(cal_index_for_offset(14) == -1, "lot de la veille : J+14 n'est plus couvert");
    now_is(at(2026, 10, 10, 12, 0));
    expect(cal_index_for_offset(0) == -1, "lot vieux de 15 jours : plus rien n'est couvert");
    cal_jours_anchor_day = -1;
}

static void test_heure_fixe_jours_coches() {
    reset();
    cfg_fixe(7 * 60, 0x1F);  // lundi → vendredi
    time_t now = now_is(at(2026, 9, 25, 6, 0));
    expect_ts(alarm_next_ring(now), at(2026, 9, 25, 7, 0), "vendredi 06:00 → aujourd'hui 07:00");
    expect_str(alarm_next_label(now), "Aujourd'hui 07:00", "libellé du jour même");

    now = now_is(at(2026, 9, 25, 20, 0));
    expect_ts(alarm_next_ring(now), at(2026, 9, 28, 7, 0), "vendredi soir → lundi (week-end décoché)");
    expect_str(alarm_next_label(now), "Lundi 07:00", "libellé avec le jour, majuscule initiale");

    now = now_is(at(2026, 9, 27, 22, 0));
    expect_str(alarm_next_label(now), "Demain 07:00", "dimanche soir → « Demain »");

    // Détail du popup : « 1er » pour le premier du mois (audit du 26/09/2026, §2.2 —
    // le détail refaisait son propre libellé et écrivait « jeudi 1 octobre »).
    now = now_is(at(2026, 9, 30, 20, 0));  // mercredi soir → jeudi 1er octobre
    expect_str(alarm_next_detail(now), "jeudi 1er octobre \xC2\xB7 en attente du calendrier",
               "détail : « 1er » pour le premier du mois");

    g_alarm_cfg.enabled = false;
    alarm_invalidate();
    expect(alarm_next_ring(now) == 0, "réveil éteint : aucune sonnerie");
    expect_str(alarm_next_label(now), "D\xC3\xA9sactiv\xC3\xA9", "libellé réveil éteint");

    g_alarm_cfg.enabled = true;
    g_alarm_cfg.days_mask = 0;
    alarm_invalidate();
    expect(alarm_next_ring(now) == 0, "aucun jour coché : aucune sonnerie");
    expect_str(alarm_next_label(now), "Aucune sonnerie pr\xC3\xA9vue", "libellé sans jour retenu");
}

static void test_sonne_une_seule_fois_puis_repetition() {
    reset();
    cfg_fixe(7 * 60, 0x7F);
    expect(!alarm_due(now_is(at(2026, 9, 25, 6, 59, 59))), "06:59:59 : pas encore");
    expect(alarm_due(now_is(at(2026, 9, 25, 7, 0, 0))), "07:00:00 : sonne");
    expect(!alarm_due(now_is(at(2026, 9, 25, 7, 0, 1))), "07:00:01 : ne re-sonne pas");
    expect_ts(alarm_next_ring(now_is(at(2026, 9, 25, 7, 0, 2))), at(2026, 9, 26, 7, 0),
              "la sonnerie consommée, la suivante est demain");

    // Répétition (snooze) de 9 min.
    alarm_snooze(at(2026, 9, 25, 7, 0, 0), 9);
    expect(alarm_snooze_count() == 1, "une répétition comptée");
    expect_str(alarm_next_label(now_is(at(2026, 9, 25, 7, 0, 10))), "R\xC3\xA9p\xC3\xA9tition 07:09 (9 min)",
               "la répétition passe devant le prochain réveil dans le libellé");
    expect(!alarm_due(now_is(at(2026, 9, 25, 7, 8, 59))), "répétition : pas avant 07:09");
    expect(alarm_due(now_is(at(2026, 9, 25, 7, 9, 0))), "répétition : sonne à 07:09");
    expect(!alarm_due(now_is(at(2026, 9, 25, 7, 9, 1))), "répétition consommée");

    // Arrêt pendant une répétition : plus rien aujourd'hui.
    alarm_snooze(at(2026, 9, 25, 7, 9, 1), 9);
    alarm_dismiss(at(2026, 9, 25, 7, 9, 5));
    expect(alarm_snooze_count() == 0, "arrêt : compteur de répétitions remis à zéro");
    expect(!alarm_due(now_is(at(2026, 9, 25, 7, 18, 1))), "arrêt : la répétition annulée ne sonne pas");
}

static void test_fenetre_de_grace_au_demarrage() {
    reset();
    cfg_fixe(7 * 60, 0x7F);
    expect(alarm_due(now_is(at(2026, 9, 25, 7, 1, 0))),
           "redémarrage à 07:01 : le réveil de 07:00 est rattrapé (grâce de 120 s)");
    reset();
    cfg_fixe(7 * 60, 0x7F);
    expect(!alarm_due(now_is(at(2026, 9, 25, 7, 5, 0))), "redémarrage à 07:05 : trop tard, pas de rattrapage");
    expect_ts(alarm_next_ring(g_now), at(2026, 9, 26, 7, 0), "… et la suivante est demain");
}

static void test_reglage_pose_dans_le_passe() {
    reset();
    cfg_fixe(8 * 60, 0x7F);
    expect(!alarm_due(now_is(at(2026, 9, 25, 7, 0, 30))), "07:00:30, réveil à 08:00 : rien");
    g_alarm_cfg.fixed_min = 7 * 60;  // « il est 07:00:30, je règle 07:00 »
    alarm_invalidate();
    expect(!alarm_due(now_is(at(2026, 9, 25, 7, 0, 31))), "réglage posé une minute dans le passé : ne sonne pas");
    expect_ts(alarm_next_ring(now_is(at(2026, 9, 25, 7, 0, 32))), at(2026, 9, 26, 7, 0), "… il sonnera demain");
}

static void test_mode_embauche() {
    reset();
    g_alarm_cfg.enabled = true;
    g_alarm_cfg.mode = AlarmMode::EMBAUCHE;
    g_alarm_cfg.lead_min = 90;
    g_alarm_cfg.earliest_min = 5 * 60;
    g_alarm_cfg.latest_min = 9 * 60;
    time_t now = now_is(at(2026, 9, 25, 4, 0));

    cal_day(0, "08:00-16:00", false);
    cal_anchor_today();
    expect_ts(alarm_next_ring(now), at(2026, 9, 25, 6, 30), "embauche 08:00 − 90 min = 06:30");
    expect_str(alarm_next_detail(now), "vendredi 25 septembre \xC2\xB7 Travail 08:00 \xE2\x80\x93 16:00",
               "détail : date + horaire du calendrier");

    cal_day(0, "06:00-14:00", false);
    alarm_invalidate();
    expect_ts(alarm_next_ring(now), at(2026, 9, 25, 5, 0), "04:30 relevé à la borne « au plus tôt » 05:00");

    cal_day(0, "11:30-19:00", false);
    alarm_invalidate();
    expect_ts(alarm_next_ring(now), at(2026, 9, 25, 9, 0), "10:00 ramené à la borne « au plus tard » 09:00");

    cal_day(0, "", false);  // travail confirmé, horaire inconnu (service commencé la veille)
    alarm_invalidate();
    expect_ts(alarm_next_ring(now), at(2026, 9, 25, 7, 0), "horaire inconnu : heure fixe");
}

static void test_repos_minimum_apres_fermeture() {
    reset();
    g_alarm_cfg.enabled = true;
    g_alarm_cfg.mode = AlarmMode::EMBAUCHE;
    g_alarm_cfg.lead_min = 90;
    g_alarm_cfg.earliest_min = 5 * 60;
    g_alarm_cfg.latest_min = 9 * 60;
    const time_t now = now_is(at(2026, 9, 25, 23, 0));
    cal_day(0, "13:00-22:30", false);  // vendredi : fermeture à 22:30
    cal_day(1, "07:00-15:00", false);  // samedi : embauche à 07:00
    cal_anchor_today();

    g_alarm_cfg.rest_hours = 0;
    alarm_invalidate();
    expect_ts(alarm_next_ring(now), at(2026, 9, 26, 5, 30), "sans repos minimum : 07:00 − 90 min = 05:30");

    g_alarm_cfg.rest_hours = 9;
    alarm_invalidate();
    expect_ts(alarm_next_ring(now), at(2026, 9, 26, 7, 30), "repos de 9 h après 22:30 : pas avant 07:30");

    g_alarm_cfg.rest_hours = 11;
    alarm_invalidate();
    expect_ts(alarm_next_ring(now), at(2026, 9, 26, 9, 0), "le repos ne fait jamais dépasser « au plus tard »");
}

// Bug §2.2 de l'audit du 25/09/2026 : HA muet depuis la veille, la case 0 est
// HIER. Avant le recalage par cal_jours_anchor_day, le samedi était lu dans la
// case du vendredi (travaillé) et le réveil sonnait un jour de repos.
static void test_calendrier_date_par_son_ancrage() {
    reset();
    g_alarm_cfg.enabled = true;
    g_alarm_cfg.mode = AlarmMode::TRAVAIL;
    g_alarm_cfg.rest_mode = AlarmRepos::SILENCE;
    g_alarm_cfg.fixed_min = 7 * 60;
    g_alarm_cfg.days_mask = 0x7F;

    now_is(at(2026, 9, 25, 12, 0));  // poussée HA du vendredi midi
    cal_day(0, "08:00-16:00", false);  // vendredi
    cal_day(1, "", true);              // samedi : repos
    cal_day(2, "10:00-18:00", false);  // dimanche
    cal_anchor_today();

    const time_t now = now_is(at(2026, 9, 26, 5, 0));  // samedi, HA muet depuis
    expect(alarm_calendar_ready(), "données d'hier : couvrent encore aujourd'hui");
    expect_ts(alarm_next_ring(now), at(2026, 9, 27, 7, 0), "samedi de repos sauté, dimanche travaillé retenu");
    expect_str(alarm_next_label(now), "Demain 07:00", "libellé");

    const time_t tard = now_is(at(2026, 10, 11, 5, 0));  // 16 jours sans poussée
    alarm_invalidate();
    expect(!alarm_calendar_ready(), "16 jours sans poussée : calendrier périmé");
    expect_ts(alarm_next_ring(tard), at(2026, 10, 11, 7, 0), "calendrier périmé : repli sur l'heure fixe");
    const std::string d = alarm_next_detail(tard);
    expect(d.find("en attente du calendrier") != std::string::npos, "détail : en attente du calendrier");
}

static void test_repos_selon_reglage() {
    reset();
    g_alarm_cfg.enabled = true;
    g_alarm_cfg.mode = AlarmMode::TRAVAIL;
    g_alarm_cfg.fixed_min = 6 * 60;
    g_alarm_cfg.days_mask = 0x7F;
    const time_t now = now_is(at(2026, 9, 25, 4, 0));
    cal_day(0, "", true);               // vendredi : repos
    cal_day(1, "08:00-16:00", false);   // samedi : travail
    cal_anchor_today();

    g_alarm_cfg.rest_mode = AlarmRepos::SILENCE;
    alarm_invalidate();
    expect_ts(alarm_next_ring(now), at(2026, 9, 26, 6, 0), "repos silencieux : on saute au samedi travaillé");

    g_alarm_cfg.rest_mode = AlarmRepos::FIXE;
    alarm_invalidate();
    expect_ts(alarm_next_ring(now), at(2026, 9, 25, 6, 0), "repos à heure fixe : sonne quand même le vendredi");

    g_alarm_cfg.days_mask = 0x1F & ~0x10;  // vendredi décoché
    alarm_invalidate();
    expect_ts(alarm_next_ring(now), at(2026, 9, 26, 6, 0),
              "jour de repos décoché : silence ; samedi décoché mais travaillé : sonne quand même");
}

static void test_calendrier_absent() {
    reset();
    g_alarm_cfg.enabled = true;
    g_alarm_cfg.mode = AlarmMode::TRAVAIL;
    g_alarm_cfg.fixed_min = 7 * 60;
    g_alarm_cfg.days_mask = 0x1F;
    const time_t now = now_is(at(2026, 9, 26, 5, 0));  // samedi, aucun calendrier reçu
    expect(!alarm_calendar_ready(), "rien reçu : calendrier pas prêt");
    expect_ts(alarm_next_ring(now), at(2026, 9, 28, 7, 0), "sans calendrier : heure fixe et jours cochés");
}

static void test_changements_d_heure() {
    reset();
    cfg_fixe(7 * 60, 0x7F);
    time_t now = now_is(at(2027, 3, 27, 22, 0));  // veille du passage à l'heure d'été
    time_t r = alarm_next_ring(now);
    expect_ts(r, at(2027, 3, 28, 7, 0), "nuit de mars : 07:00 à l'horloge murale");
    expect(r - now == 8 * 3600, "nuit de mars : 8 h réelles entre 22:00 et 07:00");

    reset();
    cfg_fixe(7 * 60, 0x7F);
    now = now_is(at(2026, 10, 24, 22, 0));  // veille du retour à l'heure d'hiver
    r = alarm_next_ring(now);
    expect_ts(r, at(2026, 10, 25, 7, 0), "nuit d'octobre : 07:00 à l'horloge murale");
    expect(r - now == 10 * 3600, "nuit d'octobre : 10 h réelles entre 22:00 et 07:00");
}

static void test_rendez_vous() {
    reset();
    const time_t rdv = at(2026, 9, 25, 14, 30);
    const std::string payload = std::to_string(static_cast<long long>(rdv)) + "|Dentiste~" +
                                std::to_string(static_cast<long long>(rdv + 3600)) + "|Kin\xC3\xA9";
    rdv_store(payload);
    std::string scr, spk;
    expect(!rdv_due(rdv - 15 * 60 - 1, 15, scr, spk), "15 min avant moins une seconde : rien");
    expect(rdv_due(rdv - 15 * 60, 15, scr, spk), "15 min avant : annonce");
    expect_str(scr, "14:30 \xC2\xB7 Dentiste (dans 15 min)", "bandeau");
    expect_str(spk, "Rappel : Dentiste, \xC3\xA0 14 heures 30, dans 15 minutes.", "phrase parlée");
    expect(!rdv_due(rdv - 15 * 60, 15, scr, spk), "déjà annoncé : pas deux fois");

    rdv_store(payload);  // HA repousse la même liste
    expect(!rdv_due(rdv - 10 * 60, 15, scr, spk), "nouvelle poussée : l'annonce faite reste faite (appariement par epoch)");
    expect_str(rdv_next_label(rdv - 60), "14:30 \xC2\xB7 Dentiste", "prochain rendez-vous");
    expect_str(rdv_next_label(rdv + 1), "15:30 \xC2\xB7 Kin\xC3\xA9", "une fois commencé, on passe au suivant");

    const time_t vieux = at(2026, 9, 25, 9, 0);
    rdv_store(std::to_string(static_cast<long long>(vieux)) + "|Oubli\xC3\xA9");
    expect(!rdv_due(vieux + 5 * 60, 15, scr, spk), "découvert 5 min après son début : marqué lu, pas annoncé");
    rdv_store("n'importe quoi~|~0|Z\xC3\xA9ro~");
    expect_str(rdv_next_label(rdv - 60), "", "entrées illisibles ignorées");
}

static void test_prereglages_et_volume() {
    expect(alarm_days_preset_index(0x7F) == 0, "0x7F = « Tous les jours »");
    expect(alarm_days_preset_index(0x1F) == 1, "0x1F = « Lundi-Vendredi »");
    expect(alarm_days_preset_index(0x55) == ALARM_DAYS_PRESET_COUNT - 1, "masque libre = « Personnalisé »");
    expect(alarm_days_preset_mask(ALARM_DAYS_PRESET_COUNT - 1) == -1, "« Personnalisé » ne change rien");

    expect(alarm_ring_gain(0.8f, 0, false) == 0.8f, "sans crescendo : plein volume d'emblée");
    expect(std::fabs(alarm_ring_gain(0.8f, 0, true) - 0.28f) < 1e-5f, "crescendo : 35 % au premier cycle");
    expect(alarm_ring_gain(0.8f, 3, true) < 0.8f, "crescendo : pas encore plein au 4e cycle");
    expect(alarm_ring_gain(0.8f, 10, true) == 0.8f, "crescendo : plafonné au volume cible");
    expect(alarm_ring_gain(1.5f, 0, false) == 1.0f, "volume borné à 1");
}

// Langue de l'écran (lot 4, 27/09/2026) : les mêmes titres en anglais, puis retour au
// français — la langue source, qui rend les clés telles quelles.
static void test_langue() {
    now_is(at(2026, 9, 30, 12, 0));
    expect_str(i18n_language_name(0), "Fran\xC3\xA7" "ais", "langue 0 = français (source)");
    expect_str(i18n_language_name(1), "English", "langue 1 = anglais");
    i18n_set_language(1);
    expect_str(format_short_day_label(0), "Wed 30", "titre court en anglais");
    expect_str(format_long_day_label(1), "Thursday, October 1", "titre long en anglais, sans « 1er »");
    expect_str(tr("Calendrier"), "Calendar", "texte d'écran traduit");
    expect_str(tr("texte absent des tables"), "texte absent des tables", "clé inconnue rendue telle quelle");
    expect_str(tr_ctx("mardi", "M"), "T", "contexte : M = mardi");
    expect_str(tr_ctx("mercredi", "M"), "W", "contexte : M = mercredi");
    expect_str(tr_ctx("clim", "Chaud"), "Heat", "contexte : mode chauffage de la clim");
    expect_str(tr("Chaud"), "Warm", "sans contexte : blanc chaud d'une lampe");
    expect_str(ha_day_name("Auj"), "Today", "nom de jour HA : aujourd'hui");
    expect_str(ha_day_name("Mar"), "Tue", "nom de jour HA : mardi");
    expect_str(ha_day_name("Mer 05"), "Mer 05", "nom de jour HA inconnu : inchangé");
    i18n_set_language(99);
    expect(i18n_language() == 0, "langue hors bornes : retour au français");
    expect_str(tr("Calendrier"), "Calendrier", "français : texte rendu tel quel");
    expect_str(format_long_day_label(1), "jeudi 1er octobre", "retour au français");
    expect_str(ha_day_name("Auj"), "Auj", "nom de jour HA : inchangé en français");
}

// Nombres reçus de HA (lot A, audit du 30/09/2026) : UBSan avait relevé des conversions
// float → int hors bornes (consigne de clim « inf », bornes « -1e30 », humidité, luminosité).
static void test_nombres_de_ha() {
    expect(std::isnan(tab5_fini_ou_nan(INFINITY)), "inf → NAN");
    expect(std::isnan(tab5_fini_ou_nan(-INFINITY)), "-inf → NAN");
    expect(std::isnan(tab5_fini_ou_nan(NAN)), "NAN reste NAN");
    expect(tab5_fini_ou_nan(21.5f) == 21.5f, "nombre fini inchangé");
    expect(tab5_fini_ou_nan(1e30f) == 1e30f, "grand nombre fini inchangé (borné plus tard)");
    expect(tab5_float_vers_int(21.9f, 7, 35, 0) == 21, "troncature, comme un cast");
    expect(tab5_float_vers_int(-0.5f, -10, 10, 3) == 0, "troncature vers zéro");
    expect(tab5_float_vers_int(INFINITY, 0, 255, 7) == 7, "inf → défaut");
    expect(tab5_float_vers_int(-INFINITY, 0, 255, 7) == 7, "-inf → défaut");
    expect(tab5_float_vers_int(NAN, 0, 255, 7) == 7, "NAN → défaut");
    expect(tab5_float_vers_int(1e30f, 0, 100, 0) == 100, "1e30 → borne haute");
    expect(tab5_float_vers_int(-1e30f, -100, 200, 0) == -100, "-1e30 → borne basse");
    expect(tab5_float_vers_int(2.56256e32f, 16, 30, 0) == 30, "valeur du fuzz → borne haute");
    expect(tab5_float_vers_int(255.0f, 0, 255, 0) == 255, "borne haute atteinte");
    expect(tab5_float_vers_int(0.0f, 0, 255, 9) == 0, "borne basse atteinte (pas le défaut)");
}

// Batterie de la tablette montée ou pas (discussion #278, 05/10/2026) : une lecture sous
// 6 V marque la batterie absente pendant 10 min. Cas mesurés : batterie ~7,2 V, sans
// batterie 4,2 V, 5,71 V stable ou 4,2 ↔ 8,39 V toutes les 1 à 3 min.
static void test_batterie_presence() {
    constexpr uint32_t MIN = 60u * 1000u;
    using P = PresenceBatterie;
    {
        DetectionBatterie d;
        expect(d.presence == P::INCONNUE, "batterie : inconnue avant la première lecture");
        expect(batterie_lecture(d, NAN, 0) == P::INCONNUE, "batterie : lecture ratée, toujours inconnue");
        expect(batterie_lecture(d, 7.2f, MIN) == P::PRESENTE, "batterie montée (7,2 V) : présente dès la 1re lecture");
        expect(batterie_lecture(d, 6.0f, 2 * MIN) == P::PRESENTE, "6,0 V pile : encore une batterie (0 %)");
        expect(batterie_lecture(d, NAN, 3 * MIN) == P::PRESENTE, "lecture ratée : décision gardée");
    }
    {
        DetectionBatterie d;
        expect(batterie_lecture(d, 5.71f, 0) == P::ABSENTE, "5,71 V stable : absente");
        for (uint32_t m = 1; m <= 30; m++)
            batterie_lecture(d, 5.71f, m * MIN);
        expect(d.presence == P::ABSENTE, "5,71 V pendant 30 min : toujours absente");
        expect(batterie_lecture(d, 4.2f, 31 * MIN) == P::ABSENTE, "4,2 V (USB) : absente");
    }
    {
        // Oscillation 4,2 ↔ 8,39 V : les lectures hautes ne suffisent pas à la faire revenir.
        DetectionBatterie d;
        const float lectures[] = {8.39f, 4.2f, 8.39f, 8.39f, 4.2f, 8.39f, 8.39f, 8.39f, 4.2f, 8.39f};
        expect(batterie_lecture(d, lectures[0], 0) == P::PRESENTE, "oscillation : 8,39 V d'abord, rien de bas vu");
        for (uint32_t m = 1; m < 10; m++)
            batterie_lecture(d, lectures[m], m * MIN);
        expect(d.presence == P::ABSENTE, "oscillation 4,2 ↔ 8,39 V : absente");
        expect(batterie_lecture(d, 8.39f, (8 + 9) * MIN) == P::ABSENTE, "9 min après la dernière basse : absente");
        expect(batterie_lecture(d, 8.39f, (8 + 10) * MIN) == P::PRESENTE, "10 min sans lecture basse : présente");
        expect(batterie_lecture(d, 5.9f, 19 * MIN) == P::ABSENTE, "une seule lecture basse suffit");
    }
    {
        // millis() reboucle après 49 jours : la fenêtre se compte quand même.
        DetectionBatterie d;
        const uint32_t avant = 0xFFFFFFFFu - 2 * MIN;
        batterie_lecture(d, 4.2f, avant);
        expect(batterie_lecture(d, 7.5f, avant + 5 * MIN) == P::ABSENTE, "rebouclage : 5 min après, absente");
        expect(batterie_lecture(d, 7.5f, avant + 10 * MIN) == P::PRESENTE, "rebouclage : 10 min après, présente");
    }
    expect(kBatterieTensionMin == 6.0f && kBatterieFenetreMs == 10u * MIN, "seuil 6,0 V, fenêtre 10 min");
}

// Console système, lignes « Batterie » et « Charge CPU » (discussion #278, 06/10/2026).
// La règle qui compte : jamais de pourcentage sans batterie détectée (le 8,39 V du
// chargeur ferait 100 %, le 5,71 V de l'USB 0 %), quel que soit le niveau reçu.
static void test_console_batterie_et_cpu() {
    using P = PresenceBatterie;
    char b[48];
    batterie_texte_console(b, sizeof(b), false, P::PRESENTE, 78.0f, 7.62f);
    expect_str(b, "Non mont\xC3\xA9" "e", "interrupteur éteint : « Non montée », même batterie détectée");
    batterie_texte_console(b, sizeof(b), true, P::ABSENTE, NAN, 5.71f);
    expect_str(b, "Sur USB", "sans batterie (5,71 V) : « Sur USB »");
    batterie_texte_console(b, sizeof(b), true, P::ABSENTE, 100.0f, 8.39f);
    expect_str(b, "Sur USB", "sans batterie, niveau 100 % reçu quand même : pas de pourcentage");
    batterie_texte_console(b, sizeof(b), true, P::ABSENTE, 0.0f, 4.2f);
    expect_str(b, "Sur USB", "sans batterie, niveau 0 % reçu quand même : pas de pourcentage");
    batterie_texte_console(b, sizeof(b), true, P::INCONNUE, 50.0f, 7.4f);
    expect_str(b, "--", "avant la première décision : « -- », ni niveau ni tension");
    batterie_texte_console(b, sizeof(b), true, P::PRESENTE, 78.4f, 7.623f);
    expect_str(b, "78% \xC2\xB7 7.62 V", "batterie : niveau puis tension");
    batterie_texte_console(b, sizeof(b), true, P::PRESENTE, NAN, 7.2f);
    expect_str(b, "-- \xC2\xB7 7.20 V", "batterie, niveau pas encore publié : la tension seule");
    batterie_texte_console(b, sizeof(b), true, P::PRESENTE, 100.0f, NAN);
    expect_str(b, "100%", "batterie, tension inconnue : le niveau seul");
    batterie_texte_console(b, 4, true, P::PRESENTE, 78.0f, 7.62f);
    expect(std::strlen(b) == 3, "tampon court : texte coupé, terminé");
    i18n_set_language(1);
    batterie_texte_console(b, sizeof(b), false, P::INCONNUE, NAN, NAN);
    expect_str(b, "Not fitted", "« Non montée » traduit en anglais");
    batterie_texte_console(b, sizeof(b), true, P::ABSENTE, NAN, NAN);
    expect_str(b, "On USB", "« Sur USB » traduit en anglais");
    i18n_set_language(0);

    // Charge d'un cœur : part de la fenêtre passée hors de la tâche inactive.
    expect(cpu_charge_pct(1000, 1000 + 2000000, 2000000) == 0, "cœur au repos toute la fenêtre : 0 %");
    expect(cpu_charge_pct(1000, 1000, 2000000) == 100, "tâche inactive jamais servie : 100 %");
    expect(cpu_charge_pct(0, 1500000, 2000000) == 25, "1,5 s de repos sur 2 s : 25 %");
    expect(cpu_charge_pct(0, 1990000, 2000000) == 1, "0,5 % arrondi à 1 %");
    expect(cpu_charge_pct(0, 2100000, 2000000) == 0, "repos compté un peu au-delà de la fenêtre : 0 %, pas négatif");
    expect(cpu_charge_pct(0xFFFFFF00u, 0x00000100u, 1000) == 49, "compteur 32 bits rebouclé : la différence compte");
    expect(cpu_charge_pct(0, 0, 0) == -1, "durée nulle : pas de mesure");
}

// Mode économie d'énergie (06/10/2026) : sur batterie d'après le courant de l'INA226
// (+ = décharge), batterie basse à 35 % (retour à 40 %), et la décision appliquée.
static void test_economie() {
    {
        EtatEconomie e;
        expect(!economie_courant_lu(e, 0.30f, false, false) && !e.sur_batterie,
               "éco : pas de batterie détectée, jamais sur batterie");
        expect(!economie_courant_lu(e, 0.30f, true, true) && !e.sur_batterie,
               "éco : en charge, jamais sur batterie (même avec du courant)");
        expect(economie_courant_lu(e, 0.30f, true, false) && e.sur_batterie, "éco : 300 mA de décharge → sur batterie");
        expect(!economie_courant_lu(e, 0.035f, true, false) && e.sur_batterie, "éco : 35 mA, entre les seuils, reste");
        expect(!economie_courant_lu(e, NAN, true, false) && e.sur_batterie, "éco : lecture ratée, rien ne change");
        expect(economie_courant_lu(e, 0.010f, true, false) && !e.sur_batterie, "éco : 10 mA → secteur");
        expect(!economie_courant_lu(e, 0.035f, true, false) && !e.sur_batterie, "éco : 35 mA depuis le secteur, reste");
        expect(!economie_courant_lu(e, -0.80f, true, false) && !e.sur_batterie, "éco : courant de charge (−), secteur");
        expect(economie_courant_lu(e, 0.050f, true, false) && e.sur_batterie, "éco : 50 mA pile, sur batterie");
        expect(economie_en_charge(e, true) && !e.sur_batterie, "éco : la charge reprend, secteur tout de suite");
        expect(!economie_en_charge(e, true), "éco : déjà sur secteur, rien ne change");
    }
    {
        EtatEconomie e;
        expect(!economie_niveau_lu(e, 20.0f) && !e.batterie_basse, "éco : 20 % sur secteur, pas basse");
        expect(economie_courant_lu(e, 0.30f, true, false) && e.batterie_basse,
               "éco : débranchée à 20 %, basse dès qu'elle se sait sur batterie");
        economie_en_charge(e, true);
        expect(!e.batterie_basse, "éco : rebranchée, plus basse");
        economie_courant_lu(e, 0.30f, true, false);
        expect(!economie_niveau_lu(e, 30.0f) && e.batterie_basse, "éco : 30 %, basse");
        expect(!economie_niveau_lu(e, 38.0f) && e.batterie_basse, "éco : 38 %, reste basse (retour à 40 %)");
        expect(!economie_niveau_lu(e, NAN) && e.batterie_basse, "éco : niveau inconnu, décision gardée");
        expect(economie_niveau_lu(e, 40.0f) && !e.batterie_basse, "éco : 40 %, plus basse");
        expect(!economie_niveau_lu(e, 36.0f) && !e.batterie_basse, "éco : 36 % en descendant, pas encore basse");
        expect(economie_niveau_lu(e, 35.0f) && e.batterie_basse, "éco : 35 % pile, basse");
    }
    {
        EntreesEconomie in;
        in.ecran_allume = true;
        in.veille_permise = true;
        in.inactif_ms = 0;
        in.choix = static_cast<uint8_t>(ChoixEconomie::JAMAIS);
        in.sur_batterie = true;
        in.batterie_basse = true;
        DecisionEconomie d = economie_decider(in);
        expect(!d.active && d.plafond == 1.0f && d.periode_ms == kEcoPeriodeNormaleMs && !d.animations_reduites,
               "éco « Jamais » : rien ne change, même sur batterie basse");
        in.choix = static_cast<uint8_t>(ChoixEconomie::SUR_BATTERIE);
        in.sur_batterie = false;
        in.batterie_basse = false;
        d = economie_decider(in);
        expect(!d.active && d.plafond == 1.0f, "éco « Sur batterie » sur secteur : inactif");
        in.sur_batterie = true;
        d = economie_decider(in);
        expect(d.active && d.plafond == kEcoPlafond && d.periode_ms == kEcoPeriodeMs && d.animations_reduites,
               "éco « Sur batterie » sur batterie : plafond 50 %, 30 images/s, sans animation");
        in.batterie_basse = true;
        d = economie_decider(in);
        expect(d.plafond == kEcoPlancher && !d.assombri, "éco : batterie basse, au plus bas");
        in.batterie_basse = false;
        in.choix = static_cast<uint8_t>(ChoixEconomie::TOUJOURS);
        in.sur_batterie = false;
        in.inactif_ms = kEcoAssombrirMs - 1;
        d = economie_decider(in);
        expect(d.active && !d.assombri && d.plafond == kEcoPlafond, "éco « Toujours » : 29,999 s sans toucher, pas encore");
        in.inactif_ms = kEcoAssombrirMs;
        d = economie_decider(in);
        expect(d.assombri && d.plafond == kEcoPlancher, "éco : 30 s sans toucher, au plus bas");
        in.veille_permise = false;
        d = economie_decider(in);
        expect(!d.assombri && d.plafond == kEcoPlafond, "éco : réveil, voix ou jeu, jamais assombri");
        in.veille_permise = true;
        in.ecran_allume = false;
        d = economie_decider(in);
        expect(!d.assombri, "éco : écran éteint, pas « assombri »");
        in.ecran_allume = true;
        in.jeu_ouvert = true;
        d = economie_decider(in);
        expect(d.periode_ms == kEcoPeriodeNormaleMs && d.animations_reduites, "éco : un jeu garde ses 60 images/s");
        in.choix = 7;
        d = economie_decider(in);
        expect(!d.active, "éco : index inconnu = Jamais");
    }
    expect(kEcoPlancher == 0.10f && kEcoPlafond == 0.50f && kEcoNiveauBas == 35.0f && kEcoAssombrirMs == 30000u,
           "éco : plancher 10 % (minimum du curseur), plafond 50 %, basse à 35 %, 30 s");
}

int main() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // Europe/Paris, comme le firmware
    tzset();
    tab5_time_source = fake_time;

    test_dates();
    test_heure_fixe_jours_coches();
    test_sonne_une_seule_fois_puis_repetition();
    test_fenetre_de_grace_au_demarrage();
    test_reglage_pose_dans_le_passe();
    test_mode_embauche();
    test_repos_minimum_apres_fermeture();
    test_calendrier_date_par_son_ancrage();
    test_repos_selon_reglage();
    test_calendrier_absent();
    test_changements_d_heure();
    test_rendez_vous();
    test_prereglages_et_volume();
    test_langue();
    test_nombres_de_ha();
    test_batterie_presence();
    test_console_batterie_et_cpu();
    test_economie();

    std::printf("=== %s (%d OK, %d FAIL) ===\n", g_fail ? "FAILED" : "ALL PASSED", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
