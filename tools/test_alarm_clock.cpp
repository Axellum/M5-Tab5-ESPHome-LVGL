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
 * changements d'heure, rendez-vous.
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

    std::printf("=== %s (%d OK, %d FAIL) ===\n", g_fail ? "FAILED" : "ALL PASSED", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
