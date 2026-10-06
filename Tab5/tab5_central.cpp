/**
 * [AI-CONTEXT]
 * @file tab5_central.cpp
 * @role Carte centrale : rotateur planning/pluie/alertes/info, bandeaux d'alertes HA,
 *       titre de page, pagination des prévisions au swipe (apply_forecast_page,
 *       handle_swipe_gesture, reset_forecast_to_main_page), planning temporaire au tap
 *       sur une tuile, réponse vocale.
 *       Unité de compilation issue de la scission de tab5_custom.cpp (lot (e) de
 *       l'audit du 06/09/2026, faite le 08/09/2026) : mêmes fonctions, même ordre,
 *       aucune logique modifiée.
 * @regle_absolue Seul point de contact avec l'API LVGL, comme avant : les YAML
 *                n'appellent que des helpers déclarés dans tab5_custom.h. Les
 *                helpers partagés entre unités sont déclarés dans tab5_internal.h.
 * @memory_constraint Éviter std::string dans les boucles de parsing ; char* + strtok_r.
 */
#include "tab5_custom.h"
#include "tab5_internal.h"
#include "tab5_registry.h"
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <array>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <vector>

// =============================================================================
// Geste de swipe (page_main.on_gesture) : pagination previsions (y >= carte centrale)
// =============================================================================

static constexpr int32_t FORECAST_SWIPE_Y_MIN = 333;  // haut de central_card (tab5-lvgl.yaml)

// Largeur des panneaux de la carte centrale : ${central_w} de tab5-ui-tokens.yaml,
// recopiée ici (le C++ ne lit pas les substitutions) et tenue égale par
// tests/test_geometrie_partagee.py.
static constexpr int32_t kLargeurPanneauCentral = 1180;

// Page de repos des previsions : journalier J0-J4, celle du boot
// (CentralPanelCtx::forecast_page = 2) et celle ou la carte centrale reprend
// son rotateur planning/pluie/alertes. C'est la cible du retour automatique.
static constexpr int FORECAST_MAIN_PAGE = 2;

// Planning temporaire (tap tuile, 6 s) : défini plus bas, avec TempPlanningCtx
// (temp_planning_active() est déclarée dans tab5_custom.h, les scripts la lisent).
static void end_temporary_planning(CentralPanelCtx& ctx);

// Police locale ou celle des styles du label (tab5_internal.h).
void ui_police(lv_obj_t* obj, esphome::font::Font* f) {
    if (obj == nullptr) return;
    if (f != nullptr) {
        esphome::lvgl::lv_obj_set_style_text_font(obj, f, LV_PART_MAIN);
    } else {
        lv_obj_remove_local_style_prop(obj, LV_STYLE_TEXT_FONT, LV_PART_MAIN);
    }
}

// Qui occupe la carte centrale (enfants de central_card, tab5-lvgl.yaml) :
//   - planning du tap : planning_wrap avec le texte du jour, 6 s (temp_planning_active()) ;
//   - réponse vocale : vocal_wrap, 8 s, accueil seulement (ctx.vocal_shown) ;
//   - titre de la pièce : page_title_wrap, mode HA, toutes les pages (ctx.ha_mode) ;
//   - titre de page : page_title_wrap, pages de prévisions 0, 1, 3, 4 ;
//   - rotateur : sur l'accueil, le panneau ctx.current_panel s'il est actif, sinon rien.
// Les deux premiers prennent la carte (prendre_carte) et la rendent à leur fin ;
// changer de page ou de mode les termine (liberer_carte) puis pose le titre ou le
// panneau de la nouvelle page (update_central_forecast_page_ui).
//
// Le rotateur (panneaux 0-7) n'a la main sur la carte centrale que sur l'accueil
// (page 2), hors planning temporaire et hors réponse vocale. Ailleurs la carte
// appartient au titre de page ou à l'overlay : une mise à jour de drapeaux (push
// HA d'alertes ou d'info, acquittement, fin de réponse vocale) y réaffichait un
// panneau transparent par-dessus (audit du 25/09/2026, §2.4). Seuls les drapeaux et
// l'index sont alors tenus à jour ; l'affichage revient au rotateur quand il
// retrouve la main (retour sur la page 2, fin du planning ou de la réponse vocale).
// En mode HA (ADR-0023), la carte porte le titre de la pièce, sur toutes les pages.
static bool rotator_owns_card(const CentralPanelCtx& ctx) {
    return ctx.forecast_page == FORECAST_MAIN_PAGE && !ctx.vocal_shown && !temp_planning_active() &&
           !ctx.ha_mode;
}

// Titre de la carte centrale sur les pages de previsions autres que l'accueil : les
// bornes reelles des 5 tuiles visibles, ex
// "Du mercredi 5 ao\xC3\xBBt au dimanche 9 ao\xC3\xBBt" ou "De 14:00 \xC3\xA0 18:00", sur une
// ligne, dans la police de la date (demande d'Axel du 05/10/2026 : le chapeau
// « Previsions horaires · 1/2 » / « Previsions journalieres · 2/3 » est retire, les
// points de pagination sous la carte disent deja la page).
// Renvoie false quand la page n'a pas de titre (page 2 = accueil : la carte
// centrale y reprend son rotateur planning/pluie/alertes) ou que les bornes manquent
// (donnees HA pas encore recues et SNTP muet) : la carte reste alors vide.
// Les bornes sont toujours donnees dans l'ordre chronologique (la plus tot ->
// la plus tard), y compris sur les pages horaires ou les tuiles sont affichees
// dans l'ordre inverse (cf. forecast_hourly.yaml).
static bool forecast_page_title_parts(int page, std::string& plage) {
    plage.clear();
    char buf[96];

    if (page == 3 || page == 4) {
        const int daily_pi = page - 2;                  // 1 = J5-J9, 2 = J10-J14
        const int premier = daily_pi * 5;
        const int dernier = premier + 4;
        std::string debut = format_long_day_label(premier);
        std::string fin   = format_long_day_label(dernier);
        if (debut.empty() || fin.empty()) {
            // SNTP pas encore synchronise : repli sur les libelles courts pousses
            // par HA ("Mer 05"), comme le fait deja refresh_daily_forecast().
            debut = ha_day_name(cal_jours_data[premier].nom_jour);
            fin   = ha_day_name(cal_jours_data[dernier].nom_jour);
        }
        if (!debut.empty() && !fin.empty()) {
            snprintf(buf, sizeof(buf), tr("Du %s au %s"), debut.c_str(), fin.c_str());
            plage = buf;
        }
        return !plage.empty();
    }

    if (page == 0 || page == 1) {
        // Pages horaires : l'index UI est inverse par rapport aux donnees
        // (apply_forecast_page appelle refresh_hourly_forecast(..., 1 - page)).
        const int hourly_pi = 1 - page;                 // 0 = 5 prochaines heures, 1 = les 5 suivantes
        const std::string& debut = cal_heures_data[hourly_pi * 5].heure_texte;
        const std::string& fin   = cal_heures_data[hourly_pi * 5 + 4].heure_texte;
        if (!debut.empty() && !fin.empty()) {
            // Plage a cheval sur minuit (22:00 -> 02:00) : sans mention explicite
            // le titre se lirait comme une plage a rebours.
            const bool lendemain = atoi(fin.c_str()) < atoi(debut.c_str());
            snprintf(buf, sizeof(buf), tr("De %s \xC3\xA0 %s%s"), debut.c_str(), fin.c_str(),
                     lendemain ? tr(" le lendemain") : "");
            plage = buf;
        }
        return !plage.empty();
    }

    return false;
}

