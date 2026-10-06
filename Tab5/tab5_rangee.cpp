/**
 * [AI-CONTEXT]
 * @file tab5_rangee.cpp
 * @role Rangée sous l'horloge (ADR-0031, 06/10/2026) : jusqu'à trois lignes qui se
 *       relaient sous l'horloge, à la place de la seule rangée des plantes.
 *         - La ligne des plantes : moisture_sensors.yaml, inchangée (les quatre pots les
 *           plus secs, tab5_cards.cpp), à la place que HA lui donne (1re par défaut) ou
 *           masquée ; absente sans pot (zones, ADR-0018).
 *         - Des lignes de quatre éléments au plus, décrits par HA comme des tuiles
 *           (modèle, NVS et états dans tab5_tuiles.cpp : rangee_element). Un capteur ou une
 *           clim montre icône + valeur, tout autre appareil son icône seule, colorée selon
 *           son état. Affichage seul : un toucher n'agit sur aucun appareil.
 *       Taille d'une ligne : sans valeur, des icônes de 70 px (celles des plantes) ; avec,
 *       la plus grande où tout tient sur les 401 px de l'horloge — icônes et valeurs de
 *       45 px (police de la date du thème), 32 px, valeurs de 22 px, puis icône au-dessus
 *       de la valeur ; au-delà (valeurs très longues), chaque valeur est coupée avec « … ».
 *       Rotation calée sur la carte centrale (demande d'Axel du 06/10/2026) : son rotateur
 *       (tab5_central_rotator_auto) appelle rangee_tour() 0,2 s avant de la faire tourner ;
 *       une ligne dure N tours (blueprint, 4 par défaut = 32 s), et quand elle change, la
 *       rangée glisse juste avant la carte centrale (cascade de haut en bas). Même
 *       animation que la carte centrale (transition_widgets). Toucher : ligne suivante
 *       (le compte des tours repart) ; appui long sur les plantes : « Mes Plantes ».
 * @architecture_constraint Deux panneaux pour les lignes de capteurs (rangee.yaml),
 *       remplis en alternance : celui qui entre n'est jamais celui qui sort. Un troisième
 *       porte les plantes. transition_widgets() anime le premier enfant d'un panneau
 *       (son contenu) et masque le panneau sortant à la fin ; montrer() coupe d'abord une
 *       transition en cours, pour qu'un toucher rapide ne laisse jamais deux lignes à
 *       l'écran. Écritures comparées d'abord (LVGL 9.5 invalide même à valeur égale).
 * @ai_instruction Les icônes posées ici sont celles de la palette des tuiles
 *       (tuile_icone), présentes dans mdi_font_70, mdi_font_45 et mdi_font_32 ;
 *       MDI_CODE_TARGETS (tools/check_tab5_code_rules.py) rattache les labels
 *       rangee_icone_* à la palette. Un texte affiché passe par tr() (celui des valeurs
 *       vient de rangee_element, déjà traduit).
 */
#include "tab5_internal.h"
#include "lvgl.h"
#include <algorithm>
#include <cstring>

RangeeUI g_rangee_ui;

