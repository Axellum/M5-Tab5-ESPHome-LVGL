/**
 * [AI-CONTEXT]
 * @file tab5_tuiles_popups.cpp
 * @role Popups d'une tuile (sorti de tab5_tuiles.cpp le 08/10/2026, lot L7 de l'audit du
 *       07/10/2026) : lumière (les lumières de la pièce, appui long d'une lum, ADR-0023),
 *       volet (appui long d'une vol sans l'option k, 05/10/2026 : position, volet dessiné
 *       qu'on fait glisser, envoyé au relâcher, Ouvrir / Stop / Fermer) et appareil (appui
 *       long d'une int, d'une act ou d'une med sans l'option t, 06/10/2026 : la fenêtre
 *       « plus d'infos » d'un tableau de bord HA, dont le grand bouton refait le toucher de
 *       la tuile). Leur base commune : PopupTuile (pièce, tuile, ouvert, revalidation).
 * @architecture_constraint Lit le modèle des tuiles par tab5_tuiles_priv.h (s_m, s_etats,
 *       vue_def, gestes…) et n'envoie rien par lui-même : envoyer_tuile / tuile_appui_piece
 *       (tab5_tuiles.cpp). Repeints quand l'état de leur tuile change (peindre_tuile →
 *       popup_*_etat), revalidés quand les définitions changent (tuiles_definir →
 *       popups_revalider), repeints au changement de thème (popups_rejouer_theme).
 *       Widgets : ui_components/light_popup.yaml, volet_popup.yaml, appareil_popup.yaml,
 *       posés dans g_tuiles_ui par tab5-tuiles.yaml.
 * @ai_instruction Un texte affiché passe par tr() ; un nom venu de HA s'affiche tel quel
 *       (ui_texte_coupe). Glyphes posés d'ici (règle 7) : glyphe_commande
 *       (MDI_CODE_TARGETS de check_tab5_code_rules.py).
 */
#include "tab5_tuiles_priv.h"
#include "tab5_geometrie.h"
#include "tab5_modele_ha.h"
#include "tab5_tuiles_icones.h"
#include "lvgl.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// kNom, kEtat, est(), etat_indisponible(), tuile_cle() : tab5_modele_ha.h.
using namespace modele_ha;

namespace tuiles {

// ─── Popups d'une tuile : ce que lumière, volet et appareil partagent ──────────────
//
// Audit du 07/10/2026 (lot L7, UI-4) : chacun des trois popups gardait sa pièce, sa tuile,
// son « ouvert ? » et sa revalidation aux nouvelles définitions, écrits trois fois.
// PopupTuile<C> les écrit une fois ; C est son conteneur dans TuilesUI (branché par
// tab5-tuiles.yaml). Ce que chaque popup a en plus (lignes du popup lumière, doigt et
// position du volet) reste dans sa propre structure.

template <lv_obj_t* TuilesUI::*C>
struct PopupTuile {
    int piece = -1;
    int tuile = -1;  // popup lumière : la tuile de la ligne choisie

    static lv_obj_t* conteneur() { return g_tuiles_ui.*C; }

    // Montré à l'écran. Fermé par sa croix (YAML), le C++ ne le voit qu'ici.
    static bool ouvert() {
        const lv_obj_t* p = conteneur();
        return p != nullptr && !lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN);
    }

    // Ouvert sur la tuile tRT (un état de cette tuile le repeint).
    bool montre(int r, int t) const { return ouvert() && r == piece && t == tuile; }

    // Sa tuile est dans la grille, hors mode héritage (qui n'ouvre ni volet ni appareil).
    bool en_grille() const { return piece >= 0 && piece < kPieces && tuile >= 0 && tuile < kTuiles && !heritage(); }
};

// Ouvre le popup `p` sur la tuile tRT : son état remis à zéro, peint, puis montré. Rien
// tant que tab5-tuiles.yaml n'a pas branché son conteneur.
template <class P>
void popup_tuile_ouvrir(P& p, int r, int t, void (*peindre)()) {
    if (P::conteneur() == nullptr) return;
    p = P{};
    p.piece = r;
    p.tuile = t;
    peindre();
    animate_popup_open(P::conteneur());
}

// Nouvelles définitions des tuiles, popup ouvert : repeint s'il montre toujours une tuile
// qui a ce popup (`valide`), sinon refermé (tuile devenue lecture seule, autre type…).
template <class P>
void popup_tuile_revalider(const P&, bool valide, void (*peindre)()) {
    if (!P::ouvert()) return;
    if (valide) peindre();
    else animate_popup_close(P::conteneur());
}

