/**
 * [AI-CONTEXT]
 * @file tab5_internal.h
 * @role Helpers partagés ENTRE les unités C++ issues de la scission de tab5_custom.cpp
 *       (lot (e), 08/09/2026). Ils étaient `static` dans le fichier unique ; ils ne font
 *       pas partie du contrat avec les YAML (qui n'incluent que tab5_custom.h).
 * @ai_instruction N'ajouter ici qu'un helper appelé depuis au moins deux unités. Un helper
 *                 propre à une unité reste `static` dans son .cpp.
 */
#pragma once
#include "tab5_custom.h"
#include "tab5_champs.h"
#include "tab5_parse.h"
#include <cstring>
#include <string>

// --- Écritures conditionnelles (audit du 26/09/2026, lot 3) ---
// En LVGL 9.5.0, lv_label_set_text() libère et réalloue le texte puis invalide le label
// même à texte identique (set_text_internal, lv_label.c), et tout lv_obj_set_style_*()
// passe par lv_obj_set_local_style_prop(), qui ne compare pas et rafraîchit le style
// (lv_obj_style.c) : un repaint pour rien à chaque capteur, interval ou push inchangé.
// Ces helpers comparent d'abord (mêmes gardes nulles que les appels remplacés).
inline void ui_text(lv_obj_t* label, const char* txt) {
    if (label == nullptr || txt == nullptr) return;
    const char* cur = lv_label_get_text(label);
    if (cur != nullptr && strcmp(cur, txt) == 0) return;
    lv_label_set_text(label, txt);
}
// Propriété de style locale de la partie principale, état par défaut (le sélecteur
// LV_PART_MAIN des lv_obj_set_style_*(o, …, LV_PART_MAIN) qu'elles remplacent), écrite
// seulement si elle change (lot L10, 08/10/2026). ui_style_num : une propriété entière
// (opacité, largeur de bordure…) ; ui_style_couleur : une couleur.
inline void ui_style_num(lv_obj_t* obj, lv_style_prop_t prop, int32_t v) {
    if (obj == nullptr) return;
    lv_style_value_t cur;
    if (lv_obj_get_local_style_prop(obj, prop, &cur, LV_PART_MAIN) == LV_STYLE_RES_FOUND && cur.num == v) return;
    lv_style_value_t val;
    val.num = v;
    lv_obj_set_local_style_prop(obj, prop, val, LV_PART_MAIN);
}
inline void ui_style_couleur(lv_obj_t* obj, lv_style_prop_t prop, lv_color_t c) {
    if (obj == nullptr) return;
    lv_style_value_t cur;
    if (lv_obj_get_local_style_prop(obj, prop, &cur, LV_PART_MAIN) == LV_STYLE_RES_FOUND && lv_color_eq(cur.color, c))
        return;
    lv_style_value_t val;
    val.color = c;
    lv_obj_set_local_style_prop(obj, prop, val, LV_PART_MAIN);
}
inline void ui_style_couleur(lv_obj_t* obj, lv_style_prop_t prop, uint32_t hex) {
    ui_style_couleur(obj, prop, lv_color_hex(hex));
}
// Couleur de texte locale (partie principale, état par défaut, comme les
// lv_obj_set_style_text_color(o, …, LV_PART_MAIN) qu'il remplace).
inline void ui_text_color(lv_obj_t* obj, uint32_t hex) { ui_style_couleur(obj, LV_STYLE_TEXT_COLOR, hex); }
// Masquage et position (zones optionnelles, lot 5). LVGL 9.5.0 compare déjà lui-même
// (lv_obj_add_flag / lv_obj_remove_flag : retour immédiat si le drapeau est déjà dans
// l'état voulu, lv_obj.c ; lv_obj_set_x/y/width/height : retour si le style local a déjà
// la valeur, lv_obj_pos.c) : ces helpers n'évitent rien de plus, ils gardent la garde
// nulle et une écriture sur une ligne. Un lv_obj_add_flag(HIDDEN) brut n'est donc pas
// une écriture « non gardée » (vérifié dans la source de LVGL 9.5.0 le 08/10/2026).
inline void ui_hidden(lv_obj_t* obj, bool hidden) {
    if (obj == nullptr || lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN) == hidden) return;
    lv_obj_set_flag(obj, LV_OBJ_FLAG_HIDDEN, hidden);
}
inline void ui_x(lv_obj_t* obj, int32_t x) {
    if (obj != nullptr && lv_obj_get_x_aligned(obj) != x) lv_obj_set_x(obj, x);
}
inline void ui_y(lv_obj_t* obj, int32_t y) {
    if (obj != nullptr && lv_obj_get_y_aligned(obj) != y) lv_obj_set_y(obj, y);
}
// Police d'un texte : `f` en style local, ou nullptr pour rendre la main aux styles du
// label (style_police_date : la police de la date du thème, qui suit un changement de
// thème ; ADR-0029). Défini dans tab5_central.cpp.
void ui_police(lv_obj_t* obj, esphome::font::Font* f);
// Vrai si l'appui en cours (ou qui vient de finir) a glissé : le doigt a bougé, ou LVGL y
// a vu un geste (swipe des prévisions ou des pièces). Un tap ou un appui long au bout
// d'un glissement ne doit rien ouvrir. LVGL remet ces deux marques à zéro à chaque
// appui ; hors appui (« Aller à l'écran », un script), aucun périphérique n'est actif :
// faux. Une seule garde pour tous les appuis (alertes_ouvrir, titre de la pièce).
inline bool ui_appui_glisse() {
    lv_indev_t* indev = lv_indev_active();
    return indev != nullptr && (lv_indev_get_press_moved(indev) || lv_indev_get_gesture_dir(indev) != LV_DIR_NONE);
}

