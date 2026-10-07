# -*- coding: utf-8 -*-
"""Alertes de la carte centrale côté écran (lot 3 du plan des alertes du 06/10/2026).

Le payload des bandeaux (`tab5_maj_alertes_ha_bulk`) est rendu ici depuis le vrai
modèle de `packages/tab5_push.yaml` : les quatre premières alertes, dans l'ordre déjà
rangé par la macro, et un en-tête « @n:total » quand il y en a plus. Côté firmware, la
lecture de l'en-tête est rejouée par le fuzz des sanitizers (graine du service) et le
compteur se voit sur l'écran « accueil-alertes-ha-compteur » du rendu hors tablette.
"""
from __future__ import annotations

import base64
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
    # Filtre de HA (homeassistant/helpers/template/extensions/base64.py, 2026.9.4) : le
    # texte encodé en UTF-8, puis en base64.
    env.filters["base64_encode"] = lambda v: base64.b64encode(
        v.encode("utf-8") if isinstance(v, str) else v).decode("utf-8")
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


# ─── Taille bornée (audit du 07/10/2026, DO-6) ───────────────────────────────
# La tablette refuse tout payload de plus de 1024 octets : les quatre bandeaux restaient
# alors sur les anciennes alertes.

def _octets(texte):
    return len(texte.encode("utf-8"))


def _limite_du_firmware():
    cpp = (RACINE / "Tab5" / "tab5_central.cpp").read_text(encoding="utf-8")
    corps = cpp[cpp.index("bool parse_and_update_ha_alerts_bulk("):]
    m = re.search(r"if \(payload\.length\(\) > (\d+)\)", corps)
    assert m, "garde de taille introuvable dans parse_and_update_ha_alerts_bulk"
    return int(m.group(1))


def test_libelle_coupe_a_100_caracteres_sans_separateur():
    long = "Capteur « température » | de la serre; " * 10
    a = [{"i": "sensor.serre#1", "g": "Rouge", "t": long, "s": "probleme"}]
    texte = payload(a).split("|", 2)[2]
    # HA retire les espaces de fin du rendu (le 100e caractère en est un ici).
    assert texte == long.replace("|", "/").replace(";", ",")[:100].rstrip()
    assert ";" not in payload(a) and payload(a).count("|") == 2


def test_pire_cas_sous_la_limite_du_firmware():
    """Quatre alertes de plus que les bandeaux (en-tête « @n: » à trois chiffres), ids de
    64 caractères avec révision, libellés de 100 caractères accentués : tout passe, sous
    la limite que lit le firmware."""
    limite = _limite_du_firmware()
    assert limite == 1024
    libelle = "Mise à jour disponible : « Système d'exploitation » — é è à ç ô û ï ë ü ÿ œ æ " * 3
    a = [{"i": "update." + "x" * 57 + str(k) + "#123", "g": "Rouge", "t": "@maj:" + libelle, "s": "maj"}
         for k in range(4)] + [{"i": f"u{k}#1", "g": "Orange", "t": "x", "s": "maj"} for k in range(200)]
    p = payload(a)
    jetons = p.split(";")
    assert jetons[0] == "@n:204" and len(jetons) == 5
    assert _octets(p) <= limite, _octets(p)


def test_cas_extreme_jamais_refuse_en_bloc():
    """Libellés tout en emoji (4 octets chacun) et ids de 200 caractères : des alertes
    restent dehors, mais le payload passe toujours (jamais plus de 1024 octets)."""
    a = [{"i": "sensor." + "y" * 193 + f"{k}#9", "g": "Rouge", "t": "🔥" * 300, "s": "probleme"}
         for k in range(6)]
    p = payload(a)
    assert _octets(p) <= 1024
    jetons = p.split(";")
    assert jetons[0] == "@n:6" and 1 <= len(jetons) - 1 < 4
    # Le comptage des octets en Jinja (base64) suit bien l'UTF-8 au caractère près.
    assert all(j.split("|")[2] == "🔥" * 100 for j in jetons[1:])