// ─── Popup lumière : les lumières de la pièce (ADR-0023) ─────────────────────────────

struct PopupLumiere : PopupTuile<&TuilesUI::lum_popup> {
    int n = 0;                 // lignes du sélecteur
    int tuiles[kTuiles] = {};  // tuile de chaque ligne, dans l'ordre des tuiles
    int choix = 0;             // ligne pilotée (current_light_slot) : tuile = tuiles[choix]
};
PopupLumiere s_pl;

// Une lumière pilotable de la pièce : tuile lum sans option r ; en mode héritage,
// lumiere_1..3 (tuiles 2 à 4) que HA n'a pas déclarées absentes.
bool est_lumiere(int r, int t) {
    if (!tuile_presente(r, t)) return false;
    if (heritage()) return r == 0 && t >= 2;
    const Def& d = s_m.tuiles[r][t];
    return d.type == static_cast<uint8_t>(Type::LUM) && !(d.options & OPT_R);
}

bool lumiere_allumee(int r, int t) {
    return heritage() ? s_h.lum[t - 2] : est(s_etats[r][t].brut, "on");
}

float lumiere_luminosite(int r, int t) {
    return heritage() ? s_h.lum_val[t - 2] : s_etats[r][t].valeur;
}

const char* lumiere_nom(int r, int t) {
    if (!heritage()) return s_m.tuiles[r][t].nom;
    return t == 2 ? tr("Chambre") : (t == 3 ? tr("Salon") : "LEDs");
}

// Clé des commandes du popup : tRT, ou lumiere_N en mode héritage (commandes 3.x).
void lumiere_cle(int r, int t, std::string& out) {
    if (heritage()) {
        out = kHeritageLumieres[t - 2];
        return;
    }
    out = tuile_cle(r, t).s;
}

// Arc et « NN % » de la ligne choisie ; pas pendant un glissement (le retour de HA
// ferait sauter le curseur sous le doigt). Éteinte, ou luminosité inconnue : 0 ; allumée,
// le % de la carte et de la roue (lum_pct).
void popup_lumiere_arc() {
    const TuilesUI& u = g_tuiles_ui;
    if (s_pl.n == 0 || u.lum_arc == nullptr || u.lum_pct == nullptr) return;
    if (lv_obj_has_state(u.lum_arc, LV_STATE_PRESSED)) return;
    const int r = s_pl.piece, t = s_pl.tuile;
    const float v = lumiere_luminosite(r, t);
    const bool allumee = lumiere_allumee(r, t);
    const int arcv = allumee ? tab5_float_vers_int(v, 0, 255, 0) : 0;
    lv_arc_set_value(u.lum_arc, arcv);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d %%", allumee ? std::max(0, lum_pct(v)) : 0);
    ui_text(u.lum_pct, buf);
}

// Une ligne du sélecteur : icône (palette ou 3.1) dorée si allumée, nom coupé.
void popup_lumiere_ligne(int i) {
    const TuilesUI& u = g_tuiles_ui;
    const int r = s_pl.piece, t = s_pl.tuiles[i];
    const bool on = lumiere_allumee(r, t);
    const char* icone = heritage() ? heritage_glyphe_selecteur(t) : tuile_icone(s_m.tuiles[r][t].icone, on, "lum");
    ui_text(u.lum_sel_icone[i], icone);
    ui_text_color(u.lum_sel_icone[i], on ? UIColor.INFO : UIColor.TEXT_DIM);
    ui_texte_coupe(u.lum_sel_nom[i], lumiere_nom(r, t), 244);  // 342 − 88 − marge
}

// Tout le popup : lignes (serrées au-delà de trois), surbrillance, titre, bouton
// marche/arrêt et arc de la ligne choisie.
void popup_lumiere_peindre() {
    const TuilesUI& u = g_tuiles_ui;
    if (u.lum_popup == nullptr) return;
    const bool serre = s_pl.n > 3;
    const int32_t hauteur = serre ? 54 : 86, pas = serre ? 60 : 96;
    for (int i = 0; i < kTuiles; i++) {
        lv_obj_t* b = u.lum_sel[i];
        const bool visible = i < s_pl.n;
        ui_hidden(b, !visible);
        if (!visible || b == nullptr) continue;
        ui_y(b, 50 + i * pas);
        if (lv_obj_get_style_height(b, LV_PART_MAIN) != hauteur) lv_obj_set_height(b, hauteur);
        highlight_button_border(b, i == s_pl.choix, UIColor.ACCENT, 3);
        popup_lumiere_ligne(i);
    }
    if (s_pl.n == 0) return;
    const int r = s_pl.piece, t = s_pl.tuile;
    if (heritage()) {
        const char* titre = t == 2 ? tr("Ampoule Chambre") : (t == 3 ? tr("Ampoule Salon") : tr("Ampoule LEDs"));
        ui_text(u.lum_titre, titre);
    } else {
        ui_text(u.lum_titre, lumiere_nom(r, t));
    }
    ui_text_color(u.lum_power, lumiere_allumee(r, t) ? UIColor.INFO : UIColor.TEXT_DIM);
    popup_lumiere_arc();
}

