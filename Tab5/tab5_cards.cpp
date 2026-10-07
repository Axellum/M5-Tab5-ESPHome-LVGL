/**
 * [AI-CONTEXT]
 * @file tab5_cards.cpp
 * @role Cartes domotique : clim (cible optimiste et retour de HA, réglages venus de
 *       l'appareil — clé « climr », ADR-0026 —, clims des tuiles — clés « crRT » et
 *       « ceRT », ADR-0027 — et clim affichée par le popup), tri dynamique des 5 plantes
 *       vers 4 slots, popup détails pots (EC / lux / température / batterie), température
 *       colorée. Unité de compilation issue de la scission de tab5_custom.cpp (lot (e) de
 *       l'audit du 06/09/2026, faite le 08/09/2026) : mêmes fonctions, même ordre,
 *       aucune logique modifiée. update_clim_from_ha_ui() y est venue de
 *       tab5_services.cpp le 29/09/2026 (ADR-0026) ; avec les clims des tuiles (ADR-0027,
 *       même jour) elle est devenue clim_blueprint_recu(), et la coloration du script
 *       tab5_clim_recolor, clim_recolorer() : tout l'affichage de la clim est ici.
 * @architecture_constraint Deux clims ne se mélangent jamais. La carte de l'accueil montre
 *       la clim du blueprint (ses globals clim_*, que tab5_maj_clim écrit) ; le popup
 *       montre la « clim affichée » (s_vue) : celle du blueprint, ou une clim de tuile
 *       (table s_ct, en PSRAM). Un retour de HA pour l'une ne touche jamais les widgets
 *       de l'autre ; les gestes du popup écrivent l'état de la clim affichée et partent à
 *       son emplacement (« clim » ou « tRT »).
 * @regle_absolue Seul point de contact avec l'API LVGL, comme avant : les YAML
 *                n'appellent que des helpers déclarés dans tab5_custom.h. Les
 *                helpers partagés entre unités sont déclarés dans tab5_internal.h.
 * @memory_constraint Éviter std::string dans les boucles de parsing ; char* + strtok_r.
 *       Les clims des tuiles : 25 cases en PSRAM, allouées à la première clé cr/ce.
 */
#include "tab5_custom.h"
#include "tab5_internal.h"
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <esp_heap_caps.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <new>
#include <vector>
#include <map>

// Carte et popup lumière : tab5_tuiles.cpp depuis les pièces (ADR-0023, 28/09/2026) —
// épaules des tuiles, cartes du mode HA, sélecteur des lumières de la pièce.

// =============================================================================
// Clim : réglages de l'appareil (ADR-0026), clims des tuiles et clim affichée par le
// popup (ADR-0027), cible optimiste (arc + boutons -/+) et retour de HA (tab5_maj_clim,
// clé ceRT)
// =============================================================================

