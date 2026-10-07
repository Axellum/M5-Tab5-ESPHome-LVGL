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
 *       (tuile_appui_maison). La roue passe devant ce popup (roue_ouvrir la met au
 *       premier plan) ; un toucher hors d'elle ne ferme qu'elle ; son « ⋯ » ouvre le
 *       popup de la tuile devant Maison, qui reste derrière.
 */
#include "tab5_internal.h"
#include "lvgl.h"
#include <cstdio>

MaisonUI g_maison_ui;

namespace {

constexpr int kPieces = 5;
constexpr int kTuiles = 5;

// Géométrie (maison_popup.yaml, tab5-ui-tokens.yaml) : carte modale de 1250 px, corps à
// y = 72 sous la barre de titre (ADR-0009), marges et écarts de 12 px entre colonnes.
constexpr int32_t kCarteL = 1250;
constexpr int32_t kMarge = 12;
constexpr int32_t kEcart = 12;
constexpr int32_t kCorpsY = 72;
// En-tête de colonne (40 px), puis les lignes (104 px, 8 px d'écart).
constexpr int32_t kEnteteH = 40;
constexpr int32_t kEnteteX = 6;       // le nom, un peu en retrait du bord de la colonne
constexpr int32_t kLigneH = 104;
constexpr int32_t kLigneEcart = 8;
constexpr int32_t kLignesY = kCorpsY + kEnteteH + kLigneEcart;
// Dans une ligne (maison_ligne.yaml) : pastille de 56 px à 12 px du bord, textes à 80 px ;
// « ⋯ » de 36 px à 12 px du bord droit, 8 px d'air avant lui ; sans lui, 12 px de marge.
constexpr int32_t kTexteX = 80;
constexpr int32_t kTexteFin = 12;
constexpr int32_t kPlusPlace = 36 + 12 + 8;
// Hauteur de la carte (tab5-ui-tokens.yaml, modal_card_h) : la dernière ligne y tient.
constexpr int32_t kCarteH = 690;
static_assert(kLignesY + kTuiles * kLigneH + (kTuiles - 1) * kLigneEcart <= kCarteH - kMarge,
              "les cinq lignes d'une pièce tiennent dans la carte");

// Largeur des colonnes de la dernière disposition (les textes d'une ligne en dépendent).
int32_t s_colonne = 0;

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

// Un état arrivé : relit les gestes de la tuile (une seule fois) puis la redessine.
void peindre_ligne(int r, int t) {
    lv_obj_t* ligne = g_maison_ui.ligne[r][t];
    bool agit = false, appui_long = false;
    if (ligne != nullptr && tuile_gestes(r, t, agit, appui_long)) dessiner_ligne(ligne, r, t, agit, appui_long);
}

// Colonnes, en-têtes et lignes d'après les définitions, puis tout le dessin.
void disposer() {
    MaisonUI& u = g_maison_ui;
    char titres[kPieces][48];
    bool occupee[kPieces];
    int n = 0;
    for (int r = 0; r < kPieces; r++) {
        occupee[r] = tuiles_piece_titre(r, titres[r], sizeof(titres[r]));
        if (occupee[r]) n++;
    }
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
        int k = 0;  // rang de la ligne : les tuiles vides ne laissent pas de trou
        for (int t = 0; t < kTuiles; t++) {
            lv_obj_t* ligne = u.ligne[r][t];
            bool agit = false, appui_long = false;
            const bool presente = occupee[r] && tuile_gestes(r, t, agit, appui_long);
            ui_hidden(ligne, !presente);
            if (!presente || ligne == nullptr) continue;
            ui_x(ligne, x);
            ui_y(ligne, kLignesY + k * (kLigneH + kLigneEcart));
            largeur(ligne, s_colonne);
            dessiner_ligne(ligne, r, t, agit, appui_long);
            k++;
        }
        if (occupee[r]) c++;
    }
    ui_hidden(u.eteindre, !lumieres);
}

}  // namespace

void maison_ouvrir() {
    MaisonUI& u = g_maison_ui;
    if (u.popup == nullptr) return;
    disposer();
    animate_popup_open(u.popup);
    ui_mark_activity();
}

bool maison_titre_appui_valide() {
    if (!g_central_ctx.ha_mode) return false;
    // Un tap au bout d'un glissement (swipe des pièces) : rien, comme l'appui long qui
    // ouvre les alertes (alertes_ouvrir). LVGL remet ces marques à zéro à chaque appui.
    lv_indev_t* indev = lv_indev_active();
    return indev == nullptr || (!lv_indev_get_press_moved(indev) && lv_indev_get_gesture_dir(indev) == LV_DIR_NONE);
}

void maison_ligne_appui(int r, int t, bool long_appui) {
    if (r < 0 || r >= kPieces || t < 0 || t >= kTuiles) return;
    lv_obj_t* ligne = g_maison_ui.ligne[r][t];
    tuile_appui_maison(r, t, long_appui, ligne != nullptr ? enfant(ligne, 0) : nullptr);
}

void maison_eteindre_lumieres() {
    for (int r = 0; r < kPieces; r++)
        if (tuiles_piece_a_lumieres(r)) tuiles_piece_eteindre(r);
}

void maison_tuile_changee(int r, int t) {
    if (!affiche() || r < 0 || r >= kPieces || t < 0 || t >= kTuiles) return;
    peindre_ligne(r, t);
}

void maison_definitions_changees() {
    if (affiche()) disposer();
}
