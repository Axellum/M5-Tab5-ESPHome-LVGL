/**
 * [AI-CONTEXT]
 * @file tab5_barre_demarrage.cpp
 * @role Barre de chargement du démarrage (16/10/2026, demande d'Axel) : dessinée sur la dalle
 *       dès qu'elle est prête (on_boot 500), jusqu'à la première image de LVGL, qui n'arrive
 *       qu'après setup(). Le dessin, la rotation et la progression sont purs
 *       (Tab5/socle/tab5_barre.h) ; ici : la dalle, la tâche, la mémoire.
 * @architecture_constraint Pendant setup(), la tâche principale est prise par les setup()
 *       des composants (plusieurs secondes d'un bloc) : une petite tâche FreeRTOS, sans
 *       affinité, redessine la barre toutes les 40 ms ; la tâche principale ne fait que
 *       changer d'étape. Une seule écrit sur la dalle à la fois : la tâche jusqu'à
 *       barre_demarrage_fermer(), qui l'attend avant de dessiner la barre pleine. Rien ne
 *       touche à LVGL. Hors tablette (rendu, pas d'ESP-IDF) : ne fait rien.
 * @ai_instruction Les couleurs viennent de la palette active (UIColor, thème déjà restauré
 *       à la bande 800) : fond = BG, comme la première image, sans saut de couleur.
 */
#include "tab5_custom.h"
#include "tab5_barre.h"
#include "tab5_geometrie.h"

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <atomic>

