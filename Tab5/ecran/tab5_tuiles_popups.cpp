/**
 * [AI-CONTEXT]
 * @file tab5_tuiles_popups.cpp
 * @role Popups d'une tuile (sorti de tab5_tuiles.cpp le 08/10/2026, lot L7 de l'audit du
 *       07/10/2026) : Lumières et Volets (à pages par pièce depuis le 09/10/2026,
 *       ADR-0046 : toutes les lumières, tous les volets de la maison, une page par pièce
 *       qui en a, PopupPieces ; à droite, la lumière choisie — arc, couleurs — ou le volet
 *       choisi — volet dessiné qu'on fait glisser, envoyé au relâcher, Ouvrir / Stop /
 *       Fermer) et appareil (appui long d'une int, d'une act ou d'une med sans l'option t,
 *       06/10/2026 : la fenêtre « plus d'infos » d'un tableau de bord HA, dont le grand
 *       bouton refait le toucher de la tuile). Leur base commune : PopupTuile (pièce,
 *       tuile, ouvert, revalidation).
 * @architecture_constraint Lit le modèle des tuiles par tab5_tuiles_priv.h (s_m, s_etats,
 *       vue_def, gestes…) et n'envoie rien par lui-même : envoyer_tuile / tuile_appui_piece
 *       (tab5_tuiles.cpp). Repeints quand l'état de leur tuile change (peindre_tuile →
 *       popup_*_etat), revalidés quand les définitions changent (tuiles_definir →
 *       popups_revalider), repeints au changement de thème (popups_rejouer_theme).
 *       Widgets : ui_components/light_popup.yaml, volet_popup.yaml, appareil_popup.yaml,
 *       piece_ligne.yaml (lignes), pages_onglet.yaml (noms des pièces), posés dans
 *       g_tuiles_ui par tab5-tuiles.yaml. Le glissement de pièce en pièce : la brique des
 *       popups à pages (tab5_pages.cpp), branchée par tuiles_brancher_popup_volet.
 * @ai_instruction Un texte affiché passe par tr() ; un nom venu de HA s'affiche tel quel
 *       (ui_texte_coupe). Glyphes posés d'ici (règle 7) : glyphe_commande, et ceux des
 *       lignes par tuile_peindre_ligne (palette des tuiles, heritage_glyphe_carte) :
 *       MDI_CODE_TARGETS de check_tab5_code_rules.py (icon_light_sel_*, volet_ligne_*_icone).
 */
#include "tab5_tuiles_priv.h"
#include "tab5_geometrie.h"
#include "tab5_modele_ha.h"
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
    bool en_grille() const { return piece >= 0 && piece < kPiecesMax && tuile >= 0 && tuile < kTuilesMax && !heritage(); }
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

// ─── Popups à pages par pièce : Lumières et Volets (ADR-0046) ───────────────────────
//
// Demande d'Axel du 09/10/2026 : les popups Lumières et Volets montrent toutes les
// lumières (tous les volets) de la maison, une page par pièce qui en a ; on passe de l'une
// à l'autre d'un glissement ou d'un tap sur son nom, en haut (la brique des popups à
// pages, tab5_pages.cpp). Depuis l'ADR-0066 (vingt éléments par pièce), une pièce qui a plus
// de lignes qu'une page n'en tient (kPlacesTuiles) prend plusieurs pages, de cinq lignes en
// cinq : rien n'est coupé, tout élément s'atteint. PopupPieces<C> garde les pages, les
// lignes de la page affichée et la ligne choisie (piece, tuile : celles que pilote la partie
// droite du popup) ; chaque popup dit quelle tuile en est une ligne (`garde`) et peint sa
// partie droite. Les lignes sont la carte du mode HA couchée (tuile_peindre_ligne), comme
// celles du popup Maison.

// Pages au plus : chaque pièce, de cinq lignes en cinq (vingt éléments = quatre pages).
constexpr int kPagesParPiece = (kTuilesMax + kPlacesTuiles - 1) / kPlacesTuiles;
constexpr int kPagesMax = kPiecesMax * kPagesParPiece;

