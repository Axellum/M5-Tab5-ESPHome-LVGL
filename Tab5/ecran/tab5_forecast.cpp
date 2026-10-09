/**
 * [AI-CONTEXT]
 * @file tab5_forecast.cpp
 * @role Météo : icônes (update_meteo_icon), couleurs température/humidité, réception
 *       des payloads bulk jours/heures (cal_jours_data / cal_heures_data ; leur lecture est
 *       dans Tab5/socle/tab5_parse.cpp depuis le lot F), rafraîchissement des 5 tuiles
 *       journalières et horaires.
 *       Unité de compilation issue de la scission de tab5_custom.cpp (lot (e) de
 *       l'audit du 06/09/2026, faite le 08/09/2026) : mêmes fonctions, même ordre,
 *       aucune logique modifiée.
 * @regle_absolue Seul point de contact avec l'API LVGL, comme avant : les YAML
 *                n'appellent que des helpers déclarés dans tab5_custom.h. Les
 *                helpers partagés entre unités sont déclarés dans tab5_internal.h.
 * @memory_constraint Pas de std::string dans une boucle de parsing : découper un char* en place.
 *       `split_fields()` (tab5_core.h) garde les champs vides ; `strtok_r` les fusionne.
 */
#include "tab5_custom.h"
#include "tab5_internal.h"
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cmath>
#include <ctime>
#include <cstring>
#include <vector>
#include <map>

// Icône d'une condition HA : glyphe principal (l1) + glyphe secondaire optionnel
// (l2), toujours dessiné DERRIÈRE l1. Table de données (audit du 26/09/2026, lot
// 7.1) à la place de la chaîne de if/else : mêmes appels LVGL pour toutes les
// conditions, connues ou non (preuve par static_assert faite pour le lot).
// Décalages en px de tuile (120 px), déjà ramenés depuis la grosse icône d'origine
// (270 px) par l'ancien calcul (int)(v * 0.4444f) : -45 → -19, -30 → -13. La
// grosse icône centrale et ses polices 270/190 px sont retirées depuis le
// 25/09/2026 (jamais affichées, ~196 Ko de flash) : les tuiles sont le seul usage.
struct MeteoIconSpec {
    const char* cond;      // état HA (comparaison exacte, casse comprise)
    const char* l1;        // glyphe principal (police f_card)
    uint32_t Palette::* l1_color;  // rôle de la palette (suit le thème)
    const char* l2;        // glyphe secondaire, nullptr = aucun (l2 masqué)
    uint32_t Palette::* l2_color;
    bool        l2_small;  // l2 en f_card_s (petit soleil / petite lune)
    int8_t      l2_x, l2_y, l1_y;
};
static constexpr MeteoIconSpec kMeteoIconDefault =  // nuage seul, aussi pour un état inconnu
    {"", MeteoIcon::CLOUD, &Palette::METEO_CLOUD,  nullptr, &Palette::TEXT_PRIMARY, false, 0, 0, 0};