// Niveau d'une alerte HA ou du bandeau info : 2 = Rouge, 1 = Orange, 0 = normal.
static uint8_t ha_alert_niveau(const std::string& couleur) {
    if (couleur.find("Rouge") != std::string::npos) return 2;
    if (couleur.find("Orange") != std::string::npos) return 1;
    return 0;
}

// Labels du bandeau central : palette UIBandeau.
static uint32_t ha_alert_couleur_niveau(uint8_t niveau) {
    if (niveau == 2) return UIBandeau.ALERT_RED;
    if (niveau == 1) return UIBandeau.WARNING;
    return UIBandeau.TEXT_PRIMARY;
}

// Dernière vigilance rouge montrée en premier (update_info_text_ui) : une seule fois
// par identifiant, pas à chaque poussée.
static std::string s_info_rouge_vue;

// Thèmes (ADR-0029, lot 2) : le niveau posé sur chaque label coloré par un niveau (4
// alertes HA, bandeau info) et le label de la réponse vocale, pour les repeindre au
// changement de thème (central_rejouer_theme()).
struct LabelNiveau {
    lv_obj_t* lbl = nullptr;
    uint8_t niveau = 0;
};
static LabelNiveau s_label_niveau[kHaAlertSlotCount + 1];  // alertes HA 0..3, puis info
static lv_obj_t* s_lbl_vocal = nullptr;

static void colorer_niveau(int index, lv_obj_t* lbl, const std::string& couleur) {
    const uint8_t niveau = ha_alert_niveau(couleur);
    s_label_niveau[index] = LabelNiveau{lbl, niveau};
    ui_text_color(lbl, ha_alert_couleur_niveau(niveau));
}

void central_rejouer_theme() {
    for (const LabelNiveau& l : s_label_niveau) {
        if (l.lbl != nullptr) ui_text_color(l.lbl, ha_alert_couleur_niveau(l.niveau));
    }
    if (s_lbl_vocal != nullptr) ui_text_color(s_lbl_vocal, UIBandeau.TEXT_PRIMARY);
}

// Les 8 panneaux du rotateur, rangés par index (0 planning, 1 pluie, 2 vigilance
// MF, 3 info, 4-7 alertes HA) : une seule liste pour les accès par index et les
// boucles « tout masquer » / « tout arrêter » (audit du 26/09/2026, lot 7.1).
static std::array<lv_obj_t*, kCentralPanelCount> central_wraps(const CentralPanelCtx& ctx) {
    return {ctx.planning_wrap, ctx.rain_wrap, ctx.alert_cont, ctx.info_wrap,
            ctx.ha_wrap[0], ctx.ha_wrap[1], ctx.ha_wrap[2], ctx.ha_wrap[3]};
}
static constexpr int kInfoPanel = 3;  // info_wrap dans central_wraps()

static lv_obj_t* central_panel_wrapper(int panel, CentralPanelCtx& ctx) {
    if (panel < 0 || panel >= kCentralPanelCount) return nullptr;
    return central_wraps(ctx)[panel];
}

// Même ordre que central_wraps() ; le planning (0) est actif sauf quand HA n'a pas
// d'agenda de travail (zone PLANNING, lot 5).
static bool central_panel_is_active(int panel, const CentralPanelCtx& ctx) {
    if (panel < 0 || panel >= kCentralPanelCount) return false;
    const bool active[kCentralPanelCount] = {!ctx.planning_off, ctx.has_rain, ctx.has_mf_alerts, ctx.has_info,
                                             ctx.has_ha[0], ctx.has_ha[1], ctx.has_ha[2], ctx.has_ha[3]};
    return active[panel];
}

// Met un panneau sur la carte : avec la transition du rotateur s'il a la carte, sinon
// l'index seulement (le panneau s'affichera quand la carte lui reviendra).
static void aller_au_panneau(int panel, CentralPanelCtx& ctx) {
    if (panel == ctx.current_panel) return;
    if (!rotator_owns_card(ctx)) {
        ctx.current_panel = panel;  // index seulement : la carte est occupée
        return;
    }

    lv_obj_t* out_obj = central_panel_wrapper(ctx.current_panel, ctx);
    lv_obj_t* in_obj = central_panel_wrapper(panel, ctx);
    transition_widgets(out_obj, in_obj);
    ctx.current_panel = panel;
}

void advance_central_panel_rotator(CentralPanelCtx& ctx) {
    int next_panel = ctx.current_panel;
    int attempts = 0;
    while (attempts < kCentralPanelCount) {
        next_panel = (next_panel + 1) % kCentralPanelCount;
        if (central_panel_is_active(next_panel, ctx)) break;
        attempts++;
    }
    aller_au_panneau(next_panel, ctx);
}

// Une alerte rouge qui arrive passe en premier (plan des alertes du 06/10/2026) : elle
// prend la carte tout de suite au lieu d'attendre son tour, puis tourne avec le reste
// (elle ne bloque pas la carte : la météo et la pluie restent visibles). Vrai quand la
// carte vient de changer sous les yeux : l'appelant relance le minuteur du rotateur,
// pour qu'elle reste un tour entier.
static void sync_central_panel_visibility(CentralPanelCtx& ctx);  // plus bas

// Popup ouvert : le panneau est posé sans transition (le rotateur, lui, ne tourne pas
// sous un popup), et la carte est juste resynchronisée sous le voile.
static bool alerte_rouge_en_tete(int panel, CentralPanelCtx& ctx) {
    if (!central_panel_is_active(panel, ctx) || ctx.current_panel == panel) return false;
    if (ModalRegistry::any_popup_visible()) {
        ctx.current_panel = panel;
        sync_central_panel_visibility(ctx);
        return false;
    }
    aller_au_panneau(panel, ctx);
    return rotator_owns_card(ctx);
}

// Coupe l'animation d'un panneau (transition du rotateur, entrée d'un bandeau d'alerte)
// et le remet à sa place, opaque. lv_anim_delete() seul ne pose pas la valeur finale
// (lv_anim.c, LVGL 9.5) : un panneau coupé en route restait décalé et à demi
// transparent (tap sur une température pendant les 190 ms d'une rotation).
// Ne touche pas au drapeau HIDDEN.
static void couper_animation(lv_obj_t* wrap) {
    if (!wrap) return;
    lv_anim_delete(wrap, nullptr);
    lv_obj_set_pos(wrap, 0, 0);
    lv_obj_set_style_opa(wrap, LV_OPA_COVER, LV_PART_MAIN);
    transition_couper(wrap);  // la transition du rotateur anime le contenu du panneau
}

static void hide_central_panel(lv_obj_t* wrap) {
    if (!wrap) return;
    // Couper la transition en cours : son callback de fin (anim_out_contenu_ready_cb)
    // masque le panneau sortant, y compris quand la synchro qui suit vient de le
    // réafficher — la carte restait vide jusqu'au tour suivant du rotateur (8 s).
    lv_obj_add_flag(wrap, LV_OBJ_FLAG_HIDDEN);
    couper_animation(wrap);
}