template <lv_obj_t* TuilesUI::* C>
struct PopupPieces : PopupTuile<C> {
    int nb = 0;                       // pages : les pièces qui ont au moins une ligne, de cinq en cinq
    int pieces[kPagesMax] = {};       // pièce de chaque page, dans l'ordre du blueprint
    int debuts[kPagesMax] = {};       // rang, parmi les lignes de sa pièce, de la première de la page
    int page = 0;                     // page affichée : piece = pieces[page]
    int n = 0;                        // lignes de la page affichée
    int tuiles[kPlacesTuiles] = {};   // tuile de chaque ligne, dans l'ordre des tuiles
    int choix = 0;                    // ligne choisie : tuile = tuiles[choix]
    int nb_onglets = 0;               // noms en haut (onglets_peindre)
    int page_onglet[kPiecesMax] = {};  // page qu'ouvre le tap sur chaque nom
};

// Pages et lignes de `p` : les pièces qui ont au moins une tuile `garde`, de cinq lignes en
// cinq ; la page de la tuile `t` de la pièce `r` (sinon la première page de la pièce, sinon
// la première), ses lignes, celle de `t` choisie (sinon la première). Faux, et `p` sans page,
// si aucune pièce n'en a. Ne touche qu'aux champs de PopupPieces : ce que le popup garde en
// plus (le doigt sur le volet) reste.
template <class P>
bool pieces_calculer(P& p, bool (*garde)(int, int), int r, int t) {
    p.nb = p.n = p.page = p.choix = 0;
    p.piece = p.tuile = -1;
    int page_r = -1, page_t = -1;
    for (int i = 0; i < kPiecesMax; i++) {
        int k = 0;  // lignes de la pièce i
        for (int j = 0; j < kTuilesMax; j++) {
            if (!garde(i, j)) continue;
            if (k % kPlacesTuiles == 0 && p.nb < kPagesMax) {
                p.pieces[p.nb] = i;
                p.debuts[p.nb] = k;
                p.nb++;
            }
            if (i == r && page_r < 0) page_r = p.nb - 1;
            if (i == r && j == t) page_t = p.nb - 1;
            k++;
        }
    }
    if (p.nb == 0) return false;
    p.page = page_t >= 0 ? page_t : (page_r >= 0 ? page_r : 0);
    p.piece = p.pieces[p.page];
    for (int j = 0, k = 0; j < kTuilesMax && p.n < kPlacesTuiles; j++) {
        if (!garde(p.piece, j) || k++ < p.debuts[p.page]) continue;
        if (j == t) p.choix = p.n;
        p.tuiles[p.n++] = j;
    }
    p.tuile = p.tuiles[p.choix];
    return true;
}

// Une ligne (piece_ligne.yaml) : la tuile tRT avec les mots, l'icône et les couleurs de sa
// carte du mode HA, la ligne choisie entourée d'accent. Enfants lus ici : 0 la pastille
// (son enfant 0 : l'icône), 1 le nom, 2 l'état. Texte : la ligne (kPieceLigneL, 342 sur le
// Tab5) − 86 (pastille) − 12 de marge.
constexpr int32_t kLigneTexte = kPieceLigneL - 98;
void ligne_peindre(lv_obj_t* b, int r, int t, bool choisie) {
    if (b == nullptr) return;
    lv_obj_t* pastille = lv_obj_get_child(b, 0);
    const TuileWidgets w{pastille, pastille != nullptr ? lv_obj_get_child(pastille, 0) : nullptr,
                         lv_obj_get_child(b, 1), lv_obj_get_child(b, 2)};
    tuile_peindre_ligne(r, t, w, kLigneTexte);
    highlight_button_border(b, choisie, UIColor.ACCENT, 3);
}

// Les lignes de la page affichée ; celles d'au-delà masquées.
template <class P>
void lignes_peindre(const P& p, lv_obj_t* const* lignes) {
    for (int i = 0; i < kPlacesTuiles; i++) {
        const bool visible = i < p.n;
        ui_hidden(lignes[i], !visible);
        if (visible) ligne_peindre(lignes[i], p.piece, p.tuiles[i], i == p.choix);
    }
}

