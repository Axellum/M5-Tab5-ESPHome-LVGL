/**
 * [AI-CONTEXT]
 * @file tab5_pages.cpp
 * @role Popup à pages (ADR-0046, 09/10/2026, demande d'Axel) : LA brique commune des popups
 *       qui ont plusieurs pages — Réglages (quatre pages), Lumières et Volets (une page par
 *       pièce, tab5_tuiles_popups.cpp), Réveil (cinq pages, alarm_render.cpp :
 *       pages_brancher et choix_peindre seulement, ses noms gardent leur place du YAML)
 *       — et, plus tard, le carrousel des clims :
 *         - le glissement gauche / droite qui change de page (gauche = la suivante, droite
 *           = la précédente, en boucle : reglages_page_voisine, tab5_core.cpp) ;
 *         - les noms des pages en haut, sur la ligne du titre (onglets d'en-tête,
 *           pages_onglet.yaml) : posés de droite à gauche jusqu'à la croix, 200 px chacun
 *           au plus, celui de la page affichée en couleur d'accent (choix_peindre, la
 *           même recette que l'option active d'une rangée des Réglages). Moins de deux
 *           pages : aucun onglet, aucun glissement.
 *       Le popup garde sa page et dit comment la compter, la lire et l'afficher
 *       (PagesPopup, tab5_internal.h) : rien n'est gardé ici.
 * @architecture_constraint Changement de page INSTANTANÉ (préférence d'Axel, AGENTS.md) :
 *       rien ne glisse ni ne fond, la page est repeinte. Couleurs par la palette active
 *       (UIColor.X) et les styles de rôle du YAML, jamais une couleur écrite ici.
 *       Géométrie des onglets = celle des quatre noms des Réglages (x 318, 528, 738, 948
 *       pour quatre pages : reglages_popup.yaml, tests/test_pages_popup.py compare).
 * @ai_warning [AI-WARNING] Le geste s'arrête au popup : LV_OBJ_FLAG_GESTURE_BUBBLE retiré
 *       (pages_brancher). Sans ça, LVGL le remonte jusqu'à page_main, dont le on_gesture
 *       change les prévisions ou la pièce derrière le popup (handle_swipe_gesture,
 *       tab5_central.cpp). Un geste parti d'un curseur, d'un arc ou d'un rouleau
 *       (luminosité, volume, arc d'une lampe, rouleaux du réveil) est son réglage, pas une page : LVGL émet aussi LV_EVENT_GESTURE
 *       pendant qu'on le glisse. pages_brancher peut être rappelé (script de pose relancé) :
 *       son rappel est retiré avant d'être ajouté, sinon un glissement sautait autant de
 *       pages (vu sur le carrousel des clims dans le rendu du 09/10/2026).
 */
#include "tab5_internal.h"
#include "lvgl.h"

#include <algorithm>

