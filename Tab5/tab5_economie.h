/**
 * [AI-CONTEXT]
 * @file tab5_economie.h
 * @role Mode économie d'énergie (06/10/2026, demande d'Axel) : la logique PURE. Le select
 *       « Tab5 Économie d'énergie » (Tab5/tab5-economie.yaml) choisit Jamais / Sur
 *       batterie (défaut) / Toujours. Actif, le mode :
 *         - plafonne la luminosité à kEcoPlafond, et la baisse au plus bas
 *           (kEcoPlancher, le minimum du curseur des Réglages) après kEcoAssombrirMs
 *           sans toucher ou sous kEcoNiveauBas % de batterie ;
 *         - coupe les animations du projet (tab5_anim.cpp : panneau tournant, alertes,
 *           icônes, horloge, glissements) ;
 *         - limite LVGL à 30 images/s (kEcoPeriodeMs), sauf pendant un jeu.
 *       « Sur batterie » se décide au courant de l'INA226 (economie_courant_lu).
 * @architecture_constraint Aucune dépendance ESPHome ni LVGL : ce fichier se compile sur
 *       PC (tools/test_alarm_clock.cpp l'inclut, g++ en CI). L'état vivant et les appels
 *       du YAML sont dans tab5_economie.cpp.
 * @ai_instruction L'ordre de ChoixEconomie est celui des options du select (la tablette
 *       garde l'INDEX choisi) : une option nouvelle s'ajoute à la fin
 *       (tests/test_economie.py relit les deux).
 */
#pragma once
#include <cmath>
#include <cstdint>

// Options du select « Tab5 Économie d'énergie », dans l'ordre.
enum class ChoixEconomie : uint8_t {
    JAMAIS = 0,
    SUR_BATTERIE = 1,  // défaut : rien ne change sur secteur
    TOUJOURS = 2,
};

// ─── Sur batterie ? (courant de l'INA226, lu toutes les 60 s) ───
// Sens : M5Unified (Power_Class.inl, getBatteryCurrent) note que le shunt du Tab5 est
// câblé pour que la charge se lise négative ; ESPHome publie le registre brut, signé.
// Donc ici + = la batterie se décharge (la tablette tourne sur elle). Non vérifié sur
// une tablette au 06/10/2026 (l'auteur n'a pas de batterie) : l'entité « Tab5 Courant
// batterie » le dit. Hystérésis : au-dessus de 50 mA de décharge on passe sur batterie,
// on en sort sous 20 mA. « En charge » (CHG_STAT) ou pas de batterie détectée : jamais.
constexpr float kEcoCourantEntreeA = 0.050f;
constexpr float kEcoCourantSortieA = 0.020f;

// ─── Batterie basse : la luminosité au plus bas ───
// Niveau d'après la tension (« Tab5 Batterie », NAN tant que la batterie n'est pas
// détectée). Hystérésis : basse à 35 % ou moins, plus basse à 40 % ou plus.
constexpr float kEcoNiveauBas = 35.0f;
constexpr float kEcoNiveauRetour = 40.0f;

// ─── Effets du mode ───
// Luminosités en fraction de la lumière « Display Backlight » (0-1, avant la correction
// gamma, comme HA et le curseur des Réglages). Le plancher est le minimum du curseur
// (reglages_popup.yaml, min_value: 10) : plus bas, l'écran devient illisible.
constexpr float kEcoPlafond = 0.50f;
constexpr float kEcoPlancher = 0.10f;
constexpr uint32_t kEcoAssombrirMs = 30u * 1000u;  // sans toucher, puis l'extinction auto
// Période de rafraîchissement de LVGL : 33 ms = 30 images/s. 16 ms = celle d'ESPHome
// (LvglComponent::refr_timer_period_, LV_DEF_REFR_PERIOD de lvgl/__init__.py).
constexpr uint32_t kEcoPeriodeMs = 33u;
constexpr uint32_t kEcoPeriodeNormaleMs = 16u;

struct EtatEconomie {
    bool sur_batterie = false;
    bool batterie_basse = false;
    // Dernier niveau reçu (%, NAN = inconnu). Gardé : le niveau n'est publié qu'à 50 mV
    // près ou toutes les 15 min, et une tablette débranchée à 30 % doit baisser dès
    // qu'elle se sait sur batterie, pas à la publication suivante.
    float niveau = NAN;
};

// Batterie basse d'après le dernier niveau : jamais sur secteur ; niveau inconnu, la
// décision reste.
inline void economie_basse_maj(EtatEconomie& e) {
    if (!e.sur_batterie) {
        e.batterie_basse = false;
    } else if (std::isfinite(e.niveau)) {
        if (e.niveau <= kEcoNiveauBas) e.batterie_basse = true;
        else if (e.niveau >= kEcoNiveauRetour) e.batterie_basse = false;
    }
}

