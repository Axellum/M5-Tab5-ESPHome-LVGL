/**
 * [AI-CONTEXT]
 * @file tab5_journal.cpp
 * @role Journal des démarrages et des coupures, pour analyser les plantages et les
 *       pannes du lien Wi-Fi (co-processeur ESP32-C6) après coup (26/09/2026).
 *       - `journal_log_message()` (logger: on_message, tab5-hardware.yaml) garde les
 *         erreurs, et les avertissements quand Home Assistant n'est pas connecté :
 *         démarrage avant HA, lien Wi-Fi tombé. Les erreurs d'ESP-IDF (dont le pilote
 *         ESP-Hosted du C6) arrivent sous l'étiquette « esp-idf ».
 *       - Les lignes vivent en `.noinit` : elles survivent à un redémarrage logiciel,
 *         à un plantage, à un chien de garde et au reset par l'USB, pas à une coupure
 *         de courant. D'où `journal_tick()` (toutes les 30 s) : après 2 min sans HA,
 *         une copie part en NVS, relue au démarrage suivant si la RAM a été perdue.
 *       - À la reconnexion de HA, le script `tab5_journal_envoi` envoie l'événement
 *         `esphome.tab5_journal` si le journal contient autre chose qu'un démarrage
 *         normal, puis `journal_mark_delivered()` le vide.
 *       Le rapport de plantage d'ESPHome (étiquette « esp32.crash », PC et pile
 *       d'appels des deux cœurs) est écrit au démarrage : il entre dans le journal.
 * @regle_absolue Aucun log ici : `journal_log_message()` est appelée PAR le logger,
 *                un ESP_LOG* bouclerait. Aucune allocation sur ce chemin, sauf la
 *                relecture de la copie NVS (une fois par démarrage, tampon PSRAM rendu).
 * @memory_constraint 32 lignes de 104 o en `.noinit` (≈ 3,3 Ko de RAM interne).
 * Numérotation : #1 = premier démarrage depuis le dernier envoi ; #0 = la suite du
 * démarrage déjà signalé (ce qui arrive après l'envoi, sans redémarrer).
 */
#include "tab5_custom.h"
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kMagic = 0x4A524E31;  // « JRN1 »
constexpr uint32_t kPrefKey = 0x7A6A726E;
constexpr int kLignes = 32;
constexpr int kTexte = 96;

// Bits de Journal::anomalie
constexpr uint8_t kAvert = 1;     // au moins un avertissement
constexpr uint8_t kErreur = 2;    // au moins une erreur (ESPHome ou ESP-IDF)
constexpr uint8_t kPlantage = 4;  // un démarrage après plantage, chien de garde, baisse de tension

struct Ligne {
    uint32_t t_ms;     // millis() au moment du message
    uint16_t boot;     // numéro du démarrage depuis le dernier envoi
    uint16_t repet;    // répétitions identiques consécutives
    char niveau;       // 'E', 'W', ou '>' pour le repère de démarrage
    char texte[kTexte - 1];
};

struct Journal {
    uint32_t magic;
    uint16_t tete;        // prochaine ligne écrite
    uint16_t nb;          // lignes en attente d'envoi (≤ kLignes)
    uint16_t perdues;     // lignes écrasées faute de place
    uint16_t demarrages;  // démarrages depuis le dernier envoi
    uint8_t anomalie;
    uint8_t reserve[3];
    Ligne lignes[kLignes];
};

// Survit aux resets logiciels, comme les données de plantage d'ESPHome (crash_handler.cpp).
Journal s_j __attribute__((section(".noinit")));

bool s_session = false;         // .bss : faux à chaque démarrage
bool s_copie_en_nvs = false;    // une copie non vide attend en NVS
uint32_t s_hors_ligne_depuis = 0;
uint32_t s_derniere_copie = 0;
uint16_t s_nb_copie = 0xFFFF;
esphome::ESPPreferenceObject s_pref;
bool s_pref_ok = false;

bool raison_anormale(esp_reset_reason_t r) {
    return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
           r == ESP_RST_WDT || r == ESP_RST_BROWNOUT || r == ESP_RST_PWR_GLITCH ||
           r == ESP_RST_CPU_LOCKUP;
}

