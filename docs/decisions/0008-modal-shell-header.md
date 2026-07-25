# ADR-0008: Cadre modal partagé (chrome) + en-tête compact

**Status:** Accepted  
**Date:** 2026-07-25

## Context

Les popups v2 (lumière, clim, pots, calendrier, console, …) dupliquaient le même chrome : voile, carte `style_modal_card`, icône/titre, croix 96×64, `scrollable: false`. L’ADR-0007 refuse de factoriser les *actions* hétérogènes des boutons clim ; le **cadre** (apparence + geometry d’en-tête), lui, est cosmétique et partageable.

Demande Axel : en-tête bas pour maximiser la zone utile — icône, titre, zones d’options (nav mois, etc.) et croix alignés à **6 px** du haut de la carte et **10 px** des bords gauche/droit.

## Decision

1. Templates `!include` dans `Tab5/ui_components/` :
   - `modal_scrim.yaml` — voile + fermeture
   - `modal_header_brand.yaml` — icône + titre (bandeau 64 px)
   - `modal_close_btn.yaml` — croix verre 96×64
2. Geometry d’en-tête **obligatoire** pour tout nouveau popup plein-carte :
   - haut : `y: 6`
   - côtés : `x: 10` (gauche) / `x: -10` (droite, croix)
   - croix et boutons d’options d’en-tête : hauteur **64** (même ligne)
3. Corps de contenu : démarrer à `y: 80` (6 + 64 + 10) sur carte 1250×690, marge basse 10 → hauteur utile **600** (ex-566 sous `y: 100`).
4. Pas d’instance runtime unique qui « swap » le centre (contrainte ESPHome/LVGL YAML statique) — seulement le chrome YAML.

## Consequences

- Changer la croix / le voile / l’alignement d’en-tête = éditer les 3 templates (ou cet ADR si la geometry change).
- Popups hors famille (assistant sous-titre 2 lignes, TV titre centré) adoptent la même geometry 6/10 quand c’est possible, sans forcer `modal_header_brand` si le layout d’en-tête diffère.
- Fermetures multi-widgets (calendrier, console) passent une `close_lambda` une ligne dans les vars.
