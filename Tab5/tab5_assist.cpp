/**
 * [AI-CONTEXT]
 * @file tab5_assist.cpp
 * @role Assistant vocal : rendu Markdown de la réponse, états du pipeline (icône micro,
 *       libellé), zone image, indicateur Ok Nabu ; décision pure du mot de réveil
 *       (WakeWord::decide, table du .h).
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
// Popup Assistant vocal (assistant_popup.yaml) — helpers de rendu
// =============================================================================

// Longueur en points de code UTF-8 (aligne les colonnes des tableaux en monospace,
// où un caractère accenté = 2/3 octets mais 1 seule cellule visuelle).
static size_t assist_utf8_cp_len(const std::string& s) {
    size_t n = 0;
    for (unsigned char c : s) if ((c & 0xC0) != 0x80) n++;
    return n;
}

// Retire les marqueurs Markdown inline (**gras**, __gras__, `code`, *ital*, ~barré~).
static std::string assist_strip_inline_md(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size();) {
        if (i + 1 < in.size() &&
            ((in[i] == '*' && in[i + 1] == '*') || (in[i] == '_' && in[i + 1] == '_'))) {
            i += 2; continue;  // **gras** / __gras__
        }
        char c = in[i];
        if (c == '`' || c == '*' || c == '_' || c == '~') { i++; continue; }
        out += c; i++;
    }
    return out;
}

// Ligne séparatrice de tableau Markdown : |---|:--:|--| (uniquement - : | espaces).
static bool assist_is_table_sep(const std::string& line) {
    bool dash = false;
    for (char c : line) {
        if (c == '-') dash = true;
        else if (c == '|' || c == ':' || c == ' ' || c == '\t') continue;
        else return false;
    }
    return dash;
}

// Éclate une ligne de tableau en cellules (gère les pipes de bord + nettoie chaque cellule).
static std::vector<std::string> assist_split_cells(const std::string& row) {
    std::string r = trim_ws(row);
    if (!r.empty() && r.front() == '|') r.erase(r.begin());
    if (!r.empty() && r.back() == '|') r.pop_back();
    std::vector<std::string> cells;
    std::string cur;
    for (char c : r) {
        if (c == '|') { cells.push_back(trim_ws(assist_strip_inline_md(cur))); cur.clear(); }
        else cur += c;
    }
    cells.push_back(trim_ws(assist_strip_inline_md(cur)));
    return cells;
}

// Nettoie un texte Markdown "léger" pour affichage LVGL. [AI-WARNING] Écrit pour
// une police à chasse fixe : depuis l'essai D8 du 26/09/2026, la réponse est en
// roboto_32_b / roboto_45_b (proportionnelles), l'alignement des tableaux par
// espaces n'est donc plus qu'approximatif.
//  - retire les marqueurs **gras**, __gras__, `code`, les # de titres ;
//  - convertit les puces "- " / "* " en "• " ;
//  - ré-aligne les tableaux Markdown (colonnes séparées par |) en largeur fixe
//    (comptage en points de code UTF-8, pas en octets) et supprime la ligne
//    séparatrice |---|---|. Rend les tableaux lisibles sans moteur de rendu.
static std::string format_assist_markdown(const std::string& in) {
    // Découpe en lignes (ignore les \r).
    std::vector<std::string> lines;
    std::string cur;
    for (char c : in) {
        if (c == '\n') { lines.push_back(cur); cur.clear(); }
        else if (c != '\r') cur += c;
    }
    lines.push_back(cur);

    std::string out;
    for (size_t i = 0; i < lines.size();) {
        const std::string& raw = lines[i];
        bool is_row = raw.find('|') != std::string::npos;

        // Bloc tableau : au moins 2 lignes consécutives contenant un '|'.
        if (is_row && i + 1 < lines.size() && lines[i + 1].find('|') != std::string::npos) {
            size_t j = i;
            std::vector<std::vector<std::string>> rows;
            while (j < lines.size() && lines[j].find('|') != std::string::npos) {
                if (!assist_is_table_sep(lines[j])) rows.push_back(assist_split_cells(lines[j]));
                j++;
            }
            // Largeur (en points de code) de chaque colonne.
            std::vector<size_t> width;
            for (auto& r : rows)
                for (size_t k = 0; k < r.size(); k++) {
                    size_t l = assist_utf8_cp_len(r[k]);
                    if (k >= width.size()) width.push_back(l);
                    else if (l > width[k]) width[k] = l;
                }
            // Ré-émission alignée (2 espaces entre colonnes).
            for (auto& r : rows) {
                std::string line;
                for (size_t k = 0; k < r.size(); k++) {
                    line += r[k];
                    size_t have = assist_utf8_cp_len(r[k]);
                    size_t pad = (k < width.size()) ? width[k] : 0;
                    if (k + 1 < r.size() && have < pad) line.append(pad - have, ' ');
                    if (k + 1 < r.size()) line += "  ";
                }
                out += line; out += "\n";
            }
            i = j;
            continue;
        }

        // Ligne normale : titres (#), puces (- * +), marqueurs inline.
        std::string s = raw;
        size_t a = s.find_first_not_of(" \t");
        if (a != std::string::npos && s[a] == '#') {
            size_t h = a;
            while (h < s.size() && s[h] == '#') h++;
            while (h < s.size() && s[h] == ' ') h++;
            s = s.substr(h);
            a = s.find_first_not_of(" \t");
        }
        if (a != std::string::npos && (s[a] == '-' || s[a] == '*' || s[a] == '+') &&
            a + 1 < s.size() && s[a + 1] == ' ') {
            s = s.substr(0, a) + "\xE2\x80\xA2 " + s.substr(a + 2);  // "• "
        }
        s = assist_strip_inline_md(s);
        out += s; out += "\n";
        i++;
    }
    if (!out.empty() && out.back() == '\n') out.pop_back();
    return out;
}

void assist_set_request(lv_obj_t* lbl_request, const std::string& texte) {
    if (!lbl_request) return;
    std::string t = trim_ws(normalize_text_utf8(texte));
    lv_label_set_text(lbl_request, t.c_str());
}

void assist_set_response(lv_obj_t* lbl_response, const std::string& texte,
    esphome::font::Font* font) {
    if (!lbl_response) return;
    // Réponse du moteur non bornée : format_assist_markdown() alloue une chaîne par
    // ligne et par cellule sur le tas INTERNE (malloc ne va pas en PSRAM ici) et le
    // fragmente. 4 Ko couvrent largement ce que la zone de texte affiche ; au-delà on
    // coupe sur une frontière de caractère UTF-8 (audit du 25/09/2026, lot 4).
    constexpr size_t kAssistMaxBytes = 4096;
    std::string src = normalize_text_utf8(texte);
    if (src.size() > kAssistMaxBytes) {
        size_t cut = kAssistMaxBytes;
        while (cut > 0 && (static_cast<unsigned char>(src[cut]) & 0xC0) == 0x80) cut--;
        src.resize(cut);
        src += "\n[...]";
    }
    std::string t = format_assist_markdown(src);
    if (font) esphome::lvgl::lv_obj_set_style_text_font(lbl_response, font, LV_PART_MAIN);
    lv_label_set_recolor(lbl_response, false);
    lv_label_set_text(lbl_response, t.c_str());
}

// Même table que le ternaire qu'elle remplace dans tab5-assist.yaml (lot 7.3 de
// l'audit du 26/09/2026). Deux tailles depuis l'essai D8 (même date) : 2 → L,
// toute autre valeur → S — le 1 de l'ancien M, encore restauré chez qui l'avait
// choisi, retombe sur S. Même règle dans assist_apply_text_size().
esphome::font::Font* assist_font(int size_idx, esphome::font::Font* f_s, esphome::font::Font* f_l) {
    return size_idx >= 2 ? f_l : f_s;
}

void assist_apply_text_size(lv_obj_t* lbl_response, int size_idx,
    esphome::font::Font* f_s, esphome::font::Font* f_l, lv_obj_t* btn_s, lv_obj_t* btn_l) {
    const bool large = size_idx >= 2;
    esphome::font::Font* f = assist_font(size_idx, f_s, f_l);
    if (lbl_response && f) esphome::lvgl::lv_obj_set_style_text_font(lbl_response, f, LV_PART_MAIN);
    // Bouton de la taille active : bordure INFO 2 px (dessinée à l'intérieur du
    // widget, la position ne bouge pas).
    highlight_button_border(btn_s, !large, UIColor::INFO);
    highlight_button_border(btn_l, large, UIColor::INFO);
}

// Couleur + libellé d'un état du pipeline (mêmes valeurs que les 5 anciens blocs).
static void assist_state_style(AssistState st, uint32_t& color, const char*& label) {
    switch (st) {
        case AssistState::LISTENING: color = UIColor::SUCCESS;  label = "Écoute…";  break;
        case AssistState::THINKING:  color = UIColor::WARNING;  label = "Analyse…"; break;
        case AssistState::SPEAKING:  color = UIColor::INFO;     label = "Réponse";  break;
        case AssistState::ERROR:     color = UIColor::ERROR;    label = "Erreur";   break;
        case AssistState::IDLE:
        default:                     color = UIColor::TEXT_DIM; label = "Prêt";     break;
    }
}

void assist_set_mic_state(lv_obj_t* icon_mic, AssistState st) {
    if (icon_mic == nullptr) return;
    uint32_t color;
    const char* label;
    assist_state_style(st, color, label);
    lv_obj_set_style_text_color(icon_mic, lv_color_hex(color), LV_PART_MAIN);
}

void assist_set_pipeline_state(lv_obj_t* icon_mic, lv_obj_t* lbl_status, AssistState st) {
    uint32_t color;
    const char* label;
    assist_state_style(st, color, label);
    if (icon_mic != nullptr) lv_obj_set_style_text_color(icon_mic, lv_color_hex(color), LV_PART_MAIN);
    if (lbl_status != nullptr) {
        lv_label_set_text(lbl_status, label);
        lv_obj_set_style_text_color(lbl_status, lv_color_hex(color), LV_PART_MAIN);
    }
}

void assist_image_state_ui(lv_obj_t* hint, lv_obj_t* img, AssistImage st) {
    const char* text = nullptr;
    if (st == AssistImage::LOADING)    text = "Chargement image...";
    else if (st == AssistImage::ERROR) text = "Image indisponible";
    if (img != nullptr) {
        if (st == AssistImage::READY) lv_obj_remove_flag(img, LV_OBJ_FLAG_HIDDEN);
        else                          lv_obj_add_flag(img, LV_OBJ_FLAG_HIDDEN);
    }
    if (hint != nullptr) {
        if (text != nullptr) {
            lv_label_set_text(hint, text);
            lv_obj_remove_flag(hint, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(hint, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void assist_wake_word_indicator_ui(lv_obj_t* lbl, bool on) {
    if (lbl == nullptr) return;
    lv_label_set_text(lbl, on ? "Ok Nabu: ON" : "Ok Nabu: OFF");
    lv_obj_set_style_text_color(lbl, lv_color_hex(on ? UIColor::SUCCESS : UIColor::TEXT_DIM), LV_PART_MAIN);
}

// =============================================================================
// Décision du mot de réveil — pure, sans LVGL ni id() : la table du .h.
// =============================================================================
namespace WakeWord {

Action decide(const Inputs& in) {
    if (in.alarm_ringing) return ALARM_STOP;
    const bool responding = in.va_stop_armed && in.audio_busy;
    if (in.is_stop) {
        if (in.volet_en_mouvement) return VOLET_STOP;
        return responding ? INTERRUPT_LISTEN : IGNORE_STOP;
    }
    if (responding) return INTERRUPT_LISTEN;
    return (in.wake_word_enabled && in.api_connected) ? START_PIPELINE : IGNORE_INACTIVE;
}

const char* action_name(Action a) {
    switch (a) {
        case ALARM_STOP:       return "ALARM_STOP";
        case VOLET_STOP:       return "VOLET_STOP";
        case INTERRUPT_LISTEN: return "INTERRUPT_LISTEN";
        case START_PIPELINE:   return "START_PIPELINE";
        case IGNORE_STOP:      return "IGNORE_STOP";
        case IGNORE_INACTIVE:  return "IGNORE_INACTIVE";
    }
    return "?";
}

}  // namespace WakeWord
