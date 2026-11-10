/**
 * [AI-CONTEXT]
 * @file tab5_creneau.h
 * @role Page « Charge » du popup Voitures (ADR-0082) : deux calculs purs, testés sur un PC
 *       (tools/test_tab5_socle.cpp), sans ESPHome ni LVGL :
 *         - creneau_solaire() : le créneau CONSEILLÉ pour charger au soleil, d'après la prévision
 *           de production de chaque heure du jour (celle que la page « Aujourd'hui » reçoit déjà,
 *           tab5_maj_energie_soleil, `prevu`) ;
 *         - plages_charge() : les plages d'heures où une voiture a chargé, d'après ses 24 kWh par
 *           heure (tab5_maj_voiture_jour), pour dessiner sa ligne de la frise.
 * @architecture_constraint En-tête seul, sans dépendance au-delà de <cmath> : listé dans les
 *       `includes:` des deux configurations pour être copié dans le build (Tab5/socle/AGENTS.md).
 *       Le créneau est une SUGGESTION fondée sur une prévision, jamais un fait : l'écran le dit
 *       (tab5_voitures_charge.cpp). Aucune donnée inventée : une heure sans prévision (NAN) n'est
 *       jamais dans un créneau.
 * @ai_instruction Changer une règle (seuil, minimum) = sa constante ici, son cas dans
 *       tools/test_tab5_socle.cpp et l'ADR-0082 ; la phrase affichée se compose dans
 *       tab5_voitures_charge.cpp avec tr(), jamais ici.
 */
#pragma once
#include <cmath>

constexpr int kCreneauHeures = 24;   // les heures locales d'un jour
// Une heure compte dans le créneau si la prévision y atteint au moins cette part du pic du jour :
// la durée pendant laquelle le soleil est près de son maximum.
constexpr float kCreneauPart = 0.70f;
// Sous ce pic (kWh dans l'heure), aucun conseil : une prévision aussi faible ne vaut pas qu'on
// déplace une charge (et le rapport à un pic de bruit n'aurait pas de sens).
constexpr float kCreneauPicMin = 0.5f;

struct CreneauSolaire {
    int debut = -1;          // première heure (0..23), -1 : aucun créneau
    int fin = -1;            // fin EXCLUE (1..24)
    bool en_cours = false;   // il a déjà commencé : l'heure courante en fait partie
    bool valide() const { return debut >= 0 && fin > debut; }
};

// Le créneau conseillé : parmi les heures qui restent, la suite d'heures de prévision connue dont chacune
// atteint kCreneauPart du pic du jour ; s'il y en a plusieurs (un nuage au milieu), celle dont la somme
// prévue est la plus grande, la plus tôt si égalité.
//   prevu       kWh prévus à chaque heure locale du jour (NAN = pas de valeur : elle coupe une suite) ;
//   n           nombre de valeurs (24 au plus, le reste est ignoré) ;
//   maintenant  minutes depuis minuit, négatif si l'horloge n'est pas réglée (tout le jour compte).
// « Les heures qui restent » : l'heure courante compte tant que moins de la moitié en est passée
// (à 10 h 45 le créneau commence à 11 h, à 12 h 10 il peut commencer à 12 h : en_cours) ; le pic reste
// celui de TOUT le jour, passé compris : après le pic, un soleil qui baisse ne donne plus de créneau.
// Sans créneau (pic sous kCreneauPicMin, rien après maintenant, pas de prévision) : valide() est faux.
inline CreneauSolaire creneau_solaire(const float* prevu, int n, int maintenant) {
    CreneauSolaire r;
    if (prevu == nullptr || n < 1) return r;
    if (n > kCreneauHeures) n = kCreneauHeures;
    float pic = 0.0f;
    for (int h = 0; h < n; h++)
        if (std::isfinite(prevu[h]) && prevu[h] > pic) pic = prevu[h];
    if (pic < kCreneauPicMin) return r;
    const float seuil = pic * kCreneauPart;
    int h0 = 0;
    if (maintenant >= 0) {
        if (maintenant >= 24 * 60) return r;   // heure absurde : pas de conseil plutôt qu'un faux
        h0 = maintenant / 60 + (maintenant % 60 >= 30 ? 1 : 0);
    }
    float meilleure = -1.0f;
    int h = h0;
    while (h < n) {
        if (!std::isfinite(prevu[h]) || prevu[h] < seuil) {
            h++;
            continue;
        }
        int fin = h;
        float somme = 0.0f;
        while (fin < n && std::isfinite(prevu[fin]) && prevu[fin] >= seuil) {
            somme += prevu[fin];
            fin++;
        }
        if (somme > meilleure) {   // strictement : à somme égale, la plus tôt reste
            meilleure = somme;
            r.debut = h;
            r.fin = fin;
        }
        h = fin;
    }
    r.en_cours = r.valide() && maintenant >= 0 && r.debut <= maintenant / 60;
    return r;
}

// --- Les heures de charge d'une voiture (sa ligne de la frise) --------------------------------

// Une heure où la voiture a reçu au moins cette énergie (kWh) a chargé : en dessous, c'est le bruit
// d'un capteur ou la veille d'une voiture branchée, pas une charge.
constexpr float kChargeHeureMin = 0.1f;
// Alterner une heure de charge et une heure sans en donne douze au plus sur vingt-quatre.
constexpr int kPlagesMax = kCreneauHeures / 2;

struct Plage {
    int debut = 0;   // première heure (0..23)
    int fin = 0;     // fin EXCLUE (1..24)
};

// Les plages d'heures consécutives où la voiture a chargé (kwh[h] ≥ kChargeHeureMin), dans l'ordre du
// temps, `max` au plus dans `out` ; renvoie leur nombre. Une heure sans valeur (NAN : heure à venir,
// voiture endormie) ou sous le seuil coupe la plage. L'heure en cours est partielle dans la série de
// Home Assistant : une charge qui vient de commencer peut y être sous le seuil ; la plage commence
// alors à l'heure suivante, ce qui est vrai de la série reçue et rien de plus.
inline int plages_charge(const float* kwh, int n, Plage* out, int max) {
    if (kwh == nullptr || out == nullptr || max < 1) return 0;
    if (n > kCreneauHeures) n = kCreneauHeures;
    int nb = 0;
    int h = 0;
    while (h < n && nb < max) {
        if (!std::isfinite(kwh[h]) || kwh[h] < kChargeHeureMin) {
            h++;
            continue;
        }
        int fin = h;
        while (fin < n && std::isfinite(kwh[fin]) && kwh[fin] >= kChargeHeureMin) fin++;
        out[nb].debut = h;
        out[nb].fin = fin;
        nb++;
        h = fin;
    }
    return nb;
}