// La ligne de la tuile `t` si la page la montre (un état a changé).
template <class P>
void ligne_de_la_tuile(const P& p, lv_obj_t* const* lignes, int t) {
    for (int i = 0; i < p.n; i++)
        if (p.tuiles[i] == t) ligne_peindre(lignes[i], p.piece, t, i == p.choix);
}

// Les noms en haut (pages_onglets : aucun sous deux), celui de la page affichée en accent.
// Un nom par pièce (celui de la carte centrale en mode HA, tuiles_piece_titre) ; une pièce
// de plusieurs pages ajoute « n/m » au sien quand elle est affichée, et le tap sur un nom
// ouvre la première page de sa pièce. Une seule pièce, de plusieurs pages : un nom par page
// (« Salon 1/2 », « Salon 2/2 »), sinon il n'y aurait rien en haut.
template <class P>
void onglets_peindre(P& p, lv_obj_t* const* onglets) {
    char noms[kPiecesMax][48];   // un nom de HA (kNom) ou « Pièce n » traduit
    char pages[kPiecesMax][25];  // « n/m », gardé entier quand le nom se coupe (25 : deux int au pire)
    const char* ptr[kPiecesMax] = {};
    const char* sfx[kPiecesMax] = {};
    int pieces = 0;
    for (int i = 0; i < p.nb; i++)
        if (i == 0 || p.pieces[i] != p.pieces[i - 1]) pieces++;
    const bool par_page = pieces == 1 && p.nb > 1;
    int courant = 0;
    p.nb_onglets = 0;
    for (int i = 0; i < p.nb && p.nb_onglets < kPiecesMax; i++) {
        const int r = p.pieces[i];
        const bool premiere = i == 0 || r != p.pieces[i - 1];
        if (!par_page && !premiere) continue;
        int debut = i, fin = i;  // pages de la pièce r : de debut à fin
        while (debut > 0 && p.pieces[debut - 1] == r) debut--;
        while (fin + 1 < p.nb && p.pieces[fin + 1] == r) fin++;
        const int o = p.nb_onglets++;
        char* nom = noms[o];
        if (!tuiles_piece_titre(r, nom, sizeof(noms[o]))) nom[0] = '\0';
        // La page dont le nom dit « n/m » : la sienne (un nom par page), sinon celle affichée.
        const int montree = par_page ? i : (r == p.piece ? p.page : -1);
        if (fin > debut && montree >= 0) {
            snprintf(pages[o], sizeof(pages[o]), " %d/%d", montree - debut + 1, fin - debut + 1);
            sfx[o] = pages[o];
        }
        if (par_page ? i == p.page : r == p.piece) courant = o;
        p.page_onglet[o] = par_page ? i : debut;
        ptr[o] = nom;
    }
    pages_onglets(onglets, kPiecesMax, ptr, p.nb_onglets, courant, sfx);
}

// La page qu'ouvre le tap sur le nom `onglet` ; -1 s'il n'y en a pas.
template <class P>
int page_de_onglet(const P& p, int onglet) {
    return onglet >= 0 && onglet < p.nb_onglets ? p.page_onglet[onglet] : -1;
}

// La première tuile de la page `page` de `p` (pieces_calculer retrouve la page par elle) ;
// -1 si les définitions ont changé et qu'elle n'existe plus.
template <class P>
int premiere_tuile(const P& p, bool (*garde)(int, int), int page) {
    if (page < 0 || page >= p.nb) return -1;
    const int r = p.pieces[page];
    for (int j = 0, k = 0; j < kTuilesMax; j++) {
        if (!garde(r, j)) continue;
        if (k++ == p.debuts[page]) return j;
    }
    return -1;
}

// ─── Popup Lumières : les lumières de la maison, pièce par pièce ───────────────────

struct PopupLumiere : PopupPieces<&TuilesUI::lum_popup> {};
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

