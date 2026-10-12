/**
 * [AI-CONTEXT]
 * @file Tab5/rendu/rendu_mesure.h
 * @role Rendu hors tablette SEULEMENT (tab5-rendu-host.yaml) : ce que montre une capture,
 *       dit par LVGL lui-même (lot 2 du portage 1024×600, ADR-0061). À chaque capture,
 *       l'action rendu_capture (Tab5/rendu/bouchons.yaml) publie l'écran affiché (le nom
 *       que donne « Écran courant » : Accueil, un popup, Arcade, « Jeu · … ») et le nombre
 *       d'objets COUPÉS : ceux qui dessinent quelque chose (un objet sans enfant, ou un
 *       fond ou une bordure) et ne tiennent pas entièrement dans la zone où LVGL les
 *       laisse paraître (leurs parents, sauf ceux en LV_OBJ_FLAG_OVERFLOW_VISIBLE, et
 *       l'écran). Parmi eux, HORS : ceux qui dépassent l'écran lui-même.
 *       tools/rendu/capturer.py les écrit par capture (ecrans-<carte>.json) ;
 *       tools/rendu/bilan_carte.py compare la Waveshare 7B au Tab5.
 * @architecture_constraint Un popup ouvert est mesuré seul (son sous-arbre) : le tableau
 *       de bord derrière son voile opaque n'est pas visible, ses débordements ne comptent
 *       pas. Sinon : l'écran actif et le calque du dessus. Un contenu qui défile (liste
 *       plus haute que sa boîte) compte aussi : la mesure se lit par comparaison (Tab5
 *       contre 7B, avant contre après), pas dans l'absolu. Jamais dans le firmware de la
 *       tablette (tests/test_rendu_host.py).
 */
#pragma once
#include "esphome.h"
#include "tab5_geometrie.h"

struct RenduCoupes {
    int coupes = 0;  // objets qui dessinent et ne tiennent pas dans leur zone visible
    int hors = 0;    // dont ceux qui dépassent l'écran
};

// Un objet qui dessine : sans enfant (texte, icône, trait…), ou avec un fond ou une bordure.
// Un conteneur transparent qui déborde ne se voit pas : ses enfants comptent pour lui.
inline bool rendu_dessine(lv_obj_t* o) {
    if (lv_obj_get_child_count(o) == 0) return true;
    return lv_obj_get_style_bg_opa(o, LV_PART_MAIN) > LV_OPA_TRANSP ||
           lv_obj_get_style_border_width(o, LV_PART_MAIN) > 0;
}

inline void rendu_parcourir(lv_obj_t* o, const lv_area_t& zone, const lv_area_t& ecran, RenduCoupes& r) {
    if (o == nullptr || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    const bool plein = lv_area_get_width(&a) > 0 && lv_area_get_height(&a) > 0;
    if (plein && rendu_dessine(o)) {
        if (!lv_area_is_in(&a, &zone, 0)) r.coupes++;
        if (!lv_area_is_in(&a, &ecran, 0)) r.hors++;
    }
    // Zone des enfants : celle-ci, réduite à l'objet s'il les coupe. Aucune intersection :
    // une zone vide (x1 > x2), où rien ne tient.
    lv_area_t enfants = zone;
    if (!lv_obj_has_flag(o, LV_OBJ_FLAG_OVERFLOW_VISIBLE) && !lv_area_intersect(&enfants, &zone, &a)) {
        enfants.x1 = 1;
        enfants.x2 = 0;
    }
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; i++) rendu_parcourir(lv_obj_get_child(o, static_cast<int32_t>(i)), enfants, ecran, r);
}

// `racine` : le popup ouvert, ou nullptr pour l'écran actif et le calque du dessus.
inline RenduCoupes rendu_mesurer(lv_obj_t* racine) {
    lv_obj_update_layout(lv_screen_active());
    // L'écran logique (paysage) de la carte : celui des coordonnées des objets.
    const lv_area_t ecran = {0, 0, kEcranL - 1, kEcranH - 1};
    RenduCoupes r;
    if (racine != nullptr) {
        rendu_parcourir(racine, ecran, ecran, r);
    } else {
        rendu_parcourir(lv_screen_active(), ecran, ecran, r);
        rendu_parcourir(lv_layer_top(), ecran, ecran, r);
    }
    return r;
}
