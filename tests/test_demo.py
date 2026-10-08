# -*- coding: utf-8 -*-
"""Mode démo (tools/demo/) : il se fait passer pour HA, sans relire le firmware. Ce qui
ne se voit qu'une fois flashé est vérifié ici :

- les emplacements poussés par la démo sont exactement ceux de la table de
  `tab5_maj_emplacements` (Tab5/paquets/tab5-api-logic.yaml, lot 6a) : une clé inconnue serait
  ignorée en silence par la tablette ;
- les clés de zones (lot 5) sont celles de la tablette (kCles, Tab5/ecran/tab5_zones.cpp) ;
- la « maison minimale » ne pousse rien pour ses zones retirées ;
- les deux modes, complet et « maison minimale », passent à blanc ;
- les codes du lot 4c (pluie « @niveau,début », bandeau « @ha|… ») et le format des
  horaires suivent le contrat ;
- les pièces (ADR-0023) ne partent qu'à un firmware qui a `tab5_maj_tuiles`, les
  définitions avant les états, et les commandes des tuiles sont journalisées par leur
  nom (grammaire des payloads : tests/test_demo_pieces.py) ;
- chaque appel a exactement les variables déclarées par le firmware : les fausses
  actions portent les vraies variables de Tab5/paquets/tab5-api-logic.yaml, donc la garde de
  `_appeler` joue enfin ici (audit du 30/09/2026 : sans variables, elle ne se
  déclenchait jamais), et --dry-run échoue sur une variable renommée."""
import asyncio
import contextlib
import io
import logging
import os
import re
from types import SimpleNamespace
from tests.commun import lire as _lire

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

import demo_pusher  # noqa: E402
import scenarios  # noqa: E402


def test_emplacements_de_la_demo_egaux_a_la_table_du_firmware():
    bloc = _lire("Tab5", "paquets", "tab5-api-logic.yaml").split("- service: tab5_maj_emplacements", 1)[1]
    cles_firmware = re.findall(r'\{"(\w+)", ', bloc.split("emplacements_appliquer", 1)[0])
    assert sorted(scenarios.EMPLACEMENTS) == sorted(cles_firmware)


def test_cles_de_zones_de_la_tablette():
    m = re.search(r"kCles\[kNbZones\] = \{(.*?)\};", _lire("Tab5", "ecran", "tab5_zones.cpp"), re.S)
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


def _pousser(monkeypatch, avec_tuiles, absentes=frozenset(), scene=None):
    """Une scène poussée à une tablette dont les actions ont les VRAIES variables du
    firmware (Tab5/paquets/tab5-api-logic.yaml) : la garde de contrat de `_appeler` joue."""
    async def instant(*_args, **_kwargs):
        return None

    monkeypatch.setattr(demo_pusher.asyncio, "sleep", instant)
    noms = list(demo_pusher.SERVICES_ATTENDUS) + ([demo_pusher.SERVICE_TUILES] if avec_tuiles else [])
    contrat = demo_pusher.lire_contrat()
    services = demo_pusher.services_du_contrat({n: contrat[n] for n in noms})
    tablette = _Tablette()
    asyncio.run(demo_pusher._pousser_scene(tablette, services, scene or scenarios.SCENES[2], absentes))
    return tablette.appels


def test_chaque_appel_a_les_variables_du_firmware(monkeypatch, caplog):
    """Toutes les scènes, les deux maisons, les deux firmwares : aucun appel écarté par la
    garde, et les clés de chaque appel sont exactement les variables de l'action."""
    contrat = demo_pusher.lire_contrat()
    appeles = set()
    with caplog.at_level(logging.WARNING, logger="demo_pusher"):
        for scene in scenarios.SCENES:
            for absentes in (frozenset(), scenarios.MAISON_MINIMALE):
                for avec_tuiles in (True, False):
                    for nom, donnees in _pousser(monkeypatch, avec_tuiles, absentes, scene):
                        assert sorted(donnees) == sorted(contrat[nom]), nom
                        appeles.add(nom)
    assert not caplog.records, [r.getMessage() for r in caplog.records]
    # Les actions de la démo : SERVICES_ATTENDUS (zones compris, poussé à part) et les pièces.
    assert appeles | {"tab5_maj_zones"} == set(demo_pusher.SERVICES_ATTENDUS) | {demo_pusher.SERVICE_TUILES}


