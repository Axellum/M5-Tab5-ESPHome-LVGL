# ADR-0069: The screen direction of the Waveshare 7B is chosen in Home Assistant, so a board mounted upside down runs the published firmware

**Status:** Proposed (2026-10-19, asked for by the author; built and tested off-device, not flashed on a board when written).
**Date:** 2026-10-19

## Context

The Waveshare ESP32-P4 7B ([ADR-0061](0061-board-and-screen-size.md)) has a landscape panel mounted upside down compared with its driver's default: LVGL turns every frame by 180° (`lvgl: rotation: ${carte_rotation}`, `carte_rotation: "180"` in `geometrie-waveshare-7b.yaml`). Since 2026-10-16 the author's board is mounted the other way round, and needs a firmware built with `-s carte_rotation 0`. Since 3.9.0-rc.6 the project publishes a 7B firmware (PR #525), built at 180°. On 2026-10-19 the published rc.6 was installed on the author's board: the picture was upside down. A local rc.6 build at 0° was flashed back, which leaves the board without the « Firmware » update entity: it no longer follows the releases.

What was checked before deciding (ESPHome 2026.9.1, the 7B's floor, in its own environment; `components/lvgl/lvgl_esphome.cpp` is the same file in 2026.9.0):

- **The rotation can change while running.** `LvglComponent::set_rotation(int)` changes the angle as long as `rotation:` is in the configuration: the key compiles the software rotation (`ROTATION_SOFTWARE`, a rotation buffer and a PPA client on the P4). Before LVGL's `setup()` the call only stores the angle, which `setup()` then uses. Neon Apron already relies on it (portrait during a game, [ADR-0012](0012-lvgl-rotation-270-pinball-portrait.md)).
- **The touch follows by itself.** The LVGL touch listener calls `rotate_coordinates()` with the current angle on every read; the GT911 `transform:` (mirror X and Y) only fits the touch panel onto the screen, whatever the angle. On 2026-10-19 the author saw the touch right on his board at 0° with the same `transform:`.
- **0° costs nothing.** At 0°, `ppa_rotate_()` returns at once and `draw_buffer_()` sends LVGL's buffer as it is (`dst = ptr`). A build with `rotation: 0` takes exactly this path: the key is present, so the software rotation is compiled, and `set_rotation(0)` is not even generated. The figures of 2026-10-16 on the author's board (4 runs each, a full-screen redraw) therefore hold as they are: about **69 ms at 0°, 83 ms at 180°**. RAM does not change (the 1.2 MB rotation buffer in PSRAM is allocated in both builds).
- **The boot order allows it.** A template `select` is set up at `setup_priority::HARDWARE` (800) and restores its value from NVS there, then calls its `on_value`. The boot bar ([#476](https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/pull/476), drawn on the panel without LVGL) opens at `on_boot` priority 500, LVGL's `setup()` runs at `PROCESSOR` (400), its first frame after `setup()`.

## Decision

- **A select of the 7B's device, « Sens de l'écran »** (`tab5_sens_ecran`, `Tab5/paquets/carte-waveshare-7b.yaml`): « Normal » (default, the board as Waveshare mounts it) or « Retourné » (upside down), `entity_category: config`, `restore_value: true`. Chosen with the mouse in Home Assistant (the tablet's dashboard, « Tablette » column, or the device page, Configuration card), kept in NVS across restarts and updates. Its help sentence is in [tablet settings](../installation/settings.md) and [`docs/waveshare-7b.md`](../waveshare-7b.md).
- **« Retourné » = 180° more than the board's rest angle**: `rotation_sens(kRotationRepos, retourne)` (`Tab5/socle/tab5_geometrie.h`, pure, checked by `static_assert` and `tools/test_tab5_socle.cpp`). 7B: 180 → 0. `carte_rotation` stays the angle of a board mounted the maker's way; the published firmware keeps 180.
- **One value read by the whole firmware**: `rotation_repos()` (an inline function of `tab5_geometrie.h`), `kRotationRepos` without a choice. `dalle_sens_choisir()` (`Tab5/ecran/tab5_dalle.cpp`) sets it and turns LVGL, unless a game holds its own angle (`rotation_tenue()`, set by Neon Apron while in portrait; it puts `rotation_repos()` back when it leaves); same angle, nothing redrawn. The boot bar and Neon Apron read `rotation_repos()` instead of the constant.
- **The 7B only.** The Tab5 keeps its compiled 270° and gets no entity: its configuration does not change. Turning a Tab5 upside down would also need the tilt games (IMU axes) and Neon Apron's portrait to follow; nobody asked for it.

## Rejected

- **Two published 7B firmwares** (180° and 0°): two more builds and manifests per release, a choice on the install page, and a wrong choice means reinstalling.
- **A « Tab5 · » list pushed by Home Assistant**: the picture would be upside down at every boot until Home Assistant pushes, and nothing at all without it; each screen of a home has its own mounting ([ADR-0063](0063-several-screens.md)), a device setting is the right place.
- **Flipping the panel in hardware** (a scan-direction register of the EK79007): would make both mountings free, but the register was not checked on this panel, and a wrong value could leave a black screen.
- **Keeping `-s carte_rotation 0`**: a local build has no « Firmware » entity and never follows the releases.

## Consequences

- A board mounted upside down runs the published firmware and gets its updates from Home Assistant; « Retourné » also draws faster (no rotation of each frame).
- The choice reaches the flash at ESPHome's next preferences write (`flash_write_interval`, 60 s by default, not changed by the project), like every setting of the tablet: a power cut within that minute keeps the previous direction.
- A new board, or one erased by the install page, starts on « Normal »: a board mounted upside down shows the picture upside down until « Retourné » is picked (one click; the touch already follows the upside-down picture).
- Never combine `-s carte_rotation 0` with « Retourné »: the picture would turn twice (`[AI-WARNING]` in `carte-waveshare-7b.yaml`). The author's board, built at 0° until now, moves once: install the published firmware (or a build without `-s carte_rotation`), then pick « Retourné ».
- API contract unchanged: one more entity of the device, no action, no event, nothing in the HA packages. The tablet's dashboard (`custom_templates/tab5_dashboard.jinja`) shows it in its « Tablette » column, with its help sentence, when the entity exists (the Tab5 has none). The off-device render has no select: its 7B captures stay at `carte_rotation`.
- `tests/test_sens_ecran.py` checks the select, that no other board has it, the boot order, that the bar and Neon Apron read the chosen angle and that the publication forces no angle.
- Not checked on a board when written: the change while running from Home Assistant, the boot bar and the first frame at 0° restored from NVS, the published firmware on the author's board.

## Update (2026-10-24)

The options of the selects named here are English since [ADR-0070](0070-home-assistant-reads-english.md) (« Normal / Upside down »; « Upside down » is what this ADR calls « Retourné »); same order, the tablet keeps the index. The decision above stands.

