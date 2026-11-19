### Ajouté

- **Documentation : partir de ce projet pour son propre écran ESPHome** : une page bilingue ([`docs/reuse.md`](docs/reuse.md)) pour qui veut un tableau de bord Home Assistant sur un écran ESP32 + LVGL avec ESPHome : ce qui se réutilise (règles de code, ADR utiles hors Tab5, dépannage, méthode `AGENTS.md` / `[AI-CONTEXT]`), comment la Waveshare 7B a été ajoutée (ADR-0061 et suivantes, substitution `tab5_carte`), ce qui est propre à l'ESP32-P4 et au Tab5, et pour l'ESP32-S3 seulement ce que disent les sources officielles (le reste « non vérifié »). Dans le menu du site et dans `docs/README.md`.

### Modifié

- **Dépannage : chaque incident dit à quoi il s'applique** ([`docs/troubleshooting.md`](docs/troubleshooting.md)) : « tout écran ESPHome + LVGL » (11 sur 20) ou « propre au Tab5 / à l'ESP32-P4 » (9 sur 20), en anglais et en français, aussi dans une colonne du tableau des symptômes. Titres et ancres inchangés.
