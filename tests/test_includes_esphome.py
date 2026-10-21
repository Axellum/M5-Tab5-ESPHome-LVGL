# -*- coding: utf-8 -*-
"""[AI-CONTEXT] Aucune valeur d'action « apply » ni champ d'événement HA ne nomme notre C++.

Depuis ESPHome 2026.10.0, deux sortes de valeurs `!lambda` sont écrites dans `main.cpp`
AVANT les `includes:` du firmware (vu le 21/10/2026, en générant le même code avec 2026.9.0
puis 2026.10.0) :
  * les champs `data:` d'un `homeassistant.event` / `homeassistant.action`, rangés dans une
    table en flash (esphome/esphome#20237) ;
  * la valeur d'une action ou d'une condition « apply » (`register_apply_action` /
    `register_apply_condition` : `select.set_index`, `rtttl.play`, `media_player.volume_set`,
    `globals.set`…), devenue une fonction statique.
Elles ne voient donc ni nos fonctions (`journal_*`, `tr()`…), ni nos constantes, ni nos
variables globales C++ : 13 erreurs « was not declared in this scope » sur la CI du
21/10/2026. Une `lambda:` ordinaire, elle, reste dans `setup()`, après les inclusions.

La règle : une telle valeur ne nomme que des paramètres (du script, du service, du
déclencheur), des `id(…)` et la bibliothèque standard. Ce qui demande notre C++ passe par
une `lambda:` (qui peut appeler un script à paramètres : `tab5_annonce_dire`,
`tab5_journal_evenement`).

`APPLY` : les noms relevés dans le code d'ESPHome 2026.10.0. Pour les relire, chercher les
appels `register_apply_action("…")` / `register_apply_condition("…")` dans son paquet Python.
"""
from __future__ import annotations

import re

import yaml

from tests.commun import REPO, TAB5, BaseChargeur, fichiers_du_depot

