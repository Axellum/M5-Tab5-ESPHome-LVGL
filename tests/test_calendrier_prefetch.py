# -*- coding: utf-8 -*-
"""Pré-fetch du calendrier : chaque mois demandé une seule fois au démarrage.

Base de HA, redémarrages du 01/10/2026 : à chaque démarrage, la tablette émettait
esphome.tab5_calendrier_mois pour octobre, novembre, puis encore octobre et novembre.
on_boot (tab5-ha-hmi.yaml) et le front montant de status_ha
(tab5-sensors-diagnostics.yaml) lancent tous deux le pré-fetch, à ~3 s d'écart, et le
script en mode `restart` repartait de zéro. tab5_cal_prefetch saute maintenant un mois
reçu il y a moins de CAL_PREFETCH_FRESH_MS, attend la fin d'un premier appel au lieu de
le couper (`queued`), et le bouton « Recharger le calendrier » force.
"""
import os
import re

import yaml

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda *_: None)


def _lire(*chemin):
    with open(os.path.join(REPO, *chemin), encoding="utf-8") as f:
        return f.read()


def _scripts():
    arbre = yaml.load(_lire("Tab5", "tab5-calendar.yaml"), Loader=_Chargeur)
    return {s["id"]: s for s in arbre["script"]}


def _chaines(noeud):
    if isinstance(noeud, str):
        yield noeud
    elif isinstance(noeud, list):
        for x in noeud:
            yield from _chaines(x)
    elif isinstance(noeud, dict):
        for x in noeud.values():
            yield from _chaines(x)


def test_prefetch_boot_sans_parametre():
    # on_boot l'appelle sans argument : ESPHome refuse un script.execute sans tous
    # les paramètres, un paramètre ici obligerait à toucher on_boot.
    s = _scripts()["tab5_cal_prefetch_boot"]
    assert "parameters" not in s
    assert s["then"] == [{"script.execute": {"id": "tab5_cal_prefetch", "force": False}}]


def test_prefetch_en_file_d_attente():
    s = _scripts()["tab5_cal_prefetch"]
    assert s["mode"] == "queued", "`restart` coupait le premier appel pendant son délai (M+1 perdu)"
    assert s["parameters"] == {"force": "bool"}


def test_chaque_demande_sautee_si_le_mois_est_frais():
    demandes = [c for c in _chaines(_scripts()["tab5_cal_prefetch"]["then"]) if "tab5_cal_request" in c]
    assert len(demandes) == 2, "mois courant puis M+1"
    for lam in demandes:
        assert re.search(r"if \(force \|\| cal_month_is_stale\([^;]*CAL_PREFETCH_FRESH_MS\)\)\s*"
                         r"id\(tab5_cal_request\)\.execute", lam), lam


def test_appelants():
    # Démarrage et reconnexion : le script sans paramètre. Bouton : le corps, forcé.
    assert "script.execute: tab5_cal_prefetch_boot" in _lire("tab5-ha-hmi.yaml")
    assert "script.execute: tab5_cal_prefetch_boot" in _lire("Tab5", "tab5-sensors-diagnostics.yaml")
    controles = yaml.load(_lire("Tab5", "tab5-ha-controls.yaml"), Loader=_Chargeur)
    bouton = next(b for b in controles["button"] if b.get("id") == "tab5_reload_calendar")
    assert bouton["on_press"] == [{"script.execute": {"id": "tab5_cal_prefetch", "force": True}}]


def test_fraicheur():
    m = re.search(r"constexpr uint32_t CAL_PREFETCH_FRESH_MS = (\d+);", _lire("Tab5", "tab5_custom.h"))
    assert m, "CAL_PREFETCH_FRESH_MS introuvable dans tab5_custom.h"
    ms = int(m.group(1))
    # Au moins 10 s : les deux appels du démarrage sont à ~3 s d'écart, plus 2 s entre M
    # et M+1. Moins que les 10 min du rendu : une reconnexion doit encore rafraîchir.
    assert 10000 <= ms < 600000, ms
