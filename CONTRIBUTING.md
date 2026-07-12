# Contributing

## English · [Français](#version-française)

Thanks for looking at this project. It is a personal firmware repo, but issues and PRs are welcome.

### Before you open a PR

1. Read [`AGENTS.md`](AGENTS.md) and [`CARTOGRAPHIE_TAB5.md`](CARTOGRAPHIE_TAB5.md).
2. Copy config templates if needed:
   ```bash
   cp Tab5/user_entities.example.yaml Tab5/user_entities.yaml
   # create secrets.yaml — see docs/installation.md
   ```
3. **Compile must pass:**
   ```bash
   python -m esphome compile tab5-ha-hmi.yaml
   ```
4. If you changed a `!include`d file: `esphome clean` before the next `run`.
5. Use the [PR template](.github/PULL_REQUEST_TEMPLATE.md) checklist.
6. Add a line to [`CHANGELOG.md`](CHANGELOG.md) for user-visible changes.
7. Never commit `secrets.yaml`, `Tab5/user_entities.yaml`, or production HA files.

### Branching

- Branch from `main`, open PR against `main` (no direct pushes).
- Commit style: `type(scope): summary` (e.g. `fix(tab5): guard nullptr in maj_clim`).

---

## Version Française

Merci d'intéresser à ce projet. C'est un firmware personnel, mais issues et PR sont les bienvenues.

### Avant d'ouvrir une PR

1. Lire [`AGENTS.md`](AGENTS.md) et [`CARTOGRAPHIE_TAB5.md`](CARTOGRAPHIE_TAB5.md).
2. Copier les modèles de config si besoin :
   ```bash
   cp Tab5/user_entities.example.yaml Tab5/user_entities.yaml
   # créer secrets.yaml — voir docs/installation.md
   ```
3. **La compilation doit passer :**
   ```bash
   python -m esphome compile tab5-ha-hmi.yaml
   ```
4. Si vous modifiez un fichier `!include` : `esphome clean` avant le prochain `run`.
5. Utiliser la checklist du [modèle de PR](.github/PULL_REQUEST_TEMPLATE.md).
6. Ajouter une entrée dans [`CHANGELOG.md`](CHANGELOG.md) pour les changements visibles.
7. Ne jamais committer `secrets.yaml`, `Tab5/user_entities.yaml`, ni les fichiers HA de prod.

### Branches

- Branche depuis `main`, PR vers `main` (pas de push direct).
- Style de commit : `type(scope): résumé` (ex. `fix(tab5): garde nullptr dans maj_clim`).
