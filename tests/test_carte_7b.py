# -*- coding: utf-8 -*-
"""Cartes et taille de l'écran (ADR-0061, 12/10/2026) : ce qu'aucun compilateur ne vérifie.

- chaque carte de `CARTES` a son `carte-<carte>.yaml` et son `geometrie-<carte>.yaml`, avec
  les mêmes six substitutions `carte_*` ; la dalle est l'écran tourné de la rotation ;
- tab5-ha-hmi.yaml choisit la carte par `tab5_carte` (défaut `tab5`), au rang de l'ancien
  `tab5_hardware`, et l'IMU selon la carte ;
- `carte-waveshare-7b.yaml` recopie de tab5-hardware.yaml ce qui ne dépend pas de la carte
  (logger, OTA, safe_mode, options d'ESP-IDF, rétroéclairage plafonné, lumière, toucher,
  lecteur, bouton) : mêmes valeurs, pour qu'une correction faite au Tab5 ne s'y perde pas ;
- ses bouchons (`!extend` / `!remove`) visent des id que le firmware du Tab5 déclare, et elle
  ne charge aucun composant du matériel du Tab5 qu'elle n'a pas ;
- la CI la compile (build-carte-7b) et rend ses écrans en 1024×600 (rendu-7b), à part.
"""
import re

import pytest
import yaml
from tests.commun import CARTES, REPO, BaseChargeur, geometrie, lire, source, sources, vue_tab5

CARTE_7B = "carte-waveshare-7b.yaml"
CLES = {"carte_ecran_l", "carte_ecran_h", "carte_dalle_l", "carte_dalle_h", "carte_rotation", "carte_police_echelle"}


class _Balise:
    """`!extend x` / `!remove x` / `!lambda …` gardés avec leur balise, pour les comparer."""

    def __init__(self, balise, valeur):
        self.balise, self.valeur = balise, valeur

    def __eq__(self, autre):
        return isinstance(autre, _Balise) and (self.balise, self.valeur) == (autre.balise, autre.valeur)

    def __repr__(self):
        return f"{self.balise} {self.valeur!r}"


class _Chargeur(BaseChargeur):
    pass


def _balise(chargeur, suffixe, noeud):
    if isinstance(noeud, yaml.ScalarNode):
        return _Balise("!" + suffixe, chargeur.construct_scalar(noeud))
    return _Balise("!" + suffixe, None)


_Chargeur.add_multi_constructor("!", _balise)


def _yaml(nom):
    return yaml.load(source(nom).read_text(encoding="utf-8"), Loader=_Chargeur) or {}


def _par_id(liste, cid):
    trouves = [e for e in liste if isinstance(e, dict) and e.get("id") == cid]
    assert len(trouves) == 1, f"{cid} : {len(trouves)} entrée(s)"
    return trouves[0]


def _tab5():
    return _yaml("tab5-hardware.yaml")


def _b7():
    return _yaml(CARTE_7B)


# ─── Cartes et géométrie ─────────────────────────────────────────────────────

@pytest.mark.parametrize("carte", CARTES)
def test_chaque_carte_a_ses_deux_fichiers(carte):
    g = geometrie(carte)
    assert set(g) == CLES, set(g) ^ CLES
    l, h, dl, dh, rot = (int(g[k]) for k in ("carte_ecran_l", "carte_ecran_h", "carte_dalle_l", "carte_dalle_h", "carte_rotation"))
    assert l > h, "taille de l'écran en paysage"
    assert rot in (0, 90, 180, 270)
    assert (dl, dh) == ((l, h) if rot in (0, 180) else (h, l)), "dalle = écran tourné de la rotation"
    assert 0.5 <= float(g["carte_police_echelle"]) <= 1
    carte_yaml = lire("Tab5", "paquets", f"carte-{carte}.yaml")
    assert f"geometrie: !include geometrie-{carte}.yaml" in carte_yaml


def test_valeurs_des_deux_cartes():
    assert geometrie("tab5") == {"carte_ecran_l": "1280", "carte_ecran_h": "720", "carte_dalle_l": "720",
                                 "carte_dalle_h": "1280", "carte_rotation": "270", "carte_police_echelle": "1"}
    assert geometrie("waveshare-7b") == {"carte_ecran_l": "1024", "carte_ecran_h": "600", "carte_dalle_l": "1024",
                                         "carte_dalle_h": "600", "carte_rotation": "180", "carte_police_echelle": "0.82"}


def test_la_racine_choisit_la_carte_et_l_imu():
    racine = lire("tab5-ha-hmi.yaml")
    assert "  tab5_carte: !include Tab5/paquets/carte-${ tab5_carte | default('tab5') | lower }.yaml" in racine
    assert "  tab5_imu: !include Tab5/paquets/tab5-imu${ '' if (tab5_carte | default('tab5') | lower) == 'tab5' else '-absente' }.yaml" in racine
    # Le matériel et l'écran du Tab5 passent par sa carte, plus par la racine.
    assert "tab5-hardware.yaml" not in re.sub(r"#[^\n]*", "", racine)
    tab5 = lire("Tab5", "paquets", "carte-tab5.yaml")
    assert "tab5_hardware: !include tab5-hardware.yaml" in tab5
    assert "tab5_ecran: !include ecran-${ tab5_ecran | default('st7123') | lower }.yaml" in tab5
    # Projet, nom et nom affiché ne dépendent pas de la carte.
    assert "name: axellum.tab5-ha-hmi" in racine


def test_l_imu_absente_garde_l_interrupteur_lu_par_les_reglages():
    absente = _yaml("tab5-imu-absente.yaml")
    assert set(absente) == {"switch"}
    (inter,) = absente["switch"]
    assert inter["id"] == "tab5_tap_to_wake" and "name" not in inter
    vrai = _par_id(_yaml("tab5-imu.yaml")["switch"], "tab5_tap_to_wake")
    for cle in ("optimistic", "restore_mode", "on_state"):
        assert inter[cle] == vrai[cle], cle


