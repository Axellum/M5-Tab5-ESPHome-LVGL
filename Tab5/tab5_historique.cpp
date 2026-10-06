/**
 * [AI-CONTEXT]
 * @file tab5_historique.cpp
 * @role Popup « Température » (ADR-0032, 06/10/2026) : l'historique d'une des deux
 *       températures de l'accueil en courbe, et, pour la seconde (serre ou dehors), la
 *       prévision de la météo à sa suite.
 *         - Appui long sur la température de la pièce (clé salon) ou sur la seconde
 *           (clé serre ; l'appui court garde l'arcade) : script tab5_historique_ouvrir.
 *         - Trois vues : 24 h (créneaux d'une heure), 7 jours (trois heures), 30 jours
 *           (un jour). Chaque créneau porte la moyenne (la courbe) et le minimum et le
 *           maximum (une barre pâle derrière elle) ; la valeur actuelle finit la courbe.
 *         - La prévision (seconde température seulement) : des points datés, en or, sur
 *           un fond teinté après « Maintenant ». Une prévision par jour porte aussi son
 *           minimum et son maximum (barre or). Case « dehors » du blueprint cochée : la
 *           prévision prolonge la courbe ; décochée (une serre) : elle reste à part,
 *           sous le nom « Dehors, prévu ».
 *       Home Assistant n'envoie rien tant que le popup est fermé : l'ouverture et chaque
 *       bouton de vue émettent esphome.tab5_historique (cle, vue), auquel le blueprint
 *       répond en lançant script.tab5_historique (packages/tab5_historique.yaml), qui lit
 *       les statistiques du recorder et la prévision et pousse tab5_maj_historique.
 * @architecture_constraint Rien en NVS : tout repart de HA à l'ouverture. Les trois vues
 *       de la clé montrée sont gardées en PSRAM (Memoire, allouée à la première ouverture,
 *       ~6 Ko) ; ouvrir l'autre clé les oublie. Les widgets du tracé sont créés une fois,
 *       dans historique_zone (historique_popup.yaml). Écritures comparées d'abord (ui_text,
 *       ui_hidden, ui_x, ui_y : LVGL 9.5 invalide même à valeur égale).
 * @ai_warning lv_line_set_points() ne COPIE PAS le tableau de points : il doit vivre aussi
 *       longtemps que la ligne (Memoire::courbe / ::prev, jamais une variable locale). Et
 *       LVGL 9.5 ne trace les pointillés que des lignes horizontales ou verticales
 *       (lv_draw_sw_line.c) : la prévision se distingue par sa couleur et son fond teinté,
 *       pas par des tirets.
 * @ai_instruction Le format de tab5_maj_historique est un contrat avec
 *       packages/tab5_historique.yaml, la démo (tools/demo/) et le rendu (tools/rendu/) :
 *       tests/test_historique.py les lit tous. Un texte affiché passe par tr().
 */
#include "tab5_internal.h"
#include "esp_heap_caps.h"
#include "lvgl.h"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>

HistoriqueUI g_historique_ui;

