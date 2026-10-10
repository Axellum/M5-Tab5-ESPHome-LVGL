/**
 * Tests hôte du socle C++ commun (audit du 07/10/2026, lot L5), sans ESPHome ni LVGL :
 *   - lecture bornée des payloads (Tab5/socle/tab5_champs.h/.cpp) : découpe, champs vides,
 *     nombres (longueur, illisible, non fini), entiers non signés, plafond commun ;
 *   - dates et heures (Tab5/socle/tab5_core.h/.cpp) : jour civil, jours du mois, heure
 *     valide, « HH:MM » strict, embauche tôt ;
 *   - ce que les deux modèles de tuiles partagent (Tab5/socle/tab5_modele_ha.h) ;
 *   - la géométrie partagée (Tab5/socle/tab5_geometrie.h), par des static_assert ;
 *   - la chronologie du démarrage (Tab5/socle/tab5_demarrage.h, 10/10/2026).
 *
 * Build & run (CI, job `python` de .github/workflows/esphome-tab5.yml) :
 *   g++ -std=c++17 -O2 -Wall -Wextra -I Tab5/socle -o test_tab5_socle \
 *       tools/test_tab5_socle.cpp Tab5/socle/tab5_champs.cpp Tab5/socle/tab5_core.cpp Tab5/socle/tab5_i18n.cpp
 *   ./test_tab5_socle
 * Le poste de dev n'a qu'un cross-compilateur RISC-V ; vérif locale possible :
 *   riscv32-esp-elf-g++ -std=c++17 -fsyntax-only -Wall -Wextra -I Tab5/socle tools/test_tab5_socle.cpp
 */
#include "tab5_champs.h"
#include "tab5_core.h"
#include "tab5_demarrage.h"
#include "tab5_geometrie.h"
#include "tab5_modele_ha.h"

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

static bool champ_vaut(const Champ& c, const char* attendu) {
    return c.n == std::strlen(attendu) && std::memcmp(c.p, attendu, c.n) == 0;
}

static bool egal_ou_nan(float a, float b) { return (std::isnan(a) && std::isnan(b)) || a == b; }

// ── Géométrie (tab5_geometrie.h) : vérifiée à la compilation ────────────────
static_assert(kCorpsW == 1202, "corps des popups à cartes : x 24..1226");
static_assert(kCorpsX + kCorpsW + kCorpsX == kCarteL, "corps centré dans la carte");
static_assert(kEcranL - kCarteL == 30 && kEcranH - kCarteH == 30, "carte à 15 px des bords");
static_assert(kGraphiqueL == kCorpsW - 2 * 18, "zone du graphique : 18 px de marge dans sa carte");
static_assert(kPieces == 5 && kTuiles == 5, "cinq pièces, cinq tuiles (ADR-0023)");

// ════════════════════════════════════════════════════════════════════════════

static void test_champ_suivant() {
    const std::string s = "a||bc;";
    const char* p = s.data();
    const char* fin = p + s.size();
    expect(champ_vaut(champ_suivant(p, fin, '|'), "a"), "champ_suivant : premier champ");
    expect(champ_vaut(champ_suivant(p, fin, '|'), ""), "champ_suivant : champ vide compté");
    const Champ c = champ_suivant(p, fin, '|');
    expect(champ_vaut(c, "bc;") && p == fin, "champ_suivant : dernier champ jusqu'à la fin");
    expect(champ_vaut(champ_suivant(p, fin, '|'), "") && p == fin, "champ_suivant : après la fin, vide");
    // « 3.1;; » : trois valeurs, la dernière vide (DO-11, énergie) — p avance après un « ; ».
    const std::string v = "3.1;;";
    p = v.data();
    fin = p + v.size();
    const Champ c1 = champ_suivant(p, fin, ';');
    expect(p > c1.p + c1.n, "champ_suivant : terminé sur un séparateur");
    champ_suivant(p, fin, ';');
    expect(p == fin, "champ_suivant : « ;; » mange le second séparateur");
}

