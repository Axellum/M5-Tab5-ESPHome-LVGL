/**
 * Tests hôte du socle C++ commun (audit du 07/10/2026, lot L5), sans ESPHome ni LVGL :
 *   - lecture bornée des payloads (Tab5/socle/tab5_champs.h/.cpp) : découpe, champs vides,
 *     nombres (longueur, illisible, non fini), entiers non signés, plafond commun ;
 *   - dates et heures (Tab5/socle/tab5_core.h/.cpp) : jour civil, jours du mois, heure
 *     valide, « HH:MM » strict, embauche tôt ;
 *   - ce que les deux modèles de tuiles partagent (Tab5/socle/tab5_modele_ha.h), dont la
 *     clim « d'ici » de la tuile − / + (clim_ici_rang, 14/10/2026) ;
 *   - la géométrie partagée (Tab5/socle/tab5_geometrie.h), par des static_assert, et le sens
 *     de l'écran d'une carte montée à l'envers (rotation_sens, rotation_repos ; ADR-0069) ;
 *   - la chronologie du démarrage (Tab5/socle/tab5_demarrage.h, 10/10/2026) ;
 *   - le niveau de la batterie d'après sa tension (Tab5/socle/tab5_batterie.h, courbe de
 *     décharge mesurée sur un Tab5, 15/10/2026) ;
 *   - la découpe d'une zone à redessiner autour d'une carte opaque (Tab5/socle/tab5_decoupe.h,
 *     popup Musique, 17/10/2026) : part couverte alignée, morceaux disjoints dont l'union est
 *     la zone, pixel par pixel ;
 *   - la disposition des éléments d'une pièce sur les cinq places (Tab5/socle/tab5_disposition.h,
 *     ADR-0066, 18/10/2026) : table de l'ADR, n = 0 … 20, pièces à trous, positions fixes du mode
 *     météo, rectangles des cases.
 *
 * Build & run (CI, job `python` de .github/workflows/esphome-tab5.yml) :
 *   g++ -std=c++17 -O2 -Wall -Wextra -I Tab5/socle -o test_tab5_socle \
 *       tools/test_tab5_socle.cpp Tab5/socle/tab5_champs.cpp Tab5/socle/tab5_core.cpp Tab5/socle/tab5_i18n.cpp
 *   ./test_tab5_socle
 * Le poste de dev n'a qu'un cross-compilateur RISC-V ; vérif locale possible :
 *   riscv32-esp-elf-g++ -std=c++17 -fsyntax-only -Wall -Wextra -I Tab5/socle tools/test_tab5_socle.cpp
 */
#include "tab5_barre.h"
#include "tab5_batterie.h"
#include "tab5_champs.h"
#include "tab5_core.h"
#include "tab5_decoupe.h"
#include "tab5_demarrage.h"
#include "tab5_disposition.h"
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
static_assert(kPiecesMax == 10 && kPiecesDefaut == 5 && kPagesMeteo == 5 && kPlacesTuiles == 5,
              "dix pièces au plus, cinq sans « pn », cinq pages de météo, cinq places (ADR-0023, ADR-0066)");
static_assert(kTuilesMax == 20 && kTuilesMax == disposition::kElementsMax && kPlacesTuiles == disposition::kPlaces,
              "vingt éléments par pièce sur cinq places (ADR-0066)");
static_assert(kTuilesMax == modele_ha::kTuilesCles, "une clé « tRT » par élément (ADR-0066)");

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

static void test_majuscules() {
    char t[64];
    std::snprintf(t, sizeof(t), "batterie r\xC3\xA9seau \xC2\xB7 d\303\251cembre");
    texte_majuscules(t);
    expect(std::strcmp(t, "BATTERIE R\xC3\x89SEAU \xC2\xB7 D\303\211CEMBRE") == 0,
           "majuscules : é → É, le point médian (C2 B7) ne bouge pas");
    std::snprintf(t, sizeof(t), "ao\xC3\xBBt \xC5\x93uvre \xC3\xB7 \xC3\xBF \xC3\x9F 1-2");
    texte_majuscules(t);
    expect(std::strcmp(t, "AO\xC3\x9BT \xC5\x92UVRE \xC3\xB7 \xC3\xBF \xC3\x9F 1-2") == 0,
           "majuscules : û, œ ; ÷, ÿ, ß et les chiffres restent");
    std::snprintf(t, sizeof(t), "\xE2\x89\x88 a");   // ≈ sur trois octets
    texte_majuscules(t);
    expect(std::strcmp(t, "\xE2\x89\x88 A") == 0, "majuscules : un caractère sur trois octets reste");
    texte_majuscules(nullptr);
    t[0] = '\0';
    texte_majuscules(t);
    expect(t[0] == '\0', "majuscules : chaîne vide");
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
    // ADR-0066 : T = 0 … 19, « 0 » … « 9 » puis « a » … « j » ; la lecture fait l'inverse.
    expect(std::strcmp(modele_ha::tuile_cle(2, 10).s, "t2a") == 0 && std::strcmp(modele_ha::tuile_cle(4, 19).s, "t4j") == 0,
           "clé de tuile : T au-delà de 9 en lettre");
    bool aller_retour = modele_ha::kTuilesCles == 20;
    for (int t = 0; t < modele_ha::kTuilesCles; t++) {
        int lu = -1;
        aller_retour = aller_retour && modele_ha::tuile_index(modele_ha::tuile_car(t), lu) && lu == t;
    }
    expect(aller_retour, "clé de tuile : tuile_index relit tuile_car, 0 … 19");
    int lu = -1;
    expect(!modele_ha::tuile_index('k', lu) && !modele_ha::tuile_index('A', lu) && !modele_ha::tuile_index('/', lu) &&
               !modele_ha::tuile_index(':', lu) && !modele_ha::tuile_index('`', lu) && lu == -1,
           "clé de tuile : ni « k », ni majuscule, ni voisin des chiffres et des lettres");
    expect(std::strcmp(modele_ha::piece_cle(3).s, "p3") == 0, "clé de pièce : pR");
    expect(std::strcmp(modele_ha::clim_piece_cle(0).s, "cp0") == 0 && std::strcmp(modele_ha::clim_piece_cle(4).s, "cp4") == 0,
           "clé de la clim d'une pièce : cpR (ADR-0040)");
    // Dix pièces (ADR-0066) : clés à un chiffre, de 0 à 9.
    expect(std::strcmp(modele_ha::tuile_cle(9, 4).s, "t94") == 0 && std::strcmp(modele_ha::piece_cle(9).s, "p9") == 0 &&
               std::strcmp(modele_ha::clim_piece_cle(7).s, "cp7") == 0,
           "clés des pièces 5 à 9 : un chiffre");
    int r = -1;
    expect(modele_ha::piece_index('0', r) && r == 0 && modele_ha::piece_index('9', r) && r == 9,
           "chiffre de pièce : 0 à 9");
    r = 3;
    expect(!modele_ha::piece_index('n', r) && !modele_ha::piece_index('a', r) && !modele_ha::piece_index('/', r) && r == 3,
           "chiffre de pièce : « pn » n'est pas une pièce, r intact");
}

