/**
 * [AI-CONTEXT]
 * @file tab5_clim.cpp
 * @role Clim (ADR-0026, ADR-0027) : cible optimiste et retour de HA, réglages venus de
 *       l'appareil — clé « climr » —, clims des tuiles — clés « crRT » et « ceRT » —, clims
 *       des pièces — « crpR » et « cepR », ADR-0040, réglées par la tuile − / + quand la
 *       zone des températures montre la pièce — et clim
 *       affichée par le popup, carte de l'accueil, coloration (clim_recolorer) et ce que la
 *       roue d'actions rapides en lit (clim_capacites_connues, clim_roue_*). Sortie de
 *       tab5_cards.cpp le 08/10/2026 (lot L7 de l'audit du 07/10/2026), lignes identiques ;
 *       elle y était venue de tab5_services.cpp le 29/09/2026 (update_clim_from_ha_ui(),
 *       devenue clim_blueprint_recu() avec les clims des tuiles, et la coloration du script
 *       tab5_clim_recolor, clim_recolorer()) : tout l'affichage de la clim est ici.
 * @architecture_constraint Deux clims ne se mélangent jamais. La carte de l'accueil montre
 *       la clim du blueprint (ses globals clim_*, que tab5_maj_clim écrit) ; le popup
 *       montre la « clim affichée » (s_vue) : celle du blueprint, ou une clim de tuile
 *       (table s_ct, en PSRAM), ou de pièce (mêmes cases, après celles des tuiles). Un
 *       retour de HA pour l'une ne touche jamais les widgets de l'autre ; les gestes du
 *       popup écrivent l'état de la clim affichée et partent à son emplacement (« clim »,
 *       « tRT » ou « cpR »).
 * @regle_absolue Seul point de contact avec l'API LVGL, comme avant : les YAML
 *                n'appellent que des helpers déclarés dans tab5_custom.h. Les
 *                helpers partagés entre unités sont déclarés dans tab5_internal.h.
 * @memory_constraint Pas de std::string dans une boucle de parsing : découper un char* en place.
 *       `split_fields()` (tab5_core.h) garde les champs vides ; `strtok_r` les fusionne.
 *       Les clims des tuiles et des pièces : 30 cases en PSRAM, allouées à la première clé
 *       cr/ce. Réglages et état lus par clim_reglages_lire / clim_etat_lire, avec leurs
 *       structures (Tab5/socle/tab5_parse.h, lot F, testés sur PC).
 */
#include "tab5_custom.h"
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include "tab5_modele_ha.h"
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

// ClimReglages (bornes kClimBorneBasse/Haute, défauts de la 3.2, capacités) et ClimEtat :
// Tab5/socle/tab5_parse.h, qui les lit (lot F).
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
// kPieces et kTuiles : tab5_geometrie.h. Modes bornés à kModeMax (tab5_parse.h).

struct ClimTuile {
    ClimReglages reglages;  // reglages.recu : crRT reçue, l'appui de la tuile ouvre le popup
    ClimEtat etat;
};

// Clims des pièces (ADR-0040) : la clim déclarée dans la section « Pièce R » du blueprint,
// clés « crpR » / « cepR » (mêmes champs que crRT / ceRT), réglée par la tuile − / + quand
// la zone des températures montre la pièce. Cinq cases de plus après celles des tuiles ;
// emplacement de ses commandes « cpR » (« pR » est déjà « Pièce : tout éteindre »).
constexpr char kPrefixePiece = 'p';
constexpr int kCasesTuiles = kPieces * kTuiles;
constexpr int kCases = kCasesTuiles + kPieces;

// 30 cases (≈ 5,4 Ko) en PSRAM, allouées à la première clé cr/ce : rien tant qu'aucune
// tuile ni aucune pièce n'a sa clim, rien en RAM interne. Pas d'EXT_RAM_BSS_ATTR : ces
// cases ont des valeurs initiales (constructeurs), et une BSS externe n'en garde aucune.
ClimTuile* s_ct = nullptr;
// Clim affichée par le popup : case r * kTuiles + t d'une tuile, kCasesTuiles + r d'une
// pièce ; -1 = celle du blueprint.
int s_vue = -1;
char s_vue_cle[4] = "";  // « tRT » de la tuile affichée, « cpR » de la pièce