// --- Tracés construits en C++ (tab5_anim.cpp, 09/10/2026) ---
// Briques d'un tracé (popups Température et Météo) : un rectangle sans style (opacité,
// rayon), une lv_line (son tableau de points doit vivre aussi longtemps qu'elle), tous deux
// non cliquables et masqués ; ui_poser() les place, les taille et les montre.
lv_obj_t* ui_rectangle(lv_obj_t* parent, lv_opa_t opa, int32_t rayon);
lv_obj_t* ui_ligne(lv_obj_t* parent, int32_t epaisseur);
void ui_poser(lv_obj_t* o, int32_t x, int32_t y, int32_t w, int32_t h);
// Courbe lissée et monotone (Fritsch-Carlson) par les n points (xs, ys) : par_segment
// points entre deux points donnés, écrits dans `out`, qui doit en tenir
// (n - 1) * par_segment + 1 et vivre aussi longtemps que la lv_line qui les montre.
// Renvoie le nombre de points écrits ; 0 sous 2 points ou au-delà de kCourbeLissePoints.
// Popup Météo (ADR-0043) et zone à gauche de l'horloge (ADR-0051).
constexpr int kCourbeLissePoints = 32;
int ui_courbe_lisse(const float* xs, const float* ys, int n, int par_segment, lv_point_precise_t* out);

// --- tab5_text.cpp ---
// Normalise un texte venu de HA (Latin-1 / mojibake) en UTF-8 valide pour LVGL.
std::string normalize_text_utf8(const std::string& in);
// Bandeau de vigilance selon la couleur Météo-France (« Vert », « Orange »…).
const char* vigilance_alert_banner_utf8(const std::string& couleur);
// Libellés de jour relatifs à aujourd'hui (offset en jours) : « Mer 09 » / « mercredi 9 septembre ».
// format_short_day_label / format_long_day_label : tab5_core.h (logique pure).
// Vrai seulement si le texte contient un markup recolor LVGL #RRGGBB (évite les faux positifs sur un '#' isolé).
bool has_lvgl_recolor_markup(const char* t);
// Pose un texte sur un label en activant le recolor LVGL seulement s'il contient du #RRGGBB
// (écrit seulement s'il change, comme ui_text).
void set_label_text_utf8(lv_obj_t* label, const char* text);
// clock_month_short_utf8() : tab5_core.h.

// --- Sortis de tab5_custom.h le 25/09/2026 (audit, lot 8d) : appelés entre unités
// C++ mais jamais depuis un YAML — ils ne font pas partie du contrat.

// Rotateur central : fait glisser le contenu (premier enfant) des deux panneaux, pas
// les panneaux ; transition_couper() la coupe et remet le contenu en place.
void transition_widgets(lv_obj_t* out_wrap, lv_obj_t* in_wrap);
void transition_couper(lv_obj_t* wrap);

// Ferme un popup UNIQUEMENT s'il est réellement affiché et qu'aucun fondu n'est
// déjà en cours dessus. Renvoie true si une fermeture a été lancée.
// Le garde-fou sur l'animation évite un clignotement : animate_popup_close()
// repart de LV_OPA_COVER, la rejouer sur un popup à moitié effacé le
// rallumerait d'un coup avant de le refaire disparaître.
bool close_popup_if_open(lv_obj_t* card);

// Glissement horizontal + fondu croisé entre deux layers (swipe prévisions).
// dir = LV_DIR_LEFT (in arrive de la droite, out part à gauche) ou
//       LV_DIR_RIGHT (in arrive de la gauche, out part à droite).
// Durée UIAnim::SWIPE_DUR. Dérivée de transition_widgets() mais en horizontal.
void animate_swipe_horizontal(lv_obj_t* out_layer, lv_obj_t* in_layer, lv_dir_t dir);

// Slide-in depuis la droite + fondu pour un bandeau d'alerte qui entre
// dans le rotateur central (alertes HA, alertes Météo-France).
// Durée UIAnim::ALERT_DUR, ease_out.
void animate_alert_enter(lv_obj_t* alert_wrap);

// « Rouleau » d'icône météo : la nouvelle icône monte depuis le bas en
// apparaissant (translate_y relatif à l'offset de base posé par
// update_meteo_icon(), donc compatible avec les icônes composées l1+l2).
// delay_ms permet d'échelonner les 5 tuiles (effet vague).
void animate_icon_roll_in(lv_obj_t* l1, lv_obj_t* l2, uint32_t delay_ms);

uint32_t get_temperature_color(float t);

bool tab5_dismiss_local_has(const std::string& store, const std::string& id);

void tab5_dismiss_local_prune(std::string& store, const std::vector<std::string>& ids_seen);

void update_rain_phrase_ui(lv_obj_t* lbl, const std::string& phrase);