APPLY = frozenset("""
    ags10.new_i2c_address ags10.set_zero_point aic3204.set_auto_mute_mode animation.next_frame
    animation.prev_frame animation.set_frame at581x.reset at581x.settings audio_adc.set_mic_gain
    audio_dac.set_volume binary_sensor.invalidate_state binary_sensor.is_off binary_sensor.is_on
    bk72xx_ble_tracker.stop_scan bl0906.reset_energy ble_server.numeric_comparison_reply
    bluetooth_password.set bm8563.read_time bm8563.start_timer bm8563.write_time button.press
    climate.control cm1106.calibrate_zero component.is_idle counter.increment counter.set_value
    cover.control cover.is_closed cover.is_open cs5460a.restart dfplayer.is_playing dfplayer.play
    dfplayer.play_folder dfplayer.play_mp3 dfplayer.set_current_track_repeat dfplayer.set_device
    dfplayer.set_eq dfplayer.set_volume dfrobot_sen0395.reset display_menu.is_active
    display.is_displaying_page ds1307.read_time ds1307.write_time emontx.send_command
    epaper_spi.full_update_next epaper_spi.is_updating esp32_ble_tracker.start_scan
    esp32_ble_tracker.stop_scan espnow.set_channel event.trigger ezo_pmp.arbitrary_command
    ezo_pmp.change_i2c_address ezo_pmp.dose_volume ezo_pmp.dose_volume_over_time
    ezo_pmp.dose_with_constant_flow_rate ezo_pmp.set_calibration_volume fan.is_off fan.is_on fan.toggle
    fan.turn_off fan.turn_on fingerprint_grow.aura_led_control fingerprint_grow.cancel_enroll
    fingerprint_grow.delete fingerprint_grow.delete_all fingerprint_grow.enroll
    fingerprint_grow.led_control globals.set grove_tb6612fng.break grove_tb6612fng.change_address
    grove_tb6612fng.no_standby grove_tb6612fng.run grove_tb6612fng.standby grove_tb6612fng.stop
    hc8.calibrate hdc302x.heater_off hdc302x.heater_on hlk_fm22x.delete hlk_fm22x.delete_all
    hlk_fm22x.enroll hlk_fm22x.reset hlk_fm22x.scan htu21d.set_heater htu21d.set_heater_level
    hub75.set_brightness it8951.update key_collector.disable key_collector.enable light.control
    light.is_off light.is_on light.turn_off light.turn_on lightwaverf.send_raw
    ln882h_ble_tracker.stop_scan lock.is_locked lock.is_unlocked max17043.sleep_mode
    max6956.set_brightness_global max6956.set_brightness_mode max7219digit.intensity
    media_player.enqueue media_player.is_muted media_player.play_media media_player.volume_set
    mhz19.detection_range_set micro_wake_word.disable_model micro_wake_word.enable_model
    micro_wake_word.is_running micro_wake_word.model_is_enabled micro_wake_word.start
    micro_wake_word.stop microphone.is_capturing microphone.is_muted midea_ac.follow_me
    mixer_speaker.apply_ducking motion.clear_calibration mqtt.connected mqtt.publish number.operation
    number.set online_image.release online_image.set_url openthread.set_poll_period output.set_level
    pcf85063.read_time pcf85063.write_time pcf8563.read_time pcf8563.write_time pmwcs3.air_calibration
    pmwcs3.new_i2c_address pmwcs3.water_calibration pn532.is_writing pulse_counter.set_total_pulses
    pulse_meter.set_total_pulses pzem6l24.reset_energy pzemac.reset_energy pzemdc.reset_energy
    remote_transmitter.digital_write rf_bridge.beep rf_bridge.learn rf_bridge.send_advanced_code
    rf_bridge.send_code rf_bridge.send_raw rf_bridge.start_advanced_sniffing
    rf_bridge.start_bucket_sniffing rf_bridge.stop_advanced_sniffing rp2_ble_tracker.stop_scan
    rtttl.is_playing rtttl.play rtttl.stop rx8130.read_time rx8130.write_time safe_mode.mark_successful
    scd30.force_recalibration_with_reference scd4x.factory_reset scd4x.perform_forced_calibration
    script.is_running script.stop select.operation select.set select.set_index sen5x.start_fan_autoclean
    sendspin.cancel_pairing_window sendspin.confirm_pairing_window sendspin.switch servo.detach
    servo.write sim800l.connect sim800l.dial sim800l.disconnect sim800l.send_sms sim800l.send_ussd
    snapshot.take sound_level.start sound_level.stop speaker_source.set_playlist_delay
    speaker.is_playing speaker.is_stopped speaker.volume_set sun.is_above_horizon sun.is_below_horizon
    switch.control switch.is_off switch.is_on sx126x.set_mode_sleep tas2780.activate
    tas2780.update_config text_sensor.state text.set time.has_time tinyusb.is_mounted tm1651.turn_off
    tm1651.turn_on uart_mux.is_local uart_mux.select_bridge uart_mux.select_local
    ufire_ec.calibrate_probe ufire_ec.reset ufire_ise.calibrate_probe_high ufire_ise.calibrate_probe_low
    ufire_ise.reset update.check update.is_available update.perform valve.control
    voice_assistant.connected voice_assistant.is_running voice_assistant.start
    voice_assistant.start_continuous voice_assistant.stop wireguard.disable wireguard.enable
    wireguard.enabled wireguard.peer_online zigbee.factory_reset
""".split())

# Actions dont les champs sont rangés dans la table en flash (esphome/esphome#20237).
EVENEMENTS_HA = frozenset({"homeassistant.event", "homeassistant.action", "homeassistant.service"})

# Ce qu'une valeur peut nommer sans notre C++ : mots du langage, bibliothèque standard,
# fonctions d'ESPHome visibles partout (esphome.h est inclus en tête de main.cpp).
PERMIS = frozenset("""
    return if else auto const char int float double bool unsigned long short size_t int32_t
    uint32_t uint8_t int8_t uint16_t int16_t true false nullptr sizeof static_cast
    std string to_string snprintf id x esphome
""".split())


class _Lambda(str):
    """Corps d'un `!lambda`."""


class _Chargeur(BaseChargeur):
    """`!lambda` gardé comme `_Lambda`, les autres balises valent None."""


_Chargeur.add_constructor("!lambda", lambda chargeur, noeud: _Lambda(chargeur.construct_scalar(noeud)))
_Chargeur.add_multi_constructor("!", lambda _chargeur, _suffixe, _noeud: None)

