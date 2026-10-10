/**
 * [AI-CONTEXT]
 * @file tab5_tuiles.cpp
 * @role Pièces et tuiles génériques (ADR-0023, 28/09/2026) : chaque page du bas est une
 *       pièce de cinq appareils au plus, décrits par Home Assistant. Ce fichier tient le
 *       modèle (définitions + états), le garde en NVS et le dessine.
 *         - Définitions : action tab5_maj_tuiles (tab5-api-logic.yaml), instantané complet
 *           « pR|nom;tRT|type|icône|options|complément|nom;… ». Gardées en NVS (magie
 *           « TUI1 ») si elles changent, chargées là où les zones le sont (zones_apply_ui,
 *           tab5_zones.cpp) : l'écran dessine les pièces avant que HA réponde.
 *         - États : clés « tRT|état|valeur|couleur » de tab5_maj_emplacements, routées par
 *           emplacements_appliquer() (tab5_zones.cpp) avant la table des emplacements 3.x.
 *           Pas gardés : « -- » grisé jusqu'à la première poussée.
 *         - Mode héritage : tant qu'aucune définition n'est arrivée (drapeau en NVS), la
 *           pièce 0 est construite depuis les emplacements 3.x (PC/TV, volet, trois
 *           lumières), avec leurs noms, icônes, comportements et commandes 3.x.
 *         - Gestes : ce que fait un appui, court ou long, sur chaque type (table kGestes,
 *           gestes(), ouvrir_fenetre(), tuile_appui_piece).
 *         - Popups des tuiles (lumière, volet, appareil) : tab5_tuiles_popups.cpp ; roue
 *           d'actions rapides d'une tuile : tab5_tuiles_roue.cpp (découpe du 08/10/2026,
 *           lot L7). Ce que les trois fichiers partagent (types du modèle, état, fonctions) :
 *           tab5_tuiles_priv.h, namespace `tuiles`. Ce fichier les prévient : un état
 *           (peindre_tuile → popup_*_etat, roue_tuile_etat), de nouvelles définitions
 *           (tuiles_definir → popups_revalider), un thème (popups_rejouer_theme).
 *         - Rangée sous l'horloge (ADR-0031, 06/10/2026) : trois lignes de quatre
 *           éléments au plus, mêmes types que les tuiles, dans la même action
 *           (« hLI|type|icône|options|complément|nom|classe », « hp|place des plantes »)
 *           et les mêmes états (clés hLI). Modèle et NVS ici (clé à part, magie « RAN1 »),
 *           dessin et rotation dans tab5_rangee.cpp (rangee_element, plus bas).
 *         - Panneau « Ok Nabu » (09/10/2026, lot 3) : le même modèle, une seconde fois
 *           (RANGEE_NABU) — clés « nLI », « np » (place de la ligne d'écoute), « nd », sa
 *           propre préférence (magie « NAB1 »). Une seule traduction et un seul dessin pour
 *           les deux zones (règle 5).
 *         - Popup Maison (ADR-0037, 07/10/2026) : toutes les pièces à la fois, une ligne
 *           par tuile. Disposé et peint par tab5_maison.cpp, qui ne lit le modèle que par
 *           les fonctions de la fin de ce fichier (tuile_gestes, tuile_peindre_ligne :
 *           même dessin que les cartes du mode HA, peindre_vue_sur ; tuile_appui_maison :
 *           le geste de la tuile). peindre_tuile et tuiles_appliquer_ui le préviennent.
 * @architecture_constraint Pièce R ↔ page du bas dans l'ordre où un swipe les atteint depuis
 *       l'accueil : R0 = page 2 (accueil), R1 = 3, R2 = 4, R3 = 1, R4 = 0. Tuile T = position
 *       visuelle, 0 = gauche (sur les pages horaires, l'objet h(4−T)).
 * @ai_instruction Interrupteur « Tab5 Appareils sur la météo » (05/10/2026) : éteint, le
 *       mode météo ne montre aucun appareil (s_appareils_meteo, lu par peindre_epaules,
 *       tuile_appui et tuile_titre_appui) ; le mode HA ne le lit pas.
 *       Les types, options, clés et commandes sont un contrat avec le blueprint :
 *       tests/test_tuiles_firmware.py les compare aux tableaux de l'ADR-0023. Un texte
 *       affiché passe par tr() ; un nom venu de HA s'affiche tel quel, filtré aux glyphes
 *       des polices (Latin-1 + cp1252 + lettres turques, table kHorsLatin1).
 */
#include "tab5_tuiles_priv.h"
#include "tab5_geometrie.h"
#include "tab5_modele_ha.h"
#include "tab5_tuiles_icones.h"
#include "lvgl.h"
#include <esp_attr.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>

// kNom, kIcone, kEtat, est(), etat_indisponible(), copier_icone() : communs avec la tuile
// − / + (tab5_modele_ha.h, lot L5) ; tuile_cle(), piece_cle() : les clés « tRT » et « pR ».
using namespace modele_ha;
using namespace tuiles;

