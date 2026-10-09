/**
 * [AI-CONTEXT]
 * @file tab5_batterie.h
 * @role Batterie de la tablette (08/10/2026, demande d'Axel) : la logique PURE de la
 *       présence de la batterie, du chargeur (CHG_EN, pi4ioe2 P7), du niveau, de la
 *       consommation et de l'alerte « batterie faible ».
 *       Pourquoi : depuis la 3.6.0 (#303) le chargeur était allumé à chaque démarrage.
 *       Sans batterie, il chargeait dans le vide (tension lue 4,2 ↔ 8,39 V, ou 5,70 V
 *       stable, et « en charge » toujours vrai) et faisait un léger souffle continu,
 *       entendu par Axel le 08/10/2026 ; couper le chargeur l'a fait taire.
 *       Règle : on ne décide de la présence que chargeur COUPÉ depuis kChargeurReposMs.
 *       Chargeur coupé, sans batterie, l'INA226 lit 1,83 à 1,94 V (tablette de l'auteur,
 *       08/10/2026, 4 lectures) ; une batterie 2S, même vide, lit plus de 5 V.
 *         - démarrage : chargeur allumé kChargeurReveilDureeMs (réveille une batterie
 *           dont la protection a coupé), puis une « sonde » : chargeur coupé, lecture ;
 *         - pas de batterie : chargeur coupé, et chaque lecture (60 s) redécide : une
 *           batterie glissée tablette allumée est vue à la lecture suivante. Si
 *           l'interrupteur « Tab5 Batterie montée » dit qu'il y en a une, un réveil
 *           toutes les kChargeurReveilMs (batterie si vide que sa protection a coupé) ;
 *         - batterie : chargeur selon « Tab5 Limite de charge » (100 % ou 80 % : arrêt à
 *           80 %, reprise à 70 %) ; une sonde toutes les kChargeurSondeMs chargeur
 *           allumé, et tout de suite après une lecture sous kBatterieBasseV (la batterie
 *           a pu être retirée : sans elle, le chargeur lit 4,2 ou 5,7 V).
 * @architecture_constraint Aucune dépendance ESPHome ni LVGL : ce fichier se compile sur
 *       PC (tools/test_alarm_clock.cpp l'inclut, g++ en CI). L'état vivant et les appels
 *       du YAML sont dans tab5_batterie.cpp ; le YAML (tab5-sensors-diagnostics.yaml)
 *       commande l'interrupteur et l'INA226.
 *       Mode de charge (09/10/2026) : « Classique » ou « Rapide » (nCHG_QC_EN, P5), pris
 *       en compte seulement chargeur allumé (charge_rapide_voulue, ADR-0045).
 * @ai_instruction L'ordre de LimiteCharge et de ModeCharge est celui des options des
 *       selects « Tab5 Limite de charge » et « Tab5 Mode de charge »
 *       (tab5-sensors-diagnostics.yaml, la tablette garde l'INDEX) : une option
 *       nouvelle s'ajoute à la fin (tests/test_batterie.py relit les deux).
 *       Pas encore mesuré (pas de batterie chez l'auteur) : chargeur arrêté et USB
 *       branché, la tablette tourne-t-elle sur l'USB ou sur sa batterie ? Si c'est sur
 *       la batterie, le mode 80 % la fait aller de 80 à 70 % au lieu de la mettre en
 *       pause, et « Tab5 Sur batterie » s'allume pendant la pause.
 */
#pragma once
#include <cmath>
#include <cstdint>

#include "tab5_core.h"  // PresenceBatterie

// Options du select « Tab5 Limite de charge », dans l'ordre.
enum class LimiteCharge : uint8_t {
    COMPLETE = 0,        // « 100 % » (défaut)
    QUATRE_VINGTS = 1,   // « 80 % » : tablette branchée en permanence
};

