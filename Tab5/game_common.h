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
#include "esp_heap_caps.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <new>

namespace esphome { namespace font { class Font; } }

namespace GameCommon {

// ---------------------------------------------------------------------------
// Pause d'affichage
// ---------------------------------------------------------------------------

// Écart entre deux ticks au-delà duquel on considère que LVGL était en pause
// (écran éteint → `lvgl.pause`, tab5-hardware.yaml) ou la boucle bloquée : les
// chronos de partie ne décomptent pas ce temps-là. Un tick normal vaut 20 à
// 200 ms ; une image très lourde (overlay plein écran) ≈ 0,5 s ; 2 s laisse de
// la marge sans pénaliser un joueur (audit du 25/09/2026, lot 5).
static constexpr uint32_t PAUSE_GAP_MS = 2000;

// ---------------------------------------------------------------------------
// Mémoire d'un jeu : rien de réservé tant qu'il est fermé
// ---------------------------------------------------------------------------
// Audit du 26/09/2026 (lot 4, choix d'Axel : « pas de réserve mémoire pour les jeux
// si non actif »). L'état d'une partie — tableaux, pointeurs LVGL, tampons texte —
// vit dans une ou deux structures créées par open() et rendues par close() ; les
// objets LVGL du jeu sont détruits à la fermeture et reconstruits à l'ouverture.
//   Internal : lu à chaque tick → RAM interne d'abord, PSRAM en secours ;
//   Psram    : lu par coup ou par événement (historiques, tables de coups) →
//              PSRAM d'abord, comme les EXT_RAM_BSS_ATTR qu'il remplace.
// `new (p) T()` : les initialiseurs par défaut des membres reprennent ceux des
// anciens globaux, et chaque ouverture repart de cet état. Ce qui doit survivre à
// une fermeture (réglages, sauvegarde NVS déjà chargée…) reste hors de ces blocs.
enum class MemPref : uint8_t { Internal, Psram };  // pas HOT : macro d'ESPHome

template <typename T>
static inline T* game_mem_new(MemPref pref) {
    const uint32_t internal = MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT;
    const uint32_t psram    = MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT;
    void* p = heap_caps_malloc(sizeof(T), pref == MemPref::Internal ? internal : psram);
    if (!p) p = heap_caps_malloc(sizeof(T), pref == MemPref::Internal ? psram : internal);
    return p ? new (p) T() : nullptr;
}

template <typename T>
static inline void game_mem_free(T*& p) {
    if (!p) return;
    p->~T();
    heap_caps_free(p);
    p = nullptr;
}

// Détruit tout ce que le jeu a créé sous ses conteneurs YAML (ui_components/
// <jeu>_game.yaml), sans toucher aux conteneurs eux-mêmes : les enfants de `root`
// qui ne sont pas dans `keep` sont supprimés, les conteneurs de `keep` sont vidés.
// Sûr depuis le callback d'un objet supprimé : LVGL 9.5 marque l'événement en
// cours (lv_event_mark_deleted), remet l'entrée tactile à zéro et retire les
// animations de l'objet (lv_obj.c, lv_obj_tree.c).
static inline void ui_destroy(lv_obj_t* root, std::initializer_list<lv_obj_t*> keep) {
    for (lv_obj_t* c : keep) {
        if (c) lv_obj_clean(c);
    }
    if (!root) return;
    for (int i = (int) lv_obj_get_child_count(root) - 1; i >= 0; i--) {
        lv_obj_t* child = lv_obj_get_child(root, i);
        bool kept = false;
        for (lv_obj_t* c : keep) kept = kept || (c == child);
        if (!kept) lv_obj_delete(child);
    }
}

// ---------------------------------------------------------------------------
// Arithmétique / aléa
// ---------------------------------------------------------------------------

static inline float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

// xorshift32 (Marsaglia). L'ÉTAT reste dans chaque jeu (`s_rng` global, ou dans son
// bloc Mem s'il est ré-amorcé à chaque partie) : il est semé par run (tick,
// sauvegarde) et parfois persisté (Trivia). Ne jamais laisser l'état à 0 : la
// suite serait alors nulle pour toujours.
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

// Même principe pour la couleur du texte (lot 5 de l'audit ressources, 26/09/2026) :
// en LVGL 9.5, poser un style invalide l'objet même à valeur identique, et les HUD
// des échecs, du Go et de Trivia recoloraient leurs libellés à chaque tick.
static inline void set_text_color_if(lv_obj_t* o, uint32_t c) {
    if (!o) return;
    const lv_color_t want = lv_color_hex(c);
    lv_style_value_t cur;
    if (lv_obj_get_local_style_prop(o, LV_STYLE_TEXT_COLOR, &cur, LV_PART_MAIN) == LV_STYLE_RES_FOUND &&
        lv_color_eq(cur.color, want)) return;
    lv_obj_set_style_text_color(o, want, LV_PART_MAIN);
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

// ---------------------------------------------------------------------------
// Mécanismes que 2 à 4 consoles recopiaient (audit du 25/09/2026, lot 8f)
// ---------------------------------------------------------------------------
// Chaque helper reproduit EXACTEMENT l'ordre des opérations d'origine (mêmes
// flottants, mêmes comparaisons strictes) ; les seuils, périodes, délais et gardes
// d'état restent dans chaque jeu — ce sont eux qui font son ressenti.

// Période d'un lv_timer, écrite seulement si elle change : le cache évite un
// lv_timer_set_period à chaque tick. go, trivia, lode, pinball (tick adaptatif :
// rapide pendant l'action, lent dans les menus).
static inline void timer_period_sync(lv_timer_t* t, uint32_t& cache, uint32_t want) {
    if (!t || want == cache) return;
    cache = want;
    lv_timer_set_period(t, want);
}

// Insère `e` dans un classement trié par `.score` décroissant, de capacité N, dont
// `count` entrées sont valides. Un ex-aequo se range APRÈS les scores égaux
// (comparaison stricte). Rend le rang (0 = premier) ou -1 si hors classement ;
// `count` monte jusqu'à N. Un tableau à sentinelle (cases vides à 0, sans
// compteur) s'utilise avec count = N : pour un score > 0 c'est le même algorithme.
// arkanoid, lode, pinball.
template <typename T, size_t N, typename C>
static inline int topn_insert(T (&arr)[N], C& count, const T& e) {
    const int n = (int) count;
    int pos = n;
    for (int i = 0; i < n; i++) {
        if (e.score > arr[i].score) { pos = i; break; }
    }
    if (pos >= (int) N) return -1;
    for (int i = (n < (int) N ? n : (int) N - 1); i > pos; i--) arr[i] = arr[i - 1];
    arr[pos] = e;
    if (n < (int) N) count = (C) (n + 1);
    return pos;
}

// Calibration « à plat » : la lecture brute courante (en g) devient le zéro,
// stockée en milli-g tronqués. arkanoid, marble, lode, pinball.
static inline void tilt_calibrate(int16_t& cal_x, int16_t& cal_y, float raw_x, float raw_y) {
    cal_x = (int16_t) (raw_x * 1000.0f);
    cal_y = (int16_t) (raw_y * 1000.0f);
}

// Inclinaison lissée : lecture brute moins la calibration, puis moyenne
// exponentielle de coefficient k (0..1). marble (k = 0.38), lode (0.35).
static inline void tilt_smooth(float& tilt_x, float& tilt_y, float raw_x, float raw_y,
                               int16_t cal_x, int16_t cal_y, float k) {
    const float ox = cal_x / 1000.0f, oy = cal_y / 1000.0f;
    const float tx = raw_x - ox, ty = raw_y - oy;
    tilt_x += (tx - tilt_x) * k;
    tilt_y += (ty - tilt_y) * k;
}

// Norme de la variation d'accélération depuis l'échantillon précédent (la gravité
// s'annule par différence), puis mémorisation de l'échantillon. draughts, go.
static inline float accel_delta_norm(float ax, float ay, float az, float& px, float& py, float& pz) {
    const float dax = ax - px, day = ay - py, daz = az - pz;
    px = ax; py = ay; pz = az;
    return sqrtf(dax * dax + day * day + daz * daz);
}

// Déclencheur de secousse : vrai si `v` dépasse strictement `thresh` et que le
// dernier déclenchement date de plus de `cooldown_ms` (réarmé alors à `now`).
// chess (norme lissée, 1.9 g / 900 ms), trivia (norme, 2.2 g / 900 ms), draughts
// (variation, 1.2 / 800 ms), go (variation, 1.4 / 1200 ms).
static inline bool shake_fire(float v, float thresh, uint32_t& last_ms, uint32_t now, uint32_t cooldown_ms) {
    if (v > thresh && (now - last_ms) > cooldown_ms) {
        last_ms = now;
        return true;
    }
    return false;
}

}  // namespace GameCommon

// Les jeux appellent les helpers sans qualification (comme avant, quand chacun avait
// sa copie) ; un helper redéfini dans le namespace d'un jeu reste prioritaire.
using namespace GameCommon;
