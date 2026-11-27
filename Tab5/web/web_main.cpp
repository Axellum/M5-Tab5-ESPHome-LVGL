/**
 * [AI-CONTEXT]
 * @file Tab5/web/web_main.cpp
 * @role Démo web SEULEMENT (essai, étape 0) : la tablette virtuelle
 *       (tab5-rendu-host.yaml, plateforme host d'ESPHome) compilée en WebAssembly par
 *       emscripten (tools/web/construire.py). Ce fichier n'est pas une source
 *       d'ESPHome : le script l'ajoute au build, à côté des sources générées.
 *       - main() : celui du host d'ESPHome (components/host/core.cpp, compilé sous un
 *         autre nom) tourne `while (1) loop();`, ce qu'un navigateur ne permet pas.
 *         Ici, setup() puis un loop() par image du navigateur
 *         (emscripten_set_main_loop, requestAnimationFrame).
 *       - Image : le flush de LVGL est enveloppé ; chaque zone dessinée est copiée,
 *         en coordonnées LOGIQUES (paysage 1280×720 sur le Tab5, avant la rotation
 *         logicielle d'ESPHome), dans un tampon RGBA que la page affiche tel quel.
 *       - Toucher : le doigt virtuel des captures (Tab5/rendu/rendu_doigt.h), dans les
 *         mêmes coordonnées logiques.
 *
 * @architecture_constraint Pas de réseau ni de sommeil dans un navigateur :
 *       construire.py lie avec --wrap=socket (aucune socket : l'API d'ESPHome et le
 *       réveil de la boucle échouent proprement à l'init) et --wrap=nanosleep
 *       (delay() rend la main tout de suite au lieu de bloquer la page).
 */
#include <emscripten.h>

#include <pthread.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "esphome.h"
#include "lvgl_private.h"
#include "rendu_doigt.h"

void setup();
void loop();

namespace {

lv_display_flush_cb_t g_flush_esphome = nullptr;  // flush d'ESPHome, appelé après la copie
uint8_t* g_rgba = nullptr;                        // image logique, 4 octets par point
int32_t g_largeur = 0;
int32_t g_hauteur = 0;
uint32_t g_version = 0;  // +1 à chaque zone copiée : la page ne repeint que si ça change
double g_duree_max_ms = 0;  // tour de boucle le plus long depuis la dernière lecture (mesure)

// Tampon à la résolution logique courante (un jeu peut passer l'écran en portrait).
void web_tampon(int32_t largeur, int32_t hauteur) {
    if (largeur == g_largeur && hauteur == g_hauteur && g_rgba != nullptr) return;
    free(g_rgba);
    g_rgba = static_cast<uint8_t*>(calloc(static_cast<size_t>(largeur) * hauteur, 4));
    g_largeur = largeur;
    g_hauteur = hauteur;
    for (size_t i = 3; g_rgba != nullptr && i < static_cast<size_t>(largeur) * hauteur * 4; i += 4) g_rgba[i] = 255;
}

// Zone dessinée par LVGL : RGB565 petit-boutiste (byte_order: little_endian du
// firmware), lignes contiguës (LV_DRAW_BUF_STRIDE_ALIGN 1), coordonnées logiques.
void web_flush(lv_display_t* disp, const lv_area_t* zone, uint8_t* px) {
    web_tampon(lv_display_get_horizontal_resolution(disp), lv_display_get_vertical_resolution(disp));
    if (g_rgba != nullptr) {
        const int32_t l = lv_area_get_width(zone);
        const uint16_t* src = reinterpret_cast<const uint16_t*>(px);
        for (int32_t y = zone->y1; y <= zone->y2; y++) {
            if (y < 0 || y >= g_hauteur) {
                src += l;
                continue;
            }
            for (int32_t x = zone->x1; x <= zone->x2; x++, src++) {
                if (x < 0 || x >= g_largeur) continue;
                const uint16_t c = *src;
                uint8_t* d = g_rgba + (static_cast<size_t>(y) * g_largeur + x) * 4;
                d[0] = static_cast<uint8_t>(((c >> 11) & 0x1F) * 255 / 31);
                d[1] = static_cast<uint8_t>(((c >> 5) & 0x3F) * 255 / 63);
                d[2] = static_cast<uint8_t>((c & 0x1F) * 255 / 31);
            }
        }
        g_version++;
    }
    g_flush_esphome(disp, zone, px);
}

// Doigt : LVGL ne lit le pointeur qu'à chaque période de son minuteur d'entrée
// (~30 ms), un clic de souris peut poser et lever entre deux lectures. La levée attend
// donc que l'appui ait été lu une fois.
bool g_appui_lu = true;
bool g_lever_differe = false;

void web_doigt_lire(lv_indev_t* indev, lv_indev_data_t* data) {
    rendu_doigt_lire(indev, data);
    if (data->state == LV_INDEV_STATE_PRESSED) {
        if (g_appui_lu && g_lever_differe) {
            g_lever_differe = false;
            rendu_doigt_lever();
        }
        g_appui_lu = true;
    }
}

void web_tour() {
    // Le flush d'ESPHome est posé au setup() du composant LVGL : on l'enveloppe une
    // fois qu'il existe.
    if (g_flush_esphome == nullptr) {
        lv_display_t* disp = lv_display_get_default();
        if (disp != nullptr && disp->flush_cb != nullptr) {
            g_flush_esphome = disp->flush_cb;
            disp->flush_cb = web_flush;
            lv_obj_invalidate(lv_screen_active());
        }
    }
    const double debut = emscripten_get_now();
    loop();
    const double duree = emscripten_get_now() - debut;
    if (duree > g_duree_max_ms) g_duree_max_ms = duree;
}

}  // namespace

