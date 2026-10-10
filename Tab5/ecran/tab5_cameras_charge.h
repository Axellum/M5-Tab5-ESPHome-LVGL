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
 *         - Deux tampons d'image : la tâche écrit dans celui qui n'est pas montré ;
 *           camera_charge_prendre() les échange dans la boucle principale.
 * @architecture_constraint Aucun lv_* ici, ni rien d'ESPHome hors du journal : la tâche ne
 *       touche qu'à ses tampons. Partage avec la boucle : un état atomique (LIBRE →
 *       EN_COURS → PRETE / ECHEC → LIBRE). Tant qu'il vaut EN_COURS, la boucle ne lit ni
 *       n'écrit rien du chargeur ; hors EN_COURS, la tâche dort.
 *       HTTPS sans vérifier le certificat : CONFIG_ESP_TLS_SKIP_SERVER_CERT_VERIFY, posé par
 *       le client http_request `assist_http` (verify_ssl: false, tab5-assist.yaml).
 *       Hors tablette (rendu, plateforme host) : un bouchon qui ne finit jamais (le popup
 *       reste sur « Chargement... », comme avant avec l'online_image muette).
 * @ai_warning [AI-WARNING] L'image montrée est lue par LVGL dans un des deux tampons :
 *       ne jamais appeler camera_charge_liberer() avant d'avoir caché le widget et retiré
 *       sa source. Ne pas mettre la pile de la tâche en PSRAM (une écriture en flash, NVS
 *       par exemple, coupe le cache pendant que la tâche tourne).
 */
#include <cstddef>
#include <cstdint>

// Image décodée, prête à montrer (lv_image_dsc_t de tab5_cameras.cpp).
struct CameraImage {
    const uint8_t* pixels = nullptr;   // RGB565 petit-boutiste
    uint32_t taille = 0;               // octets écrits par le décodeur
    int largeur = 0;                   // pixels utiles
    int hauteur = 0;
    int pas = 0;                       // octets par ligne : largeur arrondie au bloc JPEG (8 ou 16)
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
// PRETE : l'image décodée devient l'image montrée (échange des tampons), *img la décrit,
// l'état repasse à LIBRE. L'ancienne image montrée sera réécrite au chargement suivant :
// pointer le widget sur la nouvelle tout de suite. Faux si rien n'était prêt.
bool camera_charge_prendre(CameraImage* img);
// ECHEC ou PRETE (image non prise) → LIBRE.
void camera_charge_acquitter();
// Rend la mémoire : les deux images, le JPEG, la connexion. Sans effet pendant EN_COURS
// (renvoie faux). Le widget ne doit plus montrer d'image ([AI-WARNING]).
bool camera_charge_liberer();