// --- Pièces et tuiles (tab5_tuiles.cpp, ADR-0023) ---
// emplacements_appliquer (tab5_zones.cpp) : une entrée « tRT|état|valeur|couleur » (clé
// dans cle[0..n_cle), le reste après le premier '|'). Faux si la clé n'est pas celle d'une
// tuile : l'entrée suit alors la table des emplacements 3.x.
bool tuiles_etat_recu(const char* cle, size_t n_cle, const char* reste, size_t n_reste);
// zones_apply_ui (tab5_zones.cpp) : tout redessiner (définitions chargées de la NVS au
// premier appel, zones du mode héritage, bouton « HA », cartes, titre de la pièce).
void tuiles_appliquer_ui();
// apply_forecast_page (tab5_central.cpp) : épaules et boutons des tuiles de la page
// courante, en mode météo (la pièce de la page, ou rien).
void tuiles_peindre_meteo();
// tab5_central.cpp : titre de la carte centrale en mode HA — « Pièce n/N » et le nom de
// la pièce de la page courante (« Pièce n » sans nom).
bool tuiles_titre_piece(std::string& chapeau, std::string& titre);
// tab5_central.cpp (handle_swipe_gesture en mode HA) : pièce suivante ou précédente
// qui a des appareils, dans l'ordre des pages météo ; une seule pièce : rien.
void tuiles_swipe_ha(bool gauche);
// Texte venu de HA, mêmes règles que les noms des tuiles (tab5_clim.cpp : titre du popup
// clim, ADR-0026 ; libellés des alertes, bandeaux et historique). texte_ha_copier : UTF-8
// valide, sans les caractères que les polices n'ont pas, coupé sur une frontière de
// caractère (`cap` octets, zéro final compris).
// texte_ha_coupe : une ligne, coupée avec « … » au-delà de `largeur` px (60 octets au plus).
void texte_ha_copier(char* dst, size_t cap, const char* src, size_t n);
void texte_ha_coupe(lv_obj_t* lbl, const char* txt, int32_t largeur);

// --- Payload refusé (audit du 07/10/2026, lot L5) ---
// Tout récepteur de service qui refuse un payload, ou une partie, le dit : une ligne
// ESP_LOGW sous le tag de son module (« tab5.energie »…), la raison et la taille, jamais
// le contenu (long, et il peut nommer la maison). tab5_text.cpp.
void payload_refuse(const char* tag, const char* raison, size_t taille);
// Vrai, et journalisé, si `taille` dépasse `max` (par défaut kPayloadMax, tab5_champs.h) :
// le récepteur s'arrête là, l'écran garde l'état d'avant.
bool payload_trop_long(const char* tag, size_t taille, size_t max = kPayloadMax);

// --- Rangée sous l'horloge (ADR-0031) : modèle dans tab5_tuiles.cpp, dessin dans
// tab5_rangee.cpp ---
// Un élément tel qu'il s'affiche : icône de la palette et sa couleur ; un capteur ou une
// clim (mesure) y ajoute sa valeur, déjà écrite (« 21.4 ° », « 2.4 kW », « -- »).
struct RangeeElement {
    bool mesure = false;
    const char* icone = nullptr;
    uint32_t couleur = 0;
    char texte[40] = "";                // taille de la ligne d'une tuile (Vue::ligne) : un état
                                        // texte n'est pas coupé au milieu d'un caractère
    uint32_t couleur_texte = 0;
};
// Zones à lignes (lot 3, 09/10/2026) : le même modèle et le même dessin pour la rangée
// sous l'horloge (clés h…, ligne spéciale = les plantes) et le panneau « Ok Nabu » (clés
// n…, ligne spéciale = l'écoute). L'ordre est l'index du modèle (tab5_tuiles.cpp) et de
// son dessin (tab5_rangee.cpp).
enum RangeeZone : uint8_t {
    RANGEE_HORLOGE,
    RANGEE_NABU,
    RANGEE_NB,
};
// Un tour de la carte centrale, en secondes : la période du rotateur
// (tab5_central_rotator_auto, tab5-scripts.yaml : 7,8 s, rangee_tour(), puis 0,2 s et la
// carte centrale ; tests/test_rangee.py compare). Les durées du blueprint (« hd|32 »,
// « nd|32 », le 4e champ de « defil ») sont arrondies au tour le plus proche.
constexpr int kTourCentralS = 8;
// tab5_tuiles.cpp, zone z. Place de la ligne spéciale (0 à 2, -1 masquée) ; tours de la
// carte centrale par ligne (1 à 15) ; ligne de capteurs l (0 à 2) définie ; élément i
// (0 à 3) de la ligne l (faux s'il est vide).
int rangee_place_speciale(int z);
int rangee_tours(int z);
bool rangee_ligne_remplie(int z, int l);
bool rangee_element(int z, int l, int i, RangeeElement& out);
// tab5_rangee.cpp. zones_apply_ui : tout redessiner (lignes, pots présents) ; les
// définitions de la zone z ont changé (tuiles_definir) ; l'état de l'élément i de la
// ligne l de la zone z est arrivé (tuiles_etat_recu).
void rangee_appliquer_ui();
void rangee_definitions_changees(int z);
void rangee_element_change(int z, int l, int i);

