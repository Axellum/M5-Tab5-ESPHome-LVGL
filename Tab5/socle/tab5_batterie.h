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
 *         - démarrage (16/10/2026, après la panne du 11/10) : CHG_EN coupé au setup, puis
 *           une « sonde » d'abord (chargeur coupé kChargeurReposMs, lecture) : CHG_EN ne
 *           s'allume qu'après une lecture ≥ kBatterieSeuilV. Le réveil de
 *           kChargeurReveilDureeMs (batterie dont la protection a coupé) ne part qu'après
 *           une sonde qui n'a rien vu, si le démarrage précédent a VU une batterie
 *           (MemoireBatterie en NVS) et jamais après un redémarrage anormal (plantage,
 *           baisse de tension : une boucle de redémarrages rallumait 30 s le chargeur
 *           dans le vide à chaque tour, chargeur_demarrer). Une batterie neuve ou
 *           totalement plate est vue absente ; allumer « Tab5 Batterie montée » la
 *           réveille (tout de suite, puis une fois par heure) ;
 *         - pas de batterie : chargeur coupé, et chaque lecture (60 s) redécide : une
 *           batterie glissée tablette allumée est vue à la lecture suivante. Si
 *           l'interrupteur « Tab5 Batterie montée » dit qu'il y en a une, un réveil
 *           toutes les kChargeurReveilMs (batterie si vide que sa protection a coupé) ;
 *         - batterie : chargeur selon « Tab5 Limite de charge » (100 % ou 80 % : arrêt à
 *           80 %, reprise à 70 %) ; une sonde toutes les kChargeurSondeMs chargeur
 *           allumé, et tout de suite après une lecture sous kBatterieBasseV (la batterie
 *           a pu être retirée : sans elle, le chargeur lit 4,2 ou 5,7 V). Cette sonde
 *           n'est pas un arrêt de la limite 80 % : la charge reprend après elle
 *           (charge_avant_sonde, 20/10/2026, discussion #278) ;
 *         - verrous (16/10/2026) : thermique (CPU ≥ 55 °C, gardé en NVS), électrique
 *           (chargeur allumé : tension > kChargeurTensionMaxV ou courant nul pendant
 *           kChargeurCourantFaibleMs, la signature d'une charge dans le vide ; deux fois
 *           dans l'heure = chargeur verrouillé jusqu'au redémarrage). Chacun est publié
 *           (« Tab5 Sécurité chargeur ») et signalé à HA (chargeur_signal_prendre).
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
// Depuis le 16/10/2026 : une température illisible (NAN) ne le réarme plus, et il est gardé
// en NVS (tab5_batterie.cpp) : un redémarrage (provoqué par la surchauffe ou non) le
// retrouve posé, chargeur coupé au moins kChargeurVerrouMinMs puis jusqu'à une
// température lue sous kChargeurTempRearmeC.
constexpr float kChargeurTempMaxC = 55.0f;
constexpr float kChargeurTempRearmeC = 44.0f;
constexpr float kChargeurPenteMaxC = 4.0f;                    // montée brutale (+4 °C en ≤ 60 s)
constexpr uint32_t kChargeurVerrouMinMs = 15u * 60u * 1000u; // verrouillé au moins 15 min
constexpr uint32_t kFenetrePenteMs = 60u * 1000u;

// ─── Verrou électrique : la signature d'une charge dans le vide (16/10/2026) ───
// Sans batterie, chargeur allumé, l'INA226 lisait 7,7 V, 8,39 V ou 4,2 ↔ 8,4 V (03 et
// 11/10/2026) : au-dessus de kBatterieBasseV, rien ne réagissait avant la sonde
// périodique (jusqu'à 10 min ; le 11/10, le CPU est passé de 61 à 70 °C en 4 min).
// Chargeur allumé (hors réveil et hors sonde) :
//   - tension > kChargeurTensionMaxV : au-dessus d'une 2S pleine (8,4 V) ;
//   - |courant| < kChargeurCourantMinA pendant kChargeurCourantFaibleMs : une batterie
//     qui charge tire du courant (CHG_STAT, lui, dit « en charge » même sans batterie) ;
// → chargeur coupé et sonde immédiate. La sonde dit la suite : courant nul et batterie
// lue pleine (≥ kChargeTermineePct) = charge terminée, rien d'anormal (détecteur de
// courant muet jusqu'à ce que le niveau repasse sous kChargeTermineeRearmePct) ; sinon
// c'est un déclenchement, signalé à HA, et un deuxième en moins de
// kVerrouElectriqueFenetreMs verrouille le chargeur jusqu'au redémarrage.
// Non mesuré avec une batterie (aucune chez l'auteur) : la tension lue en fin de charge
// et le courant d'une batterie pleine.
constexpr float kChargeurTensionMaxV = 8.45f;
constexpr float kChargeurCourantMinA = 0.020f;
constexpr uint32_t kChargeurCourantFaibleMs = 30u * 1000u;
constexpr uint32_t kVerrouElectriqueFenetreMs = 60u * 60u * 1000u;
constexpr float kChargeTermineePct = 95.0f;
constexpr float kChargeTermineeRearmePct = 90.0f;

// Cause d'une coupure de sécurité, envoyée à HA (événement esphome.tab5_securite_chargeur,
// champ `cause`, cause_securite_texte()).
enum class CauseSecurite : uint8_t {
    AUCUNE = 0,
    THERMIQUE = 1,  // CPU trop chaud (mesure en °C)
    TENSION = 2,    // tension trop haute chargeur allumé (mesure en V)
    COURANT = 3,    // aucun courant de charge chargeur allumé (mesure en A)
};

inline const char* cause_securite_texte(CauseSecurite c) {
    switch (c) {
        case CauseSecurite::THERMIQUE: return "thermique";
        case CauseSecurite::TENSION: return "tension";
        case CauseSecurite::COURANT: return "courant";
        default: return "";
    }
}

// Une coupure à signaler à HA (une fois : chargeur_signal_prendre la vide).
struct SignalSecurite {
    CauseSecurite cause = CauseSecurite::AUCUNE;
    float mesure = NAN;
    bool verrouille = false;  // chargeur verrouillé jusqu'au redémarrage
};

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

// ─── Niveau d'après la tension : courbe de décharge MESURÉE sur un Tab5 (15/10/2026) ───
// Historique : jusqu'à la 3.9.0-rc.1, une droite 6,0 V = 0 % / 8,23 V = 100 %
// (battery-percentage.yaml, config de référence ESPHome) ; en 3.9.0-rc.2, la table à vide
// d'une autre batterie (Samsung EB535151VU, noyau Linux, #468). Toutes deux faisaient
// tenir le niveau des heures puis s'effondrer (rc.2 chez husyildiz, discussion #278 :
// 85 % de 21 h à 2 h, puis 58 → 7 % en deux heures).
// Table : la batterie d'origine du Tab5 de husyildiz (ST7121, 2S), deux décharges complètes
// à charge constante, écran éteint (≈ 0,17 A, 1,2 W, « Tab5 Courant batterie »), lues dans
// l'historique de Home Assistant (CSV joints à la discussion #278 le 15/10/2026) :
//   - nuit du 12 au 13/10 : débranchée à 20 h 19 (8,35 V), coupure de la protection à 9 h 42
//     (5,11 V), 13 h 23 de décharge. Capacité restante = temps restant jusqu'à la coupure
//     (charge constante), au premier passage sous chaque tension, tous les 25 mV par élément ;
//   - nuit du 14 au 15/10, contre-épreuve : chaque tension franchie à moins de 5 min de la
//     première nuit (8,10 à 7,20 V) ; la table prédit son niveau, heure par heure, à 3 points
//     près au plus.
// Ramenée à 0 % à 3,30 V par élément (6,60 V : il restait 1,5 % du temps, ≈ 12 min à
// 1,2 W) et 100 % à 4,175 V (8,35 V, juste après la charge) ; points en double retirés.
// Limites, connues :
//   - tension SOUS CHARGE légère (1,2 W), ce que l'INA226 lit en usage : écran allumé, la
//     tension baisse un peu plus (résistance interne) et le niveau lit un peu bas ;
//   - pendant la charge, la tension lue est plus haute : le niveau lit trop haut (la limite
//     à 80 % s'arrête avant 80 % réels) ;
//   - une seule batterie mesurée, à une température (intérieur) ;
//   - plateau de 8,00 à 8,05 V (62 → 81 %) : la tension est publiée à 50 mV près (filtre de
//     « Tab5 Tension batterie »), le niveau peut y sauter d'un coup de ~20 %.
constexpr int kBatterieElements = 2;   // 2S
struct PointDecharge {
    uint16_t mv;     // tension d'un élément sous charge légère (mV)
    uint8_t pct;     // capacité restante (%)
};
// Huit points par ligne : 25 lignes d'un point se reliraient mal.
// clang-format off
constexpr PointDecharge kCourbeDecharge[] = {
    {3300, 0},  {3350, 1},  {3500, 2},  {3550, 3},  {3625, 4},  {3675, 5},  {3700, 7},  {3725, 11},
    {3750, 13}, {3775, 15}, {3800, 18}, {3825, 22}, {3850, 30}, {3875, 41}, {3900, 47}, {3925, 51},
    {3950, 53}, {3975, 57}, {4000, 62}, {4025, 81}, {4050, 90}, {4075, 95}, {4100, 97}, {4125, 98},
    {4175, 100},
};
// clang-format on
constexpr int kCourbePoints = sizeof(kCourbeDecharge) / sizeof(kCourbeDecharge[0]);
// Bornes de la table, pour la batterie entière (2S) : 6,60 V = 0 %, 8,35 V = 100 %.
constexpr float kBatterieVideV = kBatterieElements * kCourbeDecharge[0].mv / 1000.0f;
constexpr float kBatteriePleineV = kBatterieElements * kCourbeDecharge[kCourbePoints - 1].mv / 1000.0f;

// ─── Limite de charge à 80 % ───
// Le niveau lit trop haut pendant la charge : l'arrêt se fait un peu avant 80 % réels.
// Avec la courbe mesurée (15/10/2026) : arrêt vers 8,05 V lus, reprise vers 8,02 V lus
// chargeur coupé (7,96 / 7,84 V avec la table Samsung, 7,78 / 7,56 V avec la droite).
// Les deux sont sur le plateau, 26 mV d'écart : kChargePauseMinMs évite le va-et-vient.
// Non mesuré sur une tablette.
// [AI-WARNING] La sonde périodique coupe CHG_EN kChargeurReposMs sans être un arrêt de la
// limite : après elle, la décision reprend l'état d'avant (charge_avant_sonde). Jusqu'au
// 20/10/2026 elle comptait comme un arrêt (discussion #278, husyildiz : « la limite de
// 80 % arrête la charge là où est le niveau ») : entre 70 et 80 %, la charge s'arrêtait
// à la première sonde ; sous 70 %, elle reprenait 10 min plus tard pour quelques secondes,
// jusqu'à la sonde suivante.
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
// batterie a-t-elle été vue pendant le démarrage précédent ? Depuis le 16/10/2026, seul
// VUE autorise le réveil de 30 s, et seulement après une sonde qui n'a rien vu et un
// démarrage normal (chargeur_demarrer). INCONNUE (premier démarrage, NVS vide) et AUCUNE :
// pas de réveil. Valeurs gardées en NVS : ne pas les renuméroter.
enum class MemoireBatterie : uint8_t {
    INCONNUE = 0,
    AUCUNE = 1,  // le démarrage précédent a décidé « pas de batterie », sans en voir une
    VUE = 2,     // une batterie vue (chargeur coupé, ≥ kBatterieSeuilV) au démarrage précédent
};

// Valeurs par défaut = celles d'un démarrage (chargeur_demarrer) : chargeur coupé, une
// sonde d'abord. Rien ne doit allumer CHG_EN avant une lecture ≥ kBatterieSeuilV.
struct EtatChargeur {
    PresenceBatterie presence = PresenceBatterie::INCONNUE;
    bool allume = false;           // CHG_EN commandé
    uint32_t change_ms = 0;        // instant de la dernière commande
    bool reveil = false;           // allumé kChargeurReveilDureeMs, puis une sonde
    bool reveil_apres_sonde = false;  // démarrage : réveil si la sonde ne voit pas de batterie
    bool vue = false;              // une batterie vue depuis le démarrage (MemoireBatterie::VUE)
    bool montee_avant = false;     // « Tab5 Batterie montée » au pas précédent
    bool reveil_demande = false;   // « montée » vient d'être allumé : réveil sans attendre l'heure
    bool sonde = true;             // chargeur coupé pour lire la tension
    bool lecture_demandee = false;
    bool sonde_vite = false;       // lecture basse chargeur allumé : sonder sans attendre
    uint32_t sonde_fin_ms = 0;     // fin de la dernière sonde (0 : aucune encore)
    bool charge_avant_sonde = false;  // sonde périodique d'une charge : la limite 80 % la reprend
    float tension = NAN;           // dernière lecture (V)
    float niveau = NAN;            // %, batterie présente seulement
    bool verrou_thermique = false; // sécurité anti-surchauffe active (coupure totale de charge)
    uint32_t verrou_thermique_ms = 0; // instant du dernier déclenchement de surchauffe
    float derniere_temp_cpu = NAN; // dernière température CPU (°C), NAN si la dernière était illisible
    float temp_precedente_pente = NAN; // température de référence pour calcul de pente
    uint32_t temp_precedente_ms = 0;
    // Verrou électrique (16/10/2026).
    CauseSecurite securite_sonde = CauseSecurite::AUCUNE;  // déclenchement que la sonde en cours tranche
    float securite_mesure = NAN;
    bool electrique_vu = false;    // un déclenchement électrique depuis le démarrage
    uint32_t electrique_ms = 0;    // instant du dernier
    bool verrou_electrique = false;  // chargeur verrouillé jusqu'au redémarrage
    bool courant_faible = false;   // |courant| < kChargeurCourantMinA depuis courant_faible_ms
    uint32_t courant_faible_ms = 0;
    bool courant_relu = false;     // lecture de confirmation demandée
    bool charge_terminee = false;  // batterie pleine : courant nul normal, détecteur muet
    SignalSecurite signal;         // à envoyer à HA
};

struct ActionChargeur {
    bool allumer = false;  // état voulu de CHG_EN
    bool lire = false;     // demander une lecture de l'INA226 maintenant
};

inline void chargeur_commander(EtatChargeur& c, bool allume, uint32_t maintenant_ms) {
    if (allume == c.allume) return;
    c.allume = allume;
    c.change_ms = maintenant_ms;
}

// Verrou électrique : chargeur coupé, sonde immédiate ; la fin de la sonde
// (chargeur_securite_trancher) dit si c'était une panne.
inline void chargeur_securite_declencher(EtatChargeur& c, CauseSecurite cause, float mesure,
                                         uint32_t maintenant_ms) {
    c.securite_sonde = cause;
    c.securite_mesure = mesure;
    c.reveil = false;
    c.reveil_demande = false;
    c.reveil_apres_sonde = false;
    c.courant_faible = false;
    c.sonde = true;
    c.sonde_vite = false;
    c.lecture_demandee = false;
    c.charge_avant_sonde = false;  // un vrai arrêt pour la limite 80 %, comme avant
    chargeur_commander(c, false, maintenant_ms);
}

// Fin de la sonde d'un déclenchement électrique (lecture faite, ou sonde abandonnée).
// Courant nul et batterie pleine : la charge est finie. Sinon un déclenchement, signalé ;
// le deuxième en moins de kVerrouElectriqueFenetreMs verrouille jusqu'au redémarrage.
inline void chargeur_securite_trancher(EtatChargeur& c, uint32_t maintenant_ms) {
    const CauseSecurite cause = c.securite_sonde;
    if (cause == CauseSecurite::AUCUNE) return;
    c.securite_sonde = CauseSecurite::AUCUNE;
    if (cause == CauseSecurite::COURANT && c.presence == PresenceBatterie::PRESENTE &&
        std::isfinite(c.niveau) && c.niveau >= kChargeTermineePct) {
        c.charge_terminee = true;
        return;
    }
    const bool recidive = c.electrique_vu && maintenant_ms - c.electrique_ms < kVerrouElectriqueFenetreMs;
    c.electrique_vu = true;
    c.electrique_ms = maintenant_ms;
    if (recidive) c.verrou_electrique = true;
    c.signal.cause = cause;
    c.signal.mesure = c.securite_mesure;
    c.signal.verrouille = c.verrou_electrique;
}

// Surveillance thermique appelée à chaque relevé de température interne :
// Renvoie true si le verrou thermique vient d'être déclenché (front montant).
// Température illisible : oubliée (derniere_temp_cpu = NAN), le verrou ne se lève pas.
inline bool chargeur_surveillance_thermique(EtatChargeur& c, float temp_c, uint32_t maintenant_ms) {
    if (!std::isfinite(temp_c)) {
        c.derniere_temp_cpu = NAN;
        return false;
    }
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
            c.signal.cause = CauseSecurite::THERMIQUE;
            c.signal.mesure = temp_c;
            c.signal.verrouille = c.verrou_electrique;
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
        c.niveau = c.presence == PresenceBatterie::PRESENTE ? batterie_niveau_pct(tension) : NAN;
        if (c.charge_terminee && (c.presence != PresenceBatterie::PRESENTE || c.niveau < kChargeTermineeRearmePct))
            c.charge_terminee = false;
        if (c.sonde) {
            c.sonde = false;
            c.lecture_demandee = false;
            c.sonde_fin_ms = maintenant_ms;
            chargeur_securite_trancher(c, maintenant_ms);
        }
        return c.presence != avant;
    }
    if (c.allume && !c.sonde && tension > kChargeurTensionMaxV) {
        chargeur_securite_declencher(c, CauseSecurite::TENSION, tension, maintenant_ms);
    } else if (c.allume && c.presence == PresenceBatterie::PRESENTE && tension < kBatterieBasseV) {
        c.sonde_vite = true;
    }
    c.niveau = c.presence == PresenceBatterie::PRESENTE ? batterie_niveau_pct(tension) : NAN;
    return c.presence != avant;
}