// ── Nombre de pièces et pages du bas (ADR-0066, lot A) ──────────────────────
static void test_pieces_pages() {
    expect(pieces_nombre_lire("8", 1) == 8 && pieces_nombre_lire("10", 2) == 10 && pieces_nombre_lire("1", 1) == 1,
           "pn : 1 à 10 lus");
    expect(pieces_nombre_lire("0", 1) == 1 && pieces_nombre_lire("12", 2) == 10, "pn : borné à 1 .. 10");
    expect(pieces_nombre_lire("", 0) == 5 && pieces_nombre_lire("x", 1) == 5 && pieces_nombre_lire("100", 3) == 5 &&
               pieces_nombre_lire("-3", 2) == 5 && pieces_nombre_lire(nullptr, 0) == 5,
           "pn illisible : cinq, comme sans pn");
    // Pages 0 à 4 : la table de l'ADR-0023 ; 5 et plus : page = pièce.
    const int attendu[kPiecesMax] = {4, 3, 0, 1, 2, 5, 6, 7, 8, 9};
    bool table = true;
    for (int p = 0; p < kPiecesMax; p++) table = table && piece_de_page(p) == attendu[p] && page_de_piece(attendu[p]) == p;
    expect(table, "pièce de chaque page et page de chaque pièce, l'une l'inverse de l'autre");
    expect(piece_de_page(-1) == -1 && piece_de_page(10) == -1 && page_de_piece(-1) == -1 && page_de_piece(10) == -1,
           "pièce et page hors bornes : -1");
    // Météo à cinq pages : l'[AI-WARNING] de forecast_page_suivante, inchangé.
    expect(meteo_page_suivante(2, true, 5) == 3 && meteo_page_suivante(3, true, 5) == 4 &&
               meteo_page_suivante(4, true, 5) == 2,
           "cinq pages, vers la gauche : 2 → 3 → 4 → 2");
    expect(meteo_page_suivante(2, false, 5) == 1 && meteo_page_suivante(1, false, 5) == 0 &&
               meteo_page_suivante(0, false, 5) == 2 && meteo_page_suivante(0, true, 5) == 1,
           "cinq pages, vers la droite : 2 → 1 → 0 → 2 ; 0 vers la gauche : 1");
    // Huit pièces : trois pages de plus après les jours 10-14, à gauche seulement.
    expect(meteo_page_suivante(4, true, 8) == 5 && meteo_page_suivante(5, true, 8) == 6 &&
               meteo_page_suivante(7, true, 8) == 2,
           "huit pièces, vers la gauche : 4 → 5 → 6 → 7 → 2");
    expect(meteo_page_suivante(2, false, 8) == 1 && meteo_page_suivante(0, false, 8) == 2 &&
               meteo_page_suivante(5, false, 8) == 4 && meteo_page_suivante(7, false, 8) == 6,
           "huit pièces, vers la droite : inchangé, et une page de pièce revient sur la précédente");
    expect(meteo_page_suivante(7, true, 5) == 2 && meteo_page_suivante(9, false, 6) == 2 &&
               meteo_page_suivante(4, true, 3) == 2 && meteo_page_suivante(9, true, 12) == 2,
           "page disparue (moins de pièces) : l'accueil ; pages bornées à 5 .. 10");
    // Mode HA : la page non vide la plus proche (pièces 0, 3 et 6 occupées).
    const uint32_t occupees = (1u << 0) | (1u << 3) | (1u << 6);
    expect(page_non_vide_proche(2, 7, occupees) == 2, "page de la pièce 0 occupée : elle-même");
    expect(page_non_vide_proche(0, 7, (1u << 3) | (1u << 6)) == 1, "page 0 vide : la page 1 (pièce 3)");
    expect(page_non_vide_proche(4, 7, (1u << 6) | (1u << 3)) == 6, "page 4 vide : la page 6 (à deux), pas la page 1 (à trois)");
    expect(page_non_vide_proche(4, 5, 1u << 6) == -1, "la pièce 6 hors des cinq pages : aucune");
    expect(page_non_vide_proche(4, 7, 1u << 6) == 6, "la pièce 6 sur sa page");
    expect(page_non_vide_proche(3, 5, (1u << 2) | (1u << 0)) == 2, "à égalité (pages 2 et 4), la plus proche de l'accueil");
    expect(page_non_vide_proche(2, 10, 0) == -1, "aucune pièce occupée : -1");
    // Popup Maison : cinq colonnes au plus par page, réparties.
    const int cas[][3] = {{0, 0, 0}, {1, 1, 1}, {5, 1, 5}, {6, 2, 3}, {7, 2, 4}, {8, 2, 4}, {9, 2, 5}, {10, 2, 5}};
    bool reparti = true;
    for (const auto& c : cas) {
        const PagesRepartition p = pages_repartir(c[0], 5);
        reparti = reparti && p.pages == c[1] && p.par_page == c[2];
    }
    expect(reparti, "Maison : 6 → 3 + 3, 7 → 4 + 3, 10 → 5 + 5");
}

