# Weather providers

## English · [Français](#version-française)

---

The screen does not depend on one weather service (since lot 4c, 2026-09-27).

## Forecasts and current weather

Forecasts and current weather (hourly and daily pages, the humidity drop) come from any `weather.*` entity, picked in Home Assistant from the select « Tab5 · source des prévisions », which lists the weather entities you have (by default the Météo-France city, otherwise the first weather entity, for example Met.no, which Home Assistant sets up by itself), or in the « Météo · Weather » section of the blueprint, like the rooms' devices (since 2026-10-03, discussion #278: it then writes its choice into that list and the two others below, and wins over them). The Tab5 asks each entity only for what it declares: daily forecasts, or else twice-daily ones (NWS) or hourly ones (free OpenWeatherMap) grouped by date; without hourly forecasts (Buienradar), the hourly page stays empty. Hours are shown in local time.

**Fallback.** While the chosen entity is unavailable, or has not updated for 2 hours, the Tab5 takes another weather entity that answers (OpenWeatherMap or Météo-France if you have them, otherwise any other, such as Met.no) and goes back to your choice by itself as soon as it answers again; the list keeps your choice, and `sensor.tab5_meteo` shows the entity in use in its `entite_effective` attribute. With no weather entity answering, nothing is sent: the screen keeps the last forecasts instead of showing « unavailable » at 0 °C. After 5 minutes of fallback, the centre card shows an alert such as « Météo : OpenWeatherMap utilisé, Météo-France indisponible depuis 11 h 34 » (French text, from the Home Assistant alerts), gone by itself when your source is back; the Health view of the generated dashboard shows the source in use.

## Rain in the next hour and weather warnings

Rain in the next hour and weather warnings are optional. Their source is chosen **in Home Assistant**, with the two selects of `packages/tab5_meteo_sources.yaml`, without editing YAML:

| Card | Select « Tab5 · source … » | What it needs |
|---|---|---|
| Rain in the next hour (bars and sentence) | Météo-France | the Météo-France integration (France): `sensor.<city>_next_rain` |
| | OpenWeatherMap | the OpenWeatherMap integration in **v3.0** mode, which needs a One Call subscription (1,000 calls a day free; HA polls every 10 min). Found by itself: the chosen weather entity if it is OpenWeatherMap's, otherwise its first one |
| | Buienradar | nothing to install, no key: rain radar of the Netherlands and Belgium (also Luxembourg and the edges of France and Germany), every 5 min. Non-commercial use, source to credit |
| | DWD | nothing to install, no key: the DWD radar composite through [Bright Sky](https://brightsky.dev), Germany and neighbouring countries, every 5 min |
| | Met.no | nothing to install, no key: Nowcast radar of the Nordic countries (Norway, Sweden, Finland, Denmark), every 5 min |
| | Open-Meteo | nothing to install, no key, everywhere, but a **weather model**, not a radar, in 15-min steps (true 15-min data in central Europe and North America, interpolated elsewhere). Non-commercial use |
| | Aucune (none) | the rain card is hidden |
| Weather warnings | Météo-France | `sensor.<department>_weather_alert`, found by itself (the department of the Météo-France city) |
| | MeteoAlarm | the MeteoAlarm integration (YAML only, 39 European countries, one alert at a time), found by itself (its binary sensor « Information provided by MeteoAlarm ») |
| | DWD | Germany: the *Deutscher Wetterdienst (DWD) Weather Warnings* integration (built in, set up from the UI with the name or ID of your DWD warning cell, or a device tracker). All the warnings of the region, not the pre-warnings (« Vorabinformation ») |
| | CAP Alerts | the [CAP Alerts](https://github.com/seevee/cap_alerts) integration (HACS, custom repository), one entity per alert: MeteoAlarm (Europe, every alert of your region), NWS (United States), Environment Canada, and about 100 national services through the WMO |
| | Aucune (none) | no warning icons |

Buienradar, DWD, Met.no and Open-Meteo are queried by Home Assistant itself (`rest_command.tab5_pluie` in the package), every 5 min and only while chosen, at your home location (`zone.home`) rounded to 0.01° (about 1 km). Outside their area they answer « no data ». Open-Meteo is also used when the rain list is left on Météo-France (its default) and Home Assistant has no Météo-France rain sensor, for example outside France: the rain card then works with nothing to set. Choose « Aucune » to send nothing; add the Météo-France integration later and it takes over by itself.

With DWD and CAP Alerts, a warning counts while it is in force or starts within 24 hours; the overall level is the highest of them. Both are read by `custom_templates/tab5_vigilance.jinja`, which comes with the archive (Météo-France and MeteoAlarm work without it). The hazard slot comes from the DWD code, the MeteoAlarm hazard type, or the icon CAP Alerts gives the alert; a hazard with no slot (drought, air quality…) only raises the overall level.

## Honest limits

- OpenWeatherMap was tried on the author's installation on 2026-09-27 (forecasts and rain in the next hour, on a dry day); its daily forecast covers 8 days, so the last days of the 15-day pages stay empty. MeteoAlarm and the twice-daily grouping (NWS) were tested with simulated data only.
- Met.no, Home Assistant's default weather, gives 6 days (today included) and 48 hours through its integration: the two following pages of the 15-day forecast only show the 6th day. Checked on 2026-10-02 with a real answer for Istanbul run through the code of Home Assistant 2026.9.4, by the templates in Home Assistant's engine and by `tests/test_meteo_sans_meteo_france.py`; not yet tried on a real installation.
- Buienradar, DWD, Met.no and Open-Meteo were queried on 2026-09-29 (Amsterdam, Berlin, Oslo, south-west France; a dry day) and their answers read by the same templates in Home Assistant's engine; rain itself was simulated. Open-Meteo is also queried by the fresh-install CI.
- DWD was added to the author's Home Assistant on 2026-09-29 (Berlin, a day without warnings): entities found, dates read. The warnings themselves, and everything from CAP Alerts, were tested with simulated data in Home Assistant's template engine and in the fresh-install CI.
- Frost probability exists only at Météo-France. The snowflake icon reads `sensor.<city>_snow_chance` when it exists; otherwise it follows the current condition (snowy).

---

## Version Française

---

L'écran ne dépend plus d'un seul service météo (lot 4c, 27/09/2026).

## Prévisions et météo du moment

Les prévisions et la météo du moment (pages horaires et journalières, goutte d'humidité) viennent de n'importe quelle entité `weather.*`, choisie dans Home Assistant avec la liste « Tab5 · source des prévisions », qui propose les entités météo présentes (par défaut la ville Météo-France, sinon la première entité météo, par exemple Met.no, que Home Assistant installe tout seul), ou dans la section « Météo · Weather » du blueprint, comme les appareils des pièces (depuis le 03/10/2026, discussion #278 : elle écrit alors son choix dans cette liste et les deux autres plus bas, et prime sur elles). Le Tab5 ne demande à chaque entité que ce qu'elle déclare : les prévisions journalières, sinon les demi-journées (NWS) ou les horaires (OpenWeatherMap gratuit) regroupées par date ; sans prévisions horaires (Buienradar), la page horaire reste vide. Les heures sont affichées en heure locale.

**Repli.** Tant que l'entité choisie est indisponible, ou ne s'est plus mise à jour depuis 2 heures, le Tab5 prend une autre entité météo qui répond (OpenWeatherMap ou Météo-France si vous les avez, sinon n'importe quelle autre, Met.no par exemple) et revient tout seul à votre choix dès qu'elle répond de nouveau ; la liste garde votre choix, et `sensor.tab5_meteo` montre l'entité utilisée dans son attribut `entite_effective`. Si aucune entité météo ne répond, rien n'est envoyé : l'écran garde les dernières prévisions au lieu d'afficher « indisponible » à 0 °C. Après 5 minutes de repli, la carte centrale affiche une alerte comme « Météo : OpenWeatherMap utilisé, Météo-France indisponible depuis 11 h 34 », qui disparaît d'elle-même au retour de votre source ; la vue Santé du tableau de bord généré montre la source utilisée.

## Pluie dans l'heure et vigilances

La pluie dans l'heure et les vigilances sont facultatives. Leur source se choisit **dans Home Assistant**, avec les deux listes de `packages/tab5_meteo_sources.yaml`, sans toucher au YAML :

| Carte | Liste « Tab5 · source … » | Ce qu'il faut |
|---|---|---|
| Pluie dans l'heure (barres et phrase) | Météo-France | l'intégration Météo-France (France) : `sensor.<ville>_next_rain` |
| | OpenWeatherMap | l'intégration OpenWeatherMap en mode **v3.0**, qui demande l'abonnement One Call (1 000 appels par jour gratuits ; HA interroge toutes les 10 min). Trouvée seule : l'entité météo choisie si elle est d'OpenWeatherMap, sinon sa première |
| | Buienradar | rien à installer, sans clé : radar de pluie des Pays-Bas et de la Belgique (aussi le Luxembourg et les bords de la France et de l'Allemagne), toutes les 5 min. Usage non commercial, source à citer |
| | DWD | rien à installer, sans clé : composite radar du DWD par [Bright Sky](https://brightsky.dev), Allemagne et pays voisins, toutes les 5 min |
| | Met.no | rien à installer, sans clé : radar Nowcast des pays nordiques (Norvège, Suède, Finlande, Danemark), toutes les 5 min |
| | Open-Meteo | rien à installer, sans clé, partout, mais un **modèle météo** et non un radar, au pas de 15 min (vrai pas de 15 min en Europe centrale et en Amérique du Nord, interpolé ailleurs). Usage non commercial |
| | Aucune | la carte pluie est masquée |
| Vigilances | Météo-France | `sensor.<département>_weather_alert`, trouvé seul (le département de la ville Météo-France) |
| | MeteoAlarm | l'intégration MeteoAlarm (en YAML seulement, 39 pays européens, une alerte à la fois), trouvée seule (son capteur « Information provided by MeteoAlarm ») |
| | DWD | Allemagne : l'intégration *Deutscher Wetterdienst (DWD) Weather Warnings* (fournie avec HA, réglée dans l'interface avec le nom ou le numéro de votre cellule d'alerte DWD, ou un suivi d'appareil). Toutes les alertes de la région, pas les préavis (« Vorabinformation ») |
| | CAP Alerts | l'intégration [CAP Alerts](https://github.com/seevee/cap_alerts) (HACS, dépôt personnalisé), une entité par alerte : MeteoAlarm (Europe, toutes les alertes de votre région), NWS (États-Unis), Environnement Canada, et une centaine de services nationaux par l'OMM |
| | Aucune | pas d'icônes de vigilance |

Buienradar, DWD, Met.no et Open-Meteo sont interrogés par Home Assistant lui-même (`rest_command.tab5_pluie` du package), toutes les 5 min et seulement quand ils sont choisis, aux coordonnées du domicile (`zone.home`) arrondies à 0,01° (environ 1 km). Hors de leur zone, ils répondent « pas de données ». Open-Meteo sert aussi quand la liste de la pluie est restée sur Météo-France (son choix par défaut) et que Home Assistant n'a pas de capteur de pluie Météo-France, par exemple hors de France : la carte pluie marche alors sans rien régler. Choisissez « Aucune » pour ne rien envoyer ; l'intégration Météo-France ajoutée plus tard reprend la main toute seule.

Avec DWD et CAP Alerts, une alerte compte tant qu'elle est en cours ou si elle commence dans les 24 h ; le niveau global est la plus forte. Les deux sont lues par `custom_templates/tab5_vigilance.jinja`, fourni dans l'archive (Météo-France et MeteoAlarm marchent sans lui). La case du phénomène vient du code du DWD, du type de phénomène de MeteoAlarm ou de l'icône que CAP Alerts donne à l'alerte ; un phénomène sans case (sécheresse, qualité de l'air…) ne fait que monter le niveau global.

## Limites, en toute franchise

- OpenWeatherMap a été essayé sur l'installation de l'auteur le 27/09/2026 (prévisions et pluie dans l'heure, un jour sec) ; ses prévisions journalières couvrent 8 jours, les derniers jours des pages de 15 jours restent donc vides. MeteoAlarm et le regroupement des demi-journées (NWS) n'ont été testés qu'avec des données simulées.
- Met.no, la météo installée d'office par Home Assistant, donne 6 jours (aujourd'hui compris) et 48 heures par son intégration : les deux pages suivantes des prévisions sur 15 jours ne montrent que le 6e jour. Vérifié le 02/10/2026 avec une vraie réponse pour Istanbul passée par le code de Home Assistant 2026.9.4, par les modèles dans le moteur de Home Assistant et par `tests/test_meteo_sans_meteo_france.py` ; pas encore essayé sur une vraie installation.
- Buienradar, DWD, Met.no et Open-Meteo ont été interrogés le 29/09/2026 (Amsterdam, Berlin, Oslo, Landes ; un jour sec) et leurs réponses lues par les mêmes modèles dans le moteur de Home Assistant ; la pluie elle-même a été simulée. Open-Meteo est aussi interrogé par la CI d'installation à neuf.
- Le DWD a été ajouté au Home Assistant de l'auteur le 29/09/2026 (Berlin, un jour sans alerte) : entités trouvées, dates lues. Les alertes elles-mêmes, et tout CAP Alerts, n'ont été testés qu'avec des données simulées, dans le moteur de modèles de Home Assistant et dans la CI d'installation à neuf.
- La probabilité de gel n'existe que chez Météo-France. L'icône flocon lit `sensor.<ville>_snow_chance` quand il existe ; sinon, elle suit la condition du moment (neige).