namespace {

enum Vue : int { JOUR = 0, SEMAINE = 1, MOIS = 2, NB_VUES = 3 };
constexpr const char* kVues[NB_VUES] = {"jour", "semaine", "mois"};
enum Cle : int { SALON = 0, SERRE = 1, NB_CLES = 2 };
constexpr const char* kCles[NB_CLES] = {"salon", "serre"};
enum Carte : int { MAINTENANT = 0, MINIMUM = 1, MAXIMUM = 2, PREVU = 3, NB_CARTES = 4 };

// Créneaux d'une vue (24 + l'heure en cours, 56 + les trois heures en cours, 30 + le
// jour en cours) et points de prévision (72 h d'heures au plus, ou 7 jours).
constexpr int kMesuresMax = 64;
constexpr int kPrevMax = 48;
constexpr int kBandesPrevMax = 10;
constexpr int kGrilleMax = 6;               // 5 intervalles au plus
constexpr int kAxeMax = 12;

// Géométrie (historique_popup.yaml) : corps du popup x 24..1226, y 72..670 ; cartes de
// 150 px en haut (y 72), carte du graphique de 432 px en dessous (y 238).
constexpr int32_t kCorpsX = 24;
constexpr int32_t kCorpsW = 1202;
constexpr int32_t kEcart = 16;
constexpr int32_t kMargeTexte = 44;         // 22 px de chaque côté
// Carte du graphique : titre à gauche jusqu'aux boutons de vue (3 × 150 + 2 × 10, à 18 px
// du bord droit).
constexpr int32_t kTitreW = 1202 - 22 - 488 - 24;
// Zone du tracé (historique_zone, 1166 × 344) : graduations à gauche, tracé, axe des
// temps, légende.
constexpr int32_t kZoneW = 1166;
constexpr int32_t kGradW = 52;              // libellés des degrés, alignés à droite
constexpr int32_t kTraceX0 = 64;
constexpr int32_t kTraceX1 = 1146;
constexpr int32_t kTraceY0 = 30;
constexpr int32_t kTraceY1 = 266;
constexpr int32_t kAxeY = 276;
constexpr int32_t kAxeW = 120;              // largeur d'un libellé de l'axe, texte centré
constexpr int32_t kLegendeY = 312;
constexpr int32_t kPoint = 14;              // pastille de la valeur actuelle

struct Point {
    float moy = NAN, mn = NAN, mx = NAN;
};

struct Prev {
    int32_t minute = 0;                     // depuis le début du premier créneau
    float moy = NAN, mn = NAN, mx = NAN;
};

struct Serie {
    bool recue = false;
    bool exterieur = false;
    char nom[48] = {};
    int64_t debut_jour = 0;                 // jours depuis le 1970-01-01 (date locale)
    int32_t debut_min = 0;                  // minute du jour du premier créneau
    int32_t pas = 60;                       // minutes par créneau
    int32_t maintenant = 0;                 // minutes depuis le début
    float actuel = NAN;
    int n = 0;
    Point m[kMesuresMax];
    int np = 0;
    Prev p[kPrevMax];
};

// En PSRAM : les trois vues de la clé montrée et les points des deux lignes (que LVGL
// lit à chaque dessin, cf. [AI-WARNING]).
struct Memoire {
    Serie series[NB_VUES];
    lv_point_precise_t courbe[kMesuresMax + 1];
    lv_point_precise_t prev[kPrevMax + 1];
};

Memoire* s_mem = nullptr;
int s_cle = -1;
int s_vue = JOUR;

lv_obj_t* s_fond_prev = nullptr;            // teinte de la partie prévue
lv_obj_t* s_grille[kGrilleMax] = {};
lv_obj_t* s_degres[kGrilleMax] = {};
lv_obj_t* s_bandes_prev[kBandesPrevMax] = {};
lv_obj_t* s_bandes[kMesuresMax] = {};
lv_obj_t* s_trait_maintenant = nullptr;
lv_obj_t* s_prevision = nullptr;            // lv_line
lv_obj_t* s_courbe = nullptr;               // lv_line
lv_obj_t* s_point = nullptr;
lv_obj_t* s_maintenant = nullptr;           // « Maintenant » au-dessus du trait
lv_obj_t* s_axe[kAxeMax] = {};
lv_obj_t* s_legende = nullptr;
lv_obj_t* s_leg_trait = nullptr;
lv_obj_t* s_leg_bande = nullptr;
lv_obj_t* s_leg_prev_trait = nullptr;
lv_obj_t* s_leg_prev = nullptr;             // élément « Prévu » de la légende
lv_obj_t* s_leg_txt[3] = {};
lv_obj_t* s_vide = nullptr;

// --- Lecture des payloads ---------------------------------------------------------------

// Champ suivant de [p, fin) jusqu'à `sep` : [*d, *d + *n). Avance p après le séparateur.
void champ_suivant(const char*& p, const char* fin, char sep, const char*& d, size_t& n) {
    d = p;
    while (p < fin && *p != sep) p++;
    n = static_cast<size_t>(p - d);
    if (p < fin) p++;
}

// Nombre d'un champ : NAN s'il ne se lit pas (« nan », « unknown », vide).
float lire_nombre(const char* d, size_t n) {
    char buf[24];
    if (n == 0 || n >= sizeof(buf)) return NAN;
    std::memcpy(buf, d, n);
    buf[n] = '\0';
    char* fin = nullptr;
    const float v = std::strtof(buf, &fin);
    if (fin == buf || !std::isfinite(v)) return NAN;
    return v;
}

int index_de(const std::string& nom, const char* const* noms, int nb) {
    for (int k = 0; k < nb; k++)
        if (nom == noms[k]) return k;
    return -1;
}

// --- Dates (axe des temps) : calendrier civil, sans fuseau (heure locale de HA) ---------

int64_t jours_depuis_civil(int a, int m, int j) {
    a -= m <= 2 ? 1 : 0;
    const int64_t ere = (a >= 0 ? a : a - 399) / 400;
    const int64_t ae = a - ere * 400;
    const int64_t jda = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + j - 1;
    const int64_t jde = ae * 365 + ae / 4 - ae / 100 + jda;
    return ere * 146097 + jde - 719468;
}

struct Moment {
    int annee = 2000, mois = 1, jour = 1, heure = 0, minute = 0, wday = 0;
};

Moment moment(const Serie& s, int32_t minutes) {
    const int64_t total = s.debut_jour * 1440 + s.debut_min + minutes;
    int64_t z = total / 1440;
    int64_t r = total % 1440;
    if (r < 0) {
        r += 1440;
        z--;
    }
    Moment t;
    t.heure = static_cast<int>(r / 60);
    t.minute = static_cast<int>(r % 60);
    t.wday = static_cast<int>(((z % 7) + 11) % 7);   // le 1970-01-01 est un jeudi (4)
    z += 719468;
    const int64_t ere = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t jde = z - ere * 146097;
    const int64_t ae = (jde - jde / 1460 + jde / 36524 - jde / 146096) / 365;
    const int64_t jda = jde - (365 * ae + ae / 4 - ae / 100);
    const int64_t mp = (5 * jda + 2) / 153;
    t.jour = static_cast<int>(jda - (153 * mp + 2) / 5 + 1);
    t.mois = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    t.annee = static_cast<int>(ae + ere * 400 + (t.mois <= 2 ? 1 : 0));
    return t;
}

// Un instant comme l'axe l'écrit : « 14:00 » (heures), « Lun 6 » (vue 7 jours) ou
// « 6 Oct » (vue 30 jours). avec_heure : la vue 7 jours ajoute l'heure (« Lun 6 · 03:00 »).
void libelle_moment(const Serie& s, int32_t minutes, bool jour_seul, bool avec_heure, char* out, size_t n) {
    const Moment t = moment(s, minutes);
    if (!jour_seul && s_vue == JOUR) {
        snprintf(out, n, "%02d:00", t.heure);
    } else if (s_vue == SEMAINE) {
        if (avec_heure) snprintf(out, n, "%s %d \xC2\xB7 %02d:00", day_short_utf8(t.wday), t.jour, t.heure);
        else snprintf(out, n, "%s %d", day_short_utf8(t.wday), t.jour);
    } else {
        snprintf(out, n, "%d %s", t.jour, month_short_utf8(t.mois));
    }
}

// --- Dessin ------------------------------------------------------------------------------

bool visible() {
    const HistoriqueUI& u = g_historique_ui;
    return u.popup != nullptr && !lv_obj_has_flag(u.popup, LV_OBJ_FLAG_HIDDEN);
}

void degres(char* out, size_t n, float t) {
    if (std::isnan(t)) snprintf(out, n, "-- \xC2\xB0");
    else snprintf(out, n, "%.1f \xC2\xB0", t);
}

void peindre_carte(int c, const char* nom, float valeur, const char* detail, int32_t largeur) {
    const HistoriqueUI& u = g_historique_ui;
    char v[24];
    degres(v, sizeof(v), valeur);
    ui_text(u.nom[c], nom);
    ui_text(u.valeur[c], v);
    ui_text_color(u.valeur[c], std::isnan(valeur) ? UIColor.TEXT_DIM : get_temperature_color(valeur));
    texte_ha_coupe(u.detail[c], detail, largeur - kMargeTexte);
}

// Minimum, maximum et moyenne des créneaux : k_min / k_max = leur créneau (-1 sans).
struct Bilan {
    float mn = NAN, mx = NAN, moy = NAN;
    int k_min = -1, k_max = -1;
};

Bilan bilan(const Serie& s) {
    Bilan b;
    float somme = 0.0f;
    int n = 0;
    for (int k = 0; k < s.n; k++) {
        const Point& p = s.m[k];
        const float lo = std::isnan(p.mn) ? p.moy : p.mn;
        const float hi = std::isnan(p.mx) ? p.moy : p.mx;
        if (!std::isnan(lo) && (b.k_min < 0 || lo < b.mn)) {
            b.mn = lo;
            b.k_min = k;
        }
        if (!std::isnan(hi) && (b.k_max < 0 || hi > b.mx)) {
            b.mx = hi;
            b.k_max = k;
        }
        if (!std::isnan(p.moy)) {
            somme += p.moy;
            n++;
        }
    }
    if (n > 0) b.moy = somme / static_cast<float>(n);
    return b;
}

void peindre_cartes(const Serie& s) {
    HistoriqueUI& u = g_historique_ui;
    const bool prevu = s_cle == SERRE;
    const int n = prevu ? NB_CARTES : NB_CARTES - 1;
    const int32_t largeur = (kCorpsW - (n - 1) * kEcart) / n;
    for (int c = 0; c < NB_CARTES; c++) {
        const bool montre = c < n;
        ui_hidden(u.carte[c], !montre);
        if (!montre || u.carte[c] == nullptr) continue;
        if (lv_obj_get_style_width(u.carte[c], LV_PART_MAIN) != largeur) lv_obj_set_width(u.carte[c], largeur);
        ui_x(u.carte[c], kCorpsX + c * (largeur + kEcart));
    }
    const Bilan b = s.recue ? bilan(s) : Bilan();
    char d[64], x[24];
    d[0] = '\0';
    if (!std::isnan(b.moy)) {
        degres(x, sizeof(x), b.moy);
        snprintf(d, sizeof(d), "%s %s", tr("Moyenne"), x);
    }
    peindre_carte(MAINTENANT, tr("MAINTENANT"), s.recue ? s.actuel : NAN, d, largeur);
    d[0] = '\0';
    if (b.k_min >= 0) libelle_moment(s, b.k_min * s.pas, false, true, d, sizeof(d));
    peindre_carte(MINIMUM, tr("MINIMUM"), b.mn, d, largeur);
    d[0] = '\0';
    if (b.k_max >= 0) libelle_moment(s, b.k_max * s.pas, false, true, d, sizeof(d));
    peindre_carte(MAXIMUM, tr("MAXIMUM"), b.mx, d, largeur);
    if (!prevu) return;
    // Prévision : son maximum, et son minimum en dessous.
    float lo = NAN, hi = NAN;
    for (int k = 0; k < s.np; k++) {
        const Prev& p = s.p[k];
        const float a = std::isnan(p.mn) ? p.moy : p.mn;
        const float z = std::isnan(p.mx) ? p.moy : p.mx;
        if (!std::isnan(a) && (std::isnan(lo) || a < lo)) lo = a;
        if (!std::isnan(z) && (std::isnan(hi) || z > hi)) hi = z;
    }
    d[0] = '\0';
    if (!std::isnan(lo)) {
        degres(x, sizeof(x), lo);
        snprintf(d, sizeof(d), "%s %s", tr("Minimum"), x);
    }
    peindre_carte(PREVU, s.exterieur ? tr("PRÉVU") : tr("DEHORS, PRÉVU"), hi, d, largeur);
}

// Graduations : un pas de 1, 2, 5, 10, 20 ou 50 degrés, 5 intervalles au plus.
struct Echelle {
    float bas = 0.0f, haut = 1.0f, pas = 1.0f;
};

Echelle echelle(float lo, float hi) {
    if (hi - lo < 2.0f) {
        const float c = (lo + hi) / 2.0f;
        lo = c - 1.0f;
        hi = c + 1.0f;
    }
    static constexpr float kPas[] = {1.0f, 2.0f, 5.0f, 10.0f, 20.0f, 50.0f};
    Echelle e;
    for (float p : kPas) {
        e.pas = p;
        e.bas = std::floor(lo / p) * p;
        e.haut = std::ceil(hi / p) * p;
        if (e.haut <= e.bas) e.haut = e.bas + p;
        if ((e.haut - e.bas) / p <= kGrilleMax - 1 + 0.01f) break;
    }
    return e;
}

void peindre_libelle_centre(lv_obj_t* l, const char* txt, int32_t x) {
    ui_text(l, txt);
    int32_t g = x - kAxeW / 2;
    if (g < 0) g = 0;
    if (g > kZoneW - kAxeW) g = kZoneW - kAxeW;
    ui_x(l, g);
    ui_hidden(l, false);
}

void cacher_trace() {
    ui_hidden(s_fond_prev, true);
    for (lv_obj_t* o : s_grille) ui_hidden(o, true);
    for (lv_obj_t* o : s_degres) ui_hidden(o, true);
    for (lv_obj_t* o : s_bandes_prev) ui_hidden(o, true);
    for (lv_obj_t* o : s_bandes) ui_hidden(o, true);
    ui_hidden(s_trait_maintenant, true);
    ui_hidden(s_prevision, true);
    ui_hidden(s_courbe, true);
    ui_hidden(s_point, true);
    ui_hidden(s_maintenant, true);
    for (lv_obj_t* o : s_axe) ui_hidden(o, true);
    ui_hidden(s_legende, true);
}

void poser(lv_obj_t* o, int32_t x, int32_t y, int32_t w, int32_t h) {
    if (o == nullptr) return;
    if (lv_obj_get_style_width(o, LV_PART_MAIN) != w) lv_obj_set_width(o, w);
    if (lv_obj_get_style_height(o, LV_PART_MAIN) != h) lv_obj_set_height(o, h);
    ui_x(o, x);
    ui_y(o, y);
    ui_hidden(o, false);
}

void peindre_graphique() {
    HistoriqueUI& u = g_historique_ui;
    for (int v = 0; v < NB_VUES; v++) highlight_button_border(u.vue_btn[v], v == s_vue, UIColor.ACCENT);
    if (s_courbe == nullptr || s_mem == nullptr) return;
    const Serie& s = s_mem->series[s_vue];

    // Titre : « nom · période ». Sans nom poussé : celui de l'emplacement.
    static const char* const kPeriodes[NB_VUES] = {tr_noop("24 dernières heures"), tr_noop("7 derniers jours"),
                                                   tr_noop("30 derniers jours")};
    const char* nom = s.nom;
    if (!s.recue || nom[0] == '\0') nom = s_cle == SERRE ? (s.exterieur ? tr("Extérieur") : tr("Serre")) : tr("Intérieur");
    char titre[96];
    snprintf(titre, sizeof(titre), "%s \xC2\xB7 %s", nom, tr(kPeriodes[s_vue]));
    texte_ha_coupe(u.titre, titre, kTitreW);

    // Étendue des valeurs : créneaux, valeur actuelle, prévision.
    float lo = NAN, hi = NAN;
    auto etendre = [&lo, &hi](float v) {
        if (std::isnan(v)) return;
        if (std::isnan(lo) || v < lo) lo = v;
        if (std::isnan(hi) || v > hi) hi = v;
    };
    for (int k = 0; k < s.n; k++) {
        etendre(s.m[k].moy);
        etendre(s.m[k].mn);
        etendre(s.m[k].mx);
    }
    for (int k = 0; k < s.np; k++) {
        etendre(s.p[k].moy);
        etendre(s.p[k].mn);
        etendre(s.p[k].mx);
    }
    // Sans créneau ni prévision (capteur sans statistiques) : « Aucun historique », même
    // si la valeur actuelle est connue (la carte « Maintenant » la montre).
    const bool vide = !s.recue || std::isnan(lo);
    etendre(s.actuel);
    ui_hidden(s_vide, !vide);
    if (vide) {
        ui_text(s_vide, s.recue ? tr("Aucun historique") : tr("En attente de Home Assistant"));
        cacher_trace();
        return;
    }

    // Axe des temps : du début du premier créneau à la fin du dernier, ou au dernier
    // point de prévision (une prévision par jour : jusqu'à la fin de son jour).
    const bool par_jour = s.pas >= 1440;
    int32_t fin = s.n * s.pas;
    if (s.maintenant > fin) fin = s.maintenant;
    for (int k = 0; k < s.np; k++) {
        const int32_t f = s.p[k].minute + (par_jour ? s.pas / 2 : 0);
        if (f > fin) fin = f;
    }
    if (fin <= 0) fin = 1;
    const int32_t largeur_trace = kTraceX1 - kTraceX0;
    auto x_de = [fin, largeur_trace](double minutes) {
        return kTraceX0 + static_cast<int32_t>(std::lround(minutes / fin * largeur_trace));
    };
    const Echelle e = echelle(lo, hi);
    auto y_de = [&e](float t) {
        return kTraceY1 - static_cast<int32_t>(std::lround((t - e.bas) / (e.haut - e.bas) * (kTraceY1 - kTraceY0)));
    };

    // Graduations : une ligne et son libellé par pas.
    char buf[32];
    int g = 0;
    for (float t = e.bas; t <= e.haut + 0.01f && g < kGrilleMax; t += e.pas, g++) {
        const int32_t y = y_de(t);
        poser(s_grille[g], kTraceX0, y, largeur_trace, 1);
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", t);
        ui_text(s_degres[g], buf);
        ui_y(s_degres[g], y - 13);
        ui_hidden(s_degres[g], false);
    }
    for (; g < kGrilleMax; g++) {
        ui_hidden(s_grille[g], true);
        ui_hidden(s_degres[g], true);
    }

    // Partie prévue : teinte et trait de « Maintenant ».
    const int32_t x_maintenant = x_de(s.maintenant);
    const bool prevue = s.np > 0;
    if (prevue) {
        poser(s_fond_prev, x_maintenant, kTraceY0, kTraceX1 - x_maintenant, kTraceY1 - kTraceY0);
        poser(s_trait_maintenant, x_maintenant - 1, kTraceY0 - 4, 2, kTraceY1 - kTraceY0 + 4);
        peindre_libelle_centre(s_maintenant, tr("Maintenant"), x_maintenant);
    } else {
        ui_hidden(s_fond_prev, true);
        ui_hidden(s_trait_maintenant, true);
        ui_hidden(s_maintenant, true);
    }

    // Barres du minimum au maximum de chaque créneau, 70 % de sa largeur.
    const double px_creneau = static_cast<double>(s.pas) / fin * largeur_trace;
    int32_t larg_barre = static_cast<int32_t>(px_creneau * 0.7);
    if (larg_barre < 3) larg_barre = 3;
    for (int k = 0; k < kMesuresMax; k++) {
        const bool montre = k < s.n && !std::isnan(s.m[k].mn) && !std::isnan(s.m[k].mx);
        if (!montre) {
            ui_hidden(s_bandes[k], true);
            continue;
        }
        const int32_t xc = x_de((k + 0.5) * s.pas);
        const int32_t y1 = y_de(s.m[k].mx);
        int32_t h = y_de(s.m[k].mn) - y1;
        if (h < 3) h = 3;
        poser(s_bandes[k], xc - larg_barre / 2, y1, larg_barre, h);
    }

    // Courbe des moyennes (au milieu de chaque créneau), finie par la valeur actuelle.
    lv_point_precise_t* pts = s_mem->courbe;
    int np = 0;
    int32_t dernier = -1;
    for (int k = 0; k < s.n && np < kMesuresMax; k++) {
        if (std::isnan(s.m[k].moy)) continue;
        pts[np].x = static_cast<lv_value_precise_t>(x_de((k + 0.5) * s.pas));
        pts[np].y = static_cast<lv_value_precise_t>(y_de(s.m[k].moy));
        dernier = static_cast<int32_t>((k + 0.5) * s.pas);
        np++;
    }
    const bool actuel = !std::isnan(s.actuel);
    if (actuel && s.maintenant >= dernier) {
        pts[np].x = static_cast<lv_value_precise_t>(x_maintenant);
        pts[np].y = static_cast<lv_value_precise_t>(y_de(s.actuel));
        np++;
    }
    ui_hidden(s_courbe, np < 2);
    if (np >= 2) lv_line_set_points(s_courbe, pts, static_cast<uint32_t>(np));
    if (actuel) poser(s_point, x_maintenant - kPoint / 2, y_de(s.actuel) - kPoint / 2, kPoint, kPoint);
    else ui_hidden(s_point, true);

    // Prévision : ses points dans l'ordre ; dehors, elle part de la valeur actuelle.
    lv_point_precise_t* pp = s_mem->prev;
    int npp = 0;
    if (prevue && s.exterieur && actuel) {
        pp[npp].x = static_cast<lv_value_precise_t>(x_maintenant);
        pp[npp].y = static_cast<lv_value_precise_t>(y_de(s.actuel));
        npp++;
    }
    int b = 0;
    for (int k = 0; k < s.np && npp <= kPrevMax; k++) {
        const Prev& p = s.p[k];
        if (std::isnan(p.moy)) continue;
        const int32_t x = x_de(p.minute);
        pp[npp].x = static_cast<lv_value_precise_t>(x);
        pp[npp].y = static_cast<lv_value_precise_t>(y_de(p.moy));
        npp++;
        if (b < kBandesPrevMax && !std::isnan(p.mn) && !std::isnan(p.mx)) {
            const int32_t y1 = y_de(p.mx);
            int32_t h = y_de(p.mn) - y1;
            if (h < 3) h = 3;
            poser(s_bandes_prev[b++], x - larg_barre / 2, y1, larg_barre, h);
        }
    }
    for (; b < kBandesPrevMax; b++) ui_hidden(s_bandes_prev[b], true);
    ui_hidden(s_prevision, npp < 2);
    if (npp >= 2) lv_line_set_points(s_prevision, pp, static_cast<uint32_t>(npp));

    // Axe des temps : un libellé toutes les 3 h, 6 h, 12 h, un jour, deux… (10 au plus),
    // calé sur l'heure ronde ou sur minuit.
    static constexpr int32_t kIntervalles[] = {180, 360, 720, 1440, 2880, 4320, 7200, 10080};
    int32_t iv = kIntervalles[0];
    for (int32_t c : kIntervalles) {
        iv = c;
        if (fin / c <= 10) break;
    }
    const bool jour_seul = iv >= 1440;
    const int32_t cale = jour_seul ? 1440 : iv;
    int32_t m = (cale - (s.debut_min % cale)) % cale;
    int a = 0;
    for (; m <= fin && a < kAxeMax; m += iv) {
        libelle_moment(s, m, jour_seul, false, buf, sizeof(buf));
        peindre_libelle_centre(s_axe[a++], buf, x_de(m));
    }
    for (; a < kAxeMax; a++) ui_hidden(s_axe[a], true);

    // Légende : « Prévu » (dehors) ou « Dehors, prévu » (une serre), si une prévision.
    ui_hidden(s_legende, false);
    ui_hidden(s_leg_prev, !prevue);
    if (prevue) ui_text(s_leg_txt[2], s.exterieur ? tr("Prévu") : tr("Dehors, prévu"));
}

void peindre() {
    if (!visible() || s_mem == nullptr) return;
    const Serie& s = s_mem->series[s_vue];
    peindre_cartes(s);
    peindre_graphique();
}

// Couleurs de ce que construire() a créé : à la construction et à chaque thème.
void couleurs() {
    lv_obj_set_style_bg_color(s_fond_prev, lv_color_hex(UIColor.GOLD), LV_PART_MAIN);
    for (lv_obj_t* o : s_grille) lv_obj_set_style_bg_color(o, lv_color_hex(UIColor.GLASS_RIM), LV_PART_MAIN);
    for (lv_obj_t* o : s_degres) ui_text_color(o, UIColor.TEXT_DIM);
    for (lv_obj_t* o : s_bandes_prev) lv_obj_set_style_bg_color(o, lv_color_hex(UIColor.GOLD), LV_PART_MAIN);
    for (lv_obj_t* o : s_bandes) lv_obj_set_style_bg_color(o, lv_color_hex(UIColor.ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_trait_maintenant, lv_color_hex(UIColor.TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_line_color(s_prevision, lv_color_hex(UIColor.GOLD), LV_PART_MAIN);
    lv_obj_set_style_line_color(s_courbe, lv_color_hex(UIColor.ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_point, lv_color_hex(UIColor.ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_color(s_point, lv_color_hex(UIColor.TEXT_PRIMARY), LV_PART_MAIN);
    ui_text_color(s_maintenant, UIColor.TEXT_DIM);
    for (lv_obj_t* o : s_axe) ui_text_color(o, UIColor.TEXT_DIM);
    lv_obj_set_style_bg_color(s_leg_trait, lv_color_hex(UIColor.ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_leg_bande, lv_color_hex(UIColor.ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_leg_prev_trait, lv_color_hex(UIColor.GOLD), LV_PART_MAIN);
    for (lv_obj_t* o : s_leg_txt) ui_text_color(o, UIColor.TEXT_DIM);
    ui_text_color(s_vide, UIColor.TEXT_DIM);
}

// Un rectangle sans style, non cliquable, masqué : grille, barres, teinte, traits.
lv_obj_t* rectangle(lv_obj_t* parent, lv_opa_t opa, int32_t rayon) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_set_style_bg_opa(o, opa, LV_PART_MAIN);
    lv_obj_set_style_radius(o, rayon, LV_PART_MAIN);
    lv_obj_set_size(o, 2, 2);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    return o;
}

lv_obj_t* libelle(lv_obj_t* parent) {
    lv_obj_t* l = lv_label_create(parent);
    if (g_historique_ui.police != nullptr)
        esphome::lvgl::lv_obj_set_style_text_font(l, g_historique_ui.police, LV_PART_MAIN);
    lv_label_set_text(l, "");
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

lv_obj_t* ligne(lv_obj_t* parent, int32_t epaisseur) {
    lv_obj_t* o = lv_line_create(parent);
    lv_obj_set_pos(o, 0, 0);
    lv_obj_set_style_line_width(o, epaisseur, LV_PART_MAIN);
    lv_obj_set_style_line_rounded(o, true, LV_PART_MAIN);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    return o;
}

// Élément de légende : un échantillon puis son texte, en rangée.
lv_obj_t* element_legende(lv_obj_t* parent, lv_obj_t*& echantillon, int32_t w, int32_t h, lv_opa_t opa,
                          lv_obj_t*& texte, const char* txt) {
    lv_obj_t* e = lv_obj_create(parent);
    lv_obj_remove_style_all(e);
    lv_obj_set_size(e, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(e, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(e, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(e, 10, LV_PART_MAIN);
    lv_obj_remove_flag(e, LV_OBJ_FLAG_CLICKABLE);
    echantillon = rectangle(e, opa, 2);
    lv_obj_set_size(echantillon, w, h);
    lv_obj_remove_flag(echantillon, LV_OBJ_FLAG_HIDDEN);
    texte = libelle(e);
    lv_label_set_text(texte, txt);
    lv_obj_remove_flag(texte, LV_OBJ_FLAG_HIDDEN);
    return e;
}

// Noms des cartes et widgets du tracé : une fois, à la première ouverture. L'ordre de
// création est l'ordre de dessin : teinte, grille, barres, traits, courbes, libellés.
void construire() {
    HistoriqueUI& u = g_historique_ui;
    if (s_courbe != nullptr || u.zone == nullptr) return;
    s_fond_prev = rectangle(u.zone, LV_OPA_10, 0);
    for (int k = 0; k < kGrilleMax; k++) {
        s_grille[k] = rectangle(u.zone, LV_OPA_40, 0);
        s_degres[k] = libelle(u.zone);
        lv_obj_set_width(s_degres[k], kGradW);
        lv_obj_set_style_text_align(s_degres[k], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
    }
    for (int k = 0; k < kBandesPrevMax; k++) s_bandes_prev[k] = rectangle(u.zone, LV_OPA_30, 3);
    for (int k = 0; k < kMesuresMax; k++) s_bandes[k] = rectangle(u.zone, LV_OPA_30, 3);
    s_trait_maintenant = rectangle(u.zone, LV_OPA_60, 0);
    s_prevision = ligne(u.zone, 3);
    s_courbe = ligne(u.zone, 4);
    s_point = rectangle(u.zone, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(s_point, 2, LV_PART_MAIN);
    lv_obj_set_style_border_opa(s_point, LV_OPA_COVER, LV_PART_MAIN);
    s_maintenant = libelle(u.zone);
    lv_obj_set_y(s_maintenant, 0);
    lv_obj_set_width(s_maintenant, kAxeW);
    lv_obj_set_style_text_align(s_maintenant, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    for (int k = 0; k < kAxeMax; k++) {
        lv_obj_t* l = libelle(u.zone);
        lv_obj_set_y(l, kAxeY);
        lv_obj_set_width(l, kAxeW);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        s_axe[k] = l;
    }
    s_legende = lv_obj_create(u.zone);
    lv_obj_remove_style_all(s_legende);
    lv_obj_set_size(s_legende, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_align(s_legende, LV_ALIGN_TOP_MID, 0, kLegendeY);
    lv_obj_set_flex_flow(s_legende, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(s_legende, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(s_legende, 36, LV_PART_MAIN);
    lv_obj_remove_flag(s_legende, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_legende, LV_OBJ_FLAG_HIDDEN);
    element_legende(s_legende, s_leg_trait, 32, 4, LV_OPA_COVER, s_leg_txt[0], tr("Mesuré"));
    element_legende(s_legende, s_leg_bande, 14, 22, LV_OPA_30, s_leg_txt[1], tr("Minimum et maximum"));
    s_leg_prev = element_legende(s_legende, s_leg_prev_trait, 32, 4, LV_OPA_COVER, s_leg_txt[2], tr("Prévu"));
    s_vide = libelle(u.zone);
    lv_obj_align(s_vide, LV_ALIGN_CENTER, 0, 0);
    couleurs();
}

void demander() {
    if (g_historique_ui.demander != nullptr && s_cle >= 0) g_historique_ui.demander(kCles[s_cle], kVues[s_vue]);
}

bool memoire() {
    if (s_mem != nullptr) return true;
    void* p = heap_caps_calloc(1, sizeof(Memoire), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p == nullptr) p = heap_caps_calloc(1, sizeof(Memoire), MALLOC_CAP_8BIT);
    if (p == nullptr) return false;
    s_mem = new (p) Memoire();
    return true;
}

}  // namespace

void historique_recu(const std::string& cle, const std::string& vue, const std::string& entete,
                     const std::string& mesures, const std::string& previsions) {
    const int c = index_de(cle, kCles, NB_CLES);
    const int v = index_de(vue, kVues, NB_VUES);
    // Réponse pour l'autre température (le popup a changé de clé entre-temps) : ignorée.
    if (c < 0 || v < 0 || c != s_cle || s_mem == nullptr) return;
    Serie& s = *new (&s_mem->series[v]) Serie();
    s.recue = true;

    // En-tête « nom|debut|pas|maintenant|actuel|exterieur ».
    const char* p = entete.data();
    const char* fin = p + entete.size();
    const char* d = nullptr;
    size_t n = 0;
    champ_suivant(p, fin, '|', d, n);
    texte_ha_copier(s.nom, sizeof(s.nom), d, n);
    champ_suivant(p, fin, '|', d, n);
    char date[24] = {};
    std::memcpy(date, d, n < sizeof(date) - 1 ? n : sizeof(date) - 1);
    int an = 2000, mo = 1, jo = 1, he = 0, mi = 0;
    if (std::sscanf(date, "%d-%d-%d%*c%d:%d", &an, &mo, &jo, &he, &mi) != 5 || mo < 1 || mo > 12 || jo < 1 ||
        jo > 31 || he < 0 || he > 23 || mi < 0 || mi > 59) {
        an = 2000;   // illisible : seuls les libellés de l'axe s'en servent
        mo = jo = 1;
        he = mi = 0;
    }
    s.debut_jour = jours_depuis_civil(an, mo, jo);
    s.debut_min = he * 60 + mi;
    champ_suivant(p, fin, '|', d, n);
    const float pas = lire_nombre(d, n);
    s.pas = std::isnan(pas) || pas < 1.0f ? 60 : static_cast<int32_t>(pas);
    champ_suivant(p, fin, '|', d, n);
    const float maintenant = lire_nombre(d, n);
    s.maintenant = std::isnan(maintenant) || maintenant < 0.0f ? 0 : static_cast<int32_t>(maintenant);
    champ_suivant(p, fin, '|', d, n);
    s.actuel = lire_nombre(d, n);
    champ_suivant(p, fin, '|', d, n);
    s.exterieur = n == 1 && *d == '1';

    // Créneaux « moy,min,max » séparés par « ; » (vide = pas de donnée).
    p = mesures.data();
    fin = p + mesures.size();
    while (p < fin && s.n < kMesuresMax) {
        champ_suivant(p, fin, ';', d, n);
        const char* q = d;
        const char* qf = d + n;
        const char* e = nullptr;
        size_t ne = 0;
        Point& pt = s.m[s.n++];
        champ_suivant(q, qf, ',', e, ne);
        pt.moy = lire_nombre(e, ne);
        champ_suivant(q, qf, ',', e, ne);
        pt.mn = lire_nombre(e, ne);
        champ_suivant(q, qf, ',', e, ne);
        pt.mx = lire_nombre(e, ne);
    }
    // Points de prévision « minute,moy[,min,max] », dans l'ordre du temps.
    p = previsions.data();
    fin = p + previsions.size();
    while (p < fin && s.np < kPrevMax) {
        champ_suivant(p, fin, ';', d, n);
        const char* q = d;
        const char* qf = d + n;
        const char* e = nullptr;
        size_t ne = 0;
        champ_suivant(q, qf, ',', e, ne);
        const float minute = lire_nombre(e, ne);
        if (std::isnan(minute)) continue;
        Prev& pv = s.p[s.np];
        pv.minute = static_cast<int32_t>(minute);
        champ_suivant(q, qf, ',', e, ne);
        pv.moy = lire_nombre(e, ne);
        champ_suivant(q, qf, ',', e, ne);
        pv.mn = lire_nombre(e, ne);
        champ_suivant(q, qf, ',', e, ne);
        pv.mx = lire_nombre(e, ne);
        if (s.np > 0 && pv.minute <= s.p[s.np - 1].minute) continue;   // hors de l'ordre : ignoré
        s.np++;
    }
    if (v == s_vue) peindre();
}

void historique_ouvrir(const std::string& cle) {
    HistoriqueUI& u = g_historique_ui;
    const int c = index_de(cle, kCles, NB_CLES);
    if (u.popup == nullptr || c < 0 || !memoire()) return;
    construire();
    // L'autre température : ce qui était gardé n'est plus le sien.
    if (c != s_cle)
        for (Serie& s : s_mem->series) new (&s) Serie();
    s_cle = c;
    s_vue = JOUR;
    animate_popup_open(u.popup);
    ui_mark_activity();
    peindre();
    demander();
}

void historique_choisir_vue(int vue) {
    if (vue < 0 || vue >= NB_VUES) return;
    if (vue != s_vue) {
        s_vue = vue;
        peindre();
    }
    demander();
}

// Thèmes (ADR-0029) : ce que construire() a peint une fois, puis le popup s'il est
// ouvert ; fermé, sa prochaine ouverture repeint cartes et courbes.
void historique_rejouer_theme() {
    if (s_courbe == nullptr) return;
    couleurs();
    peindre();
}
