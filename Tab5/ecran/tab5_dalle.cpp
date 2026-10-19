/**
 * [AI-CONTEXT]
 * @file tab5_dalle.cpp
 * @role Écran éteint = dalle vraiment éteinte, sur les deux cartes (audit anti-casse du
 *       16/10/2026, trou n° 5) : un voile noir couvre tout l'écran juste avant que LVGL se
 *       fige, puis la dalle reçoit, si elle les tient, les commandes DCS « display off »
 *       (0x28) et « sleep in » (0x10). Au rallumage : « sleep out » (0x11), 120 ms, « display
 *       on » (0x29), voile retiré, puis LVGL redessine l'écran entier.
 * @architecture_constraint Pourquoi : jusqu'ici « écran éteint » = rétroéclairage à 0 et LVGL
 *       en pause, la dalle continuait d'afficher l'image figée. Sur la 7B, le rétroéclairage
 *       est réglé en analogique (GPIO32 → filtre RC → contre-réaction de l'AP3032) et ne
 *       s'éteint pas tout à fait : l'image restait visible (vu par Axel le 16/10/2026, et sur
 *       le forum Home Assistant, fil « 7B working config »). Et une image fixe tenue des
 *       heures marque une dalle IPS (rémanence) ; les fabricants conseillent de l'éteindre.
 *       Le voile (PR #487) : sur une dalle IPS, des pixels noirs bloquent la lumière qui
 *       reste, et un écran noir n'est plus une image fixe. Objet plein écran sur la couche
 *       système de LVGL (au-dessus des popups), dessiné de force (`lv_refr_now`). Noir
 *       absolu (`lv_color_black()`), pas un rôle de la palette : aucun thème ne doit changer
 *       un écran éteint (règle 8 de check_tab5_code_rules.py ne vise que `lv_color_hex`).
 *       Les commandes DCS, selon la dalle (`TAB5_DALLE_EXTINCTION`) :
 *         0 = voile seul ; 1 = voile + 0x28 ; 2 = voile + 0x28 + 0x10.
 *       EK79007 (7B) : 2, tactile GT911 à part. Le 16/10/2026, 0x28 seul lui a été envoyé
 *       sans erreur mais sans effet visible (PR #487) ; 0x10 jamais essayé avant ce fichier.
 *       Les trois dalles du Tab5 : 0 tant qu'aucune tablette de la révision ne l'a essayé
 *       (celle de l'auteur est en panne depuis le 11/10/2026). ST7123 et ST7121 sont des
 *       puces écran + tactile (TDDI) : « sleep in » arrêterait sans doute aussi le tactile
 *       qui doit rallumer l'écran. ILI9881C : tactile GT911 à part, candidate au mode 2.
 *       Une dalle qui ne se réveillerait pas resterait noire jusqu'au redémarrage.
 *       Délais de la norme DCS (datasheet ILI9881C, pilotes Linux) : 120 ms entre « sleep
 *       in » et « sleep out », et entre « sleep out » et « sleep in » ; 120 ms après « sleep
 *       out » avant « display on ». Les deux premiers sont tenus ici (attente bloquante du
 *       reste, au plus 120 ms, seulement quand on rallume ou rééteint aussitôt) ; le
 *       troisième par le `delay` du script `tab5_dalle_rallumer`.
 *       ESPHome 2026.9.1 n'envoie aucune commande après l'initialisation et sa classe
 *       `mipi_dsi::MipiDsi` est `final`, son canal de commandes (`io_handle_`) protégé. On le
 *       lit par une instanciation explicite de modèle (C++ : les contrôles d'accès ne s'y
 *       appliquent pas, [temp.spec]) : pas de copie ni de patch du composant (PR #487).
 * @ai_instruction Le voile est cliquable pour absorber les appuis pendant une OTA de la 7B
 *       (LVGL tourne encore sous la dalle noire) : rien ne traverse vers les widgets cachés.
 *       Écran éteint, LVGL est figé et le toucher passe par `on_release` du tactile, qui
 *       rallume. Ne pas le mettre sur `lv_layer_top()` : un popup
 *       de la couche du haut passerait au-dessus. Passer une dalle à 1 ou 2 seulement après
 *       un essai sur la carte (lueur à 0 %, rallumage au toucher, après une OTA). Si
 *       `io_handle_` change de nom ou de type, la compilation échoue ici : relire
 *       `esphome/components/mipi_dsi/mipi_dsi.h`.
 * @sens Sens de l'écran (ADR-0069, 19/10/2026) : dalle_sens_choisir(), appelé par le select
 *       « Sens de l'écran » de la 7B (restauré de la NVS à la bande 800 du démarrage, puis à
 *       chaque choix dans Home Assistant). « Retourné » = 180° de plus que la rotation de repos :
 *       LvglComponent::set_rotation() à chaud (ESPHome 2026.9.1, rotation logicielle compilée
 *       parce que `lvgl: rotation:` est présent). En rotation 0, ESPHome envoie le tampon de
 *       LVGL tel quel (pas de PPA) : ~69 ms un écran entier au lieu de ~83 ms en 180 (mesuré le
 *       16/10/2026 avec `carte_rotation: "0"` compilé, le même chemin de code).
 * @ai_debug « Dalle eteinte (mode N) » / « Dalle rallumee » / « Sens de l'ecran : … (rotation
 *       N) » dans le journal (tag tab5.dalle) ; une commande refusée y est écrite avec son code.
 */
#include "tab5_dalle.h"

