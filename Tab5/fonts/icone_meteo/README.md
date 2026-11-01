# Police météo des écrans (`IconeMeteo.ttf`)

Les icônes météo du Tab5 et de la Waveshare 7B ne sont pas des images : ce sont les glyphes d'une petite police, [`../IconeMeteo.ttf`](../IconeMeteo.ttf) (12 glyphes, 10 Ko). LVGL les dessine comme du texte, dans les couleurs du thème, et en superpose deux pour faire une icône (un nuage et la pluie, par exemple). Ce dossier garde les dessins d'où viennent ces glyphes, pour pouvoir refaire ou compléter la police.

## D'où viennent les dessins

Ce sont mes icônes pour la carte Météo France de Home Assistant (dépôt [lovelace-icones-weather-card](https://github.com/Axellum/lovelace-icones-weather-card), jeu `icons_5`), elles-mêmes tirées du pack « The Weather is Nice Today » de Laura Reen ([icon-icons.com](https://icon-icons.com/fr/pack/The-Weather-is-Nice-Today/1370)), avec des retouches. En 2026, elles ont été redessinées pour remplir tout le cadre ; la pluie, la neige, le vent et l'éclair ont été détachés du nuage, puisque l'écran pose le nuage à part. Le nom interne de la police est resté « Untitled ».

Les SVG de ce dossier sont ces dessins, nettoyés (métadonnées d'Inkscape retirées, rendu identique au pixel ; le brouillard, animé, est figé sur sa première image). Leurs couleurs ne comptent pas : un glyphe n'a qu'une couleur, celle que l'écran lui donne.

## Les glyphes

| Code | Dessin | Fichier | Dans le firmware (`MeteoIcon::`) |
|---|---|---|---|
| U+F000 | <img src="f000-windy.svg" width="40" alt="vent"> | `f000-windy.svg` | `WIND` |
| U+F001 | <img src="f001-snowy-rainy.svg" width="40" alt="pluie et neige"> | `f001-snowy-rainy.svg` | `HAIL` (grêle, pluie et neige) |
| U+F002 | <img src="f002-snowy-6.svg" width="40" alt="neige"> | `f002-snowy-6.svg` | `SNOW` |
| U+F003 | <img src="f003-snowy-2.svg" width="40" alt="neige fine"> | `f003-snowy-2.svg` | non embarqué |
| U+F004 | <img src="f004-snowy-1.svg" width="40" alt="neige légère"> | `f004-snowy-1.svg` | non embarqué |
| U+F005 | <img src="f005-rainy-7.svg" width="40" alt="forte pluie"> | `f005-rainy-7.svg` | `HEAVY_RAIN` |
| U+F006 | <img src="f006-rainy-5.svg" width="40" alt="pluie"> | `f006-rainy-5.svg` | `RAIN` |
| U+F007 | <img src="f007-lightning.svg" width="40" alt="éclair"> | `f007-lightning.svg` | `THUNDER` |
| U+F00B | <img src="f00b-night.svg" width="40" alt="lune"> | `f00b-night.svg` | `MOON` |
| U+F00E | <img src="f00e-fog.svg" width="40" alt="brouillard"> | `f00e-fog.svg` | `FOG` |
| U+F00F | <img src="f00f-day.svg" width="40" alt="soleil"> | `f00f-day.svg` | `SUNNY` |
| U+F015 | <img src="f015-cloudy.svg" width="40" alt="nuage"> | `f015-cloudy.svg` | `CLOUD` |

La correspondance a été vérifiée au pixel le 01/11/2026, en comparant la silhouette de chaque glyphe à celle de chaque dessin. `F003` et `F005` sont deux semis de points presque identiques : `F005` ressemble le plus à `rainy-7`, de peu.

## Comment l'écran compose une icône

- Les tailles sont déclarées dans [`Tab5/paquets/tab5-styles.yaml`](../../paquets/tab5-styles.yaml) : `font_meteo_card` (120 px) et `font_meteo_card_small` (80 px) pour les cartes de prévisions, `font_meteo_48` et `font_meteo_32` pour le popup Météo, en `bpp: 2`. Chacune n'embarque que les glyphes de sa liste `glyphs:`.
- `update_meteo_icon()` ([`Tab5/ecran/tab5_forecast.cpp`](../../ecran/tab5_forecast.cpp)) cherche l'état de Home Assistant dans la table `kMeteoIcons` : le glyphe du calque 1 et sa couleur, celui du calque 2 s'il y en a un, sa couleur, sa taille et ses décalages. Par exemple :
  - `rainy` : le nuage `F015` (rôle `METEO_CLOUD`), remonté de 13 px, et la pluie `F006` (`METEO_PRECIP`) dessous ;
  - `partlycloudy` : le nuage et un petit soleil `F00F` (80 px, `METEO_CELESTIAL`) décalé de 19 px vers le haut et la gauche ;
  - `windy` : `F000` seul ; un état inconnu : le nuage seul.
- Les couleurs sont des rôles de la palette du thème (`METEO_CLOUD`, `METEO_PRECIP`, `METEO_THUNDER`, `METEO_CELESTIAL`) : un thème change la couleur des icônes sans toucher à la police.

## Refaire ou compléter la police

L'outil qui a assemblé la police en 2026 n'a pas été noté. Un outil qui fabrique une police TrueType à partir de SVG convient (FontForge ou IcoMoon, par exemple), à condition de :

1. garder les points de code du tableau : le firmware appelle chaque glyphe par son code (`MeteoIcon`, dans [`Tab5/ecran/tab5_forecast.h`](../../ecran/tab5_forecast.h)) ;
2. cadrer chaque dessin comme ici (carré de 64, dessin qui remplit le cadre), pour que les calques tombent au même endroit ;
3. ajouter un nouveau glyphe à la liste `glyphs:` des tailles qui l'affichent, dans `tab5-styles.yaml`, sinon ESPHome ne l'embarque pas ;
4. remplacer `Tab5/fonts/IconeMeteo.ttf`, compiler, puis regarder les cartes de prévisions et le popup Météo (le rendu hors tablette de la CI, `rendu-host.yml`, dit si l'écran a changé).

## Crédits

Dessins d'origine : Laura Reen, pack « The Weather is Nice Today » ; ses conditions d'utilisation sont sur la page du pack. Retouches, version agrandie et calques : Axel Duviella. Doc et nettoyage faits avec l'aide d'une IA.

*In English: the weather icons on the screens are the 12 glyphs of `IconeMeteo.ttf`, drawn by LVGL as text in the theme's colours, two layers per icon (`kMeteoIcons` in `Tab5/ecran/tab5_forecast.cpp`). This folder holds the SVG drawings the glyphs were made from, with their code points, so that the font can be rebuilt or extended.*
