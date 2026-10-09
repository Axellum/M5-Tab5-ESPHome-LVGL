# ADR-0042: A long press on the central card opens a navigation wheel to every screen of the tablet

**Status:** Accepted (2026-10-09, asked for by the author; not tried on a tablet when written).
**Date:** 2026-10-09

## Context

The central card of the home page (`central_card`: alerts, rain, planning, info, Home Assistant alerts; the forecast pages and the HA-mode room title above it) answered a long press with one thing: the alerts history. Every other screen had its own way in — a top button, a clock gesture ([ADR-0039](0039-gestes-accueil.md)), a tile's long press, the « Aller à l'écran » select from Home Assistant — and some had none from the home page (a room in HA mode needed swipes; the light and shutter windows needed a tile).

The author asked (2026-10-09) for « a multiple-choice wheel on the long press of the central bar, to pick the pages to go to: alerts, games, discussions, settings, the rooms, lights, air conditioners, temperatures, weather, calendar, alarm clock, shutters », using the wheels « wherever possible », « smart, handy, readable and very beautiful », with clean and lean code.

## Decision

- **The same wheel, no second engine.** The quick-action wheel of the tiles ([ADR-0036](0036-quick-action-wheel.md), `tab5_roue.cpp`) already draws a hub, a first ring of up to six buttons and a second ring of up to six choices for the family touched, closes on a touch, a popup, inactivity or the screen going off, and repaints on a theme change. `tab5_roue_navigation.cpp` only composes the buttons and says what a touch does (`RoueRappels`); `roue_navigation_ouvrir()` opens it anchored on the central card (`RoueUI::carte_centrale`, set by `tab5-roue.yaml`), above it.
- **Six buttons, grouped by what they are about** (twelve screens do not fit one ring of 72 px buttons):
  - **Alertes** — the alerts history (what the long press used to open; shown as the « current » state while an alert is on screen);
  - **Pièces ▸** — Maison ([ADR-0037](0037-house-popup.md)), then each room that has devices, its number in the button and its name under it: a touch puts HA mode on that room (`tuiles_aller_piece`), the room on screen is marked;
  - **Appareils ▸** — Températures, Clims, Lumières, Volets, Énergie, Plantes;
  - **Agenda ▸** — Calendrier, Réveil (the weather screen of the parallel lot goes first here, `Ecran::METEO`, once it exists);
  - **Tablette ▸** — Jeux, Réglages, Système;
  - **Assistant** — the voice assistant (« discussions »).
- **Only what this home has.** A screen with nothing to show (`ecran_disponible`: no climate, no plant, no light tile…) is not offered; an empty family disappears and the other buttons close up (the ring is re-centred by ADR-0036's layout). Recomputed at each opening, unfolding and touch: it follows the zones and rooms pushed by Home Assistant.
- **Readable: a word per button.** A new mode of the wheel (`RoueTete::mots`) writes each button's word in the button's own axis, at a distance measured on the word (`mot_recul()`: the button's radius, 4 px, then half the word's box projected on the axis), so a long word never sits on its neighbour; words of one row share the width between them (`largeurs_libres()`). When a family is unfolded, the first-ring words hide and the hub says the family's name (« Aller à » otherwise, under a compass rose). The tile wheel keeps its two link captions (« Maison », « Détails »).
- **Three new screens of the « Aller à l'écran » list and of the gestures**: `Ecran::LUMIERES`, `Ecran::VOLET`, `Ecran::TEMPERATURE`, after `MAISON` (select options 13 to 15; `ARCADE` and `NB` shift, NVS never stores an `Ecran` value). Lights and shutter open the popup of a tile without a tile being touched — the room shown in HA mode first, otherwise the first room that has one (`tuiles_ecran_ouvrir`); Température opens the history of the home page's left temperature (the living room, or the room shown in HA mode, [ADR-0040](0040-room-climate.md)), the greenhouse otherwise. Their gesture codes `lumieres`, `volet`, `temperature`, then `roue` (the wheel itself, from any clock or top-button gesture), are appended after `nabu_suivant` (index 17) in `kCodesGestes` and in the blueprint: NVS keeps the code's index.
- **Every way in.** The long press of each panel of the central card (one template, `central_bouton.yaml`) and of the page title (`btn_page_title_tap`: forecast pages, HA-mode room) opens the wheel; a long press at the end of a swipe opens nothing (`ui_appui_glisse()`).
- **Nothing new for Home Assistant.** No service variable, no event, nothing in NVS: a screen opens through the single routine `tab5_ecran_ouvrir` (ADR-0013), a room through HA mode. The contract between versions does not change (`contrat/contrat.yaml` 1.0.0); the blueprint only gains four gesture choices.

## Rejected

- **A dedicated navigation popup or a grid of icons**: a second set of widgets, chrome and geometry for what the wheel already does (rule 5), and a different gesture from the tiles'.
- **Twelve buttons on two rings without families**: the first ring holds six 72 px buttons above a card at y 375; the second ring is the families' (ADR-0036). Two levels at most keep every screen two touches away.
- **Captions only on the hub** (the tile wheel's way): the author asked for something readable; a word per button reads at a glance, and the measured distance keeps it off the neighbouring buttons whatever the word's length in the seven screen languages.
- **A new `Ecran::METEO` here**: it belongs to the weather lot (`feat/meteo-graphique`); its place is reserved at the head of the Agenda family.
- **Keeping the alerts history on the long press and the wheel elsewhere**: the alerts stay one touch further (the first button, marked when an alert is on), and the central card is the largest, most central target of the home page.

## Consequences

- The long press of the central card no longer opens the alerts history directly: « Alertes » is the first button of the wheel (`docs/notice/home.md`, `alerts.md`).
- A new screen in the wheel = one line in `kAppareils`, `kAgenda` or `kTablette` (`tab5_roue_navigation.cpp`), its icon in `RoueIcone` / `glyphe_roue` (`mdi_font_36`, rule 9); `tests/test_roue_navigation.py` reads the tables, the wiring and the geometry above the central card.
- `tools/rendu/ecrans.py` opens the wheel folded, three families unfolded, a room picked and the lights window opened by « Aller à l'écran ».
- Not tried on a tablet when written: the author judges the look and the touch.
