/**
 * [AI-CONTEXT]
 * @file tab5_lien_c6.cpp
 * @role Redémarrage du P4 quand le lien SDIO avec le co-processeur Wi-Fi (ESP32-C6) ne
 *       monte pas, et trace de l'état de ce lien (16/10/2026, boucle de redémarrages de la
 *       Waveshare 7B après un redémarrage logiciel).
 * @architecture_constraint ESP-Hosted (pilote du C6 côté P4) appelle toutes ses fonctions
 *       système par une table globale et modifiable, `g_hosted_osi_funcs`
 *       (esp_hosted_os_abstraction.h, en-tête public du composant). Ce fichier y remplace
 *       deux entrées au démarrage, sans toucher à la bibliothèque :
 *       - `_h_restart_host` : ESP-Hosted redémarre le P4 quand le transport SDIO casse
 *         (CONFIG_ESP_HOSTED_TRANSPORT_RESTART_ON_FAILURE). Le sien est un `esp_restart()`,
 *         un reset des seuls cœurs du P4 : GPIO, LDO et le reste gardent leur état, et le
 *         C6 ne reçoit que l'impulsion de 10 ms d'ESP-Hosted au démarrage suivant. Une
 *         coupure de courant, elle, remet tout à zéro, et c'est la seule chose qui a
 *         débloqué la 7B le 16/10/2026. Ici : le C6 est tenu en reset pendant le
 *         redémarrage, puis, au 2e échec d'affilée, toute la puce est réinitialisée par le
 *         chien de garde RTC (WDT_STAGE_ACTION_RESET_RTC, le plus proche d'une coupure
 *         sans couper).
 *       - `_h_event_post` : chaque événement d'ESP-Hosted passe par là ; on y lit la montée
 *         du lien (remise à zéro du compteur d'échecs), sa panne et la raison du dernier
 *         redémarrage du C6, puis on appelle la fonction d'origine.
 *       Le compteur vit en RTC_NOINIT : il survit aux redémarrages logiciels et au chien de
 *       garde, pas à une coupure.
 * @hardware Broche de reset et polarité lues dans le sdkconfig écrit par `esp32_hosted:`
 *       (CONFIG_ESP_HOSTED_GPIO_SLAVE_RESET_SLAVE, CONFIG_ESP_HOSTED_RESET_GPIO_ACTIVE_LOW) :
 *       GPIO54 sur la 7B (EN du C6 par 0 Ω, 10 kΩ de tirage et 1 µF), GPIO15 sur le Tab5.
 *       « Actif haut » = le C6 tourne quand la broche est haute.
 * @ai_instruction Les messages du redémarrage passent par esp_rom_printf : le P4 repart
 *       aussitôt, un ESP_LOG* écrit hors de la tâche principale attendrait loop() (tampon
 *       des tâches du logger) et serait perdu.
 */
#include <cstdint>

#include "esp_attr.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/wdt_hal.h"
#include "soc/rtc.h"
#include "sdkconfig.h"
#include "tab5_lien_c6.h"

extern "C" {
#include "esp_hosted_os_abstraction.h"
#include "esp_hosted_event.h"
}

