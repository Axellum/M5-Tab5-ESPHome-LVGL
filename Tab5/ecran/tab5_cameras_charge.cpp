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
#include "tab5_flux_mjpeg.h"
#include "esphome/core/log.h"
#include "driver/jpeg_decode.h"
#include "esp_heap_caps.h"
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
// Flux vidéo (ADR-0073) : le tampon de réception garde l'image la plus récente EN TÊTE et ce
// qui la suit derrière ; il grandit par doublement (128 Kio, une image courante tient dedans)
// jusqu'à une image gardée plus une qui arrive.
constexpr size_t kFluxTamponDebut = 128 * 1024;
constexpr size_t kFluxTamponMax = 2 * kJpegMax + 8192;
constexpr size_t kFluxLibreMin = 16 * 1024;   // place libre sous laquelle le tampon grandit
// Une lecture ne demande jamais plus que cela (esp_http_client_read() ne rend la main qu'une fois son
// tampon plein ou plus rien d'arrivé : sur un lien saturé, un grand tampon la ferait durer), et une
// image prête est rendue après ce temps de lecture même si des données arrivent encore.
constexpr size_t kFluxLectureMax = 32 * 1024;
constexpr uint32_t kFluxImageMs = 250;
// Lecture sans attente quand rien n'est arrivé : on réessaie à ce pas (au moins un tick).
constexpr TickType_t kFluxPasTicks = pdMS_TO_TICKS(5) > 0 ? pdMS_TO_TICKS(5) : 1;
constexpr uint32_t kFluxJournalImages = 30;   // une ligne de bilan toutes les 30 images décodées

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
// Tampon de sortie du décodeur : à la tâche ; donné à l'écran par camera_charge_prendre(),
// remplacé par un tampon rendu (camera_image_rendre) ou alloué au décodage suivant.
Tampon s_sortie;
CameraImage s_prete;     // décrit s_sortie quand l'état vaut PRETE
// Flux vidéo : à la tâche pendant EN_COURS, à la boucle sinon (comme les tampons ci-dessus).
bool s_flux_demande = false;           // le chargement lancé est une image de flux (pas une image fixe)
std::atomic<bool> s_flux_ouvert{false};   // s_http est un flux, déjà passé les en-têtes
char s_flux_cle[kCameraUrlMax] = {};   // URL du flux ouvert, sans le jeton (jusqu'au « ? »)
size_t s_flux_n = 0;                   // octets valides de s_jpeg_recu
size_t s_flux_pret = 0;                // l'image gardée occupe [0, s_flux_pret) ; 0 : aucune
uint32_t s_flux_decodees = 0;          // bilan de la ligne de journal
uint32_t s_flux_jetees = 0;
uint32_t s_flux_lecture_ms = 0;
uint32_t s_flux_decodage_ms = 0;
uint32_t s_flux_depuis_ms = 0;

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

// Décode s_jpeg_recu dans le tampon de sortie (jamais montré : l'écran a les siens).
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
    Tampon& dst = s_sortie;
    const size_t besoin = static_cast<size_t>(l) * h * 2;
    // Un tampon rendu par l'écran deux fois trop grand (une grande image d'1 Mio pour une
    // vignette de 255 Kio, mosaïque) : libéré plutôt que gardé avec la vignette.
    if (dst.p != nullptr && dst.n > 2 * besoin) tampon_rendre(dst);
    if (!tampon_au_moins(dst, besoin, JPEG_DEC_ALLOC_OUTPUT_BUFFER, 0)) {
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
    s_prete.octets = dst.n;
    s_prete.taille = ecrit;
    s_prete.largeur = static_cast<int>(info.width);
    s_prete.hauteur = static_cast<int>(info.height);
    s_prete.pas = static_cast<int>(l * 2);
    return true;
}

// ─── Flux vidéo (ADR-0073) ──────────────────────────────────────────────────────────
// Un flux « multipart/x-mixed-replace » de /api/camera_proxy_stream/ : la même connexion,
// les images l'une derrière l'autre. esp_http_client_read() REMPLIT le tampon demandé avant
// de rendre la main (il boucle tant que `len` n'est pas lu ou que le délai n'est pas écoulé,
// vérifié dans ESP-IDF 5.5.5) : sur un flux sans fin on lit donc SANS attente (délai 0 :
// -ESP_ERR_HTTP_EAGAIN quand rien n'est arrivé : un délai dépassé rend EAGAIN, 0 n'est qu'une
// fin de flux) et on réessaie au pas de kFluxPasTicks. Chaque lecture est bornée (kFluxLectureMax)
// et une image prête part après kFluxImageMs de lecture, pour qu'un lien saturé ne fige pas l'écran.