// ─── Mode de charge (09/10/2026, demande d'Axel ; ADR-0045) ───
// Options du select « Tab5 Mode de charge », dans l'ordre (la tablette garde l'INDEX).
// Il commande nCHG_QC_EN (pi4ioe2 P5, actif bas : `quick_charge`, inverted) :
// M5Unified (Power_Class.inl, setChargeCurrent, commit b926d64 du 02/10/2026) nomme
// « 500 mA » CHG_EN haut + P5 haut (charge rapide arrêtée) et « 1000 mA » CHG_EN haut +
// P5 bas (charge rapide), et coupe les deux pour 0 mA. Ces courants sont ceux de la
// bibliothèque : jamais mesurés sur une tablette (l'auteur n'a pas de batterie).
enum class ModeCharge : uint8_t {
    CLASSIQUE = 0,  // « Classique » (défaut) : charge rapide arrêtée, comme avant
    RAPIDE = 1,     // « Rapide »
};

// Charge rapide voulue : seulement chargeur ALLUMÉ (chargeur_pas() décide CHG_EN : sans
// batterie, pendant une sonde ou à 80 % il est coupé, et la charge rapide avec lui,
// comme le fait M5Unified). Index inconnu : Classique.
inline bool charge_rapide_voulue(uint8_t mode, bool chargeur_allume) {
    return chargeur_allume && static_cast<ModeCharge>(mode) == ModeCharge::RAPIDE;
}

// ─── Présence (chargeur coupé) ───
constexpr float kBatterieSeuilV = 3.0f;           // chargeur coupé : au-dessus = batterie
constexpr float kBatterieBasseV = 6.0f;           // chargeur allumé : en dessous, sonder
// Chargeur coupé depuis au moins ce délai : la tension lue est celle de la batterie (ou
// du vide). Mesuré le 08/10/2026 : 1,83 V lus 6 s après la coupure, sans batterie.
constexpr uint32_t kChargeurReposMs = 8u * 1000u;
constexpr uint32_t kChargeurReveilDureeMs = 30u * 1000u;     // allumé au démarrage et au réveil
constexpr uint32_t kChargeurReveilMs = 60u * 60u * 1000u;    // absente mais « montée » : réveil
constexpr uint32_t kChargeurSondeMs = 10u * 60u * 1000u;     // sonde périodique, chargeur allumé
constexpr uint32_t kChargeurSondeMaxMs = 30u * 1000u;        // sonde sans lecture : abandon
// Courant lu pendant une sonde, ou juste après : chargeur coupé alors que la tablette
// est peut-être branchée ; le mode économie l'ignore (chargeur_en_sonde).
constexpr uint32_t kChargeurSondeFinMs = 2u * 1000u;

// ─── Niveau d'après la tension (droite de battery-percentage.yaml, référence ESPHome) ───
// 6,0 V = 0 %, 8,23 V = 100 % ; trop haut pendant la charge.
constexpr float kBatterieVideV = 6.0f;
constexpr float kBatteriePleineV = 8.23f;

// ─── Limite de charge à 80 % ───
// Le niveau lit trop haut pendant la charge : l'arrêt se fait un peu avant 80 % réels.
constexpr float kChargeArretPct = 80.0f;
constexpr float kChargeReprisePct = 70.0f;
constexpr uint32_t kChargePauseMinMs = 10u * 60u * 1000u;   // arrêt d'au moins 10 min

// ─── Consommation : seulement sur batterie (+ = décharge, sens vérifié chez husyildiz
// le 07/10/2026). Sous ce courant, rien à afficher.
constexpr float kConsoCourantMinA = 0.020f;

// ─── Alerte « batterie faible » (sur batterie seulement) ───
constexpr float kAlerteFaiblePct = 20.0f;
constexpr float kAlerteCritiquePct = 10.0f;
constexpr float kAlerteRearmePct = 30.0f;

inline float batterie_niveau_pct(float tension) {
    if (!std::isfinite(tension)) return NAN;
    const float p = (tension - kBatterieVideV) / (kBatteriePleineV - kBatterieVideV) * 100.0f;
    return p < 0.0f ? 0.0f : (p > 100.0f ? 100.0f : p);
}

// Puissance consommée (W) : tension × courant sur batterie, NAN sinon (sur secteur, le
// courant de l'USB n'est pas mesuré).
inline float batterie_puissance_w(float tension, float courant_a, bool sur_batterie) {
    if (!sur_batterie || !std::isfinite(tension) || !std::isfinite(courant_a)) return NAN;
    if (courant_a < kConsoCourantMinA) return NAN;
    return tension * courant_a;
}

