// Rendu hors tablette (tab5-rendu-host.yaml, lot 7) : remplace l'en-tête d'ESP-IDF sur
// la plateforme `host`. Les jeux et le journal choisissent RAM interne ou PSRAM ; ici,
// tout vient de malloc. Les tailles de tas lues par la console valent 0.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdlib>

#define MALLOC_CAP_8BIT (1u << 2)
#define MALLOC_CAP_SPIRAM (1u << 10)
#define MALLOC_CAP_INTERNAL (1u << 11)

inline void *heap_caps_malloc(size_t size, uint32_t caps) { return std::malloc(size); }
inline void *heap_caps_calloc(size_t n, size_t size, uint32_t caps) { return std::calloc(n, size); }
inline void heap_caps_free(void *ptr) { std::free(ptr); }
inline size_t heap_caps_get_free_size(uint32_t caps) { return 0; }
inline size_t heap_caps_get_total_size(uint32_t caps) { return 0; }
inline size_t heap_caps_get_largest_free_block(uint32_t caps) { return 0; }
