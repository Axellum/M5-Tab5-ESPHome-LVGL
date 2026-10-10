#pragma once
/**
 * [AI-CONTEXT]
 * @file tab5_cameras_charge.h
 * @role Chargeur d'images du popup Caméras (ADR-0049, « 2026-10-10 : hors de la boucle ») :
 *       télécharge un JPEG et le décode dans une tâche FreeRTOS à part, pendant que la
 *       boucle principale (LVGL, tactile, API) continue. Seul utilisateur :
 *       tab5_cameras.cpp, qui lance une image, sonde l'état et prend l'image prête.
 *         - Téléchargement : esp_http_client d'ESP-IDF, une connexion gardée ouverte d'une
 *           image à l'autre (pas de poignée de main TLS à chaque image), 12 s au plus.
 *         - Décodage : le décodeur JPEG MATÉRIEL de l'ESP32-P4 (esp_driver_jpeg), en
 *           RGB565 petit-boutiste, le format natif de LVGL : aucune conversion après.
 *         - Tampon de sortie (ADR-0057, 10/10/2026) : la tâche décode dans SON tampon ;
 *           camera_charge_prendre() le DONNE à la boucle principale (l'écran garde la
 *           dernière image de chaque caméra), qui le rend par camera_image_rendre() quand
 *           elle n'en veut plus : gardé pour le décodage suivant s'il est libre, sinon libéré.
 * @architecture_constraint Aucun lv_* ici, ni rien d'ESPHome hors du journal : la tâche ne
 *       touche qu'à ses tampons. Partage avec la boucle : un état atomique (LIBRE →
 *       EN_COURS → PRETE / ECHEC → LIBRE). Tant qu'il vaut EN_COURS, la boucle ne lit ni
 *       n'écrit rien du chargeur ; hors EN_COURS, la tâche dort.
 *       HTTPS sans vérifier le certificat : CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY, posé par
 *       le client http_request `assist_http` (verify_ssl: false, tab5-assist.yaml).
 *       Hors tablette (rendu, plateforme host) : un bouchon sans réseau qui rend tout de
 *       suite une mire calculée (une teinte par caméra, à la taille demandée) : les
 *       captures du rendu montrent la mise en page avec des images (ADR-0057).
 * @ai_warning [AI-WARNING] Une image prise appartient à l'écran : LVGL la lit tant qu'un
 *       widget la montre. Ne la rendre (camera_image_rendre) qu'après avoir retiré la source
 *       du widget et vidé le cache d'images de LVGL. Ne pas mettre la pile de la tâche en
 *       PSRAM (une écriture en flash, NVS par exemple, coupe le cache pendant que la tâche
 *       tourne).
 */
#include <cstddef>
#include <cstdint>

// Image décodée, prête à montrer (lv_image_dsc_t de tab5_cameras.cpp).
struct CameraImage {
    uint8_t* pixels = nullptr;   // RGB565 petit-boutiste, alloué par le pilote JPEG (PSRAM)
    size_t octets = 0;           // taille allouée (à rendre avec le tampon)
    uint32_t taille = 0;         // octets écrits par le décodeur
    int largeur = 0;             // pixels utiles
    int hauteur = 0;
    int pas = 0;                 // octets par ligne : largeur arrondie au bloc JPEG (8 ou 16)
};

enum class CameraCharge : uint8_t {
    LIBRE,
    EN_COURS,
    PRETE,
    ECHEC,
};

// Lance le téléchargement et le décodage de `url` (http:// ou https://) dans la tâche.
// Faux si un chargement est déjà en cours ou si la tâche ou le décodeur n'ont pas pu
// être créés (à traiter comme un échec).
bool camera_charge_lancer(const char* url);
// Lu par la boucle principale (sondage) : EN_COURS tant que la tâche travaille.
CameraCharge camera_charge_etat();
// PRETE : *img décrit l'image décodée, dont le tampon APPARTIENT désormais à l'appelant
// (à rendre par camera_image_rendre) ; l'état repasse à LIBRE. Faux si rien n'était prêt.
bool camera_charge_prendre(CameraImage* img);
// ECHEC ou PRETE (image non prise) → LIBRE.
void camera_charge_acquitter();
// Rend le tampon d'une image prise (aucun widget ne doit plus le montrer, [AI-WARNING]) :
// gardé pour le décodage suivant si le chargeur n'en a pas et ne travaille pas, sinon
// libéré. garder = false : toujours libéré (place rendue à la PSRAM, vues_borner()).
// *img est vidé. Sans effet sur une image vide.
void camera_image_rendre(CameraImage* img, bool garder = true);
// Rend la mémoire du chargeur : son tampon de sortie, le JPEG, la connexion (les images
// prises restent à l'écran, qui les rend). Sans effet pendant EN_COURS (renvoie faux).
bool camera_charge_liberer();
// PSRAM libre (octets) : l'écran borne les images gardées (ADR-0057). Hors tablette : SIZE_MAX.
size_t camera_psram_libre();
