# ADR-0067: Whole-home HiFi in the Music popup — rooms, library and outputs through Music Assistant, pushed by Home Assistant

**Status:** Proposed (2026-10-18, asked for by the author; contract written before the code, not tried on a tablet when written). Extends the music popup of [ADR-0050](0050-music-player.md) and the favourite playlists page of [ADR-0068](0068-favourite-playlists.md) (its own pull request, #515).
**Date:** 2026-10-18

## Context

The author asked for « une gestion complète HiFi » on the screen, built on Music Assistant: players, sources, outputs, « toute la maison », beautiful, practical, accessible but extremely complete, from the smallest installation to the largest. ADR-0050 gives one player at a time (cover, transport, volume, six chips); ADR-0068 adds favourite YouTube playlists for an Apple TV.

Constraints: push-only and events-only ([ADR-0001](0001-push-only-zero-polling.md), [ADR-0025](0025-events-only.md)) — the firmware names no entity, calls no action and talks to nothing but Home Assistant; nothing of a real home in the public HA files, no `!secret` ([ADR-0024](0024-packages-without-placeholders.md)); one popup with pages and one swipe ([ADR-0046](0046-popups-a-pages.md)); the music card stays 940 × 536 on both boards (the author's choice on 2026-10-18, after a mock-up); colours from the palette; every icon in its font's `glyphs:`; instant transitions; `kPayloadMax` (16 KiB) per payload.

Verified on the author's Home Assistant on 2026-10-18 (HAOS 18.3, aarch64 VM, Core 2026.10.2):

- The Music Assistant app 2.10.7 (`d5369777_music_assistant`, host network) is found by Home Assistant (`hassio` discovery, one confirmation) and its integration `music_assistant` loads: one `media_player` per player, `mass_player_type` (`player` / `group`), `active_queue`, `group_members` when the player can group, plus one « add the current song to favourites » `button` per player.
- Players seen with no setting: AirPlay (Apple TV 4K — unavailable until paired with a PIN shown on the TV —, Freebox Player), DLNA (Samsung TV), Google Cast (Lenovo Smart Clock), and the « Home Assistant MediaPlayers » provider for the ESPHome tablet, the Steam Deck and a voice satellite.
- `music_assistant.get_library` (`config_entry_id`, `media_type`, `limit`, `offset`, `favorite`, `order_by`, response) returned the 12 radios of the library: `uri` `library://radio/N`, `name`, `image` = the image's **original URL** (a Wikipedia PNG, a favicon…), `favorite`.
- `music_assistant.play_media` (`library://radio/2`) on the tablet's Music Assistant player: the tablet's own ESPHome `media_player` went `playing` at once (11:59:51), volume 20 % set before.
- The shown track's `entity_picture` on a Music Assistant player is Music Assistant's own image proxy, in plain http on the LAN: `http://<ha>:8095/imageproxy/<id>?size=512&fmt=png`. The proxy serves sizes 0, 80, 160, 256, 512, 1024 only; `<id>` is `sha256("<provider>/<path>")` and must have been issued by Music Assistant (otherwise 404). Many radio logos fail upstream even there (Wikimedia refuses the size asked, a favicon is gone): a missing cover is normal, not a fault.
- `music_assistant.get_queue` gives the current and the next item and the number of items, not the list; Home Assistant has no action to jump to an item of the queue.

## Decision

### Pages of the Music popup

Same card, same chrome, same swipe; the popup always opens on « Lecture ». Pages, in this order, each only when it has something to show:

1. **Lecture** (ADR-0050), plus, with Music Assistant, one line under the album: « À suivre · <title — artist> » and « n titres » (the queue), and a chip « + n » after the player's name when it leads a group.
2. **Playlists** ([ADR-0068](0068-favourite-playlists.md), favourite playlists): its grid, its return to « Lecture » 600 ms after a tap and its button in the accent colour for 30 s are unchanged; since both were merged (2026-10-18), its page, its name on the title line and its swipe are those of this popup's single page brick (`tab5_musique.cpp`), no longer a second one in `tab5_lecteur.cpp`.
3. **Pièces** — every output of the home, eight at most, a grid of 4 × 2 cards: the kind's icon, the name, the title playing or the state, the volume (slider 0-100, sent on release), and up to three buttons each shown only when the player can: link / unlink to the shown player's group (`g`), power (`o`), « send the music here » (`t`, not on the shown one). A tap shows that player on « Lecture »; a long press opens the wheel ([ADR-0036](0036-quick-action-wheel.md)): source (`s`), mute (`m`), transfer (`t`), unlink. On the title line, « Tout ici » (group every available player with the shown one) and « Tout séparer », with Music Assistant only. The shown player's card has the accent border, its group's cards a thinner one.
4. **Bibliothèque** — with Music Assistant only. A row of categories (Favoris, Playlists, Albums, Artistes, Radios, Podcasts, Récents), then a grid of 5 × 2 items: cover (or the kind's icon), name, second line. A tap plays a playable item (track, radio, episode, audiobook) on the shown player — replacing the queue — and opens a container (artist → albums, album / playlist → tracks, podcast → episodes) with a back button; a long press opens the wheel: play now, play next, add to the queue, radio mode, open. « Suivants » / « Précédents » when there are more. The sources of a player (from its wheel) show in the same grid.

From the smallest to the largest home, with nothing to set on the screen:

| Home | Pages |
|---|---|
| One `media_player`, no Music Assistant | Lecture (ADR-0050) |
| Several chosen players, no Music Assistant | Lecture, Pièces: the « Tab5 · lecteurs de musique » list (six), volume, power, mute, source; link only for players that group natively (`group_members`) |
| Music Assistant | Lecture with the queue, Pièces with every Music Assistant player (eight, sorted by name), groups and transfer, Bibliothèque |

Home Assistant tells which case applies (`ma` below); the firmware keeps nothing about it.

### Contract (minor: two actions, one event)

**`tab5_maj_musique_pieces(pieces, contexte)`** — pushed on `esphome.tab5_connected`, on `ouvrir`, and when a mirror signature of the listed players changes (state, volume, mute, group, title; `mode: restart`, 1 s wait: a burst is one push). Always sent, even empty.

- `contexte` = `ma|montre|file|suivant`: `ma` `1` when the `music_assistant` integration is loaded, else `0`; `montre` the index in `pieces` of the shown player (`sensor.tab5_lecteur_montre`, ADR-0050), `-1` if absent; `file` the number of items of its queue (`0` unknown); `suivant` « title — artist » of the next item, empty if none — last field, takes the rest.
- `pieces` = up to eight entries separated by `;`, each `nom|genre|etat|volume|muet|groupe|fonctions|titre`:
  - `nom` 40 characters at most;
  - `genre` one letter for the icon: `t` TV, `e` speaker, `a` screen / tablet, `g` group, `x` other;
  - `etat` `j` playing, `p` paused, `r` on and idle, `e` off / standby, `i` unavailable;
  - `volume` 0-100, empty if unknown; `muet` `0` / `1`;
  - `groupe` the index of the piece that leads its group (its own index when it leads members), `-1` alone;
  - `fonctions` letters: `v` volume, `m` mute, `o` power, `g` can group, `s` source list, `t` can receive a transfer (a Music Assistant player);
  - `titre` « title — artist » when playing or paused, 60 characters at most.

**`tab5_maj_musique_liste(contexte, elements)`** — pushed only in answer to an event (`parcourir`, `detail`, `sources`, later `chercher`). Home Assistant keeps nothing: the tablet sends back the `ref` it received.

- `contexte` = `vue|titre|page|suite|retour`: `vue` `fav` `pl` `alb` `art` `rad` `pod` `rec` `det` (a container's content) `src` (a piece's sources) `rech` (search, later) or `err` (titre = the sentence to show); `titre` 48 characters at most; `page` from 0; `suite` `1` if a next page exists; `retour` `1` to show the back button.
- `elements` = up to ten entries separated by `;`, each `nom|sous|genre|ref|image`:
  - `nom`, `sous` 40 characters at most;
  - `genre` `t` track, `a` album, `r` artist, `p` playlist, `d` radio, `o` podcast or episode, `l` audiobook, `s` source, `x` other (decides the icon and what a tap does);
  - `ref` a Music Assistant URI (`library://album/12`, `ytmusic://playlist/…`), 160 characters at most, or for `s` the source's index in the player's `source_list`;
  - `image` an http(s) URL, may be empty — last field, takes the rest.
- Home Assistant asks small images: a Music Assistant proxy URL gets `size=160`; a `…googleusercontent.com/…=wN-hN` URL gets `=w160-h160`; other URLs go as they are. In every name, « | » becomes « / » and « ; » becomes « , »; in a URL « ; » becomes `%3B`.

**Event `esphome.tab5_musique`** (`action`, `piece`, `valeur`); `piece` is an index of the last `pieces` pushed or `-1` (the shown player):

| action | valeur | Home Assistant does |
|---|---|---|
| `ouvrir` | — | pushes `pieces` |
| `parcourir` | `vue\|page` | `get_library` (favourites, playlists, albums, artists, radios, podcasts) or the recently played, ten per page, pushes the list |
| `detail` | `ref\|page` | the content of that artist / album / playlist / podcast |
| `jouer` | `mode\|ref` | `music_assistant.play_media` on the piece; `mode` `m` now (replace), `s` next, `f` at the end, `r` radio mode |
| `montrer` | — | the piece becomes the shown player (as a chip does, ADR-0050) |
| `grouper` / `separer` | — | `media_player.join` the piece to the shown player / `media_player.unjoin` it |
| `tout_grouper` / `tout_separer` | — | every available piece that can group / every grouped piece |
| `transferer` | — | `music_assistant.transfer_queue` from the shown player to the piece, which becomes the shown one |
| `volume` | 0-100 | `media_player.volume_set` |
| `muet` / `marche` | — | toggles mute / power |
| `sources` | `page` | pushes the piece's `source_list` (`vue` `src`) |
| `source` | index | `media_player.select_source` with that entry of `source_list` |

Home Assistant rebuilds the list of pieces the same way to resolve an index (an index out of range does nothing), accepts a `ref` only if it matches `^[a-z0-9_-]+://[^|;\s]+$`, a `mode` only among the four letters, and never takes an entity or an action from the event. Every action has `continue_on_error`.

### Where the code goes

- Home Assistant: one new package, `packages/tab5_musique.yaml` (mirror signature, the two pushes, `script.tab5_musique_commande`), one branch in `packages/tab5_evenements.yaml`; it reuses `sensor.tab5_lecteur_montre` and `script.tab5_lecteur_montrer` of `packages/tab5_lecteur.yaml`. The integration's `config_entry_id` is found with `integration_entities('music_assistant')` / `config_entry_id()`, never written.
- Firmware: a new module `Tab5/ecran/tab5_musique.*` (pages Pièces and Bibliothèque, the downloads), new components `Tab5/ui_components/musique_piece.yaml` (×8) and `musique_element.yaml` (×10), the pages registered on the Music popup's `PagesPopup`. Reading in pure C++ tested on a PC (`Tab5/socle/tab5_parse.*`, a new section: `musique_pieces_lire()`, `musique_liste_lire()`; cases in `tools/test_parse.cpp`, a call in `tools/fuzz/fuzz_parse.cpp`).
- Covers of the library: ten `online_image` of 160 × 160 (RGB565, about 50 KiB each in PSRAM), downloaded one after the other from the first item; a failed download keeps the kind's icon. The shown track's cover stays ADR-0050's.

## Rejected

- **The tablet talking to Music Assistant itself** (its websocket or HTTP API): against ADR-0001 / ADR-0025, and a Music Assistant token on the device.
- **The whole queue and « play item n »**: Home Assistant's integration has no action for it; Music Assistant's own API needs a token, which would be a secret in public files (ADR-0024). Possible later through the optional Tab5 integration ([ADR-0035](0035-hacs-integration-ha-files.md)); until then « À suivre » and the count.
- **Resized images through Music Assistant's proxy for every item**: the proxy id needs the image's provider, which Home Assistant's response does not carry; only the URLs that are already proxy URLs (local files, now playing) are resized.
- **`media_player.browse_media`** instead of `get_library`: no paging (500 items at once) and no favourites filter.
- **A second popup or a bigger card**: the author kept the 940 × 536 card; one popup with pages (ADR-0046).
- **Every `media_player` of the house without Music Assistant**: ADR-0050's rejection stays (TV apps, the tablet's own speaker…); with Music Assistant, its players are already the author's choice of outputs.
- **« Don't stop the music » as a toggle**: no Home Assistant action sets it; the radio mode of `jouer` gives the same « keep playing » from an item.

## Consequences

- API contract 1.3.0 → 1.4.0 (two actions and one event added). Update order: the firmware first; new HA files with an older firmware: the two pushes fail with `continue_on_error`, nothing else stops. An older HA with the new firmware: no Pièces / Bibliothèque page (nothing pushed).
- The page names on the title line: Lecture, [Playlists], Pièces, [Bibliothèque] — up to four on a 940 px card next to the player's name; to be judged on the 7B.
- New glyphs (speaker, TV, group, link / unlink, power, transfer, album, artist, playlist, radio, podcast, history, back), new screen texts in the six languages, new screens in the off-device render (Pièces with a group, Bibliothèque with and without covers, the sources).
- Search by text needs a keyboard on the screen: a later lot, with a `chercher` action (`texte|page`) already reserved in this table.
- Not measured when written: the ten downloads' time and memory on the 7B, the time of `get_library` with a large library, a push of eight pieces during a volume drag.