// Tout le popup : noms des pièces, lignes de la page affichée (la choisie entourée), arc
// de la ligne choisie. Le titre ne bouge plus (« Lumières ») : la lumière choisie est
// celle qui est entourée.
void popup_lumiere_peindre() {
    const TuilesUI& u = g_tuiles_ui;
    if (u.lum_popup == nullptr) return;
    onglets_peindre(s_pl, u.lum_onglet);
    lignes_peindre(s_pl, u.lum_ligne);
    popup_lumiere_arc();
}

// Pages et lignes du popup (pieces_calculer) : la page de la pièce `r`, la ligne de la
// tuile `t`, et la clé des commandes de la partie droite. Faux si la maison n'a aucune
// lumière.
bool popup_lumiere_lignes(int r, int t) {
    const TuilesUI& u = g_tuiles_ui;
    if (!pieces_calculer(s_pl, est_lumiere, r, t)) {
        s_pl = PopupLumiere{};
        return false;
    }
    if (u.lum_cle != nullptr) lumiere_cle(s_pl.piece, s_pl.tuile, *u.lum_cle);
    return true;
}

// Le popup sur la pièce `r` et la lumière `t` (appui long d'une tuile, « Détails » de sa
// roue ; -1 : la première de la pièce, ou de la première pièce qui en a).
void popup_lumiere_ouvrir(int r, int t) {
    if (PopupLumiere::conteneur() == nullptr || !popup_lumiere_lignes(r, t)) return;
    popup_lumiere_peindre();
    animate_popup_open(PopupLumiere::conteneur());
}

// Nouvelles définitions des tuiles (UI-1, audit du 07/10/2026), comme les popups volet et
// appareil : ouvert, pages et lignes sont recalculées — la pièce et la lampe choisies le
// restent si elles ont encore des lumières, sinon la première — puis le popup est
// repeint ; plus aucune lumière dans la maison : refermé. Fermé (y compris par sa croix,
// que le C++ ne voit pas), il est oublié : aucun index ne vise plus une tuile disparue.
void popup_lumiere_revalider() {
    if (!s_pl.ouvert()) {
        s_pl = PopupLumiere{};
        return;
    }
    const int choisie = s_pl.n > 0 ? s_pl.tuile : -1;
    popup_tuile_revalider(s_pl, popup_lumiere_lignes(s_pl.piece, choisie), popup_lumiere_peindre);
}

// Un état a changé : la ligne de cette tuile si la page la montre, l'arc si c'est la
// lumière choisie.
void popup_lumiere_etat(int r, int t) {
    if (!s_pl.ouvert() || r != s_pl.piece) return;
    ligne_de_la_tuile(s_pl, g_tuiles_ui.lum_ligne, t);
    if (t == s_pl.tuile) popup_lumiere_arc();
}

// La page `page` (glissement, tap sur un nom) : sa pièce, ses lignes, la première choisie.
// Plus aucune lumière (définitions changées entre deux appels) : refermé.
void lumieres_afficher(int page) {
    charger();
    if (!s_pl.ouvert() || page < 0 || page >= s_pl.nb || page == s_pl.page) return;
    if (!popup_lumiere_lignes(s_pl.pieces[page], premiere_tuile(s_pl, est_lumiere, page))) {
        animate_popup_close(PopupLumiere::conteneur());
        return;
    }
    popup_lumiere_peindre();
}

// Brique des popups à pages (tab5_pages.cpp) : le glissement de page en page.
int lumieres_nombre() { return s_pl.nb; }
int lumieres_page() { return s_pl.ouvert() && s_pl.nb > 0 ? s_pl.page : -1; }
PagesPopup s_pages_lumieres{nullptr, lumieres_nombre, lumieres_page, lumieres_afficher};

