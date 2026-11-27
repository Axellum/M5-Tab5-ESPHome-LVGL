/**
 * [AI-CONTEXT]
 * @file Tab5/web/web_main.cpp
 * @role Démo web SEULEMENT (essai, étape 0) : la tablette virtuelle
 *       (tab5-rendu-host.yaml, plateforme host d'ESPHome) compilée en WebAssembly par
 *       emscripten (tools/web/construire.py). Ce fichier n'est pas une source
 *       d'ESPHome : le script l'ajoute au build, à côté des sources générées.
 *       - main() : celui du host d'ESPHome (components/host/core.cpp, compilé sous un
 *         autre nom) tourne `while (1) loop();`, ce qu'un navigateur ne permet pas.
 *         Ici, setup() puis un loop() toutes les 16 ms (emscripten_set_main_loop sur
 *         une minuterie, EM_TIMING_SETTIMEOUT). Pas sur requestAnimationFrame : quand
 *         la page n'est pas peinte (volet masqué, onglet caché), il tombe à 1 image/s,
 *         la boucle aussi, et chaque appui dure plus d'une seconde pour LVGL, donc
 *         devient un appui long (vu le 27/11/2026 : roue d'actions au lieu de basculer).
 *         La page peint de son côté (requestAnimationFrame).
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
#include "esphome/components/api/api_server.h"
#include "esphome/components/api/user_services.h"
#include "lvgl_private.h"
#include "rendu_doigt.h"

#include <array>
#include <span>
#include <string>
#include <vector>

void setup();
void loop();

// Événement de la tablette vers la page (le faux Home Assistant) : JSON
// {"service": …, "event": true|false, "data": {clé: valeur}}. La page le traite après
// le tour de boucle (setTimeout), jamais pendant : pas de réentrance dans le firmware.
EM_JS(void, web_vers_page, (const char* json), {
    if (Module.surEvenement) Module.surEvenement(UTF8ToString(json));
});

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

void json_texte(std::string& sortie, const char* s, size_t n) {
    sortie += '"';
    for (size_t i = 0; i < n; i++) {
        const char c = s[i];
        if (c == '"' || c == '\\') {
            sortie += '\\';
            sortie += c;
        } else if (static_cast<unsigned char>(c) < 0x20) {
            char u[8];
            snprintf(u, sizeof u, "\\u%04x", c);
            sortie += u;
        } else {
            sortie += c;
        }
    }
    sortie += '"';
}

// Tampon de encode_list_service_response (seul l'ESP8266 s'en sert).
std::array<char, 1024> g_brouillon{};

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

// Événements et actions que la tablette envoie à HA (homeassistant.event: des
// packages) : APIServer::send_homeassistant_action, enveloppé au lien
// (--wrap, construire.py) pour tous les appels faits hors de api_server.cpp. Sans
// client, l'original ne ferait que journaliser « dropped ».
void __wrap__ZN7esphome3api9APIServer25send_homeassistant_actionERKNS0_26HomeassistantActionRequestE(
    esphome::api::APIServer*, const esphome::api::HomeassistantActionRequest& appel) {
    std::string json = "{\"service\":";
    json_texte(json, appel.service.c_str(), appel.service.size());
    json += appel.is_event ? ",\"event\":true,\"data\":{" : ",\"event\":false,\"data\":{";
    bool premier = true;
    for (const auto* liste : {&appel.data, &appel.data_template}) {
        for (const auto& kv : *liste) {
            if (!premier) json += ',';
            premier = false;
            json_texte(json, kv.key.c_str(), kv.key.size());
            json += ':';
            json_texte(json, kv.value.c_str(), kv.value.size());
        }
    }
    json += "}}";
    web_vers_page(json.c_str());
}

// Actions de l'API (celles que Home Assistant appelle) : [{"n": nom, "a": [[variable,
// type]…]}], type 0 bool, 1 entier, 2 nombre, 3 texte (enums::ServiceArgType).
EMSCRIPTEN_KEEPALIVE const char* web_actions() {
    static std::string json;
    json = "[";
    auto* api = esphome::api::global_api_server;
    if (api != nullptr) {
        bool premier = true;
        for (auto* service : api->get_user_services()) {
            auto r = service->encode_list_service_response(std::span<char>(g_brouillon));
            if (!premier) json += ',';
            premier = false;
            json += "{\"n\":";
            json_texte(json, r.name.c_str(), r.name.size());
            json += ",\"a\":[";
            for (size_t i = 0; i < r.args.size(); i++) {
                if (i) json += ',';
                json += '[';
                json_texte(json, r.args[i].name.c_str(), r.args[i].name.size());
                json += ',' + std::to_string(static_cast<int>(r.args[i].type)) + ']';
            }
            json += "]}";
        }
    }
    json += ']';
    return json.c_str();
}

// Appelle l'action `nom` comme Home Assistant : `valeurs` = les valeurs des variables
// dans l'ordre déclaré (web_actions), séparées par \x1f, converties selon leur type.
// 0 : exécutée ; -1 : nombre de valeurs faux ; -2 : refusée ; -3 : action inconnue.
EMSCRIPTEN_KEEPALIVE int32_t web_action(const char* nom, const char* valeurs) {
    auto* api = esphome::api::global_api_server;
    if (api == nullptr) return -3;
    for (auto* service : api->get_user_services()) {
        auto r = service->encode_list_service_response(std::span<char>(g_brouillon));
        if (!(r.name == nom)) continue;
        std::vector<std::string> champs;
        if (r.args.size() > 0) {
            const char* debut = valeurs;
            for (const char* p = valeurs;; p++) {
                if (*p == '\x1f' || *p == '\0') {
                    champs.emplace_back(debut, static_cast<size_t>(p - debut));
                    if (*p == '\0') break;
                    debut = p + 1;
                }
            }
        }
        if (champs.size() != r.args.size()) return -1;
        esphome::api::ExecuteServiceRequest req;
        req.key = r.key;
        req.args.init(champs.size());
        for (size_t i = 0; i < champs.size(); i++) {
            auto& a = req.args.emplace_back();
            const std::string& v = champs[i];
            switch (r.args[i].type) {
                case esphome::api::enums::SERVICE_ARG_TYPE_BOOL:
                    a.bool_ = (v == "true" || v == "1" || v == "on");
                    break;
                case esphome::api::enums::SERVICE_ARG_TYPE_INT:
                    a.int_ = static_cast<int32_t>(strtol(v.c_str(), nullptr, 10));
                    break;
                case esphome::api::enums::SERVICE_ARG_TYPE_FLOAT:
                    a.float_ = strtof(v.c_str(), nullptr);
                    break;
                default:
                    a.string_ = esphome::StringRef(v.c_str(), v.size());
                    break;
            }
        }
        return service->execute_service(req) ? 0 : -2;
    }
    return -3;
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
    emscripten_set_main_loop_timing(EM_TIMING_SETTIMEOUT, 16);
    return 0;
}