static void sync_central_panel_visibility(CentralPanelCtx& ctx) {
    // Panneau courant devenu inactif : le premier actif dans l'ordre des index, ou 0
    // s'il n'y en a aucun (0 est alors inactif lui aussi : planning absent, lot 5).
    if (!central_panel_is_active(ctx.current_panel, ctx)) {
        ctx.current_panel = 0;
        for (int p = 0; p < kCentralPanelCount; p++) {
            if (central_panel_is_active(p, ctx)) {
                ctx.current_panel = p;
                break;
            }
        }
    }
    // Carte occupée (titre de page, planning temporaire, réponse vocale) : on ne
    // touche à rien de visible — masquer ici effacerait même le planning du tap.
    if (!rotator_owns_card(ctx)) return;

    for (lv_obj_t* w : central_wraps(ctx)) hide_central_panel(w);

    // Rien à montrer (sans planning, lot 5) : la carte reste vide.
    if (!central_panel_is_active(ctx.current_panel, ctx)) return;
    lv_obj_t* active = central_panel_wrapper(ctx.current_panel, ctx);
    if (active) lv_obj_remove_flag(active, LV_OBJ_FLAG_HIDDEN);
}

void central_planning_set_off(bool off) {
    if (g_central_ctx.planning_off == off) return;
    g_central_ctx.planning_off = off;
    sync_central_panel_visibility(g_central_ctx);
}

// Pluie (1) et vigilance (2) : le drapeau suit les données poussées par HA. Posé seul,
// il laissait le panneau affiché (barres vides, aucune icône) jusqu'au tour suivant du
// rotateur, et une carte vide le restait jusqu'au tour suivant.
static void central_panneau_actif(int panel, bool actif, bool& drapeau, CentralPanelCtx& ctx) {
    if (drapeau == actif) return;
    const bool carte_vide = !central_panel_is_active(ctx.current_panel, ctx);
    drapeau = actif;
    if (!actif) {
        if (ctx.current_panel != panel) return;
        // Le suivant, avec la transition du rotateur ; aucun autre panneau actif :
        // la synchro vide la carte.
        advance_central_panel_rotator(ctx);
        if (ctx.current_panel == panel) sync_central_panel_visibility(ctx);
    } else if (carte_vide) {
        sync_central_panel_visibility(ctx);
    }
}

void central_set_pluie(bool actif) {
    central_panneau_actif(1, actif, g_central_ctx.has_rain, g_central_ctx);
}

void central_set_vigilance(bool actif) {
    central_panneau_actif(2, actif, g_central_ctx.has_mf_alerts, g_central_ctx);
}

