# ADR-0049: A Cameras popup — a still image of each Home Assistant camera, refreshed while the popup is open

**Status:** Proposed (2026-10-09; draft PR, not tried on a tablet nor with a real camera when written).
**Date:** 2026-10-09

## Context

A tester (discussion #278, Tab5 with a battery, Tapo cameras) asked to see his cameras on the tablet. The author's choice: no live video (no decoder for H.264 on this firmware, a stream would hold the Wi-Fi and the main loop), but a **still image** of each camera, refreshed every few seconds, **only while the popup is open**.

What Home Assistant offers, read in `homeassistant/components/camera/__init__.py` (branch `dev`, 2026-10-09):

- every camera has an `entity_picture` attribute, `/api/camera_proxy/<entity_id>?token=<token>`; the token is rotated every 5 minutes (`TOKEN_CHANGE_INTERVAL`) and the previous one stays valid (`access_tokens`, a `deque(maxlen=2)`): a URL read at any time works for at least 5 more minutes;
- `CameraImageView` reads `width` and `height` from the query and, **only when both are given** and the image is a JPEG, scales it down with TurboJPEG by the smallest factor of 1/8 … 7/8 that stays **at or above** both sizes (`scale_jpeg_camera_image`, quality 75). Without TurboJPEG the image comes unscaled.

On the tablet, ESPHome 2026.9 has `online_image` (already used for the image of the voice answer, `tab5-assist.yaml`): JPEG through JPEGDEC, decoded **in one call** once the whole file is downloaded, `resize: WxH` keeping the proportions, buffers from `RAMAllocator` (PSRAM first), `online_image.release` to free the image. Its `update()` waits for the HTTP headers synchronously (the `http_request` client of the assistant: 12 s timeout).

## Decision

- **One popup, one page per camera** (`ui_components/cameras_popup.yaml`, `Tab5/ecran/tab5_cameras.cpp`), shared chrome ([ADR-0009](0009-modal-shell-header.md), header icon `cctv` U+F07AE), registered as « Caméras » ([ADR-0013](0013-single-registry-consoles-modals.md)): a 960 × 540 image (16:9) in a glass frame, the camera's name and « Image de 14:32:05 » under it, one pagination dot per camera (`pagination_afficher()`, the dots of the home page). A swipe changes camera through the shared paged-popup brick (`pages_brancher()`, [ADR-0046](0046-popups-a-pages.md)). The names are not in the title bar like the Lights popup: eight names would get 95 px each; the name sits under the image instead. Messages through `tr()`: « En attente de Home Assistant », « Aucune caméra choisie », « Chargement... », « Image indisponible », « Adresse de Home Assistant inconnue », « Plus d'image depuis … ».
- **One image at a time.** One `online_image` (`cameras_image`, JPEG, RGB565, `resize: 960x540`) for all cameras; the next download starts 5 s after the previous one ended (`tab5_cameras_attente`, `mode: restart`: a single pending callback), the camera shown only. When the popup is closed (cross, the 45 s inactivity return, another screen), the next callback — or the end of the download in flight — releases the image (`online_image.release`) and nothing more is requested.
- **Contract: a minor addition** (`contrat/contrat.yaml` 1.1.0). Event `esphome.tab5_cameras` (no data), sent at the opening, every 4 minutes while open and after a failed image (30 s at most often); action `tab5_maj_cameras(adresse, cameras)`: `cameras` = « nom|image;… » in the blueprint's order, eight at most, `image` = the camera's `entity_picture` (or a full URL); `adresse` = the base URL of Home Assistant as the tablet must call it, empty for the default. Payload reading is pure C++ in `tab5_parse` (section 9: `cameras_lire()`, `camera_url()`, `camera_base_depuis_hote()`), tested by `tools/test_parse.cpp` and fuzzed by `tools/fuzz/fuzz_parse.cpp` (`;` selector).
- **The URL.** A relative `entity_picture` is prefixed with the blueprint's « Adresse de Home Assistant » when given, otherwise with `http://<address of the API client>:8123` — the address Home Assistant connects to the tablet from (`on_client_connected`, client « Home Assistant »; IPv6 in brackets, a zone `%eth0` refused). The tablet adds `width=960&height=540` to a `/api/camera_proxy/` URL so that HA sends at most a 3/8 or 1/2 scaled image (a 2560 × 1440 camera arrives as 960 × 540, a 1920 × 1080 one as 960 × 540, a 2304 × 1296 one as 1152 × 648, then reduced by the tablet). A URL with a space or a control character is refused (it would break the HTTP request).
- **Token rotation without a loop in HA.** The tablet asks again every 4 minutes (< 5 minutes, the rotation), so the URL it holds is always valid; a 401 (or any failed image) asks again too. The blueprint answers each request with one action, from the `entity_picture` of the moment: **no script, no package** — the Énergie model ([ADR-0028](0028-solar-energy-popup.md)) keeps a script looping in HA because it pushes live values; here the tablet already polls the image, a loop on the HA side would only duplicate the timer.
- **Opening.** Option « Caméras » at the end of the « Aller à l'écran » select (`Ecran::CAMERAS`, index 18; ARCADE moves to 19, which is not stored): Home Assistant can open it, for instance from a doorbell automation, with no new service. Gesture code `cameras` at the end of `kCodesGestes` (index 23, stored in NVS) and of the blueprint's gesture lists ([ADR-0039](0039-gestes-accueil.md)); glyph `cctv` in `mdi_font_70`, `mdi_font_26` (top buttons and their mini icons) and `mdi_font_32` (header). Always available (`ecran_disponible()`): the tablet does not know the cameras before it asks; an empty section answers with an empty list and the popup says so.
- **Blueprint.** Section « Caméras · Cameras » (collapsed): `cameras_liste` (entities of domain `camera`, multiple, reorder) and `cameras_adresse` (text, empty by default). No real value, no placeholder ([ADR-0024](0024-packages-without-placeholders.md)): the example address in the description is `https://ha.example.com` (a reserved example domain; `.local` names are not relied on, their resolution by the tablet is not checked).

## Rejected

- **A tile for a camera.** A new tile type (`cam`) would have to be added to the tile model, the blueprint's tile domains, the room pages and the tile popups; the popup with the select and a gesture covers the need for a fraction of the cost. It can come later, opening this popup on the camera.
- **A navigation-wheel button.** The « Appareils » family of the wheel ([ADR-0042](0042-navigation-wheel.md)) is full (six choices); adding a family is a layout change of the wheel. Left for later.
- **Live video or MJPEG** (`/api/camera_proxy_stream`): a never-ending response would hold the main loop of `online_image`; no H.264/H.265 decoder.
- **HA pushing the image itself** (base64 in a service call): an ESPHome API string is not meant for 100 kB payloads, and HA would push while nobody looks.
- **A script and a package in HA re-pushing on token change** (the request's first idea): it works, but needs one more file to install and a loop per open popup, for what a 4-minute request of the tablet does.

## Costs (estimated, not measured on the tablet)

- **Memory.** The decoded image: 960 × 540 × 2 = 1 036 800 bytes in PSRAM while the popup is open, released at closing. The download buffer grows to the size of the JPEG (the whole file is needed to decode; about 100 to 200 kB at 960 × 540, estimate) and **stays** allocated after `release()` (an ESPHome limit: `DownloadBuffer` only grows). JPEGDEC's working state comes with the decoder (size not checked).
- **Main loop.** While HA prepares the snapshot, the loop waits for the headers (from a few hundred ms for a camera with a still-image URL to several seconds for one that HA reads through ffmpeg or a stream, up to the 12 s timeout); then the JPEG is decoded in one call, pixel by pixel through ESPHome's draw callback — estimated 0.2 to 0.5 s for 960 × 540, more for 1152 × 648. The screen does not refresh and touch is not read during that time, once every 5 s or more. To be measured on the tablet (`[online_image]` log lines « Image fully downloaded, N bytes in M ms »); the 5 s gap is the knob.
- **Formats.** Progressive JPEG is refused by ESPHome's decoder (« Image indisponible »); a camera whose `entity_picture` is not a JPEG (a PNG) fails the same way.

## Consequences

- The author decides whether 5 s between images and the 45 s auto-close (the popup is a popup: no touch for 45 s closes it, a doorbell image included) are right.
- A Home Assistant behind a reverse proxy, on another port or in https needs the « Adresse de Home Assistant » field (`verify_ssl: false` on the tablet's HTTP client already).
- Two tablets on the same automation are not a case: one automation per tablet (the blueprint's rule).