namespace {

constexpr int kPlaces = 3;          // lignes à l'écran au plus, plantes comprises
constexpr int kParLigne = 4;        // éléments d'une ligne
constexpr int8_t kPlantes = -1;     // dans l'ordre des lignes : celle des plantes
constexpr int32_t kLargeur = 401;   // largeur de l'horloge (rangee.yaml)
constexpr int32_t kMarge = 8;       // écart minimal entre deux éléments, et aux bords
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
constexpr Taille kIconesSeules = {0, 0, false};
constexpr Taille kTailles[] = {
    {1, 0, false},
    {2, 1, false},
    {2, 2, false},
    {2, 2, true},
};
constexpr int kNbTailles = sizeof(kTailles) / sizeof(kTailles[0]);

bool s_pret = false;           // widgets posés, premier dessin fait
int8_t s_ordre[kPlaces] = {};  // lignes affichées tour à tour : kPlantes ou 0 à 2
int s_n = 0;
int s_courante = 0;            // index dans s_ordre
int s_tours = 0;               // tours de la carte centrale depuis le dernier changement
lv_obj_t* s_vu = nullptr;      // panneau à l'écran

// ─── Mesure et polices ──────────────────────────────────────────────────────────────

int32_t largeur_texte(const char* txt, const lv_font_t* f) {
    if (txt == nullptr || txt[0] == '\0' || f == nullptr) return 0;
    lv_point_t p;
    lv_text_get_size(&p, txt, f, 0, 0, LV_COORD_MAX, LV_TEXT_FLAG_NONE);
    return p.x;
}

const lv_font_t* police_lv(esphome::font::Font* f) { return f != nullptr ? f->get_lv_font() : nullptr; }

const lv_font_t* police_icone(const Taille& t) { return police_lv(g_rangee_ui.police_icone[t.icone]); }

// La police des valeurs : celle de la date (thème) pour la grande taille.
const lv_font_t* police_texte(const Taille& t) {
    const RangeeUI& u = g_rangee_ui;
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
int32_t largeur_ligne(const RangeeElement* e, const bool* present, const Taille& t) {
    const lv_font_t* fi = police_icone(t);
    const lv_font_t* ft = police_texte(t);
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

// La ligne de capteurs `l` dans le panneau `p` (0 ou 1).
void remplir(int p, int l) {
    const RangeeUI& u = g_rangee_ui;
    RangeeElement e[kParLigne];
    bool present[kParLigne];
    int n = 0, n_mesures = 0;
    for (int i = 0; i < kParLigne; i++) {
        present[i] = rangee_element(l, i, e[i]);
        if (!present[i]) continue;
        n++;
        if (e[i].mesure) n_mesures++;
    }
    const int32_t place = kLargeur - (n + 1) * kMarge;
    const Taille* t = &kIconesSeules;
    bool couper = false;
    if (n_mesures > 0) {
        t = &kTailles[kNbTailles - 1];
        for (const Taille& essai : kTailles) {
            if (largeur_ligne(e, present, essai) <= place) {
                t = &essai;
                break;
            }
        }
        couper = largeur_ligne(e, present, *t) > place;
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

// Panneau de capteurs à l'écran : 0, 1, ou -1 (les plantes, ou rien).
int panneau_affiche() {
    const RangeeUI& u = g_rangee_ui;
    if (s_vu != nullptr && s_vu == u.panneau[0]) return 0;
    if (s_vu != nullptr && s_vu == u.panneau[1]) return 1;
    return -1;
}

// Pastilles sous la rangée, comme celles de la carte centrale (pagination_afficher) :
// une par ligne, la courante large et opaque ; aucune sous deux lignes.
void pastilles() {
    const RangeeUI& u = g_rangee_ui;
    ui_hidden(u.pastilles_cadre, s_n < 2);
    for (int i = 0; i < kPlaces; i++) {
        lv_obj_t* b = u.pastilles[i];
        if (b == nullptr) continue;
        ui_hidden(b, i >= s_n);
        const bool active = i == s_courante;
        const int32_t w = active ? 30 : 16;
        const lv_opa_t opa = active ? 255 : 100;
        if (lv_obj_get_style_width(b, LV_PART_MAIN) != w) lv_obj_set_width(b, w);
        if (lv_obj_get_style_bg_opa(b, LV_PART_MAIN) != opa) lv_obj_set_style_bg_opa(b, opa, LV_PART_MAIN);
    }
}

// Ligne j de l'ordre à l'écran, avec la transition de la carte centrale ou d'un coup.
void montrer(int j, bool anime) {
    const RangeeUI& u = g_rangee_ui;
    if (s_n == 0) return;
    j = std::max(0, std::min(j, s_n - 1));
    lv_obj_t* entrant = u.panneau_plantes;
    if (s_ordre[j] != kPlantes) {
        const int p = panneau_affiche() == 0 ? 1 : 0;  // jamais celui qui sort
        remplir(p, s_ordre[j]);
        entrant = u.panneau[p];
    }
    // Une transition en cours (toucher rapide) : coupée, seul le panneau affiché reste.
    for (lv_obj_t* w : {u.panneau_plantes, u.panneau[0], u.panneau[1]}) {
        transition_couper(w);
        if (w != s_vu) ui_hidden(w, true);
    }
    if (anime && s_vu != nullptr && s_vu != entrant) {
        transition_widgets(s_vu, entrant);
    } else {
        if (s_vu != entrant) ui_hidden(s_vu, true);
        ui_hidden(entrant, false);
    }
    s_vu = entrant;
    s_courante = j;
    pastilles();
}

// La ligne à l'écran, redessinée sur place (état reçu, thème).
void repeindre() {
    const int p = panneau_affiche();
    if (p >= 0 && s_n > 0 && s_ordre[s_courante] != kPlantes) remplir(p, s_ordre[s_courante]);
}

// Lignes affichées tour à tour : les lignes de capteurs remplies, dans l'ordre du
// blueprint, et celle des plantes à sa place (si elle n'est pas masquée et qu'il y a
// des pots) ; trois au plus — avec les plantes, la troisième ligne de capteurs attend.
int calculer_ordre(int8_t ordre[kPlaces]) {
    int lignes[kPlaces];
    int nl = 0;
    for (int l = 0; l < kPlaces; l++)
        if (rangee_ligne_remplie(l)) lignes[nl++] = l;
    const int place = rangee_place_plantes();
    const int ici = (place >= 0 && zones_pots_presents() > 0) ? std::min(place, nl) : -1;
    int n = 0;
    for (int k = 0; k <= nl && n < kPlaces; k++) {
        if (k == ici) ordre[n++] = kPlantes;
        if (k < nl && n < kPlaces) ordre[n++] = static_cast<int8_t>(lignes[k]);
    }
    return n;
}

void suivante() {
    s_tours = 0;
    if (s_n >= 2) montrer((s_courante + 1) % s_n, true);
}

}  // namespace

void rangee_appliquer_ui() {
    const RangeeUI& u = g_rangee_ui;
    if (u.zone == nullptr || u.panneau_plantes == nullptr) return;
    s_pret = true;
    int8_t ordre[kPlaces] = {};
    const int n = calculer_ordre(ordre);
    const bool meme = s_vu != nullptr && n == s_n && std::memcmp(ordre, s_ordre, sizeof(ordre)) == 0;
    std::memcpy(s_ordre, ordre, sizeof(ordre));
    s_n = n;
    ui_hidden(u.zone, n == 0);
    ui_hidden(u.toucher, n == 0);
    if (n == 0) {
        s_vu = nullptr;
        s_courante = 0;
        pastilles();
        return;
    }
    if (meme) {
        repeindre();
        pastilles();
        return;
    }
    // Autres lignes (définitions, pots apparus ou disparus) : la première, sans animation.
    s_tours = 0;
    montrer(0, false);
}

void rangee_definitions_changees() {
    if (s_pret) rangee_appliquer_ui();
}

void rangee_element_change(int l, int i) {
    (void) i;  // la ligne entière : sa taille peut changer avec la largeur d'une valeur
    if (s_pret && s_n > 0 && s_ordre[s_courante] == l) repeindre();
}

void rangee_tour() {
    if (!s_pret || s_n < 2) {
        s_tours = 0;
        return;
    }
    if (++s_tours >= rangee_tours()) suivante();
}

void rangee_toucher() {
    if (s_pret) suivante();
}

bool rangee_plantes_affichees() {
    return s_pret && s_n > 0 && s_ordre[s_courante] == kPlantes;
}

void rangee_recaler() {
    if (!s_pret || s_n == 0) return;
    s_tours = 0;
    montrer(0, false);
}

void rangee_rejouer_theme() {
    if (s_pret) repeindre();
}
