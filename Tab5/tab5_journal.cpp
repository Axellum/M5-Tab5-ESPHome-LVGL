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
 *         de courant. D'où `journal_tick()` (toutes les 30 s) : quand le Wi-Fi manque
 *         depuis 90 s, une copie part en NVS, relue au démarrage suivant si la RAM a
 *         été perdue.
 *       - À la reconnexion de HA, le script `tab5_journal_envoi` envoie l'événement
 *         `esphome.tab5_journal` si quelque chose le justifie (bits `anomalie`, plus
 *         bas), puis `journal_mark_delivered()` le vide. Les lignes écrites avant la
 *         première connexion à HA ne sont que du contexte : au démarrage, le lien
 *         ESP-Hosted « not yet up » et les drapeaux d'état d'ESPHome sont normaux
 *         (fausse alerte au flash du 26/09/2026, 22:23).
 *       Le rapport de plantage d'ESPHome (étiquette « esp32.crash », PC et pile
 *       d'appels des deux cœurs) est écrit par Logger::pre_setup(), AVANT que le
 *       déclencheur `on_message` existe (main.cpp généré : pre_setup() puis
 *       `new LoggerMessageTrigger`) : il n'arrive jamais ici au démarrage, seulement
 *       aux abonnements aux logs, bien après 5 s (lignes ignorées). Depuis le 06/10/2026
 *       le journal le lit par `esp32::crash_handler_has_data()` (posé par arch_init(),
 *       avant tout) et le rejoue (`crash_handler_log()`, hors du chemin du logger : au
 *       premier journal_tick() ou à l'envoi) quand la raison du reset est un plantage ;
 *       une fois le journal livré à HA, `crash_handler_clear()`, comme ESPHome après
 *       un abonnement aux logs : sinon un vieux rapport jamais lu ferait passer chaque
 *       appui sur le bouton d'alimentation pour un plantage.
 * @regle_absolue Aucun log ici : `journal_log_message()` est appelée PAR le logger,
 *                un ESP_LOG* bouclerait (seul rejouer_rapport() en fait écrire, et
 *                jamais depuis ce chemin : journal_tick() et journal_has_report()). Aucune allocation sur ce chemin, sauf la
 *                relecture de la copie NVS (une fois par démarrage, tampon PSRAM rendu).
 * @memory_constraint 32 lignes de 104 o en `.noinit` (≈ 3,3 Ko de RAM interne).
 * Numérotation : #1 = premier démarrage depuis le dernier envoi ; #0 = la suite du
 * démarrage déjà signalé (ce qui arrive après l'envoi, sans redémarrer).
 * Tablette neuve (28/09/2026) : une marque en NVS, absente juste après un effacement.
 * Le flash par l'USB (page d'installation en mode téléchargement) finit par un reset du
 * chien de garde RTC : ESP_RST_WDT, « other watchdogs » pour ESPHome. Sans la marque,
 * le premier démarrage d'une installation passait pour un plantage (alerte sur le
 * téléphone, vu à l'installation à neuf du 28/09). Seul ce cas est excusé : une panique
 * ou un chien de garde de tâche alertent toujours, même au premier démarrage. (Depuis le
 * 06/10, ESP_RST_WDT sans rapport n'alerte plus du tout, voir plus bas ; la marque garde
 * son libellé propre, « First boot after install ».) Le Wi-Fi
 * pas encore réglé et HA qui tarde à ajouter la tablette ne sont pas des anomalies non
 * plus, tant que la tablette n'a jamais vu son réseau.
 *
 * [AI-WARNING] ESP_RST_WDT SANS rapport de plantage n'est pas une anomalie (06/10/2026,
 * discussion #278). Un appui court sur le bouton d'alimentation redémarre la tablette
 * en ~10 s avec cette raison, sans aucun rapport `esp32.crash` (vu chez husyildiz, avec
 * batterie, et reproduit chez Axel, sur USB, le 06/10 à 13:39). Le firmware ne gère pas
 * ce bouton : c'est le matériel. Aucune source trouvée sur son câblage (doc et schéma
 * de M5Stack muets), donc aucun moyen sûr de le reconnaître : on l'appelle « bouton
 * d'alimentation ou chien de garde RTC » et on joint le code brut du ROM
 * (`esp_rom_get_reset_reason(0)`, « rst 0x.. ») pour trancher au prochain appui.
 * Pourquoi c'est sûr sur cette config (ESP-IDF 5.5.5, sources locales lues) :
 *   - esp32p4/reset_reason.c : ESP_RST_WDT regroupe six codes du ROM, chiens de garde
 *     RTC (CORE_RWDT 0x09, CPU_RWDT 0x0D, SYS_RWDT 0x10), super chien de garde (0x12) et
 *     chiens de garde des groupes de timers (CORE_MWDT 0x07, CPU_MWDT 0x0B). Les chiens
 *     de garde de tâche et d'interruption passent par la panique : raison
 *     ESP_RST_TASK_WDT / ESP_RST_INT_WDT (indice gardé en RTC), qui alertent toujours.
 *   - CONFIG_BOOTLOADER_WDT_ENABLE=y, 9000 ms, DISABLE_IN_USER_CODE non posé : le
 *     chien de garde RTC ne surveille que le démarrage, l'application le coupe
 *     (startup_funcs.c, init_disable_rtc_wdt). En marche, il n'est réarmé que par
 *     esp_restart (garde de 1 s, system_internal.c) et par le gestionnaire de panique
 *     (panic.c), APRÈS qu'ESPHome a écrit son rapport (crash_handler.cpp enveloppe
 *     esp_panic_handler) : un vrai plantage arrive donc avec un rapport
 *     (crash_handler_has_data()), qui pose kPlantage, et alerte toujours.
 *   - Ce qui n'alerte plus : un démarrage bloqué plus de 9 s (bootloader), et un chien
 *     de garde de timer qui réinitialise sans passer par la panique (interruptions
 *     bloquées). Rares ; la raison et le code « rst » restent dans l'historique de HA.
 *   - Un redémarrage demandé (OTA, bouton de HA) donne ESP_RST_SW sur cette tablette :
 *     `rst:0xc (SW_CPU_RESET)` au port série, et plus de 20 OTA du 01 au 06/10 sans
 *     aucun événement `esphome.tab5_journal` (base de HA). Le texte « Reboot request
 *     from … » qu'ESPHome lit pour ESP_RST_WDT (debug_esp32.cpp : la source enregistrée
 *     par on_shutdown n'est jamais effacée) est donc périmé ici : journal_raison_ha()
 *     le remplace.
 */
