/**
 * [AI-CONTEXT]
 * @file tab5_reglages.cpp
 * @role Popup « Réglages » (06/10/2026, demande d'Axel), en quatre pages depuis le
 *       08/10/2026 : « Écran » (luminosité, extinction auto, rallumage par « Okay Nabu »
 *       et par une tape), « Apparence » (thème, clair ou sombre, nuit du mode Auto,
 *       langue), « Batterie » (limite de charge, économie d'énergie, batterie montée ; état,
 *       niveau, tension et consommation lus) et « Système » (l'ancienne console système,
 *       console_sys.yaml, remplie par le YAML). Les noms des pages sont en haut, à côté du
 *       titre, celle affichée en couleur d'accent.
 *       Ouvert par un tap sur le bouton central du haut (engrenage, page Écran), son appui
 *       long (page Système) ou par « Aller à l'écran → Réglages / Console système ». Ce
 *       fichier ne fait que peindre et changer de page : les gestes écrivent les entités
 *       (script tab5_reglages_choisir, tab5-reglages.yaml), et chaque entité relance
 *       tab5_reglages_sync_ui quand elle change, qui appelle reglages_peindre(). Le popup
 *       montre donc toujours ce que HA voit.
 * @architecture_constraint Rien gardé ici, sauf la page affichée, la langue en attente de
 *       confirmation et une copie du dernier état peint (reglages_rejouer_theme, appelée par
 *       theme_rejouer_ui) : la vérité est dans les entités (restore_value / restore_mode).
 *       Changer de langue redémarre la tablette (on_value du select « Langue ») : une
 *       pastille de langue ouvre d'abord une confirmation, jamais le redémarrage direct.
 *       Changement de page INSTANTANÉ (préférence d'Axel, AGENTS.md) : les conteneurs des
 *       pages sont masqués / montrés, sans glissement ni fondu.
 * @ai_instruction Un réglage de plus : sa valeur dans ReglageId (tab5_custom.h, à la fin),
 *       son cas dans tab5_reglages_choisir, son champ dans ReglagesEtat et ReglagesUI, sa
 *       ligne dans tab5_reglages_sync_ui et tab5_reglages_ouvrir, et un
 *       `script.execute: tab5_reglages_sync_ui` dans le on_value / on_state de l'entité.
 *       Un texte affiché passe par tr(). Une page de plus : sa valeur dans ReglagesPage
 *       (avant REGLAGES_NB_PAGES), son conteneur et son nom (reglages_onglet.yaml) dans
 *       reglages_popup.yaml, leurs lignes dans tab5_reglages_ouvrir.
 * @ai_warning [AI-WARNING] Le geste de changement de page s'arrête au popup : le drapeau
 *       LV_OBJ_FLAG_GESTURE_BUBBLE est retiré de reglages_popup (reglages_preparer). Sans
 *       ça, LVGL le remonte jusqu'à page_main, dont le on_gesture change les prévisions
 *       ou la pièce derrière le popup (handle_swipe_gesture, tab5_central.cpp). Un geste
 *       parti d'un curseur (luminosité, volume de la page Système) n'est pas un
 *       changement de page : LVGL émet aussi LV_EVENT_GESTURE pendant qu'on le glisse.
 */
#include "tab5_internal.h"
#include "tab5_economie.h"  // economie_sur_batterie() : ligne « État » de la page Batterie
#include "tab5_themes_data.h"  // THEMES[] : nom du thème choisi

#include <cstdio>
#include <string>

ReglagesUI g_reglages_ui;