namespace tuiles {

// kPieces et kTuiles : tab5_geometrie.h. Pièce de chaque page du bas (index = g_central_ctx.forecast_page) : R0 = page 2 (accueil),
// R1 = 3, R2 = 4, R3 = 1, R4 = 0 — l'ordre où un swipe les atteint depuis l'accueil.
constexpr int kPieceDePage[kPieces] = {4, 3, 0, 1, 2};

// Types de tuile (ADR-0023) : Type, les options OPT_*, Def, Modele, Etat, Heritage, Vue,
// Minuterie, Gestes et ClimCible sont dans tab5_tuiles_priv.h (partagés avec les popups et
// la roue). Noms des types, dans l'ordre de Type :
constexpr const char* kTypes[] = {"", "lum", "int", "vol", "med", "act", "cap", "bin", "cli"};
constexpr int kNbTypes = sizeof(kTypes) / sizeof(kTypes[0]);

// Sens d'un volet à l'arrêt (28/09/2026, retour de la 3.1 demandé par Axel) : un toucher
// sur le titre de la tuile le bascule, la flèche le montre, l'appui suivant le suit.
constexpr uint8_t SENS_INCONNU = 0;
constexpr uint8_t SENS_OUVRIR = 1;
constexpr uint8_t SENS_FERMER = 2;

constexpr uint32_t kMagic = 0x54554931;    // « TUI1 »
constexpr uint32_t kPrefKey = 0x7475696C;  // « tuil »

// Rangée sous l'horloge (ADR-0031) : trois lignes de quatre éléments, décrits comme des
// tuiles, plus la classe d'appareil d'un capteur (sa couleur : température, humidité,
// batterie, puissance). Dans sa propre préférence : le modèle des pièces, et donc ce qui
// est gardé en NVS depuis la 3.2, ne change pas de taille.
constexpr int kLignes = 3;
constexpr int kElements = 4;
constexpr size_t kClasse = 16;  // classe d'appareil (device_class) : [a-z0-9_]{1,15}
static_assert(kClasse == kIcone, "copier_icone sert aussi aux classes d'appareil");

struct DefRangee {
    Def d;
    char classe[kClasse];
};

// Exactement ce qui part en NVS (octets seulement, sans bourrage : memcmp fiable). Le
// même pour les deux zones (RangeeZone) : la rangée sous l'horloge et le panneau Ok Nabu.
struct ModeleRangee {
    uint32_t magic;
    int8_t plantes;       // place de la ligne spéciale : 0 à 2, -1 masquée (les plantes
                          // sous l'horloge, l'écoute « Ok Nabu » dans son panneau)
    uint8_t tours;        // une ligne dure `tours` tours de la carte centrale (1 à 15)
    uint8_t reserve[2];
    DefRangee el[kLignes][kElements];
};

// Par zone (ordre de RangeeZone) : la lettre de ses clés, sa magie et sa clé de NVS. La
// rangée garde celles de l'ADR-0031 (son enregistrement d'avant reste lu).
constexpr char kLettreZone[RANGEE_NB] = {'h', 'n'};
constexpr uint32_t kMagicRangee[RANGEE_NB] = {0x52414E31, 0x4E414231};    // « RAN1 », « NAB1 »
constexpr uint32_t kPrefKeyRangee[RANGEE_NB] = {0x72616E67, 0x6E616275};  // « rang », « nabu »
// La rangée est calée sur le rotateur de la carte centrale (demande d'Axel du 06/10/2026) :
// un tour = sa période, kTourCentralS (tab5_internal.h). Le blueprint envoie la durée
// d'une ligne en secondes (« hd|32 », « nd|32 ») ; sans elle, 4 tours (32 s).
constexpr uint8_t kToursDefaut = 4;
constexpr uint8_t kToursMax = 15;

// ~2,3 Ko lus au dessin et aux poussées seulement : en PSRAM (BSS externe, remise à zéro
// au démarrage, CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY), pas dans les ~226 Ko de
// RAM interne libre. La rangée et le panneau Ok Nabu (~1,2 Ko chacun avec leurs états) aussi.
EXT_RAM_BSS_ATTR Modele s_m;
EXT_RAM_BSS_ATTR Etat s_etats[kPieces][kTuiles];
EXT_RAM_BSS_ATTR ModeleRangee s_rg[RANGEE_NB];
EXT_RAM_BSS_ATTR Etat s_etats_rg[RANGEE_NB][kLignes][kElements];
bool s_charge = false;
esphome::ESPPreferenceObject s_pref;
esphome::ESPPreferenceObject s_pref_rg[RANGEE_NB];

// Une zone à lignes vide (rien reçu, ou un blueprint d'avant elle) : sa ligne spéciale
// seule, en première place, 4 tours — les plantes sous l'horloge (l'écran d'avant
// l'ADR-0031), « Ok Nabu: ON / OFF » dans le panneau (l'écran d'avant le lot 3).
void rangee_vide(ModeleRangee& g, int z) {
    g = ModeleRangee{};
    g.magic = kMagicRangee[z];
    g.tours = kToursDefaut;
}

// Zone d'une lettre de clé (« h », « n »), -1 sinon.
int zone_de_lettre(char c) {
    for (int z = 0; z < RANGEE_NB; z++)
        if (kLettreZone[z] == c) return z;
    return -1;
}
// Interrupteur « Tab5 Appareils sur la météo » (tab5-ha-controls.yaml, 05/10/2026,
// discussion #278) : éteint, le mode météo montre les prévisions seules — ni épaules, ni
// bouton d'action, ni bascule du sens d'un volet par le titre. Le mode HA ne change pas.
// Allumé par défaut (comportement de l'ADR-0023) ; l'interrupteur le garde en mémoire.
bool s_appareils_meteo = true;

void etat_vider(Etat& e) {
    e = Etat{};
    e.valeur = NAN;
}

void charger() {
    if (s_charge) return;
    s_charge = true;
    for (auto& piece : s_etats)
        for (Etat& e : piece) etat_vider(e);
    s_pref = esphome::global_preferences->make_preference<Modele>(kPrefKey);
    Modele m{};
    if (s_pref.load(&m) && m.magic == kMagic) {
        s_m = m;
        ESP_LOGI("tab5.tuiles", "Définitions des pièces relues de la NVS");
    } else {
        s_m = Modele{};
        s_m.magic = kMagic;
    }
    for (auto& zone : s_etats_rg)
        for (auto& ligne : zone)
            for (Etat& e : ligne) etat_vider(e);
    for (int z = 0; z < RANGEE_NB; z++) {
        s_pref_rg[z] = esphome::global_preferences->make_preference<ModeleRangee>(kPrefKeyRangee[z]);
        ModeleRangee g{};
        if (s_pref_rg[z].load(&g) && g.magic == kMagicRangee[z]) {
            s_rg[z] = g;
            ESP_LOGI("tab5.tuiles", "%s relue de la NVS", z == RANGEE_NABU ? "Panneau Ok Nabu" : "Rangée sous l'horloge");
        } else {
            rangee_vide(s_rg[z], z);  // rien reçu : la ligne spéciale seule
        }
    }
}

// ─── Texte venu de HA : glyphes des polices, 24 octets au plus ───────────────────────

// Hors Latin-1, les caractères que les polices &latin1 dessinent (tab5-styles.yaml) :
// la ponctuation de Windows-1252, İ ō ř (questions de Trial Poursuite) et Ğ ğ ı Ş ş
// (écran en turc, 02/10/2026).
// tests/test_tuiles_firmware.py compare cette table à la liste des glyphes.
constexpr uint16_t kHorsLatin1[] = {
    0x20AC, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
    0x2039, 0x0152, 0x017D, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x017E, 0x0178, 0x0130, 0x014D, 0x0159,
    0x011E, 0x011F, 0x0131, 0x015E, 0x015F,
};

bool glyphe_disponible(uint32_t cp) {
    if (cp >= 0x20 && cp <= 0x7E) return true;
    if (cp >= 0xA1 && cp <= 0xFF) return cp != 0xAD;  // ni espace insécable ni trait conditionnel
    for (uint16_t c : kHorsLatin1)
        if (c == cp) return true;
    return false;
}

// Point de code UTF-8 suivant (U+FFFD si la séquence est invalide) ; avance `p`.
uint32_t utf8_suivant(const char*& p, const char* fin) {
    const unsigned char c = static_cast<unsigned char>(*p++);
    if (c < 0x80) return c;
    int n = 0;
    uint32_t cp = 0;
    if ((c & 0xE0) == 0xC0) { n = 1; cp = c & 0x1F; }
    else if ((c & 0xF0) == 0xE0) { n = 2; cp = c & 0x0F; }
    else if ((c & 0xF8) == 0xF0) { n = 3; cp = c & 0x07; }
    else return 0xFFFD;
    for (int i = 0; i < n; i++) {
        if (p >= fin || (static_cast<unsigned char>(*p) & 0xC0) != 0x80) return 0xFFFD;
        cp = (cp << 6) | (static_cast<unsigned char>(*p++) & 0x3F);
    }
    return cp;
}

size_t utf8_ecrire(uint32_t cp, char* out) {
    if (cp < 0x80) { out[0] = static_cast<char>(cp); return 1; }
    if (cp < 0x800) {
        out[0] = static_cast<char>(0xC0 | (cp >> 6));
        out[1] = static_cast<char>(0x80 | (cp & 0x3F));
        return 2;
    }
    out[0] = static_cast<char>(0xE0 | (cp >> 12));
    out[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out[2] = static_cast<char>(0x80 | (cp & 0x3F));
    return 3;
}

// Copie un texte de HA dans `dst` (capacité `cap`, zéro final compris) : UTF-8 valide
// (Latin-1 et mojibake réparés par normalize_text_utf8), sans les caractères que les
// polices n'ont pas, coupé sur une frontière de caractère. L'espace insécable devient
// une espace.
void copier_texte(char* dst, size_t cap, const char* src, size_t n) {
    std::memset(dst, 0, cap);
    const std::string propre = normalize_text_utf8(std::string(src, n));
    const char* p = propre.data();
    const char* fin = p + propre.size();
    size_t k = 0;
    while (p < fin) {
        uint32_t cp = utf8_suivant(p, fin);
        if (cp == 0xA0) cp = ' ';
        if (!glyphe_disponible(cp)) continue;
        char tmp[4];
        const size_t w = utf8_ecrire(cp, tmp);
        if (k + w >= cap) break;
        std::memcpy(dst + k, tmp, w);
        k += w;
    }
}

uint8_t lire_options(const char* s, size_t n) {
    uint8_t o = 0;
    for (size_t i = 0; i < n; i++) {
        const char* l = std::strchr(kLettresOptions, s[i]);
        if (l != nullptr && s[i] != '\0') o |= static_cast<uint8_t>(1u << (l - kLettresOptions));
    }
    return o;
}

Type lire_type(const char* s, size_t n) {
    for (int i = 1; i < kNbTypes; i++)
        if (std::strlen(kTypes[i]) == n && std::strncmp(kTypes[i], s, n) == 0) return static_cast<Type>(i);
    return Type::VIDE;  // type inconnu (blueprint plus récent) : tuile vide
}

bool chiffre_0_4(char c, int& v) {
    if (c < '0' || c > '4') return false;
    v = c - '0';
    return true;
}

// Champs d'une entrée séparés par '|' (au plus `max`, le reste ignoré) : Champ et
// champs_decouper() de tab5_champs.h.
int decouper(const char* s, size_t n, Champ* out, int max) { return champs_decouper(s, n, '|', out, max); }

// « type|icône|options|complément|nom » (champs 1 à 5 d'une entrée tRT ou hLI) dans `d`.
void lire_def(const Champ* f, int nf, Def& d) {
    d.type = static_cast<uint8_t>(lire_type(f[1].p, f[1].n));
    if (nf > 2) copier_icone(d.icone, f[2].p, f[2].n);
    if (nf > 3) d.options = lire_options(f[3].p, f[3].n);
    if (nf > 4) copier_texte(d.complement, kComplement, f[4].p, f[4].n);
    if (nf > 5) copier_texte(d.nom, kNom, f[5].p, f[5].n);
    if (d.type == static_cast<uint8_t>(Type::VIDE)) d = Def{};
}

// Une entrée d'une zone à lignes (f[0] = sa clé, lettre de la zone comprise), la même
// grammaire pour la rangée (h) et le panneau Ok Nabu (n) :
//   - « xp|0 » à « xp|2 » : place de la ligne spéciale (plantes, écoute) ; autre chose
//     (« xp|- ») : masquée ;
//   - « xd|secondes » : durée d'une ligne, arrondie au tour de la carte centrale le plus
//     proche (1 à 15 tours) ; illisible : le défaut ;
//   - « xLI|type|icône|options|complément|nom|classe » : élément I de la ligne L.
void lire_entree_rangee(const Champ* f, int nf, ModeleRangee& g) {
    if (f[0].n == 2 && f[0].p[1] == 'p') {
        int place = 0;
        const bool ok = f[1].n == 1 && chiffre_0_4(f[1].p[0], place) && place < kLignes;
        g.plantes = static_cast<int8_t>(ok ? place : -1);
        return;
    }
    if (f[0].n == 2 && f[0].p[1] == 'd') {
        int s = 0;
        bool ok = f[1].n > 0 && f[1].n <= 3;
        for (size_t k = 0; ok && k < f[1].n; k++) {
            ok = f[1].p[k] >= '0' && f[1].p[k] <= '9';
            s = s * 10 + (f[1].p[k] - '0');
        }
        const int tours = (s + kTourCentralS / 2) / kTourCentralS;
        g.tours = ok ? static_cast<uint8_t>(std::max(1, std::min<int>(kToursMax, tours))) : kToursDefaut;
        return;
    }
    if (f[0].n == 3) {
        int l = 0, i = 0;
        if (!chiffre_0_4(f[0].p[1], l) || l >= kLignes || !chiffre_0_4(f[0].p[2], i) || i >= kElements) return;
        DefRangee& e = g.el[l][i];
        lire_def(f, nf, e.d);
        if (nf > 6 && e.d.type != static_cast<uint8_t>(Type::VIDE)) copier_icone(e.classe, f[6].p, f[6].n);
    }
}

// Une entrée de tab5_maj_tuiles dans `m` (pièces) ou `g` (zones à lignes : rangée sous
// l'horloge, clés h…, et panneau Ok Nabu, clés n…, mêmes champs) ; une clé inconnue est
// ignorée.
void lire_entree(const char* s, size_t n, Modele& m, ModeleRangee (&zones)[RANGEE_NB]) {
    Champ f[7];
    const int nf = decouper(s, n, f, 7);
    if (nf < 2) return;
    int r = 0, t = 0;
    if (f[0].n == 2 && f[0].p[0] == 'p' && chiffre_0_4(f[0].p[1], r)) {
        copier_texte(m.pieces[r], kNom, f[1].p, f[1].n);
        return;
    }
    const int z = zone_de_lettre(f[0].p[0]);
    if (z >= 0 && (f[0].n == 2 || f[0].n == 3)) {
        lire_entree_rangee(f, nf, zones[z]);
        return;
    }
    if (f[0].n != 3 || f[0].p[0] != 't' || !chiffre_0_4(f[0].p[1], r) || !chiffre_0_4(f[0].p[2], t)) return;
    lire_def(f, nf, m.tuiles[r][t]);
}

// ─── Mode héritage : les emplacements 3.x forment la pièce 0 ─────────────────────────

Heritage s_h;

bool heritage() { return s_m.recues == 0; }

// Tuile présente : un appareil défini — en mode héritage, un emplacement 3.x que HA n'a
// pas déclaré absent (zones, ADR-0018), dans la pièce 0 seulement.
bool tuile_presente(int r, int t) {
    if (r < 0 || r >= kPieces || t < 0 || t >= kTuiles) return false;
    if (heritage()) return r == 0 && !zone_tuile_absente(t);
    return s_m.tuiles[r][t].type != static_cast<uint8_t>(Type::VIDE);
}

bool piece_non_vide(int r) {
    for (int t = 0; t < kTuiles; t++)
        if (tuile_presente(r, t)) return true;
    return false;
}

bool aucun_appareil() {
    for (int r = 0; r < kPieces; r++)
        if (piece_non_vide(r)) return false;
    return true;
}

int piece_courante() {
    const int p = g_central_ctx.forecast_page;
    return (p >= 0 && p < kPieces) ? kPieceDePage[p] : 0;
}

// Page la plus proche de `page` dont la pièce a des appareils (à égalité, la plus proche
// de l'accueil) ; -1 s'il n'y en a aucune.
int page_non_vide_proche(int page) {
    int choix = -1, d_choix = 99, h_choix = 99;
    for (int p = 0; p < kPieces; p++) {
        if (!piece_non_vide(kPieceDePage[p])) continue;
        const int d = std::abs(p - page), h = std::abs(p - 2);
        if (d < d_choix || (d == d_choix && h < h_choix)) {
            choix = p;
            d_choix = d;
            h_choix = h;
        }
    }
    return choix;
}

// ─── Glyphes posés d'ici (règle 7 : MDI_CODE_TARGETS de check_tab5_code_rules.py) ────

// Épaule droite (32 px) : l'ampoule d'une lumière…
const char* glyphe_ampoule(bool allumee) {
    return allumee ? "\U000F06E8" : "\U000F0335";
}

// … ou la flèche du prochain mouvement d'un volet : 0 arrêter (pause), 1 ouvrir,
// -1 fermer.
const char* glyphe_fleche(int sens) {
    if (sens == 0) return "\U000F03E4";
    return sens > 0 ? "\U000F005D" : "\U000F0045";
}

// Mode héritage : les icônes de la 3.1 sur les épaules gauches de l'accueil (32 px)…
const char* heritage_glyphe_epaule(int t, bool volet_ferme) {
    switch (t) {
        case 0: return "\U000F07C0";   // desktop-classic : la TV (le PC sans TV)
        case 1: return volet_ferme ? "\U000F111C" : "\U000F111E";
        case 2: return "\U000F02E3";   // bed
        case 3: return "\U000F04B9";   // sofa
        default: return "\U000F1051";  // led-strip-variant
    }
}

// … et sur les cartes du mode HA (70 px), les lignes du popup Maison (32 px) et celles du
// popup Lumières (45 px, ADR-0046 : il dessinait avant ses lignes avec ses propres glyphes).
const char* heritage_glyphe_carte(int t) {
    switch (t) {
        case 0: return "\U000F0379";   // monitor
        case 1: return "\U000F111E";   // window-shutter-open
        case 2: return "\U000F02E3";   // bed
        case 3: return "\U000F04B9";   // sofa
        default: return "\U000F1051";  // led-strip-variant
    }
}

// ─── Ce qu'une tuile montre, selon son type et son état (ADR-0023) ────────────────────

// Minuteries d'une tuile : confirmation (option k, 3 s), « OK » après « lancer » (1 s).
Minuterie s_confirmation;
Minuterie s_ok;
Minuterie s_sens;  // sens d'un volet basculé : « Ouvrir » / « Fermer » sur la ligne d'état
constexpr uint32_t kConfirmationMs = 3000;
constexpr uint32_t kOkMs = 1000;
constexpr uint32_t kSensMs = 2000;

bool minuterie_sur(const Minuterie& m, int r, int t) { return m.timer != nullptr && m.r == r && m.t == t; }

void peindre_tuile(int r, int t);

void minuterie_fin(lv_timer_t* timer) {
    Minuterie* m = static_cast<Minuterie*>(lv_timer_get_user_data(timer));
    const int r = m->r, t = m->t;
    m->timer = nullptr;  // un seul tour : LVGL supprime le timer après ce rappel
    m->r = m->t = -1;
    peindre_tuile(r, t);
}

void minuterie_arreter(Minuterie& m) {
    if (m.timer == nullptr) return;
    lv_timer_delete(m.timer);
    const int r = m.r, t = m.t;
    m.timer = nullptr;
    m.r = m.t = -1;
    peindre_tuile(r, t);
}

void minuterie_armer(Minuterie& m, int r, int t, uint32_t ms) {
    minuterie_arreter(m);
    m.r = r;
    m.t = t;
    m.timer = lv_timer_create(minuterie_fin, ms, &m);
    lv_timer_set_repeat_count(m.timer, 1);
    peindre_tuile(r, t);
}

// Couleur propre d'une lumière (rgb_color), éclaircie vers le blanc si elle se perdrait
// sur le fond sombre des cartes (luminance < 110 sur 255) ; sur un thème clair (ADR-0029),
// assombrie vers le noir si elle se perdrait sur le blanc (luminance > 150).
uint32_t couleur_lisible(uint32_t c) {
    const int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
    const int y = (2126 * r + 7152 * g + 722 * b) / 10000;
    if (palette_claire(UIColor)) {
        constexpr int kMax = 150;
        if (y <= kMax) return c;
        const int k = kMax * 256 / y;
        auto vers_noir = [k](int v) { return v * k / 256; };
        return (static_cast<uint32_t>(vers_noir(r)) << 16) | (static_cast<uint32_t>(vers_noir(g)) << 8) |
               static_cast<uint32_t>(vers_noir(b));
    }
    constexpr int kMin = 110;
    if (y >= kMin) return c;
    const int k = (kMin - y) * 256 / (255 - y);
    auto vers_blanc = [k](int v) { return v + ((255 - v) * k) / 256; };
    return (static_cast<uint32_t>(vers_blanc(r)) << 16) | (static_cast<uint32_t>(vers_blanc(g)) << 8) |
           static_cast<uint32_t>(vers_blanc(b));
}

// « 21.4 °C », « 45 % », « 1250 W » : une décimale sous 100 si elle compte.
void formater_mesure(char* out, size_t n, float v, const char* unite) {
    const bool entier = std::fabs(v) >= 100.0f || std::fabs(v - std::round(v)) < 0.05f;
    if (unite[0] != '\0') snprintf(out, n, entier ? "%.0f %s" : "%.1f %s", v, unite);
    else snprintf(out, n, entier ? "%.0f" : "%.1f", v);
}

// Familles de classes d'appareil d'un `bin` : quels mots pour « actif » / « inactif ».
enum class Famille : uint8_t { OUVERTURE, DETECTION, PRESENCE, VERROU, AUTRE };
Famille famille_bin(const char* classe) {
    static constexpr const char* kOuverture[] = {"door", "window", "opening", "garage_door"};
    static constexpr const char* kDetection[] = {"motion", "occupancy", "moving", "vibration", "sound"};
    for (const char* c : kOuverture)
        if (est(classe, c)) return Famille::OUVERTURE;
    for (const char* c : kDetection)
        if (est(classe, c)) return Famille::DETECTION;
    if (est(classe, "presence")) return Famille::PRESENCE;
    if (est(classe, "lock")) return Famille::VERROU;
    return Famille::AUTRE;
}

// Volet / vanne : « ouvert » = état open (même entrouvert), mouvement = opening / closing.
bool vol_mouvement(const char* s) { return est(s, "opening") || est(s, "closing"); }
// Sens à l'arrêt : celui choisi par le titre, sinon d'après l'état (ouvert → fermer).
uint8_t vol_sens(const Etat& e) {
    if (e.sens != SENS_INCONNU) return e.sens;
    return est(e.brut, "open") ? SENS_FERMER : SENS_OUVRIR;
}
// Au bout de sa course, le sens repart dans l'autre : fermé → ouvrir ; tout à fait ouvert
// (position 100, ou volet qui n'en donne pas) → fermer. Arrêté en route (position 1-99, ou
// -1 pour le volet à course simulée, « Partiel ») : le sens choisi reste.
void vol_sens_suivre(Etat& e) {
    if (vol_mouvement(e.brut)) return;
    if (est(e.brut, "closed")) e.sens = SENS_OUVRIR;
    else if (est(e.brut, "open") && (std::isnan(e.valeur) || e.valeur >= 100.0f)) e.sens = SENS_FERMER;
}
// Appui court (ADR-0023, mise à jour du 28/09) : en mouvement → arrêter (pause) ; sinon
// dans le sens choisi.
const char* vol_appui(const Etat& e) {
    if (vol_mouvement(e.brut)) return "arreter";
    return vol_sens(e) == SENS_FERMER ? "fermer" : "ouvrir";
}
// Appui long : l'autre de ouvrir / fermer (en mouvement, le sens contraire).
const char* vol_appui_long(const Etat& e) {
    if (est(e.brut, "opening")) return "fermer";
    if (est(e.brut, "closing")) return "ouvrir";
    return vol_sens(e) == SENS_FERMER ? "ouvrir" : "fermer";
}

// ─── Gestes d'une tuile : une ligne par type (ADR-0023 ; appui long : ADR-0036) ─────
//
// Audit du 07/10/2026 (lot L7, UI-3) : « que fait l'appui long de ce type » était écrit
// dans cinq switch (toucher, popup ouvert d'ailleurs, popup Maison, popup d'un appareil,
// boutons visibles), le bloc de la clim deux fois. La table kGestes le dit une fois ;
// gestes() y applique les options ; ouvrir_fenetre() ouvre la fenêtre choisie.

// Gestes d'un type, avant ses options. `agit` : un appui fait quelque chose (sinon le
// bouton de la tuile météo est masqué) ; un capteur et une clim n'agissent qu'avec leurs
// options (gestes()). `commande` : celle de l'appui court ; nullptr : un volet suit son
// sens (vol_appui), une clim et un capteur ouvrent leur fenêtre. `roue` : l'appui long
// ouvre d'abord la roue d'actions rapides (ADR-0036 ; tuile_roue_ouvrir dit si la tuile en
// a une). `fenetre` : l'appui long sans roue, l'appui court d'une clim ou d'un capteur.
struct GesteType {
    bool agit;
    const char* commande;
    bool roue;
    Fenetre fenetre;
};
constexpr GesteType kGestes[] = {
    {false, nullptr, false, Fenetre::AUCUNE},     // vide
    {true, "basculer", true, Fenetre::LUMIERE},   // lum : popup des lumières de la pièce
    {true, "basculer", false, Fenetre::APPAREIL}, // int : popup de l'appareil (06/10/2026)
    {true, nullptr, true, Fenetre::VOLET},        // vol : popup du volet (05/10/2026)
    {true, "basculer", false, Fenetre::LECTEUR},  // med : lecteur de musique (ADR-0050), t : télécommande
    {true, "lancer", false, Fenetre::APPAREIL},   // act : « OK » 1 s après
    {false, nullptr, false, Fenetre::ENERGIE},    // cap : popup Énergie (option e, ADR-0028)
    {false, nullptr, false, Fenetre::AUCUNE},     // bin : lecture seule
    {false, nullptr, true, Fenetre::CLIM},        // cli : popup de la clim (ADR-0026, ADR-0027)
};
static_assert(sizeof(kGestes) / sizeof(kGestes[0]) == kNbTypes, "un geste par type de kTypes");

// Options (lettres de l'ADR-0023) :
//   r  lecture seule : aucun geste ;
//   o  « allumer » au lieu de « basculer » (jamais éteinte depuis l'écran) ;
//   k  commande confirmée par un second appui (s_confirmation) : pas de roue, dont les
//      boutons passeraient outre ; un volet garde son ancien appui long (l'autre sens,
//      confirmé), sans popup. Sans effet sur une clim ;
//   t  med : la télécommande de la TV du blueprint au lieu du popup de l'appareil ;
//   m  cli : la clim du blueprint ; sans m, celle de la tuile quand la tablette en a les
//      réglages (clé crRT, ADR-0027 : `clim_connue`) ;
//   e  cap : un capteur de la section « Énergie » du blueprint, qui ouvre son popup.
Gestes gestes(const Def& d, bool clim_connue) {
    Gestes g;
    if (d.type >= kNbTypes || (d.options & OPT_R)) return g;
    const Type type = static_cast<Type>(d.type);
    const GesteType& l = kGestes[d.type];
    g.agit = l.agit || (type == Type::CAP && (d.options & OPT_E)) ||
             (type == Type::CLI && ((d.options & OPT_M) || clim_connue));
    g.commande = (l.commande != nullptr && type != Type::ACT && (d.options & OPT_O)) ? "allumer" : l.commande;
    const bool confirme = (d.options & OPT_K) && type != Type::CLI;
    g.roue = l.roue && !confirme;
    g.fenetre = l.fenetre;
    if (type == Type::MED && (d.options & OPT_T)) g.fenetre = Fenetre::TELECOMMANDE;
    if (type == Type::VOL && confirme) g.fenetre = Fenetre::AUCUNE;
    return g;
}

// La clim d'une tuile cli : celle du blueprint avec l'option m (r = t = -1 pour
// tab5_clim.cpp, emplacement « clim »), sinon la sienne (tRT, ADR-0027). Lue par la
// fenêtre CLIM et par la roue (capacités, consignes, bascules, jauge, commandes).

ClimCible clim_cible(const Def& d, int r, int t) {
    ClimCible c;
    if (d.options & OPT_M) return c;
    c.r = r;
    c.t = t;
    c.cle = tuile_cle(r, t);
    return c;
}

// Épaule droite d'un volet : la flèche de ce qu'un appui ferait (pause en mouvement,
// sinon le sens choisi), comme la 3.1.
void vue_fleche_volet(const Etat& e, Vue& v) {
    if (vol_mouvement(e.brut)) {
        v.droite = glyphe_fleche(0);
        v.couleur_droite = UIColor.INFO;
    } else {
        v.droite = glyphe_fleche(vol_sens(e) == SENS_FERMER ? -1 : 1);
        v.couleur_droite = UIColor.TEXT_DIM;
    }
}

// Ce que montre l'appareil `d` dans l'état `e` (tuile tRT ; élément de la rangée sous
// l'horloge avec r = -1 : aucune minuterie, aucune clim de tuile ne le vise).
void vue_def(const Def& d, const Etat& e, int r, int t, Vue& v) {
    const Type type = static_cast<Type>(d.type);
    const char* s = e.brut;
    bool actif = false;
    uint32_t c = UIColor.TEXT_DIM;
    v.nom = d.nom;
    v.agit = gestes(d, type == Type::CLI && r >= 0 && clim_tuile_connue(r, t)).agit;
    switch (type) {
        case Type::LUM:
            actif = est(s, "on");
            if (actif && (d.options & OPT_D) && lum_pct(e.valeur) >= 0) {
                snprintf(v.ligne, sizeof(v.ligne), "%d %%", lum_pct(e.valeur));
            } else {
                snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? tr("Allumé") : tr("Éteint"));
            }
            c = actif ? (e.a_couleur ? couleur_lisible(e.couleur) : UIColor.INFO) : UIColor.TEXT_DIM;
            v.droite = glyphe_ampoule(actif);
            v.couleur_droite = c;
            break;
        case Type::INT:
            actif = est(s, "on");
            snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? tr("Allumé") : tr("Éteint"));
            c = actif ? UIColor.SUCCESS : UIColor.TEXT_DIM;
            break;
        case Type::VOL:
            actif = !est(s, "closed");
            if (vol_mouvement(s)) {
                snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Mouvement"));
                c = UIColor.INFO;
            } else if (est(s, "open")) {
                if (!std::isnan(e.valeur) && e.valeur > 0.0f && e.valeur < 100.0f)
                    snprintf(v.ligne, sizeof(v.ligne), "%.0f %%", e.valeur);
                else
                    snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Ouvert"));
                c = UIColor.SUCCESS;
            } else {
                snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Fermé"));
            }
            vue_fleche_volet(e, v);
            break;
        case Type::MED:
            actif = !est(s, "off") && !est(s, "standby");
            if (est(s, "playing")) snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Lecture"));
            else if (est(s, "paused")) snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Pause"));
            else snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? tr("Allumé") : tr("Éteint"));
            c = actif ? UIColor.SUCCESS : UIColor.TEXT_DIM;
            break;
        case Type::ACT:
            actif = minuterie_sur(s_ok, r, t);
            snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? "OK" : tr("Lancer"));
            c = actif ? UIColor.SUCCESS : UIColor.ACCENT;
            break;
        case Type::CAP:
            actif = true;
            // Option e : W / kW, kWh / MWh en unités courtes (« 3.45 kW »), comme le popup.
            if (std::isnan(e.valeur)) snprintf(v.ligne, sizeof(v.ligne), "%s", s);
            else if (!((d.options & OPT_E) && energie_formater(v.ligne, sizeof(v.ligne), e.valeur, d.complement)))
                formater_mesure(v.ligne, sizeof(v.ligne), e.valeur, d.complement);
            c = (d.complement[0] != '\0' && std::strncmp(d.complement, "\xC2\xB0", 2) == 0 && !std::isnan(e.valeur))
                    ? get_temperature_color(e.valeur) : UIColor.INFO;
            break;
        case Type::BIN: {
            const Famille f = famille_bin(d.complement);
            if (f == Famille::VERROU) {
                // binary_sensor « lock » : on = déverrouillé ; entité lock : locked / unlocked.
                const bool verrouille = est(s, "locked") || est(s, "off");
                snprintf(v.ligne, sizeof(v.ligne), "%s", verrouille ? tr("Verrouillé") : tr("Ouvert"));
                actif = !verrouille;
                c = verrouille ? UIColor.SUCCESS : UIColor.WARNING;
                break;
            }
            actif = est(s, "on") || est(s, "home");
            switch (f) {
                case Famille::OUVERTURE:
                    snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? tr("Ouvert") : tr("Fermé"));
                    break;
                case Famille::DETECTION:
                    snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? tr("Détecté") : tr("Rien"));
                    break;
                case Famille::PRESENCE:
                    snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? tr("Présent") : tr("Absent"));
                    break;
                default:
                    snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? tr("Actif") : tr("Inactif"));
                    break;
            }
            c = actif ? (f == Famille::PRESENCE ? UIColor.SUCCESS : UIColor.WARNING) : UIColor.TEXT_DIM;
            break;
        }
        case Type::CLI:
            actif = !est(s, "off");
            if (!std::isnan(e.valeur)) snprintf(v.ligne, sizeof(v.ligne), "%.1f \xC2\xB0", e.valeur);
            else snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? "--" : tr("Éteint"));
            if (est(s, "cool")) c = UIColor.CLIM_COOL_ACTIVE;
            else if (est(s, "heat")) c = UIColor.CLIM_HEAT_ACTIVE;
            else c = actif ? UIColor.SUCCESS : UIColor.TEXT_DIM;
            break;
        default:
            break;
    }
    uint32_t c_ligne = c;
    // Pas encore d'état depuis le démarrage (les états ne sont pas gardés) : « -- » grisé.
    // unavailable / unknown : « Hors ligne », grisé (un script ou une scène n'en a pas).
    if (!e.recu) {
        actif = false;
        c = c_ligne = UIColor.INACTIVE;
        v.droite = nullptr;
        snprintf(v.ligne, sizeof(v.ligne), "--");
    } else if (est(s, "unavailable") || (est(s, "unknown") && type != Type::ACT)) {
        actif = false;
        c = c_ligne = UIColor.INACTIVE;
        v.droite = nullptr;
        snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Hors ligne"));
    } else if (type == Type::CLI && !std::isnan(e.valeur)) {
        c_ligne = get_temperature_color(e.valeur);
    }
    // Sens d'un volet basculé par le titre : la ligne d'état le dit 2 s (en mode HA, la
    // carte n'a pas de flèche).
    if (type == Type::VOL && e.recu && !vol_mouvement(s) && minuterie_sur(s_sens, r, t)) {
        c_ligne = UIColor.ACCENT;
        snprintf(v.ligne, sizeof(v.ligne), "%s", vol_sens(e) == SENS_FERMER ? tr("Fermer") : tr("Ouvrir"));
    }
    // Option k : le premier appui arme, la ligne d'état demande le second (3 s).
    if (minuterie_sur(s_confirmation, r, t)) {
        c = c_ligne = UIColor.WARNING;
        snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Confirmer ?"));
    }
    const char* icone = tuile_icone(d.icone, actif, kTypes[d.type]);
    v.icone = v.icone_carte = icone;
    v.couleur = v.couleur_carte = c;
    v.couleur_ligne = c_ligne;
    v.actif = actif;
}

