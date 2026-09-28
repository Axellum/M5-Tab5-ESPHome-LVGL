/**
 * [AI-CONTEXT]
 * @file tab5_tuiles_icones.h
 * @role Palette des icônes des tuiles de pièce (ADR-0023) : code de palette → glyphe MDI,
 *       variante éteinte / allumée, icône par défaut de chaque type de tuile.
 * @architecture_constraint GÉNÉRÉ par tools/gen_tuiles_icones.py depuis
 *       Tab5/tuiles_icones.yaml : NE PAS MODIFIER À LA MAIN (--check échoue en CI).
 *       API figée : `tuile_icone(code, actif, type)`, `tuiles_icones::kPalette`,
 *       `tuiles_icones::defaut_du_type(type)`.
 *       Chaque glyphe est dans mdi_font_70, mdi_font_45 et mdi_font_32 (partie
 *       « tuiles » de tab5-styles.yaml, même générateur) ; la règle 7 le vérifie via
 *       MDI_CODE_TARGETS (tools/check_tab5_code_rules.py). La table doit rester la
 *       première chose du fichier qui porte des glyphes : un glyphe écrit dans une
 *       fonction serait rattaché à cette fonction par la règle 7.
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

// 51 codes, 80 glyphes distincts.
inline constexpr TuileIcone kPalette[] = {
    {"ampoule",        "\U000F0335", "\U000F06E8"},  // lightbulb / lightbulb-on
    {"plafonnier",     "\U000F0769", "\U000F0769"},  // ceiling-light
    {"lampadaire",     "\U000F08DD", "\U000F08DD"},  // floor-lamp
    {"lampe",          "\U000F06B5", "\U000F06B5"},  // lamp
    {"lampe_bureau",   "\U000F095F", "\U000F1B20"},  // desk-lamp / desk-lamp-on
    {"led",            "\U000F1051", "\U000F1051"},  // led-strip-variant
    {"guirlande",      "\U000F12BB", "\U000F12BA"},  // string-lights-off / string-lights
    {"applique",       "\U000F091C", "\U000F091C"},  // wall-sconce
    {"lustre",         "\U000F1793", "\U000F1793"},  // chandelier
    {"lit",            "\U000F02E3", "\U000F02E3"},  // bed
    {"canape",         "\U000F04B9", "\U000F04B9"},  // sofa
    {"prise",          "\U000F06A6", "\U000F06A5"},  // power-plug-off / power-plug
    {"interrupteur",   "\U000F1A26", "\U000F1A25"},  // toggle-switch-variant-off / toggle-switch-variant
    {"ordinateur",     "\U000F0379", "\U000F0379"},  // monitor
    {"tv",             "\U000F083B", "\U000F0502"},  // television-off / television
    {"enceinte",       "\U000F04C4", "\U000F04C3"},  // speaker-off / speaker
    {"console",        "\U000F02B5", "\U000F02B4"},  // controller-off / controller
    {"cafetiere",      "\U000F109F", "\U000F109F"},  // coffee-maker
    {"lave_linge",     "\U000F11BD", "\U000F072A"},  // washing-machine-off / washing-machine
    {"lave_vaisselle", "\U000F11B9", "\U000F0AAC"},  // dishwasher-off / dishwasher
    {"aspirateur",     "\U000F1C01", "\U000F070D"},  // robot-vacuum-off / robot-vacuum
    {"ventilateur",    "\U000F081D", "\U000F0210"},  // fan-off / fan
    {"clim",           "\U000F001B", "\U000F001B"},  // air-conditioner
    {"radiateur",      "\U000F0AD8", "\U000F0438"},  // radiator-off / radiator
    {"chauffe_eau",    "\U000F11B4", "\U000F0F92"},  // water-boiler-off / water-boiler
    {"pompe",          "\U000F1B22", "\U000F1402"},  // pump-off / pump
    {"arrosage",       "\U000F1060", "\U000F1060"},  // sprinkler-variant
    {"vanne",          "\U000F1067", "\U000F1068"},  // valve-closed / valve-open
    {"humidificateur", "\U000F1466", "\U000F1099"},  // air-humidifier-off / air-humidifier
    {"volet",          "\U000F111C", "\U000F111E"},  // window-shutter / window-shutter-open
    {"rideau",         "\U000F1847", "\U000F1846"},  // curtains-closed / curtains
    {"store",          "\U000F00AC", "\U000F1011"},  // blinds / blinds-open
    {"garage",         "\U000F06D9", "\U000F06DA"},  // garage / garage-open
    {"portail",        "\U000F0299", "\U000F116A"},  // gate / gate-open
    {"porte",          "\U000F081B", "\U000F081C"},  // door-closed / door-open
    {"fenetre",        "\U000F05AE", "\U000F05B1"},  // window-closed / window-open
    {"serrure",        "\U000F033E", "\U000F033F"},  // lock / lock-open
    {"thermometre",    "\U000F050F", "\U000F050F"},  // thermometer
    {"humidite",       "\U000F058E", "\U000F058E"},  // water-percent
    {"batterie",       "\U000F0079", "\U000F0079"},  // battery
    {"mouvement",      "\U000F1435", "\U000F0D91"},  // motion-sensor-off / motion-sensor
    {"presence",       "\U000F06A1", "\U000F02DC"},  // home-outline / home
    {"energie",        "\U000F0241", "\U000F0241"},  // flash
    {"plante",         "\U000F024A", "\U000F024A"},  // flower
    {"co2",            "\U000F07E4", "\U000F07E4"},  // molecule-co2
    {"fumee",          "\U000F0392", "\U000F192E"},  // smoke-detector / smoke-detector-alert
    {"mesure",         "\U000F029A", "\U000F029A"},  // gauge
    {"etat",           "\U000F0130", "\U000F0133"},  // checkbox-blank-circle-outline / checkbox-marked-circle
    {"scene",          "\U000F03D8", "\U000F03D8"},  // palette
    {"script",         "\U000F0BC2", "\U000F0BC2"},  // script-text
    {"bouton",         "\U000F12A8", "\U000F12A8"},  // gesture-tap-button
};

struct TuileDefaut {
    const char *type;  // type de tuile (ADR-0023)
    const char *code;  // code de palette montré quand HA n'en envoie pas
};

inline constexpr TuileDefaut kDefautsParType[] = {
    {"lum", "ampoule"},
    {"int", "interrupteur"},
    {"vol", "volet"},
    {"med", "tv"},
    {"act", "bouton"},
    {"cap", "mesure"},
    {"bin", "etat"},
    {"cli", "clim"},
};

// Code montré quand ni le code reçu ni le type de la tuile ne sont connus.
inline constexpr const char *kRepli = "etat";

inline const TuileIcone *trouver(const char *code) {
    if (code == nullptr || code[0] == '\0') return nullptr;
    for (const auto &ic : kPalette) {
        if (std::strcmp(ic.code, code) == 0) return &ic;
    }
    return nullptr;
}

// Icône par défaut d'un type de tuile (lum, int, vol, med, act, cap, bin, cli) ; kRepli sinon.
inline const char *defaut_du_type(const char *type) {
    if (type != nullptr) {
        for (const auto &d : kDefautsParType) {
            if (std::strcmp(d.type, type) == 0) return d.code;
        }
    }
    return kRepli;
}

}  // namespace tuiles_icones

// Glyphe à afficher pour une tuile : le code de palette reçu de HA, sinon le défaut de son
// type ; `actif` choisit la variante allumée. Ne renvoie jamais nullptr.
inline const char *tuile_icone(const char *code, bool actif, const char *type) {
    using namespace tuiles_icones;
    const TuileIcone *ic = trouver(code);
    if (ic == nullptr) ic = trouver(defaut_du_type(type));
    if (ic == nullptr) ic = &kPalette[0];
    return actif ? ic->allume : ic->eteint;
}
