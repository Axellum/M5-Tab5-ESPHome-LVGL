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
 *         - Popups des tuiles : lumière (appui long d'une lum), volet (appui long d'une
 *           vol sans l'option k, 05/10/2026 : position, volet dessiné qu'on fait glisser
 *           (06/10/2026), envoyé au relâcher, Ouvrir / Stop / Fermer) et appareil (appui
 *           long d'une int, d'une act ou d'une med sans l'option t, 06/10/2026 : la
 *           fenêtre « plus d'infos » d'un tableau de bord HA, dont le grand bouton refait
 *           le toucher de la tuile), repeints quand l'état de leur tuile change. Quand
 *           les définitions changent, popup ouvert, tous trois sont revalidés (repeints,
 *           ou refermés si leur tuile a disparu ; tuiles_definir).
 *         - Rangée sous l'horloge (ADR-0031, 06/10/2026) : trois lignes de quatre
 *           éléments au plus, mêmes types que les tuiles, dans la même action
 *           (« hLI|type|icône|options|complément|nom|classe », « hp|place des plantes »)
 *           et les mêmes états (clés hLI). Modèle et NVS ici (clé à part, magie « RAN1 »),
 *           dessin et rotation dans tab5_rangee.cpp (rangee_element, plus bas).
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
#include "tab5_internal.h"
#include "tab5_tuiles_icones.h"
#include "tab5_geometrie.h"
#include "tab5_modele_ha.h"
#include "lvgl.h"
#include <esp_attr.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>

// kNom, kIcone, kEtat, est(), etat_indisponible(), copier_icone() : communs avec la tuile
// − / + (tab5_modele_ha.h, lot L5).
using namespace modele_ha;