def test_la_garde_refuse_un_argument_manquant_ou_en_trop(caplog):
    """La garde de `_appeler` : un appel qui n'a pas exactement les variables déclarées
    n'est pas envoyé (Home Assistant le refuserait aussi), et le journal le dit."""
    services = demo_pusher.services_du_contrat({"tab5_maj_info_texte": ("texte", "couleur", "meteo_id")})
    tablette = _Tablette()
    with caplog.at_level(logging.ERROR, logger="demo_pusher"):
        asyncio.run(demo_pusher._appeler(tablette, services, "tab5_maj_info_texte", texte="a", couleur="b"))
        asyncio.run(demo_pusher._appeler(tablette, services, "tab5_maj_info_texte",
                                         texte="a", couleur="b", meteo_id="", source="x"))
        asyncio.run(demo_pusher._appeler(tablette, services, "tab5_maj_info_texte",
                                         texte="a", couleur="b", meteo_id=""))
    assert tablette.appels == [("tab5_maj_info_texte", {"texte": "a", "couleur": "b", "meteo_id": ""})]
    messages = [r.getMessage() for r in caplog.records]
    assert len(messages) == 2
    assert "manquant(s) ['meteo_id']" in messages[0] and "en trop ['source']" in messages[1]


def test_dry_run_echoue_sur_un_contrat_change(monkeypatch, tmp_path):
    """--dry-run lit le contrat du firmware : une variable renommée dans
    Tab5/paquets/tab5-api-logic.yaml le fait échouer (étape de la CI)."""
    texte = _lire("Tab5", "paquets", "tab5-api-logic.yaml")
    assert texte.count("        meteo_id:\n") == 1
    faux = tmp_path / "tab5-api-logic.yaml"
    faux.write_text(texte.replace("        meteo_id:\n", "        meteo_ref:\n"), encoding="utf-8")
    monkeypatch.setattr(demo_pusher, "API_LOGIC", faux)
    sortie = io.StringIO()
    with contextlib.redirect_stdout(sortie):
        try:
            demo_pusher._dry_run(frozenset())
        except SystemExit as fin:
            assert "écart(s)" in str(fin)
        else:
            raise AssertionError("--dry-run aurait dû échouer")
    assert "ÉCART : tab5_maj_info_texte : argument(s) manquant(s) ['meteo_ref'] ; argument(s) en trop ['meteo_id']" \
        in sortie.getvalue()


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
        pieces, rangee = scenarios.pieces_de(absentes), scenarios.rangee_de(absentes)
        reglables = scenarios.reglables_de(absentes)
        donnees = dict(appels)
        assert donnees[demo_pusher.SERVICE_TUILES]["payload"] == scenarios.build_tuiles_payload(
            pieces, rangee, reglables)
        clim = None if "clim" in absentes else scenarios.SCENES[2].clim
        assert donnees["tab5_maj_emplacements"]["payload"] == scenarios.build_emplacements_payload(
            absentes, pieces, clim, rangee, reglables)
        assert re.search(r"(^|;)t00\|", donnees["tab5_maj_emplacements"]["payload"])
        # Rangée sous l'horloge (ADR-0031) : ses réglages toujours, ses éléments s'il y en a.
        assert "hp|0;hd|32;" in donnees[demo_pusher.SERVICE_TUILES]["payload"]
        assert bool(re.search(r"(^|;)h00\|", donnees["tab5_maj_emplacements"]["payload"])) == (not absentes)
        # Tuile − / + (ADR-0033) : ses appareils dans la maison complète seulement.
        for service in (demo_pusher.SERVICE_TUILES, "tab5_maj_emplacements"):
            assert bool(re.search(r"(^|;)r0\|", donnees[service]["payload"])) == (not absentes)


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
