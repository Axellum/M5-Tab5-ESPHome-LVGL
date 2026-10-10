# ADR-0050: A music player popup for any media_player, pushed by Home Assistant

**Status:** Proposed (2026-10-10, asked for by the author; not tried on a tablet when written). Amends the long press of a `med` tile in [ADR-0023](0023-rooms-generic-tiles.md).
**Date:** 2026-10-10

## Context

The author asked for « une pop up qui ne prend pas tout l'écran, très belle, pour faire un lecteur de musique genre Spotify », practical, with the cover art. He has no Spotify: his players are an Apple TV, a Samsung TV, a Freebox, a Steam Deck and the tablet itself, all `media_player` entities in Home Assistant. Before this, a `med` tile's long press opened the generic device popup (on / off and a state line).

Constraints: push-only ([ADR-0001](0001-push-only-zero-polling.md)) — the tablet never reads a Home Assistant state; events-only ([ADR-0025](0025-events-only.md)) — the firmware never calls an action and names no entity; nothing guessed nor hardcoded in the public HA files ([ADR-0024](0024-packages-without-placeholders.md)); the shared modal chrome ([ADR-0009](0009-modal-shell-header.md)) and the single registry ([ADR-0013](0013-single-registry-consoles-modals.md)); colours from the palette (code rule 1); every icon in its font's `glyphs:` (code rule 9); no burst of pushes while a track plays.

## Decision

- **A centred card, not a full screen**: 940 × 536 (`lecteur_card_w` / `lecteur_card_h`, `kLecteurCarteL` / `kLecteurCarteH`) over the 70 % scrim, the shared header (icon, the player's name, the current app as a chip, the close cross). The 360 px cover on the left; on the right the title, artist and album, a draggable position bar with the elapsed time and the duration, shuffle / previous / play-pause / next / repeat, the volume with mute; one chip per chosen player at the bottom. A button shows only when the player supports it (`supported_features`, read by HA as letters, like a climate's capabilities in [ADR-0026](0026-climate-from-device.md)). Off or standby: « Lecteur éteint » and an « Allumer » button; no player chosen: a sentence that says where to choose them.
- **Generic**: any `media_player`. The players offered are the multi-select list « Tab5 · lecteurs de musique · music players » (`packages/tab5_reglages.yaml`, six at most, its memory `input_text.tab5_choix_lecteurs`); a HA select's state must be one of its options, so the list's first option is a summary (« n choisi(s) · n chosen ») and the others toggle « ✓ » / « ○ ».
- **One push service, two variables** — `tab5_maj_lecteur(lecteurs, etat)`: `lecteurs` = « nom|genre;… » (the chips), `etat` = the shown player, sixteen « | » fields, the image last so it takes the rest. Read in pure C++ tested on a PC (`Tab5/socle/tab5_parse.*`, section 9, `tools/test_parse.cpp`, fuzzed by `tools/fuzz/fuzz_parse.cpp`). The position is advanced by HA up to the send and then by the tablet once a second while playing (`lecteur_position()`), so HA does not push every second.
- **The shown player** is decided by HA (`packages/tab5_lecteur.yaml`): the one picked on screen (a chip, or a `med` tile through the blueprint), else the first of the list that plays, else the first of the list; a listed player that starts playing becomes the shown one when the shown one does not play. A mirror sensor (`sensor.tab5_signature_du_lecteur`) changes with the shown player's state and attributes; one script pushes, `mode: restart` with a 400 ms wait, so a burst of attribute changes leaves as one push.
- **Commands are events**: `esphome.tab5_lecteur` (`action`, `lecteur`, `valeur`). `packages/tab5_evenements.yaml` hands them to `script.tab5_lecteur_commande`, which acts only on a player of the chosen list (by its index) or on the shown one (`-1`): no action nor entity name comes from the event.
- **Cover art**: `entity_picture` (a relative path served by HA with its token) is completed by the tablet with the address of HA it sees (`http://<address>:8123`) and downloaded by an `online_image` (decoded to RGB565, 360 × 360). A full URL is used as is.
- **Three ways in**: the long press of a `med` tile without option `t` (`Fenetre::LECTEUR`, one row of `kGestes`; with `t` it stays the remote); a **« now playing » mini-bar** on the home screen, over the « Ok Nabu » frame while the shown player plays and for 5 minutes after a pause (cover, title, artist, play-pause; a tap opens the popup) — the author's choice, the left zone stays to another lot; the gesture code **`musique`**, appended at the end of `kCodesGestes` (index 24, after `zone_gauche_suivante`; the list is stored by index) and offered by the blueprint, plus « Musique » at the end of « Aller à l'écran » (`Ecran::MUSIQUE`).

## Rejected

- **A full-screen popup**: asked against by the author (« qui ne prend pas tout l'écran »).
- **A Spotify-only player** (Spotify Connect API, playlists, search): he has no Spotify, and the tablet would need a token and a web client against push-only.
- **The tablet reading `media_player` states itself** (`homeassistant` text sensors): one subscription per attribute and per player, entity IDs in the firmware (code rule 8), against ADR-0001.
- **HA pushing the position every second**: a push per second per player for a bar the tablet can advance alone.
- **Guessing the players** (every `media_player` of the house): a TV's built-in apps, the tablet's own speaker and the Steam Deck would all appear; the list is the author's choice, like the other « Tab5 · … » lists.
- **Opening it from the − / + tile**: not asked for.

## Consequences

- A minor contract version: `contrat/contrat.yaml` 1.1.0 (one action and one event added). Update order: the firmware first; new HA files with an older firmware fail the player push without stopping anything else (`continue_on_error`).
- A cover from a HA served only in https on its port does not arrive (the relative path is completed in http): the music note shows instead. Documented in the package's header.
- The mini-bar hides the « Ok Nabu » frame while music plays: its wake-word toggle comes back 5 minutes after a pause or when playback stops (to be judged on the screen by the author). Since [ADR-0051](0051-left-zone-choice.md)'s second lot, the zone left of the clock can show a compact player on the same data and commands; the mini-bar stays hidden while it does.
- The 70 % scrim and the 360 px cover cost a full-card redraw at opening; the position bar repaints once a second while playing.
- New glyphs in `mdi_font_26`, `_32`, `_45`, `_70` and `_120`; seven screen texts in six languages; the HA dashboard of the Tab5 shows the new list.
- The off-device render stubs `online_image` (no download): the cover there is the music note.
