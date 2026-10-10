/**
 * [AI-CONTEXT]
 * @file tab5_demarrage.cpp
 * @role Chronologie du démarrage (10/10/2026) : l'horloge des marques et l'étape « ctor ».
 *       Les étapes, le texte et l'état sont dans Tab5/socle/tab5_demarrage.h (purs,
 *       testés sur PC) ; ce fichier n'ajoute que ce qui dépend d'ESP-IDF.
 * @architecture_constraint Rien ici ne journalise ni ne bloque : les marques tombent
 *       pendant setup() et avant main(). L'horloge est esp_timer (remis à zéro par
 *       ESP-IDF à l'étape CORE de son lancement, avant les constructeurs C++ :
 *       esp_system/startup.c, do_core_init puis do_global_ctors), jamais millis(), qui
 *       compte les ticks de FreeRTOS et vaut 0 jusqu'au lancement de l'ordonnanceur.
 *       Tablette virtuelle (tab5-rendu-host.yaml, sans ESP-IDF) : une horloge monotone.
 */
#include "tab5_demarrage.h"

#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#else
#include <chrono>
#endif

uint32_t demarrage_horloge_ms() {
#if defined(ESP_PLATFORM)
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
#else
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
#endif
}

bool demarrage_marquer(EtapeDemarrage e) { return demarrage_noter(e, demarrage_horloge_ms()); }

bool demarrage_noter_ordo(uint32_t millis_maintenant) {
    return demarrage_noter(EtapeDemarrage::ORDO, chrono_ordo(demarrage_horloge_ms(), millis_maintenant));
}

// « ctor » : construit avec les autres objets globaux (do_global_ctors d'ESP-IDF), avant
// l'ordonnanceur et app_main. Le constructeur d'ESP-Hosted (lien SDIO vers le C6) tourne
// dans la même phase, dans un ordre que rien ne fixe. chrono_demarrage() est une variable
// statique de fonction : prête quel que soit l'ordre des constructeurs.
namespace {
struct MarqueConstructeurs {
    MarqueConstructeurs() { demarrage_marquer(EtapeDemarrage::CTOR); }
};
const MarqueConstructeurs s_marque_constructeurs;
}  // namespace
