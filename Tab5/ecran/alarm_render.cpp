/**
 * [AI-CONTEXT]
 * @file alarm_render.cpp
 * @role Rendu LVGL du réveil : popup de réglage, calque de sonnerie, pastille de
 *       la barre d'état. Sorti d'alarm_clock.cpp le 25/09/2026 (audit, lot 8b) pour
 *       que le moteur reste compilable et testable sur PC. Ne lit l'état du moteur
 *       que par son API publique (alarm_clock.h).
 *       Popup en cinq pages depuis le 09/10/2026 (demande d'Axel : « plus clair, plus
 *       grand, avec le switch pour passer d'une partie à l'autre comme les Paramètres,
 *       les roues dès que possible ») : noms des pages en haut et geste gauche / droite
 *       partagés avec les Réglages (PopupPages, tab5_pages.cpp) ; chaque valeur à
 *       plage (heure, minutes, bornes, délai, repos, répétition, durée max, rendez-vous)
 *       se règle sur un rouleau LVGL (lv_roller, rouleau.yaml), dont les options sont
 *       écrites ici (textes traduits, valeur hors pas venue de HA insérée à sa place).
 * @architecture_constraint Écritures comparées d'abord (ui_text, ui_text_color ; lot L10,
 *       08/10/2026) : tab5_alarm_sync_ui repeint tout le popup à chaque réglage du réveil,
 *       et la sonnerie repeint son calque à chaque cycle. Un rouleau n'est réécrit (ses
 *       options) que si sa valeur hors pas change, et jamais repositionné sous le doigt.
 *       Ce rendu reste hors d'alarm_clock.cpp, que tools/test_alarm_clock.cpp compile
 *       sans LVGL ; le calcul index ↔ valeur des rouleaux est pur (rouleau_*,
 *       tab5_core.cpp, testé).
 * @ai_instruction Un rouleau de plus : sa valeur dans ReveilRouleau (alarm_render.h), sa
 *       ligne dans kRouleaux (mêmes bornes et pas que l'entité, tests/test_alarme_popup.py),
 *       sa valeur dans valeur_rouleau(), son cas dans le script tab5_alarm_rouleau
 *       (tab5-alarm.yaml), son include de rouleau.yaml et son pointeur dans tab5_alarm_open.
 */
#include "alarm_render.h"
#include "tab5_custom.h"
#include "tab5_internal.h"   // ui_text(), ui_text_color(), choix_peindre(), PopupPages

#include <cstdio>

ReveilUI g_reveil_ui;