_CHAINE = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')
_COMMENTAIRE = re.compile(r"//[^\n]*|/\*.*?\*/", re.S)
_ID = re.compile(r"\bid\s*\(\s*\w+\s*\)")
_NOM = re.compile(r"(?<![\w.>:])[A-Za-z_]\w*")
_LOCALE = re.compile(r"\b(?:char|int|float|double|bool|auto|size_t|string|u?int\d+_t)\s+[*&]?\s*([A-Za-z_]\w*)")


def _noms_libres(corps: str) -> set[str]:
    """Identifiants d'une valeur qui ne sont ni un membre (`.x`, `->x`, `::x`), ni un `id(…)`,
    ni une variable déclarée dans la valeur (`char b[16];`)."""
    texte = _ID.sub(" ", _CHAINE.sub(" ", _COMMENTAIRE.sub(" ", corps)))
    locales = {m.group(1) for m in _LOCALE.finditer(texte)}
    return {m.group(0) for m in _NOM.finditer(texte)} - locales


def _lambdas(noeud):
    """Tous les `!lambda` d'un sous-arbre."""
    if isinstance(noeud, _Lambda):
        yield noeud
    elif isinstance(noeud, dict):
        for valeur in noeud.values():
            yield from _lambdas(valeur)
    elif isinstance(noeud, list):
        for valeur in noeud:
            yield from _lambdas(valeur)


def _parametres(noeud) -> set[str]:
    """Paramètres d'un script (`parameters:`) ou d'un service de l'API (`variables:`)."""
    noms = set()
    for cle in ("parameters", "variables"):
        if isinstance(noeud.get(cle), dict):
            noms |= set(noeud[cle])
    return noms


def _fautes(noeud, parametres: frozenset[str], fichier: str):
    if isinstance(noeud, list):
        for valeur in noeud:
            yield from _fautes(valeur, parametres, fichier)
        return
    if not isinstance(noeud, dict):
        return
    parametres = parametres | _parametres(noeud)
    for cle, valeur in noeud.items():
        if cle in APPLY or cle in EVENEMENTS_HA:
            for corps in _lambdas(valeur):
                for nom in sorted(_noms_libres(corps) - PERMIS - parametres):
                    yield f"{fichier} : {cle} nomme « {nom} »"
        yield from _fautes(valeur, parametres, fichier)


def _yaml_du_firmware():
    yield REPO / "tab5-ha-hmi.yaml"
    yield REPO / "tab5-rendu-host.yaml"
    yield from fichiers_du_depot(TAB5, "*.yaml")


def test_liste_apply_lue():
    """La liste n'est pas vide par accident, et contient les trois actions du 21/10/2026."""
    assert len(APPLY) > 150
    assert {"select.set_index", "rtttl.play", "media_player.volume_set", "globals.set"} <= APPLY


def test_detecteur():
    """Le détecteur voit bien un symbole à nous, et laisse passer paramètres et id()."""
    assert _noms_libres('return std::string(tr("Volet arrêté."));') == {"return", "std", "tr"}
    assert _noms_libres("return id(alarm_ring_cycle) - 1 + cfg.volume;") == {"return", "cfg"}
    exemple = {"script": [{"id": "s", "parameters": {"message": "string"}, "then": [
        {"homeassistant.event": {"event": "esphome.x", "data": {
            "a": _Lambda("return message;"), "b": _Lambda("return journal_boot_count();")}}}]}]}
    assert list(_fautes(exemple, frozenset(), "x.yaml")) == ["x.yaml : homeassistant.event nomme « journal_boot_count »"]


def test_aucune_valeur_avant_les_includes_ne_nomme_notre_cpp():
    fautes = []
    for chemin in _yaml_du_firmware():
        # Une fusion `<<: !include x` se résout sur les nœuds, avant toute balise : clé neutre.
        texte = chemin.read_text(encoding="utf-8").replace("<<: !include", "inclus_: !include")
        contenu = yaml.load(texte, Loader=_Chargeur)
        fautes += _fautes(contenu, frozenset(), chemin.relative_to(REPO).as_posix())
    assert not fautes, ("Valeur écrite avant les includes: par ESPHome 2026.10+ (passer par une "
                        "lambda: ou un script à paramètres) :\n" + "\n".join(fautes))