void vue_nouvelle(int r, int t, Vue& v) { vue_def(s_m.tuiles[r][t], s_etats[r][t], r, t, v); }

// Mode héritage : ce que la 3.1 montrait, à l'identique (ou presque : « -- » avant la
// première donnée, comme les tuiles).
void vue_heritage(int t, Vue& v) {
    v.agit = true;
    switch (t) {
        case 0: {
            v.nom = tr("PC Bureau");
            // Épaule : l'état de la TV, ou celui du PC quand il n'y a pas de TV (lot 5).
            const bool epaule = (zone_absente(Zone::TV) && !zone_absente(Zone::PC)) ? s_h.pc : s_h.tv;
            v.icone = heritage_glyphe_epaule(0, false);
            v.couleur = epaule ? UIColor.SUCCESS : UIColor.TEXT_DIM;
            v.icone_carte = heritage_glyphe_carte(0);
            const uint32_t c = s_h.pc ? UIColor.SUCCESS : UIColor.TEXT_DIM;
            v.couleur_carte = s_h.pc_recu ? c : UIColor.INACTIVE;
            if (s_h.pc_recu) snprintf(v.ligne, sizeof(v.ligne), "%s", s_h.pc ? tr("Allumé") : tr("Éteint"));
            else snprintf(v.ligne, sizeof(v.ligne), "--");
            v.couleur_ligne = v.couleur_carte;
            break;
        }
        case 1: {
            v.nom = tr("Volet");
            v.icone_carte = heritage_glyphe_carte(1);
            const int8_t o = s_h.volet_ouvert;
            const bool mouvement = est(s_h.volet, "En_mouvement");
            // Épaule gauche : dernier état connu hors mouvement (la 3.1 la laissait telle
            // quelle pendant la course) ; droite : pause en mouvement, sinon le sens de la
            // prochaine commande (volet_target_open, basculé par btn_j1_dir).
            v.icone = heritage_glyphe_epaule(1, o == 0);
            v.couleur = o == 1 ? UIColor.SUCCESS : (o == 0 ? UIColor.ERROR : UIColor.TEXT_DIM);
            const bool ouvrir = g_tuiles_ui.volet_sens == nullptr || *g_tuiles_ui.volet_sens;
            v.droite = glyphe_fleche(mouvement ? 0 : (ouvrir ? 1 : -1));
            v.couleur_droite = mouvement ? UIColor.INFO : UIColor.TEXT_DIM;
            if (mouvement) {
                snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Mouvement"));
                v.couleur_carte = UIColor.INFO;
            } else if (o >= 0) {
                snprintf(v.ligne, sizeof(v.ligne), "%s", o == 1 ? tr("Ouvert") : tr("Fermé"));
                v.couleur_carte = o == 1 ? UIColor.SUCCESS : UIColor.TEXT_DIM;
            } else {
                snprintf(v.ligne, sizeof(v.ligne), "--");
            }
            v.couleur_ligne = v.couleur_carte;
            break;
        }
        default: {
            const int i = t - 2;
            v.nom = i == 0 ? tr("Chambre") : (i == 1 ? tr("Salon") : "LEDs");
            v.icone_carte = heritage_glyphe_carte(t);
            const uint32_t c = s_h.lum[i] ? UIColor.INFO : UIColor.TEXT_DIM;
            v.icone = heritage_glyphe_epaule(t, false);
            v.couleur = c;
            v.droite = glyphe_ampoule(s_h.lum[i]);
            v.couleur_droite = c;
            v.couleur_carte = s_h.lum_recu[i] ? c : UIColor.INACTIVE;
            if (s_h.lum_recu[i]) snprintf(v.ligne, sizeof(v.ligne), "%s", s_h.lum[i] ? tr("Allumé") : tr("Éteint"));
            else snprintf(v.ligne, sizeof(v.ligne), "--");
            v.couleur_ligne = v.couleur_carte;
            break;
        }
    }
}