namespace {

// Pages (ReveilPage), noms en haut et geste : tab5_pages.cpp.
PopupPages s_pages;

// ─── Rouleaux ───────────────────────────────────────────────────────────────
enum class FormatRouleau : uint8_t {
    DEUX_CHIFFRES,  // « 07 »
    HHMM,           // « 05:15 » (minutes depuis minuit)
    DUREE,          // « 1h30 » (minutes)
    HEURES,         // « 9 h »
    MINUTES,        // « 15 min »
};

struct RouleauDef {
    int bas;
    int haut;
    int pas;
    FormatRouleau format;
    bool boucle;  // LV_ROLLER_MODE_INFINITE : 23 → 00 d'un geste (heure et minutes)
};

// Bornes et pas des entités de tab5-alarm.yaml (number : min_value, max_value, step) ;
// les heures et les minutes de l'heure fixe vont de 5 en 5 comme les anciens boutons
// ± 5 min (« on règle un réveil, pas un chronomètre »), les bornes du mode Ouverture
// de 15 en 15. Une valeur réglée autrement depuis HA est montrée telle quelle
// (rouleau_extra). tests/test_alarme_popup.py compare cette table aux entités.
constexpr RouleauDef kRouleaux[REVEIL_NB_ROULEAUX] = {
    {0, 23, 1, FormatRouleau::DEUX_CHIFFRES, true},   // REVEIL_ROULEAU_HEURE
    {0, 59, 5, FormatRouleau::DEUX_CHIFFRES, true},   // REVEIL_ROULEAU_MINUTES
    {0, 1439, 15, FormatRouleau::HHMM, false},        // REVEIL_ROULEAU_PAS_AVANT
    {0, 1439, 15, FormatRouleau::HHMM, false},        // REVEIL_ROULEAU_PAS_APRES
    {0, 240, 5, FormatRouleau::DUREE, false},         // REVEIL_ROULEAU_DELAI
    {0, 14, 1, FormatRouleau::HEURES, false},         // REVEIL_ROULEAU_REPOS
    {1, 30, 1, FormatRouleau::MINUTES, false},        // REVEIL_ROULEAU_REPETITION
    {1, 60, 1, FormatRouleau::MINUTES, false},        // REVEIL_ROULEAU_DUREE_MAX
    {0, 120, 5, FormatRouleau::MINUTES, false},       // REVEIL_ROULEAU_RDV_AVANT
};

// Valeur hors pas insérée dans chaque rouleau (−1 : aucune) et options déjà écrites.
int s_extra[REVEIL_NB_ROULEAUX];
bool s_ecrit[REVEIL_NB_ROULEAUX];

void texte_rouleau(char* b, size_t n, FormatRouleau f, int v) {
    switch (f) {
        case FormatRouleau::DEUX_CHIFFRES: snprintf(b, n, "%02d", v); break;
        case FormatRouleau::HHMM: alarm_hhmm(v, b, n); break;
        case FormatRouleau::DUREE: snprintf(b, n, tr("%dh%02d"), v / 60, v % 60); break;
        case FormatRouleau::HEURES: snprintf(b, n, "%d h", v); break;
        case FormatRouleau::MINUTES: snprintf(b, n, tr("%d min"), v); break;
    }
}

// Options d'un rouleau, une par ligne (lv_roller_set_options copie le texte). 97 options
// au plus (bornes du mode Ouverture : 96 pas + une valeur hors pas) de 16 octets au plus.
void ecrire_options(lv_obj_t* r, const RouleauDef& d, int extra) {
    static char opts[97 * 16];
    const int n = rouleau_nombre(d.bas, d.haut, d.pas, extra);
    size_t pos = 0;
    opts[0] = '\0';
    for (int i = 0; i < n && pos < sizeof(opts); i++) {
        char un[16];
        texte_rouleau(un, sizeof(un), d.format, rouleau_valeur(d.bas, d.haut, d.pas, extra, i));
        const int ecrit = snprintf(opts + pos, sizeof(opts) - pos, i == 0 ? "%s" : "\n%s", un);
        if (ecrit < 0) break;
        pos += static_cast<size_t>(ecrit);
    }
    lv_roller_set_options(r, opts, d.boucle ? LV_ROLLER_MODE_INFINITE : LV_ROLLER_MODE_NORMAL);
}

// Rouleau `k` sur `valeur`. Ses options ne sont réécrites que si la valeur hors pas
// change (lv_roller_set_options remet la sélection à 0 : on la repose alors toujours).
// Sinon, pas de repositionnement sous le doigt : la peinture qui suit un réglage (ou
// un changement venu de HA pendant un glissement) ferait sauter le rouleau.
void peindre_rouleau(int k, int valeur) {
    lv_obj_t* const r = g_reveil_ui.rouleau[k];
    if (r == nullptr) return;
    const RouleauDef& d = kRouleaux[k];
    const int extra = rouleau_extra(d.bas, d.haut, d.pas, valeur);
    const bool reecrit = !s_ecrit[k] || extra != s_extra[k];
    if (reecrit) {
        ecrire_options(r, d, extra);
        s_extra[k] = extra;
        s_ecrit[k] = true;
    } else if (lv_obj_has_state(r, LV_STATE_PRESSED)) {
        return;
    }
    const int i = rouleau_index(d.bas, d.haut, d.pas, extra, valeur);
    if (reecrit || static_cast<int>(lv_roller_get_selected(r)) != i)
        lv_roller_set_selected(r, static_cast<uint32_t>(i), LV_ANIM_OFF);
}

// Valeur montrée par chaque rouleau, lue dans g_alarm_cfg.
int valeur_rouleau(int k) {
    const AlarmCfg& c = g_alarm_cfg;
    switch (k) {
        case REVEIL_ROULEAU_HEURE: return c.fixed_min / 60;
        case REVEIL_ROULEAU_MINUTES: return c.fixed_min % 60;
        case REVEIL_ROULEAU_PAS_AVANT: return c.earliest_min;
        case REVEIL_ROULEAU_PAS_APRES: return c.latest_min;
        case REVEIL_ROULEAU_DELAI: return c.lead_min;
        case REVEIL_ROULEAU_REPOS: return c.rest_hours;
        case REVEIL_ROULEAU_REPETITION: return c.snooze_min;
        case REVEIL_ROULEAU_DUREE_MAX: return c.max_ring_min;
        default: return c.rdv_lead_min;
    }
}

// Bascule visuelle de la grande bascule « Réveil » (page Heure) : bordure colorée
// (highlight_button_border) + libellé et icône assortis.
void set_toggle(lv_obj_t* btn, lv_obj_t* lbl, bool on, const char* on_txt, const char* off_txt,
                uint32_t on_color) {
    if (btn != nullptr) highlight_button_border(btn, on, on_color);
    if (lbl != nullptr) {
        ui_text(lbl, tr(on ? on_txt : off_txt));
        ui_text_color(lbl, on ? on_color : UIColor.TEXT_DIM);
    }
}

// Oui / Non : 0 = Oui, 1 = Non (ordre des boutons du YAML, comme les Réglages).
inline int oui_non(bool v) { return v ? 0 : 1; }

}  // namespace