namespace {

// Onglets d'en-tête (pages_onglet.yaml, reglages_onglet.yaml) : sur la ligne du titre de
// la carte modale, de x = 318 (après l'icône et le titre) à 1148 (8 px avant la croix,
// modal_header.yaml), 10 px entre deux, 200 px au plus ; le nom à 8 px des bords.
constexpr int32_t kOngletsDebut = 318;
constexpr int32_t kOngletsFin = 1148;
constexpr int32_t kOngletEcart = 10;
constexpr int32_t kOngletL = 200;
constexpr int32_t kOngletMarge = 8;

// LV_EVENT_GESTURE du popup (pages_brancher). LVGL l'émet pendant l'appui, dès que le
// doigt a parcouru gesture_min_distance (lv_indev.c, indev_gesture), comme pour les
// prévisions. lv_indev_wait_release() : le lever du doigt qui suit ne déclenche rien (ni
// le bouton où le geste est parti, ni un appui long) — le « tap au bout d'un glissement
// ignoré » des autres popups, pour tout le popup.
void geste_rappel(lv_event_t* e) {
    const PagesPopup* p = static_cast<const PagesPopup*>(lv_event_get_user_data(e));
    lv_indev_t* const indev = lv_indev_active();
    if (p == nullptr || indev == nullptr || p->nombre == nullptr || p->courante == nullptr || p->afficher == nullptr)
        return;
    const lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) return;
    // Un curseur, un arc ou un rouleau glissé de côté : c'est son réglage, pas une page. Un
    // rouleau (lv_roller, réveil, 09/10/2026) se glisse de haut en bas, mais un doigt en
    // biais y donne aussi un geste gauche / droite.
    for (lv_obj_t* o = lv_indev_get_active_obj(); o != nullptr && o != p->popup; o = lv_obj_get_parent(o)) {
        if (lv_obj_check_type(o, &lv_slider_class) || lv_obj_check_type(o, &lv_arc_class)) return;
#if LV_USE_ROLLER
        if (lv_obj_check_type(o, &lv_roller_class)) return;
#endif
    }
    const int n = p->nombre();
    if (n < 2) return;
    lv_indev_wait_release(indev);
    p->afficher(reglages_page_voisine(p->courante(), n, dir == LV_DIR_LEFT));
}

}  // namespace

void pages_brancher(PagesPopup* p) {
    if (p == nullptr || p->popup == nullptr) return;
    // Geste gauche / droite : arrêté au popup ([AI-WARNING] de l'en-tête), traité ici.
    lv_obj_remove_flag(p->popup, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_remove_event_cb_with_user_data(p->popup, geste_rappel, p);
    lv_obj_add_event_cb(p->popup, geste_rappel, LV_EVENT_GESTURE, p);
}

// L'option active en couleur d'accent (bordure et texte), les autres exactement au style
// du bouton (style_clim_btn, bordure et formes du thème) : la bordure locale est retirée au
// lieu du gris « inactif » de highlight_button_border, comme le bouton « HA »
// (tab5_tuiles.cpp). Le texte est le premier enfant du bouton.
void choix_peindre(lv_obj_t* const* boutons, int n, int actif) {
    for (int i = 0; i < n; i++) {
        lv_obj_t* const b = boutons[i];
        if (b == nullptr) continue;
        const bool on = (i == actif);
        if (on) {
            highlight_button_border(b, true, UIColor.ACCENT, 3);
        } else {
            for (lv_style_prop_t p : {LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_OPA, LV_STYLE_BORDER_WIDTH})
                lv_obj_remove_local_style_prop(b, p, LV_PART_MAIN);
        }
        ui_text_color(lv_obj_get_child(b, 0), on ? UIColor.ACCENT : UIColor.TEXT_SOFT);
    }
}

int32_t pages_onglet_largeur(int n) {
    if (n < 1) return kOngletL;
    return std::min(kOngletL, (kOngletsFin - kOngletsDebut - (n - 1) * kOngletEcart) / n);
}

void pages_onglets(lv_obj_t* const* onglets, int max, const char* const* noms, int n, int courante) {
    const bool montres = n >= 2;
    const int32_t w = pages_onglet_largeur(n);
    for (int i = 0; i < max; i++) {
        lv_obj_t* const b = onglets[i];
        const bool visible = montres && i < n;
        ui_hidden(b, !visible);
        if (!visible || b == nullptr) continue;
        // Collés à la croix, de droite à gauche : quatre pages tombent sur les noms des
        // Réglages ; cinq se partagent la place (158 px).
        ui_x(b, kOngletsFin - (n - i) * w - (n - 1 - i) * kOngletEcart);
        lv_obj_set_width(b, w);  // LVGL 9.5.0 compare lui-même (lv_obj_pos.c)
        if (noms != nullptr && noms[i] != nullptr) texte_ha_coupe(lv_obj_get_child(b, 0), noms[i], w - 2 * kOngletMarge);
    }
    if (montres) choix_peindre(onglets, std::min(n, max), courante);
}