namespace {

// Langue choisie, en attente de « Confirmer » (−1 : aucune).
int s_langue_choix = -1;

// Page affichée (ReglagesPage) : posée par reglages_afficher_page(), jamais ailleurs.
int s_page = REGLAGES_PAGE_ECRAN;

// Dernier état peint, pour reglages_rejouer_theme() (repeinture d'un changement de thème).
ReglagesEtat s_etat;

// Une rangée de boutons à choix : celui de l'option active en couleur d'accent (bordure
// et texte), les autres exactement au style du bouton (style_clim_btn, bordure et
// formes du thème) : on retire la bordure locale au lieu du gris « inactif » de
// highlight_button_border, comme le bouton « HA » (tab5_tuiles.cpp). Le texte est le
// premier enfant du bouton (reglages_choix_btn.yaml, reglages_onglet.yaml).
void peindre_choix(lv_obj_t* const* boutons, int n, int actif) {
    for (int i = 0; i < n; i++) {
        lv_obj_t* const b = boutons[i];
        if (b == nullptr) continue;
        const bool on = (i == actif);
        if (on) {
            highlight_button_border(b, true, UIColor.ACCENT, 3);
        } else {
            for (lv_style_prop_t p : {LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_OPA, LV_STYLE_BORDER_WIDTH})
                lv_obj_remove_local_style_prop(b, p, LV_PART_MAIN);
        }
        ui_text_color(lv_obj_get_child(b, 0), on ? UIColor.ACCENT : UIColor.TEXT_SOFT);
    }
}

// Oui / Non : 0 = Oui, 1 = Non (ordre des boutons du YAML).
inline int oui_non(bool v) { return v ? 0 : 1; }

// Page Batterie : état, niveau, tension et consommation d'après le dernier état gardé par
// le C++ (présence et mesures : tab5_batterie.cpp ; niveau publié et CHG_STAT :
// tab5_zones.cpp ; sur batterie : tab5_economie.cpp). Textes : tab5_core.cpp (testés).
void peindre_batterie() {
    const ReglagesUI& u = g_reglages_ui;
    if (u.popup == nullptr) return;
    const PresenceBatterie presence = batterie_presence();
    ui_text(u.batt_etat, batterie_etat_texte(presence, batterie_en_charge_lue(), economie_sur_batterie()));
    char buf[16];
    batterie_valeur_texte(buf, sizeof(buf), presence, batterie_niveau_lu(), MesureBatterie::NIVEAU);
    ui_text(u.batt_niveau, buf);
    batterie_valeur_texte(buf, sizeof(buf), presence, batterie_derniere_tension(), MesureBatterie::TENSION);
    ui_text(u.batt_tension, buf);
    batterie_valeur_texte(buf, sizeof(buf), presence, batterie_derniere_consommation(),
                          MesureBatterie::CONSOMMATION);
    ui_text(u.batt_conso, buf);
}

// Confirmations ouvertes (langue ; redémarrer HA ou la tablette, page Système) : refermées
// quand on change de page ou qu'on ferme le popup, comme « Annuler ».
void fermer_confirmations() {
    reglages_confirmation_fermer();
    ui_hidden(g_reglages_ui.confirm_ha, true);
    ui_hidden(g_reglages_ui.confirm_reboot, true);
}

// LV_EVENT_GESTURE du popup (reglages_preparer) : gauche = page suivante, droite = page
// précédente, en boucle (reglages_page_voisine, tab5_core.cpp). LVGL l'émet pendant
// l'appui, dès que le doigt a parcouru gesture_min_distance (lv_indev.c, indev_gesture),
// comme pour les prévisions. lv_indev_wait_release() : le lever du doigt qui suit ne
// déclenche rien (ni le bouton où le geste est parti, ni un appui long), c'est le
// « tap qui suit un glissement est ignoré » des autres popups, pour tout le popup.
void geste_rappel(lv_event_t* /*e*/) {
    lv_indev_t* const indev = lv_indev_active();
    if (indev == nullptr) return;
    const lv_dir_t dir = lv_indev_get_gesture_dir(indev);
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) return;
    // Un curseur (luminosité, volume) glissé de côté : c'est son réglage, pas une page.
    for (lv_obj_t* o = lv_indev_get_active_obj(); o != nullptr && o != g_reglages_ui.popup; o = lv_obj_get_parent(o)) {
        if (lv_obj_check_type(o, &lv_slider_class)) return;
    }
    lv_indev_wait_release(indev);
    reglages_afficher_page(reglages_page_voisine(s_page, REGLAGES_NB_PAGES, dir == LV_DIR_LEFT));
}

}  // namespace

void reglages_preparer() {
    const ReglagesUI& u = g_reglages_ui;
    const size_t n = i18n_language_count();
    for (int i = 0; i < REGLAGES_NB_LANGUES; i++) {
        lv_obj_t* const b = u.langue[i];
        if (b == nullptr) continue;
        const bool existe = static_cast<size_t>(i) < n;
        ui_hidden(b, !existe);
        if (existe) ui_text(lv_obj_get_child(b, 0), i18n_language_name(i));
    }
    // Geste gauche / droite : arrêté au popup ([AI-WARNING] de l'en-tête), traité ici.
    if (u.popup != nullptr) {
        lv_obj_remove_flag(u.popup, LV_OBJ_FLAG_GESTURE_BUBBLE);
        lv_obj_add_event_cb(u.popup, geste_rappel, LV_EVENT_GESTURE, nullptr);
    }
}

