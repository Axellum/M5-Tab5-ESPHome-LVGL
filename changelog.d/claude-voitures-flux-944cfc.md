**Contrat HA ↔ firmware** : le firmware d'abord (depuis v3.9.0).

### Ajouté

- **Les voitures électriques dans le Flux** : la page « Flux » du popup Énergie montre jusqu'à deux voitures, chacune un cercle (nom, batterie, puissance de charge, état « En charge », « Branchée », « Débranchée ») relié au solaire et au réseau, avec l'énergie ajoutée et l'heure de fin de charge au coin. La charge est servie après la maison (solaire, batterie, réseau) et partagée entre les voitures selon leur puissance ; le capteur de la maison n'est plus compté deux fois (case « Le compteur de la maison est séparé des voitures » du blueprint, décochée par défaut). Avec la batterie et deux voitures, un seul cercle « 2 voitures ». Rien n'est dessiné sur la 7B (page trop basse). Sans voiture choisie, la page est celle d'avant. Action `tab5_maj_voiture` (contrat 1.5.0, ajout pur), section « Énergie : voitures » du blueprint, textes dans les sept langues, [ADR-0076](docs/decisions/0076-cars-on-the-flux-page.md).
- **Démo et rendu** : `demo_pusher.py --voitures` (une voiture en charge à 7,2 kW, l'autre branchée) et cinq captures du Flux (sans voiture, une, deux, avec la batterie).
