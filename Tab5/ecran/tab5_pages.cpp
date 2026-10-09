/**
 * [AI-CONTEXT]
 * @file tab5_pages.cpp
 * @role Popup à pages (09/10/2026) : ce que les popups « Réglages » (tab5_reglages.cpp) et
 *       « Réveil » (alarm_render.cpp) partagent pour changer de page. Les noms des pages
 *       sont en haut, à côté du titre (reglages_onglet.yaml), celle affichée en couleur
 *       d'accent ; un geste gauche / droite dans le popup montre la page suivante /
 *       précédente, en boucle (reglages_page_voisine, tab5_core.cpp). Sorti de
 *       tab5_reglages.cpp sans rien changer à son comportement, pour que le réveil prenne
 *       le même mécanisme au lieu d'une copie (règle 5).
 * @architecture_constraint Changement de page INSTANTANÉ (préférence d'Axel, AGENTS.md) :
 *       les conteneurs des pages sont masqués / montrés, sans glissement ni fondu. Rien
 *       gardé ici : chaque popup garde son PopupPages (page affichée comprise).
 * @ai_warning [AI-WARNING] Le geste s'arrête au popup : pages_brancher() retire
 *       LV_OBJ_FLAG_GESTURE_BUBBLE du popup. Sans ça, LVGL le remonte jusqu'à page_main,
 *       dont le on_gesture change les prévisions ou la pièce derrière le popup
 *       (handle_swipe_gesture, tab5_central.cpp). Un geste parti d'un curseur ou d'un
 *       rouleau n'est pas un changement de page : LVGL émet aussi LV_EVENT_GESTURE pendant
 *       qu'on les glisse (de biais).
 */
#include "tab5_internal.h"

namespace {

// LV_EVENT_GESTURE du popup (pages_brancher) : gauche = page suivante, droite = page
// précédente, en boucle. LVGL l'émet pendant l'appui, dès que le doigt a parcouru
// gesture_min_distance (lv_indev.c, indev_gesture), comme pour les prévisions.
// lv_indev_wait_release() : le lever du doigt qui suit ne déclenche rien (ni le bouton où
// le geste est parti, ni un appui long), c'est le « tap qui suit un glissement est
// ignoré » des autres popups, pour tout le popup.
void geste_rappel(lv_event_t* e) {
    PopupPages* const p = static_cast<PopupPages*>(lv_event_get_user_data(e));
    lv_indev_t* const indev = lv_indev_active();
    if (p == nullptr || indev == nullptr) return;
    const lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) return;
    // Un curseur (luminosité, volume) ou un rouleau (heures du réveil) glissé de côté :
    // c'est son réglage, pas une page.
    for (lv_obj_t* o = lv_indev_get_active_obj(); o != nullptr && o != p->popup; o = lv_obj_get_parent(o)) {
        if (lv_obj_check_type(o, &lv_slider_class)) return;
#if LV_USE_ROLLER
        if (lv_obj_check_type(o, &lv_roller_class)) return;
#endif
    }
    lv_indev_wait_release(indev);
    const int page = reglages_page_voisine(p->courante, p->n, dir == LV_DIR_LEFT);
    if (p->afficher != nullptr) {
        p->afficher(page);
    } else {
        pages_montrer(*p, page);
    }
}

}  // namespace

void choix_bouton(lv_obj_t* b, bool on) {
    if (b == nullptr) return;
    if (on) {
        highlight_button_border(b, true, UIColor.ACCENT, 3);
    } else {
        for (lv_style_prop_t prop : {LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_OPA, LV_STYLE_BORDER_WIDTH})
            lv_obj_remove_local_style_prop(b, prop, LV_PART_MAIN);
    }
    ui_text_color(lv_obj_get_child(b, 0), on ? UIColor.ACCENT : UIColor.TEXT_SOFT);
}

void choix_peindre(lv_obj_t* const* boutons, int n, int actif) {
    if (boutons == nullptr) return;
    for (int i = 0; i < n; i++) choix_bouton(boutons[i], i == actif);
}

void pages_brancher(PopupPages& p) {
    if (p.popup == nullptr) return;
    lv_obj_remove_flag(p.popup, LV_OBJ_FLAG_GESTURE_BUBBLE);
    lv_obj_add_event_cb(p.popup, geste_rappel, LV_EVENT_GESTURE, &p);
}

void pages_montrer(PopupPages& p, int page) {
    if (page < 0 || page >= p.n) page = 0;
    p.courante = page;
    if (p.page != nullptr) {
        for (int i = 0; i < p.n; i++) ui_hidden(p.page[i], i != page);
    }
    choix_peindre(p.onglet, p.n, page);
}

bool pages_visible(const PopupPages& p, int page) {
    return p.popup != nullptr && p.courante == page && !lv_obj_has_flag(p.popup, LV_OBJ_FLAG_HIDDEN);
}
