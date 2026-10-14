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
 *           dont la protection a coupé), puis une « sonde » : chargeur coupé, lecture.
 *           Depuis le 10/10/2026 (demande d'Axel), CHG_EN est coupé au setup et ce réveil
 *           part au premier pas, sauf si le démarrage précédent n'a vu AUCUNE batterie
 *           (MemoireBatterie en NVS, chargeur_demarrer) : alors sonde tout de suite, sans
 *           allumer. Risque : une batterie neuve ou totalement plate posée sur une
 *           tablette qui n'en avait pas est vue absente ; allumer « Tab5 Batterie
 *           montée » la réveille (tout de suite, puis une fois par heure) ;
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
// Rehaussé de 3,0 V à 5,5 V (11/10/2026, ADR-0061) : une batterie 2S (7,2 V nominal)
// ne descend jamais sous 5,5 V (coupure BMS à ~6,0 V). À 3,0 V, la tension résiduelle
// sans batterie (3,29 V relevés le 11/10) déclenchait un faux positif qui allumait
// le chargeur dans le vide et faisait surchauffer l'ESP32-P4 à plus de 70 °C.
constexpr float kBatterieSeuilV = 5.5f;           // chargeur coupé : au-dessus = batterie
constexpr float kBatterieBasseV = 6.0f;           // chargeur allumé : en dessous, sonder

// ─── Sécurité thermique anti-surchauffe (11/10/2026, ADR-0061) ───
// Température normale du Tab5 : 36 à 40 °C. Au-delà de kChargeurTempMaxC (55 °C) ou
// en cas de montée brutale (+4 °C en ≤ 60 s avec CPU > 48 °C), le chargeur est
// immédiatement verrouillé et coupé pendant au moins kChargeurVerrouMinMs (15 min).
// Réarmement automatique possible uniquement si la température repasse sous kChargeurTempRearmeC (44 °C).
constexpr float kChargeurTempMaxC = 55.0f;
constexpr float kChargeurTempRearmeC = 44.0f;
constexpr float kChargeurPenteMaxC = 4.0f;                    // montée brutale (+4 °C en ≤ 60 s)
constexpr uint32_t kChargeurVerrouMinMs = 15u * 60u * 1000u; // verrouillé au moins 15 min
constexpr uint32_t kFenetrePenteMs = 60u * 1000u;

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

// ─── Niveau d'après la tension : courbe de décharge Li-ion (14/10/2026, discussion #278) ───
// Avant : une droite 6,0 V = 0 % / 8,23 V = 100 % (battery-percentage.yaml, config de
// référence ESPHome du Tab5). Une batterie lithium n'est pas linéaire : un long plateau
// vers 3,7-3,8 V par élément, puis un effondrement. Chez husyildiz (14/10/2026), le niveau
// descendait de 100 à ~65 % en 11 h, puis de 65 à 0 % en ~1 h : la droite donnait 63 % à
// 7,4 V, là où la courbe ci-dessous en donne 19.
// Table : tension à vide (OCV, 25 °C) d'UN élément lithium-ion 4,2 V → capacité restante,
// `samsung_ocv_cap_eb535151vu` du noyau Linux (drivers/power/supply/samsung-sdi-battery.c,
// commit b5289ba57a27, « Data from vendor boardfile board-janice-bm.c » : batterie Samsung
// EB535151VU, 1500 mAh, charge à 4,2 V). Recopiée telle quelle, points en mV, croissants.
// Ce n'est PAS la courbe mesurée de la batterie du Tab5 (2S, 7,4 V, 2000 mAh ; aucune
// courbe publiée par M5Stack trouvée le 14/10/2026). M5Unified (Power_Class.inl, commit
// a98dc51d2071) reste linéaire pour le Tab5 : demi-tension, 3,30 V = 0 %, 4,15 V ≈ 100 %.
// Limites, connues :
//   - tension À VIDE : sur batterie, l'INA226 lit la tension sous charge, un peu plus basse
//     (résistance interne) : le niveau est plutôt sous-estimé, d'autant plus que la
//     tablette consomme ;
//   - pendant la charge, la tension lue est plus haute qu'au repos : le niveau lit trop haut
//     (la limite à 80 % s'arrête avant 80 % réels) ;
//   - 0 % dès 3,30 V par élément (6,60 V) : la courbe Samsung s'arrête là. Plus bas, il
//     reste peu de chose (1 % entre 3,30 et 3,35 V sur cette courbe) ;
//   - sur le plateau (30 à 50 %), 34 mV par élément séparent 30 et 50 % : le niveau y bouge
//     par sauts (la tension est publiée à 50 mV près, filtre de « Tab5 Tension batterie »).
constexpr int kBatterieElements = 2;   // 2S
struct PointDecharge {
    uint16_t mv;     // tension à vide d'un élément (mV)
    uint8_t pct;     // capacité restante (%)
};
// Huit points par ligne : 29 lignes d'un point se reliraient mal.
// clang-format off
constexpr PointDecharge kCourbeDecharge[] = {
    {3300, 0},  {3352, 1},  {3405, 2},  {3497, 4},  {3566, 6},  {3593, 7},  {3638, 9},  {3662, 10},
    {3680, 12}, {3685, 15}, {3691, 18}, {3711, 20}, {3743, 25}, {3764, 30}, {3778, 40}, {3798, 50},
    {3816, 55}, {3839, 59}, {3874, 62}, {3891, 65}, {3920, 70}, {3962, 77}, {3988, 82}, {4013, 85},
    {4057, 89}, {4078, 92}, {4105, 95}, {4148, 99}, {4178, 100},
};
// clang-format on
constexpr int kCourbePoints = sizeof(kCourbeDecharge) / sizeof(kCourbeDecharge[0]);
// Bornes de la table, pour la batterie entière (2S) : 6,60 V = 0 %, 8,356 V = 100 %.
constexpr float kBatterieVideV = kBatterieElements * kCourbeDecharge[0].mv / 1000.0f;
constexpr float kBatteriePleineV = kBatterieElements * kCourbeDecharge[kCourbePoints - 1].mv / 1000.0f;

// ─── Limite de charge à 80 % ───
// Le niveau lit trop haut pendant la charge : l'arrêt se fait un peu avant 80 % réels.
// Avec la courbe (14/10/2026) : arrêt vers 7,96 V lus (7,78 V avec la droite), reprise
// vers 7,84 V lus chargeur coupé (7,56 V avec la droite).
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

// Niveau (%) d'une tension de la batterie entière (V) : la tension d'un élément, puis
// interpolation linéaire entre deux points de kCourbeDecharge ; borné à 0..100, NAN si la
// tension n'est pas finie.
inline float batterie_niveau_pct(float tension) {
    if (!std::isfinite(tension)) return NAN;
    const float mv = tension * 1000.0f / kBatterieElements;
    if (mv <= kCourbeDecharge[0].mv) return 0.0f;
    if (mv >= kCourbeDecharge[kCourbePoints - 1].mv) return 100.0f;
    for (int i = 1; i < kCourbePoints; i++) {
        const PointDecharge& b = kCourbeDecharge[i];
        if (mv > b.mv) continue;
        const PointDecharge& a = kCourbeDecharge[i - 1];
        return a.pct + (mv - a.mv) * (b.pct - a.pct) / static_cast<float>(b.mv - a.mv);
    }
    return 100.0f;  // inatteignable (mv < dernier point)
}

// Puissance consommée (W) : tension × courant sur batterie, NAN sinon (sur secteur, le
// courant de l'USB n'est pas mesuré).
inline float batterie_puissance_w(float tension, float courant_a, bool sur_batterie) {
    if (!sur_batterie || !std::isfinite(tension) || !std::isfinite(courant_a)) return NAN;
    if (courant_a < kConsoCourantMinA) return NAN;
    return tension * courant_a;
}

// ─── Mémoire du démarrage précédent (10/10/2026, demande d'Axel) ───
// Gardée en NVS (globals `tab5_batterie_memoire`, tab5-sensors-diagnostics.yaml) : une
// batterie a-t-elle été vue pendant le démarrage précédent ? Sans batterie, les 30 s de
// réveil au démarrage faisaient charger dans le vide (8,39 V lus, pic de consommation,
// puis souffle) : si le démarrage précédent n'en a vu AUCUNE, le chargeur reste coupé et
// une sonde lit la tension tout de suite. Inconnue (premier démarrage, NVS vide) : réveil,
// comme avant. Valeurs gardées en NVS : ne pas les renuméroter.
enum class MemoireBatterie : uint8_t {
    INCONNUE = 0,
    AUCUNE = 1,  // le démarrage précédent a décidé « pas de batterie », sans en voir une
    VUE = 2,     // une batterie vue (chargeur coupé, ≥ kBatterieSeuilV) au démarrage précédent
};

struct EtatChargeur {
    PresenceBatterie presence = PresenceBatterie::INCONNUE;
    bool allume = true;            // CHG_EN commandé (voir chargeur_demarrer)
    uint32_t change_ms = 0;        // instant de la dernière commande
    bool reveil = true;            // allumé kChargeurReveilDureeMs, puis une sonde
    bool vue = false;              // une batterie vue depuis le démarrage (MemoireBatterie::VUE)
    bool montee_avant = false;     // « Tab5 Batterie montée » au pas précédent
    bool reveil_demande = false;   // « montée » vient d'être allumé : réveil sans attendre l'heure
    bool sonde = false;            // chargeur coupé pour lire la tension
    bool lecture_demandee = false;
    bool sonde_vite = false;       // lecture basse chargeur allumé : sonder sans attendre
    uint32_t sonde_fin_ms = 0;     // fin de la dernière sonde (0 : aucune encore)
    float tension = NAN;           // dernière lecture (V)
    float niveau = NAN;            // %, batterie présente seulement
    bool verrou_thermique = false; // sécurité anti-surchauffe active (coupure totale de charge)
    uint32_t verrou_thermique_ms = 0; // instant du dernier déclenchement de surchauffe
    float derniere_temp_cpu = NAN; // dernière température CPU (°C)
    float temp_precedente_pente = NAN; // température de référence pour calcul de pente
    uint32_t temp_precedente_ms = 0;
};

struct ActionChargeur {
    bool allumer = true;   // état voulu de CHG_EN
    bool lire = false;     // demander une lecture de l'INA226 maintenant
};

// Surveillance thermique appelée à chaque relevé de température interne :
// Renvoie true si le verrou thermique vient d'être déclenché (front montant).
inline bool chargeur_surveillance_thermique(EtatChargeur& c, float temp_c, uint32_t maintenant_ms) {
    if (!std::isfinite(temp_c)) return false;
    c.derniere_temp_cpu = temp_c;

    // Calcul de pente sur fenêtre glissante (~60 s)
    if (c.temp_precedente_ms == 0 || maintenant_ms - c.temp_precedente_ms >= kFenetrePenteMs) {
        c.temp_precedente_pente = temp_c;
        c.temp_precedente_ms = maintenant_ms;
    }
    const bool pente_anormale = std::isfinite(c.temp_precedente_pente) &&
                                (temp_c - c.temp_precedente_pente >= kChargeurPenteMaxC) &&
                                (temp_c >= 48.0f);

    const bool declenchement = (temp_c >= kChargeurTempMaxC) || pente_anormale;

    if (declenchement) {
        c.verrou_thermique_ms = maintenant_ms;
        if (!c.verrou_thermique) {
            c.verrou_thermique = true;
            c.allume = false;
            c.change_ms = maintenant_ms;
            return true;
        }
    } else if (c.verrou_thermique) {
        // Réarmement après délai minimum de sécurité ET retour sous le seuil sain (44 °C)
        if (maintenant_ms - c.verrou_thermique_ms >= kChargeurVerrouMinMs &&
            temp_c <= kChargeurTempRearmeC) {
            c.verrou_thermique = false;
        }
    }
    return false;
}

// Une lecture de la tension (V) à `maintenant_ms` (millis(), différences non signées).
// Vrai si la présence a changé. Lecture ratée (NAN) : rien ne change.
inline bool chargeur_lecture(EtatChargeur& c, float tension, uint32_t maintenant_ms) {
    c.tension = tension;  // NAN si l'INA226 ne répond pas : pas de vieille tension affichée
    if (!std::isfinite(tension)) return false;
    const PresenceBatterie avant = c.presence;
    if (!c.allume && maintenant_ms - c.change_ms >= kChargeurReposMs) {
        c.presence = tension >= kBatterieSeuilV ? PresenceBatterie::PRESENTE : PresenceBatterie::ABSENTE;
        if (c.presence == PresenceBatterie::PRESENTE) c.vue = true;
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
    if (c.verrou_thermique) return false;
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
    // Verrouillage thermique actif : chargeur INTERDIT (aucune tentative de réveil ni de sonde)
    if (c.verrou_thermique) {
        if (maintenant_ms - c.verrou_thermique_ms >= kChargeurVerrouMinMs &&
            (!std::isfinite(c.derniere_temp_cpu) || c.derniere_temp_cpu <= kChargeurTempRearmeC)) {
            c.verrou_thermique = false;
        } else {
            c.allume = false;
            c.sonde = false;
            c.reveil = false;
            c.reveil_demande = false;
            a.allumer = false;
            a.lire = false;
            return a;
        }
    }
    // « Tab5 Batterie montée » allumé : le moyen manuel de réveiller une batterie neuve ou
    // si vide que sa protection a coupé (vue absente), sans attendre l'heure.
    if (montee && !c.montee_avant) c.reveil_demande = true;
    c.montee_avant = montee;
    if (!montee || c.presence == PresenceBatterie::PRESENTE) c.reveil_demande = false;
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
        c.reveil_demande = false;  // ce réveil vaut celui demandé
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
               (c.reveil_demande || maintenant_ms - c.change_ms >= kChargeurReveilMs)) {
        c.reveil = true;
        c.reveil_demande = false;
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

// Premier pas après le démarrage (chargeur_pas, une fois). CHG_EN est COUPÉ depuis son
// setup (`restore_mode: ALWAYS_OFF`, 10/10/2026) ; `memoire` : MemoireBatterie gardée en
// NVS par le démarrage précédent, `montee` : « Tab5 Batterie montée » restauré.
//   - AUCUNE : chargeur laissé coupé, sonde tout de suite (lecture au bout de
//     kChargeurReposMs) : pas de pic, pas de charge dans le vide ;
//   - VUE, INCONNUE ou valeur inconnue : réveil de kChargeurReveilDureeMs puis sonde,
//     comme avant (le chargeur s'allume à ce premier pas, à la fin de setup()).
// Une batterie neuve ou si vide qu'elle lit moins de kBatterieSeuilV, branchée sur une
// tablette qui n'en avait pas : vue absente ; allumer « Tab5 Batterie montée » la réveille
// (reveil_demande, puis une fois par heure).
inline void chargeur_demarrer(EtatChargeur& c, uint8_t memoire, bool montee, uint32_t maintenant_ms) {
    c.allume = false;
    c.change_ms = maintenant_ms;
    c.montee_avant = montee;  // restauré : pas un « vient d'être allumé »
    c.reveil_demande = false;
    c.lecture_demandee = false;
    if (static_cast<MemoireBatterie>(memoire) == MemoireBatterie::AUCUNE) {
        c.reveil = false;
        c.sonde = true;
    } else {
        c.reveil = true;
        c.sonde = false;
    }
}

// Mémoire à garder pour le démarrage suivant : VUE dès qu'une batterie a été vue pendant
// ce démarrage (même retirée depuis), AUCUNE si la présence a été décidée absente sans
// jamais en voir, sinon (rien de décidé encore) celle d'avant.
inline uint8_t batterie_memoire_suivante(const EtatChargeur& c, uint8_t memoire) {
    if (c.vue) return static_cast<uint8_t>(MemoireBatterie::VUE);
    if (c.presence == PresenceBatterie::ABSENTE) return static_cast<uint8_t>(MemoireBatterie::AUCUNE);
    return memoire;
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
// Interval de 1 s : état voulu de CHG_EN, et s'il faut lire l'INA226 maintenant. Au premier
// appel, chargeur_demarrer() avec `memoire` (MemoireBatterie du démarrage précédent).
ActionChargeur chargeur_pas(uint8_t limite, bool montee, uint8_t memoire, uint32_t maintenant_ms);
// Mémoire à garder en NVS pour le démarrage suivant (batterie_memoire_suivante) ; journal
// au changement.
uint8_t chargeur_memoire(uint8_t memoire);
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
// Surveillance thermique (11/10/2026) : relève la température CPU et verrouille le chargeur si surchauffe.
bool chargeur_thermique_releve(float temp_c, uint32_t maintenant_ms);
bool chargeur_est_verrouille_thermique();
// Rendu hors tablette (Tab5/rendu/bouchons.yaml) : chargeur coupé depuis longtemps, la
// lecture suivante décide la présence d'après sa tension.
void chargeur_rendu_coupe(uint32_t maintenant_ms);
