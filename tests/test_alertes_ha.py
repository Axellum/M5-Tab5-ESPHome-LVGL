# -*- coding: utf-8 -*-
"""Mémoire des alertes lues côté Home Assistant (packages/tab5_alerts.yaml).

Une alerte touchée sur la tablette ne doit plus revenir, même après un redémarrage
ou un plantage de HA. Lu dans le YAML réel ; le comportement de HA lui-même est
vérifié par le job « Installation dans un HA neuf » (verifier_memoire_alertes).
"""
from __future__ import annotations

from pathlib import Path

import yaml

RACINE = Path(__file__).resolve().parents[1]
HA = RACINE / "HomeAssistant_Config"


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


def _charger(*chemin: str):
    return yaml.load((HA.joinpath(*chemin)).read_text(encoding="utf-8"), Loader=_Chargeur) or {}


def test_aucun_input_text_des_packages_n_a_d_initial():
    """`initial:` empêche HA de restaurer la valeur au démarrage (input_text de HA
    2026.9 : restauration seulement si la valeur de départ est None). Avec
    `initial: ""`, toutes les alertes lues revenaient à chaque démarrage de HA."""
    fautifs = []
    for fichier in sorted((HA / "packages").glob("*.yaml")):
        for cle, conf in (_charger("packages", fichier.name).get("input_text") or {}).items():
            if isinstance(conf, dict) and "initial" in conf:
                fautifs.append(f"{fichier.name} : input_text.{cle}")
    assert not fautifs, fautifs


def test_le_snippet_de_la_memoire_n_a_pas_d_initial():
    texte = (HA / "snippets" / "tab5_alerts_dismissed_input_text.yaml").read_text(encoding="utf-8")
    conf = yaml.load(texte, Loader=_Chargeur)["tab5_alerts_dismissed"]
    assert "initial" not in conf


def test_le_tap_est_ecrit_sur_le_disque_tout_de_suite():
    """HA n'écrit les états restaurés qu'à l'arrêt propre et toutes les 15 min : après
    un plantage, un tap non sauvegardé serait perdu. La sauvegarde suit l'écriture."""
    actions = [a.get("action") for a in _charger("packages", "tab5_alerts.yaml")["script"]
               ["tab5_dismiss_alert"]["sequence"] if isinstance(a, dict)]
    assert "input_text.set_value" in actions
    assert "homeassistant.save_persistent_states" in actions
    assert actions.index("homeassistant.save_persistent_states") > actions.index("input_text.set_value")
