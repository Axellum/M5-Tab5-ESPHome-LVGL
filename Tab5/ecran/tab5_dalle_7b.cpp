/**
 * [AI-CONTEXT]
 * @file tab5_dalle_7b.cpp
 * @role Écran de la Waveshare 7B vraiment noir quand le rétroéclairage s'éteint (16/10/2026,
 *       audit de la 7B) : un voile noir couvre tout l'écran juste avant que LVGL se fige.
 * @architecture_constraint Le rétroéclairage de la 7B se règle en analogique (GPIO32 → filtre
 *       RC → contre-réaction du pilote AP3032, schéma Waveshare) : à 0 % les LED ne s'éteignent
 *       pas, et la dernière image, figée par `lvgl.pause`, restait visible (vu par Axel le
 *       16/10/2026 avec l'extinction auto à 1 min ; signalé aussi sur le forum Home Assistant,
 *       fil « 7B working config »). La commande DCS « display off » (0x28) de la dalle EK79007
 *       a été essayée le même soir : envoyée sans erreur, sans effet visible, abandonnée.
 *       Sur une dalle IPS, des pixels noirs bloquent la lumière qui reste : on dessine donc un
 *       objet noir plein écran sur la couche système de LVGL (au-dessus des popups), on force
 *       son dessin (`lv_refr_now`), puis le rétroéclairage fige LVGL. Au rallumage, le voile
 *       est caché avant `lvgl.resume`, qui redessine l'écran entier.
 *       Noir absolu (`lv_color_black()`), pas un rôle de la palette : c'est l'écran éteint,
 *       aucun thème ne doit le changer (règle 8 de check_tab5_code_rules.py : seules les
 *       couleurs littérales `lv_color_hex(0x…)` y sont visées).
 * @ai_instruction Le voile ne reçoit aucun appui (pas cliquable) : pendant l'extinction, LVGL
 *       est figé et le toucher passe par `on_release` du tactile, qui rallume. Ne pas le
 *       déplacer sur `lv_layer_top()` : un popup de la couche du haut passerait au-dessus.
 */
#include "tab5_dalle_7b.h"

#include "esphome/core/log.h"
#include "lvgl.h"

namespace {

constexpr const char* TAG = "tab5.dalle";
lv_obj_t* s_voile = nullptr;

lv_obj_t* voile() {
    if (s_voile != nullptr) return s_voile;
    s_voile = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(s_voile);
    lv_obj_set_size(s_voile, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_voile, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_voile, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_voile, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_voile, LV_OBJ_FLAG_HIDDEN);
    return s_voile;
}

}  // namespace

void dalle_noire(bool noire) {
    if (!noire) {
        // Rallumage (et démarrage, rétroéclairage restauré) : rien à retirer tant qu'aucune
        // extinction n'a créé le voile.
        if (s_voile == nullptr) return;
        lv_obj_add_flag(s_voile, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Voile noir retire");
        return;
    }
    if (lv_display_get_default() == nullptr) return;  // LVGL pas encore prêt
    lv_obj_remove_flag(voile(), LV_OBJ_FLAG_HIDDEN);
    lv_refr_now(nullptr);  // dessiné avant que lvgl.pause fige l'affichage
    ESP_LOGI(TAG, "Ecran au noir");
}