// ─── Réglages venus de l'appareil (ADR-0026) ─────────────────────────────────────
// Clé « climr » de tab5_maj_emplacements : « climr|min|max|pas|unité|capacités|nom »,
// poussée par le blueprint avant tab5_maj_clim (connexion, rechargement, changement de
// la clim). Pas de NVS. Tant que rien n'est reçu, les valeurs par défaut ci-dessous
// sont celles de la 3.2 (Daikin de l'auteur) et aucun widget n'est touché : un
// blueprint plus ancien garde l'écran d'avant.
namespace {

// Plage plausible des bornes d'une clim, en °C comme en °F : des « min|max » reçus
// au-delà sont ignorés, et l'arc n'en reçoit jamais d'autres (lot A, audit du 30/09/2026).
constexpr int kClimBorneBasse = -100;
constexpr int kClimBorneHaute = 200;

struct ClimReglages {
    float min = 16.0f;
    float max = 30.0f;
    float pas = 0.5f;
    bool fahrenheit = false;
    // Lettres des boutons que l'appareil gère (tableau de l'ADR-0026) : c froid, h chaud,
    // d sec, f ventilation, e Éco, b Boost, q Silence, s Oscillation, w Brise.
    char capacites[16] = "chdfebqsw";
    char nom[49] = "";  // friendly_name de la clim, 48 octets au plus
    bool recu = false;
};
ClimReglages s_clim;
// Dernières valeurs affichées de la clim du blueprint : reposées quand ses réglages
// changent (bornes de l'arc, format de la cible, unité de la pièce) et quand le popup
// revient à elle.
float s_clim_consigne = NAN;
float s_clim_piece = NAN;

// ─── Clims des tuiles (ADR-0027) ─────────────────────────────────────────────────
// Une tuile cli sans l'option m (m = la clim du blueprint) a la sienne : réglages (clé
// « crRT », les champs de climr) et état (clé « ceRT », les champs de tab5_maj_clim),
// poussés par le blueprint avec les tuiles. Pas de NVS. Préfixes lus par le blueprint :
// ne pas les renommer sans lui (tests/test_clim.py).
constexpr char kCleReglagesTuile[] = "cr";
constexpr char kCleEtatTuile[] = "ce";
constexpr int kPieces = 5;
constexpr int kTuiles = 5;
// Modes gardés sur 15 octets au plus : la chaîne reste dans son std::string (petite
// chaîne, sans allocation). Aucun mode de HA n'est plus long.
constexpr size_t kModeMax = 15;

struct ClimEtat {
    float consigne = NAN;  // NaN : inconnue (« -- » ; − / + ne font rien, l'arc la choisit)
    float piece = NAN;     // température de la pièce
    std::string mode;
    std::string preset;
    std::string ventilation;
    std::string oscillation;
};

struct ClimTuile {
    ClimReglages reglages;  // reglages.recu : crRT reçue, l'appui de la tuile ouvre le popup
    ClimEtat etat;
};

// 25 cases (≈ 4,5 Ko) en PSRAM, allouées à la première clé cr/ce : rien tant qu'aucune
// tuile n'a sa clim, rien en RAM interne. Pas d'EXT_RAM_BSS_ATTR : ces cases ont des
// valeurs initiales (constructeurs), et une BSS externe n'en garde aucune.
ClimTuile* s_ct = nullptr;
// Clim affichée par le popup : case r * kTuiles + t d'une tuile ; -1 = celle du blueprint.
int s_vue = -1;
char s_vue_cle[4] = "";  // « tRT » de la tuile affichée

// Consigne d'une clim de tuile en attente de son débounce : clé et valeur prises au geste.
struct Attente {
    char cle[4] = "";
    float valeur = NAN;
};
Attente s_attente;

ClimTuile* tuile_clim(int r, int t, bool creer) {
    if (r < 0 || r >= kPieces || t < 0 || t >= kTuiles) return nullptr;
    if (s_ct == nullptr) {
        if (!creer) return nullptr;
        const size_t octets = sizeof(ClimTuile) * kPieces * kTuiles;
        void* p = heap_caps_malloc(octets, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (p == nullptr) p = heap_caps_malloc(octets, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (p == nullptr) {
            ESP_LOGE("tab5.clim", "Clims des tuiles : %u octets introuvables", static_cast<unsigned>(octets));
            return nullptr;
        }
        ClimTuile* table = static_cast<ClimTuile*>(p);
        for (int i = 0; i < kPieces * kTuiles; i++) new (&table[i]) ClimTuile();
        s_ct = table;
        ESP_LOGI("tab5.clim", "Clims des tuiles : %u octets", static_cast<unsigned>(octets));
    }
    return &s_ct[r * kTuiles + t];
}

bool vue_tuile() { return s_vue >= 0 && s_ct != nullptr; }
const ClimReglages& vue_reglages() { return vue_tuile() ? s_ct[s_vue].reglages : s_clim; }

// Champs de l'état de la clim affichée : ceux du blueprint sont ses globals (pointeurs
// posés par tab5_clim_ui), ceux d'une tuile sa case de la table.
enum class Champ : uint8_t { MODE, PRESET, VENTILATION, OSCILLATION };

std::string& champ_vue(Champ c) {
    if (vue_tuile()) {
        ClimEtat& e = s_ct[s_vue].etat;
        switch (c) {
            case Champ::MODE: return e.mode;
            case Champ::PRESET: return e.preset;
            case Champ::VENTILATION: return e.ventilation;
            default: return e.oscillation;
        }
    }
    const ClimUI& u = g_clim_ui;
    std::string* p = nullptr;
    switch (c) {
        case Champ::MODE: p = u.mode_bp; break;
        case Champ::PRESET: p = u.preset_bp; break;
        case Champ::VENTILATION: p = u.ventilation_bp; break;
        default: p = u.oscillation_bp; break;
    }
    // Pointeurs posés à la fin du setup, avant tout geste et toute poussée de HA.
    static std::string vide;
    if (p == nullptr) {
        vide.clear();
        return vide;
    }
    return *p;
}

// Consigne sur laquelle les boutons − / + du popup comptent : clim_target_temp pour la
// clim du blueprint (comme sa carte), la consigne reçue (ou choisie) pour une tuile.
float vue_consigne_base() {
    if (vue_tuile()) return s_ct[s_vue].etat.consigne;
    return g_clim_ui.consigne_bp != nullptr ? *g_clim_ui.consigne_bp : NAN;
}

// Titre du popup : de x = 52 (modal_header.yaml) à la croix (80 px à 10 px du bord de la
// carte de 1250), avec de l'air.
constexpr int32_t kLargeurTitreClim = 1080;

// Positions de la carte OPTIONS (climate_popup.yaml, tout affiché) : titre de section,
// son contenu 26 px dessous ; rangée des préréglages 92 px, bouton 88 px, 10 px entre deux
// boutons, 24 px entre deux sections.
constexpr int32_t kOptionsY0 = 50;
constexpr int32_t kOptionsSousTitre = 26;
constexpr int32_t kOptionsRangee = 92;
constexpr int32_t kOptionsBouton = 88;
constexpr int32_t kOptionsEntreBoutons = 10;
constexpr int32_t kOptionsEntreSections = 24;

// Un bouton que la clim affichée sait faire (lettre du tableau de l'ADR-0026).
bool clim_capacite(char lettre) {
    return lettre != '\0' && std::strchr(vue_reglages().capacites, lettre) != nullptr;
}

// Format de la cible : « %.1f » si le pas est fractionnaire (0,5), « %.0f » sinon (1 °F).
bool clim_pas_entier(const ClimReglages& r) {
    return std::fabs(r.pas - std::round(r.pas)) < 0.001f;
}

const char* clim_unite(const ClimReglages& r) {
    return r.fahrenheit ? "\xC2\xB0" "F" : "\xC2\xB0" "C";
}

// Consigne inconnue (capteur HA indisponible = NaN) : « -- », comme au boot — "%.1f"
// écrirait « nan », sans glyphe dans roboto_55_b (popup clim).
void clim_format_consigne(const ClimReglages& r, char* buf, size_t n, float t) {
    if (std::isnan(t)) snprintf(buf, n, "--");
    else snprintf(buf, n, clim_pas_entier(r) ? "%.0f" : "%.1f", t);
}

// Un pas de plus (sens > 0) ou de moins, borné aux limites de l'appareil.
float consigne_suivante(const ClimReglages& r, float t, int sens) {
    float v = t + (sens < 0 ? -r.pas : r.pas);
    if (v < r.min) v = r.min;
    if (v > r.max) v = r.max;
    return v;
}

// Nombre d'un champ (point décimal) ; `defaut` s'il est vide ou illisible.
float lire_nombre(const char* p, size_t n, float defaut) {
    char tmp[16];
    if (n == 0 || n >= sizeof(tmp)) return defaut;
    std::memcpy(tmp, p, n);
    tmp[n] = '\0';
    char* bout = nullptr;
    const float v = strtof(tmp, &bout);
    return (bout == tmp || std::isnan(v) || std::isinf(v)) ? defaut : v;
}

// Champs séparés par '|' (au plus `max`, le dernier prend tout le reste : le blueprint y a
// remplacé « | » par « / ») : début et longueur de chacun. Renvoie leur nombre.
int decouper(const char* s, size_t n, const char* champ[], size_t taille[], int max) {
    int k = 0;
    size_t debut = 0;
    for (size_t i = 0; i <= n && k < max; i++) {
        if (i == n || (s[i] == '|' && k < max - 1)) {
            champ[k] = s + debut;
            taille[k] = i - debut;
            k++;
            debut = i + 1;
        }
    }
    return k;
}

// Réglages « min|max|pas|unité|capacités|nom » (climr ou crRT, sans la clé) dans `r`,
// dont les valeurs servent de défaut à un nombre illisible. Renvoie le nombre de champs :
// moins de 5, rien n'est changé.
int lire_reglages(const char* reste, size_t n, ClimReglages& r) {
    const char* champ[6] = {};
    size_t taille[6] = {};
    const int k = decouper(reste, n, champ, taille, 6);
    if (k < 5) return k;
    const float mn = lire_nombre(champ[0], taille[0], r.min);
    const float mx = lire_nombre(champ[1], taille[1], r.max);
    // Bornes hors de toute clim réelle (« -1e30 ») ignorées : elles deviendraient celles
    // de l'arc (lot A de l'audit du 30/09/2026).
    if (mn < mx && mn >= kClimBorneBasse && mx <= kClimBorneHaute) {
        r.min = mn;
        r.max = mx;
    }
    const float pas = lire_nombre(champ[2], taille[2], r.pas);
    if (pas > 0.0f && pas <= 10.0f) r.pas = pas;
    // « °F » ou « °C » (UTF-8) : la dernière lettre suffit.
    r.fahrenheit = taille[3] > 0 && champ[3][taille[3] - 1] == 'F';
    size_t j = 0;
    for (size_t i = 0; i < taille[4] && j + 1 < sizeof(r.capacites); i++)
        if (champ[4][i] >= 'a' && champ[4][i] <= 'z') r.capacites[j++] = champ[4][i];
    r.capacites[j] = '\0';
    if (k == 6) texte_ha_copier(r.nom, sizeof(r.nom), champ[5], taille[5]);
    else r.nom[0] = '\0';
    r.recu = true;
    return k;
}

// État « consigne|pièce|mode|préréglage|ventilation|oscillation » (ceRT, sans la clé) :
// les nombres « nan » ou illisibles sont inconnus, les modes gardés tels quels (bornés).
void lire_etat(const char* reste, size_t n, ClimEtat& e) {
    const char* champ[6] = {};
    size_t taille[6] = {};
    const int k = decouper(reste, n, champ, taille, 6);
    e.consigne = k > 0 ? lire_nombre(champ[0], taille[0], NAN) : NAN;
    e.piece = k > 1 ? lire_nombre(champ[1], taille[1], NAN) : NAN;
    std::string* modes[4] = {&e.mode, &e.preset, &e.ventilation, &e.oscillation};
    for (int i = 0; i < 4; i++) {
        if (k > 2 + i) modes[i]->assign(champ[2 + i], std::min(taille[2 + i], kModeMax));
        else modes[i]->clear();
    }
}

// Carte OPTIONS : les sections qui ont un bouton visible s'empilent depuis le haut, une
// section sans bouton disparaît avec son titre. Dans la rangée flex des préréglages et
// dans la pile des modes, LVGL saute les objets masqués : pas de trou.
void clim_options_empiler() {
    const ClimUI& u = g_clim_ui;
    const bool eco = clim_capacite('e');
    const bool boost = clim_capacite('b');
    const bool silence = clim_capacite('q');
    const bool oscillation = clim_capacite('s');
    const bool brise = clim_capacite('w');
    int32_t y = kOptionsY0;

    const bool presets = eco || boost;
    ui_hidden(u.eco, !eco);
    ui_hidden(u.boost, !boost);
    ui_hidden(u.titre_presets, !presets);
    ui_hidden(u.rangee_presets, !presets);
    if (presets) {
        ui_y(u.titre_presets, y);
        ui_y(u.rangee_presets, y + kOptionsSousTitre);
        y += kOptionsSousTitre + kOptionsRangee + kOptionsEntreSections;
    }

    ui_hidden(u.silence, !silence);
    ui_hidden(u.titre_ventilation, !silence);
    if (silence) {
        ui_y(u.titre_ventilation, y);
        ui_y(u.silence, y + kOptionsSousTitre);
        y += kOptionsSousTitre + kOptionsBouton + kOptionsEntreSections;
    }

    const bool flux = oscillation || brise;
    ui_hidden(u.oscillation, !oscillation);
    ui_hidden(u.brise, !brise);
    ui_hidden(u.titre_flux, !flux);
    if (flux) {
        ui_y(u.titre_flux, y);
        int32_t yb = y + kOptionsSousTitre;
        if (oscillation) {
            ui_y(u.oscillation, yb);
            yb += kOptionsBouton + kOptionsEntreBoutons;
        }
        if (brise) ui_y(u.brise, yb);
    }
}

// Cible du popup et son arc, au format de la clim affichée. Consigne inconnue : « -- »,
// l'arc reste où il est. Un retour de HA ne bouge pas l'arc pendant qu'on le glisse (il
// sauterait sous le doigt), comme celui du popup lumière.
void popup_consigne_ui(float t, bool depuis_ha) {
    const ClimUI& u = g_clim_ui;
    char buf[16];
    clim_format_consigne(vue_reglages(), buf, sizeof(buf), t);
    ui_text(u.consigne_popup, buf);
    if (!std::isfinite(t) || u.arc == nullptr) return;
    if (depuis_ha && lv_obj_has_state(u.arc, LV_STATE_PRESSED)) return;
    lv_arc_set_value(u.arc, tab5_float_vers_int(t, lv_arc_get_min_value(u.arc), lv_arc_get_max_value(u.arc), 0));
}

// Température de la pièce (popup), dans l'unité de la clim affichée ; inconnue : « -- ».
void popup_piece_ui(float p) {
    char buf[20];
    const char* unite = clim_unite(vue_reglages());
    if (std::isnan(p)) snprintf(buf, sizeof(buf), "-- %s", unite);
    else snprintf(buf, sizeof(buf), "%.1f %s", p, unite);
    ui_text(g_clim_ui.piece, buf);
}

// Réglages de la clim affichée sur le popup : bornes de l'arc, unité, titre, boutons.
void popup_reglages_ui(float consigne) {
    const ClimUI& u = g_clim_ui;
    const ClimReglages& r = vue_reglages();

    // Arc : bornes entières qui englobent celles de l'appareil, puis la consigne de
    // nouveau (lv_arc_set_range la ramène dans les anciennes bornes ; aucun des deux
    // n'émet LV_EVENT_VALUE_CHANGED, donc pas de on_value ni d'envoi à HA).
    if (u.arc != nullptr) {
        const int32_t bas = tab5_float_vers_int(std::floor(r.min), kClimBorneBasse, kClimBorneHaute, kClimBorneBasse);
        const int32_t haut = tab5_float_vers_int(std::ceil(r.max), kClimBorneBasse, kClimBorneHaute, kClimBorneHaute);
        if (lv_arc_get_min_value(u.arc) != bas || lv_arc_get_max_value(u.arc) != haut)
            lv_arc_set_range(u.arc, bas, haut);
        if (std::isfinite(consigne)) lv_arc_set_value(u.arc, tab5_float_vers_int(consigne, bas, haut, bas));
    }
    ui_text(u.unite, clim_unite(r));

    // Titre : le nom de la clim dans HA ; sans nom, « Climatisation ».
    texte_ha_coupe(u.titre, r.nom[0] != '\0' ? r.nom : tr("Climatisation"), kLargeurTitreClim);

    // Carte MODE : les modes que l'appareil n'a pas disparaissent (« Éteint » reste).
    ui_hidden(u.mode_froid, !clim_capacite('c'));
    ui_hidden(u.mode_chaud, !clim_capacite('h'));
    ui_hidden(u.mode_sec, !clim_capacite('d'));
    ui_hidden(u.mode_ventilation, !clim_capacite('f'));

    clim_options_empiler();
}

// Tout le popup d'après la clim affichée (elle vient de changer) : réglages, cible,
// pièce, couleurs.
void popup_peindre() {
    const float consigne = vue_tuile() ? s_ct[s_vue].etat.consigne : s_clim_consigne;
    const float piece = vue_tuile() ? s_ct[s_vue].etat.piece : s_clim_piece;
    popup_reglages_ui(consigne);
    popup_consigne_ui(consigne, false);
    popup_piece_ui(piece);
    clim_recolorer();
}

bool popup_visible() {
    const lv_obj_t* p = g_clim_ui.popup;
    return p != nullptr && !lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN);
}

// Popup refermé sans sa croix ni son voile (retour à l'accueil par inactivité, « Aller à
// l'écran ») alors qu'il montrait une tuile : la clim affichée redevient celle du
// blueprint, avant qu'un retour de HA ne soit rangé au mauvais endroit.
void vue_verifier() {
    if (s_vue < 0 || popup_visible()) return;
    s_vue = -1;
    popup_peindre();
}

// Carte de l'accueil : la cible de la clim du blueprint, au format de son pas.
void carte_consigne_ui(float t) {
    char buf[16];
    clim_format_consigne(s_clim, buf, sizeof(buf), t);
    ui_text(g_clim_ui.consigne_carte, buf);
}

// Geste sur la consigne du popup : affichage tout de suite, envoi au débounce de la clim
// affichée — celle du blueprint relit clim_target_temp (tab5_debounce_clim_temp), une
// tuile sa clé et sa valeur gardées ici (tab5_debounce_clim_tuile) : un popup fermé
// avant la fin du débounce envoie quand même à la bonne clim.
void consigne_geste(float t) {
    const ClimUI& u = g_clim_ui;
    if (vue_tuile()) {
        s_ct[s_vue].etat.consigne = t;
        std::memcpy(s_attente.cle, s_vue_cle, sizeof(s_attente.cle));
        s_attente.valeur = t;
        popup_consigne_ui(t, false);
        if (u.debounce_tuile != nullptr) u.debounce_tuile();
        return;
    }
    if (u.consigne_bp != nullptr) *u.consigne_bp = t;
    s_clim_consigne = t;
    popup_consigne_ui(t, false);
    if (u.debounce_blueprint != nullptr) u.debounce_blueprint();
}

// Couleur d'une cible selon le mode : bleu en froid, rouge en chaud, blanc sinon.
uint32_t couleur_consigne(const std::string& mode) {
    if (mode == "cool") return UIColor.CLIM_COOL_ACTIVE;
    if (mode == "heat") return UIColor.CLIM_HEAT_ACTIVE;
    return UIColor.TEXT_PRIMARY;
}

}  // namespace

ClimUI g_clim_ui;

void clim_reglages_recu(const char* reste, size_t n) {
    const int k = lire_reglages(reste, n, s_clim);
    if (k < 5) {
        ESP_LOGW("tab5.clim", "Reglages de la clim illisibles (%d champs) : ignores", k);
        return;
    }
    ESP_LOGI("tab5.clim", "Reglages de la clim : %.1f-%.1f, pas %.2f, %s, [%s]",
             s_clim.min, s_clim.max, s_clim.pas, s_clim.fahrenheit ? "F" : "C", s_clim.capacites);
    vue_verifier();
    // Carte de l'accueil : format de la cible. Consigne pas encore reçue : les labels
    // gardent leur texte de démarrage (tab5_maj_clim suit juste après).
    if (!std::isnan(s_clim_consigne)) carte_consigne_ui(s_clim_consigne);
    // Popup : seulement s'il montre la clim du blueprint.
    if (vue_tuile()) return;
    popup_reglages_ui(s_clim_consigne);
    if (!std::isnan(s_clim_consigne)) popup_consigne_ui(s_clim_consigne, true);
    if (!std::isnan(s_clim_piece)) popup_piece_ui(s_clim_piece);
}

bool clim_tuile_recu(const char* cle, size_t n_cle, const char* reste, size_t n_reste) {
    if (n_cle != 4) return false;
    const bool reglages = std::strncmp(cle, kCleReglagesTuile, 2) == 0;
    const bool etat = std::strncmp(cle, kCleEtatTuile, 2) == 0;
    if (!reglages && !etat) return false;
    if (cle[2] < '0' || cle[2] > '4' || cle[3] < '0' || cle[3] > '4') return false;
    const int r = cle[2] - '0', t = cle[3] - '0';
    ClimTuile* c = tuile_clim(r, t, true);
    if (c == nullptr) return true;  // pas de mémoire : ignorée (journal de tuile_clim)
    vue_verifier();
    const bool affichee = vue_tuile() && s_vue == r * kTuiles + t;
    if (reglages) {
        // Défauts de la 3.2 pour une clim encore inconnue, ses derniers réglages sinon.
        ClimReglages lu = c->reglages.recu ? c->reglages : ClimReglages{};
        const int k = lire_reglages(reste, n_reste, lu);
        if (k < 5) {
            ESP_LOGW("tab5.clim", "Reglages de la clim t%d%d illisibles (%d champs) : ignores", r, t, k);
            return true;
        }
        c->reglages = lu;
        ESP_LOGI("tab5.clim", "Reglages de la clim t%d%d : %.1f-%.1f, pas %.2f, %s, [%s]", r, t,
                 lu.min, lu.max, lu.pas, lu.fahrenheit ? "F" : "C", lu.capacites);
        tuiles_repeindre(r, t);  // son bouton apparaît : l'appui ouvre le popup
        if (affichee) popup_peindre();
        return true;
    }
    lire_etat(reste, n_reste, c->etat);
    if (affichee) {
        popup_consigne_ui(c->etat.consigne, true);
        popup_piece_ui(c->etat.piece);
        clim_recolorer();
    }
    return true;
}

bool clim_tuile_connue(int r, int t) {
    const ClimTuile* c = tuile_clim(r, t, false);
    return c != nullptr && c->reglages.recu;
}

bool clim_afficher_tuile(int r, int t) {
    if (!clim_tuile_connue(r, t)) return false;
    s_vue = r * kTuiles + t;
    s_vue_cle[0] = 't';
    s_vue_cle[1] = static_cast<char>('0' + r);
    s_vue_cle[2] = static_cast<char>('0' + t);
    s_vue_cle[3] = '\0';
    popup_peindre();
    return true;
}

void clim_afficher_blueprint() {
    if (s_vue < 0) return;
    s_vue = -1;
    popup_peindre();
}

void clim_tuile_oublier(int r, int t) {
    ClimTuile* c = tuile_clim(r, t, false);
    if (c == nullptr) return;
    *c = ClimTuile();
    const char cle[4] = {'t', static_cast<char>('0' + r), static_cast<char>('0' + t), '\0'};
    // Une consigne en attente irait à l'appareil qui a pris sa place : elle ne part pas.
    if (std::strcmp(s_attente.cle, cle) == 0) s_attente = Attente{};
    if (s_vue != r * kTuiles + t) return;
    if (popup_visible()) animate_popup_close(g_clim_ui.popup);
    s_vue = -1;
    popup_peindre();
}

const char* clim_affichee_cle() {
    vue_verifier();
    return vue_tuile() ? s_vue_cle : "clim";
}

void clim_popup_consigne(float t) { consigne_geste(t); }

void clim_popup_pas(int sens) {
    const float base = vue_consigne_base();
    if (std::isnan(base)) return;  // consigne inconnue : l'arc la choisit
    consigne_geste(consigne_suivante(vue_reglages(), base, sens));
}

void clim_popup_mode(const char* mode) {
    champ_vue(Champ::MODE) = mode;
    clim_recolorer();
}

// Bascules : actif sous tous ses noms (clim_*_actif) → retour à none / auto / stop ; sinon
// le nom de la Daikin (away, boost, quiet, swing, windnice), que le blueprint traduit.
void clim_popup_preset(const char* bouton) {
    std::string& p = champ_vue(Champ::PRESET);
    p = clim_preset_actif(p, bouton) ? std::string("none") : std::string(bouton);
    clim_recolorer();
}

void clim_popup_silence() {
    std::string& f = champ_vue(Champ::VENTILATION);
    f = clim_silence_actif(f) ? std::string("auto") : std::string("quiet");
    clim_recolorer();
}

void clim_popup_oscillation() {
    std::string& s = champ_vue(Champ::OSCILLATION);
    s = clim_oscillation_actif(s) ? std::string("stop") : std::string("swing");
    clim_recolorer();
}

void clim_popup_brise() {
    std::string& s = champ_vue(Champ::OSCILLATION);
    s = (s == "windnice") ? std::string("stop") : std::string("windnice");
    clim_recolorer();
}

const std::string& clim_affichee_preset() { return champ_vue(Champ::PRESET); }
const std::string& clim_affichee_ventilation() { return champ_vue(Champ::VENTILATION); }
const std::string& clim_affichee_oscillation() { return champ_vue(Champ::OSCILLATION); }

const char* clim_tuile_attente_cle() { return s_attente.cle; }

std::string clim_tuile_attente_texte() { return clim_consigne_texte(s_attente.valeur); }

float clim_consigne_suivante(float t, int sens) { return consigne_suivante(s_clim, t, sens); }

std::string clim_consigne_texte(float t) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%.2f", t);
    // « 21.50 » → « 21.5 », « 72.00 » → « 72 » (« nan » reste « nan »).
    if (std::strchr(buf, '.') != nullptr) {
        size_t n = std::strlen(buf);
        while (n > 0 && buf[n - 1] == '0') buf[--n] = '\0';
        if (n > 0 && buf[n - 1] == '.') buf[--n] = '\0';
    }
    return std::string(buf);
}

