// Audit qualité (item 4) : le strict minimum d'« esphome.h » pour compiler un moteur de jeu
// PUR (chess_ai.cpp) sur PC, hors ESPHome. Si un moteur a besoin de plus que ça, c'est
// qu'il n'est pas pur : le constat compte autant que le test.
#pragma once

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>

namespace esphome {
inline uint32_t millis() {
    using namespace std::chrono;
    return static_cast<uint32_t>(duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count());
}
}  // namespace esphome

#define ESP_LOG_HOTE(niveau, tag, format, ...) std::printf("[" niveau "][%s] " format "\n", tag, ##__VA_ARGS__)
#define ESP_LOGE(tag, format, ...) ESP_LOG_HOTE("E", tag, format, ##__VA_ARGS__)
#define ESP_LOGW(tag, format, ...) ESP_LOG_HOTE("W", tag, format, ##__VA_ARGS__)
#define ESP_LOGI(tag, format, ...) ESP_LOG_HOTE("I", tag, format, ##__VA_ARGS__)
#define ESP_LOGD(tag, format, ...) ESP_LOG_HOTE("D", tag, format, ##__VA_ARGS__)
#define ESP_LOGV(tag, format, ...) ESP_LOG_HOTE("V", tag, format, ##__VA_ARGS__)
