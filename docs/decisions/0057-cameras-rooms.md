# ADR-0057: Cameras by room — the Home Assistant area of each camera, a room column, the last image of each camera kept

**Status:** Proposed (2026-10-10; draft PR, not tried on a tablet nor with several real cameras when written).
**Date:** 2026-10-10

## Context

The Cameras popup of [ADR-0049](0049-cameras-popup.md) showed up to eight cameras, one page each, in the blueprint's order. The author asked (2026-10-10, « fais comme tu le vois ») to handle **several cameras in several rooms**, « intuitive, complete, clean, beautiful, very well optimised », in two steps: rooms and navigation first, then a mosaic.

What was there: one image downloaded and decoded at a time off the main loop (`tab5_cameras_charge.cpp`, ADR-0049 « Off the main loop »), two image buffers owned by the loader, the image released as soon as another camera was shown — so every swipe started on « Chargement... », and a camera offline only said « Image indisponible » after a failed try, which could keep the loader busy up to 12 s.

## Decision

- **The room comes from Home Assistant, with no setting.** The blueprint adds `area_name(camera)` (the entity's area, else its device's) to each record, and, for a camera whose state is `unavailable`, the Unix time since when (`last_changed`). Record: « nom|image|pièce|depuis ». The `tab5_maj_cameras(adresse, cameras)` signature does not change, so `contrat/contrat.yaml` does not either (it records variables, not payload formats); the field addition rides on contract 1.2.0, not yet published. Both ways work: a firmware of 1.2.0 reads « nom|image » and ignores the rest (an offline camera without image is skipped there, as before); this firmware reads « nom|image » from an older blueprint as cameras without room, online.
- **Sixteen cameras** (`kCamerasMax` 8 → 16): payload reading and room grouping are pure C++ in `tab5_parse` (`cameras_lire()`: rank of each camera's room among the distinct rooms, in the order of their first camera, the empty room last), tested by `tools/test_parse.cpp` and fuzzed by `tools/fuzz/fuzz_parse.cpp` (invariants: never more rooms than cameras, every rank in range).
- **A room column, only when it helps.** With two rooms or more, a column of chips on the left (`cameras_puce.yaml`, 17 = « Toutes » + one per room, scrollable past eight), each with its number of cameras; the image frame moves to the right edge (x 270), so the column gets 238 px — a whole room name in `roboto_22`, where the 143 px on each side of a centred frame would have cut most of them. With one room (or an older blueprint), no column and the frame stays centred: nothing changes for a single-room home. The selected chip is in the accent colour (`choix_peindre()`, the recipe of every « choice » row); « Autres » = the cameras without a room.
- **Swipe and dots inside the chosen room.** The shared paged-popup brick ([ADR-0046](0046-popups-a-pages.md)) counts and shows the cameras of the filter only; « Toutes » keeps the camera shown, a room shows its first camera (or keeps the shown one if it is in it). Under the image: « Pièce · Caméra » and the time of the image.
- **Remembered.** The chosen room and camera are kept in NVS (`cams`, by a hash of their names, not an index that the next list could shift), and found again at the next opening, also after a reboot. ESPHome batches flash writes; a swipe a second costs nothing.
- **The last image of every camera is kept** while the popup is open: the loader now **gives** its output buffer to the screen (`camera_charge_prendre()`) and gets one back when the screen drops an image (`camera_image_rendre()`), so swiping back to a camera shows its last image at once, then the new one replaces it. Budget: 8 MiB of images at most (eight 960 × 540 or thirteen 640 × 480) and never below 6 MiB of free PSRAM (23 MB free at rest, `docs/performance.md`); past that, the image seen longest ago goes first, never the one shown, and its buffer is freed (not handed back to the loader, or free PSRAM would not rise). Outside that budget: the loader's output buffer and the received JPEG, about 2 MiB at most, so about 10 MiB in all. Everything is released at closing, as before.
- **A scheduler instead of « the page shown »:** the camera shown every 5 s; between two of its images, its neighbours in the filter (next, then previous) that have no image yet, once each — a swipe then lands on an image. Each camera has its own failure count and wait (10 s, 30 s, 60 s), so a failing camera no longer delays the others' turns. A camera that Home Assistant says `unavailable` is never asked for; two failures in a row also make it « offline » on the tablet's side.
- **Offline, said plainly.** In the middle of the frame: « Hors ligne depuis 14:32 » (today), « … depuis Lun 14:32 » (this week), « … depuis le 3 Oct » (older), over the camera's last image at 40 % opacity when there is one. Three new screen texts plus « Toutes » and « Autres », in the seven languages.
- **Off the tablet**, the loader's stub returns a computed test pattern (a tint per camera, at the requested size) instead of staying on « Chargement... », so the off-device render shows the layout with images: captures `cameras` and `cameras-piece-hors-ligne`.

## Rejected

- **Room chips as header tabs** (`pages_onglets()`): 158 px each with five rooms, and the tabs are already the brick's « page » names, while here a room is a filter and the camera is the page.
- **A room setting in the blueprint**: Home Assistant already knows the area of each camera; a second place to say it would drift.
- **Keeping the images after closing**: up to 8 MiB held while the popup is not shown; reopening shows « Chargement... » for about half a second per camera instead.
- **Aborting a download when the user swipes away**: `esp_http_client` cannot be interrupted from another task while it waits for the headers; the scheduler moves on as soon as the loader is free (12 s at worst for a camera that times out).

## Costs (not measured on the tablet when written)

- **Memory**: the camera table moves to PSRAM (`EXT_RAM_BSS_ATTR`, 2 × ~6.5 kB with the rebuild buffer), nothing more in internal RAM. Images: as above, released at closing.
- **Main loop**: unchanged per image (the loader does the work); the screen now repaints a column of 17 chips at most when the list arrives (rare) and on a chip tap.
- **Home Assistant**: one or two more requests at the opening (the neighbours), then one every 5 s as before.

## Consequences

- The author judges on the screen: the column at 238 px, the frame moved right with two rooms or more, the « Hors ligne » text over a dimmed image.
- Blueprint to copy into Home Assistant for the rooms and the offline time; the firmware works with the older blueprint (no rooms, no offline time) and the older firmware with the new blueprint.