bool clim_eco_actif(const std::string& preset) {
    return preset == "eco" || preset == "away";
}

bool clim_silence_actif(const std::string& fan) {
    return fan == "quiet" || fan == "silence" || fan == "Silence" || fan == "low";
}

bool clim_oscillation_actif(const std::string& swing) {
    return swing == "swing" || swing == "on" || swing == "both" || swing == "vertical" ||
           swing == "3d" || swing == "horizontal";
}

bool clim_preset_actif(const std::string& preset, const char* bouton) {
    return std::strcmp(bouton, "away") == 0 ? clim_eco_actif(preset) : preset == bouton;
}

// ─── Cible, températures et couleurs ─────────────────────────────────────────────

// Carte de l'accueil (climate_card.yaml) : label de la cible, sans arc (arc nullptr) ;
// une consigne inconnue s'écrit « -- » et l'arc reste où il est.
void update_clim_target_ui(lv_obj_t* lbl_target, lv_obj_t* arc, float target) {
    if (lbl_target == nullptr) return;
    s_clim_consigne = target;
    char buf[16];
    clim_format_consigne(s_clim, buf, sizeof(buf), target);
    ui_text(lbl_target, buf);
    if (!std::isfinite(target)) return;
    if (arc != nullptr)
        lv_arc_set_value(arc, tab5_float_vers_int(target, lv_arc_get_min_value(arc), lv_arc_get_max_value(arc), 0));
}

