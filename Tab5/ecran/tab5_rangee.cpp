/**
 * [AI-CONTEXT]
 * @file tab5_rangee.cpp
 * @role Zones à lignes de l'accueil : la rangée sous l'horloge (ADR-0031, 06/10/2026) et,
 *       depuis le lot 3 (09/10/2026, ADR-0041), le panneau « Ok Nabu ». Le même dessin
 *       et la même rotation pour les deux (struct Defileur, une par RangeeZone) : jusqu'à
 *       trois lignes qui se relaient.
 *         - La ligne spéciale : sous l'horloge, celle des plantes (moisture_sensors.yaml,
 *           inchangée : les quatre pots les plus secs, tab5_cards.cpp ; absente sans pot,
 *           zones, ADR-0018) ; dans le panneau, celle de l'écoute (« Ok Nabu: ON / OFF »,
 *           lbl_ok_nabu, peint par assist_wake_word_indicator_ui). À la place que HA lui
 *           donne (1re par défaut) ou masquée.
 *         - Des lignes de quatre éléments au plus, décrits par HA comme des tuiles
 *           (modèle, NVS et états dans tab5_tuiles.cpp : rangee_element). Un capteur ou une
 *           clim montre icône + valeur, tout autre appareil son icône seule, colorée selon
 *           son état. Affichage seul : un toucher n'agit sur aucun appareil.
 *       Taille d'une ligne : sans valeur, des icônes de 70 px (celles des plantes ; 45 dans
 *       le cadre Ok Nabu, kIconesSeulesCadre) ; avec,
 *       la plus grande où tout tient sur 401 px (la largeur de l'horloge, et celle du
 *       panneau Ok Nabu dans sa bordure) — icônes et valeurs de 45 px (police de la date
 *       du thème), 32 px, valeurs de 22 px, puis icône au-dessus de la valeur ; au-delà
 *       (valeurs très longues), chaque valeur est coupée avec « … ».
 *       Rotation calée sur la carte centrale (demande d'Axel du 06/10/2026) : son rotateur
 *       (tab5_central_rotator_auto) appelle rangee_tour() 0,2 s avant de la faire tourner ;
 *       une ligne dure N tours (blueprint, 4 par défaut = 32 s), et quand elle change, la
 *       zone glisse juste avant la carte centrale (cascade de haut en bas). Même
 *       animation que la carte centrale (transition_widgets). Défilement au choix
 *       (blueprint, clé defil, plus bas) : « auto » tourne ainsi, « fixe » ne bouge que
 *       par un geste ; défauts : la rangée en auto (comme avant), le panneau Ok Nabu fixe.
 *       Un geste (toucher de la rangée, « ligne suivante » de l'horloge) montre la ligne
 *       suivante et remet le compte à zéro : la ligne choisie reste une durée entière.
 *       Appui long sur les plantes : « Mes Plantes » ; tap sur l'écoute : le mot de réveil.
 * @architecture_constraint Deux panneaux pour les lignes de capteurs de chaque zone
 *       (rangee_panneau.yaml : rangee_a / _b sous l'horloge, nabu_a / _b dans btn_ok_nabu,
 *       tab5-lvgl.yaml), remplis en alternance : celui qui entre n'est jamais
 *       celui qui sort. Un troisième porte la ligne spéciale. transition_widgets() anime le
 *       premier enfant d'un panneau (son contenu) et masque le panneau sortant à la fin ;
 *       montrer() coupe d'abord une transition en cours, pour qu'un toucher rapide ne
 *       laisse jamais deux lignes à l'écran. Écritures comparées d'abord (LVGL 9.5
 *       invalide même à valeur égale).
 * @ai_instruction Les icônes posées ici sont celles de la palette des tuiles
 *       (tuile_icone), présentes dans mdi_font_70, mdi_font_45 et mdi_font_32 ;
 *       MDI_CODE_TARGETS (tools/check_tab5_code_rules.py) rattache les labels
 *       rangee_icone_* (ceux du panneau Ok Nabu compris : rangee_icone_na0 …) à la
 *       palette. Un texte affiché passe par tr() (celui des valeurs vient de
 *       rangee_element, déjà traduit). Une zone à lignes de plus : une valeur de
 *       RangeeZone, sa lettre de clé (tab5_tuiles.cpp), ses widgets, une Defileur ici.
 */