// ── Clim « d'ici » de la tuile − / + (modele_ha::clim_ici_rang, 14/10/2026) ──
// Rangées comme clims_enumerer : blueprint (r = t = piece = -1), tuiles (t >= 0), puis la
// clim propre de chaque pièce (t = -1, r = piece).
struct RefClim {
    signed char r, t, piece;
};

static void test_clim_ici() {
    using modele_ha::clim_ici_rang;
    const RefClim bp = {-1, -1, -1};
    const RefClim tuile_salon = {0, 2, 0};      // tuile cli sans m, pièce 0
    const RefClim tuile_salon_2 = {0, 4, 0};
    const RefClim propre_salon = {0, -1, 0};    // clim propre de la pièce 0 (ADR-0040)
    const RefClim propre_bureau = {3, -1, 3};

    expect(clim_ici_rang<RefClim>(nullptr, 0, 0) == -1, "clim d'ici : aucune clim, aucune");
    // Une seule clim dans la maison : partout, mode météo compris, même hors de sa pièce.
    const RefClim une[] = {propre_bureau};
    expect(clim_ici_rang(une, 1, 3) == 0 && clim_ici_rang(une, 1, 0) == 0 && clim_ici_rang(une, 1, -1) == 0,
           "clim d'ici : une seule clim (celle du bureau), sur toutes les pièces");
    const RefClim une_bp[] = {bp};
    expect(clim_ici_rang(une_bp, 1, 2) == 0, "clim d'ici : seule la clim du blueprint, partout");
    // Plusieurs, avec la clim du blueprint.
    const RefClim l[] = {bp, tuile_salon, tuile_salon_2, propre_salon, propre_bureau};
    expect(clim_ici_rang(l, 5, 0) == 3, "clim d'ici : la clim propre de la pièce avant ses tuiles");
    expect(clim_ici_rang(l, 5, 3) == 4, "clim d'ici : la clim propre du bureau sur le bureau");
    expect(clim_ici_rang(l, 5, 1) == 0, "clim d'ici : pièce sans clim, celle du blueprint");
    expect(clim_ici_rang(l, 5, -1) == 0, "clim d'ici : mode météo, celle du blueprint");
    const RefClim tuiles[] = {bp, tuile_salon, tuile_salon_2, propre_bureau};
    expect(clim_ici_rang(tuiles, 4, 0) == 1, "clim d'ici : sans clim propre, la première tuile cli de la pièce");
    // Plusieurs, sans clim du blueprint : rien sur une pièce sans clim ni en mode météo.
    const RefClim sans_bp[] = {tuile_salon, propre_bureau};
    expect(clim_ici_rang(sans_bp, 2, 0) == 0 && clim_ici_rang(sans_bp, 2, 3) == 1,
           "clim d'ici : sans clim du blueprint, celle de la pièce");
    expect(clim_ici_rang(sans_bp, 2, 1) == -1 && clim_ici_rang(sans_bp, 2, -1) == -1,
           "clim d'ici : sans clim du blueprint, aucune sur une pièce sans clim");
    // Plus de huit clims (dix pièces, ADR-0066 ; relecture du 18/10/2026) : la clim de la
    // dixième pièce est la douzième de la liste, au-delà des huit pastilles du carrousel.
    // La règle voit toute la liste qu'on lui donne ; clim_ref_ici lui donne clims_toutes.
    RefClim maison[12] = {bp, tuile_salon};
    for (int r = 0; r < 10; r++) maison[2 + r] = {static_cast<signed char>(r), -1, static_cast<signed char>(r)};
    expect(clim_ici_rang(maison, 12, 9) == 11 && clim_ici_rang(maison, 12, 7) == 9,
           "clim d'ici : plus de huit clims, celle des pièces 8 et 10 (au-delà de la huitième)");
    expect(clim_ici_rang(maison, 12, 0) == 2 && clim_ici_rang(maison, 12, -1) == 0,
           "clim d'ici : plus de huit clims, la pièce 1 et le mode météo inchangés");
    // Les huit premières seules (l'ancien appel, plafonné aux pastilles) : la pièce 10 tombait
    // sur la clim du blueprint.
    expect(clim_ici_rang(maison, 8, 9) == 0, "clim d'ici : coupée à huit, la pièce 10 perdait la sienne");
    // Sans clim du blueprint, neuf clims de pièce : la neuvième pièce a la sienne (coupée à huit,
    // aucune : pas de ligne « Clim », et le toucher de la température visait la pièce 1).
    const RefClim* const pieces = maison + 2;
    expect(clim_ici_rang(pieces, 9, 8) == 8 && clim_ici_rang(pieces, 8, 8) == -1,
           "clim d'ici : sans clim du blueprint, la neuvième pièce, au-delà de huit");
}

