/**
 * [AI-CONTEXT]
 * @file tab5_batterie.cpp
 * @role Batterie de la tablette (08/10/2026) : l'état vivant (présence, chargeur, alerte)
 *       et les appels du YAML. Les règles sont dans tab5_batterie.h (pures, testées sur
 *       PC) ; tab5-sensors-diagnostics.yaml lit l'INA226 et commande CHG_EN chaque
 *       seconde d'après chargeur_pas().
 * @architecture_constraint Ni LVGL ni entité ici : l'icône et la console lisent
 *       batterie_presence() / batterie_derniere_tension() (tab5_zones.cpp).
 * @ai_instruction [AI-DEBUG] Chaque commande du chargeur est journalisée en INFO
 *       (tag tab5.batterie) avec sa raison : « sonde », « réveil », « pas de batterie »,
 *       « limite 80 % »… C'est le premier endroit à lire si le souffle revient.
 */
#include "tab5_batterie.h"

#include "esphome/core/log.h"

static const char* const TAG = "tab5.batterie";

static EtatChargeur s_chargeur;
static EtatAlerteBatterie s_alerte;
static float s_puissance_w = NAN;

bool chargeur_tension(float tension, uint32_t maintenant_ms) {
    return chargeur_lecture(s_chargeur, tension, maintenant_ms);
}

ActionChargeur chargeur_pas(uint8_t limite, bool montee, uint32_t maintenant_ms) {
    const bool avant = s_chargeur.allume;
    const bool sonde_avant = s_chargeur.sonde;
    const ActionChargeur a = chargeur_tick(s_chargeur, limite, montee, maintenant_ms);
    if (a.allumer != avant) {
        const char* raison;
        if (s_chargeur.sonde && !sonde_avant) raison = "sonde : tension lue chargeur coupe";
        else if (s_chargeur.reveil) raison = "reveil d'une batterie montee";
        else if (s_chargeur.presence == PresenceBatterie::ABSENTE) raison = "pas de batterie";
        else if (s_chargeur.presence == PresenceBatterie::INCONNUE) raison = "batterie inconnue";
        else if (static_cast<LimiteCharge>(limite) == LimiteCharge::QUATRE_VINGTS) raison = "limite 80 %";
        else raison = "batterie";
        ESP_LOGI(TAG, "Chargeur %s (%s, niveau %.0f %%)", a.allumer ? "allume" : "coupe", raison,
                 s_chargeur.niveau);
    }
    return a;
}

bool chargeur_sonde_en_cours(uint32_t maintenant_ms) {
    return chargeur_en_sonde(s_chargeur, maintenant_ms);
}

PresenceBatterie batterie_presence() { return s_chargeur.presence; }

bool batterie_presente() { return s_chargeur.presence == PresenceBatterie::PRESENTE; }

float batterie_derniere_tension() { return s_chargeur.tension; }

float batterie_niveau_de(float tension) {
    return batterie_presente() ? batterie_niveau_pct(tension) : NAN;
}

float batterie_consommation(float courant_a, bool sur_batterie) {
    s_puissance_w = batterie_puissance_w(s_chargeur.tension, courant_a, sur_batterie);
    return s_puissance_w;
}

float batterie_derniere_consommation() { return s_puissance_w; }

int batterie_alerte(float niveau, bool sur_batterie) {
    const int seuil = batterie_alerte_lue(s_alerte, niveau, sur_batterie);
    if (seuil > 0) ESP_LOGI(TAG, "Batterie faible : %.0f %% (seuil %d %%)", niveau, seuil);
    return seuil;
}

void chargeur_rendu_coupe(uint32_t maintenant_ms) {
    s_chargeur.reveil = false;
    s_chargeur.sonde = false;
    s_chargeur.allume = false;
    s_chargeur.change_ms = maintenant_ms - kChargeurReposMs;  // non signé : vrai dès 0 ms
}