// Une lecture du courant de la batterie (A, + = décharge) : chargeur allumé hors réveil
// et hors sonde, un courant nul pendant kChargeurCourantFaibleMs déclenche le verrou
// électrique. La lecture de confirmation est demandée par chargeur_tick (courant_relu).
inline void chargeur_lecture_courant(EtatChargeur& c, float courant_a, uint32_t maintenant_ms) {
    if (!std::isfinite(courant_a)) return;
    if (!c.allume || c.sonde || c.reveil || c.charge_terminee || std::fabs(courant_a) >= kChargeurCourantMinA) {
        c.courant_faible = false;
        return;
    }
    if (!c.courant_faible) {
        c.courant_faible = true;
        c.courant_faible_ms = maintenant_ms;
        c.courant_relu = false;
        return;
    }
    if (maintenant_ms - c.courant_faible_ms >= kChargeurCourantFaibleMs)
        chargeur_securite_declencher(c, CauseSecurite::COURANT, courant_a, maintenant_ms);
}

// Courant lu chargeur coupé pour une sonde (ou à la lecture qui la termine) : à ignorer.
inline bool chargeur_en_sonde(const EtatChargeur& c, uint32_t maintenant_ms) {
    return c.sonde || (c.sonde_fin_ms != 0 && maintenant_ms - c.sonde_fin_ms < kChargeurSondeFinMs);
}

