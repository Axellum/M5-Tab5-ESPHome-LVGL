/**
 * [AI-CONTEXT]
 * @file game_common.h
 * @role Helpers PARTAGÉS par les 8 consoles (lot (f) de l'audit du 06/09/2026, fait le
 *       08/09/2026) : ils étaient recopiés à l'identique dans chaque `*_game.cpp`
 *       (`mk_rect`/`mk_label` ×8, `show`/`set_bg`/`set_border`/`set_text_if` ×8 à
 *       quelques gardes près, `clampf` ×3, xorshift32 ×5, cycle NVS + magic ×8).
 *       En-tête « header-only » : tout est `static inline` (une copie par unité,
 *       aucun warning « defined but not used ») ou template.
 * @architecture_constraint Ce fichier ne dépend que d'ESPHome/LVGL, jamais du HMI
 *       (pas de tab5_custom.h) : une console reste un sous-module isolé. Les
 *       palettes restent LOCALES à chaque jeu (`<Jeu>::Pal`), voir ADR-0014.
 * @ai_instruction Un jeu peut REDÉFINIR un helper dans son namespace (ex. `Go::set_bg`
 *       remet aussi `bg_grad_dir` à NONE) : sa définition masque celle-ci pour la
 *       recherche non qualifiée. C'est POUR ÇA que tout vit dans `namespace GameCommon`
 *       + `using namespace` : défini dans l'espace global, un helper serait aussi trouvé
 *       par ADL (l'argument `lv_obj_t*` est un type de l'espace global) et l'appel
 *       deviendrait ambigu. N'ajoute ici qu'un helper utilisé par au moins trois jeux.
 */
#pragma once
#include "esphome.h"
#include "esphome/core/preferences.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cstdint>
#include <cstring>

namespace esphome { namespace font { class Font; } }

namespace GameCommon {

// ---------------------------------------------------------------------------
// Arithmétique / aléa
// ---------------------------------------------------------------------------

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// xorshift32 (Marsaglia). L'ÉTAT reste dans chaque jeu (`static uint32_t s_rng`) :
// il est semé par run (tick, sauvegarde) et parfois persisté (Trivia). Ne jamais
// laisser l'état à 0 : la suite serait alors nulle pour toujours.
static inline uint32_t xorshift32_next(uint32_t& s) {
    s ^= s << 13;
    s ^= s >> 17;
    s ^= s << 5;
    return s;
}

// ---------------------------------------------------------------------------
// Widgets LVGL nus (aucun style du thème : on part d'une base connue)
// ---------------------------------------------------------------------------

// Rectangle nu : on retire tout le style du thème, ni scroll ni clic.
static inline lv_obj_t* mk_rect(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(o, LV_OPA_COVER, LV_PART_MAIN);
    return o;
}

// Label nu avec police ESPHome (nullptr = police LVGL par défaut) et couleur.
static inline lv_obj_t* mk_label(lv_obj_t* parent, const esphome::font::Font* f, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_remove_style_all(l);
    if (f) esphome::lvgl::lv_obj_set_style_text_font(l, f, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(color), LV_PART_MAIN);
    lv_label_set_text(l, "");
    return l;
}

// Affiche/masque sans invalider si l'état ne change pas (variante du flipper,
// équivalente aux autres : poser un flag déjà posé ne fait rien).
static inline void show(lv_obj_t* o, bool v) {
    if (!o) return;
    if (v == !lv_obj_has_flag(o, LV_OBJ_FLAG_HIDDEN)) return;
    if (v) lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
    else   lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
}

static inline void set_bg(lv_obj_t* o, uint32_t c, lv_opa_t opa = LV_OPA_COVER) {
    if (!o) return;
    lv_obj_set_style_bg_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(o, opa, LV_PART_MAIN);
}

static inline void set_border(lv_obj_t* o, uint32_t c, int w, lv_opa_t opa = LV_OPA_COVER) {
    if (!o) return;
    lv_obj_set_style_border_color(o, lv_color_hex(c), LV_PART_MAIN);
    lv_obj_set_style_border_width(o, w, LV_PART_MAIN);
    lv_obj_set_style_border_opa(o, opa, LV_PART_MAIN);
}

// Ne réécrit le texte que s'il change : évite une invalidation LVGL par tick.
static inline void set_text_if(lv_obj_t* l, const char* txt) {
    if (!l || !txt) return;
    const char* cur = lv_label_get_text(l);
    if (cur && strcmp(cur, txt) == 0) return;
    lv_label_set_text(l, txt);
}

// ---------------------------------------------------------------------------
// Persistance NVS d'une structure de sauvegarde (trivially copyable, champ `magic`)
// ---------------------------------------------------------------------------
// Cycle identique dans les 8 jeux : make_preference<T>(clé) une seule fois,
// load() puis comparaison du magic (un layout qui a changé = magic bumpé =
// sauvegarde rejetée, l'appelant remet SES défauts), save() qui pose le magic
// et force la synchro. `ready()` = load() a déjà été appelé (les jeux ne sauvent
// jamais avant d'avoir chargé).
template <typename T>
struct NvsSlot {
    NvsSlot(uint32_t key, uint32_t magic) : key_(key), magic_(magic) {}

    // false = absente, illisible ou magic différent : l'appelant remet ses défauts
    // (y compris `out.magic`, ou laisse save() le poser).
    bool load(T& out) {
        ensure();
        return pref_.load(&out) && out.magic == magic_;
    }

    bool save(T& v) {
        if (!ready_) return false;
        v.magic = magic_;
        const bool ok = pref_.save(&v);
        esphome::global_preferences->sync();
        return ok;
    }

    bool ready() const { return ready_; }

  private:
    void ensure() {
        if (ready_) return;
        pref_ = esphome::global_preferences->make_preference<T>(key_);
        ready_ = true;
    }
    esphome::ESPPreferenceObject pref_;
    bool ready_ = false;
    uint32_t key_;
    uint32_t magic_;
};

}  // namespace GameCommon

// Les jeux appellent les helpers sans qualification (comme avant, quand chacun avait
// sa copie) ; un helper redéfini dans le namespace d'un jeu reste prioritaire.
using namespace GameCommon;
