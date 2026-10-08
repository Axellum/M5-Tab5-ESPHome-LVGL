/**
 * [AI-CONTEXT]
 * @file tab5_journal.h
 * @role Journal des démarrages et des coupures (tab5_journal.cpp).
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

// =============================================================================
// Journal des démarrages et des coupures (tab5_journal.cpp, 26/09/2026) : plantages
// et pannes du lien Wi-Fi (C6), envoyés à HA dans l'événement esphome.tab5_journal.
// =============================================================================
// logger: on_message (tab5-hardware.yaml) : garde les erreurs, et les avertissements
// tant que Home Assistant n'est pas connecté.
void journal_log_message(uint8_t level, const char* tag, const char* message);
// interval 30 s (tab5-sensors-diagnostics.yaml) : copie en NVS quand le Wi-Fi manque 90 s.
void journal_tick();
// Script tab5_journal_envoi, à chaque connexion de HA :
bool journal_has_report();          // autre chose qu'un démarrage normal
bool journal_is_serious();          // plantage, erreur ou démarrage sans HA
std::string journal_reset_reason(); // raison du dernier démarrage, en clair (+ code ROM si chien de garde)
// Filtre du capteur « Tab5 Raison du redémarrage » : premier démarrage après une
// installation par l'USB (flash effacée) → « First boot after install (…) » ; chien de
// garde sans rapport de plantage (bouton d'alimentation, 06/10/2026) → « Power button or
// RTC watchdog (rst 0x..) », au lieu d'un « Reboot request from … » périmé.
std::string journal_raison_ha(const std::string& raison);
std::string journal_boot_count();   // démarrages depuis le dernier envoi
std::string journal_report_text();  // lignes en attente, une par ligne
void journal_mark_delivered();      // vide le journal (et sa copie NVS)