static void test_decouper() {
    Champ f[4];
    const char* s = "a|b|c|d|e";
    int n = champs_decouper(s, std::strlen(s), '|', f, 3);
    expect(n == 3 && champ_vaut(f[2], "c"), "champs_decouper : au-delà de max, le reste est ignoré");
    n = champs_decouper_reste(s, std::strlen(s), '|', f, 3);
    expect(n == 3 && champ_vaut(f[2], "c|d|e"), "champs_decouper_reste : le dernier prend le reste");
    n = champs_decouper("", 0, '|', f, 4);
    expect(n == 1 && f[0].n == 0, "champs_decouper : un texte vide donne un champ vide");
    n = champs_decouper("|", 1, '|', f, 4);
    expect(n == 2 && f[0].n == 0 && f[1].n == 0, "champs_decouper : « | » donne deux champs vides");
    n = champs_decouper("a||b", 4, '|', f, 4);
    expect(n == 3 && f[1].n == 0 && champ_vaut(f[2], "b"), "champs_decouper : champs vides gardés");
    n = champs_decouper_reste("1|2", 3, '|', f, 1);
    expect(n == 1 && champ_vaut(f[0], "1|2"), "champs_decouper_reste : max 1 = tout");
    expect(champs_decouper("a|b", 3, '|', f, 0) == 0, "champs_decouper : max 0");
    expect(champ_est(Champ{"1", 1}, "1") && !champ_est(Champ{"10", 2}, "1") && !champ_est(Champ{"", 0}, "1"),
           "champ_est : longueur et octets");
}

static void test_nombres() {
    expect(champ_nombre("21.5", 4, NAN) == 21.5f, "nombre : 21.5");
    expect(champ_nombre("-3", 2, NAN) == -3.0f, "nombre : négatif");
    expect(champ_nombre("21.5 °C", 8, NAN) == 21.5f, "nombre : unité après le nombre ignorée");
    expect(std::isnan(champ_nombre("", 0, NAN)), "nombre : champ vide = défaut");
    expect(champ_nombre("", 0, 7.0f) == 7.0f, "nombre : défaut rendu tel quel");
    expect(champ_nombre("unknown", 7, 1.0f) == 1.0f, "nombre : illisible = défaut");
    expect(champ_nombre("nan", 3, 2.0f) == 2.0f, "nombre : « nan » = défaut");
    expect(champ_nombre("inf", 3, 2.0f) == 2.0f, "nombre : « inf » = défaut");
    expect(champ_nombre("1e99", 4, 2.0f) == 2.0f, "nombre : hors des float = défaut");
    // Un flottant de HA écrit en entier (18 caractères) : lu (la clim le perdait, tampon 16).
    const char* long_ha = "21.299999999999997";
    expect(std::fabs(champ_nombre(long_ha, std::strlen(long_ha), NAN) - 21.3f) < 1e-4f,
           "nombre : 18 caractères lus");
    // 31 caractères : la limite ; 32 : défaut, jamais un nombre tronqué.
    std::string borne(kChampNombreMax, '0');
    borne[0] = '1';
    expect(champ_nombre(borne.data(), borne.size(), NAN) > 1e29f, "nombre : 31 caractères lus");
    std::string trop = "1" + std::string(kChampNombreMax, '0');
    expect(std::isnan(champ_nombre(trop.data(), trop.size(), NAN)), "nombre : 32 caractères = défaut");
    // Le champ n'est pas zéro-terminé : seuls ses n octets comptent.
    expect(champ_nombre("12|34", 2, NAN) == 12.0f, "nombre : lu dans ses n octets seulement");
    expect(egal_ou_nan(champ_nombre(Champ{"0.5", 3}, NAN), 0.5f), "nombre : surcharge Champ");
    expect(std::isnan(champ_nombre(nullptr, 3, NAN)), "nombre : pointeur nul = défaut");
}

static void test_entiers() {
    const uint32_t kMax = 4102444800u;  // l'epoch maximal des alertes (2100)
    expect(champ_entier("1790000000", 10, kMax, 0) == 1790000000u, "entier : epoch");
    expect(champ_entier("", 0, kMax, 0) == 0, "entier : vide = défaut");
    expect(champ_entier("abc", 3, kMax, 9) == 9, "entier : illisible = défaut");
    expect(champ_entier("-1", 2, kMax, 0) == 0, "entier : « -1 » au-dessus du maximum = défaut");
    expect(champ_entier("4102444801", 10, kMax, 0) == 0, "entier : au-dessus du maximum = défaut");
    expect(champ_entier("4102444800", 10, kMax, 0) == kMax, "entier : maximum compris");
    expect(champ_entier("1234567890123456", 16, kMax, 5) == 5, "entier : 16 caractères = défaut");
    expect(champ_entier(Champ{"42|", 2}, 100, 0) == 42, "entier : surcharge Champ, n octets");
    expect(kPayloadMax == 16u * 1024u, "plafond commun : 16 Ko");
}

