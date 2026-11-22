### Corrigé

- **ESPHome 2026.11** : le firmware compile de nouveau avec ESPHome 2026.11.0 (Tab5 et Waveshare 7B), sans changer le plancher `min_version: 2026.9.0`. Le journal du lien avec le co-processeur Wi-Fi suit ESP-Hosted 3.x (événements reçus par la boucle d'ESP-IDF : la raison du redémarrage du C6 n'est plus notée quand il démarre avant le firmware) ; les événements envoyés à Home Assistant suivent le nouveau type de chaîne de l'API ; le changement de thème tient compte du cache de styles que LVGL 9.6 active ([ADR-0029](docs/decisions/0029-themes-palette.md)).
