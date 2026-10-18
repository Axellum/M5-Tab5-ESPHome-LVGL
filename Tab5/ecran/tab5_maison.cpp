/**
 * [AI-CONTEXT]
 * @file tab5_maison.cpp
 * @role Popup « Maison » (ADR-0037, 07/10/2026, discussion #278 : « grouping switches,
 *       blinds etc. in room section; minimalist design ») : toute la maison d'un coup
 *       d'œil, pièce par pièce, comme un tableau de bord Home Assistant, sans passer
 *       les pièces une à une au doigt.
 *         - Une colonne par pièce qui a au moins un appareil, dans l'ordre du blueprint
 *           (Pièce 1 → N : l'index R, pas l'ordre des pages), largeur
 *           (1250 − 24 − 12 × (n − 1)) / n ; un en-tête (le nom de la pièce, « Pièce n »
 *           sans nom), puis une ligne par élément non vide, dans l'ordre des éléments.
 *         - Pages (ADR-0066) : un groupe de cinq colonnes au plus (lot A, plus de cinq
 *           pièces : réparties, sept pièces = 4 + 3, pages_repartir, même largeur de
 *           colonne sur chaque page) × une tranche de cinq lignes (lot B, plus de cinq
 *           éléments dans une pièce : chaque page montre les cinq lignes suivantes de
 *           chaque colonne du groupe). Le glissement gauche / droite (pages_brancher,
 *           ADR-0046) les parcourt toutes. Onglets à côté du titre : un par groupe
 *           (« Pièces 1 à 4 », « n/m » ajouté à celui du groupe affiché quand il a
 *           plusieurs tranches, comme une pièce de plusieurs pages des popups Lumières et
 *           Volets), ou, avec un seul groupe, un par tranche (« 1–5 », « 6–10 »…). Rien
 *           n'est coupé : tout élément a sa ligne sur une page. Les widgets sont des places
 *           (colonne C, ligne K) : s_place dit quelle tuile tRT chacune montre sur la page
 *           affichée.
 *         - Une ligne = la carte du mode HA de sa tuile, couchée : pastille ronde de
 *           l'icône dans la couleur de l'état, nom, ligne d'état (tuile_peindre_ligne,
 *           tab5_tuiles.cpp : les mêmes mots et couleurs), et « ⋯ » quand la tuile a un
 *           appui long. Gestes = ceux de la tuile (tuile_appui_maison).
 *         - « Éteindre les lumières » dans la barre de titre : « Pièce : tout éteindre »
 *           (pR / eteindre) de chaque pièce qui a des lumières.
 *         - Aucune pièce : « Aucun appareil ».
 * @architecture_constraint Rien de nouveau avec Home Assistant : ni donnée, ni commande,
 *       ni service. Widgets en YAML (maison_popup.yaml, gabarits maison_entete.yaml et
 *       maison_ligne.yaml : la règle 7 de tools/check_tab5_code_rules.py y retrouve les
 *       icônes de la palette), disposés et peints ici à chaque ouverture ; ensuite,
 *       seulement s'il est affiché : un état (maison_tuile_changee) repeint sa ligne, des
 *       définitions, des zones ou un thème (maison_definitions_changees, appelée par
 *       tuiles_appliquer_ui) redisposent tout. Fermé, il ne coûte qu'un test.
 *       Popup du registre (ADR-0013, « Maison », POPUP), chrome partagé (ADR-0009) : ce
 *       n'est pas une page (ADR-0002). Un popup ouvert depuis une ligne (lumière, volet,
 *       clim, appareil, télécommande, énergie) passe devant (animate_popup_open) et le
 *       laisse ouvert derrière : sa croix y ramène.
 * @ai_instruction Couleurs : celles des tuiles (UIColor par vue_def) et des styles de rôle
 *       du YAML, jamais une couleur écrite ici. Un texte affiché passe par tr().
 *       Appui long et « ⋯ » d'une ligne : la roue d'actions rapides (ADR-0036) autour de
 *       la pastille de la ligne (enfant 0), sinon le popup de la tuile
 *       (tuile_appui_maison), sans son lien « Maison ». La roue passe devant ce popup
 *       (roue_ouvrir la met au premier plan) ; un toucher hors d'elle ne ferme qu'elle ;
 *       son « Détails » ouvre le popup de la tuile devant Maison, qui reste derrière.
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include "lvgl.h"
#include <algorithm>
#include <cstdio>

MaisonUI g_maison_ui;

namespace {

// Géométrie (maison_popup.yaml, tab5-ui-tokens.yaml) : carte modale de kCarteL px, corps à
// y = kCorpsY sous la barre de titre (ADR-0009 ; kPiecesMax, kTuilesMax, kPlacesTuiles,
// kCarteL, kCarteH, kCorpsY : tab5_geometrie.h), marges et écarts de 12 px entre colonnes.
constexpr int32_t kMarge = 12;
constexpr int32_t kEcart = 12;
// En-tête de colonne (40 px), puis les lignes (104 px, 8 px d'écart). Sur une carte moins
// haute que celle du Tab5 (ADR-0061, lot 2 du portage), les cinq lignes se partagent la
// hauteur qui reste (81 px sur la Waveshare 7B) : posée par disposer(), le YAML dit 104.
constexpr int32_t kEnteteH = 40;
constexpr int32_t kEnteteX = 6;       // le nom, un peu en retrait du bord de la colonne
constexpr int32_t kLigneHTab5 = 104;
constexpr int32_t kLigneEcart = 8;
constexpr int32_t kLignesY = kCorpsY + kEnteteH + kLigneEcart;
constexpr int32_t kLigneHMax = (kCarteH - kMarge - kLignesY - (kMaisonLignes - 1) * kLigneEcart) / kMaisonLignes;
constexpr int32_t kLigneH = kLigneHMax < kLigneHTab5 ? kLigneHMax : kLigneHTab5;
// Dans une ligne (maison_ligne.yaml) : pastille de 56 px à 12 px du bord, textes à 80 px ;
// « ⋯ » de 36 px à 12 px du bord droit, 8 px d'air avant lui ; sans lui, 12 px de marge.
constexpr int32_t kTexteX = 80;
constexpr int32_t kTexteFin = 12;
constexpr int32_t kPlusPlace = 36 + 12 + 8;
// Hauteur de la carte (kCarteH) : la dernière ligne y tient, sur toutes les cartes ; une
// ligne garde la place de sa pastille (56 px) et de ses deux textes.
static_assert(kLignesY + kMaisonLignes * kLigneH + (kMaisonLignes - 1) * kLigneEcart <= kCarteH - kMarge,
              "les cinq lignes d'une page tiennent dans la carte");
static_assert(kLigneH >= 72, "une ligne de la Maison garde sa pastille et ses deux textes");
static_assert(!kMiseEnPageTab5 || kLigneH == kLigneHTab5, "Tab5 : lignes de 104 px");

// Onglets des pages (pages_onglet.yaml, ADR-0046) : sur la ligne du titre, à la place des
// onglets des Réglages (x 318 sur le Tab5, ramenée à la carte par onglet_x), jusqu'au
// bouton « Éteindre les lumières » (320 px à 100 px du bord droit, maison_popup.yaml) moins
// 10 px ; 200 px au plus, 10 px entre deux, le nom à 8 px des bords. Le bouton garde sa
// largeur sur une carte plus étroite : quand la place manque (Waveshare 7B, rendu du
// 18/10/2026 : « Pièces 1 à… »), les onglets commencent plus tôt, jusqu'au début de la zone
// des onglets (kOngletsDebutTab5, après le titre), au lieu de rétrécir.
constexpr int32_t kOngletsX = onglet_x(318);
constexpr int32_t kOngletsXMin = kOngletsDebutTab5;
constexpr int32_t kOngletsFinMaison = kCarteL - 100 - 320 - 10;
constexpr int32_t kOngletEcart = 10;
constexpr int32_t kOngletMarge = 8;
static_assert(kMaisonColonnes == 5 && kMaisonLignes == kPlacesTuiles,
              "les places : cinq colonnes, cinq lignes comme les places de l'accueil");
static_assert(kMaisonGroupesMax * kMaisonColonnes >= kPiecesMax, "toutes les pièces ont un groupe");
static_assert(kMaisonTranchesMax * kMaisonLignes >= kTuilesMax, "tous les éléments ont une ligne");

// Largeur des colonnes de la dernière disposition (les textes d'une ligne en dépendent).
int32_t s_colonne = 0;

// Pièces occupées (ADR-0066 lot A) : dans l'ordre du blueprint, réparties en groupes de
// cinq colonnes au plus (pages_repartir, tab5_core.cpp : sept pièces = 4 + 3).
int s_pieces[kPiecesMax] = {};  // pièce de chaque colonne, tous groupes confondus
int s_nb = 0;                   // colonnes (pièces occupées)
PagesRepartition s_rep;
// Pages (groupe × tranche de cinq lignes, lot B), à plat : le glissement les parcourt.
int s_groupe[kMaisonPagesMax] = {};
int s_tranche[kMaisonPagesMax] = {};
int s_tranches[kMaisonGroupesMax] = {};  // tranches de chaque groupe (sa colonne la plus longue)
int s_nb_pages = 0;
int s_page = 0;  // page affichée
// Onglets montrés et la page qu'ouvre chacun (peindre_onglets).
int s_nb_onglets = 0;
int s_page_onglet[kMaisonOngletsMax] = {};
// La tuile que montre chaque place de la page affichée ({-1, -1} : aucune).
struct Place {
    int r = -1;
    int t = -1;
};
Place s_place[kMaisonColonnes][kMaisonLignes];

bool affiche() {
    const lv_obj_t* p = g_maison_ui.popup;
    return p != nullptr && !lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN);
}

// Widgets d'une ligne, dans l'ordre de ses enfants (maison_ligne.yaml) : pastille (et son
// icône), nom, état, « ⋯ ».
lv_obj_t* enfant(lv_obj_t* o, int i) { return o != nullptr ? lv_obj_get_child(o, i) : nullptr; }

void largeur(lv_obj_t* o, int32_t w) {
    if (o != nullptr && lv_obj_get_style_width(o, LV_PART_MAIN) != w) lv_obj_set_width(o, w);
}

void hauteur(lv_obj_t* o, int32_t h) {
    if (o != nullptr && lv_obj_get_style_height(o, LV_PART_MAIN) != h) lv_obj_set_height(o, h);
}

void cliquable(lv_obj_t* o, bool oui) {
    if (o != nullptr && lv_obj_has_flag(o, LV_OBJ_FLAG_CLICKABLE) != oui) lv_obj_set_flag(o, LV_OBJ_FLAG_CLICKABLE, oui);
}

// La ligne de la tuile tRT, ses gestes déjà lus (tuile_gestes) : « ⋯ » si elle a un appui
// long, pressable si son toucher fait quelque chose, puis le dessin de sa carte du mode HA.
void dessiner_ligne(lv_obj_t* ligne, int r, int t, bool agit, bool appui_long) {
    lv_obj_t* pastille = enfant(ligne, 0);
    ui_hidden(enfant(ligne, 3), !appui_long);
    cliquable(ligne, agit);
    const int32_t texte = s_colonne - kTexteX - (appui_long ? kPlusPlace : kTexteFin);
    tuile_peindre_ligne(r, t, {pastille, enfant(pastille, 0), enfant(ligne, 1), enfant(ligne, 2)}, texte);
}

// Un état arrivé : la place qui montre la tuile tRT (s'il y en a une sur la page affichée),
// ses gestes relus une seule fois, puis son dessin.
void peindre_ligne(int r, int t) {
    for (int c = 0; c < kMaisonColonnes; c++)
        for (int k = 0; k < kMaisonLignes; k++) {
            if (s_place[c][k].r != r || s_place[c][k].t != t) continue;
            lv_obj_t* ligne = g_maison_ui.ligne[c][k];
            bool agit = false, appui_long = false;
            if (ligne != nullptr && tuile_gestes(r, t, agit, appui_long)) dessiner_ligne(ligne, r, t, agit, appui_long);
            return;
        }
}

// Première et dernière pièce (numéros du blueprint) du groupe `g`.
void groupe_bornes(int g, int& premiere, int& derniere) {
    const int debut = g * s_rep.par_page;
    const int fin = std::min(s_nb, debut + s_rep.par_page) - 1;
    premiere = debut < s_nb ? s_pieces[debut] + 1 : 0;
    derniere = fin >= debut ? s_pieces[fin] + 1 : 0;
}

// Première page du groupe `g`.
int premiere_page(int g) {
    for (int p = 0; p < s_nb_pages; p++)
        if (s_groupe[p] == g) return p;
    return 0;
}

// Onglets à côté du titre, aucun sous deux pages ; celui de la page affichée en accent
// (choix_peindre, tab5_pages.cpp). Un seul groupe : un par tranche, « 1–5 », « 6–10 »… ;
// plusieurs groupes : un par groupe, « Pièces 1 à 4 », et « n/m » gardé entier après celui
// du groupe affiché quand il a plusieurs tranches (texte_ha_coupe avec un suffixe).
void peindre_onglets() {
    const MaisonUI& u = g_maison_ui;
    const bool par_tranche = s_rep.pages <= 1;
    const int voulus = s_nb_pages >= 2 ? (par_tranche ? s_nb_pages : s_rep.pages) : 0;
    const int n = std::min(voulus, kMaisonOngletsMax);
    s_nb_onglets = n;
    const int32_t w =
        n > 0 ? std::min<int32_t>(200, (kOngletsFinMaison - kOngletsXMin - (n - 1) * kOngletEcart) / n) : 0;
    const int32_t x0 = std::min<int32_t>(kOngletsX, kOngletsFinMaison - n * w - (n - 1) * kOngletEcart);
    const int g_affiche = s_nb_pages > 0 ? s_groupe[s_page] : 0;
    char nom[48];
    char suffixe[25];  // « n/m » (25 : deux int au pire)
    for (int o = 0; o < kMaisonOngletsMax; o++) {
        lv_obj_t* const b = u.onglet[o];
        ui_hidden(b, o >= n);
        if (o >= n || b == nullptr) continue;
        ui_x(b, x0 + o * (w + kOngletEcart));
        lv_obj_set_width(b, w);  // LVGL 9.5.0 compare lui-même (lv_obj_pos.c)
        const char* sfx = nullptr;
        if (par_tranche) {
            snprintf(nom, sizeof(nom), "%d\xE2\x80\x93%d", o * kMaisonLignes + 1, (o + 1) * kMaisonLignes);
            s_page_onglet[o] = o;
        } else {
            int premiere = 0, derniere = 0;
            groupe_bornes(o, premiere, derniere);
            snprintf(nom, sizeof(nom), tr("Pièces %d à %d"), premiere, derniere);
            s_page_onglet[o] = premiere_page(o);
            if (o == g_affiche && o < kMaisonGroupesMax && s_tranches[o] > 1) {
                snprintf(suffixe, sizeof(suffixe), " %d/%d", s_tranche[s_page] + 1, s_tranches[o]);
                sfx = suffixe;
            }
        }
        texte_ha_coupe(lv_obj_get_child(b, 0), nom, w - 2 * kOngletMarge, sfx);
    }
    if (n > 0) choix_peindre(u.onglet, n, par_tranche ? s_page : g_affiche);
}

// Pièces occupées, groupes, tranches et pages d'après les définitions (la page affichée
// gardée si elle existe toujours), puis la page : colonnes, en-têtes, lignes et tout le
// dessin. Les gestes de chaque élément ne sont lus qu'une fois (tuile_gestes).
void disposer() {
    MaisonUI& u = g_maison_ui;
    struct LigneLue {
        int8_t t;
        bool agit;
        bool appui_long;
    };
    LigneLue lues[kPiecesMax][kTuilesMax];
    int nb_lues[kPiecesMax] = {};
    s_nb = 0;
    bool lumieres = false;
    char titre[48];
    for (int r = 0; r < tuiles_pieces_nombre(); r++) {
        if (!tuiles_piece_titre(r, titre, sizeof(titre))) continue;
        const int c = s_nb++;
        s_pieces[c] = r;
        lumieres = lumieres || tuiles_piece_a_lumieres(r);
        for (int t = 0; t < kTuilesMax; t++) {
            bool agit = false, appui_long = false;
            if (tuile_gestes(r, t, agit, appui_long))
                lues[c][nb_lues[c]++] = LigneLue{static_cast<int8_t>(t), agit, appui_long};
        }
    }
    s_rep = pages_repartir(s_nb, kMaisonColonnes);
    // Pages : chaque groupe, de cinq lignes en cinq (sa colonne la plus longue décide).
    s_nb_pages = 0;
    for (int g = 0; g < s_rep.pages && g < kMaisonGroupesMax; g++) {
        int lignes = 0;
        for (int c = g * s_rep.par_page; c < std::min(s_nb, (g + 1) * s_rep.par_page); c++)
            lignes = std::max(lignes, nb_lues[c]);
        s_tranches[g] = std::max(1, std::min(kMaisonTranchesMax, (lignes + kMaisonLignes - 1) / kMaisonLignes));
        for (int j = 0; j < s_tranches[g] && s_nb_pages < kMaisonPagesMax; j++) {
            s_groupe[s_nb_pages] = g;
            s_tranche[s_nb_pages] = j;
            s_nb_pages++;
        }
    }
    if (s_page >= s_nb_pages) s_page = 0;
    const int g = s_nb_pages > 0 ? s_groupe[s_page] : 0;
    const int premiere_ligne = s_nb_pages > 0 ? s_tranche[s_page] * kMaisonLignes : 0;
    const int debut = g * s_rep.par_page;
    const int n = std::min(s_nb - debut, s_rep.par_page);  // colonnes de cette page
    // Même largeur sur toutes les pages (celle d'une page pleine) : les colonnes restent
    // alignées d'une page à l'autre ; la dernière page peut en avoir une de moins.
    s_colonne = s_rep.par_page > 0 ? (kCarteL - 2 * kMarge - (s_rep.par_page - 1) * kEcart) / s_rep.par_page : 0;
    ui_hidden(u.vide, s_nb > 0);
    for (int c = 0; c < kMaisonColonnes; c++) {
        const bool occupee = c < n;
        const int col = debut + c;
        const int r = occupee ? s_pieces[col] : -1;
        const int32_t x = kMarge + c * (s_colonne + kEcart);
        lv_obj_t* entete = u.entete[c];
        ui_hidden(entete, !occupee);
        if (occupee && entete != nullptr && tuiles_piece_titre(r, titre, sizeof(titre))) {
            const lv_font_t* f = lv_obj_get_style_text_font(entete, LV_PART_MAIN);
            const int32_t h = f != nullptr ? lv_font_get_line_height(f) : 0;
            ui_x(entete, x + kEnteteX);
            ui_y(entete, kCorpsY + (kEnteteH - h) / 2);
            texte_ha_coupe(entete, titre, s_colonne - 2 * kEnteteX);
        }
        // Les lignes de la tranche affichée : les tuiles vides ne laissent pas de trou.
        int k = 0;
        for (int i = premiere_ligne; occupee && i < nb_lues[col] && k < kMaisonLignes; i++, k++) {
            const int t = lues[col][i].t;
            const bool agit = lues[col][i].agit, appui_long = lues[col][i].appui_long;
            s_place[c][k] = Place{r, t};
            lv_obj_t* ligne = u.ligne[c][k];
            ui_hidden(ligne, false);
            if (ligne != nullptr) {
                ui_x(ligne, x);
                ui_y(ligne, kLignesY + k * (kLigneH + kLigneEcart));
                largeur(ligne, s_colonne);
                hauteur(ligne, kLigneH);
                dessiner_ligne(ligne, r, t, agit, appui_long);
            }
        }
        for (; k < kMaisonLignes; k++) {
            s_place[c][k] = Place{};
            ui_hidden(u.ligne[c][k], true);
        }
    }
    peindre_onglets();
    ui_hidden(u.eteindre, !lumieres);
}

// Pages du popup pour la brique commune (pages_brancher, tab5_pages.cpp) : gauche = page
// suivante, droite = précédente, en boucle ; une seule page : pas de glissement.
int nombre_pages() { return s_nb_pages; }
int page_courante() { return s_page; }
void afficher_page(int p) {
    if (p < 0 || p >= s_nb_pages || p == s_page) return;
    s_page = p;
    disposer();
    ui_mark_activity();
}
PagesPopup s_pages;

}  // namespace

void maison_ouvrir() {
    MaisonUI& u = g_maison_ui;
    if (u.popup == nullptr) return;
    s_pages.popup = u.popup;
    s_pages.nombre = nombre_pages;
    s_pages.courante = page_courante;
    s_pages.afficher = afficher_page;
    pages_brancher(&s_pages);  // rappelé à chaque ouverture : sans effet de plus
    // La première page du groupe de la pièce montrée (un tap sur son titre, mode HA ou page
    // de pièce de la météo), sinon la première.
    s_page = 0;
    disposer();
    const int r_ha = tuiles_piece_montree();
    for (int i = 0; i < s_nb && s_rep.par_page > 0; i++) {
        if (s_pieces[i] != r_ha) continue;
        const int p = premiere_page(i / s_rep.par_page);
        if (p != s_page) {
            s_page = p;
            disposer();
        }
        break;
    }
    animate_popup_open(u.popup);
    ui_mark_activity();
}

bool maison_titre_appui_valide() {
    // Mode HA, ou une page de pièce de la météo (ADR-0066) : le titre est celui d'une pièce.
    if (!g_central_ctx.ha_mode && g_central_ctx.forecast_page < kPagesMeteo) return false;
    // Un tap au bout d'un glissement (swipe des pièces) : rien, comme l'appui long qui
    // ouvre les alertes (alertes_ouvrir).
    return !ui_appui_glisse();
}

void maison_ligne_appui(int c, int k, bool long_appui) {
    if (c < 0 || c >= kMaisonColonnes || k < 0 || k >= kMaisonLignes) return;
    const Place pl = s_place[c][k];
    if (pl.r < 0) return;
    lv_obj_t* ligne = g_maison_ui.ligne[c][k];
    tuile_appui_maison(pl.r, pl.t, long_appui, ligne != nullptr ? enfant(ligne, 0) : nullptr);
}

void maison_page(int onglet) {
    if (onglet >= 0 && onglet < s_nb_onglets) afficher_page(s_page_onglet[onglet]);
}

void maison_eteindre_lumieres() {
    for (int r = 0; r < tuiles_pieces_nombre(); r++)
        if (tuiles_piece_a_lumieres(r)) tuiles_piece_eteindre(r);
}

void maison_tuile_changee(int r, int t) {
    if (!affiche() || r < 0 || r >= kPiecesMax || t < 0 || t >= kTuilesMax) return;
    peindre_ligne(r, t);
}

void maison_definitions_changees() {
    if (affiche()) disposer();
}
