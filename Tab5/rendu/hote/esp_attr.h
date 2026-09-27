// Rendu hors tablette (lot 7) : remplace l'en-tête d'ESP-IDF sur la plateforme `host`.
// Les attributs de placement mémoire du P4 n'ont pas de sens ici.
#pragma once

#define EXT_RAM_BSS_ATTR
#define IRAM_ATTR
#define RTC_NOINIT_ATTR
