/**
 * [AI-CONTEXT]
 * @file tab5_calendar.h
 * @role Calendrier mensuel (tab5_calendar.cpp) : cache des mois, grille, détail d'un jour.
 * @architecture_constraint Sorti de tab5_custom.h le 08/10/2026, lignes recopiées telles
 *       quelles : tab5_custom.h l'inclut, les lambdas YAML et les unités `tab5_*.cpp` n'ont
 *       rien à changer. Une fonction déclarée ici a un appelant hors de son fichier (règle 12
 *       de tools/check_tab5_code_rules.py, qui lit tab5_custom.h et ses en-têtes).
 * @ai_instruction Une déclaration nouvelle de ce module va ici ; un module nouveau = un
 *       en-tête de plus, inclus par tab5_custom.h et listé sous `includes:` des deux
 *       configurations racine (tab5-ha-hmi.yaml, tab5-rendu-host.yaml).
 */
#pragma once
#include "esphome.h"
#include <string>

namespace esphome { namespace font { class Font; } }

// =============================================================================
// Popup calendrier mensuel (calendar_popup.yaml, appui long sur l'horloge)
// Grille 7×6 lundi-en-tête calculée EN LOCAL (SNTP) ; HA enrichit chaque mois à
// la demande via tab5_maj_calendrier_mois (codes + heures + details optionnels)
// et chaque jour tapé via tab5_maj_calendrier_jour si le cache details est vide.
// =============================================================================

// Bits des codes jour (2 chars hex par jour, poussés par script.tab5_calendrier_mois)
constexpr int CAL_BIT_TRAVAIL  = 1;
constexpr int CAL_BIT_FERIE    = 2;
constexpr int CAL_BIT_VACANCES = 4;   // vacances scolaires (Zone A)
constexpr int CAL_BIT_RDV      = 8;
constexpr int CAL_BIT_ANNIV    = 16;

struct CalDetailLineUI {
    lv_obj_t* icon;   // glyphe MDI typé (travail/férié/vacances/RDV/anniv/fête)
    lv_obj_t* txt;    // texte de la ligne
};

// Cache mensuel (TTL + eviction : max 3 mois M-1/M/M+1, stale-while-revalidate)
bool cal_month_needs_fetch(int year, int month);
// true si le mois est en cache mais plus vieux que ttl_ms (refresh silencieux conseillé)
bool cal_month_is_stale(int year, int month, uint32_t ttl_ms = 600000);  // défaut 10 min
// Pré-fetch du démarrage et de la reconnexion (tab5_cal_prefetch, tab5-calendar.yaml) :
// un mois reçu il y a moins de 30 s n'est pas redemandé. Au démarrage, on_boot et
// status_ha lancent tous deux le pré-fetch, à 3 s d'écart ; une reconnexion de HA
// plus tard redemande le mois, reçu il y a plus de 30 s.
constexpr uint32_t CAL_PREFETCH_FRESH_MS = 30000;
// Évince les mois distants de >1 par rapport à (year, month) — garde max 3 entrées.
void cal_cache_evict_distant(int year, int month);
// Décale (year, month) de `delta` mois en passant l'année (déc. + 1 = janv. suivant).
void cal_shift_month(int& year, int& month, int delta);
void cal_store_month_data(const std::string& annee, const std::string& mois,
    const std::string& codes, const std::string& heures, const std::string& details = "");

// Construit les 42 cellules de la grille (168 px de large, lundi en tête) dans le
// parent de `anchor`, juste avant lui (la légende) : même rang que l'ancien gabarit
// cal_day_cell.yaml (lot 8, 26/09/2026). `grid_y` = ${cal_grid_y}, `grid_h` =
// ${cal_grid_h} : la hauteur et le y des lignes sont posés par cal_render_month()
// selon le nombre de semaines du mois. Un tap court sur la cellule i appelle
// on_tap(i). Une seule fois : false si la grille existe déjà (appeler à chaque
// ouverture ne coûte rien).
bool cal_grid_build(lv_obj_t* anchor, int32_t grid_y, int32_t grid_h,
                    const esphome::font::Font* font_num,
                    const esphome::font::Font* font_text, void (*on_tap)(int));

// Rendu complet du mois affiché dans la grille : numéros + alignement lundi-dimanche
// + weekend + aujourd'hui calculés localement, enrichissement HA appliqué si le mois
// est en cache. Sans effet sur les cellules tant que cal_grid_build() n'a pas tourné.
void cal_render_month(lv_obj_t* lbl_month,
    int view_year, int view_month, int today_year, int today_month, int today_day);

// "" si la cellule est hors mois, sinon date ISO "YYYY-MM-DD" du jour tapé.
std::string cal_date_for_cell(int view_year, int view_month, int cell_idx);

// Détail jour embarqué dans le payload mois (champs séparés par ~). "" si absent.
std::string cal_cached_day_detail(int year, int month, int day);
// true seulement si le champ ~ de CE jour est non vide (sinon fallback script _jour).
bool cal_day_has_embedded_detail(int year, int month, int day);

// Sous-popup détail : titre "Mardi 21 Juillet" + statut Chargement/HA hors ligne.
void cal_show_day_detail_loading(lv_obj_t* day_popup, lv_obj_t* lbl_title,
    lv_obj_t* lbl_status, CalDetailLineUI lines[6], const std::string& date_iso,
    bool ha_online);

// Remplit les 6 lignes du détail depuis le payload HA ("type|texte;...").
void cal_render_day_detail(const std::string& payload, lv_obj_t* lbl_status,
    CalDetailLineUI lines[6]);

// Styles partagés des cases du calendrier (créés à la construction de la grille).
void cal_styles_repeindre();