void vue(int r, int t, Vue& v) {
    if (heritage()) vue_heritage(t, v);
    else vue_nouvelle(r, t, v);
}

// ─── Dessin ─────────────────────────────────────────────────────────────────────────

// Texte sur une ligne, coupé avec « … » s'il dépasse `largeur` px dans la police du label.
void ui_texte_coupe(lv_obj_t* lbl, const char* txt, int32_t largeur) {
    if (lbl == nullptr || txt == nullptr) return;
    const lv_font_t* f = lv_obj_get_style_text_font(lbl, LV_PART_MAIN);
    if (f == nullptr) {
        ui_text(lbl, txt);
        return;
    }
    const int32_t esp = lv_obj_get_style_text_letter_space(lbl, LV_PART_MAIN);
    const int32_t w_points = lv_font_get_glyph_width(f, 0x2026, 0) + esp;
    const char* fin = txt + std::strlen(txt);
    const char* p = txt;
    const char* coupe = txt;  // dernière coupure où le texte et « … » tiennent encore
    int32_t w = 0;
    while (p < fin) {
        const uint32_t cp = utf8_suivant(p, fin);
        const char* q = p;
        const uint32_t suivant = q < fin ? utf8_suivant(q, fin) : 0;
        w += lv_font_get_glyph_width(f, cp, suivant) + esp;
        if (w + w_points <= largeur) coupe = p;
    }
    if (w <= largeur) {
        ui_text(lbl, txt);
        return;
    }
    char buf[64];
    const size_t n = std::min<size_t>(static_cast<size_t>(coupe - txt), sizeof(buf) - 4);
    std::memcpy(buf, txt, n);
    std::memcpy(buf + n, "\xE2\x80\xA6", 4);  // « … » et le zéro final
    ui_text(lbl, buf);
}

