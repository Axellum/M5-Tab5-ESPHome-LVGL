/**
 * [AI-CONTEXT]
 * @file tab5_roue.cpp
 * @role Roue d'actions rapides (ADR-0036, 07/10/2026, discussion #278 : « one click opens
 *       a mini popup, like a spring, or a circular popup ») : un moyeu posé sur une ancre
 *       (la pastille d'une carte du mode HA, le centre d'une tuile météo, la pastille d'une
 *       ligne du popup Maison) et deux anneaux de boutons ronds au-dessus, devant un voile.
 *       Premier anneau : les commandes de l'appareil, ses familles de réglages et deux
 *       liens aux bouts (« Maison », « Détails ») ; toucher une famille déplie, sur le
 *       second anneau et centrés sur elle, ses choix (luminosités, couleurs, modes…) —
 *       la « roue qui en lance une deuxième, dans la même roue » voulue par l'auteur le
 *       07/10/2026. Ce fichier ne sait rien des appareils : il place, peint, ouvre, déplie
 *       et ferme. Celui qui l'ouvre (tuile_roue_ouvrir, tab5_tuiles.cpp) donne le moyeu, les
 *       boutons, la couleur d'état et les rappels (RoueRappels) : ce que fait un toucher,
 *       les choix d'une famille, le repeint au changement de thème ou d'état.
 * @architecture_constraint Widgets : ui_components/roue_actions.yaml (voile plein écran,
 *       bandes, jauge, moyeu, mots) et ses roue_bouton.yaml, roue_choix.yaml,
 *       roue_legende.yaml, posés dans g_roue_ui par tab5-tuiles.yaml. Sous-fenêtre
 *       (SUBWINDOW du registre, comme la liste de la tuile − / +) : refermée avec les
 *       popups, par l'inactivité, à l'ouverture d'un popup (animate_popup_open), à
 *       l'extinction de l'écran et quand les définitions des tuiles changent ; repeinte, pas
 *       refermée, au changement de thème (la tâche « clair » du rendu compare la bascule à
 *       chaud au démarrage à froid, roue ouverte). Ouverture, dépliage et fermeture secs,
 *       sans animation (« transitions instantanées »). Couleurs : la palette (UIColor), la
 *       couleur d'état reçue et, pour les pastilles d'une lampe, la couleur qu'elle prendra
 *       (une donnée, comme dans le popup lumière) ; aucune littérale ici (règle 8).
 * @ai_instruction Géométrie (kRayon, kDiametre, kRayon2, kDiametre2, kPasAngle,
 *       kPasAngle2, kPivot, kMarge, table kSin5) : tools/rendu/ecrans.py la refait
 *       (roue_centres, roue_choix_centres) pour toucher « Détails » et les choix dans le
 *       rendu ; tests/test_roue.py compare les deux. Un glyphe de plus = sa ligne dans
 *       glyphe_roue et dans les glyphes de mdi_font_36 (règle 9 ; MDI_CODE_TARGETS rattache
 *       glyphe_roue aux labels roue_bouton_*_icone et roue_choix_*_icone).
 */
#include "tab5_internal.h"
#include "tab5_geometrie.h"
#include "lvgl.h"
#include <algorithm>
#include <cstdio>

RoueUI g_roue_ui;