// Défilement au choix (lot 3, 09/10/2026) : « auto » (les lignes ou les appareils se
// relaient seuls, calés sur la carte centrale) ou « fixe » (seulement par un geste), une
// zone à la fois. Clé « defil|rangée|nabu|clim|secondes » de tab5_maj_emplacements
// (blueprint, avec tous les états), gardée en NVS ; tab5_rangee.cpp. Sans la clé : la
// rangée en auto, le panneau Ok Nabu et la tuile − / + fixes (l'écran d'avant).
enum class Defilement : uint8_t {
    RANGEE,
    NABU,
    CLIM,
    NB,
};
bool defilement_auto(Defilement d);
// Tours de la carte centrale par appareil de la tuile − / + en auto (1 à 15).
int defilement_tours_clim();
// « defil|… » reçue (emplacements_appliquer) ; defilement_defaut() : un payload de gestes
// sans elle (blueprint d'avant le lot 3) remet les défauts.
void defilement_recu(const char* valeur, size_t n);
void defilement_defaut();
// tab5_reglables.cpp : un tour de la carte centrale pour la tuile − / + (rangee_tour).
void reglables_tour();

// --- Tuile − / + au choix (tab5_reglables.cpp, ADR-0033) ---
// tuiles_definir (tab5_tuiles.cpp) : les entrées « rN|type|icône|options|lien|min|max|pas|
// unité|nom » du même instantané (sans clé r : aucun appareil du blueprint). Vrai si
// elles ont changé.
bool reglables_definir(const std::string& payload);
// emplacements_appliquer (tab5_zones.cpp) : « rN|état|valeur ». Faux si la clé n'est pas
// la leur : l'entrée suit alors la table des emplacements 3.x.
bool reglables_etat_recu(const char* cle, size_t n_cle, const char* reste, size_t n_reste);
// zones_apply_ui : la tuile − / + (masquée sans clim ni appareil) et ce qu'elle montre.
void reglables_appliquer_ui();
// tab5_clim.cpp, la clim du blueprint a changé (consigne, mode) : sa ligne de la liste.
void reglables_clim_changee();
// theme_rejouer_ui (tab5_theme.cpp).
void reglables_rejouer_theme();
// tab5_tuiles.cpp : ouvre le popup de la tuile tRT (lumière, volet, télécommande, clim :
// ce que fait son appui long, ou son appui pour une clim). Faux si elle n'en a pas.
bool tuile_ouvrir_popup(int r, int t);
// tab5_clim.cpp, pour la liste de la tuile − / + : nom de la clim du blueprint (vide
// tant que HA ne l'a pas donné) ; sa consigne écrite comme sur la carte (« 21.5 »,
// « -- ») et sa couleur (bleu en froid, rouge en chaud), et la couleur de son icône.
const char* clim_nom();
uint32_t clim_carte_valeur(char* buf, size_t n, uint32_t& couleur_valeur);

// --- Roue d'actions rapides (tab5_roue.cpp, ADR-0036) ---
// Icône d'un bouton (glyphe_roue, mdi_font_36) ; AUCUNE : un texte ou une pastille. Après
// REGLAGES : celles de la roue de navigation (ADR-0042), familles puis destinations ;
// METEO (popup Météo, ADR-0043) puis CAMERAS (popup Caméras, ADR-0049) en dernier.
enum class RoueIcone : uint8_t {
    AUCUNE,
    ETEINDRE,
    ALLUMER,
    LUMINOSITE,
    BLANCS,
    COULEURS,
    OUVRIR,
    STOP,
    FERMER,
    POSITION,
    MODE,
    CHAUFFER,
    REFROIDIR,
    SECHER,
    VENTILER,
    CONSIGNE,
    OPTIONS,
    ECO,
    BOOST,
    SILENCE,
    OSCILLATION,
    BRISE,
    MAISON,
    REGLAGES,
    ALERTES,
    PIECES,
    APPAREILS,
    AGENDA,
    TABLETTE,
    ASSISTANT,
    LUMIERES,
    VOLET,
    CLIMS,
    TEMPERATURE,
    ENERGIE,
    PLANTES,
    CALENDRIER,
    REVEIL,
    JEUX,
    ENGRENAGE,
    SYSTEME,
    METEO,
    CAMERAS,
};
// Bouton du premier anneau : une commande, une famille (son toucher déplie le second
// anneau au-dessus de lui) ou un lien (« Maison », « Détails » : une fenêtre).
enum class RoueGenre : uint8_t { ACTION, FAMILLE, LIEN };
struct RoueBouton {
    RoueIcone icone = RoueIcone::AUCUNE;
    RoueGenre genre = RoueGenre::ACTION;
    bool courant = false;           // action : l'état de l'appareil (verre teinté, halo)
    // Son mot, déjà traduit : sous un lien ; avec RoueTete::mots, au-delà de chaque bouton
    // qui en a un (roue de navigation, ADR-0042).
    const char* legende = nullptr;
};
// Bouton du second anneau : une icône, un texte (« 50 % », « 21.5° ») ou une pastille de
// couleur (celle qu'une lampe prendra), avec un mot dessous s'il le faut (« Chaud »).
struct RoueChoix {
    RoueIcone icone = RoueIcone::AUCUNE;
    char texte[12] = "";
    bool a_pastille = false;
    uint32_t pastille = 0;
    const char* legende = nullptr;  // déjà traduit ; nullptr : aucun
    bool courant = false;
};
// Moyeu, posé sur l'ancre : l'icône et la ligne d'état de la carte de l'appareil, son nom
// dessous, une jauge en arc (luminosité, position, consigne) dans la couleur d'état.
struct RoueTete {
    const char* icone = nullptr;    // glyphe de la palette des tuiles (mdi_font_45)
    const char* valeur = "";
    const char* nom = "";
    int jauge = -1;                 // 0 à 100 ; -1 : pas de jauge
    uint32_t couleur = 0;           // couleur d'état de l'appareil
    // Roue de navigation (ADR-0042) : chaque bouton du premier anneau porte son mot au-delà
    // de lui (pas seulement les liens, dessous) ; une famille dépliée les cache et écrit
    // le sien dans le moyeu, à la place de `valeur`.
    bool mots = false;
};
// Ce que fait la roue au toucher, fournie par celui qui l'ouvre (tab5_tuiles_roue.cpp,
// tab5_roue_navigation.cpp).
struct RoueRappels {
    void (*choisir)(int i) = nullptr;                   // action ou lien i, roue fermée
    int (*famille)(int i, RoueChoix* out) = nullptr;    // choix de la famille i (≤ kRoueChoix)
    void (*choisir_choix)(int i, int j) = nullptr;      // choix j de la famille i, roue fermée
    void (*rejouer)() = nullptr;                        // repeindre (thème, état) : rouvre ou ferme
};
// Ouvre la roue autour du centre de `ancre` (n boutons de gauche à droite, n ≤
// kRoueBoutons ; la roue de navigation, ADR-0042 : la carte centrale). `garder` : un
// repeint de la même roue (thème, état poussé), la famille dépliée le reste si son bouton
// est toujours la même famille. Faux, et rien d'ouvert, sans widgets.
bool roue_ouvrir(lv_obj_t* ancre, const RoueTete& tete, const RoueBouton* b, int n, const RoueRappels& r,
                 bool garder);