#include "tab5_custom.h"
#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#if __has_include(<esp_rom_sys.h>)
#include <esp_rom_sys.h>  // code de reset brut du ROM ; absent du rendu hors tablette
#define TAB5_JOURNAL_CODE_ROM 1
#endif
#ifdef USE_ESP32_CRASH_HANDLER
#include "esphome/components/esp32/crash_handler.h"
#endif
#include <cstdio>
#include <cstring>

namespace {

constexpr uint32_t kMagic = 0x4A524E31;  // « JRN1 »
constexpr uint32_t kPrefKey = 0x7A6A726E;
constexpr uint32_t kMarqueKey = 0x7A6A6D71;  // marque « déjà démarrée ici »
constexpr const char* kTexteInstallation = "premier démarrage après installation";
// Préfixe lu par la garde « reboot inattendu » (packages/tab5_health.yaml) : ne pas le
// changer sans elle (tests/test_premier_demarrage.py compare les deux).
constexpr const char* kRaisonInstallationHa = "First boot after install";
// ESP_RST_WDT sans rapport de plantage (06/10/2026) : le bouton d'alimentation, en
// pratique. Préfixe lu aussi par la garde « reboot inattendu » (tests/test_bouton_alim.py).
constexpr const char* kTexteBouton = "bouton d'alimentation ou chien de garde RTC";
constexpr const char* kRaisonBoutonHa = "Power button or RTC watchdog";
constexpr int kLignes = 32;
constexpr int kTexte = 96;

// Bits de Journal::anomalie : ce qui justifie un envoi. Les deux premiers sont « graves »
// (notification sur le téléphone côté HA), les deux autres seulement notés.
constexpr uint8_t kPlantage = 1;  // reset anormal, ou rapport de plantage d'ESPHome
constexpr uint8_t kWifi = 2;      // Wi-Fi absent 90 s ou plus (lien C6, routeur)
constexpr uint8_t kErreur = 4;    // erreur ESPHome ou ESP-IDF après la connexion à HA
constexpr uint8_t kLent = 8;      // HA joint plus de 90 s après le démarrage
constexpr uint32_t kSeuilMs = 90000;

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
bool s_ha_vu = false;           // HA joint au moins une fois depuis le démarrage
uint32_t s_wifi_absent_depuis = 0;
uint32_t s_derniere_copie = 0;
uint16_t s_nb_copie = 0xFFFF;
esphome::ESPPreferenceObject s_pref;
esphome::ESPPreferenceObject s_pref_marque;
bool s_pref_ok = false;
bool s_neuve = false;           // marque absente au démarrage (flash effacée)
bool s_marque_a_ecrire = false;
bool s_installation = false;    // neuve ET relancée par le chien de garde RTC
bool s_wifi_vu = false;         // réseau joint au moins une fois depuis le démarrage
esp_reset_reason_t s_raison = ESP_RST_UNKNOWN;  // lue une fois, à l'ouverture
unsigned s_code_rom = 0;        // code brut du ROM (« rst 0x.. »), 0 hors tablette
bool s_rapport_plantage = false;  // rapport de plantage d'ESPHome valide à ce démarrage
bool s_rejeu_fait = false;      // rapport rejoué (ou écarté) une fois par démarrage
bool s_rejeu_en_cours = false;  // ses lignes passent malgré la règle des 5 s
#ifdef USE_ESP32_CRASH_HANDLER
bool s_rapport_dans_journal = false;  // rejoué : à effacer une fois le journal livré
#endif

// Reset anormal à lui seul. ESP_RST_WDT n'y est PAS (voir [AI-WARNING] en tête) : il ne
// l'est qu'avec un rapport de plantage (reset_anormal()), sinon c'est le bouton.
bool raison_anormale(esp_reset_reason_t r) {
    return r == ESP_RST_PANIC || r == ESP_RST_INT_WDT || r == ESP_RST_TASK_WDT ||
           r == ESP_RST_BROWNOUT || r == ESP_RST_PWR_GLITCH || r == ESP_RST_CPU_LOCKUP;
}

// Reset qui pose kPlantage : raison anormale, ou chien de garde AVEC rapport de plantage.
bool reset_anormal(esp_reset_reason_t r, bool rapport_plantage) {
    return raison_anormale(r) || (r == ESP_RST_WDT && rapport_plantage);
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

// Raison de CE démarrage, en clair. ESP_RST_WDT : « plantage » seulement avec un rapport
// de plantage, sinon le bouton ; avec le code du ROM dans les deux cas.
void texte_demarrage(char* buf, size_t taille) {
    if (s_installation) {
        snprintf(buf, taille, "%s", kTexteInstallation);
    } else if (s_raison == ESP_RST_WDT) {
        snprintf(buf, taille, "%s (rst 0x%02X)",
                 s_rapport_plantage ? raison_texte(s_raison) : kTexteBouton, s_code_rom);
    } else {
        snprintf(buf, taille, "%s", raison_texte(s_raison));
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
    s_pref_marque = esphome::global_preferences->make_preference<uint32_t>(kMarqueKey);
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
    // Lue seulement ici ; écrite par journal_tick(), hors du chemin du logger.
    uint32_t marque = 0;
    s_neuve = !(s_pref_marque.load(&marque) && marque == kMagic);
    s_marque_a_ecrire = s_neuve;
    const esp_reset_reason_t r = esp_reset_reason();
    s_raison = r;
#ifdef TAB5_JOURNAL_CODE_ROM
    s_code_rom = (unsigned) esp_rom_get_reset_reason(0);
#endif
#ifdef USE_ESP32_CRASH_HANDLER
    // Lu par arch_init() avant tout, encore valide ici (voir l'en-tête).
    s_rapport_plantage = esphome::esp32::crash_handler_has_data();
#endif
    s_installation = s_neuve && r == ESP_RST_WDT;
    if (s_j.demarrages < 0xFFFF) s_j.demarrages++;
    // kPlantage et kWifi d'un démarrage précédent restent posés jusqu'à l'envoi.
    if (reset_anormal(r, s_rapport_plantage) && !s_installation) s_j.anomalie |= kPlantage;
    char texte[kTexte];
    char repere[kTexte];
    texte_demarrage(texte, sizeof(texte));
    snprintf(repere, sizeof(repere), "démarrage, raison : %s", texte);
    ajouter('>', repere);
}

// Rejoue le rapport de plantage d'ESPHome dans le journal, une fois par démarrage et
// seulement si la raison du reset est un plantage (un vieux rapport jamais lu ne
// s'accroche pas à un redémarrage normal). Jamais depuis journal_log_message() : ses
// ESP_LOGE repassent par le logger, donc par le journal.
void rejouer_rapport() {
    if (s_rejeu_fait) return;
    s_rejeu_fait = true;
#ifdef USE_ESP32_CRASH_HANDLER
    if (!s_rapport_plantage || !reset_anormal(s_raison, true)) return;
    s_rejeu_en_cours = true;
    esphome::esp32::crash_handler_log();
    s_rejeu_en_cours = false;
    s_rapport_dans_journal = true;
#endif
}

void copier_en_nvs() {
    preparer_pref();
    s_pref.save(&s_j);
    esphome::global_preferences->sync();  // avant qu'on débranche la tablette
    s_copie_en_nvs = true;
    s_derniere_copie = esphome::millis();
    s_nb_copie = s_j.nb;
}

void marquer_ha_vu() {
    if (s_ha_vu) return;
    s_ha_vu = true;
    // Tablette neuve : le temps de régler le Wi-Fi puis de l'ajouter dans HA.
    if (esphome::millis() > kSeuilMs && !s_neuve) s_j.anomalie |= kLent;
}

}  // namespace

void journal_log_message(uint8_t level, const char* tag, const char* message) {
    if (message == nullptr) return;
    ouvrir_session();
    const bool idf = tag != nullptr && strcmp(tag, "esp-idf") == 0;
    const bool crash = tag != nullptr && strcmp(tag, "esp32.crash") == 0;
    // Le rapport de plantage n'arrive ici que rejoué par rejouer_rapport(), ou réécrit à
    // l'abonnement aux logs de chaque client (ignoré : c'est le même).
    if (crash && !s_rejeu_en_cours) return;
    const bool ha = ha_connecte();
    if (ha) marquer_ha_vu();
    const bool erreur = level <= ESPHOME_LOG_LEVEL_ERROR || idf;
    const bool avert = level == ESPHOME_LOG_LEVEL_WARN;
    if (!erreur && !(avert && !ha)) return;
    // Avertissements de durée d'ESPHome (« took a long time ») : attendus au démarrage.
    if (!erreur && strstr(message, "took a long time") != nullptr) return;
    char texte[kTexte];
    copier_sans_ansi(texte, sizeof(texte), message);
    // Une ligne seule ne justifie un envoi qu'après la première connexion à HA ; avant,
    // c'est le démarrage, gardé comme contexte. Un rapport de plantage, toujours.
    if (crash) s_j.anomalie |= kPlantage;
    else if (erreur && s_ha_vu) s_j.anomalie |= kErreur;
    ajouter(erreur ? 'E' : 'W', texte);
}

void journal_tick() {
    ouvrir_session();
    rejouer_rapport();
    if (s_marque_a_ecrire) {
        s_marque_a_ecrire = false;
        const uint32_t marque = kMagic;
        s_pref_marque.save(&marque);
        esphome::global_preferences->sync();
    }
    const uint32_t maintenant = esphome::millis();
    if (ha_connecte()) marquer_ha_vu();
    // HA absent mais Wi-Fi présent (HA redémarre, maintenance) : rien à signaler.
    if (esphome::network::is_connected()) {
        s_wifi_vu = true;
        s_wifi_absent_depuis = 0;
        return;
    }
    // Tablette neuve qui n'a jamais vu de réseau : Wi-Fi pas encore réglé, pas une panne.
    if (s_neuve && !s_wifi_vu) return;
    if (s_wifi_absent_depuis == 0) {
        s_wifi_absent_depuis = maintenant == 0 ? 1 : maintenant;
        return;
    }
    if (maintenant - s_wifi_absent_depuis < kSeuilMs) return;
    s_j.anomalie |= kWifi;
    // Copie en NVS, pour survivre à une coupure de courant (tablette figée, débranchée).
    // Ensuite au plus une copie toutes les 15 min, et seulement s'il y a du nouveau.
    if (s_j.nb == s_nb_copie) return;
    if (s_derniere_copie != 0 && maintenant - s_derniere_copie < 900000) return;
    copier_en_nvs();
}

// Appelée par le script d'envoi, HA connecté. `demarrages > 1` : un démarrage n'a
// jamais joint HA (Wi-Fi absent, ou HA absent plus d'une heure : reboot_timeout).
bool journal_has_report() {
    ouvrir_session();
    rejouer_rapport();  // avant l'envoi, si journal_tick() n'est pas encore passé
    if (ha_connecte()) marquer_ha_vu();
    return s_j.nb > 0 && (s_j.anomalie != 0 || s_j.demarrages > 1);
}

bool journal_is_serious() {
    ouvrir_session();
    return (s_j.anomalie & (kPlantage | kWifi)) != 0;
}

std::string journal_reset_reason() {
    ouvrir_session();
    char texte[kTexte];
    texte_demarrage(texte, sizeof(texte));
    return texte;
}

// Filtre du capteur debug (publié par dump_config(), bien après arch_init() qui lit le
// rapport de plantage : s_rapport_plantage est à jour dès ouvrir_session()).
std::string journal_raison_ha(const std::string& raison) {
    ouvrir_session();
    if (s_installation) return std::string(kRaisonInstallationHa) + " (" + raison + ")";
    if (s_raison != ESP_RST_WDT) return raison;
    // ESP_RST_WDT : le « Reboot request from … » d'ESPHome serait celui d'un redémarrage
    // précédent (voir [AI-WARNING] en tête) ; on ne le garde pas.
    char texte[64];
    snprintf(texte, sizeof(texte), "%s (rst 0x%02X)",
             s_rapport_plantage ? "Crash, other watchdogs" : kRaisonBoutonHa, s_code_rom);
    return texte;
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
#ifdef USE_ESP32_CRASH_HANDLER
    // Rapport livré à HA avec le journal : effacé pour le démarrage suivant, comme
    // ESPHome après un abonnement aux logs (il reste lisible pendant celui-ci).
    if (s_rapport_dans_journal) {
        esphome::esp32::crash_handler_clear();
        s_rapport_dans_journal = false;
    }
#endif
}