// Fond local d'un objet (pastille, remplissage), écrit seulement s'il change : comme
// ui_text_color (tab5_internal.h), un lv_obj_set_style_* invalide l'objet même à
// valeur égale.
void ui_fond(lv_obj_t* obj, uint32_t hex) {
    if (obj == nullptr) return;
    const lv_color_t voulu = lv_color_hex(hex);
    lv_style_value_t cur;
    if (lv_obj_get_local_style_prop(obj, LV_STYLE_BG_COLOR, &cur, LV_PART_MAIN) == LV_STYLE_RES_FOUND &&
        lv_color_eq(cur.color, voulu)) {
        return;
    }
    lv_obj_set_style_bg_color(obj, voulu, LV_PART_MAIN);
}

// Nom et état d'une carte du mode HA : la carte fait 230 px, 12 px de marge de chaque
// côté (façon carte « tile » de HA depuis le 06/10/2026 : plus d'onglets de 200 px).
constexpr int32_t kLargeurCarteTexte = 206;

// Une tuile façon carte « tile » de HA (06/10/2026, discussion #278) : l'icône en couleur
// pleine dans une pastille ronde de la même couleur à 20 % (opacité posée par le YAML), le
// nom, l'état dans sa couleur, textes coupés à `largeur` px. Les cartes du mode HA et les
// lignes du popup Maison (ADR-0037, tuile_peindre_ligne) passent par ici : mêmes mots,
// mêmes couleurs.
void peindre_vue_sur(const Vue& v, const TuileWidgets& w, int32_t largeur) {
    if (v.icone_carte != nullptr) ui_text(w.icone, v.icone_carte);
    ui_text_color(w.icone, v.couleur_carte);
    ui_fond(w.pastille, v.couleur_carte);
    ui_texte_coupe(w.nom, v.nom, largeur);
    ui_texte_coupe(w.etat, v.ligne, largeur);
    ui_text_color(w.etat, v.couleur_ligne);
}

// Une carte du mode HA.
void peindre_carte(int r, int t) {
    const TuilesUI& u = g_tuiles_ui;
    if (u.carte_icone[t] == nullptr) return;
    Vue v;
    vue(r, t, v);
    peindre_vue_sur(v, {u.carte_pastille[t], u.carte_icone[t], u.carte_nom[t], u.carte_etat[t]}, kLargeurCarteTexte);
}

// Cartes du mode HA : la pièce de la page courante, cartes vides masquées et les autres
// centrées (formule des zones, ADR-0018 : pas de 250 px).
void peindre_cartes() {
    const TuilesUI& u = g_tuiles_ui;
    if (!g_central_ctx.ha_mode) return;
    const int r = piece_courante();
    int n = 0;
    for (int t = 0; t < kTuiles; t++)
        if (u.carte[t] != nullptr && tuile_presente(r, t)) n++;
    int32_t x = (kEcranL - (n * 250 - 20)) / 2;
    for (int t = 0; t < kTuiles; t++) {
        const bool presente = tuile_presente(r, t);
        ui_hidden(u.carte[t], !presente);
        if (!presente || u.carte[t] == nullptr) continue;
        ui_x(u.carte[t], x);
        x += 250;
        peindre_carte(r, t);
    }
}

// Mode météo : les épaules et le bouton d'une tuile (masqués sans appareil, ou tous
// quand « Tab5 Appareils sur la météo » est éteint ; bouton masqué aussi quand un appui
// ne ferait rien : cap, bin, option r).
void peindre_epaules(int r, int t, lv_obj_t* gauche, lv_obj_t* droite, lv_obj_t* bouton) {
    const bool presente = s_appareils_meteo && tuile_presente(r, t);
    Vue v;
    if (presente) vue(r, t, v);
    ui_hidden(bouton, !presente || !v.agit);
    ui_hidden(gauche, !presente);
    ui_hidden(droite, !presente || v.droite == nullptr);
    if (!presente) return;
    if (gauche != nullptr) {
        if (v.icone != nullptr) ui_text(gauche, v.icone);
        ui_text_color(gauche, v.couleur);
    }
    if (droite != nullptr && v.droite != nullptr) {
        ui_text(droite, v.droite);
        ui_text_color(droite, v.couleur_droite);
    }
}

// Widgets de la tuile T sur le calque météo de la page courante (journalier : pages 2 à
// 4, mêmes objets ; horaire : pages 0 et 1).
void widgets_meteo(int t, lv_obj_t*& gauche, lv_obj_t*& droite, lv_obj_t*& bouton) {
    const TuilesUI& u = g_tuiles_ui;
    const bool jours = g_central_ctx.forecast_page >= 2;
    gauche = jours ? u.jour_g[t] : u.heure_g[t];
    droite = jours ? u.jour_d[t] : u.heure_d[t];
    bouton = jours ? u.jour_bouton[t] : u.heure_bouton[t];
}

// Mode météo : les appareils de la pièce de la page courante (ADR-0023). Sur un calque
// masqué (mode HA), rien à faire : la sortie du mode HA repeint.
void peindre_meteo() {
    if (g_central_ctx.ha_mode) return;
    const int r = piece_courante();
    for (int t = 0; t < kTuiles; t++) {
        lv_obj_t *g, *d, *b;
        widgets_meteo(t, g, d, b);
        peindre_epaules(r, t, g, d, b);
    }
    // Sens du volet 3.x : tuile 1 de l'accueil, mode héritage seulement.
    ui_hidden(g_tuiles_ui.jour_sens, !(s_appareils_meteo && heritage() && r == 0 && tuile_presente(0, 1)));
}

// Une tuile a changé (état, minuterie) : la repeindre là où elle est affichée.
void peindre_tuile(int r, int t) {
    if (r < 0 || r >= kPieces || t < 0 || t >= kTuiles) return;
    popup_lumiere_etat(r, t);
    popup_volet_etat(r, t);
    popup_appareil_etat(r, t);
    roue_tuile_etat(r, t);
    maison_tuile_changee(r, t);  // popup Maison (ADR-0037) : sa ligne, s'il est affiché
    if (r != piece_courante()) return;
    if (g_central_ctx.ha_mode) {
        if (tuile_presente(r, t)) peindre_carte(r, t);
        return;
    }
    lv_obj_t *g, *d, *b;
    widgets_meteo(t, g, d, b);
    peindre_epaules(r, t, g, d, b);
}

void peindre_heritage(int t) {
    if (heritage()) peindre_tuile(0, t);
}

// Bouton « HA » : masqué sans aucun appareil ; entouré de bleu quand le mode HA est actif,
// comme le bouton « Domo » du micro (tab5-assist.yaml). Son ICÔNE n'est pas touchée : sa
// couleur dit si HA est connecté (tab5-sensors-diagnostics.yaml), demande d'Axel du
// 28/09. Ne touche au style qu'au changement.
bool s_bouton_actif = false;
void bouton_ha_peindre() {
    const TuilesUI& u = g_tuiles_ui;
    ui_hidden(u.bouton_ha, aucun_appareil());
    if (u.bouton_ha == nullptr || s_bouton_actif == g_central_ctx.ha_mode) return;
    s_bouton_actif = g_central_ctx.ha_mode;
    if (s_bouton_actif) {
        highlight_button_border(u.bouton_ha, true, UIColor.INFO);
        return;
    }
    // Retour exact au style du bouton (style_clim_btn_page : liseré à 35 %), pas au gris
    // « inactif » de highlight_button_border (40 %) : le rendu hors tablette le voyait.
    for (lv_style_prop_t p : {LV_STYLE_BORDER_COLOR, LV_STYLE_BORDER_OPA, LV_STYLE_BORDER_WIDTH})
        lv_obj_remove_local_style_prop(u.bouton_ha, p, LV_PART_MAIN);
}

// Change de page sans toucher aux calques météo (mode HA) : contexte (seule source de la
// page depuis le 28/09/2026), pastilles.
void aller_page(int page) {
    const TuilesUI& u = g_tuiles_ui;
    if (page < 0 || page >= kPieces) return;
    g_central_ctx.forecast_page = page;
    pagination_afficher(u.pastilles, page);
}

// ─── Commandes ──────────────────────────────────────────────────────────────────────

void envoyer(const char* emplacement, const char* action) {
    if (g_tuiles_ui.envoyer != nullptr) g_tuiles_ui.envoyer(emplacement, action, "");
}