// theme_rejouer_ui (tab5_theme.cpp) : roue ouverte repeinte dans la nouvelle palette.
void roue_rejouer_theme();
// tab5_tuiles_roue.cpp : la roue de la tuile tRT, autour de `ancre` (pastille d'une carte du
// mode HA, bouton d'une tuile météo, ou tout autre widget : une ligne d'une liste ;
// `depuis_maison` : sans le lien « Maison »). Faux, et rien d'ouvert, quand la tuile n'en
// a pas (type sans roue, option k ou r, clim sans capacité connue) : l'appelant ouvre
// alors le popup (tuile_ouvrir_popup).
bool tuile_roue_ouvrir(int r, int t, lv_obj_t* ancre, bool depuis_maison = false);
// tab5_tuiles_roue.cpp (ADR-0048) : la roue de la clim `c` (celle du blueprint, d'une
// tuile ou d'une pièce, ClimRef plus bas), ouverte par le toucher de la température de la
// pièce (clim_temperature_ouvrir, tab5_clim.cpp) autour de `ancre` : les boutons clim de
// la roue d'une tuile cli (même code), « Clims ▸ » à la place de « Maison » quand la
// tablette en connaît plusieurs, « Détails » = le carrousel sur elle. Faux, et rien
// d'ouvert, sans réglages reçus pour elle (l'appelant ouvre alors le carrousel).
struct ClimRef;
bool clim_roue_ouvrir(const ClimRef& c, lv_obj_t* ancre);
// tab5_clim.cpp, quand HA pousse les réglages ou l'état d'une clim (ou qu'elle est
// oubliée) : la roue ouverte sur une clim (tuile cli ou température) se repeint, ou se
// ferme si elle n'en a plus. Ne coûte qu'un test roue fermée.
void roue_clim_changee();
// tab5_clim.cpp, pour la roue. Les clims y sont désignées par (r, t) comme dans ClimRef :
// r < 0 la clim du blueprint, t < 0 la clim de la pièce r (ADR-0040), sinon celle de la
// tuile tRT. Lettres de capacité (ADR-0026), seulement si HA les a poussées (climr, crRT,
// crpR) ; nullptr sinon.
const char* clim_capacites_connues(int r, int t);
// Mode poussé par HA pour cette clim (« cool », « off »…) ; « » inconnu.
const char* clim_mode_connu(int r, int t);
// Moyeu de la roue de cette clim et sa ligne dans « Clims ▸ » : nom (celui de HA, sinon la
// pièce, sinon « Climatisation »), ligne d'état comme une tuile cli (température de la
// pièce, « -- », « Éteint », « Hors ligne »), consigne écrite (« 21.5° », « -- »), couleur
// d'état (celle de l'icône de la tuile − / +), allumée ou non.
struct ClimTete {
    char nom[49] = "";
    char ligne[24] = "";
    char consigne[10] = "";
    uint32_t couleur = 0;
    bool actif = false;
};
void clim_tete(int r, int t, ClimTete& out);
// Consignes que la roue propose à cette clim : la sienne et deux pas de chaque côté, dans
// ses bornes, croissantes, sans doublon (au plus 5) ; `courant` : le rang de la sienne.
// 0 si sa consigne ou ses réglages sont inconnus. `textes` : comme sur la carte, avec le
// degré (« 21.5° »).
int clim_roue_consignes(int r, int t, float valeurs[5], char textes[5][10], int& courant);
// Bascules du popup que cette clim a (lettres e, b, q, s, w, dans cet ordre) : son état,
// et la commande et la valeur que le popup enverrait à leur toucher (preset away / boost,
// ventilation quiet, oscillation swing / windnice, ou leur retour à none, auto, stop).
struct ClimBascule {
    char lettre;
    bool actif;
    const char* commande;
    const char* valeur;
};
int clim_roue_bascules(int r, int t, ClimBascule out[5]);
// Consigne de cette clim en 0-100 entre ses bornes (jauge du moyeu) ; -1 si inconnue.
int clim_roue_jauge(int r, int t);
// --- Popup Maison (ADR-0037) : dessin dans tab5_maison.cpp, modèle dans tab5_tuiles.cpp ---
// Widgets d'une tuile dessinée façon carte « tile » de HA : pastille ronde (fond = couleur
// de l'état, opacité posée par le YAML), son icône, le nom, la ligne d'état.
struct TuileWidgets {
    lv_obj_t* pastille;
    lv_obj_t* icone;
    lv_obj_t* nom;
    lv_obj_t* etat;
};
// tab5_tuiles.cpp. Titre de la pièce R (son nom, « Pièce n » sans nom) ; faux si elle n'a
// aucun appareil. A-t-elle une lumière pilotable (tuile lum sans l'option r) ?
bool tuiles_piece_titre(int r, char* out, size_t n);
bool tuiles_piece_a_lumieres(int r);
// « Pièce : tout éteindre » de la pièce R (pR / eteindre ; lumieres / eteindre en mode
// héritage), comme « Tout éteindre » du popup lumière.
void tuiles_piece_eteindre(int r);
// Faux si la tuile tRT est vide. `agit` : un toucher fait quelque chose ; `appui_long` :
// elle a un appui long (bouton « ⋯ » de sa ligne).
bool tuile_gestes(int r, int t, bool& agit, bool& appui_long);
// La tuile tRT sur les widgets `w` (mots et couleurs de sa carte du mode HA), textes coupés
// à `largeur` px. Faux si elle est vide.
bool tuile_peindre_ligne(int r, int t, const TuileWidgets& w, int32_t largeur);
// Geste d'une ligne : celui de la tuile (toucher, ou appui long : la roue d'actions rapides
// autour de `ancre`, la pastille de la ligne, sinon le popup de la tuile).
void tuile_appui_maison(int r, int t, bool long_appui, lv_obj_t* ancre);
// tab5_maison.cpp, appelées par tab5_tuiles.cpp : l'état ou une minuterie de la tuile tRT a
// changé (sa ligne, si le popup est affiché) ; les définitions, les zones ou le thème ont
// changé (tout, s'il est affiché). Ne coûtent qu'un test quand il est fermé.
void maison_tuile_changee(int r, int t);
void maison_definitions_changees();