static void clear_ha_alert_slot(HaAlertSlotUI& slot) {
    if (slot.id_store) slot.id_store->clear();
    if (slot.lbl) {
        lv_label_set_recolor(slot.lbl, false);
        lv_label_set_text(slot.lbl, "");
    }
    if (slot.cpt) lv_obj_add_flag(slot.cpt, LV_OBJ_FLAG_HIDDEN);
    if (slot.wrap) {
        lv_obj_add_flag(slot.wrap, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_x(slot.wrap, 0);  // Reset X (animate_alert_enter peut avoir laisse un offset)
        lv_obj_set_style_opa(slot.wrap, LV_OPA_COVER, LV_PART_MAIN);
    }
}

// Libellés codés (lot 4c, 27/09/2026), composés dans la langue de la tablette :
// « @maj:<titre> » → « 1 MAJ · <titre> », « @indispo:<n> » → « <n> indispo »,
// « @vigi:<niveau> » → « Vigilance Rouge » (historique des alertes, lot 4 du
// 06/10/2026). Tout autre libellé (nom d'un capteur en erreur, ancien package HA)
// s'affiche tel quel.
std::string ha_alerte_texte(const char* brut) {
    char tmp[200];
    if (strncmp(brut, "@maj:", 5) == 0) {
        snprintf(tmp, sizeof(tmp), tr("1 MAJ · %s"), brut + 5);
        return normalize_text_utf8(tmp);
    }
    if (strncmp(brut, "@indispo:", 9) == 0) {
        snprintf(tmp, sizeof(tmp), tr("%d indispo"), atoi(brut + 9));
        return tmp;
    }
    if (strncmp(brut, "@vigi:", 6) == 0) {
        const char* niveau = brut + 6;
        if (strcmp(niveau, "Rouge") == 0) return tr("Vigilance Rouge");
        if (strcmp(niveau, "Orange") == 0) return tr("Vigilance Orange");
        if (strcmp(niveau, "Jaune") == 0) return tr("Vigilance Jaune");
        return normalize_text_utf8(niveau);
    }
    return normalize_text_utf8(brut);
}

bool parse_and_update_ha_alerts_bulk(const std::string& payload, HaAlertSlotUI slots[4],
    CentralPanelCtx& ctx, std::string& dismissed_local) {

    // 1E : Sauvegarde des IDs precedents pour detecter les nouvelles alertes.
    std::string prev_ids[4];
    for (int i = 0; i < kHaAlertSlotCount; i++) {
        if (slots[i].id_store) prev_ids[i] = *slots[i].id_store;
    }
    int new_alert_slot = -1;
    int new_red_slot = -1;

    // Rejet AVANT de vider les slots : vidés puis rejetés, ils restaient vides alors
    // que le YAML recopiait les anciens has_ha = true — le rotateur montrait des
    // panneaux vides et le tap d'acquittement n'avait plus d'id (audit 25/09, §2.4).
    if (payload.length() > 1024) {
        ESP_LOGE("TAB5", "Payload alertes HA trop long (%d octets).", (int) payload.length());
        return false;
    }

    for (int i = 0; i < kHaAlertSlotCount; i++) {
        ctx.has_ha[i] = false;
        clear_ha_alert_slot(slots[i]);
    }

    if (payload.empty()) {
        sync_central_panel_visibility(ctx);
        return false;
    }

    char buf[1025];
    strncpy(buf, payload.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int slot_idx = 0;
    // En-tête « @n:N » (alertes du 06/10/2026, lot 3) : HA a N alertes à lire en tout,
    // plus que les 4 bandeaux. Un jeton d'un seul champ : l'ancien firmware l'ignore.
    int a_lire = 0;
    int masquees_ici = 0;  // tapées sur la dalle, pas encore retirées par HA
    std::vector<std::string> ids_seen;
    char* saveptr1 = nullptr;
    char* token = strtok_r(buf, ";", &saveptr1);
    while (token != nullptr && slot_idx < kHaAlertSlotCount) {
        if (strncmp(token, "@n:", 3) == 0) {
            a_lire = atoi(token + 3);
            token = strtok_r(nullptr, ";", &saveptr1);
            continue;
        }
        char* parts[3];
        const int num_parts = split_fields(token, '|', parts, 3);
        if (num_parts >= 3 && slots[slot_idx].wrap && slots[slot_idx].lbl && slots[slot_idx].id_store) {
            std::string aid = parts[0];
            ids_seen.push_back(aid);
            if (tab5_dismiss_local_has(dismissed_local, aid)) {
                masquees_ici++;
                token = strtok_r(nullptr, ";", &saveptr1);
                continue;
            }
            *slots[slot_idx].id_store = aid;
            std::string texte = ha_alerte_texte(parts[2]);
            ctx.has_ha[slot_idx] = !texte.empty();
            colorer_niveau(slot_idx, slots[slot_idx].lbl, parts[1]);
            lv_label_set_recolor(slots[slot_idx].lbl, false);
            ui_text(slots[slot_idx].lbl, texte.c_str());
            // 1E : Detecte si cette alerte est nouvelle (ID absent du precedent batch).
            bool is_new = true;
            for (int j = 0; j < kHaAlertSlotCount; j++) {
                if (prev_ids[j] == aid) { is_new = false; break; }
            }
            if (is_new && new_alert_slot < 0) new_alert_slot = slot_idx;
            if (is_new && new_red_slot < 0 && ctx.has_ha[slot_idx] && ha_alert_niveau(parts[1]) == 2) {
                new_red_slot = slot_idx;
            }
            slot_idx++;
        }
        token = strtok_r(nullptr, ";", &saveptr1);
    }

    tab5_dismiss_local_prune(dismissed_local, ids_seen);

    // Compteur « k/N » : seulement quand des alertes à lire attendent derrière les
    // bandeaux affichés (celles tapées ici ne comptent plus).
    const int total = a_lire - masquees_ici;
    if (total > slot_idx) {
        char cpt[16];
        for (int i = 0; i < slot_idx; i++) {
            if (!slots[i].cpt) continue;
            snprintf(cpt, sizeof(cpt), "%d/%d", i + 1, total);
            ui_text(slots[i].cpt, cpt);
            lv_obj_remove_flag(slots[i].cpt, LV_OBJ_FLAG_HIDDEN);
        }
    }

    sync_central_panel_visibility(ctx);

    // Une alerte rouge nouvelle prend la carte tout de suite (sa transition tient lieu
    // d'entrée animée).
    if (new_red_slot >= 0 && alerte_rouge_en_tete(kHaAlertPanelBase + new_red_slot, ctx)) return true;

    // 1E : Anime l'entree du bandeau si une nouvelle alerte est active — seulement si le
    // rotateur a la carte : l'animation démasque le bandeau, qui passait sinon par-dessus
    // le titre de page (ou de pièce, en mode HA).
    if (new_alert_slot >= 0 && rotator_owns_card(ctx)) {
        int alert_panel = kHaAlertPanelBase + new_alert_slot;
        if (ctx.current_panel == alert_panel && slots[new_alert_slot].wrap) {
            animate_alert_enter(slots[new_alert_slot].wrap);
        }
    }
    // Alerte rouge nouvelle arrivée sur le panneau déjà affiché : un tour entier aussi.
    return new_red_slot >= 0 && rotator_owns_card(ctx) && !ModalRegistry::any_popup_visible() &&
           ctx.current_panel == kHaAlertPanelBase + new_red_slot;
}

// Tap d'acquittement (info, alerte HA ; drapeau déjà baissé par l'appelant) : le
// panneau quitte la carte sans attendre HA. Affiché → le suivant, avec la transition
// du rotateur ; sinon une synchro (sans effet visible si la carte est occupée).
static void retirer_panneau(int panel, lv_obj_t* lbl, lv_obj_t* wrap, CentralPanelCtx& ctx) {
    if (lbl) {
        lv_label_set_recolor(lbl, false);
        lv_label_set_text(lbl, "");
    }
    if (wrap) lv_obj_add_flag(wrap, LV_OBJ_FLAG_HIDDEN);
    if (ctx.current_panel == panel) {
        advance_central_panel_rotator(ctx);
    } else {
        sync_central_panel_visibility(ctx);
    }
}

void dismiss_central_info_immediate(lv_obj_t* lbl_info, CentralPanelCtx& ctx) {
    ctx.has_info = false;
    retirer_panneau(kInfoPanel, lbl_info, ctx.info_wrap, ctx);
}

void dismiss_ha_alert_slot_immediate(int slot_idx, lv_obj_t* wrap, lv_obj_t* lbl,
    std::string& id_store, CentralPanelCtx& ctx) {

    if (slot_idx < 0 || slot_idx >= kHaAlertSlotCount) return;
    id_store.clear();
    ctx.has_ha[slot_idx] = false;
    retirer_panneau(kHaAlertPanelBase + slot_idx, lbl, wrap, ctx);
}

// « Tout marquer comme lu » (popup Alertes, lot 4) : mêmes gestes qu'un tap sur chaque
// bandeau (tab5_dismiss_ha_alert, tab5_dismiss_info_tap), sans l'événement : le script
// tab5_alertes_tout_lu envoie un seul alert_id « * ».
void central_tout_marquer_lu(HaAlertSlotUI slots[4], lv_obj_t* lbl_info, const std::string& info_id,
                             std::string& dismissed_local, CentralPanelCtx& ctx) {
    for (int i = 0; i < kHaAlertSlotCount; i++) {
        HaAlertSlotUI& s = slots[i];
        if (s.id_store == nullptr || s.id_store->empty()) continue;
        tab5_dismiss_local_add(dismissed_local, *s.id_store);
        dismiss_ha_alert_slot_immediate(i, s.wrap, s.lbl, *s.id_store, ctx);
    }
    if (ctx.has_info && !info_id.empty()) {
        tab5_dismiss_local_add(dismissed_local, info_id);
        dismiss_central_info_immediate(lbl_info, ctx);
    }
}

// Pose le titre sans rien decider de la visibilite, dans la police de la date
// (style_police_date, tab5-lvgl.yaml). Pages de previsions : une ligne, la plage des
// tuiles. Mode HA : chapeau discret (roboto_22 attenue, « Pièce n/N ») + nom de la
// piece dessous ; sans nom, le chapeau prend la ligne principale et se recentre.
// Renvoie false quand la page n'a pas de titre (accueil) : rien n'est ecrit.
static bool set_forecast_page_title_text(int forecast_page, lv_obj_t* lbl_page_title,
                                         CentralPanelCtx& ctx) {
    std::string chapeau, plage;
    // Mode HA : « Pièce n/N » et le nom de la pièce (tab5_tuiles.cpp), sur toutes les pages.
    const bool a_titre = ctx.ha_mode ? tuiles_titre_piece(chapeau, plage)
                                     : forecast_page_title_parts(forecast_page, plage);
    if (!a_titre) return false;

    const bool deux_lignes = !chapeau.empty() && !plage.empty();
    if (ctx.page_title_sub) {
        lv_label_set_recolor(ctx.page_title_sub, false);
        lv_label_set_text(ctx.page_title_sub, deux_lignes ? chapeau.c_str() : "");
    }
    lv_label_set_recolor(lbl_page_title, false);
    lv_label_set_text(lbl_page_title, plage.empty() ? chapeau.c_str() : plage.c_str());
    lv_obj_align(lbl_page_title, LV_ALIGN_CENTER, 0, deux_lignes ? 13 : 0);
    return true;
}

void update_central_forecast_page_ui(int forecast_page,
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, CentralPanelCtx& ctx) {

    if (!page_title_wrap || !lbl_page_title) return;

    for (lv_obj_t* w : central_wraps(ctx))
        if (w) lv_obj_add_flag(w, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN);

    if (forecast_page == 2 && !ctx.ha_mode) {
        // Panneau courant inactif (planning retiré, lot 5) : la synchro choisit le
        // suivant, ou laisse la carte vide.
        if (!central_panel_is_active(ctx.current_panel, ctx)) {
            sync_central_panel_visibility(ctx);
            return;
        }
        lv_obj_t* active = central_panel_wrapper(ctx.current_panel, ctx);
        if (active) lv_obj_remove_flag(active, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    if (!set_forecast_page_title_text(forecast_page, lbl_page_title, ctx)) return;
    lv_obj_remove_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN);
}

void refresh_forecast_page_title_ui(int forecast_page,
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, CentralPanelCtx& ctx) {

    if (!page_title_wrap || !lbl_page_title) return;
    // No-op si le titre n'est pas a l'ecran (accueil, planning temporaire 6 s,
    // reponse vocale) : un push HA ne doit jamais reprendre la carte centrale a
    // ce qui l'occupe. On se contente de reecrire le texte, sans toucher a la
    // visibilite des panneaux — contrairement a update_central_forecast_page_ui().
    // En mode HA, le titre est celui de la pièce : les prévisions n'y touchent pas.
    if (ctx.ha_mode || lv_obj_has_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN)) return;
    set_forecast_page_title_text(forecast_page, lbl_page_title, ctx);
}

// Bandeau info codé (lot 4c, 27/09/2026). HA n'envoie plus de texte français mais
// des compteurs, composés ici dans la langue de la tablette :
//   @ha|<nb MAJ>|<titre de la MAJ s'il n'y en a qu'une>|<nb erreurs>|<nb indispo>|<jaune 0/1>|<vigilance>
// vigilance = « rouge », « orange » ou vide (vigilance non acquittée côté HA).
// Le bandeau « Alerte Météo … » ne dépend plus de la COULEUR : avec l'ancien texte,
// une MAJ HA (couleur Orange) ou une erreur (Rouge) affichaient à tort ce bandeau
// quand la vigilance était verte. Français identique à l'ancien modèle HA.
static std::string compose_info_code(const std::string& code, const std::string& meteo_id,
                                     const std::string& dismissed_local) {
    char buf[256];
    snprintf(buf, sizeof(buf), "%s", code.c_str() + 4);  // après « @ha| »
    char* f[6];
    const int n = split_fields(buf, '|', f, 6);
    auto champ = [&](int i) -> const char* { return i < n ? f[i] : ""; };
    const int nb_maj = atoi(champ(0));
    const char* titre = champ(1);
    const int nb_err = atoi(champ(2));
    const int nb_indispo = atoi(champ(3));
    const bool jaune = atoi(champ(4)) != 0;
    const char* vigi = champ(5);
    const bool meteo_vue = !meteo_id.empty() && tab5_dismiss_local_has(dismissed_local, meteo_id);
    if (!meteo_vue) {
        if (strcmp(vigi, "rouge") == 0) return vigilance_alert_banner_utf8("Rouge");
        if (strcmp(vigi, "orange") == 0) return vigilance_alert_banner_utf8("Orange");
    }
    std::string ligne;
    char tmp[192];
    auto ajoute = [&](const char* partie) {
        if (!ligne.empty()) ligne += " · ";
        ligne += partie;
    };
    if (nb_maj == 1) {
        snprintf(tmp, sizeof(tmp), tr("1 MAJ · %s"), titre);
        ajoute(tmp);
    } else if (nb_maj > 1) {
        snprintf(tmp, sizeof(tmp), tr("%d MAJ"), nb_maj);
        ajoute(tmp);
    }
    if (nb_err == 1) {
        ajoute(tr("1 erreur"));
    } else if (nb_err > 1) {
        snprintf(tmp, sizeof(tmp), tr("%d erreurs"), nb_err);
        ajoute(tmp);
    }
    if (nb_indispo > 0) {
        snprintf(tmp, sizeof(tmp), tr("%d indispo"), nb_indispo);
        ajoute(tmp);
    }
    if (jaune && !meteo_vue) {
        std::string t = tr("Vigilance Jaune");
        if (!ligne.empty()) t += " · " + ligne;
        return t;
    }
    return ligne;
}

bool update_info_text_ui(lv_obj_t* lbl_info, lv_obj_t* info_wrap, lv_obj_t* planning_wrap,
    const std::string& texte, const std::string& couleur, const std::string& meteo_id,
    std::string& dismissed_local, CentralPanelCtx& ctx,
    esphome::font::Font* font_small) {

    if (!lbl_info) return false;

    std::string t = trim_ws(texte);

    if (t.rfind("@ha|", 0) == 0) {
        // Codé (package HA du lot 4c et après) : texte composé ici.
        t = normalize_text_utf8(compose_info_code(t, meteo_id, dismissed_local));
    } else if (const char* banner = vigilance_alert_banner_utf8(couleur)) {
        // Ancien package HA : bannière vigilance selon la couleur (comportement d'avant).
        if (!meteo_id.empty() && tab5_dismiss_local_has(dismissed_local, meteo_id)) {
            if (t.empty()) {
                ctx.has_info = false;
                lv_label_set_recolor(lbl_info, false);
                lv_label_set_text(lbl_info, "");
                return false;
            }
            t = normalize_text_utf8(t);
        } else {
            t = banner;
        }
    } else if (!t.empty()) {
        t = normalize_text_utf8(t);
    }

    ctx.has_info = !t.empty();
    if (t.empty()) {
        lv_label_set_text(lbl_info, "");
        if (ctx.current_panel == kInfoPanel && info_wrap && planning_wrap) {
            if (ctx.planning_off) {
                // Pas de planning (lot 5) : le panneau actif suivant, ou une carte vide.
                sync_central_panel_visibility(ctx);
                return false;
            }
            // Transition visible seulement si le rotateur a la carte : sinon elle
            // faisait surgir le panneau planning par-dessus le titre de page.
            if (rotator_owns_card(ctx)) transition_widgets(info_wrap, planning_wrap);
            ctx.current_panel = 0;
        }
        return false;
    }

    bool multi_ligne = t.find('\n') != std::string::npos;
    bool has_recolor_markup = has_lvgl_recolor_markup(t);
    // Une ligne : la police de la date du thème (style_police_date du label) ; deux
    // lignes ne tiennent qu'en 32 px.
    ui_police(lbl_info, multi_ligne ? font_small : nullptr);

    // Même règle de couleur que les bandeaux d'alertes HA (Rouge, Orange, sinon blanc).
    colorer_niveau(kHaAlertSlotCount, lbl_info, couleur);

    lv_label_set_recolor(lbl_info, has_recolor_markup);
    lv_label_set_text(lbl_info, t.c_str());

    // Vigilance rouge nouvelle (son identifiant change avec le niveau ou les phénomènes) :
    // elle passe en premier, comme un bandeau d'alerte rouge.
    if (ha_alert_niveau(couleur) != 2 || meteo_id.empty() || meteo_id == s_info_rouge_vue ||
        tab5_dismiss_local_has(dismissed_local, meteo_id)) {
        return false;
    }
    s_info_rouge_vue = meteo_id;
    // Déjà sur la carte : elle y reste un tour entier aussi.
    if (ctx.current_panel == kInfoPanel) return rotator_owns_card(ctx) && !ModalRegistry::any_popup_visible();
    return alerte_rouge_en_tete(kInfoPanel, ctx);
}

// -----------------------------------------------------------------------------
// Phrase pluie composée ici (lot 4c, 27/09/2026)
// -----------------------------------------------------------------------------
// HA envoyait une phrase française toute faite (« Averses dans 12 mn ») et la
// renvoyait chaque minute pour le décompte. Il envoie désormais un code
// « @niveau,début » :
//   - niveau -1 = pas de données, 0 = temps sec, 1 à 4 = pluie faible, modérée,
//     forte, très forte, 5 = pluie d'intensité inconnue ;
//   - début = heure UTC (epoch) du début de la pluie, 0 s'il pleut déjà ;
//   - « @- » = aucune source de pluie dans l'heure : la phrase reste vide.
// La tablette traduit et décompte elle-même (rain_phrase_tick(), chaque minute). Le
// français est celui de l'ancien modèle HA, à l'octet près. Un texte sans « @ »
// (package HA d'avant le lot 4c) s'affiche tel quel.
namespace {
struct RainPhrase {
    lv_obj_t* lbl = nullptr;
    bool code = false;   // dernier envoi = un code (sinon texte brut, rien à décompter)
    int niveau = 0;      // -2 = aucune source
    int64_t debut = 0;
};
RainPhrase g_rain_phrase;
}  // namespace

// Forte et très forte → « Averses », comme le faisait le modèle HA.
static const char* rain_level_label(int niveau) {
    switch (niveau) {
        case 1: return tr("Pluie faible");
        case 2: return tr("Pluie modérée");
        case 3:
        case 4: return tr("Averses");
        default: return tr("Pluie");
    }
}

static void rain_phrase_render() {
    const RainPhrase& s = g_rain_phrase;
    if (s.lbl == nullptr || !s.code) return;
    char buf[96];
    if (s.niveau < -1) {
        buf[0] = '\0';
    } else if (s.niveau < 0) {
        snprintf(buf, sizeof(buf), "%s", tr("Pas de données"));
    } else if (s.niveau == 0) {
        snprintf(buf, sizeof(buf), "%s", tr("Temps sec"));
    } else {
        const char* label = rain_level_label(s.niveau);
        // Minutes entières, arrondies vers zéro comme le `| int` de l'ancien modèle.
        // Heure pas encore valide (avant NTP et RX8130) : pas de décompte.
        const time_t now = time(nullptr);
        const long minutes = (s.debut > 0 && now > 1600000000) ? (long) ((s.debut - (int64_t) now) / 60) : 0;
        if (minutes <= 0) snprintf(buf, sizeof(buf), "%s", label);
        else snprintf(buf, sizeof(buf), tr("%s dans %ld mn"), label, minutes);
    }
    ui_text(s.lbl, buf);
}

void rain_phrase_tick() { rain_phrase_render(); }

void update_rain_phrase_ui(lv_obj_t* lbl, const std::string& phrase) {
    if (!lbl) return;
    lv_label_set_recolor(lbl, false);
    RainPhrase& s = g_rain_phrase;
    s.lbl = lbl;
    if (!phrase.empty() && phrase[0] == '@') {
        s.code = true;
        if (phrase.size() >= 2 && phrase[1] == '-') {
            s.niveau = -2;
            s.debut = 0;
        } else {
            s.niveau = atoi(phrase.c_str() + 1);
            const char* virgule = strchr(phrase.c_str(), ',');
            s.debut = virgule ? strtoll(virgule + 1, nullptr, 10) : 0;
        }
        rain_phrase_render();
        return;
    }
    s.code = false;
    std::string t = normalize_text_utf8(phrase);
    lv_label_set_text(lbl, t.c_str());
}

// Changer de page ou de mode (météo ↔ HA) met fin aux overlays de la carte centrale
// (audit du 25/09/2026, §2.4) : sans ça, le timer du planning temporaire réaffichait
// 6 s plus tard le titre de la page d'ORIGINE sur la nouvelle page (ou sur la pièce),
// et la réponse vocale restait visible sous le nouveau titre. Le script YAML de la
// réponse vocale finit son délai sans effet visible (masquer un objet masqué ; la
// synchro ne réaffiche que si le rotateur a la main). L'appelant pose ensuite
// l'occupant de la nouvelle page (update_central_forecast_page_ui).
static void terminer_reponse_vocale(CentralPanelCtx& ctx) {
    if (!ctx.vocal_shown) return;
    if (ctx.vocal_wrap) lv_obj_add_flag(ctx.vocal_wrap, LV_OBJ_FLAG_HIDDEN);
    ctx.vocal_shown = false;
}

static void liberer_carte(CentralPanelCtx& ctx) {
    end_temporary_planning(ctx);
    terminer_reponse_vocale(ctx);
}

// Applique une page de previsions : donnees, calque, pastilles, carte centrale.
// Factorise entre les deux seules facons de changer de page — le swipe manuel
// (handle_swipe_gesture) et le retour automatique d'inactivite
// (reset_forecast_to_main_page) — pour qu'elles laissent l'ecran dans
// exactement le meme etat. `dir` ne sert qu'a l'animation de changement de
// calque (horaire <-> journalier).
static void apply_forecast_page(int old_page, int page, lv_dir_t dir,
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_card, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5],
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    CentralPanelCtx& ctx) {

        liberer_carte(ctx);
        ctx.forecast_page = page;

        // Detection de changement de layer (horaire <-> journalier).
        // L'animation de swipe horizontal n'a de sens que lors d'un changement de layer.
        bool old_is_daily = (old_page >= 2);
        bool new_is_daily = (page >= 2);

        if (old_is_daily != new_is_daily) {
            // Changement de layer : refresh des donnees AVANT l'animation,
            // puis animation du glissement horizontal + fondu croise.
            // Le calque entier glisse deja : pas de rouleau d'icone en plus
            // (deux animations sur la meme zone = bruit visuel + repaint double).
            g_forecast_roll_suppress = true;
            if (new_is_daily) {
                refresh_daily_forecast(day_slots, page - 2, f_card, f_card_s);
            } else {
                refresh_hourly_forecast(hour_slots, 1 - page, f_card, f_card_s);
            }
            g_forecast_roll_suppress = false;
            lv_obj_t* out_layer = old_is_daily ? layer_forecast_daily : layer_forecast_hourly;
            lv_obj_t* in_layer  = new_is_daily ? layer_forecast_daily : layer_forecast_hourly;
            animate_swipe_horizontal(out_layer, in_layer, dir);
        } else {
            // Meme layer (page intra-journalier ou intra-horaire) : refresh instantane.
            lv_obj_set_flag(layer_forecast_daily, LV_OBJ_FLAG_HIDDEN, !new_is_daily);
            lv_obj_set_flag(layer_forecast_hourly, LV_OBJ_FLAG_HIDDEN, new_is_daily);
            if (new_is_daily) {
                refresh_daily_forecast(day_slots, page - 2, f_card, f_card_s);
            } else {
                refresh_hourly_forecast(hour_slots, 1 - page, f_card, f_card_s);
            }
        }

        pagination_afficher(pbars, page);

        // Épaules et boutons des appareils de la pièce de cette page (ADR-0023).
        tuiles_peindre_meteo();

        update_central_forecast_page_ui(page, page_title_wrap, lbl_page_title, ctx);
}

void pagination_afficher(lv_obj_t* const pbars[5], int page) {
    for (int i = 0; i < 5; i++) {
        if (pbars[i] == nullptr) continue;
        if (i == page) {
            lv_obj_set_width(pbars[i], 30);
            lv_obj_set_style_bg_opa(pbars[i], 255, LV_PART_MAIN);
        } else {
            lv_obj_set_width(pbars[i], 16);
            lv_obj_set_style_bg_opa(pbars[i], 100, LV_PART_MAIN);
        }
    }
}

// [AI-WARNING] NE PAS "corriger" en wrap 0<->4 : comportement volontaire, deja teste et
// valide par Axel (revert du 05/07/2026 d'un changement fait a tort suite a
// un audit LLM qui l'avait signale comme un bug de pagination "confuse").
// Pages 0-1 = horaire, 2-4 = journalier. LEFT boucle sur 2/3/4 une fois
// dans le journalier (ne revient pas seul vers l'horaire) ; RIGHT traverse
// tout vers le bas et boucle 0->2 (retour au debut du journalier, pas un
// tour complet vers 4). Le mode HA (pièces, ADR-0023) suit le même ordre.
int forecast_page_suivante(int page, bool gauche) {
    if (gauche) return page >= 4 ? 2 : page + 1;
    return page <= 0 ? 2 : page - 1;
}

void central_mode_ha(lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title, CentralPanelCtx& ctx) {
    // Comme un changement de page : le planning du tap et la réponse vocale cèdent la
    // carte (sinon le timer de 6 s réaffichait l'ancien titre par-dessus la pièce).
    liberer_carte(ctx);
    update_central_forecast_page_ui(ctx.forecast_page, page_title_wrap, lbl_page_title, ctx);
}

void handle_swipe_gesture(lv_dir_t dir, int32_t pt_y,
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_card, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5],
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    CentralPanelCtx& ctx) {

    // [AI-DEBUG] Un swipe qui ne pagine pas se diagnostique ici : si cette ligne n'apparait
    // pas pendant le geste, LVGL a consomme le drag en scroll et n'a jamais emis
    // LV_EVENT_GESTURE — chercher l'objet scrollable sous le doigt (cf. [AI-WARNING] du
    // panneau titre dans tab5-lvgl.yaml), pas dans cette fonction.
    // Le logger du projet tourne en `level: INFO` (tab5-hardware.yaml) : passer
    // temporairement a DEBUG pour voir cette trace, elle est muette autrement.
    ESP_LOGD("TAB5", "swipe: dir=%d y=%d page=%d", (int) dir, (int) pt_y, ctx.forecast_page);

    if (pt_y < FORECAST_SWIPE_Y_MIN) return;
    if (dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT) return;

    // Mode HA (ADR-0023) : le swipe change de pièce, jamais de calque météo — il
    // réaffichait les prévisions sous les cartes (bug relevé le 28/09/2026).
    if (ctx.ha_mode) {
        tuiles_swipe_ha(dir == LV_DIR_LEFT);
        return;
    }

    const int old_page = ctx.forecast_page;
    // Bouclage volontaire : [AI-WARNING] de forecast_page_suivante() ci-dessus.
    // apply_forecast_page() pose ctx.forecast_page.
    const int page = forecast_page_suivante(old_page, dir == LV_DIR_LEFT);

    apply_forecast_page(old_page, page, dir,
        layer_forecast_daily, layer_forecast_hourly, day_slots, hour_slots,
        f_card, f_card_s, pbars,
        page_title_wrap, lbl_page_title, ctx);
}