void clim_blueprint_recu(float consigne, float piece) {
    if (g_clim_ui.consigne_carte == nullptr) return;
    vue_verifier();
    s_clim_consigne = consigne;
    s_clim_piece = piece;
    carte_consigne_ui(consigne);
    // Le popup montre une clim de tuile : il n'est pas à elle.
    if (!vue_tuile()) {
        popup_consigne_ui(consigne, true);
        popup_piece_ui(piece);
    }
    clim_recolorer();
    reglables_clim_changee();  // sa ligne de la liste de la tuile − / + (ADR-0033)
}

const char* clim_nom() { return s_clim.nom; }

uint32_t clim_carte_valeur(char* buf, size_t n, uint32_t& couleur_valeur) {
    clim_format_consigne(s_clim, buf, n, s_clim_consigne);
    static const std::string kSansMode;
    const std::string& mode = g_clim_ui.mode_bp != nullptr ? *g_clim_ui.mode_bp : kSansMode;
    couleur_valeur = couleur_consigne(mode);
    if (mode == "cool" || mode == "heat") return couleur_valeur;
    if (mode == "off") return UIColor.TEXT_DIM;
    if (mode == "unavailable" || mode == "unknown") return UIColor.INACTIVE;
    return UIColor.SUCCESS;
}

void clim_recolorer() {
    const ClimUI& u = g_clim_ui;
    if (u.icone_froid == nullptr) return;

    // « Chaud » est aussi un blanc de lampe (« Warm » en anglais) : le mode de la clim a
    // sa propre traduction (« Heat »), clé « clim|Chaud » (lot 4).
    ui_text(u.libelle_chaud, tr_ctx("clim", "Chaud"));

    // Cible de la carte de l'accueil : la clim du blueprint, quoi que montre le popup.
    // Bleu si froid, rouge si chaud, blanc sinon.
    static const std::string kSansMode;
    ui_text_color(u.consigne_carte, couleur_consigne(u.mode_bp != nullptr ? *u.mode_bp : kSansMode));

    // Popup : la clim affichée.
    const std::string& mode = champ_vue(Champ::MODE);
    const std::string& preset = champ_vue(Champ::PRESET);
    const std::string& fan = champ_vue(Champ::VENTILATION);
    const std::string& swing = champ_vue(Champ::OSCILLATION);
    ui_text_color(u.consigne_popup, couleur_consigne(mode));

    // Mode (Cool/Heat/Dry/Fan/Off). Autre mode (heat_cool, auto… : pas de bouton,
    // ADR-0026) : aucun ne s'allume, plutôt que « Éteint » sur une clim qui tourne.
    const bool eteint = mode == "off" || mode == "unavailable" || mode == "unknown";
    ui_text_color(u.icone_froid, mode == "cool" ? UIColor.CLIM_COOL_ACTIVE : UIColor.CLIM_COOL_INACTIVE);
    ui_text_color(u.icone_chaud, mode == "heat" ? UIColor.CLIM_HEAT_ACTIVE : UIColor.CLIM_HEAT_INACTIVE);
    ui_text_color(u.icone_sec, mode == "dry" ? UIColor.CLIM_COOL_ACTIVE : UIColor.CLIM_COOL_INACTIVE);
    ui_text_color(u.icone_ventilation, mode == "fan_only" ? UIColor.CLIM_ECO : UIColor.CLIM_TRACK_INACTIVE);
    ui_text_color(u.icone_eteint, eteint ? UIColor.CLIM_OFF_ACTIVE : UIColor.CLIM_OFF_INACTIVE);

    // Actif ou non : clim_*_actif(), les mêmes que les bascules du popup et que les
    // listes du blueprint (ADR-0026, tests/test_clim.py). Éco : eco ou away ; Boost :
    // boost ; Silence : quiet, silence, Silence, low ; Oscillation : swing, on, both,
    // vertical, 3d, horizontal (windnice exclu : bouton « Brise » séparé, Daikin Onecta).
    ui_text_color(u.icone_eco, clim_eco_actif(preset) ? UIColor.CLIM_ECO : UIColor.CLIM_TRACK_INACTIVE);
    ui_text_color(u.icone_boost, preset == "boost" ? UIColor.CLIM_HEAT_ACTIVE : UIColor.CLIM_TRACK_INACTIVE);
    ui_text_color(u.icone_silence, clim_silence_actif(fan) ? UIColor.CLIM_COOL_ACTIVE : UIColor.CLIM_TRACK_INACTIVE);
    ui_text_color(u.icone_oscillation,
                  clim_oscillation_actif(swing) ? UIColor.CLIM_COOL_ACTIVE : UIColor.CLIM_TRACK_INACTIVE);
    ui_text_color(u.icone_brise, swing == "windnice" ? UIColor.CLIM_COOL_ACTIVE : UIColor.CLIM_TRACK_INACTIVE);
}

