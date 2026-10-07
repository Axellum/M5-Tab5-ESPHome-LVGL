/**
 * [AI-CONTEXT]
 * @file tab5_roue.cpp
 * @role Roue d'actions rapides (ADR-0036, 07/10/2026, discussion #278 : « one click opens
 *       a mini popup, like a spring, or a circular popup ») : jusqu'à six boutons ronds
 *       posés sur un arc autour d'une ancre (la pastille d'une carte du mode HA, le centre
 *       d'une tuile météo ; demain une ligne d'une liste), devant un halo de verre. Ce
 *       fichier ne sait rien des appareils : il place, peint, ouvre et ferme la roue. Celui
 *       qui l'ouvre (tuile_roue_ouvrir, tab5_tuiles.cpp) donne les boutons, la couleur
 *       d'état et ce que fait chaque toucher (rappel `choisir`), et sait la repeindre au
 *       changement de thème (rappel `rejouer`).
 * @architecture_constraint Widgets : ui_components/roue_actions.yaml (capteur plein écran,
 *       halo) et ses six roue_bouton.yaml, posés dans g_roue_ui par tab5-tuiles.yaml.
 *       Sous-fenêtre (SUBWINDOW du registre, comme la liste de la tuile − / +) : refermée
 *       avec les popups, par l'inactivité, à l'ouverture d'un popup (animate_popup_open),
 *       à l'extinction de l'écran et quand les définitions des tuiles changent ; repeinte,
 *       pas refermée, au changement de thème (la tâche « clair » du rendu compare la
 *       bascule à chaud au démarrage à froid, roue ouverte). Ouverture et fermeture
 *       sèches, sans animation (« transitions instantanées »).
 * @ai_instruction Géométrie (kRayon, kDiametre, kPasAngle, kPivot, kMarge, table kSin5) :
 *       tools/rendu/ecrans.py la refait (roue_centres) pour toucher « ⋯ » dans le rendu ;
 *       tests/test_roue.py compare les deux. Un glyphe de plus = sa ligne dans glyphe_roue
 *       et dans les glyphes de mdi_font_36 (règle 9 ; MDI_CODE_TARGETS rattache
 *       glyphe_roue aux labels roue_bouton_*_icone). Aucune couleur en dur : UIColor et la
 *       couleur d'état reçue.
 */
#include "tab5_internal.h"
#include "lvgl.h"
#include <algorithm>
#include <cstdio>

RoueUI g_roue_ui;