void envoyer_tuile(int r, int t, const char* action) {
    envoyer(tuile_cle(r, t).s, action);
}

void ouvrir_popup(lv_obj_t* popup) {
    if (popup != nullptr) animate_popup_open(popup);
}

// Mode héritage : les gestes de la 3.1 (tuile 0 PC + télécommande, 1 volet, 2-4 lumières).
void appui_heritage(int t, bool long_appui) {
    switch (t) {
        case 0:
            if (!long_appui) envoyer("pc", "basculer");
            else if (!zone_absente(Zone::TV)) telecommande_ouvrir(0);
            return;
        case 1:
            if (!long_appui && g_tuiles_ui.volet_tap != nullptr) g_tuiles_ui.volet_tap();
            return;
        default:
            if (long_appui) popup_lumiere_ouvrir(0, t);
            else envoyer(kHeritageLumieres[t - 2], "basculer");
            return;
    }
}

// ─── Rangée sous l'horloge (ADR-0031) ───────────────────────────────────────────────

// Définitions d'une zone à lignes (rangée, panneau Ok Nabu) lues dans tab5_maj_tuiles :
// gardées et redessinées si elles changent. Un élément qui change d'appareil repart grisé
// (son état était l'ancien).
bool rangee_definir(int z, const ModeleRangee& neuf) {
    ModeleRangee& g = s_rg[z];
    if (std::memcmp(&neuf, &g, sizeof(ModeleRangee)) == 0) return false;
    int n = 0;
    for (int l = 0; l < kLignes; l++)
        for (int i = 0; i < kElements; i++) {
            if (std::memcmp(&neuf.el[l][i], &g.el[l][i], sizeof(DefRangee)) != 0) etat_vider(s_etats_rg[z][l][i]);
            if (neuf.el[l][i].d.type != static_cast<uint8_t>(Type::VIDE)) n++;
        }
    g = neuf;
    s_pref_rg[z].save(&g);
    ESP_LOGI("tab5.tuiles", "%s : %d élément(s), ligne spéciale %d, %d tour(s) par ligne",
             z == RANGEE_NABU ? "Panneau Ok Nabu" : "Rangée sous l'horloge", n, g.plantes + 1, g.tours);
    rangee_definitions_changees(z);
    return true;
}

// « état|valeur|couleur » (le reste d'une entrée tRT ou hLI de tab5_maj_emplacements).
void etat_lire(Etat& e, const char* reste, size_t n_reste) {
    Champ f[3];
    const int nf = decouper(reste, n_reste, f, 3);
    const size_t n = nf > 0 ? std::min(f[0].n, kEtat - 1) : 0;
    std::memset(e.brut, 0, sizeof(e.brut));
    if (n > 0) std::memcpy(e.brut, f[0].p, n);
    // Inconnue (NaN) si vide, illisible ou non finie (« nan », « inf »).
    e.valeur = nf > 1 ? champ_nombre(f[1], NAN) : NAN;
    e.a_couleur = false;
    if (nf > 2 && f[2].n == 6) {
        char tmp[7];
        std::memcpy(tmp, f[2].p, 6);
        tmp[6] = '\0';
        char* bout = nullptr;
        const unsigned long c = strtoul(tmp, &bout, 16);
        if (bout == tmp + 6) {
            e.couleur = static_cast<uint32_t>(c);
            e.a_couleur = true;
        }
    }
    e.recu = true;
}

}  // namespace tuiles

TuilesUI g_tuiles_ui;

// Pour les autres unités (titre du popup clim, ADR-0026) : un nom venu de HA suit les
// mêmes règles que ceux des tuiles. Déclarées dans tab5_internal.h.
void texte_ha_copier(char* dst, size_t cap, const char* src, size_t n) { copier_texte(dst, cap, src, n); }
void texte_ha_coupe(lv_obj_t* lbl, const char* txt, int32_t largeur) { ui_texte_coupe(lbl, txt, largeur); }

bool tuiles_definir(const std::string& payload) {
    charger();
    // Payload faux : les définitions d'avant restent (NVS comprise).
    if (payload_trop_long("tab5.tuiles", payload.size())) return false;
    // Tuile − / + au choix (ADR-0033) : ses clés rN sont dans le même instantané.
    const bool reglables_changes = reglables_definir(payload);
    // Instantané complet : ce qui n'est pas listé est vide. Construit à part (tas, le temps
    // de la comparaison), pour n'écrire la NVS et ne redessiner que si quelque chose change.
    std::unique_ptr<Modele> neuf(new Modele());
    neuf->magic = kMagic;
    neuf->recues = 1;
    // La rangée sous l'horloge aussi (ADR-0031) et le panneau Ok Nabu (lot 3) : sans clé
    // h (n), les plantes (l'écoute) seules, en première ligne — un blueprint plus ancien
    // garde l'écran d'avant.
    struct Rangees {
        ModeleRangee z[RANGEE_NB];
    };
    std::unique_ptr<Rangees> rangees(new Rangees());
    for (int z = 0; z < RANGEE_NB; z++) rangee_vide(rangees->z[z], z);
    size_t debut = 0;
    while (debut < payload.size()) {
        size_t fin = payload.find(';', debut);
        if (fin == std::string::npos) fin = payload.size();
        lire_entree(payload.data() + debut, fin - debut, *neuf, rangees->z);
        debut = fin + 1;
    }
    bool rangee_changee = false;
    for (int z = 0; z < RANGEE_NB; z++) rangee_changee |= rangee_definir(z, rangees->z[z]);
    if (std::memcmp(neuf.get(), &s_m, sizeof(Modele)) == 0) return rangee_changee || reglables_changes;
    // Une tuile qui change d'appareil repart grisée : l'état reçu était celui de l'ancien.
    // Sa clim aussi est oubliée (ADR-0027) : le blueprint renvoie ses réglages juste après.
    for (int r = 0; r < kPieces; r++)
        for (int t = 0; t < kTuiles; t++)
            if (std::memcmp(&neuf->tuiles[r][t], &s_m.tuiles[r][t], sizeof(Def)) != 0) {
                etat_vider(s_etats[r][t]);
                clim_tuile_oublier(r, t);
            }
    const bool premiere = s_m.recues == 0;
    s_m = *neuf;
    s_pref.save(&s_m);
    int n = 0;
    for (const auto& piece : s_m.tuiles)
        for (const Def& d : piece)
            if (d.type != static_cast<uint8_t>(Type::VIDE)) n++;
    ESP_LOGI("tab5.tuiles", "Définitions reçues : %d appareil(s)%s", n,
             premiere ? " (fin du mode héritage)" : "");
    // Une minuterie peut viser une tuile qui n'existe plus.
    minuterie_arreter(s_confirmation);
    minuterie_arreter(s_ok);
    minuterie_arreter(s_sens);
    // Roue d'actions rapides (ADR-0036) : ses boutons étaient ceux de l'ancienne définition.
    roue_actions_fermer();
    // Popups lumière, volet et appareil : revalidés (tab5_tuiles_popups.cpp).
    popups_revalider();
    tuiles_appliquer_ui();
    // Gestes de l'accueil réglés sur Lumières ou Volets (ADR-0046) : disponibles selon les
    // tuiles, leurs icônes suivent.
    boutons_haut_apply_ui();
    return true;
}

bool tuiles_etat_recu(const char* cle, size_t n_cle, const char* reste, size_t n_reste) {
    int r = 0, t = 0;
    if (n_cle != 3) return false;
    // Élément I de la ligne L de la rangée sous l'horloge (« hLI », ADR-0031) ou du
    // panneau Ok Nabu (« nLI », lot 3).
    const int z = zone_de_lettre(cle[0]);
    if (z >= 0) {
        if (!chiffre_0_4(cle[1], r) || r >= kLignes || !chiffre_0_4(cle[2], t) || t >= kElements) return false;
        charger();
        etat_lire(s_etats_rg[z][r][t], reste, n_reste);
        rangee_element_change(z, r, t);
        return true;
    }
    if (cle[0] != 't' || !chiffre_0_4(cle[1], r) || !chiffre_0_4(cle[2], t)) return false;
    charger();
    Etat& e = s_etats[r][t];
    etat_lire(e, reste, n_reste);
    if (s_m.tuiles[r][t].type == static_cast<uint8_t>(Type::VOL)) vol_sens_suivre(e);
    // Popup du volet : un état poussé remplace la position envoyée au relâcher.
    popup_volet_etat_pousse(r, t);
    if (!heritage()) peindre_tuile(r, t);
    return true;
}

// ─── Zones à lignes (rangée ADR-0031, panneau Ok Nabu), lues par tab5_rangee.cpp ────

namespace {
bool zone_valide(int z) { return z >= 0 && z < RANGEE_NB; }
}  // namespace

int rangee_place_speciale(int z) {
    charger();
    return zone_valide(z) ? s_rg[z].plantes : -1;
}

int rangee_tours(int z) {
    charger();
    return zone_valide(z) ? std::max<int>(1, s_rg[z].tours) : kToursDefaut;
}

bool rangee_ligne_remplie(int z, int l) {
    charger();
    if (!zone_valide(z) || l < 0 || l >= kLignes) return false;
    for (const DefRangee& g : s_rg[z].el[l])
        if (g.d.type != static_cast<uint8_t>(Type::VIDE)) return true;
    return false;
}

// Un élément : l'icône et sa couleur comme sur une tuile (vue_def) ; un capteur ou une
// clim y ajoute sa valeur. Celle d'un capteur est écrite court (« 21.4 ° », « 2.4 kW »)
// et colorée selon sa nature (classe d'appareil) : l'échelle des températures de
// l'écran, celle de l'humidité, celle des batteries, l'or pour la puissance et l'énergie.
// Les tuiles gardent leurs couleurs (« INFO » pour un capteur qui n'est pas en °).
bool rangee_element(int z, int l, int i, RangeeElement& out) {
    charger();
    out = RangeeElement{};
    if (!zone_valide(z) || l < 0 || l >= kLignes || i < 0 || i >= kElements) return false;
    const DefRangee& g = s_rg[z].el[l][i];
    const Type type = static_cast<Type>(g.d.type);
    if (type == Type::VIDE) return false;
    const Etat& e = s_etats_rg[z][l][i];
    Vue v;
    vue_def(g.d, e, -1, i, v);
    out.icone = v.icone;
    out.couleur = v.couleur;
    out.mesure = type == Type::CAP || type == Type::CLI;
    if (!out.mesure) return true;
    snprintf(out.texte, sizeof(out.texte), "%s", v.ligne);
    out.couleur_texte = v.couleur_ligne;
    // Pas encore reçu, hors ligne, ou pas un nombre : le texte de la tuile.
    if (type != Type::CAP || !e.recu || std::isnan(e.valeur) || etat_indisponible(e.brut))
        return true;
    const char* unite = g.d.complement;
    const bool degres = std::strncmp(unite, "\xC2\xB0", 2) == 0 || est(g.classe, "temperature");
    uint32_t c = v.couleur;
    if (degres) {
        formater_mesure(out.texte, sizeof(out.texte), e.valeur, "\xC2\xB0");
        // Échelle en °C : une mesure en °F y est ramenée.
        c = get_temperature_color(est(unite, "\xC2\xB0" "F") ? (e.valeur - 32.0f) * 5.0f / 9.0f : e.valeur);
    } else {
        if (!energie_formater(out.texte, sizeof(out.texte), e.valeur, unite))
            formater_mesure(out.texte, sizeof(out.texte), e.valeur, unite);
        if (est(g.classe, "humidity") || est(g.classe, "moisture")) c = get_humidity_color(e.valeur);
        else if (est(g.classe, "battery")) c = get_battery_color(e.valeur);
        else if (est(g.classe, "power") || est(g.classe, "energy") || est(g.d.icone, "solaire")) c = UIColor.GOLD;
    }
    out.couleur = out.couleur_texte = c;
    return true;
}

