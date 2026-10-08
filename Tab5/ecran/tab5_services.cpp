/**
 * [AI-CONTEXT]
 * @file tab5_services.cpp
 * @role Services HA (tab5-api-logic.yaml) : volet, vigilance Météo-France, pluie 1 h,
 *       icône neige/pluie, texte du planning. Logique sortie des lambdas le 08/09/2026
 *       (lot (a)), à l'identique. La cible clim depuis HA est dans tab5_clim.cpp
 *       depuis le 29/09/2026 (ADR-0026).
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
#include <algorithm>
#include <cmath>
#include <ctime>
#include <cstring>
#include <vector>
#include <map>

// -----------------------------------------------------------------------------
// Garde anti-rendu des poussées HA (contrat : tab5_custom.h, PushChannel).
// -----------------------------------------------------------------------------

static uint32_t fnv1a_32(const char* data, size_t len) {
    uint32_t h = 2166136261u;
    for (size_t k = 0; k < len; k++) {
        h ^= static_cast<unsigned char>(data[k]);
        h *= 16777619u;
    }
    return h;
}

bool push_unchanged(PushChannel ch, const char* data, size_t len) {
    struct Last {
        uint32_t hash = 0;
        uint32_t len = 0;
        bool seen = false;
    };
    static Last s_last[static_cast<int>(PushChannel::COUNT)];
    const int i = static_cast<int>(ch);
    if (i < 0 || i >= static_cast<int>(PushChannel::COUNT) || data == nullptr) return false;
    Last& last = s_last[i];
    const uint32_t h = fnv1a_32(data, len);
    const uint32_t n = static_cast<uint32_t>(len);
    if (last.seen && last.hash == h && last.len == n) return true;
    last.hash = h;
    last.len = n;
    last.seen = true;
    return false;
}

bool push_unchanged(PushChannel ch, const std::string& payload) {
    return push_unchanged(ch, payload.data(), payload.size());
}

// -----------------------------------------------------------------------------
// Services HA (tab5-api-logic.yaml) — logique sortie des lambdas le 08/09/2026.
// Le comportement est celui des anciens lambdas, à l'identique ; seules les
// gardes contre les pointeurs nuls ont été étendues à chaque widget.
// -----------------------------------------------------------------------------

// Volet : update_volet_ui() a rejoint les pièces le 28/09/2026 (ADR-0023) — le volet 3.x
// est la tuile 1 de la pièce 0 du mode héritage (tuiles_heritage_volet, tab5_tuiles.cpp).

// Thèmes (ADR-0029, lot 2) : le dernier payload de vigilance et ses widgets, pour
// repeindre la date et les icônes au changement de thème (vigilance_rejouer()).
static std::string s_vigilance_payload;
static VigilanceUI s_vigilance_ui{};

// Fond clair : le jaune officiel ne se lit pas sur du blanc. L'icône passe en pastille
// (fond = couleur du niveau, glyphe à l'encre du thème). Fond sombre : aucune
// propriété posée, comme avant les thèmes.
static void vigilance_pastille(lv_obj_t* slot, bool pastille, uint32_t couleur) {
    if (pastille) {
        lv_obj_set_style_bg_color(slot, lv_color_hex(couleur), LV_PART_MAIN);
        lv_obj_set_style_bg_opa(slot, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_radius(slot, 12, LV_PART_MAIN);
        lv_obj_set_style_pad_all(slot, 6, LV_PART_MAIN);
        return;
    }
    for (lv_style_prop_t prop : {LV_STYLE_BG_COLOR, LV_STYLE_BG_OPA, LV_STYLE_RADIUS, LV_STYLE_PAD_TOP,
                                 LV_STYLE_PAD_BOTTOM, LV_STYLE_PAD_LEFT, LV_STYLE_PAD_RIGHT}) {
        lv_obj_remove_local_style_prop(slot, prop, LV_PART_MAIN);
    }
}

bool parse_and_update_vigilance(const std::string& payload, const VigilanceUI& ui) {
    if (ui.lbl_phrase == nullptr) return false;
    if (&payload != &s_vigilance_payload) {
        s_vigilance_payload = payload;
        s_vigilance_ui = ui;
    }
    // 1024 (était 512) : la phrase de vigilance peut être longue, un payload
    // complet dépassait parfois 512 et tronquait les derniers champs (#T165).
    char buf[1024];
    strncpy(buf, payload.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    // Plus long que le tampon : coupé (les derniers champs manquent, #T165), donc dit.
    if (payload.size() >= sizeof(buf)) payload_refuse("tab5.vigilance", "coupé à 1023 octets", payload.size());

    // strtok_r saute les champs vides consécutifs ("||"), comme l'ancien lambda :
    // un champ vide décalerait les suivants. Contrat HA inchangé — HA envoie
    // toujours "Vert" plutôt qu'une chaîne vide. 13 champs depuis le lot 4c
    // (27/09/2026) : brouillard et feux de forêt en fin de payload, pour les
    // alertes MeteoAlarm qui n'ont pas de case Météo-France. Un payload à 11
    // champs (Météo-France) laisse ces deux cases vides.
    char* saveptr = nullptr;
    const char* fields[13];
    for (int i = 0; i < 13; i++) {
        char* tok = strtok_r(i == 0 ? buf : nullptr, "|", &saveptr);
        fields[i] = tok ? tok : "";
    }
    const char* phrase_pluie = fields[0];
    const char* globale = fields[1];

    update_rain_phrase_ui(ui.lbl_phrase, std::string(phrase_pluie));
    if (ui.lbl_pluie_val != nullptr)  lv_obj_add_flag(ui.lbl_pluie_val, LV_OBJ_FLAG_HIDDEN);
    if (ui.lbl_pluie_unit != nullptr) lv_obj_add_flag(ui.lbl_pluie_unit, LV_OBJ_FLAG_HIDDEN);

    // Couleur de la date (sous l'horloge : palette UIHorloge) selon la vigilance globale.
    uint32_t col_date = UIHorloge.SUCCESS;
    if (strcmp(globale, "Jaune") == 0)       col_date = UIHorloge.ALERT_DATE_YELLOW;
    else if (strcmp(globale, "Orange") == 0) col_date = UIHorloge.ALERT_DATE_ORANGE;
    else if (strcmp(globale, "Rouge") == 0)  col_date = UIHorloge.ALERT_DATE_RED;
    if (ui.lbl_date != nullptr) lv_obj_set_style_text_color(ui.lbl_date, lv_color_hex(col_date), LV_PART_MAIN);

    // Phénomènes, dans l'ordre du payload, avec leur glyphe MDI.
    static const char* const kIcons[11] = {
        "\U000F059D",  // vent
        "\U000F0EFA",  // inondation
        "\U000F0593",  // orages
        "\U000F0596",  // pluie-inondation
        "\U000F0F36",  // neige-verglas
        "\U000F0F29",  // grand froid
        "\U000F078D",  // vagues-submersion
        "\U000F0E01",  // canicule
        "\U000F1A48",  // avalanches (landslide : MDI n'a pas d'icône avalanche)
        "\U000F0591",  // brouillard (MeteoAlarm)
        "\U000F0238",  // feux de forêt (MeteoAlarm)
    };
    struct AlertEntry { const char* icon; const char* level; };
    constexpr size_t MAX_ALERTES = 4;
    AlertEntry actives[MAX_ALERTES];
    size_t active_count = 0;
    for (int i = 0; i < 11 && active_count < MAX_ALERTES; i++) {
        const char* state = fields[2 + i];
        if (strlen(state) == 0 || strcmp(state, "Vert") == 0 || strcmp(state, "unknown") == 0) continue;
        actives[active_count++] = AlertEntry{kIcons[i], state};
    }

    // Icônes du bandeau central : palette UIBandeau (sombre en clair pour certains thèmes).
    const bool pastille = palette_claire(UIBandeau);
    for (size_t i = 0; i < 4; i++) {
        lv_obj_t* slot = ui.slots[i];
        if (slot == nullptr) continue;
        const bool shown = i < active_count;
        if (shown) {
            lv_label_set_text(slot, actives[i].icon);
            uint32_t c = UIBandeau.ALERT_YELLOW;
            if (strcmp(actives[i].level, "Orange") == 0)     c = UIBandeau.ALERT_ORANGE;
            else if (strcmp(actives[i].level, "Rouge") == 0) c = UIBandeau.ALERT_RED;
            lv_obj_set_style_text_color(slot, lv_color_hex(pastille ? UIBandeau.TEXT_PRIMARY : c), LV_PART_MAIN);
            lv_obj_set_style_text_opa(slot, 255, LV_PART_MAIN);
            vigilance_pastille(slot, pastille, c);
        }
        lv_obj_set_flag(slot, LV_OBJ_FLAG_HIDDEN, !shown);
    }
    return active_count > 0;
}

void vigilance_rejouer() {
    if (!s_vigilance_payload.empty()) parse_and_update_vigilance(s_vigilance_payload, s_vigilance_ui);
}

// Intensité → couleur + hauteur (px) d'une barre. Deux écritures acceptées : le
// libellé Météo-France (« Pluie faible » … « Pluie très forte »), ou un niveau
// chiffré « 0 » à « 4 » (lot 4c, 27/09/2026) que les adaptateurs des autres
// fournisseurs calculent côté HA à partir des mm/h. Tout autre texte vide la barre.
static int rain_level(const std::string& intensite) {
    if (intensite.size() == 1 && intensite[0] >= '0' && intensite[0] <= '4') return intensite[0] - '0';
    if (intensite == "Pluie faible")     return 1;
    if (intensite == "Pluie modérée")    return 2;
    if (intensite == "Pluie forte")      return 3;
    if (intensite == "Pluie très forte" || intensite == "Pluie trés forte") return 4;
    return 0;
}

// Barres du bandeau central : palette UIBandeau.
static void rain_level_style(int niveau, uint32_t& color, int& height) {
    color = UIBandeau.CLIM_TRACK_INACTIVE;  // barre vide
    height = 0;
    switch (niveau) {
        case 1: color = UIBandeau.RAIN_LIGHT;    height = 13; break;  // ~1/4 hauteur
        case 2: color = UIBandeau.RAIN_MODERATE; height = 25; break;  // 1/2
        case 3: color = UIBandeau.RAIN_HEAVY;    height = 38; break;  // 3/4
        case 4: color = UIBandeau.RAIN_EXTREME;  height = 50; break;  // max
        default: break;
    }
}

// Hauteurs posées, barre par barre. has_rain se calcule ICI et pas en relisant
// lv_obj_get_height() : sous LVGL 9 cette lecture renvoie les coordonnées
// courantes (obj->coords), mises à jour seulement au prochain rafraîchissement
// de layout — juste après lv_obj_set_height() elle rend encore l'ancienne
// hauteur, donc 0, et le panneau « Pluie » ne tournait jamais (constaté le
// 08/09/2026 depuis HA : « Pluie forte » poussée, rotation Planning/Info/Alerte
// inchangée). Le même bilan sert aux deux services (unitaire et bulk).
static int s_rain_bar_height[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
// Thèmes : niveau et widget de chaque barre déjà posée, pour la repeindre
// (rain_bars_rejouer()) sans attendre la prochaine poussée.
static int s_rain_bar_level[9] = {0, 0, 0, 0, 0, 0, 0, 0, 0};
static lv_obj_t* s_rain_bar_obj[9] = {};

static bool rain_any_bar() {
    for (int i = 0; i < 9; i++) {
        if (s_rain_bar_height[i] > 0) return true;
    }
    return false;
}

// Histogramme pluie 1 h : 9 barres de 5 min (rb_0_in … rb_8_in). intensite =
// libellé Météo-France (« Pluie faible » … « Pluie très forte »), tout autre
// texte vide la barre. Retourne true si au moins une barre est non vide — à
// stocker dans has_rain.
static bool update_rain_bar_ui(int idx, const std::string& intensite, lv_obj_t* const bars[9]) {
    if (idx >= 0 && idx < 9 && bars[idx] != nullptr) {
        uint32_t c;
        int h;
        const int niveau = rain_level(intensite);
        rain_level_style(niveau, c, h);
        lv_obj_set_style_bg_color(bars[idx], lv_color_hex(c), LV_PART_MAIN);
        lv_obj_set_height(bars[idx], h);
        s_rain_bar_height[idx] = h;
        s_rain_bar_level[idx] = niveau;
        s_rain_bar_obj[idx] = bars[idx];
    }
    return rain_any_bar();
}

void rain_bars_rejouer() {
    for (int i = 0; i < 9; i++) {
        if (s_rain_bar_obj[i] == nullptr) continue;
        uint32_t c;
        int h;
        rain_level_style(s_rain_bar_level[i], c, h);
        lv_obj_set_style_bg_color(s_rain_bar_obj[i], lv_color_hex(c), LV_PART_MAIN);
    }
}

// Bulk (ADR-0003) : « idx|intensité;idx|intensité;… », les 9 barres en UN appel HA
// au lieu de neuf. Chaque enregistrement passe par update_rain_bar_ui() (même
// table rain_level_style()) ; un enregistrement sans '|' ou hors 0..8 est ignoré.
// Tampon fixe : un payload trop long est refusé en bloc (log WARN), les barres
// restent en l'état. Retourne has_rain (au moins une barre non vide).
bool update_rain_bars_bulk_ui(const std::string& payload, lv_obj_t* const bars[9]) {
    char buf[256];
    if (payload_trop_long("tab5.rain", payload.size(), sizeof(buf) - 1)) return rain_any_bar();
    strncpy(buf, payload.c_str(), sizeof(buf));
    buf[sizeof(buf) - 1] = '\0';
    char* save = nullptr;
    for (char* rec = strtok_r(buf, ";", &save); rec != nullptr; rec = strtok_r(nullptr, ";", &save)) {
        char* sep = strchr(rec, '|');
        if (sep == nullptr) continue;
        *sep = '\0';
        update_rain_bar_ui(atoi(rec), std::string(sep + 1), bars);
    }
    return rain_any_bar();
}

// Thèmes : la dernière prédiction posée (rain_predict_rejouer()).
static lv_obj_t* s_predict_icon = nullptr;
static int s_predict_neige = 0;
static float s_predict_humidite = 0.0f;

void update_rain_predict_icon_ui(lv_obj_t* icon, int neige, float humidite) {
    if (icon == nullptr) return;
    s_predict_icon = icon;
    s_predict_neige = neige;
    s_predict_humidite = humidite;
    if (neige >= 5) {
        lv_label_set_text(icon, "\U000F0598");  // flocon
        lv_obj_set_style_text_color(icon, lv_color_hex(UIBandeau.WARNING), LV_PART_MAIN);
    } else {
        lv_label_set_text(icon, "\U000F0597");  // goutte
        lv_obj_set_style_text_color(icon, lv_color_hex(get_humidity_color(humidite, UIBandeau)), LV_PART_MAIN);
    }
}

void rain_predict_rejouer() {
    if (s_predict_icon != nullptr) update_rain_predict_icon_ui(s_predict_icon, s_predict_neige, s_predict_humidite);
}

// update_clim_from_ha_ui() : tab5_clim.cpp (tab5_cards.cpp du 29/09 au 08/10/2026) (ADR-0026), avec les
// réglages de la clim qui fixent le format de la cible et l'unité ; devenue
// clim_blueprint_recu() le même jour (clims des tuiles, ADR-0027).

// Bandeau planning, retenu à sa première écriture pour planning_rejouer_theme().
static lv_obj_t* s_planning_lbl = nullptr;
static std::string* s_plan_ligne_1 = nullptr;
static std::string* s_plan_ligne_2 = nullptr;

static std::string sans_numero(const std::string& s) {
    if (s.rfind("1/ ", 0) == 0) return s.substr(3);
    if (s.rfind("2/ ", 0) == 0) return s.substr(3);
    if (s.rfind("1/", 0) == 0) return s.substr(2);
    if (s.rfind("2/", 0) == 0) return s.substr(2);
    return s;
}

void update_planning_text_ui(lv_obj_t* lbl, const std::string& l1, const std::string& l2,
    std::string& plan_ligne_1, std::string& plan_ligne_2) {
    if (!lbl) return;
    s_planning_lbl = lbl;
    s_plan_ligne_1 = &plan_ligne_1;
    s_plan_ligne_2 = &plan_ligne_2;
    std::string line1 = sans_numero(l1);
    std::string line2 = sans_numero(l2);
    plan_ligne_1 = line1;
    plan_ligne_2 = line2;
    std::string combined = line1;
    if (!line2.empty()) {
        combined += "   |   " + line2;
    }
    combined = normalize_text_utf8(combined);
    set_label_text_utf8(lbl, combined.c_str());
}

// Couleurs du balisage recolor de LVGL (« #RRGGBB texte# ») : un rôle de la palette du
// bandeau (UIBandeau), jamais une valeur en dur. Thèmes (05/10/2026) : le blanc écrit
// en dur rendait « Auj. » et les horaires invisibles sur le bandeau clair des thèmes
// sans `zones_sombres:` (Ardoise, Bento, Graphite… en mode clair). Le texte est
// recalculé au changement de thème (planning_rejouer_theme).
static std::string couleur_recolor(uint32_t c) {
    char hex[8];
    snprintf(hex, sizeof(hex), "%06X", static_cast<unsigned>(c & 0xFFFFFFu));
    return hex;
}

// Bandeau planning vide, dans le texte atténué du bandeau.
static std::string planning_vide() {
    return "#" + couleur_recolor(UIBandeau.TEXT_DIM) + " " + tr("Aucun travail de prévu") + "#";
}

void build_planning_lines_from_jours(std::string& out_l1, std::string& out_l2) {
    out_l1.clear();
    out_l2.clear();

    time_t now_raw = tab5_time_source(nullptr);
    if (now_raw <= 0) {
        out_l1 = planning_vide();
        return;
    }
    struct tm now_tm;
    if (localtime_r(&now_raw, &now_tm) == nullptr) {
        out_l1 = planning_vide();
        return;
    }

    std::string lines[2];
    int n = 0;
    for (int jour = 0; jour < 15 && n < 2; jour++) {
        // `jour` compte depuis AUJOURD'HUI ; la case lue est recalée sur le jour du lot
        // poussé par HA (muet depuis hier soir → aujourd'hui = case 1). Lot non daté
        // (reçu avant la synchro SNTP) : case 0 = aujourd'hui, comme avant le recalage.
        const int idx = (cal_jours_anchor_day < 0) ? jour : cal_index_for_offset(jour);
        if (idx < 0) continue;
        const DayForecastData& d = cal_jours_data[idx];
        const std::string& h = d.heures_ouverture;
        if (h.size() < 11 || d.est_repos) continue;  // "HH:MM-HH:MM"

        // Aujourd'hui : ignorer le créneau s'il a déjà commencé (même règle que l'ancien Jinja HA)
        // Un début illisible compte comme 00:00 (déjà commencé), comme avant.
        if (jour == 0) {
            const int now_min = now_tm.tm_hour * 60 + now_tm.tm_min;
            if (now_min >= std::max(0, hhmm_minutes(h))) continue;
        }

        std::string j_name;
        if (jour == 0) j_name = tr("Auj.");
        else if (jour == 1) j_name = tr("Dem.");
        else {
            // Date civile de J+jour normalisée à midi : `maintenant + jour × 86 400 s`
            // tombait sur le mauvais jour les nuits de changement d'heure (audit §5).
            struct tm day_tm;
            if (!local_day_from_offset(jour, day_tm)) continue;
            j_name = std::string(day_short_utf8(day_tm.tm_wday)) + ".";   // « Dim. »
        }

        // Embauche tôt : EARLY ; sinon le texte du bandeau ; « Dem. » : INFO.
        const bool early = cal_is_early_shift(h);
        const std::string hex = couleur_recolor(early ? UIBandeau.EARLY : UIBandeau.TEXT_PRIMARY);
        std::string j_colored;
        if (jour == 1) j_colored = "#" + couleur_recolor(UIBandeau.INFO) + " " + j_name + "#";
        else j_colored = "#" + hex + " " + j_name + "#";

        char line[96];
        snprintf(line, sizeof(line), "%d/ %s : #%s %s#", n + 1, j_colored.c_str(), hex.c_str(), h.c_str());
        lines[n++] = line;
    }

    if (n == 0) out_l1 = planning_vide();
    else {
        out_l1 = lines[0];
        if (n > 1) out_l2 = lines[1];
    }
}

// Thème changé : les couleurs sont dans le texte, il est recalculé avec la nouvelle
// palette. Pendant le planning du tap (6 s), seules les lignes à rendre changent. Un
// texte venu de l'ancien service tab5_maj_planning (compatibilité) est remplacé par
// celui des prévisions, comme à leur prochaine poussée.
void planning_rejouer_theme() {
    if (s_planning_lbl == nullptr || s_plan_ligne_1 == nullptr || s_plan_ligne_2 == nullptr) return;
    std::string l1, l2;
    build_planning_lines_from_jours(l1, l2);
    if (temp_planning_active()) {
        *s_plan_ligne_1 = sans_numero(l1);
        *s_plan_ligne_2 = sans_numero(l2);
        planning_temporaire_lignes(*s_plan_ligne_1, *s_plan_ligne_2);
        return;
    }
    update_planning_text_ui(s_planning_lbl, l1, l2, *s_plan_ligne_1, *s_plan_ligne_2);
}

// -----------------------------------------------------------------------------
// Fuseau horaire de Home Assistant (lot 6b, ADR-0020). ESPHome applique le fuseau
// que HA envoie avec l'heure (api_connection.cpp, on_get_time_response), mais ne le
// garde pas : au démarrage suivant, il repartirait avec celui de la compilation (UTC
// pour un firmware compilé par la CI), et le réveil sonnerait à la mauvaise heure
// tant que HA ne répond pas. On range donc en NVS le fuseau que HA a donné, et on le
// remet au démarrage.
// -----------------------------------------------------------------------------
#ifdef USE_TIME_TIMEZONE
#include "esphome/components/time/posix_tz.h"

namespace {

constexpr uint32_t kFuseauMagic = 0x545A3031;    // « TZ01 »
constexpr uint32_t kFuseauPrefKey = 0x747A6861;  // « tzha »

struct FuseauSauve {
    uint32_t magic;
    esphome::time::ParsedTimezone tz;
};

esphome::ESPPreferenceObject s_fuseau_pref;
bool s_fuseau_pret = false;
esphome::time::ParsedTimezone s_fuseau_range{};  // ce qui est en NVS, une fois connu
bool s_fuseau_connu = false;
// HA a donné l'heure depuis le démarrage. L'heure système est commune à toutes les
// horloges (sntp, rx8130, homeassistant) : sa validité ne dit pas qui l'a donnée.
bool s_fuseau_de_ha = false;

// Champ par champ : le bourrage des structures n'a pas de valeur définie.
bool meme_regle(const esphome::time::DSTRule& a, const esphome::time::DSTRule& b) {
    return a.time_seconds == b.time_seconds && a.day == b.day && a.type == b.type &&
           a.month == b.month && a.week == b.week && a.day_of_week == b.day_of_week;
}

bool meme_fuseau(const esphome::time::ParsedTimezone& a, const esphome::time::ParsedTimezone& b) {
    return a.std_offset_seconds == b.std_offset_seconds &&
           a.dst_offset_seconds == b.dst_offset_seconds &&
           meme_regle(a.dst_start, b.dst_start) && meme_regle(a.dst_end, b.dst_end);
}

// Lit la NVS une seule fois ; vrai si elle contient un fuseau.
bool fuseau_charger() {
    if (!s_fuseau_pret) {
        s_fuseau_pret = true;
        s_fuseau_pref = esphome::global_preferences->make_preference<FuseauSauve>(kFuseauPrefKey);
        FuseauSauve s{};
        if (s_fuseau_pref.load(&s) && s.magic == kFuseauMagic) {
            s_fuseau_range = s.tz;
            s_fuseau_connu = true;
        }
    }
    return s_fuseau_connu;
}

// Décalage d'hiver en heures, pour les logs (POSIX compte positivement vers l'ouest).
int decalage_hiver_h(const esphome::time::ParsedTimezone& tz) {
    return static_cast<int>(-tz.std_offset_seconds / 3600);
}

}  // namespace

void fuseau_restaurer() {
    static bool fait = false;  // une seule fois : l'interval qui l'appelle revient chaque jour
    if (fait) return;
    fait = true;
    if (!fuseau_charger()) {
        ESP_LOGI("tab5.fuseau", "Aucun fuseau de HA en mémoire : celui de la compilation");
        return;
    }
    esphome::time::set_global_tz(s_fuseau_range);
    ESP_LOGI("tab5.fuseau", "Fuseau du dernier passage de HA remis (UTC%+d h en hiver)",
             decalage_hiver_h(s_fuseau_range));
}

void fuseau_recu_de_ha() { s_fuseau_de_ha = true; }

void fuseau_memoriser() {
    // Seul un fuseau venu de HA se range. HA l'applique juste APRÈS le déclencheur
    // on_time_sync qui appelle fuseau_recu_de_ha() : le tick minute suivant le voit.
    if (!s_fuseau_de_ha) return;
    const esphome::time::ParsedTimezone& tz = esphome::time::get_global_tz();
    if (fuseau_charger() && meme_fuseau(tz, s_fuseau_range)) return;
    FuseauSauve s{};
    s.magic = kFuseauMagic;
    s.tz = tz;
    if (s_fuseau_pref.save(&s)) {
        s_fuseau_range = tz;
        s_fuseau_connu = true;
        ESP_LOGI("tab5.fuseau", "Fuseau de HA gardé pour le prochain démarrage (UTC%+d h en hiver)",
                 decalage_hiver_h(tz));
    }
}
#else
void fuseau_restaurer() {}
void fuseau_recu_de_ha() {}
void fuseau_memoriser() {}
#endif
