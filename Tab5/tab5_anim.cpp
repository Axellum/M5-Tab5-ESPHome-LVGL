/**
 * [AI-CONTEXT]
 * @file tab5_anim.cpp
 * @role Animations LVGL : transitions de widgets, ouverture/fermeture des popups, swipe
 *       horizontal, entrée des alertes, fondu de calques ; retour automatique à l'écran
 *       principal (inactivité, UIIdle) ; rouleau d'icône météo ; horloge à rouleau
 *       (layout, chiffres) et date ; styles pressed des boutons verre.
 *       Unité de compilation issue de la scission de tab5_custom.cpp (lot (e) de
 *       l'audit du 06/09/2026, faite le 08/09/2026) : mêmes fonctions, même ordre,
 *       aucune logique modifiée.
 * @regle_absolue Seul point de contact avec l'API LVGL, comme avant : les YAML
 *                n'appellent que des helpers déclarés dans tab5_custom.h. Les
 *                helpers partagés entre unités sont déclarés dans tab5_internal.h.
 * @memory_constraint Éviter std::string dans les boucles de parsing ; char* + strtok_r.
 */
#include "tab5_custom.h"
#include "tab5_internal.h"
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <ctime>
#include <cstring>
#include <vector>
#include <map>

// Callback d'animation de position Y (#T225 : evite cast ABI lv_obj_set_y).
static void anim_y_cb(void* obj, int32_t v) {
    lv_obj_set_y((lv_obj_t*)obj, (lv_coord_t)v);
}

static void anim_out_y_ready_cb(lv_anim_t* a) {
    lv_obj_t* o = (lv_obj_t*)a->var;
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_y(o, 0);
    lv_obj_set_style_opa(o, LV_OPA_COVER, LV_PART_MAIN);
}

// Callback d'animation d'opacite : signature compatible lv_anim_exec_xcb_t (void*, int32_t).
// lv_obj_set_style_opa() prend 3 arguments et ne peut donc pas etre castee directement.
static void anim_opa_cb(void* obj, int32_t v) {
    lv_obj_set_style_opa((lv_obj_t*)obj, (lv_opa_t)v, LV_PART_MAIN);
}

// Callback d'animation de position X (glissement horizontal, swipe previsions).
static void anim_x_cb(void* obj, int32_t v) {
    lv_obj_set_x((lv_obj_t*)obj, (lv_coord_t)v);
}

// Callback d'animation de translate_y (rouleaux : horloge, icones meteo).
// On anime translate_y et non y : les icones meteo posent deja un offset de
// base via lv_obj_set_style_translate_y() dans update_meteo_icon(), et les
// labels de l'horloge sont alignes (align + y). L'offset de base est integre
// aux bornes de l'animation par l'appelant, ce callback reste donc trivial.
static void anim_ty_cb(void* obj, int32_t v) {
    lv_obj_set_style_translate_y((lv_obj_t*)obj, (lv_coord_t)v, LV_PART_MAIN);
}

// Cache l'objet a la fin de l'animation (fermeture popup : card + scrim).
// Reinitialise aussi l'opacité et le scale pour un prochain open propre.
static void anim_hide_ready_cb(lv_anim_t* a) {
    lv_obj_t* o = (lv_obj_t*)a->var;
    if (!o) return;
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_x(o, 256, LV_PART_MAIN);
    lv_obj_set_style_transform_scale_y(o, 256, LV_PART_MAIN);
}

// Cache le layer sortant apres un swipe horizontal et reinitialise sa position X
// + opacité pour le prochain swipe (sinon il resterait invisible mais pas caché).
static void anim_swipe_out_ready_cb(lv_anim_t* a) {
    lv_obj_t* o = (lv_obj_t*)a->var;
    if (!o) return;
    lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(o, 0);
    lv_obj_set_style_opa(o, LV_OPA_COVER, LV_PART_MAIN);
}