void reset_forecast_to_main_page(
    lv_obj_t* layer_forecast_daily, lv_obj_t* layer_forecast_hourly,
    WeatherDaySlot day_slots[5], WeatherHourSlot hour_slots[5],
    esphome::font::Font* f_card, esphome::font::Font* f_card_s,
    lv_obj_t* pbars[5],
    lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
    CentralPanelCtx& ctx) {

    const int old_page = ctx.forecast_page;
    if (old_page == FORECAST_MAIN_PAGE) return;  // deja au panneau principal

    // Sens de l'animation : depuis l'horaire (0/1) le calque journalier arrive
    // par la droite, comme un swipe vers la gauche ; depuis 3/4 on recule, donc
    // swipe vers la droite (sans effet visible : meme calque, refresh direct).
    const lv_dir_t dir = (old_page < FORECAST_MAIN_PAGE) ? LV_DIR_LEFT : LV_DIR_RIGHT;

    apply_forecast_page(old_page, FORECAST_MAIN_PAGE, dir,
        layer_forecast_daily, layer_forecast_hourly, day_slots, hour_slots,
        f_card, f_card_s, pbars,
        page_title_wrap, lbl_page_title, ctx);
}


// =============================================================================
// Planning jour au tap sur tuile météo (carte centrale 6s)
// =============================================================================