#include "esp_lcd_panel_io.h"
#include "esphome/components/lvgl/lvgl_esphome.h"
#include "esphome/components/mipi_dsi/mipi_dsi.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "lvgl.h"
#include "tab5_geometrie.h"  // rotation_sens(), rotation_repos() (ADR-0069)

#ifndef TAB5_DALLE_EXTINCTION
#define TAB5_DALLE_EXTINCTION 0
#endif
#ifndef TAB5_DALLE_NOIRE_OTA
#define TAB5_DALLE_NOIRE_OTA 0
#endif

namespace {

constexpr const char* TAG = "tab5.dalle";
constexpr int kMode = TAB5_DALLE_EXTINCTION;
static_assert(kMode >= 0 && kMode <= 2, "carte_dalle_extinction : 0, 1 ou 2");
constexpr uint8_t kDcsDormir = 0x10;
constexpr uint8_t kDcsReveiller = 0x11;
constexpr uint8_t kDcsEteindre = 0x28;
constexpr uint8_t kDcsAllumer = 0x29;
constexpr uint32_t kDelaiVeilleMs = 120;

lv_obj_t* s_voile = nullptr;
bool s_eteinte = false;           // 0x28 envoyé, 0x29 pas encore
bool s_endormie = false;          // 0x10 envoyé, 0x11 pas encore
uint32_t s_veille_ms = 0;         // dernier 0x10 ou 0x11 envoyé (millis)

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

bool commande(esphome::mipi_dsi::MipiDsi* ecran, uint8_t cmd) {
    if (ecran == nullptr) return false;
    esp_lcd_panel_io_handle_t io = ecran->*canal_dsi(CanalDsi{});
    if (io == nullptr) return false;  // écran pas encore initialisé
    const esp_err_t err = esp_lcd_panel_io_tx_param(io, cmd, nullptr, 0);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Commande 0x%02X refusee (%s)", cmd, esp_err_to_name(err));
        return false;
    }
    return true;
}

// 120 ms au moins entre deux changements de veille (0x10 ↔ 0x11) : attend le reste.
void attendre_veille() {
    const uint32_t ecoule = esphome::millis() - s_veille_ms;
    if (ecoule < kDelaiVeilleMs) esphome::delay(kDelaiVeilleMs - ecoule);
}

lv_obj_t* voile() {
    if (s_voile != nullptr) return s_voile;
    s_voile = lv_obj_create(lv_layer_sys());
    lv_obj_remove_style_all(s_voile);
    lv_obj_set_size(s_voile, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_voile, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_voile, LV_OPA_COVER, 0);
    lv_obj_add_flag(s_voile, LV_OBJ_FLAG_HIDDEN);
    return s_voile;
}

}  // namespace

void dalle_eteindre(esphome::mipi_dsi::MipiDsi* ecran) {
    if (lv_display_get_default() != nullptr) {
        lv_obj_remove_flag(voile(), LV_OBJ_FLAG_HIDDEN);
        lv_refr_now(nullptr);  // dessiné avant que lvgl.pause fige l'affichage
    }
    if (kMode >= 1 && !s_eteinte) s_eteinte = commande(ecran, kDcsEteindre);
    if (kMode >= 2 && !s_endormie) {
        attendre_veille();
        if (commande(ecran, kDcsDormir)) {
            s_endormie = true;
            s_veille_ms = esphome::millis();
        }
    }
    ESP_LOGI(TAG, "Dalle eteinte (mode %d)", kMode);
}

bool dalle_reveiller(esphome::mipi_dsi::MipiDsi* ecran) {
    if (!s_endormie) return false;
    attendre_veille();
    // Refusée : la dalle est dite réveillée quand même, pour ne pas rester noire à jamais
    // sur une erreur de transmission (le journal le dit).
    commande(ecran, kDcsReveiller);
    s_endormie = false;
    s_veille_ms = esphome::millis();
    return true;
}

void dalle_rallumer(esphome::mipi_dsi::MipiDsi* ecran) {
    // Rééteinte pendant l'attente du script sans que la lumière le sache : ne devrait pas
    // arriver (le script vérifie la lumière), mais une dalle qui dort sous un rétroéclairage
    // allumé serait un écran noir jusqu'au redémarrage.
    if (s_endormie && dalle_reveiller(ecran)) esphome::delay(kDelaiVeilleMs);
    if (s_eteinte) {
        attendre_veille();  // 120 ms après 0x11, même si le script a été relancé entre-temps
        commande(ecran, kDcsAllumer);
        s_eteinte = false;
    }
    if (s_voile == nullptr) return;  // aucune extinction depuis le démarrage
    lv_obj_add_flag(s_voile, LV_OBJ_FLAG_HIDDEN);
    ESP_LOGI(TAG, "Dalle rallumee");
}

bool dalle_noire_pendant_ota() { return TAB5_DALLE_NOIRE_OTA != 0; }

void dalle_sens_choisir(esphome::lvgl::LvglComponent* lvgl, bool retourne) {
    const int angle = rotation_sens(kRotationRepos, retourne);
    rotation_repos_courante() = angle;
    // Un jeu tient sa rotation (portrait) : il remettra la nouvelle en sortant. Même angle :
    // rien à redessiner.
    if (lvgl != nullptr && !rotation_tenue() && static_cast<int>(lvgl->get_rotation()) != angle)
        lvgl->set_rotation(angle);
    ESP_LOGI(TAG, "Sens de l'ecran : %s (rotation %d)", retourne ? "retourne" : "normal", angle);
}