// --- Popup à pages (tab5_pages.cpp, ADR-0046) ---
// La brique commune des popups à plusieurs pages (Réglages ; Lumières et Volets, une page
// par pièce ; Réveil) : glisser à gauche / à droite = page suivante / précédente, en boucle,
// arrêté au popup, sauf parti d'un curseur, d'un arc ou d'un rouleau ; noms des pages en haut. Le popup garde sa
// page : `nombre` (pages aujourd'hui), `courante` (celle affichée, -1 aucune), `afficher`
// (montre la page i, repeint ses onglets). L'objet doit vivre aussi longtemps que le popup
// (une variable de fichier) : son adresse est la donnée du rappel LVGL.
struct PagesPopup {
    lv_obj_t* popup = nullptr;
    int (*nombre)() = nullptr;
    int (*courante)() = nullptr;
    void (*afficher)(int page) = nullptr;
};
// Branche le geste (une fois, même rappelé : l'ancien rappel est retiré d'abord).
void pages_brancher(PagesPopup* p);
// Onglets d'en-tête (pages_onglet.yaml) : les `n` premiers montrés quand il y a au moins
// deux pages (aucun sinon), collés à la croix, `noms[i]` coupé avec « … », celui de
// `courante` en accent ; les autres masqués. `max` : onglets posés par le YAML.
void pages_onglets(lv_obj_t* const* onglets, int max, const char* const* noms, int n, int courante);
// Largeur d'un onglet pour `n` pages (200 px au plus, 158 à cinq).
int32_t pages_onglet_largeur(int n);
// Une rangée de boutons à choix : celui d'index `actif` en accent (bordure et texte, premier
// enfant), les autres au style du bouton. Onglets des pages, options des Réglages.
void choix_peindre(lv_obj_t* const* boutons, int n, int actif);

// --- Alertes (tab5_central.cpp) ---
// Libellé codé d'une alerte, composé dans la langue de l'écran : « @maj:<titre> » →
// « 1 MAJ · <titre> », « @indispo:<n> » → « <n> indispo », « @vigi:<niveau> » →
// « Vigilance Rouge »… Tout autre libellé (nom d'une entité) s'affiche tel quel. Bandeaux
// de la carte centrale et historique du popup « Alertes » (tab5_alertes.cpp).
std::string ha_alerte_texte(const char* brut);

