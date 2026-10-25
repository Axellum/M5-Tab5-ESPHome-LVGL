/**
 * Tests hôte du moteur de réveil (Tab5/socle/alarm_clock.cpp + Tab5/socle/tab5_core.cpp),
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
 * et le chargeur de la batterie (tab5_batterie.h, header seul, 08/10 : présence lue
 * chargeur coupé, sondes, limite 80 %, consommation, alerte de batterie faible),
 * puis les lignes « Batterie » et « Charge CPU » de la console système (06/10), les pages
 * des Réglages et les textes de leur page Batterie (08/10),
 * et les règles du mode économie d'énergie (tab5_economie.h, header seul, 06/10) :
 * sur batterie au courant de l'INA226, batterie basse, plafond de luminosité.
 *
 * Build & run (CI, job `python`) :
 *   g++ -std=c++17 -O2 -Wall -Wextra -I Tab5/socle -o test_alarm_clock \
 *       tools/test_alarm_clock.cpp Tab5/socle/alarm_clock.cpp Tab5/socle/tab5_core.cpp
 *   ./test_alarm_clock
 * Le poste de dev n'a qu'un cross-compilateur RISC-V ; vérif locale possible :
 *   riscv32-esp-elf-g++ -std=c++17 -fsyntax-only -I Tab5/socle tools/test_alarm_clock.cpp
 */
#include "alarm_clock.h"
#include "tab5_core.h"
#include "tab5_batterie.h"
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

    // « Prochain réveil » publié vers HA : en anglais quelle que soit la langue de l'écran
    // (I18nPourHa, ADR-0070), puis la langue de l'écran revient à la sortie de la garde.
    i18n_set_language(0);
    {
        const I18nPourHa en;
        expect_str(alarm_next_label(now), "Tomorrow 07:00", "état pour HA : en anglais sur un écran en français");
    }
    expect_str(alarm_next_label(now), "Demain 07:00", "garde refermée : l'écran retrouve le français");
    i18n_set_language(2);
    {
        const I18nPourHa en;
        expect(i18n_language() == 1, "garde : l'anglais, même depuis une autre langue");
    }
    expect(i18n_language() == 2, "garde refermée : la langue de l'écran revient");
    i18n_set_language(0);

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
    // Initiales des jours du réveil (« mardi|M ») retirées le 09/10/2026 (noms courts) :
    // la même lettre, deux contextes, ce sont maintenant les pièces d'échecs en notation.
    expect_str(tr_ctx("san", "D"), "Q", "contexte : D = dame (notation)");
    expect_str(tr_ctx("san", "T"), "R", "contexte : T = tour (notation)");
    expect_str(tr_ctx("nudge", "Douce"), "Low", "contexte : secousse douce du flipper");
    expect_str(tr("Douce"), "Gentle", "sans contexte : mélodie douce du réveil");
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

// Batterie et chargeur (tab5_batterie.h, 08/10/2026) : la présence se lit chargeur
// COUPÉ (sans batterie 1,9 V mesurés ; chargeur allumé il lisait 4,2 ↔ 8,39 V ou 5,71 V et
// soufflait). Chaque seconde, chargeur_tick() dit l'état de CHG_EN et s'il faut lire.
static uint32_t tic_jusqua(EtatChargeur& c, uint8_t limite, bool montee, uint32_t de, uint32_t a,
                           bool* toujours_allume, bool* toujours_coupe, int* lectures) {
    for (uint32_t ms = de; ms <= a; ms += 1000) {
        const ActionChargeur act = chargeur_tick(c, limite, montee, ms);
        if (toujours_allume != nullptr && !act.allumer) *toujours_allume = false;
        if (toujours_coupe != nullptr && act.allumer) *toujours_coupe = false;
        if (lectures != nullptr && act.lire) (*lectures)++;
    }
    return a;
}

// Une batterie qui charge : sa tension au repos monte de `pente` V par minute de charge.
struct BatterieSimulee {
    float v0;          // tension au départ (V)
    float pente;       // V gagnés par minute de charge
    int charge_s;      // secondes de charge
    float tension() const { return v0 + pente * static_cast<float>(charge_s) / 60.0f; }
};

// La tablette seconde par seconde, sondes du chargeur comprises : une lecture de l'INA226
// à chaque demande (fin du repos d'une sonde) et toutes les 60 s (update_interval).
static void simuler_charge(EtatChargeur& c, BatterieSimulee& b, uint8_t limite, uint32_t de, uint32_t a) {
    for (uint32_t ms = de; ms <= a; ms += 1000) {
        const ActionChargeur act = chargeur_tick(c, limite, false, ms);
        if (act.allumer) b.charge_s++;
        if (act.lire || ms % 60000u == 0) chargeur_lecture(c, b.tension(), ms);
    }
}

