# ADR-0043: A Weather popup — the forecast Home Assistant already pushes, drawn as graphs on three pages

**Status:** Accepted (2026-10-09, asked for by the author; not tried on a tablet when written).
**Date:** 2026-10-09

## Context

The home page shows the weather as five tiles (hourly or daily, two pages of each, [ADR-0002](0002-single-page-swipe-navigation.md)) and the rain of the next hour as nine bars on the central card. Home Assistant pushes more than that: 15 hourly slots (the third block was dropped on 2026-09-25 because no screen showed it), 15 days, and two services whose fields had been « reserved » since the big central weather icon was removed: the current weather (`tab5_maj_meteo_actuelle`: condition, temperature, humidity) and the probabilities (`tab5_maj_probabilites`: UV index, frost, snow).

The author asked for a graphic weather screen « much more visual, beautiful, clear and practical, in several parts », with:

1. only what HA already pushes — no new service, no new variable if possible;
2. pages switched like the Settings popup (names next to the title, a swipe), factored rather than copied;
3. the hourly curve with icons, rain bars and « now » marked; the days as low-to-high bars with a gradient; a details page;
4. reachable from the « Aller à l'écran » select, a home gesture ([ADR-0039](0039-gestes-accueil.md)) and a long press on the forecast tiles when that gesture is free; the long press of the central card unchanged.

## Decision

- **One popup, three pages** (`ui_components/meteo_popup.yaml`, `Tab5/ecran/tab5_meteo.cpp`), shared chrome ([ADR-0009](0009-modal-shell-header.md)), registered as « Météo » ([ADR-0013](0013-single-registry-consoles-modals.md)):
  - **« Aujourd'hui »**: a « Maintenant » card (icon, temperature, condition in words, low and high of the day, rain over the hours shown), then up to 15 hours: hour, icon, a smoothed temperature curve (monotone cubic, Fritsch-Carlson: it never overshoots between two hours), points and values coloured by temperature, rain bars in mm; the current hour on a tinted column, « Demain » where the hours pass midnight;
  - **« 10 jours »**: one row per day from today (name, icon, low, a bar from low to high on the common scale of the ten days with a gradient of the two temperature colours, high), the current temperature as a dot on today's row;
  - **« Détails »**: the rain of the next hour (the nine bars of the central card on a time scale — 5 min then 10 min — against dashed level lines, and the central card's sentence), then four cards: humidity, UV index (WHO levels), frost and snow probabilities.
- **Contract unchanged.** The popup reads `cal_heures_data`, `cal_jours_data`, the nine rain levels (`pluie_barre_niveau()`) and the rain sentence (`pluie_phrase_lue()`) where the tiles and the central card keep them, and keeps the current weather and the probabilities the two services already receive (`meteo_actuelle_recue()`, `meteo_probabilites_recues()`); their descriptions lose the word « reserved ». `contrat/contrat.yaml` (names and types) does not change.
- **15 hours again.** `packages/tab5_push.yaml` sends three hourly blocks (`count: 3`) instead of two, and the demo the same (`(0, 5, 10)`). A firmware from before accepts the third block without showing it (it always did); a package from before sends ten hours, and the popup shows the ten it has.
- **Factored with the Settings and the Temperature popup** (code rule 5): the swipe and the active page name go through the shared paged-popup brick (`pages_brancher()` with a static `PagesPopup`, `choix_peindre()`: `tab5_pages.cpp`, [ADR-0046](0046-popups-a-pages.md)); the page names stay at the fixed places of `reglages_onglet.yaml`, which takes a `prefixe` (`reglages` or `meteo`: `${prefixe}_afficher_page(page)`); the plot pieces `ui_rectangle()` / `ui_ligne()` / `ui_poser()` moved out of `tab5_historique.cpp`. The weather icons are `update_meteo_icon()` with a scale (`echelle_pct`, 40 % → two new fonts, `font_meteo_48` and `font_meteo_32`, the same 10 + 2 glyphs).
- **Built once, painted on change.** The plot widgets are created at the first opening (about 240 objects), then only moved, shown and coloured through the compare-first helpers. The popup repaints its page when it opens, on a page change, on each push while it is shown (`meteo_donnees_changees()`, called by the hourly, daily, rain and sentence handlers) and on a theme change (`meteo_rejouer_theme()`). Colours come from the palette (`UIColor`, [ADR-0029](0029-themes-palette.md)); no shadow.
- **Access.** Option « Météo » at the end of the select (`Ecran::METEO`, index 16, after the navigation wheel's screens of [ADR-0042](0042-navigation-wheel.md); ARCADE moves to 17, which is not stored); gesture code `meteo` at the end of `kCodesGestes` (index 22, after the wheel's codes) and of the blueprint's list; first choice of the « Agenda ▸ » family of the navigation wheel (`RoueIcone::METEO`); glyph `weather-partly-cloudy` (U+F0595) as header icon and in `mdi_font_26` / `mdi_font_70` for the top buttons. A long press on the body of a forecast tile opens it **only when the tile has no device** (its invisible button is hidden: `meteo_tuile_libre()`); a tile with a device keeps the device's long press ([ADR-0023](0023-rooms-generic-tiles.md)). The central card's long press (alerts) is not touched.

## Rejected

- **A new service with a dedicated weather payload**: Home Assistant refuses a call with an extra or missing variable, so a firmware and a package of different versions would stop pushing; everything needed was already pushed.
- **`lv_chart`**: disabled in the build (`LV_USE_CHART 0`, noted in [ADR-0032](0032-temperature-history-popup.md)) and its series styling cannot colour each point by temperature; the Temperature popup already draws with `lv_line` and `lv_obj`.
- **A copy of the Settings' page names and swipe**: two copies of the same geometry, accent painting and slider exception (rule 5).
- **A long press on every forecast tile**: it would take the long press of a tile's device (light wheel, shutter, climate).
- **Dashed curve or area fill under the curve**: LVGL 9.5 dashes only horizontal or vertical lines, and an area fill would need a mask per frame.

## Consequences

- The popup works with what a 3.x package pushes; only the hours 10-14 need the package of this version.
- A new page = a value of `MeteoPage`, its container and name in `meteo_popup.yaml`, its lines in `tab5_meteo_ouvrir` and its case in `peindre()` (header of `tab5_meteo.cpp`).
- The off-device render captures the three pages (`meteo-aujourdhui`, `meteo-jours`, `meteo-details`).