// --- Énergie (tab5_energie.cpp, ADR-0028) ---
// Puissance ou énergie en unités courtes : W / kW / MW → « 850 W », « 3.45 kW », « 12.5 kW » ;
// Wh / kWh / MWh → « 4.20 kWh », « 312 kWh », « 3.85 MWh ». Faux (rien d'écrit) pour
// une autre unité. Aussi la valeur d'une tuile cap à l'option e (tab5_tuiles.cpp).
bool energie_formater(char* out, size_t n, float v, const char* unite);

// --- Clim (tab5_clim.cpp, ADR-0026, ADR-0027) ---
// emplacements_appliquer (tab5_zones.cpp) : entrée « climr|min|max|pas|unité|capacités|nom »
// (`reste` = ce qui suit « climr| »). Range les réglages et les applique aux widgets de
// g_clim_ui.
void clim_reglages_recu(const char* reste, size_t n);
// emplacements_appliquer : entrées des clims de tuile, « crRT|min|max|pas|unité|capacités|
// nom » (réglages, comme climr) et « ceRT|consigne|pièce|mode|préréglage|ventilation|
// oscillation » (état). Faux si la clé n'est pas l'une d'elles : l'entrée suit alors la
// table des emplacements 3.x.
bool clim_tuile_recu(const char* cle, size_t n_cle, const char* reste, size_t n_reste);
// tab5_tuiles.cpp : la tablette a-t-elle les réglages de la clim de la tuile tRT ? (son
// appui ouvre alors le popup sur elle).
bool clim_tuile_connue(int r, int t);
// tab5_tuiles.cpp, appui d'une tuile cli sans l'option m : le popup montre cette clim.
// Faux, et rien ne change, sans ses réglages.
bool clim_afficher_tuile(int r, int t);
// tab5_tuiles.cpp (tuiles_definir) : la tuile a changé de définition, ses réglages et son
// état sont oubliés (le blueprint les renvoie juste après) ; si le popup la montrait, il
// se ferme et revient à la clim du blueprint.
void clim_tuile_oublier(int r, int t);
// tab5_clim.cpp, réglages d'une clim de tuile reçus : repeindre la tuile (son bouton
// apparaît, ADR-0027).
void tuiles_repeindre(int r, int t);
// Clim de la pièce R (ADR-0040, clés crpR / cepR, emplacement des commandes « cpR »).
// La tablette a-t-elle ses réglages ? (tab5_piece_climat.cpp : la tuile − / + la règle).
bool clim_piece_connue(int r);
// tab5_reglables.cpp, toucher de la consigne de la tuile − / + : le popup montre cette
// clim. Faux, et rien ne change, sans ses réglages.
bool clim_afficher_piece(int r);
// tab5_reglables.cpp, − / + : un pas de plus (sens > 0) ou de moins, borné ; affichage
// tout de suite, envoi par tab5_debounce_clim_tuile. Consigne inconnue : rien.
void clim_piece_pas(int r, int sens);
// tab5_piece_climat.cpp : la pièce n'a plus de clim — réglages et état oubliés, popup
// refermé s'il la montrait, consigne en attente annulée.
void clim_piece_oublier(int r);
// tab5_reglables.cpp, la tuile − / + : consigne écrite comme sur la carte (« 21.5 »,
// « -- ») et sa couleur ; renvoie la couleur de l'icône (comme clim_carte_valeur).
uint32_t clim_piece_carte(int r, char* buf, size_t n, uint32_t& couleur_valeur);
// Les clims que la tablette connaît, dans l'ordre des pages du carrousel du popup clim
// (ADR-0038) : celle du blueprint d'abord (sauf si HA l'a déclarée absente), puis celle
// de chaque tuile cli sans l'option m dont la tablette a les réglages (crRT), pièce par
// pièce, tuile par tuile, puis la clim propre de chaque pièce (ADR-0040, réglages crpR
// reçus) ; une clim du même nom qu'une clim déjà listée (la clim du blueprint posée aussi
// sur une tuile sans m) n'est pas listée deux fois. LA seule liste : une clim de plus =
// une ligne dans clims_enumerer() et un cas dans clim_ref_afficher() (tab5_clim.cpp).
struct ClimRef {
    int8_t r = -1;      // tuile tRT ; t = -1 : la clim de la pièce r ; r = t = -1 : celle du blueprint
    int8_t t = -1;
    int8_t piece = -1;  // pièce où elle est (ouverture sur la pièce affichée) ; -1 : aucune
};
// Écrit au plus `max` clims dans `out`, dans l'ordre ci-dessus ; renvoie leur nombre.
int clims_enumerer(ClimRef* out, int max);
// La clim que vise le toucher de la température de la pièce : celle de la pièce affichée
// en mode HA (sa clim propre, sinon la première de ses tuiles), sinon la première de
// clims_enumerer (celle du blueprint). Faux si la tablette n'en connaît aucune.
bool clim_ref_choisir(ClimRef& out);
// Le carrousel (popup clim, ADR-0038) ouvert sur la clim `c` : « Détails » de sa roue, ou
// le toucher de la température quand elle n'a pas de roue. Faux si le popup ne peut pas
// la montrer (réglages d'une clim de tuile ou de pièce pas reçus).
bool clim_carrousel_ouvrir_sur(const ClimRef& c);
// tab5_tuiles.cpp : la pièce affichée en mode HA (ADR-0023), -1 en mode météo ou en mode
// héritage (aucune pièce reçue).
int tuiles_piece_mode_ha();

