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
   and so must the host tests (`pip install -r requirements-dev.txt` once):
   ```bash
   python -m pytest
   ```
4. Install the pre-commit hooks once (`pre-commit install`, `pre-commit` comes with
   `requirements-dev.txt`): yamllint, BOM check, secrets check and HA-placeholder check
   run before each commit. `pre-commit run --all-files` checks the whole tree — the CI
   runs the same hooks.
5. If you changed a `!include`d file: `esphome clean` before the next `run`.
6. Use the [PR template](.github/PULL_REQUEST_TEMPLATE.md) checklist.
7. Add a line to [`CHANGELOG.md`](CHANGELOG.md) for user-visible changes.
8. Never commit `secrets.yaml`, `Tab5/user_entities.yaml`, or production HA files.

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
   et les tests hôte aussi (`pip install -r requirements-dev.txt` une fois) :
   ```bash
   python -m pytest
   ```
4. Installer les hooks pre-commit une fois (`pre-commit install`, `pre-commit` vient avec
   `requirements-dev.txt`) : yamllint, détection de BOM, vérificateur de secrets et de
   placeholders HA tournent avant chaque commit. `pre-commit run --all-files` vérifie tout
   le dépôt — la CI rejoue les mêmes hooks.
5. Si vous modifiez un fichier `!include` : `esphome clean` avant le prochain `run`.
6. Utiliser la checklist du [modèle de PR](.github/PULL_REQUEST_TEMPLATE.md).
7. Ajouter une entrée dans [`CHANGELOG.md`](CHANGELOG.md) pour les changements visibles.
8. Ne jamais committer `secrets.yaml`, `Tab5/user_entities.yaml`, ni les fichiers HA de prod.

### Branches

- Branche depuis `main`, PR vers `main` (pas de push direct).
- Style de commit : `type(scope): résumé` (ex. `fix(tab5): garde nullptr dans maj_clim`).
