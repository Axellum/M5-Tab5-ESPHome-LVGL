/**
 * [AI-CONTEXT]
 * @file tab5_demarrage.cpp
 * @role Le démarrage, côté couche écran :
 *       - la chronologie (10/10/2026) : l'horloge des marques et l'étape « ctor ». Les
 *         étapes, le texte et l'état sont dans Tab5/socle/tab5_demarrage.h (purs, testés
 *         sur PC) ; ce fichier n'ajoute que ce qui dépend d'ESP-IDF ;
 *       - LVGL préparé avant les widgets (16/10/2026, demarrage_preparer_lvgl) : ce qui
 *         rendait le setup() d'ESPHome quadratique en nombre de labels (voir plus bas).
 * @architecture_constraint Rien ici ne journalise ni ne bloque : les marques tombent
 *       pendant setup() et avant main(). L'horloge est esp_timer (remis à zéro par
 *       ESP-IDF à l'étape CORE de son lancement, avant les constructeurs C++ :
 *       esp_system/startup.c, do_core_init puis do_global_ctors), jamais millis(), qui
 *       compte les ticks de FreeRTOS et vaut 0 jusqu'au lancement de l'ordonnanceur.
 *       Tablette virtuelle (tab5-rendu-host.yaml, sans ESP-IDF) : une horloge monotone.
 */
#include "tab5_demarrage.h"
#include "lvgl.h"
#include "tab5_geometrie.h"

#if defined(ESP_PLATFORM)
#include <esp_timer.h>
#else
#include <chrono>
#endif

uint32_t demarrage_horloge_ms() {
#if defined(ESP_PLATFORM)
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
#else
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
#endif
}

bool demarrage_marquer(EtapeDemarrage e) { return demarrage_noter(e, demarrage_horloge_ms()); }

bool demarrage_noter_ordo(uint32_t millis_maintenant) {
    return demarrage_noter(EtapeDemarrage::ORDO, chrono_ordo(demarrage_horloge_ms(), millis_maintenant));
}

// « ctor » : construit avec les autres objets globaux (do_global_ctors d'ESP-IDF), avant
// l'ordonnanceur et app_main. Le constructeur d'ESP-Hosted (lien SDIO vers le C6) tourne
// dans la même phase, dans un ordre que rien ne fixe. chrono_demarrage() est une variable
// statique de fonction : prête quel que soit l'ordre des constructeurs.
namespace {
struct MarqueConstructeurs {
    MarqueConstructeurs() { demarrage_marquer(EtapeDemarrage::CTOR); }
};
const MarqueConstructeurs s_marque_constructeurs;
}  // namespace

