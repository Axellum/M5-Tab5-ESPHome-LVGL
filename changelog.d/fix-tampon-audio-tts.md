### Corrigé

- **Moins de grésillements dans les réponses vocales** : le tampon de lecture du haut-parleur passe de 500 ms à 2 s sur le Tab5 et la Waveshare 7B. La réponse arrive de Home Assistant pendant qu'elle est générée, et chaque hoquet du Wi-Fi vidait l'ancien tampon : le haut-parleur comblait avec du silence, ce qui s'entendait sur les longues réponses d'un modèle de langage. Aucun délai ajouté ; 384 Kio de PSRAM pendant qu'un son joue ([voix](docs/voice_assistant.md)).