static void test_chargeur() {
    constexpr uint32_t S = 1000u;
    constexpr uint32_t MIN = 60u * S;
    using P = PresenceBatterie;
    constexpr uint8_t AUCUNE = static_cast<uint8_t>(MemoireBatterie::AUCUNE);
    constexpr uint8_t VUE = static_cast<uint8_t>(MemoireBatterie::VUE);
    constexpr uint8_t INCONNUE = static_cast<uint8_t>(MemoireBatterie::INCONNUE);
    {
        // Premier démarrage sans batterie (modèle vendu sans, mémoire vide) : depuis le
        // 16/10/2026, une sonde d'abord, CHG_EN jamais allumé.
        EtatChargeur c;
        chargeur_demarrer(c, INCONNUE, false, false, false, 0);
        expect(!c.allume && c.sonde && !c.reveil, "démarrage : CHG_EN coupé, sonde d'abord");
        int lectures = 0;
        bool coupe = true;
        tic_jusqua(c, 0, false, 0, 7 * S, nullptr, &coupe, &lectures);
        expect(coupe && lectures == 0, "repos de la sonde : coupé, pas encore de lecture");
        expect(!chargeur_lecture(c, 4.0f, 5 * S) && c.presence == P::INCONNUE,
               "lecture 5 s après la coupure : trop tôt pour décider");
        const ActionChargeur a = chargeur_tick(c, 0, false, 8 * S);
        expect(!a.allumer && a.lire, "8 s de repos : une lecture demandée");
        expect(!chargeur_tick(c, 0, false, 9 * S).lire, "une seule lecture demandée par sonde");
        expect(chargeur_en_sonde(c, 9 * S), "sonde en cours : le courant lu est ignoré");
        expect(chargeur_lecture(c, 1.9f, 9 * S) && c.presence == P::ABSENTE, "1,9 V chargeur coupé : pas de batterie");
        expect(std::isnan(c.niveau), "sans batterie : niveau inconnu");
        expect(chargeur_en_sonde(c, 10 * S) && !chargeur_en_sonde(c, 11 * S),
               "courant ignoré 2 s encore après la fin de la sonde");
        coupe = true;
        tic_jusqua(c, 0, false, 10 * S, 3 * 60 * MIN, nullptr, &coupe, nullptr);
        expect(coupe, "premier démarrage sans batterie : chargeur jamais allumé (ni réveil ni souffle)");
        expect(batterie_memoire_suivante(c, INCONNUE) == AUCUNE, "mémoire : aucune batterie");
        expect(!chargeur_lecture(c, 1.83f, 3 * 60 * MIN) && c.presence == P::ABSENTE, "lecture suivante : toujours absente");
        // Batterie glissée tablette allumée : vue à la lecture suivante, chargeur allumé.
        expect(chargeur_lecture(c, 7.4f, 3 * 60 * MIN + MIN) && c.presence == P::PRESENTE,
               "batterie glissée : présente à la lecture suivante");
        expect(chargeur_tick(c, 0, false, 3 * 60 * MIN + MIN + S).allumer, "batterie lue ≥ 5,5 V : chargeur allumé");
    }
    {
        // Démarrage avec une batterie : chargée seulement après la lecture de la sonde,
        // puis retirée pendant la charge.
        EtatChargeur c;
        chargeur_demarrer(c, VUE, false, false, false, 0);
        bool coupe = true;
        tic_jusqua(c, 0, false, 0, 8 * S, nullptr, &coupe, nullptr);
        expect(coupe, "batterie : CHG_EN coupé tant que rien n'est lu");
        expect(chargeur_lecture(c, 7.2f, 8 * S) && c.presence == P::PRESENTE, "7,2 V chargeur coupé : batterie");
        expect(std::fabs(c.niveau - 3.67f) < 0.1f, "niveau d'après la courbe mesurée (7,2 V ≈ 4 %)");
        expect(!chargeur_en_sonde(c, 8 * S + 2 * S), "fin de sonde + 2 s : le courant compte de nouveau");
        bool allume = true;
        // Fin de la sonde à 8 s : la suivante tombe à 8 s + 10 min.
        tic_jusqua(c, 0, false, 9 * S, 8 * S + 10 * MIN - S, &allume, nullptr, nullptr);
        expect(allume, "batterie, limite 100 % : chargeur allumé jusqu'à la sonde suivante");
        expect(!c.reveil, "batterie vue par la sonde : pas de réveil");
        expect(!chargeur_tick(c, 0, false, 8 * S + 10 * MIN).allumer && c.sonde, "sonde toutes les 10 min");
        chargeur_tick(c, 0, false, 8 * S + 10 * MIN + 8 * S);
        expect(!chargeur_lecture(c, 7.3f, 8 * S + 10 * MIN + 8 * S) && c.presence == P::PRESENTE, "sonde : toujours là");
        expect(chargeur_tick(c, 0, false, 8 * S + 10 * MIN + 9 * S).allumer, "après la sonde : rallumé");
        // Retirée : le chargeur lit le vide (5,71 V), sous 6 V : sonde sans attendre.
        const uint32_t t0 = 20 * MIN;
        expect(!chargeur_lecture(c, 5.71f, t0), "lecture basse chargeur allumé : rien de décidé encore");
        expect(!chargeur_tick(c, 0, false, t0 + S).allumer && c.sonde, "lecture basse : sonde tout de suite");
        chargeur_tick(c, 0, false, t0 + 9 * S);
        expect(chargeur_lecture(c, 1.9f, t0 + 9 * S) && c.presence == P::ABSENTE, "batterie retirée : absente");
        coupe = true;
        tic_jusqua(c, 0, false, t0 + 10 * S, t0 + 30 * MIN, nullptr, &coupe, nullptr);
        expect(coupe, "batterie retirée : chargeur coupé");
        expect(batterie_memoire_suivante(c, VUE) == VUE, "vue pendant ce démarrage : mémoire « vue »");
    }
    {
        // INA226 muet : la sonde est abandonnée après 30 s et le chargeur reste COUPÉ
        // (16/10/2026 : jamais allumé à l'aveugle), jusqu'à une lecture.
        EtatChargeur c;
        chargeur_demarrer(c, VUE, false, false, false, 0);
        tic_jusqua(c, 0, false, 0, 29 * S, nullptr, nullptr, nullptr);
        expect(c.sonde, "sonde sans lecture : en cours jusqu'à 30 s");
        expect(!chargeur_tick(c, 0, false, 30 * S).allumer, "présence inconnue : chargeur coupé");
        expect(!c.sonde && c.presence == P::INCONNUE && !c.reveil, "sonde abandonnée, ni présence ni réveil");
        bool coupe = true;
        tic_jusqua(c, 0, false, 31 * S, 20 * MIN, nullptr, &coupe, nullptr);
        expect(coupe, "INA226 muet : coupé tout du long");
        expect(!chargeur_lecture(c, NAN, 20 * MIN) && c.presence == P::INCONNUE, "lecture ratée : rien ne change");
        expect(chargeur_lecture(c, 7.4f, 21 * MIN) && c.presence == P::PRESENTE, "l'INA226 répond : batterie");
        expect(chargeur_tick(c, 0, false, 21 * MIN + S).allumer, "puis chargée");
    }
    {
        // Réveil de 30 s (batterie dont la protection a coupé) : seulement après une sonde
        // vide, mémoire « vue » et redémarrage normal.
        EtatChargeur c;
        chargeur_demarrer(c, VUE, false, false, false, 0);
        expect(c.reveil_apres_sonde, "mémoire « vue », démarrage normal : réveil prévu après la sonde");
        chargeur_tick(c, 0, false, 8 * S);
        expect(chargeur_lecture(c, 0.4f, 8 * S) && c.presence == P::ABSENTE, "sonde : rien vu");
        expect(chargeur_tick(c, 0, false, 9 * S).allumer && c.reveil, "puis le réveil");
        bool allume = true;
        tic_jusqua(c, 0, false, 10 * S, 38 * S, &allume, nullptr, nullptr);
        expect(allume, "réveil : allumé 30 s");
        expect(!chargeur_tick(c, 0, false, 39 * S).allumer && c.sonde, "puis une sonde");
        expect(chargeur_tick(c, 0, false, 47 * S).lire, "lecture de la sonde");
        expect(chargeur_lecture(c, 6.3f, 47 * S) && c.presence == P::PRESENTE, "batterie réveillée : présente");
        expect(chargeur_tick(c, 0, false, 48 * S).allumer, "puis chargée");
        // Pas de batterie après le réveil : coupé, et la mémoire passe à « aucune ».
        EtatChargeur d;
        chargeur_demarrer(d, VUE, false, false, false, 0);
        chargeur_tick(d, 0, false, 8 * S);
        chargeur_lecture(d, 1.9f, 8 * S);
        tic_jusqua(d, 0, false, 9 * S, 46 * S, nullptr, nullptr, nullptr);
        expect(d.sonde && chargeur_tick(d, 0, false, 47 * S).lire, "réveil vide : sonde");
        chargeur_lecture(d, 1.9f, 47 * S);
        bool coupe = true;
        tic_jusqua(d, 0, false, 48 * S, 3 * 60 * MIN, nullptr, &coupe, nullptr);
        expect(coupe && d.presence == P::ABSENTE, "réveil vide : coupé ensuite, plus de réveil");
        expect(batterie_memoire_suivante(d, VUE) == AUCUNE, "mémoire : aucune (pas de réveil au suivant)");
    }
    for (const uint8_t memoire : {AUCUNE, INCONNUE, static_cast<uint8_t>(7)}) {
        // Mémoire « aucune », vide ou illisible : une sonde vide, et c'est tout.
        EtatChargeur c;
        chargeur_demarrer(c, memoire, false, false, false, 0);
        chargeur_tick(c, 0, false, 8 * S);
        chargeur_lecture(c, 0.4f, 8 * S);
        bool coupe = true;
        tic_jusqua(c, 0, false, 9 * S, 60 * MIN, nullptr, &coupe, nullptr);
        expect(coupe, "mémoire autre que « vue » : aucun réveil au démarrage");
    }
    {
        // Redémarrage anormal (plantage, baisse de tension) : jamais de réveil, même si une
        // batterie était vue. Boucle de 20 redémarrages de 20 s : CHG_EN jamais allumé.
        bool coupe = true;
        for (int tour = 0; tour < 20; tour++) {
            EtatChargeur c;
            chargeur_demarrer(c, VUE, false, true, false, 0);
            expect(!c.reveil_apres_sonde, "redémarrage anormal : pas de réveil prévu");
            tic_jusqua(c, 0, false, 0, 7 * S, nullptr, &coupe, nullptr);
            chargeur_tick(c, 0, false, 8 * S);
            chargeur_lecture(c, 1.7f, 8 * S);
            tic_jusqua(c, 0, false, 9 * S, 20 * S, nullptr, &coupe, nullptr);
        }
        expect(coupe, "boucle de redémarrages anormaux : CHG_EN jamais allumé");
        // Boucle de redémarrages normaux : le premier tour fait le réveil, la mémoire
        // « aucune » est décidée dès la sonde (8 s) et coupe les suivants.
        EtatChargeur c;
        chargeur_demarrer(c, VUE, false, false, false, 0);
        chargeur_tick(c, 0, false, 8 * S);
        chargeur_lecture(c, 1.9f, 8 * S);
        expect(batterie_memoire_suivante(c, VUE) == AUCUNE, "sonde vide : mémoire « aucune » dès 8 s");
    }
    {
        // Batterie montée mais pas vue (protection coupée) : un réveil par heure.
        EtatChargeur c;
        chargeur_demarrer(c, AUCUNE, true, false, false, 0);
        chargeur_tick(c, 0, true, 8 * S);
        chargeur_lecture(c, 0.4f, 8 * S);
        bool coupe = true;
        tic_jusqua(c, 0, true, 9 * S, 60 * MIN - S, nullptr, &coupe, nullptr);
        expect(coupe && c.presence == P::ABSENTE, "absente : coupé pendant une heure");
        expect(chargeur_tick(c, 0, true, 60 * MIN).allumer && c.reveil, "« montée » : réveil au bout d'une heure");
        bool allume = true;
        tic_jusqua(c, 0, true, 60 * MIN + S, 60 * MIN + 29 * S, &allume, nullptr, nullptr);
        expect(allume, "réveil : allumé 30 s");
        expect(!chargeur_tick(c, 0, true, 60 * MIN + 30 * S).allumer && c.sonde, "puis une sonde");
        chargeur_tick(c, 0, true, 60 * MIN + 38 * S);
        expect(chargeur_lecture(c, 6.3f, 60 * MIN + 38 * S) && c.presence == P::PRESENTE, "batterie réveillée : présente");
        EtatChargeur d;
        chargeur_demarrer(d, AUCUNE, false, false, false, 0);
        chargeur_tick(d, 0, false, 8 * S);
        chargeur_lecture(d, 1.9f, 8 * S);
        coupe = true;
        tic_jusqua(d, 0, false, 9 * S, 5 * 60 * MIN, nullptr, &coupe, nullptr);
        expect(coupe, "interrupteur « montée » éteint : jamais de réveil (pas de souffle)");
    }
    {
        // Limite 80 % : arrêt à 80 %, reprise à 70 % après au moins 10 min d'arrêt.
        constexpr uint8_t L80 = static_cast<uint8_t>(LimiteCharge::QUATRE_VINGTS);
        EtatChargeur c;
        chargeur_demarrer(c, VUE, false, false, false, 0);
        chargeur_tick(c, L80, false, 8 * S);
        chargeur_lecture(c, 8.10f, 8 * S);  // 90 % (courbe mesurée)
        bool coupe = true;
        tic_jusqua(c, L80, false, 9 * S, 4 * MIN, nullptr, &coupe, nullptr);
        expect(coupe, "80 % : batterie à 90 % au démarrage, chargeur coupé");
        chargeur_lecture(c, 8.0f, 5 * MIN);  // 62 % (4,000 V par élément)
        expect(c.presence == P::PRESENTE && std::fabs(c.niveau - 62.0f) < 0.1f, "chargeur coupé : niveau relu");
        expect(!chargeur_tick(c, L80, false, 5 * MIN + S).allumer, "62 % après 5 min d'arrêt : pas encore (10 min)");
        expect(chargeur_tick(c, L80, false, 11 * MIN).allumer, "62 % après 10 min d'arrêt : reprise");
        // La règle seule (sans les sondes périodiques du chargeur allumé).
        EtatChargeur r;
        r.sonde = false;
        r.presence = P::PRESENTE;
        r.allume = true;
        r.niveau = batterie_niveau_pct(8.04f);  // ≈ 77 % (courbe mesurée)
        expect(chargeur_voulu(r, L80, 15 * MIN), "77 % en charge : continue");
        r.niveau = batterie_niveau_pct(8.05f);  // 81 %
        expect(!chargeur_voulu(r, L80, 20 * MIN), "80 % atteint : arrêt");
        r.allume = false;
        r.change_ms = 20 * MIN;
        r.niveau = batterie_niveau_pct(8.03f);  // ≈ 73 %
        expect(!chargeur_voulu(r, L80, 40 * MIN), "73 % : reste à l'arrêt (reprise à 70 %)");
        r.niveau = NAN;
        expect(!chargeur_voulu(r, L80, 40 * MIN), "niveau inconnu : décision gardée");
        expect(chargeur_voulu(r, 0, 40 * MIN), "limite à 100 % : charge tout de suite");
        expect(chargeur_voulu(r, 9, 40 * MIN), "index inconnu : 100 %");
        r.presence = P::INCONNUE;
        expect(!chargeur_voulu(r, 0, 40 * MIN), "présence inconnue : jamais de charge");

        // Avec les sondes périodiques (discussion #278, 20/10/2026 : « la limite de 80 %
        // arrête la charge là où est le niveau ») : une sonde n'est pas un arrêt de la limite.
        // 75 % en charge (limite 100 %, puis 80 % choisie) : la charge continue après chaque
        // sonde et s'arrête à 80 %.
        EtatChargeur e;
        BatterieSimulee b{8.035f, 0.0005f, 0};  // ≈ 75 %, 80 % après ≈ 25 min de charge
        chargeur_demarrer(e, VUE, false, false, false, 0);
        simuler_charge(e, b, 0, 0, 2 * MIN);
        expect(e.allume && e.niveau > 74.0f && e.niveau < 77.0f, "75 %, limite 100 % : en charge");
        simuler_charge(e, b, L80, 2 * MIN + S, 10 * MIN + 30 * S);
        expect(e.sonde_fin_ms > 10 * MIN && e.allume, "80 % choisie à 75 % : la charge continue après la sonde");
        simuler_charge(e, b, L80, 10 * MIN + 31 * S, 30 * MIN);
        expect(!e.allume && e.niveau >= 80.0f, "puis la charge s'arrête à 80 %");
        expect(b.charge_s > 24 * 60, "jusque-là, coupée seulement 8 s par sonde (≈ 25 min de charge)");
        const int charge_80 = b.charge_s;
        simuler_charge(e, b, L80, 30 * MIN + S, 90 * MIN);
        expect(b.charge_s == charge_80, "80 % atteint : à l'arrêt (reprise à 70 %)");
        // Sous 70 % (59 %) : 10 min d'arrêt au démarrage, puis la charge tient entre deux
        // sondes (avant le 20/10/2026 : quelques secondes toutes les 10 min).
        EtatChargeur f;
        BatterieSimulee g{7.97f, 0.0002f, 0};  // ≈ 59 %, reste sous 70 %
        chargeur_demarrer(f, VUE, false, false, false, 0);
        simuler_charge(f, g, L80, 0, 10 * MIN - S);
        expect(g.charge_s == 0, "59 %, limite 80 % : 10 min d'arrêt au démarrage");
        simuler_charge(f, g, L80, 10 * MIN, 60 * MIN);
        expect(g.charge_s > 48 * 60, "59 % : en charge au moins 48 min sur 50 (8 s de coupure par sonde)");
        // Une coupure de sécurité, elle, reste un arrêt pour la limite (comme avant).
        EtatChargeur h;
        BatterieSimulee hb{8.035f, 0.0f, 0};
        chargeur_demarrer(h, VUE, false, false, false, 0);
        simuler_charge(h, hb, 0, 0, 2 * MIN);
        chargeur_lecture(h, 8.6f, 2 * MIN + 30 * S);  // tension trop haute : sonde de sécurité
        simuler_charge(h, hb, L80, 2 * MIN + 31 * S, 20 * MIN);
        expect(h.electrique_vu && !h.allume, "80 %, coupure de sécurité à 75 % : arrêt (reprise à 70 %)");
    }
    {
        // Batterie posée tablette éteinte, après un démarrage sans : vue par la sonde.
        const uint32_t t0 = 8 * S;
        EtatChargeur c;
        chargeur_demarrer(c, AUCUNE, false, false, false, t0);
        expect(batterie_memoire_suivante(c, AUCUNE) == AUCUNE, "rien de décidé : mémoire d'avant");
        chargeur_tick(c, 0, false, t0 + 8 * S);
        expect(chargeur_lecture(c, 7.4f, t0 + 8 * S) && c.presence == P::PRESENTE,
               "batterie posée tablette éteinte : vue par la sonde de démarrage");
        expect(chargeur_tick(c, 0, false, t0 + 9 * S).allumer, "puis chargée");
        expect(batterie_memoire_suivante(c, AUCUNE) == VUE, "mémoire : batterie vue");
        // Le moyen manuel : allumer « Tab5 Batterie montée » réveille tout de suite.
        EtatChargeur m;
        chargeur_demarrer(m, AUCUNE, false, false, false, t0);
        chargeur_tick(m, 0, false, t0 + 8 * S);
        chargeur_lecture(m, 0.4f, t0 + 8 * S);  // batterie neuve, protection coupée
        expect(m.presence == P::ABSENTE, "batterie plate : vue absente");
        expect(!chargeur_tick(m, 0, false, t0 + 2 * MIN).allumer, "interrupteur éteint : coupé");
        expect(chargeur_tick(m, 0, true, t0 + 2 * MIN + S).allumer && m.reveil,
               "« montée » allumé : réveil tout de suite, sans attendre l'heure");
        bool allume = true;
        tic_jusqua(m, 0, true, t0 + 2 * MIN + 2 * S, t0 + 2 * MIN + 30 * S, &allume, nullptr, nullptr);
        expect(allume, "réveil manuel : allumé 30 s");
        expect(!chargeur_tick(m, 0, true, t0 + 2 * MIN + 31 * S).allumer && m.sonde, "puis une sonde");
        // Interrupteur déjà allumé au démarrage : pas un réveil immédiat, celui de l'heure.
        EtatChargeur d;
        chargeur_demarrer(d, AUCUNE, true, false, false, t0);
        chargeur_tick(d, 0, true, t0 + 8 * S);
        chargeur_lecture(d, 1.9f, t0 + 8 * S);
        bool coupe = true;
        tic_jusqua(d, 0, true, t0 + 9 * S, t0 + 60 * MIN - S, nullptr, &coupe, nullptr);
        expect(coupe, "« montée » restauré allumé : pas de réveil avant l'heure");
        expect(chargeur_tick(d, 0, true, t0 + 60 * MIN).allumer && d.reveil, "réveil au bout d'une heure");
        expect(AUCUNE == 1 && VUE == 2 && INCONNUE == 0, "valeurs gardées en NVS : 0 inconnue, 1 aucune, 2 vue");
    }
    {
        // millis() reboucle après 49 jours : les durées se comptent quand même.
        EtatChargeur c;
        c.sonde = false;
        c.allume = true;
        c.presence = P::PRESENTE;
        const uint32_t base = 0xFFFFFFFFu - 3 * S;
        c.sonde_fin_ms = base - 10 * MIN;
        expect(!chargeur_tick(c, 0, false, base).allumer && c.sonde, "rebouclage : sonde à l'heure");
        expect(chargeur_tick(c, 0, false, base + 8 * S).lire, "rebouclage : lecture 8 s après");
        expect(!chargeur_lecture(c, 7.4f, base + 8 * S) && !c.sonde, "rebouclage : sonde terminée");
    }
    expect(kBatterieSeuilV == 5.5f && kBatterieBasseV == 6.0f && kChargeurReposMs == 8u * S,
           "seuils : 5,5 V chargeur coupé (évite faux positif tension résiduelle), 6,0 V chargeur allumé, 8 s de repos");

    // Une batterie chargée normalement, prête pour les verrous (lue à 7,6 V, ≈ 18 %).
    const auto batterie_en_charge = [](EtatChargeur& c, uint32_t t) {
        chargeur_demarrer(c, VUE, false, false, false, t);
        chargeur_tick(c, 0, false, t + 8 * S);
        chargeur_lecture(c, 7.6f, t + 8 * S);
        chargeur_tick(c, 0, false, t + 9 * S);
    };

    // Sécurité thermique anti-surchauffe (11/10/2026, ADR-0061 ; NVS et NAN le 16/10/2026)
    {
        EtatChargeur c;
        batterie_en_charge(c, 0);
        expect(c.allume, "charge normale autorisée");

        // Montée à 55 °C : verrouillage thermique immédiat
        expect(chargeur_surveillance_thermique(c, 55.5f, 10 * S), "déclenchement verrouillage thermique à 55,5 °C");
        expect(c.verrou_thermique && chargeur_securite_posee(c), "verrou thermique actif (« Tab5 Sécurité chargeur »)");
        const SignalSecurite s = chargeur_signal_prendre(c);
        expect(s.cause == CauseSecurite::THERMIQUE && std::fabs(s.mesure - 55.5f) < 0.01f && !s.verrouille,
               "signalé à HA : thermique, 55,5 °C");
        expect(chargeur_signal_prendre(c).cause == CauseSecurite::AUCUNE, "signalé une seule fois");
        expect(!chargeur_tick(c, 0, false, 11 * S).allumer, "chargeur immédiatement coupé et verrouillé");
        expect(!chargeur_voulu(c, 0, 11 * S), "chargeur refusé tant que verrouillé");

        // 10 min après à 50 °C : toujours verrouillé (minimum 15 min ET sous 44 °C)
        expect(!chargeur_surveillance_thermique(c, 50.0f, 10 * MIN), "pas de réarmement prématuré");
        expect(!chargeur_tick(c, 0, false, 10 * MIN).allumer, "toujours coupé à 10 min");

        // 16 min après mais température à 46 °C : toujours verrouillé (doit être ≤ 44 °C)
        chargeur_surveillance_thermique(c, 46.0f, 16 * MIN);
        expect(!chargeur_tick(c, 0, false, 16 * MIN).allumer, "toujours coupé si CPU > 44 °C");

        // Température illisible : le verrou ne se lève pas (16/10/2026).
        chargeur_surveillance_thermique(c, NAN, 17 * MIN);
        bool coupe = true;
        tic_jusqua(c, 0, false, 17 * MIN, 60 * MIN, nullptr, &coupe, nullptr);
        expect(coupe && c.verrou_thermique, "température NAN : verrou gardé, chargeur coupé");

        // Température redescendue à 40 °C : réarmement
        chargeur_surveillance_thermique(c, 40.0f, 60 * MIN + 10 * S);
        expect(chargeur_tick(c, 0, false, 60 * MIN + 10 * S).allumer, "réarmement après refroidissement sous 44 °C");
        expect(!c.verrou_thermique && !chargeur_securite_posee(c), "verrou thermique levé");
    }
    {
        // Verrou thermique gardé en NVS : le démarrage suivant le retrouve posé, chargeur
        // coupé au moins 15 min puis jusqu'à une température lue saine.
        EtatChargeur c;
        chargeur_demarrer(c, VUE, false, false, true, 0);
        expect(c.verrou_thermique && chargeur_securite_posee(c), "verrou thermique restauré au démarrage");
        bool coupe = true;
        for (uint32_t t = 0; t < 15 * MIN; t += 10 * S) {
            chargeur_surveillance_thermique(c, 40.0f, t);
            if (t == 10 * S) chargeur_lecture(c, 7.6f, t);
            if (chargeur_tick(c, 0, false, t).allumer) coupe = false;
        }
        expect(coupe && c.presence == P::PRESENTE, "verrou restauré : batterie vue mais coupé 15 min");
        expect(chargeur_signal_prendre(c).cause == CauseSecurite::AUCUNE, "verrou restauré : pas de nouvel événement");
        chargeur_surveillance_thermique(c, 40.0f, 15 * MIN);
        expect(chargeur_tick(c, 0, false, 15 * MIN).allumer, "15 min et 40 °C lus : chargeur rendu");
        // Sans aucune température lue : jamais levé.
        EtatChargeur d;
        chargeur_demarrer(d, VUE, false, false, true, 0);
        chargeur_lecture(d, 7.6f, 10 * S);
        coupe = true;
        tic_jusqua(d, 0, false, 0, 3 * 60 * MIN, nullptr, &coupe, nullptr);
        expect(coupe, "verrou restauré, aucune température lue : coupé 3 h");
    }

    // Verrou électrique : la signature d'une charge dans le vide (16/10/2026).
    {
        // Tension au-dessus d'une 2S pleine, chargeur allumé : coupé, sonde immédiate.
        EtatChargeur c;
        batterie_en_charge(c, 0);
        expect(!chargeur_lecture(c, 8.45f, 1 * MIN) && c.allume, "8,45 V : pas encore au-dessus du seuil");
        chargeur_lecture(c, 8.47f, 2 * MIN);
        expect(!c.allume && c.sonde, "8,47 V chargeur allumé : coupé, sonde immédiate");
        expect(!chargeur_tick(c, 0, false, 2 * MIN + S).allumer, "pendant la sonde : coupé");
        expect(chargeur_signal_prendre(c).cause == CauseSecurite::AUCUNE, "rien de signalé avant la fin de la sonde");
        chargeur_tick(c, 0, false, 2 * MIN + 8 * S);
        chargeur_lecture(c, 7.9f, 2 * MIN + 8 * S);
        SignalSecurite s = chargeur_signal_prendre(c);
        expect(s.cause == CauseSecurite::TENSION && std::fabs(s.mesure - 8.47f) < 0.001f && !s.verrouille,
               "signalé : tension, 8,47 V, pas encore verrouillé");
        expect(chargeur_tick(c, 0, false, 2 * MIN + 9 * S).allumer && !chargeur_securite_posee(c),
               "un déclenchement : la charge reprend");
        // Deuxième dans l'heure : verrouillé jusqu'au redémarrage.
        chargeur_lecture(c, 8.50f, 40 * MIN);
        chargeur_tick(c, 0, false, 40 * MIN + 8 * S);
        chargeur_lecture(c, 7.9f, 40 * MIN + 8 * S);
        s = chargeur_signal_prendre(c);
        expect(s.cause == CauseSecurite::TENSION && s.verrouille, "deuxième dans l'heure : signalé « verrouillé »");
        expect(c.verrou_electrique && chargeur_securite_posee(c), "chargeur verrouillé (« Tab5 Sécurité chargeur »)");
        bool coupe = true;
        tic_jusqua(c, 0, true, 40 * MIN + 9 * S, 6 * 60 * MIN, nullptr, &coupe, nullptr);
        expect(coupe, "verrouillé : coupé 5 h, ni charge ni réveil");
        // Deux déclenchements à plus d'une heure d'écart : pas de verrou.
        EtatChargeur d;
        batterie_en_charge(d, 0);
        chargeur_lecture(d, 8.5f, 1 * MIN);
        chargeur_tick(d, 0, false, 1 * MIN + 8 * S);
        chargeur_lecture(d, 7.9f, 1 * MIN + 8 * S);
        chargeur_tick(d, 0, false, 1 * MIN + 9 * S);
        chargeur_lecture(d, 8.5f, 70 * MIN);
        chargeur_tick(d, 0, false, 70 * MIN + 8 * S);
        chargeur_lecture(d, 7.9f, 70 * MIN + 8 * S);
        expect(!d.verrou_electrique && chargeur_tick(d, 0, false, 70 * MIN + 9 * S).allumer,
               "deux déclenchements à 69 min d'écart : pas de verrou");
    }
    {
        // Aucun courant de charge, chargeur allumé (une fausse présence, la panne du 11/10) :
        // coupé au bout de 30 s, et la sonde tranche.
        EtatChargeur c;
        chargeur_demarrer(c, VUE, false, false, false, 0);
        chargeur_tick(c, 0, false, 8 * S);
        chargeur_lecture(c, 5.6f, 8 * S);  // fausse présence (au-dessus du seuil)
        expect(chargeur_tick(c, 0, false, 9 * S).allumer, "présence lue : chargeur allumé");
        chargeur_lecture_courant(c, -0.002f, 10 * S);
        expect(c.courant_faible && c.allume, "premier courant nul : on attend 30 s");
        expect(!chargeur_tick(c, 0, false, 39 * S).lire, "29 s : pas encore de relecture");
        expect(chargeur_tick(c, 0, false, 40 * S).lire, "30 s : relecture du courant sans attendre la minute");
        expect(!chargeur_tick(c, 0, false, 41 * S).lire, "une seule relecture");
        chargeur_lecture_courant(c, 0.001f, 41 * S);
        expect(!c.allume && c.sonde, "toujours nul : coupé, sonde immédiate");
        chargeur_tick(c, 0, false, 49 * S);
        chargeur_lecture(c, 5.6f, 49 * S);
        const SignalSecurite s = chargeur_signal_prendre(c);
        expect(s.cause == CauseSecurite::COURANT && !s.verrouille, "signalé : courant, pas encore verrouillé");
        expect(chargeur_tick(c, 0, false, 50 * S).allumer, "rallumé une fois");
        chargeur_lecture_courant(c, 0.0f, 51 * S);
        chargeur_lecture_courant(c, 0.0f, 81 * S);
        chargeur_tick(c, 0, false, 89 * S);
        chargeur_lecture(c, 5.6f, 89 * S);
        expect(c.verrou_electrique && chargeur_signal_prendre(c).verrouille,
               "deuxième courant nul dans l'heure : verrouillé jusqu'au redémarrage");
        bool coupe = true;
        tic_jusqua(c, 0, false, 90 * S, 3 * 60 * MIN, nullptr, &coupe, nullptr);
        expect(coupe, "chargeur verrouillé 3 h");
        // Vraie batterie qui charge (courant négatif) ou qui alimente la tablette : rien.
        EtatChargeur b;
        batterie_en_charge(b, 0);
        for (uint32_t t = 10 * S; t < 30 * MIN; t += MIN) chargeur_lecture_courant(b, -0.45f, t);
        chargeur_lecture_courant(b, 0.17f, 30 * MIN);
        expect(b.allume && !b.sonde && !b.courant_faible, "batterie qui charge ou alimente : aucun déclenchement");
        // Pendant le réveil (30 s, sonde ensuite de toute façon) : ignoré.
        EtatChargeur r;
        chargeur_demarrer(r, VUE, false, false, false, 0);
        chargeur_tick(r, 0, false, 8 * S);
        chargeur_lecture(r, 0.4f, 8 * S);
        chargeur_tick(r, 0, false, 9 * S);
        chargeur_lecture_courant(r, 0.0f, 10 * S);
        chargeur_lecture_courant(r, 0.0f, 38 * S);
        expect(r.reveil && !r.courant_faible, "réveil : courant nul ignoré");
        expect(chargeur_tick(r, 0, false, 39 * S).allumer == false && r.securite_sonde == CauseSecurite::AUCUNE,
               "fin du réveil : sonde ordinaire, pas de déclenchement");
    }
    {
        // Batterie pleine : la charge s'arrête, le courant est nul. La sonde lit une batterie
        // pleine (≥ 95 %) : ni événement ni compte, le détecteur se tait jusqu'à 90 %.
        EtatChargeur c;
        batterie_en_charge(c, 0);
        chargeur_lecture_courant(c, -0.004f, 1 * MIN);
        chargeur_lecture_courant(c, -0.003f, 2 * MIN);
        expect(c.sonde && !c.allume, "fin de charge : courant nul, sonde");
        chargeur_tick(c, 0, false, 2 * MIN + 8 * S);
        chargeur_lecture(c, 8.30f, 2 * MIN + 8 * S);
        expect(c.charge_terminee && chargeur_signal_prendre(c).cause == CauseSecurite::AUCUNE && !c.electrique_vu,
               "batterie pleine : charge terminée, rien de signalé, rien de compté");
        expect(chargeur_tick(c, 0, false, 2 * MIN + 9 * S).allumer, "batterie pleine : chargeur laissé allumé, comme avant");
        for (uint32_t t = 3 * MIN; t < 12 * MIN; t += MIN) chargeur_lecture_courant(c, 0.0f, t);
        expect(c.allume && !c.sonde, "charge terminée : le courant nul ne déclenche plus");
        // Sonde périodique : la batterie est redescendue à 62 % : détecteur réarmé.
        tic_jusqua(c, 0, false, 2 * MIN + 10 * S, 12 * MIN + 16 * S, nullptr, nullptr, nullptr);
        chargeur_lecture(c, 8.0f, 12 * MIN + 16 * S);
        expect(!c.charge_terminee, "niveau sous 90 % : détecteur de courant réarmé");
        // Le verrou de tension, lui, reste armé batterie pleine.
        chargeur_tick(c, 0, false, 12 * MIN + 17 * S);
        chargeur_lecture(c, 8.6f, 13 * MIN);
        expect(c.sonde && !c.allume, "tension trop haute : déclenché même batterie pleine");
    }

    // Mode de charge (09/10/2026, ADR-0045) : la charge rapide seulement chargeur allumé.
    {
        constexpr uint8_t CLASSIQUE = static_cast<uint8_t>(ModeCharge::CLASSIQUE);
        constexpr uint8_t RAPIDE = static_cast<uint8_t>(ModeCharge::RAPIDE);
        expect(!charge_rapide_voulue(CLASSIQUE, true), "mode de charge « Classique » : charge rapide arrêtée");
        expect(charge_rapide_voulue(RAPIDE, true), "mode de charge « Rapide », chargeur allumé : charge rapide");
        expect(!charge_rapide_voulue(RAPIDE, false), "« Rapide », chargeur coupé (sans batterie, sonde, 80 %) : arrêtée");
        expect(!charge_rapide_voulue(5, true), "mode de charge : index inconnu = « Classique »");
        // Avec le chargeur réel : coupé pendant la sonde du démarrage, la charge rapide
        // avec lui ; allumée seulement avec la batterie lue.
        EtatChargeur c;
        chargeur_demarrer(c, VUE, false, false, false, 0);
        const ActionChargeur sonde = chargeur_tick(c, 0, false, 0);
        expect(!sonde.allumer && !charge_rapide_voulue(RAPIDE, sonde.allumer), "sonde du démarrage : rapide coupée");
        chargeur_tick(c, 0, false, 8 * S);
        chargeur_lecture(c, 7.4f, 8 * S);
        const ActionChargeur charge = chargeur_tick(c, 0, false, 9 * S);
        expect(charge.allumer && charge_rapide_voulue(RAPIDE, charge.allumer), "batterie lue : rapide si choisie");
    }

    // Niveau (courbe mesurée, 15/10/2026 : cas détaillés dans tools/test_tab5_socle.cpp), consommation.
    expect(batterie_niveau_pct(6.6f) == 0.0f && batterie_niveau_pct(8.36f) == 100.0f, "niveau : 6,6 V = 0 %, 8,36 V = 100 %");
    expect(std::isnan(batterie_niveau_pct(NAN)), "niveau : tension inconnue");
    expect(std::fabs(batterie_puissance_w(7.4f, 0.4f, true) - 2.96f) < 0.001f, "consommation : 7,4 V × 0,4 A = 2,96 W");
    expect(std::isnan(batterie_puissance_w(7.4f, 0.4f, false)), "consommation : inconnue sur secteur");
    expect(std::isnan(batterie_puissance_w(7.4f, 0.01f, true)), "consommation : sous 20 mA, rien");
    expect(std::isnan(batterie_puissance_w(7.4f, -0.3f, true)), "consommation : batterie qui charge, rien");
    expect(std::isnan(batterie_puissance_w(NAN, 0.4f, true)), "consommation : tension inconnue");

    // Alerte de batterie faible : une fois par seuil, réarmée sur secteur ou à 30 %.
    EtatAlerteBatterie al;
    expect(batterie_alerte_lue(al, 25.0f, true) == 0, "alerte : 25 %, rien");
    expect(batterie_alerte_lue(al, 19.0f, true) == 20, "alerte : sous 20 %");
    expect(batterie_alerte_lue(al, 18.0f, true) == 0, "alerte : une seule fois");
    expect(batterie_alerte_lue(al, NAN, true) == 0, "alerte : niveau inconnu, rien");
    expect(batterie_alerte_lue(al, 10.0f, true) == 10, "alerte : 10 %, presque vide");
    expect(batterie_alerte_lue(al, 5.0f, true) == 0, "alerte : 10 % une seule fois");
    expect(batterie_alerte_lue(al, 22.0f, true) == 0, "alerte : remontée à 22 %, pas réarmée");
    expect(batterie_alerte_lue(al, 19.0f, true) == 0, "alerte : sous 20 % de nouveau, déjà envoyée");
    expect(batterie_alerte_lue(al, 19.0f, false) == 0, "alerte : sur secteur, rien et réarmée");
    expect(batterie_alerte_lue(al, 19.0f, true) == 20, "alerte : débranchée à 19 %, de nouveau");
    expect(batterie_alerte_lue(al, 31.0f, true) == 0 && batterie_alerte_lue(al, 20.0f, true) == 20,
           "alerte : réarmée à 30 %");
    EtatAlerteBatterie al2;
    expect(batterie_alerte_lue(al2, 8.0f, true) == 10 && batterie_alerte_lue(al2, 7.0f, true) == 0,
           "alerte : 8 % d'emblée, seulement « presque vide »");
}