size_t flux_chemin_longueur(const char* url) {
    size_t i = 0;
    while (url[i] != '\0' && url[i] != '?') i++;
    return i;
}

void flux_fermer() {
    connexion_fermer();
    s_flux_ouvert.store(false);
    s_flux_cle[0] = '\0';
    s_flux_n = 0;
    s_flux_pret = 0;
}

// Ouvre le flux de s_url et passe ses en-têtes (HA les envoie quand la caméra donne sa
// première image : ffmpeg met une à trois secondes à démarrer, kDelaiHttpMs les couvre).
bool flux_ouvrir() {
    flux_fermer();
    esp_http_client_config_t cfg = {};
    cfg.url = s_url;
    cfg.timeout_ms = kDelaiHttpMs;
    cfg.buffer_size = 4096;
    cfg.buffer_size_tx = 1024;
    cfg.skip_cert_common_name_check = true;   // pas de keep_alive : le flux garde la connexion
    s_http = esp_http_client_init(&cfg);
    if (s_http == nullptr) return false;
    const esp_err_t err = esp_http_client_open(s_http, 0);
    const int64_t longueur = err == ESP_OK ? esp_http_client_fetch_headers(s_http) : -1;
    if (err != ESP_OK || longueur < 0) {
        ESP_LOGW(TAG, "flux : Home Assistant injoignable (%s)", err != ESP_OK ? esp_err_to_name(err) : "pas de réponse");
        connexion_fermer();
        return false;
    }
    const int statut = esp_http_client_get_status_code(s_http);
    if (statut != 200) {
        // 401 : jeton périmé ; 404 : pas un flux de caméra. Jamais l'URL (le jeton).
        ESP_LOGW(TAG, "flux : Home Assistant répond %d", statut);
        connexion_fermer();
        return false;
    }
    esp_http_client_set_timeout_ms(s_http, 0);   // lectures sans attente (voir plus haut)
    std::snprintf(s_flux_cle, sizeof(s_flux_cle), "%.*s", static_cast<int>(flux_chemin_longueur(s_url)), s_url);
    s_flux_ouvert.store(true);
    s_flux_depuis_ms = ms();
    s_flux_decodees = s_flux_jetees = s_flux_lecture_ms = s_flux_decodage_ms = 0;
    return true;
}

