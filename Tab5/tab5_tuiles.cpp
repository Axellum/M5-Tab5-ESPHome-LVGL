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
 * @architecture_constraint Pièce R ↔ page du bas dans l'ordre où un swipe les atteint depuis
 *       l'accueil : R0 = page 2 (accueil), R1 = 3, R2 = 4, R3 = 1, R4 = 0. Tuile T = position
 *       visuelle, 0 = gauche (sur les pages horaires, l'objet h(4−T)).
 * @ai_instruction Les types, options, clés et commandes sont un contrat avec le blueprint :
 *       tests/test_tuiles_firmware.py les compare aux tableaux de l'ADR-0023. Un texte
 *       affiché passe par tr() ; un nom venu de HA s'affiche tel quel, filtré aux glyphes
 *       des polices (Latin-1 + cp1252 + lettres turques, table kHorsLatin1).
 */
#include "tab5_internal.h"
#include "tab5_tuiles_icones.h"
#include "lvgl.h"
#include <esp_attr.h>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace {

constexpr int kPieces = 5;
constexpr int kTuiles = 5;
// Pièce de chaque page du bas (index = g_central_ctx.forecast_page) : R0 = page 2 (accueil),
// R1 = 3, R2 = 4, R3 = 1, R4 = 0 — l'ordre où un swipe les atteint depuis l'accueil.
constexpr int kPieceDePage[kPieces] = {4, 3, 0, 1, 2};

// Types de tuile (ADR-0023), dans l'ordre de kTypes.
enum class Type : uint8_t { VIDE, LUM, INT, VOL, MED, ACT, CAP, BIN, CLI };
constexpr const char* kTypes[] = {"", "lum", "int", "vol", "med", "act", "cap", "bin", "cli"};
constexpr int kNbTypes = sizeof(kTypes) / sizeof(kTypes[0]);

// Options : une lettre chacune, bit i = lettre i de kLettresOptions.
//   d graduable, c couleur, o allumer seulement, k confirmer, r lecture seule,
//   t télécommande TV du blueprint, m climatisation du blueprint (sans m, une tuile cli a
//   sa propre clim dans le popup dès que HA en a envoyé les réglages, ADR-0027).
constexpr char kLettresOptions[] = "dcokrtm";
enum : uint8_t { OPT_D = 1, OPT_C = 2, OPT_O = 4, OPT_K = 8, OPT_R = 16, OPT_T = 32, OPT_M = 64 };

// Taille des champs gardés (octets, zéro final compris).
constexpr size_t kNom = 25;          // nom affiché : 24 octets au plus
constexpr size_t kIcone = 16;        // code de palette [a-z0-9_]{1,15}
constexpr size_t kComplement = 16;   // unité (cap) ou classe d'appareil (bin)
constexpr size_t kEtat = 16;         // état HA tel quel

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

// ~2,3 Ko lus au dessin et aux poussées seulement : en PSRAM (BSS externe, remise à zéro
// au démarrage, CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY), pas dans les ~226 Ko de
// RAM interne libre.
EXT_RAM_BSS_ATTR Modele s_m;
EXT_RAM_BSS_ATTR Etat s_etats[kPieces][kTuiles];
bool s_charge = false;
esphome::ESPPreferenceObject s_pref;

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

// Code de palette : [a-z0-9_]{1,15}, sinon vide (défaut du type).
void copier_icone(char* dst, const char* src, size_t n) {
    std::memset(dst, 0, kIcone);
    if (n == 0 || n >= kIcone) return;
    for (size_t i = 0; i < n; i++) {
        const char c = src[i];
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_')) return;
    }
    std::memcpy(dst, src, n);
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

// Champs d'une entrée séparés par '|' (au plus `max`) : début et longueur de chacun.
struct Champ {
    const char* p;
    size_t n;
};
int decouper(const char* s, size_t n, Champ* out, int max) {
    int k = 0;
    size_t debut = 0;
    for (size_t i = 0; i <= n && k < max; i++) {
        if (i == n || s[i] == '|') {
            out[k++] = {s + debut, i - debut};
            debut = i + 1;
        }
    }
    return k;
}

// Une entrée de tab5_maj_tuiles dans `m` ; une clé inconnue est ignorée.
void lire_entree(const char* s, size_t n, Modele& m) {
    Champ f[6];
    const int nf = decouper(s, n, f, 6);
    if (nf < 2) return;
    int r = 0, t = 0;
    if (f[0].n == 2 && f[0].p[0] == 'p' && chiffre_0_4(f[0].p[1], r)) {
        copier_texte(m.pieces[r], kNom, f[1].p, f[1].n);
        return;
    }
    if (f[0].n != 3 || f[0].p[0] != 't' || !chiffre_0_4(f[0].p[1], r) || !chiffre_0_4(f[0].p[2], t)) return;
    Def& d = m.tuiles[r][t];
    d.type = static_cast<uint8_t>(lire_type(f[1].p, f[1].n));
    if (nf > 2) copier_icone(d.icone, f[2].p, f[2].n);
    if (nf > 3) d.options = lire_options(f[3].p, f[3].n);
    if (nf > 4) copier_texte(d.complement, kComplement, f[4].p, f[4].n);
    if (nf > 5) copier_texte(d.nom, kNom, f[5].p, f[5].n);
    if (d.type == static_cast<uint8_t>(Type::VIDE)) d = Def{};
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

bool est(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

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
    uint32_t couleur = UIColor::INACTIVE;
    const char* icone_carte = nullptr;  // carte du mode HA (70 px) ; nullptr : inchangée
    uint32_t couleur_carte = UIColor::INACTIVE;
    const char* droite = nullptr;       // épaule droite ; nullptr : masquée
    uint32_t couleur_droite = UIColor::TEXT_DIM;
    const char* nom = "";
    char ligne[40] = "";                // ligne d'état de la carte
    uint32_t couleur_ligne = UIColor::INACTIVE;
    bool agit = false;                  // un appui fait quelque chose
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
// sur le fond ardoise des cartes (luminance < 110 sur 255).
uint32_t couleur_lisible(uint32_t c) {
    const int r = (c >> 16) & 0xFF, g = (c >> 8) & 0xFF, b = c & 0xFF;
    const int y = (2126 * r + 7152 * g + 722 * b) / 10000;
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
// tablette en a les réglages (clé crRT, ADR-0027 ; `clim_connue`).
bool type_agit(Type type, uint8_t options, bool clim_connue) {
    if (options & OPT_R) return false;
    switch (type) {
        case Type::LUM: case Type::INT: case Type::VOL: case Type::MED: case Type::ACT: return true;
        case Type::CLI: return (options & OPT_M) != 0 || clim_connue;
        default: return false;
    }
}

// Épaule droite d'un volet : la flèche de ce qu'un appui ferait (pause en mouvement,
// sinon le sens choisi), comme la 3.1.
void vue_fleche_volet(const Etat& e, Vue& v) {
    if (vol_mouvement(e.brut)) {
        v.droite = glyphe_fleche(0);
        v.couleur_droite = UIColor::INFO;
    } else {
        v.droite = glyphe_fleche(vol_sens(e) == SENS_FERMER ? -1 : 1);
        v.couleur_droite = UIColor::TEXT_DIM;
    }
}

void vue_nouvelle(int r, int t, Vue& v) {
    const Def& d = s_m.tuiles[r][t];
    const Etat& e = s_etats[r][t];
    const Type type = static_cast<Type>(d.type);
    const char* s = e.brut;
    bool actif = false;
    uint32_t c = UIColor::TEXT_DIM;
    v.nom = d.nom;
    v.agit = type_agit(type, d.options, type == Type::CLI && clim_tuile_connue(r, t));
    switch (type) {
        case Type::LUM:
            actif = est(s, "on");
            if (actif && (d.options & OPT_D) && !std::isnan(e.valeur)) {
                const int pct = std::max(1, std::min(100, static_cast<int>(std::lround(e.valeur * 100.0f / 255.0f))));
                snprintf(v.ligne, sizeof(v.ligne), "%d %%", pct);
            } else {
                snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? tr("Allumé") : tr("Éteint"));
            }
            c = actif ? (e.a_couleur ? couleur_lisible(e.couleur) : UIColor::INFO) : UIColor::TEXT_DIM;
            v.droite = glyphe_ampoule(actif);
            v.couleur_droite = c;
            break;
        case Type::INT:
            actif = est(s, "on");
            snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? tr("Allumé") : tr("Éteint"));
            c = actif ? UIColor::SUCCESS : UIColor::TEXT_DIM;
            break;
        case Type::VOL:
            actif = !est(s, "closed");
            if (vol_mouvement(s)) {
                snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Mouvement"));
                c = UIColor::INFO;
            } else if (est(s, "open")) {
                if (!std::isnan(e.valeur) && e.valeur > 0.0f && e.valeur < 100.0f)
                    snprintf(v.ligne, sizeof(v.ligne), "%.0f %%", e.valeur);
                else
                    snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Ouvert"));
                c = UIColor::SUCCESS;
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
            c = actif ? UIColor::SUCCESS : UIColor::TEXT_DIM;
            break;
        case Type::ACT:
            actif = minuterie_sur(s_ok, r, t);
            snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? "OK" : tr("Lancer"));
            c = actif ? UIColor::SUCCESS : UIColor::ACCENT;
            break;
        case Type::CAP:
            actif = true;
            if (!std::isnan(e.valeur)) formater_mesure(v.ligne, sizeof(v.ligne), e.valeur, d.complement);
            else snprintf(v.ligne, sizeof(v.ligne), "%s", s);
            c = (d.complement[0] != '\0' && std::strncmp(d.complement, "\xC2\xB0", 2) == 0 && !std::isnan(e.valeur))
                    ? get_temperature_color(e.valeur) : UIColor::INFO;
            break;
        case Type::BIN: {
            const Famille f = famille_bin(d.complement);
            if (f == Famille::VERROU) {
                // binary_sensor « lock » : on = déverrouillé ; entité lock : locked / unlocked.
                const bool verrouille = est(s, "locked") || est(s, "off");
                snprintf(v.ligne, sizeof(v.ligne), "%s", verrouille ? tr("Verrouillé") : tr("Ouvert"));
                actif = !verrouille;
                c = verrouille ? UIColor::SUCCESS : UIColor::WARNING;
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
            c = actif ? (f == Famille::PRESENCE ? UIColor::SUCCESS : UIColor::WARNING) : UIColor::TEXT_DIM;
            break;
        }
        case Type::CLI:
            actif = !est(s, "off");
            if (!std::isnan(e.valeur)) snprintf(v.ligne, sizeof(v.ligne), "%.1f \xC2\xB0", e.valeur);
            else snprintf(v.ligne, sizeof(v.ligne), "%s", actif ? "--" : tr("Éteint"));
            if (est(s, "cool")) c = UIColor::CLIM_COOL_ACTIVE;
            else if (est(s, "heat")) c = UIColor::CLIM_HEAT_ACTIVE;
            else c = actif ? UIColor::SUCCESS : UIColor::TEXT_DIM;
            break;
        default:
            break;
    }
    uint32_t c_ligne = c;
    // Pas encore d'état depuis le démarrage (les états ne sont pas gardés) : « -- » grisé.
    // unavailable / unknown : « Hors ligne », grisé (un script ou une scène n'en a pas).
    if (!e.recu) {
        actif = false;
        c = c_ligne = UIColor::INACTIVE;
        v.droite = nullptr;
        snprintf(v.ligne, sizeof(v.ligne), "--");
    } else if (est(s, "unavailable") || (est(s, "unknown") && type != Type::ACT)) {
        actif = false;
        c = c_ligne = UIColor::INACTIVE;
        v.droite = nullptr;
        snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Hors ligne"));
    } else if (type == Type::CLI && !std::isnan(e.valeur)) {
        c_ligne = get_temperature_color(e.valeur);
    }
    // Sens d'un volet basculé par le titre : la ligne d'état le dit 2 s (en mode HA, la
    // carte n'a pas de flèche).
    if (type == Type::VOL && e.recu && !vol_mouvement(s) && minuterie_sur(s_sens, r, t)) {
        c_ligne = UIColor::ACCENT;
        snprintf(v.ligne, sizeof(v.ligne), "%s", vol_sens(e) == SENS_FERMER ? tr("Fermer") : tr("Ouvrir"));
    }
    // Option k : le premier appui arme, la ligne d'état demande le second (3 s).
    if (minuterie_sur(s_confirmation, r, t)) {
        c = c_ligne = UIColor::WARNING;
        snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Confirmer ?"));
    }
    const char* icone = tuile_icone(d.icone, actif, kTypes[d.type]);
    v.icone = v.icone_carte = icone;
    v.couleur = v.couleur_carte = c;
    v.couleur_ligne = c_ligne;
}

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
            v.couleur = epaule ? UIColor::SUCCESS : UIColor::TEXT_DIM;
            v.icone_carte = heritage_glyphe_carte(0);
            const uint32_t c = s_h.pc ? UIColor::SUCCESS : UIColor::TEXT_DIM;
            v.couleur_carte = s_h.pc_recu ? c : UIColor::INACTIVE;
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
            v.couleur = o == 1 ? UIColor::SUCCESS : (o == 0 ? UIColor::ERROR : UIColor::TEXT_DIM);
            const bool ouvrir = g_tuiles_ui.volet_sens == nullptr || *g_tuiles_ui.volet_sens;
            v.droite = glyphe_fleche(mouvement ? 0 : (ouvrir ? 1 : -1));
            v.couleur_droite = mouvement ? UIColor::INFO : UIColor::TEXT_DIM;
            if (mouvement) {
                snprintf(v.ligne, sizeof(v.ligne), "%s", tr("Mouvement"));
                v.couleur_carte = UIColor::INFO;
            } else if (o >= 0) {
                snprintf(v.ligne, sizeof(v.ligne), "%s", o == 1 ? tr("Ouvert") : tr("Fermé"));
                v.couleur_carte = o == 1 ? UIColor::SUCCESS : UIColor::TEXT_DIM;
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
            const uint32_t c = s_h.lum[i] ? UIColor::INFO : UIColor::TEXT_DIM;
            v.icone = heritage_glyphe_epaule(t, false);
            v.couleur = c;
            v.droite = glyphe_ampoule(s_h.lum[i]);
            v.couleur_droite = c;
            v.couleur_carte = s_h.lum_recu[i] ? c : UIColor::INACTIVE;
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

// Onglets titre et état des cartes : 200 px, 6 px de marge de chaque côté.
constexpr int32_t kLargeurOnglet = 188;

void peindre_carte(int r, int t) {
    const TuilesUI& u = g_tuiles_ui;
    if (u.carte_icone[t] == nullptr) return;
    Vue v;
    vue(r, t, v);
    if (v.icone_carte != nullptr) ui_text(u.carte_icone[t], v.icone_carte);
    ui_text_color(u.carte_icone[t], v.couleur_carte);
    ui_texte_coupe(u.carte_nom[t], v.nom, kLargeurOnglet);
    ui_texte_coupe(u.carte_etat[t], v.ligne, kLargeurOnglet);
    ui_text_color(u.carte_etat[t], v.couleur_ligne);
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
    int32_t x = (1280 - (n * 250 - 20)) / 2;
    for (int t = 0; t < kTuiles; t++) {
        const bool presente = tuile_presente(r, t);
        ui_hidden(u.carte[t], !presente);
        if (!presente || u.carte[t] == nullptr) continue;
        ui_x(u.carte[t], x);
        x += 250;
        peindre_carte(r, t);
    }
}

// Mode météo : les épaules et le bouton d'une tuile (masqués sans appareil ; bouton
// masqué aussi quand un appui ne ferait rien : cap, bin, option r).
void peindre_epaules(int r, int t, lv_obj_t* gauche, lv_obj_t* droite, lv_obj_t* bouton) {
    const bool presente = tuile_presente(r, t);
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
    ui_hidden(g_tuiles_ui.jour_sens, !(heritage() && r == 0 && tuile_presente(0, 1)));
}

void popup_lumiere_etat(int r, int t);

// Une tuile a changé (état, minuterie) : la repeindre là où elle est affichée.
void peindre_tuile(int r, int t) {
    if (r < 0 || r >= kPieces || t < 0 || t >= kTuiles) return;
    popup_lumiere_etat(r, t);
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
        highlight_button_border(u.bouton_ha, true, UIColor::INFO);
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
// ferait sauter le curseur sous le doigt). Éteinte : 0.
void popup_lumiere_arc() {
    const TuilesUI& u = g_tuiles_ui;
    if (s_pl.n == 0 || u.lum_arc == nullptr || u.lum_pct == nullptr) return;
    if (lv_obj_has_state(u.lum_arc, LV_STATE_PRESSED)) return;
    const int r = s_pl.piece, t = s_pl.tuiles[s_pl.choix];
    const float v = lumiere_luminosite(r, t);
    const int arcv = lumiere_allumee(r, t) ? tab5_float_vers_int(v, 0, 255, 0) : 0;
    lv_arc_set_value(u.lum_arc, arcv);
    char buf[12];
    snprintf(buf, sizeof(buf), "%d %%", arcv * 100 / 255);
    ui_text(u.lum_pct, buf);
}

// Une ligne du sélecteur : icône (palette ou 3.1) dorée si allumée, nom coupé.
void popup_lumiere_ligne(int i) {
    const TuilesUI& u = g_tuiles_ui;
    const int r = s_pl.piece, t = s_pl.tuiles[i];
    const bool on = lumiere_allumee(r, t);
    const char* icone = heritage() ? heritage_glyphe_selecteur(t) : tuile_icone(s_m.tuiles[r][t].icone, on, "lum");
    ui_text(u.lum_sel_icone[i], icone);
    ui_text_color(u.lum_sel_icone[i], on ? UIColor::INFO : UIColor::TEXT_DIM);
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
        highlight_button_border(b, i == s_pl.choix, UIColor::ACCENT, 3);
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
    ui_text_color(u.lum_power, lumiere_allumee(r, t) ? UIColor::INFO : UIColor::TEXT_DIM);
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

// Un état a changé : la ligne de cette tuile si le popup la montre.
void popup_lumiere_etat(int r, int t) {
    if (!popup_ouvert() || r != s_pl.piece) return;
    const TuilesUI& u = g_tuiles_ui;
    for (int i = 0; i < s_pl.n; i++) {
        if (s_pl.tuiles[i] != t) continue;
        popup_lumiere_ligne(i);
        if (i == s_pl.choix) {
            ui_text_color(u.lum_power, lumiere_allumee(r, t) ? UIColor::INFO : UIColor::TEXT_DIM);
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

}  // namespace

TuilesUI g_tuiles_ui;

// Pour les autres unités (titre du popup clim, ADR-0026) : un nom venu de HA suit les
// mêmes règles que ceux des tuiles. Déclarées dans tab5_internal.h.
void texte_ha_copier(char* dst, size_t cap, const char* src, size_t n) { copier_texte(dst, cap, src, n); }
void texte_ha_coupe(lv_obj_t* lbl, const char* txt, int32_t largeur) { ui_texte_coupe(lbl, txt, largeur); }

bool tuiles_definir(const std::string& payload) {
    charger();
    // Instantané complet : ce qui n'est pas listé est vide. Construit à part (tas, le temps
    // de la comparaison), pour n'écrire la NVS et ne redessiner que si quelque chose change.
    std::unique_ptr<Modele> neuf(new Modele());
    neuf->magic = kMagic;
    neuf->recues = 1;
    size_t debut = 0;
    while (debut < payload.size()) {
        size_t fin = payload.find(';', debut);
        if (fin == std::string::npos) fin = payload.size();
        lire_entree(payload.data() + debut, fin - debut, *neuf);
        debut = fin + 1;
    }
    if (std::memcmp(neuf.get(), &s_m, sizeof(Modele)) == 0) return false;
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
    tuiles_appliquer_ui();
    return true;
}

bool tuiles_etat_recu(const char* cle, size_t n_cle, const char* reste, size_t n_reste) {
    int r = 0, t = 0;
    if (n_cle != 3 || cle[0] != 't' || !chiffre_0_4(cle[1], r) || !chiffre_0_4(cle[2], t)) return false;
    charger();
    Champ f[3];
    const int nf = decouper(reste, n_reste, f, 3);
    Etat& e = s_etats[r][t];
    const size_t n = nf > 0 ? std::min(f[0].n, kEtat - 1) : 0;
    std::memset(e.brut, 0, sizeof(e.brut));
    if (n > 0) std::memcpy(e.brut, f[0].p, n);
    e.valeur = NAN;
    if (nf > 1 && f[1].n > 0 && f[1].n < 24) {
        char tmp[24];
        std::memcpy(tmp, f[1].p, f[1].n);
        tmp[f[1].n] = '\0';
        char* bout = nullptr;
        const float v = strtof(tmp, &bout);
        if (bout != tmp) e.valeur = v;  // « nan » donne NaN aussi
    }
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
    if (s_m.tuiles[r][t].type == static_cast<uint8_t>(Type::VOL)) vol_sens_suivre(e);
    if (!heritage()) peindre_tuile(r, t);
    return true;
}

void tuiles_appliquer_ui() {
    charger();
    CentralPanelCtx& ctx = g_central_ctx;
    const TuilesUI& u = g_tuiles_ui;
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

void tuiles_repeindre(int r, int t) {
    charger();
    if (!heritage()) peindre_tuile(r, t);
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

void tuile_appui(int t, bool long_appui) {
    charger();
    const int r = piece_courante();
    if (!tuile_presente(r, t)) return;
    if (heritage()) {
        appui_heritage(t, long_appui);
        return;
    }
    const Def& d = s_m.tuiles[r][t];
    const Etat& e = s_etats[r][t];
    const Type type = static_cast<Type>(d.type);
    // cap, bin, option r, cli sans m dont la tablette n'a pas les réglages
    if (!type_agit(type, d.options, type == Type::CLI && clim_tuile_connue(r, t))) return;
    // Tableau de l'ADR-0023 : appui court, puis appui long.
    const char* action = nullptr;
    switch (type) {
        case Type::LUM:
            if (long_appui) {
                popup_lumiere_ouvrir(r, t);
                return;
            }
            action = (d.options & OPT_O) ? "allumer" : "basculer";
            break;
        case Type::INT:
            if (long_appui) return;
            action = (d.options & OPT_O) ? "allumer" : "basculer";
            break;
        case Type::VOL:
            action = long_appui ? vol_appui_long(e) : vol_appui(e);
            break;
        case Type::MED:
            if (long_appui) {
                if (d.options & OPT_T) ouvrir_popup(g_tuiles_ui.popup_tv);
                return;
            }
            action = (d.options & OPT_O) ? "allumer" : "basculer";
            break;
        case Type::ACT:
            if (long_appui) return;
            action = "lancer";
            break;
        case Type::CLI:
            // Option m : la clim du blueprint ; sinon celle de la tuile (ADR-0027), que
            // type_agit sait connue. Le popup revient à la clim du blueprint à sa fermeture.
            if (long_appui) return;
            if (d.options & OPT_M) clim_afficher_blueprint();
            else if (!clim_afficher_tuile(r, t)) return;
            ouvrir_popup(g_tuiles_ui.popup_clim);
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

void popup_lumiere_tout_eteindre() {
    charger();
    if (heritage()) {
        envoyer("lumieres", "eteindre");
        return;
    }
    const char cle[3] = {'p', static_cast<char>('0' + s_pl.piece), '\0'};
    envoyer(cle, "eteindre");
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
