/**
 * [AI-CONTEXT]
 * @file tab5_i18n.h
 * @role Traduction de l'écran (lot 4 de l'audit « ouverture », 27/09/2026). Façon
 *       gettext : le texte FRANÇAIS est la clé. `tr("Calendrier")` rend « Calendar »
 *       en anglais, et le texte reçu tel quel en français ou si la traduction manque.
 *       Les tables viennent de Tab5/lang/ (un .yaml par langue), générées dans tab5_i18n_data.h par
 *       tools/gen_i18n.py (ajouter une langue : voir l'en-tête de ce script).
 * @architecture_constraint PUR : ni LVGL, ni ESPHome, ni HA. tab5_core.cpp (dates)
 *       l'utilise et se compile sur PC avec les tests du réveil. Le passage sur les
 *       widgets (textes posés par le YAML) est dans tab5_text.cpp :
 *       i18n_apply_boot(), appelé une fois en fin de setup (on_boot -100).
 * @ai_instruction Traduire ce que l'écran AFFICHE ou que la tablette DIT, jamais ce
 *       que Home Assistant lit : noms d'entités, options de select, états exposés,
 *       codes des payloads restent en français (les renommer casse l'historique et
 *       les automatisations). Une valeur HA affichée à l'écran se traduit au moment
 *       de l'affichage : `tr(etat.c_str())`.
 * @memory_constraint Tables en flash (const). Aucune allocation dans tr() ; la
 *       langue change par redémarrage (select « Langue », tab5-ha-controls.yaml).
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string>
#include <utility>

// Langue courante : 0 = français (langue source), puis l'ordre des `_index`.
uint8_t i18n_language();
void i18n_set_language(uint8_t lang);
size_t i18n_language_count();
const char* i18n_language_name(size_t lang);   // nom natif : « Français », « English »
const char* i18n_language_code(size_t lang);   // « fr », « en »

// Traduction d'un texte français. Renvoie `fr` lui-même s'il n'y a rien à traduire :
// le pointeur rendu vit donc au moins aussi longtemps que l'argument.
const char* tr(const char* fr);
// Même chose avec un contexte, quand un mot a deux sens (« Jeu » : jeudi ou jeu).
const char* tr_ctx(const char* ctx, const char* fr);
// Marque un texte rangé dans une table (`static const char* const k[] = {tr_noop("…")}`) :
// ne traduit rien, mais tools/i18n_keys.py le relève comme clé à traduire. La
// traduction se fait à l'affichage : tr(k[i]). Même rôle que N_() de gettext.
constexpr const char* tr_noop(const char* fr) { return fr; }

// Modèle traduit puis rempli : tr_fill("{jour} {quantieme} {mois}", {{"jour", "lundi"}, …}).
// Chaque {nom} du modèle (traduit) est remplacé ; un nom inconnu reste tel quel.
std::string tr_fill(const char* pattern_fr,
                    std::initializer_list<std::pair<const char*, std::string>> vars);