// ── Chronologie du démarrage (tab5_demarrage.h, 10/10/2026) ─────────────────
static void test_chronologie() {
    ChronoDemarrage c;
    char buf[kChronoTexteMax];
    chrono_texte(c, PartieChrono::SUITE, buf, sizeof(buf));
    expect(std::strcmp(buf, "ordo=-; retro=-; dessin=-; image=-; tard=-; wifi=-; api=-; fin600=-; chargeur=-") == 0,
           "chronologie : rien de vu, chaque étape à « - »");
    chrono_texte(c, PartieChrono::SETUP, buf, sizeof(buf));
    expect(std::strncmp(buf, "ctor=-; objets=-; setup=-; bus=-; expandeur=-;", 46) == 0 &&
               std::strstr(buf, "zones=-") != nullptr && std::strstr(buf, "ordo") == nullptr,
           "chronologie : le premier texte s'arrête avant « ordo »");
    expect(chrono_marquer(c, EtapeDemarrage::EXPANDEUR, 1490) && chrono_vue(c, EtapeDemarrage::EXPANDEUR),
           "chronologie : étape marquée");
    expect(!chrono_marquer(c, EtapeDemarrage::EXPANDEUR, 9000) &&
               c.ms[static_cast<size_t>(EtapeDemarrage::EXPANDEUR)] == 1490,
           "chronologie : une seule fois par démarrage (la première)");
    chrono_marquer(c, EtapeDemarrage::IMAGE, 8650);
    chrono_marquer(c, EtapeDemarrage::RETRO, 0);
    chrono_texte(c, PartieChrono::SUITE, buf, sizeof(buf));
    expect(std::strcmp(buf, "ordo=-; retro=0; dessin=-; image=8650; tard=-; wifi=-; api=-; fin600=-; chargeur=-") == 0,
           "chronologie : ordre des étapes, pas celui des marques ; 0 ms est une valeur");
    expect(!chrono_marquer(c, EtapeDemarrage::NOMBRE, 1) && !chrono_vue(c, EtapeDemarrage::NOMBRE),
           "chronologie : étape hors liste ignorée");
    // Au pire : premier texte à 6 chiffres par étape (setup() fini avant 1 000 s), second
    // à 10 (uint32_t). Chacun sous 255 caractères (état texte de Home Assistant).
    ChronoDemarrage pleine;
    for (size_t i = 0; i < kEtapesDemarrage; i++)
        chrono_marquer(pleine, static_cast<EtapeDemarrage>(i), i < kEtapesSuite ? 999999u : 0xFFFFFFFFu);
    size_t n = chrono_texte(pleine, PartieChrono::SETUP, buf, sizeof(buf));
    expect(n == std::strlen(buf) && n <= 255 && std::strstr(buf, "; zones=999999") != nullptr,
           "chronologie : premier texte au pire, entier et sous 255 caractères");
    n = chrono_texte(pleine, PartieChrono::SUITE, buf, sizeof(buf));
    expect(n == std::strlen(buf) && n <= 255 && std::strstr(buf, "; chargeur=4294967295") != nullptr,
           "chronologie : second texte au pire, entier et sous 255 caractères");
    ChronoDemarrage enorme;
    for (size_t i = 0; i < kEtapesDemarrage; i++)
        chrono_marquer(enorme, static_cast<EtapeDemarrage>(i), 0xFFFFFFFFu);
    n = chrono_texte(enorme, PartieChrono::SETUP, buf, sizeof(buf));
    expect(n == sizeof(buf) - 1 && std::strlen(buf) == n, "chronologie : au-delà, texte coupé, jamais débordé");
    char petit[12];
    const size_t m = chrono_texte(enorme, PartieChrono::SUITE, petit, sizeof(petit));
    expect(m == sizeof(petit) - 1 && std::strlen(petit) == m && std::strncmp(petit, "ordo=429496", 11) == 0,
           "chronologie : tampon trop petit, texte coupé et terminé");
    expect(chrono_texte(enorme, PartieChrono::SETUP, nullptr, 8) == 0 &&
               chrono_texte(enorme, PartieChrono::SETUP, petit, 0) == 0,
           "chronologie : pas de tampon, rien d'écrit");
    expect(demarrage_noter(EtapeDemarrage::WIFI, 10550) && !demarrage_noter(EtapeDemarrage::WIFI, 1) &&
               chrono_demarrage().ms[static_cast<size_t>(EtapeDemarrage::WIFI)] == 10550,
           "chronologie de la tablette : un seul état");
    expect(chrono_ordo(9300, 7191) == 2109 && chrono_ordo(5, 5) == 0 && chrono_ordo(4, 5) == 0,
           "chronologie : ordo = horloge - millis(), jamais négatif");
}

// ── Niveau de la batterie d'après sa tension (tab5_batterie.h, 15/10/2026) ──
// Courbe de décharge par élément mesurée sur un Tab5 (deux nuits à 1,2 W, discussion #278),
// batterie 2S : la droite 6,0 → 8,23 V de la 3.9.0-rc.1 donnait 63 % à 7,4 V, la table
// Samsung de la rc.2 19 %, la mesure 7 %.
static bool proche(float a, float b) { return std::fabs(a - b) < 0.05f; }