void tuiles_appliquer_ui() {
    charger();
    CentralPanelCtx& ctx = g_central_ctx;
    const TuilesUI& u = g_tuiles_ui;
    // Popup Maison (ADR-0037) : définitions, zones ou thème changés, il se redispose s'il
    // est affiché (sinon rien : il se dispose à chaque ouverture).
    maison_definitions_changees();
    if (ctx.ha_mode) {
        if (aucun_appareil()) {
            tuiles_mode_ha(false);
            return;
        }
        // Une pièce vide reste affichée (« Aucun appareil ») : le swipe passe par les cinq
        // pages (28/09, demande d'Axel, une seule pièce configurée = swipe muet sinon).
        peindre_cartes();
        // Titre (nom ou nombre de pièces changé) ; une réponse vocale garde la carte.
        if (!ctx.vocal_shown) update_central_forecast_page_ui(ctx.forecast_page, u.titre_cadre, u.titre, ctx);
    }
    peindre_meteo();
    bouton_ha_peindre();
}

void tuiles_peindre_meteo() {
    charger();
    peindre_meteo();
}

// Thèmes (ADR-0029) : tuiles, cartes, bouton « HA » (sa garde au changement forcée),
// popups lumière, volet et appareil, depuis le dernier état ; rien avant le premier
// dessin des tuiles.
void tuiles_rejouer_theme() {
    if (!s_charge) return;
    s_bouton_actif = !g_central_ctx.ha_mode;
    tuiles_appliquer_ui();
    popups_rejouer_theme();
}

void tuiles_repeindre(int r, int t) {
    charger();
    if (!heritage()) peindre_tuile(r, t);
}

void tuiles_appareils_meteo(bool montres) {
    if (montres == s_appareils_meteo) return;
    s_appareils_meteo = montres;
    ESP_LOGI("tab5.tuiles", "Appareils sur la météo : %s", montres ? "oui" : "non");
    // Restauré au setup, avant le premier dessin : tuiles_appliquer_ui() lira le réglage.
    // Ensuite, tout de suite : les épaules de la page courante (rien en mode HA).
    if (s_charge) peindre_meteo();
}