static constexpr MeteoIconSpec kMeteoIcons[] = {
    // cond                  l1                 l1_color                   l2                     l2_color                   petit  l2_x l2_y l1_y
    {"clear-night",          MeteoIcon::MOON,  &Palette::METEO_CELESTIAL, nullptr,               &Palette::TEXT_PRIMARY,    false,   0,   0,   0},
    {"cloudy",               MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     nullptr,               &Palette::TEXT_PRIMARY,    false,   0,   0,   0},
    {"fog",                  MeteoIcon::FOG,   &Palette::METEO_CLOUD,     nullptr,               &Palette::TEXT_PRIMARY,    false,   0,   0,   0},
    // OpenWeatherMap : fumée, poussière, sable, cendres (codes 711/731/751/761/762,
    // const.py de HA 2026.9.4) — un voile, comme le brouillard.
    {"exceptional",          MeteoIcon::FOG,   &Palette::METEO_CLOUD,     nullptr,               &Palette::TEXT_PRIMARY,    false,   0,   0,   0},
    {"Clear",                MeteoIcon::SUNNY, &Palette::METEO_CELESTIAL, nullptr,               &Palette::TEXT_PRIMARY,    false,   0,   0,   0},
    {"sunny",                MeteoIcon::SUNNY, &Palette::METEO_CELESTIAL, nullptr,               &Palette::TEXT_PRIMARY,    false,   0,   0,   0},
    {"partlycloudy",         MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     MeteoIcon::SUNNY,      &Palette::METEO_CELESTIAL, true,  -19, -19,   0},
    {"partlycloudy-night",   MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     MeteoIcon::MOON,       &Palette::METEO_CELESTIAL, true,  -19, -19,   0},
    {"partlycloudy_night",   MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     MeteoIcon::MOON,       &Palette::METEO_CELESTIAL, true,  -19, -19,   0},
    {"hail",                 MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     MeteoIcon::HAIL,       &Palette::METEO_PRECIP,    false,   0,   0, -13},
    {"snowy-rainy",          MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     MeteoIcon::HAIL,       &Palette::METEO_PRECIP,    false,   0,   0, -13},
    {"lightning",            MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     MeteoIcon::THUNDER,    &Palette::METEO_THUNDER,   false,   0,   0, -13},
    {"thunder",              MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     MeteoIcon::THUNDER,    &Palette::METEO_THUNDER,   false,   0,   0, -13},
    {"lightning-rainy",      MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     MeteoIcon::THUNDER,    &Palette::METEO_THUNDER,   false,   0,   0, -13},
    {"pouring",              MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     MeteoIcon::HEAVY_RAIN, &Palette::METEO_PRECIP,    false,   0,   0, -13},
    {"rainy",                MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     MeteoIcon::RAIN,       &Palette::METEO_PRECIP,    false,   0,   0, -13},
    {"snowy",                MeteoIcon::CLOUD, &Palette::METEO_CLOUD,     MeteoIcon::SNOW,       &Palette::METEO_PRECIP,    false,   0,   0, -13},
    {"windy",                MeteoIcon::WIND,  &Palette::METEO_CLOUD,     nullptr,               &Palette::TEXT_PRIMARY,    false,   0,   0,   0},
    {"windy-variant",        MeteoIcon::WIND,  &Palette::METEO_CLOUD,     nullptr,               &Palette::TEXT_PRIMARY,    false,   0,   0,   0},
};