namespace {

// Écran (paysage) et géométrie de la roue (ADR-0036) : boutons ronds de 76 px, centres sur
// un arc de rayon 200 autour de l'ancre, 30° entre deux, l'éventail centré sur le haut
// (90°) ; 12 px de marge aux bords de l'écran ; un éventail qui déborde pivote par pas de
// 5°. Halo : 200 + 38 + 8 = 246 px de rayon autour de l'ancre.
constexpr int32_t kEcranL = 1280;
constexpr int32_t kEcranH = 720;
constexpr int32_t kRayon = 200;
constexpr int32_t kDiametre = 76;
constexpr int32_t kMarge = 12;
constexpr int kPasAngle = 30;
constexpr int kPivot = 5;
constexpr int32_t kHalo = 246;
// Fond du bouton de l'état courant : la couleur d'état à 20 %, comme la pastille des
// cartes du mode HA (switches_card.yaml : bg_opa 20 %).
constexpr lv_opa_t kOpaCourant = LV_OPA_20;

// sin des multiples de 5° de 0 à 90°, × 32767, arrondis : tout angle de la roue en est un
// (90 ± 15·(n−1), pas de 30 et de 5). Table plutôt que lv_trigo_sin : le rendu
// (tools/rendu/ecrans.py) refait exactement le même calcul.
constexpr int32_t kSin5[19] = {0,     2856,  5690,  8481,  11207, 13848, 16383, 18794, 21062, 23170,
                               25101, 26841, 28377, 29697, 30791, 31650, 32269, 32642, 32767};

int32_t sin5(int deg) {
    deg %= 360;
    if (deg < 0) deg += 360;
    int32_t signe = 1;
    if (deg >= 180) {
        deg -= 180;
        signe = -1;
    }
    if (deg > 90) deg = 180 - deg;
    return signe * kSin5[deg / kPivot];
}

int32_t cos5(int deg) { return sin5(deg + 90); }

// r · v / 32767, arrondi au plus proche (moitié loin de zéro).
int32_t echelle(int32_t r, int32_t v) {
    const int32_t p = r * v;
    return p >= 0 ? (p + 16383) / 32767 : -((-p + 16383) / 32767);
}

// Centres des n boutons dans le sens horaire (de gauche à droite sans pivot) autour de
// l'ancre (xa, ya), dans le repère de l'écran. L'arc est au-dessus de l'ancre ; en
// dessous seulement s'il n'y tient pas (une ancre près du haut : une ligne de liste). Un
// bouton qui sortirait à gauche ou à droite (tuile du bord) fait pivoter l'éventail de 5°
// en 5° du côté opposé jusqu'à ce que tous tiennent : l'éventail descend alors le long
// de la tuile. Au pire (tuile du bord, six boutons, ancre basse), un bouton passé sous
// l'écran y est ramené.
void disposer(int32_t xa, int32_t ya, int n, int32_t cx[], int32_t cy[]) {
    constexpr int32_t r = kDiametre / 2;
    const bool dessous = ya - kRayon - r < kMarge;
    int decalage = 0;
    for (int essai = 0; essai <= 180 / kPivot; essai++) {
        int32_t gauche = kEcranL, droite = 0;
        for (int i = 0; i < n; i++) {
            const int a = 90 + (n - 1) * kPasAngle / 2 - i * kPasAngle + decalage;
            cx[i] = xa + echelle(kRayon, cos5(a));
            const int32_t dy = echelle(kRayon, sin5(a));
            cy[i] = dessous ? ya + dy : ya - dy;
            gauche = std::min(gauche, cx[i]);
            droite = std::max(droite, cx[i]);
        }
        if (gauche - r < kMarge) decalage -= kPivot;
        else if (droite + r > kEcranL - kMarge) decalage += kPivot;
        else break;
    }
    for (int i = 0; i < n; i++) {
        cx[i] = std::clamp(cx[i], kMarge + r, kEcranL - kMarge - r);
        cy[i] = std::clamp(cy[i], kMarge + r, kEcranH - kMarge - r);
    }
}

// Glyphes des boutons (mdi_font_36, règle 9 : MDI_CODE_TARGETS de check_tab5_code_rules.py).
const char* glyphe_roue(RoueIcone i) {
    switch (i) {
        case RoueIcone::ETEINDRE: return "\U000F0425";   // power
        case RoueIcone::OUVRIR: return "\U000F0737";     // arrow-up-bold
        case RoueIcone::STOP: return "\U000F04DB";       // stop
        case RoueIcone::FERMER: return "\U000F072E";     // arrow-down-bold
        case RoueIcone::CHAUFFER: return "\U000F0238";   // fire
        case RoueIcone::REFROIDIR: return "\U000F0717";  // snowflake
        case RoueIcone::SECHER: return "\U000F058E";     // water-percent
        case RoueIcone::VENTILER: return "\U000F0210";   // fan
        default: return "\U000F01D8";                    // dots-horizontal : « ⋯ »
    }
}

// Ouverte depuis : nombre de boutons montrés, et les rappels de celui qui l'a ouverte.
struct Roue {
    int n = 0;
    void (*choisir)(int) = nullptr;
    void (*rejouer)() = nullptr;
};
Roue s_roue;

// Fond du bouton : la couleur d'état à 20 % pour l'état courant ; sinon le verre de son
// style (style_clim_btn), propriétés locales retirées.
void fond_bouton(lv_obj_t* b, bool courant, uint32_t couleur) {
    if (b == nullptr) return;
    if (courant) {
        lv_obj_set_style_bg_color(b, lv_color_hex(couleur), LV_PART_MAIN);
        lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_NONE, LV_PART_MAIN);
        lv_obj_set_style_bg_opa(b, kOpaCourant, LV_PART_MAIN);
        return;
    }
    for (lv_style_prop_t p : {LV_STYLE_BG_COLOR, LV_STYLE_BG_GRAD_DIR, LV_STYLE_BG_OPA})
        lv_obj_remove_local_style_prop(b, p, LV_PART_MAIN);
}