// ─── Popup Volets (05/10/2026, discussion #278 ; à pages par pièce, ADR-0046) ───────
//
// Les volets de la maison, pièce par pièce ; à droite, le volet choisi : un volet dessiné
// (06/10/2026, même discussion : « like ha animation, not a basic slider ») qu'on fait
// glisser du doigt (position connue seulement, envoyée au relâcher : action « position »),
// son nom, la position en grand, l'état en mots et Ouvrir / Stop / Fermer (les commandes
// de la tuile). Tant qu'il est ouvert, il suit l'état des volets de la page
// (peindre_tuile → popup_volet_etat) : chaque position poussée par HA redessine le
// tablier directement, sans interpolation ni fondu (préférence d'Axel : transitions
// instantanées) — le volet dessiné descend quand le vrai descend, au rythme des poussées.

struct PopupVolet : PopupPieces<&TuilesUI::vol_popup> {
    bool saisi = false;   // un doigt tient le volet (position connue à l'appui)
    bool glisse = false;  // il a glissé au-delà du seuil : son relâcher envoie la position
    bool cible = false;   // position envoyée : dessinée jusqu'au prochain état de HA
    bool estompe = false; // position dessinée = un repère (position inconnue)
    int32_t y_appui = 0;  // ordonnée du doigt à l'appui (écran)
    int pos_appui = 0;    // position dessinée à l'appui
    int pos = 0;          // position dessinée : 0 fermé, 100 ouvert
};
PopupVolet s_pv;

// Géométrie (volet_popup.yaml) : à droite du volet dessiné (x 392..796 de la carte), le
// nom en haut (y 56), la position (y 104, chiffres de 130 px), l'état dessous, ou seul
// entre le nom et les commandes (y 404) quand la position n'est pas connue. Valeurs du
// Tab5, ramenées au corps de la carte (lot 4 du portage, tab5_geometrie.h).
constexpr int32_t kVoletEtatSous = 56 + hauteur_carte(224);
constexpr int32_t kVoletEtatSeul = 56 + hauteur_carte(144);
constexpr int32_t kVoletNomLargeur = largeur_carte(380);
// Volet dessiné : hauteur de la fenêtre et du tablier (volet_fenetre, volet_tablier de
// volet_popup.yaml, tests/test_tuiles_firmware.py compare) : 12 lames entières dans le
// corps moins 142 px (y 56, coffre et bas du cadre, 32 px d'air) ; 38 px sur le Tab5.
constexpr int kVoletLames = 12;
constexpr int32_t kVoletLameH = (kCorpsH - 142) / kVoletLames;
constexpr int32_t kVoletFenetreH = kVoletLames * kVoletLameH;
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

// Un volet du popup : tuile vol sans l'option r (lecture seule) ni k (ses commandes
// passent par la confirmation de sa tuile, qui n'ouvre pas ce popup). Le mode héritage
// n'ouvre jamais ce popup (son volet 3.x a son propre sens, tuiles_heritage_volet_sens).
bool est_volet(int r, int t) {
    if (heritage() || !tuile_presente(r, t)) return false;
    const Def& d = s_m.tuiles[r][t];
    return d.type == static_cast<uint8_t>(Type::VOL) && !(d.options & (OPT_R | OPT_K));
}