// Console système, lignes « Batterie » et « Charge CPU » (discussion #278, 06/10/2026).
// La règle qui compte : jamais de pourcentage sans batterie détectée (le 8,39 V du
// chargeur ferait 100 %), quel que soit le niveau reçu. Sur batterie, la consommation
// remplace la tension (08/10/2026).
static void test_console_batterie_et_cpu() {
    using P = PresenceBatterie;
    char b[48];
    batterie_texte_console(b, sizeof(b), false, P::PRESENTE, 78.0f, 7.62f, NAN);
    expect_str(b, "Non mont\xC3\xA9" "e", "interrupteur éteint : « Non montée », même batterie détectée");
    batterie_texte_console(b, sizeof(b), true, P::ABSENTE, NAN, 1.9f, NAN);
    expect_str(b, "Sur USB", "sans batterie (1,9 V chargeur coupé) : « Sur USB »");
    batterie_texte_console(b, sizeof(b), true, P::ABSENTE, 100.0f, 8.39f, NAN);
    expect_str(b, "Sur USB", "sans batterie, niveau 100 % reçu quand même : pas de pourcentage");
    batterie_texte_console(b, sizeof(b), true, P::ABSENTE, 0.0f, 4.2f, 3.0f);
    expect_str(b, "Sur USB", "sans batterie, niveau et puissance reçus quand même : rien");
    batterie_texte_console(b, sizeof(b), true, P::INCONNUE, 50.0f, 7.4f, 2.5f);
    expect_str(b, "--", "avant la première décision : « -- », ni niveau ni tension");
    batterie_texte_console(b, sizeof(b), true, P::PRESENTE, 78.4f, 7.623f, NAN);
    expect_str(b, "78% \xC2\xB7 7.62 V", "batterie sur secteur : niveau puis tension");
    batterie_texte_console(b, sizeof(b), true, P::PRESENTE, 78.4f, 7.623f, 3.14f);
    expect_str(b, "78% \xC2\xB7 3.1 W", "sur batterie : niveau puis consommation");
    batterie_texte_console(b, sizeof(b), true, P::PRESENTE, NAN, 7.4f, 2.96f);
    expect_str(b, "-- \xC2\xB7 3.0 W", "sur batterie, niveau pas encore publié : la consommation seule");
    batterie_texte_console(b, sizeof(b), true, P::PRESENTE, NAN, 7.2f, NAN);
    expect_str(b, "-- \xC2\xB7 7.20 V", "batterie, niveau pas encore publié : la tension seule");
    batterie_texte_console(b, sizeof(b), true, P::PRESENTE, 100.0f, NAN, NAN);
    expect_str(b, "100%", "batterie, tension inconnue : le niveau seul");
    batterie_texte_console(b, 4, true, P::PRESENTE, 78.0f, 7.62f, NAN);
    expect(std::strlen(b) == 3, "tampon court : texte coupé, terminé");
    i18n_set_language(1);
    batterie_texte_console(b, sizeof(b), false, P::INCONNUE, NAN, NAN, NAN);
    expect_str(b, "Not fitted", "« Non montée » traduit en anglais");
    batterie_texte_console(b, sizeof(b), true, P::ABSENTE, NAN, NAN, NAN);
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

// Réglages en quatre pages (08/10/2026) : le geste boucle aux deux bouts ; page Batterie,
// jamais de valeur ni d'état « batterie » sans batterie détectée.
static void test_reglages_pages_et_batterie() {
    expect(reglages_page_voisine(0, 4, true) == 1, "Écran, geste vers la gauche : Apparence");
    expect(reglages_page_voisine(2, 4, true) == 3, "Batterie, vers la gauche : Système");
    expect(reglages_page_voisine(3, 4, true) == 0, "Système, vers la gauche : retour à Écran (boucle)");
    expect(reglages_page_voisine(0, 4, false) == 3, "Écran, vers la droite : Système (boucle)");
    expect(reglages_page_voisine(3, 4, false) == 2, "Système, vers la droite : Batterie");
    expect(reglages_page_voisine(7, 4, true) == 0 && reglages_page_voisine(-1, 4, false) == 0,
           "page hors bornes : la première");
    expect(reglages_page_voisine(0, 0, true) == 0, "aucune page : 0");
    expect(reglages_page_voisine(4, 5, true) == 0 && reglages_page_voisine(0, 5, false) == 4,
           "Réveil (5 pages) : Annonces → Heure et Heure → Annonces, en boucle");
    // Rouleau des pièces du mode HA (18/10/2026, tuiles_swipe_ha) : cinq gestes dans le même
    // sens passent par les cinq pièces, dans l'ordre, et reviennent à la première.
    int gauche = 0, droite = 0;
    char vues_g[6] = {}, vues_d[6] = {};
    for (int i = 0; i < 5; i++) {
        gauche = reglages_page_voisine(gauche, 5, true);
        droite = reglages_page_voisine(droite, 5, false);
        vues_g[i] = static_cast<char>('1' + gauche);
        vues_d[i] = static_cast<char>('1' + droite);
    }
    expect(std::strcmp(vues_g, "23451") == 0, "pièces, vers la gauche : 2, 3, 4, 5 puis la 1");
    expect(std::strcmp(vues_d, "54321") == 0, "pièces, vers la droite : 5, 4, 3, 2 puis la 1");

    using P = PresenceBatterie;
    expect_str(batterie_etat_texte(P::INCONNUE, true, true), "Mesure en cours", "avant la première décision");
    expect_str(batterie_etat_texte(P::ABSENTE, true, false), "Pas de batterie d\xC3\xA9tect\xC3\xA9" "e",
               "sans batterie, CHG_STAT dit « en charge » : ignoré");
    expect_str(batterie_etat_texte(P::PRESENTE, true, true), "Sur batterie", "courant de décharge : sur batterie");
    expect_str(batterie_etat_texte(P::PRESENTE, true, false), "En charge", "batterie et CHG_STAT : en charge");
    expect_str(batterie_etat_texte(P::PRESENTE, false, false), "Sur USB", "batterie pleine ou en pause : sur USB");

    char b[16];
    batterie_valeur_texte(b, sizeof(b), P::PRESENTE, 78.4f, MesureBatterie::NIVEAU);
    expect_str(b, "78 %", "niveau arrondi, espace avant %");
    batterie_valeur_texte(b, sizeof(b), P::PRESENTE, 7.623f, MesureBatterie::TENSION);
    expect_str(b, "7.62 V", "tension, comme la console");
    batterie_valeur_texte(b, sizeof(b), P::PRESENTE, 3.14f, MesureBatterie::CONSOMMATION);
    expect_str(b, "3.1 W", "consommation, comme la console");
    batterie_valeur_texte(b, sizeof(b), P::ABSENTE, 8.39f, MesureBatterie::TENSION);
    expect_str(b, "--", "sans batterie : la tension du chargeur n'est pas montrée");
    batterie_valeur_texte(b, sizeof(b), P::INCONNUE, 100.0f, MesureBatterie::NIVEAU);
    expect_str(b, "--", "avant la première décision : pas de niveau");
    batterie_valeur_texte(b, sizeof(b), P::PRESENTE, NAN, MesureBatterie::CONSOMMATION);
    expect_str(b, "--", "consommation inconnue (sur secteur) : « -- »");
    i18n_set_language(1);
    expect_str(batterie_etat_texte(P::ABSENTE, false, false), "No battery detected", "traduit en anglais");
    i18n_set_language(0);
}

// Mode économie d'énergie (06/10/2026) : sur batterie d'après le courant de l'INA226
// (+ = décharge), batterie basse à 35 % (retour à 60 % depuis le 14/10/2026), et la
// décision appliquée.
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
        expect(!economie_niveau_lu(e, 38.0f) && e.batterie_basse, "éco : 38 %, reste basse (retour à 60 %)");
        expect(!economie_niveau_lu(e, NAN) && e.batterie_basse, "éco : niveau inconnu, décision gardée");
        expect(!economie_niveau_lu(e, 40.0f) && e.batterie_basse, "éco : 40 % (l'ancien retour), reste basse");
        expect(!economie_niveau_lu(e, 59.0f) && e.batterie_basse, "éco : 59 %, reste basse");
        expect(economie_niveau_lu(e, 60.0f) && !e.batterie_basse, "éco : 60 % pile, plus basse");
        expect(!economie_niveau_lu(e, 36.0f) && !e.batterie_basse, "éco : 36 % en descendant, pas encore basse");
        expect(economie_niveau_lu(e, 35.0f) && e.batterie_basse, "éco : 35 % pile, basse");
    }
    {
        // L'écart en TENSION entre basse et retour, sur la courbe (mesurée le 15/10/2026) : au
        // moins les 111,5 mV de l'ancienne droite (2,23 V × 5 %). Première tension (au mV)
        // qui donne chaque seuil.
        auto tension_mv = [](float pct) {
            for (int mv = 6000; mv <= 8600; mv++)
                if (batterie_niveau_pct(mv / 1000.0f) >= pct) return mv;
            return -1;
        };
        const int bas = tension_mv(kEcoNiveauBas);
        const int retour = tension_mv(kEcoNiveauRetour);
        expect(std::abs(bas - 7723) <= 1 && std::abs(retour - 7980) <= 1, "éco : basse à 7,723 V, retour à 7,980 V lus");
        expect(retour - bas >= 112, "éco : au moins 0,11 V entre basse et retour (14 mV avec un retour à 40 %)");
        expect(tension_mv(40.0f) - bas < 112, "éco : un retour à 40 % serait trop près (contre-épreuve)");
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
        expect(!d.active && d.plafond == 1.0f && d.periode_ms == kEcoPeriodeNormaleMs &&
                   d.animations == ChoixAnimations::COMPLETES,
               "éco « Jamais » : rien ne change, même sur batterie basse");
        in.choix = static_cast<uint8_t>(ChoixEconomie::SUR_BATTERIE);
        in.sur_batterie = false;
        in.batterie_basse = false;
        d = economie_decider(in);
        expect(!d.active && d.plafond == 1.0f, "éco « Sur batterie » sur secteur : inactif");
        in.sur_batterie = true;
        d = economie_decider(in);
        expect(d.active && d.plafond == kEcoPlafond && d.periode_ms == kEcoPeriodeMs &&
                   d.animations == ChoixAnimations::AUCUNE,
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
        expect(d.periode_ms == kEcoPeriodeNormaleMs && d.animations == ChoixAnimations::AUCUNE,
               "éco : un jeu garde ses 60 images/s");
        in.choix = 7;
        d = economie_decider(in);
        expect(!d.active, "éco : index inconnu = Jamais");
    }
    expect(kEcoPlancher == 0.10f && kEcoPlafond == 0.50f && kEcoNiveauBas == 35.0f && kEcoAssombrirMs == 30000u,
           "éco : plancher 10 % (minimum du curseur), plafond 50 %, basse à 35 %, 30 s");

    // Animations (09/10/2026, ADR-0045) : un seul point de décision, le mode économie
    // actif impose « Aucune ».
    {
        using A = ChoixAnimations;
        constexpr uint8_t COMPLETES = 0, ESSENTIELLES = 1, AUCUNE = 2;
        expect(animations_effectives(COMPLETES, false) == A::COMPLETES, "animations : « Complètes » choisi");
        expect(animations_effectives(ESSENTIELLES, false) == A::ESSENTIELLES, "animations : « Essentielles » choisi");
        expect(animations_effectives(AUCUNE, false) == A::AUCUNE, "animations : « Aucune » choisi");
        expect(animations_effectives(9, false) == A::COMPLETES, "animations : index inconnu = « Complètes »");
        expect(animations_effectives(COMPLETES, true) == A::AUCUNE && animations_effectives(ESSENTIELLES, true) == A::AUCUNE,
               "animations : mode économie actif, « Aucune » quel que soit le choix");
        EntreesEconomie in;
        in.animations = ESSENTIELLES;
        in.choix = static_cast<uint8_t>(ChoixEconomie::SUR_BATTERIE);
        in.sur_batterie = false;
        expect(economie_decider(in).animations == A::ESSENTIELLES, "animations : éco inactive, le choix passe");
        in.sur_batterie = true;
        expect(economie_decider(in).animations == A::AUCUNE, "animations : éco active (sur batterie), « Aucune »");
        in.choix = static_cast<uint8_t>(ChoixEconomie::JAMAIS);
        expect(economie_decider(in).animations == A::ESSENTIELLES, "animations : éco « Jamais », le choix passe");
    }

    // Wi-Fi éco (09/10/2026, ADR-0045) : mêmes options que le mode économie, jamais
    // pendant un flux (voix, son, OTA).
    {
        constexpr uint8_t JAMAIS = 0, SUR_BATTERIE = 1, TOUJOURS = 2;
        expect(!wifi_eco_voulu(JAMAIS, true, false), "Wi-Fi éco « Jamais » : jamais, même sur batterie");
        expect(!wifi_eco_voulu(SUR_BATTERIE, false, false), "Wi-Fi éco « Sur batterie » sur secteur : non");
        expect(wifi_eco_voulu(SUR_BATTERIE, true, false), "Wi-Fi éco « Sur batterie » sur batterie : oui");
        expect(wifi_eco_voulu(TOUJOURS, false, false), "Wi-Fi éco « Toujours » : oui, sur secteur aussi");
        expect(!wifi_eco_voulu(TOUJOURS, true, true), "Wi-Fi éco : jamais pendant la voix, un son ou une OTA");
        expect(!wifi_eco_voulu(7, true, false), "Wi-Fi éco : index inconnu = « Jamais »");
    }
}