void reveil_preparer() {
    ReveilUI& u = g_reveil_ui;
    if (u.popup == nullptr) return;
    // Noms courts des jours, traduits (« Lun » … « Dim ») : day_short_utf8 compte depuis
    // dimanche, les pastilles depuis lundi.
    for (int i = 0; i < 7; i++) ui_text(u.day_lbl[i], day_short_utf8((i + 1) % 7));
    for (int k = 0; k < REVEIL_NB_ROULEAUX; k++) {
        s_ecrit[k] = false;
        s_extra[k] = -1;
    }
    // Geste gauche / droite : arrêté au popup, ignoré depuis un rouleau ou le curseur du
    // volume ([AI-WARNING] de tab5_pages.cpp).
    s_pages.popup = u.popup;
    s_pages.page = u.page;
    s_pages.onglet = u.onglet;
    s_pages.n = REVEIL_NB_PAGES;
    s_pages.afficher = reveil_afficher_page;
    pages_brancher(s_pages);
}

void reveil_afficher_page(int page) {
    if (g_reveil_ui.popup == nullptr) return;
    pages_montrer(s_pages, page);  // hors bornes : la page Heure (la première)
}

int reveil_rouleau_valeur(int quoi, int index) {
    if (quoi < 0 || quoi >= REVEIL_NB_ROULEAUX) return -1;
    const RouleauDef& d = kRouleaux[quoi];
    return rouleau_valeur(d.bas, d.haut, d.pas, s_extra[quoi], index);
}

// Dernier état peint (repris par reveil_rejouer_theme).
namespace {
struct DernierReveil {
    time_t now = 0;
    bool crescendo = false;
    bool tts_on = false;
    bool rdv_on = false;
} s_dernier;
}  // namespace

// Changement de thème (theme_rejouer_ui, tab5_theme.cpp) : les couleurs posées ici
// (nom de la page affichée et option active en accent, texte des autres, bascule,
// phrases) reprennent la palette active, popup ouvert ou non. Vu au rendu du 09/10/2026 :
// sans cet appel, les noms des pages gardaient les couleurs du thème d'avant.
void reveil_rejouer_theme() {
    if (g_reveil_ui.popup == nullptr) return;
    choix_peindre(g_reveil_ui.onglet, REVEIL_NB_PAGES, s_pages.courante);
    alarm_render_settings(s_dernier.now, s_dernier.crescendo, s_dernier.tts_on, s_dernier.rdv_on);
}

