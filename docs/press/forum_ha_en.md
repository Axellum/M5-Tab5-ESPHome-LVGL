# Forum Home Assistant — brouillon (EN), à publier à la 3.0

> **Lot 8 de l'audit « ouverture »** (choix d'Axel, 27/09/2026) : forum HA et Hackster
> seulement, textes préparés maintenant, **publiés par Axel après la 3.0** (flasheur web
> du lot 6c), ton « partagé au cas où ». Catégorie : **Share your Projects**.
>
> **Mis à jour le 04/10/2026** pour les pièces de la 3.2 (blueprint et limites) ; le reste date de la 3.0
> (« French or English » : sept langues aujourd'hui, titre « Installing (3.0) ») — à relire en entier avant de publier.
>
> **À vérifier juste avant de publier :**
> - la 3.0.0 est publiée et le flasheur web (https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/) la propose en version stable ;
> - les limites ci-dessous sont toujours vraies (révisions testées, pièces et zones à place unique) ;
> - image : `docs/images/rendu/1-journee-ensoleillee-en.png` (capture générée) ou la
>   photo `docs/images/tab5_photo_home.jpg` ; le GIF `docs/images/tab5_ui_tour.gif` en
>   second si le forum l'accepte.

---

## Title

M5Stack Tab5 as a Home Assistant wall screen (ESPHome + LVGL, push-only)

## Body

Hi all,

I have been using an M5Stack Tab5 as the screen of my home for a few months. I'm sharing the firmware here in case it's useful to someone with the same tablet.

*(image)*

**What it is**

- ESPHome firmware with an LVGL interface (1280×720), running on the tablet itself: no browser, no dashboard page.
- Home Assistant pushes what changed; the tablet never polls.
- Clock and alarm clock, 15-day forecast, rain in the next hour, climate, lights, shutter, plant sensors, a TV remote, a local "Okay Nabu" wake word, and a few offline games.
- French or English, switchable from Home Assistant.

**Installing (3.0)**

- Flash it from the browser over USB: https://axellum.github.io/M5-Tab5-ESPHome-LVGL/install/. The Wi-Fi is set right after, from the same page or from the tablet's fallback access point.
- Add the tablet in Home Assistant; Home Assistant gives it its encryption key. There is no secret in the firmware, and updates must be signed.
- Pick your devices with a blueprint. Since 3.2 the tiles at the bottom are rooms: up to 5 rooms of 5 devices (lights, switches, covers, media players, scenes, sensors…), with their names and icons taken from Home Assistant. Climate, TV, phone battery, two temperatures and up to 5 plant sensors have their own places. What you leave empty disappears from the screen.
- Weather comes from any weather entity. Rain in the next hour and warnings come from Météo-France by default; OpenWeatherMap and MeteoAlarm can be chosen instead.
- Without Home Assistant, a demo mode fills the screen with made-up data.

**Limits**

- Used every day on the ST7123 revision; the ST7121 build runs on another user's unit; the original ILI9881C build compiles but has never run.
- The layout started from my home: outside the rooms, each zone has a single place (one climate card, one TV, two temperatures, up to 5 plants).
- It was written with AI assistants; I'm more the architect than the author.

Repository, documentation and Discussions: https://github.com/Axellum/M5-Tab5-ESPHome-LVGL
