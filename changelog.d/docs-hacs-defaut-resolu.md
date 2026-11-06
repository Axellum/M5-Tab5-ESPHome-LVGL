### Corrigé

- **Documentation HACS : le défaut « Got more than 8190 bytes » est résolu** : la page d'installation, le
  guide « Fichiers Home Assistant » et l'entrée de dépannage disaient encore « défaut de HACS 2.0.5, posez
  les fichiers à la main jusqu'à ce que HACS soit corrigé ». Vérifié le 06/11/2026 : l'en-tête de GitHub
  mesure 3 994 octets sur `tab5_hacs.zip` (il en faisait plus de 9 000 les 23 et 24 octobre), Home Assistant
  2026.10.4 accepte des en-têtes jusqu'à 64 Kio (home-assistant/core#188329), le ticket hacs/integration#5643
  est fermé, et HACS lui-même n'a pas changé (2.0.5 reste sa dernière release). Les textes disent maintenant
  que c'est résolu et gardent la copie à la main pour un Home Assistant plus ancien. Un téléchargement de
  Tab5 par HACS n'a pas été refait après le correctif (dit dans le dépannage).