static void test_dates() {
    expect(jour_civil(1970, 1, 1) == 0, "jour civil : 01/01/1970 = 0");
    expect(jour_civil(1969, 12, 31) == -1, "jour civil : la veille = -1");
    expect(jour_civil(2000, 2, 29) == 11016, "jour civil : 29/02/2000");
    expect(jour_civil(2026, 9, 25) == 20721, "jour civil : 25/09/2026 (= local_day_number_today)");
    expect(jour_civil(2100, 3, 1) == 47541, "jour civil : 01/03/2100 (2100 pas bissextile)");
    expect(jour_civil(2200, 12, 31) == 84370, "jour civil : 31/12/2200 (borne de l'historique)");

    expect(annee_bissextile(2024) && annee_bissextile(2000) && !annee_bissextile(2100) && !annee_bissextile(2026),
           "bissextile : 4, 100, 400");
    expect(jours_du_mois(2026, 2) == 28 && jours_du_mois(2024, 2) == 29, "jours du mois : février");
    expect(jours_du_mois(2026, 4) == 30 && jours_du_mois(2026, 12) == 31, "jours du mois : avril, décembre");
    expect(jours_du_mois(2026, 0) == 31 && jours_du_mois(2026, 13) == 31, "jours du mois : hors bornes = 31");

    expect(!tab5_heure_valide(0) && !tab5_heure_valide(kHeureValideMin - 1), "heure valide : avant 2020");
    expect(tab5_heure_valide(kHeureValideMin) && tab5_heure_valide(1790000000), "heure valide : dès 2020");
    g_now = kHeureValideMin - 3600 * 24;  // 31/12/2019 : pas encore réglée
    expect(local_day_number_today() == -1, "jour d'aujourd'hui : -1 avant l'heure valide");
    g_now = 1790000000;
    expect(local_day_number_today() > 20000, "jour d'aujourd'hui : connu après");
}

static void test_hhmm() {
    expect(hhmm_minutes("00:00") == 0 && hhmm_minutes("23:59") == 23 * 60 + 59, "HH:MM : bornes");
    expect(hhmm_minutes("08:30-16:00") == 510, "HH:MM : suivi d'autre chose");
    expect(hhmm_minutes("24:00") == -1 && hhmm_minutes("12:60") == -1, "HH:MM : hors bornes");
    expect(hhmm_minutes("8:30") == -1 && hhmm_minutes("08h30") == -1 && hhmm_minutes("ab:cd") == -1,
           "HH:MM : format strict");
    expect(hhmm_minutes("") == -1 && hhmm_minutes("0") == -1 && hhmm_minutes("08:3") == -1,
           "HH:MM : texte court (pas de lecture au-delà du zéro final)");
    expect(hhmm_minutes(static_cast<const char*>(nullptr)) == -1, "HH:MM : pointeur nul");
    const std::string h = "08:00-16:30";
    expect(hhmm_minutes(h, 0) == 480 && hhmm_minutes(h, 6) == 990, "HH:MM : début et fin d'un créneau");
    expect(hhmm_minutes(h, 11) == -1 && hhmm_minutes(h, 99) == -1, "HH:MM : position hors du texte");
    expect(cal_is_early_shift("08:59-16:00") && !cal_is_early_shift("09:00-17:00"), "embauche tôt : avant 09:00");
    expect(!cal_is_early_shift("") && !cal_is_early_shift("xx:yy-16:00") && !cal_is_early_shift("08"),
           "embauche tôt : heure illisible = non");
}

