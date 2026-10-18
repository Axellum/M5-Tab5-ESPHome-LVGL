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
 *       l'écran). Parmi eux, HORS : ceux qui dépassent l'écran lui-même. BARRES (18/10/2026) :
 *       les barres de défilement que LVGL dessine (lv_obj_get_scrollbar_area : défilable, mode
 *       pas OFF, contenu plus grand que la boîte), dans la zone visible ; chacune est écrite
 *       au journal (« Barre de défilement », tag tab5.rendu) avec la boîte de son objet, pour
 *       la retrouver dans la capture.
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

#include <algorithm>

struct RenduCoupes {
    int coupes = 0;  // objets qui dessinent et ne tiennent pas dans leur zone visible
    int hors = 0;    // dont ceux qui dépassent l'écran
    int barres = 0;  // barres de défilement dessinées dans la zone visible
};

// Un objet qui dessine : sans enfant (texte, icône, trait…), ou avec un fond ou une bordure.
// Un conteneur transparent qui déborde ne se voit pas : ses enfants comptent pour lui.
inline bool rendu_dessine(lv_obj_t* o) {
    if (lv_obj_get_child_count(o) == 0) return true;
    return lv_obj_get_style_bg_opa(o, LV_PART_MAIN) > LV_OPA_TRANSP ||
           lv_obj_get_style_border_width(o, LV_PART_MAIN) > 0;
}

// Calcul des zones écrit ici : lv_area_is_in() et lv_area_intersect() sont privés en
// LVGL 9.5 (lv_area_private.h), et la bibliothèque du rendu hors tablette ne les déclare
// pas sous ce nom (CI du 12/10/2026). Zones inclusives (x2, y2 compris), comme LVGL.
inline bool rendu_dedans(const lv_area_t& a, const lv_area_t& z) {
    return a.x1 >= z.x1 && a.y1 >= z.y1 && a.x2 <= z.x2 && a.y2 <= z.y2;
}

// Une zone non vide qui touche `z`.
inline bool rendu_touche(const lv_area_t& a, const lv_area_t& z) {
    return a.x2 >= a.x1 && a.y2 >= a.y1 && a.x1 <= z.x2 && a.x2 >= z.x1 && a.y1 <= z.y2 && a.y2 >= z.y1;
}

// Une barre de défilement de `o` (verticale ou horizontale) qui paraît dans `zone` : comptée
// et écrite au journal avec la boîte de l'objet et ce qui dépasse de chaque côté.
inline void rendu_barres(lv_obj_t* o, const lv_area_t& a, const lv_area_t& zone, const char* nom, RenduCoupes& r) {
    lv_area_t hor, ver;
    lv_obj_get_scrollbar_area(o, &hor, &ver);
    const bool h = rendu_touche(hor, zone), v = rendu_touche(ver, zone);
    if (!h && !v) return;
    r.barres++;
    ESP_LOGI("tab5.rendu", "Barre de défilement %s (%s) : objet %d,%d %dx%d, dépasse g %d d %d h %d b %d",
             h && v ? "verticale et horizontale" : (v ? "verticale" : "horizontale"), nom != nullptr ? nom : "",
             static_cast<int>(a.x1), static_cast<int>(a.y1), static_cast<int>(a.x2 - a.x1 + 1),
             static_cast<int>(a.y2 - a.y1 + 1), static_cast<int>(lv_obj_get_scroll_left(o)),
             static_cast<int>(lv_obj_get_scroll_right(o)), static_cast<int>(lv_obj_get_scroll_top(o)),
             static_cast<int>(lv_obj_get_scroll_bottom(o)));
}

inline void rendu_parcourir(lv_obj_t* o, const lv_area_t& zone, const lv_area_t& ecran, RenduCoupes& r,
                            const char* nom = nullptr) {
    if (o == nullptr || lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    lv_area_t a;
    lv_obj_get_coords(o, &a);
    const bool plein = a.x2 >= a.x1 && a.y2 >= a.y1;
    if (plein && rendu_dessine(o)) {
        if (!rendu_dedans(a, zone)) r.coupes++;
        if (!rendu_dedans(a, ecran)) r.hors++;
    }
    if (plein) rendu_barres(o, a, zone, nom, r);
    // Zone des enfants : celle-ci, réduite à l'objet s'il les coupe (intersection ; vide,
    // x1 > x2 ou y1 > y2, quand ils ne se touchent pas : rien n'y tient).
    lv_area_t enfants = zone;
    if (!lv_obj_has_flag(o, LV_OBJ_FLAG_OVERFLOW_VISIBLE)) {
        enfants.x1 = std::max(zone.x1, a.x1);
        enfants.y1 = std::max(zone.y1, a.y1);
        enfants.x2 = std::min(zone.x2, a.x2);
        enfants.y2 = std::min(zone.y2, a.y2);
    }
    const uint32_t n = lv_obj_get_child_count(o);
    for (uint32_t i = 0; i < n; i++)
        rendu_parcourir(lv_obj_get_child(o, static_cast<int32_t>(i)), enfants, ecran, r, nom);
}

// `racine` : le popup ouvert, ou nullptr pour l'écran actif et le calque du dessus. `nom` :
// la capture, citée au journal à chaque barre de défilement.
inline RenduCoupes rendu_mesurer(lv_obj_t* racine, const char* nom = nullptr) {
    lv_obj_update_layout(lv_screen_active());
    // L'écran logique (paysage) de la carte : celui des coordonnées des objets.
    const lv_area_t ecran = {0, 0, kEcranL - 1, kEcranH - 1};
    RenduCoupes r;
    if (racine != nullptr) {
        rendu_parcourir(racine, ecran, ecran, r, nom);
    } else {
        rendu_parcourir(lv_screen_active(), ecran, ecran, r, nom);
        rendu_parcourir(lv_layer_top(), ecran, ecran, r, nom);
    }
    return r;
}