extern "C" {

// Pas de réseau dans la page (voir @architecture_constraint).
int __wrap_socket(int, int, int) {
    errno = EAFNOSUPPORT;
    return -1;
}

// Le journal d'ESPHome (host) écrit le nom du fil d'exécution ; emscripten sans fils
// n'a pas cette fonction. Un seul fil ici : celui de la page.
int pthread_getname_np(pthread_t, char* nom, size_t taille) {
    if (nom != nullptr && taille > 0) snprintf(nom, taille, "page");
    return 0;
}

// delay() d'ESPHome : rendre la main tout de suite plutôt que d'attendre en boucle.
int __wrap_nanosleep(const struct timespec*, struct timespec* reste) {
    if (reste != nullptr) {
        reste->tv_sec = 0;
        reste->tv_nsec = 0;
    }
    return 0;
}

EMSCRIPTEN_KEEPALIVE uint8_t* web_image() { return g_rgba; }
EMSCRIPTEN_KEEPALIVE int32_t web_largeur() { return g_largeur; }
EMSCRIPTEN_KEEPALIVE int32_t web_hauteur() { return g_hauteur; }
EMSCRIPTEN_KEEPALIVE uint32_t web_version() { return g_version; }

// Mesure : durée (ms) du plus long tour de boucle depuis l'appel précédent.
EMSCRIPTEN_KEEPALIVE double web_duree_max_ms() {
    const double d = g_duree_max_ms;
    g_duree_max_ms = 0;
    return d;
}

// Doigt posé (appuye = 1, déplacé s'il l'était déjà) ou levé (0), en coordonnées
// logiques.
EMSCRIPTEN_KEEPALIVE void web_toucher(int32_t x, int32_t y, int32_t appuye) {
    if (appuye) {
        const bool nouveau = !rendu_doigt().appuye;
        rendu_doigt_poser(x, y);
        lv_indev_set_read_cb(rendu_doigt().indev, web_doigt_lire);
        if (nouveau) g_appui_lu = false;
        g_lever_differe = false;
    } else if (g_appui_lu) {
        rendu_doigt_lever();
    } else {
        g_lever_differe = true;  // levé à la lecture qui suit le premier appui lu
    }
}

}  // extern "C"

int main() {
    esphome::host::setup_preferences();
    setup();
    emscripten_set_main_loop(web_tour, 0, false);
    return 0;
}
