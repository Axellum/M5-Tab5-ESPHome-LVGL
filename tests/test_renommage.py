# -*- coding: utf-8 -*-
"""Noms anglais des entités de la tablette dans HA (custom_components/tab5/renommage.py).

Les identifiants uniques d'exemple ont la forme de ceux du registre de l'auteur, lus le
24/10/2026 (Tab5 ST7121, Home Assistant 2026.10) ; la MAC est une fausse.
"""
from __future__ import annotations

import importlib.util
import re
import sys

from tests.commun import REPO, ChargeurSansBalises, charger

INTEGRATION = REPO / "custom_components" / "tab5"


def _module(nom: str):
    spec = importlib.util.spec_from_file_location(f"tab5_{nom}", INTEGRATION / f"{nom}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


renommage = _module("renommage")
MAC = "AA:BB:CC:DD:EE:FF"


def test_nom_firmware_lu_dans_l_identifiant_unique():
    assert renommage.nom_firmware("select", f"{MAC}/0/select/Thème") == "Thème"
    assert renommage.nom_firmware("sensor", f"{MAC}/0/text_sensor/Prochain réveil") == "Prochain réveil"
    assert (renommage.nom_firmware("text", f"{MAC}/0/text_info/Réveil : URL de sonnerie (option)")
            == "Réveil : URL de sonnerie (option)")
    # même nom, autre type : pas la même entité ; hors table : rien
    assert renommage.nom_firmware("switch", f"{MAC}/0/select/Thème") is None
    assert renommage.nom_firmware("number", f"{MAC}/0/number/Volume") is None
    # versions 1 (object_id : « é » → « _ ») et 2 d'un HA plus ancien
    assert renommage.nom_firmware("switch", "aabbccddeeff-switch-r_veil") == "Réveil"
    assert renommage.nom_firmware("sensor", "aabbccddeeff-sensor-tab5_uptime") == "Tab5 Uptime"
    assert renommage.nom_firmware("sensor", "aabbccddeeff-text_sensor-Prochain réveil@3") == "Prochain réveil"


def test_entity_id():
    assert renommage.nouvel_entity_id("select.tab5_theme", "Thème", "Theme") is None  # même slug
    assert renommage.nouvel_entity_id("switch.tab5_reveil", "Réveil", "Alarm") == "switch.tab5_alarm"
    assert (renommage.nouvel_entity_id("sensor.tab5_tab5_uptime", "Tab5 Uptime", "Uptime")
            == "sensor.tab5_uptime")
    assert (renommage.nouvel_entity_id("media_player.tab5_tab5_media_player", "tab5_media_player",
                                       "Media Player") == "media_player.tab5_media_player")
    # « _2 » : sans lui d'abord, avec lui s'il est pris (un second écran) ; préfixe de la 7B
    # de l'auteur, lu dans son HA le 24/10/2026
    assert (renommage.nouveaux_entity_ids("sensor.m5stack_tab5_home_assistant_hmi_tab5_uptime_2",
                                          "Tab5 Uptime", "Uptime")
            == ("sensor.m5stack_tab5_home_assistant_hmi_uptime",
                "sensor.m5stack_tab5_home_assistant_hmi_uptime_2"))
    assert (renommage.nouvel_entity_id("time.tab5_ha_hmi_reveil_heure", "Réveil : heure",
                                       "Alarm: time") == "time.tab5_ha_hmi_alarm_time")
    # choisi par l'utilisateur : gardé
    assert renommage.nouvel_entity_id("switch.mon_reveil_chambre", "Réveil", "Alarm") is None
    # déjà migré : « tab5_uptime » est aussi le slug de « Tab5 Uptime » ; le préfixe de
    # l'appareil n'est jamais vide, donc rien (trouvé par le banc de HA le 24/10/2026)
    assert renommage.nouvel_entity_id("sensor.tab5_uptime", "Tab5 Uptime", "Uptime") is None


def test_plan():
    entrees = [
        ("switch.tab5_reveil", f"{MAC}/0/switch/Réveil", None),
        ("select.tab5_theme", f"{MAC}/0/select/Thème", None),  # même identifiant, nom à traduire
        ("sensor.tab5_tab5_uptime", f"{MAC}/0/sensor/Tab5 Uptime", "Démarrage"),  # nom de l'utilisateur
        ("number.tab5_volume", f"{MAC}/0/number/Volume", None),  # hors table
    ]
    plan = {r.entity_id: r for r in renommage.planifier(entrees)}
    assert plan["switch.tab5_reveil"] == renommage.Renommage("switch.tab5_reveil", ("switch.tab5_alarm",), "Alarm")
    assert plan["select.tab5_theme"] == renommage.Renommage("select.tab5_theme", (), "Theme")
    assert plan["sensor.tab5_tab5_uptime"] == renommage.Renommage("sensor.tab5_tab5_uptime",
                                                                  ("sensor.tab5_uptime",), None)
    assert "number.tab5_volume" not in plan
    # deux écrans du même nom : celui sans « _2 » passe d'abord et prend l'identifiant nu
    deux = renommage.planifier([("switch.tab5_reveil_2", f"{MAC}/0/switch/Réveil", None),
                                ("switch.tab5_reveil", "11:22:33:44:55:66/0/switch/Réveil", None)])
    assert [r.entity_id for r in deux] == ["switch.tab5_reveil", "switch.tab5_reveil_2"]
    # déjà migré : plus rien, la réparation disparaît
    faits = [("switch.tab5_alarm", f"{MAC}/0/switch/Réveil", "Alarm"),
             ("select.tab5_theme", f"{MAC}/0/select/Thème", "Theme")]
    assert renommage.planifier(faits) == []


def test_table_sans_collision():
    vus: dict[tuple[str, str], str] = {}
    for (domaine, ancien), neuf in renommage.ESPHOME.items():
        assert neuf != ancien and neuf.strip() == neuf
        assert domaine in renommage.TYPES
        cle = (domaine, renommage.slug(neuf))
        assert cle not in vus, f"{neuf} et {vus.get(cle)} donnent le même identifiant"
        vus[cle] = neuf
        assert not re.search(r"[àâçéèêëîïôûùüœ]", neuf), neuf


def _noms_firmware() -> set[str]:
    noms = set()
    for f in [REPO / "tab5-ha-hmi.yaml", *sorted((REPO / "Tab5" / "paquets").glob("*.yaml"))]:
        for m in re.finditer(r"^\s*(?:-\s+)?name:\s*[\"']?(.+?)[\"']?\s*$", f.read_text(encoding="utf-8"), re.M):
            noms.add(m.group(1))
    return noms


def test_noms_d_avant_sont_ceux_du_firmware():
    """Le firmware garde ses noms (clé ESPHome, réglages enregistrés) : la table les suit."""
    noms = _noms_firmware()
    for (_, ancien), neuf in renommage.ESPHOME.items():
        assert ancien in noms, f"« {ancien} » n'est plus un nom du firmware (ne jamais le renommer)"
        assert neuf not in noms, f"« {neuf} » : le firmware ne prend pas le nom anglais"


def test_fichiers_ha_acceptent_les_deux_identifiants():
    """Un motif qui trouve une entité du firmware par la fin de son identifiant accepte celui
    d'avant et l'anglais : la migration peut se faire avant ou après les fichiers."""
    textes = {p: p.read_text(encoding="utf-8") for p in (REPO / "HomeAssistant_Config").rglob("*")
              if p.suffix in {".yaml", ".jinja"}}
    for (domaine, ancien), neuf in renommage.ESPHOME.items():
        a, n = renommage.slug(ancien), renommage.slug(neuf)
        if a == n:
            continue
        for fin in {a, a.removeprefix("tab5_")} - {n}:
            seul = re.compile(r"(" + re.escape(domaine) + r"\[\.\]\.\*_" + re.escape(fin) + r"\(_\[0-9\]\+\)\?\$"
                              r"|'" + re.escape(domaine) + r"', '" + re.escape(fin) + r"'\))")
            for p, t in textes.items():
                assert not seul.search(t), f"{p.name} : « {fin} » sans « {n} »"


def _listes_des_packages() -> dict[str, list[str]]:
    """{input_select.x : options} des packages et des optionnels."""
    listes = {}
    for dossier in ("packages", "optionnel"):
        for f in sorted((REPO / "HomeAssistant_Config" / dossier).glob("*.yaml")):
            paquet = charger("HomeAssistant_Config", dossier, f.name, chargeur=ChargeurSansBalises) or {}
            for cle, conf in (paquet.get("input_select") or {}).items():
                listes[f"input_select.{cle}"] = list((conf or {}).get("options") or [])
    return listes


def test_options_des_packages_sont_celles_de_la_table():
    """Chaque liste de OPTIONS_PAQUETS existe dans les packages et y propose les options
    anglaises de la table, jamais celles d'avant : l'intégration ne traduit l'option choisie
    que vers une option qui existe (Gestionnaire._options_gardees). Et une liste des packages
    qui propose encore « Oui », « Aucune »… a sa ligne dans la table."""
    listes = _listes_des_packages()
    for eid, table in renommage.OPTIONS_PAQUETS.items():
        assert eid in listes, f"{eid} : absente des packages"
        options = listes[eid]
        for avant, apres in table.items():
            assert apres in options, f"{eid} : « {apres} » absente ({options})"
            assert avant not in options, f"{eid} : « {avant} » encore proposée"
        assert len(set(options)) == len(options), eid
    francais = {"Oui", "Non", "Aucun", "Aucune", "Toutes", "Jaune", "Rouge", "Home Assistant seulement"}
    for eid, options in listes.items():
        assert not francais & set(options), f"{eid} : {sorted(francais & set(options))} sans traduction"