// Rend la taille de l'image la plus récente reçue, qui se trouve alors au début de
// s_jpeg_recu (l'adresse alignée que veut le décodeur), 0 en cas d'échec (flux fermé).
// Ouvre le flux s'il ne l'est pas déjà pour cette caméra (même URL au jeton près). Lit et
// range tout ce qui est déjà arrivé, jette les images plus anciennes sans les décoder, et
// n'attend (kDelaiHttpMs au plus) que si aucune image complète n'est encore là.
size_t flux_image() {
    const size_t lc = flux_chemin_longueur(s_url);
    const bool meme = s_flux_ouvert.load() && std::strlen(s_flux_cle) == lc && std::strncmp(s_flux_cle, s_url, lc) == 0;
    if (!meme && !flux_ouvrir()) return 0;
    // L'image rendue la fois d'avant est jetée : ce qui la suit remonte en tête.
    if (s_flux_pret > 0) {
        std::memmove(s_jpeg_recu.p, s_jpeg_recu.p + s_flux_pret, s_flux_n - s_flux_pret);
        s_flux_n -= s_flux_pret;
        s_flux_pret = 0;
    }
    if (!tampon_au_moins(s_jpeg_recu, kFluxTamponDebut, JPEG_DEC_ALLOC_INPUT_BUFFER, s_flux_n)) {
        ESP_LOGW(TAG, "flux : mémoire insuffisante");
        flux_fermer();
        return 0;
    }
    const uint32_t t0 = ms();
    for (;;) {
        // 1. Les images déjà complètes dans le tampon : la dernière gagne.
        for (;;) {
            flux::Image img;
            size_t consomme = 0;
            uint8_t* queue = s_jpeg_recu.p + s_flux_pret;
            const size_t qn = s_flux_n - s_flux_pret;
            const flux::Etat e = flux::extraire(queue, qn, kJpegMax, &img, &consomme);
            if (e == flux::Etat::IMAGE) {
                if (s_flux_pret > 0) s_flux_jetees++;
                const size_t debut = s_flux_pret + img.debut;
                std::memmove(s_jpeg_recu.p, s_jpeg_recu.p + debut, s_flux_n - debut);
                s_flux_n -= debut;
                s_flux_pret = img.n;
                continue;
            }
            if (e == flux::Etat::ATTENDRE) {
                if (consomme > 0) {   // la fin de l'image précédente (« \r\n »)
                    std::memmove(queue, queue + consomme, qn - consomme);
                    s_flux_n -= consomme;
                }
                break;
            }
            // Le format d'ESP_LOGW doit être un littéral (concaténé par le macro) : le texte en argument.
            ESP_LOGW(TAG, "flux : %s",
                     e == flux::Etat::FIN           ? "terminé par Home Assistant"
                     : e == flux::Etat::TROP_GRANDE ? "image trop grande (plus de 1 Mio)"
                                                    : "ce n'est pas un flux d'images JPEG");
            if (s_flux_pret > 0) return s_flux_pret;   // celle-ci d'abord ; l'échec revient à l'appel suivant
            flux_fermer();
            return 0;
        }
        // 2. Lire ce qui est déjà arrivé (sans attendre), le tampon agrandi au besoin.
        if (s_jpeg_recu.n - s_flux_n < kFluxLibreMin && s_jpeg_recu.n < kFluxTamponMax) {
            const size_t plus = s_jpeg_recu.n * 2 < kFluxTamponMax ? s_jpeg_recu.n * 2 : kFluxTamponMax;
            tampon_au_moins(s_jpeg_recu, plus, JPEG_DEC_ALLOC_INPUT_BUFFER, s_flux_n);   // raté : on lit dans ce qui reste
        }
        if (s_flux_n >= s_jpeg_recu.n) {
            ESP_LOGW(TAG, "flux : tampon plein (%u octets)", static_cast<unsigned>(s_flux_n));
            flux_fermer();
            return 0;
        }
        const size_t place = s_jpeg_recu.n - s_flux_n;
        const int lu = esp_http_client_read(s_http, reinterpret_cast<char*>(s_jpeg_recu.p) + s_flux_n,
                                            static_cast<int>(place < kFluxLectureMax ? place : kFluxLectureMax));
        if (lu > 0) {
            s_flux_n += static_cast<size_t>(lu);
            // Des données arrivent sans pause : l'image la plus récente d'abord, la suite à l'appel suivant.
            if (s_flux_pret > 0 && ms() - t0 >= kFluxImageMs) return s_flux_pret;
            continue;
        }
        if (lu == -ESP_ERR_HTTP_EAGAIN) {
            if (s_flux_pret > 0) return s_flux_pret;   // plus rien en attente : la plus récente
            if (ms() - t0 >= static_cast<uint32_t>(kDelaiHttpMs)) {
                ESP_LOGW(TAG, "flux : aucune image depuis %d s", kDelaiHttpMs / 1000);
                flux_fermer();
                return 0;
            }
            vTaskDelay(kFluxPasTicks);
            continue;
        }
        // 0 : Home Assistant a fermé ; négatif : erreur de lecture.
        if (s_flux_pret > 0) return s_flux_pret;   // l'image reçue d'abord ; la lecture rate encore à l'appel suivant
        if (lu == 0) ESP_LOGW(TAG, "flux : fermé par Home Assistant");
        else ESP_LOGW(TAG, "flux : erreur de lecture (%d)", lu);
        flux_fermer();
        return 0;
    }
}

// [AI-DEBUG] Une ligne de bilan toutes les kFluxJournalImages images décodées : débit réel,
// images jetées (le flux va plus vite que l'écran), temps de lecture (attente de la caméra
// comprise) et de décodage par image.
void flux_bilan(uint32_t lecture_ms, uint32_t decodage_ms) {
    s_flux_decodees++;
    s_flux_lecture_ms += lecture_ms;
    s_flux_decodage_ms += decodage_ms;
    if (s_flux_decodees < kFluxJournalImages) return;
    const uint32_t duree = ms() - s_flux_depuis_ms;
    const uint32_t dixiemes = duree > 0 ? s_flux_decodees * 10000u / duree : 0;
    ESP_LOGI(TAG, "flux : %u images en %u ms (%u,%u par seconde), %u jetées, lecture %u ms et décodage %u ms par image, pile libre %u",
             static_cast<unsigned>(s_flux_decodees), static_cast<unsigned>(duree), static_cast<unsigned>(dixiemes / 10),
             static_cast<unsigned>(dixiemes % 10), static_cast<unsigned>(s_flux_jetees),
             static_cast<unsigned>(s_flux_lecture_ms / s_flux_decodees),
             static_cast<unsigned>(s_flux_decodage_ms / s_flux_decodees),
             static_cast<unsigned>(uxTaskGetStackHighWaterMark(nullptr)));
    s_flux_decodees = s_flux_jetees = s_flux_lecture_ms = s_flux_decodage_ms = 0;
    s_flux_depuis_ms = ms();
}

