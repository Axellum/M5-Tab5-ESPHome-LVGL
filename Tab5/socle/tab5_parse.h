/**
 * [AI-CONTEXT]
 * @file tab5_parse.h
 * @role Lecture des payloads poussés par Home Assistant (lot F de l'audit du 30/09/2026,
 *       §2.4) : ce que chaque service reçoit, découpé et converti en données simples, sans
 *       rien peindre. Le code de l'écran (Tab5/ecran/) appelle ces fonctions puis pose
 *       ses widgets ; la lecture elle-même se teste et se fuzze sur PC
 *       (tools/test_parse.cpp, g++ en CI ; tools/fuzz/fuzz_parse.cpp, libFuzzer).
 *       Une famille par section, dans l'ordre du plan : prévisions, vigilance, alertes et
 *       bandeau info, pluie, puis les suivantes.
 * @architecture_constraint Logique PURE, comme tab5_core et tab5_champs : ni ESPHome ni
 *       LVGL (tests/test_rangement.py). Un refus journalisé (payload_refuse,
 *       payload_trop_long) reste chez l'appelant, avant l'appel.
 * @ai_instruction Extraction NEUTRE : chaque fonction reprend à l'identique la boucle
 *       qu'elle remplace, travers compris (strtok_r qui fusionne les champs vides, atoi
 *       qui lit « 3x » comme 3, tampons de pile coupés à leur taille). tools/test_parse.cpp
 *       fige ces comportements ; en changer un est un changement de contrat, dans une PR
 *       à part, pas un nettoyage.
 */
#pragma once
#include <cstddef>
#include <cstdint>

#include "tab5_core.h"

// ─── 1. Prévisions (tab5_maj_meteo_heures_bulk / tab5_maj_meteo_jours_bulk) ───
// Payloads « idx|heure|condition|temp|pluvio;… » et
// « jour|nom|condition|tmin|tmax|repos|dimanche|passé|heures;… », un enregistrement par
// « ; ». Lus dans un tampon de pile de kPrevisionsMax octets : l'appelant refuse avant un
// payload plus long (payload_trop_long), au-delà la fin serait ignorée.
constexpr size_t kPrevisionsMax = 2048;

// Premier créneau du bloc horaire (atoi du payload : « 5|… » → 5, illisible → 0).
int previsions_premier_creneau(const char* payload);

// Créneaux horaires : enregistrement d'au moins 5 champs (« | », champs vides gardés)
// et d'index 0 à 14 → heures[idx] (heure, condition, température, pluie en mm ; atof, donc
// « abc » = 0). Le reste est ignoré. Les enregistrements vides (« ;; ») sont sautés
// (strtok_r).
void previsions_heures_lire(const char* payload, HourForecastData heures[15]);

// Jours : enregistrement d'au moins 9 champs et de jour 0 à 14 → jours[jour] ; les trois
// drapeaux valent vrai si le champ commence par « 1 ». Le jour 0 date le lot :
// `ancre` = local_day_number_today() au moment de la lecture (-1 si l'heure n'est pas
// réglée), comme cal_jours_anchor_day.
void previsions_jours_lire(const char* payload, DayForecastData jours[15], int32_t& ancre);
