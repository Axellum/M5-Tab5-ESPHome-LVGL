# -*- coding: utf-8 -*-
"""Test de consommation sur batterie (08/10/2026, discussion #278).

- firmware : l'interrupteur « Tab5 Mesure de consommation » (coupé au démarrage) fait lire
  l'INA226 toutes les 2 s, et se coupe seul au bout de 2 h ;
- Home Assistant : le script de packages/tab5_mesure_conso.yaml retrouve chaque entité de la
  tablette par la fin de son entity_id ; chaque fin cherchée doit être celle d'une entité
  publiée par le firmware (un nom changé dans le firmware ferait taire le test sans bruit),
  et les options qu'il choisit doivent exister dans les selects."""
import re

import yaml
from tests.commun import REPO, source
from tests.test_tableau_de_bord import _Chargeur, _entites_du_firmware

PACKAGE = REPO / "HomeAssistant_Config" / "packages" / "tab5_mesure_conso.yaml"
DIAG = "tab5-sensors-diagnostics.yaml"


def _yaml(chemin):
    return yaml.load(chemin.read_text(encoding="utf-8"), Loader=_Chargeur)


def _script():
    return yaml.safe_load(PACKAGE.read_text(encoding="utf-8"))["script"]["tab5_mesure_consommation"]


def _options(nom):
    for f in sorted((REPO / "Tab5").rglob("*.yaml")):
        if {"rendu", "lang"} & set(f.relative_to(REPO / "Tab5").parts[:-1]) or f.name.startswith("user_entities"):
            continue
        contenu = _yaml(f)
        for s in (contenu or {}).get("select", []) if isinstance(contenu, dict) else []:
            if isinstance(s, dict) and s.get("name") == nom:
                return s["options"]
    raise AssertionError(f"select « {nom} » introuvable dans le firmware")


def test_interrupteur_de_mesure_fine():
    diag = _yaml(source(DIAG))
    sw = next(s for s in diag["switch"] if s.get("id") == "tab5_mesure_conso")
    assert sw["name"] == "Tab5 Mesure de consommation"
    assert sw["restore_mode"] == "ALWAYS_OFF", "coupé à chaque démarrage"
    assert sw["on_turn_on"] == [{"script.execute": "tab5_mesure_conso_fin"}]
    assert sw["on_turn_off"] == [{"script.stop": "tab5_mesure_conso_fin"}]
    fin = next(s for s in diag["script"] if s["id"] == "tab5_mesure_conso_fin")
    assert fin["mode"] == "restart" and fin["then"] == [{"delay": "2h"}, {"switch.turn_off": "tab5_mesure_conso"}]
    lecture = [i for i in diag["interval"] if "tab5_mesure_conso" in yaml.safe_dump(i)]
    assert len(lecture) == 1 and lecture[0]["interval"] == "2s"
    assert "component.update: ina226_batterie" in yaml.safe_dump(lecture[0])


def test_le_script_retrouve_des_entites_du_firmware():
    texte = PACKAGE.read_text(encoding="utf-8")
    cherchees = re.findall(r"select\('match', '(\w+)\[\.\]\.\*_(\w+)\$'\)", texte)
    assert len(cherchees) >= 13, cherchees
    firmware = _entites_du_firmware()
    absentes = [(d, f) for d, f in cherchees
                if not any(dd == d and (s == f or s.endswith("_" + f)) for dd, s in firmware)]
    assert not absentes, absentes


def test_les_options_choisies_existent():
    cas = next(s for s in _script()["sequence"] if "variables" in s and "cas" in s["variables"])["variables"]["cas"]
    noms = [c["nom"] for c in cas]
    assert len(noms) == len(set(noms)) and noms[0] == "reference", "la référence d'abord, sans doublon"
    assert noms[-1] == "reference_end"
    assert {c["eco"] for c in cas} <= set(_options("Tab5 Économie d'énergie"))
    assert {c["theme"] for c in cas} - {""} <= set(_options("Thème"))
    assert {c["mode"] for c in cas} - {""} <= set(_options("Clair ou sombre"))
    assert "Jamais" in _options("Tab5 Extinction auto de l'écran")
    assert all(0 <= c["ecran"] <= 100 for c in cas)


def test_reglages_remis_et_resultat_en_anglais():
    texte = PACKAGE.read_text(encoding="utf-8")
    for e in ("e_ecran", "e_micro", "e_hp", "e_theme", "e_mode", "e_eco", "e_extinction"):
        assert texte.count(f'entity_id: "{{{{ {e} }}}}"') >= 2, f"{e} : réglé pendant le test et remis à la fin"
    assert "avant.extinction" in texte and "avant.luminosite" in texte
    tableau = next(s for s in _script()["sequence"] if "variables" in s and "tableau" in s["variables"])["variables"]["tableau"]
    assert "| Case | Average W |" in tableau
