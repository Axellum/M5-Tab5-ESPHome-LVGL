### Modifié

- **Menu du site : un seul groupe « Matériel »** (documentation seulement, aucun firmware ni fichier Home Assistant) : la M5Stack Tab5 (fiche, batterie, brochage), la Waveshare 7B et le comparatif « Tab5 ou Waveshare 7B ? » sont réunis sous un groupe « Matériel » placé avant « Installer », au lieu d'être dans « Comprendre le projet », la Tab5 d'un côté et la 7B à part. Les adresses des pages ne changent pas ; `tools/site/menu.yml`.
- **Fil d'Ariane visible retiré** : le chemin du menu n'est plus affiché au-dessus du titre de chaque page (`navigation.path` de `tools/site/mkdocs.yml`). Le `BreadcrumbList` du balisage pour les moteurs de recherche reste.
