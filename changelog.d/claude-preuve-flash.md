### Ajouté

- **Preuve d'un flash par l'API** (outil du PC, aucun changement de firmware) : `tools/prouver_flash.py`
  prend avant le flash un instantané de l'appareil (`--avant` : MAC, version, heure de compilation,
  heure et raison du démarrage, état de chaque entité, réglages) et le compare après (`--apres`) : même
  appareil, version et heure de compilation attendues, un seul redémarrage depuis l'instantané et
  aucun pendant `--attente` (120 s), raison sans plantage, aucune entité perdue, aucun réglage changé ;
  code de sortie non nul si un contrôle échoue. La clé vient de Home Assistant (`tools/tab5_cle_api.py`)
  et n'est jamais affichée. Comparaison testée sans appareil (`tests/test_prouver_flash.py`) ; ce
  script avait été réécrit au brouillon par au moins huit sessions (audit du 30/10/2026).