// =============================================================================
// Tri dynamique plantes : 5 capteurs -> 4 slots (2 secs + mediane + humide)
// =============================================================================

// Dernières entrées reçues, rejouées par moisture_slots_refresh() quand les zones
// changent (un pot déclaré absent par HA, ou de retour).
static float s_pots_vals[5] = {NAN, NAN, NAN, NAN, NAN};
static const char* s_pots_icons[5] = {};
static MoistureSlotUI s_pots_slots[4] = {};
static bool s_pots_prets = false;

void sort_and_update_moisture_slots(float values[5], const char* icons_utf8[5],
    MoistureSlotUI slots[4]) {

    // Garde de securite contre les pointeurs nuls si LVGL n'est pas encore initialise
    for (int s = 0; s < 4; s++) {
        if (slots[s].icon_lbl == nullptr) {
            return;
        }
    }
    for (int i = 0; i < 5; i++) {
        s_pots_vals[i] = values[i];
        s_pots_icons[i] = icons_utf8[i];
    }
    for (int s = 0; s < 4; s++) s_pots_slots[s] = slots[s];
    s_pots_prets = true;
    moisture_slots_refresh();
}

void moisture_slots_refresh() {
    if (!s_pots_prets) return;
    const float* values = s_pots_vals;
    const char* const* icons_utf8 = s_pots_icons;
    MoistureSlotUI* slots = s_pots_slots;

    // 1) Pots présents (zones, lot 5) : valides (pas NaN) d'un côté, hors ligne de
    // l'autre, dans l'ordre des capteurs.
    struct Entry { int idx; float val; };
    Entry valid[5];
    int n_valid = 0;
    int hors_ligne[5];
    int n_hors_ligne = 0;

    for (int i = 0; i < 5; i++) {
        if (zone_absente(static_cast<Zone>(static_cast<int>(Zone::POT_1) + i))) continue;
        if (!std::isnan(values[i])) {
            valid[n_valid++] = {i, values[i]};
        } else {
            hors_ligne[n_hors_ligne++] = i;
        }
    }
    const int n_presents = n_valid + n_hors_ligne;
    const int n_slots = n_presents < 4 ? n_presents : 4;

    // 2) Tri par valeur croissante (bubble sort, max 5 elements)
    for (int i = 0; i < n_valid - 1; i++) {
        for (int j = 0; j < n_valid - i - 1; j++) {
            if (valid[j].val > valid[j+1].val) {
                Entry tmp = valid[j];
                valid[j] = valid[j+1];
                valid[j+1] = tmp;
            }
        }
    }

    // 3) Deux façons de remplir les emplacements :
    //    - résumé, avec 5 pots dont au moins 4 valides : le plus sec, le 2e plus sec,
    //      la médiane et le plus humide ;
    //    - sinon, un emplacement par pot : les valides du plus sec au plus humide, puis
    //      les hors ligne en gris. Plus de pot répété sur deux emplacements.
    const bool resume = (n_presents == 5 && n_valid >= 4);
    int selected[4] = {0, 1, n_valid / 2, n_valid - 1};
    if (resume) {
        // Eviter les doublons si mediane == slot 1 ou slot 3
        if (selected[2] <= selected[1]) selected[2] = selected[1] + 1;
        if (selected[2] >= selected[3] && selected[3] > 0) selected[2] = selected[3] - 1;
    }

    // 4) Mise a jour des emplacements LVGL ; ceux en trop sont masqués (la rangée,
    // en flex SPACE_EVENLY, recentre les autres).
    for (int s = 0; s < 4; s++) {
        ui_hidden(lv_obj_get_parent(slots[s].icon_lbl), s >= n_slots);
        if (s >= n_slots) continue;

        int pot;
        float val;
        if (resume) {
            pot = valid[selected[s]].idx;
            val = valid[selected[s]].val;
        } else if (s < n_valid) {
            pot = valid[s].idx;
            val = valid[s].val;
        } else {
            pot = hors_ligne[s - n_valid];
            val = NAN;
        }
        // Icone du capteur d'origine, seule depuis le 05/10/2026 (plus de « Pot X » /
        // « Moy: » dessous) : le nom et la valeur sont dans le popup « Mes Plantes ».
        ui_text(slots[s].icon_lbl, icons_utf8[pot]);

        // Couleur colorimetrique (grise hors ligne)
        ui_text_color(slots[s].icon_lbl, std::isnan(val) ? UIColor.INACTIVE : get_humidity_color(val));
    }
}

