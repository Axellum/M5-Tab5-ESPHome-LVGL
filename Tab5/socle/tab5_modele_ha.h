/**
 * [AI-CONTEXT]
 * @file tab5_modele_ha.h
 * @role Ce que les deux modèles d'appareils de HA ont vraiment en commun (audit du
 *       07/10/2026, lot L5) : les tuiles de pièce (tab5_tuiles.cpp, ADR-0023) et la tuile
 *       − / + (tab5_reglables.cpp, ADR-0033). Tailles des textes gardés, comparaison d'un
 *       état, état « hors ligne », code d'icône de la palette ; et la clé d'emplacement
 *       d'une tuile (tuile_cle, lot L7), lue aussi par les clims des tuiles (tab5_clim.cpp).
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

// Caractère T d'une clé de tuile (« tRT », « crRT », « ceRT », ADR-0066) : l'élément T de sa
// pièce, 0 … 19, écrit « 0 » … « 9 » puis « a » … « j ». Les clés d'avant (« t00 » … « t44 »)
// gardent leur sens. tuile_index() le relit ; faux pour tout autre caractère (une clé qu'un
// firmware plus ancien ignorait, « tRa » … « tRj », il la lit maintenant).
constexpr char kCarsTuile[] = "0123456789abcdefghij";
constexpr int kTuilesCles = sizeof(kCarsTuile) - 1;

inline char tuile_car(int t) { return (t >= 0 && t < kTuilesCles) ? kCarsTuile[t] : '?'; }

inline bool tuile_index(char c, int& t) {
    if (c >= '0' && c <= '9') t = c - '0';
    else if (c >= 'a' && c < 'a' + (kTuilesCles - 10)) t = 10 + (c - 'a');
    else return false;
    return true;
}

// Clé d'emplacement d'une tuile de pièce, « tRT » (R pièce, un chiffre ; T élément,
// tuile_car), ou d'une pièce entière, « pR » (« Tout éteindre ») : l'emplacement que reçoit
// l'événement esphome.tab5_action (ADR-0023) ; les clims des tuiles gardent la même
// (ADR-0027). Rendue par valeur, le texte dans `s` : `envoyer(tuile_cle(r, t).s, action)`.
// Audit du 07/10/2026 (lot L7) : elle était écrite à la main six fois.
struct CleTuile {
    char s[4] = "";
};

inline CleTuile tuile_cle(int r, int t) {
    CleTuile c;
    c.s[0] = 't';
    c.s[1] = static_cast<char>('0' + r);
    c.s[2] = tuile_car(t);
    return c;
}

inline CleTuile piece_cle(int r) {
    CleTuile c;
    c.s[0] = 'p';
    c.s[1] = static_cast<char>('0' + r);
    return c;
}

// Chiffre de la pièce dans une clé (« pR », « tRT », « crpR », « cepR », lien « tRT » de la
// tuile − / +) : '0' à '9' depuis l'ADR-0066 (lot A, 18/10/2026), '0' à '4' avant. Faux,
// `r` intact, pour tout autre caractère. Le nombre de pièces choisi dans HA (« pn ») borne
// ensuite ce que l'écran montre, pas ce qui est lu.
inline bool piece_index(char c, int& r) {
    if (c < '0' || c > '9') return false;
    r = c - '0';
    return true;
}

// Emplacement des commandes de la clim de la pièce R (ADR-0040) : « cpR ». Pas « pR »
// + action : « pR » / « eteindre » est déjà « Pièce : tout éteindre ».
inline CleTuile clim_piece_cle(int r) {
    CleTuile c;
    c.s[0] = 'c';
    c.s[1] = 'p';
    c.s[2] = static_cast<char>('0' + r);
    return c;
}

// La clim « d'ici » (14/10/2026, discussion #278, décision d'Axel) parmi les `n` clims de
// `l`, rangées comme clims_enumerer (tab5_clim.cpp) : celle du blueprint d'abord quand elle
// existe (r < 0), puis celles des tuiles (t >= 0), puis la clim propre de chaque pièce
// (t < 0, ADR-0040), chacune avec sa pièce (`piece`, -1 pour celle du blueprint).
//   - une seule clim dans la maison : elle, quelle que soit la pièce affichée ;
//   - plusieurs : celle de la pièce `piece_affichee` (-1 : aucune, mode météo) — sa clim
//     propre d'abord, sinon la première de ses tuiles —, sinon celle du blueprint ;
//   - sinon -1 (aucune clim, ou plusieurs et ni la pièce ni le blueprint n'en ont).
// Renvoie son rang dans `l`. Ref : tout type à champs r, t et piece (ClimRef). Pure,
// testée par tools/test_tab5_socle.cpp.
template <class Ref>
int clim_ici_rang(const Ref* l, int n, int piece_affichee) {
    if (l == nullptr || n <= 0) return -1;
    if (n == 1) return 0;
    int i = -1;
    for (int k = 0; piece_affichee >= 0 && k < n; k++) {
        if (l[k].piece != piece_affichee) continue;
        if (l[k].t < 0) return k;  // la clim propre de la pièce passe avant ses tuiles
        if (i < 0) i = k;
    }
    if (i >= 0) return i;
    return l[0].r < 0 ? 0 : -1;
}

}  // namespace modele_ha