struct EtatChargeur {
    PresenceBatterie presence = PresenceBatterie::INCONNUE;
    bool allume = true;            // CHG_EN commandé (allumé au démarrage, ALWAYS_ON)
    uint32_t change_ms = 0;        // instant de la dernière commande
    bool reveil = true;            // allumé kChargeurReveilDureeMs, puis une sonde
    bool sonde = false;            // chargeur coupé pour lire la tension
    bool lecture_demandee = false;
    bool sonde_vite = false;       // lecture basse chargeur allumé : sonder sans attendre
    uint32_t sonde_fin_ms = 0;     // fin de la dernière sonde (0 : aucune encore)
    float tension = NAN;           // dernière lecture (V)
    float niveau = NAN;            // %, batterie présente seulement
};

struct ActionChargeur {
    bool allumer = true;   // état voulu de CHG_EN
    bool lire = false;     // demander une lecture de l'INA226 maintenant
};

// Une lecture de la tension (V) à `maintenant_ms` (millis(), différences non signées).
// Vrai si la présence a changé. Lecture ratée (NAN) : rien ne change.
inline bool chargeur_lecture(EtatChargeur& c, float tension, uint32_t maintenant_ms) {
    c.tension = tension;  // NAN si l'INA226 ne répond pas : pas de vieille tension affichée
    if (!std::isfinite(tension)) return false;
    const PresenceBatterie avant = c.presence;
    if (!c.allume && maintenant_ms - c.change_ms >= kChargeurReposMs) {
        c.presence = tension >= kBatterieSeuilV ? PresenceBatterie::PRESENTE : PresenceBatterie::ABSENTE;
        if (c.sonde) {
            c.sonde = false;
            c.lecture_demandee = false;
            c.sonde_fin_ms = maintenant_ms;
        }
    } else if (c.allume && c.presence == PresenceBatterie::PRESENTE && tension < kBatterieBasseV) {
        c.sonde_vite = true;
    }
    c.niveau = c.presence == PresenceBatterie::PRESENTE ? batterie_niveau_pct(tension) : NAN;
    return c.presence != avant;
}

// Courant lu chargeur coupé pour une sonde (ou à la lecture qui la termine) : à ignorer.
inline bool chargeur_en_sonde(const EtatChargeur& c, uint32_t maintenant_ms) {
    return c.sonde || (c.sonde_fin_ms != 0 && maintenant_ms - c.sonde_fin_ms < kChargeurSondeFinMs);
}

// Chargeur voulu hors réveil et hors sonde.
inline bool chargeur_voulu(const EtatChargeur& c, uint8_t limite, uint32_t maintenant_ms) {
    switch (c.presence) {
        case PresenceBatterie::ABSENTE: return false;
        case PresenceBatterie::INCONNUE: return true;
        default: break;
    }
    if (static_cast<LimiteCharge>(limite) != LimiteCharge::QUATRE_VINGTS) return true;
    if (!std::isfinite(c.niveau)) return c.allume;
    if (c.allume) return c.niveau < kChargeArretPct;
    return c.niveau <= kChargeReprisePct && maintenant_ms - c.change_ms >= kChargePauseMinMs;
}

inline void chargeur_commander(EtatChargeur& c, bool allume, uint32_t maintenant_ms) {
    if (allume == c.allume) return;
    c.allume = allume;
    c.change_ms = maintenant_ms;
}

