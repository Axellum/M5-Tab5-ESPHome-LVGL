# Fragments du CHANGELOG

Une pull request n'écrit plus dans [`CHANGELOG.md`](../CHANGELOG.md) : elle dépose son entrée
ici, dans un fichier à elle, pour que deux PR ouvertes en même temps n'entrent plus en conflit
sur `## [Unreleased]` (audit du 30/10/2026). *A pull request no longer edits `CHANGELOG.md`: it
adds its own file here, so parallel pull requests stop conflicting on `[Unreleased]`.*

## Écrire son entrée

1. Nom du fichier : le nom de la branche, `/` changé en `-`, en minuscules
   (`claude/changelog-fragments` → `claude-changelog-fragments.md`).
   `python tools/changelog_assembler.py --nouveau Ajouté` le crée depuis la branche courante.
2. Contenu : un morceau de `[Unreleased]`, écrit comme dans le CHANGELOG (liens relatifs à la
   **racine** du dépôt, pas à ce dossier) :

   ```markdown
   ### Ajouté

   - **Titre court** : ce qui change, pour qui, la PR ou l'ADR.
   ```

   Sous-titres permis, dans cet ordre : `Ajouté`, `Modifié`, `Obsolète`, `Retiré`, `Corrigé`,
   `Sécurité` (Keep a Changelog). Plusieurs dans un même fragment si la PR fait plusieurs choses.
3. Une étiquette « **Contrat HA ↔ firmware** : … (depuis vX.Y.Z). » (sortie de
   `python tools/contrat_api.py --matrice`) se pose **avant** le premier `###` : l'assemblage la
   met en tête de la section, où `tests/test_contrat_versions.py` la lit.

`python -m pytest` (et la CI) valide chaque fragment : `tests/test_changelog_fragments.py`.

## Assembler

Au moment d'une release (skill `release-tab5`), ou dans une PR `chore(changelog)` à part,
jamais dans une PR de fonctionnalité :

```bash
python tools/changelog_assembler.py --dry-run          # voir le résultat
python tools/changelog_assembler.py                    # dans [Unreleased], puis supprime les fragments
python tools/changelog_assembler.py --section 3.10.0   # dans `## [3.10.0]`, déjà écrite
```

Chaque entrée va en tête de son sous-titre, la plus récente d'abord (date du commit qui a
ajouté le fragment) ; les entrées déjà sous `[Unreleased]` ne bougent pas. Ce `README.md` reste.
