// Rendu hors tablette (lot 7) : remplace l'en-tête de FreeRTOS sur la plateforme `host`.
// L'IA des dames journalise la pile libre de sa tâche : pas de tâche FreeRTOS ici.
#pragma once

inline unsigned uxTaskGetStackHighWaterMark(void *task) { return 0; }