void alarm_render_settings(time_t now, bool crescendo, bool tts_on, bool rdv_on) {
    const ReveilUI& u = g_reveil_ui;
    if (u.popup == nullptr) return;  // jamais ouvert : rien à peindre
    s_dernier = {now, crescendo, tts_on, rdv_on};
    const AlarmCfg& c = g_alarm_cfg;
    char buf[96];

    // ── Page Heure ──
    set_toggle(u.btn_enable, u.lbl_enable, c.enabled, "R\xC3\xA9veil actif", "R\xC3\xA9veil \xC3\xA9teint",
               UIColor.SUCCESS);
    if (u.icon_enable != nullptr) {
        // F0020 = alarm, F0023 = alarm-off (codepoints vérifiés dans le TTF du projet).
        ui_text(u.icon_enable, c.enabled ? "\U000F0020" : "\U000F0023");
        ui_text_color(u.icon_enable, c.enabled ? UIColor.SUCCESS : UIColor.TEXT_DIM);
    }
    if (u.lbl_next != nullptr) ui_text(u.lbl_next, alarm_next_label(now).c_str());
    if (u.lbl_next_sub != nullptr) ui_text(u.lbl_next_sub, alarm_next_detail(now).c_str());

    for (int k = 0; k < REVEIL_NB_ROULEAUX; k++) peindre_rouleau(k, valeur_rouleau(k));

    // ── Page Jours ──
    choix_peindre(u.mode, AlarmMode::COUNT, c.mode);
    if (u.lbl_mode_hint != nullptr) {
        // Une phrase par mode, dans l'ordre d'AlarmMode (traduite à l'affichage).
        static const char* const kHints[AlarmMode::COUNT] = {
            tr_noop("Sonne \xC3\xA0 l'heure fixe, les jours coch\xC3\xA9s ci-dessous."),
            tr_noop("Sonne \xC3\xA0 l'heure fixe, uniquement les jours travaill\xC3\xA9s."),
            tr_noop("Sonne avant l'ouverture lue dans le calendrier."),
        };
        ui_text(u.lbl_mode_hint, tr(kHints[c.mode >= 0 && c.mode < AlarmMode::COUNT ? c.mode : AlarmMode::EMBAUCHE]));
    }
    // Les jours cochés servent à l'heure fixe : en mode Fixe, et les jours sans travail
    // quand « Jours de repos » vaut « Heure fixe » (alarm_clock.cpp). Sinon estompés.
    const bool jours_utiles = c.mode == AlarmMode::FIXE || c.rest_mode == AlarmRepos::FIXE;
    for (int i = 0; i < 7; i++) {
        const bool coche = (c.days_mask >> i) & 1;
        if (jours_utiles) {
            choix_bouton(u.day_btn[i], coche);
            continue;
        }
        // Jours inutiles : la bordure de choix_bouton, le texte estompé. Une seule écriture
        // de couleur (choix_bouton puis TEXT_DIM repeindrait la pastille à chaque réglage).
        if (u.day_btn[i] != nullptr) {
            if (coche) {
                highlight_button_border(u.day_btn[i], true, UIColor.ACCENT, 3);
            } else {
                for (lv_style_prop_t prop : {LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_OPA, LV_STYLE_BORDER_WIDTH})
                    lv_obj_remove_local_style_prop(u.day_btn[i], prop, LV_PART_MAIN);
            }
        }
        ui_text_color(u.day_lbl[i], UIColor.TEXT_DIM);
    }
    const int prereglage = alarm_days_preset_index(c.days_mask);  // « Personnalisé » : aucun
    choix_peindre(u.prereglage, REVEIL_NB_PREREGLAGES, prereglage < REVEIL_NB_PREREGLAGES ? prereglage : -1);
    choix_peindre(u.repos, 2, c.rest_mode == AlarmRepos::FIXE ? 1 : 0);
    if (u.lbl_repos_hint != nullptr) {
        ui_text(u.lbl_repos_hint,
                c.mode == AlarmMode::FIXE
                    ? tr("En mode Fixe, seuls les jours coch\xC3\xA9s comptent.")
                    : tr("Jour sans travail au calendrier : silence, ou l'heure fixe si le jour est coch\xC3\xA9."));
    }

    // ── Page Ouverture ──
    if (u.lbl_ouverture_hint != nullptr) {
        if (c.mode == AlarmMode::EMBAUCHE) {
            char delai[16], avant[8], apres[8];
            texte_rouleau(delai, sizeof(delai), FormatRouleau::DUREE, c.lead_min);
            alarm_hhmm(c.earliest_min, avant, sizeof(avant));
            alarm_hhmm(c.latest_min, apres, sizeof(apres));
            const std::string t = tr_fill("Sonne {avance} avant l'ouverture lue dans le calendrier, jamais avant {avant} ni apr\xC3\xA8s {apres}.",
                                          {{"avance", delai}, {"avant", avant}, {"apres", apres}});
            ui_text(u.lbl_ouverture_hint, t.c_str());
            ui_text_color(u.lbl_ouverture_hint, UIColor.TEXT_SOFT);
        } else {
            ui_text(u.lbl_ouverture_hint,
                    tr("Ces r\xC3\xA9glages ne servent qu'au mode \xC2\xAB Ouverture \xC2\xBB (page Jours)."));
            ui_text_color(u.lbl_ouverture_hint, UIColor.WARNING);
        }
    }

    // ── Page Sonnerie ── (le nom d'une mélodie est aussi une option du select HA : le YAML
    // le pose, traduit au démarrage)
    choix_peindre(u.melodie, ALARM_MELODY_COUNT, c.melody);
    if (u.slider_vol != nullptr) {
        const int pct = static_cast<int>(c.volume * 100.0f + 0.5f);
        // lv_slider_set_value ne déclenche pas LV_EVENT_VALUE_CHANGED : pas de rebouclage sur
        // on_value. Pas pendant un glissement (le curseur sauterait sous le doigt).
        if (!lv_obj_has_state(u.slider_vol, LV_STATE_PRESSED) && lv_slider_get_value(u.slider_vol) != pct)
            lv_slider_set_value(u.slider_vol, pct, LV_ANIM_OFF);
        if (u.lbl_vol != nullptr) {
            snprintf(buf, sizeof(buf), "%d %%", pct);
            ui_text(u.lbl_vol, buf);
        }
    }
    choix_peindre(u.progressif, 2, oui_non(crescendo));

    // ── Page Annonces ──
    choix_peindre(u.tts, 2, oui_non(tts_on));
    choix_peindre(u.rdv, 2, oui_non(rdv_on));
    if (u.lbl_rdv_next != nullptr) {
        // Plusieurs lignes (label à hauteur fixe, « … » au-delà) : la carte a la place d'un
        // titre long, que l'ancienne barre du bas coupait sur une ligne.
        const std::string n = rdv_next_label(now);
        char txt[200];
        texte_ha_copier(txt, sizeof(txt), n.c_str(), n.size());
        ui_text(u.lbl_rdv_next, n.empty() ? tr("Aucun rendez-vous \xC3\xA0 venir") : txt);
        ui_text_color(u.lbl_rdv_next, n.empty() ? UIColor.TEXT_DIM : UIColor.TEXT_SOFT);
    }
}

