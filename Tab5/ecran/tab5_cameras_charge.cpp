/**
 * [AI-CONTEXT]
 * @file tab5_cameras_charge.cpp
 * @role Chargeur d'images du popup Caméras hors de la boucle principale (tâche FreeRTOS,
 *       esp_http_client, décodeur JPEG matériel). Contrat et contraintes :
 *       tab5_cameras_charge.h.
 * @ai_instruction [AI-DEBUG] Une ligne INFO par image (« tab5.cameras ») : taille, octets,
 *       temps de HA et du décodage. Mesures du 10/10/2026 : ADR-0049.
 */
#include "tab5_cameras_charge.h"

#if defined(ESP_PLATFORM)

#include "tab5_parse.h"
#include "esphome/core/log.h"
#include "driver/jpeg_decode.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

const char* const TAG = "tab5.cameras";

constexpr uint32_t kPileOctets = 12288;      // esp_http_client + mbedTLS (poignée de main)
constexpr UBaseType_t kPriorite = 1;         // celle de la boucle d'ESPHome (loopTask)
constexpr BaseType_t kCoeur = 0;             // la boucle d'ESPHome tourne sur le cœur 1
constexpr int kDelaiHttpMs = 12000;          // comme assist_http (tab5-assist.yaml)
constexpr int kDelaiJpegMs = 2000;           // décodeur matériel : quelques ms en VGA
constexpr size_t kJpegPrevu = 64 * 1024;     // sans Content-Length (réponse par morceaux)
constexpr size_t kJpegMax = 1024 * 1024;     // au-delà : refusé
// 1280 × 736 (arrondi au bloc de 16) : HA réduit à 960 × 540 ou au facteur JPEG juste
// au-dessus (1152 × 648 pour une 2304 × 1296), plus grand n'arrive pas en pratique.
constexpr uint32_t kPixelsMax = 1280u * 736u;

struct Tampon {
    uint8_t* p = nullptr;
    size_t n = 0;   // taille allouée (alignée par le pilote)
};

std::atomic<uint8_t> s_etat{static_cast<uint8_t>(CameraCharge::LIBRE)};
TaskHandle_t s_tache = nullptr;
jpeg_decoder_handle_t s_jpeg = nullptr;
esp_http_client_handle_t s_http = nullptr;
char s_url[kCameraUrlMax] = {};
Tampon s_jpeg_recu;
Tampon s_image[2];
int s_montree = 0;       // index du tampon que LVGL montre ; la tâche écrit dans l'autre
CameraImage s_prete;     // décrit l'autre tampon quand l'état vaut PRETE

void poser(CameraCharge e) { s_etat.store(static_cast<uint8_t>(e), std::memory_order_release); }

uint32_t ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

// Tampon d'au moins `n` octets alloué par le pilote JPEG (aligné, accessible au DMA, en
// PSRAM). `garder` octets du contenu sont recopiés quand il faut un tampon plus grand.
bool tampon_au_moins(Tampon& t, size_t n, jpeg_dec_buffer_alloc_direction_t sens, size_t garder) {
    if (t.p != nullptr && t.n >= n) return true;
    jpeg_decode_memory_alloc_cfg_t cfg = {};
    cfg.buffer_direction = sens;
    size_t alloue = 0;
    auto* p = static_cast<uint8_t*>(jpeg_alloc_decoder_mem(n, &cfg, &alloue));
    if (p == nullptr) return false;
    if (garder > 0 && t.p != nullptr) std::memcpy(p, t.p, garder);
    std::free(t.p);
    t.p = p;
    t.n = alloue;
    return true;
}

void tampon_rendre(Tampon& t) {
    std::free(t.p);
    t = {};
}

void connexion_fermer() {
    if (s_http == nullptr) return;
    esp_http_client_cleanup(s_http);
    s_http = nullptr;
}

