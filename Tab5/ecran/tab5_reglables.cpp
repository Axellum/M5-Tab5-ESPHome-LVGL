/**
 * [AI-CONTEXT]
 * @file tab5_reglables.cpp
 * @role Tuile − / + au choix (ADR-0033, 06/10/2026, demande d'Axel) : les boutons − / +
 *       de la carte clim de l'accueil (climate_card.yaml) règlent l'appareil choisi dans
 *       une liste qui se déroule à l'appui long sur la valeur entre − et + (toucher la
 *       température du salon jusqu'au 09/10/2026, devenu le carrousel des clims, ADR-0038) :
 *         - la clim du blueprint, en tête quand elle existe : rien ne change pour elle
 *           (consigne clim_target, globals clim_*, popup clim au toucher de la valeur) ;
 *         - jusqu'à huit appareils choisis dans le blueprint (clés rN de tab5_maj_tuiles,
 *           états rN de tab5_maj_emplacements) : volume d'un lecteur, luminosité d'une
 *           lampe, consigne d'une clim ou d'un chauffe-eau, humidité, vitesse d'un
 *           ventilateur, position d'un volet ou d'une vanne, valeur d'un nombre ;
 *         - le volume de la tablette, en dernier (local : tab5_volume_apply).
 *       Le choix reste fixe jusqu'au suivant, même après un redémarrage (NVS « RGC1 »),
 *       reconnu par le type et le nom de l'appareil plutôt que par sa place : réordonner
 *       la liste dans le blueprint ne change pas d'appareil. Défilement au choix (lot 3,
 *       09/10/2026, ADR-0041, clé defil) : « fixe » par défaut (ce qui précède) ; en
 *       « auto », la clim et les appareils du blueprint se relaient seuls, calés sur la
 *       carte centrale (reglables_tour), sans écrire la NVS ; un geste remet le compte à
 *       zéro.
 *       La ligne « Clim » est la clim « d'ici » (clim_ref_ici, tab5_clim.cpp ; 14/10/2026,
 *       discussion #278, décision d'Axel) : la seule de la maison quand il n'y en a qu'une,
 *       sur toutes les pièces ; avec plusieurs, celle de la pièce affichée en mode HA (sa
 *       clim propre, ADR-0040, sinon la première de ses tuiles cli), sinon celle du
 *       blueprint, sinon pas de ligne « Clim » sur cette pièce. Une clim de tuile ou de
 *       pièce se peint comme un appareil (vue_clim_ref), − / + par clim_ref_pas(), la valeur
 *       ouvre le carrousel sur elle ; celle du blueprint garde son chemin (clim_target). Un
 *       autre appareil choisi reste choisi sur toutes les pièces (même NVS « RGC1 »).
 *       Jusqu'au 14/10/2026, la clim propre de la pièce passait avant le choix : la liste
 *       s'ouvrait mais choisir un volet ou le son de la tablette n'y changeait rien.
 *       Toucher la valeur : le popup de la tuile qui porte la même entité, sinon la
 *       télécommande (TV), sinon, pour un volet ou une vanne, sa roue Ouvrir / Stop /
 *       Fermer / Position (16/10/2026, discussion #278, amendement de l'ADR-0033).
 * @architecture_constraint Push-only et événements seuls (ADR-0001, ADR-0025) : la
 *       tablette envoie esphome.tab5_action (rN / regler / valeur, ou consigne pour une
 *       clim) ; le blueprint borne la valeur et choisit l'action par le domaine de
 *       l'entité, qu'il ne commande que si elle est dans sa liste. − / + : la valeur
 *       affichée change tout de suite (optimiste), un seul envoi par geste (script
 *       tab5_debounce_reglable, 250 ms, comme la clim) ; la clé, la commande et la
 *       valeur sont prises au geste, et un changement d'appareil envoie d'abord ce qui
 *       attend. Définitions en NVS (« REG1 ») : la tuile montre le bon appareil dès le
 *       démarrage ; les états attendent HA (« -- »).
 * @ai_instruction Le modèle (types, champs, bornes) est celui du blueprint
 *       « Tab5 — emplacements » (types_reglables, reglables_bornes) :
 *       tests/test_reglables.py compare. Les icônes sont celles de la palette des tuiles
 *       (tuile_icone, mdi_font_45) ; MDI_CODE_TARGETS (tools/check_tab5_code_rules.py)
 *       rattache reglable_icone et reglable_ligne_*_icone à la palette. Texte affiché :
 *       tr(). Aucune couleur en dur : UIColor.
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include "tab5_tuiles_icones.h"
#include "tab5_modele_ha.h"
#include "lvgl.h"
#include <esp_attr.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

ReglablesUI g_reglables_ui;

// kNom, kIcone, kEtat, est(), etat_indisponible(), copier_icone() : communs avec les
// tuiles de pièce (tab5_modele_ha.h, lot L5).
using namespace modele_ha;

namespace {

constexpr int kHA = 8;  // appareils du blueprint (« huit au plus »)
static_assert(kReglablesLignes == kHA + 2, "clim + appareils du blueprint + tablette");

// Types de la clé rN (tableau de l'ADR-0033 ; types_reglables du blueprint).
enum class Type : uint8_t { VIDE, SON, LUM, CLI, EAU, HUM, VEN, VOL, NBR };
constexpr const char* kTypes[] = {"", "son", "lum", "cli", "eau", "hum", "ven", "vol", "nbr"};
constexpr int kNbTypes = sizeof(kTypes) / sizeof(kTypes[0]);
// Icône de chaque type quand HA n'en donne pas une de la palette.
constexpr const char* kIconesDefaut[kNbTypes] = {"etat",           "enceinte",    "ampoule",
                                                  "clim",           "chauffe_eau", "humidificateur",
                                                  "ventilateur",    "volet",       "mesure"};

constexpr size_t kUnite = 8;    // unité : 7 octets au plus (kNom, kIcone, kEtat : tab5_modele_ha.h)

// Définition d'un appareil. Pas d'initialiseur de membre : les structures sont remises
// à zéro par memset (octets de bourrage compris), pour que memcmp ne voie jamais une
// différence qui n'en est pas une (écriture NVS pour rien).
struct Def {
    uint8_t type;      // Type ; VIDE = case libre
    uint8_t tv;        // option t : la TV du blueprint (sa valeur ouvre la télécommande)
    int8_t lien_r;     // tuile tRT qui porte la même entité (-1 : aucune)
    int8_t lien_t;
    float min;
    float max;
    float pas;
    char icone[kIcone];
    char unite[kUnite];
    char nom[kNom];
};

struct Modele {
    uint32_t magic;
    Def d[kHA];
};

struct Etat {
    char brut[kEtat];
    float valeur;  // NaN : inconnue (« -- » ; − / + ne font rien)
    bool recu;
};

struct Choix {
    uint32_t magic;
    uint32_t id;  // identite() de l'appareil choisi
};

constexpr uint32_t kMagic = 0x52454731;       // « REG1 »
constexpr uint32_t kPrefKey = 0x7265676C;     // « regl »
constexpr uint32_t kMagicChoix = 0x52474331;  // « RGC1 »
constexpr uint32_t kPrefKeyChoix = 0x72676368;  // « rgch »
constexpr uint32_t kIdClim = 1;
constexpr uint32_t kIdTablette = 2;

// Volume de la tablette : en %, pas de 5.
constexpr float kPasTablette = 5.0f;

EXT_RAM_BSS_ATTR Modele s_m;
EXT_RAM_BSS_ATTR Etat s_etats[kHA];
uint32_t s_choix = kIdClim;
// Le choix gardé en NVS : celui d'un geste. Le défilement « auto » (reglables_tour) change
// s_choix sans l'écrire (une écriture par tour userait la flash) : au démarrage, la tuile
// revient au dernier appareil choisi à la main.
uint32_t s_choix_sauve = kIdClim;
// Tours de la carte centrale depuis le dernier changement d'appareil (défilement « auto ») ;
// un geste sur la tuile le remet à zéro.
int s_tours = 0;
bool s_charge = false;
esphome::ESPPreferenceObject s_pref;
esphome::ESPPreferenceObject s_pref_choix;

// Valeur en attente du débounce : clé, commande et valeur prises au geste.
struct Attente {
    char cle[4];
    char commande[10];
    float valeur;
};
Attente s_attente = {"", "", NAN};

// ─── Modèle ─────────────────────────────────────────────────────────────────────────

void etat_vider(Etat& e) {
    std::memset(&e, 0, sizeof(e));
    e.valeur = NAN;
}

void charger() {
    if (s_charge) return;
    s_charge = true;
    for (Etat& e : s_etats) etat_vider(e);
    s_pref = esphome::global_preferences->make_preference<Modele>(kPrefKey);
    Modele m;
    std::memset(&m, 0, sizeof(m));
    if (s_pref.load(&m) && m.magic == kMagic) {
        s_m = m;
        ESP_LOGI("tab5.reglables", "Appareils de la tuile -/+ relus de la NVS");
    } else {
        // Cases vides comme les écrit reglables_definir() (sans lien : -1), pour que le
        // premier instantané sans clé rN ne diffère pas (pas d'écriture NVS pour rien).
        std::memset(&s_m, 0, sizeof(s_m));
        s_m.magic = kMagic;
        for (Def& d : s_m.d) d.lien_r = d.lien_t = -1;
    }
    s_pref_choix = esphome::global_preferences->make_preference<Choix>(kPrefKeyChoix);
    Choix c{};
    if (s_pref_choix.load(&c) && c.magic == kMagicChoix && c.id != 0) s_choix = c.id;
    s_choix_sauve = s_choix;
}

bool hors_ligne(const Etat& e) { return !e.recu || etat_indisponible(e.brut); }

int nb_ha() {
    int n = 0;
    for (const Def& d : s_m.d)
        if (d.type != static_cast<uint8_t>(Type::VIDE)) n++;
    return n;
}

// La clim d'ici (clim_ref_ici, tab5_clim.cpp) : la ligne « Clim » de la liste existe.
bool clim_ici() {
    ClimRef c;
    return clim_ref_ici(c);
}

// La tuile − / + existe avec une clim d'ici ou au moins un appareil du blueprint ; sans rien
// des deux, elle reste masquée comme avant (le volume de la tablette seul ne la montre pas).
bool tuile_visible() { return clim_ici() || nb_ha() > 0; }

// Entrées de la liste, dans l'ordre affiché : clim, appareils du blueprint, tablette.
enum class Sorte : uint8_t { CLIM, HA, TABLETTE };
struct Entree {
    Sorte sorte;
    int8_t i;  // case de s_m.d (HA)
};

int lister(Entree out[kReglablesLignes]) {
    int n = 0;
    if (clim_ici()) out[n++] = {Sorte::CLIM, -1};
    for (int i = 0; i < kHA; i++)
        if (s_m.d[i].type != static_cast<uint8_t>(Type::VIDE)) out[n++] = {Sorte::HA, static_cast<int8_t>(i)};
    out[n++] = {Sorte::TABLETTE, -1};
    return n;
}

// Identité d'une entrée, gardée en NVS : 1 la clim, 2 la tablette ; un appareil du
// blueprint, son type, son nom et son rang parmi ceux de même type et même nom (deux
// « Lampe » restent deux choix) — FNV-1a, bit fort levé pour ne jamais valoir 1 ou 2.
uint32_t identite(const Entree& e) {
    if (e.sorte == Sorte::CLIM) return kIdClim;
    if (e.sorte == Sorte::TABLETTE) return kIdTablette;
    const Def& d = s_m.d[e.i];
    uint8_t rang = 0;
    for (int i = 0; i < e.i; i++)
        if (s_m.d[i].type == d.type && std::strcmp(s_m.d[i].nom, d.nom) == 0) rang++;
    uint32_t h = 2166136261u;
    auto melanger = [&h](uint8_t c) { h = (h ^ c) * 16777619u; };
    melanger(d.type);
    for (const char* p = d.nom; *p != '\0'; p++) melanger(static_cast<uint8_t>(*p));
    melanger(rang);
    return h | 0x80000000u;
}

// L'entrée choisie ; disparue (clim retirée, appareil enlevé du blueprint) : la première.
int choisie(const Entree* l, int n) {
    for (int k = 0; k < n; k++)
        if (identite(l[k]) == s_choix) return k;
    return 0;
}

Entree entree_choisie() {
    Entree l[kReglablesLignes];
    const int n = lister(l);
    return l[choisie(l, n)];
}

// La clim de tuile ou de pièce que tient la ligne « Clim » quand `en` est cette ligne
// (clim_ref_ici) ; faux pour un autre appareil et pour la clim du blueprint (sa consigne
// clim_target, ses globals clim_*, son popup par climate_card.yaml).
bool clim_autre(const Entree& en, ClimRef& c) { return en.sorte == Sorte::CLIM && clim_ref_ici(c) && c.r >= 0; }

Type type_de(const Def& d) {
    return d.type < kNbTypes ? static_cast<Type>(d.type) : Type::VIDE;
}

// ─── Ce qu'un appareil montre ───────────────────────────────────────────────────────

bool actif(const Def& d, const Etat& e) {
    if (hors_ligne(e)) return false;
    switch (type_de(d)) {
        case Type::SON: return !est(e.brut, "off") && !est(e.brut, "standby");
        case Type::LUM:
        case Type::VEN:
        case Type::HUM: return est(e.brut, "on");
        case Type::CLI:
        case Type::EAU: return !est(e.brut, "off");
        case Type::VOL: return !est(e.brut, "closed");
        default: return true;
    }
}

// Couleur de l'icône (et de la valeur d'une clim) selon le type et l'état, comme les
// tuiles : hors ligne grisé, éteint discret.
uint32_t couleur_icone(const Def& d, const Etat& e) {
    if (hors_ligne(e)) return UIColor.INACTIVE;
    if (!actif(d, e)) return UIColor.TEXT_DIM;
    switch (type_de(d)) {
        case Type::CLI:
            if (est(e.brut, "cool")) return UIColor.CLIM_COOL_ACTIVE;
            if (est(e.brut, "heat")) return UIColor.CLIM_HEAT_ACTIVE;
            return UIColor.SUCCESS;
        case Type::SON:
        case Type::VOL: return UIColor.SUCCESS;
        default: return UIColor.INFO;
    }
}

const char* glyphe(const Def& d, bool on) {
    const char* code = tuiles_icones::trouver(d.icone) != nullptr ? d.icone : kIconesDefaut[static_cast<int>(type_de(d))];
    return tuile_icone(code, on, nullptr);
}

bool unite_degres(const char* unite) { return std::strncmp(unite, "\xC2\xB0", 2) == 0; }

// « 35 % », « 21.5 ° », « 1.5 kW » ; « -- » sans valeur. % : sans décimale ; sinon les
// décimales du pas (0, 1 ou 2).
void formater(char* out, size_t n, float v, float pas, const char* unite) {
    if (std::isnan(v)) {
        snprintf(out, n, "--");
        return;
    }
    int dec = 2;
    if (est(unite, "%") || std::fabs(pas - std::round(pas)) < 0.001f) dec = 0;
    else if (std::fabs(pas * 10.0f - std::round(pas * 10.0f)) < 0.01f) dec = 1;
    if (unite[0] == '\0') snprintf(out, n, "%.*f", dec, v);
    else if (unite_degres(unite)) snprintf(out, n, "%.*f \xC2\xB0", dec, v);
    else snprintf(out, n, "%.*f %s", dec, v, unite);
}

// Un pas de plus (sens > 0) ou de moins, sur la grille du pas à partir du minimum
// (37 % → 40 % ou 30 %, pas 10), borné ; arrondi au centième (pas de « 21.499999 »).
float suivante(float v, float min, float max, float pas, int sens) {
    if (!(pas > 0.0f)) pas = 1.0f;
    const float k = (v - min) / pas;
    const float m = sens > 0 ? std::floor(k + 0.001f) + 1.0f : std::ceil(k - 0.001f) - 1.0f;
    float r = min + m * pas;
    r = std::round(r * 100.0f) / 100.0f;
    return std::max(min, std::min(max, r));
}

float volume_tablette_pct() {
    const float* v = g_reglables_ui.volume;
    return v != nullptr ? std::round(*v * 100.0f) : NAN;
}

// Icône, couleur et texte d'une entrée, pour la carte et pour la liste.
struct Vue {
    const char* icone;
    uint32_t couleur_icone;
    char valeur[24];
    uint32_t couleur_valeur;
    char nom[kNom + 24];
};

// Une clim de tuile ou de pièce (clim_autre) : son icône et sa consigne comme la clim du
// blueprint (clim_ref_carte), son nom comme dans la roue et le popup (clim_tete : celui de
// HA, sinon la pièce, sinon « Climatisation »).
void vue_clim_ref(const ClimRef& c, Vue& v) {
    v.icone = tuile_icone("clim", true, nullptr);
    v.couleur_icone = clim_ref_carte(c, v.valeur, sizeof(v.valeur), v.couleur_valeur);  // « 22.0 ° »
    ClimTete t;
    clim_tete(c.r, c.t, t);
    snprintf(v.nom, sizeof(v.nom), "%s", t.nom);
}

void vue(const Entree& en, Vue& v) {
    v.couleur_valeur = UIColor.TEXT_PRIMARY;
    v.nom[0] = '\0';
    switch (en.sorte) {
        case Sorte::CLIM: {
            ClimRef c;
            if (clim_autre(en, c)) {
                vue_clim_ref(c, v);
                return;
            }
            v.icone = tuile_icone("clim", true, nullptr);
            v.couleur_icone = clim_carte_valeur(v.valeur, sizeof(v.valeur), v.couleur_valeur);  // « 20.0 ° »
            const char* nom = clim_nom();
            snprintf(v.nom, sizeof(v.nom), "%s", (nom != nullptr && nom[0] != '\0') ? nom : tr("Climatisation"));
            return;
        }
        case Sorte::TABLETTE: {
            // Le son de la tablette : un haut-parleur, barré à 0 % ou en muet. « Tablette »
            // et l'icône d'une tablette ne disaient pas que c'était son volume (discussion
            // #278, 07/10/2026). « Son de la tablette » : « Volume de la tablette » ne tient
            // pas dans kLargeurNom en roboto_32_b (306 px pour 290).
            const float pct = volume_tablette_pct();
            const bool muet = g_reglables_ui.muet != nullptr && *g_reglables_ui.muet;
            v.icone = tuile_icone("enceinte", !muet && pct > 0.0f, nullptr);
            v.couleur_icone = UIColor.INFO;
            formater(v.valeur, sizeof(v.valeur), pct, kPasTablette, "%");
            snprintf(v.nom, sizeof(v.nom), "%s", tr("Son de la tablette"));
            return;
        }
        case Sorte::HA:
        default: {
            const Def& d = s_m.d[en.i];
            const Etat& e = s_etats[en.i];
            const bool on = actif(d, e);
            v.icone = glyphe(d, on);
            v.couleur_icone = couleur_icone(d, e);
            formater(v.valeur, sizeof(v.valeur), hors_ligne(e) ? NAN : e.valeur, d.pas, d.unite);
            // Une clim : sa consigne en bleu en froid, en rouge en chaud, comme celle du
            // blueprint (couleur_consigne, tab5_clim.cpp).
            if (hors_ligne(e)) v.couleur_valeur = UIColor.INACTIVE;
            else if (type_de(d) == Type::CLI && (est(e.brut, "cool") || est(e.brut, "heat")))
                v.couleur_valeur = v.couleur_icone;
            snprintf(v.nom, sizeof(v.nom), "%s", d.nom);
            return;
        }
    }
}

// ─── Dessin ─────────────────────────────────────────────────────────────────────────

// Largeur du nom dans une ligne de la liste (reglables_liste.yaml : panneau de 520 px,
// marges de 8, icône de 45 à 12 px du bord, valeur à droite) et de la valeur, à droite
// (de 492 jusqu'à 12 px après le nom). Une unité longue d'un number ne déborde pas.
// Sur une autre carte (lot 3 du portage 1024×600), le panneau et la valeur sont ramenés à
// son écran (acc_x, jeton acc_reglables_l) ; le nom garde son x (après l'icône de 45 px) et
// prend la place qui reste : 520 - 110 - 120 = 290 sur le Tab5.
constexpr int32_t kLargeurValeurLigne = acc_x(120);
constexpr int32_t kLargeurNom = acc_x(520) - 110 - kLargeurValeurLigne;
// Valeur sur la carte : entre − (finit à 75) et + (commence à 330), moins l'icône (45)
// et son écart (8) ; ramenée à l'écran de la carte comme la tuile.
constexpr int32_t kLargeurValeurCarte = acc_x(190);

// L'icône de la clim contre sa consigne (09/10/2026, lot A), à 8 px à sa gauche, le groupe
// centré dans la tuile (18/10/2026, demande d'Axel : la consigne seule au centre laissait
// l'icône et la consigne 27 px à gauche, serrées contre le − sur la 7B). align_to
// (climate_card.yaml) ne place qu'une fois ; la largeur de la consigne change avec son
// texte et la police du thème, alors les deux suivent chaque changement de taille de l'une
// ou de l'autre. lv_obj_align() reste en vigueur aux mises en page suivantes (centre de la
// tuile + décalage), même quand ce rappel tombe pendant une mise en page (la position de la
// consigne n'y est pas encore recalculée, sa largeur si). Déplacer ne change aucune taille :
// pas de nouveau rappel.
constexpr int32_t kEcartIconeConsigne = 8;
bool s_icone_suivie = false;

void consigne_icone_placer(lv_event_t*) {
    const ReglablesUI& u = g_reglables_ui;
    if (u.consigne_clim == nullptr || u.consigne_icone == nullptr) return;
    const int32_t w = lv_obj_get_width(u.consigne_clim);
    const int32_t wi = lv_obj_get_width(u.consigne_icone);
    // Groupe icône + écart + consigne centré : la consigne décalée à droite de la moitié de
    // l'icône et de l'écart (rien sans icône) ; bord droit de l'icône = x1 de la consigne − écart.
    const int32_t dx = wi > 0 ? (wi + kEcartIconeConsigne) / 2 : 0;
    lv_obj_align(u.consigne_clim, LV_ALIGN_CENTER, dx, 0);
    lv_obj_align(u.consigne_icone, LV_ALIGN_CENTER, dx - (w / 2) - kEcartIconeConsigne - wi + wi / 2, 0);
}

void consigne_icone_suivre() {
    const ReglablesUI& u = g_reglables_ui;
    if (s_icone_suivie || u.consigne_clim == nullptr || u.consigne_icone == nullptr) return;
    s_icone_suivie = true;
    lv_obj_add_event_cb(u.consigne_clim, consigne_icone_placer, LV_EVENT_SIZE_CHANGED, nullptr);
    lv_obj_add_event_cb(u.consigne_icone, consigne_icone_placer, LV_EVENT_SIZE_CHANGED, nullptr);
}

void peindre_carte() {
    const ReglablesUI& u = g_reglables_ui;
    if (u.zone == nullptr) return;
    consigne_icone_suivre();
    ui_hidden(u.zone, !tuile_visible());
    // La clim du blueprint choisie : sa consigne clim_target. Une clim de tuile ou de pièce
    // (la clim d'ici, clim_autre) ou un autre appareil : la rangée icône + valeur.
    const Entree en = entree_choisie();
    ClimRef c;
    const bool clim = en.sorte == Sorte::CLIM && !clim_autre(en, c);
    ui_hidden(u.consigne_clim, !clim);
    ui_hidden(u.consigne_icone, !clim);
    ui_hidden(u.rangee, clim);
    Vue v;
    vue(en, v);
    if (clim) {
        // La clim aussi a son icône, à gauche de sa consigne (09/10/2026, lot A : les
        // minutes passent d'un appareil à l'autre sans dérouler la liste, l'icône dit
        // lequel). La consigne elle-même reste peinte par tab5_clim.cpp (clim_target).
        ui_text(u.consigne_icone, v.icone);
        ui_text_color(u.consigne_icone, v.couleur_icone);
        return;
    }
    ui_text(u.icone, v.icone);
    ui_text_color(u.icone, v.couleur_icone);
    texte_ha_coupe(u.valeur, v.valeur, kLargeurValeurCarte);
    ui_text_color(u.valeur, v.couleur_valeur);
}

bool liste_ouverte() {
    const lv_obj_t* l = g_reglables_ui.liste;
    return l != nullptr && !lv_obj_has_flag(l, LV_OBJ_FLAG_HIDDEN);
}

void peindre_liste() {
    const ReglablesUI& u = g_reglables_ui;
    Entree l[kReglablesLignes];
    const int n = lister(l);
    const int sel = choisie(l, n);
    for (int k = 0; k < kReglablesLignes; k++) {
        const bool montree = k < n;
        ui_hidden(u.ligne[k], !montree);
        if (!montree) continue;
        Vue v;
        vue(l[k], v);
        ui_text(u.ligne_icone[k], v.icone);
        ui_text_color(u.ligne_icone[k], v.couleur_icone);
        texte_ha_coupe(u.ligne_nom[k], v.nom, kLargeurNom);
        ui_text_color(u.ligne_nom[k], k == sel ? UIColor.ACCENT : UIColor.TEXT_PRIMARY);
        texte_ha_coupe(u.ligne_valeur[k], v.valeur, kLargeurValeurLigne);
        ui_text_color(u.ligne_valeur[k], v.couleur_valeur);
        // Ligne choisie : un liseré d'accent (styles de la ligne, reglables_ligne.yaml),
        // réécrit seulement s'il change (un set de style invalide même à valeur égale).
        if (u.ligne[k] != nullptr) {
            const int32_t bord = k == sel ? 2 : 0;
            if (lv_obj_get_style_border_width(u.ligne[k], LV_PART_MAIN) != bord)
                lv_obj_set_style_border_width(u.ligne[k], bord, LV_PART_MAIN);
            const lv_color_t accent = lv_color_hex(UIColor.ACCENT);
            if (!lv_color_eq(lv_obj_get_style_border_color(u.ligne[k], LV_PART_MAIN), accent))
                lv_obj_set_style_border_color(u.ligne[k], accent, LV_PART_MAIN);
        }
    }
}

void repeindre() {
    peindre_carte();
    if (liste_ouverte()) peindre_liste();
}

// ─── Commandes ──────────────────────────────────────────────────────────────────────

void envoyer_attente() {
    if (s_attente.cle[0] == '\0') return;
    const Attente a = s_attente;
    s_attente.cle[0] = '\0';
    if (g_reglables_ui.envoyer != nullptr)
        g_reglables_ui.envoyer(a.cle, a.commande, clim_consigne_texte(a.valeur).c_str());
}

// Une entrée « rN|… » : N de 0 à 7.
bool cle_reglable(const char* cle, size_t n, int& i) {
    if (n != 2 || cle[0] != 'r' || cle[1] < '0' || cle[1] > '0' + kHA - 1) return false;
    i = cle[1] - '0';
    return true;
}

// Champs séparés par '|', au plus `max` (le dernier prend le reste) : Champ et
// champs_decouper_reste() de tab5_champs.h ; un nombre : champ_nombre().
int decouper(const char* s, size_t n, Champ* out, int max) { return champs_decouper_reste(s, n, '|', out, max); }

// « type|icône|options|lien|min|max|pas|unité|nom » (après « rN| ») dans `d`.
void lire_def(const char* s, size_t n, Def& d) {
    Champ f[9];
    const int k = decouper(s, n, f, 9);
    std::memset(&d, 0, sizeof(d));
    d.lien_r = d.lien_t = -1;
    if (k < 9) return;  // illisible : case vide
    for (int t = 1; t < kNbTypes; t++)
        if (f[0].n == std::strlen(kTypes[t]) && std::strncmp(f[0].p, kTypes[t], f[0].n) == 0) d.type = t;
    if (d.type == 0) return;
    copier_icone(d.icone, f[1].p, f[1].n);
    d.tv = std::memchr(f[2].p, 't', f[2].n) != nullptr ? 1 : 0;
    // Lien « tRT » vers la tuile de pièce du même appareil : pièce R de 0 à 9 (ADR-0066 lot A),
    // élément T = 0 … 9 puis a … j (lot B).
    int lien_r = 0, lien_t = -1;
    if (f[3].n == 3 && f[3].p[0] == 't' && modele_ha::piece_index(f[3].p[1], lien_r) &&
        tuile_index(f[3].p[2], lien_t) && lien_t < kTuilesMax) {
        d.lien_r = static_cast<int8_t>(lien_r);
        d.lien_t = static_cast<int8_t>(lien_t);
    }
    d.min = champ_nombre(f[4], 0.0f);
    d.max = champ_nombre(f[5], 100.0f);
    d.pas = champ_nombre(f[6], 1.0f);
    if (!(d.max > d.min)) d.max = d.min + 1.0f;
    if (!(d.pas > 0.0f)) d.pas = 1.0f;
    texte_ha_copier(d.unite, kUnite, f[7].p, f[7].n);
    texte_ha_copier(d.nom, kNom, f[8].p, f[8].n);
}

// ─── Roue d'un volet qui n'est la tuile d'aucune pièce (16/10/2026, discussion #278) ──
//
// Un volet (une vanne) de la liste n'a de popup que par la tuile qui porte la même entité
// (lien) ; sans elle, toucher sa valeur ne faisait rien et seuls − / + le réglaient. Le
// toucher ouvre alors la roue d'actions (tab5_roue.cpp) : Ouvrir, Stop, Fermer comme celle
// d'une tuile vol, et « Position ▸ » (25, 50, 75 %) quand il en a une réglable (pas < 100,
// ADR-0033). Ni « Maison » ni « Détails » : il n'a pas de popup. Commandes rN / ouvrir,
// fermer, arreter (branche « ouvrir, fermer, arrêter » du blueprint, qui accepte les clés
// rN depuis ce jour) et rN / regler / position (celle de − / +). Aucune mise à jour
// optimiste, comme la roue d'une tuile.

// clang-format off
enum class RoueReg : uint8_t { OUVRIR, ARRETER, FERMER, POSITION };
// clang-format on
constexpr uint8_t kRouePositionsReg[] = {25, 50, 75};  // celles de la roue d'une tuile vol
constexpr int kRoueRegBoutons = 4;

struct RoueReglable {
    int i = -1;  // case de s_m.d
    int n = 0;
    RoueReg action[kRoueRegBoutons] = {};
};
RoueReglable s_rr;

bool roue_reglable_vol(int i) { return i >= 0 && i < kHA && type_de(s_m.d[i]) == Type::VOL; }

// Position réglable et connue (0-100) : de quoi marquer un choix et dessiner la jauge.
bool roue_position_connue(const Def& d, const Etat& e) {
    return d.pas < 100.0f && !hors_ligne(e) && !std::isnan(e.valeur) && e.valeur >= 0.0f && e.valeur <= 100.0f;
}

// Boutons du premier anneau du volet i (0 : ce n'est plus un volet). Ouvert jusqu'en haut
// (ou sans position) : Ouvrir marqué ; fermé : Fermer marqué.
int roue_reglable_composer(int i, RoueBouton b[kRoueRegBoutons], RoueReglable& rr) {
    rr.n = 0;
    if (!roue_reglable_vol(i)) return 0;
    const Def& d = s_m.d[i];
    const Etat& e = s_etats[i];
    auto ajouter = [&](RoueReg a, RoueIcone ic, RoueGenre g, bool courant) {
        b[rr.n] = RoueBouton{ic, g, courant, nullptr};
        rr.action[rr.n++] = a;
    };
    const bool partiel = !std::isnan(e.valeur) && e.valeur > 0.0f && e.valeur < 100.0f;
    ajouter(RoueReg::OUVRIR, RoueIcone::OUVRIR, RoueGenre::ACTION, !hors_ligne(e) && est(e.brut, "open") && !partiel);
    ajouter(RoueReg::ARRETER, RoueIcone::STOP, RoueGenre::ACTION, false);
    ajouter(RoueReg::FERMER, RoueIcone::FERMER, RoueGenre::ACTION, !hors_ligne(e) && est(e.brut, "closed"));
    if (roue_position_connue(d, e)) ajouter(RoueReg::POSITION, RoueIcone::POSITION, RoueGenre::FAMILLE, false);
    return rr.n;
}

void roue_reglable_envoyer(int i, const char* commande, const char* valeur) {
    if (g_reglables_ui.envoyer == nullptr) return;
    envoyer_attente();  // un − / + en attente part d'abord
    const char cle[3] = {'r', static_cast<char>('0' + i), '\0'};
    g_reglables_ui.envoyer(cle, commande, valeur);
}

void roue_reglable_choisir(int k) {
    charger();
    const RoueReglable rr = s_rr;
    if (k < 0 || k >= rr.n || !roue_reglable_vol(rr.i)) return;
    switch (rr.action[k]) {
        case RoueReg::OUVRIR: roue_reglable_envoyer(rr.i, "ouvrir", ""); return;
        case RoueReg::ARRETER: roue_reglable_envoyer(rr.i, "arreter", ""); return;
        case RoueReg::FERMER: roue_reglable_envoyer(rr.i, "fermer", ""); return;
        default: return;
    }
}

int roue_reglable_famille(int k, RoueChoix* c) {
    charger();
    const RoueReglable& rr = s_rr;
    if (k < 0 || k >= rr.n || rr.action[k] != RoueReg::POSITION || !roue_reglable_vol(rr.i)) return 0;
    const Def& d = s_m.d[rr.i];
    const Etat& e = s_etats[rr.i];
    if (!roue_position_connue(d, e)) return 0;  // perdue roue ouverte
    const long position = std::lround(e.valeur);
    int m = 0;
    for (uint8_t p : kRouePositionsReg) {
        c[m] = RoueChoix{};
        c[m].courant = position == p;
        snprintf(c[m].texte, sizeof(c[m].texte), "%u %%", static_cast<unsigned>(p));
        m++;
    }
    return m;
}

void roue_reglable_choisir_choix(int k, int j) {
    charger();
    RoueChoix c[kRoueChoix];
    const int m = roue_reglable_famille(k, c);
    if (j < 0 || j >= m) return;
    char valeur[8];
    snprintf(valeur, sizeof(valeur), "%u", static_cast<unsigned>(kRouePositionsReg[j]));
    roue_reglable_envoyer(s_rr.i, "regler", valeur);
}

void roue_reglable_rejouer();

// Moyeu (icône, valeur, nom, jauge de la position) et boutons du volet i, autour de la
// zone − / + (ancre basse de la roue d'une clim). Faux sans roue.
bool roue_reglable_peindre(int i, bool garder) {
    RoueBouton b[kRoueRegBoutons];
    RoueReglable rr;
    rr.i = i;
    if (roue_reglable_composer(i, b, rr) == 0 || g_reglables_ui.zone == nullptr) return false;
    Vue v;
    vue(Entree{Sorte::HA, static_cast<int8_t>(i)}, v);
    const Def& d = s_m.d[i];
    const Etat& e = s_etats[i];
    RoueTete tete;
    tete.icone = v.icone;
    tete.valeur = v.valeur;
    tete.nom = v.nom;
    tete.couleur = v.couleur_icone;
    tete.jauge = roue_position_connue(d, e) ? static_cast<int>(std::lround(e.valeur)) : -1;
    RoueRappels rappels;
    rappels.choisir = roue_reglable_choisir;
    rappels.famille = roue_reglable_famille;
    rappels.choisir_choix = roue_reglable_choisir_choix;
    rappels.rejouer = roue_reglable_rejouer;
    s_rr = rr;
    return roue_ouvrir(clim_ancre_basse(g_reglables_ui.zone), tete, b, rr.n, rappels, garder);
}

// Thème, état ou définitions changés roue ouverte : la même roue repeinte ; plus un volet,
// elle se ferme.
void roue_reglable_rejouer() {
    charger();
    if (!roue_reglable_peindre(s_rr.i, true)) roue_actions_fermer();
}

// Un état ou des définitions reçus : la roue ouverte sur un volet de la liste suit.
void roue_reglable_suivre() {
    if (roue_ouverte_par(roue_reglable_rejouer)) roue_reglable_rejouer();
}

}  // namespace

// ─── API (tab5_custom.h, tab5_internal.h) ───────────────────────────────────────────

bool reglables_definir(const std::string& payload) {
    charger();
    Modele neuf;
    std::memset(&neuf, 0, sizeof(neuf));
    neuf.magic = kMagic;
    for (Def& d : neuf.d) d.lien_r = d.lien_t = -1;
    size_t debut = 0;
    while (debut < payload.size()) {
        size_t fin = payload.find(';', debut);
        if (fin == std::string::npos) fin = payload.size();
        const size_t p1 = payload.find('|', debut);
        int i = 0;
        if (p1 != std::string::npos && p1 < fin && cle_reglable(payload.data() + debut, p1 - debut, i))
            lire_def(payload.data() + p1 + 1, fin - p1 - 1, neuf.d[i]);
        debut = fin + 1;
    }
    // Les cases vides reprennent leurs octets par défaut (lire_def a pu en écrire).
    for (Def& d : neuf.d)
        if (d.type == 0) {
            std::memset(&d, 0, sizeof(d));
            d.lien_r = d.lien_t = -1;
        }
    if (std::memcmp(&neuf, &s_m, sizeof(Modele)) == 0) return false;
    // Un appareil qui change repart sans état (celui reçu était l'ancien) ; une valeur en
    // attente pour lui ne part pas.
    for (int i = 0; i < kHA; i++)
        if (std::memcmp(&neuf.d[i], &s_m.d[i], sizeof(Def)) != 0) {
            etat_vider(s_etats[i]);
            if (s_attente.cle[0] == 'r' && s_attente.cle[1] == '0' + i) s_attente.cle[0] = '\0';
        }
    s_m = neuf;
    s_pref.save(&s_m);
    ESP_LOGI("tab5.reglables", "Tuile -/+ : %d appareil(s) du blueprint", nb_ha());
    repeindre();
    roue_reglable_suivre();
    return true;
}

bool reglables_etat_recu(const char* cle, size_t n_cle, const char* reste, size_t n_reste) {
    int i = 0;
    if (!cle_reglable(cle, n_cle, i)) return false;
    charger();
    Champ f[2];
    const int k = decouper(reste, n_reste, f, 2);
    Etat& e = s_etats[i];
    std::memset(e.brut, 0, sizeof(e.brut));
    if (k > 0) std::memcpy(e.brut, f[0].p, std::min(f[0].n, kEtat - 1));
    e.valeur = k > 1 ? champ_nombre(f[1], NAN) : NAN;
    // Un geste en cours sur cet appareil : la valeur affichée reste la sienne (l'état qui
    // arrive est celui d'avant le geste), comme la consigne de la clim.
    if (s_attente.cle[0] == 'r' && s_attente.cle[1] == '0' + i) e.valeur = s_attente.valeur;
    e.recu = true;
    repeindre();
    if (s_rr.i == i) roue_reglable_suivre();
    return true;
}

void reglables_appliquer_ui() {
    charger();
    repeindre();
}

bool reglables_clim_choisie() {
    charger();
    // Une clim de tuile ou de pièce (la clim d'ici) n'est pas celle du blueprint : − / + et
    // la consigne passent par reglables_pas() et reglables_valeur_appui().
    const Entree en = entree_choisie();
    ClimRef c;
    return en.sorte == Sorte::CLIM && !clim_autre(en, c);
}

void reglables_pas(int sens) {
    charger();
    s_tours = 0;  // un geste : l'appareil réglé reste une durée entière (défilement auto)
    const ReglablesUI& u = g_reglables_ui;
    const Entree en = entree_choisie();
    ClimRef c;
    if (clim_autre(en, c)) {
        envoyer_attente();  // un geste sur un appareil de la liste part d'abord
        clim_ref_pas(c, sens);  // repeint la tuile (reglables_clim_changee)
        return;
    }
    if (en.sorte == Sorte::TABLETTE) {
        const float v = volume_tablette_pct();
        if (std::isnan(v) || u.volume_regler == nullptr) return;
        // tab5_volume_apply : point d'entrée unique du volume (sliders, HA), il repeint
        // la carte par reglables_volume_tablette().
        u.volume_regler(suivante(v, 0.0f, 100.0f, kPasTablette, sens) / 100.0f);
        return;
    }
    if (en.sorte != Sorte::HA) return;
    const Def& d = s_m.d[en.i];
    Etat& e = s_etats[en.i];
    if (hors_ligne(e) || std::isnan(e.valeur)) return;  // valeur inconnue : rien à compter
    const float v = suivante(e.valeur, d.min, d.max, d.pas, sens);
    if (v == e.valeur) return;
    // Une valeur en attente pour un autre appareil part d'abord.
    if (s_attente.cle[0] != '\0' && !(s_attente.cle[1] == '0' + en.i)) envoyer_attente();
    e.valeur = v;
    s_attente.cle[0] = 'r';
    s_attente.cle[1] = static_cast<char>('0' + en.i);
    s_attente.cle[2] = '\0';
    snprintf(s_attente.commande, sizeof(s_attente.commande), "%s", type_de(d) == Type::CLI ? "consigne" : "regler");
    s_attente.valeur = v;
    peindre_carte();
    if (u.debounce != nullptr) u.debounce();
}

void reglables_envoyer_attente() {
    charger();
    envoyer_attente();
}

void reglables_valeur_appui() {
    charger();
    // Une clim de tuile ou de pièce (la clim d'ici) : le carrousel du popup clim, sur elle.
    const Entree en = entree_choisie();
    ClimRef c;
    if (clim_autre(en, c)) {
        clim_carrousel_ouvrir_sur(c);
        return;
    }
    if (en.sorte != Sorte::HA) return;  // la clim : son popup (climate_card.yaml) ; la tablette : rien
    const Def& d = s_m.d[en.i];
    // Le popup de la tuile qui porte la même entité (ce que fait son appui long, ou son
    // appui pour une clim) ; sinon la télécommande de la TV du blueprint ; sinon, un volet :
    // sa roue (16/10/2026, discussion #278).
    if (d.lien_r >= 0 && tuile_ouvrir_popup(d.lien_r, d.lien_t)) return;
    if (d.tv) {
        telecommande_ouvrir(0);  // la page de la TV (ADR-0056)
        return;
    }
    if (roue_reglable_vol(en.i)) roue_reglable_peindre(en.i, false);
}

void reglables_liste_basculer() {
    charger();
    const ReglablesUI& u = g_reglables_ui;
    if (u.liste == nullptr) return;
    s_tours = 0;
    if (liste_ouverte()) {
        reglables_liste_fermer();
        return;
    }
    if (!tuile_visible()) return;
    peindre_liste();
    ui_hidden(u.liste, false);
}

void reglables_liste_fermer() { ui_hidden(g_reglables_ui.liste, true); }

bool reglables_liste_ouverte() { return liste_ouverte(); }

void reglables_choisir(int ligne) {
    charger();
    Entree l[kReglablesLignes];
    const int n = lister(l);
    if (ligne < 0 || ligne >= n) return;
    // Un geste sur l'appareil d'avant part tout de suite, avant de changer.
    envoyer_attente();
    s_tours = 0;
    const uint32_t id = identite(l[ligne]);
    s_choix = id;
    if (id != s_choix_sauve) {
        s_choix_sauve = id;
        Choix c{kMagicChoix, id};
        s_pref_choix.save(&c);
        ESP_LOGI("tab5.reglables", "Tuile -/+ : ligne %d choisie", ligne);
    }
    reglables_liste_fermer();
    peindre_carte();
}

// Défilement « auto » de la tuile − / + (lot 3, 09/10/2026, ADR-0041) : appelé à chaque
// tour de la carte centrale (rangee_tour, écran allumé, aucune fenêtre ouverte). Tous les
// defilement_tours_clim() tours, l'appareil suivant de la liste — la clim et ceux du
// blueprint, pas le volume de la tablette (il se choisit à la main) —, sans écrire la NVS
// ni dérouler la liste. Rien tant que la liste est ouverte ou qu'une valeur attend son
// envoi : le compte repart, l'appareil qu'on règle ne s'en va pas sous le doigt. La ligne
// « Clim » du relais est la clim d'ici (clim_ref_ici) ; jusqu'au 14/10/2026, la tuile
// restait sur la clim propre de la pièce et le choix tournait sans se voir.
void reglables_tour() {
    if (!s_charge || !defilement_auto(Defilement::CLIM) || !tuile_visible() || liste_ouverte() ||
        s_attente.cle[0] != '\0') {
        s_tours = 0;
        return;
    }
    if (++s_tours < defilement_tours_clim()) return;
    s_tours = 0;
    Entree l[kReglablesLignes];
    const int n = lister(l);
    int relais = 0;
    for (int k = 0; k < n; k++)
        if (l[k].sorte != Sorte::TABLETTE) relais++;
    if (relais < 2) return;  // un seul appareil : rien à faire tourner
    int j = choisie(l, n);
    do {
        j = (j + 1) % n;
    } while (l[j].sorte == Sorte::TABLETTE);
    s_choix = identite(l[j]);
    peindre_carte();
}

void reglables_suivant() {
    charger();
    if (!tuile_visible()) return;  // tuile masquée : rien à régler
    Entree l[kReglablesLignes];
    const int n = lister(l);
    // Même chemin qu'un toucher de ligne : la valeur en attente part, le choix va en NVS,
    // la liste (ouverte ou non) est refermée.
    reglables_choisir((choisie(l, n) + 1) % n);
}

void reglables_volume_tablette() {
    if (!s_charge) return;  // avant le premier dessin : zones_apply_ui peindra
    // Appelé à chaque cran d'un slider de volume : ne repeindre que ce qui le montre.
    if (entree_choisie().sorte == Sorte::TABLETTE) peindre_carte();
    if (liste_ouverte()) peindre_liste();
}

void reglables_clim_changee() {
    if (!s_charge) return;
    repeindre();
}

void reglables_rejouer_theme() {
    if (!s_charge) return;
    repeindre();
}