// Un verrou de sécurité est posé (« Tab5 Sécurité chargeur ») : thermique ou électrique.
inline bool chargeur_securite_posee(const EtatChargeur& c) {
    return c.verrou_thermique || c.verrou_electrique;
}

// La coupure à signaler à HA, une seule fois (vide ensuite).
inline SignalSecurite chargeur_signal_prendre(EtatChargeur& c) {
    const SignalSecurite s = c.signal;
    c.signal = SignalSecurite();
    return s;
}

// Chargeur voulu hors réveil et hors sonde. Présence inconnue (aucune lecture chargeur
// coupé n'a abouti, INA226 muet) : coupé depuis le 16/10/2026, jamais allumé à l'aveugle.
// Limite 80 % : une charge coupée par la sonde périodique compte encore comme une charge
// (charge_avant_sonde) ; seul un arrêt de la limite attend 70 % et kChargePauseMinMs.
inline bool chargeur_voulu(const EtatChargeur& c, uint8_t limite, uint32_t maintenant_ms) {
    if (chargeur_securite_posee(c)) return false;
    if (c.presence != PresenceBatterie::PRESENTE) return false;
    if (static_cast<LimiteCharge>(limite) != LimiteCharge::QUATRE_VINGTS) return true;
    const bool en_charge = c.allume || c.charge_avant_sonde;
    if (!std::isfinite(c.niveau)) return en_charge;
    if (en_charge) return c.niveau < kChargeArretPct;
    return c.niveau <= kChargeReprisePct && maintenant_ms - c.change_ms >= kChargePauseMinMs;
}