static std::string get_day_planning_display_text(int jour) {
    if (jour < 0 || jour >= 15) return tr("Jour hors plage");
    const DayForecastData& d = cal_jours_data[jour];
    const std::string& h = d.heures_ouverture;

    std::string label;
    if (jour == 0) label = tr("Auj.");
    else {
        std::string short_lbl = format_short_day_label(jour);
        label = short_lbl.empty() ? std::string(ha_day_name(d.nom_jour)) : short_lbl;
    }
    if (label.empty()) label = tr("Jour");

    if (!h.empty()) {
        // Embauche tôt (< 9 h) : rôle EARLY de la palette du bandeau (recolor LVGL,
        // « RRGGBB ») ; le reste dans la couleur du libellé.
        if (cal_is_early_shift(h)) {
            char couleur[8];
            snprintf(couleur, sizeof(couleur), "%06X", static_cast<unsigned>(UIBandeau.EARLY & 0xFFFFFFu));
            return tr_fill("{jour} : #{couleur} {horaire}#", {{"jour", label}, {"couleur", couleur}, {"horaire", h}});
        }
        return tr_fill("{jour} : {horaire}", {{"jour", label}, {"horaire", h}});
    }
    if (d.est_repos) return tr_fill("{jour} : repos", {{"jour", label}});
    return tr_fill("{jour} : pas d'horaire", {{"jour", label}});
}