// =============================================================================
// Popup details pots : 5 cartes fixes (humidite/statut + EC/lux/temp/batterie)
// =============================================================================

uint32_t get_battery_color(float x) {
    if (std::isnan(x)) return UIColor.INACTIVE;
    if (x > 80.0f) return UIColor.SUCCESS;
    if (x > 40.0f) return UIColor.INFO;
    if (x >= 20.0f) return UIColor.WARNING;
    return UIColor.ERROR;
}

// Thèmes (ADR-0029) : ce que les capteurs ont peint (valeur par label, cartes du popup
// des pots, carte PC), rejoué par cartes_rejouer_theme().
struct MesurePeinte {
    lv_obj_t* lbl;
    float x;
    int metrique;  // PotMetric, ou -1 : update_temp_ui()
};
static MesurePeinte s_mesures[24] = {};
static int s_nb_mesures = 0;
static float s_pots_popup_vals[5] = {};
static PotDetailUI s_pots_popup_cartes[5] = {};
static bool s_pots_popup_peint = false;
static lv_obj_t* s_icone_pc = nullptr;
static bool s_pc_actif = false;

static void mesure_retenir(lv_obj_t* lbl, float x, int metrique) {
    for (int i = 0; i < s_nb_mesures; i++) {
        if (s_mesures[i].lbl == lbl) {
            s_mesures[i].x = x;
            s_mesures[i].metrique = metrique;
            return;
        }
    }
    if (s_nb_mesures < static_cast<int>(sizeof(s_mesures) / sizeof(s_mesures[0])))
        s_mesures[s_nb_mesures++] = {lbl, x, metrique};
}

