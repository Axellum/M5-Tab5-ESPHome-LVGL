/**
 * [AI-CONTEXT]
 * @file tab5_lien_c6.cpp
 * @role Tout redémarrage logiciel du P4 devient un reset du cœur numérique entier (niveau
 *       CORE), pour que le lien SDIO avec le co-processeur Wi-Fi (ESP32-C6) remonte au
 *       démarrage suivant ; et trace des pannes de ce lien (16/10/2026, boucle de
 *       redémarrages de la Waveshare 7B). Compilé pour les deux cartes (tab5-hardware.yaml,
 *       carte-waveshare-7b.yaml) : même lien C6 sur le Tab5, dont l'écran noir après OTA du
 *       05/08/2026 (lien jamais remonté, réglé par une coupure seule) est de la même famille.
 *       Sur le Tab5, essayé par compilation seulement (tablette au SAV le 16/10/2026).
 * @architecture_constraint Constat sur la 7B (capture série, docs/troubleshooting.md) : après
 *       quelques redémarrages logiciels, un démarrage voit le lien SDIO tomber (lectures en
 *       timeout, 0x107) ; le P4 redémarre alors par `esp_restart()`, et chaque démarrage
 *       suivant échoue pareil, jusqu'au mode sans échec. `esp_restart()` ne remet à zéro que
 *       les CPU du P4 (rst:0xc, SW_CPU_RESET) : l'état fautif, côté P4, y survit. Un reset
 *       par l'USB (niveau CORE, rst « USB peripheral ») a débloqué la carte aussitôt, sans
 *       toucher au C6 ; une coupure aussi. D'où `esp_rom_software_reset_system()` (le reset
 *       que le chargeur d'amorçage d'ESP-IDF utilise) à la place du reset des CPU, pour tout
 *       redémarrage logiciel : bouton, OTA, safe_mode, ESP-Hosted. ESP-IDF le rapporte comme
 *       un redémarrage logiciel (RESET_REASON_CORE_SW → ESP_RST_SW) : la raison affichée et
 *       le journal des démarrages ne changent pas. Les paniques ne passent pas par ici
 *       (pas de gestionnaire d'arrêt) et gardent le reset des CPU.
 *       Le gestionnaire est enregistré dans un constructeur, donc parmi les premiers :
 *       `esp_restart()` les appelle du dernier enregistré au premier, les autres (arrêt du
 *       Wi-Fi, ESPHome) passent avant lui.
 *       La panne du transport et la raison du dernier redémarrage du C6 sont journalisées.
 *       Deux versions d'ESP-Hosted selon ESPHome, choisies par `__has_include` :
 *       - 2.x (ESPHome ≤ 2026.10) : `_h_event_post` (table `g_hosted_osi_funcs`, en-tête
 *         public du composant) est enveloppé dès le constructeur ;
 *       - 3.x (ESPHome ≥ 2026.11, esphome#21394) : la table n'existe plus, ESP-Hosted poste
 *         ses événements (base `EH_HOST_EVENT`) sur la boucle d'événements par défaut. Le
 *         gestionnaire s'y inscrit dans `lien_c6_demarrage()` (setup()) : un constructeur
 *         ne peut pas créer la boucle (tâche + mutex avant l'ordonnanceur). Un CP_INIT
 *         posté avant setup() n'est donc pas journalisé ; la panne du transport, qui suit
 *         le démarrage, l'est.
 * @ai_instruction Ne pas revenir à `esp_restart()` seul sans refaire l'essai de
 *       docs/troubleshooting.md (redémarrages par l'API en série, capture COM8). Les
 *       messages écrits juste avant un reset passent par esp_rom_printf : un ESP_LOG* hors
 *       de la tâche principale attend loop() et serait perdu.
 */
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "tab5_lien_c6.h"