// Une lecture du courant (A, + = décharge). Lecture ratée (NAN) : rien ne change.
// Vrai si « sur batterie » a changé (la batterie basse suit).
inline bool economie_courant_lu(EtatEconomie& e, float courant_a, bool batterie_presente, bool en_charge) {
    const bool avant = e.sur_batterie;
    if (!batterie_presente || en_charge) {
        e.sur_batterie = false;
    } else if (std::isfinite(courant_a)) {
        if (courant_a >= kEcoCourantEntreeA) e.sur_batterie = true;
        else if (courant_a <= kEcoCourantSortieA) e.sur_batterie = false;
    }
    economie_basse_maj(e);
    return e.sur_batterie != avant;
}

// La tablette se remet à charger : plus sur batterie tout de suite, sans attendre la
// lecture suivante du courant. Vrai si « sur batterie » a changé.
inline bool economie_en_charge(EtatEconomie& e, bool en_charge) {
    if (!en_charge || !e.sur_batterie) return false;
    e.sur_batterie = false;
    economie_basse_maj(e);
    return true;
}

// Un niveau de batterie (%, NAN = inconnu). Vrai si « batterie basse » a changé.
inline bool economie_niveau_lu(EtatEconomie& e, float niveau) {
    const bool avant = e.batterie_basse;
    e.niveau = niveau;
    economie_basse_maj(e);
    return e.batterie_basse != avant;
}

struct EntreesEconomie {
    uint8_t choix = 0;            // index du select (ChoixEconomie)
    bool sur_batterie = false;
    bool batterie_basse = false;
    bool ecran_allume = false;
    // Rien ne retient l'écran (réveil qui sonne, voix, jeu, mise à jour, démarrage) :
    // la même liste que l'extinction auto, qui la calcule (tab5-ha-controls.yaml).
    bool veille_permise = false;
    uint32_t inactif_ms = 0;      // depuis le dernier toucher ou le dernier allumage
    bool jeu_ouvert = false;
};

struct DecisionEconomie {
    bool active = false;
    bool assombri = false;
    float plafond = 1.0f;          // luminosité maximale (0-1, avant la correction gamma)
    uint32_t periode_ms = kEcoPeriodeNormaleMs;
    bool animations_reduites = false;
};

inline bool economie_active(uint8_t choix, bool sur_batterie) {
    switch (static_cast<ChoixEconomie>(choix)) {
        case ChoixEconomie::TOUJOURS: return true;
        case ChoixEconomie::SUR_BATTERIE: return sur_batterie;
        default: return false;  // Jamais, ou un index inconnu
    }
}

inline DecisionEconomie economie_decider(const EntreesEconomie& in) {
    DecisionEconomie d;
    d.active = economie_active(in.choix, in.sur_batterie);
    if (!d.active) return d;
    d.assombri = in.ecran_allume && in.veille_permise && in.inactif_ms >= kEcoAssombrirMs;
    d.plafond = (d.assombri || in.batterie_basse) ? kEcoPlancher : kEcoPlafond;
    // Un jeu garde ses 60 images/s (Neon Apron, Arcanoïde…) ; ses animations sont les
    // siennes, pas celles de tab5_anim.cpp.
    d.periode_ms = in.jeu_ouvert ? kEcoPeriodeNormaleMs : kEcoPeriodeMs;
    d.animations_reduites = true;
    return d;
}

// ─── Appels du YAML (tab5_economie.cpp : un seul état, celui de la tablette) ───
// INA226 (tab5-sensors-diagnostics.yaml) et CHG_STAT : vrai si « sur batterie » a changé.
bool economie_courant(float courant_a, bool batterie_presente, bool en_charge);
bool economie_charge(bool en_charge);
void economie_niveau(float niveau);
bool economie_sur_batterie();
bool economie_batterie_basse();
// Rétroéclairage plafonné : la lumière écrit `demande` (après gamma) dans la sortie
// template, qui envoie au PWM le minimum de `demande` et du plafond courant.
float economie_sortie_retro(float demande);
// Nouveau plafond (après gamma). Vrai s'il a changé : le YAML renvoie alors
// economie_sortie_actuelle() au PWM, la lumière ne réécrivant rien d'elle-même.
bool economie_plafond_sortie(float plafond);
float economie_sortie_actuelle();
// Vrai si la période de LVGL demandée a changé (le YAML l'applique alors).
bool economie_periode(uint32_t periode_ms);