namespace {

const char* const TAG = "tab5.barre";

struct EtatBarre {
    esphome::display::Display* ecran = nullptr;
    barre::Rect ui{};
    barre::Rect dalle{};
    int rotation = 0;  // celle de LVGL au démarrage (rotation_repos())
    uint16_t* src = nullptr;  // la barre à l'endroit
    uint16_t* dst = nullptr;  // la même, tournée pour la dalle
    uint32_t fond = 0, piste = 0, plein = 0;
    int dessines = -1;  // pixels remplis à l'écran
    // Étape en cours : écrite par la tâche principale, lue par la tâche de la barre.
    portMUX_TYPE verrou = portMUX_INITIALIZER_UNLOCKED;
    int etape = 0;
    float depart = 0.0f;
    uint32_t debut_ms = 0;
    float montree = 0.0f;  // fraction affichée
    std::atomic<bool> arret{false};
    SemaphoreHandle_t finie = nullptr;
};
EtatBarre g_barre;

uint32_t maintenant_ms() { return static_cast<uint32_t>(esp_timer_get_time() / 1000); }

uint16_t* allouer(size_t pixels) {
    void* p = heap_caps_malloc(pixels * sizeof(uint16_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (p == nullptr) p = heap_caps_malloc(pixels * sizeof(uint16_t), MALLOC_CAP_8BIT);
    return static_cast<uint16_t*>(p);
}

void envoyer(const barre::Rect& r, const uint16_t* pixels) {
    g_barre.ecran->draw_pixels_at(r.x, r.y, r.l, r.h, reinterpret_cast<const uint8_t*>(pixels),
                                  esphome::display::COLOR_ORDER_RGB, esphome::display::COLOR_BITNESS_565, false);
}

void dessiner(int remplis) {
    auto& b = g_barre;
    if (remplis == b.dessines) return;
    b.dessines = remplis;
    barre::peindre(b.src, b.ui.l, b.ui.h, remplis, b.fond, b.piste, b.plein);
    barre::tourner(b.src, b.dst, b.ui.l, b.ui.h, b.rotation);
    envoyer(b.dalle, b.dst);
}

// Toute la dalle à la couleur du fond, par bandes (le tampon d'écran de la dalle contient
// encore ce qu'il avait avant le redémarrage, ou du bruit).
bool peindre_fond(int dalle_l, int dalle_h) {
    constexpr int kBande = 16;
    uint16_t* bande = allouer(static_cast<size_t>(dalle_l) * kBande);
    if (bande == nullptr) return false;
    const uint16_t c = barre::rgb565(g_barre.fond);
    for (int i = 0; i < dalle_l * kBande; i++) bande[i] = c;
    for (int y = 0; y < dalle_h; y += kBande) {
        const int h = dalle_h - y < kBande ? dalle_h - y : kBande;
        envoyer(barre::Rect{0, y, dalle_l, h}, bande);
    }
    heap_caps_free(bande);
    return true;
}

void tache_barre(void*) {
    auto& b = g_barre;
    while (!b.arret.load()) {
        portENTER_CRITICAL(&b.verrou);
        const int etape = b.etape;
        const float depart = b.depart;
        const uint32_t debut = b.debut_ms;
        portEXIT_CRITICAL(&b.verrou);
        const barre::Jalon& j = barre::kJalons[etape];
        const float f = barre::progression(depart, j.cible, maintenant_ms() - debut, j.duree_ms);
        portENTER_CRITICAL(&b.verrou);
        b.montree = f;
        portEXIT_CRITICAL(&b.verrou);
        dessiner(barre::remplis(f, b.ui.l));
        vTaskDelay(pdMS_TO_TICKS(40));
    }
    xSemaphoreGive(b.finie);
    vTaskDelete(nullptr);
}

void liberer() {
    auto& b = g_barre;
    heap_caps_free(b.src);
    heap_caps_free(b.dst);
    b.src = b.dst = nullptr;
    if (b.finie != nullptr) vSemaphoreDelete(b.finie);
    b.finie = nullptr;
    b.ecran = nullptr;
}

}  // namespace

bool barre_demarrage_ouvrir(esphome::display::Display* ecran) {
    auto& b = g_barre;
    if (ecran == nullptr || ecran->is_failed() || b.ecran != nullptr) return false;
    const int dalle_l = ecran->get_native_width();
    const int dalle_h = ecran->get_native_height();
    b.ecran = ecran;
    b.ui = barre::geometrie(kEcranL, kEcranH);
    // Le sens de l'écran (ADR-0069) est déjà restauré de la NVS (select de la carte, bande 800).
    b.rotation = rotation_repos();
    b.dalle = barre::rect_dalle(b.ui, b.rotation, dalle_l, dalle_h);
    b.fond = UIColor.BG;
    b.piste = UIColor.ARC_TRACK;
    b.plein = UIColor.ACCENT;
    b.src = allouer(static_cast<size_t>(b.ui.l) * b.ui.h);
    b.dst = allouer(static_cast<size_t>(b.ui.l) * b.ui.h);
    b.finie = xSemaphoreCreateBinary();
    if (b.src == nullptr || b.dst == nullptr || b.finie == nullptr || !peindre_fond(dalle_l, dalle_h)) {
        ESP_LOGW(TAG, "Barre de chargement abandonnee (memoire)");
        liberer();
        return false;
    }
    b.etape = 0;
    b.depart = 0.0f;
    b.debut_ms = maintenant_ms();
    b.arret.store(false);
    dessiner(0);
    if (xTaskCreate(tache_barre, "tab5_barre", 4096, nullptr, 1, nullptr) != pdPASS) {
        // Sans tâche, la barre reste vide jusqu'à la première image : mieux qu'un écran noir.
        ESP_LOGW(TAG, "Barre de chargement sans tache");
        xSemaphoreGive(b.finie);
    }
    return true;
}

void barre_demarrage_etape(int etape) {
    auto& b = g_barre;
    if (b.ecran == nullptr) return;
    if (etape < 0) etape = 0;
    if (etape >= barre::kEtapes) etape = barre::kEtapes - 1;
    portENTER_CRITICAL(&b.verrou);
    b.depart = b.montree;
    b.etape = etape;
    b.debut_ms = maintenant_ms();
    portEXIT_CRITICAL(&b.verrou);
}

void barre_demarrage_fermer() {
    auto& b = g_barre;
    if (b.ecran == nullptr) return;
    b.arret.store(true);
    if (xSemaphoreTake(b.finie, pdMS_TO_TICKS(1000)) != pdTRUE) {
        // La tâche peut encore écrire : ne rien libérer sous elle (quelques Ko perdus).
        ESP_LOGW(TAG, "Tache de la barre pas arretee");
        b.ecran = nullptr;
        return;
    }
    dessiner(b.ui.l);
    liberer();
}

#else  // Rendu hors tablette : pas de dalle à part, LVGL dessine seul.

bool barre_demarrage_ouvrir(esphome::display::Display*) { return false; }
void barre_demarrage_etape(int) {}
void barre_demarrage_fermer() {}

#endif