namespace {

const char* const TAG = "tab5.c6";

constexpr gpio_num_t kResetC6 = static_cast<gpio_num_t>(CONFIG_ESP_HOSTED_GPIO_SLAVE_RESET_SLAVE);
#ifdef CONFIG_ESP_HOSTED_RESET_GPIO_ACTIVE_LOW
constexpr uint32_t kNiveauReset = 1;  // C6 en marche à l'état bas
#else
constexpr uint32_t kNiveauReset = 0;  // C6 en marche à l'état haut (7B, Tab5)
#endif

constexpr uint32_t kMagie = 0x7B0C6A11;
struct EtatLien {
    uint32_t magie;
    uint32_t echecs;  // redémarrages demandés par ESP-Hosted depuis le dernier lien monté
};
RTC_NOINIT_ATTR EtatLien s_lien;

int (*s_event_post_origine)(esp_event_base_t, int32_t, void*, size_t, uint32_t) = nullptr;

void etat_valide() {
    if (s_lien.magie != kMagie) {
        s_lien.magie = kMagie;
        s_lien.echecs = 0;
    }
}

// Reset de toute la puce par le chien de garde RTC : cœurs, périphériques HP et domaine
// LP, comme au réveil d'une coupure, l'alimentation en moins. La broche de reset du C6
// retombe en entrée : EN remonte par son 10 kΩ, ESP-Hosted refera son impulsion.
[[noreturn]] void reset_puce_entiere() {
    wdt_hal_context_t ctx;
    wdt_hal_init(&ctx, WDT_RWDT, 0, false);
    const uint32_t ticks = rtc_clk_slow_freq_get_hz() / 20;  // 50 ms
    wdt_hal_write_protect_disable(&ctx);
    wdt_hal_config_stage(&ctx, WDT_STAGE0, ticks, WDT_STAGE_ACTION_RESET_RTC);
    wdt_hal_enable(&ctx);
    wdt_hal_write_protect_enable(&ctx);
    while (true) esp_rom_delay_us(1000);
}

int redemarrer_hote() {
    etat_valide();
    const uint32_t n = ++s_lien.echecs;
    const char* tache = pcTaskGetName(nullptr);
    esp_rom_printf(
        "\n[tab5.c6] lien SDIO avec le C6 perdu : ESP-Hosted redemarre le P4 "
        "(tache %s, echec %u d'affilee)\n",
        tache ? tache : "?", (unsigned) n);
    // Le C6 reste en reset pendant tout le redémarrage du P4 (esp_restart ne touche pas
    // aux GPIO), jusqu'à l'impulsion d'ESP-Hosted au démarrage suivant (~1 s).
    gpio_set_level(kResetC6, kNiveauReset);
    // Comme hosted_restart_host() : l'arrêt du Wi-Fi passerait par le C6, qui ne répond plus.
    esp_unregister_shutdown_handler(reinterpret_cast<shutdown_handler_t>(esp_wifi_stop));
    if (n >= 2) {
        esp_rom_printf("[tab5.c6] reset complet de la puce (chien de garde RTC)\n");
        reset_puce_entiere();
    }
    esp_restart();
    return 0;
}

int evenement(esp_event_base_t base, int32_t id, void* data, size_t taille, uint32_t attente) {
    if (base == ESP_HOSTED_EVENT) {
        etat_valide();
        if (id == ESP_HOSTED_EVENT_TRANSPORT_UP) {
            ESP_LOGW(TAG, "Lien SDIO avec le C6 monte (apres %u redemarrage(s) demande(s) par ESP-Hosted)",
                     (unsigned) s_lien.echecs);
            s_lien.echecs = 0;
        } else if (id == ESP_HOSTED_EVENT_TRANSPORT_FAILURE) {
            esp_rom_printf("\n[tab5.c6] panne du transport SDIO signalee par ESP-Hosted\n");
        } else if (id == ESP_HOSTED_EVENT_CP_INIT && data != nullptr) {
            const auto* init = static_cast<const esp_hosted_event_init_t*>(data);
            ESP_LOGW(TAG, "C6 pret, raison de son dernier redemarrage : %d", (int) init->reason);
        }
    }
    return s_event_post_origine ? s_event_post_origine(base, id, data, taille, attente) : 0;
}

// Avant app_main : la table est une donnée statique, rien d'autre n'est touché ici.
__attribute__((constructor)) void brancher_lien_c6() {
    etat_valide();
    s_event_post_origine = g_hosted_osi_funcs._h_event_post;
    g_hosted_osi_funcs._h_event_post = evenement;
    g_hosted_osi_funcs._h_restart_host = redemarrer_hote;
}

}  // namespace

// Valeur initiale du global `lien_c6_branche` (carte-waveshare-7b.yaml), évaluée dans
// setup() avant les composants. Elle garde aussi ce fichier dans le binaire : les sources
// d'ESPHome sont liées en bibliothèque statique, un objet dont rien n'est appelé serait
// jeté avec son constructeur.
bool lien_c6_demarrage() {
    etat_valide();
    if (s_lien.echecs > 0) {
        ESP_LOGW(TAG, "Demarrage apres %u redemarrage(s) demande(s) par ESP-Hosted (lien C6)",
                 (unsigned) s_lien.echecs);
    }
    return g_hosted_osi_funcs._h_restart_host == redemarrer_hote;
}
