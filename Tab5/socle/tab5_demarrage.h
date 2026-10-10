/**
 * [AI-CONTEXT]
 * @file tab5_demarrage.h
 * @role Chronologie du démarrage (10/10/2026, demande d'Axel) : l'instant de quelques étapes
 *       fixes du démarrage, publié vers Home Assistant par deux capteurs
 *       (tab5-sensors-diagnostics.yaml) : « Tab5 Chronologie du démarrage » (jusqu'à la fin
 *       de setup()) et « Tab5 Chronologie du démarrage (suite) » (après). Pourquoi : une
 *       tablette sur secteur n'a pas de journal série, et la mesure du 10/10/2026 (7,2 s
 *       avant l'expandeur, 14 s entre l'expandeur et le rétroéclairage) ne disait pas ce
 *       qui les occupe.
 *       Horloge : esp_timer_get_time() / 1000 (demarrage_horloge_ms, défini avec l'état
 *       vivant dans Tab5/ecran/tab5_demarrage.cpp : le socle reste pur), le compteur matériel
 *       remis à zéro par ESP-IDF à son initialisation (esp_timer_impl_early_init, étape
 *       CORE, avant les constructeurs C++) : il ne compte ni la ROM ni le bootloader.
 *       millis() d'ESPHome compte les ticks de FreeRTOS (CONFIG_FREERTOS_HZ=1000), donc
 *       depuis le lancement de l'ordonnanceur : « ordo » donne l'écart (millis = valeur -
 *       ordo). La liste des étapes, où chacune est marquée et comment la lire :
 *       docs/performance.md, « Chronologie du démarrage ».
 * @architecture_constraint Aucune dépendance ESPHome ni LVGL : ce fichier se compile sur PC
 *       (tools/test_tab5_socle.cpp, g++ en CI). Observer sans retarder : marquer = une
 *       lecture d'horloge et une écriture en mémoire ; rien n'est journalisé ni publié
 *       pendant setup() (le YAML ne publie qu'à partir de la première image).
 *       Dans la séquence on_boot de tab5-ha-hmi.yaml ([AI-WARNING-CRITICAL], ADR-0005),
 *       autorisation d'Axel du 10/10/2026 : des marques seulement, en actions à part (la
 *       lambda du delay(1000) reste telle quelle), et des entrées qui ne font que marquer,
 *       à des priorités libres, pour borner les bandes de priorité de setup().
 * @ai_instruction Le texte suit l'ordre de EtapeDemarrage : une étape nouvelle se range
 *       avant ORDO (premier texte, setup()) ou après (texte « suite »), avec son nom au même
 *       rang de kEtapesNoms (tests/test_chronologie_demarrage.py relit les deux et la
 *       longueur au pire). Le texte va à Home Assistant : jamais traduit (pas de tr()).
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>

enum class EtapeDemarrage : uint8_t {
    // Premier texte : jusqu'à la fin de setup().
    CTOR = 0,
    OBJETS,
    SETUP,
    BUS,
    EXPANDEUR,
    AVANT1S,
    APRES1S,
    P600,
    DONNEES,
    LVGL,
    WIFIINIT,
    RESEAU,
    ECOUTE,
    FIN,
    I18N,
    ZONES,
    // Second texte (« suite ») : la boucle.
    ORDO,
    RETRO,
    DESSIN,
    IMAGE,
    TARD,
    WIFI,
    API,
    FIN600,
    CHARGEUR,
    NOMBRE,
};

constexpr size_t kEtapesDemarrage = static_cast<size_t>(EtapeDemarrage::NOMBRE);
static_assert(kEtapesDemarrage <= 32, "ChronoDemarrage::vues : un bit par étape");
// Première étape du second texte.
constexpr size_t kEtapesSuite = static_cast<size_t>(EtapeDemarrage::ORDO);

// Noms publiés (« étape=ms »), dans l'ordre de EtapeDemarrage.
constexpr const char* kEtapesNoms[kEtapesDemarrage] = {
    "ctor",
    "objets",
    "setup",
    "bus",
    "expandeur",
    "avant1s",
    "apres1s",
    "p600",
    "donnees",
    "lvgl",
    "wifiinit",
    "reseau",
    "ecoute",
    "fin",
    "i18n",
    "zones",
    "ordo",
    "retro",
    "dessin",
    "image",
    "tard",
    "wifi",
    "api",
    "fin600",
    "chargeur",
};

// Un texte publié : moins de 255 caractères (limite d'un état texte de Home Assistant).
// Au pire : premier texte avec chaque étape à 6 chiffres (setup() fini avant 1 000 s),
// second texte avec chaque étape à 10 chiffres (tools/test_tab5_socle.cpp). Au-delà, le
// texte est coupé, jamais débordé.
constexpr size_t kChronoTexteMax = 256;

enum class PartieChrono : uint8_t {
    SETUP = 0,  // CTOR … ZONES
    SUITE = 1,  // ORDO … CHARGEUR
};

struct ChronoDemarrage {
    uint32_t ms[kEtapesDemarrage] = {};
    uint32_t vues = 0;  // un bit par étape déjà marquée
};

inline bool chrono_vue(const ChronoDemarrage& c, EtapeDemarrage e) {
    const size_t i = static_cast<size_t>(e);
    return i < kEtapesDemarrage && (c.vues & (1u << i)) != 0;
}

// Marque une étape à `maintenant_ms`, une seule fois par démarrage : vrai si elle vient de
// l'être (le YAML ne publie que dans ce cas).
inline bool chrono_marquer(ChronoDemarrage& c, EtapeDemarrage e, uint32_t maintenant_ms) {
    const size_t i = static_cast<size_t>(e);
    if (i >= kEtapesDemarrage || chrono_vue(c, e)) return false;
    c.ms[i] = maintenant_ms;
    c.vues |= 1u << i;
    return true;
}

// « ctor=312; objets=1480; setup=-; … » : les étapes d'une partie, dans l'ordre ; « - »
// pour une étape pas encore vue. Renvoie la longueur écrite (coupée à n - 1).
inline size_t chrono_texte(const ChronoDemarrage& c, PartieChrono p, char* buf, size_t n) {
    if (buf == nullptr || n == 0) return 0;
    buf[0] = '\0';
    const size_t debut = p == PartieChrono::SETUP ? 0 : kEtapesSuite;
    const size_t fin = p == PartieChrono::SETUP ? kEtapesSuite : kEtapesDemarrage;
    size_t pos = 0;
    for (size_t i = debut; i < fin && pos + 1 < n; i++) {
        const char* sep = i == debut ? "" : "; ";
        int k;
        if (chrono_vue(c, static_cast<EtapeDemarrage>(i))) {
            k = std::snprintf(buf + pos, n - pos, "%s%s=%lu", sep, kEtapesNoms[i],
                              static_cast<unsigned long>(c.ms[i]));
        } else {
            k = std::snprintf(buf + pos, n - pos, "%s%s=-", sep, kEtapesNoms[i]);
        }
        if (k < 0) break;
        pos += static_cast<size_t>(k);
        if (pos >= n) pos = n - 1;
    }
    return pos;
}

// « ordo » : l'instant `horloge_ms` où millis() valait `millis_ms`, ramené à millis() = 0
// (lancement de l'ordonnanceur). 0 si l'horloge est en retard (ne doit pas arriver).
inline uint32_t chrono_ordo(uint32_t horloge_ms, uint32_t millis_ms) {
    return horloge_ms >= millis_ms ? horloge_ms - millis_ms : 0;
}

// ─── L'état de la tablette (un seul démarrage à la fois) et les appels du YAML ───
inline ChronoDemarrage& chrono_demarrage() {
    static ChronoDemarrage c;
    return c;
}

// Une valeur donnée ; les étapes du YAML passent par demarrage_marquer().
inline bool demarrage_noter(EtapeDemarrage e, uint32_t ms) { return chrono_marquer(chrono_demarrage(), e, ms); }

inline size_t demarrage_texte(PartieChrono p, char* buf, size_t n) {
    return chrono_texte(chrono_demarrage(), p, buf, n);
}

// Définis dans Tab5/ecran/tab5_demarrage.cpp (l'horloge d'ESP-IDF, et « ctor », marqué
// par un constructeur global de ce fichier) :
uint32_t demarrage_horloge_ms();                     // ms depuis l'initialisation d'esp_timer
bool demarrage_marquer(EtapeDemarrage e);            // l'étape, à demarrage_horloge_ms()
bool demarrage_noter_ordo(uint32_t millis_maintenant);  // ORDO, avec millis() lu par l'appelant
