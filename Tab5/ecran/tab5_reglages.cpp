/**
 * [AI-CONTEXT]
 * @file tab5_reglages.cpp
 * @role Popup « Réglages » (06/10/2026, demande d'Axel), en quatre pages depuis le
 *       08/10/2026 : « Écran » (luminosité, extinction auto, rallumage par « Okay Nabu »
 *       et par une tape), « Apparence » (thème, clair ou sombre, nuit du mode Auto,
 *       animations, langue), « Batterie » (limite et mode de charge, économie d'énergie,
 *       Wi-Fi éco, batterie montée ; état, niveau, tension et consommation lus ; mode de
 *       charge, Wi-Fi éco et animations depuis le 09/10/2026, ADR-0045) et « Système »
 *       (l'ancienne console système,
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
 *       pages sont masqués / montrés, sans glissement ni fondu. Le mécanisme des pages
 *       (noms en haut, geste, choix en couleur d'accent) est partagé avec le popup du
 *       réveil depuis le 09/10/2026 : tab5_pages.cpp (PopupPages).
 * @ai_instruction Un réglage de plus : sa valeur dans ReglageId (tab5_custom.h, à la fin),
 *       son cas dans tab5_reglages_choisir, son champ dans ReglagesEtat et ReglagesUI, sa
 *       ligne dans tab5_reglages_sync_ui et tab5_reglages_ouvrir, et un
 *       `script.execute: tab5_reglages_sync_ui` dans le on_value / on_state de l'entité.
 *       Un texte affiché passe par tr(). Une page de plus : sa valeur dans ReglagesPage
 *       (avant REGLAGES_NB_PAGES), son conteneur et son nom (reglages_onglet.yaml) dans
 *       reglages_popup.yaml, leurs lignes dans tab5_reglages_ouvrir.
 * @ai_warning [AI-WARNING] Le geste de changement de page s'arrête au popup : le drapeau
 *       LV_OBJ_FLAG_GESTURE_BUBBLE est retiré de reglages_popup (reglages_preparer →
 *       pages_brancher, tab5_pages.cpp). Sans ça, LVGL le remonte jusqu'à page_main, dont
 *       le on_gesture change les prévisions ou la pièce derrière le popup
 *       (handle_swipe_gesture, tab5_central.cpp). Un geste parti d'un curseur (luminosité,
 *       volume de la page Système) n'est pas un changement de page : LVGL émet aussi
 *       LV_EVENT_GESTURE pendant qu'on le glisse.
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

// Pages (ReglagesPage), noms en haut et geste : tab5_pages.cpp. Page affichée posée par
// reglages_afficher_page() (pages_montrer), jamais ailleurs.
PopupPages s_pages;

// Dernier état peint, pour reglages_rejouer_theme() (repeinture d'un changement de thème).
ReglagesEtat s_etat;

// Une rangée de boutons à choix : celui de l'option active en couleur d'accent (bordure
// et texte), les autres exactement au style du bouton (style_clim_btn, bordure et
// formes du thème) : on retire la bordure locale au lieu du gris « inactif » de
// highlight_button_border, comme le bouton « HA » (tab5_tuiles.cpp). Le texte est le
// premier enfant du bouton (reglages_choix_btn.yaml, reglages_onglet.yaml). Partagé
// avec le popup du réveil : choix_peindre (tab5_pages.cpp).
inline void peindre_choix(lv_obj_t* const* boutons, int n, int actif) { choix_peindre(boutons, n, actif); }

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
    // Geste gauche / droite : arrêté au popup ([AI-WARNING] de l'en-tête), page voisine
    // montrée par reglages_afficher_page (confirmations refermées, page peinte).
    if (u.popup != nullptr) {
        s_pages.popup = u.popup;
        s_pages.page = u.page;
        s_pages.onglet = u.onglet;
        s_pages.n = REGLAGES_NB_PAGES;
        s_pages.afficher = reglages_afficher_page;
        pages_brancher(s_pages);
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
    peindre_choix(u.animations, REGLAGES_NB_ANIMATIONS, e.animations);

    peindre_choix(u.langue, REGLAGES_NB_LANGUES, e.langue);

    peindre_choix(u.limite, REGLAGES_NB_LIMITES, e.limite);
    peindre_choix(u.mode_charge, REGLAGES_NB_MODES_CHARGE, e.mode_charge);
    peindre_choix(u.economie, REGLAGES_NB_ECONOMIE, e.economie);
    peindre_choix(u.wifi_eco, REGLAGES_NB_WIFI_ECO, e.wifi_eco);
    peindre_choix(u.montee, 2, oui_non(e.montee));
}

// Changement de thème (theme_rejouer_ui, tab5_theme.cpp) : les couleurs posées ici
// (accent de l'option active et du nom de la page, texte des autres) reprennent la
// palette active.
void reglages_rejouer_theme() {
    reglages_peindre(s_etat);
    if (g_reglages_ui.popup != nullptr) peindre_choix(g_reglages_ui.onglet, REGLAGES_NB_PAGES, s_pages.courante);
}

void reglages_afficher_page(int page) {
    const ReglagesUI& u = g_reglages_ui;
    if (u.popup == nullptr) return;
    fermer_confirmations();
    pages_montrer(s_pages, page);  // hors bornes : la page Écran (la première)
    if (s_pages.courante == REGLAGES_PAGE_BATTERIE) peindre_batterie();
    if (u.page_montree != nullptr) u.page_montree(s_pages.courante);
}

bool reglages_page_visible(int page) { return pages_visible(s_pages, page); }

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