bool ouverte() {
    const lv_obj_t* f = g_roue_ui.fond;
    return f != nullptr && !lv_obj_has_flag(f, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

// ─── API (tab5_custom.h, tab5_internal.h) ───────────────────────────────────────────

void roue_brancher() {
    static bool fait = false;
    RoueUI& u = g_roue_ui;
    if (fait || u.fond == nullptr) return;
    fait = true;
    // Un glissement sur la roue ouverte ne remonte pas jusqu'à la page (swipe des
    // prévisions ou des pièces sous elle) : un toucher hors des boutons la ferme, un
    // geste ne fait rien.
    lv_obj_remove_flag(u.fond, LV_OBJ_FLAG_GESTURE_BUBBLE);
    // Effet d'appui des boutons de verre (tab5_anim.cpp) : il retient les boutons de
    // rayon 18 et ceux marqués USER_1. Les boutons ronds de la roue sont marqués ici, à
    // la fin du setup, avant apply_pressed_scale_to_tree (2 s après le démarrage).
    for (lv_obj_t* b : u.bouton)
        if (b != nullptr) lv_obj_add_flag(b, LV_OBJ_FLAG_USER_1);
}

bool roue_ouvrir(lv_obj_t* ancre, const RoueBouton* b, int n, uint32_t couleur, void (*choisir)(int),
                 void (*rejouer)()) {
    RoueUI& u = g_roue_ui;
    if (u.fond == nullptr || u.halo == nullptr || ancre == nullptr || b == nullptr || n < 1 || n > kRoueBoutons)
        return false;
    // Centre de l'ancre, dans le repère du capteur plein écran (posé en 0, 0).
    lv_obj_update_layout(ancre);
    lv_area_t a, f;
    lv_obj_get_coords(ancre, &a);
    lv_obj_get_coords(u.fond, &f);
    const int32_t xa = a.x1 + lv_area_get_width(&a) / 2 - f.x1;
    const int32_t ya = a.y1 + lv_area_get_height(&a) / 2 - f.y1;
    int32_t cx[kRoueBoutons], cy[kRoueBoutons];
    disposer(xa, ya, n, cx, cy);

    // Halo : disque de verre centré sur l'ancre (coupé par les bords de l'écran), bord
    // dans la couleur d'état (opacité de roue_actions.yaml).
    ui_x(u.halo, xa - kHalo);
    ui_y(u.halo, ya - kHalo);
    lv_obj_set_style_border_color(u.halo, lv_color_hex(couleur), LV_PART_MAIN);

    for (int i = 0; i < kRoueBoutons; i++) {
        const bool montre = i < n;
        ui_hidden(u.bouton[i], !montre);
        if (!montre) continue;
        ui_x(u.bouton[i], cx[i] - kDiametre / 2);
        ui_y(u.bouton[i], cy[i] - kDiametre / 2);
        const RoueBouton& d = b[i];
        const bool plus = d.pct == 0 && d.icone == RoueIcone::PLUS;
        const uint32_t encre = d.courant ? couleur : (plus ? UIColor.TEXT_SOFT : UIColor.TEXT_PRIMARY);
        fond_bouton(u.bouton[i], d.courant, couleur);
        // Un pourcentage s'écrit (police UI grasse) à la place de l'icône.
        ui_hidden(u.icone[i], d.pct != 0);
        ui_hidden(u.texte[i], d.pct == 0);
        if (d.pct != 0) {
            char txt[8];
            snprintf(txt, sizeof(txt), "%u %%", static_cast<unsigned>(d.pct));
            ui_text(u.texte[i], txt);
            ui_text_color(u.texte[i], encre);
        } else {
            ui_text(u.icone[i], glyphe_roue(d.icone));
            ui_text_color(u.icone[i], encre);
        }
    }
    s_roue.n = n;
    s_roue.choisir = choisir;
    s_roue.rejouer = rejouer;
    ui_hidden(u.fond, false);
    return true;
}

void roue_actions_choisir(int n) {
    if (!ouverte() || n < 0 || n >= s_roue.n) return;
    void (*choisir)(int) = s_roue.choisir;
    roue_actions_fermer();
    if (choisir != nullptr) choisir(n);
}

void roue_actions_fermer() { ui_hidden(g_roue_ui.fond, true); }

bool roue_actions_ouverte() { return ouverte(); }

void roue_rejouer_theme() {
    if (ouverte() && s_roue.rejouer != nullptr) s_roue.rejouer();
}