// Transition "verre depoli" : glissement vertical + fondu croise.
//   - Sortie : descend en accelerant (ease_in) tout en s'effacant.
//   - Entree : arrive du haut en decelerant (ease_out) tout en apparaissant.
// Duree/amplitude : UIAnim::PANEL_* (190ms / 28px depuis le 28/07 — etait
// 450ms / 84px, trop lent pour un rotateur qui tourne toutes les 8s).
// Pas de transform_scale (trop couteux sur les grands objets).
void transition_widgets(lv_obj_t* out_obj, lv_obj_t* in_obj) {
    if (out_obj == in_obj) return;

    const uint32_t DUR    = UIAnim::PANEL_DUR;
    const int32_t  OFFSET = UIAnim::PANEL_OFFSET;

    if (out_obj) {
        lv_anim_t a_out_y;
        lv_anim_init(&a_out_y);
        lv_anim_set_var(&a_out_y, out_obj);
        lv_anim_set_values(&a_out_y, 0, OFFSET);
        lv_anim_set_time(&a_out_y, DUR);
        lv_anim_set_path_cb(&a_out_y, lv_anim_path_ease_in);
        lv_anim_set_exec_cb(&a_out_y, anim_y_cb);
        lv_anim_set_ready_cb(&a_out_y, anim_out_y_ready_cb);
        lv_anim_start(&a_out_y);

        lv_anim_t a_out_o;
        lv_anim_init(&a_out_o);
        lv_anim_set_var(&a_out_o, out_obj);
        lv_anim_set_values(&a_out_o, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_time(&a_out_o, DUR);
        lv_anim_set_path_cb(&a_out_o, lv_anim_path_ease_in);
        lv_anim_set_exec_cb(&a_out_o, anim_opa_cb);
        lv_anim_start(&a_out_o);
    }

    if (in_obj) {
        lv_obj_clear_flag(in_obj, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_y(in_obj, -OFFSET);
        lv_obj_set_style_opa(in_obj, LV_OPA_TRANSP, LV_PART_MAIN);

        lv_anim_t a_in_y;
        lv_anim_init(&a_in_y);
        lv_anim_set_var(&a_in_y, in_obj);
        lv_anim_set_values(&a_in_y, -OFFSET, 0);
        lv_anim_set_time(&a_in_y, DUR);
        lv_anim_set_path_cb(&a_in_y, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&a_in_y, anim_y_cb);
        lv_anim_start(&a_in_y);

        lv_anim_t a_in_o;
        lv_anim_init(&a_in_o);
        lv_anim_set_var(&a_in_o, in_obj);
        lv_anim_set_values(&a_in_o, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&a_in_o, DUR);
        lv_anim_set_path_cb(&a_in_o, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&a_in_o, anim_opa_cb);
        lv_anim_start(&a_in_o);
    }
}

// =============================================================================
// Helpers d'animation LVGL (popups, swipe, alertes) — 1A du plan.
// Reutilisent les callbacks ci-dessus (anim_y_cb/anim_opa_cb/anim_x_cb).
// =============================================================================

// Ouverture/fermeture d'un popup : affichage/masquage instantané.
// Affichage instantané : unhide + opa COVER directement (pas de fondu).
void animate_popup_open(lv_obj_t* card, lv_obj_t* scrim) {
    // Affichage instantané — pas de fondu (réactivité maximale).
    if (scrim) {
        lv_anim_delete(scrim, anim_opa_cb);
        lv_obj_clear_flag(scrim, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(scrim, LV_OPA_COVER, LV_PART_MAIN);
    }
    if (card) {
        lv_anim_delete(card, anim_opa_cb);
        lv_obj_clear_flag(card, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_opa(card, LV_OPA_COVER, LV_PART_MAIN);
    }
}

// Masquage instantané : cache card + scrim directement (LV_OBJ_FLAG_HIDDEN).
void animate_popup_close(lv_obj_t* card, lv_obj_t* scrim) {
    // Masquage instantané — pas de fondu (réactivité maximale).
    if (scrim) {
        lv_anim_delete(scrim, anim_opa_cb);
        lv_obj_add_flag(scrim, LV_OBJ_FLAG_HIDDEN);
    }
    if (card) {
        lv_anim_delete(card, anim_opa_cb);
        lv_obj_add_flag(card, LV_OBJ_FLAG_HIDDEN);
    }
}

// =============================================================================
// Retour automatique a l'ecran principal (inactivite tactile) — voir UIIdle
// dans tab5_custom.h pour les delais et le raisonnement.
// L'horloge d'inactivite est celle de LVGL : l'indev ecrit last_activity_time
// a chaque lecture "pressed", donc "inactif" veut bien dire "personne n'a
// touche la dalle" — inutile d'instrumenter les 200 boutons du HMI.
// =============================================================================

uint32_t ui_idle_ms() {
    return lv_display_get_inactive_time(NULL);  // NULL = display par defaut
}

void ui_mark_activity() {
    lv_display_trigger_activity(NULL);
}

bool close_popup_if_open(lv_obj_t* card) {
    if (card == nullptr) return false;
    if (lv_obj_has_flag(card, LV_OBJ_FLAG_HIDDEN)) return false;
    // Fondu deja en cours (ouverture ou fermeture) : ne pas le rejouer.
    // animate_popup_close() repart de LV_OPA_COVER, donc relancer sur un popup
    // a moitie efface le rallumerait d'un coup avant de le refaire disparaitre.
    if (lv_anim_get(card, anim_opa_cb) != nullptr) return false;
    animate_popup_close(card, nullptr);
    return true;
}

// Glissement horizontal + fondu croise entre deux layers (swipe previsions).
// dir = LV_DIR_LEFT (in arrive de la droite, out part a gauche) ou
//       LV_DIR_RIGHT (in arrive de la gauche, out part a droite).
// Duree/amplitude : UIAnim::SWIPE_* (200ms / 110px — etait 350ms / 200px).
// C'est une reponse directe a un geste : elle doit "coller" au doigt.
// Derivee de transition_widgets() mais en horizontal.
void animate_swipe_horizontal(lv_obj_t* out_layer, lv_obj_t* in_layer, lv_dir_t dir) {
    if (out_layer == in_layer) return;

    const uint32_t DUR    = UIAnim::SWIPE_DUR;
    const int32_t  OFFSET = UIAnim::SWIPE_OFFSET;
    const int32_t in_start_x  = (dir == LV_DIR_LEFT) ? OFFSET : -OFFSET;
    const int32_t out_end_x   = (dir == LV_DIR_LEFT) ? -OFFSET : OFFSET;

    if (out_layer) {
        // Cancel anims precedentes (swipe rapide repete)
        lv_anim_delete(out_layer, anim_x_cb);
        lv_anim_delete(out_layer, anim_opa_cb);
        lv_anim_t a_out_x;
        lv_anim_init(&a_out_x);
        lv_anim_set_var(&a_out_x, out_layer);
        lv_anim_set_values(&a_out_x, 0, out_end_x);
        lv_anim_set_time(&a_out_x, DUR);
        lv_anim_set_path_cb(&a_out_x, lv_anim_path_ease_in);
        lv_anim_set_exec_cb(&a_out_x, anim_x_cb);
        lv_anim_start(&a_out_x);

        lv_anim_t a_out_o;
        lv_anim_init(&a_out_o);
        lv_anim_set_var(&a_out_o, out_layer);
        lv_anim_set_values(&a_out_o, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_time(&a_out_o, DUR);
        lv_anim_set_path_cb(&a_out_o, lv_anim_path_ease_in);
        lv_anim_set_exec_cb(&a_out_o, anim_opa_cb);
        lv_anim_set_ready_cb(&a_out_o, anim_swipe_out_ready_cb);
        lv_anim_start(&a_out_o);
    }
    if (in_layer) {
        lv_anim_delete(in_layer, anim_x_cb);
        lv_anim_delete(in_layer, anim_opa_cb);
        lv_obj_clear_flag(in_layer, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(in_layer, in_start_x);
        lv_obj_set_style_opa(in_layer, LV_OPA_TRANSP, LV_PART_MAIN);

        lv_anim_t a_in_x;
        lv_anim_init(&a_in_x);
        lv_anim_set_var(&a_in_x, in_layer);
        lv_anim_set_values(&a_in_x, in_start_x, 0);
        lv_anim_set_time(&a_in_x, DUR);
        lv_anim_set_path_cb(&a_in_x, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&a_in_x, anim_x_cb);
        lv_anim_start(&a_in_x);

        lv_anim_t a_in_o;
        lv_anim_init(&a_in_o);
        lv_anim_set_var(&a_in_o, in_layer);
        lv_anim_set_values(&a_in_o, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&a_in_o, DUR);
        lv_anim_set_path_cb(&a_in_o, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&a_in_o, anim_opa_cb);
        lv_anim_start(&a_in_o);
    }
}

// Slide-in depuis la droite + fondu pour un bandeau d'alerte qui entre
// dans le rotateur central (alertes HA, alertes Meteo-France).
// Duree/amplitude : UIAnim::ALERT_* (180ms / 44px — etait 300ms / 100px).
void animate_alert_enter(lv_obj_t* alert_wrap) {
    if (!alert_wrap) return;
    const uint32_t DUR    = UIAnim::ALERT_DUR;
    const int32_t  OFFSET = UIAnim::ALERT_OFFSET;

    lv_obj_clear_flag(alert_wrap, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_x(alert_wrap, OFFSET);
    lv_obj_set_style_opa(alert_wrap, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_anim_t a_x;
    lv_anim_init(&a_x);
    lv_anim_set_var(&a_x, alert_wrap);
    lv_anim_set_values(&a_x, OFFSET, 0);
    lv_anim_set_time(&a_x, DUR);
    lv_anim_set_path_cb(&a_x, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a_x, anim_x_cb);
    lv_anim_start(&a_x);

    lv_anim_t a_o;
    lv_anim_init(&a_o);
    lv_anim_set_var(&a_o, alert_wrap);
    lv_anim_set_values(&a_o, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a_o, DUR);
    lv_anim_set_path_cb(&a_o, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a_o, anim_opa_cb);
    lv_anim_start(&a_o);
}

// Fondu croise pur entre deux calques plein cadre (previsions <-> switches HA).
// Pas de glissement : les deux calques occupent exactement la meme zone, un
// deplacement ferait "sauter" le contenu. Duree UIAnim::SWIPE_DUR.
// Annule aussi les anims X d'un swipe en cours et remet x=0 — sinon un tap HA
// pendant/juste apres un swipe laisse le calque decale ou en train de glisser.
// Si le calque entrant est deja visible (ex: swipe previsions sous overlay HA),
// on ne le remet pas transparent : rejouer l'entree le blankerait.
void animate_crossfade_layers(lv_obj_t* out_layer, lv_obj_t* in_layer) {
    if (out_layer == in_layer) return;
    const uint32_t DUR = UIAnim::SWIPE_DUR;

    if (out_layer) {
        lv_anim_delete(out_layer, anim_opa_cb);
        lv_anim_delete(out_layer, anim_x_cb);
        lv_obj_set_x(out_layer, 0);
        lv_anim_t a_out;
        lv_anim_init(&a_out);
        lv_anim_set_var(&a_out, out_layer);
        lv_anim_set_values(&a_out, LV_OPA_COVER, LV_OPA_TRANSP);
        lv_anim_set_time(&a_out, DUR);
        lv_anim_set_path_cb(&a_out, lv_anim_path_ease_in);
        lv_anim_set_exec_cb(&a_out, anim_opa_cb);
        lv_anim_set_ready_cb(&a_out, anim_hide_ready_cb);
        lv_anim_start(&a_out);
    }
    if (in_layer) {
        lv_anim_delete(in_layer, anim_opa_cb);
        lv_anim_delete(in_layer, anim_x_cb);
        lv_obj_set_x(in_layer, 0);

        const bool already_visible =
            !lv_obj_has_flag(in_layer, LV_OBJ_FLAG_HIDDEN) &&
            lv_obj_get_style_opa(in_layer, LV_PART_MAIN) > LV_OPA_TRANSP;

        lv_obj_clear_flag(in_layer, LV_OBJ_FLAG_HIDDEN);

        if (already_visible) {
            // Deja a l'ecran : rester opaque, pas de replay d'entree.
            lv_obj_set_style_opa(in_layer, LV_OPA_COVER, LV_PART_MAIN);
            return;
        }

        lv_obj_set_style_opa(in_layer, LV_OPA_TRANSP, LV_PART_MAIN);

        lv_anim_t a_in;
        lv_anim_init(&a_in);
        lv_anim_set_var(&a_in, in_layer);
        lv_anim_set_values(&a_in, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_time(&a_in, DUR);
        lv_anim_set_path_cb(&a_in, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&a_in, anim_opa_cb);
        lv_anim_start(&a_in);
    }
}

// =============================================================================
// Rouleau d'icone meteo
// L'icone est deja peinte (glyphe + offset de base poses par
// update_meteo_icon()) : on ne fait que la faire *entrer*. Elle part de
// base+ROLL_ICON_PX (en dessous) a opacite nulle et remonte a sa place en
// apparaissant. Pas de sortie animee : il n'y a que 2 labels par tuile (l1/l2),
// un vrai fondu croise demanderait 2 labels de plus par tuile (x10 tuiles).
// A 190ms le raccord se lit comme un basculement de volet, pas comme un saut.
// =============================================================================
bool g_forecast_roll_suppress = false;

static void roll_in_one_label(lv_obj_t* o, uint32_t delay_ms) {
    if (!o) return;
    if (lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;

    lv_anim_delete(o, anim_ty_cb);
    lv_anim_delete(o, anim_opa_cb);

    // Base = offset pose par update_meteo_icon() juste avant (l1_y / l2_y).
    const int32_t base = lv_obj_get_style_translate_y(o, LV_PART_MAIN);

    // Etat de depart applique tout de suite : avec un delay, LVGL n'appelle pas
    // exec_cb avant la fin du delai — sans ca l'icone clignoterait en place.
    lv_obj_set_style_translate_y(o, (lv_coord_t)(base + UIAnim::ROLL_ICON_PX), LV_PART_MAIN);
    lv_obj_set_style_opa(o, LV_OPA_TRANSP, LV_PART_MAIN);

    lv_anim_t a_y;
    lv_anim_init(&a_y);
    lv_anim_set_var(&a_y, o);
    lv_anim_set_values(&a_y, base + UIAnim::ROLL_ICON_PX, base);
    lv_anim_set_time(&a_y, UIAnim::ROLL_ICON);
    lv_anim_set_delay(&a_y, delay_ms);
    lv_anim_set_path_cb(&a_y, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a_y, anim_ty_cb);
    lv_anim_start(&a_y);

    lv_anim_t a_o;
    lv_anim_init(&a_o);
    lv_anim_set_var(&a_o, o);
    lv_anim_set_values(&a_o, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_time(&a_o, UIAnim::ROLL_ICON);
    lv_anim_set_delay(&a_o, delay_ms);
    lv_anim_set_path_cb(&a_o, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a_o, anim_opa_cb);
    lv_anim_start(&a_o);
}

void animate_icon_roll_in(lv_obj_t* l1, lv_obj_t* l2, uint32_t delay_ms) {
    if (g_forecast_roll_suppress) return;
    roll_in_one_label(l1, delay_ms);
    roll_in_one_label(l2, delay_ms);
}

// =============================================================================
// Horloge a rouleau
// =============================================================================
ClockRollerCtx g_clock_roller;

// Mesure la largeur d'un texte dans la police du label, sans toucher aux
// internes de la police : on ecrit le texte, on relance le layout, on lit la
// largeur, on remet l'ancien texte. Le label est en taille-contenu (defaut
// ESPHome pour un label sans width).
static int measure_label_text_w(lv_obj_t* lbl, const char* probe) {
    if (!lbl) return 0;
    char saved[16];
    const char* cur = lv_label_get_text(lbl);
    snprintf(saved, sizeof(saved), "%s", cur ? cur : "");
    lv_label_set_text(lbl, probe);
    lv_obj_update_layout(lbl);
    const int w = lv_obj_get_width(lbl);
    lv_label_set_text(lbl, saved);
    lv_obj_update_layout(lbl);
    return w;
}

void layout_clock_roller(lv_obj_t* clock_tile, esphome::font::Font* clock_font) {
    ClockRollerCtx& c = g_clock_roller;
    if (c.ready) return;
    if (!clock_tile || !clock_font || !c.colon) return;
    for (int i = 0; i < 4; i++) {
        if (!c.d[i].wrap || !c.d[i].lbl[0] || !c.d[i].lbl[1]) return;
    }

    lv_obj_update_layout(clock_tile);

    // --- Metriques exactes de la police (ESPHome les calcule au build) ---
    // clock_font DOIT etre la police posee sur les 4 labels en YAML.
    // Les chiffres montent exactement a la hauteur de capitale : capheight
    // donne donc la hauteur d'encre reelle, sans ratio devine.
    const int line_h     = clock_font->get_height();     // hauteur de ligne (152 @130b)
    const int baseline_y = clock_font->get_baseline();   // haut de boite -> ligne de base (121)
    const int cap_h      = clock_font->get_capheight();  // hauteur des chiffres (92)
    const int ink_top    = baseline_y - cap_h;           // marge vide au-dessus des chiffres (29)

    // Boite de rognage : juste l'encre + une marge de 6px en haut et en bas.
    // Elle doit rester plus courte que la boite du label, sinon le chiffre qui
    // arrive deborderait sur la date (posee 130px plus bas dans la tuile).
    const int pad = 6;
    const int box_h = cap_h + 2 * pad;
    const int lbl_y = -(ink_top - pad);              // recale l'encre dans la boite

    // Avance d'un chiffre (identique pour 0-9 : chiffres tabulaires).
    const int w_digit = measure_label_text_w(c.d[0].lbl[0], "8");
    const int w_colon = measure_label_text_w(c.colon, ":");
    if (w_digit <= 0 || box_h <= 0) return;

    // --- Centrage de HH:MM dans la tuile ---
    // Les deux chiffres d'un groupe sont colles (leur avance fait deja
    // l'espacement) ; seul le ":" recoit une respiration de chaque cote.
    const int gap = 4;
    const int total_w = 4 * w_digit + w_colon + 2 * gap;
    const int tile_w = lv_obj_get_content_width(clock_tile);
    const int x0 = (tile_w - total_w) / 2;
    // y de reference : celui pose en YAML sur le 1er wrap, corrige de la marge
    // d'encre supprimee (on veut les chiffres exactement ou ils etaient).
    const int y0 = lv_obj_get_y(c.d[0].wrap) + (ink_top - pad);

    const int x_digit[4] = {
        x0,
        x0 + w_digit,
        x0 + 2 * w_digit + gap + w_colon + gap,
        x0 + 3 * w_digit + gap + w_colon + gap,
    };

    for (int i = 0; i < 4; i++) {
        ClockDigitRoller& r = c.d[i];
        lv_obj_set_size(r.wrap, w_digit, box_h);
        lv_obj_set_pos(r.wrap, x_digit[i], y0);
        // Le rognage des enfants par le parent EST le rouleau : sans lui les
        // deux chiffres se verraient l'un au-dessus de l'autre pendant la
        // rotation. (Defaut LVGL, mis explicitement pour ne pas en dependre.)
        lv_obj_clear_flag(r.wrap, LV_OBJ_FLAG_OVERFLOW_VISIBLE);

        for (int k = 0; k < 2; k++) {
            lv_obj_set_y(r.lbl[k], lbl_y);
            // Le label en attente patiente hors de la boite (juste en dessous).
            lv_obj_set_style_translate_y(r.lbl[k], k == 0 ? 0 : box_h, LV_PART_MAIN);
        }
        r.cur = 0;
    }
    lv_obj_set_pos(c.colon, x0 + 2 * w_digit + gap, y0 + lbl_y);

    c.box_h = box_h;
    c.ready = true;

    // Trace unique au boot : la geometrie est deduite de la police, donc non
    // verifiable en lisant le YAML. Ces valeurs permettent de controler le
    // rendu sans avoir la dalle sous les yeux.
    ESP_LOGI("TAB5", "Rouleau horloge: line_h=%d baseline=%d cap=%d ink_top=%d box_h=%d "
                     "w_digit=%d w_colon=%d x0=%d y0=%d lbl_y=%d",
             line_h, baseline_y, cap_h, ink_top, box_h, w_digit, w_colon, x0, y0, lbl_y);
}

// Fait tourner un chiffre vers sa nouvelle valeur. Les deux labels glissent
// d'une hauteur de boite vers le haut : l'ancien sort par le haut, le nouveau
// — pose une boite plus bas — prend sa place.
static void roll_clock_digit(ClockDigitRoller& r, int box_h, char digit) {
    lv_obj_t* out_lbl = r.lbl[r.cur];
    lv_obj_t* in_lbl  = r.lbl[r.cur ^ 1];
    if (!out_lbl || !in_lbl) return;

    lv_anim_delete(out_lbl, anim_ty_cb);
    lv_anim_delete(in_lbl, anim_ty_cb);

    const char new_text[2] = {digit, '\0'};
    lv_label_set_text(in_lbl, new_text);
    lv_obj_set_style_translate_y(in_lbl, box_h, LV_PART_MAIN);

    lv_anim_t a_out;
    lv_anim_init(&a_out);
    lv_anim_set_var(&a_out, out_lbl);
    lv_anim_set_values(&a_out, 0, -box_h);
    lv_anim_set_time(&a_out, UIAnim::ROLL_CLOCK);
    lv_anim_set_path_cb(&a_out, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a_out, anim_ty_cb);
    lv_anim_start(&a_out);

    lv_anim_t a_in;
    lv_anim_init(&a_in);
    lv_anim_set_var(&a_in, in_lbl);
    lv_anim_set_values(&a_in, box_h, 0);
    lv_anim_set_time(&a_in, UIAnim::ROLL_CLOCK);
    lv_anim_set_path_cb(&a_in, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a_in, anim_ty_cb);
    lv_anim_start(&a_in);

    r.cur ^= 1;
    r.shown = digit;
}

// Pose un chiffre sans animation (premier affichage, ou layout pas encore pret).
// Les DEUX labels recoivent le texte : tant que layout_clock_roller() n'a pas
// tourne, box_h vaut 0 et le label en attente se superpose a l'affiche — avec
// le meme texte ca ne se voit pas, avec deux valeurs differentes si.
static void set_clock_digit_immediate(ClockDigitRoller& r, int box_h, char digit) {
    if (!r.lbl[r.cur]) return;
    const char text[2] = {digit, '\0'};
    lv_label_set_text(r.lbl[r.cur], text);
    lv_obj_set_style_translate_y(r.lbl[r.cur], 0, LV_PART_MAIN);
    if (r.lbl[r.cur ^ 1]) {
        lv_label_set_text(r.lbl[r.cur ^ 1], text);
        lv_obj_set_style_translate_y(r.lbl[r.cur ^ 1], box_h, LV_PART_MAIN);
    }
    r.shown = digit;
}

// Horloge + date (appelée par time: on_time de tab5-sensors-diagnostics.yaml).

void update_clock_date_ui(lv_obj_t* lbl_date,
    int hour, int minute, int day_of_week, int day_of_month, int month) {
    ClockRollerCtx& c = g_clock_roller;
    if (c.d[0].lbl[0]) {
        char hhmm[5];
        snprintf(hhmm, sizeof(hhmm), "%02d%02d", hour, minute);

        // Un rouleau par chiffre : de 22 a 23 mn, seule l'unite tourne.
        // shown == 0 (jamais peint) ou layout pas encore mesure -> pose directe.
        for (int i = 0; i < 4; i++) {
            ClockDigitRoller& r = c.d[i];
            if (r.shown == hhmm[i] && c.ready) continue;
            if (r.shown == 0 || !c.ready) set_clock_digit_immediate(r, c.box_h, hhmm[i]);
            else                          roll_clock_digit(r, c.box_h, hhmm[i]);
        }
    }
    if (lbl_date) {
        static const char* days[] = {"Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam"};
        const char* day = (day_of_week >= 1 && day_of_week <= 7) ? days[day_of_week - 1] : "";
        char buf_date[64];
        snprintf(buf_date, sizeof(buf_date), "%s %02d %s", day, day_of_month, clock_month_short_utf8(month));
        lv_label_set_recolor(lbl_date, false);
        lv_label_set_text(lbl_date, buf_date);
    }
}



// =============================================================================
// 1D : Micro-interactions boutons verre (transform_scale au pressed)
// ESPHome ne supporte pas state_pressed dans style_definitions -> on injecte
// un style pressed partage via lv_obj_add_style(obj, style, LV_STATE_PRESSED).
// La transition (80ms ease_out) est gereee nativement par LVGL.
// =============================================================================
static lv_style_t style_btn_pressed;
static bool btn_styles_inited = false;

static void ensure_btn_styles_inited() {
    if (btn_styles_inited) return;
    // Scale instantané, sans transition : réactivité maximale au tap.
    lv_style_init(&style_btn_pressed);
    lv_style_set_transform_scale_x(&style_btn_pressed, 240);  // 240/256 ~= 94%
    lv_style_set_transform_scale_y(&style_btn_pressed, 240);
    lv_style_set_bg_opa(&style_btn_pressed, LV_OPA_30);       // assombrit le verre
    btn_styles_inited = true;
}

void setup_button_press_animation(lv_obj_t* btn) {
    if (!btn) return;
    ensure_btn_styles_inited();
    // Pivot au centre pour un scale symetrique (pas depuis le coin haut-gauche).
    lv_obj_set_style_transform_pivot_x(btn, lv_obj_get_width(btn) / 2, LV_PART_MAIN);
    lv_obj_set_style_transform_pivot_y(btn, lv_obj_get_height(btn) / 2, LV_PART_MAIN);
    lv_obj_add_style(btn, &style_btn_pressed, LV_STATE_PRESSED);
}

void apply_pressed_scale_to_tree(lv_obj_t* root) {
    if (!root) return;
    // Heuristique : objet clickable + radius 18 = bouton verre (style_clim_btn).
    // Inclut aussi les tuiles meteo cliquables (effet desirable : feedback tactile).
    if (lv_obj_has_flag(root, LV_OBJ_FLAG_CLICKABLE)) {
        lv_coord_t radius = lv_obj_get_style_radius(root, LV_PART_MAIN);
        if (radius == 18) {
            setup_button_press_animation(root);
        }
    }
    // Recursion dans les enfants
    uint32_t cnt = lv_obj_get_child_cnt(root);
    for (uint32_t i = 0; i < cnt; i++) {
        apply_pressed_scale_to_tree(lv_obj_get_child(root, i));
    }
}

// Le jeu de bille a ete extrait dans marble_game.cpp (namespace Marble) :
// roguelite plein ecran, trop volumineux pour cohabiter ici.

void highlight_button_border(lv_obj_t* btn, bool active, uint32_t color) {
    if (!btn) return;
    lv_obj_set_style_border_color(btn, lv_color_hex(active ? color : UIColor::GLASS_RIM), LV_PART_MAIN);
    lv_obj_set_style_border_opa(btn, active ? LV_OPA_COVER : LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_border_width(btn, active ? 2 : 1, LV_PART_MAIN);
}