void tache(void*) {
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        const uint32_t t0 = ms();
        if (s_flux_demande) {
            const size_t octets = flux_image();
            const uint32_t t1 = ms();
            const bool ok = octets > 0 && decoder(octets);
            if (ok) flux_bilan(t1 - t0, ms() - t1);
            else flux_fermer();   // le flux rate : l'écran passe aux images fixes
            poser(ok ? CameraCharge::PRETE : CameraCharge::ECHEC);
            continue;
        }
        if (s_flux_ouvert.load()) flux_fermer();   // une image fixe : la connexion de flux n'est pas la sienne
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

namespace {

// Le décodeur et la tâche, créés au premier chargement. Faux : rien n'a pu l'être.
bool preparer() {
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
    return true;
}

// Réveille la tâche sur `url` : une image fixe, ou la suivante du flux.
bool lancer(const char* url, bool flux) {
    if (camera_charge_etat() == CameraCharge::EN_COURS) return false;
    if (!preparer()) return false;
    std::snprintf(s_url, sizeof(s_url), "%s", url);
    s_flux_demande = flux;
    poser(CameraCharge::EN_COURS);
    xTaskNotifyGive(s_tache);
    return true;
}

}  // namespace

bool camera_charge_lancer(const char* url) { return lancer(url, false); }

bool camera_charge_lancer_flux(const char* url) { return lancer(url, true); }

bool camera_flux_ouvert() { return s_flux_ouvert.load(); }

bool camera_flux_fermer() {
    if (camera_charge_etat() == CameraCharge::EN_COURS) return false;
    flux_fermer();
    return true;
}

bool camera_charge_prendre(CameraImage* img) {
    if (camera_charge_etat() != CameraCharge::PRETE) return false;
    *img = s_prete;
    s_prete = {};
    s_sortie = {};   // donné à l'écran : le décodage suivant en aura un autre
    poser(CameraCharge::LIBRE);
    return true;
}

void camera_charge_acquitter() {
    if (camera_charge_etat() != CameraCharge::EN_COURS) poser(CameraCharge::LIBRE);
}

void camera_image_rendre(CameraImage* img, bool garder) {
    if (img == nullptr || img->pixels == nullptr) return;
    // La tâche ne touche à s_sortie que pendant EN_COURS : hors de là, il est à la boucle.
    if (garder && camera_charge_etat() != CameraCharge::EN_COURS && s_sortie.p == nullptr) {
        s_sortie.p = img->pixels;
        s_sortie.n = img->octets;
    } else {
        std::free(img->pixels);
    }
    *img = {};
}

bool camera_charge_liberer() {
    const CameraCharge e = camera_charge_etat();
    if (e == CameraCharge::EN_COURS) return false;
    if (e != CameraCharge::LIBRE) poser(CameraCharge::LIBRE);
    s_prete = {};
    tampon_rendre(s_sortie);
    flux_fermer();   // avant le tampon de réception : il en garde l'état
    tampon_rendre(s_jpeg_recu);
    return true;
}

size_t camera_psram_libre() { return heap_caps_get_free_size(MALLOC_CAP_SPIRAM); }

#else  // Rendu hors tablette (plateforme host) : rien n'est téléchargé.

#include <cstdlib>
#include <cstring>

// Une mire calculée à la place de l'image (ADR-0057) : la capture du rendu montre la mise
// en page avec des images (pièces, mosaïque, caméra hors ligne grisée) sans réseau. Taille
// = celle que HA renverrait pour une caméra 1920 × 1080 : la plus petite réduction en
// huitièmes qui couvre width × height de l'URL (camera_proxy, jamais sous la demande),
// 640 × 360 sans. Sur le Tab5, les demandes tombent juste (960 × 540, 480 × 270) ; sur
// la 7B, une vignette arrive en 480 × 270 pour une case de 356 × 198 : le rendu passe
// par la réduction de l'image (LV_DRAW_SW_SUPPORT_RGB565A8, build_flags), comme la
// carte. Teinte tirée du chemin de l'image (sans le jeton) : chaque caméra a la sienne,
// la même d'une image à l'autre.
namespace {

CameraCharge s_etat_hote = CameraCharge::LIBRE;
CameraImage s_prete_hote;

int parametre(const char* url, const char* cle, int defaut, int max) {
    const char* p = std::strstr(url, cle);
    if (p == nullptr) return defaut;
    const int v = std::atoi(p + std::strlen(cle));
    return v > 0 && v <= max ? v : defaut;
}

}  // namespace

CameraCharge camera_charge_etat() { return s_etat_hote; }

bool camera_charge_lancer(const char* url) {
    if (s_etat_hote == CameraCharge::EN_COURS) return false;
    int l = parametre(url, "width=", 0, 1920);
    int h = parametre(url, "height=", 0, 1080);
    if (l == 0 || h == 0) {
        l = 640;
        h = 360;
    } else {
        int huitiemes = 1;
        while (huitiemes < 8 && (240 * huitiemes < l || 135 * huitiemes < h)) huitiemes++;
        l = 240 * huitiemes;
        h = 135 * huitiemes;
    }
    uint32_t graine = 2166136261u;   // FNV-1a du chemin, jusqu'au « ? »
    for (const char* p = url; *p != '\0' && *p != '?'; p++) graine = (graine ^ static_cast<uint8_t>(*p)) * 16777619u;
    const size_t octets = static_cast<size_t>(l) * static_cast<size_t>(h) * 2;
    auto* px = static_cast<uint8_t*>(std::malloc(octets));
    if (px == nullptr) {
        s_etat_hote = CameraCharge::ECHEC;
        return true;
    }
    // Dégradé vertical (ciel → sol) dans la teinte de la caméra, bandes diagonales douces.
    // Bornes : r ≤ 13 + 12 < 32, g ≤ 27 + 23 < 64, b ≤ 15 + 14 < 32 (RGB565 sans débordement).
    const int r0 = 4 + static_cast<int>(graine % 10), g0 = 8 + static_cast<int>((graine >> 8) % 20),
              b0 = 6 + static_cast<int>((graine >> 16) % 10);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < l; x++) {
            const int bande = ((x + y) / 48) % 2;
            const int r = r0 + (y * 10) / h + bande * 2;
            const int g = g0 + (y * 20) / h + bande * 3;
            const int b = b0 + ((h - y) * 12) / h + bande * 2;
            const uint16_t v = static_cast<uint16_t>(((r & 31) << 11) | ((g & 63) << 5) | (b & 31));
            px[(static_cast<size_t>(y) * l + x) * 2] = static_cast<uint8_t>(v & 0xFF);
            px[(static_cast<size_t>(y) * l + x) * 2 + 1] = static_cast<uint8_t>(v >> 8);
        }
    }
    s_prete_hote = {px, octets, static_cast<uint32_t>(octets), l, h, l * 2};
    s_etat_hote = CameraCharge::PRETE;
    return true;
}

// Vidéo : la même mire (640 × 360, le flux n'a pas de width / height), une image par appel.
bool camera_charge_lancer_flux(const char* url) { return camera_charge_lancer(url); }

bool camera_flux_ouvert() { return false; }

bool camera_flux_fermer() { return s_etat_hote != CameraCharge::EN_COURS; }

bool camera_charge_prendre(CameraImage* img) {
    if (s_etat_hote != CameraCharge::PRETE) return false;
    *img = s_prete_hote;
    s_prete_hote = {};
    s_etat_hote = CameraCharge::LIBRE;
    return true;
}

void camera_charge_acquitter() {
    if (s_etat_hote != CameraCharge::EN_COURS) s_etat_hote = CameraCharge::LIBRE;
}

void camera_image_rendre(CameraImage* img, bool) {
    if (img == nullptr || img->pixels == nullptr) return;
    std::free(img->pixels);
    *img = {};
}

bool camera_charge_liberer() {
    if (s_etat_hote == CameraCharge::EN_COURS) return false;
    std::free(s_prete_hote.pixels);
    s_prete_hote = {};
    s_etat_hote = CameraCharge::LIBRE;
    return true;
}

size_t camera_psram_libre() { return SIZE_MAX; }

#endif