namespace {

// Écran (paysage, kEcranL × kEcranH : tab5_geometrie.h) et géométrie de la roue (ADR-0036) :
// premier anneau de boutons de 72 px,
// centres à 180 px de l'ancre, 30° entre deux ; second anneau de boutons de 72 px à 290 px,
// 20° entre deux, centré sur la famille touchée ; 12 px de marge aux bords ; un éventail
// qui déborde pivote par pas de 5°. Moyeu de 120 px sur l'ancre, jauge de 148 px autour.
constexpr int32_t kRayon = 180;
constexpr int32_t kDiametre = 72;
constexpr int32_t kRayon2 = 290;
constexpr int32_t kDiametre2 = 72;
constexpr int32_t kMarge = 12;
constexpr int kPasAngle = 30;
constexpr int kPasAngle2 = 20;
constexpr int kPivot = 5;
constexpr int32_t kMoyeu = 120;
constexpr int32_t kJauge = 148;
// Bandes de verre sous les anneaux (arcs à bouts ronds) : largeur, et une seconde bande
// plus étroite au milieu (l'indicateur de l'arc, en retrait de kRetrait de chaque côté) qui
// donne au verre un cœur plus clair que ses bords. 12 px de verre autour des boutons ; 14 px
// entre les deux bandes.
constexpr int32_t kBande = 96;
constexpr int32_t kBande2 = 96;
constexpr int32_t kRetrait = 10;
// Mots : celui d'un choix au-delà de son bouton (centre à kRayon2 + kLegende2), ceux des
// liens sous leur bouton ; labels de kLegendeL px, texte centré. Nom sous le moyeu.
constexpr int32_t kLegende2 = 60;
constexpr int32_t kLegendeL = 150;
constexpr int32_t kLegendeH = 28;
constexpr int32_t kNomL = 320;
// Point d'une famille : 8 px, à l'intérieur du bouton, vers l'extérieur de la roue.
constexpr int32_t kPoint = 8;

// sin des multiples de 5° de 0 à 90°, × 32767, arrondis : tout angle de la roue en est un
// (pas de 30, de 20 et de 5, centres sur 90 ou sur une famille). Table plutôt que
// lv_trigo_sin : le rendu (tools/rendu/ecrans.py) refait exactement le même calcul.
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

int normaliser(int deg) {
    deg %= 360;
    return deg < 0 ? deg + 360 : deg;
}

// Centres de n boutons de rayon `r` sur un arc de rayon `rayon` autour de l'ancre (xa, ya),
// `pas` degrés entre deux, l'éventail centré sur l'angle `centre` (90 : la verticale), dans
// le sens horaire (de gauche à droite sans pivot), dans le repère de l'écran. Au-dessus de
// l'ancre, en dessous si `dessous` (symétrie). Un bouton qui sortirait à gauche ou à droite
// fait pivoter l'éventail de 5° en 5° du côté opposé, un bouton qui sortirait en haut ou en
// bas vers la verticale, jusqu'à ce que tous tiennent ; au pire, un bouton est ramené dans
// l'écran. `angles` : l'angle de chaque centre (le second
// anneau se centre sur celui de sa famille).
void disposer(int32_t xa, int32_t ya, int n, int32_t rayon, int pas, int centre, int32_t r, bool dessous,
              int32_t cx[], int32_t cy[], int angles[]) {
    int decalage = 0;
    for (int essai = 0; essai <= 180 / kPivot; essai++) {
        int32_t gauche = kEcranL, droite = 0;
        for (int i = 0; i < n; i++) {
            const int a = centre + (n - 1) * pas / 2 - i * pas + decalage;
            angles[i] = a;
            cx[i] = xa + echelle(rayon, cos5(a));
            const int32_t dy = echelle(rayon, sin5(a));
            cy[i] = dessous ? ya + dy : ya - dy;
            gauche = std::min(gauche, cx[i]);
            droite = std::max(droite, cx[i]);
        }
        int sens = 0;
        if (gauche - r < kMarge) sens = -kPivot;
        else if (droite + r > kEcranL - kMarge) sens = kPivot;
        // Un bouton qui sortirait en haut ou en bas (un éventail déjà pivoté près d'un bord
        // descend sous l'ancre) : vers la verticale de l'ancre.
        for (int i = 0; i < n && sens == 0; i++)
            if (cy[i] - r < kMarge || cy[i] + r > kEcranH - kMarge) sens = angles[i] < 90 ? kPivot : -kPivot;
        if (sens == 0) break;
        decalage += sens;
    }
    for (int i = 0; i < n; i++) {
        cx[i] = std::clamp(cx[i], kMarge + r, kEcranL - kMarge - r);
        cy[i] = std::clamp(cy[i], kMarge + r, kEcranH - kMarge - r);
    }
}

// Glyphes des boutons (mdi_font_36, règle 9 : MDI_CODE_TARGETS de check_tab5_code_rules.py).
// Ceux de la clim sont ceux de son popup (modes, préréglages, ventilation, flux d'air).
const char* glyphe_roue(RoueIcone i) {
    switch (i) {
        case RoueIcone::ETEINDRE: return "\U000F0425";     // power
        case RoueIcone::ALLUMER: return "\U000F06E8";      // lightbulb-on
        case RoueIcone::LUMINOSITE: return "\U000F00DF";   // brightness-6
        case RoueIcone::BLANCS: return "\U000F05A8";       // white-balance-sunny
        case RoueIcone::COULEURS: return "\U000F03D8";     // palette
        case RoueIcone::OUVRIR: return "\U000F0737";       // arrow-up-bold
        case RoueIcone::STOP: return "\U000F04DB";         // stop
        case RoueIcone::FERMER: return "\U000F072E";       // arrow-down-bold
        case RoueIcone::POSITION: return "\U000F084F";     // arrow-expand-vertical
        case RoueIcone::MODE: return "\U000F0393";         // thermostat
        case RoueIcone::CHAUFFER: return "\U000F0238";     // fire
        case RoueIcone::REFROIDIR: return "\U000F0717";    // snowflake
        case RoueIcone::SECHER: return "\U000F058E";       // water-percent
        case RoueIcone::VENTILER: return "\U000F0210";     // fan
        case RoueIcone::CONSIGNE: return "\U000F050F";     // thermometer
        case RoueIcone::OPTIONS: return "\U000F0AE2";      // star-four-points
        case RoueIcone::ECO: return "\U000F032A";          // leaf
        case RoueIcone::BOOST: return "\U000F14DE";        // rocket-launch
        case RoueIcone::SILENCE: return "\U000F146D";      // fan-chevron-down
        case RoueIcone::OSCILLATION: return "\U000F0E79";  // arrow-up-down
        case RoueIcone::BRISE: return "\U000F059D";        // weather-windy
        case RoueIcone::MAISON: return "\U000F02DC";       // home
        case RoueIcone::REGLAGES: return "\U000F1542";     // tune-variant
        default: return "";
    }
}

// Roue ouverte : ce qu'il faut pour la repeindre, la déplier et répondre à un toucher.
struct Roue {
    int n = 0;
    RoueBouton bouton[kRoueBoutons] = {};
    int angle[kRoueBoutons] = {};
    int32_t cx[kRoueBoutons] = {};
    int32_t cy[kRoueBoutons] = {};
    int famille = -1;  // bouton dont le second anneau est déplié ; -1 : replié
    int m = 0;         // choix montrés
    int32_t xa = 0;
    int32_t ya = 0;
    bool dessous = false;
    uint32_t couleur = 0;
    RoueRappels rappels;
};
Roue s_roue;

bool ouverte() {
    const lv_obj_t* f = g_roue_ui.fond;
    return f != nullptr && !lv_obj_has_flag(f, LV_OBJ_FLAG_HIDDEN);
}

void taille(lv_obj_t* o, int32_t w, int32_t h) {
    if (o == nullptr) return;
    if (lv_obj_get_style_width(o, LV_PART_MAIN) != w || lv_obj_get_style_height(o, LV_PART_MAIN) != h)
        lv_obj_set_size(o, w, h);
}

lv_color_t melange(uint32_t a, uint32_t b, uint8_t part_a) {
    return lv_color_mix(lv_color_hex(a), lv_color_hex(b), part_a);
}

// Propriétés locales que la roue pose sur un bouton (verre teinté, liseré, halo) : retirées
// avant chaque peinture, le bouton retrouve son verre partagé (style_clim_btn) et ce qu'un
// thème en fait (formes).
constexpr lv_style_prop_t kProprietes[] = {
    LV_STYLE_BG_COLOR,     LV_STYLE_BG_GRAD_COLOR, LV_STYLE_BG_GRAD_DIR, LV_STYLE_BG_OPA,
    LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_WIDTH,  LV_STYLE_BORDER_OPA,  LV_STYLE_SHADOW_WIDTH,
    LV_STYLE_SHADOW_COLOR, LV_STYLE_SHADOW_OPA,    LV_STYLE_SHADOW_SPREAD,
};

enum class Aspect : uint8_t { VERRE, COURANT, DEPLIE, LIEN, PASTILLE, PASTILLE_COURANTE };

// Dernier aspect posé sur un bouton (UI-7, audit du 07/10/2026). La roue se repeint à
// chaque état poussé par HA, à chaque dépliage et au changement de thème ; reposer les
// mêmes propriétés locales invaliderait le bouton, et son halo de 24 px, pour rien. Un
// bouton n'est repeint que si son aspect, sa couleur ou une couleur de la palette qu'il
// lit (thème) a changé. Seule la roue pose ces propriétés (kProprietes) ; un thème ne
// touche que les styles partagés.
struct Pose {
    Aspect aspect = Aspect::VERRE;
    bool posee = false;    // rien posé encore : le premier repeint pose toujours
    uint32_t couleur = 0;  // couleur d'état ou de la pastille
    uint32_t haut = 0;     // couleurs de la palette lues (GLASS_HI / GLASS_LO, liseré)
    uint32_t bas = 0;
    bool operator==(const Pose& o) const {
        return aspect == o.aspect && posee == o.posee && couleur == o.couleur && haut == o.haut && bas == o.bas;
    }
};
Pose s_pose_bouton[kRoueBoutons];
Pose s_pose_choix[kRoueChoix];

// Vrai si `pose` diffère de ce qui est posé : `posee` devient `pose`, propriétés de la roue
// retirées (le bouton retrouve son verre partagé, style_clim_btn, et ce qu'un thème en
// fait) ; faux : le bouton a déjà cet aspect, rien à faire.
bool reposer(lv_obj_t* b, Pose& posee, Pose pose) {
    pose.posee = true;
    if (pose == posee) return false;
    posee = pose;
    for (lv_style_prop_t p : kProprietes) lv_obj_remove_local_style_prop(b, p, LV_PART_MAIN);
    return true;
}

// Aspect d'un bouton rond : le verre des boutons des popups ; l'état courant de l'appareil
// en verre teinté de sa couleur, liseré plein et halo (la seule ombre de la roue : une
// ombre coûte à chaque repeint, docs/performance.md) ; une famille dépliée en verre
// légèrement teinté et liseré ; un lien en verre transparent (contour seul).
void aspect(lv_obj_t* b, Pose& posee, Aspect a, uint32_t couleur) {
    if (b == nullptr) return;
    Pose pose;
    pose.aspect = a;
    if (a == Aspect::COURANT || a == Aspect::DEPLIE) {
        pose.couleur = couleur;
        pose.haut = UIColor.GLASS_HI;
        pose.bas = UIColor.GLASS_LO;
    }
    if (!reposer(b, posee, pose)) return;
    switch (a) {
        case Aspect::COURANT:
            lv_obj_set_style_bg_color(b, melange(couleur, UIColor.GLASS_HI, 140), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_color(b, melange(couleur, UIColor.GLASS_LO, 80), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_color(b, lv_color_hex(couleur), LV_PART_MAIN);
            lv_obj_set_style_border_width(b, 2, LV_PART_MAIN);
            lv_obj_set_style_border_opa(b, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_shadow_color(b, lv_color_hex(couleur), LV_PART_MAIN);
            lv_obj_set_style_shadow_width(b, 24, LV_PART_MAIN);
            lv_obj_set_style_shadow_spread(b, 1, LV_PART_MAIN);
            lv_obj_set_style_shadow_opa(b, LV_OPA_50, LV_PART_MAIN);
            break;
        case Aspect::DEPLIE:
            lv_obj_set_style_bg_color(b, melange(couleur, UIColor.GLASS_HI, 70), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_color(b, melange(couleur, UIColor.GLASS_LO, 30), LV_PART_MAIN);
            lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, LV_PART_MAIN);
            lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_border_color(b, lv_color_hex(couleur), LV_PART_MAIN);
            lv_obj_set_style_border_width(b, 2, LV_PART_MAIN);
            lv_obj_set_style_border_opa(b, LV_OPA_COVER, LV_PART_MAIN);
            break;
        case Aspect::LIEN:
            lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_width(b, 2, LV_PART_MAIN);
            lv_obj_set_style_border_opa(b, LV_OPA_50, LV_PART_MAIN);
            break;
        default:
            break;
    }
}

// Pastille de couleur d'une lampe : la couleur, plus claire en haut et plus sombre en bas
// (une bille), liseré de verre ; celle de l'état courant cerclée et auréolée.
void aspect_pastille(lv_obj_t* b, Pose& posee, uint32_t c, bool courant) {
    if (b == nullptr) return;
    Pose pose;
    pose.aspect = courant ? Aspect::PASTILLE_COURANTE : Aspect::PASTILLE;
    pose.couleur = c;
    pose.haut = courant ? UIColor.TEXT_PRIMARY : UIColor.GLASS_RIM;
    if (!reposer(b, posee, pose)) return;
    const lv_color_t couleur = lv_color_hex(c);
    lv_obj_set_style_bg_color(b, lv_color_lighten(couleur, 85), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_color(b, lv_color_darken(couleur, 40), LV_PART_MAIN);
    lv_obj_set_style_bg_grad_dir(b, LV_GRAD_DIR_VER, LV_PART_MAIN);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_border_color(b, lv_color_hex(courant ? UIColor.TEXT_PRIMARY : UIColor.GLASS_RIM), LV_PART_MAIN);
    lv_obj_set_style_border_width(b, courant ? 3 : 2, LV_PART_MAIN);
    lv_obj_set_style_border_opa(b, courant ? LV_OPA_COVER : LV_OPA_60, LV_PART_MAIN);
    if (courant) {
        lv_obj_set_style_shadow_color(b, couleur, LV_PART_MAIN);
        lv_obj_set_style_shadow_width(b, 24, LV_PART_MAIN);
        lv_obj_set_style_shadow_spread(b, 1, LV_PART_MAIN);
        lv_obj_set_style_shadow_opa(b, LV_OPA_60, LV_PART_MAIN);
    }
}

// Bande de verre sous un anneau : un arc à bouts ronds de rayon `rayon` (au centre des
// boutons) autour de l'ancre, du bouton le plus à gauche (angle a_max) au plus à droite
// (a_min), dans le sens des aiguilles d'une montre — par le haut, ou par le bas si la roue
// est sous l'ancre. Les bouts ronds (rayon kBande / 2) enveloppent les boutons extrêmes.
void bande(lv_obj_t* arc, int32_t rayon, int32_t largeur, int a_max, int a_min, uint32_t teinte,
           uint8_t part_teinte) {
    if (arc == nullptr) return;
    const Roue& s = s_roue;
    const int32_t d = 2 * rayon + largeur;
    taille(arc, d, d);
    ui_x(arc, s.xa - d / 2);
    ui_y(arc, s.ya - d / 2);
    if (a_max == a_min) {
        a_max++;
        a_min--;
    }
    const int debut = normaliser(s.dessous ? a_min : -a_max);
    const int fin = normaliser(s.dessous ? a_max : -a_min);
    lv_arc_set_bg_angles(arc, debut, fin);
    lv_arc_set_angles(arc, debut, fin);
    const lv_color_t verre = melange(teinte, UIColor.GLASS_HI, part_teinte);
    lv_obj_set_style_arc_width(arc, largeur, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, verre, LV_PART_MAIN);
    lv_obj_set_style_arc_opa(arc, LV_OPA_40, LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, largeur - 2 * kRetrait, LV_PART_INDICATOR);
    lv_obj_set_style_pad_all(arc, kRetrait, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(arc, verre, LV_PART_INDICATOR);
    lv_obj_set_style_arc_opa(arc, LV_OPA_30, LV_PART_INDICATOR);
    ui_hidden(arc, false);
}

// Un mot (lien, choix) centré sur (x, y), dans l'écran.
void legende(lv_obj_t* lbl, const char* txt, int32_t x, int32_t y) {
    if (lbl == nullptr) return;
    const bool montre = txt != nullptr && txt[0] != '\0';
    ui_hidden(lbl, !montre);
    if (!montre) return;
    texte_ha_coupe(lbl, txt, kLegendeL);
    ui_x(lbl, std::clamp(x - kLegendeL / 2, int32_t{0}, kEcranL - kLegendeL));
    ui_y(lbl, std::clamp(y - kLegendeH / 2, int32_t{0}, kEcranH - kLegendeH));
}

// Point d'une famille, dans son bouton (aligné au centre : x, y en sont les écarts), vers
// l'extérieur de la roue (angle `a`), entre l'icône et le bord.
void point(lv_obj_t* p, int a, bool deplie, uint32_t couleur) {
    if (p == nullptr) return;
    constexpr int32_t d = kDiametre / 2 - kPoint / 2 - 6;
    const int32_t dy = echelle(d, sin5(a));
    ui_x(p, echelle(d, cos5(a)));
    ui_y(p, s_roue.dessous ? dy : -dy);
    lv_obj_set_style_bg_color(p, lv_color_hex(deplie ? couleur : UIColor.TEXT_DIM), LV_PART_MAIN);
    ui_hidden(p, false);
}

// Premier anneau : aspect des boutons (famille dépliée comprise), icônes, points, mots des liens.
void peindre_premier_anneau() {
    RoueUI& u = g_roue_ui;
    const Roue& s = s_roue;
    int liens = 0;
    for (int i = 0; i < kRoueBoutons; i++) {
        const bool montre = i < s.n;
        ui_hidden(u.bouton[i], !montre);
        if (!montre) continue;
        const RoueBouton& b = s.bouton[i];
        ui_x(u.bouton[i], s.cx[i] - kDiametre / 2);
        ui_y(u.bouton[i], s.cy[i] - kDiametre / 2);
        Aspect a = Aspect::VERRE;
        uint32_t encre = UIColor.TEXT_PRIMARY;
        if (b.genre == RoueGenre::LIEN) {
            a = Aspect::LIEN;
            encre = UIColor.TEXT_SOFT;
        } else if (b.genre == RoueGenre::FAMILLE && s.famille == i) {
            a = Aspect::DEPLIE;
            encre = s.couleur;
        } else if (b.courant) {
            a = Aspect::COURANT;
        }
        aspect(u.bouton[i], s_pose_bouton[i], a, s.couleur);
        ui_text(u.icone[i], glyphe_roue(b.icone));
        ui_text_color(u.icone[i], encre);
        if (b.genre == RoueGenre::FAMILLE) point(u.point[i], s.angle[i], s.famille == i, s.couleur);
        else ui_hidden(u.point[i], true);
        if (b.genre == RoueGenre::LIEN && liens < 2)
            legende(u.lien_legende[liens++], b.legende, s.cx[i], s.cy[i] + kDiametre / 2 + kLegendeH / 2 + 2);
    }
    for (int k = liens; k < 2; k++) ui_hidden(u.lien_legende[k], true);
}

// Second anneau : les choix de la famille dépliée (rappel `famille`), centrés sur elle, et
// sa bande teintée de la couleur d'état ; tout masqué si aucune ne l'est.
void peindre_second_anneau() {
    RoueUI& u = g_roue_ui;
    Roue& s = s_roue;
    RoueChoix c[kRoueChoix];
    s.m = 0;
    if (s.famille >= 0 && s.rappels.famille != nullptr)
        s.m = std::clamp(s.rappels.famille(s.famille, c), 0, kRoueChoix);
    if (s.m == 0) s.famille = -1;
    int32_t cx[kRoueChoix], cy[kRoueChoix];
    int angle[kRoueChoix];
    if (s.m > 0)
        disposer(s.xa, s.ya, s.m, kRayon2, kPasAngle2, s.angle[s.famille], kDiametre2 / 2, s.dessous, cx, cy, angle);
    for (int j = 0; j < kRoueChoix; j++) {
        const bool montre = j < s.m;
        ui_hidden(u.choix[j], !montre);
        if (!montre) {
            ui_hidden(u.choix_legende[j], true);
            continue;
        }
        const RoueChoix& k = c[j];
        ui_x(u.choix[j], cx[j] - kDiametre2 / 2);
        ui_y(u.choix[j], cy[j] - kDiametre2 / 2);
        if (k.a_pastille) aspect_pastille(u.choix[j], s_pose_choix[j], k.pastille, k.courant);
        else aspect(u.choix[j], s_pose_choix[j], k.courant ? Aspect::COURANT : Aspect::VERRE, s.couleur);
        const bool icone = k.icone != RoueIcone::AUCUNE;
        ui_hidden(u.choix_icone[j], !icone);
        ui_hidden(u.choix_texte[j], icone || k.texte[0] == '\0');
        if (icone) {
            ui_text(u.choix_icone[j], glyphe_roue(k.icone));
            ui_text_color(u.choix_icone[j], UIColor.TEXT_PRIMARY);
        } else if (k.texte[0] != '\0') {
            ui_text(u.choix_texte[j], k.texte);
            ui_text_color(u.choix_texte[j], UIColor.TEXT_PRIMARY);
        }
        const int32_t dy = echelle(kRayon2 + kLegende2, sin5(angle[j]));
        legende(u.choix_legende[j], k.legende, s.xa + echelle(kRayon2 + kLegende2, cos5(angle[j])),
                s.dessous ? s.ya + dy : s.ya - dy);
    }
    if (s.m > 0) {
        int a_max = angle[0], a_min = angle[0];
        for (int j = 1; j < s.m; j++) {
            a_max = std::max(a_max, angle[j]);
            a_min = std::min(a_min, angle[j]);
        }
        bande(u.bande[1], kRayon2, kBande2, a_max, a_min, s.couleur, 90);
    } else {
        ui_hidden(u.bande[1], true);
    }
}

void peindre_anneaux() {
    peindre_second_anneau();  // il peut replier une famille vide : le premier le montre
    peindre_premier_anneau();
}

}  // namespace

// ─── API (tab5_custom.h, tab5_internal.h) ───────────────────────────────────────────

void roue_brancher() {
    static bool fait = false;
    RoueUI& u = g_roue_ui;
    if (fait || u.fond == nullptr) return;
    fait = true;
    // Un glissement sur la roue ouverte ne remonte pas jusqu'à la page (swipe des
    // prévisions ou des pièces sous elle) : un toucher hors des boutons replie ou ferme
    // la roue, un geste ne fait rien.
    lv_obj_remove_flag(u.fond, LV_OBJ_FLAG_GESTURE_BUBBLE);
    // Arcs de dessin seulement : ni bouton (knob), ni toucher (il va au voile), ni marge.
    for (lv_obj_t* arc : {u.bande[0], u.bande[1], u.jauge}) {
        if (arc == nullptr) continue;
        lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
        lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
        lv_obj_set_style_pad_all(arc, 0, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_MAIN);
        lv_obj_set_style_arc_rounded(arc, true, LV_PART_INDICATOR);
    }
    // Effet d'appui des boutons de verre (tab5_anim.cpp) : il retient les boutons de
    // rayon 18 et ceux marqués USER_1. Les boutons ronds de la roue sont marqués ici, à
    // la fin du setup, avant apply_pressed_scale_to_tree (2 s après le démarrage).
    for (lv_obj_t* b : u.bouton)
        if (b != nullptr) lv_obj_add_flag(b, LV_OBJ_FLAG_USER_1);
    for (lv_obj_t* b : u.choix)
        if (b != nullptr) lv_obj_add_flag(b, LV_OBJ_FLAG_USER_1);
    if (u.moyeu != nullptr) lv_obj_add_flag(u.moyeu, LV_OBJ_FLAG_USER_1);
}

bool roue_ouvrir(lv_obj_t* ancre, const RoueTete& tete, const RoueBouton* b, int n, const RoueRappels& r,
                 bool garder) {
    RoueUI& u = g_roue_ui;
    if (u.fond == nullptr || u.moyeu == nullptr || ancre == nullptr || b == nullptr || n < 1 || n > kRoueBoutons)
        return false;
    Roue& s = s_roue;
    // Centre de l'ancre, dans le repère du voile plein écran (posé en 0, 0).
    lv_obj_update_layout(ancre);
    lv_area_t a, f;
    lv_obj_get_coords(ancre, &a);
    lv_obj_get_coords(u.fond, &f);
    s.xa = a.x1 + lv_area_get_width(&a) / 2 - f.x1;
    s.ya = a.y1 + lv_area_get_height(&a) / 2 - f.y1;
    // Au-dessus de l'ancre ; en dessous seulement si le second anneau et ses mots n'y
    // tiennent pas (une ancre haute : une ligne du haut du popup Maison). Décidé pour les
    // deux anneaux à l'ouverture : déplier une famille ne fait pas sauter le premier.
    s.dessous = s.ya - (kRayon2 + kLegende2 + kLegendeH / 2) < 0;
    // Un repeint (thème, état poussé) garde la famille dépliée si elle est toujours là, au
    // même rang (la même icône : une famille par icône).
    const bool repeinte = garder && ouverte();
    int famille = -1;
    if (repeinte && s.famille >= 0 && s.famille < n && b[s.famille].genre == RoueGenre::FAMILLE &&
        b[s.famille].icone == s.bouton[s.famille].icone)
        famille = s.famille;
    s.n = n;
    for (int i = 0; i < n; i++) s.bouton[i] = b[i];
    s.famille = famille;
    s.couleur = tete.couleur;
    s.rappels = r;
    disposer(s.xa, s.ya, n, kRayon, kPasAngle, 90, kDiametre / 2, s.dessous, s.cx, s.cy, s.angle);

    // Moyeu sur l'ancre : l'icône et la ligne d'état de la carte, liseré de la couleur d'état.
    ui_x(u.moyeu, s.xa - kMoyeu / 2);
    ui_y(u.moyeu, s.ya - kMoyeu / 2);
    if (tete.icone != nullptr) ui_text(u.moyeu_icone, tete.icone);
    ui_text_color(u.moyeu_icone, tete.couleur);
    texte_ha_coupe(u.moyeu_valeur, tete.valeur, kMoyeu - 16);
    ui_text_color(u.moyeu_valeur, UIColor.TEXT_PRIMARY);
    lv_obj_set_style_border_color(u.moyeu, lv_color_hex(tete.couleur), LV_PART_MAIN);
    lv_obj_set_style_border_width(u.moyeu, 2, LV_PART_MAIN);
    lv_obj_set_style_border_opa(u.moyeu, LV_OPA_80, LV_PART_MAIN);
    // Jauge autour du moyeu (piste des arcs, valeur dans la couleur d'état).
    if (u.jauge != nullptr) {
        const bool jauge = tete.jauge >= 0;
        ui_hidden(u.jauge, !jauge);
        if (jauge) {
            ui_x(u.jauge, s.xa - kJauge / 2);
            ui_y(u.jauge, s.ya - kJauge / 2);
            lv_arc_set_value(u.jauge, std::clamp(tete.jauge, 0, 100));
            lv_obj_set_style_arc_color(u.jauge, lv_color_hex(UIColor.ARC_TRACK), LV_PART_MAIN);
            lv_obj_set_style_arc_color(u.jauge, lv_color_hex(tete.couleur), LV_PART_INDICATOR);
            lv_obj_set_style_arc_opa(u.jauge, tete.jauge > 0 ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_INDICATOR);
        }
    }
    // Nom sous la jauge (au-dessus si la roue est sous l'ancre), dans l'écran.
    if (u.nom != nullptr) {
        texte_ha_coupe(u.nom, tete.nom, kNomL);
        const int32_t y = s.dessous ? s.ya - kJauge / 2 - 4 - kLegendeH : s.ya + kJauge / 2 + 4;
        ui_x(u.nom, std::clamp(s.xa - kNomL / 2, int32_t{0}, kEcranL - kNomL));
        ui_y(u.nom, std::clamp(y, int32_t{0}, kEcranH - kLegendeH));
        ui_hidden(u.nom, tete.nom == nullptr || tete.nom[0] == '\0');
    }
    // Bande du premier anneau : verre neutre.
    int a_max = s.angle[0], a_min = s.angle[0];
    for (int i = 1; i < n; i++) {
        a_max = std::max(a_max, s.angle[i]);
        a_min = std::min(a_min, s.angle[i]);
    }
    bande(u.bande[0], kRayon, kBande, a_max, a_min, UIColor.GLASS_HI, 255);
    peindre_anneaux();
    // Au premier plan, comme un popup (animate_popup_open) : le voile est inclus avant les
    // popups dans tab5-lvgl.yaml, et une roue ouverte sur une ligne du popup Maison
    // (ADR-0037) doit le couvrir. Sans popup ouvert, l'ordre ne change rien à l'écran.
    // Pas à un repeint : la sonnerie du réveil (au-dessus de tout, et qui ne ferme pas la
    // roue) passerait dessous, son « Arrêter » sous le voile.
    if (!repeinte) lv_obj_move_to_index(u.fond, -1);
    ui_hidden(u.fond, false);
    return true;
}

void roue_actions_choisir(int n) {
    Roue& s = s_roue;
    if (!ouverte() || n < 0 || n >= s.n) return;
    // Une famille : son second anneau se déplie (un autre se replie), ou se replie.
    if (s.bouton[n].genre == RoueGenre::FAMILLE) {
        s.famille = s.famille == n ? -1 : n;
        peindre_anneaux();
        return;
    }
    void (*choisir)(int) = s.rappels.choisir;
    roue_actions_fermer();
    if (choisir != nullptr) choisir(n);
}

void roue_choix_toucher(int n) {
    const Roue& s = s_roue;
    if (!ouverte() || s.famille < 0 || n < 0 || n >= s.m) return;
    const int famille = s.famille;
    void (*choisir)(int, int) = s.rappels.choisir_choix;
    roue_actions_fermer();
    if (choisir != nullptr) choisir(famille, n);
}

void roue_replier_ou_fermer() {
    Roue& s = s_roue;
    if (!ouverte()) return;
    if (s.famille >= 0) {
        s.famille = -1;
        peindre_anneaux();
        return;
    }
    roue_actions_fermer();
}

void roue_actions_fermer() {
    s_roue.famille = -1;
    ui_hidden(g_roue_ui.fond, true);
}

bool roue_actions_ouverte() { return ouverte(); }

void roue_rejouer_theme() {
    if (ouverte() && s_roue.rappels.rejouer != nullptr) s_roue.rappels.rejouer();
}
