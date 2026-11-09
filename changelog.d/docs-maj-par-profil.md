### Ajouté

- **Notes de release par façon d'installer** : [`tools/gestes_maj.py`](tools/gestes_maj.py) lit ce qu'une release a changé (fichiers du firmware, fichiers Home Assistant, code de l'intégration) et l'ordre du contrat HA ↔ firmware, puis dit, pour l'intégration de HACS et pour le zip à la main, chaque geste « requis », « recommandé », « facultatif » ou « fait par l'intégration », avec le nombre de gestes (`python tools/gestes_maj.py v3.9.0 v3.9.1`, `--en` pour la release GitHub). Cas d'essai, la 3.9.1 : le firmware est recommandé, les fichiers Home Assistant et le tableau de bord ne servent que pour le cas « aucune source météo trouvée », et `tab5_hacs.zip` n'est jamais à télécharger à la main. Testé par `tests/test_gestes_maj.py`.

### Modifié

- **Mises à jour** : [`docs/installation/updates.md`](docs/installation/updates.md) ouvre « Fichiers Home Assistant » par un tableau « ce qu'une release vous demande » : 2 gestes avec l'intégration de HACS (Installer, redémarrer), 5 à la main, le tableau de bord (3 gestes) seulement si les notes le demandent. L'ordre « fichiers Home Assistant d'abord » ne vaut plus quand les notes disent « compatible dans les deux sens ». Le Marketplace de Home Assistant 2026.11 (HACS intégré) est mentionné d'après ses notes de version ; une première installation de Tab5 par ce chemin n'est pas essayée.
