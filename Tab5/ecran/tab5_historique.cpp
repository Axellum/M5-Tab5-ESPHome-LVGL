/**
 * [AI-CONTEXT]
 * @file tab5_historique.cpp
 * @role Popup « Température » (ADR-0032, 06/10/2026) : l'historique d'une température en
 *       courbe, son humidité quand la pièce en a une (ADR-0047, 09/10/2026), et, pour la
 *       seconde température (serre ou dehors), la prévision de la météo à sa suite.
 *         - Appui long sur la température de la pièce (clé salon) ou sur la seconde
 *           (clé serre ; l'appui court garde l'arcade) : script tab5_historique_ouvrir.
 *           En mode HA, sur une pièce dont le blueprint a déclaré la température
 *           (ADR-0040), l'appui long de gauche (ou de droite, sur son humidité) montre la
 *           sienne : clé pR (R = 0 à 4).
 *         - Une page par température connue (ADR-0047) : salon, les pièces à température
 *           déclarée, serre. Leurs noms sont des onglets à côté du titre (celui affiché
 *           en couleur d'accent), un glissement à gauche ou à droite passe à la suivante
 *           ou à la précédente, en boucle. Changement instantané ; la vue (24 h, 7 jours,
 *           30 jours) reste la même.
 *         - Trois vues : 24 h (créneaux d'une heure), 7 jours (trois heures), 30 jours
 *           (un jour). Chaque créneau porte la moyenne (la courbe) et le minimum et le
 *           maximum (une barre pâle derrière elle) ; la valeur actuelle finit la courbe.
 *         - L'humidité (ADR-0047), quand HA en envoie : une seconde courbe sur le même
 *           tracé, son échelle en % à droite (les mêmes lignes de grille que les degrés),
 *           et une quatrième carte (valeur actuelle, plage de la période). Sans humidité :
 *           exactement le rendu d'avant.
 *         - La prévision (seconde température seulement) : des points datés, en or, sur
 *           un fond teinté après « Maintenant ». Une prévision par jour porte aussi son
 *           minimum et son maximum (barre or). Case « dehors » du blueprint cochée : la
 *           prévision prolonge la courbe ; décochée (une serre) : elle reste à part,
 *           sous le nom « Dehors, prévu ».
 *       Home Assistant n'envoie rien tant que le popup est fermé : l'ouverture, chaque
 *       onglet et chaque bouton de vue émettent esphome.tab5_historique (cle, vue), auquel
 *       le blueprint répond en lançant script.tab5_historique
 *       (packages/tab5_historique.yaml), qui lit les statistiques du recorder et la
 *       prévision et pousse tab5_maj_historique.
 * @architecture_constraint Rien en NVS : tout repart de HA à chaque ouverture et à chaque
 *       page. Les trois vues de chaque température déjà montrée sont gardées en PSRAM
 *       (Memoire, allouée à la première ouverture, ~40 Ko) : une page déjà vue se peint
 *       tout de suite, puis la réponse de HA la remplace. Lecture du payload :
 *       historique_lire() (Tab5/socle/tab5_parse.h, testée sur PC et fuzzée). Les widgets
 *       du tracé sont créés une fois, dans historique_zone (historique_popup.yaml).
 *       Écritures comparées d'abord (ui_text, ui_hidden, ui_x, ui_y : LVGL 9.5 invalide
 *       même à valeur égale).
 * @ai_warning lv_line_set_points() ne COPIE PAS le tableau de points : il doit vivre aussi
 *       longtemps que la ligne (Memoire::courbe / ::prev / ::humidite, jamais une variable
 *       locale). Et LVGL 9.5 ne trace les pointillés que des lignes horizontales ou
 *       verticales (lv_draw_sw_line.c) : la prévision se distingue par sa couleur et son
 *       fond teinté, pas par des tirets.
 * @ai_warning [AI-WARNING] Le geste de changement de page s'arrête au popup : le drapeau
 *       LV_OBJ_FLAG_GESTURE_BUBBLE est retiré de historique_popup (construire), comme les
 *       Réglages et le carrousel des clims. Sans ça, LVGL le remonte jusqu'à page_main, qui
 *       changerait les prévisions ou la pièce derrière le popup.
 * @ai_instruction Le format de tab5_maj_historique est un contrat avec
 *       packages/tab5_historique.yaml, la démo (tools/demo/) et le rendu (tools/rendu/) :
 *       tests/test_historique.py les lit tous. Un texte affiché passe par tr(). Les onglets
 *       et le geste sont écrits ici en attendant la brique commune « popup à pages » (lot
 *       feat/popups-par-piece) : à y brancher quand elle sera publiée.
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
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
// salon et serre : les deux températures de l'accueil ; p0 à p4 : la température d'une
// pièce en mode HA (ADR-0040, tab5_piece_climat.cpp), comme le salon (pas de prévision).
enum Cle : int { SALON = 0,
                 SERRE = 1,
                 PIECE_0 = 2,
                 NB_CLES = 7 };
constexpr const char* kCles[NB_CLES] = {"salon", "serre", "p0", "p1", "p2", "p3", "p4"};
// Ordre des onglets : la pièce de l'accueil, les pièces, puis la seconde température.
constexpr int kOrdreOnglets[NB_CLES] = {SALON, PIECE_0, PIECE_0 + 1, PIECE_0 + 2, PIECE_0 + 3, PIECE_0 + 4, SERRE};
enum Carte : int { MAINTENANT = 0,
                   MINIMUM = 1,
                   MAXIMUM = 2,
                   QUATRIEME = 3,
                   NB_CARTES = 4 };

// Créneaux d'une vue et points de prévision : les plafonds de la lecture (tab5_parse.h).
constexpr int kMesuresMax = kHistoriqueMesuresMax;
constexpr int kPrevMax = kHistoriquePrevMax;
constexpr int kBandesPrevMax = 10;
constexpr int kGrilleMax = 6;               // 5 intervalles au plus
constexpr int kAxeMax = 12;

// Géométrie (historique_popup.yaml) : corps du popup x 24..1226, y 72..670 (kCorpsX,
// kCorpsW, kCartesEcart : tab5_geometrie.h) ; cartes de 150 px en haut (y 72), carte du
// graphique de 432 px en dessous (y 238).
constexpr int32_t kMargeTexte = 44;         // 22 px de chaque côté
// Carte du graphique : titre à gauche jusqu'aux boutons de vue (3 × 150 + 2 × 10, à 18 px
// du bord droit).
constexpr int32_t kTitreW = kCorpsW - 22 - 488 - 24;
// Zone du tracé (historique_zone, kGraphiqueL × 344) : graduations à gauche, tracé, axe des
// temps (libellés de kAxeLibelleL px, texte centré), légende.
constexpr int32_t kGradW = 52;              // libellés des degrés, alignés à droite
constexpr int32_t kTraceX0 = 64;
constexpr int32_t kTraceX1 = kGraphiqueL - 20;
constexpr int32_t kTraceY0 = 30;
constexpr int32_t kTraceY1 = 266;
constexpr int32_t kAxeY = 276;
constexpr int32_t kLegendeY = 312;
constexpr int32_t kPoint = 14;              // pastille de la valeur actuelle
// Humidité (ADR-0047) : le tracé s'arrête plus tôt, ses graduations « 60 % » à droite.
constexpr int32_t kHumGradW = 74;
constexpr int32_t kTraceX1Humidite = kGraphiqueL - kHumGradW - 10;
constexpr int32_t kPointHumidite = 12;

// Onglets (historique_onglet.yaml) : sur la ligne du titre, de la fin du titre à la croix
// (80 px à 10 px du bord, modal_header.yaml), 200 px au plus et 10 px d'écart, calés à
// droite : quatre onglets tombent aux places de ceux des Réglages (x 318 à 1148).
constexpr int kOngletsMax = NB_CLES;
constexpr int32_t kOngletL = 200;
constexpr int32_t kOngletEcart = 10;
constexpr int32_t kOngletsFin = kCarteL - 102;
constexpr int32_t kTitrePopupX = 52;        // x du titre (modal_header.yaml)

using Serie = HistoriqueSerie;
using Point = HistoriquePoint;
using Prev = HistoriquePrev;

// En PSRAM : les trois vues de chaque température et les points des trois lignes (que
// LVGL lit à chaque dessin, cf. [AI-WARNING]).
struct Memoire {
    Serie series[NB_CLES][NB_VUES];
    lv_point_precise_t courbe[kMesuresMax + 1];
    lv_point_precise_t prev[kPrevMax + 1];
    lv_point_precise_t humidite[kMesuresMax + 1];
};

Memoire* s_mem = nullptr;
int s_cle = -1;
int s_vue = JOUR;
int s_onglets[kOngletsMax] = {};            // clés des onglets, dans l'ordre affiché
int s_nb_onglets = 0;

lv_obj_t* s_fond_prev = nullptr;            // teinte de la partie prévue
lv_obj_t* s_grille[kGrilleMax] = {};
lv_obj_t* s_degres[kGrilleMax] = {};
lv_obj_t* s_pourcents[kGrilleMax] = {};     // graduations de l'humidité, à droite
lv_obj_t* s_bandes_prev[kBandesPrevMax] = {};
lv_obj_t* s_bandes[kMesuresMax] = {};
lv_obj_t* s_trait_maintenant = nullptr;
lv_obj_t* s_prevision = nullptr;            // lv_line
lv_obj_t* s_hum_courbe = nullptr;           // lv_line, sous celle des températures
lv_obj_t* s_courbe = nullptr;               // lv_line
lv_obj_t* s_hum_point = nullptr;
lv_obj_t* s_point = nullptr;
lv_obj_t* s_maintenant = nullptr;           // « Maintenant » au-dessus du trait
lv_obj_t* s_axe[kAxeMax] = {};
lv_obj_t* s_legende = nullptr;
lv_obj_t* s_leg_trait = nullptr;
lv_obj_t* s_leg_bande = nullptr;
lv_obj_t* s_leg_prev_trait = nullptr;
lv_obj_t* s_leg_prev = nullptr;             // élément « Prévu » de la légende
lv_obj_t* s_leg_hum_trait = nullptr;
lv_obj_t* s_leg_hum = nullptr;              // élément « Humidité » de la légende
lv_obj_t* s_leg_txt[4] = {};
lv_obj_t* s_vide = nullptr;

const Serie& serie() { return s_mem->series[s_cle][s_vue]; }

bool humidite_connue(uint8_t h) { return h != kHumiditeAucune; }

int index_de(const std::string& nom, const char* const* noms, int nb) {
    for (int k = 0; k < nb; k++)
        if (nom == noms[k]) return k;
    return -1;
}

// --- Couleurs ------------------------------------------------------------------------------

// Courbe de l'humidité : HUMIDITY_WET, le bleu de l'air humide ; l'accent secondaire quand
// ce bleu se confond avec l'accent, la couleur des températures (thème Graphite, tout en
// bleu : écart de 19 sur 255 en sombre, 21 en clair).
uint32_t couleur_humidite() {
    const uint32_t a = UIColor.HUMIDITY_WET, b = UIColor.ACCENT;
    const int dr = static_cast<int>((a >> 16) & 0xFF) - static_cast<int>((b >> 16) & 0xFF);
    const int dv = static_cast<int>((a >> 8) & 0xFF) - static_cast<int>((b >> 8) & 0xFF);
    const int db = static_cast<int>(a & 0xFF) - static_cast<int>(b & 0xFF);
    return dr * dr + dv * dv + db * db < 48 * 48 ? UIColor.ACCENT_ALT : a;
}

// --- Dates (axe des temps) : calendrier civil, sans fuseau (heure locale de HA) ---------
// Date → jour : jour_civil() (tab5_core.h) ; jour → date : moment() ci-dessous.

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

void pourcent(char* out, size_t n, uint8_t h) {
    if (!humidite_connue(h)) snprintf(out, n, "-- %%");
    else snprintf(out, n, "%d %%", static_cast<int>(h));
}

void peindre_carte_texte(int c, const char* nom, const char* valeur, uint32_t couleur, const char* detail,
                         int32_t largeur) {
    const HistoriqueUI& u = g_historique_ui;
    ui_text(u.nom[c], nom);
    ui_text(u.valeur[c], valeur);
    ui_text_color(u.valeur[c], couleur);
    texte_ha_coupe(u.detail[c], detail, largeur - kMargeTexte);
}

void peindre_carte(int c, const char* nom, float valeur, const char* detail, int32_t largeur) {
    char v[24];
    degres(v, sizeof(v), valeur);
    peindre_carte_texte(c, nom, v, std::isnan(valeur) ? UIColor.TEXT_DIM : get_temperature_color(valeur), detail,
                        largeur);
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

// Plage de l'humidité sur la période (minimum des minimums, maximum des maximums ; la
// moyenne d'un créneau à défaut), la valeur actuelle comprise. Faux sans aucune mesure.
bool plage_humidite(const Serie& s, uint8_t& mn, uint8_t& mx) {
    mn = kHumiditeAucune;
    mx = kHumiditeAucune;
    auto etendre_h = [&](uint8_t h) {
        if (!humidite_connue(h)) return;
        if (!humidite_connue(mn) || h < mn) mn = h;
        if (!humidite_connue(mx) || h > mx) mx = h;
    };
    for (int k = 0; k < s.n; k++) {
        const Point& p = s.m[k];
        etendre_h(humidite_connue(p.h_mn) ? p.h_mn : p.h_moy);
        etendre_h(humidite_connue(p.h_mx) ? p.h_mx : p.h_moy);
    }
    etendre_h(s.h_actuelle);
    return humidite_connue(mn);
}

void peindre_cartes(const Serie& s) {
    HistoriqueUI& u = g_historique_ui;
    // Quatrième carte : la prévision pour la seconde température, l'humidité d'une pièce
    // qui en a une (ADR-0047), rien sinon.
    const bool prevu = s_cle == SERRE;
    const bool humidite = !prevu && s.recue && s.humidite;
    const int n = prevu || humidite ? NB_CARTES : NB_CARTES - 1;
    const int32_t largeur = (kCorpsW - (n - 1) * kCartesEcart) / n;
    for (int c = 0; c < NB_CARTES; c++) {
        const bool montre = c < n;
        ui_hidden(u.carte[c], !montre);
        if (!montre || u.carte[c] == nullptr) continue;
        if (lv_obj_get_style_width(u.carte[c], LV_PART_MAIN) != largeur) lv_obj_set_width(u.carte[c], largeur);
        ui_x(u.carte[c], kCorpsX + c * (largeur + kCartesEcart));
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
    if (humidite) {
        // L'humidité actuelle, en couleur de son dégradé (comme l'accueil), et sa plage.
        uint8_t mn, mx;
        d[0] = '\0';
        if (plage_humidite(s, mn, mx)) snprintf(d, sizeof(d), tr("Entre %d et %d %%"), mn, mx);
        pourcent(x, sizeof(x), s.h_actuelle);
        peindre_carte_texte(QUATRIEME, tr("HUMIDITÉ"), x,
                            humidite_connue(s.h_actuelle) ? get_humidity_color(s.h_actuelle) : UIColor.TEXT_DIM, d,
                            largeur);
        return;
    }
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
    peindre_carte(QUATRIEME, s.exterieur ? tr("PRÉVU") : tr("DEHORS, PRÉVU"), hi, d, largeur);
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

// Graduations de l'humidité sur les `n` intervalles des degrés (mêmes lignes de grille) :
// le plus petit pas de 1, 2, 5, 10, 20, 25 ou 50 % qui couvre la plage, au moins 4 %
// d'écart (une humidité stable ne colle pas à une ligne), sans dépasser 100 %.
struct EchelleHumidite {
    int bas = 0, pas = 10;
};

EchelleHumidite echelle_humidite(int lo, int hi, int n) {
    if (hi - lo < 4) {
        const int c = (lo + hi) / 2;
        lo = c - 2 < 0 ? 0 : c - 2;
        hi = lo + 4;
    }
    static constexpr int kPas[] = {1, 2, 5, 10, 20, 25, 50};
    EchelleHumidite e;
    for (int p : kPas) {
        e.pas = p;
        e.bas = lo / p * p;
        if (e.bas + n * p >= hi) break;
    }
    if (e.bas + n * e.pas > 100 && n * e.pas <= 100) e.bas = 100 - n * e.pas;  // chaque pas divise 100
    if (e.bas < 0) e.bas = 0;
    return e;
}

void peindre_libelle_centre(lv_obj_t* l, const char* txt, int32_t x) {
    ui_text(l, txt);
    int32_t g = x - kAxeLibelleL / 2;
    if (g < 0) g = 0;
    if (g > kGraphiqueL - kAxeLibelleL) g = kGraphiqueL - kAxeLibelleL;
    ui_x(l, g);
    ui_hidden(l, false);
}

void cacher_trace() {
    ui_hidden(s_fond_prev, true);
    for (lv_obj_t* o : s_grille) ui_hidden(o, true);
    for (lv_obj_t* o : s_degres) ui_hidden(o, true);
    for (lv_obj_t* o : s_pourcents) ui_hidden(o, true);
    for (lv_obj_t* o : s_bandes_prev) ui_hidden(o, true);
    for (lv_obj_t* o : s_bandes) ui_hidden(o, true);
    ui_hidden(s_trait_maintenant, true);
    ui_hidden(s_prevision, true);
    ui_hidden(s_hum_courbe, true);
    ui_hidden(s_courbe, true);
    ui_hidden(s_hum_point, true);
    ui_hidden(s_point, true);
    ui_hidden(s_maintenant, true);
    for (lv_obj_t* o : s_axe) ui_hidden(o, true);
    ui_hidden(s_legende, true);
}

// Briques du tracé : ui_poser(), ui_rectangle(), ui_ligne() (tab5_anim.cpp, partagées avec
// le popup Météo depuis le 09/10/2026).
constexpr auto poser = ui_poser;
constexpr auto rectangle = ui_rectangle;
constexpr auto ligne = ui_ligne;

// --- Graphique : une fonction par étape (lot L10 de l'audit du 07/10, ex-peindre_graphique
// d'un seul bloc de 200 lignes ; mêmes calculs, dans le même ordre) ---------------------

constexpr const char* kPeriodes[NB_VUES] = {tr_noop("24 dernières heures"), tr_noop("7 derniers jours"),
                                           tr_noop("30 derniers jours")};

// Dernier titre posé et ce dont il dépend (DO-10) : texte_ha_coupe() pose le nom seul,
// puis la période s'y ajoute, soit deux écritures du label à chaque repeint. Même nom,
// même période, même police, même largeur, et le label porte encore ce titre : rien
// n'est écrit. Un nom plus long que `nom` ne se mémorise pas (il est réécrit à chaque
// fois, comme avant).
struct TitrePose {
    char nom[64] = {};
    char suffixe[64] = {};
    const lv_font_t* police = nullptr;
    int32_t espace = 0;
    int32_t largeur = -1;
    char titre[128] = {};
};
TitrePose s_titre;

// Nom d'une température quand HA n'en a pas poussé : celui de l'emplacement (« Extérieur »
// ou « Serre », « Intérieur »), ou de la pièce (celui de sa carte centrale en mode HA).
void nom_par_defaut(int c, bool exterieur, char* out, size_t n) {
    if (c >= PIECE_0) {
        if (!tuiles_piece_titre(c - PIECE_0, out, n)) snprintf(out, n, tr("Pièce %d"), c - PIECE_0 + 1);
        return;
    }
    snprintf(out, n, "%s", c == SERRE ? (exterieur ? tr("Extérieur") : tr("Serre")) : tr("Intérieur"));
}

// Titre : « nom · période ». Sans nom poussé : celui de l'emplacement. Un nom long est
// coupé (« … »), jamais la période : le nom seul, à la largeur que la période laisse.
void peindre_titre(const Serie& s) {
    HistoriqueUI& u = g_historique_ui;
    if (u.titre == nullptr) return;
    char defaut[48];
    const char* nom = s.nom;
    if (!s.recue || nom[0] == '\0') {
        nom_par_defaut(s_cle, s.exterieur, defaut, sizeof(defaut));
        nom = defaut;
    }
    char suffixe[64];
    snprintf(suffixe, sizeof(suffixe), " \xC2\xB7 %s", tr(kPeriodes[s_vue]));
    int32_t largeur_nom = kTitreW;
    const lv_font_t* police = lv_obj_get_style_text_font(u.titre, LV_PART_MAIN);
    const int32_t espace = lv_obj_get_style_text_letter_space(u.titre, LV_PART_MAIN);
    if (police != nullptr) {
        lv_point_t taille;
        lv_text_get_size(&taille, suffixe, police, espace, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
        largeur_nom -= taille.x;
    }
    TitrePose& t = s_titre;
    const char* actuel = lv_label_get_text(u.titre);
    if (t.largeur == largeur_nom && t.police == police && t.espace == espace && strcmp(t.nom, nom) == 0 &&
        strcmp(t.suffixe, suffixe) == 0 && actuel != nullptr && strcmp(actuel, t.titre) == 0) {
        return;
    }
    texte_ha_coupe(u.titre, nom, largeur_nom);
    char titre[128];
    snprintf(titre, sizeof(titre), "%s%s", lv_label_get_text(u.titre), suffixe);
    ui_text(u.titre, titre);
    snprintf(t.nom, sizeof(t.nom), "%s", nom);
    snprintf(t.suffixe, sizeof(t.suffixe), "%s", suffixe);
    t.police = police;
    t.espace = espace;
    t.largeur = largeur_nom;
    snprintf(t.titre, sizeof(t.titre), "%s", lv_label_get_text(u.titre));
}

// Étendue des valeurs des créneaux et de la prévision (NAN, NAN s'il n'y en a pas).
void etendre(float v, float& lo, float& hi) {
    if (std::isnan(v)) return;
    if (std::isnan(lo) || v < lo) lo = v;
    if (std::isnan(hi) || v > hi) hi = v;
}

void etendue(const Serie& s, float& lo, float& hi) {
    lo = NAN;
    hi = NAN;
    for (int k = 0; k < s.n; k++) {
        etendre(s.m[k].moy, lo, hi);
        etendre(s.m[k].mn, lo, hi);
        etendre(s.m[k].mx, lo, hi);
    }
    for (int k = 0; k < s.np; k++) {
        etendre(s.p[k].moy, lo, hi);
        etendre(s.p[k].mn, lo, hi);
        etendre(s.p[k].mx, lo, hi);
    }
}

// Repère du tracé : minutes depuis le début du premier créneau → x, degrés → y ; avec
// l'humidité, le tracé s'arrête à kTraceX1Humidite et les % ont leur échelle.
struct Repere {
    int32_t fin = 1;   // minutes couvertes par l'axe des temps
    int32_t x1 = kTraceX1;
    Echelle e;
    bool humidite = false;
    int intervalles = 1;
    EchelleHumidite h;
    int32_t x(double minutes) const {
        return kTraceX0 + static_cast<int32_t>(std::lround(minutes / fin * (x1 - kTraceX0)));
    }
    int32_t y(float t) const {
        return kTraceY1 - static_cast<int32_t>(std::lround((t - e.bas) / (e.haut - e.bas) * (kTraceY1 - kTraceY0)));
    }
    int32_t y_humidite(uint8_t v) const {
        const float f = static_cast<float>(v - h.bas) / static_cast<float>(intervalles * h.pas);
        return kTraceY1 - static_cast<int32_t>(std::lround(f * (kTraceY1 - kTraceY0)));
    }
};

// Axe des temps : du début du premier créneau à la fin du dernier, ou au dernier point
// de prévision (une prévision par jour : jusqu'à la fin de son jour).
Repere repere(const Serie& s, float lo, float hi) {
    const bool par_jour = s.pas >= 1440;
    Repere r;
    r.fin = s.n * s.pas;
    if (s.maintenant > r.fin) r.fin = s.maintenant;
    for (int k = 0; k < s.np; k++) {
        const int32_t f = s.p[k].minute + (par_jour ? s.pas / 2 : 0);
        if (f > r.fin) r.fin = f;
    }
    if (r.fin <= 0) r.fin = 1;
    r.e = echelle(lo, hi);
    r.intervalles = static_cast<int>(std::lround((r.e.haut - r.e.bas) / r.e.pas));
    if (r.intervalles < 1) r.intervalles = 1;
    if (r.intervalles > kGrilleMax - 1) r.intervalles = kGrilleMax - 1;
    return r;
}

// Graduations : une ligne et son libellé par pas ; avec l'humidité, ses % à droite sur les
// mêmes lignes, et chaque échelle dans la couleur de sa courbe.
void peindre_graduations(const Repere& r, bool degres_visibles) {
    char buf[32];
    int g = 0;
    const uint32_t couleur_degres = r.humidite ? UIColor.ACCENT : UIColor.TEXT_DIM;
    const uint32_t couleur_pourcents = couleur_humidite();
    for (float t = r.e.bas; t <= r.e.haut + 0.01f && g < kGrilleMax; t += r.e.pas, g++) {
        const int32_t y = r.y(t);
        poser(s_grille[g], kTraceX0, y, r.x1 - kTraceX0, 1);
        snprintf(buf, sizeof(buf), "%.0f\xC2\xB0", t);
        ui_text(s_degres[g], buf);
        ui_text_color(s_degres[g], couleur_degres);
        ui_y(s_degres[g], y - 13);
        ui_hidden(s_degres[g], !degres_visibles);
        // Au-delà de 100 % (trois intervalles de 50 % pour une humidité de 20 à 99 %) :
        // la ligne reste, sans libellé.
        const bool pourcent_montre = r.humidite && g <= r.intervalles && r.h.bas + g * r.h.pas <= 100;
        if (pourcent_montre) {
            snprintf(buf, sizeof(buf), "%d %%", r.h.bas + g * r.h.pas);
            ui_text(s_pourcents[g], buf);
            ui_text_color(s_pourcents[g], couleur_pourcents);
            ui_x(s_pourcents[g], r.x1 + 10);
            ui_y(s_pourcents[g], y - 13);
        }
        ui_hidden(s_pourcents[g], !pourcent_montre);
    }
    for (; g < kGrilleMax; g++) {
        ui_hidden(s_grille[g], true);
        ui_hidden(s_degres[g], true);
        ui_hidden(s_pourcents[g], true);
    }
}

// Partie prévue : teinte et trait de « Maintenant ».
void peindre_partie_prevue(const Repere& r, bool prevue, int32_t x_maintenant) {
    if (prevue) {
        poser(s_fond_prev, x_maintenant, kTraceY0, r.x1 - x_maintenant, kTraceY1 - kTraceY0);
        poser(s_trait_maintenant, x_maintenant - 1, kTraceY0 - 4, 2, kTraceY1 - kTraceY0 + 4);
        peindre_libelle_centre(s_maintenant, tr("Maintenant"), x_maintenant);
    } else {
        ui_hidden(s_fond_prev, true);
        ui_hidden(s_trait_maintenant, true);
        ui_hidden(s_maintenant, true);
    }
}

// Milieu du créneau k, jamais après « Maintenant » : le créneau en cours n'est pas fini
// (30 jours à 7 h 45 : son milieu, midi, serait déjà dans la partie prévue).
double milieu(const Serie& s, int k) {
    const double m = (k + 0.5) * s.pas;
    return s.maintenant >= k * s.pas && s.maintenant < m ? static_cast<double>(s.maintenant) : m;
}

// Barres du minimum au maximum de chaque créneau, 70 % de sa largeur. Renvoie cette
// largeur (les barres de la prévision la reprennent).
int32_t peindre_barres(const Serie& s, const Repere& r) {
    const double px_creneau = static_cast<double>(s.pas) / r.fin * (r.x1 - kTraceX0);
    int32_t larg_barre = static_cast<int32_t>(px_creneau * 0.7);
    if (larg_barre < 3) larg_barre = 3;
    for (int k = 0; k < kMesuresMax; k++) {
        const bool montre = k < s.n && !std::isnan(s.m[k].mn) && !std::isnan(s.m[k].mx);
        if (!montre) {
            ui_hidden(s_bandes[k], true);
            continue;
        }
        const int32_t xc = r.x(milieu(s, k));
        const int32_t y1 = r.y(s.m[k].mx);
        int32_t h = r.y(s.m[k].mn) - y1;
        if (h < 3) h = 3;
        poser(s_bandes[k], xc - larg_barre / 2, y1, larg_barre, h);
    }
    return larg_barre;
}

// Une courbe : la valeur de chaque créneau qui en a une (au milieu du créneau), finie par
// la valeur actuelle sur le trait de « Maintenant », et sa pastille. `y_de(k)` : y du
// créneau k, ou -1 sans valeur ; `y_actuel` : -1 sans valeur actuelle.
template <typename YDe>
void peindre_une_courbe(const Serie& s, const Repere& r, int32_t x_maintenant, lv_obj_t* ligne, lv_point_precise_t* pts,
                        lv_obj_t* point, int32_t taille_point, YDe y_de, int32_t y_actuel) {
    int np = 0;
    double dernier = -1;
    for (int k = 0; k < s.n && np < kMesuresMax; k++) {
        const int32_t y = y_de(k);
        if (y < 0) continue;
        dernier = milieu(s, k);
        pts[np].x = static_cast<lv_value_precise_t>(r.x(dernier));
        pts[np].y = static_cast<lv_value_precise_t>(y);
        np++;
    }
    const bool actuel = y_actuel >= 0;
    if (actuel && s.maintenant >= dernier) {
        // Créneau en cours posé sur le trait : la valeur actuelle prend sa place.
        if (np > 0 && dernier == s.maintenant) np--;
        pts[np].x = static_cast<lv_value_precise_t>(x_maintenant);
        pts[np].y = static_cast<lv_value_precise_t>(y_actuel);
        np++;
    }
    ui_hidden(ligne, np < 2);
    if (np >= 2) lv_line_set_points(ligne, pts, static_cast<uint32_t>(np));
    if (actuel) poser(point, x_maintenant - taille_point / 2, y_actuel - taille_point / 2, taille_point, taille_point);
    else ui_hidden(point, true);
}

// Courbe des moyennes des températures et sa pastille.
void peindre_courbe(const Serie& s, const Repere& r, int32_t x_maintenant) {
    peindre_une_courbe(
        s, r, x_maintenant, s_courbe, s_mem->courbe, s_point, kPoint,
        [&](int k) { return std::isnan(s.m[k].moy) ? -1 : r.y(s.m[k].moy); },
        std::isnan(s.actuel) ? -1 : r.y(s.actuel));
}

// Courbe des moyennes de l'humidité (ADR-0047), sous celle des températures.
void peindre_humidite(const Serie& s, const Repere& r, int32_t x_maintenant) {
    if (!r.humidite) {
        ui_hidden(s_hum_courbe, true);
        ui_hidden(s_hum_point, true);
        return;
    }
    peindre_une_courbe(
        s, r, x_maintenant, s_hum_courbe, s_mem->humidite, s_hum_point, kPointHumidite,
        [&](int k) { return humidite_connue(s.m[k].h_moy) ? r.y_humidite(s.m[k].h_moy) : -1; },
        humidite_connue(s.h_actuelle) ? r.y_humidite(s.h_actuelle) : -1);
}

// Prévision : ses points dans l'ordre ; dehors, elle part de la valeur actuelle. Une
// prévision par jour porte aussi sa barre du minimum au maximum.
void peindre_prevision(const Serie& s, const Repere& r, bool prevue, int32_t x_maintenant, int32_t larg_barre) {
    lv_point_precise_t* pp = s_mem->prev;
    int npp = 0;
    if (prevue && s.exterieur && !std::isnan(s.actuel)) {
        pp[npp].x = static_cast<lv_value_precise_t>(x_maintenant);
        pp[npp].y = static_cast<lv_value_precise_t>(r.y(s.actuel));
        npp++;
    }
    int b = 0;
    for (int k = 0; k < s.np && npp <= kPrevMax; k++) {
        const Prev& p = s.p[k];
        if (std::isnan(p.moy)) continue;
        const int32_t x = r.x(p.minute);
        pp[npp].x = static_cast<lv_value_precise_t>(x);
        pp[npp].y = static_cast<lv_value_precise_t>(r.y(p.moy));
        npp++;
        if (b < kBandesPrevMax && !std::isnan(p.mn) && !std::isnan(p.mx)) {
            const int32_t y1 = r.y(p.mx);
            int32_t h = r.y(p.mn) - y1;
            if (h < 3) h = 3;
            poser(s_bandes_prev[b++], x - larg_barre / 2, y1, larg_barre, h);
        }
    }
    for (; b < kBandesPrevMax; b++) ui_hidden(s_bandes_prev[b], true);
    ui_hidden(s_prevision, npp < 2);
    if (npp >= 2) lv_line_set_points(s_prevision, pp, static_cast<uint32_t>(npp));
}

// Axe des temps : un libellé toutes les 3 h, 6 h, 12 h, un jour, deux… (10 au plus),
// calé sur l'heure ronde ou sur minuit.
void peindre_axe(const Serie& s, const Repere& r) {
    static constexpr int32_t kIntervalles[] = {180, 360, 720, 1440, 2880, 4320, 7200, 10080};
    int32_t iv = kIntervalles[0];
    for (int32_t c : kIntervalles) {
        iv = c;
        if (r.fin / c <= 10) break;
    }
    const bool jour_seul = iv >= 1440;
    const int32_t cale = jour_seul ? 1440 : iv;
    int32_t m = (cale - (s.debut_min % cale)) % cale;
    char buf[32];
    int a = 0;
    for (; m <= r.fin && a < kAxeMax; m += iv) {
        libelle_moment(s, m, jour_seul, false, buf, sizeof(buf));
        peindre_libelle_centre(s_axe[a++], buf, r.x(m));
    }
    for (; a < kAxeMax; a++) ui_hidden(s_axe[a], true);
}

// Légende : « Mesuré » (« Température » à côté de l'humidité), « Minimum et maximum »,
// « Prévu » (dehors) ou « Dehors, prévu » (une serre) s'il y a une prévision, « Humidité ».
void peindre_legende(const Serie& s, bool prevue, bool humidite) {
    ui_hidden(s_legende, false);
    ui_text(s_leg_txt[0], humidite ? tr("Température") : tr("Mesuré"));
    ui_hidden(s_leg_prev, !prevue);
    if (prevue) ui_text(s_leg_txt[2], s.exterieur ? tr("Prévu") : tr("Dehors, prévu"));
    ui_hidden(s_leg_hum, !humidite);
}

void peindre_graphique() {
    HistoriqueUI& u = g_historique_ui;
    for (int v = 0; v < NB_VUES; v++) highlight_button_border(u.vue_btn[v], v == s_vue, UIColor.ACCENT);
    if (s_courbe == nullptr || s_mem == nullptr || s_cle < 0) return;
    const Serie& s = serie();
    peindre_titre(s);

    // Étendue des valeurs : créneaux, valeur actuelle, prévision. Sans créneau ni
    // prévision (capteur sans statistiques) : « Aucun historique », même si la valeur
    // actuelle est connue (la carte « Maintenant » la montre). L'humidité compte comme
    // une mesure : une pièce dont seule l'humidité a des statistiques montre sa courbe.
    float lo, hi;
    etendue(s, lo, hi);
    uint8_t hmin = kHumiditeAucune, hmax = kHumiditeAucune;
    bool humidite_mesuree = false;
    if (s.recue && s.humidite) {
        for (int k = 0; k < s.n && !humidite_mesuree; k++) humidite_mesuree = humidite_connue(s.m[k].h_moy);
        if (!plage_humidite(s, hmin, hmax)) humidite_mesuree = false;
    }
    const bool vide = !s.recue || (std::isnan(lo) && !humidite_mesuree);
    etendre(s.actuel, lo, hi);
    ui_hidden(s_vide, !vide);
    if (vide) {
        ui_text(s_vide, s.recue ? tr("Aucun historique") : tr("En attente de Home Assistant"));
        cacher_trace();
        return;
    }

    // Sans aucune température (seulement l'humidité) : une échelle quelconque, sans ses
    // libellés.
    const bool degres_visibles = !std::isnan(lo);
    Repere r = repere(s, degres_visibles ? lo : 0.0f, degres_visibles ? hi : 1.0f);
    r.humidite = s.humidite && humidite_connue(hmin);
    if (r.humidite) {
        r.x1 = kTraceX1Humidite;
        r.h = echelle_humidite(hmin, hmax, r.intervalles);
    }
    peindre_graduations(r, degres_visibles);
    const int32_t x_maintenant = r.x(s.maintenant);
    const bool prevue = s.np > 0;
    peindre_partie_prevue(r, prevue, x_maintenant);
    const int32_t larg_barre = peindre_barres(s, r);
    peindre_humidite(s, r, x_maintenant);
    peindre_courbe(s, r, x_maintenant);
    peindre_prevision(s, r, prevue, x_maintenant, larg_barre);
    peindre_axe(s, r);
    peindre_legende(s, prevue, r.humidite);
}

void peindre() {
    if (!visible() || s_mem == nullptr || s_cle < 0) return;
    peindre_cartes(serie());
    peindre_graphique();
}

// --- Onglets et geste (ADR-0047) ---------------------------------------------------------

// Une température a sa page quand elle a une source : la zone salon ou serre présente
// (zones absentes : tab5_maj_zones), une température déclarée pour la pièce R.
bool cle_disponible(int c) {
    if (c == SALON) return !zone_absente(Zone::SALON);
    if (c == SERRE) return !zone_absente(Zone::SERRE);
    return piece_climat_a_temperature(c - PIECE_0);
}

// Pages du popup, dans l'ordre des onglets ; la page montrée en fait toujours partie.
void lister_onglets() {
    s_nb_onglets = 0;
    for (int c : kOrdreOnglets)
        if (c == s_cle || cle_disponible(c)) s_onglets[s_nb_onglets++] = c;
}

// Nom d'un onglet : pour une pièce, le sien (sa carte centrale) ; pour le salon et la
// serre, celui que HA a poussé (la pièce du capteur) dès qu'une réponse est arrivée.
void nom_onglet(int c, char* out, size_t n) {
    if (c < PIECE_0 && s_mem != nullptr) {
        for (const Serie& s : s_mem->series[c]) {
            if (s.recue && s.nom[0] != '\0') {
                snprintf(out, n, "%s", s.nom);
                return;
            }
        }
    }
    bool exterieur = false;
    if (s_mem != nullptr)
        for (const Serie& s : s_mem->series[c]) exterieur = exterieur || (s.recue && s.exterieur);
    nom_par_defaut(c, exterieur, out, n);
}

// Onglet actif comme l'option active des Réglages (peindre_choix, tab5_reglages.cpp) :
// bordure et texte en accent ; les autres au style du bouton (style_clim_btn).
void peindre_onglet(lv_obj_t* b, bool actif) {
    if (actif) {
        highlight_button_border(b, true, UIColor.ACCENT, 3);
    } else {
        for (lv_style_prop_t p : {LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_OPA, LV_STYLE_BORDER_WIDTH})
            lv_obj_remove_local_style_prop(b, p, LV_PART_MAIN);
    }
    ui_text_color(lv_obj_get_child(b, 0), actif ? UIColor.ACCENT : UIColor.TEXT_SOFT);
}

// Fin du titre « Température » : sa police (celle des titres du thème) et son texte.
int32_t fin_du_titre() {
    lv_obj_t* const t = g_historique_ui.titre_popup;
    if (t == nullptr) return kTitrePopupX;
    const lv_font_t* police = lv_obj_get_style_text_font(t, LV_PART_MAIN);
    const char* texte = lv_label_get_text(t);
    if (police == nullptr || texte == nullptr) return kTitrePopupX;
    lv_point_t taille;
    lv_text_get_size(&taille, texte, police, lv_obj_get_style_text_letter_space(t, LV_PART_MAIN), 0, LV_COORD_MAX,
                     LV_TEXT_FLAG_NONE);
    return kTitrePopupX + taille.x;
}

// Une seule page : pas d'onglet (rien à choisir).
void peindre_onglets() {
    const HistoriqueUI& u = g_historique_ui;
    const int n = s_nb_onglets >= 2 ? s_nb_onglets : 0;
    int32_t debut = fin_du_titre() + 24;
    int32_t w = n > 0 ? (kOngletsFin - debut - (n - 1) * kOngletEcart) / n : 0;
    if (w > kOngletL) w = kOngletL;
    if (w < 60) w = 60;
    debut = kOngletsFin - n * w - (n - 1) * kOngletEcart;
    char nom[48];
    for (int i = 0; i < kOngletsMax; i++) {
        lv_obj_t* const b = u.onglet[i];
        if (b == nullptr) continue;
        ui_hidden(b, i >= n);
        if (i >= n) continue;
        if (lv_obj_get_style_width(b, LV_PART_MAIN) != w) lv_obj_set_width(b, w);
        ui_x(b, debut + i * (w + kOngletEcart));
        lv_obj_t* const l = lv_obj_get_child(b, 0);
        if (l != nullptr && lv_obj_get_style_width(l, LV_PART_MAIN) != w - 10) lv_obj_set_width(l, w - 10);
        nom_onglet(s_onglets[i], nom, sizeof(nom));
        ui_text(l, nom);
        peindre_onglet(b, s_onglets[i] == s_cle);
    }
}

void demander() {
    if (g_historique_ui.demander != nullptr && s_cle >= 0) g_historique_ui.demander(kCles[s_cle], kVues[s_vue]);
}

// Montre la température `c` à la même vue : ce qui est gardé tout de suite, puis la
// réponse de HA.
void montrer_cle(int c) {
    if (c < 0 || c >= NB_CLES || c == s_cle) return;
    s_cle = c;
    ui_mark_activity();
    peindre_onglets();
    peindre();
    demander();
}

// LV_EVENT_GESTURE du popup (construire), comme les pages des Réglages : gauche = page
// suivante, droite = précédente, en boucle (reglages_page_voisine, tab5_core.cpp).
// lv_indev_wait_release() : le lever du doigt qui suit ne déclenche rien (ni le bouton
// où le geste est parti, ni un appui long).
void geste(lv_event_t* /*e*/) {
    lv_indev_t* const indev = lv_indev_active();
    if (indev == nullptr || s_nb_onglets < 2) return;
    const lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) return;
    int rang = 0;
    for (int i = 0; i < s_nb_onglets; i++)
        if (s_onglets[i] == s_cle) rang = i;
    lv_indev_wait_release(indev);
    montrer_cle(s_onglets[reglages_page_voisine(rang, s_nb_onglets, dir == LV_DIR_LEFT)]);
}