static void ring_paint_clock(const AlarmRingUI& ui, time_t now) {
  if (ui.lbl_time == nullptr) return;
  struct tm t;
  if (localtime_r(&now, &t) == nullptr) return;
  char buf[8];
  snprintf(buf, sizeof(buf), "%02d:%02d", t.tm_hour, t.tm_min);
  ui_text(ui.lbl_time, buf);
}

void alarm_ring_show(const AlarmRingUI& ui, time_t now, const std::string& sub) {
  if (ui.root == nullptr) return;
  ring_paint_clock(ui, now);
  if (ui.lbl_sub != nullptr) ui_text(ui.lbl_sub, sub.c_str());
  if (ui.icon != nullptr) ui_text(ui.icon, "\U000F0020");
  alarm_ring_refresh(ui, now, g_alarm_cfg.snooze_min, alarm_snooze_count());
  lv_obj_remove_flag(ui.root, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_to_index(ui.root, -1);
}

void alarm_ring_refresh(const AlarmRingUI& ui, time_t now, int snooze_min, int snooze_count) {
  ring_paint_clock(ui, now);
  char buf[64];
  if (ui.lbl_title != nullptr) {
    if (snooze_count > 0) {
      snprintf(buf, sizeof(buf), tr("R\xC3\xA9p\xC3\xA9tition %d"), snooze_count);
    } else {
      snprintf(buf, sizeof(buf), "%s", tr("R\xC3\xA9veil"));
    }
    ui_text(ui.lbl_title, buf);
  }
  if (ui.lbl_snooze != nullptr) {
    snprintf(buf, sizeof(buf), tr("R\xC3\xA9p\xC3\xA9ter \xC2\xB7 %d min"), snooze_min);
    ui_text(ui.lbl_snooze, buf);
  }
}

void alarm_ring_hide(const AlarmRingUI& ui) {
  if (ui.root != nullptr) lv_obj_add_flag(ui.root, LV_OBJ_FLAG_HIDDEN);
}

void alarm_render_status_icon(lv_obj_t* icon, time_t now) {
  if (icon == nullptr) return;
  if (!g_alarm_cfg.enabled) {
    ui_text(icon, "\U000F0023");  // alarm-off
    ui_text_color(icon, UIColor.INACTIVE);
    return;
  }
  ui_text(icon, "\U000F0020");  // alarm
  // Vert quand la prochaine sonnerie est réellement calculée, ambre quand le
  // réveil est armé mais qu'aucun jour n'est retenu (piège classique : mode
  // « jours travaillés » + semaine de congés, ou tous les jours décochés).
  const bool armed = alarm_next_ring(now) != 0;
  ui_text_color(icon, armed ? UIColor.SUCCESS : UIColor.WARNING);
}