/*
 * [AI-WARNING] LVGL préparé avant les widgets (16/10/2026) — ne pas retirer sans mesurer.
 *
 * Pourquoi. L'écran restait noir ~21 s au démarrage (Tab5, 10/10/2026) : LVGL ne dessine
 * qu'après setup(), et ~90 % du calcul de setup() partait dans deux parcours de LVGL 9.5 :
 *  1. Chaque label dont le texte est à recalculer s'abonne à l'affichage
 *     (lv_label_mark_need_refr_text, lv_label.c : lv_display_add_event_cb(…,
 *     LV_EVENT_UPDATE_LAYOUT_COMPLETED, label)) et ne se désabonne qu'à la fin d'une mise
 *     en page. Aucune n'a lieu avant la première image : ~1 300 labels s'abonnent l'un
 *     après l'autre. Or chaque lv_obj_mark_layout_as_dirty() (création d'un objet, style
 *     changé) envoie LV_EVENT_REFR_REQUEST à ce même affichage, dont l'envoi parcourt
 *     toute la liste : coût quadratique.
 *  2. ESPHome crée l'affichage en 240 × 240 (constructeur de LvglComponent,
 *     lvgl_esphome.cpp) et ne lui donne la taille de l'écran que dans son setup(), une
 *     fois les ~1 800 widgets construits : update_resolution (lv_display.c) marque la
 *     mise en page de chaque objet, et LV_EVENT_RESOLUTION_CHANGED réinitialise le thème
 *     par défaut de LVGL (resolution_change_event_cb, lv_theme_default.c : 240 px = petit
 *     écran, 1 024 ou 1 280 = grand), qui refait le style de tous les objets
 *     (lv_obj_report_style_change(NULL)) : encore un REFR_REQUEST par objet.
 * Mesure (tablette virtuelle sous callgrind, branche essai/mesure-demarrage jamais mergée,
 * 16/10/2026) : 14 548 M instructions dans setup(), 1 290 M avec les deux remèdes
 * ci-dessous ; le vidage seul était plus lent qu'aucun remède (2,6 s contre 1,5 s en natif).
 *
 * Remèdes, appelés une fois, juste après la création de l'affichage et avant les widgets
 * (lambda du premier style, style_repere_demarrage de tab5-styles.yaml : le seul point
 * d'ESPHome entre les deux) :
 *  1. la taille finale tout de suite (kEcranL × kEcranH, celle que LvglComponent::setup()
 *     calcule d'après la dalle et `lvgl: rotation:`) : le thème se réinitialise sur un
 *     affichage presque vide, et setup() trouve la même taille (lv_display_set_resolution
 *     ne fait alors rien). Si elles différaient un jour (autre carte), setup() reposerait
 *     la sienne : correct, seulement plus lent ;
 *  2. jusqu'à la première image, dès que l'affichage a kAbonnesMax abonnés, envoyer
 *     nous-mêmes LV_EVENT_UPDATE_LAYOUT_COMPLETED : les labels recalculent leur texte et se
 *     désabonnent, ce que LVGL fait à chaque mise en page. Retiré au premier
 *     LV_EVENT_REFR_START : LVGL met alors en page à chaque image et vide la liste
 *     lui-même, comme avant ce correctif.
 * L'écran n'en change pas : un texte recalculé avant la mise en page l'est de nouveau
 * après si la taille ou le style du label changent (LV_EVENT_SIZE_CHANGED et
 * LV_EVENT_STYLE_CHANGED le réabonnent, lv_label_event) ; le thème ne choisit ses styles
 * selon la taille de l'écran que pour lv_keyboard, absent du firmware. Preuve : le rendu
 * de tous les écrans « identique aux références » (job comparer de rendu-host.yml).
 */
namespace {
constexpr uint32_t kAbonnesMax = 64;
bool s_vidage_en_cours = false;  // le vidage envoie des REFR_REQUEST : pas de réentrée

void vider_abonnements(lv_event_t* e) {
    if (s_vidage_en_cours) return;
    auto* disp = static_cast<lv_display_t*>(lv_event_get_user_data(e));
    if (lv_display_get_event_count(disp) < kAbonnesMax) return;
    s_vidage_en_cours = true;
    lv_display_send_event(disp, LV_EVENT_UPDATE_LAYOUT_COMPLETED, nullptr);
    s_vidage_en_cours = false;
}

// Retirer un rappel pendant l'envoi d'un événement est prévu par LVGL (lv_event.c : marqué,
// libéré à la fin de l'envoi), comme le fait chaque label qui se désabonne.
void fin_du_vidage(lv_event_t* e) {
    auto* disp = static_cast<lv_display_t*>(lv_event_get_user_data(e));
    lv_display_remove_event_cb_with_user_data(disp, vider_abonnements, disp);
    lv_display_remove_event_cb_with_user_data(disp, fin_du_vidage, disp);
}
}  // namespace

void demarrage_preparer_lvgl() {
    lv_display_t* disp = lv_display_get_default();
    if (disp == nullptr) return;
    lv_display_set_resolution(disp, kEcranL, kEcranH);
    lv_display_add_event_cb(disp, vider_abonnements, LV_EVENT_REFR_REQUEST, disp);
    lv_display_add_event_cb(disp, fin_du_vidage, LV_EVENT_REFR_START, disp);
}