void update_pots_popup_moisture_ui(const float values[5], PotDetailUI cards[5]) {
    for (int i = 0; i < 5; i++) {
        s_pots_popup_vals[i] = values[i];
        s_pots_popup_cartes[i] = cards[i];
    }
    s_pots_popup_peint = true;
    for (int i = 0; i < 5; i++) {
        if (cards[i].icon_lbl == nullptr || cards[i].moist_lbl == nullptr
            || cards[i].status_lbl == nullptr) {
            continue;
        }
        const float v = values[i];
        const uint32_t c = get_humidity_color(v);  // NaN -> MOISTURE_NAN (gris)
        ui_text_color(cards[i].icon_lbl, c);
        if (std::isnan(v)) {
            ui_text(cards[i].moist_lbl, "--");
            ui_text_color(cards[i].moist_lbl, UIColor.INACTIVE);
            ui_text(cards[i].status_lbl, tr("Hors ligne"));
            ui_text_color(cards[i].status_lbl, UIColor.TEXT_DIM);
            continue;
        }
        char buf[12];
        snprintf(buf, sizeof(buf), "%.0f %%", v);
        ui_text(cards[i].moist_lbl, buf);
        ui_text_color(cards[i].moist_lbl, c);
        // Seuils alignes sur get_humidity_color : <=14 = zone rouge (ALERT_RED)
        if (v <= 14.0f) {
            ui_text(cards[i].status_lbl, tr("\xC3\x80 arroser !"));
            ui_text_color(cards[i].status_lbl, UIColor.ERROR);
        } else if (v <= 20.0f) {
            ui_text(cards[i].status_lbl, tr("Bient\xC3\xB4t sec"));
            ui_text_color(cards[i].status_lbl, UIColor.WARNING);
        } else {
            ui_text(cards[i].status_lbl, "OK");
            ui_text_color(cards[i].status_lbl, UIColor.SUCCESS);
        }
    }
}

void update_pot_metric_ui(lv_obj_t* value_lbl, float x, PotMetric metric) {
    if (value_lbl == nullptr) return;
    mesure_retenir(value_lbl, x, static_cast<int>(metric));
    if (std::isnan(x)) {
        ui_text(value_lbl, "--");
        ui_text_color(value_lbl, UIColor.INACTIVE);
        return;
    }
    char buf[16];
    uint32_t color = UIColor.TEXT_SOFT;
    switch (metric) {
        case PotMetric::CONDUCTIVITY:
            snprintf(buf, sizeof(buf), "%.0f \xC2\xB5S/cm", x);
            break;
        case PotMetric::ILLUMINANCE:
            snprintf(buf, sizeof(buf), "%.0f lx", x);
            break;
        case PotMetric::TEMPERATURE:
            snprintf(buf, sizeof(buf), "%.1f \xC2\xB0" "C", x);
            color = get_temperature_color(x);
            break;
        case PotMetric::BATTERY:
            snprintf(buf, sizeof(buf), "%.0f %%", x);
            color = get_battery_color(x);
            break;
    }
    ui_text(value_lbl, buf);
    ui_text_color(value_lbl, color);
}

// Met a jour un label de temperature (texte + couleur gradient). Factorise
// depuis temp_serre/temp_salon (tab5-sensors-domotique.yaml, Phase 3, #T164).
void update_temp_ui(lv_obj_t* label, float x) {
    if (label == nullptr) return;
    mesure_retenir(label, x, -1);
    if (std::isnan(x)) {
        ui_text(label, "-- \xC2\xB0");
        ui_text_color(label, UIColor.TEXT_DIM);
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%.1f \xC2\xB0", x);
        ui_text(label, buf);
        uint32_t c_int = get_temperature_color(x);
        ui_text_color(label, c_int);
    }
}