// Contexte de l'affichage temporaire du planning (tap sur une tuile météo, 6 s) :
// tout ce que le timer de restauration doit retrouver, regroupé ici plutôt qu'en
// neuf `static` de fichier (audit du 06/09/2026, §4.2 point 18 — même approche que
// CentralPanelCtx). Une seule instance : un seul affichage temporaire à la fois,
// un nouveau tap remplace le précédent (son timer est supprimé).
struct TempPlanningCtx {
    lv_timer_t* restore_timer = nullptr;   // timer 6 s en cours, nullptr sinon
    std::string plan_l1;                   // bandeau planning à restaurer, ligne 1
    std::string plan_l2;                   // ... et ligne 2 (vide si absente)
    lv_obj_t* lbl_planning = nullptr;
    int forecast_page_restore = 2;         // page prévisions à rétablir (2 = journalière)
    lv_obj_t* page_title_wrap = nullptr;
    lv_obj_t* lbl_page_title = nullptr;
    int central_panel_restore = 0;         // panneau central à rétablir
};
static TempPlanningCtx s_temp_planning;

// Seule source de « planning du tap affiché » : son timer de 6 s tourne. Le global
// ESPHome is_showing_temp_planning, qui recopiait ce timer, est retiré (28/09/2026).
bool temp_planning_active() { return s_temp_planning.restore_timer != nullptr; }

void planning_temporaire_lignes(const std::string& l1, const std::string& l2) {
    if (s_temp_planning.restore_timer == nullptr) return;
    s_temp_planning.plan_l1 = l1;
    s_temp_planning.plan_l2 = l2;
}