static void test_niveau_batterie() {
    // Bornes : 2 × 3,300 V = 0 %, 2 × 4,175 V = 100 %, borné au-delà.
    expect(kBatterieVideV == 6.6f && kBatteriePleineV == 8.35f, "niveau : bornes de la table pour 2 éléments");
    expect(batterie_niveau_pct(6.6f) == 0.0f && batterie_niveau_pct(6.0f) == 0.0f &&
               batterie_niveau_pct(0.0f) == 0.0f && batterie_niveau_pct(-3.0f) == 0.0f,
           "niveau : 0 % à 6,6 V et dessous");
    expect(batterie_niveau_pct(8.35f) == 100.0f && batterie_niveau_pct(8.4f) == 100.0f &&
               batterie_niveau_pct(12.0f) == 100.0f,
           "niveau : 100 % à 8,35 V et au-dessus");
    expect(std::isnan(batterie_niveau_pct(NAN)) && std::isnan(batterie_niveau_pct(INFINITY)) &&
               std::isnan(batterie_niveau_pct(-INFINITY)),
           "niveau : tension non finie, NAN");
    // Points de la table (tension d'un élément × 2) : valeur exacte, aux arrondis près.
    expect(proche(batterie_niveau_pct(8.0f), 62.0f), "niveau : 4,000 V par élément = 62 %");
    expect(proche(batterie_niveau_pct(7.7f), 30.0f), "niveau : 3,850 V par élément = 30 %");
    expect(proche(batterie_niveau_pct(7.6f), 18.0f), "niveau : 3,800 V par élément = 18 %");
    expect(proche(batterie_niveau_pct(7.4f), 7.0f), "niveau : 7,4 V = 7 % (la droite disait 63 %, la rc.2 19 %)");
    expect(proche(batterie_niveau_pct(8.2f), 97.0f), "niveau : 8,2 V = 97 %");
    expect(proche(batterie_niveau_pct(7.0f), 2.0f), "niveau : 7,0 V = 2 %");
    // Entre deux points : interpolation linéaire.
    expect(proche(batterie_niveau_pct(8.03f), 73.4f), "niveau : 8,03 V ≈ 73 % (sur le plateau)");
    expect(proche(batterie_niveau_pct(7.81f), 47.8f), "niveau : 7,81 V ≈ 48 %");
    expect(proche(batterie_niveau_pct(7.3f), 4.5f), "niveau : 7,3 V ≈ 4,5 %");
    // Table croissante (tension et capacité), puis courbe monotone et continue, mV par mV.
    bool table_croissante = true;
    for (int i = 1; i < kCourbePoints; i++)
        table_croissante = table_croissante && kCourbeDecharge[i].mv > kCourbeDecharge[i - 1].mv &&
                           kCourbeDecharge[i].pct > kCourbeDecharge[i - 1].pct;
    expect(table_croissante && kCourbeDecharge[0].pct == 0 && kCourbeDecharge[kCourbePoints - 1].pct == 100,
           "niveau : table strictement croissante, de 0 à 100 %");
    bool monotone = true;
    bool sans_saut = true;
    float avant = batterie_niveau_pct(6.0f);
    for (int mv = 6001; mv <= 8600; mv++) {
        const float p = batterie_niveau_pct(mv / 1000.0f);
        if (!(p >= avant) || p < 0.0f || p > 100.0f) monotone = false;
        // Le plus raide : 19 % pour 25 mV par élément (62 → 81 %), soit 0,38 % par mV de la
        // batterie entière ; au-delà de 1 %, ce serait un saut.
        if (p - avant > 1.0f) sans_saut = false;
        avant = p;
    }
    expect(monotone, "niveau : croissant avec la tension, dans 0..100");
    expect(sans_saut, "niveau : pas de saut d'un millivolt à l'autre");
}

// ── Barre de chargement du démarrage (tab5_barre.h, 16/10/2026) ─────────────
static void test_barre_demarrage() {
    expect(barre::rgb565(0xFFFFFF) == 0xFFFF && barre::rgb565(0x000000) == 0, "barre : blanc et noir en RGB565");
    expect(barre::rgb565(0xFF0000) == 0xF800 && barre::rgb565(0x00FF00) == 0x07E0 && barre::rgb565(0x0000FF) == 0x001F,
           "barre : rouge, vert, bleu en RGB565");
    const barre::Rect g = barre::geometrie(1280, 720);
    expect(g.x == 384 && g.y == 354 && g.l == 512 && g.h == 12, "barre : 512 × 12 centrée sur le Tab5");
    const barre::Rect g7 = barre::geometrie(1024, 600);
    expect(g7.l == 409 && g7.h == 10 && g7.x == 307 && g7.y == 295, "barre : 409 × 10 centrée en 1024 × 600");

    // Rotation : chaque point de l'interface arrive où LVGL l'enverrait (draw_buffer_ d'ESPHome).
    const int rotations[] = {0, 90, 180, 270};
    for (int rot : rotations) {
        const bool couche = rot == 90 || rot == 270;
        const int dl = couche ? 720 : 1280, dh = couche ? 1280 : 720;  // la dalle
        const barre::Rect ui{100, 50, 7, 3};
        const barre::Rect d = barre::rect_dalle(ui, rot, dl, dh);
        uint16_t src[21], dst[21];
        for (int i = 0; i < 21; i++) src[i] = static_cast<uint16_t>(i + 1);
        barre::tourner(src, dst, ui.l, ui.h, rot);
        bool juste = d.l * d.h == 21 && d.x >= 0 && d.y >= 0 && d.x + d.l <= dl && d.y + d.h <= dh;
        for (int r = 0; r < ui.h && juste; r++) {
            for (int c = 0; c < ui.l; c++) {
                const int u = ui.x + c, v = ui.y + r;
                int xn = u, yn = v;  // 0°
                if (rot == 270) {
                    xn = v;
                    yn = dh - 1 - u;
                } else if (rot == 180) {
                    xn = dl - 1 - u;
                    yn = dh - 1 - v;
                } else if (rot == 90) {
                    xn = dl - 1 - v;
                    yn = u;
                }
                const int i = (yn - d.y) * d.l + (xn - d.x);
                if (xn < d.x || yn < d.y || xn >= d.x + d.l || yn >= d.y + d.h || dst[i] != src[r * ui.l + c]) juste = false;
            }
        }
        char msg[64];
        std::snprintf(msg, sizeof msg, "barre : rotation %d, chaque pixel à sa place", rot);
        expect(juste, msg);
    }

    // Dessin : piste, partie pleine, bords arrondis (coins au fond).
    constexpr int L = 64, H = 12;
    uint16_t buf[L * H];
    const uint32_t fond = 0x101010, piste = 0x404040, plein = 0x20A0FF;
    barre::peindre(buf, L, H, barre::remplis(0.5f, L), fond, piste, plein);
    expect(barre::remplis(0.5f, L) == 32 && barre::remplis(0.0f, L) == 0 && barre::remplis(1.5f, L) == L,
           "barre : pixels remplis bornés");
    expect(buf[0] == barre::rgb565(fond) && buf[L - 1] == barre::rgb565(fond), "barre : coins arrondis au fond");
    expect(buf[(H / 2) * L + 16] == barre::rgb565(plein), "barre : la moitié gauche pleine");
    expect(buf[(H / 2) * L + 48] == barre::rgb565(piste), "barre : la moitié droite en piste");
    barre::peindre(buf, L, H, 0, fond, piste, plein);
    expect(buf[(H / 2) * L + 16] == barre::rgb565(piste), "barre : vide = piste seule");

    // Progression : part de la fraction montrée, avance, ne dépasse pas la cible.
    expect(barre::progression(0.2f, 0.5f, 0, 6000) == 0.2f, "barre : au départ, la fraction montrée");
    float avant = 0.2f;
    bool monte = true;
    for (uint32_t t = 100; t <= 60000; t += 100) {
        const float f = barre::progression(0.2f, 0.5f, t, 6000);
        if (f < avant || f > 0.5f) monte = false;
        avant = f;
    }
    expect(monte, "barre : croissante, jamais au-delà de la cible");
    expect(barre::progression(0.2f, 0.5f, 18000, 6000) < 0.5f, "barre : toujours sous la cible à trois fois la durée");
    expect(barre::progression(0.0f, 1.0f, 6000, 6000) > 0.85f, "barre : 86 % du chemin à la durée typique");
    expect(barre::progression(0.6f, 0.5f, 1000, 6000) == 0.6f, "barre : ne recule jamais");
    expect(barre::kEtapes == 3 && barre::kJalons[barre::kEtapes - 1].cible == 1.0f, "barre : la dernière étape va au bout");
}