// =============================================================================
// Icônes d'état (bandeau + cartes) et carte PC — appelées par les sensors YAML,
// qui ne touchent plus LVGL (règle 2, audit du 06/09/2026 §4.1 point 12).
// =============================================================================

void set_icon_color_ui(lv_obj_t* icon, uint32_t color) {
    if (icon == nullptr) return;
    ui_text_color(icon, color);
}

void set_icon_active_ui(lv_obj_t* icon, bool active, uint32_t color_on, uint32_t color_off) {
    set_icon_color_ui(icon, active ? color_on : color_off);
}

void update_pc_status_ui(bool active, lv_obj_t* icon_pc) {
    if (icon_pc == nullptr) return;
    s_icone_pc = icon_pc;
    s_pc_actif = active;
    set_icon_active_ui(icon_pc, active, UIColor.SUCCESS, UIColor.TEXT_PRIMARY);
}

void cartes_rejouer_theme() {
    for (int i = 0; i < s_nb_mesures; i++) {
        const MesurePeinte m = s_mesures[i];
        if (m.metrique < 0) update_temp_ui(m.lbl, m.x);
        else update_pot_metric_ui(m.lbl, m.x, static_cast<PotMetric>(m.metrique));
    }
    if (s_pots_popup_peint) {
        const float vals[5] = {s_pots_popup_vals[0], s_pots_popup_vals[1], s_pots_popup_vals[2],
                               s_pots_popup_vals[3], s_pots_popup_vals[4]};
        PotDetailUI cartes[5];
        for (int i = 0; i < 5; i++) cartes[i] = s_pots_popup_cartes[i];
        update_pots_popup_moisture_ui(vals, cartes);
    }
    if (s_icone_pc != nullptr) update_pc_status_ui(s_pc_actif, s_icone_pc);
    clim_recolorer();
    moisture_slots_refresh();
}

// Roue d'actions rapides (ADR-0036, tab5_tuiles.cpp) : les modes qu'elle offre sont ceux que
// HA a poussés pour cette clim (climr pour celle du blueprint, crRT pour une tuile) ; les
// capacités par défaut de la 3.2 ne comptent pas (rien reçu : pas de roue, le popup).
const char* clim_capacites_connues(int r, int t) {
    if (r < 0) return s_clim.recu ? s_clim.capacites : nullptr;
    const ClimTuile* c = tuile_clim(r, t, false);
    return (c != nullptr && c->reglages.recu) ? c->reglages.capacites : nullptr;
}

namespace {

// Clim que vise la roue : réglages reçus, consigne et bascules de la clim du blueprint
// (r < 0 : ses globals, comme sa carte) ou de celle de la tuile tRT. Faux si HA n'a pas
// poussé ses réglages (la roue ne lui offre alors rien de plus que le popup).
struct ClimRoue {
    const ClimReglages* reglages = nullptr;
    float consigne = NAN;
    const std::string* preset = nullptr;
    const std::string* ventilation = nullptr;
    const std::string* oscillation = nullptr;
};

bool clim_roue(int r, int t, ClimRoue& c) {
    static const std::string kVide;
    if (r < 0) {
        if (!s_clim.recu) return false;
        const ClimUI& u = g_clim_ui;
        c.reglages = &s_clim;
        c.consigne = u.consigne_bp != nullptr ? *u.consigne_bp : NAN;
        c.preset = u.preset_bp != nullptr ? u.preset_bp : &kVide;
        c.ventilation = u.ventilation_bp != nullptr ? u.ventilation_bp : &kVide;
        c.oscillation = u.oscillation_bp != nullptr ? u.oscillation_bp : &kVide;
        return true;
    }
    const ClimTuile* ct = tuile_clim(r, t, false);
    if (ct == nullptr || !ct->reglages.recu) return false;
    c.reglages = &ct->reglages;
    c.consigne = ct->etat.consigne;
    c.preset = &ct->etat.preset;
    c.ventilation = &ct->etat.ventilation;
    c.oscillation = &ct->etat.oscillation;
    return true;
}

}  // namespace

int clim_roue_consignes(int r, int t, float valeurs[5], char textes[5][10], int& courant) {
    courant = -1;
    ClimRoue c;
    if (!clim_roue(r, t, c) || !std::isfinite(c.consigne)) return 0;
    const ClimReglages& g = *c.reglages;
    int n = 0;
    for (int k = -2; k <= 2; k++) {
        const float v = c.consigne + static_cast<float>(k) * g.pas;
        // Bornes de l'appareil (au millième près : 16 + 4 × 0,5 n'est pas toujours 18 pile).
        if (v < g.min - 0.001f || v > g.max + 0.001f) continue;
        char buf[8];
        clim_format_consigne(g, buf, sizeof(buf), v);
        valeurs[n] = v;
        snprintf(textes[n], 10, "%s\xC2\xB0", buf);
        if (k == 0) courant = n;
        n++;
    }
    return n;
}

int clim_roue_bascules(int r, int t, ClimBascule out[5]) {
    ClimRoue c;
    if (!clim_roue(r, t, c)) return 0;
    const char* capacites = c.reglages->capacites;
    auto a = [capacites](char lettre) { return std::strchr(capacites, lettre) != nullptr; };
    int n = 0;
    // Les mêmes « actif » et les mêmes valeurs que clim_popup_preset, _silence,
    // _oscillation et _brise (Éco envoie « away », actif sur eco ou away).
    if (a('e')) {
        const bool on = clim_eco_actif(*c.preset);
        out[n++] = {'e', on, "preset", on ? "none" : "away"};
    }
    if (a('b')) {
        const bool on = clim_preset_actif(*c.preset, "boost");
        out[n++] = {'b', on, "preset", on ? "none" : "boost"};
    }
    if (a('q')) {
        const bool on = clim_silence_actif(*c.ventilation);
        out[n++] = {'q', on, "ventilation", on ? "auto" : "quiet"};
    }
    if (a('s')) {
        const bool on = clim_oscillation_actif(*c.oscillation);
        out[n++] = {'s', on, "oscillation", on ? "stop" : "swing"};
    }
    if (a('w')) {
        const bool on = *c.oscillation == "windnice";
        out[n++] = {'w', on, "oscillation", on ? "stop" : "windnice"};
    }
    return n;
}

int clim_roue_jauge(int r, int t) {
    ClimRoue c;
    if (!clim_roue(r, t, c) || !std::isfinite(c.consigne) || !(c.reglages->max > c.reglages->min)) return -1;
    const float p = (c.consigne - c.reglages->min) * 100.0f / (c.reglages->max - c.reglages->min);
    return std::clamp(static_cast<int>(std::lround(p)), 0, 100);
}