// Couleurs de ce que construire() a créé : à la construction et à chaque thème.
void couleurs() {
    const uint32_t humidite = couleur_humidite();
    lv_obj_set_style_bg_color(s_fond_prev, lv_color_hex(UIColor.GOLD), LV_PART_MAIN);
    for (lv_obj_t* o : s_grille) lv_obj_set_style_bg_color(o, lv_color_hex(UIColor.GLASS_RIM), LV_PART_MAIN);
    for (lv_obj_t* o : s_degres) ui_text_color(o, UIColor.TEXT_DIM);
    for (lv_obj_t* o : s_pourcents) ui_text_color(o, humidite);
    for (lv_obj_t* o : s_bandes_prev) lv_obj_set_style_bg_color(o, lv_color_hex(UIColor.GOLD), LV_PART_MAIN);
    for (lv_obj_t* o : s_bandes) lv_obj_set_style_bg_color(o, lv_color_hex(UIColor.ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_trait_maintenant, lv_color_hex(UIColor.TEXT_DIM), LV_PART_MAIN);
    lv_obj_set_style_line_color(s_prevision, lv_color_hex(UIColor.GOLD), LV_PART_MAIN);
    lv_obj_set_style_line_color(s_hum_courbe, lv_color_hex(humidite), LV_PART_MAIN);
    lv_obj_set_style_line_color(s_courbe, lv_color_hex(UIColor.ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_hum_point, lv_color_hex(humidite), LV_PART_MAIN);
    lv_obj_set_style_border_color(s_hum_point, lv_color_hex(UIColor.TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_point, lv_color_hex(UIColor.ACCENT), LV_PART_MAIN);
    lv_obj_set_style_border_color(s_point, lv_color_hex(UIColor.TEXT_PRIMARY), LV_PART_MAIN);
    ui_text_color(s_maintenant, UIColor.TEXT_DIM);
    for (lv_obj_t* o : s_axe) ui_text_color(o, UIColor.TEXT_DIM);
    lv_obj_set_style_bg_color(s_leg_trait, lv_color_hex(UIColor.ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_leg_bande, lv_color_hex(UIColor.ACCENT), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_leg_prev_trait, lv_color_hex(UIColor.GOLD), LV_PART_MAIN);
    lv_obj_set_style_bg_color(s_leg_hum_trait, lv_color_hex(humidite), LV_PART_MAIN);
    for (lv_obj_t* o : s_leg_txt) ui_text_color(o, UIColor.TEXT_DIM);
    ui_text_color(s_vide, UIColor.TEXT_DIM);
}

lv_obj_t* libelle(lv_obj_t* parent) {
    lv_obj_t* l = lv_label_create(parent);
    if (g_historique_ui.police != nullptr)
        esphome::lvgl::lv_obj_set_style_text_font(l, g_historique_ui.police, LV_PART_MAIN);
    lv_label_set_text(l, "");
    lv_obj_add_flag(l, LV_OBJ_FLAG_HIDDEN);
    return l;
}

// Pastille de la valeur actuelle : pleine, cerclée.
lv_obj_t* pastille(lv_obj_t* parent) {
    lv_obj_t* o = rectangle(parent, LV_OPA_COVER, LV_RADIUS_CIRCLE);
    lv_obj_set_style_border_width(o, 2, LV_PART_MAIN);
    lv_obj_set_style_border_opa(o, LV_OPA_COVER, LV_PART_MAIN);
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
// création est l'ordre de dessin : teinte, grille, barres, traits, courbes (l'humidité
// sous la température), libellés. Et le geste des pages, arrêté au popup.
void construire() {
    HistoriqueUI& u = g_historique_ui;
    if (s_courbe != nullptr || u.zone == nullptr) return;
    s_fond_prev = rectangle(u.zone, LV_OPA_10, 0);
    for (int k = 0; k < kGrilleMax; k++) {
        s_grille[k] = rectangle(u.zone, LV_OPA_40, 0);
        s_degres[k] = libelle(u.zone);
        lv_obj_set_width(s_degres[k], kGradW);
        lv_obj_set_style_text_align(s_degres[k], LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);
        s_pourcents[k] = libelle(u.zone);
        lv_obj_set_width(s_pourcents[k], kHumGradW);
    }
    for (int k = 0; k < kBandesPrevMax; k++) s_bandes_prev[k] = rectangle(u.zone, LV_OPA_30, 3);
    for (int k = 0; k < kMesuresMax; k++) s_bandes[k] = rectangle(u.zone, LV_OPA_30, 3);
    s_trait_maintenant = rectangle(u.zone, LV_OPA_60, 0);
    s_prevision = ligne(u.zone, 3);
    s_hum_courbe = ligne(u.zone, 3);
    s_courbe = ligne(u.zone, 4);
    s_hum_point = pastille(u.zone);
    s_point = pastille(u.zone);
    s_maintenant = libelle(u.zone);
    lv_obj_set_y(s_maintenant, 0);
    lv_obj_set_width(s_maintenant, kAxeLibelleL);
    lv_obj_set_style_text_align(s_maintenant, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    for (int k = 0; k < kAxeMax; k++) {
        lv_obj_t* l = libelle(u.zone);
        lv_obj_set_y(l, kAxeY);
        lv_obj_set_width(l, kAxeLibelleL);
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
    s_leg_hum = element_legende(s_legende, s_leg_hum_trait, 32, 3, LV_OPA_COVER, s_leg_txt[3], tr("Humidité"));
    lv_obj_add_flag(s_leg_hum, LV_OBJ_FLAG_HIDDEN);
    s_vide = libelle(u.zone);
    lv_obj_align(s_vide, LV_ALIGN_CENTER, 0, 0);
    couleurs();
    // Geste gauche / droite : arrêté au popup ([AI-WARNING] de l'en-tête), traité ici.
    lv_obj_remove_flag(u.popup, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_event_cb(u.popup, geste);
    lv_obj_add_event_cb(u.popup, geste, LV_EVENT_GESTURE, nullptr);
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
    if (c < 0 || v < 0) {
        payload_refuse("tab5.historique", "clé ou vue inconnue", cle.size() + vue.size());
        return;
    }
    if (payload_trop_long("tab5.historique", entete.size() + mesures.size() + previsions.size())) return;
    // Avant la première ouverture, aucune demande n'est partie : rien à ranger.
    if (s_mem == nullptr) return;
    // Rangée sous sa température et sa vue, même si le popup en montre une autre (une page
    // quittée entre la demande et la réponse) : la page se peindra tout de suite à son retour.
    Serie& s = s_mem->series[c][v];
    const Champ nom = historique_lire(Champ{entete.data(), entete.size()}, Champ{mesures.data(), mesures.size()},
                                      Champ{previsions.data(), previsions.size()}, s);
    texte_ha_copier(s.nom, sizeof(s.nom), nom.p, nom.n);
    if (!visible()) return;
    if (c < PIECE_0) peindre_onglets();  // le nom poussé par HA devient celui de l'onglet
    if (c == s_cle && v == s_vue) peindre();
}

void historique_ouvrir(const std::string& cle) {
    HistoriqueUI& u = g_historique_ui;
    const int c = index_de(cle, kCles, NB_CLES);
    if (u.popup == nullptr || c < 0 || !memoire()) return;
    construire();
    s_cle = c;
    s_vue = JOUR;
    lister_onglets();
    animate_popup_open(u.popup);
    ui_mark_activity();
    peindre_onglets();
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

void historique_onglet(int n) {
    if (n < 0 || n >= s_nb_onglets) return;
    montrer_cle(s_onglets[n]);
}

// Thèmes (ADR-0029) : ce que construire() a peint une fois, puis le popup s'il est
// ouvert ; fermé, sa prochaine ouverture repeint cartes, onglets et courbes.
void historique_rejouer_theme() {
    if (s_courbe == nullptr) return;
    couleurs();
    if (visible()) peindre_onglets();
    peindre();
}