namespace {

// kPieces et kTuiles : tab5_geometrie.h. Pièce de chaque page du bas (index = g_central_ctx.forecast_page) : R0 = page 2 (accueil),
// R1 = 3, R2 = 4, R3 = 1, R4 = 0 — l'ordre où un swipe les atteint depuis l'accueil.
constexpr int kPieceDePage[kPieces] = {4, 3, 0, 1, 2};

// Types de tuile (ADR-0023), dans l'ordre de kTypes.
enum class Type : uint8_t { VIDE, LUM, INT, VOL, MED, ACT, CAP, BIN, CLI };
constexpr const char* kTypes[] = {"", "lum", "int", "vol", "med", "act", "cap", "bin", "cli"};
constexpr int kNbTypes = sizeof(kTypes) / sizeof(kTypes[0]);

// Options : une lettre chacune, bit i = lettre i de kLettresOptions.
//   d graduable, c couleur, o allumer seulement, k confirmer, r lecture seule,
//   t télécommande TV du blueprint, m climatisation du blueprint (sans m, une tuile cli a
//   sa propre clim dans le popup dès que HA en a envoyé les réglages, ADR-0027),
//   e capteur de la section « Énergie » du blueprint (un cap qui ouvre le popup Énergie,
//   ADR-0028). Un firmware plus ancien ignore une lettre qu'il ne connaît pas.
constexpr char kLettresOptions[] = "dcokrtme";
enum : uint8_t { OPT_D = 1, OPT_C = 2, OPT_O = 4, OPT_K = 8, OPT_R = 16, OPT_T = 32, OPT_M = 64, OPT_E = 128 };

// Taille des champs gardés (octets, zéro final compris) ; kNom, kIcone et kEtat :
// tab5_modele_ha.h.
constexpr size_t kComplement = 16;   // unité (cap) ou classe d'appareil (bin)

struct Def {
    uint8_t type;       // Type
    uint8_t options;    // OPT_*
    char icone[kIcone];
    char complement[kComplement];
    char nom[kNom];
};

// Exactement ce qui part en NVS : tout en octets, sans bourrage (memcmp fiable).
struct Modele {
    uint32_t magic;
    uint8_t recues;       // 1 dès la première tab5_maj_tuiles : fin du mode héritage
    uint8_t reserve[3];
    char pieces[kPieces][kNom];
    Def tuiles[kPieces][kTuiles];
};

struct Etat {
    char brut[kEtat];     // état HA ("" tant que rien n'est reçu)
    float valeur;         // luminosité, position, mesure, température ; NaN sinon
    uint32_t couleur;     // couleur propre d'une lumière (rgb_color)
    bool a_couleur;
    bool recu;
    uint8_t sens;         // volet : SENS_INCONNU, SENS_OUVRIR, SENS_FERMER (toucher du titre)
};

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

// Exactement ce qui part en NVS (octets seulement, sans bourrage : memcmp fiable).
struct ModeleRangee {
    uint32_t magic;
    int8_t plantes;       // place de la ligne des plantes : 0 à 2, -1 masquée
    uint8_t tours;        // une ligne dure `tours` tours de la carte centrale (1 à 15)
    uint8_t reserve[2];
    DefRangee el[kLignes][kElements];
};

constexpr uint32_t kMagicRangee = 0x52414E31;    // « RAN1 »
constexpr uint32_t kPrefKeyRangee = 0x72616E67;  // « rang »
// La rangée est calée sur le rotateur de la carte centrale (demande d'Axel du 06/10/2026) :
// un tour = sa période, 8 s (tab5_central_rotator_auto, tab5-scripts.yaml : 7,8 s, la
// rangée, puis 0,2 s et la carte centrale ; tests/test_rangee.py compare). Le blueprint
// envoie la durée d'une ligne en secondes (« hd|32 ») ; sans elle, 4 tours (32 s).
constexpr int kTourCentralS = 8;
constexpr uint8_t kToursDefaut = 4;
constexpr uint8_t kToursMax = 15;

// ~2,3 Ko lus au dessin et aux poussées seulement : en PSRAM (BSS externe, remise à zéro
// au démarrage, CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY), pas dans les ~226 Ko de
// RAM interne libre. La rangée (~1,2 Ko avec ses états) aussi.
EXT_RAM_BSS_ATTR Modele s_m;
EXT_RAM_BSS_ATTR Etat s_etats[kPieces][kTuiles];
EXT_RAM_BSS_ATTR ModeleRangee s_rg;
EXT_RAM_BSS_ATTR Etat s_etats_rg[kLignes][kElements];
bool s_charge = false;
esphome::ESPPreferenceObject s_pref;
esphome::ESPPreferenceObject s_pref_rg;
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
    for (auto& ligne : s_etats_rg)
        for (Etat& e : ligne) etat_vider(e);
    s_pref_rg = esphome::global_preferences->make_preference<ModeleRangee>(kPrefKeyRangee);
    ModeleRangee g{};
    if (s_pref_rg.load(&g) && g.magic == kMagicRangee) {
        s_rg = g;
        ESP_LOGI("tab5.tuiles", "Rangée sous l'horloge relue de la NVS");
    } else {
        // Rien reçu : les plantes seules, en première ligne (l'écran d'avant l'ADR-0031).
        s_rg = ModeleRangee{};
        s_rg.magic = kMagicRangee;
        s_rg.tours = kToursDefaut;
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

// Une entrée de tab5_maj_tuiles dans `m` (pièces) ou `g` (rangée sous l'horloge) ; une
// clé inconnue est ignorée.
void lire_entree(const char* s, size_t n, Modele& m, ModeleRangee& g) {
    Champ f[7];
    const int nf = decouper(s, n, f, 7);
    if (nf < 2) return;
    int r = 0, t = 0;
    if (f[0].n == 2 && f[0].p[0] == 'p' && chiffre_0_4(f[0].p[1], r)) {
        copier_texte(m.pieces[r], kNom, f[1].p, f[1].n);
        return;
    }
    // « hp|0 » à « hp|2 » : place de la ligne des plantes ; autre chose (« hp|- ») : masquée.
    if (f[0].n == 2 && f[0].p[0] == 'h' && f[0].p[1] == 'p') {
        int place = 0;
        const bool ok = f[1].n == 1 && chiffre_0_4(f[1].p[0], place) && place < kLignes;
        g.plantes = static_cast<int8_t>(ok ? place : -1);
        return;
    }
    // « hd|secondes » : durée d'une ligne, arrondie au tour de la carte centrale le plus
    // proche (1 à 15 tours) ; illisible : le défaut.
    if (f[0].n == 2 && f[0].p[0] == 'h' && f[0].p[1] == 'd') {
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
    // « hLI|type|icône|options|complément|nom|classe » : élément I de la ligne L.
    if (f[0].n == 3 && f[0].p[0] == 'h') {
        int l = 0, i = 0;
        if (!chiffre_0_4(f[0].p[1], l) || l >= kLignes || !chiffre_0_4(f[0].p[2], i) || i >= kElements) return;
        DefRangee& e = g.el[l][i];
        lire_def(f, nf, e.d);
        if (nf > 6 && e.d.type != static_cast<uint8_t>(Type::VIDE)) copier_icone(e.classe, f[6].p, f[6].n);
        return;
    }
    if (f[0].n != 3 || f[0].p[0] != 't' || !chiffre_0_4(f[0].p[1], r) || !chiffre_0_4(f[0].p[2], t)) return;
    lire_def(f, nf, m.tuiles[r][t]);
}

// ─── Mode héritage : les emplacements 3.x forment la pièce 0 ─────────────────────────

struct Heritage {
    bool pc = false;
    bool pc_recu = false;
    bool tv = false;
    bool lum[3] = {};
    bool lum_recu[3] = {};
    float lum_val[3] = {NAN, NAN, NAN};  // luminosité 0-255 (arc du popup)
    char volet[kEtat] = "";      // dernier etat_physique (En_mouvement, Ouvert, Ferme…)
    int8_t volet_ouvert = -1;    // dernier état connu hors mouvement : 1 ouvert, 0 fermé
};
Heritage s_h;

// Commandes 3.x des trois lumières (tuiles 2 à 4), comme les boutons de la 3.1.
constexpr const char* kHeritageLumieres[3] = {"lumiere_1", "lumiere_2", "lumiere_3"};

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

// … dans le sélecteur du popup lumière (45 px, tuiles 2 à 4)…
const char* heritage_glyphe_selecteur(int t) {
    switch (t) {
        case 2: return "\U000F02E3";   // bed
        case 3: return "\U000F04B9";   // sofa
        default: return "\U000F1051";  // led-strip-variant
    }
}

// … et sur les cartes du mode HA (70 px).
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

struct Vue {
    const char* icone = nullptr;        // épaule gauche (32 px) ; nullptr : inchangée
    uint32_t couleur = UIColor.INACTIVE;
    const char* icone_carte = nullptr;  // carte du mode HA (70 px) ; nullptr : inchangée
    uint32_t couleur_carte = UIColor.INACTIVE;
    const char* droite = nullptr;       // épaule droite ; nullptr : masquée
    uint32_t couleur_droite = UIColor.TEXT_DIM;
    const char* nom = "";
    char ligne[40] = "";                // ligne d'état de la carte
    uint32_t couleur_ligne = UIColor.INACTIVE;
    bool agit = false;                  // un appui fait quelque chose
    bool actif = false;                 // allumé, ouvert, en lecture… (icône « on »)
};

// Minuteries d'une tuile : confirmation (option k, 3 s) et « OK » après « lancer » (1 s).
struct Minuterie {
    int r = -1;
    int t = -1;
    lv_timer_t* timer = nullptr;
};
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

// Un appui fait-il quelque chose ? (sinon le bouton de la tuile météo est masqué). Option
// r : aucun appui du tout. Une clim : celle du blueprint (option m), ou la sienne quand la
// tablette en a les réglages (clé crRT, ADR-0027 ; `clim_connue`). Un capteur : seulement
// celui de la section « Énergie » (option e, ADR-0028), qui ouvre son popup.
bool type_agit(Type type, uint8_t options, bool clim_connue) {
    if (options & OPT_R) return false;
    switch (type) {
        case Type::LUM: case Type::INT: case Type::VOL: case Type::MED: case Type::ACT: return true;
        case Type::CLI: return (options & OPT_M) != 0 || clim_connue;
        case Type::CAP: return (options & OPT_E) != 0;
        default: return false;
    }
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
    v.agit = type_agit(type, d.options, type == Type::CLI && r >= 0 && clim_tuile_connue(r, t));
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

void popup_lumiere_etat(int r, int t);
void popup_volet_etat(int r, int t);
void popup_appareil_etat(int r, int t);
// Roue d'actions rapides ouverte sur cette tuile : repeinte (boutons courants, moyeu, jauge).
void roue_tuile_etat(int r, int t);

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

// ─── Popup lumière : les lumières de la pièce (ADR-0023) ─────────────────────────────

struct PopupLumiere {
    int piece = 0;
    int n = 0;                 // lignes du sélecteur
    int tuiles[kTuiles] = {};  // tuile de chaque ligne, dans l'ordre des tuiles
    int choix = 0;             // ligne pilotée (current_light_slot)
};
PopupLumiere s_pl;

// Une lumière pilotable de la pièce : tuile lum sans option r ; en mode héritage,
// lumiere_1..3 (tuiles 2 à 4) que HA n'a pas déclarées absentes.
bool est_lumiere(int r, int t) {
    if (!tuile_presente(r, t)) return false;
    if (heritage()) return r == 0 && t >= 2;
    const Def& d = s_m.tuiles[r][t];
    return d.type == static_cast<uint8_t>(Type::LUM) && !(d.options & OPT_R);
}

bool lumiere_allumee(int r, int t) {
    return heritage() ? s_h.lum[t - 2] : est(s_etats[r][t].brut, "on");
}

float lumiere_luminosite(int r, int t) {
    return heritage() ? s_h.lum_val[t - 2] : s_etats[r][t].valeur;
}

const char* lumiere_nom(int r, int t) {
    if (!heritage()) return s_m.tuiles[r][t].nom;
    return t == 2 ? tr("Chambre") : (t == 3 ? tr("Salon") : "LEDs");
}

// Clé des commandes du popup : tRT, ou lumiere_N en mode héritage (commandes 3.x).
void lumiere_cle(int r, int t, std::string& out) {
    if (heritage()) {
        out = kHeritageLumieres[t - 2];
        return;
    }
    const char cle[4] = {'t', static_cast<char>('0' + r), static_cast<char>('0' + t), '\0'};
    out = cle;
}

bool popup_ouvert() {
    const lv_obj_t* p = g_tuiles_ui.lum_popup;
    return p != nullptr && !lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN);
}

// Arc et « NN % » de la ligne choisie ; pas pendant un glissement (le retour de HA
// ferait sauter le curseur sous le doigt). Éteinte, ou luminosité inconnue : 0 ; allumée,
// le % de la carte et de la roue (lum_pct).
void popup_lumiere_arc() {
    const TuilesUI& u = g_tuiles_ui;
    if (s_pl.n == 0 || u.lum_arc == nullptr || u.lum_pct == nullptr) return;
    if (lv_obj_has_state(u.lum_arc, LV_STATE_PRESSED)) return;
    const int r = s_pl.piece, t = s_pl.tuiles[s_pl.choix];
    const float v = lumiere_luminosite(r, t);
    const bool allumee = lumiere_allumee(r, t);
    const int arcv = allumee ? tab5_float_vers_int(v, 0, 255, 0) : 0;
    lv_arc_set_value(u.lum_arc, arcv);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d %%", allumee ? std::max(0, lum_pct(v)) : 0);
    ui_text(u.lum_pct, buf);
}

// Une ligne du sélecteur : icône (palette ou 3.1) dorée si allumée, nom coupé.
void popup_lumiere_ligne(int i) {
    const TuilesUI& u = g_tuiles_ui;
    const int r = s_pl.piece, t = s_pl.tuiles[i];
    const bool on = lumiere_allumee(r, t);
    const char* icone = heritage() ? heritage_glyphe_selecteur(t) : tuile_icone(s_m.tuiles[r][t].icone, on, "lum");
    ui_text(u.lum_sel_icone[i], icone);
    ui_text_color(u.lum_sel_icone[i], on ? UIColor.INFO : UIColor.TEXT_DIM);
    ui_texte_coupe(u.lum_sel_nom[i], lumiere_nom(r, t), 244);  // 342 − 88 − marge
}

// Tout le popup : lignes (serrées au-delà de trois), surbrillance, titre, bouton
// marche/arrêt et arc de la ligne choisie.
void popup_lumiere_peindre() {
    const TuilesUI& u = g_tuiles_ui;
    if (u.lum_popup == nullptr) return;
    const bool serre = s_pl.n > 3;
    const int32_t hauteur = serre ? 54 : 86, pas = serre ? 60 : 96;
    for (int i = 0; i < kTuiles; i++) {
        lv_obj_t* b = u.lum_sel[i];
        const bool visible = i < s_pl.n;
        ui_hidden(b, !visible);
        if (!visible || b == nullptr) continue;
        ui_y(b, 50 + i * pas);
        if (lv_obj_get_style_height(b, LV_PART_MAIN) != hauteur) lv_obj_set_height(b, hauteur);
        highlight_button_border(b, i == s_pl.choix, UIColor.ACCENT, 3);
        popup_lumiere_ligne(i);
    }
    if (s_pl.n == 0) return;
    const int r = s_pl.piece, t = s_pl.tuiles[s_pl.choix];
    if (heritage()) {
        const char* titre = t == 2 ? tr("Ampoule Chambre") : (t == 3 ? tr("Ampoule Salon") : tr("Ampoule LEDs"));
        ui_text(u.lum_titre, titre);
    } else {
        ui_text(u.lum_titre, lumiere_nom(r, t));
    }
    ui_text_color(u.lum_power, lumiere_allumee(r, t) ? UIColor.INFO : UIColor.TEXT_DIM);
    popup_lumiere_arc();
}

// Lumières de la pièce `r`, ligne choisie = celle de la tuile `t` (appui long).
void popup_lumiere_ouvrir(int r, int t) {
    const TuilesUI& u = g_tuiles_ui;
    if (u.lum_popup == nullptr) return;
    s_pl = PopupLumiere{};
    s_pl.piece = r;
    for (int i = 0; i < kTuiles; i++)
        if (est_lumiere(r, i)) {
            if (i == t) s_pl.choix = s_pl.n;
            s_pl.tuiles[s_pl.n++] = i;
        }
    if (s_pl.n == 0) return;
    if (u.lum_cle != nullptr) lumiere_cle(r, s_pl.tuiles[s_pl.choix], *u.lum_cle);
    popup_lumiere_peindre();
    animate_popup_open(u.lum_popup);
}

// Nouvelles définitions des tuiles (UI-1, audit du 07/10/2026), comme les popups volet et
// appareil : ouvert, les lignes de la même pièce sont recalculées — la lampe choisie le
// reste si elle est encore une lumière, sinon la première — puis le popup est repeint ;
// plus aucune lumière dans la pièce : refermé. Fermé (y compris par sa croix, que le C++
// ne voit pas), il est oublié : aucun index ne vise plus une tuile disparue.
void popup_lumiere_revalider() {
    const TuilesUI& u = g_tuiles_ui;
    if (!popup_ouvert()) {
        s_pl = PopupLumiere{};
        return;
    }
    const int r = s_pl.piece;
    const int choisie = s_pl.n > 0 ? s_pl.tuiles[s_pl.choix] : -1;
    s_pl = PopupLumiere{};
    s_pl.piece = r;
    for (int i = 0; i < kTuiles; i++)
        if (est_lumiere(r, i)) {
            if (i == choisie) s_pl.choix = s_pl.n;
            s_pl.tuiles[s_pl.n++] = i;
        }
    if (s_pl.n == 0) {
        animate_popup_close(u.lum_popup);
        return;
    }
    if (u.lum_cle != nullptr) lumiere_cle(r, s_pl.tuiles[s_pl.choix], *u.lum_cle);
    popup_lumiere_peindre();
}

// Un état a changé : la ligne de cette tuile si le popup la montre.
void popup_lumiere_etat(int r, int t) {
    if (!popup_ouvert() || r != s_pl.piece) return;
    const TuilesUI& u = g_tuiles_ui;
    for (int i = 0; i < s_pl.n; i++) {
        if (s_pl.tuiles[i] != t) continue;
        popup_lumiere_ligne(i);
        if (i == s_pl.choix) {
            ui_text_color(u.lum_power, lumiere_allumee(r, t) ? UIColor.INFO : UIColor.TEXT_DIM);
            popup_lumiere_arc();
        }
    }
}

// ─── Commandes ──────────────────────────────────────────────────────────────────────

void envoyer(const char* emplacement, const char* action) {
    if (g_tuiles_ui.envoyer != nullptr) g_tuiles_ui.envoyer(emplacement, action, "");
}

void envoyer_tuile(int r, int t, const char* action) {
    const char cle[4] = {'t', static_cast<char>('0' + r), static_cast<char>('0' + t), '\0'};
    envoyer(cle, action);
}

void ouvrir_popup(lv_obj_t* popup) {
    if (popup != nullptr) animate_popup_open(popup);
}

// ─── Popup du volet (05/10/2026, discussion #278) ───────────────────────────────────
//
// Appui long d'une tuile vol sans l'option k : un volet dessiné (06/10/2026, même
// discussion : « like ha animation, not a basic slider ») qu'on fait glisser du doigt
// (position connue seulement, envoyée au relâcher : action « position »), la position en
// grand, l'état en mots et Ouvrir / Stop / Fermer (les commandes de la tuile). Tant qu'il
// est ouvert, il suit l'état de SA tuile (peindre_tuile → popup_volet_etat) : chaque
// position poussée par HA redessine le tablier directement, sans interpolation ni fondu
// (préférence d'Axel : transitions instantanées) — le volet dessiné descend quand le vrai
// descend, au rythme des poussées.

struct PopupVolet {
    int piece = -1;
    int tuile = -1;
    bool saisi = false;   // un doigt tient le volet (position connue à l'appui)
    bool glisse = false;  // il a glissé au-delà du seuil : son relâcher envoie la position
    bool cible = false;   // position envoyée : dessinée jusqu'au prochain état de HA
    bool estompe = false; // position dessinée = un repère (position inconnue)
    int32_t y_appui = 0;  // ordonnée du doigt à l'appui (écran)
    int pos_appui = 0;    // position dessinée à l'appui
    int pos = 0;          // position dessinée : 0 fermé, 100 ouvert
};
PopupVolet s_pv;

// Géométrie (volet_popup.yaml) : l'état sous la position, ou seul au milieu de la carte
// (598 px de haut, 53 px de texte) ; titre : la barre d'en-tête moins l'icône et la croix.
constexpr int32_t kVoletEtatSous = 360;
constexpr int32_t kVoletEtatSeul = 272;
constexpr int32_t kVoletTitreLargeur = 1000;
// Volet dessiné : hauteur de la fenêtre et du tablier (volet_fenetre, volet_tablier de
// volet_popup.yaml, tests/test_tuiles_firmware.py compare), lames de 38 px.
constexpr int32_t kVoletFenetreH = 456;
constexpr int32_t kVoletLameH = 38;
constexpr int kVoletLames = kVoletFenetreH / kVoletLameH;
static_assert(kVoletLames * kVoletLameH == kVoletFenetreH, "les lames couvrent la fenêtre");
// Un doigt qui bouge de moins que ça n'a pas glissé : un toucher n'envoie rien (pas
// d'ordre de plus à un volet en route).
constexpr int32_t kVoletSeuilGlisse = 12;
// Position inconnue, ni ouvert ni fermé (partiel, en mouvement, hors ligne) : le tablier
// à mi-hauteur, lames estompées — un repère, pas une mesure.
constexpr int kVoletMilieu = 50;

// Style partagé des lames, créé avec elles (lames_construire). Ses couleurs viennent de
// la palette active et sont reposées par popup_volet_dessiner quand elles changent
// (thème, volet estompé) : un seul lv_obj_report_style_change, pas un par lame.
lv_style_t s_lame;
bool s_lame_pret = false;
uint32_t s_lame_fond = 0;
uint32_t s_lame_joint = 0;
lv_opa_t s_lame_opa = LV_OPA_COVER;

bool popup_volet_ouvert() {
    const lv_obj_t* p = g_tuiles_ui.vol_popup;
    return p != nullptr && !lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN);
}

// La tuile du popup est-elle toujours un volet pilotable ? (les définitions peuvent
// changer popup ouvert ; le mode héritage n'ouvre jamais ce popup.)
bool popup_volet_valide() {
    const int r = s_pv.piece, t = s_pv.tuile;
    if (r < 0 || r >= kPieces || t < 0 || t >= kTuiles || heritage()) return false;
    const Def& d = s_m.tuiles[r][t];
    return d.type == static_cast<uint8_t>(Type::VOL) && !(d.options & OPT_R);
}

// Position 0-100 connue : un état en ligne et une valeur dans les bornes. NaN : le volet
// n'en donne pas ; -1 : « Partiel » du volet à course simulée (arrêté en route).
bool vol_position_connue(const Etat& e) {
    if (!e.recu || etat_indisponible(e.brut)) return false;
    return !std::isnan(e.valeur) && e.valeur >= 0.0f && e.valeur <= 100.0f;
}

// Position à dessiner : la vraie si elle est connue ; sinon l'état — fermé en bas, ouvert
// en haut, le reste (partiel, en mouvement, hors ligne, rien reçu) à mi-hauteur, estompé.
int vol_position_dessin(const Etat& e, bool& estompe) {
    estompe = false;
    if (vol_position_connue(e)) return tab5_float_vers_int(e.valeur, 0, 100, 0);
    if (e.recu && est(e.brut, "closed")) return 0;
    // « open » sans position (NaN) : ouvert ; avec -1 (volet à course simulée) : partiel.
    if (e.recu && est(e.brut, "open") && !(e.valeur < 0.0f)) return 100;
    estompe = true;
    return kVoletMilieu;
}

// L'état en mots, dans les couleurs de la tuile. Ouvert mais arrêté en route (-1 du
// volet à course simulée, ou une position entre les deux bouts) : « Partiel ».
const char* vol_etat_mots(const Etat& e, uint32_t& couleur) {
    couleur = UIColor.INACTIVE;
    if (!e.recu) return "--";
    if (etat_indisponible(e.brut)) return tr("Hors ligne");
    if (vol_mouvement(e.brut)) {
        couleur = UIColor.INFO;
        return tr("En mouvement");
    }
    if (est(e.brut, "closed")) {
        couleur = UIColor.TEXT_DIM;
        return tr("Fermé");
    }
    couleur = UIColor.SUCCESS;
    if (e.valeur < 0.0f || (e.valeur > 0.0f && e.valeur < 100.0f)) return tr("Partiel");
    return tr("Ouvert");
}

void popup_volet_nombre(int pos) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%d", pos);
    ui_text(g_tuiles_ui.vol_nombre, buf);
}

// Builder des lames (règle 5) : 12 lames de 38 px empilées dans le tablier, chacune un
// aplat d'accent et un joint de 4 px en bas, toutes sur le style partagé s_lame. La
// dernière (en bas) fait le bord du tablier. Non cliquables : le toucher va au cadre.
void lames_construire(lv_obj_t* tablier) {
    lv_style_init(&s_lame);
    lv_style_set_radius(&s_lame, 0);
    lv_style_set_pad_all(&s_lame, 0);
    lv_style_set_bg_opa(&s_lame, LV_OPA_COVER);
    lv_style_set_border_side(&s_lame, LV_BORDER_SIDE_BOTTOM);
    lv_style_set_border_width(&s_lame, 4);
    lv_style_set_border_opa(&s_lame, LV_OPA_60);
    s_lame_fond = UIColor.ACCENT;
    s_lame_joint = UIColor.GLASS_LO;
    s_lame_opa = LV_OPA_COVER;
    lv_style_set_bg_color(&s_lame, lv_color_hex(s_lame_fond));
    lv_style_set_border_color(&s_lame, lv_color_hex(s_lame_joint));
    s_lame_pret = true;
    for (int i = 0; i < kVoletLames; i++) {
        lv_obj_t* lame = lv_obj_create(tablier);
        lv_obj_remove_style_all(lame);
        lv_obj_add_style(lame, &s_lame, LV_PART_MAIN);
        lv_obj_remove_flag(lame, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_remove_flag(lame, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_pos(lame, 0, i * kVoletLameH);
        lv_obj_set_size(lame, lv_pct(100), kVoletLameH);
    }
}

// Le volet dessiné à `pos` (0 fermé, 100 ouvert) : le tablier remonte de pos % de la
// fenêtre, qui le rogne (à 100 % il est rentré dans le coffre). Lames pleines, ou
// estompées quand la position n'est qu'un repère.
void popup_volet_dessiner(int pos, bool estompe) {
    ui_y(g_tuiles_ui.vol_tablier, -(std::clamp(pos, 0, 100) * kVoletFenetreH) / 100);
    if (!s_lame_pret) return;
    const uint32_t fond = UIColor.ACCENT;
    const uint32_t joint = UIColor.GLASS_LO;
    const lv_opa_t opa = estompe ? LV_OPA_40 : LV_OPA_COVER;
    if (fond == s_lame_fond && joint == s_lame_joint && opa == s_lame_opa) return;
    s_lame_fond = fond;
    s_lame_joint = joint;
    s_lame_opa = opa;
    lv_style_set_bg_color(&s_lame, lv_color_hex(fond));
    lv_style_set_border_color(&s_lame, lv_color_hex(joint));
    lv_style_set_bg_opa(&s_lame, opa);
    lv_obj_report_style_change(&s_lame);
}

// Titre, volet dessiné, position (rangée « 45 % », ou rien), état en mots.
void popup_volet_peindre() {
    const TuilesUI& u = g_tuiles_ui;
    if (u.vol_popup == nullptr || !popup_volet_valide()) return;
    const int r = s_pv.piece, t = s_pv.tuile;
    const Etat& e = s_etats[r][t];
    ui_texte_coupe(u.vol_titre, s_m.tuiles[r][t].nom, kVoletTitreLargeur);
    const bool connue = vol_position_connue(e);
    ui_hidden(u.vol_position, !connue);
    // Pas pendant un glissement : le retour de HA ferait sauter le volet sous le doigt
    // (le dessin et le nombre suivent alors le doigt, volet_cadre_rappel). Ni après le
    // relâcher, tant que HA n'a pas poussé d'état (tuiles_etat_recu) : un repeint de
    // thème ou de définitions garde la position envoyée, comme un démarrage à froid.
    if (!s_pv.saisi && !s_pv.cible) s_pv.pos = vol_position_dessin(e, s_pv.estompe);
    // Toujours redessiné : les couleurs des lames suivent la palette (thème).
    popup_volet_dessiner(s_pv.pos, s_pv.estompe);
    if (connue) popup_volet_nombre(s_pv.pos);
    uint32_t couleur = UIColor.INACTIVE;
    ui_text(u.vol_etat, vol_etat_mots(e, couleur));
    ui_text_color(u.vol_etat, couleur);
    ui_y(u.vol_etat, connue ? kVoletEtatSous : kVoletEtatSeul);
}

void popup_volet_ouvrir(int r, int t) {
    const TuilesUI& u = g_tuiles_ui;
    if (u.vol_popup == nullptr) return;
    s_pv = PopupVolet{};
    s_pv.piece = r;
    s_pv.tuile = t;
    popup_volet_peindre();
    animate_popup_open(u.vol_popup);
}

// Un état a changé : le popup, s'il montre cette tuile.
void popup_volet_etat(int r, int t) {
    if (popup_volet_ouvert() && r == s_pv.piece && t == s_pv.tuile) popup_volet_peindre();
}

// Relâcher après un glissement : « position » + 0-100 à la tuile du popup (événement
// esphome.tab5_action ; le blueprint la passe à cover / valve.set_…_position de l'entité
// de CETTE tuile, quand elle sait le faire). Rien sans position connue au relâcher (le
// volet a pu passer hors ligne pendant le geste) : vrai si la position est partie.
bool popup_volet_envoyer_position() {
    const TuilesUI& u = g_tuiles_ui;
    if (!popup_volet_valide() || u.envoyer == nullptr) return false;
    if (!vol_position_connue(s_etats[s_pv.piece][s_pv.tuile])) return false;
    char valeur[8];
    snprintf(valeur, sizeof(valeur), "%d", std::clamp(s_pv.pos, 0, 100));
    const char cle[4] = {'t', static_cast<char>('0' + s_pv.piece), static_cast<char>('0' + s_pv.tuile), '\0'};
    u.envoyer(cle, "position", valeur);
    s_pv.cible = true;
    return true;
}

// Cadre du volet dessiné : on attrape le tablier n'importe où et on le tire. Son bord suit
// le doigt (vers le bas, il descend : la position baisse) ; seul le relâcher envoie (un
// seul set_position par geste), et seulement après un vrai glissement (kVoletSeuilGlisse) :
// un toucher n'envoie rien. Position inconnue à l'appui : rien ne glisse. PRESS_LOST
// aussi : un doigt perdu en route relâche ailleurs. Après un toucher, ou un relâcher qui
// n'envoie rien, le dessin reprend l'état de HA (popup_volet_peindre).
void volet_cadre_rappel(lv_event_t* ev) {
    lv_point_t p = {0, 0};
    lv_indev_t* indev = lv_indev_active();
    if (indev != nullptr) lv_indev_get_point(indev, &p);
    switch (lv_event_get_code(ev)) {
        case LV_EVENT_PRESSED:
            s_pv.glisse = false;
            s_pv.saisi = popup_volet_valide() && vol_position_connue(s_etats[s_pv.piece][s_pv.tuile]);
            s_pv.y_appui = p.y;
            s_pv.pos_appui = s_pv.pos;
            if (s_pv.saisi) s_pv.estompe = false;
            break;
        case LV_EVENT_PRESSING: {
            if (!s_pv.saisi) break;
            const int32_t dy = p.y - s_pv.y_appui;
            if (!s_pv.glisse && std::abs(dy) < kVoletSeuilGlisse) break;
            s_pv.glisse = true;
            const int pos = std::clamp(s_pv.pos_appui - static_cast<int>(dy * 100 / kVoletFenetreH), 0, 100);
            if (pos == s_pv.pos) break;
            s_pv.pos = pos;
            popup_volet_dessiner(pos, false);
            popup_volet_nombre(pos);
            break;
        }
        case LV_EVENT_RELEASED:
        case LV_EVENT_PRESS_LOST: {
            const bool envoyer = s_pv.saisi && s_pv.glisse;
            s_pv.saisi = false;
            s_pv.glisse = false;
            if (envoyer && popup_volet_envoyer_position()) break;
            popup_volet_peindre();
            break;
        }
        default:
            break;
    }
}

// ─── Popup d'un appareil (06/10/2026, discussion #278) ──────────────────────────────
//
// « Buttons can have pop up screen like ha dashboard » : l'appui long d'une tuile qui
// n'avait pas de popup (int, act, med sans l'option t) ouvre, pour cette tuile, la
// fenêtre « plus d'infos » d'un tableau de bord HA : son icône dans une pastille ronde
// de la couleur de son état, l'état en mots, sa pièce, ses options, et un grand bouton
// qui fait EXACTEMENT ce que fait le toucher de la tuile (tuile_appui_piece) : même
// commande, même confirmation (option k : la minuterie de la tuile, la tuile ET le popup
// demandent « Confirmer ? »), même « OK » après « lancer ». Il ne montre que ce que HA
// pousse déjà pour les tuiles (définition, état) : rien d'inventé. Tant qu'il est
// ouvert, il suit l'état de SA tuile (peindre_tuile → popup_appareil_etat).

struct PopupAppareil {
    int piece = -1;
    int tuile = -1;
};
PopupAppareil s_pa;

// Géométrie (appareil_popup.yaml) : titre = la barre d'en-tête moins l'icône et la
// croix ; textes de la carte ÉTAT (800 px) et de la carte COMMANDE (386 px) ; grand
// bouton de 380 px, rempli à moitié (allumé : en haut ; éteint : en bas) ou en entier
// (scène, script, bouton).
constexpr int32_t kAppareilTitreLargeur = 1000;
constexpr int32_t kAppareilTexteLargeur = 740;
constexpr int32_t kAppareilActionLargeur = 350;
constexpr int32_t kAppareilBoutonHauteur = 380;

// Les types qui ont ce popup : int, act, et med sans l'option t (avec t, la télécommande).
bool a_popup_appareil(Type type, uint8_t options) {
    switch (type) {
        case Type::INT: case Type::ACT: return true;
        case Type::MED: return (options & OPT_T) == 0;
        default: return false;
    }
}

bool popup_appareil_ouvert() {
    const lv_obj_t* p = g_tuiles_ui.app_popup;
    return p != nullptr && !lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN);
}

// La tuile du popup a-t-elle toujours ce popup ? (les définitions peuvent changer popup
// ouvert ; option r : jamais ; le mode héritage n'ouvre jamais ce popup.)
bool popup_appareil_valide() {
    const int r = s_pa.piece, t = s_pa.tuile;
    if (r < 0 || r >= kPieces || t < 0 || t >= kTuiles || heritage()) return false;
    const Def& d = s_m.tuiles[r][t];
    return !(d.options & OPT_R) && a_popup_appareil(static_cast<Type>(d.type), d.options);
}

// Glyphe du grand bouton (mdi_font_45, règle 9) : lecture pour une scène, un script, un
// bouton ; marche / arrêt sinon.
const char* glyphe_commande(bool lancer) {
    return lancer ? "\U000F040A" : "\U000F0425";
}

// Tout le popup, depuis la définition et l'état de sa tuile (vue_def : les mêmes mots,
// icône et couleurs que la carte du mode HA).
void popup_appareil_peindre() {
    const TuilesUI& u = g_tuiles_ui;
    if (u.app_popup == nullptr || !popup_appareil_valide()) return;
    const int r = s_pa.piece, t = s_pa.tuile;
    const Def& d = s_m.tuiles[r][t];
    const Etat& e = s_etats[r][t];
    const Type type = static_cast<Type>(d.type);
    Vue v;
    vue_def(d, e, r, t, v);
    const bool lancer = type == Type::ACT;
    const bool confirmer = minuterie_sur(s_confirmation, r, t);
    const bool ok = minuterie_sur(s_ok, r, t);
    ui_texte_coupe(u.app_titre, d.nom, kAppareilTitreLargeur);
    // Pastille : l'icône de la tuile en couleur pleine sur sa couleur à 20 % (le YAML).
    if (v.icone_carte != nullptr) ui_text(u.app_icone, v.icone_carte);
    ui_text_color(u.app_icone, v.couleur_carte);
    ui_fond(u.app_pastille, v.couleur_carte);
    // État en mots : la ligne d'état de la carte, sauf « Lancer » (une action, pas un
    // état) : un script en route (« on ») dit « En cours », le reste « Prêt ».
    const char* etat = v.ligne;
    if (lancer && std::strcmp(v.ligne, tr("Lancer")) == 0) etat = est(e.brut, "on") ? tr("En cours") : tr("Prêt");
    ui_texte_coupe(u.app_etat, etat, kAppareilTexteLargeur);
    ui_text_color(u.app_etat, v.couleur_ligne);
    // La pièce : son nom, ou « Pièce n » quand HA n'en donne pas.
    char buf[64];
    if (s_m.pieces[r][0] != '\0') snprintf(buf, sizeof(buf), tr("Pièce : %s"), s_m.pieces[r]);
    else snprintf(buf, sizeof(buf), tr("Pièce %d"), r + 1);
    ui_texte_coupe(u.app_piece, buf, kAppareilTexteLargeur);
    // Options de la tuile (blueprint, « Personnaliser des tuiles »), une par ligne.
    char options[96] = "";
    if ((d.options & OPT_O) && !lancer) snprintf(options, sizeof(options), "%s", tr("Allumer seulement"));
    if (d.options & OPT_K) {
        const size_t n = std::strlen(options);
        snprintf(options + n, sizeof(options) - n, "%s%s", n > 0 ? "\n" : "", tr("Confirmer chaque commande"));
    }
    ui_text(u.app_options, options);
    ui_hidden(u.app_options, options[0] == '\0');
    // Grand bouton : rempli en haut quand l'appareil est allumé, en bas sinon, en entier
    // pour une scène ; dans la couleur de la carte (ambre pendant une confirmation).
    lv_obj_t* f = u.app_remplissage;
    const int32_t h = lancer ? kAppareilBoutonHauteur : kAppareilBoutonHauteur / 2;
    if (f != nullptr && lv_obj_get_style_height(f, LV_PART_MAIN) != h) lv_obj_set_height(f, h);
    ui_y(f, (lancer || v.actif) ? 0 : kAppareilBoutonHauteur - h);
    ui_fond(f, v.couleur_carte);
    ui_text(u.app_commande_icone, glyphe_commande(lancer));
    ui_text_color(u.app_commande_icone, v.couleur_carte);
    // Ce que fera l'appui : la commande de la tuile (basculer, allumer avec l'option o,
    // lancer), dite en mots ; ambre quand elle attend sa confirmation.
    const char* action = tr("Éteindre");
    if (lancer) action = ok ? "OK" : tr("Lancer");
    else if ((d.options & OPT_O) || !v.actif) action = tr("Allumer");
    uint32_t c_action = UIColor.ACCENT;
    if (confirmer) c_action = UIColor.WARNING;
    else if (ok) c_action = UIColor.SUCCESS;
    else if (!lancer && (d.options & OPT_O) && v.actif) c_action = UIColor.TEXT_DIM;  // déjà allumé
    ui_texte_coupe(u.app_action, action, kAppareilActionLargeur);
    ui_text_color(u.app_action, c_action);
}

void popup_appareil_ouvrir(int r, int t) {
    const TuilesUI& u = g_tuiles_ui;
    if (u.app_popup == nullptr) return;
    s_pa = PopupAppareil{};
    s_pa.piece = r;
    s_pa.tuile = t;
    popup_appareil_peindre();
    animate_popup_open(u.app_popup);
}

// Un état ou une minuterie a changé : le popup, s'il montre cette tuile.
void popup_appareil_etat(int r, int t) {
    if (popup_appareil_ouvert() && r == s_pa.piece && t == s_pa.tuile) popup_appareil_peindre();
}

// Mode héritage : les gestes de la 3.1 (tuile 0 PC + télécommande, 1 volet, 2-4 lumières).
void appui_heritage(int t, bool long_appui) {
    switch (t) {
        case 0:
            if (!long_appui) envoyer("pc", "basculer");
            else if (!zone_absente(Zone::TV)) ouvrir_popup(g_tuiles_ui.popup_tv);
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

// Définitions de la rangée lues dans tab5_maj_tuiles : gardées et redessinées si elles
// changent. Un élément qui change d'appareil repart grisé (son état était l'ancien).
bool rangee_definir(const ModeleRangee& neuf) {
    if (std::memcmp(&neuf, &s_rg, sizeof(ModeleRangee)) == 0) return false;
    int n = 0;
    for (int l = 0; l < kLignes; l++)
        for (int i = 0; i < kElements; i++) {
            if (std::memcmp(&neuf.el[l][i], &s_rg.el[l][i], sizeof(DefRangee)) != 0) etat_vider(s_etats_rg[l][i]);
            if (neuf.el[l][i].d.type != static_cast<uint8_t>(Type::VIDE)) n++;
        }
    s_rg = neuf;
    s_pref_rg.save(&s_rg);
    ESP_LOGI("tab5.tuiles", "Rangée sous l'horloge : %d élément(s), plantes %d, %d tour(s) par ligne", n,
             s_rg.plantes + 1, s_rg.tours);
    rangee_definitions_changees();
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

}  // namespace

TuilesUI g_tuiles_ui;

// Pour les autres unités (titre du popup clim, ADR-0026) : un nom venu de HA suit les
// mêmes règles que ceux des tuiles. Déclarées dans tab5_internal.h.
void texte_ha_copier(char* dst, size_t cap, const char* src, size_t n) { copier_texte(dst, cap, src, n); }
void texte_ha_coupe(lv_obj_t* lbl, const char* txt, int32_t largeur) { ui_texte_coupe(lbl, txt, largeur); }

bool tuiles_definir(const std::string& payload) {
    charger();
    // Tuile − / + au choix (ADR-0033) : ses clés rN sont dans le même instantané.
    const bool reglables_changes = reglables_definir(payload);
    // Instantané complet : ce qui n'est pas listé est vide. Construit à part (tas, le temps
    // de la comparaison), pour n'écrire la NVS et ne redessiner que si quelque chose change.
    std::unique_ptr<Modele> neuf(new Modele());
    neuf->magic = kMagic;
    neuf->recues = 1;
    // La rangée sous l'horloge aussi (ADR-0031) : sans clé h, les plantes seules, en
    // première ligne — un blueprint d'avant la rangée garde l'écran d'avant.
    std::unique_ptr<ModeleRangee> rangee(new ModeleRangee());
    rangee->magic = kMagicRangee;
    rangee->tours = kToursDefaut;
    size_t debut = 0;
    while (debut < payload.size()) {
        size_t fin = payload.find(';', debut);
        if (fin == std::string::npos) fin = payload.size();
        lire_entree(payload.data() + debut, fin - debut, *neuf, *rangee);
        debut = fin + 1;
    }
    const bool rangee_changee = rangee_definir(*rangee);
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
    // Popup du volet ouvert sur une tuile qui n'est plus un volet pilotable : refermé.
    if (popup_volet_ouvert()) {
        if (popup_volet_valide()) popup_volet_peindre();
        else animate_popup_close(g_tuiles_ui.vol_popup);
    }
    // Même chose pour le popup d'un appareil (tuile devenue lecture seule, autre type…).
    if (popup_appareil_ouvert()) {
        if (popup_appareil_valide()) popup_appareil_peindre();
        else animate_popup_close(g_tuiles_ui.app_popup);
    }
    // Et le popup lumière : ses lignes étaient des index de tuiles de l'ancienne définition.
    popup_lumiere_revalider();
    tuiles_appliquer_ui();
    return true;
}

bool tuiles_etat_recu(const char* cle, size_t n_cle, const char* reste, size_t n_reste) {
    int r = 0, t = 0;
    if (n_cle != 3) return false;
    // Élément I de la ligne L de la rangée sous l'horloge (ADR-0031).
    if (cle[0] == 'h') {
        if (!chiffre_0_4(cle[1], r) || r >= kLignes || !chiffre_0_4(cle[2], t) || t >= kElements) return false;
        charger();
        etat_lire(s_etats_rg[r][t], reste, n_reste);
        rangee_element_change(r, t);
        return true;
    }
    if (cle[0] != 't' || !chiffre_0_4(cle[1], r) || !chiffre_0_4(cle[2], t)) return false;
    charger();
    Etat& e = s_etats[r][t];
    etat_lire(e, reste, n_reste);
    if (s_m.tuiles[r][t].type == static_cast<uint8_t>(Type::VOL)) vol_sens_suivre(e);
    // Popup du volet : un état poussé remplace la position envoyée au relâcher.
    if (r == s_pv.piece && t == s_pv.tuile) s_pv.cible = false;
    if (!heritage()) peindre_tuile(r, t);
    return true;
}

// ─── Rangée sous l'horloge (ADR-0031), lue par tab5_rangee.cpp ──────────────────────

int rangee_place_plantes() {
    charger();
    return s_rg.plantes;
}

int rangee_tours() {
    charger();
    return std::max<int>(1, s_rg.tours);
}

bool rangee_ligne_remplie(int l) {
    charger();
    if (l < 0 || l >= kLignes) return false;
    for (const DefRangee& g : s_rg.el[l])
        if (g.d.type != static_cast<uint8_t>(Type::VIDE)) return true;
    return false;
}

// Un élément : l'icône et sa couleur comme sur une tuile (vue_def) ; un capteur ou une
// clim y ajoute sa valeur. Celle d'un capteur est écrite court (« 21.4 ° », « 2.4 kW »)
// et colorée selon sa nature (classe d'appareil) : l'échelle des températures de
// l'écran, celle de l'humidité, celle des batteries, l'or pour la puissance et l'énergie.
// Les tuiles gardent leurs couleurs (« INFO » pour un capteur qui n'est pas en °).
bool rangee_element(int l, int i, RangeeElement& out) {
    charger();
    out = RangeeElement{};
    if (l < 0 || l >= kLignes || i < 0 || i >= kElements) return false;
    const DefRangee& g = s_rg.el[l][i];
    const Type type = static_cast<Type>(g.d.type);
    if (type == Type::VIDE) return false;
    const Etat& e = s_etats_rg[l][i];
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
    if (s_pl.n > 0) popup_lumiere_peindre();
    if (popup_volet_ouvert()) popup_volet_peindre();
    if (popup_appareil_ouvert()) popup_appareil_peindre();
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
}

// Mode HA : la page suivante dans l'ordre de la météo, pièce vide comprise (elle dit
// « Aucun appareil ») — comme les cinq pages de prévisions. Avant le 28/09, les pièces
// vides étaient sautées : avec une seule pièce configurée, le swipe ne faisait rien.
void tuiles_swipe_ha(bool gauche) {
    charger();
    aller_page(forecast_page_suivante(g_central_ctx.forecast_page, gauche));
    peindre_cartes();
    central_mode_ha(g_tuiles_ui.titre_cadre, g_tuiles_ui.titre, g_central_ctx);
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

// Roue d'actions rapides de la tuile T de la page courante (ADR-0036, plus bas).
static bool roue_de_la_tuile(int r, int t);

// Appui sur la tuile T de la pièce R : tuile de la page courante (tuile_appui), ou grand
// bouton du popup d'un appareil (popup_appareil_appui, appui court). Un seul chemin pour
// les deux : la même commande, la même confirmation (option k), le même « OK ».
static void tuile_appui_piece(int r, int t, bool long_appui) {
    if (!tuile_presente(r, t)) return;
    if (heritage()) {
        appui_heritage(t, long_appui);
        return;
    }
    const Def& d = s_m.tuiles[r][t];
    const Etat& e = s_etats[r][t];
    const Type type = static_cast<Type>(d.type);
    // cap sans e, bin, option r, cli sans m dont la tablette n'a pas les réglages
    if (!type_agit(type, d.options, type == Type::CLI && clim_tuile_connue(r, t))) return;
    // Tableau de l'ADR-0023 : appui court, puis appui long.
    const char* action = nullptr;
    switch (type) {
        case Type::LUM:
            // Appui long : la roue d'actions rapides (ADR-0036) ; sans elle (option k), le
            // popup des lumières de la pièce, comme avant.
            if (long_appui) {
                if (!roue_de_la_tuile(r, t)) popup_lumiere_ouvrir(r, t);
                return;
            }
            action = (d.options & OPT_O) ? "allumer" : "basculer";
            break;
        case Type::INT:
            // Appui long (06/10/2026, discussion #278) : le popup de l'appareil.
            if (long_appui) {
                popup_appareil_ouvrir(r, t);
                return;
            }
            action = (d.options & OPT_O) ? "allumer" : "basculer";
            break;
        case Type::VOL:
            // Appui long (05/10/2026, discussion #278) : le popup du volet. Avec l'option
            // k, l'ancien appui long (l'autre sens, confirmé) : le popup ne doit jamais
            // contourner la confirmation.
            // Depuis le 07/10/2026 (ADR-0036), la roue d'actions rapides d'abord, dont
            // « Détails » ouvre ce popup ; l'option k ne l'ouvre jamais non plus.
            if (long_appui && !(d.options & OPT_K)) {
                if (!roue_de_la_tuile(r, t)) popup_volet_ouvrir(r, t);
                return;
            }
            action = long_appui ? vol_appui_long(e) : vol_appui(e);
            break;
        case Type::MED:
            // Appui long : la télécommande de la TV du blueprint (option t), sinon le popup
            // de l'appareil (06/10/2026).
            if (long_appui) {
                if (d.options & OPT_T) ouvrir_popup(g_tuiles_ui.popup_tv);
                else popup_appareil_ouvrir(r, t);
                return;
            }
            action = (d.options & OPT_O) ? "allumer" : "basculer";
            break;
        case Type::ACT:
            if (long_appui) {
                popup_appareil_ouvrir(r, t);
                return;
            }
            action = "lancer";
            break;
        case Type::CLI:
            // Option m : la clim du blueprint ; sinon celle de la tuile (ADR-0027), que
            // type_agit sait connue. Le popup revient à la clim du blueprint à sa fermeture.
            // Appui long (ADR-0036) : la roue d'actions rapides ; sans elle (aucune
            // capacité reçue), ce popup, comme l'appui court.
            if (long_appui && roue_de_la_tuile(r, t)) return;
            if (d.options & OPT_M) clim_afficher_blueprint();
            else if (!clim_afficher_tuile(r, t)) return;
            ouvrir_popup(g_tuiles_ui.popup_clim);
            return;
        case Type::CAP:
            // Option e (type_agit) : le popup Énergie, au toucher comme à l'appui long.
            if (g_tuiles_ui.energie_ouvrir != nullptr) g_tuiles_ui.energie_ouvrir();
            return;
        default:
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
bool tuile_ouvrir_popup(int r, int t) {
    charger();
    if (heritage() || !tuile_presente(r, t)) return false;
    const Def& d = s_m.tuiles[r][t];
    if (d.options & OPT_R) return false;
    switch (static_cast<Type>(d.type)) {
        case Type::LUM:
            popup_lumiere_ouvrir(r, t);
            return true;
        case Type::VOL:
            if (d.options & OPT_K) return false;
            popup_volet_ouvrir(r, t);
            return true;
        case Type::MED:
            if (!(d.options & OPT_T)) return false;
            ouvrir_popup(g_tuiles_ui.popup_tv);
            return true;
        case Type::CLI:
            if (d.options & OPT_M) clim_afficher_blueprint();
            else if (!clim_afficher_tuile(r, t)) return false;
            ouvrir_popup(g_tuiles_ui.popup_clim);
            return true;
        default:
            return false;
    }
}

// ─── Roue d'actions rapides (ADR-0036, 07/10/2026, discussion #278) ─────────────────
//
// L'appui long d'une lum, d'un vol ou d'une cli ouvre la roue (tab5_roue.cpp). Premier
// anneau : « Maison » (le popup de toutes les pièces, sauf quand la roue s'ouvre depuis
// lui), les commandes de la tuile et ses familles de réglages, puis « Détails » (le popup
// complet de son appui long d'avant, tuile_ouvrir_popup). Toucher une famille déplie ses
// choix sur le second anneau : luminosités, blancs et couleurs d'une lampe, positions d'un
// volet, modes, consignes et options d'une clim. Aucune commande nouvelle (mêmes
// événements esphome.tab5_action, mêmes valeurs que la tuile et ses popups : ADR-0023,
// ADR-0026, ADR-0027), aucune mise à jour optimiste nouvelle. Un choix ferme la roue.

namespace {

enum class RoueAction : uint8_t {
    MAISON, REGLAGES, ALLUMER, ETEINDRE, OUVRIR, ARRETER, FERMER, CLIM_ARRET,
    LUMINOSITE, BLANCS, COULEURS, POSITION, MODE, CONSIGNE, OPTIONS,
};

// Modes d'une clim offerts par la roue, dans cet ordre : lettre de capacité (ADR-0026),
// mode envoyé (commande « mode », comme les boutons du popup), icône. « auto » et
// « heat_cool » n'ont ni lettre ni commande (ADR-0026) : la roue ne les offre pas.
struct RoueModeClim {
    char lettre;
    const char* mode;
    RoueIcone icone;
};
constexpr RoueModeClim kRoueModesClim[] = {
    {'h', "heat", RoueIcone::CHAUFFER},
    {'c', "cool", RoueIcone::REFROIDIR},
    {'d', "dry", RoueIcone::SECHER},
    {'f', "fan_only", RoueIcone::VENTILER},
};
// Luminosités offertes (commande luminosite_pct, comme les raccourcis du popup lumière).
constexpr uint8_t kRoueLuminosites[] = {10, 25, 50, 75, 100};
// Positions offertes à un volet qui donne la sienne (commande position, comme le volet
// dessiné du popup).
constexpr uint8_t kRouePositions[] = {25, 50, 75};
// Pastilles d'une lampe à couleur (option c, commande couleur) : le nom envoyé à HA
// (color_name), la couleur montrée (celles du popup lumière) et, pour un blanc, son mot.
struct RouePastille {
    const char* nom;
    uint32_t couleur;
    const char* legende;
};
constexpr RouePastille kRoueBlancs[] = {
    {"warmwhite", 0xFFC864, tr_noop("Chaud")},
    {"navajowhite", 0xFFDEAD, tr_noop("Crème")},
    {"white", 0xFFFFFF, tr_noop("Froid")},
};
constexpr RouePastille kRoueCouleurs[] = {
    {"red", 0xFF2020, nullptr},  {"orange", 0xFFA500, nullptr}, {"gold", 0xFFD700, nullptr},
    {"green", 0x22C55E, nullptr}, {"blue", 0x3B82F6, nullptr},  {"purple", 0xA855F7, nullptr},
};

// La roue ouverte : sa tuile, son ancre, sa vue d'origine et ce que fait chaque bouton.
struct RoueTuile {
    int r = -1;
    int t = -1;
    lv_obj_t* ancre = nullptr;
    bool depuis_maison = false;
    int n = 0;
    RoueAction action[kRoueBoutons] = {};
};
RoueTuile s_rt;

// Ce qu'envoie un choix du second anneau (à l'emplacement de la tuile, « clim » pour la
// clim du blueprint).
struct RoueEnvoi {
    const char* commande = nullptr;
    char valeur[16] = "";
};

// Clim de la tuile tRT pour tab5_cards.cpp : -1, -1 pour celle du blueprint (option m).
void roue_clim(const Def& d, int r, int t, int& rc, int& tc) {
    const bool blueprint = (d.options & OPT_M) != 0;
    rc = blueprint ? -1 : r;
    tc = blueprint ? -1 : t;
}

// Boutons du premier anneau de la tuile tRT dans `b` (leurs actions dans `rt`) :
// « Maison » d'abord (sauf depuis lui), « Détails » en dernier. 0 sans roue : type sans
// roue, option r, option k (une lampe ou un volet à confirmer garde son appui long
// d'avant), clim sans capacité reçue, aucune commande. Le bouton de l'état courant (lampe
// allumée ou éteinte, volet ouvert ou fermé, clim arrêtée) est marqué.
int roue_composer(int r, int t, bool depuis_maison, RoueBouton b[kRoueBoutons], RoueTuile& rt) {
    if (heritage() || !tuile_presente(r, t)) return 0;
    const Def& d = s_m.tuiles[r][t];
    const Etat& e = s_etats[r][t];
    if (d.options & OPT_R) return 0;
    int n = 0;
    auto ajouter = [&](RoueAction a, RoueIcone i, RoueGenre g, bool courant, const char* legende) {
        if (n >= kRoueBoutons) return;
        b[n] = RoueBouton{i, g, courant, legende};
        rt.action[n] = a;
        n++;
    };
    auto commande = [&](RoueAction a, RoueIcone i, bool courant) {
        ajouter(a, i, RoueGenre::ACTION, courant, nullptr);
    };
    auto famille = [&](RoueAction a, RoueIcone i) { ajouter(a, i, RoueGenre::FAMILLE, false, nullptr); };
    if (!depuis_maison) ajouter(RoueAction::MAISON, RoueIcone::MAISON, RoueGenre::LIEN, false, tr("Maison"));
    const int premiere = n;
    switch (static_cast<Type>(d.type)) {
        case Type::LUM: {
            if (d.options & OPT_K) return 0;
            const bool allumee = est(e.brut, "on");
            // Option o : jamais éteinte depuis l'écran, pas d'Éteindre.
            if (d.options & OPT_D) {
                // Variateur : la commande qui change l'état, puis les luminosités.
                if (allumee && !(d.options & OPT_O)) commande(RoueAction::ETEINDRE, RoueIcone::ETEINDRE, false);
                else commande(RoueAction::ALLUMER, RoueIcone::ALLUMER, allumee);
                famille(RoueAction::LUMINOSITE, RoueIcone::LUMINOSITE);
            } else {
                commande(RoueAction::ALLUMER, RoueIcone::ALLUMER, allumee);
                if (!(d.options & OPT_O)) commande(RoueAction::ETEINDRE, RoueIcone::ETEINDRE, est(e.brut, "off"));
            }
            if (d.options & OPT_C) {
                famille(RoueAction::BLANCS, RoueIcone::BLANCS);
                famille(RoueAction::COULEURS, RoueIcone::COULEURS);
            }
            break;
        }
        case Type::VOL: {
            if (d.options & OPT_K) return 0;
            // « Ouvert » et « Fermé » comme les mots du popup (vol_etat_mots) : ouvert mais
            // arrêté en route, c'est « Partiel », pas Ouvrir.
            const bool partiel = e.valeur < 0.0f || (e.valeur > 0.0f && e.valeur < 100.0f);
            commande(RoueAction::OUVRIR, RoueIcone::OUVRIR, est(e.brut, "open") && !partiel);
            commande(RoueAction::ARRETER, RoueIcone::STOP, false);
            commande(RoueAction::FERMER, RoueIcone::FERMER, est(e.brut, "closed"));
            if (vol_position_connue(e)) famille(RoueAction::POSITION, RoueIcone::POSITION);
            break;
        }
        case Type::CLI: {
            // La clim du blueprint (option m) ou celle de la tuile : ce que HA a poussé pour
            // elle, rien d'autre.
            int rc, tc;
            roue_clim(d, r, t, rc, tc);
            const char* capacites = clim_capacites_connues(rc, tc);
            if (capacites == nullptr) return 0;
            commande(RoueAction::CLIM_ARRET, RoueIcone::ETEINDRE, est(e.brut, "off"));
            bool modes = false;
            for (const RoueModeClim& m : kRoueModesClim) modes = modes || std::strchr(capacites, m.lettre) != nullptr;
            if (modes) famille(RoueAction::MODE, RoueIcone::MODE);
            float valeurs[5];
            char textes[5][10];
            int courant = -1;
            if (clim_roue_consignes(rc, tc, valeurs, textes, courant) > 0) famille(RoueAction::CONSIGNE, RoueIcone::CONSIGNE);
            ClimBascule bascules[5];
            if (clim_roue_bascules(rc, tc, bascules) > 0) famille(RoueAction::OPTIONS, RoueIcone::OPTIONS);
            break;
        }
        default:
            return 0;
    }
    if (n == premiere) return 0;
    // « Détails » (UI-13, décision d'Axel du 07/10/2026) : « Réglages » désignait aussi les
    // Réglages de la tablette (engrenage).
    ajouter(RoueAction::REGLAGES, RoueIcone::REGLAGES, RoueGenre::LIEN, false, tr("Détails"));
    return n;
}

const char* roue_legende_mode(char lettre) {
    switch (lettre) {
        case 'h': return tr_ctx("clim", "Chaud");
        case 'c': return tr("Froid");
        case 'd': return tr("Sec");
        default: return tr("Ventilation");
    }
}

RoueIcone roue_icone_bascule(char lettre) {
    switch (lettre) {
        case 'e': return RoueIcone::ECO;
        case 'b': return RoueIcone::BOOST;
        case 'q': return RoueIcone::SILENCE;
        case 's': return RoueIcone::OSCILLATION;
        default: return RoueIcone::BRISE;
    }
}

const char* roue_legende_bascule(char lettre) {
    switch (lettre) {
        case 'e': return tr("Éco");
        case 'b': return tr("Boost");
        case 'q': return tr("Silence");
        case 's': return tr("Oscillation");
        default: return tr("Brise");
    }
}

// Choix du second anneau de la famille i de `rt` dans `c`, ce qu'ils envoient dans `env` ;
// renvoie leur nombre (0 : rien à déplier). Le choix de l'état courant (luminosité,
// position, mode, consigne, option active) est marqué ; une couleur ne l'est jamais (l'état
// poussé n'en dit que la teinte affichée).
int roue_choix(const RoueTuile& rt, int i, RoueChoix c[kRoueChoix], RoueEnvoi env[kRoueChoix]) {
    if (i < 0 || i >= rt.n || heritage() || !tuile_presente(rt.r, rt.t)) return 0;
    const Def& d = s_m.tuiles[rt.r][rt.t];
    const Etat& e = s_etats[rt.r][rt.t];
    int rc, tc;
    roue_clim(d, rt.r, rt.t, rc, tc);
    int m = 0;
    RoueChoix rebut;
    auto choix = [&](const char* commande, const char* valeur, bool courant) -> RoueChoix& {
        if (m >= kRoueChoix) return rebut;
        c[m] = RoueChoix{};
        c[m].courant = courant;
        env[m].commande = commande;
        snprintf(env[m].valeur, sizeof(env[m].valeur), "%s", valeur);
        return c[m++];
    };
    auto pastilles = [&](const RouePastille* p, size_t nb) {
        for (size_t k = 0; k < nb; k++) {
            RoueChoix& x = choix("couleur", p[k].nom, false);
            x.a_pastille = true;
            x.pastille = p[k].couleur;
            if (p[k].legende != nullptr) x.legende = tr(p[k].legende);
        }
    };
    char nombre[8];
    switch (rt.action[i]) {
        case RoueAction::LUMINOSITE: {
            // Luminosité 0-255 de l'état, en % (128 → 50) ; éteinte ou inconnue : -1.
            const int pct = est(e.brut, "on") ? lum_pct(e.valeur) : -1;
            for (uint8_t p : kRoueLuminosites) {
                snprintf(nombre, sizeof(nombre), "%u", static_cast<unsigned>(p));
                RoueChoix& x = choix("luminosite_pct", nombre, pct == p);
                snprintf(x.texte, sizeof(x.texte), "%u %%", static_cast<unsigned>(p));
            }
            break;
        }
        case RoueAction::BLANCS:
            pastilles(kRoueBlancs, sizeof(kRoueBlancs) / sizeof(kRoueBlancs[0]));
            break;
        case RoueAction::COULEURS:
            pastilles(kRoueCouleurs, sizeof(kRoueCouleurs) / sizeof(kRoueCouleurs[0]));
            break;
        case RoueAction::POSITION: {
            // Jamais sans position connue (le volet a pu la perdre roue ouverte).
            if (!vol_position_connue(e)) break;
            const long position = std::lround(e.valeur);
            for (uint8_t p : kRouePositions) {
                snprintf(nombre, sizeof(nombre), "%u", static_cast<unsigned>(p));
                RoueChoix& x = choix("position", nombre, position == p);
                snprintf(x.texte, sizeof(x.texte), "%u %%", static_cast<unsigned>(p));
            }
            break;
        }
        case RoueAction::MODE: {
            const char* capacites = clim_capacites_connues(rc, tc);
            if (capacites == nullptr) break;
            for (const RoueModeClim& md : kRoueModesClim) {
                if (std::strchr(capacites, md.lettre) == nullptr) continue;
                RoueChoix& x = choix("mode", md.mode, est(e.brut, md.mode));
                x.icone = md.icone;
                x.legende = roue_legende_mode(md.lettre);
            }
            break;
        }
        case RoueAction::CONSIGNE: {
            // La consigne et deux pas de chaque côté, dans les bornes de la clim ; envoyée
            // comme celle du popup (clim_consigne_texte : « 21.5 »).
            float valeurs[5];
            char textes[5][10];
            int courant = -1;
            const int nb = clim_roue_consignes(rc, tc, valeurs, textes, courant);
            for (int k = 0; k < nb; k++) {
                RoueChoix& x = choix("consigne", clim_consigne_texte(valeurs[k]).c_str(), k == courant);
                snprintf(x.texte, sizeof(x.texte), "%.9s", textes[k]);  // une ligne de textes[5][10]
            }
            break;
        }
        case RoueAction::OPTIONS: {
            // Bascules du popup (Éco, Boost, Silence, Oscillation, Brise) : même commande,
            // même valeur que leur bouton (clim_popup_preset, _silence, _oscillation, _brise).
            ClimBascule bascules[5];
            const int nb = clim_roue_bascules(rc, tc, bascules);
            for (int k = 0; k < nb; k++) {
                RoueChoix& x = choix(bascules[k].commande, bascules[k].valeur, bascules[k].actif);
                x.icone = roue_icone_bascule(bascules[k].lettre);
                x.legende = roue_legende_bascule(bascules[k].lettre);
            }
            break;
        }
        default:
            break;
    }
    return m;
}

// Jauge du moyeu : luminosité (0 éteinte), position du volet, consigne de la clim dans ses
// bornes ; -1 sans valeur (lampe sans variateur, position ou consigne inconnue).
int roue_jauge(int r, int t) {
    const Def& d = s_m.tuiles[r][t];
    const Etat& e = s_etats[r][t];
    switch (static_cast<Type>(d.type)) {
        case Type::LUM:
            if (!(d.options & OPT_D)) return -1;
            if (!est(e.brut, "on")) return 0;
            return lum_pct(e.valeur);
        case Type::VOL:
            return vol_position_connue(e) ? std::clamp(static_cast<int>(std::lround(e.valeur)), 0, 100) : -1;
        case Type::CLI: {
            int rc, tc;
            roue_clim(d, r, t, rc, tc);
            return clim_roue_jauge(rc, tc);
        }
        default:
            return -1;
    }
}

void roue_tuile_rejouer();

// Toucher du bouton i du premier anneau (roue déjà fermée ; une famille, tab5_roue.cpp la
// déplie sans passer par ici) : la commande, par les chemins de la tuile, ou un lien.
void roue_tuile_choisir(int i) {
    charger();
    const RoueTuile rt = s_rt;
    if (i < 0 || i >= rt.n || heritage() || !tuile_presente(rt.r, rt.t)) return;
    const Def& d = s_m.tuiles[rt.r][rt.t];
    const TuilesUI& u = g_tuiles_ui;
    const char cle[4] = {'t', static_cast<char>('0' + rt.r), static_cast<char>('0' + rt.t), '\0'};
    // Une clim : à l'emplacement que vise son popup (« clim » pour celle du blueprint).
    const char* cle_clim = (d.options & OPT_M) ? "clim" : cle;
    switch (rt.action[i]) {
        case RoueAction::MAISON:
            if (g_roue_ui.ouvrir_ecran != nullptr) g_roue_ui.ouvrir_ecran(static_cast<int>(Ecran::MAISON));
            return;
        case RoueAction::REGLAGES:
            tuile_ouvrir_popup(rt.r, rt.t);
            return;
        case RoueAction::ALLUMER:
            envoyer_tuile(rt.r, rt.t, "allumer");
            return;
        case RoueAction::ETEINDRE:
            envoyer_tuile(rt.r, rt.t, "eteindre");
            return;
        case RoueAction::OUVRIR:
            envoyer_tuile(rt.r, rt.t, "ouvrir");
            return;
        case RoueAction::ARRETER:
            envoyer_tuile(rt.r, rt.t, "arreter");
            return;
        case RoueAction::FERMER:
            envoyer_tuile(rt.r, rt.t, "fermer");
            return;
        case RoueAction::CLIM_ARRET:
            if (u.envoyer != nullptr) u.envoyer(cle_clim, "eteindre", "");
            return;
        default:
            return;
    }
}

// Choix de la famille dépliée (rappel `famille` de tab5_roue.cpp), recalculés à chaque
// dépliage et à chaque repeint : ils suivent l'état poussé.
int roue_tuile_famille(int i, RoueChoix* c) {
    charger();
    RoueEnvoi env[kRoueChoix];
    return roue_choix(s_rt, i, c, env);
}

// Toucher du choix j de la famille i (roue déjà fermée) : sa commande, recalculée sur
// l'état d'aujourd'hui.
void roue_tuile_choisir_choix(int i, int j) {
    charger();
    const RoueTuile rt = s_rt;
    RoueChoix c[kRoueChoix];
    RoueEnvoi env[kRoueChoix];
    const int m = roue_choix(rt, i, c, env);
    const TuilesUI& u = g_tuiles_ui;
    if (j < 0 || j >= m || env[j].commande == nullptr || u.envoyer == nullptr) return;
    const Def& d = s_m.tuiles[rt.r][rt.t];
    const char cle[4] = {'t', static_cast<char>('0' + rt.r), static_cast<char>('0' + rt.t), '\0'};
    const bool clim = static_cast<Type>(d.type) == Type::CLI;
    const char* cle_clim = (d.options & OPT_M) ? "clim" : cle;
    u.envoyer(clim ? cle_clim : cle, env[j].commande, env[j].valeur);
}

// Boutons, moyeu (icône, ligne d'état, nom, jauge) et couleur d'état (celle de la pastille
// de la carte, peindre_carte) de la tuile de `rt`, puis la roue autour de son ancre.
// `garder` : un repeint (thème, état poussé) garde la famille dépliée. Faux sans roue.
bool roue_tuile_peindre(RoueTuile& rt, bool garder) {
    RoueBouton b[kRoueBoutons];
    rt.n = roue_composer(rt.r, rt.t, rt.depuis_maison, b, rt);
    if (rt.n == 0) return false;
    Vue v;
    vue(rt.r, rt.t, v);
    RoueTete tete;
    tete.icone = v.icone_carte != nullptr ? v.icone_carte : "";
    tete.valeur = v.ligne;
    tete.nom = v.nom;
    tete.couleur = v.couleur_carte;
    tete.jauge = roue_jauge(rt.r, rt.t);
    RoueRappels rappels;
    rappels.choisir = roue_tuile_choisir;
    rappels.famille = roue_tuile_famille;
    rappels.choisir_choix = roue_tuile_choisir_choix;
    rappels.rejouer = roue_tuile_rejouer;
    s_rt = rt;
    return roue_ouvrir(rt.ancre, tete, b, rt.n, rappels, garder);
}

// Changement de thème, roue ouverte : la même roue dans la nouvelle palette ; si la tuile
// n'en a plus (état ou définition changés entre-temps), elle se ferme.
void roue_tuile_rejouer() {
    charger();
    RoueTuile rt = s_rt;
    if (!roue_tuile_peindre(rt, true)) roue_actions_fermer();
}

// HA a poussé un état de la tuile tRT (peindre_tuile) : la roue ouverte sur elle suit
// (bouton et choix de l'état courant, moyeu, jauge) ; elle se ferme si la tuile n'en a plus.
void roue_tuile_etat(int r, int t) {
    if (roue_actions_ouverte() && s_rt.r == r && s_rt.t == t) roue_tuile_rejouer();
}

}  // namespace

bool tuile_roue_ouvrir(int r, int t, lv_obj_t* ancre, bool depuis_maison) {
    charger();
    if (ancre == nullptr || r < 0 || r >= kPieces || t < 0 || t >= kTuiles) return false;
    RoueTuile rt;
    rt.r = r;
    rt.t = t;
    rt.ancre = ancre;
    rt.depuis_maison = depuis_maison;
    return roue_tuile_peindre(rt, false);
}

// Ancre de la tuile T de la page courante : la pastille de sa carte (mode HA), le bouton
// au centre de sa tuile météo sinon.
static bool roue_de_la_tuile(int r, int t) {
    if (g_central_ctx.ha_mode) return tuile_roue_ouvrir(r, t, g_tuiles_ui.carte_pastille[t]);
    lv_obj_t *g, *d, *b;
    widgets_meteo(t, g, d, b);
    return tuile_roue_ouvrir(r, t, b);
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

void popup_lumiere_choisir(int idx) {
    charger();
    const TuilesUI& u = g_tuiles_ui;
    if (idx < 0 || idx >= s_pl.n || u.lum_popup == nullptr) return;
    s_pl.choix = idx;
    if (u.lum_cle != nullptr) lumiere_cle(s_pl.piece, s_pl.tuiles[idx], *u.lum_cle);
    popup_lumiere_peindre();
    if (!popup_ouvert()) animate_popup_open(u.lum_popup);
}

void popup_volet_commande(const char* action) {
    charger();
    if (action == nullptr || !popup_volet_valide()) return;
    envoyer_tuile(s_pv.piece, s_pv.tuile, action);
}

// Grand bouton du popup d'un appareil : le toucher de SA tuile, par le même chemin.
void popup_appareil_appui() {
    charger();
    if (!popup_appareil_valide()) return;
    tuile_appui_piece(s_pa.piece, s_pa.tuile, false);
}

void tuiles_brancher_popup_volet() {
    static bool fait = false;
    lv_obj_t* cadre = g_tuiles_ui.vol_cadre;
    if (fait || cadre == nullptr || g_tuiles_ui.vol_tablier == nullptr) return;
    fait = true;
    lames_construire(g_tuiles_ui.vol_tablier);
    // Un glissement sur le volet ne remonte pas en geste jusqu'à la page (le swipe des
    // prévisions, sous le popup).
    lv_obj_remove_flag(cadre, LV_OBJ_FLAG_GESTURE_BUBBLE);
    for (lv_event_code_t code : {LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST})
        lv_obj_add_event_cb(cadre, volet_cadre_rappel, code, nullptr);
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
    const char cle[3] = {'p', static_cast<char>('0' + r), '\0'};
    envoyer(cle, "eteindre");
}

void popup_lumiere_tout_eteindre() { tuiles_piece_eteindre(s_pl.piece); }

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

// Une ligne du popup Maison agit-elle au toucher (comme la tuile : type_agit), et a-t-elle
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
    agit = type_agit(type, d.options, type == Type::CLI && clim_tuile_connue(r, t));
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
