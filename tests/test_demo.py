# -*- coding: utf-8 -*-
"""Mode démo (tools/demo/) : il se fait passer pour HA, sans relire le firmware. Ce qui
ne se voit qu'une fois flashé est vérifié ici :

- les emplacements poussés par la démo sont exactement ceux de la table de
  `tab5_maj_emplacements` (Tab5/tab5-api-logic.yaml, lot 6a) : une clé inconnue serait
  ignorée en silence par la tablette ;
- les clés de zones (lot 5) sont celles de la tablette (kCles, Tab5/tab5_zones.cpp) ;
- la « maison minimale » ne pousse rien pour ses zones retirées ;
- les deux modes, complet et « maison minimale », passent à blanc ;
- les codes du lot 4c (pluie « @niveau,début », bandeau « @ha|… ») et le format des
  horaires suivent le contrat ;
- les pièces (ADR-0023) ne partent qu'à un firmware qui a `tab5_maj_tuiles`, les
  définitions avant les états, et les commandes des tuiles sont journalisées par leur
  nom (grammaire des payloads : tests/test_demo_pieces.py)."""
import asyncio
import contextlib
import io
import logging
import os
import re
import sys
from types import SimpleNamespace

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "demo"))

import demo_pusher  # noqa: E402
import scenarios  # noqa: E402


def _lire(*parts):
    with open(os.path.join(REPO, *parts), encoding="utf-8") as f:
        return f.read()


def test_emplacements_de_la_demo_egaux_a_la_table_du_firmware():
    bloc = _lire("Tab5", "tab5-api-logic.yaml").split("- service: tab5_maj_emplacements", 1)[1]
    cles_firmware = re.findall(r'\{"(\w+)", ', bloc.split("emplacements_appliquer", 1)[0])
    assert sorted(scenarios.EMPLACEMENTS) == sorted(cles_firmware)


def test_cles_de_zones_de_la_tablette():
    m = re.search(r"kCles\[kNbZones\] = \{(.*?)\};", _lire("Tab5", "tab5_zones.cpp"), re.S)
    kcles = re.findall(r'"([a-z0-9_]+)"', m.group(1))
    assert list(scenarios.ZONES_SUIVIES) + list(scenarios.ZONES_HA) == kcles
    for cle in scenarios.EMPLACEMENTS:
        assert scenarios.zone_de(cle) in scenarios.ZONES_SUIVIES, cle


def test_maison_minimale_ne_pousse_pas_ses_zones():
    complet = scenarios.build_emplacements_payload()
    minimal = scenarios.build_emplacements_payload(scenarios.MAISON_MINIMALE)
    for cle in scenarios.EMPLACEMENTS:
        assert f"{cle}|" in complet
        if scenarios.zone_de(cle) in scenarios.MAISON_MINIMALE:
            assert not re.search(rf"(^|;){cle}\|", minimal), cle
        else:
            assert re.search(rf"(^|;){cle}\|", minimal), cle


def test_deux_modes_a_blanc():
    for absentes in (frozenset(), scenarios.MAISON_MINIMALE):
        sortie = io.StringIO()
        with contextlib.redirect_stdout(sortie):
            demo_pusher._dry_run(absentes)
        assert "OK" in sortie.getvalue()
        assert "tab5_maj_tuiles" in sortie.getvalue()


class _Tablette:
    """Client aioesphomeapi factice : note chaque action appelée."""

    def __init__(self):
        self.appels = []

    async def execute_service(self, service, data):
        self.appels.append((service.name, dict(data)))


def _pousser(monkeypatch, avec_tuiles, absentes=frozenset()):
    async def instant(*_args, **_kwargs):
        return None

    monkeypatch.setattr(demo_pusher.asyncio, "sleep", instant)
    noms = list(demo_pusher.SERVICES_ATTENDUS) + ([demo_pusher.SERVICE_TUILES] if avec_tuiles else [])
    services = {n: SimpleNamespace(name=n, args=[SimpleNamespace(name="payload")]
                                   if n in ("tab5_maj_emplacements", demo_pusher.SERVICE_TUILES) else [])
                for n in noms}
    tablette = _Tablette()
    asyncio.run(demo_pusher._pousser_scene(tablette, services, scenarios.SCENES[2], absentes))
    return tablette.appels


def test_firmware_3x_sans_pieces(monkeypatch):
    """Comme le blueprint : sans l'action, ni définitions ni clés tRT."""
    appels = _pousser(monkeypatch, avec_tuiles=False)
    assert demo_pusher.SERVICE_TUILES not in [nom for nom, _ in appels]
    (payload,) = [d["payload"] for nom, d in appels if nom == "tab5_maj_emplacements"]
    assert payload == scenarios.build_emplacements_payload()


def test_firmware_3_2_definitions_puis_etats(monkeypatch):
    for absentes in (frozenset(), scenarios.MAISON_MINIMALE):
        appels = _pousser(monkeypatch, avec_tuiles=True, absentes=absentes)
        noms = [nom for nom, _ in appels]
        assert noms.index(demo_pusher.SERVICE_TUILES) < noms.index("tab5_maj_emplacements")
        pieces = scenarios.pieces_de(absentes)
        donnees = dict(appels)
        assert donnees[demo_pusher.SERVICE_TUILES]["payload"] == scenarios.build_tuiles_payload(pieces)
        clim = None if "clim" in absentes else scenarios.SCENES[2].clim
        assert donnees["tab5_maj_emplacements"]["payload"] == scenarios.build_emplacements_payload(
            absentes, pieces, clim)
        assert re.search(r"(^|;)t00\|", donnees["tab5_maj_emplacements"]["payload"])


def test_commandes_des_tuiles_journalisees(caplog):
    gerer = demo_pusher._gerer_appel_service(True, lambda: None, scenarios.PIECES)
    with caplog.at_level(logging.INFO, logger="demo_pusher"):
        for cle, action in (("t43", "basculer"), ("p0", "eteindre"), ("lumiere_2", "basculer")):
            gerer(SimpleNamespace(is_event=True, service="esphome.tab5_action",
                                  data={"emplacement": cle, "action": action, "valeur": ""}))
    texte = caplog.text
    assert "t43 (Jardin › Guirlande lumineuse de la terrasse, lum) : basculer" in texte
    assert "p0 (Salon, toutes ses lumières) : eteindre" in texte
    assert "lumiere_2 : basculer" in texte


def test_codes_du_lot_4c():
    for scene in scenarios.SCENES:
        assert re.fullmatch(r"@-?\d,\d+", scenarios.code_pluie(*scene.pluie)), scene.nom
        assert scene.info_texte[0].startswith("@ha|"), scene.nom
        assert scene.info_texte[0].count("|") == 6, scene.nom
        for jour in scene.jours:
            assert jour.heures_ouverture == "" or re.fullmatch(r"\d\d:\d\d-\d\d:\d\d", jour.heures_ouverture)


def test_noms_de_jours_comme_ha():
    """Le jour seul (« Auj », « Mer »…), comme tab5_push.yaml : la tablette le traduit
    (ha_day_name). « Auj 16 » restait en français sur un écran anglais (lot 7)."""
    ha = open(os.path.join(REPO, "HomeAssistant_Config", "packages", "tab5_push.yaml"), encoding="utf-8").read()
    noms = set(re.search(r'days_names = \[([^\]]+)\]', ha).group(1).replace('"', "").replace(" ", "").split(","))
    noms.add("Auj")
    for scene in scenarios.SCENES:
        for jour in scene.jours:
            assert jour.nom_jour in noms, (scene.nom, jour.nom_jour)