// À appeler chaque seconde (`montee` : interrupteur « Tab5 Batterie montée »). Renvoie
// l'état voulu de CHG_EN et s'il faut lire l'INA226 tout de suite (fin du repos d'une
// sonde : sans elle, la lecture suivante attendrait jusqu'à 60 s).
inline ActionChargeur chargeur_tick(EtatChargeur& c, uint8_t limite, bool montee, uint32_t maintenant_ms) {
    ActionChargeur a;
    if (c.sonde) {
        const uint32_t ecoule = maintenant_ms - c.change_ms;
        if (ecoule < kChargeurSondeMaxMs) {
            if (ecoule >= kChargeurReposMs && !c.lecture_demandee) {
                c.lecture_demandee = true;
                a.lire = true;
            }
            a.allumer = false;
            return a;
        }
        // Pas de lecture (INA226 muet) : sonde abandonnée, présence gardée.
        c.sonde = false;
        c.lecture_demandee = false;
        c.sonde_fin_ms = maintenant_ms;
    }
    bool sonder = false;
    if (c.reveil) {
        chargeur_commander(c, true, maintenant_ms);
        if (maintenant_ms - c.change_ms < kChargeurReveilDureeMs) {
            a.allumer = true;
            return a;
        }
        c.reveil = false;
        sonder = true;
    } else if (c.allume && c.presence != PresenceBatterie::ABSENTE) {
        // Batterie (ou inconnue après une sonde ratée) : retirée depuis ?
        sonder = c.sonde_vite || maintenant_ms - c.sonde_fin_ms >= kChargeurSondeMs;
    } else if (c.presence == PresenceBatterie::ABSENTE && montee && !c.allume &&
               maintenant_ms - c.change_ms >= kChargeurReveilMs) {
        c.reveil = true;
        chargeur_commander(c, true, maintenant_ms);
        a.allumer = true;
        return a;
    }
    if (sonder) {
        c.sonde = true;
        c.sonde_vite = false;
        c.lecture_demandee = false;
        chargeur_commander(c, false, maintenant_ms);
        a.allumer = false;
        return a;
    }
    chargeur_commander(c, chargeur_voulu(c, limite, maintenant_ms), maintenant_ms);
    a.allumer = c.allume;
    return a;
}

// ─── Alerte « batterie faible » ───
struct EtatAlerteBatterie {
    uint8_t envoyee = 0;   // 0 aucune, 1 « faible » envoyée, 2 « presque vide » envoyée
};

// Un niveau (%) : renvoie le seuil franchi à annoncer (20 ou 10), 0 sinon. Une seule
// alerte par seuil et par décharge : réarmée sur secteur ou à kAlerteRearmePct.
inline int batterie_alerte_lue(EtatAlerteBatterie& a, float niveau, bool sur_batterie) {
    if (!sur_batterie) {
        a.envoyee = 0;
        return 0;
    }
    if (!std::isfinite(niveau)) return 0;
    if (niveau >= kAlerteRearmePct) {
        a.envoyee = 0;
        return 0;
    }
    if (niveau <= kAlerteCritiquePct && a.envoyee < 2) {
        a.envoyee = 2;
        return static_cast<int>(kAlerteCritiquePct);
    }
    if (niveau <= kAlerteFaiblePct && a.envoyee < 1) {
        a.envoyee = 1;
        return static_cast<int>(kAlerteFaiblePct);
    }
    return 0;
}

// ─── Appels du YAML (tab5_batterie.cpp : un seul état, celui de la tablette) ───
// Une lecture de la tension (par batterie_tension_ui, tab5_zones.cpp) : vrai si la
// présence a changé.
bool chargeur_tension(float tension, uint32_t maintenant_ms);
// Interval de 1 s : état voulu de CHG_EN, et s'il faut lire l'INA226 maintenant.
ActionChargeur chargeur_pas(uint8_t limite, bool montee, uint32_t maintenant_ms);
// Lecture du courant à ignorer (mode économie) : chargeur coupé pour une sonde.
bool chargeur_sonde_en_cours(uint32_t maintenant_ms);
PresenceBatterie batterie_presence();
bool batterie_presente();
float batterie_derniere_tension();
// Niveau (%) de la dernière lecture : NAN sans batterie détectée.
float batterie_niveau_de(float tension);
// Consommation (W) d'une lecture du courant (A), la dernière tension et « sur batterie »
// (NAN hors batterie) ; gardée pour la ligne « Batterie » de la console.
float batterie_consommation(float courant_a, bool sur_batterie);
float batterie_derniere_consommation();
// Niveau publié : seuil d'alerte franchi (20 ou 10), 0 sinon.
int batterie_alerte(float niveau, bool sur_batterie);
// Rendu hors tablette (Tab5/rendu/bouchons.yaml) : chargeur coupé depuis longtemps, la
// lecture suivante décide la présence d'après sa tension.
void chargeur_rendu_coupe(uint32_t maintenant_ms);