// Rouleaux du popup Réveil (09/10/2026) : une valeur hors pas venue de HA est montrée
// telle quelle (insérée à sa place), jamais arrondie en silence ; index ↔ valeur
// réciproques, bornes tenues.
static void test_rouleaux() {
    // Minutes de 5 en 5 : 00, 05 … 55 (12 options).
    expect(rouleau_extra(0, 59, 5, 15) == -1, "minute sur le pas : pas d'extra");
    expect(rouleau_extra(0, 59, 5, 3) == 3 && rouleau_extra(0, 59, 5, 57) == 57, "minute hors pas : extra");
    expect(rouleau_extra(0, 59, 5, 75) == -1 && rouleau_extra(0, 59, 5, -2) == -1, "hors bornes : pas d'extra");
    expect(rouleau_nombre(0, 59, 5, -1) == 12, "minutes : 12 options");
    expect(rouleau_nombre(0, 59, 5, 3) == 13, "minutes avec 03 : 13 options");
    expect(rouleau_valeur(0, 59, 5, -1, 0) == 0 && rouleau_valeur(0, 59, 5, -1, 11) == 55, "première et dernière");
    expect(rouleau_valeur(0, 59, 5, -1, 12) == -1 && rouleau_valeur(0, 59, 5, -1, -1) == -1, "index hors bornes");
    expect(rouleau_valeur(0, 59, 5, 3, 1) == 3 && rouleau_valeur(0, 59, 5, 3, 2) == 5, "03 entre 00 et 05");
    expect(rouleau_valeur(0, 59, 5, 57, 12) == 57 && rouleau_valeur(0, 59, 5, 57, 11) == 55, "57 après 55");
    expect(rouleau_index(0, 59, 5, 3, 3) == 1 && rouleau_index(0, 59, 5, 3, 5) == 2, "index autour de l'extra");
    expect(rouleau_index(0, 59, 5, -1, 58) == 11, "58 sans extra : 55, le pas le plus proche dans les bornes");
    expect(rouleau_index(0, 59, 5, -1, 7) == 1, "7 sans extra : 05");
    // Réciprocité sur tous les rouleaux du popup (bornes et pas de tab5-alarm.yaml).
    const int r[][3] = {{0, 23, 1}, {0, 59, 5}, {0, 1439, 15}, {0, 240, 5}, {0, 14, 1}, {1, 30, 1}, {1, 60, 1}, {0, 120, 5}};
    for (const auto& d : r) {
        for (int extra : {-1, d[0] + 1}) {
            const int e = rouleau_extra(d[0], d[1], d[2], extra);
            const int n = rouleau_nombre(d[0], d[1], d[2], e);
            bool ok = n >= 1;
            for (int i = 0; i < n && ok; i++) ok = rouleau_index(d[0], d[1], d[2], e, rouleau_valeur(d[0], d[1], d[2], e, i)) == i;
            expect(ok, "rouleau : index → valeur → index identique");
        }
    }
    expect(rouleau_index(1, 30, 1, -1, 0) == 0 && rouleau_index(1, 30, 1, -1, 99) == 29, "valeur ramenée dans les bornes");
    expect(rouleau_valeur(0, 1439, 15, -1, 95) == 1425, "pas avant / après : 23:45 en dernier");
    expect(rouleau_nombre(0, 10, 0, -1) == 11, "pas nul compté comme 1");
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
    test_chargeur();
    test_console_batterie_et_cpu();
    test_reglages_pages_et_batterie();
    test_rouleaux();
    test_economie();

    std::printf("=== %s (%d OK, %d FAIL) ===\n", g_fail ? "FAILED" : "ALL PASSED", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