// À appeler chaque seconde (`montee` : interrupteur « Tab5 Batterie montée »). Renvoie
// l'état voulu de CHG_EN et s'il faut lire l'INA226 tout de suite (fin du repos d'une
// sonde, confirmation d'un courant nul : sans elle, la lecture suivante attendrait 60 s).
inline ActionChargeur chargeur_tick(EtatChargeur& c, uint8_t limite, bool montee, uint32_t maintenant_ms) {
    ActionChargeur a;
    // Verrouillage thermique actif : chargeur INTERDIT (aucune tentative de réveil ni de sonde).
    // Levé seulement par une température LUE saine (une lecture NAN ne le lève plus).
    if (c.verrou_thermique) {
        if (maintenant_ms - c.verrou_thermique_ms >= kChargeurVerrouMinMs &&
            std::isfinite(c.derniere_temp_cpu) && c.derniere_temp_cpu <= kChargeurTempRearmeC) {
            c.verrou_thermique = false;
        } else {
            c.allume = false;
            c.sonde = false;
            c.reveil = false;
            c.reveil_demande = false;
            c.reveil_apres_sonde = false;
            c.courant_faible = false;
            c.securite_sonde = CauseSecurite::AUCUNE;  // sonde interrompue : le verrou thermique prime
            c.charge_avant_sonde = false;  // un vrai arrêt pour la limite 80 %
            a.allumer = false;
            a.lire = false;
            return a;
        }
    }
    // Verrou électrique : coupé jusqu'au redémarrage ; les lectures (chargeur coupé)
    // continuent de dire la présence.
    if (c.verrou_electrique) {
        c.reveil = false;
        c.reveil_demande = false;
        c.reveil_apres_sonde = false;
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
            c.allume = false;
            a.allumer = false;
            return a;
        }
        // Pas de lecture (INA226 muet) : sonde abandonnée, présence gardée ; un
        // déclenchement électrique compte quand même.
        c.sonde = false;
        c.lecture_demandee = false;
        c.sonde_fin_ms = maintenant_ms;
        chargeur_securite_trancher(c, maintenant_ms);
    }
    // Démarrage : la sonde n'a pas vu de batterie, mais le démarrage précédent en avait
    // vu une (démarrage normal) : un réveil, au cas où sa protection l'a coupée.
    if (c.reveil_apres_sonde) {
        c.reveil_apres_sonde = false;
        if (c.presence == PresenceBatterie::ABSENTE && !chargeur_securite_posee(c)) {
            c.reveil = true;
            chargeur_commander(c, true, maintenant_ms);
        }
    }
    bool sonder = false;
    bool sonde_de_charge = false;  // sonde périodique d'une charge (pas la fin d'un réveil)
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
        // Batterie : retirée depuis ?
        sonder = c.sonde_vite || maintenant_ms - c.sonde_fin_ms >= kChargeurSondeMs;
        sonde_de_charge = true;
    } else if (c.presence == PresenceBatterie::ABSENTE && montee && !c.allume && !chargeur_securite_posee(c) &&
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
        c.courant_faible = false;
        c.charge_avant_sonde = sonde_de_charge;  // après la sonde, la limite 80 % la reprend
        chargeur_commander(c, false, maintenant_ms);
        a.allumer = false;
        return a;
    }
    chargeur_commander(c, chargeur_voulu(c, limite, maintenant_ms), maintenant_ms);
    c.charge_avant_sonde = false;  // décidée : CHG_EN dit la suite
    a.allumer = c.allume;
    if (!c.allume) {
        c.courant_faible = false;
    } else if (c.courant_faible && !c.courant_relu && maintenant_ms - c.courant_faible_ms >= kChargeurCourantFaibleMs) {
        c.courant_relu = true;  // confirmer le courant nul sans attendre la minute
        a.lire = true;
    }
    return a;
}