// Lignes du popup : les lumières de la pièce `r`, ligne choisie = celle de la tuile `t`
// (sinon la première). Faux si la pièce n'en a aucune.
bool popup_lumiere_lignes(int r, int t) {
    const TuilesUI& u = g_tuiles_ui;
    s_pl = PopupLumiere{};
    s_pl.piece = r;
    for (int i = 0; i < kTuiles; i++)
        if (est_lumiere(r, i)) {
            if (i == t) s_pl.choix = s_pl.n;
            s_pl.tuiles[s_pl.n++] = i;
        }
    if (s_pl.n == 0) return false;
    s_pl.tuile = s_pl.tuiles[s_pl.choix];
    if (u.lum_cle != nullptr) lumiere_cle(r, s_pl.tuile, *u.lum_cle);
    return true;
}

// Lumières de la pièce `r`, ligne choisie = celle de la tuile `t` (appui long).
void popup_lumiere_ouvrir(int r, int t) {
    if (PopupLumiere::conteneur() == nullptr || !popup_lumiere_lignes(r, t)) return;
    popup_lumiere_peindre();
    animate_popup_open(PopupLumiere::conteneur());
}

// Nouvelles définitions des tuiles (UI-1, audit du 07/10/2026), comme les popups volet et
// appareil : ouvert, les lignes de la même pièce sont recalculées — la lampe choisie le
// reste si elle est encore une lumière, sinon la première — puis le popup est repeint ;
// plus aucune lumière dans la pièce : refermé. Fermé (y compris par sa croix, que le C++
// ne voit pas), il est oublié : aucun index ne vise plus une tuile disparue.
void popup_lumiere_revalider() {
    if (!s_pl.ouvert()) {
        s_pl = PopupLumiere{};
        return;
    }
    const int choisie = s_pl.n > 0 ? s_pl.tuile : -1;
    popup_tuile_revalider(s_pl, popup_lumiere_lignes(s_pl.piece, choisie), popup_lumiere_peindre);
}

// Un état a changé : la ligne de cette tuile si le popup la montre.
void popup_lumiere_etat(int r, int t) {
    if (!s_pl.ouvert() || r != s_pl.piece) return;
    const TuilesUI& u = g_tuiles_ui;
    for (int i = 0; i < s_pl.n; i++) {
        if (s_pl.tuiles[i] != t) continue;
        popup_lumiere_ligne(i);
        if (i == s_pl.choix) {
            ui_text_color(u.lum_power, lumiere_allumee(r, t) ? UIColor.INFO : UIColor.TEXT_DIM);
            popup_lumiere_arc();
        }
    }
}

// ─── Popup du volet (05/10/2026, discussion #278) ───────────────────────────────────
//
// Appui long d'une tuile vol sans l'option k : un volet dessiné (06/10/2026, même
// discussion : « like ha animation, not a basic slider ») qu'on fait glisser du doigt
// (position connue seulement, envoyée au relâcher : action « position »), la position en
// grand, l'état en mots et Ouvrir / Stop / Fermer (les commandes de la tuile). Tant qu'il
// est ouvert, il suit l'état de SA tuile (peindre_tuile → popup_volet_etat) : chaque
// position poussée par HA redessine le tablier directement, sans interpolation ni fondu
// (préférence d'Axel : transitions instantanées) — le volet dessiné descend quand le vrai
// descend, au rythme des poussées.

struct PopupVolet : PopupTuile<&TuilesUI::vol_popup> {
    bool saisi = false;   // un doigt tient le volet (position connue à l'appui)
    bool glisse = false;  // il a glissé au-delà du seuil : son relâcher envoie la position
    bool cible = false;   // position envoyée : dessinée jusqu'au prochain état de HA
    bool estompe = false; // position dessinée = un repère (position inconnue)
    int32_t y_appui = 0;  // ordonnée du doigt à l'appui (écran)
    int pos_appui = 0;    // position dessinée à l'appui
    int pos = 0;          // position dessinée : 0 fermé, 100 ouvert
};
PopupVolet s_pv;