void update_meteo_icon(lv_obj_t* l1_obj, lv_obj_t* l2_obj, const std::string& state, esphome::font::Font* f_card, esphome::font::Font* f_card_s) {
    const MeteoIconSpec* s = &kMeteoIconDefault;
    for (const MeteoIconSpec& e : kMeteoIcons) {
        if (state == e.cond) { s = &e; break; }
    }

    if (l1_obj) {
        lv_obj_remove_flag(l1_obj, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(l1_obj, s->l1);
        lv_obj_set_style_text_color(l1_obj, lv_color_hex(UIColor.*s->l1_color), LV_PART_MAIN);
        lv_obj_set_style_translate_y(l1_obj, s->l1_y, LV_PART_MAIN);
        esphome::lvgl::lv_obj_set_style_text_font(l1_obj, f_card, LV_PART_MAIN);
        if (s->l2 && l2_obj) { lv_obj_move_to_index(l1_obj, -1); }
    }
    if (l2_obj) {
        if (s->l2) {
            lv_obj_remove_flag(l2_obj, LV_OBJ_FLAG_HIDDEN);
            lv_label_set_text(l2_obj, s->l2);
            lv_obj_set_style_text_color(l2_obj, lv_color_hex(UIColor.*s->l2_color), LV_PART_MAIN);
            lv_obj_set_style_translate_x(l2_obj, s->l2_x, LV_PART_MAIN);
            lv_obj_set_style_translate_y(l2_obj, s->l2_y, LV_PART_MAIN);
            esphome::lvgl::lv_obj_set_style_text_font(l2_obj, s->l2_small ? f_card_s : f_card, LV_PART_MAIN);
        } else {
            lv_obj_add_flag(l2_obj, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

// D'un ancrage à l'autre, canal par canal (thèmes, ADR-0029, lot 2). `R` = le type du
// rapport des formules d'avant les rôles (float, ou double pour la dernière tranche
// de température) : même calcul, même troncature, donc le thème d'origine garde ses
// couleurs au niveau près (comparé aux anciennes formules, en float32 / float64, pour
// chaque humidité de 0 à 100 % et chaque température de −20 à 50 °C par 0,05 °C).
template <typename R>
static uint32_t degrade(uint32_t a, uint32_t b, R r) {
    uint32_t out = 0;
    for (int d = 16; d >= 0; d -= 8) {
        const int ca = static_cast<int>((a >> d) & 0xFF);
        const int cb = static_cast<int>((b >> d) & 0xFF);
        out |= static_cast<uint32_t>(static_cast<int>(ca + (cb - ca) * r) & 0xFF) << d;
    }
    return out;
}

// Ancrages : HUM_GRAD_14 (sec) → HUM_GRAD_22 → HUM_GRAD_30 (confort) → HUMIDITY_WET
// (80 %), par paliers de 3 % au-dessus de 30 %.
uint32_t get_humidity_color(float x, const Palette& p) {
    if (!std::isfinite(x)) return p.MOISTURE_NAN;
    const int val = tab5_float_vers_int(x, 0, 100, 0);
    if (val <= 14) return p.HUM_GRAD_14;
    if (val >= 80) return p.HUMIDITY_WET;
    if (val >= 30) {
        float step = floor((val - 30) / 3.0) * 3.0;
        float ratio = step / 50.0;
        return degrade(p.HUM_GRAD_30, p.HUMIDITY_WET, ratio);
    }
    if (val >= 22) {
        float ratio = (val - 22) / 8.0;
        return degrade(p.HUM_GRAD_22, p.HUM_GRAD_30, ratio);
    }
    float ratio = (val - 14) / 8.0;
    return degrade(p.HUM_GRAD_14, p.HUM_GRAD_22, ratio);
}

// Ancrages : TEMP_GRAD_M12 → TEMP_GRAD_0_NEG (0 °C), puis TEMP_GRAD_0_POS → TEMP_GRAD_14
// → TEMP_GRAD_24 → TEMP_GRAD_35, par paliers de 2 °C.
uint32_t get_temperature_color(float t) {
    if (std::isnan(t)) return UIColor.TEMP_NAN;
    if (t <= -12) return UIColor.TEMP_GRAD_M12;
    if (t <= 0) {
        float r = floor((t + 12) / 2.0) * 2.0 / 12.0;
        return degrade(UIColor.TEMP_GRAD_M12, UIColor.TEMP_GRAD_0_NEG, r);
    }
    if (t <= 14) {
        float r = floor(t / 2.0) * 2.0 / 14.0;
        return degrade(UIColor.TEMP_GRAD_0_POS, UIColor.TEMP_GRAD_14, r);
    }
    if (t <= 24) {
        float r = floor((t - 14) / 2.0) * 2.0 / 10.0;
        return degrade(UIColor.TEMP_GRAD_14, UIColor.TEMP_GRAD_24, r);
    }
    float s = floor((t - 24) / 2.0) * 2.0;
    if (s > 11) s = 11;
    return degrade(UIColor.TEMP_GRAD_24, UIColor.TEMP_GRAD_35, s / 11.0);
}

// =============================================================================
// AXE8 (Phase 4) : Helpers de parsing bulk pour previsions meteo
// Centralise le parsing du payload serialise et la mise a jour LVGL
// =============================================================================

// Lecture des payloads : previsions_heures_lire() / previsions_jours_lire()
// (Tab5/socle/tab5_parse.h, lot F de l'audit du 30/09/2026), testées sur PC.
static void parse_and_update_heures_bulk(const std::string& payload) {
    if (payload.empty()) return;
    if (payload_trop_long("tab5.forecast", payload.size(), kPrevisionsMax)) return;  // tampon de pile
    ESP_LOGD("tab5.forecast", "Received heures bulk payload length: %d", payload.length());
    previsions_heures_lire(payload.c_str(), cal_heures_data);
}

bool accept_heures_bulk(const std::string& payload, int forecast_page) {
    const int premier = previsions_premier_creneau(payload.c_str());  // idx du 1er créneau du bloc
    if (premier < 0 || premier >= 15) {
        payload_refuse("tab5.forecast", "heures : premier créneau hors de 0 à 14", payload.size());
        return false;
    }
    const int bloc = premier / 5;
    const auto canal = static_cast<PushChannel>(static_cast<int>(PushChannel::HEURES_0) + bloc);
    if (push_unchanged(canal, payload)) return false;
    parse_and_update_heures_bulk(payload);
    // Rendu inutile quand le calque horaire est masqué : apply_forecast_page()
    // repeint depuis cal_heures_data au changement de page.
    return forecast_page < 2 && bloc == 1 - forecast_page;
}

void parse_and_update_jours_bulk(const std::string& payload) {
    if (payload.empty()) return;
    if (payload_trop_long("tab5.forecast", payload.size(), kPrevisionsMax)) return;  // tampon de pile
    ESP_LOGD("tab5.forecast", "Received jours bulk payload length: %d", payload.length());
    // Le jour 0 date le lot : cal_jours_anchor_day (tab5_core.h).
    previsions_jours_lire(payload.c_str(), cal_jours_data, cal_jours_anchor_day);
}

// =============================================================================
// Prévisions périmées (08/10/2026, demande d'Axel après l'incident du 07-08/10 :
// Météo-France figée de 21 h 04 à 11 h 34, puis indisponible, et la tablette
// montrait les prévisions sans rien dire). Le contrat ne change pas : la tablette
// note l'heure de chaque poussée des prévisions (jours ou heures) et, passé
// kPrevisionsPerimeesMin sans poussée, l'écrit au-dessus des tuiles (« Prévisions
// de 11 h 42 », « Prévisions d'hier 21 h 04 »). En temps normal, rien ne s'affiche.
// Ce que ça voit : HA qui ne pousse plus (HA arrêté, automatisation coupée, liaison
// perdue). Ce que ça ne voit PAS : une source figée que HA continue de pousser
// (Météo-France retire les créneaux passés, le payload change donc chaque heure
// même figé) ; c'est à HA de ne plus pousser une source périmée.
// =============================================================================
namespace {
// HA pousse les prévisions toutes les 10 min (time_pattern /10 de la poussée
// complète, HomeAssistant_Config/packages/tab5_push.yaml) et à chaque reconnexion.
// 30 min = trois poussées manquées de suite : un passage abandonné ou un
// redémarrage de HA (quelques minutes, repoussé au démarrage) ne l'affichent pas.
// Pas 60 : sans aucun client API, la tablette redémarre au bout de 60 min
// (api: reboot_timeout) et perd ses prévisions — à 60, la mention n'apparaîtrait
// presque jamais pendant une panne de HA.
constexpr int kPrevisionsPerimeesMin = 30;

// Dernière poussée des jours ou des heures, l'heure étant valide ; 0 = aucune.
time_t g_previsions_recues = 0;
}  // namespace

void previsions_recues(lv_obj_t* zone, lv_obj_t* lbl, int il_y_a_min) {
    const time_t now = tab5_time_source(nullptr);
    // Heure pas encore réglée (avant SNTP et RX8130) : pas de date à retenir.
    if (tab5_heure_valide(now)) g_previsions_recues = now - (time_t) il_y_a_min * 60;
    previsions_fraicheur_tick(zone, lbl);
}

void previsions_fraicheur_tick(lv_obj_t* zone, lv_obj_t* lbl) {
    if (zone == nullptr || lbl == nullptr) return;
    const time_t recues = g_previsions_recues;
    const time_t now = tab5_time_source(nullptr);
    // Rien jamais reçu, heure invalide ou horloge revenue en arrière : rien à dire
    // (le cas « pas encore de données » a déjà son affichage). Mode HA : les tuiles
    // montrent les appareils, pas les prévisions.
    const bool perimees = recues != 0 && tab5_heure_valide(now) && !g_central_ctx.ha_mode &&
                          now - recues > (time_t) kPrevisionsPerimeesMin * 60;
    if (perimees) {
        struct tm r {}, n {};
        localtime_r(&recues, &r);
        localtime_r(&now, &n);
        const int32_t jours = jour_civil(n.tm_year + 1900, n.tm_mon + 1, n.tm_mday) -
                              jour_civil(r.tm_year + 1900, r.tm_mon + 1, r.tm_mday);
        char buf[80];
        if (jours <= 0) snprintf(buf, sizeof(buf), tr("Prévisions de %d h %02d"), r.tm_hour, r.tm_min);
        else if (jours == 1) snprintf(buf, sizeof(buf), tr("Prévisions d'hier %d h %02d"), r.tm_hour, r.tm_min);
        else snprintf(buf, sizeof(buf), tr("Prévisions vieilles de %d jours"), (int) jours);
        ui_text(lbl, buf);  // le texte d'abord : jamais une image avec l'ancien texte
    }
    ui_hidden(zone, !perimees);
}

// Condition meteo actuellement peinte par tuile (5 jours + 5 heures). Sert a
// ne declencher le rouleau que quand l'icone change vraiment : les payloads HA
// retombent souvent sur la meme condition, et repeindre une icone identique
// coutait deja un invalidate LVGL pour rien.
static char s_day_icon_cond[5][20] = {};
static char s_hour_icon_cond[5][20] = {};

// SAME = la tuile affiche deja cette condition : rien a repeindre (update_meteo_icon
// ne depend que de la condition ; audit du 26/09/2026, lot 3 — avant, les 5 tuiles
// etaient repeintes a chaque rafraichissement, 3 ecritures sur 5 relancant la mise
// en page). FIRST = premier remplissage (au boot les 5 tuiles se peignent d'un coup :
// pas d'animation). CHANGED = nouvelle condition : peinture + rouleau.
enum class IconCond : uint8_t { SAME, FIRST, CHANGED };
static IconCond icon_cond_update(char* cache, const std::string& cond) {
    if (cache[0] != '\0' && strncmp(cache, cond.c_str(), sizeof(s_day_icon_cond[0]) - 1) == 0)
        return IconCond::SAME;
    const bool first = (cache[0] == '\0');
    snprintf(cache, sizeof(s_day_icon_cond[0]), "%s", cond.c_str());
    return first ? IconCond::FIRST : IconCond::CHANGED;
}

void refresh_daily_forecast(WeatherDaySlot slots[], int page_index,
    esphome::font::Font* f_card, esphome::font::Font* f_card_s) {

    if (page_index < 0 || page_index > 2) return;

    for (int i = 0; i < 5; i++) {
        int jour = page_index * 5 + i;
        WeatherDaySlot& slot = slots[i];
        if (!slot.day_lbl) continue;

        DayForecastData& data = cal_jours_data[jour];

        // Titre : page accueil (0) = nom_jour HA traduit ; pages 2-3 = "Lun 16" via SNTP
        if (page_index > 0) {
            std::string date_lbl = format_short_day_label(jour);
            ui_text(slot.day_lbl, date_lbl.empty() ? ha_day_name(data.nom_jour) : date_lbl.c_str());
        } else {
            ui_text(slot.day_lbl, ha_day_name(data.nom_jour));
        }

        lv_obj_remove_flag(slot.max_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_remove_flag(slot.min_lbl, LV_OBJ_FLAG_HIDDEN);

        // Tmin / Tmax colors
        uint32_t cmax = get_temperature_color(data.tmax);
        uint32_t cmin = get_temperature_color(data.tmin);
        // `%x` attend un `unsigned int` ; `uint32_t` est un `long unsigned int`
        // sur cette cible -> -Wformat. La conversion est l'identité (32 bits des
        // deux côtés) et ces valeurs sont des couleurs RGB, bornées à 0xFFFFFF.
        char buftx[64]; snprintf(buftx, sizeof(buftx), "#%06x %.0f# / ", (unsigned) cmax, data.tmax);
        char buftn[64]; snprintf(buftn, sizeof(buftn), " #%06x %.0f# \xC2\xB0", (unsigned) cmin, data.tmin);

        ui_text(slot.max_lbl, data.est_passe ? "-- / " : buftx);
        ui_text(slot.min_lbl, data.est_passe ? "-- \xC2\xB0" : buftn);
        const IconCond ic = icon_cond_update(s_day_icon_cond[i], data.condition);
        if (ic != IconCond::SAME) update_meteo_icon(slot.icon_l1, slot.icon_l2, data.condition, f_card, f_card_s);
        // Rouleau echelonne de gauche a droite (effet vague) — apres
        // update_meteo_icon() qui pose le glyphe et son offset de base.
        if (ic == IconCond::CHANGED) animate_icon_roll_in(slot.icon_l1, slot.icon_l2, i * UIAnim::ROLL_STAGGER);

        // Coloring day names
        uint8_t opa = data.est_passe ? 100 : 255;
        uint32_t col = UIColor.TEXT_PRIMARY;
        const bool is_early = !data.est_repos && cal_is_early_shift(data.heures_ouverture);

        if (jour == 0) col = UIColor.INFO;                                              // Aujourd'hui : cyan info
        else if (data.est_dimanche) col = data.est_repos ? UIColor.WARNING : UIColor.ERROR;
        else if (data.est_repos) col = UIColor.SUCCESS;                                 // Jour de repos : emeraude
        else if (is_early) col = UIColor.EARLY;                                         // Embauche < 9h : orange
        if (data.est_passe) col = UIColor.PAST;                                         // Jour passe : ardoise estompee

        ui_text_color(slot.day_lbl, col);
        lv_obj_set_style_text_opa(slot.day_lbl, opa, LV_PART_MAIN);
        lv_obj_set_style_text_opa(slot.icon_l1, opa, LV_PART_MAIN);
        lv_obj_set_style_text_opa(slot.icon_l2, opa, LV_PART_MAIN);
        lv_obj_set_style_text_opa(slot.max_lbl, opa, LV_PART_MAIN);
        lv_obj_set_style_text_opa(slot.min_lbl, opa, LV_PART_MAIN);
        
        lv_label_set_recolor(slot.max_lbl, true);
        lv_label_set_recolor(slot.min_lbl, true);
        ui_text_color(slot.max_lbl, UIColor.TEXT_PRIMARY);
        ui_text_color(slot.min_lbl, UIColor.TEXT_PRIMARY);
    }
    // Épaules et boutons d'appareil : ceux de la pièce de la page (ADR-0023), posés par
    // tuiles_peindre_meteo() au changement de page — rien à faire ici.
}

void refresh_hourly_forecast(WeatherHourSlot slots[], int page_index,
    esphome::font::Font* f_card, esphome::font::Font* f_card_s) {
    
    if (page_index < 0 || page_index > 2) return;

    for (int i = 0; i < 5; i++) {
        // Slot i on screen (left-to-right) corresponds to time index: page_index * 5 + i,
        // l'heure la plus proche à gauche, comme les jours (discussion #278 ; avant le
        // 03/10/2026 : 4 - i, les heures se lisaient de droite à gauche). Les objets
        // restent nommés h4 (gauche) … h0 (droite) : g_hour_slots[i] = h(4 - i).
        int idx = page_index * 5 + i;
        WeatherHourSlot& slot = slots[i];
        if (!slot.time_lbl) continue;

        HourForecastData& data = cal_heures_data[idx];

        ui_text(slot.time_lbl, data.heure_texte.c_str());
        
        uint32_t c_t = get_temperature_color(data.temp);
        char b_t[32]; snprintf(b_t, sizeof(b_t), "#%06x %.0f#\xC2\xB0", (unsigned) c_t, data.temp);  // cf. -Wformat plus haut
        ui_text(slot.temp_lbl, b_t);
        lv_label_set_recolor(slot.temp_lbl, true);
        ui_text_color(slot.temp_lbl, UIColor.TEXT_PRIMARY);

        char b_p[32];
        if (data.pluvio > 0) {
            // Une décimale sous 10 mm, aucune au-delà : « 12mm » tient dans l'onglet
            // (forecast_hour_card.yaml), « 12.5mm » non.
            snprintf(b_p, sizeof(b_p), data.pluvio < 9.95f ? "%.1fmm" : "%.0fmm", data.pluvio);
            ui_text(slot.prob_lbl, b_p);
            ui_text_color(slot.prob_lbl, UIColor.METEO_PRECIP);
        } else {
            ui_text(slot.prob_lbl, "-");
            ui_text_color(slot.prob_lbl, UIColor.CLIM_TRACK_INACTIVE);
        }

        const IconCond ic = icon_cond_update(s_hour_icon_cond[i], data.condition);
        if (ic != IconCond::SAME) update_meteo_icon(slot.icon_l1, slot.icon_l2, data.condition, f_card, f_card_s);
        if (ic == IconCond::CHANGED) animate_icon_roll_in(slot.icon_l1, slot.icon_l2, i * UIAnim::ROLL_STAGGER);
    }
}

// Thèmes (ADR-0029) : caches d'icônes vidés (la prochaine peinture de chaque tuile les
// repeint, sans rouleau), puis les tuiles du calque affiché si elles ont déjà été peintes.
void forecast_rejouer_theme(esphome::font::Font* f_card, esphome::font::Font* f_card_s) {
    bool jours = false, heures = false;
    for (int i = 0; i < 5; i++) {
        jours |= s_day_icon_cond[i][0] != '\0';
        heures |= s_hour_icon_cond[i][0] != '\0';
        s_day_icon_cond[i][0] = '\0';
        s_hour_icon_cond[i][0] = '\0';
    }
    const int fp = g_central_ctx.forecast_page;
    if (fp >= 2 && jours) refresh_daily_forecast(g_day_slots, fp - 2, f_card, f_card_s);
    else if (fp < 2 && heures) refresh_hourly_forecast(g_hour_slots, 1 - fp, f_card, f_card_s);
}