static void test_modele_ha() {
    char ic[modele_ha::kIcone];
    modele_ha::copier_icone(ic, "ampoule_2", 9);
    expect(std::strcmp(ic, "ampoule_2") == 0, "icône : code valide copié");
    modele_ha::copier_icone(ic, "mdi:lamp", 8);
    expect(ic[0] == '\0', "icône : caractère hors [a-z0-9_] = vide");
    modele_ha::copier_icone(ic, "abcdefghijklmnop", 16);
    expect(ic[0] == '\0', "icône : 16 caractères = vide, pas coupé");
    modele_ha::copier_icone(ic, "abcdefghijklmno", 15);
    expect(std::strlen(ic) == 15, "icône : 15 caractères gardés");
    modele_ha::copier_icone(ic, "x", 0);
    expect(ic[0] == '\0', "icône : vide");
    expect(modele_ha::etat_indisponible("unavailable") && modele_ha::etat_indisponible("unknown") &&
               !modele_ha::etat_indisponible("on") && !modele_ha::etat_indisponible(""),
           "état indisponible : unavailable, unknown");
    expect(modele_ha::kNom == 25 && modele_ha::kEtat == 16, "tailles gardées");
    expect(std::strcmp(modele_ha::tuile_cle(0, 4).s, "t04") == 0 && std::strcmp(modele_ha::tuile_cle(4, 0).s, "t40") == 0,
           "clé de tuile : tRT");
    expect(std::strcmp(modele_ha::piece_cle(3).s, "p3") == 0, "clé de pièce : pR");
    expect(std::strcmp(modele_ha::clim_piece_cle(0).s, "cp0") == 0 && std::strcmp(modele_ha::clim_piece_cle(4).s, "cp4") == 0,
           "clé de la clim d'une pièce : cpR (ADR-0040)");
}

// ── Chronologie du démarrage (tab5_demarrage.h, 10/10/2026) ─────────────────
static void test_chronologie() {
    ChronoDemarrage c;
    char buf[kChronoTexteMax];
    chrono_texte(c, buf, sizeof(buf));
    expect(std::strcmp(buf, "expandeur=-; retro=-; dessin=-; image=-; wifi=-; api=-") == 0,
           "chronologie : rien de vu, chaque étape à « - »");
    expect(chrono_marquer(c, EtapeDemarrage::EXPANDEUR, 1490) && chrono_vue(c, EtapeDemarrage::EXPANDEUR),
           "chronologie : étape marquée");
    expect(!chrono_marquer(c, EtapeDemarrage::EXPANDEUR, 9000) && c.ms[0] == 1490,
           "chronologie : une seule fois par démarrage (la première)");
    chrono_marquer(c, EtapeDemarrage::IMAGE, 8650);
    chrono_marquer(c, EtapeDemarrage::RETRO, 0);
    chrono_texte(c, buf, sizeof(buf));
    expect(std::strcmp(buf, "expandeur=1490; retro=0; dessin=-; image=8650; wifi=-; api=-") == 0,
           "chronologie : ordre des étapes, pas celui des marques ; 0 ms est une valeur");
    expect(!chrono_marquer(c, EtapeDemarrage::NOMBRE, 1) && !chrono_vue(c, EtapeDemarrage::NOMBRE),
           "chronologie : étape hors liste ignorée");
    ChronoDemarrage pleine;
    for (size_t i = 0; i < kEtapesDemarrage; i++)
        chrono_marquer(pleine, static_cast<EtapeDemarrage>(i), 0xFFFFFFFFu);
    const size_t n = chrono_texte(pleine, buf, sizeof(buf));
    expect(n == std::strlen(buf) && n + 1 < sizeof(buf), "chronologie : toutes les étapes au maximum tiennent");
    expect(n < 255, "chronologie : moins de 255 caractères (état texte de Home Assistant)");
    char petit[12];
    const size_t m = chrono_texte(pleine, petit, sizeof(petit));
    expect(m == sizeof(petit) - 1 && std::strlen(petit) == m && std::strncmp(petit, "expandeur=4", 11) == 0,
           "chronologie : tampon trop petit, texte coupé et terminé");
    expect(chrono_texte(pleine, nullptr, 8) == 0 && chrono_texte(pleine, petit, 0) == 0,
           "chronologie : pas de tampon, rien d'écrit");
    expect(demarrage_marquer(EtapeDemarrage::WIFI, 10550) && !demarrage_marquer(EtapeDemarrage::WIFI, 1) &&
               chrono_demarrage().ms[static_cast<size_t>(EtapeDemarrage::WIFI)] == 10550,
           "chronologie de la tablette : un seul état");
}

int main() {
    setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);  // Europe/Paris, comme le firmware
    tzset();
    tab5_time_source = fake_time;

    test_champ_suivant();
    test_decouper();
    test_nombres();
    test_entiers();
    test_dates();
    test_hhmm();
    test_modele_ha();
    test_chronologie();

    std::printf("=== %s (%d OK, %d FAIL) ===\n", g_fail ? "FAILED" : "ALL PASSED", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
