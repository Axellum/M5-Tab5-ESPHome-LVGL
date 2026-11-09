### Corrigé

- **Pots, températures, clim et tuiles absents après le redémarrage d'un écran** : la poussée de connexion
  partait parfois vers une autre carte, hors ligne (« Not connected to tab5-ha-hmi »), et tout ce
  qu'elle portait était perdu jusqu'à la reconnexion suivante. Cause : l'écran publie son
  préfixe d'actions ~30 ms après son « HA API Status », et la liste des destinataires, calculée
  avant l'attente de la connexion, retombait sur le nom par défaut `tab5_ha_hmi`. `tab5_pret`
  (`custom_templates/tab5_tablette.jinja`) attend maintenant aussi ce préfixe
  (`tab5_prefixe_publie`) et le blueprint « Tab5 — emplacements » recalcule ses destinataires
  après l'attente (`tab5_tablettes`). À déployer avec le blueprint **et** le fichier
  `custom_templates/tab5_tablette.jinja` (les modèles d'abord). Constaté sur le Tab5 le 09/11/2026 à
  17 h 42 (trace de l'automatisation).