// Lit le corps de la réponse dans s_jpeg_recu. Renvoie sa taille, 0 en cas d'échec.
size_t lire_corps(int64_t longueur) {
    if (longueur > static_cast<int64_t>(kJpegMax)) {
        ESP_LOGW(TAG, "image refusée : %lld octets", static_cast<long long>(longueur));
        return 0;
    }
    const size_t prevu = longueur > 0 ? static_cast<size_t>(longueur) : kJpegPrevu;
    if (!tampon_au_moins(s_jpeg_recu, prevu, JPEG_DEC_ALLOC_INPUT_BUFFER, 0)) return 0;
    size_t n = 0;
    for (;;) {
        if (longueur > 0 && n >= static_cast<size_t>(longueur)) return n;
        if (n == s_jpeg_recu.n) {
            const size_t plus = n * 2 < kJpegMax ? n * 2 : kJpegMax;
            if (plus <= n || !tampon_au_moins(s_jpeg_recu, plus, JPEG_DEC_ALLOC_INPUT_BUFFER, n)) {
                ESP_LOGW(TAG, "image refusée : plus de %u octets", static_cast<unsigned>(n));
                return 0;
            }
        }
        const int lu = esp_http_client_read(s_http, reinterpret_cast<char*>(s_jpeg_recu.p) + n,
                                            static_cast<int>(s_jpeg_recu.n - n));
        if (lu < 0) return 0;
        if (lu == 0) return esp_http_client_is_complete_data_received(s_http) ? n : 0;
        n += static_cast<size_t>(lu);
    }
}

// Télécharge s_url. Une connexion gardée d'une image à l'autre que HA aurait fermée entre-
// temps échoue à la première lecture : un second essai, sur une connexion neuve.
size_t telecharger() {
    for (int essai = 0; essai < 2; essai++) {
        const bool reprise = s_http != nullptr;
        if (!reprise) {
            esp_http_client_config_t cfg = {};
            cfg.url = s_url;
            cfg.timeout_ms = kDelaiHttpMs;
            cfg.buffer_size = 4096;
            cfg.buffer_size_tx = 1024;
            cfg.keep_alive_enable = true;
            cfg.skip_cert_common_name_check = true;
            s_http = esp_http_client_init(&cfg);
            if (s_http == nullptr) return 0;
        } else if (esp_http_client_set_url(s_http, s_url) != ESP_OK) {
            connexion_fermer();
            continue;
        }
        esp_err_t err = esp_http_client_open(s_http, 0);
        const int64_t longueur = err == ESP_OK ? esp_http_client_fetch_headers(s_http) : -1;
        if (err != ESP_OK || longueur < 0) {
            connexion_fermer();
            if (reprise) continue;
            ESP_LOGW(TAG, "Home Assistant injoignable (%s)", esp_err_to_name(err));
            return 0;
        }
        const int statut = esp_http_client_get_status_code(s_http);
        if (statut != 200) {
            // 401 : jeton périmé ; 500 / 502 : caméra hors ligne. Jamais l'URL (le jeton).
            ESP_LOGW(TAG, "Home Assistant répond %d", statut);
            connexion_fermer();
            return 0;
        }
        const size_t n = lire_corps(longueur);
        if (n == 0) {
            connexion_fermer();
            if (reprise) continue;
            return 0;
        }
        // HA a répondu « Connection: close » : la garder ferait échouer le premier essai de
        // chaque image suivante.
        if (!esp_http_client_is_persistent_connection(s_http)) connexion_fermer();
        return n;
    }
    return 0;
}

// Décode s_jpeg_recu dans le tampon qui n'est pas montré.
bool decoder(size_t n) {
    jpeg_decode_picture_info_t info = {};
    if (jpeg_decoder_get_info(s_jpeg_recu.p, n, &info) != ESP_OK || info.width == 0 || info.height == 0) {
        ESP_LOGW(TAG, "JPEG illisible (%u octets)", static_cast<unsigned>(n));
        return false;
    }
    // Le décodeur écrit des blocs entiers : 16 × 16 en 4:2:0, 16 × 8 en 4:2:2, 8 × 8 sinon.
    uint32_t bx = 8, by = 8;
    if (info.sample_method == JPEG_DOWN_SAMPLING_YUV420) bx = by = 16;
    else if (info.sample_method == JPEG_DOWN_SAMPLING_YUV422) bx = 16;
    const uint32_t l = (info.width + bx - 1) / bx * bx;
    const uint32_t h = (info.height + by - 1) / by * by;
    if (l * h > kPixelsMax) {
        ESP_LOGW(TAG, "image trop grande : %ux%u", static_cast<unsigned>(info.width), static_cast<unsigned>(info.height));
        return false;
    }
    Tampon& dst = s_image[1 - s_montree];
    if (!tampon_au_moins(dst, static_cast<size_t>(l) * h * 2, JPEG_DEC_ALLOC_OUTPUT_BUFFER, 0)) {
        ESP_LOGW(TAG, "mémoire insuffisante pour %ux%u", static_cast<unsigned>(l), static_cast<unsigned>(h));
        return false;
    }
    jpeg_decode_cfg_t cfg = {};
    cfg.output_format = JPEG_DECODE_OUT_FORMAT_RGB565;
    cfg.rgb_order = JPEG_DEC_RGB_ELEMENT_ORDER_BGR;   // petit-boutiste : LV_COLOR_FORMAT_RGB565
    cfg.conv_std = JPEG_YUV_RGB_CONV_STD_BT601;
    uint32_t ecrit = 0;
    const esp_err_t err = jpeg_decoder_process(s_jpeg, &cfg, s_jpeg_recu.p, static_cast<uint32_t>(n), dst.p,
                                               static_cast<uint32_t>(dst.n), &ecrit);
    if (err != ESP_OK) {
        // JPEG progressif, 4:1:1, niveaux de gris… : non pris en charge par le matériel.
        ESP_LOGW(TAG, "décodage refusé (%s)", esp_err_to_name(err));
        return false;
    }
    s_prete.pixels = dst.p;
    s_prete.taille = ecrit;
    s_prete.largeur = static_cast<int>(info.width);
    s_prete.hauteur = static_cast<int>(info.height);
    s_prete.pas = static_cast<int>(l * 2);
    return true;
}