// Géométrie (volet_popup.yaml) : l'état sous la position, ou seul au milieu de la carte
// (598 px de haut, 53 px de texte) ; titre : la barre d'en-tête moins l'icône et la croix.
constexpr int32_t kVoletEtatSous = 360;
constexpr int32_t kVoletEtatSeul = 272;
constexpr int32_t kVoletTitreLargeur = 1000;
// Volet dessiné : hauteur de la fenêtre et du tablier (volet_fenetre, volet_tablier de
// volet_popup.yaml, tests/test_tuiles_firmware.py compare), lames de 38 px.
constexpr int32_t kVoletFenetreH = 456;
constexpr int32_t kVoletLameH = 38;
constexpr int kVoletLames = kVoletFenetreH / kVoletLameH;
static_assert(kVoletLames * kVoletLameH == kVoletFenetreH, "les lames couvrent la fenêtre");
// Un doigt qui bouge de moins que ça n'a pas glissé : un toucher n'envoie rien (pas
// d'ordre de plus à un volet en route).
constexpr int32_t kVoletSeuilGlisse = 12;
// Position inconnue, ni ouvert ni fermé (partiel, en mouvement, hors ligne) : le tablier
// à mi-hauteur, lames estompées — un repère, pas une mesure.
constexpr int kVoletMilieu = 50;

// Style partagé des lames, créé avec elles (lames_construire). Ses couleurs viennent de
// la palette active et sont reposées par popup_volet_dessiner quand elles changent
// (thème, volet estompé) : un seul lv_obj_report_style_change, pas un par lame.
lv_style_t s_lame;
bool s_lame_pret = false;
uint32_t s_lame_fond = 0;
uint32_t s_lame_joint = 0;
lv_opa_t s_lame_opa = LV_OPA_COVER;

// La tuile du popup est-elle toujours un volet pilotable ? (les définitions peuvent
// changer popup ouvert ; le mode héritage n'ouvre jamais ce popup.)
bool popup_volet_valide() {
    if (!s_pv.en_grille()) return false;
    const Def& d = s_m.tuiles[s_pv.piece][s_pv.tuile];
    return d.type == static_cast<uint8_t>(Type::VOL) && !(d.options & OPT_R);
}

// Position 0-100 connue : un état en ligne et une valeur dans les bornes. NaN : le volet
// n'en donne pas ; -1 : « Partiel » du volet à course simulée (arrêté en route).
bool vol_position_connue(const Etat& e) {
    if (!e.recu || etat_indisponible(e.brut)) return false;
    return !std::isnan(e.valeur) && e.valeur >= 0.0f && e.valeur <= 100.0f;
}

// Position à dessiner : la vraie si elle est connue ; sinon l'état — fermé en bas, ouvert
// en haut, le reste (partiel, en mouvement, hors ligne, rien reçu) à mi-hauteur, estompé.
int vol_position_dessin(const Etat& e, bool& estompe) {
    estompe = false;
    if (vol_position_connue(e)) return tab5_float_vers_int(e.valeur, 0, 100, 0);
    if (e.recu && est(e.brut, "closed")) return 0;
    // « open » sans position (NaN) : ouvert ; avec -1 (volet à course simulée) : partiel.
    if (e.recu && est(e.brut, "open") && !(e.valeur < 0.0f)) return 100;
    estompe = true;
    return kVoletMilieu;
}

// L'état en mots, dans les couleurs de la tuile. Ouvert mais arrêté en route (-1 du
// volet à course simulée, ou une position entre les deux bouts) : « Partiel ».
const char* vol_etat_mots(const Etat& e, uint32_t& couleur) {
    couleur = UIColor.INACTIVE;
    if (!e.recu) return "--";
    if (etat_indisponible(e.brut)) return tr("Hors ligne");
    if (vol_mouvement(e.brut)) {
        couleur = UIColor.INFO;
        return tr("En mouvement");
    }
    if (est(e.brut, "closed")) {
        couleur = UIColor.TEXT_DIM;
        return tr("Fermé");
    }
    couleur = UIColor.SUCCESS;
    if (e.valeur < 0.0f || (e.valeur > 0.0f && e.valeur < 100.0f)) return tr("Partiel");
    return tr("Ouvert");
}