const char* raison_texte(esp_reset_reason_t r) {
    switch (r) {
        case ESP_RST_POWERON:    return "mise sous tension";
        case ESP_RST_EXT:        return "broche de reset";
        case ESP_RST_SW:         return "redémarrage logiciel";
        case ESP_RST_PANIC:      return "plantage (exception)";
        case ESP_RST_INT_WDT:    return "plantage (chien de garde d'interruption)";
        case ESP_RST_TASK_WDT:   return "plantage (chien de garde de tâche)";
        case ESP_RST_WDT:        return "plantage (chien de garde)";
        case ESP_RST_DEEPSLEEP:  return "sortie de veille profonde";
        case ESP_RST_BROWNOUT:   return "baisse de tension";
        case ESP_RST_SDIO:       return "reset SDIO";
        case ESP_RST_USB:        return "reset par l'USB";
        case ESP_RST_JTAG:       return "reset JTAG";
        case ESP_RST_EFUSE:      return "erreur eFuse";
        case ESP_RST_PWR_GLITCH: return "micro-coupure d'alimentation";
        case ESP_RST_CPU_LOCKUP: return "plantage (blocage du CPU)";
        default:                 return "inconnue";
    }
}

bool ha_connecte() {
    return esphome::api::global_api_server != nullptr &&
           esphome::api::global_api_server->is_connected_with_state_subscription();
}

// Copie `src` dans `dst` sans les codes couleur ANSI (ESC [ … m) ni fin de ligne.
void copier_sans_ansi(char* dst, size_t taille, const char* src) {
    size_t n = 0;
    for (const char* p = src; *p != '\0' && n + 1 < taille; ++p) {
        if (*p == '\033') {
            while (*p != '\0' && *p != 'm') ++p;
            if (*p == '\0') break;
            continue;
        }
        if (*p == '\r' || *p == '\n') continue;
        dst[n++] = *p;
    }
    dst[n] = '\0';
}

void ajouter(char niveau, const char* texte) {
    // Même message que la dernière ligne : on compte la répétition au lieu d'écraser
    // le journal (le Wi-Fi qui réessaie en boucle, par exemple).
    if (s_j.nb > 0) {
        Ligne& prec = s_j.lignes[(s_j.tete + kLignes - 1) % kLignes];
        if (prec.boot == s_j.demarrages && prec.niveau == niveau &&
            strncmp(prec.texte, texte, sizeof(prec.texte)) == 0) {
            if (prec.repet < 0xFFFF) prec.repet++;
            return;
        }
    }
    Ligne& l = s_j.lignes[s_j.tete];
    l.t_ms = esphome::millis();
    l.boot = s_j.demarrages;
    l.repet = 0;
    l.niveau = niveau;
    snprintf(l.texte, sizeof(l.texte), "%s", texte);
    s_j.tete = (uint16_t) ((s_j.tete + 1) % kLignes);
    if (s_j.nb < kLignes) s_j.nb++;
    else s_j.perdues++;
}

void preparer_pref() {
    if (s_pref_ok) return;
    s_pref = esphome::global_preferences->make_preference<Journal>(kPrefKey);
    s_pref_ok = true;
}

// Au premier appel de chaque démarrage : valide la RAM conservée (sinon, mise sous
// tension : on repart de la copie NVS s'il y en a une) et pose le repère du démarrage.
void ouvrir_session() {
    if (s_session) return;
    s_session = true;
    preparer_pref();  // global_preferences existe dès app_main(), avant le logger
    // Copie NVS relue une fois par démarrage, dans un tampon PSRAM rendu aussitôt :
    // ni 3,3 Ko sur la pile de la boucle, ni 3,3 Ko de RAM interne réservés à vie.
    auto* copie = static_cast<Journal*>(heap_caps_malloc(sizeof(Journal), MALLOC_CAP_SPIRAM));
    const bool copie_ok = copie != nullptr && s_pref.load(copie) && copie->magic == kMagic &&
                          copie->nb > 0 && copie->nb <= kLignes && copie->tete < kLignes;
    s_copie_en_nvs = copie_ok;
    const bool ram_ok = s_j.magic == kMagic && s_j.nb <= kLignes && s_j.tete < kLignes;
    if (!ram_ok) {
        if (copie_ok) memcpy(&s_j, copie, sizeof(s_j));
        else {
            memset(&s_j, 0, sizeof(s_j));
            s_j.magic = kMagic;
        }
    }
    heap_caps_free(copie);
    const esp_reset_reason_t r = esp_reset_reason();
    if (s_j.demarrages < 0xFFFF) s_j.demarrages++;
    if (raison_anormale(r)) s_j.anomalie |= kPlantage;
    char repere[kTexte];
    snprintf(repere, sizeof(repere), "démarrage, raison : %s", raison_texte(r));
    ajouter('>', repere);
}

