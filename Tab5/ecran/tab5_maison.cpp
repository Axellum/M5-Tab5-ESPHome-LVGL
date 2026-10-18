/**
 * [AI-CONTEXT]
 * @file tab5_maison.cpp
 * @role Popup « Maison » (ADR-0037, 07/10/2026, discussion #278 : « grouping switches,
 *       blinds etc. in room section; minimalist design ») : toute la maison d'un coup
 *       d'œil, pièce par pièce, comme un tableau de bord Home Assistant, sans passer
 *       les cinq pièces une à une au doigt.
 *         - Une colonne par pièce qui a au moins un appareil, dans l'ordre du blueprint
 *           (Pièce 1 → 5 : l'index R, pas l'ordre des pages), largeur
 *           (1250 − 24 − 12 × (n − 1)) / n ; un en-tête (le nom de la pièce, « Pièce n »
 *           sans nom), puis une ligne par tuile non vide, dans l'ordre des tuiles.
 *         - Cinq lignes par colonne et par page (ADR-0066 : vingt éléments par pièce) : une
 *           pièce de plus de cinq éléments donne des pages au popup, chacune les cinq
 *           lignes suivantes de chaque colonne (« 1–5 », « 6–10 »… en haut, entre le titre
 *           et « Éteindre les lumières » ; glissement de la brique des popups à pages,
 *           ADR-0046). Rien n'est coupé : tout élément a sa ligne sur une page.
 *         - Une ligne = la carte du mode HA de sa tuile, couchée : pastille ronde de
 *           l'icône dans la couleur de l'état, nom, ligne d'état (tuile_peindre_ligne,
 *           tab5_tuiles.cpp : les mêmes mots et couleurs), et « ⋯ » quand la tuile a un
 *           appui long. Gestes = ceux de la tuile (tuile_appui_maison).
 *         - « Éteindre les lumières » dans la barre de titre : « Pièce : tout éteindre »
 *           (pR / eteindre) de chaque pièce qui a des lumières.
 *         - Aucune pièce : « Aucun appareil ».
 * @architecture_constraint Rien de nouveau avec Home Assistant : ni donnée, ni commande,
 *       ni service. Widgets en YAML (maison_popup.yaml, gabarits maison_entete.yaml,
 *       maison_ligne.yaml — la ligne [R][K] : la K-ième ligne de la colonne R sur la page
 *       affichée, s_elements dit quel élément — et pages_onglet.yaml : la règle 7 de
 *       tools/check_tab5_code_rules.py y retrouve les icônes de la palette), disposés et
 *       peints ici à chaque ouverture (première page) ; ensuite,
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
// y = kCorpsY sous la barre de titre (ADR-0009 ; kPieces, kTuilesMax, kPlacesTuiles,
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
// Lignes d'une colonne sur une page (cinq, comme les places de l'accueil) ; pages au plus :
// vingt éléments par pièce (ADR-0066), de cinq en cinq.
constexpr int kLignesPage = kPlacesTuiles;
constexpr int kPagesMax = (kTuilesMax + kLignesPage - 1) / kLignesPage;
constexpr int32_t kLigneHMax = (kCarteH - kMarge - kLignesY - (kLignesPage - 1) * kLigneEcart) / kLignesPage;
constexpr int32_t kLigneH = kLigneHMax < kLigneHTab5 ? kLigneHMax : kLigneHTab5;
// Dans une ligne (maison_ligne.yaml) : pastille de 56 px à 12 px du bord, textes à 80 px ;
// « ⋯ » de 36 px à 12 px du bord droit, 8 px d'air avant lui ; sans lui, 12 px de marge.
constexpr int32_t kTexteX = 80;
constexpr int32_t kTexteFin = 12;
constexpr int32_t kPlusPlace = 36 + 12 + 8;
// Hauteur de la carte (kCarteH) : la dernière ligne y tient, sur toutes les cartes ; une
// ligne garde la place de sa pastille (56 px) et de ses deux textes.
static_assert(kLignesY + kLignesPage * kLigneH + (kLignesPage - 1) * kLigneEcart <= kCarteH - kMarge,
              "les cinq lignes d'une pièce tiennent dans la carte");
static_assert(kLigneH >= 72, "une ligne de la Maison garde sa pastille et ses deux textes");
static_assert(!kMiseEnPageTab5 || kLigneH == kLigneHTab5, "Tab5 : lignes de 104 px");

// Noms des pages (pages_onglet.yaml) : sur la ligne du titre, de la fin du titre (+24) à
// « Éteindre les lumières » (16 px avant lui) ou, sans lui, à la fin des onglets de toutes
// les cartes (kOngletsFin) ; 130 px au plus, 60 au moins, 10 px d'écart, calés à droite.
constexpr int32_t kTitreX = 52;  // x du titre (modal_header.yaml)
constexpr int32_t kOngletL = 130;
constexpr int32_t kOngletEcart = 10;
// « Éteindre les lumières » (maison_popup.yaml) : 320 px, 100 px du bord droit de la carte.
constexpr int32_t kEteindreX = kCarteL - 100 - 320;
static_assert(kOngletsFin <= kCarteL - 90, "les onglets s'arrêtent avant la croix");

// Largeur des colonnes de la dernière disposition (les textes d'une ligne en dépendent).
int32_t s_colonne = 0;
// Page affichée, nombre de pages, et l'élément T de chaque ligne [R][K] de la page (-1 :
// ligne vide) : maison_ligne_appui et maison_tuile_changee le relisent.
int s_page = 0;
int s_pages = 1;
int8_t s_elements[kPieces][kLignesPage];

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

// Un état arrivé : relit les gestes de la tuile (une seule fois) puis redessine sa ligne,
// si la page affichée la montre.
void peindre_ligne(int r, int t) {
    for (int k = 0; k < kLignesPage; k++) {
        if (s_elements[r][k] != t) continue;
        lv_obj_t* ligne = g_maison_ui.ligne[r][k];
        bool agit = false, appui_long = false;
        if (ligne != nullptr && tuile_gestes(r, t, agit, appui_long)) dessiner_ligne(ligne, r, t, agit, appui_long);
    }
}

// Fin du titre « Maison » (sa police, son texte traduit) : là où les noms des pages peuvent
// commencer.
int32_t fin_du_titre() {
    lv_obj_t* const t = g_maison_ui.titre;
    const lv_font_t* police = t != nullptr ? lv_obj_get_style_text_font(t, LV_PART_MAIN) : nullptr;
    const char* texte = t != nullptr ? lv_label_get_text(t) : nullptr;
    if (police == nullptr || texte == nullptr) return kTitreX;
    lv_point_t taille;
    lv_text_get_size(&taille, texte, police, lv_obj_get_style_text_letter_space(t, LV_PART_MAIN), 0, LV_COORD_MAX,
                     LV_TEXT_FLAG_NONE);
    return kTitreX + taille.x;
}

// Les noms des pages : « 1–5 », « 6–10 »… (les lignes de chaque colonne qu'elles montrent),
// aucun sous deux pages, celui de la page affichée en accent (choix_peindre).
void peindre_onglets(bool eteindre) {
    const MaisonUI& u = g_maison_ui;
    const int n = s_pages >= 2 ? s_pages : 0;
    const int32_t fin = eteindre ? kEteindreX - 16 : kOngletsFin;
    const int32_t debut = fin_du_titre() + 24;
    const int32_t w = n > 0 ? std::max<int32_t>(60, std::min<int32_t>(kOngletL, (fin - debut - (n - 1) * kOngletEcart) / n)) : 0;
    for (int i = 0; i < kPagesMax; i++) {
        lv_obj_t* const b = u.onglet[i];
        ui_hidden(b, i >= n);
        if (i >= n || b == nullptr) continue;
        if (lv_obj_get_style_width(b, LV_PART_MAIN) != w) lv_obj_set_width(b, w);
        ui_x(b, fin - (n - i) * w - (n - 1 - i) * kOngletEcart);
        char nom[16];
        snprintf(nom, sizeof(nom), "%d\xE2\x80\x93%d", i * kLignesPage + 1, (i + 1) * kLignesPage);
        ui_text(lv_obj_get_child(b, 0), nom);
    }
    if (n > 0) choix_peindre(u.onglet, n, s_page);
}

// Colonnes, en-têtes et lignes de la page `s_page` d'après les définitions (pages
// recomptées, la page ramenée dans leur nombre), puis tout le dessin.
// Une ligne possible d'une colonne : l'élément T et ses gestes, lus une fois (tuile_gestes).
struct LigneLue {
    int8_t t;
    bool agit;
    bool appui_long;
};

void disposer() {
    MaisonUI& u = g_maison_ui;
    char titres[kPieces][48];
    bool occupee[kPieces];
    LigneLue lues[kPieces][kTuilesMax];
    int nb[kPieces] = {};
    int n = 0, lignes_max = 0;
    for (int r = 0; r < kPieces; r++) {
        occupee[r] = tuiles_piece_titre(r, titres[r], sizeof(titres[r]));
        if (!occupee[r]) continue;
        n++;
        for (int t = 0; t < kTuilesMax; t++) {
            LigneLue& l = lues[r][nb[r]];
            l.t = static_cast<int8_t>(t);
            if (tuile_gestes(r, t, l.agit, l.appui_long)) nb[r]++;
        }
        lignes_max = std::max(lignes_max, nb[r]);
    }
    s_pages = std::max(1, (lignes_max + kLignesPage - 1) / kLignesPage);
    s_page = std::min(std::max(s_page, 0), s_pages - 1);
    s_colonne = n > 0 ? (kCarteL - 2 * kMarge - (n - 1) * kEcart) / n : 0;
    ui_hidden(u.vide, n > 0);
    bool lumieres = false;
    int c = 0;
    for (int r = 0; r < kPieces; r++) {
        const int32_t x = kMarge + c * (s_colonne + kEcart);
        lv_obj_t* entete = u.entete[r];
        ui_hidden(entete, !occupee[r]);
        if (occupee[r] && entete != nullptr) {
            const lv_font_t* f = lv_obj_get_style_text_font(entete, LV_PART_MAIN);
            const int32_t h = f != nullptr ? lv_font_get_line_height(f) : 0;
            ui_x(entete, x + kEnteteX);
            ui_y(entete, kCorpsY + (kEnteteH - h) / 2);
            texte_ha_coupe(entete, titres[r], s_colonne - 2 * kEnteteX);
            lumieres = lumieres || tuiles_piece_a_lumieres(r);
        }
        // Les lignes de la page : rangs s_page × 5 à s_page × 5 + 4 parmi les tuiles non
        // vides de la pièce (les tuiles vides ne laissent pas de trou).
        int k = 0;
        for (int i = s_page * kLignesPage; occupee[r] && i < nb[r] && k < kLignesPage; i++, k++) {
            const LigneLue& l = lues[r][i];
            lv_obj_t* ligne = u.ligne[r][k];
            s_elements[r][k] = l.t;
            ui_hidden(ligne, false);
            if (ligne == nullptr) continue;
            ui_x(ligne, x);
            ui_y(ligne, kLignesY + k * (kLigneH + kLigneEcart));
            largeur(ligne, s_colonne);
            hauteur(ligne, kLigneH);
            const int t = l.t;
            const bool agit = l.agit, appui_long = l.appui_long;
            dessiner_ligne(ligne, r, t, agit, appui_long);
        }
        for (; k < kLignesPage; k++) {
            s_elements[r][k] = -1;
            ui_hidden(u.ligne[r][k], true);
        }
        if (occupee[r]) c++;
    }
    ui_hidden(u.eteindre, !lumieres);
    peindre_onglets(lumieres);
}

// Brique des popups à pages (tab5_pages.cpp) : le glissement de page en page.
int nombre_pages() { return s_pages; }
int page_courante() { return affiche() ? s_page : -1; }
void afficher_page(int page) {
    if (!affiche() || page < 0 || page >= s_pages || page == s_page) return;
    s_page = page;
    disposer();
}
PagesPopup s_pages_maison{nullptr, nombre_pages, page_courante, afficher_page};

}  // namespace

void maison_ouvrir() {
    MaisonUI& u = g_maison_ui;
    if (u.popup == nullptr) return;
    s_pages_maison.popup = u.popup;
    pages_brancher(&s_pages_maison);  // une fois (rappelé : sans effet)
    s_page = 0;
    disposer();
    animate_popup_open(u.popup);
    ui_mark_activity();
}

bool maison_titre_appui_valide() {
    if (!g_central_ctx.ha_mode) return false;
    // Un tap au bout d'un glissement (swipe des pièces) : rien, comme l'appui long qui
    // ouvre les alertes (alertes_ouvrir).
    return !ui_appui_glisse();
}

void maison_ligne_appui(int r, int k, bool long_appui) {
    if (r < 0 || r >= kPieces || k < 0 || k >= kLignesPage || s_elements[r][k] < 0) return;
    lv_obj_t* ligne = g_maison_ui.ligne[r][k];
    tuile_appui_maison(r, s_elements[r][k], long_appui, ligne != nullptr ? enfant(ligne, 0) : nullptr);
}

void maison_page(int page) { afficher_page(page); }

void maison_eteindre_lumieres() {
    for (int r = 0; r < kPieces; r++)
        if (tuiles_piece_a_lumieres(r)) tuiles_piece_eteindre(r);
}

void maison_tuile_changee(int r, int t) {
    if (!affiche() || r < 0 || r >= kPieces || t < 0 || t >= kTuilesMax) return;
    peindre_ligne(r, t);
}

void maison_definitions_changees() {
    if (affiche()) disposer();
}
