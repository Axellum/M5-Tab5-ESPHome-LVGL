/**
 * [AI-CONTEXT]
 * @file tab5_tuiles_icones.h
 * @role Palette des icônes des tuiles de pièce (ADR-0023) : code de palette → glyphe MDI,
 *       variante éteinte / allumée, icône par défaut de chaque type de tuile.
 * @architecture_constraint AMORCE du contrat (28/09/2026) : ce fichier sera ÉCRIT par
 *       tools/gen_tuiles_icones.py depuis Tab5/tuiles_icones.yaml (lot « palette »). Il ne
 *       contient pour l'instant que des glyphes déjà présents dans mdi_font_32, _45 et _70.
 *       Seule l'API ci-dessous est figée : `tuile_icone(code, actif, type)`.
 *       Aucune dépendance (ni ESPHome, ni LVGL) : que des tables constantes.
 */
#pragma once
#include <cstring>

struct TuileIcone {
    const char *code;    // code de palette envoyé par HA ([a-z0-9_]{1,15})
    const char *eteint;  // glyphe UTF-8 quand l'appareil est éteint / fermé / inactif
    const char *allume;  // glyphe UTF-8 quand il est allumé / ouvert / actif
};

namespace tuiles_icones {

inline constexpr TuileIcone kPalette[] = {
    {"lit", "\U000F02E3", "\U000F02E3"},
    {"canape", "\U000F04B9", "\U000F04B9"},
    {"led", "\U000F1051", "\U000F1051"},
    {"ordinateur", "\U000F0379", "\U000F0379"},
};

// Icône par défaut d'un type de tuile (lum, int, vol, med, act, cap, bin, cli).
inline const char *defaut_du_type(const char *type) {
    (void) type;
    return "ordinateur";
}

}  // namespace tuiles_icones

// Glyphe à afficher pour une tuile : le code de palette reçu de HA, sinon le défaut de son
// type ; `actif` choisit la variante allumée. Ne renvoie jamais nullptr.
inline const char *tuile_icone(const char *code, bool actif, const char *type) {
    using namespace tuiles_icones;
    const char *cherche = (code != nullptr && code[0] != '\0') ? code : defaut_du_type(type);
    for (int passe = 0; passe < 2; passe++) {
        for (const auto &ic : kPalette) {
            if (std::strcmp(ic.code, cherche) == 0) return actif ? ic.allume : ic.eteint;
        }
        cherche = defaut_du_type(type);
    }
    return kPalette[0].eteint;
}