// Le volet choisi est-il toujours un volet du popup ? (les définitions peuvent changer
// popup ouvert.)
bool popup_volet_valide() {
    return s_pv.en_grille() && est_volet(s_pv.piece, s_pv.tuile);
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

// Le volet choisi : nom, volet dessiné, position (rangée « 45 % », ou rien), état en mots.
void popup_volet_peindre() {
    const TuilesUI& u = g_tuiles_ui;
    if (u.vol_popup == nullptr || !popup_volet_valide()) return;
    const int r = s_pv.piece, t = s_pv.tuile;
    const Etat& e = s_etats[r][t];
    ui_texte_coupe(u.vol_nom, s_m.tuiles[r][t].nom, kVoletNomLargeur);
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

// Un autre volet choisi : le doigt et la position envoyée du précédent oubliés (le
// dessin reprend l'état de HA du nouveau). Le seul autre oubli de la position envoyée :
// l'état poussé par HA (popup_volet_etat_pousse).
void volet_doigt_oublier() {
    s_pv.saisi = false;
    s_pv.glisse = false;
    s_pv.cible = false;
}

// Tout le popup : noms des pièces, lignes de la page affichée, volet choisi.
void popup_volet_page_peindre() {
    const TuilesUI& u = g_tuiles_ui;
    if (u.vol_popup == nullptr) return;
    onglets_peindre(s_pv, u.vol_onglet);
    lignes_peindre(s_pv, u.vol_ligne);
    popup_volet_peindre();
}

// Le popup sur la pièce `r` et le volet `t` (-1 : le premier de la pièce, ou de la
// première pièce qui en a) : le doigt, la position envoyée et le dessin d'avant oubliés,
// comme à chaque changement de volet choisi. Faux si la maison n'a aucun volet.
bool popup_volet_montrer(int r, int t) {
    s_pv = PopupVolet{};
    if (!pieces_calculer(s_pv, est_volet, r, t)) return false;
    popup_volet_page_peindre();
    return true;
}

// Appui long d'une tuile vol, « Détails » de sa roue, « Aller à l'écran → Volets ».
void popup_volet_ouvrir(int r, int t) {
    if (PopupVolet::conteneur() == nullptr || !popup_volet_montrer(r, t)) return;
    animate_popup_open(PopupVolet::conteneur());
}

// Nouvelles définitions, popup ouvert : pages et lignes recalculées (la pièce et le volet
// choisis le restent s'ils sont encore là, sinon le premier) ; le doigt et la position
// envoyée restent si c'est le même volet. Plus aucun volet : refermé.
void popup_volet_revalider() {
    if (!s_pv.ouvert()) return;
    const int r = s_pv.piece, t = s_pv.tuile;
    if (!pieces_calculer(s_pv, est_volet, r, t)) {
        animate_popup_close(PopupVolet::conteneur());
        return;
    }
    if (s_pv.piece != r || s_pv.tuile != t) volet_doigt_oublier();
    popup_volet_page_peindre();
}

// Un état a changé : la ligne de cette tuile si la page la montre, et le volet dessiné
// si c'est le volet choisi.
void popup_volet_etat(int r, int t) {
    if (!s_pv.ouvert() || r != s_pv.piece) return;
    ligne_de_la_tuile(s_pv, g_tuiles_ui.vol_ligne, t);
    if (s_pv.montre(r, t)) popup_volet_peindre();
}

// La page `page` (glissement, tap sur un nom). Plus aucun volet : refermé.
void volets_afficher(int page) {
    charger();
    if (!s_pv.ouvert() || page < 0 || page >= s_pv.nb || page == s_pv.page) return;
    if (!popup_volet_montrer(s_pv.pieces[page], premiere_tuile(s_pv, est_volet, page)))
        animate_popup_close(PopupVolet::conteneur());
}

// Brique des popups à pages (tab5_pages.cpp) : le glissement de page en page. Un
// glissement sur le volet dessiné le règle et ne remonte pas jusque-là (son cadre garde
// le geste, tuiles_brancher_popup_volet).
int volets_nombre() { return s_pv.nb; }
int volets_page() { return s_pv.ouvert() && s_pv.nb > 0 ? s_pv.page : -1; }
PagesPopup s_pages_volets{nullptr, volets_nombre, volets_page, volets_afficher};

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
// (scène, script, bouton). Valeurs du Tab5, ramenées au corps de la carte (lot 4 du
// portage, ADR-0061 ; tests/test_appareil_popup.py les compare au YAML).
constexpr int32_t kAppareilTitreLargeur = largeur_carte(1000);
constexpr int32_t kAppareilTexteLargeur = largeur_carte(800) - 60;
constexpr int32_t kAppareilActionLargeur = kCorpsW - largeur_carte(816) - 36;
constexpr int32_t kAppareilBoutonHauteur = hauteur_carte(380);

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

// Nouvelles définitions des tuiles (tuiles_definir) : le popup d'un appareil ouvert sur
// une tuile qui n'a plus ce popup (tuile devenue lecture seule, autre type…) refermé,
// repeint sinon ; les popups Lumières et Volets revalidés (leurs pages et lignes étaient
// des index de pièces et de tuiles de l'ancienne définition).
void popups_revalider() {
    popup_volet_revalider();
    popup_tuile_revalider(s_pa, popup_appareil_valide(), popup_appareil_peindre);
    popup_lumiere_revalider();
}

// Thèmes (ADR-0029) : les trois popups repeints depuis le dernier état.
void popups_rejouer_theme() {
    if (s_pl.ouvert() && s_pl.n > 0) popup_lumiere_peindre();
    if (s_pv.ouvert()) popup_volet_page_peindre();
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
    if (idx < 0 || idx >= s_pl.n || !s_pl.ouvert()) return;
    s_pl.choix = idx;
    s_pl.tuile = s_pl.tuiles[idx];
    if (u.lum_cle != nullptr) lumiere_cle(s_pl.piece, s_pl.tuile, *u.lum_cle);
    lignes_peindre(s_pl, u.lum_ligne);
    popup_lumiere_arc();
}

void popup_lumiere_page(int onglet) { lumieres_afficher(page_de_onglet(s_pl, onglet)); }

// Toucher de la pastille d'une ligne : le toucher de sa tuile (allumer / éteindre, avec la
// confirmation de l'option k) ; appui long : sa roue, ancrée sur la pastille, sinon le
// chemin de sa tuile (tuile_appui_maison, comme une ligne du popup Maison).
void popup_lumiere_ligne_appui(int idx, bool long_appui) {
    charger();
    if (idx < 0 || idx >= s_pl.n || !s_pl.ouvert()) return;
    lv_obj_t* ligne = g_tuiles_ui.lum_ligne[idx];
    tuile_appui_maison(s_pl.piece, s_pl.tuiles[idx], long_appui, ligne != nullptr ? lv_obj_get_child(ligne, 0) : nullptr);
}

void popup_volet_choisir(int idx) {
    charger();
    if (idx < 0 || idx >= s_pv.n || !s_pv.ouvert() || idx == s_pv.choix) return;
    s_pv.choix = idx;
    s_pv.tuile = s_pv.tuiles[idx];
    volet_doigt_oublier();
    lignes_peindre(s_pv, g_tuiles_ui.vol_ligne);
    popup_volet_peindre();
}

void popup_volet_page(int onglet) { volets_afficher(page_de_onglet(s_pv, onglet)); }

// Toucher de la pastille : le toucher de la tuile (le sens du volet : ouvrir, fermer, ou
// arrêter un volet en route) ; appui long : sa roue, sinon le chemin de sa tuile.
void popup_volet_ligne_appui(int idx, bool long_appui) {
    charger();
    if (idx < 0 || idx >= s_pv.n || !s_pv.ouvert()) return;
    lv_obj_t* ligne = g_tuiles_ui.vol_ligne[idx];
    tuile_appui_maison(s_pv.piece, s_pv.tuiles[idx], long_appui, ligne != nullptr ? lv_obj_get_child(ligne, 0) : nullptr);
}

void popup_volet_commande(const char* action) {
    charger();
    if (action == nullptr || !popup_volet_valide()) return;
    envoyer_tuile(s_pv.piece, s_pv.tuile, action);
}

// « Tout ouvrir » / « Tout fermer » : la commande à chaque volet de la pièce affichée (toutes
// ses pages, ADR-0066), une action par volet (esphome.tab5_action), comme son bouton.
void popup_volets_tout(const char* action) {
    charger();
    if (action == nullptr || !s_pv.ouvert()) return;
    for (int t = 0; t < kTuilesMax; t++)
        if (est_volet(s_pv.piece, t)) envoyer_tuile(s_pv.piece, t, action);
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
    // De pièce en pièce d'un glissement (ADR-0046), les deux popups à pages par pièce.
    s_pages_volets.popup = g_tuiles_ui.vol_popup;
    pages_brancher(&s_pages_volets);
    s_pages_lumieres.popup = g_tuiles_ui.lum_popup;
    pages_brancher(&s_pages_lumieres);
}

void popup_lumiere_tout_eteindre() { tuiles_piece_eteindre(s_pl.piece); }