#include "tab5_internal.h"
#include "lvgl.h"
#include <algorithm>
#include <cstring>

RangeeUI g_rangee_ui;
RangeeUI g_nabu_ui;

namespace {

constexpr int kPlaces = 3;          // lignes à l'écran au plus, ligne spéciale comprise
constexpr int kParLigne = 4;        // éléments d'une ligne
constexpr int8_t kSpeciale = -1;    // dans l'ordre des lignes : les plantes, l'écoute
constexpr int32_t kLargeur = 401;   // largeur de l'horloge (rangee.yaml), des panneaux Ok Nabu
constexpr int32_t kMarge = 8;       // écart minimal entre deux éléments, et aux bords
// Dans le cadre du panneau Ok Nabu, plus d'air aux bords : le cadre du thème peut être une
// gélule (Capsule : rayon 45, borné à 36 par les 72 px du cadre, bordure de 4 px) ; avec
// 17 px, la boîte d'un élément reste dans son arrondi intérieur dans les 21 thèmes
// (tests/test_nabu.py le calcule ; 14 px tant que le cadre faisait 90 px de haut).
constexpr int32_t kMargeCadre = 17;
constexpr int32_t kEcart = 6;       // entre l'icône et la valeur (pad_column de rangee_element.yaml)

// Tailles d'une ligne qui a des valeurs, de la plus grande à la plus petite. icone :
// police_icone (0 = 70, 1 = 45, 2 = 32 px) ; texte : 0 = la police de la date du thème
// (45 px, style_police_date, comme les autres textes de 45 px de l'accueil), 1 = 32 px,
// 2 = 22 px (police_texte[texte - 1]) ; empile : l'icône au-dessus de la valeur.
struct Taille {
    int icone;
    int texte;
    bool empile;
};
// Une ligne sans valeur : des icônes de 70 px sous l'horloge (celles des plantes, aussi
// hautes que la rangée) ; de 45 px dans le cadre Ok Nabu, dont l'intérieur a la hauteur de
// la rangée (ligne_zone_h, 70 px) mais une bordure, parfois arrondie en gélule (09/10/2026).
constexpr Taille kIconesSeules = {0, 0, false};
constexpr Taille kIconesSeulesCadre = {1, 0, false};
constexpr Taille kTailles[] = {
    {1, 0, false},
    {2, 1, false},
    {2, 2, false},
    {2, 2, true},
};
constexpr int kNbTailles = sizeof(kTailles) / sizeof(kTailles[0]);

// Une zone à lignes : ses widgets, son modèle (index RangeeZone de tab5_tuiles.cpp) et où
// elle en est.
struct Defileur {
    RangeeZone z;
    Defilement defilement;     // son réglage auto / fixe
    RangeeUI& u;
    int32_t marge;             // écart minimal entre deux éléments et aux bords
    Taille seules;             // taille d'une ligne sans valeur
    bool pret = false;        // widgets posés, premier dessin fait
    int8_t ordre[kPlaces] = {};  // lignes affichées tour à tour : kSpeciale ou 0 à 2
    int n = 0;
    int courante = 0;          // index dans ordre
    int tours = 0;             // tours de la carte centrale depuis le dernier changement
    lv_obj_t* vu = nullptr;    // panneau à l'écran
};
Defileur s_zones[RANGEE_NB] = {
    {RANGEE_HORLOGE, Defilement::RANGEE, g_rangee_ui, kMarge, kIconesSeules},
    {RANGEE_NABU, Defilement::NABU, g_nabu_ui, kMargeCadre, kIconesSeulesCadre},
};

// ─── Mesure et polices ──────────────────────────────────────────────────────────────

int32_t largeur_texte(const char* txt, const lv_font_t* f) {
    if (txt == nullptr || txt[0] == '\0' || f == nullptr) return 0;
    lv_point_t p;
    lv_text_get_size(&p, txt, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return p.x;
}

const lv_font_t* police_lv(esphome::font::Font* f) { return f != nullptr ? f->get_lv_font() : nullptr; }

const lv_font_t* police_icone(const RangeeUI& u, const Taille& t) { return police_lv(u.police_icone[t.icone]); }

// La police des valeurs : celle de la date (thème) pour la grande taille.
const lv_font_t* police_texte(const RangeeUI& u, const Taille& t) {
    if (t.texte == 0) return u.date != nullptr ? lv_obj_get_style_text_font(u.date, LV_PART_MAIN) : nullptr;
    return police_lv(u.police_texte[t.texte - 1]);
}

// Police locale `f` sur le label, ou nullptr : celle de ses styles (style_police_date) ;
// sans rien toucher si c'est déjà la bonne.
void police_si(lv_obj_t* o, esphome::font::Font* f) {
    if (o == nullptr) return;
    lv_style_value_t v;
    const bool locale = lv_obj_get_local_style_prop(o, LV_STYLE_TEXT_FONT, &v, LV_PART_MAIN) == LV_STYLE_RES_FOUND;
    if (f == nullptr ? !locale : (locale && v.ptr == police_lv(f))) return;
    ui_police(o, f);
}

// Largeur d'une ligne dans la taille `t`, marges non comprises.
int32_t largeur_ligne(const RangeeUI& u, const RangeeElement* e, const bool* present, const Taille& t) {
    const lv_font_t* fi = police_icone(u, t);
    const lv_font_t* ft = police_texte(u, t);
    int32_t w = 0;
    for (int i = 0; i < kParLigne; i++) {
        if (!present[i]) continue;
        const int32_t wi = largeur_texte(e[i].icone, fi);
        const int32_t wt = e[i].mesure ? largeur_texte(e[i].texte, ft) : 0;
        if (t.empile) w += std::max(wi, wt);
        else w += wi + (e[i].mesure ? kEcart + wt : 0);
    }
    return w;
}

// ─── Dessin ─────────────────────────────────────────────────────────────────────────

// La ligne de capteurs `l` de la zone dans son panneau `p` (0 ou 1).
void remplir(const Defileur& d, int p, int l) {
    const RangeeUI& u = d.u;
    RangeeElement e[kParLigne];
    bool present[kParLigne];
    int n = 0, n_mesures = 0;
    for (int i = 0; i < kParLigne; i++) {
        present[i] = rangee_element(d.z, l, i, e[i]);
        if (!present[i]) continue;
        n++;
        if (e[i].mesure) n_mesures++;
    }
    const int32_t place = kLargeur - (n + 1) * d.marge;
    const Taille* t = &d.seules;
    bool couper = false;
    if (n_mesures > 0) {
        t = &kTailles[kNbTailles - 1];
        for (const Taille& essai : kTailles) {
            if (largeur_ligne(u, e, present, essai) <= place) {
                t = &essai;
                break;
            }
        }
        couper = largeur_ligne(u, e, present, *t) > place;
    }
    const int32_t par_element = n > 0 ? place / n : place;
    const lv_flex_flow_t flux = t->empile ? LV_FLEX_FLOW_COLUMN : LV_FLEX_FLOW_ROW;
    for (int i = 0; i < kParLigne; i++) {
        lv_obj_t* el = u.element[p][i];
        lv_obj_t* ic = u.icone[p][i];
        lv_obj_t* tx = u.texte[p][i];
        if (el == nullptr || ic == nullptr || tx == nullptr) continue;
        ui_hidden(el, !present[i]);
        if (!present[i]) continue;
        if (lv_obj_get_style_flex_flow(el, LV_PART_MAIN) != flux) lv_obj_set_flex_flow(el, flux);
        police_si(ic, u.police_icone[t->icone]);
        ui_text(ic, e[i].icone);
        ui_text_color(ic, e[i].couleur);
        ui_hidden(tx, !e[i].mesure);
        if (!e[i].mesure) continue;
        police_si(tx, t->texte == 0 ? nullptr : u.police_texte[t->texte - 1]);
        if (couper) texte_ha_coupe(tx, e[i].texte, par_element);
        else ui_text(tx, e[i].texte);
        ui_text_color(tx, e[i].couleur_texte);
    }
}

// Panneau de capteurs à l'écran : 0, 1, ou -1 (la ligne spéciale, ou rien).
int panneau_affiche(const Defileur& d) {
    if (d.vu != nullptr && d.vu == d.u.panneau[0]) return 0;
    if (d.vu != nullptr && d.vu == d.u.panneau[1]) return 1;
    return -1;
}

// Pastilles sous la zone, celles de la carte centrale (pagination_afficher, la même
// recette) : une par ligne, la courante large et opaque ; aucune sous deux lignes.
void pastilles(const Defileur& d) {
    const RangeeUI& u = d.u;
    ui_hidden(u.pastilles_cadre, d.n < 2);
    for (int i = 0; i < kPlaces; i++) ui_hidden(u.pastilles[i], i >= d.n);
    pagination_afficher(u.pastilles, kPlaces, d.courante);
}

// Ligne j de l'ordre à l'écran, avec la transition de la carte centrale ou d'un coup.
void montrer(Defileur& d, int j, bool anime) {
    const RangeeUI& u = d.u;
    if (d.n == 0) return;
    j = std::max(0, std::min(j, d.n - 1));
    lv_obj_t* entrant = u.panneau_special;
    if (d.ordre[j] != kSpeciale) {
        const int p = panneau_affiche(d) == 0 ? 1 : 0;  // jamais celui qui sort
        remplir(d, p, d.ordre[j]);
        entrant = u.panneau[p];
    }
    // Une transition en cours (toucher rapide) : coupée, seul le panneau affiché reste.
    for (lv_obj_t* w : {u.panneau_special, u.panneau[0], u.panneau[1]}) {
        transition_couper(w);
        if (w != d.vu) ui_hidden(w, true);
    }
    if (anime && d.vu != nullptr && d.vu != entrant) {
        transition_widgets(d.vu, entrant);
    } else {
        if (d.vu != entrant) ui_hidden(d.vu, true);
        ui_hidden(entrant, false);
    }
    d.vu = entrant;
    d.courante = j;
    pastilles(d);
}

// La ligne à l'écran, redessinée sur place (état reçu, thème).
void repeindre(const Defileur& d) {
    const int p = panneau_affiche(d);
    if (p >= 0 && d.n > 0 && d.ordre[d.courante] != kSpeciale) remplir(d, p, d.ordre[d.courante]);
}

// La ligne spéciale peut-elle s'afficher ? Les plantes : s'il y a des pots ; l'écoute :
// toujours (le mot de réveil existe sur toute tablette).
bool speciale_disponible(const Defileur& d) { return d.z == RANGEE_NABU || zones_pots_presents() > 0; }

// Lignes affichées tour à tour : les lignes de capteurs remplies, dans l'ordre du
// blueprint, et la ligne spéciale à sa place (si elle n'est pas masquée et qu'elle peut
// s'afficher) ; trois au plus — avec elle, la troisième ligne de capteurs attend.
int calculer_ordre(const Defileur& d, int8_t ordre[kPlaces]) {
    int lignes[kPlaces];
    int nl = 0;
    for (int l = 0; l < kPlaces; l++)
        if (rangee_ligne_remplie(d.z, l)) lignes[nl++] = l;
    const int place = rangee_place_speciale(d.z);
    const int ici = (place >= 0 && speciale_disponible(d)) ? std::min(place, nl) : -1;
    int n = 0;
    for (int k = 0; k <= nl && n < kPlaces; k++) {
        if (k == ici) ordre[n++] = kSpeciale;
        if (k < nl && n < kPlaces) ordre[n++] = static_cast<int8_t>(lignes[k]);
    }
    return n;
}

void suivante(Defileur& d) {
    d.tours = 0;
    if (d.n >= 2) montrer(d, (d.courante + 1) % d.n, true);
}

void appliquer(Defileur& d) {
    const RangeeUI& u = d.u;
    if (u.zone == nullptr || u.panneau_special == nullptr) return;
    d.pret = true;
    int8_t ordre[kPlaces] = {};
    const int n = calculer_ordre(d, ordre);
    const bool meme = d.vu != nullptr && n == d.n && std::memcmp(ordre, d.ordre, sizeof(ordre)) == 0;
    std::memcpy(d.ordre, ordre, sizeof(ordre));
    d.n = n;
    ui_hidden(u.zone, n == 0);
    ui_hidden(u.toucher, n == 0);
    if (n == 0) {
        d.vu = nullptr;
        d.courante = 0;
        pastilles(d);
        return;
    }
    if (meme) {
        repeindre(d);
        pastilles(d);
        return;
    }
    // Autres lignes (définitions, pots apparus ou disparus) : la première, sans animation.
    d.tours = 0;
    montrer(d, 0, false);
}

void tour(Defileur& d) {
    if (!d.pret || d.n < 2 || !defilement_auto(d.defilement)) {
        d.tours = 0;
        return;
    }
    if (++d.tours >= rangee_tours(d.z)) suivante(d);
}

Defileur* zone(int z) { return (z >= 0 && z < RANGEE_NB) ? &s_zones[z] : nullptr; }

// ─── Défilement au choix (clé defil) ────────────────────────────────────────────────

// « defil|rangée|nabu|clim|secondes » : chaque zone « auto » ou « fixe » (autre chose : son
// défaut) ; secondes = durée d'un appareil de la tuile − / + en auto, arrondie au tour de
// la carte centrale (1 à 15 tours). Gardé en NVS : le bon défilement dès le démarrage.
constexpr char kCleAuto[] = "auto";
constexpr char kCleFixe[] = "fixe";
constexpr int kNbDefil = static_cast<int>(Defilement::NB);
// Défauts (sans la clé, ou une valeur inconnue) : l'écran d'avant le lot 3.
constexpr bool kAutoDefaut[kNbDefil] = {true, false, false};
constexpr uint8_t kToursClimDefaut = 4;
constexpr uint8_t kToursClimMax = 15;
constexpr uint32_t kMagicDefil = 0x44454631;    // « DEF1 »
constexpr uint32_t kPrefKeyDefil = 0x6465666C;  // « defl »

struct SauvegardeDefil {
    uint32_t magic;
    uint8_t auto_[kNbDefil];
    uint8_t tours_clim;
};

struct EtatDefil {
    bool charge = false;
    bool auto_[kNbDefil] = {kAutoDefaut[0], kAutoDefaut[1], kAutoDefaut[2]};
    uint8_t tours_clim = kToursClimDefaut;
    esphome::ESPPreferenceObject pref;
};
EtatDefil s_defil;

void defil_charger() {
    if (s_defil.charge) return;
    s_defil.charge = true;
    s_defil.pref = esphome::global_preferences->make_preference<SauvegardeDefil>(kPrefKeyDefil);
    SauvegardeDefil s{};
    if (s_defil.pref.load(&s) && s.magic == kMagicDefil) {
        for (int i = 0; i < kNbDefil; i++) s_defil.auto_[i] = s.auto_[i] != 0;
        s_defil.tours_clim = std::max<uint8_t>(1, std::min(kToursClimMax, s.tours_clim));
    }
}

// Garde et journalise un réglage s'il change.
void defil_garder(const bool* auto_, uint8_t tours_clim) {
    defil_charger();
    if (std::memcmp(auto_, s_defil.auto_, sizeof(s_defil.auto_)) == 0 && tours_clim == s_defil.tours_clim) return;
    std::memcpy(s_defil.auto_, auto_, sizeof(s_defil.auto_));
    s_defil.tours_clim = tours_clim;
    SauvegardeDefil s;
    std::memset(&s, 0, sizeof(s));  // bourrage compris : rien d'indéterminé en NVS
    s.magic = kMagicDefil;
    for (int i = 0; i < kNbDefil; i++) s.auto_[i] = auto_[i] ? 1 : 0;
    s.tours_clim = tours_clim;
    s_defil.pref.save(&s);
    ESP_LOGI("tab5.rangee", "Défilement : rangée %s, Ok Nabu %s, tuile -/+ %s (%d tour(s))",
             auto_[0] ? kCleAuto : kCleFixe, auto_[1] ? kCleAuto : kCleFixe, auto_[2] ? kCleAuto : kCleFixe,
             tours_clim);
    // Une zone passée en fixe garde sa ligne ; repassée en auto, son compte repart de zéro.
    for (Defileur& d : s_zones) d.tours = 0;
}

}  // namespace

// ─── API (tab5_rangee.h, tab5_internal.h) ───────────────────────────────────────────

void rangee_appliquer_ui() {
    for (Defileur& d : s_zones) appliquer(d);
}

void rangee_definitions_changees(int z) {
    Defileur* d = zone(z);
    if (d != nullptr && d->pret) appliquer(*d);
}

void rangee_element_change(int z, int l, int i) {
    (void) i;  // la ligne entière : sa taille peut changer avec la largeur d'une valeur
    const Defileur* d = zone(z);
    if (d != nullptr && d->pret && d->n > 0 && d->ordre[d->courante] == l) repeindre(*d);
}

void rangee_tour() {
    for (Defileur& d : s_zones) tour(d);
    reglables_tour();  // tuile − / + en défilement « auto » (tab5_reglables.cpp)
}

void rangee_toucher() {
    if (s_zones[RANGEE_HORLOGE].pret) suivante(s_zones[RANGEE_HORLOGE]);
}

void nabu_suivant() {
    if (s_zones[RANGEE_NABU].pret) suivante(s_zones[RANGEE_NABU]);
}

bool nabu_ecoute_affichee() {
    const Defileur& d = s_zones[RANGEE_NABU];
    // Avant le premier dessin, le panneau montre « Ok Nabu: ON / OFF » (nabu_ecoute, tab5-lvgl.yaml).
    if (!d.pret) return true;
    return d.n > 0 && d.ordre[d.courante] == kSpeciale;
}

bool rangee_plantes_affichees() {
    const Defileur& d = s_zones[RANGEE_HORLOGE];
    return d.pret && d.n > 0 && d.ordre[d.courante] == kSpeciale;
}

void rangee_recaler() {
    for (Defileur& d : s_zones) {
        if (!d.pret || d.n == 0) continue;
        d.tours = 0;
        montrer(d, 0, false);
    }
}

void rangee_rejouer_theme() {
    for (const Defileur& d : s_zones)
        if (d.pret) repeindre(d);
}

bool defilement_auto(Defilement d) {
    const int i = static_cast<int>(d);
    if (i < 0 || i >= kNbDefil) return false;
    defil_charger();
    return s_defil.auto_[i];
}

int defilement_tours_clim() {
    defil_charger();
    return s_defil.tours_clim;
}

void defilement_recu(const char* valeur, size_t n) {
    Champ f[kNbDefil + 1];
    const int k = champs_decouper(valeur, n, '|', f, kNbDefil + 1);
    bool auto_[kNbDefil];
    for (int i = 0; i < kNbDefil; i++) {
        auto_[i] = kAutoDefaut[i];
        if (i < k && champ_est(f[i], kCleAuto)) auto_[i] = true;
        else if (i < k && champ_est(f[i], kCleFixe)) auto_[i] = false;
    }
    // Secondes illisibles, absentes ou au-delà de 999 : le défaut (4 tours).
    uint8_t tours = kToursClimDefaut;
    const uint32_t s = k > kNbDefil ? champ_entier(f[kNbDefil], 999, 0) : 0;
    if (s > 0) {
        const int t = static_cast<int>((s + kTourCentralS / 2) / kTourCentralS);
        tours = static_cast<uint8_t>(std::max(1, std::min<int>(kToursClimMax, t)));
    }
    defil_garder(auto_, tours);
}

void defilement_defaut() { defil_garder(kAutoDefaut, kToursClimDefaut); }

// Ligne des plantes : une mise à jour d'un des 5 capteurs d'humidité (script
// tab5_pots_maj, tab5-sensors-domotique.yaml). Le tri et le dessin restent dans
// tab5_cards.cpp ; ici, ce que la lambda recopiée 5 fois faisait avant le 08/10/2026.
bool pots_humidite_maj(const bool publies[5], const float vals[5], MoistureSlotUI slots[4],
                       PotDetailUI cards[5]) {
    // Zones (lot 5) : un capteur qui a publié quoi que ce soit (NaN compris,
    // « unavailable ») existe dans HA — son pot reste ou revient à l'écran.
    bool zones_changees = false;
    for (int i = 0; i < 5; i++)
        if (publies[i]) zones_changees |= zone_vue(static_cast<Zone>(static_cast<int>(Zone::POT_1) + i));
    // Icône de chaque pot (capteur N), dans mdi_font_70 (moisture_sensors.yaml) ; les cartes
    // du popup ont les mêmes (pots_popup.yaml).
    const char* icones[5] = {"\U000F0E66", "\U000F09F1", "\U000F0D08", "\U000F024A", "\U000F02E5"};
    float tri[5];
    std::copy(vals, vals + 5, tri);
    sort_and_update_moisture_slots(tri, icones, slots);
    // Popup « Mes Plantes » : humidité + statut des 5 cartes FIXES (carte N = capteur N).
    update_pots_popup_moisture_ui(vals, cards);
    return zones_changees;
}