void tache(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const uint32_t t0 = ms();
        const size_t n = telecharger();
        const uint32_t t1 = ms();
        const bool ok = n > 0 && decoder(n);
        // [AI-DEBUG] Pile restante au plus bas (octets) : la poignée de main TLS est le pic.
        if (ok)
            ESP_LOGI(TAG, "image %dx%d, %u octets : HA %u ms, décodage %u ms, pile libre %u", s_prete.largeur,
                     s_prete.hauteur, static_cast<unsigned>(n), static_cast<unsigned>(t1 - t0),
                     static_cast<unsigned>(ms() - t1), static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
        poser(ok ? CameraCharge::PRETE : CameraCharge::ECHEC);
    }
}

}  // namespace

CameraCharge camera_charge_etat() { return static_cast<CameraCharge>(s_etat.load(std::memory_order_acquire)); }

bool camera_charge_lancer(const char* url) {
    if (camera_charge_etat() == CameraCharge::EN_COURS) return false;
    if (s_jpeg == nullptr) {
        jpeg_decode_engine_cfg_t cfg = {};
        cfg.intr_priority = 0;
        cfg.timeout_ms = kDelaiJpegMs;
        if (jpeg_new_decoder_engine(&cfg, &s_jpeg) != ESP_OK) {
            s_jpeg = nullptr;
            ESP_LOGE(TAG, "décodeur JPEG matériel indisponible");
            return false;
        }
    }
    if (s_tache == nullptr &&
        xTaskCreatePinnedToCore(tache, "tab5_cameras", kPileOctets, nullptr, kPriorite, &s_tache, kCoeur) != pdPASS) {
        s_tache = nullptr;
        ESP_LOGE(TAG, "tâche de chargement impossible à créer");
        return false;
    }
    std::snprintf(s_url, sizeof(s_url), "%s", url);
    poser(CameraCharge::EN_COURS);
    xTaskNotifyGive(s_tache);
    return true;
}

bool camera_charge_prendre(CameraImage* img) {
    if (camera_charge_etat() != CameraCharge::PRETE) return false;
    s_montree = 1 - s_montree;
    *img = s_prete;
    poser(CameraCharge::LIBRE);
    return true;
}

void camera_charge_acquitter() {
    if (camera_charge_etat() != CameraCharge::EN_COURS) poser(CameraCharge::LIBRE);
}

bool camera_charge_liberer() {
    const CameraCharge e = camera_charge_etat();
    if (e == CameraCharge::EN_COURS) return false;
    if (e != CameraCharge::LIBRE) poser(CameraCharge::LIBRE);
    tampon_rendre(s_image[0]);
    tampon_rendre(s_image[1]);
    tampon_rendre(s_jpeg_recu);
    connexion_fermer();
    return true;
}

#else  // Rendu hors tablette : rien n'est téléchargé, le popup reste sur « Chargement... ».

CameraCharge camera_charge_etat() { return CameraCharge::EN_COURS; }
bool camera_charge_lancer(const char*) { return true; }
bool camera_charge_prendre(CameraImage*) { return false; }
void camera_charge_acquitter() {}
bool camera_charge_liberer() { return false; }

#endif