// Termine le planning temporaire en cours : supprime le timer, rend le texte normal
// du bandeau et le panneau d'origine. Ne décide PAS de la visibilité — l'appelant
// la fixe selon la page (timer de 6 s, ou changement de page). Sans effet si aucun
// planning temporaire n'est affiché.
static void end_temporary_planning(CentralPanelCtx& ctx) {
    TempPlanningCtx& tp = s_temp_planning;
    if (tp.restore_timer == nullptr) return;
    lv_timer_delete(tp.restore_timer);
    tp.restore_timer = nullptr;
    ctx.current_panel = tp.central_panel_restore;
    // Texte normal rendu dans tous les cas : un tap sur la page 3 ou 4 laissait le
    // texte du tap dans le bandeau planning, réaffiché tel quel au retour sur 2.
    if (tp.lbl_planning) {
        std::string combined = tp.plan_l1;
        if (!tp.plan_l2.empty()) {
            combined += "   |   " + tp.plan_l2;
        }
        set_label_text_utf8(tp.lbl_planning, combined.c_str());
    }
}

static void planning_restore_timer_cb(lv_timer_t* /*timer*/) {
    // end_temporary_planning() supprime ce timer depuis son propre callback :
    // autorisé par LVGL 9 (comme l'ancien lv_timer_delete(timer) ici même).
    const int page = s_temp_planning.forecast_page_restore;
    end_temporary_planning(g_central_ctx);
    if (page != FORECAST_MAIN_PAGE) {
        update_central_forecast_page_ui(page, s_temp_planning.page_title_wrap,
            s_temp_planning.lbl_page_title, g_central_ctx);
    } else {
        // Réaffiche le panneau rétabli (et masque le planning s'il n'était pas
        // celui d'origine) ; sans effet si une réponse vocale occupe la carte.
        sync_central_panel_visibility(g_central_ctx);
    }
}

// Planning du tap ou réponse vocale prennent la carte : animations coupées sur les 8
// panneaux et le titre (une transition du rotateur en cours masquerait ou déplacerait
// un panneau à sa fin) et panneaux remis à leur place, titre masqué, panneaux masqués
// sauf `garder` (nullptr : tous).
static void prendre_carte(lv_obj_t* page_title_wrap, CentralPanelCtx& ctx, lv_obj_t* garder) {
    if (page_title_wrap) {
        lv_anim_delete(page_title_wrap, nullptr);
        lv_obj_add_flag(page_title_wrap, LV_OBJ_FLAG_HIDDEN);
    }
    for (lv_obj_t* w : central_wraps(ctx)) {
        if (!w) continue;
        lv_obj_set_flag(w, LV_OBJ_FLAG_HIDDEN, w != garder);
        couper_animation(w);
    }
}

void show_temporary_planning(int tuile, lv_obj_t* lbl_planning,
                             lv_obj_t* page_title_wrap, lv_obj_t* lbl_page_title,
                             const std::string& plan_l1, const std::string& plan_l2,
                             CentralPanelCtx& ctx) {
    if (!lbl_planning) return;

    // Jour de la tuile, aligné sur refresh_daily_forecast() : page journalière 2-4
    // (bornée, comme le faisait la lambda de forecast_day_temp_tab.yaml).
    int page = ctx.forecast_page;
    if (page < 2) page = 2;
    if (page > 4) page = 4;
    const int jour = (page - 2) * 5 + tuile;

    TempPlanningCtx& tp = s_temp_planning;
    // Second tap pendant les 6 s : garder le panneau d'ORIGINE, le premier tap a
    // déjà mis le panneau courant à 0 (on restaurait le planning au lieu du
    // panneau d'avant — observation du 08/09/2026).
    if (tp.restore_timer == nullptr) tp.central_panel_restore = ctx.current_panel;
    ctx.current_panel = 0;

    std::string text = get_day_planning_display_text(jour);
    set_label_text_utf8(lbl_planning, text.c_str());

    // Seul le planning reste visible. Une réponse vocale à l'écran se termine, comme au
    // changement de page : elle restait dessous et les deux textes se superposaient.
    // Le script de la réponse finit ses 8 s sans rien réafficher (hide_vocal_response_ui).
    terminer_reponse_vocale(ctx);
    prendre_carte(page_title_wrap, ctx, ctx.planning_wrap);

    tp.plan_l1 = plan_l1;
    tp.plan_l2 = plan_l2;
    tp.lbl_planning = lbl_planning;
    tp.forecast_page_restore = ctx.forecast_page;
    tp.page_title_wrap = page_title_wrap;
    tp.lbl_page_title = lbl_page_title;

    if (tp.restore_timer != nullptr) {
        lv_timer_delete(tp.restore_timer);
        tp.restore_timer = nullptr;
    }

    tp.restore_timer = lv_timer_create(planning_restore_timer_cb, 6000, nullptr);
}

void show_vocal_response_ui(const std::string& texte,
    lv_obj_t* vocal_wrap, lv_obj_t* lbl_vocal,
    lv_obj_t* page_title_wrap, CentralPanelCtx& ctx) {

    if (!vocal_wrap || !lbl_vocal) return;

    const std::string t = trim_ws(normalize_text_utf8(texte));
    if (t.empty()) return;

    lv_anim_delete(vocal_wrap, nullptr);
    prendre_carte(page_title_wrap, ctx, nullptr);

    // Police : celle de la date du thème, portée par le style du label (tab5-lvgl.yaml).
    lv_obj_set_style_text_color(lbl_vocal, lv_color_hex(UIBandeau.TEXT_PRIMARY), LV_PART_MAIN);
    s_lbl_vocal = lbl_vocal;
    lv_label_set_recolor(lbl_vocal, false);

    // Phrase longue : défilement horizontal sur la largeur carte centrale.
    constexpr size_t kScrollMinChars = 42;
    if (t.size() > kScrollMinChars) {
        lv_obj_set_width(lbl_vocal, kLargeurPanneauCentral);
        lv_label_set_long_mode(lbl_vocal, LV_LABEL_LONG_MODE_SCROLL_CIRCULAR);
    } else {
        lv_obj_set_width(lbl_vocal, LV_SIZE_CONTENT);
        lv_label_set_long_mode(lbl_vocal, LV_LABEL_LONG_MODE_CLIP);
    }
    lv_label_set_text(lbl_vocal, t.c_str());

    lv_obj_remove_flag(vocal_wrap, LV_OBJ_FLAG_HIDDEN);
    ctx.vocal_wrap = vocal_wrap;
    ctx.vocal_shown = true;
}

void hide_vocal_response_ui(lv_obj_t* vocal_wrap, lv_obj_t* lbl_vocal, CentralPanelCtx& ctx) {
    if (lbl_vocal) {
        lv_label_set_text(lbl_vocal, "");
        lv_label_set_long_mode(lbl_vocal, LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_width(lbl_vocal, LV_SIZE_CONTENT);
    }
    if (vocal_wrap) lv_obj_add_flag(vocal_wrap, LV_OBJ_FLAG_HIDDEN);
    ctx.vocal_shown = false;

    // Mode HA : la réponse avait masqué le titre de la pièce, il revient.
    if (ctx.ha_mode) {
        update_central_forecast_page_ui(ctx.forecast_page, g_tuiles_ui.titre_cadre, g_tuiles_ui.titre, ctx);
        return;
    }
    // Ne réaffiche un panneau du rotateur que s'il a la main (page 2, pas de
    // planning temporaire) : si l'on a balayé pendant les 8 s, la carte est au titre.
    sync_central_panel_visibility(ctx);
}