// ESP-Hosted 2.x (ESPHome ≤ 2026.10) a cet en-tête ; la 3.x (ESPHome ≥ 2026.11) ne l'a plus.
#if __has_include("esp_hosted_os_abstraction.h")
#define TAB5_ESP_HOSTED_2X 1
extern "C" {
#include "esp_hosted_os_abstraction.h"
#include "esp_hosted_event.h"
}
#else
#define TAB5_ESP_HOSTED_2X 0
#include "esp_event.h"
extern "C" {
#include "esp_hosted_event.h"
}
#endif

// En dernier : redéfinit ESP_LOG* vers le logger d'ESPHome (ceux d'ESP-IDF sont plafonnés à
// ERROR dans ce firmware, un ESP_LOGW n'y serait même pas compilé).
#include "esphome/core/log.h"

namespace {

const char* const TAG = "tab5.c6";

bool s_reset_branche = false;

// Dernier gestionnaire d'arrêt appelé par esp_restart() : reset CORE au lieu du reset des CPU.
void reset_coeur_entier() {
    esp_rom_software_reset_system();
}

// Commun aux deux versions : ce qu'on journalise d'un événement d'ESP-Hosted.
void journaliser(int32_t id, const void* data) {
    if (id == ESP_HOSTED_EVENT_TRANSPORT_FAILURE) {
        // Souvent suivi d'un redémarrage immédiat par ESP-Hosted.
        esp_rom_printf("\n[tab5.c6] panne du transport SDIO signalee par ESP-Hosted\n");
    } else if (id == ESP_HOSTED_EVENT_CP_INIT && data != nullptr) {
        const auto* init = static_cast<const esp_hosted_event_init_t*>(data);
        ESP_LOGI(TAG, "C6 pret, raison de son dernier redemarrage : %d", (int) init->reason);
    }
}

#if TAB5_ESP_HOSTED_2X
int (*s_event_post_origine)(esp_event_base_t, int32_t, void*, size_t, uint32_t) = nullptr;

int evenement(esp_event_base_t base, int32_t id, void* data, size_t taille, uint32_t attente) {
    if (base == ESP_HOSTED_EVENT) journaliser(id, data);
    return s_event_post_origine ? s_event_post_origine(base, id, data, taille, attente) : 0;
}
#else
bool s_evenements_branches = false;

void evenement(void*, esp_event_base_t, int32_t id, void* data) {
    journaliser(id, data);
}

// Dans setup() : la boucle par défaut existe déjà (ESP-Hosted la crée dans sa tâche
// d'init) ; sinon on la crée, ESP_ERR_INVALID_STATE voulant dire « déjà là ».
void brancher_evenements() {
    const esp_err_t boucle = esp_event_loop_create_default();
    if (boucle != ESP_OK && boucle != ESP_ERR_INVALID_STATE) return;
    s_evenements_branches =
        esp_event_handler_register(EH_HOST_EVENT, ESP_EVENT_ANY_ID, evenement, nullptr) == ESP_OK;
}
#endif

// Avant app_main : rien d'autre que des écritures en mémoire.
__attribute__((constructor)) void brancher_lien_c6() {
    s_reset_branche = esp_register_shutdown_handler(reset_coeur_entier) == ESP_OK;
#if TAB5_ESP_HOSTED_2X
    s_event_post_origine = g_hosted_osi_funcs._h_event_post;
    g_hosted_osi_funcs._h_event_post = evenement;
#endif
}

}  // namespace

// Valeur initiale du global `lien_c6_branche` (tab5-hardware.yaml, carte-waveshare-7b.yaml), évaluée dans
// setup() avant les composants. Elle garde aussi ce fichier dans le binaire : les sources
// d'ESPHome sont liées en bibliothèque statique, un objet dont rien n'est appelé serait
// jeté avec son constructeur.
bool lien_c6_demarrage() {
#if !TAB5_ESP_HOSTED_2X
    brancher_evenements();
    if (!s_evenements_branches) {
        ESP_LOGW(TAG, "Evenements d'ESP-Hosted non branches : pannes du lien C6 non journalisees");
    }
#endif
    if (!s_reset_branche) {
        ESP_LOGE(TAG, "Reset CORE non branche : un redemarrage logiciel ne fera qu'un reset des CPU");
    }
    return s_reset_branche;
}
