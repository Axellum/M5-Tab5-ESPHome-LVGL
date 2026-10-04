/**
 * [AI-CONTEXT]
 * @file tab5_calendar.cpp
 * @role Popup calendrier mensuel : cache des mois (TTL, éviction), rendu de la grille
 *       7×6, détail d'un jour (chargement + rendu des lignes).
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
#include "lvgl.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <ctime>
#include <cstring>
#include <vector>
#include <map>

// =============================================================================
// Popup calendrier mensuel (calendar_popup.yaml, appui long sur l'horloge)
// =============================================================================

struct CalMonthData {
    std::string codes;    // 62 hex (2/jour) — bits CAL_BIT_* de tab5_custom.h
    std::string heures;   // 31 champs "HH:MM-HH:MM" séparés par | (vides autorisés)
    std::string details;  // 31 champs "type|texte;..." séparés par ~ (vide = rien prévu)
    bool has_details = false;  // true si HA a fourni le champ details (même tout vide)
    uint32_t stored_at = 0;    // millis() au stockage — pour le TTL stale-while-revalidate
};

// Cache par mois (clé = annee*12 + mois-1). Stratégie stale-while-revalidate :
// les données sont affichées immédiatement même si périmées, un refresh silencieux
// est lancé en parallèle. Eviction : max 3 mois (M-1, M, M+1) autour de la vue courante.
static std::map<int, CalMonthData> s_cal_month_cache;

static int cal_cache_key(int y, int m) { return y * 12 + (m - 1); }

bool cal_month_needs_fetch(int year, int month) {
    return s_cal_month_cache.find(cal_cache_key(year, month)) == s_cal_month_cache.end();
}

bool cal_month_is_stale(int year, int month, uint32_t ttl_ms) {
    const auto it = s_cal_month_cache.find(cal_cache_key(year, month));
    if (it == s_cal_month_cache.end()) return true;  // absent = périmé
    return (esphome::millis() - it->second.stored_at) > ttl_ms;
}

void cal_cache_evict_distant(int year, int month) {
    // Garde uniquement les mois dans [M-1, M+1] autour de la vue courante.
    const int center = cal_cache_key(year, month);
    for (auto it = s_cal_month_cache.begin(); it != s_cal_month_cache.end(); ) {
        if (abs(it->first - center) > 1) it = s_cal_month_cache.erase(it);
        else ++it;
    }
}

// Seul calcul « mois ± delta » du calendrier (navigation ◀/▶, préchargement des
// mois adjacents au rendu et au boot) : avant le lot 7.3 de l'audit du 26/09/2026,
// il était recopié cinq fois dans tab5-calendar.yaml. L'année suit le passage
// janvier ↔ décembre. Mois attendu dans 1..12 (les seules valeurs que la vue
// stocke : SNTP ou ce calcul lui-même).
void cal_shift_month(int& year, int& month, int delta) {
    month += delta;
    while (month < 1)  { month += 12; year--; }
    while (month > 12) { month -= 12; year++; }
}

void cal_store_month_data(const std::string& annee, const std::string& mois,
    const std::string& codes, const std::string& heures, const std::string& details) {
    const int y = atoi(annee.c_str());
    const int m = atoi(mois.c_str());
    if (y < 2000 || y > 2100 || m < 1 || m > 12) return;
    CalMonthData data;
    data.codes = codes;
    data.heures = heures;
    data.details = details;
    // HA envoie toujours 30× '~' minimum (31 champs). Absent / "" = vieux HA → fallback jour.
    data.has_details = !details.empty();
    data.stored_at = esphome::millis();
    s_cal_month_cache[cal_cache_key(y, m)] = data;
}

// n-ième champ d'une chaîne délimitée par un séparateur — champs vides autorisés
static std::string cal_field_delim(const std::string& s, int idx, char delim) {
    size_t start = 0;
    for (int i = 0; i < idx; i++) {
        const size_t p = s.find(delim, start);
        if (p == std::string::npos) return "";
        start = p + 1;
    }
    size_t end = s.find(delim, start);
    if (end == std::string::npos) end = s.size();
    return s.substr(start, end - start);
}

std::string cal_cached_day_detail(int year, int month, int day) {
    if (day < 1 || day > 31) return "";
    const auto it = s_cal_month_cache.find(cal_cache_key(year, month));
    if (it == s_cal_month_cache.end() || !it->second.has_details) return "";
    return cal_field_delim(it->second.details, day - 1, '~');
}

bool cal_day_has_embedded_detail(int year, int month, int day) {
    // Champ ~ vide ≠ « rien de prévu confirmé » : avec get_events borné à aujourd'hui,
    // les jours passés n'ont souvent pas de détail embarqué → fallback script _jour.
    return !cal_cached_day_detail(year, month, day).empty();
}

static int cal_days_in_month(int y, int m) {
    static const int dm[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) return 30;
    int d = dm[m - 1];
    if (m == 2 && ((y % 4 == 0 && y % 100 != 0) || y % 400 == 0)) d = 29;
    return d;
}

// Jour de la semaine (0 = lundi ... 6 = dimanche), algorithme de Sakamoto —
// aucune dépendance à mktime/timezone, valable pour tout le calendrier grégorien.
static int cal_weekday_mon0(int y, int m, int d) {
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    const int w = (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;  // 0 = dimanche
    return (w + 6) % 7;
}

// « Janvier » / « Lundi » : noms de tab5_core.cpp, majuscule initiale (titres).
static std::string cal_month_name_utf8(int month) {
    return fr_capitalized(month_long_utf8(month));
}

// wd_mon0 : 0 = lundi … 6 = dimanche (grille qui commence le lundi).
static std::string cal_weekday_name_utf8(int wd_mon0) {
    if (wd_mon0 < 0 || wd_mon0 > 6) return "";
    return fr_capitalized(day_long_utf8((wd_mon0 + 1) % 7));
}

// n-ième champ d'une chaîne délimitée par | — champs vides autorisés
// (strtok_r fusionnerait les séparateurs consécutifs, donc parcours manuel).
static std::string cal_field(const std::string& s, int idx) {
    return cal_field_delim(s, idx, '|');
}

static int cal_hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

// =============================================================================
// Grille du mois : 42 cellules construites ici (lot 8 de l'audit du 26/09/2026)
// =============================================================================
// Avant : 42 `!include` de cal_day_cell.yaml. ESPHome recopiait le code de chaque
// instance dans setup() (≈ 45 Ko de flash) et posait un bouton invisible sur chaque
// cellule, avec son déclencheur, son automatisation et son action (252 widgets).
// Ici, une boucle crée les mêmes objets avec les mêmes propriétés que le main.cpp
// généré, à la première ouverture du calendrier. La cellule reçoit le tap elle-même.

struct CalCellUI {
    lv_obj_t* cell;   // fond (teinte vacances scolaires) + bordure (aujourd'hui)
    lv_obj_t* num;    // numéro du jour
    lv_obj_t* sub;    // heures de travail "09:30-20:15"
    lv_obj_t* dot;    // pastille RDV (dorée)
    lv_obj_t* dot2;   // pastille anniversaire (rose)
};
static CalCellUI s_cal_cells[42] = {};
static void (*s_cal_on_tap)(int) = nullptr;
// Colonnes de la grille, lundi en tête : case c à x = kCalColX0 + c × kCalColPas, large
// de kCalColW. Les en-têtes « Lun »…« Dim » de calendar_popup.yaml sont écrits sur les
// mêmes colonnes (x et width en clair) : tests/test_geometrie_partagee.py vérifie qu'ils
// restent alignés sur ces trois valeurs.
static constexpr int32_t kCalColX0 = 25;
static constexpr int32_t kCalColPas = 172;
static constexpr int32_t kCalColW = 168;
// Zone de la grille (${cal_grid_y} / ${cal_grid_h}) : cal_render_month() y répartit
// les semaines du mois affiché.
static int32_t s_cal_grid_y = 0;
static int32_t s_cal_grid_h = 0;

static void cal_cell_tap_cb(lv_event_t* e) {
    if (s_cal_on_tap) s_cal_on_tap((int) (intptr_t) lv_event_get_user_data(e));
}

// Fond d'une case, opaque depuis le 01/10/2026 : la teinte qu'elle avait en transparence
// (`teinte` à `opa` sur la carte du popup), calculée ici une fois pour toutes, comme le
// verre pré-mélangé de tab5-styles.yaml. Une case simplement passée à 100 % serait gris
// clair sous des chiffres gris (week-end, jours passés : TEXT_DIM), illisibles.
// La carte (style_modal_card) est un dégradé vertical : sa couleur est relue sur son style
// à la hauteur du milieu de la ligne, puis mélangée comme LVGL le fait (lv_color_mix).
static lv_color_t cal_fond_case(lv_obj_t* carte, int32_t y_milieu, uint32_t teinte, lv_opa_t opa) {
    int32_t h = lv_obj_get_height(carte);
    if (h <= 0) h = lv_obj_get_style_height(carte, LV_PART_MAIN);
    // y de la case : depuis le bord intérieur de la carte ; le dégradé part du bord extérieur.
    const int32_t y = y_milieu + lv_obj_get_style_border_width(carte, LV_PART_MAIN);
    const int32_t y0 = h * lv_obj_get_style_bg_main_stop(carte, LV_PART_MAIN) / 255;
    const int32_t y1 = h * lv_obj_get_style_bg_grad_stop(carte, LV_PART_MAIN) / 255;
    lv_opa_t part_bas = LV_OPA_TRANSP;
    if (y >= y1) part_bas = LV_OPA_COVER;
    else if (y > y0 && y1 > y0) part_bas = (lv_opa_t) ((y - y0) * 255 / (y1 - y0));
    const lv_color_t fond = lv_color_mix(lv_obj_get_style_bg_grad_color(carte, LV_PART_MAIN),
                                         lv_obj_get_style_bg_color(carte, LV_PART_MAIN), part_bas);
    return lv_color_mix(lv_color_hex(teinte), fond, opa);
}

// Styles partagés des 42 cases (audit des conteneurs du 01/10/2026) : ce qui est commun
// à toutes les cases est posé une fois, au lieu de 39 propriétés locales par case
// (≈ 1 600 en tout). Ne reste en local que ce qui varie : x et y de la case, police des
// libellés, puis ce que cal_render_month() pose à chaque rendu (y, hauteur, fond,
// bordure, couleur des textes). Un style local passe toujours avant un style ajouté,
// et un style ajouté passe avant ceux du thème posés à la création : rendu identique.
static lv_style_t s_cal_style_case;
static lv_style_t s_cal_style_num;
static lv_style_t s_cal_style_sub;
static lv_style_t s_cal_style_pastille_rdv;
static lv_style_t s_cal_style_pastille_anniv;

// Pastille 14 px en haut à droite (décalée de x depuis le bord droit).
static void cal_style_pastille(lv_style_t* st, int32_t x, uint32_t color) {
    lv_style_init(st);
    lv_style_set_align(st, LV_ALIGN_TOP_RIGHT);
    lv_style_set_bg_color(st, lv_color_hex(color));
    lv_style_set_border_width(st, 0);
    lv_style_set_height(st, 14);
    lv_style_set_radius(st, 7);
    lv_style_set_width(st, 14);
    lv_style_set_x(st, x);
    lv_style_set_y(st, 6);
}

// Libellé de case : la couleur que le `theme:` ESPHome pose sur un label YAML
// (TEXT_SOFT), puis sa place dans la case.
static void cal_style_libelle(lv_style_t* st, lv_align_t align, int32_t y) {
    lv_style_init(st);
    lv_style_set_text_color(st, lv_color_hex(UIColor.TEXT_SOFT));
    lv_style_set_align(st, align);
    lv_style_set_y(st, y);
}

static void cal_styles_init() {
    // Case : fond (teinte vacances scolaires) et bordure (aujourd'hui) éteints jusqu'au
    // rendu ; hauteur provisoire, recalculée par cal_render_month().
    lv_style_init(&s_cal_style_case);
    lv_style_set_align(&s_cal_style_case, LV_ALIGN_TOP_LEFT);
    lv_style_set_bg_color(&s_cal_style_case, lv_color_hex(UIColor.GLASS_RIM));
    lv_style_set_bg_opa(&s_cal_style_case, LV_OPA_TRANSP);
    lv_style_set_border_color(&s_cal_style_case, lv_color_hex(UIColor.ACCENT));
    lv_style_set_border_opa(&s_cal_style_case, LV_OPA_TRANSP);
    lv_style_set_border_width(&s_cal_style_case, 2);
    lv_style_set_height(&s_cal_style_case, 86);
    lv_style_set_pad_all(&s_cal_style_case, 0);
    lv_style_set_radius(&s_cal_style_case, 12);
    lv_style_set_width(&s_cal_style_case, kCalColW);
    // Numéro du jour, centré sous le nom du jour de la tête de grille ; heures de
    // travail en bas de la case.
    cal_style_libelle(&s_cal_style_num, LV_ALIGN_TOP_MID, 4);
    cal_style_libelle(&s_cal_style_sub, LV_ALIGN_BOTTOM_MID, -6);
    // Pastille RDV (dorée), pastille anniversaire (rose) à sa gauche.
    cal_style_pastille(&s_cal_style_pastille_rdv, -8, UIColor.GOLD);
    cal_style_pastille(&s_cal_style_pastille_anniv, -28, UIColor.WARM_PINK);
}

// Thèmes (ADR-0029) : les couleurs des styles des cases, en place (les cases gardent leurs
// styles) ; cal_render_month() repeint ensuite ce qu'il pose en local.
void cal_styles_repeindre() {
    if (s_cal_cells[0].cell == nullptr) return;
    lv_style_set_bg_color(&s_cal_style_case, lv_color_hex(UIColor.GLASS_RIM));
    lv_style_set_border_color(&s_cal_style_case, lv_color_hex(UIColor.ACCENT));
    lv_style_set_text_color(&s_cal_style_num, lv_color_hex(UIColor.TEXT_SOFT));
    lv_style_set_text_color(&s_cal_style_sub, lv_color_hex(UIColor.TEXT_SOFT));
    lv_style_set_bg_color(&s_cal_style_pastille_rdv, lv_color_hex(UIColor.GOLD));
    lv_style_set_bg_color(&s_cal_style_pastille_anniv, lv_color_hex(UIColor.WARM_PINK));
    for (lv_style_t* st : {&s_cal_style_case, &s_cal_style_num, &s_cal_style_sub, &s_cal_style_pastille_rdv,
                           &s_cal_style_pastille_anniv})
        lv_obj_report_style_change(st);
}

// Pastille masquée tant que le rendu ne l'allume pas.
static lv_obj_t* cal_dot_create(lv_obj_t* cell, const lv_style_t* style) {
    lv_obj_t* d = lv_obj_create(cell);
    lv_obj_add_style(d, style, LV_PART_MAIN);
    lv_obj_add_flag(d, LV_OBJ_FLAG_HIDDEN);
    // Plus de bouton par-dessus : une pastille cliquable garderait le tap pour
    // elle, et le jour ne s'ouvrirait pas.
    lv_obj_remove_flag(d, LV_OBJ_FLAG_CLICKABLE);
    return d;
}

// Libellé : son style partagé, et la police en local (roboto_22, celle du `theme:`
// ESPHome, quand la sienne n'est pas donnée).
static lv_obj_t* cal_label_create(lv_obj_t* cell, const lv_style_t* style,
                                  const esphome::font::Font* font,
                                  const esphome::font::Font* font_theme) {
    lv_obj_t* l = lv_label_create(cell);
    lv_obj_add_style(l, style, LV_PART_MAIN);
    esphome::lvgl::lv_obj_set_style_text_font(l, font ? font : font_theme, LV_PART_MAIN);
    lv_label_set_text(l, "");
    return l;
}

bool cal_grid_build(lv_obj_t* anchor, int32_t grid_y, int32_t grid_h,
                    const esphome::font::Font* font_num,
                    const esphome::font::Font* font_text, void (*on_tap)(int)) {
    if (s_cal_cells[0].cell != nullptr) return false;   // déjà construite
    lv_obj_t* parent = anchor ? lv_obj_get_parent(anchor) : nullptr;
    if (parent == nullptr) return false;
    s_cal_on_tap = on_tap;
    s_cal_grid_y = grid_y;
    s_cal_grid_h = grid_h;
    cal_styles_init();
    for (int i = 0; i < 42; i++) {
        // Case de kCalColW de large dans la colonne i % 7 (lundi en tête). Hauteur et y :
        // posés au rendu (cal_render_month), selon le nombre de semaines du mois.
        lv_obj_t* c = lv_obj_create(parent);
        lv_obj_move_to_index(c, lv_obj_get_index(anchor));   // même rang qu'en YAML
        lv_obj_add_style(c, &s_cal_style_case, LV_PART_MAIN);
        lv_obj_set_style_x(c, kCalColX0 + (i % 7) * kCalColPas, LV_PART_MAIN);
        lv_obj_set_style_y(c, grid_y + (i / 7) * 90, LV_PART_MAIN);
        lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
        // Tap court -> détail du jour (ex-bouton invisible, on_short_click).
        lv_obj_add_event_cb(c, cal_cell_tap_cb, LV_EVENT_SHORT_CLICKED, (void*) (intptr_t) i);

        CalCellUI& ui = s_cal_cells[i];
        ui.cell = c;
        // Numéro du jour, centré sous le nom du jour de la tête de grille (il était
        // collé à gauche : 74 px de décalage avec « Lun », « Mar »…). Couleur : blanc /
        // weekend estompé / férié rose / passé estompé.
        ui.num = cal_label_create(c, &s_cal_style_num, font_num, font_text);
        // Heures de travail du jour ("09:30-20:15", orange si embauche < 9h)
        ui.sub = cal_label_create(c, &s_cal_style_sub, nullptr, font_text);
        // Pastille RDV (dorée) + pastille anniversaire (rose)
        ui.dot = cal_dot_create(c, &s_cal_style_pastille_rdv);
        ui.dot2 = cal_dot_create(c, &s_cal_style_pastille_anniv);
    }
    return true;
}

void cal_render_month(lv_obj_t* lbl_month,
    int view_year, int view_month, int today_year, int today_month, int today_day) {
    if (!lbl_month || view_month < 1 || view_month > 12) return;

    char buf[48];
    snprintf(buf, sizeof(buf), "%s %d", cal_month_name_utf8(view_month).c_str(), view_year);
    lv_label_set_text(lbl_month, buf);

    const int first_col = cal_weekday_mon0(view_year, view_month, 1);
    const int ndays = cal_days_in_month(view_year, view_month);

    const CalMonthData* data = nullptr;
    const auto it = s_cal_month_cache.find(cal_cache_key(view_year, view_month));
    if (it != s_cal_month_cache.end()) data = &it->second;

    // Lignes : autant que de semaines dans le mois (4 à 6), réparties sur toute la
    // hauteur de la grille et centrées. Avec 6 lignes fixes, un mois de 5 semaines
    // laissait une bande vide au-dessus de la légende (calendrier « trop haut »).
    const int rows = (first_col + ndays + 6) / 7;
    const int32_t gap = 4;
    const int32_t row_h = (s_cal_grid_h - (rows - 1) * gap) / rows;
    const int32_t rows_y = s_cal_grid_y + (s_cal_grid_h - (rows * row_h + (rows - 1) * gap)) / 2;

    const bool has_today = (today_year > 0);
    for (int i = 0; i < 42; i++) {
        const CalCellUI& c = s_cal_cells[i];
        if (!c.cell || !c.num || !c.sub || !c.dot || !c.dot2) continue;

        const int row = i / 7;
        if (row >= rows) {
            lv_obj_add_flag(c.cell, LV_OBJ_FLAG_HIDDEN);
            continue;
        }
        lv_obj_remove_flag(c.cell, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_y(c.cell, rows_y + row * (row_h + gap), LV_PART_MAIN);
        lv_obj_set_style_height(c.cell, row_h, LV_PART_MAIN);

        const int day = i - first_col + 1;
        if (day < 1 || day > ndays) {
            // Cellule hors mois : tout éteint (le tap est neutralisé par
            // cal_date_for_cell qui renvoie "").
            lv_label_set_text(c.num, "");
            lv_label_set_text(c.sub, "");
            lv_obj_add_flag(c.dot, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(c.dot2, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_bg_opa(c.cell, LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_border_opa(c.cell, LV_OPA_TRANSP, LV_PART_MAIN);
            continue;
        }

        int code = 0;
        std::string heures;
        if (data) {
            if ((int)data->codes.size() >= day * 2) {
                code = cal_hex_val(data->codes[(day - 1) * 2]) * 16
                     + cal_hex_val(data->codes[(day - 1) * 2 + 1]);
            }
            heures = cal_field(data->heures, day - 1);
        }

        snprintf(buf, sizeof(buf), "%d", day);
        lv_label_set_text(c.num, buf);

        const bool is_today = has_today && view_year == today_year
            && view_month == today_month && day == today_day;
        const bool is_past = has_today && (view_year < today_year
            || (view_year == today_year && view_month < today_month)
            || (view_year == today_year && view_month == today_month && day < today_day));
        const int col = i % 7;  // 5-6 = samedi/dimanche

        // Priorités du numéro : aujourd'hui > passé > férié > weekend > normal
        uint32_t num_color = UIColor.TEXT_SOFT;
        if (col >= 5) num_color = UIColor.TEXT_DIM;
        if (code & CAL_BIT_FERIE) num_color = UIColor.ERROR;
        // Passé : TEXT_DIM, pas PAST (ardoise) — le 29 du mois, presque toute la grille
        // était en ardoise sur le bleu, illisible. Le fond plus pâle suffit à l'estomper.
        if (is_past) num_color = UIColor.TEXT_DIM;
        if (is_today) num_color = UIColor.ACCENT;
        lv_obj_set_style_text_color(c.num, lv_color_hex(num_color), LV_PART_MAIN);

        // Heures de travail dans la case (orange si embauche < 9h — même
        // convention que les tuiles / bandeau, estompé si jour passé)
        if (!heures.empty()) {
            lv_label_set_text(c.sub, heures.c_str());
            uint32_t h_color = UIColor.TEXT_SOFT;
            if (cal_is_early_shift(heures)) {
                h_color = UIColor.EARLY;
            }
            if (is_past) h_color = UIColor.TEXT_DIM;
            lv_obj_set_style_text_color(c.sub, lv_color_hex(h_color), LV_PART_MAIN);
        } else {
            lv_label_set_text(c.sub, "");
        }

        // Fond : violet doux = vacances scolaires, sinon verre (plus pâle si passé) pour
        // que chaque jour se lise comme une case ; bordure cyan = aujourd'hui. Opaque,
        // pré-mélangé sur la carte (cal_fond_case).
        const bool vacances = (code & CAL_BIT_VACANCES) != 0;
        lv_obj_set_style_bg_color(c.cell,
            cal_fond_case(lv_obj_get_parent(c.cell), rows_y + row * (row_h + gap) + row_h / 2,
                          vacances ? UIColor.ACCENT_ALT : UIColor.GLASS_RIM,
                          vacances ? LV_OPA_30 : (is_past ? LV_OPA_10 : LV_OPA_20)),
            LV_PART_MAIN);
        lv_obj_set_style_bg_opa(c.cell, LV_OPA_COVER, LV_PART_MAIN);
        lv_obj_set_style_border_opa(c.cell,
            is_today ? LV_OPA_COVER : LV_OPA_TRANSP, LV_PART_MAIN);

        if (code & CAL_BIT_RDV) lv_obj_remove_flag(c.dot, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(c.dot, LV_OBJ_FLAG_HIDDEN);
        if (code & CAL_BIT_ANNIV) lv_obj_remove_flag(c.dot2, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(c.dot2, LV_OBJ_FLAG_HIDDEN);
    }
}

std::string cal_date_for_cell(int view_year, int view_month, int cell_idx) {
    if (view_month < 1 || view_month > 12 || cell_idx < 0 || cell_idx >= 42) return "";
    const int day = cell_idx - cal_weekday_mon0(view_year, view_month, 1) + 1;
    if (day < 1 || day > cal_days_in_month(view_year, view_month)) return "";
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", view_year, view_month, day);
    return std::string(buf);
}

// Icône MDI + couleur d'une ligne de détail selon son type (payload HA).
static void cal_detail_type_style(const char* type, const char** icon, uint32_t* color) {
    if (strcmp(type, "travail") == 0)       { *icon = "\U000F00D6"; *color = UIColor.INFO; }
    else if (strcmp(type, "ferie") == 0)    { *icon = "\U000F1056"; *color = UIColor.ERROR; }
    else if (strcmp(type, "vacances") == 0) { *icon = "\U000F0474"; *color = UIColor.ACCENT_ALT; }
    else if (strcmp(type, "rdv") == 0)      { *icon = "\U000F00F0"; *color = UIColor.GOLD; }
    else if (strcmp(type, "anniv") == 0)    { *icon = "\U000F00EB"; *color = UIColor.WARM_PINK; }
    else if (strcmp(type, "fete") == 0)     { *icon = "\U000F09D3"; *color = UIColor.TEXT_DIM; }
    else                                    { *icon = "\U000F00F0"; *color = UIColor.TEXT_DIM; }
}

void cal_show_day_detail_loading(lv_obj_t* day_popup, lv_obj_t* lbl_title,
    lv_obj_t* lbl_status, CalDetailLineUI lines[6], const std::string& date_iso,
    bool ha_online) {
    if (!day_popup || !lbl_title || !lbl_status) return;
    int y = 0, m = 0, d = 0;
    if (sscanf(date_iso.c_str(), "%d-%d-%d", &y, &m, &d) != 3) return;

    // Même modèle que les titres de jours (tab5_core.cpp) : « Lundi 5 octobre »,
    // « Monday, October 5 » — ici sans « 1er », comme avant.
    const std::string titre = tr_fill("{jour} {quantieme} {mois}",
        {{"jour", cal_weekday_name_utf8(cal_weekday_mon0(y, m, d))},
         {"quantieme", std::to_string(d)},
         {"mois", cal_month_name_utf8(m)}});
    lv_label_set_text(lbl_title, titre.c_str());

    lv_label_set_text(lbl_status,
        tr(ha_online ? "Chargement..." : "Home Assistant hors ligne"));
    lv_obj_remove_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < 6; i++) {
        if (lines[i].icon) lv_obj_add_flag(lines[i].icon, LV_OBJ_FLAG_HIDDEN);
        if (lines[i].txt) lv_obj_add_flag(lines[i].txt, LV_OBJ_FLAG_HIDDEN);
    }
    lv_obj_remove_flag(day_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_to_index(day_popup, -1);
}

// Thèmes (ADR-0029) : icône et type de chaque ligne du détail affiché, pour
// cal_detail_rejouer() (nullptr : ligne vide).
static lv_obj_t* s_detail_icone[6] = {};
static char s_detail_type[6][12] = {};

void cal_detail_rejouer() {
    for (int i = 0; i < 6; i++) {
        if (s_detail_icone[i] == nullptr) continue;
        const char* icon;
        uint32_t color;
        cal_detail_type_style(s_detail_type[i], &icon, &color);
        lv_obj_set_style_text_color(s_detail_icone[i], lv_color_hex(color), LV_PART_MAIN);
    }
}

void cal_render_day_detail(const std::string& payload, lv_obj_t* lbl_status,
    CalDetailLineUI lines[6]) {
    if (!lbl_status) return;
    for (lv_obj_t*& o : s_detail_icone) o = nullptr;

    // Payload "type|texte;type|texte;..." construit par script.tab5_calendrier_jour
    // (HA) — textes déjà sanitisés (| et ; remplacés) et limités à 6 lignes.
    char buf[1024];
    strncpy(buf, payload.c_str(), sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    int line_count = 0;
    char* saveptr;
    char* tok = strtok_r(buf, ";", &saveptr);
    while (tok != nullptr && line_count < 6) {
        char* sep = strchr(tok, '|');
        if (sep != nullptr && *(sep + 1) != '\0'
            && lines[line_count].icon && lines[line_count].txt) {
            *sep = '\0';
            const char* icon;
            uint32_t color;
            cal_detail_type_style(tok, &icon, &color);
            lv_label_set_text(lines[line_count].icon, icon);
            lv_obj_set_style_text_color(lines[line_count].icon, lv_color_hex(color), LV_PART_MAIN);
            s_detail_icone[line_count] = lines[line_count].icon;
            snprintf(s_detail_type[line_count], sizeof(s_detail_type[0]), "%s", tok);
            std::string txt = normalize_text_utf8(std::string(sep + 1));
            // Ligne de travail : HA l'écrit en français, « Travail 08:00 – 16:00 »
            // (packages/tab5_calendar.yaml) ; le mot suit la langue de l'écran.
            if (strcmp(tok, "travail") == 0 && txt.compare(0, 7, "Travail") == 0 &&
                (txt.size() == 7 || txt[7] == ' ')) {
                txt.replace(0, 7, tr("Travail"));
            }
            set_label_text_utf8(lines[line_count].txt, txt.c_str());
            lv_obj_remove_flag(lines[line_count].icon, LV_OBJ_FLAG_HIDDEN);
            lv_obj_remove_flag(lines[line_count].txt, LV_OBJ_FLAG_HIDDEN);
            line_count++;
        }
        tok = strtok_r(nullptr, ";", &saveptr);
    }

    for (int i = line_count; i < 6; i++) {
        if (lines[i].icon) lv_obj_add_flag(lines[i].icon, LV_OBJ_FLAG_HIDDEN);
        if (lines[i].txt) lv_obj_add_flag(lines[i].txt, LV_OBJ_FLAG_HIDDEN);
    }

    if (line_count == 0) {
        lv_label_set_text(lbl_status, tr("Rien de pr\xC3\xA9vu ce jour"));
        lv_obj_remove_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(lbl_status, LV_OBJ_FLAG_HIDDEN);
    }
}