void copier_en_nvs() {
    preparer_pref();
    s_pref.save(&s_j);
    esphome::global_preferences->sync();  // avant qu'on débranche la tablette
    s_copie_en_nvs = true;
    s_derniere_copie = esphome::millis();
    s_nb_copie = s_j.nb;
}

}  // namespace

void journal_log_message(uint8_t level, const char* tag, const char* message) {
    if (message == nullptr) return;
    ouvrir_session();
    const bool idf = tag != nullptr && strcmp(tag, "esp-idf") == 0;
    const bool crash = tag != nullptr && strcmp(tag, "esp32.crash") == 0;
    // Le rapport de plantage est écrit au démarrage, puis réécrit à l'abonnement aux
    // logs de chaque client : on ne garde que le premier passage.
    if (crash && esphome::millis() > 5000) return;
    const bool erreur = level <= ESPHOME_LOG_LEVEL_ERROR || idf;
    const bool avert = level == ESPHOME_LOG_LEVEL_WARN;
    if (!erreur && !(avert && !ha_connecte())) return;
    // Avertissements de durée d'ESPHome (« took a long time ») : attendus au démarrage.
    if (!erreur && strstr(message, "took a long time") != nullptr) return;
    char texte[kTexte];
    copier_sans_ansi(texte, sizeof(texte), message);
    s_j.anomalie |= erreur ? kErreur : kAvert;
    ajouter(erreur ? 'E' : 'W', texte);
}

void journal_tick() {
    ouvrir_session();
    const uint32_t maintenant = esphome::millis();
    if (ha_connecte()) {
        s_hors_ligne_depuis = 0;
        return;
    }
    if (s_hors_ligne_depuis == 0) {
        s_hors_ligne_depuis = maintenant == 0 ? 1 : maintenant;
        return;
    }
    // 2 min sans HA : copie en NVS, pour survivre à une coupure de courant. Ensuite au
    // plus une copie toutes les 15 min, et seulement s'il y a du nouveau.
    if (maintenant - s_hors_ligne_depuis < 120000) return;
    if (s_j.nb == s_nb_copie) return;
    if (s_derniere_copie != 0 && maintenant - s_derniere_copie < 900000) return;
    copier_en_nvs();
}

bool journal_has_report() {
    ouvrir_session();
    return s_j.nb > 0 && (s_j.anomalie != 0 || s_j.demarrages > 1 || s_j.perdues > 0);
}

bool journal_is_serious() {
    ouvrir_session();
    return (s_j.anomalie & (kErreur | kPlantage)) != 0 || s_j.demarrages > 1;
}

std::string journal_reset_reason() {
    return raison_texte(esp_reset_reason());
}

std::string journal_boot_count() {
    ouvrir_session();
    return std::to_string(s_j.demarrages);
}

std::string journal_report_text() {
    ouvrir_session();
    std::string out;
    out.reserve((size_t) s_j.nb * 80 + 64);
    if (s_j.perdues > 0) {
        char tete[64];
        snprintf(tete, sizeof(tete), "(%u lignes plus anciennes perdues)\n", (unsigned) s_j.perdues);
        out += tete;
    }
    const int debut = (s_j.tete + kLignes - s_j.nb) % kLignes;
    for (int i = 0; i < s_j.nb; ++i) {
        const Ligne& l = s_j.lignes[(debut + i) % kLignes];
        char ligne[kTexte + 48];
        if (l.niveau == '>') {
            snprintf(ligne, sizeof(ligne), "#%u %s\n", (unsigned) l.boot, l.texte);
        } else if (l.repet > 0) {
            snprintf(ligne, sizeof(ligne), "#%u +%u,%us %s (x%u)\n", (unsigned) l.boot,
                     (unsigned) (l.t_ms / 1000), (unsigned) ((l.t_ms % 1000) / 100), l.texte,
                     (unsigned) l.repet + 1);
        } else {
            snprintf(ligne, sizeof(ligne), "#%u +%u,%us %s\n", (unsigned) l.boot,
                     (unsigned) (l.t_ms / 1000), (unsigned) ((l.t_ms % 1000) / 100), l.texte);
        }
        out += ligne;
    }
    return out;
}

void journal_mark_delivered() {
    ouvrir_session();
    s_j.nb = 0;
    s_j.perdues = 0;
    s_j.demarrages = 0;
    s_j.anomalie = 0;
    if (s_copie_en_nvs) {
        copier_en_nvs();  // nb = 0 : la copie ne sera plus reprise
        s_copie_en_nvs = false;
    }
    s_nb_copie = 0xFFFF;
}