void tuiles_mode_ha(bool actif) {
    charger();
    CentralPanelCtx& ctx = g_central_ctx;
    const TuilesUI& u = g_tuiles_ui;
    if (actif == ctx.ha_mode) return;
    if (u.calque_ha == nullptr || u.calque_jours == nullptr || u.calque_heures == nullptr) return;
    const int page = ctx.forecast_page;
    lv_obj_t* meteo = page >= 2 ? u.calque_jours : u.calque_heures;
    lv_obj_t* autre = page >= 2 ? u.calque_heures : u.calque_jours;
    if (actif) {
        // Page sans appareil : la pièce la plus proche qui en a.
        const int cible = page_non_vide_proche(page);
        if (cible < 0) return;  // aucun appareil : le bouton est masqué
        ctx.ha_mode = true;
        if (cible != page) aller_page(cible);
        peindre_cartes();
    } else {
        ctx.ha_mode = false;
        // La météo de la page courante : on a pu changer de pièce en mode HA.
        g_forecast_roll_suppress = true;
        if (page >= 2) refresh_daily_forecast(g_day_slots, page - 2, u.police_meteo, u.police_meteo_petite);
        else refresh_hourly_forecast(g_hour_slots, 1 - page, u.police_meteo, u.police_meteo_petite);
        g_forecast_roll_suppress = false;
        peindre_meteo();
    }
    // L'autre calque météo : animation coupée, remis en place et masqué (un swipe en cours
    // continuerait sinon sous HIDDEN et polluerait le prochain affichage).
    lv_anim_delete(autre, nullptr);
    lv_obj_set_x(autre, 0);
    lv_obj_set_style_opa(autre, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_add_flag(autre, LV_OBJ_FLAG_HIDDEN);
    if (actif) animate_crossfade_layers(meteo, u.calque_ha);
    else animate_crossfade_layers(u.calque_ha, meteo);
    central_mode_ha(u.titre_cadre, u.titre, ctx);
    bouton_ha_peindre();
    // Zone des températures et tuile − / + : la pièce (sa température déclarée, sa clim)
    // ou, hors du mode HA, le salon et la serre (ADR-0040).
    accueil_temperatures_ui();
    reglables_clim_changee();
}

// Mode HA : la pièce de la page `page` — cartes, titre de la carte centrale, zone des
// températures et tuile − / + (ADR-0040). Le swipe et la roue de navigation.
static void montrer_page_ha(int page) {
    aller_page(page);
    peindre_cartes();
    central_mode_ha(g_tuiles_ui.titre_cadre, g_tuiles_ui.titre, g_central_ctx);
    accueil_temperatures_ui();  // la nouvelle pièce, ou le salon et la serre (ADR-0040)
    reglables_clim_changee();
}

// Mode HA : la page suivante dans l'ordre de la météo, pièce vide comprise (elle dit
// « Aucun appareil ») — comme les cinq pages de prévisions. Avant le 28/09, les pièces
// vides étaient sautées : avec une seule pièce configurée, le swipe ne faisait rien.
void tuiles_swipe_ha(bool gauche) {
    charger();
    montrer_page_ha(forecast_page_suivante(g_central_ctx.forecast_page, gauche));
}

bool tuiles_aller_piece(int r) {
    charger();
    if (r < 0 || r >= kPieces || !piece_non_vide(r)) return false;
    int page = -1;
    for (int p = 0; p < kPieces; p++)
        if (kPieceDePage[p] == r) page = p;
    if (page < 0) return false;
    // D'abord le mode HA par son seul chemin (calques, bouton « HA ») : il se pose sur la
    // pièce non vide la plus proche de la page affichée, puis on va sur celle demandée.
    if (!g_central_ctx.ha_mode) tuiles_mode_ha(true);
    if (!g_central_ctx.ha_mode) return false;
    if (g_central_ctx.forecast_page != page) montrer_page_ha(page);
    return true;
}

// La tuile qu'ouvre l'écran `e` (Lumières, Volet) : la pièce de la page affichée d'abord,
// puis les pièces dans l'ordre du blueprint. Le popup s'ouvre sur la page de cette pièce
// (une page par pièce, ADR-0046) ; ses critères sont ceux des pages : est_lumiere (lum sans
// l'option r ; mode héritage : les lumières de la 3.1) et est_volet (vol sans l'option r
// ni k ; mode héritage : aucun), sinon l'écran s'ouvrirait sur un popup vide.
static bool tuile_de_ecran(Ecran e, int& r_out, int& t_out) {
    charger();
    const int premiere = piece_courante();
    for (int i = -1; i < kPieces; i++) {
        const int r = i < 0 ? premiere : i;
        if (i == premiere) continue;  // déjà vue
        for (int t = 0; t < kTuiles; t++) {
            const bool ok = e == Ecran::LUMIERES ? est_lumiere(r, t) : e == Ecran::VOLET && est_volet(r, t);
            if (ok) {
                r_out = r;
                t_out = t;
                return true;
            }
        }
    }
    return false;
}

bool tuiles_ecran_disponible(Ecran e) {
    int r, t;
    return tuile_de_ecran(e, r, t);
}

void tuiles_ecran_ouvrir(Ecran e) {
    int r, t;
    if (!tuile_de_ecran(e, r, t)) return;
    if (e == Ecran::LUMIERES) popup_lumiere_ouvrir(r, t);
    else popup_volet_ouvrir(r, t);
}

// « Pièce n/5 » (n = numéro de la pièce dans le blueprint), puis son nom, « Pièce n »
// sans nom, ou « Aucun appareil » pour une pièce vide.
bool tuiles_titre_piece(std::string& chapeau, std::string& titre) {
    charger();
    const int r = piece_courante();
    char buf[48];
    snprintf(buf, sizeof(buf), tr("Pièce %d/%d"), r + 1, kPieces);
    chapeau = buf;
    if (!piece_non_vide(r)) {
        titre = tr("Aucun appareil");
    } else if (!heritage() && s_m.pieces[r][0] != '\0') {
        titre = s_m.pieces[r];
    } else {
        snprintf(buf, sizeof(buf), tr("Pièce %d"), r + 1);
        titre = buf;
    }
    return true;
}

// Toucher du titre d'une tuile (mode météo : l'onglet du jour ou de l'heure ; mode HA :
// l'onglet du nom) : bascule le sens d'un volet, comme le bouton de titre de la 3.1.
void tuile_titre_appui(int t) {
    charger();
    // Mode météo sans appareils : un titre de prévision ne fait rien.
    if (!g_central_ctx.ha_mode && !s_appareils_meteo) return;
    const int r = piece_courante();
    if (heritage()) {
        if (r == 0 && t == 1 && tuile_presente(0, 1)) tuiles_heritage_volet_sens();
        return;
    }
    if (!tuile_presente(r, t)) return;
    const Def& d = s_m.tuiles[r][t];
    if (static_cast<Type>(d.type) != Type::VOL || (d.options & OPT_R)) return;
    Etat& e = s_etats[r][t];
    e.sens = vol_sens(e) == SENS_FERMER ? SENS_OUVRIR : SENS_FERMER;
    minuterie_armer(s_sens, r, t, kSensMs);  // repeint : flèche, et la ligne d'état 2 s
}

static void titre_rappel(lv_event_t* ev) {
    tuile_titre_appui(static_cast<int>(reinterpret_cast<intptr_t>(lv_event_get_user_data(ev))));
}

// Rend cliquables les onglets de titre des tuiles (jours, heures, cartes HA). Une seule
// fois : tab5_tuiles_ui est rejoué à chaque réponse des zones.
void tuiles_brancher_titres() {
    static bool fait = false;
    if (fait) return;
    fait = true;
    const TuilesUI& u = g_tuiles_ui;
    for (int t = 0; t < kTuiles; t++) {
        for (lv_obj_t* libelle : {u.jour_titre[t], u.heure_titre[t], u.carte_nom[t]}) {
            if (libelle == nullptr) continue;
            lv_obj_t* onglet = lv_obj_get_parent(libelle);
            if (onglet == nullptr) continue;
            lv_obj_add_flag(onglet, LV_OBJ_FLAG_CLICKABLE);
            lv_obj_add_event_cb(onglet, titre_rappel, LV_EVENT_SHORT_CLICKED,
                                reinterpret_cast<void*>(static_cast<intptr_t>(t)));
        }
    }
}

// La fenêtre `f` de la tuile tRT (définition `d`). Faux si rien ne s'ouvre : aucune
// fenêtre, ou une clim de tuile dont la tablette n'a pas les réglages.
static bool ouvrir_fenetre(Fenetre f, const Def& d, int r, int t) {
    switch (f) {
        case Fenetre::LUMIERE:
            popup_lumiere_ouvrir(r, t);
            return true;
        case Fenetre::VOLET:
            popup_volet_ouvrir(r, t);
            return true;
        case Fenetre::APPAREIL:
            popup_appareil_ouvrir(r, t);
            return true;
        case Fenetre::TELECOMMANDE:
            telecommande_ouvrir(0);  // la page de la TV (ADR-0056)
            return true;
        case Fenetre::CLIM: {
            // La clim du blueprint (option m) ou celle de la tuile (ADR-0027). Le popup
            // revient à la clim du blueprint à sa fermeture.
            const ClimCible c = clim_cible(d, r, t);
            if (c.r < 0) clim_afficher_blueprint();
            else if (!clim_afficher_tuile(c.r, c.t)) return false;
            ouvrir_popup(g_tuiles_ui.popup_clim);
            return true;
        }
        case Fenetre::ENERGIE:
            if (g_tuiles_ui.energie_ouvrir != nullptr) g_tuiles_ui.energie_ouvrir();
            return true;
        case Fenetre::LECTEUR:
            // Popup Musique (ADR-0050) : HA retrouve le media_player de la tuile par sa clé.
            if (g_tuiles_ui.lecteur_ouvrir != nullptr) g_tuiles_ui.lecteur_ouvrir(modele_ha::tuile_cle(r, t).s);
            return true;
        default:
            return false;
    }
}

// Appui sur la tuile T de la pièce R : tuile de la page courante (tuile_appui), ou grand
// bouton du popup d'un appareil (popup_appareil_appui, tab5_tuiles_popups.cpp, appui
// court). Un seul chemin pour les deux : la même commande, la même confirmation (option
// k), le même « OK ». Ce que fait chaque type : kGestes et gestes(), plus haut.
void tuiles::tuile_appui_piece(int r, int t, bool long_appui) {
    if (!tuile_presente(r, t)) return;
    if (heritage()) {
        appui_heritage(t, long_appui);
        return;
    }
    const Def& d = s_m.tuiles[r][t];
    const Etat& e = s_etats[r][t];
    const Type type = static_cast<Type>(d.type);
    // cap sans e, bin, option r, cli sans m dont la tablette n'a pas les réglages
    const Gestes g = gestes(d, type == Type::CLI && clim_tuile_connue(r, t));
    if (!g.agit) return;
    // Appui long : la roue d'actions rapides (ADR-0036), sinon la fenêtre du type ; un
    // volet à confirmer (option k) n'en a aucune : l'autre sens, confirmé.
    if (long_appui) {
        if (g.roue && roue_de_la_tuile(r, t)) return;
        if (g.fenetre != Fenetre::AUCUNE) {
            ouvrir_fenetre(g.fenetre, d, r, t);
            return;
        }
    }
    // Appui court (tableau de l'ADR-0023) : la commande ; une clim, un capteur : sa fenêtre.
    const char* action = type == Type::VOL ? (long_appui ? vol_appui_long(e) : vol_appui(e)) : g.commande;
    if (action == nullptr) {
        ouvrir_fenetre(g.fenetre, d, r, t);
        return;
    }
    // Option k : un second appui dans les 3 s envoie ; la ligne d'état le demande.
    if (d.options & OPT_K) {
        if (!minuterie_sur(s_confirmation, r, t)) {
            minuterie_armer(s_confirmation, r, t, kConfirmationMs);
            return;
        }
        minuterie_arreter(s_confirmation);
    }
    envoyer_tuile(r, t, action);
    if (type == Type::ACT) minuterie_armer(s_ok, r, t, kOkMs);
}

// Tuile − / + au choix (ADR-0033) : le toucher de la valeur d'un appareil qui est aussi
// dans une pièce ouvre le popup de sa tuile, quelle que soit la page affichée — celui de
// son appui long (lumière, volet sans l'option k, télécommande de la TV), ou de son appui
// pour une clim. Option r (lecture seule) : rien, comme sur la tuile.
// Le popup propre à l'appareil seulement (kGestes) : ni celui d'un appareil générique
// (int, act), ni le popup Énergie d'un capteur, ni le lecteur de musique d'un med sans t
// (ADR-0050 : ses trois ouvertures sont l'appui long, la mini-barre et un geste).
bool tuile_ouvrir_popup(int r, int t) {
    charger();
    if (heritage() || !tuile_presente(r, t)) return false;
    const Def& d = s_m.tuiles[r][t];
    const Fenetre f = gestes(d, false).fenetre;
    if (f == Fenetre::APPAREIL || f == Fenetre::ENERGIE || f == Fenetre::LECTEUR) return false;
    return ouvrir_fenetre(f, d, r, t);
}

void tuile_appui(int t, bool long_appui) {
    charger();
    // Mode météo sans appareils : boutons masqués ; pas d'appui non plus par une autre voie.
    if (!g_central_ctx.ha_mode && !s_appareils_meteo) return;
    tuile_appui_piece(piece_courante(), t, long_appui);
}

void tuiles_heritage_pc(bool actif) {
    charger();
    s_h.pc = actif;
    s_h.pc_recu = true;
    peindre_heritage(0);
}

void tuiles_heritage_tv(bool actif) {
    charger();
    s_h.tv = actif;
    peindre_heritage(0);
}

void tuiles_heritage_lumiere(int i, bool allumee) {
    if (i < 0 || i > 2) return;
    charger();
    s_h.lum[i] = allumee;
    s_h.lum_recu[i] = true;
    peindre_heritage(2 + i);
}

void tuiles_heritage_luminosite(int i, float luminosite) {
    if (i < 0 || i > 2) return;
    charger();
    s_h.lum_val[i] = luminosite;
    if (heritage()) popup_lumiere_etat(0, 2 + i);
}

// « Pièce : tout éteindre » (pR / eteindre : toutes les lumières de la pièce R ; en mode
// héritage, lumieres / eteindre). Popup lumière (« Tout éteindre ») et popup Maison
// (« Éteindre les lumières », une fois par pièce qui a des lumières, ADR-0037).
void tuiles_piece_eteindre(int r) {
    charger();
    if (heritage()) {
        envoyer("lumieres", "eteindre");
        return;
    }
    if (r < 0 || r >= kPieces) return;
    envoyer(piece_cle(r).s, "eteindre");
}

bool tuiles_heritage_volet(const std::string& etat) {
    charger();
    std::memset(s_h.volet, 0, sizeof(s_h.volet));
    std::memcpy(s_h.volet, etat.data(), std::min(etat.size(), kEtat - 1));
    if (etat == "Ouvert" || etat == "Partiel" || etat == "open") s_h.volet_ouvert = 1;
    else if (etat == "Ferme" || etat == "closed") s_h.volet_ouvert = 0;
    peindre_heritage(1);
    return etat == "En_mouvement";
}

void tuiles_heritage_volet_sens() {
    charger();
    bool* sens = g_tuiles_ui.volet_sens;
    if (sens == nullptr) return;
    *sens = !*sens;
    peindre_heritage(1);
}

// ─── Popup Maison (ADR-0037, 07/10/2026, discussion #278) ───────────────────────────
//
// Toute la maison, pièce par pièce, dans un popup (tab5_maison.cpp le dispose et le
// peint). Il ne lit rien d'autre que les tuiles : ce qu'il montre et ce qu'il fait passe
// par les fonctions des cartes du mode HA (vue, peindre_vue_sur) et des tuiles
// (tuile_appui_piece). Aucune donnée ni commande nouvelle.

// Carrousel du popup clim (ADR-0038, tab5_clim.cpp) : il s'ouvre sur la clim de la pièce
// affichée en mode HA. En mode météo, ou sans pièces reçues, aucune.
int tuiles_piece_mode_ha() {
    charger();
    if (!g_central_ctx.ha_mode || heritage()) return -1;
    return piece_courante();
}

bool tuiles_piece_titre(int r, char* out, size_t n) {
    charger();
    if (r < 0 || r >= kPieces || out == nullptr || n == 0 || !piece_non_vide(r)) return false;
    // Comme le titre de la carte centrale en mode HA (tuiles_titre_piece).
    if (!heritage() && s_m.pieces[r][0] != '\0') snprintf(out, n, "%s", s_m.pieces[r]);
    else snprintf(out, n, tr("Pièce %d"), r + 1);
    return true;
}

bool tuiles_piece_a_lumieres(int r) {
    charger();
    for (int t = 0; t < kTuiles; t++)
        if (est_lumiere(r, t)) return true;
    return false;
}

// Une ligne du popup Maison agit-elle au toucher (comme la tuile : gestes), et a-t-elle
// un appui long (bouton « ⋯ ») ? lum, int, vol, med, act, et cli quand elle agit ; ni cap
// (même avec l'option e : son toucher ouvre le popup Énergie), ni bin, ni l'option r.
// Mode héritage : la 3.1 (tuile 0 : télécommande s'il y a une TV ; 1 : rien ; 2-4 : popup
// lumière).
bool tuile_gestes(int r, int t, bool& agit, bool& appui_long) {
    charger();
    agit = appui_long = false;
    if (!tuile_presente(r, t)) return false;
    if (heritage()) {
        agit = true;
        appui_long = t == 0 ? !zone_absente(Zone::TV) : t >= 2;
        return true;
    }
    const Def& d = s_m.tuiles[r][t];
    const Type type = static_cast<Type>(d.type);
    agit = gestes(d, type == Type::CLI && clim_tuile_connue(r, t)).agit;
    appui_long = agit && type != Type::CAP && type != Type::BIN;
    return true;
}

bool tuile_peindre_ligne(int r, int t, const TuileWidgets& w, int32_t largeur) {
    charger();
    if (!tuile_presente(r, t)) return false;
    Vue v;
    vue(r, t, v);
    peindre_vue_sur(v, w, largeur);
    return true;
}

// Toucher, appui long ou « ⋯ » d'une ligne : le geste de la tuile, par le même chemin
// (tuile_appui_piece : même commande, même confirmation avec l'option k, même popup).
// Appui long et « ⋯ » : d'abord la roue d'actions rapides (ADR-0036, sans son lien « Maison »), ancrée sur la
// pastille de la ligne (`ancre`) et non sur la carte du mode HA ; sans roue pour cette
// tuile, le chemin de la tuile (son popup ; une clim sans roue ouvre le sien).
void tuile_appui_maison(int r, int t, bool long_appui, lv_obj_t* ancre) {
    charger();
    if (long_appui && tuile_roue_ouvrir(r, t, ancre, true)) return;
    tuile_appui_piece(r, t, long_appui);
}
