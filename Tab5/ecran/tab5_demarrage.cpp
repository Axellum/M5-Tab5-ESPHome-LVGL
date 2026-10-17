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
 * qu'après setup(), et ~90 % du calcul de setup() partait dans deux parcours de LVGL 9.5
 * (vérifié sur 9.5.0 ; à relire dans lv_label.c et lv_theme_default.c si LVGL change) :
 *  1. Chaque label dont le texte est à recalculer s'abonne à l'affichage
 *     (lv_label_mark_need_refr_text, lv_label.c : lv_display_add_event_cb(…,
 *     LV_EVENT_UPDATE_LAYOUT_COMPLETED, label)) et ne se désabonne qu'à la fin d'une mise
 *     en page (lv_obj_update_layout). Avant la première image il n'y en a presque pas
 *     (seuls quelques appels de lv_image.c ou lv_obj_align_to() en font une) : ~1 300
 *     labels s'abonnent l'un après l'autre. Or chaque objet créé, restylé ou invalidé
 *     (lv_obj_mark_layout_as_dirty, lv_inv_area) envoie LV_EVENT_REFR_REQUEST à ce même
 *     affichage, dont l'envoi parcourt toute la liste : coût quadratique.
 *  2. ESPHome crée l'affichage en 240 × 240 (constructeur de LvglComponent,
 *     lvgl_esphome.cpp) et ne lui donne la taille de l'écran que dans son setup(), une
 *     fois les ~1 800 widgets construits : update_resolution (lv_display.c) marque la
 *     mise en page de chaque objet, et LV_EVENT_RESOLUTION_CHANGED réinitialise le thème
 *     par défaut de LVGL (resolution_change_event_cb, lv_theme_default.c : 240 px = petit
 *     écran, 1 024 ou 1 280 = grand), qui refait le style de tous les objets
 *     (lv_obj_report_style_change(NULL)) : encore un REFR_REQUEST par objet.
 * Mesure (tablette virtuelle, même runner, branche essai/mesure-demarrage-avant-apres
 * jamais mergée, 16/10/2026) : 14 549 M instructions dans setup() (callgrind), 1 308 M
 * avec les deux remèdes ci-dessous ; 1,5 s puis 0,18 s en natif. Le vidage seul (branche
 * essai/mesure-demarrage) était plus lent qu'aucun remède : 2,6 s contre 1,5 s. Sur la
 * Waveshare 7B (même jour) : première image à 5,6 s au lieu de 20,7 s.
 *
 * Remèdes, appelés une fois, juste après la création de l'affichage et avant les widgets,
 * par la lambda du premier style (style_repere_demarrage, tab5-styles.yaml : ESPHome
 * évalue le `theme:` puis les styles entre les deux) :
 *  1. la taille finale tout de suite (kEcranL × kEcranH, celle que LvglComponent::setup()
 *     calcule d'après la dalle et `lvgl: rotation:`) : le thème se réinitialise sur un
 *     affichage presque vide, et setup() trouve la même taille (lv_display_set_resolution
 *     ne fait alors rien). Si elles différaient un jour (autre carte), setup() reposerait
 *     la sienne : correct, seulement plus lent ;
 *  2. jusqu'à la première image, dès que l'affichage a kAbonnesMax abonnés, envoyer
 *     nous-mêmes LV_EVENT_UPDATE_LAYOUT_COMPLETED : les labels recalculent leur texte et se
 *     désabonnent, ce que LVGL fait à chaque mise en page. Retiré au premier
 *     LV_EVENT_REFR_START : LVGL met alors en page à chaque image et vide la liste
 *     lui-même, comme avant ce correctif. Si LVGL cessait un jour d'abonner les labels,
 *     la liste resterait courte et ce rappel ne ferait rien.
 * L'écran n'en change pas : un texte recalculé avant la mise en page l'est de nouveau
 * après si la taille ou le style du label changent (LV_EVENT_SIZE_CHANGED et
 * LV_EVENT_STYLE_CHANGED le réabonnent, lv_label_event) ; le thème ne choisit ses styles
 * selon la taille de l'écran que pour lv_keyboard, absent du firmware. À prouver à chaque
 * changement par le rendu de tous les écrans (job comparer de rendu-host.yml, « identique
 * aux références ») : fait pour la PR #478.
 * Affichage : lv_display_get_default(), le premier créé (lv_display_create) ; ESPHome n'en
 * crée qu'un, celui de LvglComponent.
 */
namespace {
constexpr uint32_t kAbonnesMax = 64;
// Garde indispensable : le vidage envoie lui-même des REFR_REQUEST (labels recalculés), et
// lv_display_get_event_count() compte encore les abonnés retirés tant que l'envoi le plus
// extérieur n'est pas fini (lv_event.c ne les libère qu'à ce moment) : sans elle, chaque
// REFR_REQUEST imbriqué relancerait un vidage.
bool s_vidage_en_cours = false;
// Demandes en cours (démarrage, bascule de thème) : le rappel reste branché jusqu'à la
// dernière fin, une bascule pendant le démarrage ne le retire pas avant la première image.
int s_vidage_demandes = 0;

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
    lv_display_remove_event_cb_with_user_data(disp, fin_du_vidage, disp);
    lvgl_vidage_abonnes(false);
}
}  // namespace

void lvgl_vidage_abonnes(bool actif) {
    lv_display_t* disp = lv_display_get_default();
    if (disp == nullptr) return;
    if (actif) {
        if (s_vidage_demandes++ == 0) lv_display_add_event_cb(disp, vider_abonnements, LV_EVENT_REFR_REQUEST, disp);
    } else if (s_vidage_demandes > 0 && --s_vidage_demandes == 0) {
        lv_display_remove_event_cb_with_user_data(disp, vider_abonnements, disp);
    }
}

void demarrage_preparer_lvgl() {
    lv_display_t* disp = lv_display_get_default();
    if (disp == nullptr) return;
    lv_display_set_resolution(disp, kEcranL, kEcranH);
    lvgl_vidage_abonnes(true);
    lv_display_add_event_cb(disp, fin_du_vidage, LV_EVENT_REFR_START, disp);
}
