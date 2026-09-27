// Rendu hors tablette (lot 7) : remplace l'en-tête d'ESP-Hosted sur la plateforme `host`.
// La console lit la version du co-processeur Wi-Fi (C6) : il n'y en a pas ici.
#pragma once

#include <cstdint>

#ifndef ESP_OK
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#endif

typedef struct {
  uint32_t major1;
  uint32_t minor1;
  uint32_t patch1;
} esp_hosted_coprocessor_fwver_t;

inline esp_err_t esp_hosted_get_coprocessor_fwversion(esp_hosted_coprocessor_fwver_t *ver) { return ESP_FAIL; }
