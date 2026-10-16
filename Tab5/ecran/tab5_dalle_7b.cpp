/**
 * [AI-CONTEXT]
 * @file tab5_dalle_7b.cpp
 * @role Éteint la dalle de la Waveshare 7B quand le rétroéclairage s'éteint, et la rallume
 *       avec lui (16/10/2026, audit de la 7B).
 * @architecture_constraint Sur la 7B, le rétroéclairage est réglé en analogique (GPIO32 → filtre
 *       RC → contre-réaction du pilote AP3032, schéma Waveshare) : à 0 % les LED ne s'éteignent
 *       pas tout à fait, et la dernière image, figée par `lvgl.pause`, reste faiblement visible
 *       (signalé sur le forum Home Assistant, fil « 7B working config », 02/12/2025). La
 *       commande DCS « display off » (0x28) fait passer la dalle au noir, « display on » (0x29)
 *       la rallume : le rétroéclairage, lui, ne change pas.
 *       ESPHome 2026.9.1 n'envoie pas ces commandes après l'initialisation, et sa classe
 *       `mipi_dsi::MipiDsi` est `final`, son canal de commandes (`io_handle_`) protégé.
 *       On le lit par une instanciation explicite de modèle (C++ : les contrôles d'accès n'y
 *       s'appliquent pas, [temp.spec]) : pas de copie ni de patch du composant.
 * @ai_instruction Si ESPHome expose un jour une action d'allumage de la dalle, la prendre et
 *       supprimer ce fichier. Si le membre `io_handle_` change de nom ou de type, la compilation
 *       de la 7B échoue ici : relire `esphome/components/mipi_dsi/mipi_dsi.h`.
 */
#include "tab5_dalle_7b.h"

#include "esphome/components/mipi_dsi/mipi_dsi.h"
#include "esphome/core/log.h"
#include "esp_lcd_panel_io.h"

namespace {

constexpr const char* TAG = "tab5.dalle";
constexpr int kDcsAffichageEteint = 0x28;
constexpr int kDcsAffichageAllume = 0x29;

// Accès à MipiDsi::io_handle_ (voir l'en-tête du fichier).
struct CanalDsi {
    using type = esp_lcd_panel_io_handle_t esphome::mipi_dsi::MipiDsi::*;
    friend type canal_dsi(CanalDsi);
};
template <typename Cle, typename Cle::type Membre>
struct LireCanal {
    friend typename Cle::type canal_dsi(Cle) { return Membre; }
};
template struct LireCanal<CanalDsi, &esphome::mipi_dsi::MipiDsi::io_handle_>;

}  // namespace

void dalle_afficher(esphome::mipi_dsi::MipiDsi* ecran, bool allumee) {
    if (ecran == nullptr) return;
    esp_lcd_panel_io_handle_t io = ecran->*canal_dsi(CanalDsi{});
    if (io == nullptr) return;  // écran pas encore initialisé (rétroéclairage restauré avant lui)
    const int commande = allumee ? kDcsAffichageAllume : kDcsAffichageEteint;
    const esp_err_t err = esp_lcd_panel_io_tx_param(io, commande, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Dalle %s : commande 0x%02X refusee (%s)", allumee ? "allumee" : "eteinte", commande,
                 esp_err_to_name(err));
        return;
    }
    ESP_LOGI(TAG, "Dalle %s", allumee ? "allumee" : "eteinte");
}