// Consigne d'une clim de tuile ou de pièce en attente de son débounce : clé et valeur
// prises au geste.
struct Attente {
    char cle[4] = "";
    float valeur = NAN;
};
Attente s_attente;

ClimTuile* case_clim(int i, bool creer) {
    if (i < 0 || i >= kCases) return nullptr;
    if (s_ct == nullptr) {
        if (!creer) return nullptr;
        const size_t octets = sizeof(ClimTuile) * kCases;
        void* p = heap_caps_malloc(octets, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (p == nullptr) p = heap_caps_malloc(octets, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
        if (p == nullptr) {
            ESP_LOGE("tab5.clim", "Clims des tuiles : %u octets introuvables", static_cast<unsigned>(octets));
            return nullptr;
        }
        ClimTuile* table = static_cast<ClimTuile*>(p);
        for (int k = 0; k < kCases; k++) new (&table[k]) ClimTuile();
        s_ct = table;
        ESP_LOGI("tab5.clim", "Clims des tuiles : %u octets", static_cast<unsigned>(octets));
    }
    return &s_ct[i];
}

ClimTuile* tuile_clim(int r, int t, bool creer) {
    if (r < 0 || r >= kPieces || t < 0 || t >= kTuiles) return nullptr;
    return case_clim(r * kTuiles + t, creer);
}

ClimTuile* piece_clim(int r, bool creer) {
    if (r < 0 || r >= kPieces) return nullptr;
    return case_clim(kCasesTuiles + r, creer);
}

// Emplacement des commandes de la clim de la pièce R : « cpR » (modele_ha::clim_piece_cle).
void cle_piece(int r, char out[4]) { std::memcpy(out, modele_ha::clim_piece_cle(r).s, 4); }

bool vue_tuile() { return s_vue >= 0 && s_ct != nullptr; }
const ClimReglages& vue_reglages() { return vue_tuile() ? s_ct[s_vue].reglages : s_clim; }

// Champs de l'état de la clim affichée : ceux du blueprint sont ses globals (pointeurs
// posés par tab5_clim_ui), ceux d'une tuile sa case de la table.
enum class ChampClim : uint8_t { MODE, PRESET, VENTILATION, OSCILLATION };

std::string& champ_vue(ChampClim c) {
    if (vue_tuile()) {
        ClimEtat& e = s_ct[s_vue].etat;
        switch (c) {
            case ChampClim::MODE: return e.mode;
            case ChampClim::PRESET: return e.preset;
            case ChampClim::VENTILATION: return e.ventilation;
            default: return e.oscillation;
        }
    }
    const ClimUI& u = g_clim_ui;
    std::string* p = nullptr;
    switch (c) {
        case ChampClim::MODE: p = u.mode_bp; break;
        case ChampClim::PRESET: p = u.preset_bp; break;
        case ChampClim::VENTILATION: p = u.ventilation_bp; break;
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

// Réglages « min|max|pas|unité|capacités|nom » (climr ou crRT, sans la clé) : lus par
// clim_reglages_lire() (Tab5/socle/tab5_parse.cpp, lot F) ; le nom passe ici par
// texte_ha_copier (glyphes des polices). Moins de 5 champs : rien n'est changé.
int lire_reglages(const char* reste, size_t n, ClimReglages& r) {
    Champ nom{};
    const int k = clim_reglages_lire(reste, n, r, nom);
    if (k == 6) texte_ha_copier(r.nom, sizeof(r.nom), nom.p, nom.n);
    return k;
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

// Icône d'une clim sur la tuile − / + et dans sa liste : la couleur de la consigne en
// froid ou en chaud, discrète éteinte, grisée hors ligne, verte dans un autre mode.
uint32_t couleur_icone_carte(const std::string& mode) {
    if (mode == "cool" || mode == "heat") return couleur_consigne(mode);
    if (mode == "off") return UIColor.TEXT_DIM;
    if (mode == "unavailable" || mode == "unknown") return UIColor.INACTIVE;
    return UIColor.SUCCESS;
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
    // « crpR » / « cepR » : la clim de la pièce R (ADR-0040) ; « crRT » / « ceRT » : celle
    // de la tuile tRT. Un firmware d'avant l'ADR-0040 ignore « p » (chiffre attendu).
    const bool piece = cle[2] == kPrefixePiece;
    if (piece ? (cle[3] < '0' || cle[3] > '4') : (cle[2] < '0' || cle[2] > '4' || cle[3] < '0' || cle[3] > '4'))
        return false;
    const int r = piece ? cle[3] - '0' : cle[2] - '0';
    const int t = piece ? -1 : cle[3] - '0';
    const int i = piece ? kCasesTuiles + r : r * kTuiles + t;
    ClimTuile* c = case_clim(i, true);
    if (c == nullptr) return true;  // pas de mémoire : ignorée (journal de case_clim)
    vue_verifier();
    const bool affichee = vue_tuile() && s_vue == i;
    if (reglages) {
        // Défauts de la 3.2 pour une clim encore inconnue, ses derniers réglages sinon.
        ClimReglages lu = c->reglages.recu ? c->reglages : ClimReglages{};
        const int k = lire_reglages(reste, n_reste, lu);
        if (k < 5) {
            ESP_LOGW("tab5.clim", "Reglages de la clim %.4s illisibles (%d champs) : ignores", cle, k);
            return true;
        }
        c->reglages = lu;
        ESP_LOGI("tab5.clim", "Reglages de la clim %.4s : %.1f-%.1f, pas %.2f, %s, [%s]", cle,
                 lu.min, lu.max, lu.pas, lu.fahrenheit ? "F" : "C", lu.capacites);
        // Tuile : son bouton apparaît (l'appui ouvre le popup). Pièce : la tuile − / + peut
        // la régler.
        if (piece) reglables_clim_changee();
        else tuiles_repeindre(r, t);
        if (affichee) popup_peindre();
        return true;
    }
    clim_etat_lire(reste, n_reste, c->etat);  // Tab5/socle/tab5_parse.cpp (lot F)
    if (affichee) {
        popup_consigne_ui(c->etat.consigne, true);
        popup_piece_ui(c->etat.piece);
        clim_recolorer();
    }
    if (piece) reglables_clim_changee();  // sa consigne sur la tuile − / +
    return true;
}

bool clim_tuile_connue(int r, int t) {
    const ClimTuile* c = tuile_clim(r, t, false);
    return c != nullptr && c->reglages.recu;
}

bool clim_afficher_tuile(int r, int t) {
    if (!clim_tuile_connue(r, t)) return false;
    s_vue = r * kTuiles + t;
    std::memcpy(s_vue_cle, modele_ha::tuile_cle(r, t).s, sizeof(s_vue_cle));
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
    // Une consigne en attente irait à l'appareil qui a pris sa place : elle ne part pas.
    if (std::strcmp(s_attente.cle, modele_ha::tuile_cle(r, t).s) == 0) s_attente = Attente{};
    if (s_vue != r * kTuiles + t) return;
    if (popup_visible()) animate_popup_close(g_clim_ui.popup);
    s_vue = -1;
    popup_peindre();
}

bool clim_piece_connue(int r) {
    const ClimTuile* c = piece_clim(r, false);
    return c != nullptr && c->reglages.recu;
}

bool clim_afficher_piece(int r) {
    if (!clim_piece_connue(r)) return false;
    s_vue = kCasesTuiles + r;
    cle_piece(r, s_vue_cle);
    popup_peindre();
    return true;
}

void clim_piece_pas(int r, int sens) {
    ClimTuile* c = piece_clim(r, false);
    if (c == nullptr || !c->reglages.recu || std::isnan(c->etat.consigne)) return;  // inconnue : rien
    const float v = consigne_suivante(c->reglages, c->etat.consigne, sens);
    c->etat.consigne = v;
    // Même chemin que le popup sur une clim de tuile : clé et valeur gardées pour
    // tab5_debounce_clim_tuile, qui envoie « cpR » / consigne 250 ms après le dernier appui.
    cle_piece(r, s_attente.cle);
    s_attente.valeur = v;
    if (vue_tuile() && s_vue == kCasesTuiles + r) popup_consigne_ui(v, false);
    if (g_clim_ui.debounce_tuile != nullptr) g_clim_ui.debounce_tuile();
    reglables_clim_changee();
}

void clim_piece_oublier(int r) {
    ClimTuile* c = piece_clim(r, false);
    if (c == nullptr) return;
    *c = ClimTuile();
    char cle[4];
    cle_piece(r, cle);
    // Une consigne en attente irait à une clim que la pièce n'a plus : elle ne part pas.
    if (std::strcmp(s_attente.cle, cle) == 0) s_attente = Attente{};
    if (s_vue != kCasesTuiles + r) return;
    if (popup_visible()) animate_popup_close(g_clim_ui.popup);
    s_vue = -1;
    popup_peindre();
}

uint32_t clim_piece_carte(int r, char* buf, size_t n, uint32_t& couleur_valeur) {
    const ClimTuile* c = piece_clim(r, false);
    static const std::string kSansMode;
    const ClimReglages& g = c != nullptr ? c->reglages : s_clim;
    clim_format_consigne(g, buf, n, c != nullptr ? c->etat.consigne : NAN);
    const std::string& mode = c != nullptr ? c->etat.mode : kSansMode;
    couleur_valeur = couleur_consigne(mode);
    return couleur_icone_carte(mode);
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
    champ_vue(ChampClim::MODE) = mode;
    clim_recolorer();
    reglables_clim_changee();  // l'icône de la clim sur la tuile − / + prend la couleur du mode
}

// Bascules : actif sous tous ses noms (clim_*_actif) → retour à none / auto / stop ; sinon
// le nom de la Daikin (away, boost, quiet, swing, windnice), que le blueprint traduit.
void clim_popup_preset(const char* bouton) {
    std::string& p = champ_vue(ChampClim::PRESET);
    p = clim_preset_actif(p, bouton) ? std::string("none") : std::string(bouton);
    clim_recolorer();
}

void clim_popup_silence() {
    std::string& f = champ_vue(ChampClim::VENTILATION);
    f = clim_silence_actif(f) ? std::string("auto") : std::string("quiet");
    clim_recolorer();
}

void clim_popup_oscillation() {
    std::string& s = champ_vue(ChampClim::OSCILLATION);
    s = clim_oscillation_actif(s) ? std::string("stop") : std::string("swing");
    clim_recolorer();
}

void clim_popup_brise() {
    std::string& s = champ_vue(ChampClim::OSCILLATION);
    s = (s == "windnice") ? std::string("stop") : std::string("windnice");
    clim_recolorer();
}

const std::string& clim_affichee_preset() { return champ_vue(ChampClim::PRESET); }
const std::string& clim_affichee_ventilation() { return champ_vue(ChampClim::VENTILATION); }
const std::string& clim_affichee_oscillation() { return champ_vue(ChampClim::OSCILLATION); }

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
    return couleur_icone_carte(mode);
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
    const std::string& mode = champ_vue(ChampClim::MODE);
    const std::string& preset = champ_vue(ChampClim::PRESET);
    const std::string& fan = champ_vue(ChampClim::VENTILATION);
    const std::string& swing = champ_vue(ChampClim::OSCILLATION);
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

// Roue d'actions rapides (ADR-0036, tab5_tuiles_roue.cpp) : les modes qu'elle offre sont ceux que
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
