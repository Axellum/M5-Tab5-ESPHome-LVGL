/**
 * [AI-CONTEXT]
 * @file tab5_modele_ha.h
 * @role Ce que les deux modèles d'appareils de HA ont vraiment en commun (audit du
 *       07/10/2026, lot L5) : les tuiles de pièce (tab5_tuiles.cpp, ADR-0023) et la tuile
 *       − / + (tab5_reglables.cpp, ADR-0033). Tailles des textes gardés, comparaison d'un
 *       état, état « hors ligne », code d'icône de la palette ; et la clé d'emplacement
 *       d'une tuile (tuile_cle, lot L7), lue aussi par les clims des tuiles (tab5_cards.cpp).
 * @architecture_constraint En-tête seul, pur (ni ESPHome ni LVGL), dans le namespace
 *       `modele_ha` : il est aussi inclus dans main.cpp (includes: des deux
 *       configurations), des noms courts comme `est` ou `kNom` n'y entrent pas. Testé
 *       par tools/test_tab5_socle.cpp.
 * @ai_instruction Les types (Type), définitions (Def), états (Etat) et vues (Vue) des
 *       deux fichiers portent le même nom mais PAS le même contenu : types différents
 *       (lum/int/vol… contre son/lum/cli/eau…), champs différents, et Def / Etat sont
 *       écrits tels quels en NVS (magies « TUI1 », « RAN1 », « REG1 ») — les fusionner
 *       changerait la taille gardée et effacerait les définitions au premier démarrage.
 *       Ils restent dans leur fichier ; seul ce qui est identique vient ici. Lecture
 *       d'un champ, d'un nombre : tab5_champs.h.
 */
#pragma once
#include <cstddef>
#include <cstring>

namespace modele_ha {

// Taille des textes gardés (octets, zéro final compris), les mêmes pour les deux modèles.
constexpr size_t kNom = 25;    // nom affiché : 24 octets au plus
constexpr size_t kIcone = 16;  // code de palette ou classe d'appareil : [a-z0-9_]{1,15}
constexpr size_t kEtat = 16;   // état HA tel quel

// Deux textes égaux (un état reçu et un mot de HA : « on », « closed »…).
inline bool est(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

// État indisponible dans HA : « unavailable » ou « unknown » (« Hors ligne », grisé).
inline bool etat_indisponible(const char* brut) { return est(brut, "unavailable") || est(brut, "unknown"); }

// Code de palette, ou classe d'appareil : [a-z0-9_]{1,15} copié dans `dst` (kIcone
// octets), sinon vide (l'icône par défaut du type). Un code avec un autre caractère ou
// trop long n'est pas réparé ni coupé : il vaut vide.
inline void copier_icone(char* dst, const char* src, size_t n) {
    std::memset(dst, 0, kIcone);
    if (n == 0 || n >= kIcone) return;
    for (size_t i = 0; i < n; i++) {
        const char c = src[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return;
    }
    std::memcpy(dst, src, n);
}

// Clé d'emplacement d'une tuile de pièce, « tRT » (R pièce, T tuile, de 0 à 4), ou d'une
// pièce entière, « pR » (« Tout éteindre ») : l'emplacement que reçoit l'événement
// esphome.tab5_action (ADR-0023) ; les clims des tuiles gardent la même (ADR-0027). Rendue
// par valeur, le texte dans `s` : `envoyer(tuile_cle(r, t).s, action)`. Audit du
// 07/10/2026 (lot L7) : elle était écrite à la main six fois.
struct CleTuile {
    char s[4] = "";
};

inline CleTuile tuile_cle(int r, int t) {
    CleTuile c;
    c.s[0] = 't';
    c.s[1] = static_cast<char>('0' + r);
    c.s[2] = static_cast<char>('0' + t);
    return c;
}

inline CleTuile piece_cle(int r) {
    CleTuile c;
    c.s[0] = 'p';
    c.s[1] = static_cast<char>('0' + r);
    return c;
}

}  // namespace modele_ha