void reglages_luminosite_ui(int pourcent) {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d %%", pourcent);
    ui_text(g_reglages_ui.lum_valeur, buf);
}

void reglages_peindre(const ReglagesEtat& e) {
    const ReglagesUI& u = g_reglages_ui;
    if (u.popup == nullptr) return;
    s_etat = e;

    // Curseur : borné à sa plage (10-100 %, reglages_popup.yaml) ; le rétroéclairage a pu
    // être réglé plus bas depuis HA. Pas pendant un glissement : une repeinture (thème,
    // entité changée par HA) ferait sauter le curseur sous le doigt (comme le volet,
    // tab5_tuiles.cpp).
    if (u.lum_slider != nullptr && !lv_obj_has_state(u.lum_slider, LV_STATE_PRESSED)) {
        if (lv_slider_get_value(u.lum_slider) != e.luminosite)
            lv_slider_set_value(u.lum_slider, e.luminosite, LV_ANIM_OFF);
        reglages_luminosite_ui(e.luminosite);
    }

    peindre_choix(u.extinction, REGLAGES_NB_EXTINCTION, e.extinction);
    peindre_choix(u.okay_nabu, 2, oui_non(e.okay_nabu));
    peindre_choix(u.tape, 2, oui_non(e.tape));

    // Nom du thème : un nom propre, écrit comme dans le select de HA (jamais traduit).
    const int theme = (e.theme >= 0 && e.theme < THEME_COUNT) ? e.theme : 0;
    ui_text(u.theme_nom, THEMES[theme].nom);
    peindre_choix(u.mode, REGLAGES_NB_MODES, e.mode);
    peindre_choix(u.nuit, 2, oui_non(e.nuit));

    peindre_choix(u.langue, REGLAGES_NB_LANGUES, e.langue);

    peindre_choix(u.limite, REGLAGES_NB_LIMITES, e.limite);
    peindre_choix(u.economie, REGLAGES_NB_ECONOMIE, e.economie);
    peindre_choix(u.montee, 2, oui_non(e.montee));
}

// Changement de thème (theme_rejouer_ui, tab5_theme.cpp) : les couleurs posées ici
// (accent de l'option active et du nom de la page, texte des autres) reprennent la
// palette active.
void reglages_rejouer_theme() {
    reglages_peindre(s_etat);
    if (g_reglages_ui.popup != nullptr) peindre_choix(g_reglages_ui.onglet, REGLAGES_NB_PAGES, s_page);
}

void reglages_afficher_page(int page) {
    const ReglagesUI& u = g_reglages_ui;
    if (u.popup == nullptr) return;
    if (page < 0 || page >= REGLAGES_NB_PAGES) page = REGLAGES_PAGE_ECRAN;
    fermer_confirmations();
    s_page = page;
    for (int i = 0; i < REGLAGES_NB_PAGES; i++) ui_hidden(u.page[i], i != page);
    peindre_choix(u.onglet, REGLAGES_NB_PAGES, page);
    if (page == REGLAGES_PAGE_BATTERIE) peindre_batterie();
    if (u.page_montree != nullptr) u.page_montree(page);
}

bool reglages_page_visible(int page) {
    const ReglagesUI& u = g_reglages_ui;
    return u.popup != nullptr && s_page == page && !lv_obj_has_flag(u.popup, LV_OBJ_FLAG_HIDDEN);
}

void reglages_batterie_peindre() {
    if (reglages_page_visible(REGLAGES_PAGE_BATTERIE)) peindre_batterie();
}

void reglages_fermer() {
    fermer_confirmations();
    animate_popup_close(g_reglages_ui.popup);
}

void reglages_langue_demander(int langue) {
    const ReglagesUI& u = g_reglages_ui;
    if (u.confirmation == nullptr || langue < 0 || static_cast<size_t>(langue) >= i18n_language_count() ||
        langue == static_cast<int>(i18n_language())) {
        return;
    }
    s_langue_choix = langue;
    const std::string texte = tr_fill("La tablette redémarre en {langue}.", {{"langue", i18n_language_name(langue)}});
    ui_text(u.confirmation_texte, texte.c_str());
    ui_hidden(u.confirmation, false);
    lv_obj_move_foreground(u.confirmation);
}

int reglages_langue_confirmer() {
    const int langue = s_langue_choix;
    reglages_confirmation_fermer();
    return langue;
}

void reglages_confirmation_fermer() {
    s_langue_choix = -1;
    ui_hidden(g_reglages_ui.confirmation, true);
}
