### Ajouté

- **Plus de conflit de CHANGELOG ni de numéro d'ADR entre PR parallèles** (outillage, audit du
  30/10/2026, lot A) : une PR ne touche plus `CHANGELOG.md`, elle dépose son entrée dans
  `changelog.d/<branche>.md` (format dans [`changelog.d/README.md`](changelog.d/README.md)) ;
  `tools/changelog_assembler.py` les range sous `[Unreleased]` (ou sous la section d'une version)
  à la release, puis les supprime, et `tests/test_changelog_fragments.py` valide chaque fragment.
  `tools/prochain_adr.py` donne le prochain numéro d'ADR libre en comptant les PR ouvertes, et
  `--write` recale le nombre d'ADR du README et de la cartographie. Les étiquettes « Contrat HA ↔
  firmware » se lisent aussi dans les fragments. Aucun changement de firmware.
