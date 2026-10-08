/**
 * [AI-CONTEXT]
 * @file tab5_reglages.cpp
 * @role Popup « Réglages » (06/10/2026, demande d'Axel) : les réglages de l'écran qu'on
 *       veut changer sans passer par Home Assistant. Carte « Écran » : luminosité,
 *       extinction auto, rallumage par « Okay Nabu » et par une tape. Carte
 *       « Apparence » : thème, clair ou sombre, nuit du mode Auto, langue.
 *       Ouvert par un tap sur le bouton central du haut (engrenage) ou par « Aller à
 *       l'écran → Réglages ». Ce fichier ne fait que peindre : les gestes écrivent les
 *       entités (script tab5_reglages_choisir, tab5-reglages.yaml), et chaque entité
 *       relance tab5_reglages_sync_ui quand elle change, qui appelle reglages_peindre().
 *       Le popup montre donc toujours ce que HA voit.
 * @architecture_constraint Rien gardé ici, sauf la langue en attente de confirmation et
 *       une copie du dernier état peint (reglages_rejouer_theme, appelée par
 *       theme_rejouer_ui) : la vérité est dans les entités (restore_value / restore_mode). Changer de langue
 *       redémarre la tablette (on_value du select « Langue ») : une pastille de langue
 *       ouvre d'abord une confirmation, jamais le redémarrage direct.
 * @ai_instruction Un réglage de plus : sa valeur dans ReglageId (tab5_custom.h, à la fin),
 *       son cas dans tab5_reglages_choisir, son champ dans ReglagesEtat et ReglagesUI, sa
 *       ligne dans tab5_reglages_sync_ui et tab5_reglages_ouvrir, et un
 *       `script.execute: tab5_reglages_sync_ui` dans le on_value / on_state de l'entité.
 *       Un texte affiché passe par tr().
 */
#include "tab5_internal.h"
#include "tab5_themes_data.h"  // THEMES[] : nom du thème choisi

#include <cstdio>
#include <string>

ReglagesUI g_reglages_ui;

namespace {

// Langue choisie, en attente de « Confirmer » (−1 : aucune).
int s_langue_choix = -1;

// Dernier état peint, pour reglages_rejouer_theme() (repeinture d'un changement de thème).
ReglagesEtat s_etat;

// Une rangée de boutons à choix : celui de l'option active en couleur d'accent (bordure
// et texte), les autres exactement au style du bouton (style_clim_btn, bordure et
// formes du thème) : on retire la bordure locale au lieu du gris « inactif » de
// highlight_button_border, comme le bouton « HA » (tab5_tuiles.cpp). Le texte est le
// premier enfant du bouton (reglages_choix_btn.yaml).
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
}

// Changement de thème (theme_rejouer_ui, tab5_theme.cpp) : les couleurs posées ici
// (accent de l'option active, texte des autres) reprennent la palette active.
void reglages_rejouer_theme() {
    reglages_peindre(s_etat);
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
