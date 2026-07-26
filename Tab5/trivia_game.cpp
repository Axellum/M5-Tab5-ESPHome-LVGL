/**
 * [AI-CONTEXT]
 * @file trivia_game.cpp
 * @role Jeu « Trial Poursuite » — clone Trivial Pursuit rétro-salon.
 * @architecture_constraint Plein écran 1280x720. Le YAML ne fournit que 4
 *      conteneurs vides + 3 polices ; tout le reste est construit ici. Les objets
 *      LVGL sont PRÉALLOUÉS une seule fois (pool) puis réutilisés par show/hide :
 *      aucune allocation LVGL dans la boucle de jeu. Persistance NVS via
 *      esphome::global_preferences (aucune dépendance Home Assistant).
 * @ai_instruction Hot-path = tick() : pas de std::string, pas de to_string(), pas
 *      de new/delete. Les libellés HUD ne sont réécrits que quand leur valeur change.
 */
#include "trivia_game.h"
#include "trivia_questions.h"
#include "esphome/core/preferences.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>

namespace Trivia {

// ===========================================================================
// 1. Constantes & géométrie
// ===========================================================================
static constexpr int FW = 1280, FH = 672, HUD_H = 48;
static constexpr uint32_t SAVE_MAGIC = 0x54525631u;  // "TRV1"
static constexpr uint32_t PREF_KEY   = 0x54525641u;  // cle NVS dediee

// Couleurs des 6 catégories (palette TP classique)
static constexpr uint32_t CAT_COLORS[6] = {
    0x2196F3,  // Géographie — Bleu
    0xE91E63,  // Divertissement — Rose
    0xFFC107,  // Histoire — Jaune
    0x795548,  // Arts & Littérature — Marron
    0x4CAF50,  // Sciences & Nature — Vert
    0xFF9800   // Sports & Loisirs — Orange
};
static const char* CAT_NAMES[6] = {
    "Géographie", "Divertissement", "Histoire",
    "Arts & Litt.", "Sciences", "Sports"
};

// Couleurs pions équipes (6 couleurs distinctes)
static constexpr uint32_t PAWN_COLORS[6] = {
    0xFF5252, 0x448AFF, 0x69F0AE, 0xFFD740, 0xE040FB, 0xFF6E40
};

// Noms prédéfinis pour le setup
static const char* PRESET_NAMES[12] = {
    "Axel", "Marie", "Lucas", "Emma", "Hugo", "Léa",
    "Nathan", "Chloé", "Louis", "Jade", "Gabriel", "Alice"
};

// ===========================================================================
// 2. Générateur pseudo-aléatoire (xorshift32)
// ===========================================================================
static uint32_t s_rng = 0xDEADBEEFu;
static inline uint32_t rnd() {
    s_rng ^= s_rng << 13; s_rng ^= s_rng >> 17; s_rng ^= s_rng << 5;
    return s_rng;
}
static inline int rnd_range(int lo, int hi) {
    if (hi <= lo) return lo;
    return lo + (int)(rnd() % (uint32_t)(hi - lo + 1));
}

// ===========================================================================
// 3. États du jeu
// ===========================================================================
enum GameState : uint8_t {
    ST_HUB = 0, ST_SETUP, ST_PLAYING, ST_QUESTION,
    ST_FINAL, ST_VICTORY, ST_STATS, ST_RULES, ST_SETTINGS
};

// Types de cases du plateau
enum CellType : uint8_t {
    CELL_CAT = 0,   // case catégorie (couleur selon position)
    CELL_REROLL,    // relancer le dé
    CELL_CHOICE,    // catégorie au choix
    CELL_HQ         // centre / quartier général (finale)
};

// ===========================================================================
// 4. Plateau circulaire — 42 cases
// ===========================================================================
// Layout : 42 cases en cercle. Cases spéciales aux indices 0,7,14,21,28,35 (HQ/reroll/choice).
// Les autres cases sont des catégories (index % 6).
struct BoardCell {
    CellType type;
    uint8_t  cat;  // catégorie si CELL_CAT (0..5), sinon 0
};

static BoardCell s_board[TRIVIA_BOARD_SIZE];

// Construit le plateau une seule fois
static void build_board() {
    for (int i = 0; i < TRIVIA_BOARD_SIZE; i++) {
        if (i % 7 == 0) {
            // Case spéciale tous les 7 : alterne HQ, REROLL, CHOICE
            int sp = (i / 7) % 3;
            if (sp == 0) { s_board[i] = {CELL_HQ, 0}; }
            else if (sp == 1) { s_board[i] = {CELL_REROLL, 0}; }
            else { s_board[i] = {CELL_CHOICE, 0}; }
        } else {
            s_board[i] = {CELL_CAT, (uint8_t)(i % 6)};
        }
    }
}

// Coordonnées d'une case sur le cercle (centre plateau 640,336)
static void cell_pos(int idx, int* x, int* y) {
    float angle = (float)idx * 2.0f * 3.14159265f / (float)TRIVIA_BOARD_SIZE - 1.5708f;
    *x = 640 + (int)(280.0f * cosf(angle));
    *y = 336 + (int)(280.0f * sinf(angle));
}

// ===========================================================================
// 5. État de partie
// ===========================================================================
struct Team {
    char    name[16];
    uint8_t color_idx;
    uint8_t wedges;   // masque 6 bits (part gagnée par catégorie)
    uint8_t pos;      // position plateau 0..41
};

static GameState  s_state = ST_HUB;
static uint8_t    s_n_teams = 0;
static uint8_t    s_cur_team = 0;
static uint8_t    s_turn = 1;
static uint8_t    s_difficulty = 0;   // 0=Mixte 1=Facile+Moyen 2=Tout
static uint8_t    s_timer_sec = 30;
static Team       s_teams[TRIVIA_MAX_TEAMS];

// Sac de questions par catégorie (shuffle sans remise)
static uint8_t s_bag_idx[TRIVIA_NCAT];       // curseur courant
static uint8_t s_bag_order[TRIVIA_NCAT][150]; // indices mélangés (max 150/cat)

// Question en cours
static const TriviaQuestion* s_cur_q = nullptr;
static uint8_t s_choices[4];    // ordre mélangé des 4 réponses (0=bonne,1-3=leurres)
static uint8_t s_correct_slot;  // slot de la bonne réponse dans s_choices
static bool    s_answered = false;
static bool    s_last_correct = false;

// Timer question
static uint32_t s_q_start_ms = 0;
static uint32_t s_q_remaining_ms = 0;

// Dé
static uint8_t s_dice_val = 0;
static bool    s_dice_rolling = false;
static uint32_t s_dice_start_ms = 0;

// IMU shake
static float    s_imu_mag = 0.0f;
static uint32_t s_last_shake_ms = 0;

// ===========================================================================
// 6. UI — pointeurs et objets préalloués
// ===========================================================================
static UI s_ui;
static bool s_open = false;
static lv_timer_t* s_timer = nullptr;

// Objets LVGL préalloués (construits une fois)
static lv_obj_t* s_hub_panel = nullptr;
static lv_obj_t* s_setup_panel = nullptr;
static lv_obj_t* s_q_modal = nullptr;
static lv_obj_t* s_q_label = nullptr;
static lv_obj_t* s_q_btns[4] = {};
static lv_obj_t* s_q_btn_labels[4] = {};
static lv_obj_t* s_q_timer_bar = nullptr;
static lv_obj_t* s_hud_team = nullptr;
static lv_obj_t* s_hud_dice = nullptr;
static lv_obj_t* s_hud_turn = nullptr;
static lv_obj_t* s_hud_timer = nullptr;
static lv_obj_t* s_board_cells[TRIVIA_BOARD_SIZE] = {};
static lv_obj_t* s_pawns[TRIVIA_MAX_TEAMS] = {};
static lv_obj_t* s_wedges_ui[TRIVIA_MAX_TEAMS] = {};
static lv_obj_t* s_victory_panel = nullptr;
static lv_obj_t* s_dice_btn = nullptr;
static bool s_ui_built = false;

// Buffers texte (pas d'alloc dans le tick)
static char s_buf[128];

// ===========================================================================
// 7. Tirage de questions — shuffle sans remise par catégorie
// ===========================================================================

// Initialise les sacs pour une nouvelle partie
static void init_bags() {
    for (int c = 0; c < TRIVIA_NCAT; c++) {
        uint8_t n = TRIVIA_BANK_SIZES[c];
        for (int i = 0; i < n; i++) s_bag_order[c][i] = (uint8_t)i;
        // Fisher-Yates shuffle
        for (int i = n - 1; i > 0; i--) {
            int j = rnd_range(0, i);
            uint8_t tmp = s_bag_order[c][i];
            s_bag_order[c][i] = s_bag_order[c][j];
            s_bag_order[c][j] = tmp;
        }
        s_bag_idx[c] = 0;
    }
}

// Tire une question d'une catégorie avec filtre de difficulté
// diff_mask : 0x07=tout, 0x03=facile+moyen, 0x01=facile seul
static const TriviaQuestion* draw_question(uint8_t cat, uint8_t diff_mask) {
    uint8_t n = TRIVIA_BANK_SIZES[cat];
    const TriviaQuestion* bank = TRIVIA_BANKS[cat];
    // Cherche dans le sac à partir du curseur
    for (int attempt = 0; attempt < n; attempt++) {
        uint8_t idx = s_bag_order[cat][(s_bag_idx[cat] + attempt) % n];
        uint8_t d = bank[idx].difficulty;
        if ((diff_mask >> d) & 1) {
            s_bag_idx[cat] = (s_bag_idx[cat] + attempt + 1) % n;
            return &bank[idx];
        }
    }
    // Fallback : première question dispo
    uint8_t idx = s_bag_order[cat][s_bag_idx[cat] % n];
    s_bag_idx[cat] = (s_bag_idx[cat] + 1) % n;
    return &bank[idx];
}

// Masque de difficulté selon réglage
static uint8_t diff_mask() {
    if (s_difficulty == 0) return 0x07;  // Mixte
    if (s_difficulty == 1) return 0x03;  // Facile + Moyen
    return 0x07;  // Tout
}

// ===========================================================================
// 8. Préparation d'une question (mélange des 4 choix)
// ===========================================================================
static void prepare_question(uint8_t cat) {
    s_cur_q = draw_question(cat, diff_mask());
    if (!s_cur_q) return;
    // Mélange l'ordre des 4 réponses (seed = question index + tour)
    s_choices[0] = 0; s_choices[1] = 1; s_choices[2] = 2; s_choices[3] = 3;
    for (int i = 3; i > 0; i--) {
        int j = rnd_range(0, i);
        uint8_t tmp = s_choices[i]; s_choices[i] = s_choices[j]; s_choices[j] = tmp;
    }
    // Trouve le slot de la bonne réponse
    for (int i = 0; i < 4; i++) {
        if (s_choices[i] == 0) { s_correct_slot = (uint8_t)i; break; }
    }
    s_answered = false;
    s_q_start_ms = millis();
    s_q_remaining_ms = (s_timer_sec > 0) ? (uint32_t)s_timer_sec * 1000 : 0xFFFFFFFF;
}

// Récupère le texte d'un choix (0=bonne, 1-3=leurres)
static const char* choice_text(uint8_t slot) {
    uint8_t c = s_choices[slot];
    if (c == 0) return s_cur_q->a;
    if (c == 1) return s_cur_q->w1;
    if (c == 2) return s_cur_q->w2;
    return s_cur_q->w3;
}

// ===========================================================================
// 9. Logique de jeu — résolution d'une case
// ===========================================================================

// Passe à l'équipe suivante
static void next_team() {
    s_cur_team = (s_cur_team + 1) % s_n_teams;
    if (s_cur_team == 0) s_turn++;
}

// Vérifie si une équipe a les 6 parts
static bool has_all_wedges(uint8_t team) {
    return s_teams[team].wedges == 0x3F;
}

// Résout la case où le pion vient d'arriver
static void resolve_cell() {
    uint8_t pos = s_teams[s_cur_team].pos;
    BoardCell& cell = s_board[pos];

    if (cell.type == CELL_HQ) {
        // Case centre : si camembert complet → finale
        if (has_all_wedges(s_cur_team)) {
            s_state = ST_FINAL;
            // TODO: afficher modal finale
        } else {
            // Pas encore complet : rejouer
            s_state = ST_PLAYING;
        }
    } else if (cell.type == CELL_REROLL) {
        // Relancer le dé gratuitement
        s_state = ST_PLAYING;
    } else if (cell.type == CELL_CHOICE) {
        // Catégorie au choix → on tire une question d'une catégorie au hasard
        uint8_t cat = (uint8_t)rnd_range(0, 5);
        prepare_question(cat);
        s_state = ST_QUESTION;
    } else {
        // Case catégorie
        prepare_question(cell.cat);
        s_state = ST_QUESTION;
    }
}

// Traite la réponse du joueur
static void handle_answer(uint8_t slot) {
    if (s_answered || !s_cur_q) return;
    s_answered = true;
    s_last_correct = (slot == s_correct_slot);

    // Stats globales
    // (sera sauvegardé au close)

    if (s_last_correct) {
        // Bonne réponse : gagne une part si case catégorie et part pas encore gagnée
        uint8_t pos = s_teams[s_cur_team].pos;
        BoardCell& cell = s_board[pos];
        if (cell.type == CELL_CAT) {
            uint8_t cat = cell.cat;
            if (!(s_teams[s_cur_team].wedges & (1 << cat))) {
                s_teams[s_cur_team].wedges |= (1 << cat);
            }
        }
        // Rejouer (comme TP)
        s_state = ST_PLAYING;
    } else {
        // Mauvaise réponse : équipe suivante
        next_team();
        s_state = ST_PLAYING;
    }
}

// ===========================================================================
// 10. Dé — lancer et animation
// ===========================================================================
static void roll_dice() {
    if (s_state != ST_PLAYING || s_dice_rolling) return;
    s_dice_rolling = true;
    s_dice_start_ms = millis();
    s_dice_val = (uint8_t)rnd_range(1, 6);
}

// Animation du dé (500 ms de roulement)
static void tick_dice() {
    if (!s_dice_rolling) return;
    uint32_t elapsed = millis() - s_dice_start_ms;
    if (elapsed < 500) {
        // Affiche une valeur aléatoire pendant l'animation
        s_dice_val = (uint8_t)rnd_range(1, 6);
    } else {
        // Fin d'animation : valeur finale
        s_dice_val = (uint8_t)(1 + (s_dice_val - 1) % 6);
        s_dice_rolling = false;
        // Déplace le pion
        s_teams[s_cur_team].pos = (s_teams[s_cur_team].pos + s_dice_val) % TRIVIA_BOARD_SIZE;
        // Résout la case
        resolve_cell();
    }
}

// ===========================================================================
// 11. IMU — détection de secousse (lancer le dé)
// ===========================================================================
void on_imu(float ax, float ay, float az) {
    float mag = sqrtf(ax*ax + ay*ay + az*az);
    s_imu_mag = mag;
    // Secousse franche > 2.2 g, debounce 800 ms
    uint32_t now = millis();
    if (mag > 2.2f && (now - s_last_shake_ms) > 800) {
        s_last_shake_ms = now;
        if (s_open && s_state == ST_PLAYING && !s_dice_rolling) {
            roll_dice();
        }
    }
}

// ===========================================================================
// 12. Persistance NVS
// ===========================================================================
static TriviaSave s_save;
static esphome::ESPPreferenceObject s_pref;
static bool s_prefs_init = false;

void persist_load() {
    if (!s_prefs_init) {
        s_pref = esphome::global_preferences->make_preference<TriviaSave>(PREF_KEY);
        s_prefs_init = true;
    }
    // ESPPreferences ne propose pas get/put (c'est l'API Arduino) : on passe
    // par un ESPPreferenceObject type, comme les 7 autres consoles du Tab.
    if (!s_pref.load(&s_save) || s_save.magic != SAVE_MAGIC) {
        memset(&s_save, 0, sizeof(s_save));
        s_save.magic = SAVE_MAGIC;
    }
}

void persist_save() {
    if (!s_prefs_init) persist_load();
    s_save.magic = SAVE_MAGIC;
    // Sauvegarde la partie en cours
    s_save.n_teams = s_n_teams;
    s_save.current_team = s_cur_team;
    s_save.turn_num = s_turn;
    s_save.difficulty = s_difficulty;
    s_save.timer_sec = s_timer_sec;
    s_save.rng_seed = s_rng;
    for (int i = 0; i < s_n_teams; i++) {
        strncpy(s_save.teams[i].name, s_teams[i].name, 15);
        s_save.teams[i].color_idx = s_teams[i].color_idx;
        s_save.teams[i].wedges = s_teams[i].wedges;
        s_save.teams[i].pos = s_teams[i].pos;
    }
    s_pref.save(&s_save);
    esphome::global_preferences->sync();
}

// ===========================================================================
// 13. Construction UI (une seule fois)
// ===========================================================================
static lv_obj_t* make_label(lv_obj_t* parent, const char* txt, int x, int y,
                            const esphome::font::Font* f, uint32_t color) {
    lv_obj_t* l = lv_label_create(parent);
    lv_label_set_text(l, txt);
    lv_obj_set_pos(l, x, y);
    esphome::lvgl::lv_obj_set_style_text_font(l, f, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
    return l;
}

static lv_obj_t* make_btn(lv_obj_t* parent, const char* txt, int x, int y, int w, int h,
                          uint32_t bg, const esphome::font::Font* f) {
    lv_obj_t* b = lv_obj_create(parent);
    lv_obj_remove_style_all(b);
    lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_clear_flag(b, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(b, x, y);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(bg), 0);
    lv_obj_set_style_bg_opa(b, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_t* l = lv_label_create(b);
    lv_label_set_text(l, txt);
    lv_obj_center(l);
    esphome::lvgl::lv_obj_set_style_text_font(l, f, LV_PART_MAIN);
    lv_obj_set_style_text_color(l, lv_color_hex(0xFFFFFF), 0);
    return b;
}

// Callback générique bouton hub
static void hub_cb(lv_event_t* e) {
    int action = (int)(intptr_t)lv_event_get_user_data(e);
    switch (action) {
        case 0: // Nouvelle partie → setup
            s_state = ST_SETUP;
            break;
        case 1: // Reprendre
            if (s_save.n_teams > 0) {
                // Restaure la partie
                s_n_teams = s_save.n_teams;
                s_cur_team = s_save.current_team;
                s_turn = s_save.turn_num;
                s_difficulty = s_save.difficulty;
                s_timer_sec = s_save.timer_sec;
                s_rng = s_save.rng_seed;
                for (int i = 0; i < s_n_teams; i++) {
                    strncpy(s_teams[i].name, s_save.teams[i].name, 15);
                    s_teams[i].color_idx = s_save.teams[i].color_idx;
                    s_teams[i].wedges = s_save.teams[i].wedges;
                    s_teams[i].pos = s_save.teams[i].pos;
                }
                init_bags();
                s_state = ST_PLAYING;
            }
            break;
        case 2: s_state = ST_STATS; break;
        case 3: s_state = ST_RULES; break;
        case 4: s_state = ST_SETTINGS; break;
        case 5: close(); break;  // Quitter
    }
}

// Callback bouton dé
static void dice_cb(lv_event_t* e) {
    (void)e;
    roll_dice();
}

// Callback réponse question
static void answer_cb(lv_event_t* e) {
    int slot = (int)(intptr_t)lv_event_get_user_data(e);
    handle_answer((uint8_t)slot);
}

static void build_ui() {
    if (s_ui_built) return;
    s_ui_built = true;

    lv_obj_t* root = s_ui.root;
    lv_obj_t* hud = s_ui.hud;
    lv_obj_t* board = s_ui.board;
    lv_obj_t* panel = s_ui.panel;

    // --- HUD ---
    s_hud_team = make_label(hud, "Équipe: -", 10, 12, s_ui.f_small, 0xFFFFFF);
    s_hud_dice = make_label(hud, "Dé: -", 400, 12, s_ui.f_small, 0xFFD740);
    s_hud_turn = make_label(hud, "Tour: 1", 600, 12, s_ui.f_small, 0xAAAAAA);
    s_hud_timer = make_label(hud, "", 900, 12, s_ui.f_small, 0xFF5252);

    // --- Plateau : cases circulaires ---
    build_board();
    for (int i = 0; i < TRIVIA_BOARD_SIZE; i++) {
        int cx, cy;
        cell_pos(i, &cx, &cy);
        lv_obj_t* cell = lv_obj_create(board);
        lv_obj_set_pos(cell, cx - 18, cy - 18);
        lv_obj_set_size(cell, 36, 36);
        lv_obj_set_style_radius(cell, 18, 0);
        lv_obj_set_style_border_width(cell, 2, 0);
        uint32_t col = 0x333333;
        if (s_board[i].type == CELL_CAT) col = CAT_COLORS[s_board[i].cat];
        else if (s_board[i].type == CELL_HQ) col = 0xFFFFFF;
        else if (s_board[i].type == CELL_REROLL) col = 0x9C27B0;
        else col = 0x00BCD4;
        lv_obj_set_style_bg_color(cell, lv_color_hex(col), 0);
        lv_obj_set_style_bg_opa(cell, LV_OPA_70, 0);
        lv_obj_set_style_border_color(cell, lv_color_hex(0x555555), 0);
        lv_obj_clear_flag(cell, LV_OBJ_FLAG_SCROLLABLE);
        s_board_cells[i] = cell;
    }

    // Pions
    for (int i = 0; i < TRIVIA_MAX_TEAMS; i++) {
        lv_obj_t* p = lv_obj_create(board);
        lv_obj_set_size(p, 20, 20);
        lv_obj_set_style_radius(p, 10, 0);
        lv_obj_set_style_bg_color(p, lv_color_hex(PAWN_COLORS[i]), 0);
        lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(p, 2, 0);
        lv_obj_set_style_border_color(p, lv_color_hex(0xFFFFFF), 0);
        lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_add_flag(p, LV_OBJ_FLAG_HIDDEN);
        s_pawns[i] = p;
    }

    // Bouton dé (centre du plateau)
    s_dice_btn = make_btn(board, "DÉ", 590, 306, 100, 60, 0x333333, s_ui.f_mid);
    lv_obj_add_event_cb(s_dice_btn, dice_cb, LV_EVENT_CLICKED, nullptr);

    // --- Panel (hub / question / victoire) ---
    // Hub
    s_hub_panel = lv_obj_create(panel);
    lv_obj_set_size(s_hub_panel, 500, 500);
    lv_obj_align(s_hub_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_hub_panel, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(s_hub_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_hub_panel, 20, 0);
    lv_obj_set_style_border_width(s_hub_panel, 0, 0);
    lv_obj_clear_flag(s_hub_panel, LV_OBJ_FLAG_SCROLLABLE);

    make_label(s_hub_panel, "TRIAL POURSUITE", 80, 20, s_ui.f_big, 0xFFD740);
    const char* hub_btns[] = {"Nouvelle partie", "Reprendre", "Stats", "Règles", "Réglages", "Quitter"};
    for (int i = 0; i < 6; i++) {
        lv_obj_t* b = make_btn(s_hub_panel, hub_btns[i], 100, 90 + i * 65, 300, 50,
                               (i == 5) ? 0x8B0000 : 0x2E4057, s_ui.f_small);
        lv_obj_add_event_cb(b, hub_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    // Modal question
    s_q_modal = lv_obj_create(panel);
    lv_obj_set_size(s_q_modal, 1100, 550);
    lv_obj_align(s_q_modal, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_color(s_q_modal, lv_color_hex(0x0D1B2A), 0);
    lv_obj_set_style_bg_opa(s_q_modal, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_q_modal, 20, 0);
    lv_obj_set_style_border_width(s_q_modal, 3, 0);
    lv_obj_set_style_border_color(s_q_modal, lv_color_hex(0x444444), 0);
    lv_obj_clear_flag(s_q_modal, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_q_modal, LV_OBJ_FLAG_HIDDEN);

    s_q_label = make_label(s_q_modal, "", 40, 30, s_ui.f_mid, 0xFFFFFF);
    lv_obj_set_width(s_q_label, 1020);
    lv_label_set_long_mode(s_q_label, LV_LABEL_LONG_WRAP);

    // 4 boutons réponse (grille 2x2)
    for (int i = 0; i < 4; i++) {
        int bx = 40 + (i % 2) * 530;
        int by = 250 + (i / 2) * 130;
        s_q_btns[i] = lv_obj_create(s_q_modal);
        lv_obj_remove_style_all(s_q_btns[i]);
        lv_obj_add_flag(s_q_btns[i], LV_OBJ_FLAG_CLICKABLE);
        lv_obj_clear_flag(s_q_btns[i], LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(s_q_btns[i], bx, by);
        lv_obj_set_size(s_q_btns[i], 500, 110);
        lv_obj_set_style_bg_color(s_q_btns[i], lv_color_hex(0x2E4057), 0);
        lv_obj_set_style_bg_opa(s_q_btns[i], LV_OPA_COVER, 0);
        lv_obj_set_style_radius(s_q_btns[i], 14, 0);
        s_q_btn_labels[i] = lv_label_create(s_q_btns[i]);
        lv_label_set_text(s_q_btn_labels[i], "");
        lv_obj_center(s_q_btn_labels[i]);
        esphome::lvgl::lv_obj_set_style_text_font(s_q_btn_labels[i], s_ui.f_small, LV_PART_MAIN);
        lv_obj_set_style_text_color(s_q_btn_labels[i], lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_width(s_q_btn_labels[i], 470);
        lv_label_set_long_mode(s_q_btn_labels[i], LV_LABEL_LONG_WRAP);
        lv_obj_add_event_cb(s_q_btns[i], answer_cb, LV_EVENT_CLICKED, (void*)(intptr_t)i);
    }

    // Barre timer
    s_q_timer_bar = lv_obj_create(s_q_modal);
    lv_obj_set_pos(s_q_timer_bar, 40, 200);
    lv_obj_set_size(s_q_timer_bar, 1020, 12);
    lv_obj_set_style_bg_color(s_q_timer_bar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_opa(s_q_timer_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_q_timer_bar, 6, 0);
    lv_obj_set_style_border_width(s_q_timer_bar, 0, 0);
    lv_obj_clear_flag(s_q_timer_bar, LV_OBJ_FLAG_SCROLLABLE);

    // Panel victoire
    s_victory_panel = lv_obj_create(panel);
    lv_obj_set_size(s_victory_panel, 600, 400);
    lv_obj_align(s_victory_panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(s_victory_panel, lv_color_hex(0x1A1A2E), 0);
    lv_obj_set_style_bg_opa(s_victory_panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(s_victory_panel, 20, 0);
    lv_obj_set_style_border_width(s_victory_panel, 0, 0);
    lv_obj_clear_flag(s_victory_panel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_victory_panel, LV_OBJ_FLAG_HIDDEN);
    make_label(s_victory_panel, "VICTOIRE !", 180, 30, s_ui.f_big, 0xFFD740);
}

// ===========================================================================
// 14. Mise à jour UI (appelée à chaque tick)
// ===========================================================================
static void update_ui() {
    // Visibilité des panneaux selon l'état
    bool show_hub = (s_state == ST_HUB || s_state == ST_STATS || s_state == ST_RULES || s_state == ST_SETTINGS);
    bool show_q = (s_state == ST_QUESTION || s_state == ST_FINAL);
    bool show_victory = (s_state == ST_VICTORY);

    if (s_hub_panel) {
        if (show_hub) lv_obj_clear_flag(s_hub_panel, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_hub_panel, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_q_modal) {
        if (show_q) lv_obj_clear_flag(s_q_modal, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_q_modal, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_victory_panel) {
        if (show_victory) lv_obj_clear_flag(s_victory_panel, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_victory_panel, LV_OBJ_FLAG_HIDDEN);
    }
    if (s_dice_btn) {
        if (s_state == ST_PLAYING) lv_obj_clear_flag(s_dice_btn, LV_OBJ_FLAG_HIDDEN);
        else lv_obj_add_flag(s_dice_btn, LV_OBJ_FLAG_HIDDEN);
    }

    // HUD
    if (s_n_teams > 0 && s_hud_team) {
        snprintf(s_buf, sizeof(s_buf), "Équipe: %s", s_teams[s_cur_team].name);
        lv_label_set_text(s_hud_team, s_buf);
        lv_obj_set_style_text_color(s_hud_team, lv_color_hex(PAWN_COLORS[s_teams[s_cur_team].color_idx]), 0);
    }
    if (s_hud_dice) {
        if (s_dice_rolling) snprintf(s_buf, sizeof(s_buf), "Dé: ...");
        else snprintf(s_buf, sizeof(s_buf), "Dé: %d", s_dice_val);
        lv_label_set_text(s_hud_dice, s_buf);
    }
    if (s_hud_turn) {
        snprintf(s_buf, sizeof(s_buf), "Tour: %d", s_turn);
        lv_label_set_text(s_hud_turn, s_buf);
    }

    // Pions sur le plateau
    for (int i = 0; i < s_n_teams; i++) {
        int cx, cy;
        cell_pos(s_teams[i].pos, &cx, &cy);
        // Offset pour ne pas superposer
        int ox = (i % 3) * 8 - 8;
        int oy = (i / 3) * 8 - 4;
        lv_obj_set_pos(s_pawns[i], cx - 10 + ox, cy - 10 + oy);
        lv_obj_clear_flag(s_pawns[i], LV_OBJ_FLAG_HIDDEN);
    }
    for (int i = s_n_teams; i < TRIVIA_MAX_TEAMS; i++) {
        lv_obj_add_flag(s_pawns[i], LV_OBJ_FLAG_HIDDEN);
    }

    // Modal question
    if (show_q && s_cur_q) {
        lv_label_set_text(s_q_label, s_cur_q->q);
        for (int i = 0; i < 4; i++) {
            lv_label_set_text(s_q_btn_labels[i], choice_text((uint8_t)i));
            // Couleur feedback après réponse
            if (s_answered) {
                uint32_t col = (i == s_correct_slot) ? 0x4CAF50 : 0x8B0000;
                lv_obj_set_style_bg_color(s_q_btns[i], lv_color_hex(col), 0);
            } else {
                lv_obj_set_style_bg_color(s_q_btns[i], lv_color_hex(0x2E4057), 0);
            }
        }
        // Timer bar
        if (s_q_remaining_ms != 0xFFFFFFFF) {
            uint32_t elapsed = millis() - s_q_start_ms;
            uint32_t total = (uint32_t)s_timer_sec * 1000;
            int pct = (elapsed < total) ? (int)(100 - elapsed * 100 / total) : 0;
            lv_obj_set_width(s_q_timer_bar, (lv_coord_t)(1020 * pct / 100));
            if (pct > 50) lv_obj_set_style_bg_color(s_q_timer_bar, lv_color_hex(0x4CAF50), 0);
            else if (pct > 20) lv_obj_set_style_bg_color(s_q_timer_bar, lv_color_hex(0xFFC107), 0);
            else lv_obj_set_style_bg_color(s_q_timer_bar, lv_color_hex(0xFF5252), 0);
            // Timeout = mauvaise réponse
            if (elapsed >= total && !s_answered) {
                handle_answer(255);  // slot invalide = timeout
            }
        }
        // Timer HUD
        if (s_hud_timer && s_q_remaining_ms != 0xFFFFFFFF) {
            uint32_t elapsed = millis() - s_q_start_ms;
            int sec_left = (int)((s_timer_sec * 1000 - elapsed) / 1000);
            if (sec_left < 0) sec_left = 0;
            snprintf(s_buf, sizeof(s_buf), "Temps: %ds", sec_left);
            lv_label_set_text(s_hud_timer, s_buf);
        }
    } else if (s_hud_timer) {
        lv_label_set_text(s_hud_timer, "");
    }
}

// ===========================================================================
// 15. Tick principal (lv_timer ~30 Hz)
// ===========================================================================
static void tick(lv_timer_t* t) {
    (void)t;
    if (!s_open) return;

    // Animation dé
    tick_dice();

    // Mise à jour UI
    update_ui();
}

// ===========================================================================
// 16. API publique
// ===========================================================================
void open(const UI& ui) {
    s_ui = ui;
    if (!s_prefs_init) persist_load();
    build_ui();

    s_open = true;
    s_state = ST_HUB;
    lv_obj_clear_flag(s_ui.root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_ui.root);

    // Démarre le timer de gameplay (33 ms ≈ 30 Hz)
    if (!s_timer) {
        s_timer = lv_timer_create(tick, 33, nullptr);
    }
}

void close() {
    if (!s_open) return;
    s_open = false;
    // Sauvegarde
    persist_save();
    // Arrête le timer
    if (s_timer) {
        lv_timer_delete(s_timer);
        s_timer = nullptr;
    }
    // Masque l'overlay
    lv_obj_add_flag(s_ui.root, LV_OBJ_FLAG_HIDDEN);
}

bool is_open() { return s_open; }

}  // namespace Trivia