// ── Sens de l'écran (tab5_geometrie.h, ADR-0069) ──────────────────────────
static void test_sens_ecran() {
    expect(rotation_repos() == kRotationRepos, "sens : sans choix, la rotation de repos de la carte");
    const int repos[] = {0, 90, 180, 270};
    bool juste = true;
    for (int r : repos) {
        juste = juste && rotation_sens(r, false) == r && rotation_sens(r, true) == (r + 180) % 360 &&
                rotation_sens(rotation_sens(r, true), true) == r;
    }
    expect(juste, "sens : retourné = un demi-tour de plus, deux fois = le sens d'origine");
    rotation_repos_courante() = rotation_sens(kRotationRepos, true);
    expect(rotation_repos() == (kRotationRepos + 180) % 360, "sens : le choix posé est celui que tout le firmware lit");
    rotation_repos_courante() = kRotationRepos;
    expect(!rotation_tenue(), "sens : aucun jeu ne tient la rotation au démarrage");
}

// ── Découpe autour d'une carte opaque (tab5_decoupe.h, popup Musique) ──────
static bool dans_zone(const decoupe::Zone& z, int32_t x, int32_t y) {
    return x >= z.x1 && x <= z.x2 && y >= z.y1 && y <= z.y2;
}

// Chaque pixel de `a` est dans exactement une des zones rendues, et aucun pixel hors de `a`.
static bool partition_exacte(const decoupe::Zone& a, const decoupe::Zone& dans, const decoupe::Zone* m, int n,
                             int32_t l, int32_t h) {
    for (int32_t y = -1; y <= h; y++) {
        for (int32_t x = -1; x <= l; x++) {
            int fois = dans_zone(dans, x, y) ? 1 : 0;
            for (int i = 0; i < n; i++) fois += dans_zone(m[i], x, y) ? 1 : 0;
            if (fois != (dans_zone(a, x, y) ? 1 : 0)) return false;
        }
    }
    return true;
}

static void test_decoupe_carte() {
    using decoupe::Zone;
    // Carte du popup Musique (940 × 536) centrée sur la 7B (1024 × 600), rayon 24, arrondi 2.
    const Zone carte = {42, 32, 981, 567};
    const Zone c = decoupe::couverte(carte, 24, 2);
    expect(c == Zone{44, 58, 979, 541}, "decoupe : part couverte de la carte Musique sur la 7B");
    Zone dans;
    Zone m[4];
    expect(decoupe::couper(Zone{0, 0, 1023, 599}, c, dans, m) == 4 && dans == c,
           "decoupe : l'écran entier donne la part couverte et quatre morceaux");
    expect(m[0] == Zone{0, 0, 1023, 57} && m[1] == Zone{0, 542, 1023, 599} && m[2] == Zone{0, 58, 43, 541} &&
               m[3] == Zone{980, 58, 1023, 541},
           "decoupe : dessus, dessous, gauche, droite");
    expect(decoupe::couper(Zone{100, 100, 200, 200}, c, dans, m) == -1, "decoupe : zone déjà dans la carte, rien");
    expect(decoupe::couper(Zone{0, 0, 30, 20}, c, dans, m) == -1, "decoupe : zone hors de la carte, rien");
    expect(decoupe::couper(Zone{0, 0, 1, 599}, c, dans, m) == -1, "decoupe : bande d'arrondi de LVGL (x 0..1), rien");
    expect(decoupe::couper(Zone{5, 5, 4, 9}, c, dans, m) == -1, "decoupe : zone vide, rien");
    expect(decoupe::couverte(Zone{10, 10, 40, 40}, 0x7FFF, 1).vide(), "decoupe : cercle (rayon maximal), rien de sûr");
    expect(decoupe::couverte(Zone{-5, 10, 40, 40}, 0, 1).vide(), "decoupe : carte hors de l'écran, rien");
    expect(decoupe::couverte(Zone{10, 10, 12, 60}, 0, 4).vide(), "decoupe : carte plus étroite que l'arrondi, rien");

    // Part couverte : dans la carte, hors des coins, alignée, et la plus grande ainsi.
    bool ok = true;
    for (int32_t n = 1; n <= 4; n++) {
        for (int32_t r = 0; r <= 45; r += 3) {
            for (int32_t x1 = 0; x1 < 6; x1++) {
                for (int32_t y1 = 0; y1 < 6; y1++) {
                    const Zone k = {x1, y1, x1 + 200 + 3 * r, y1 + 120 + r};
                    const Zone z = decoupe::couverte(k, r, n);
                    if (z.vide()) {
                        ok = false;
                        continue;
                    }
                    ok = ok && z.x1 >= k.x1 + 1 && z.x2 <= k.x2 - 1 && z.y1 >= k.y1 + r + 1 && z.y2 <= k.y2 - r - 1;
                    ok = ok && z.x1 % n == 0 && z.y1 % n == 0 && (z.x2 + 1) % n == 0 && (z.y2 + 1) % n == 0;
                    ok = ok && z.x1 - (k.x1 + 1) < n && z.y1 - (k.y1 + r + 1) < n && (k.x2 - 1) - z.x2 < n &&
                         (k.y2 - r - 1) - z.y2 < n;
                }
            }
        }
    }
    expect(ok, "decoupe : part couverte dans la carte, hors des coins, alignée, au plus près");

    // Partition exacte, pixel par pixel, sur un petit écran (64 × 40) et des zones pseudo-aléatoires.
    const Zone p = {10, 8, 49, 29};
    uint32_t s = 12345;
    auto alea = [&s](int32_t borne) {
        s = s * 1103515245u + 12345u;
        return static_cast<int32_t>((s >> 16) % static_cast<uint32_t>(borne));
    };
    bool part = true;
    int coupes = 0;
    for (int i = 0; i < 2000; i++) {
        const int32_t xa = alea(64), xb = alea(64), ya = alea(40), yb = alea(40);
        const Zone a = {std::min(xa, xb), std::min(ya, yb), std::max(xa, xb), std::max(ya, yb)};
        const int n = decoupe::couper(a, p, dans, m);
        if (n < 0) {
            const Zone inter = {std::max(a.x1, p.x1), std::max(a.y1, p.y1), std::min(a.x2, p.x2), std::min(a.y2, p.y2)};
            part = part && (inter.vide() || inter == a);
            continue;
        }
        coupes++;
        part = part && n >= 1 && n <= 4 && !dans.vide() && partition_exacte(a, dans, m, n, 64, 40);
        for (int j = 0; j < n; j++) part = part && !m[j].vide();
    }
    expect(part && coupes > 500, "decoupe : morceaux disjoints, union exacte, aucun vide (2 000 zones)");
}