// --- Popup Météo (tab5_meteo.cpp, ADR-0043) ---
// Les prévisions (heures, jours), la pluie dans l'heure ou sa phrase viennent de changer :
// le popup se repeint s'il est affiché (un test sinon). tab5_forecast.cpp,
// tab5_services.cpp, tab5_central.cpp.
void meteo_donnees_changees();
// tab5_services.cpp : niveau (0 sec à 4 très forte) de la barre i (0 à 8) de la pluie dans
// l'heure ; -1 tant que HA n'a rien poussé.
int pluie_barre_niveau(int i);
// tab5_central.cpp : la phrase de la pluie telle que la carte centrale l'affiche
// (« Pluie faible dans 12 mn », « Temps sec ») et son niveau (-1 pas de données, 0 sec,
// 1 à 4, 5 intensité inconnue ; -2 sans source ou phrase en texte brut). "" sans phrase.
const char* pluie_phrase_lue(int& niveau);
// tab5_meteo.cpp, partagés avec la zone à gauche de l'horloge (ADR-0051) : créneaux
// horaires lisibles depuis le premier (cal_heures_data : heure « HH:MM » et condition
// connue), et couleur d'une pluie horaire en mm (rôles RAIN_* de la palette).
int previsions_heures_lisibles();
uint32_t couleur_pluie_mm(float mm);

// --- Zone à gauche de l'horloge (tab5_zone_gauche.cpp, ADR-0051) ---
// tab5_zones.cpp : la clé « gauche|défaut|c1|… » de tab5_maj_emplacements (lue par
// zone_gauche_lire, tab5_parse.h). Gardée en NVS ; la zone change si le contenu de départ
// change ou si celui affiché n'est plus proposé.
void zone_gauche_recu(const char* valeur, size_t n);
// tab5_forecast.cpp : les prévisions horaires viennent de changer ; le graphique est à
// repeindre (tout de suite s'il est affiché, sinon à sa prochaine apparition).
void zone_gauche_donnees_changees();
// tab5_lecteur.cpp, pour la zone à gauche de l'horloge (lecteur compact, ADR-0051 lot 2) :
// montre ou masque son contenu (repeint s'il a changé caché ; la mini-barre « en lecture »
// se masque tant qu'il est montré, revient ensuite), et dit s'il a quelque chose à montrer
// (faux seulement quand HA a dit qu'aucun lecteur n'est choisi).
void lecteur_zone_montrer(bool montre);
bool lecteur_zone_disponible();

// --- tab5_central.cpp, pour les pièces ---
// Page atteinte par un swipe depuis `page` (bouclage volontaire, [AI-WARNING] de
// handle_swipe_gesture) : gauche 0→1→2→3→4→2, droite 4→3→2→1→0→2.
int forecast_page_suivante(int page, bool gauche);
// Pastilles de pagination : la page courante large et opaque, les autres étroites et
// pâles ; n pastilles (5 pour les pages, 3 pour la rangée sous l'horloge).
void pagination_afficher(lv_obj_t* const* pbars, int n, int page);
inline void pagination_afficher(lv_obj_t* const pbars[5], int page) { pagination_afficher(pbars, 5, page); }
// Carte centrale au changement de mode HA (ctx.ha_mode déjà posé) : fin du planning
// temporaire et de la réponse vocale, puis titre de la pièce ou panneaux habituels.
void central_mode_ha(lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, CentralPanelCtx& ctx);

// --- Thèmes (ADR-0029, lot 2), appelées par theme_rejouer_ui() de tab5_theme.cpp ---
// Repeintures par module, appelées par theme_rejouer_ui() (aucune ne crée de widget ni
// n'envoie rien à Home Assistant ; chacune ne repeint que ce qu'elle a déjà peint).
void central_rejouer_theme();
void vigilance_rejouer();
void rain_bars_rejouer();
void rain_predict_rejouer();
void tuiles_rejouer_theme();
void rangee_rejouer_theme();
// Mesures des capteurs (températures, pots, carte PC), clim et plantes de l'accueil.
void cartes_rejouer_theme();
void energie_rejouer_theme();
void reglages_rejouer_theme();
// Popup du réveil (alarm_render.cpp) : noms des pages et réglages, depuis le dernier état.
void reveil_rejouer_theme();
// Batterie de la tablette (tab5_zones.cpp), lue par la page Batterie des Réglages
// (tab5_reglages.cpp, 08/10/2026) : dernier niveau publié (%, NAN inconnu) et dernier
// état de CHG_STAT, ceux de l'icône du bandeau.
float batterie_niveau_lu();
bool batterie_en_charge_lue();
void historique_rejouer_theme();
void meteo_rejouer_theme();
void zone_gauche_rejouer_theme();
void alertes_rejouer_theme();
void zones_rejouer_theme();
void assist_rejouer_theme();
void cal_detail_rejouer();
// Bandeau planning (tab5_services.cpp) : ses couleurs sont dans son texte, recalculé.
void planning_rejouer_theme();
// Planning du tap affiché (tab5_central.cpp) : remplace les lignes rendues à la fin
// des 6 s ; sans effet hors du planning du tap.
void planning_temporaire_lignes(const std::string& l1, const std::string& l2);