# ─── Ce que la 7B recopie de tab5-hardware.yaml ──────────────────────────────

@pytest.mark.parametrize("bloc", ["logger", "psram", "ota", "safe_mode", "debug"])
def test_blocs_recopies(bloc):
    assert _b7()[bloc] == _tab5()[bloc], f"{bloc} : recopier tab5-hardware.yaml dans {CARTE_7B}"


FREQ_360 = ("CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_360", "CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ")


def test_esp_idf_recopie():
    t, w = _tab5()["esp32"], _b7()["esp32"]
    # Tout pareil, sauf la fréquence du processeur : 360 MHz sur le Tab5 (P4 v1.x, sa limite),
    # 400 MHz sur la 7B (P4 v3 : ESP-IDF 5.5 n'y offre pas 360, ses options étaient ignorées
    # et le build tournait déjà à 400, audit anti-casse du 16/10/2026).
    options_t = dict(t["framework"]["sdkconfig_options"])
    assert (options_t.pop(FREQ_360[0]), options_t.pop(FREQ_360[1])) == ("y", "360")
    assert {**t["framework"], "sdkconfig_options": options_t} == w["framework"]
    assert not set(FREQ_360) & set(w["framework"]["sdkconfig_options"])
    assert w["cpu_frequency"] == "400MHz"
    assert (w["variant"], w["engineering_sample"], w["flash_size"]) == ("esp32p4", False, "32MB")


@pytest.mark.parametrize("composant, cid", [("output", "backlight_plafonne"), ("light", "backlight"),
                                            ("script", "tab5_dalle_rallumer"), ("script", "tab5_dalle_ota"),
                                            ("media_player", "speaker_player"), ("button", "btn_restart")])
def test_entrees_recopiees(composant, cid):
    assert _par_id(_b7()[composant], cid) == _par_id(_tab5()[composant], cid), f"{composant} {cid}"


def test_toucher_recopie():
    t, w = _par_id(_tab5()["touchscreen"], "touch"), _par_id(_b7()["touchscreen"], "touch")
    for cle in ("on_touch", "on_release"):
        assert w.get(cle) == t.get(cle), cle


def test_toucher_de_la_7b_essaye_sur_la_carte():
    """Reset GPIO23 et inversion X/Y avec LVGL à 180° : réglages essayés sur la carte le
    14/10/2026 (sans eux, le tactile ne répondait presque plus)."""
    w = _par_id(_b7()["touchscreen"], "touch")
    assert w["reset_pin"] == "GPIO23"
    assert w["transform"] == {"swap_xy": False, "mirror_x": True, "mirror_y": True}
    assert geometrie("waveshare-7b")["carte_rotation"] == "180"


def test_audio_memes_reglages():
    t, w = _tab5(), _b7()
    adc_t, adc_w = _par_id(t["audio_adc"], "es7210_adc"), _par_id(w["audio_adc"], "es7210_adc")
    assert {k: v for k, v in adc_w.items()} == {k: v for k, v in adc_t.items() if k in adc_w}
    mic_t, mic_w = _par_id(t["microphone"], "tab5_microphone"), _par_id(w["microphone"], "tab5_microphone")
    assert {k: v for k, v in mic_w.items() if k != "i2s_din_pin"} == {k: v for k, v in mic_t.items() if k != "i2s_din_pin"}
    assert _par_id(w["display"], "tab5_display")["dimensions"] == _par_id(t["display"], "tab5_display")["dimensions"]


# ─── Bouchons du matériel absent ─────────────────────────────────────────────

def _ids_du_tab5():
    ids = set()

    def parcourir(noeud):
        if isinstance(noeud, dict):
            if isinstance(noeud.get("id"), str):
                ids.add(noeud["id"])
            for v in noeud.values():
                parcourir(v)
        elif isinstance(noeud, list):
            for v in noeud:
                parcourir(v)

    for chemin in vue_tab5([REPO / "tab5-ha-hmi.yaml", *sources("*.yaml")]):
        if chemin.name != CARTE_7B:
            parcourir(yaml.load(chemin.read_text(encoding="utf-8"), Loader=_Chargeur))
    return ids


def test_bouchons_sur_des_id_du_tab5():
    vises = re.findall(r"id: !(?:extend|remove) (\w+)", lire("Tab5", "paquets", CARTE_7B))
    assert len(vises) >= 12, vises
    connus = _ids_du_tab5()
    assert not set(vises) - connus, f"id inconnus du firmware du Tab5 : {set(vises) - connus}"


def test_pas_de_materiel_du_tab5():
    texte = re.sub(r"#[^\n]*", "", lire("Tab5", "paquets", CARTE_7B))
    for composant in ("pi4ioe5v6408", "platform: ina226", "platform: rx8130", "bmi270", "es8388", "st7123"):
        assert composant not in texte, composant
    # Les bouchons de broches du Tab5 retirent la broche, ils n'en donnent pas une autre
    # (sauf l'ampli, vraie broche de la 7B).
    assert texte.count("pin: !remove") == 8


def test_ci_compile_et_rend_la_7b():
    ci = lire(".github", "workflows", "esphome-tab5.yml")
    assert "  build-carte-7b:" in ci and "printf '\\ntab5_carte: waveshare-7b\\n'" in ci
    rendu = lire(".github", "workflows", "rendu-host.yml")
    assert "  rendu-7b:" in rendu and "-s tab5_carte waveshare-7b" in rendu
    assert "name: rendu-1024x600" in rendu