void popup_volet_nombre(int pos) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", pos);
    ui_text(g_tuiles_ui.vol_nombre, buf);
}

// Builder des lames (règle 5) : 12 lames de 38 px empilées dans le tablier, chacune un
// aplat d'accent et un joint de 4 px en bas, toutes sur le style partagé s_lame. La
// dernière (en bas) fait le bord du tablier. Non cliquables : le toucher va au cadre.
void lames_construire(lv_obj_t* tablier) {
    lv_style_init(&s_lame);
    lv_style_set_radius(&s_lame, 0);
    lv_style_set_pad_all(&s_lame, 0);
    lv_style_set_bg_opa(&s_lame, LV_OPA_COVER);
    lv_style_set_border_side(&s_lame, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_width(&s_lame, 4);
    lv_style_set_border_opa(&s_lame, LV_OPA_60);
    s_lame_fond = UIColor.ACCENT;
    s_lame_joint = UIColor.GLASS_LO;
    s_lame_opa = LV_OPA_COVER;
    lv_style_set_bg_color(&s_lame, lv_color_hex(s_lame_fond));
    lv_style_set_border_color(&s_lame, lv_color_hex(s_lame_joint));
    s_lame_pret = true;
    for (int i = 0; i < kVoletLames; i++) {
        lv_obj_t* lame = lv_obj_create(tablier);
        lv_obj_remove_style_all(lame);
        lv_obj_add_style(lame, &s_lame, LV_PART_MAIN);
        lv_obj_remove_flag(lame, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(lame, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(lame, 0, i * kVoletLameH);
        lv_obj_set_size(lame, lv_pct(100), kVoletLameH);
    }
}

// Le volet dessiné à `pos` (0 fermé, 100 ouvert) : le tablier remonte de pos % de la
// fenêtre, qui le rogne (à 100 % il est rentré dans le coffre). Lames pleines, ou
// estompées quand la position n'est qu'un repère.
void popup_volet_dessiner(int pos, bool estompe) {
    ui_y(g_tuiles_ui.vol_tablier, -(std::clamp(pos, 0, 100) * kVoletFenetreH) / 100);
    if (!s_lame_pret) return;
    const uint32_t fond = UIColor.ACCENT;
    const uint32_t joint = UIColor.GLASS_LO;
    const lv_opa_t opa = estompe ? LV_OPA_40 : LV_OPA_COVER;
    if (fond == s_lame_fond && joint == s_lame_joint && opa == s_lame_opa) return;
    s_lame_fond = fond;
    s_lame_joint = joint;
    s_lame_opa = opa;
    lv_style_set_bg_color(&s_lame, lv_color_hex(fond));
    lv_style_set_border_color(&s_lame, lv_color_hex(joint));
    lv_style_set_bg_opa(&s_lame, opa);
    lv_obj_report_style_change(&s_lame);
}

// Titre, volet dessiné, position (rangée « 45 % », ou rien), état en mots.
void popup_volet_peindre() {
    const TuilesUI& u = g_tuiles_ui;
    if (u.vol_popup == nullptr || !popup_volet_valide()) return;
    const int r = s_pv.piece, t = s_pv.tuile;
    const Etat& e = s_etats[r][t];
    ui_texte_coupe(u.vol_titre, s_m.tuiles[r][t].nom, kVoletTitreLargeur);
    const bool connue = vol_position_connue(e);
    ui_hidden(u.vol_position, !connue);
    // Pas pendant un glissement : le retour de HA ferait sauter le volet sous le doigt
    // (le dessin et le nombre suivent alors le doigt, volet_cadre_rappel). Ni après le
    // relâcher, tant que HA n'a pas poussé d'état (tuiles_etat_recu) : un repeint de
    // thème ou de définitions garde la position envoyée, comme un démarrage à froid.
    if (!s_pv.saisi && !s_pv.cible) s_pv.pos = vol_position_dessin(e, s_pv.estompe);
    // Toujours redessiné : les couleurs des lames suivent la palette (thème).
    popup_volet_dessiner(s_pv.pos, s_pv.estompe);
    if (connue) popup_volet_nombre(s_pv.pos);
    uint32_t couleur = UIColor.INACTIVE;
    ui_text(u.vol_etat, vol_etat_mots(e, couleur));
    ui_text_color(u.vol_etat, couleur);
    ui_y(u.vol_etat, connue ? kVoletEtatSous : kVoletEtatSeul);
}

void popup_volet_ouvrir(int r, int t) { popup_tuile_ouvrir(s_pv, r, t, popup_volet_peindre); }

// Un état a changé : le popup, s'il montre cette tuile.
void popup_volet_etat(int r, int t) {
    if (s_pv.montre(r, t)) popup_volet_peindre();
}

// Relâcher après un glissement : « position » + 0-100 à la tuile du popup (événement
// esphome.tab5_action ; le blueprint la passe à cover / valve.set_…_position de l'entité
// de CETTE tuile, quand elle sait le faire). Rien sans position connue au relâcher (le
// volet a pu passer hors ligne pendant le geste) : vrai si la position est partie.
bool popup_volet_envoyer_position() {
    const TuilesUI& u = g_tuiles_ui;
    if (!popup_volet_valide() || u.envoyer == nullptr) return false;
    if (!vol_position_connue(s_etats[s_pv.piece][s_pv.tuile])) return false;
    char valeur[8];
    snprintf(valeur, sizeof(valeur), "%d", std::clamp(s_pv.pos, 0, 100));
    u.envoyer(tuile_cle(s_pv.piece, s_pv.tuile).s, "position", valeur);
    s_pv.cible = true;
    return true;
}

// Cadre du volet dessiné : on attrape le tablier n'importe où et on le tire. Son bord suit
// le doigt (vers le bas, il descend : la position baisse) ; seul le relâcher envoie (un
// seul set_position par geste), et seulement après un vrai glissement (kVoletSeuilGlisse) :
// un toucher n'envoie rien. Position inconnue à l'appui : rien ne glisse. PRESS_LOST
// aussi : un doigt perdu en route relâche ailleurs. Après un toucher, ou un relâcher qui
// n'envoie rien, le dessin reprend l'état de HA (popup_volet_peindre).
void volet_cadre_rappel(lv_event_t* ev) {
    lv_point_t p = {0, 0};
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr) lv_indev_get_point(indev, &p);
    switch (lv_event_get_code(ev)) {
        case LV_EVENT_PRESSED:
            s_pv.glisse = false;
            s_pv.saisi = popup_volet_valide() && vol_position_connue(s_etats[s_pv.piece][s_pv.tuile]);
            s_pv.y_appui = p.y;
            s_pv.pos_appui = s_pv.pos;
            if (s_pv.saisi) s_pv.estompe = false;
            break;
        case LV_EVENT_PRESSING: {
            if (!s_pv.saisi) break;
            const int32_t dy = p.y - s_pv.y_appui;
            if (!s_pv.glisse && std::abs(dy) < kVoletSeuilGlisse) break;
            s_pv.glisse = true;
            const int pos = std::clamp(s_pv.pos_appui - static_cast<int>(dy * 100 / kVoletFenetreH), 0, 100);
            if (pos == s_pv.pos) break;
            s_pv.pos = pos;
            popup_volet_dessiner(pos, false);
            popup_volet_nombre(pos);
            break;
        }
        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST: {
            const bool envoyer = s_pv.saisi && s_pv.glisse;
            s_pv.saisi = false;
            s_pv.glisse = false;
            if (envoyer && popup_volet_envoyer_position()) break;
            popup_volet_peindre();
            break;
        }
        default:
            break;
    }
}

// ─── Popup d'un appareil (06/10/2026, discussion #278) ──────────────────────────────
//
// « Buttons can have pop up screen like ha dashboard » : l'appui long d'une tuile qui
// n'avait pas de popup (int, act, med sans l'option t) ouvre, pour cette tuile, la
// fenêtre « plus d'infos » d'un tableau de bord HA : son icône dans une pastille ronde
// de la couleur de son état, l'état en mots, sa pièce, ses options, et un grand bouton
// qui fait EXACTEMENT ce que fait le toucher de la tuile (tuile_appui_piece) : même
// commande, même confirmation (option k : la minuterie de la tuile, la tuile ET le popup
// demandent « Confirmer ? »), même « OK » après « lancer ». Il ne montre que ce que HA
// pousse déjà pour les tuiles (définition, état) : rien d'inventé. Tant qu'il est
// ouvert, il suit l'état de SA tuile (peindre_tuile → popup_appareil_etat).

struct PopupAppareil : PopupTuile<&TuilesUI::app_popup> {};
PopupAppareil s_pa;

// Géométrie (appareil_popup.yaml) : titre = la barre d'en-tête moins l'icône et la
// croix ; textes de la carte ÉTAT (800 px) et de la carte COMMANDE (386 px) ; grand
// bouton de 380 px, rempli à moitié (allumé : en haut ; éteint : en bas) ou en entier
// (scène, script, bouton).
constexpr int32_t kAppareilTitreLargeur = 1000;
constexpr int32_t kAppareilTexteLargeur = 740;
constexpr int32_t kAppareilActionLargeur = 350;
constexpr int32_t kAppareilBoutonHauteur = 380;

// La tuile du popup a-t-elle toujours ce popup ? (les définitions peuvent changer popup
// ouvert ; kGestes : int, act, med sans l'option t ; option r : jamais ; le mode héritage
// n'ouvre jamais ce popup.)
bool popup_appareil_valide() {
    return s_pa.en_grille() && gestes(s_m.tuiles[s_pa.piece][s_pa.tuile], false).fenetre == Fenetre::APPAREIL;
}

// Glyphe du grand bouton (mdi_font_45, règle 9) : lecture pour une scène, un script, un
// bouton ; marche / arrêt sinon.
const char* glyphe_commande(bool lancer) {
    return lancer ? "\U000F040A" : "\U000F0425";
}

// Tout le popup, depuis la définition et l'état de sa tuile (vue_def : les mêmes mots,
// icône et couleurs que la carte du mode HA).
void popup_appareil_peindre() {
    const TuilesUI& u = g_tuiles_ui;
    if (u.app_popup == nullptr || !popup_appareil_valide()) return;
    const int r = s_pa.piece, t = s_pa.tuile;
    const Def& d = s_m.tuiles[r][t];
    const Etat& e = s_etats[r][t];
    const Type type = static_cast<Type>(d.type);
    Vue v;
    vue_def(d, e, r, t, v);
    const bool lancer = type == Type::ACT;
    const bool confirmer = minuterie_sur(s_confirmation, r, t);
    const bool ok = minuterie_sur(s_ok, r, t);
    ui_texte_coupe(u.app_titre, d.nom, kAppareilTitreLargeur);
    // Pastille : l'icône de la tuile en couleur pleine sur sa couleur à 20 % (le YAML).
    if (v.icone_carte != nullptr) ui_text(u.app_icone, v.icone_carte);
    ui_text_color(u.app_icone, v.couleur_carte);
    ui_fond(u.app_pastille, v.couleur_carte);
    // État en mots : la ligne d'état de la carte, sauf « Lancer » (une action, pas un
    // état) : un script en route (« on ») dit « En cours », le reste « Prêt ».
    const char* etat = v.ligne;
    if (lancer && std::strcmp(v.ligne, tr("Lancer")) == 0) etat = est(e.brut, "on") ? tr("En cours") : tr("Prêt");
    ui_texte_coupe(u.app_etat, etat, kAppareilTexteLargeur);
    ui_text_color(u.app_etat, v.couleur_ligne);
    // La pièce : son nom, ou « Pièce n » quand HA n'en donne pas.
    char buf[64];
    if (s_m.pieces[r][0] != '\0') snprintf(buf, sizeof(buf), tr("Pièce : %s"), s_m.pieces[r]);
    else snprintf(buf, sizeof(buf), tr("Pièce %d"), r + 1);
    ui_texte_coupe(u.app_piece, buf, kAppareilTexteLargeur);
    // Options de la tuile (blueprint, « Personnaliser des tuiles »), une par ligne.
    char options[96] = "";
    if ((d.options & OPT_O) && !lancer) snprintf(options, sizeof(options), "%s", tr("Allumer seulement"));
    if (d.options & OPT_K) {
        const size_t n = std::strlen(options);
        snprintf(options + n, sizeof(options) - n, "%s%s", n > 0 ? "\n" : "", tr("Confirmer chaque commande"));
    }
    ui_text(u.app_options, options);
    ui_hidden(u.app_options, options[0] == '\0');
    // Grand bouton : rempli en haut quand l'appareil est allumé, en bas sinon, en entier
    // pour une scène ; dans la couleur de la carte (ambre pendant une confirmation).
    lv_obj_t* f = u.app_remplissage;
    const int32_t h = lancer ? kAppareilBoutonHauteur : kAppareilBoutonHauteur / 2;
    if (f != nullptr && lv_obj_get_style_height(f, LV_PART_MAIN) != h) lv_obj_set_height(f, h);
    ui_y(f, (lancer || v.actif) ? 0 : kAppareilBoutonHauteur - h);
    ui_fond(f, v.couleur_carte);
    ui_text(u.app_commande_icone, glyphe_commande(lancer));
    ui_text_color(u.app_commande_icone, v.couleur_carte);
    // Ce que fera l'appui : la commande de la tuile (basculer, allumer avec l'option o,
    // lancer), dite en mots ; ambre quand elle attend sa confirmation.
    const char* action = tr("Éteindre");
    if (lancer) action = ok ? "OK" : tr("Lancer");
    else if ((d.options & OPT_O) || !v.actif) action = tr("Allumer");
    uint32_t c_action = UIColor.ACCENT;
    if (confirmer) c_action = UIColor.WARNING;
    else if (ok) c_action = UIColor.SUCCESS;
    else if (!lancer && (d.options & OPT_O) && v.actif) c_action = UIColor.TEXT_DIM;  // déjà allumé
    ui_texte_coupe(u.app_action, action, kAppareilActionLargeur);
    ui_text_color(u.app_action, c_action);
}

void popup_appareil_ouvrir(int r, int t) { popup_tuile_ouvrir(s_pa, r, t, popup_appareil_peindre); }

// Un état ou une minuterie a changé : le popup, s'il montre cette tuile.
void popup_appareil_etat(int r, int t) {
    if (s_pa.montre(r, t)) popup_appareil_peindre();
}

// ─── Ce que tab5_tuiles.cpp demande aux trois popups ────────────────────────────────

// Nouvelles définitions des tuiles (tuiles_definir) : popups volet et appareil ouverts sur
// une tuile qui n'a plus ce popup (plus un volet pilotable, tuile devenue lecture seule,
// autre type…) refermés, repeints sinon ; le popup lumière revalidé (ses lignes étaient des
// index de tuiles de l'ancienne définition).
void popups_revalider() {
    popup_tuile_revalider(s_pv, popup_volet_valide(), popup_volet_peindre);
    popup_tuile_revalider(s_pa, popup_appareil_valide(), popup_appareil_peindre);
    popup_lumiere_revalider();
}

// Thèmes (ADR-0029) : les trois popups repeints depuis le dernier état.
void popups_rejouer_theme() {
    if (s_pl.n > 0) popup_lumiere_peindre();
    if (s_pv.ouvert()) popup_volet_peindre();
    if (s_pa.ouvert()) popup_appareil_peindre();
}

// HA a poussé l'état de la tuile tRT (tuiles_etat_recu) : il remplace, dans le popup du
// volet, la position envoyée au relâcher.
void popup_volet_etat_pousse(int r, int t) {
    if (r == s_pv.piece && t == s_pv.tuile) s_pv.cible = false;
}

}  // namespace tuiles

using namespace tuiles;

void popup_lumiere_choisir(int idx) {
    charger();
    const TuilesUI& u = g_tuiles_ui;
    if (idx < 0 || idx >= s_pl.n || u.lum_popup == nullptr) return;
    s_pl.choix = idx;
    s_pl.tuile = s_pl.tuiles[idx];
    if (u.lum_cle != nullptr) lumiere_cle(s_pl.piece, s_pl.tuile, *u.lum_cle);
    popup_lumiere_peindre();
    if (!s_pl.ouvert()) animate_popup_open(u.lum_popup);
}

void popup_volet_commande(const char* action) {
    charger();
    if (action == nullptr || !popup_volet_valide()) return;
    envoyer_tuile(s_pv.piece, s_pv.tuile, action);
}

// Grand bouton du popup d'un appareil : le toucher de SA tuile, par le même chemin.
void popup_appareil_appui() {
    charger();
    if (!popup_appareil_valide()) return;
    tuile_appui_piece(s_pa.piece, s_pa.tuile, false);
}

void tuiles_brancher_popup_volet() {
    static bool fait = false;
    lv_obj_t* cadre = g_tuiles_ui.vol_cadre;
    if (fait || cadre == nullptr || g_tuiles_ui.vol_tablier == nullptr) return;
    fait = true;
    lames_construire(g_tuiles_ui.vol_tablier);
    // Un glissement sur le volet ne remonte pas en geste jusqu'à la page (le swipe des
    // prévisions, sous le popup).
    lv_obj_remove_flag(cadre, LV_OBJ_FLAG_GESTURE_BUBBLE);
    for (lv_event_code_t code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
        lv_obj_add_event_cb(cadre, volet_cadre_rappel, code, nullptr);
}

void popup_lumiere_tout_eteindre() { tuiles_piece_eteindre(s_pl.piece); }
