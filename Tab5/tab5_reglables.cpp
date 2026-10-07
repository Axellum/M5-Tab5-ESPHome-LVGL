/**
 * [AI-CONTEXT]
 * @file tab5_reglables.cpp
 * @role Tuile − / + au choix (ADR-0033, 06/10/2026, demande d'Axel) : les boutons − / +
 *       de la carte clim de l'accueil (climate_card.yaml) règlent l'appareil choisi dans
 *       une liste qui se déroule au toucher de la température du salon :
 *         - la clim du blueprint, en tête quand elle existe : rien ne change pour elle
 *           (consigne clim_target, globals clim_*, popup clim au toucher de la valeur) ;
 *         - jusqu'à huit appareils choisis dans le blueprint (clés rN de tab5_maj_tuiles,
 *           états rN de tab5_maj_emplacements) : volume d'un lecteur, luminosité d'une
 *           lampe, consigne d'une clim ou d'un chauffe-eau, humidité, vitesse d'un
 *           ventilateur, position d'un volet ou d'une vanne, valeur d'un nombre ;
 *         - le volume de la tablette, en dernier (local : tab5_volume_apply).
 *       Le choix reste fixe jusqu'au suivant, même après un redémarrage (NVS « RGC1 »),
 *       reconnu par le type et le nom de l'appareil plutôt que par sa place : réordonner
 *       la liste dans le blueprint ne change pas d'appareil.
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
#include "tab5_tuiles_icones.h"
#include "lvgl.h"
#include <esp_attr.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

ReglablesUI g_reglables_ui;

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

constexpr size_t kNom = 25;     // nom affiché : 24 octets au plus, comme une tuile
constexpr size_t kIcone = 16;   // code de palette [a-z0-9_]{1,15}
constexpr size_t kUnite = 8;    // unité : 7 octets au plus
constexpr size_t kEtat = 16;    // état HA tel quel

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
}

bool est(const char* s, const char* mot) { return std::strcmp(s, mot) == 0; }

bool hors_ligne(const Etat& e) { return !e.recu || est(e.brut, "unavailable") || est(e.brut, "unknown"); }

int nb_ha() {
    int n = 0;
    for (const Def& d : s_m.d)
        if (d.type != static_cast<uint8_t>(Type::VIDE)) n++;
    return n;
}

bool clim_presente() { return !zone_absente(Zone::CLIM); }

// La tuile − / + existe avec une clim ou au moins un appareil du blueprint ; sans rien
// des deux, elle reste masquée comme avant (le volume de la tablette seul ne la montre pas).
bool tuile_visible() { return clim_presente() || nb_ha() > 0; }

// Entrées de la liste, dans l'ordre affiché : clim, appareils du blueprint, tablette.
enum class Sorte : uint8_t { CLIM, HA, TABLETTE };
struct Entree {
    Sorte sorte;
    int8_t i;  // case de s_m.d (HA)
};

int lister(Entree out[kReglablesLignes]) {
    int n = 0;
    if (clim_presente()) out[n++] = {Sorte::CLIM, -1};
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

void vue(const Entree& en, Vue& v) {
    v.couleur_valeur = UIColor.TEXT_PRIMARY;
    v.nom[0] = '\0';
    switch (en.sorte) {
        case Sorte::CLIM: {
            v.icone = tuile_icone("clim", true, nullptr);
            v.couleur_icone = clim_carte_valeur(v.valeur, sizeof(v.valeur), v.couleur_valeur);
            if (!est(v.valeur, "--")) {
                const size_t k = std::strlen(v.valeur);
                snprintf(v.valeur + k, sizeof(v.valeur) - k, " \xC2\xB0");
            }
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
            // blueprint (couleur_consigne, tab5_cards.cpp).
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
constexpr int32_t kLargeurNom = 290;
constexpr int32_t kLargeurValeurLigne = 120;
// Valeur sur la carte : entre − (finit à 75) et + (commence à 330), moins l'icône (45)
// et son écart (8).
constexpr int32_t kLargeurValeurCarte = 190;

void peindre_carte() {
    const ReglablesUI& u = g_reglables_ui;
    if (u.zone == nullptr) return;
    ui_hidden(u.zone, !tuile_visible());
    const Entree en = entree_choisie();
    const bool clim = en.sorte == Sorte::CLIM;
    ui_hidden(u.consigne_clim, !clim);
    ui_hidden(u.rangee, clim);
    if (clim) return;
    Vue v;
    vue(en, v);
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

struct Champ {
    const char* p;
    size_t n;
};

// Champs séparés par '|', au plus `max` (le dernier prend le reste).
int decouper(const char* s, size_t n, Champ* out, int max) {
    int k = 0;
    size_t debut = 0;
    for (size_t i = 0; i <= n && k < max; i++) {
        if (i == n || (s[i] == '|' && k < max - 1)) {
            out[k++] = {s + debut, i - debut};
            debut = i + 1;
        }
    }
    return k;
}

float nombre(const Champ& c, float defaut) {
    char tmp[24];
    if (c.n == 0 || c.n >= sizeof(tmp)) return defaut;
    std::memcpy(tmp, c.p, c.n);
    tmp[c.n] = '\0';
    char* bout = nullptr;
    const float v = strtof(tmp, &bout);
    return (bout == tmp || !std::isfinite(v)) ? defaut : v;
}

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
    size_t j = 0;
    for (size_t i = 0; i < f[1].n && j < kIcone - 1; i++) {
        const char c = f[1].p[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_') d.icone[j++] = c;
    }
    d.tv = std::memchr(f[2].p, 't', f[2].n) != nullptr ? 1 : 0;
    if (f[3].n == 3 && f[3].p[0] == 't' && f[3].p[1] >= '0' && f[3].p[1] <= '4' && f[3].p[2] >= '0' &&
        f[3].p[2] <= '4') {
        d.lien_r = static_cast<int8_t>(f[3].p[1] - '0');
        d.lien_t = static_cast<int8_t>(f[3].p[2] - '0');
    }
    d.min = nombre(f[4], 0.0f);
    d.max = nombre(f[5], 100.0f);
    d.pas = nombre(f[6], 1.0f);
    if (!(d.max > d.min)) d.max = d.min + 1.0f;
    if (!(d.pas > 0.0f)) d.pas = 1.0f;
    texte_ha_copier(d.unite, kUnite, f[7].p, f[7].n);
    texte_ha_copier(d.nom, kNom, f[8].p, f[8].n);
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
    e.valeur = k > 1 ? nombre(f[1], NAN) : NAN;
    // Un geste en cours sur cet appareil : la valeur affichée reste la sienne (l'état qui
    // arrive est celui d'avant le geste), comme la consigne de la clim.
    if (s_attente.cle[0] == 'r' && s_attente.cle[1] == '0' + i) e.valeur = s_attente.valeur;
    e.recu = true;
    repeindre();
    return true;
}

void reglables_appliquer_ui() {
    charger();
    repeindre();
}

bool reglables_clim_choisie() {
    charger();
    return entree_choisie().sorte == Sorte::CLIM;
}

void reglables_pas(int sens) {
    charger();
    const ReglablesUI& u = g_reglables_ui;
    const Entree en = entree_choisie();
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
    const Entree en = entree_choisie();
    if (en.sorte != Sorte::HA) return;  // la clim : son popup (climate_card.yaml) ; la tablette : rien
    const Def& d = s_m.d[en.i];
    // Le popup de la tuile qui porte la même entité (ce que fait son appui long, ou son
    // appui pour une clim) ; sinon la télécommande de la TV du blueprint.
    if (d.lien_r >= 0 && tuile_ouvrir_popup(d.lien_r, d.lien_t)) return;
    if (d.tv && g_tuiles_ui.popup_tv != nullptr) animate_popup_open(g_tuiles_ui.popup_tv);
}

void reglables_liste_basculer() {
    charger();
    const ReglablesUI& u = g_reglables_ui;
    if (u.liste == nullptr) return;
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
    const uint32_t id = identite(l[ligne]);
    if (id != s_choix) {
        s_choix = id;
        Choix c{kMagicChoix, s_choix};
        s_pref_choix.save(&c);
        ESP_LOGI("tab5.reglables", "Tuile -/+ : ligne %d choisie", ligne);
    }
    reglables_liste_fermer();
    peindre_carte();
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