// Premier pas après le démarrage (chargeur_pas, une fois). CHG_EN est COUPÉ depuis son
// setup (`restore_mode: ALWAYS_OFF`, 10/10/2026). Depuis le 16/10/2026, toujours une sonde
// d'abord, chargeur coupé (lecture au bout de kChargeurReposMs) : CHG_EN ne s'allume
// qu'après une lecture ≥ kBatterieSeuilV, ou pour un réveil si cette sonde ne voit rien,
// que `memoire` (MemoireBatterie du démarrage précédent) vaut VUE et que le démarrage
// n'est pas `anormal` (plantage, baisse de tension : journal_demarrage_anormal()).
// `verrou_thermique` : le verrou thermique était posé au démarrage précédent (NVS) ; il
// repart posé, chargeur coupé au moins kChargeurVerrouMinMs, puis jusqu'à une
// température lue saine. `montee` : « Tab5 Batterie montée » restauré.
inline void chargeur_demarrer(EtatChargeur& c, uint8_t memoire, bool montee, bool anormal, bool verrou_thermique,
                              uint32_t maintenant_ms) {
    c.allume = false;
    c.change_ms = maintenant_ms;
    c.montee_avant = montee;  // restauré : pas un « vient d'être allumé »
    c.reveil_demande = false;
    c.lecture_demandee = false;
    c.reveil = false;
    c.sonde = true;
    c.charge_avant_sonde = false;  // démarrage : chargeur arrêté, la pause de 10 min compte
    c.reveil_apres_sonde = static_cast<MemoireBatterie>(memoire) == MemoireBatterie::VUE && !anormal;
    if (verrou_thermique) {
        c.verrou_thermique = true;
        c.verrou_thermique_ms = maintenant_ms;
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
// appel, chargeur_demarrer() avec `memoire` (MemoireBatterie du démarrage précédent),
// `anormal` (journal_demarrage_anormal()) et le verrou thermique gardé en NVS.
ActionChargeur chargeur_pas(uint8_t limite, bool montee, uint8_t memoire, bool anormal, uint32_t maintenant_ms);
// Une lecture du courant de la batterie (A) : verrou électrique (chargeur_lecture_courant).
void chargeur_courant(float courant_a, uint32_t maintenant_ms);
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
// « Tab5 Sécurité chargeur » (16/10/2026) : un verrou thermique ou électrique est posé.
bool chargeur_securite_active();
// La coupure de sécurité à envoyer à HA (événement esphome.tab5_securite_chargeur), une
// fois ; cause AUCUNE s'il n'y en a pas.
SignalSecurite chargeur_securite_signal();
// Rendu hors tablette (Tab5/rendu/bouchons.yaml) : chargeur coupé depuis longtemps, la
// lecture suivante décide la présence d'après sa tension.
void chargeur_rendu_coupe(uint32_t maintenant_ms);
