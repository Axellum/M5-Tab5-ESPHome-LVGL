# ADR-0034: Alerts of the central card — Home Assistant remembers each alert and its revision, a read alert comes back only when it changes, subscriptions and history live in HA

**Status:** Accepted (2026-10-06, asked for and decided by the author; lots 0 to 3 deployed on the author's Home Assistant, lot 4 not yet tried on a tablet)
**Date:** 2026-10-06

## Context

The central card of the home screen rotates the schedule, the rain, the weather warning and up to four Home Assistant alert banners (updates, `problem` sensors, unavailable entities). The author asked for three things:

1. choose which kinds of alerts are shown (« abonnements », subscriptions);
2. an alert stays until it is tapped, and then never comes back — even after a restart of Home Assistant — unless it changes;
3. keep a history of the alerts, and show it.

Before this work, a tap added the alert's id to `input_text.tab5_alerts_dismissed`, a list that a nightly automation purged. That list was declared with `initial: ""`. With an initial value, HA does not restore the old one (code of `input_text` in HA 2026.9.4), so every restart emptied it. The author's HA history shows it on 29/09 at 13:43 and on 03/10 at 04:49. An id also said nothing about *what* was read: an update read at 2026.9 and a new update to 2026.10 had the same id.

The constraints still hold:
- push-only and events-only (ADR-0001, ADR-0025): the firmware names no entity and calls no action;
- public HA files carry no real value (ADR-0024);
- an older firmware or older HA files must keep working.

## Decision

- **HA decides, the tablet shows.**
  - The sensor « Tab5 Alertes » (`sensor.tab5_alertes`, `packages/tab5_alerts.yaml`) holds the alert memory in its attributes. The rules are the macros of `custom_templates/tab5_alertes.jinja`.
  - The tablet keeps nothing but what it last received, plus a local cache of its own taps. That cache only hides a banner until HA's next push.
- **An alert is a source plus a revision.**
  - The tablet receives `id#revision` and sends back, on a tap, the one it was showing (event `esphome.tab5_alerte_lue`).
  - Once read, an alert comes back only when its revision changes: a newer version, another warning level or other phenomena, a new unavailable entity.
  - It also comes back when it really stops and starts again: back to normal for 5 minutes, or gone for an hour.
  - It never comes back from a source that is unavailable or unknown, nor in the 15 minutes after HA starts. An unread alert that goes back to normal leaves the screen at once.
- **The memory survives restarts and crashes.**
  - The sensor restores its attributes at start.
  - The automation `tab5_alertes_sauvegarde` calls `homeassistant.save_persistent_states` as soon as an alert is read. Without it, HA only writes restored states at a clean stop and every 15 minutes (`STATE_DUMP_INTERVAL`), so a crash lost the tap.
  - The computation is one block (the `variables:` of a trigger-based template), so a tap cannot be lost behind the computation of the minute.
  - No `input_text` of the packages has an `initial:` any more.
- **Subscriptions are six lists « Tab5 · alertes : … »**, set in HA or on the Settings page of the tablet's dashboard:
  - updates;
  - weather warning from yellow, orange or red;
  - `problem` sensors;
  - unavailable entities;
  - entities labelled « Tab5 · alerte »;
  - batteries under 10 to 30 %.

  They are lists rather than switches: without `initial`, a list starts on its first option and HA restores the choice, whereas an `input_boolean` would start off. Unsubscribing hides at once. The alerts stay tracked, so subscribing again does not bring back what was already read.
- **Order on the screen.**
  - HA sends the banners red, orange, yellow, then oldest first.
  - Past four, each banner shows its rank (« 2/6 ») from a `@n:total` token at the head of the payload. An older firmware ignores the token.
  - A new red alert takes the central card at once, and the rotator restarts so it stays a full turn. Then it rotates with the rest: the weather and the rain stay visible.
- **History.**
  - HA keeps the last 30 alerts (appeared, read, ended, severity, text, source) in the sensor's `historique` attribute.
  - A long press on the central card opens the « Alertes » popup (since [ADR-0042](0042-navigation-wheel.md), 2026-10-09: through « Alertes », the first button of the navigation wheel that the long press opens) (ADR-0009 chrome, ADR-0013 registry). It shows the 20 latest alerts the user is subscribed to, and a « Tout marquer comme lu » button that sends `alert_id: "*"`.
  - The popup **asks** for the history when it opens (event `esphome.tab5_alertes_historique`). HA answers with the action `tab5_maj_alertes_historique` and pushes again while the tablet's current screen is « Alertes » and the history changes.
  - The Health view of the HA dashboard shows the same list.

## Compatibility

- **History on request, not in the alert push.** The validated plan put the history in `tab5_push_alertes`. A new HA would then call an action unknown to an older firmware, and « Action not found » stops the script: `continue_on_error` does not catch it. Asked for by the tablet instead, the history leaves only toward a firmware that knows it, so firmware and HA files can be updated in either order. A new firmware with older HA files shows « En attente de Home Assistant ».
- **The rank token `@n:` has no « | »**, so an older firmware's parser skips it.
- **Former list.** `input_text.tab5_alerts_dismissed` is read once, then never written. The exception is « ha:unavailable », which had silenced the unavailable-entities alert for good: it shows once after the update. The nightly purge is gone.
- **Older history entries**, written before the source was kept, infer it from their id (`update.*`, `meteo:vigilance`, `ha:indispo`).

## Consequences

- One more package (`tab5-alertes.yaml`), one more C++ unit (`tab5_alertes.cpp`) and one more UI file (`alertes_popup.yaml`). There is one more action (22) and one more event.
- **Deviations from the plan, accepted:**
  - a 15-minute grace after HA starts;
  - the memory in template variables rather than in helpers;
  - lists instead of switches;
  - mobile-app phones excluded from the battery alerts (charged every day);
  - a history of 30;
  - a red warning also jumps first;
  - a red alert counts as new after a reboot of the tablet.
- **Proofs:**
  - `tests/test_alertes_ha.py` replays the real macros in a Jinja sandbox (restarts, crashes, versions, outages, taps, subscriptions, history payload);
  - `tests/test_alertes_ecran.py` holds the rank token;
  - the « Installation dans un HA neuf » job reads a demo alert, kills and restarts HA, installs an update, and subscribes and unsubscribes;
  - the off-device render shows the screens « accueil-alertes-ha-compteur » and « alertes »;
  - the sanitizers fuzz the new action.
- **Left open:**
  - the four central banners are named in two places of the firmware YAML (2026-10-08): the whole table in `Tab5/paquets/tab5-alertes.yaml` (script `tab5_ha_alert_slots_init`), and their frames in the `on_boot` of `tab5-ha-hmi.yaml`, copied in `tab5-rendu-host.yaml` — a sequence not touched without the author's agreement;
  - the long press of the forecast tiles can still follow a swipe (the alerts popup refuses it, through LVGL's `press_moved` / `gesture_dir`).
