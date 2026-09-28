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
 *       des polices (Latin-1 + cp1252, table kHorsLatin1).
 */
#include "tab5_internal.h"
#include "lvgl.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>

namespace {

constexpr int kPieces = 5;
constexpr int kTuiles = 5;

// Types de tuile (ADR-0023), dans l'ordre de kTypes.
enum class Type : uint8_t { VIDE, LUM, INT, VOL, MED, ACT, CAP, BIN, CLI };
constexpr const char* kTypes[] = {"", "lum", "int", "vol", "med", "act", "cap", "bin", "cli"};
constexpr int kNbTypes = sizeof(kTypes) / sizeof(kTypes[0]);

// Options : une lettre chacune, bit i = lettre i de kLettresOptions.
//   d graduable, c couleur, o allumer seulement, k confirmer, r lecture seule,
//   t télécommande TV du blueprint, m climatisation du blueprint.
constexpr char kLettresOptions[] = "dcoktrm";
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
};

constexpr uint32_t kMagic = 0x54554931;    // « TUI1 »
constexpr uint32_t kPrefKey = 0x7475696C;  // « tuil »

Modele s_m{};
Etat s_etats[kPieces][kTuiles];
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
// la ponctuation de Windows-1252 et İ ō ř (questions de Trial Poursuite).
// tests/test_tuiles_firmware.py compare cette table à la liste des glyphes.
constexpr uint16_t kHorsLatin1[] = {
    0x20AC, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021, 0x02C6, 0x2030, 0x0160,
    0x2039, 0x0152, 0x017D, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
    0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x017E, 0x0178, 0x0130, 0x014D, 0x0159,
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

}  // namespace

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
    for (int r = 0; r < kPieces; r++)
        for (int t = 0; t < kTuiles; t++)
            if (std::memcmp(&neuf->tuiles[r][t], &s_m.tuiles[r][t], sizeof(Def)) != 0) etat_vider(s_etats[r][t]);
    const bool premiere = s_m.recues == 0;
    s_m = *neuf;
    s_pref.save(&s_m);
    int n = 0;
    for (const auto& piece : s_m.tuiles)
        for (const Def& d : piece)
            if (d.type != static_cast<uint8_t>(Type::VIDE)) n++;
    ESP_LOGI("tab5.tuiles", "Définitions reçues : %d appareil(s)%s", n,
             premiere ? " (fin du mode héritage)" : "");
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
    return true;
}