// ── Disposition des éléments d'une pièce (tab5_disposition.h, ADR-0066) ──────
static void test_disposition() {
    using namespace disposition;
    // La table de l'ADR-0066, n = 6 … 20 : entières, moitiés, quarts.
    static const int kEntieres[] = {4, 3, 2, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0};
    static const int kMoities[] = {1, 2, 3, 4, 5, 3, 4, 2, 3, 1, 2, 0, 1, 0, 0};
    static const int kQuarts[] = {0, 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5};
    bool table = true;
    for (int n = 6; n <= 20; n++) {
        const Compte c = compter(n);
        table = table && c.entieres == kEntieres[n - 6] && c.moities == kMoities[n - 6] && c.quarts == kQuarts[n - 6];
    }
    expect(table, "disposition : la table de l'ADR-0066, n = 6 … 20");
    bool petits = true;
    for (int n = 0; n <= 5; n++) {
        const Compte c = compter(n);
        petits = petits && c.entieres == n && c.moities == 0 && c.quarts == 0;
    }
    expect(petits, "disposition : jusqu'à cinq éléments, des tuiles entières");
    expect(compter(-3).entieres == 0 && compter(21).quarts == 5 && compter(99).entieres == 0, "disposition : n borné");

    // Une liste sans trou de n éléments : chacun posé une fois, dans l'ordre de la liste, places
    // de gauche à droite entières → moitiés → quarts, cases remplies dans l'ordre, une seule case
    // vide (la dernière, à 19).
    bool ordre = true, une_fois = true, cases_ok = true, utilisees_ok = true, vides_ok = true;
    for (int n = 0; n <= 20; n++) {
        const uint32_t presents = n == 0 ? 0u : ((1u << n) - 1u);
        const Disposition d = disposer(presents, false);
        utilisees_ok = utilisees_ok && d.n == n && d.utilisees == (n < kPlaces ? n : kPlaces) && !d.positions_fixes;
        int suivant = 0, vides = 0, rang_coupe = 0;
        for (int p = 0; p < kPlaces; p++) {
            const Place& pl = d.place[p];
            if (p >= d.utilisees) {
                cases_ok = cases_ok && pl.coupe == Coupe::VIDE;
                continue;
            }
            const int r = pl.coupe == Coupe::ENTIERE ? 0 : (pl.coupe == Coupe::MOITIES ? 1 : 2);
            ordre = ordre && pl.coupe != Coupe::VIDE && r >= rang_coupe;
            rang_coupe = r;
            for (int k = 0; k < kCasesMax; k++) {
                const int t = pl.element[k];
                if (k >= cases_de(pl.coupe)) {
                    cases_ok = cases_ok && t == -1;
                    continue;
                }
                if (t < 0) {
                    vides++;
                    continue;
                }
                ordre = ordre && t == suivant++;
                une_fois = une_fois && d.place_de[t] == p && d.case_de[t] == k;
            }
        }
        une_fois = une_fois && suivant == n;
        vides_ok = vides_ok && vides == (n == 19 ? 1 : 0);
        if (n == 19) vides_ok = vides_ok && d.place[4].element[3] == -1 && d.place[4].element[2] == 18;
        for (int t = n; t < kElementsMax; t++) une_fois = une_fois && d.place_de[t] == -1 && d.case_de[t] == -1;
    }
    expect(utilisees_ok, "disposition : n éléments, min(n, 5) places occupées");
    expect(ordre, "disposition : entières, puis moitiés, puis quarts ; éléments dans l'ordre de la liste");
    expect(une_fois, "disposition : chaque élément posé une fois, place et case relues");
    expect(cases_ok, "disposition : aucune case au-delà de la coupe ni place au-delà des occupées");
    expect(vides_ok, "disposition : aucune case vide, sauf le dernier quart à 19");

    // Exemples de l'ADR : à 6, la cinquième place en deux ; à 11, une entière, trois moitiés, un quart.
    const Disposition six = disposer(0x3Fu, false);
    expect(six.place[3].coupe == Coupe::ENTIERE && six.place[4].coupe == Coupe::MOITIES &&
               six.place[4].element[0] == 4 && six.place[4].element[1] == 5,
           "disposition : à 6, la dernière place coupée haut (4) / bas (5)");
    const Disposition onze = disposer(0x7FFu, false);
    expect(onze.place[0].coupe == Coupe::ENTIERE && onze.place[3].coupe == Coupe::MOITIES &&
               onze.place[4].coupe == Coupe::QUARTS && onze.place[4].element[0] == 7 && onze.place[4].element[3] == 10,
           "disposition : à 11, quart haut gauche 7 … bas droite 10");

    // Pièce à trous : les présents se suivent ; en positions fixes (mode météo, ≤ 5 éléments
    // d'index < 5), l'élément T reste sur la place T.
    const uint32_t espaces = (1u << 0) | (1u << 2) | (1u << 4);
    const Disposition fixe = disposer(espaces, true);
    expect(fixe.positions_fixes && fixe.place[0].element[0] == 0 && fixe.place[1].coupe == Coupe::VIDE &&
               fixe.place[2].element[0] == 2 && fixe.place[3].coupe == Coupe::VIDE && fixe.place[4].element[0] == 4,
           "disposition : positions fixes, trous gardés");
    const Disposition suivis = disposer(espaces, false);
    expect(!suivis.positions_fixes && suivis.utilisees == 3 && suivis.place[1].element[0] == 2 &&
               suivis.place[2].element[0] == 4 && suivis.place_de[4] == 2,
           "disposition : sans positions fixes, les présents se suivent");
    const uint32_t loin = (1u << 1) | (1u << 7);
    const Disposition loin_d = disposer(loin, true);
    expect(!loin_d.positions_fixes && loin_d.place[0].element[0] == 1 && loin_d.place[1].element[0] == 7,
           "disposition : un index ≥ 5 (« t07 ») défait les positions fixes, rien n'est perdu");
    const Disposition six_fixes = disposer(0x3Fu, true);
    expect(!six_fixes.positions_fixes && six_fixes.place[4].coupe == Coupe::MOITIES,
           "disposition : au-delà de cinq, la coupe même en mode météo");
    expect(disposer(0xFFF00000u, false).n == 0, "disposition : bits au-delà du vingtième ignorés");

    // Rectangles des cases (Tab5 : place de 230 × 275, 4 px d'écart).
    const Rect e = case_rect(Coupe::ENTIERE, 0, 230, 275, 4);
    expect(e.x == 0 && e.y == 0 && e.w == 230 && e.h == 275, "case : une entière couvre la place");
    const Rect haut = case_rect(Coupe::MOITIES, 0, 230, 275, 4), bas = case_rect(Coupe::MOITIES, 1, 230, 275, 4);
    expect(haut.y == 0 && haut.w == 230 && haut.h == 135 && bas.y == 140 && bas.h == 135 && bas.y + bas.h == 275,
           "case : moitiés de 230 × 135, haut et bas collés aux bords");
    const Rect q0 = case_rect(Coupe::QUARTS, 0, 230, 275, 4), q3 = case_rect(Coupe::QUARTS, 3, 230, 275, 4);
    const Rect q1 = case_rect(Coupe::QUARTS, 1, 230, 275, 4), q2 = case_rect(Coupe::QUARTS, 2, 230, 275, 4);
    expect(q0.x == 0 && q0.y == 0 && q0.w == 113 && q0.h == 135 && q1.x == 117 && q1.y == 0 && q2.x == 0 &&
               q2.y == 140 && q3.x == 117 && q3.y == 140 && q3.x + q3.w == 230,
           "case : quarts de 113 × 135 en deux colonnes");
    // Toute place, toute coupe : cases dans la place, disjointes, d'écart au moins `ecart`.
    bool dedans = true;
    for (int l = 150; l <= 260; l += 7)
        for (int h = 180; h <= 300; h += 11)
            for (Coupe c : {Coupe::MOITIES, Coupe::QUARTS})
                for (int a = 0; a < cases_de(c); a++) {
                    const Rect ra = case_rect(c, a, l, h, 4);
                    dedans = dedans && ra.x >= 0 && ra.y >= 0 && ra.x + ra.w <= l && ra.y + ra.h <= h && ra.w > 0 && ra.h > 0;
                    for (int b = a + 1; b < cases_de(c); b++) {
                        const Rect rb = case_rect(c, b, l, h, 4);
                        const bool a_gauche = ra.x + ra.w + 4 <= rb.x || rb.x + rb.w + 4 <= ra.x;
                        const bool a_dessus = ra.y + ra.h + 4 <= rb.y || rb.y + rb.h + 4 <= ra.y;
                        dedans = dedans && (a_gauche || a_dessus);
                    }
                }
    expect(dedans, "case : dans la place, disjointes, 4 px d'écart au moins (places de 150 à 260 px)");
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
    test_majuscules();
    test_hhmm();
    test_modele_ha();
    test_pieces_pages();
    test_clim_ici();
    test_chronologie();
    test_niveau_batterie();
    test_barre_demarrage();
    test_sens_ecran();
    test_decoupe_carte();
    test_disposition();

    std::printf("=== %s (%d OK, %d FAIL) ===\n", g_fail ? "FAILED" : "ALL PASSED", g_ok, g_fail);
    return g_fail ? 1 : 0;
}
