### Corrigé

- **Pluie dans l'heure par Vigie, hors de France : plus de quart d'heure de décalage** : Vigie garde la réponse d'Open-Meteo 15 min sans la caler sur les quarts d'heure, et la tablette datait ses quatre pas de 15 min d'après l'heure de la réponse ; la pluie pouvait s'afficher un quart d'heure trop tard. Home Assistant lit maintenant l'heure de début que Vigie donne (`rain_nowcast.start_utc`) et garde l'ancienne règle pour une réponse qui ne l'a pas. Les pas de 5 min de Météo-France (en France) ne changent pas ([ADR-0084](docs/decisions/0084-vigie-default-rain-source.md)).
