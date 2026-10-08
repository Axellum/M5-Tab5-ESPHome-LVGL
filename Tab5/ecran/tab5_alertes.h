/**
 * [AI-CONTEXT]
 * @file tab5_alertes.h
 * @role Popup Alertes, historique (tab5_alertes.cpp).
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
// Historique des alertes (lot 4 du plan des alertes, 06/10/2026) — tab5_alertes.cpp
// =============================================================================
// Popup « Alertes » (alertes_popup.yaml) : les 20 dernières alertes, une ligne chacune
// (pastille de la gravité, libellé, « apparue 14 h 02 · lue 14 h 10 · terminée 15 h 30 »),
// et « Tout marquer comme lu ». Ouvert par un appui long sur la carte centrale ou par
// « Aller à l'écran → Alertes ». Home Assistant répond à l'événement
// esphome.tab5_alertes_historique par l'action tab5_maj_alertes_historique.
//
// Widgets posés par le script tab5_alertes_ouvrir (tab5-alertes.yaml) à la première
// ouverture : id() n'existe que dans une lambda YAML.
struct AlertesUI {
    lv_obj_t* popup = nullptr;                          // alertes_popup
    lv_obj_t* liste = nullptr;                          // alertes_liste : lignes créées en C++
    lv_obj_t* attente = nullptr;                        // alertes_attente : rien reçu, ou liste vide
    lv_obj_t* tout_lu = nullptr;                        // btn_alertes_tout_lu
    const esphome::font::Font* police = nullptr;        // roboto_32_b : libellés
    const esphome::font::Font* police_heures = nullptr; // roboto_22 : heures
    // Événement esphome.tab5_alertes_historique (script tab5_alertes_demande, lambda sans capture).
    void (*demander)() = nullptr;
};
extern AlertesUI g_alertes_ui;

// Action tab5_maj_alertes_historique : « apparue|lue|terminée|gravité|libellé » séparés
// par « ; », la plus récente d'abord, 20 au plus. Heures en secondes epoch (0 = pas
// encore) ; gravité Rouge, Orange ou Jaune ; libellé codé comme ceux des bandeaux
// (« @maj:titre », « @indispo:nombre », « @vigi:niveau ») ou nom d'une entité.
void alertes_historique_recu(const std::string& payload);
// Ouvre le popup, le peint avec la dernière liste reçue, et demande la nouvelle à HA.
void alertes_ouvrir();
// « Tout marquer comme lu » : marque lues les lignes en cours, avant la réponse de HA.
void alertes_tout_lu_local();
