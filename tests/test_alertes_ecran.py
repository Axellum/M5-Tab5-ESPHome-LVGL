# -*- coding: utf-8 -*-
"""Alertes de la carte centrale côté écran (lot 3 du plan des alertes du 06/10/2026).

Le payload des bandeaux (`tab5_maj_alertes_ha_bulk`) est rendu ici depuis le vrai
modèle de `packages/tab5_push.yaml` : les quatre premières alertes, dans l'ordre déjà
rangé par la macro, et un en-tête « @n:total » quand il y en a plus. Côté firmware, la
lecture de l'en-tête est rejouée par le fuzz des sanitizers (graine du service) et le
compteur se voit sur l'écran « accueil-alertes-ha-compteur » du rendu hors tablette.
"""
from __future__ import annotations

import re
from pathlib import Path

import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

RACINE = Path(__file__).resolve().parents[1]
PUSH = RACINE / "HomeAssistant_Config" / "packages" / "tab5_push.yaml"
SERVICE = "esphome.tab5_ha_hmi_tab5_maj_alertes_ha_bulk"


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda *_: None)


def _etapes(sequence):
    for etape in sequence or []:
        yield etape
        for cle in ("then", "else", "sequence", "default"):
            if isinstance(etape.get(cle), list):
                yield from _etapes(etape[cle])
        for branche in etape.get("choose", []) or []:
            yield from _etapes(branche.get("sequence"))


def _modele_payload() -> str:
    paquet = yaml.load(PUSH.read_text(encoding="utf-8"), Loader=_Chargeur)
    appels = [e for e in _etapes(paquet["script"]["tab5_push_alertes"]["sequence"])
              if e.get("action") == SERVICE]
    assert len(appels) == 1
    return appels[0]["data"]["payload"]


def payload(alertes) -> str:
    env = ImmutableSandboxedEnvironment()
    return env.from_string(_modele_payload()).render(alertes=alertes).strip()


def alerte(i, gravite="Orange", source="maj"):
    return {"i": f"{i}#1", "g": gravite, "t": f"@maj:{i}", "s": source}


def test_quatre_alertes_ou_moins_sans_en_tete():
    """Le payload d'avant, à l'octet : rien ne change pour un firmware d'avant."""
    a = [alerte("update.core", "Rouge"), alerte("ha:indispo", "Orange", "indispo")]
    assert payload(a) == "update.core#1|Rouge|@maj:update.core;ha:indispo#1|Orange|@maj:ha:indispo"
    assert payload([]) == ""


def test_plus_de_quatre_un_en_tete_et_les_quatre_premieres():
    a = [alerte(f"update.u{k}", "Rouge" if k < 2 else "Jaune") for k in range(6)]
    jetons = payload(a).split(";")
    assert jetons[0] == "@n:6"
    assert [j.split("|")[0] for j in jetons[1:]] == [f"update.u{k}#1" for k in range(4)]
    # Le jaune s'affiche en orange sur la tablette (deux couleurs de bandeau).
    assert [j.split("|")[1] for j in jetons[1:]] == ["Rouge", "Rouge", "Orange", "Orange"]


def test_la_vigilance_ne_compte_pas():
    """Elle a son propre bandeau (info), pas un des quatre."""
    a = [alerte("meteo:vigilance", "Rouge", "vigilance")] + [alerte(f"u{k}") for k in range(4)]
    assert not payload(a).startswith("@n:")
    a.append(alerte("u4"))
    jetons = payload(a).split(";")
    assert jetons[0] == "@n:5" and "meteo:vigilance" not in payload(a)


def test_l_en_tete_reste_ignore_par_un_firmware_d_avant():
    """Le firmware d'avant le lot 3 saute un jeton de moins de trois champs (split_fields
    sur « | ») : l'en-tête ne doit contenir aucun « | »."""
    a = [alerte(f"u{k}") for k in range(7)]
    en_tete = payload(a).split(";")[0]
    assert re.fullmatch(r"@n:\d+", en_tete)


def test_le_firmware_lit_l_en_tete_et_affiche_le_rang():
    """Garde-fou de lecture du C++ : l'en-tête est reconnu avant le découpage en champs,
    et le compteur n'apparaît que si des alertes attendent derrière les bandeaux."""
    cpp = (RACINE / "Tab5" / "tab5_central.cpp").read_text(encoding="utf-8")
    corps = cpp[cpp.index("bool parse_and_update_ha_alerts_bulk("):]
    corps = corps[:corps.index("\n}\n")]
    assert corps.index('strncmp(token, "@n:", 3)') < corps.index("split_fields(token")
    assert "if (total > slot_idx)" in corps
    yaml_panneau = (RACINE / "Tab5" / "ui_components" / "ha_alert_panel.yaml").read_text(encoding="utf-8")
    assert 'id: "lbl_ha_alert_cpt_${n}"' in yaml_panneau
