# -*- coding: utf-8 -*-
"""Mode démo (tools/demo/) : il se fait passer pour HA, sans relire le firmware. Ce qui
ne se voit qu'une fois flashé est vérifié ici :

- les entités simulées sont celles du modèle Tab5/user_entities.example.yaml, avec
  lequel la démo se flashe (l'entité PC y avait divergé le 16/07/2026 : le PC ne
  répondait plus) ;
- les clés de zones (lot 5) sont celles de la tablette (kCles, Tab5/tab5_zones.cpp) ;
- les deux modes, complet et « maison minimale », passent à blanc ;
- les codes du lot 4c (pluie « @niveau,début », bandeau « @ha|… ») et le format des
  horaires suivent le contrat."""
import contextlib
import io
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "demo"))

import demo_pusher  # noqa: E402
import scenarios  # noqa: E402


def _modele():
    valeurs = {}
    with open(os.path.join(REPO, "Tab5", "user_entities.example.yaml"), encoding="utf-8") as f:
        for ligne in f:
            m = re.match(r"^(entity_\w+):\s*(\S+)\s*$", ligne)
            if m:
                valeurs[m.group(1)] = m.group(2)
    return valeurs


def test_entites_simulees_du_modele():
    modele = _modele()
    for cle, entite in scenarios.MIRROR_ENTITIES.items():
        assert modele.get(cle) == entite, f"{cle} : démo {entite}, modèle {modele.get(cle)}"


def test_cles_de_zones_de_la_tablette():
    with open(os.path.join(REPO, "Tab5", "tab5_zones.cpp"), encoding="utf-8") as f:
        m = re.search(r"kCles\[kNbZones\] = \{(.*?)\};", f.read(), re.S)
    kcles = re.findall(r'"([a-z0-9_]+)"', m.group(1))
    assert list(scenarios.ZONE_ENTITES) + list(scenarios.ZONES_HA) == kcles
    for cle in scenarios.ZONE_ENTITES.values():
        assert cle in scenarios.MIRROR_ENTITIES, f"zone sans entité simulée : {cle}"


def test_maison_minimale_ne_repond_pas_pour_ses_zones():
    for cle in scenarios.MAISON_MINIMALE - set(scenarios.ZONES_HA):
        entite = scenarios.MIRROR_ENTITIES[scenarios.ZONE_ENTITES[cle]]
        assert scenarios.mirror_state_for(entite) is not None
        assert scenarios.mirror_state_for(entite, scenarios.MAISON_MINIMALE) is None


def test_deux_modes_a_blanc():
    for absentes in (frozenset(), scenarios.MAISON_MINIMALE):
        sortie = io.StringIO()
        with contextlib.redirect_stdout(sortie):
            demo_pusher._dry_run(absentes)
        assert "OK" in sortie.getvalue()


def test_codes_du_lot_4c():
    for scene in scenarios.SCENES:
        assert re.fullmatch(r"@-?\d,\d+", scenarios.code_pluie(*scene.pluie)), scene.nom
        assert scene.info_texte[0].startswith("@ha|"), scene.nom
        assert scene.info_texte[0].count("|") == 6, scene.nom
        for jour in scene.jours:
            assert jour.heures_ouverture == "" or re.fullmatch(r"\d\d:\d\d-\d\d:\d\d", jour.heures_ouverture)
