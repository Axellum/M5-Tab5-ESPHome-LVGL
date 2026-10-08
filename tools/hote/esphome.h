// [AI-CONTEXT] Le strict minimum d'« esphome.h » pour compiler sur PC, hors ESPHome,
// les moteurs de jeu PURS testés par la CI (job `python` d'esphome-tab5.yml) :
//   - tools/test_chess_engine.cpp    : Tab5/jeux/chess_ai.cpp (millis, ESP_LOG*) ;
//   - tools/test_draughts_engine.cpp : le bloc Engine de Tab5/jeux/draughts_game.cpp, dont
//     l'en-tête nomme lv_obj_t et LvglComponent sans s'en servir (déclarés ici, vides).
// Repris de tools/audit/hote/esphome.h (branche audit/relances, audit du 30/09/2026).
// Si un moteur a besoin de plus que ça, c'est qu'il n'est plus pur : le constat compte
// autant que le test. À ne pas confondre avec Tab5/rendu/hote/ (rendu hors tablette).
#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace esphome {
inline uint32_t millis() {
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
namespace lvgl {
class LvglComponent;
}  // namespace lvgl
}  // namespace esphome

struct _lv_obj_t;
typedef struct _lv_obj_t lv_obj_t;

#define ESP_LOG_HOTE(niveau, tag, format, ...) std::printf("[" niveau "][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) ESP_LOG_HOTE("E", tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) ESP_LOG_HOTE("W", tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) ESP_LOG_HOTE("I", tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) ESP_LOG_HOTE("D", tag, format, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) ESP_LOG_HOTE("V", tag, format, ##__VA_ARGS__)
