/**
 * [AI-CONTEXT]
 * @file tab5_demarrage.h
 * @role Chronologie du démarrage (10/10/2026, demande d'Axel) : l'instant (millis(), en ms
 *       depuis le lancement d'ESP-IDF) de quelques étapes fixes du démarrage, publié vers
 *       Home Assistant par le capteur « Tab5 Chronologie du démarrage »
 *       (tab5-sensors-diagnostics.yaml). Pourquoi : une tablette sur secteur n'a pas de
 *       journal série, et la mesure du 01/10/2026 (docs/performance.md, « Démarrage, phase
 *       par phase ») ne disait ni ce qui occupe les ~5 s de setup() ni quand le
 *       rétroéclairage s'allume.
 *       Étapes, dans l'ordre de kEtapesNoms :
 *         - expandeur : allumage de l'alimentation USB (pi4ioe2 P3, `usb_5v_power`),
 *           composants matériels (priorité 800), AVANT l'attente bloquante de 1 s de
 *           l'on_boot 700 ;
 *         - retro : première écriture non nulle du PWM du rétroéclairage ;
 *         - dessin : premier dessin de LVGL (LV_EVENT_RENDER_START), juste après setup() ;
 *         - image : fin de la première image envoyée à l'écran ;
 *         - wifi : Wi-Fi connecté ; api : premier client de l'API (Home Assistant) ;
 *         - chargeur : premier allumage du chargeur de la batterie (CHG_EN). « - » quand
 *           le démarrage précédent n'a vu aucune batterie (tab5_batterie.h).
 * @architecture_constraint Aucune dépendance ESPHome ni LVGL : ce fichier se compile sur PC
 *       (tools/test_tab5_socle.cpp, g++ en CI). Observer sans retarder : marquer = une
 *       lecture de millis() et une écriture en mémoire ; rien n'est journalisé ni publié
 *       pendant setup() (le YAML ne publie qu'à partir de la première image).
 *       La séquence on_boot de tab5-ha-hmi.yaml ([AI-WARNING-CRITICAL], ADR-0005) n'est
 *       PAS instrumentée : « expandeur » et « dessin » l'encadrent.
 * @ai_instruction Une étape nouvelle s'ajoute à la FIN de EtapeDemarrage et de
 *       kEtapesNoms (le texte publié garde l'ordre ; tests/test_chronologie_demarrage.py
 *       relit les deux). Le texte va à Home Assistant : jamais traduit (pas de tr()).
 */
#pragma once
#include <cstddef>
#include <cstdint>
#include <cstdio>

enum class EtapeDemarrage : uint8_t {
    EXPANDEUR = 0,
    RETRO,
    DESSIN,
    IMAGE,
    WIFI,
    API,
    CHARGEUR,
    NOMBRE,
};

constexpr size_t kEtapesDemarrage = static_cast<size_t>(EtapeDemarrage::NOMBRE);

// Noms publiés (« étape=ms »), dans l'ordre de EtapeDemarrage.
constexpr const char* kEtapesNoms[kEtapesDemarrage] = {
    "expandeur",
    "retro",
    "dessin",
    "image",
    "wifi",
    "api",
    "chargeur",
};

// Texte publié : au plus kEtapesDemarrage × « nom=4294967295; », moins de 255 caractères
// (limite d'un état texte dans Home Assistant).
constexpr size_t kChronoTexteMax = 160;

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

// « expandeur=1490; retro=1532; dessin=-; … » : toutes les étapes, dans l'ordre ; « - »
// pour une étape pas encore vue. Renvoie la longueur écrite (coupée à n - 1).
inline size_t chrono_texte(const ChronoDemarrage& c, char* buf, size_t n) {
    if (buf == nullptr || n == 0) return 0;
    buf[0] = '\0';
    size_t pos = 0;
    for (size_t i = 0; i < kEtapesDemarrage && pos + 1 < n; i++) {
        const char* sep = i == 0 ? "" : "; ";
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

// ─── L'état de la tablette (un seul démarrage à la fois) et les appels du YAML ───
inline ChronoDemarrage& chrono_demarrage() {
    static ChronoDemarrage c;
    return c;
}

inline bool demarrage_marquer(EtapeDemarrage e, uint32_t maintenant_ms) {
    return chrono_marquer(chrono_demarrage(), e, maintenant_ms);
}

inline size_t demarrage_texte(char* buf, size_t n) { return chrono_texte(chrono_demarrage(), buf, n); }
