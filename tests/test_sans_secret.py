# -*- coding: utf-8 -*-
"""Firmware sans secret (lot 6b, ADR-0020). Un même binaire doit pouvoir servir à tout
le monde : rien de personnel ne doit revenir dans le YAML du firmware.

- aucun `!secret` (Wi-Fi, clé API, mot de passe de l'AP) ;
- clé API fournie par HA (`encryption:` sans `key:`), fenêtre d'appairage bornée ;
- OTA sans chiffrement ni mot de passe, mais images signées (clé hors git) ;
- Wi-Fi sans identifiants, Improv par USB, AP de secours ouvert ;
- fuseau venu de HA (plus de `tab5_fuseau`), identité de projet déclarée ;
- les outils du PC trouvent la clé gardée par HA sans jamais l'afficher."""
import json
import os
import re
import sys

import yaml

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))

from tab5_cle_api import cle_depuis_config_ha, trouver_cle  # noqa: E402


class _Chargeur(yaml.SafeLoader):
    """Lit les YAML ESPHome sans résoudre leurs balises (!include, !lambda, !secret…)."""


def _balise(chargeur, suffixe, noeud):
    if isinstance(noeud, yaml.MappingNode):
        return chargeur.construct_mapping(noeud)
    if isinstance(noeud, yaml.SequenceNode):
        return chargeur.construct_sequence(noeud)
    return {"!" + suffixe: chargeur.construct_scalar(noeud)}


_Chargeur.add_multi_constructor("!", _balise)


def _lire(*chemin):
    with open(os.path.join(REPO, *chemin), encoding="utf-8") as f:
        return f.read()


def _yaml(*chemin):
    return yaml.load(_lire(*chemin), Loader=_Chargeur)


def _fichiers_firmware():
    fichiers = [os.path.join(REPO, "tab5-ha-hmi.yaml")]
    for dossier in (os.path.join(REPO, "Tab5"), os.path.join(REPO, "Tab5", "ui_components")):
        fichiers += [os.path.join(dossier, n) for n in sorted(os.listdir(dossier))
                     if n.endswith(".yaml") and n != "user_entities.yaml"]
    return fichiers


def test_aucun_secret_dans_le_firmware():
    fautifs = []
    for chemin in _fichiers_firmware():
        with open(chemin, encoding="utf-8") as f:
            for num, ligne in enumerate(f, 1):
                code = ligne.split("#", 1)[0]
                if "!secret" in code or "tab5_fuseau" in code:
                    fautifs.append(f"{os.path.relpath(chemin, REPO)}:{num}")
    assert not fautifs, "secret ou réglage retiré encore lu par le firmware : " + ", ".join(fautifs)


def test_cle_api_fournie_par_ha_et_fenetre_bornee():
    api = _yaml("Tab5", "tab5-api-logic.yaml")
    assert api["api"]["encryption"] == {}, "une clé compilée rendrait le binaire personnel"
    assert re.fullmatch(r"\d+min", api["provisioning"]["timeout"])


def test_ota_signee_sans_chiffrement_ni_mot_de_passe():
    materiel = _yaml("Tab5", "tab5-hardware.yaml")
    # Liste (lot 6c) : un firmware publié y ajoute `http_request` (publication-commune).
    otas = materiel["ota"] + _yaml("Tab5", "publication-commune.yaml")["ota"]
    assert [o["platform"] for o in otas] == ["esphome", "http_request"]
    for ota in otas:
        assert "encryption" not in ota and "password" not in ota
    signe = materiel["esp32"]["framework"]["advanced"]["signed_ota_verification"]
    assert signe["signing_scheme"] == "rsa3072"
    # Chemin réglable, défaut à la racine, là où .gitignore l'exclut.
    assert "tab5_cle_signature" in signe["signing_key"]
    assert "tab5_signature.pem" in signe["signing_key"]
    assert "*.pem" in _lire(".gitignore").splitlines()


def test_wifi_sans_identifiants_improv_et_ap_ouvert():
    diag = _yaml("Tab5", "tab5-sensors-diagnostics.yaml")
    wifi = diag["wifi"]
    assert not {"ssid", "password", "networks"} & wifi.keys()
    assert "password" not in wifi["ap"]
    assert "improv_serial" in diag
    assert "captive_portal" in _yaml("tab5-ha-hmi.yaml")


def test_fuseau_de_ha_garde_pour_le_demarrage():
    diag = _yaml("Tab5", "tab5-sensors-diagnostics.yaml")
    horloges = {h["platform"]: h for h in diag["time"]}
    ha = horloges["homeassistant"]
    assert "timezone" not in ha, "un fuseau fixé empêcherait celui de HA"
    assert "fuseau_recu_de_ha" in str(ha["on_time_sync"])
    for plateforme in ("sntp", "rx8130"):
        assert "timezone" not in horloges[plateforme]
    texte = _lire("Tab5", "tab5-sensors-diagnostics.yaml")
    assert "fuseau_restaurer();" in texte and "fuseau_memoriser();" in texte


def test_identite_de_projet():
    projet = _yaml("tab5-ha-hmi.yaml")["esphome"]["project"]
    assert projet["name"] == "axellum.tab5-ha-hmi"
    # Version du tag pour un firmware publié (lot 6c), « -dev » par défaut en local.
    defaut = re.fullmatch(r"\$\{ tab5_version \| default\('([^']+)'\) \}", projet["version"])
    assert defaut, projet["version"]
    assert re.fullmatch(r"\d+\.\d+\.\d+-dev", defaut.group(1))


def _slug(texte):
    """Identifiant d'entité que HA tire d'un nom (minuscules, `_` pour le reste)."""
    return re.sub(r"[^a-z0-9]+", "_", texte.lower()).strip("_")


def test_entites_par_defaut_generiques():
    """Un binaire publié ne lit pas de user_entities.yaml : les entités HA qu'il appelle
    doivent avoir des défauts qui existent chez tout le monde (lot 6c). Celles de la
    tablette dérivent du nom livré de l'appareil, les autres viennent du package
    public tab5_push.yaml ; le modèle ne garde aucune valeur active à remplacer."""
    defauts = _yaml("Tab5", "tab5-scripts.yaml")["substitutions"]
    appareil = _slug(_yaml("tab5-ha-hmi.yaml")["esphome"]["friendly_name"])
    for cle in ("entity_tab5_satellite", "entity_tab5_media_player", "entity_tab5_pipeline_select"):
        assert defauts[cle].split(".", 1)[1].startswith(appareil + "_"), cle

    push = _yaml("HomeAssistant_Config", "packages", "tab5_push.yaml")
    assert defauts["entity_primary_active"].split(".", 1) == ["input_boolean", "is_primary_active"]
    assert "is_primary_active" in push["input_boolean"]
    alias = {_slug(a["alias"]) for a in push["automation"] if a.get("id") == "tab5_ha_hmi_updater"}
    assert defauts["entity_push_automation"] == "automation." + alias.pop()

    modele = _yaml("Tab5", "user_entities.example.yaml")
    actives = sorted(k for k in modele if k.startswith("entity_"))
    assert actives == [], f"à commenter (défauts dans tab5-scripts.yaml) : {actives}"


def test_ci_sans_secrets_factices():
    ci = _lire(".github", "workflows", "esphome-tab5.yml")
    assert "secrets.yaml" not in ci.replace("plus de secrets.yaml", "")
    assert "api_encryption_key" not in ci
    assert ci.count("openssl genrsa -out tab5_signature.pem 3072") == 3


def _config_ha(tmp_path, entrees):
    stockage = tmp_path / ".storage"
    stockage.mkdir()
    (stockage / "core.config_entries").write_text(
        json.dumps({"version": 1, "data": {"entries": entrees}}), encoding="utf-8")
    return tmp_path


def test_cle_trouvee_dans_ha_par_ip_ou_par_nom(tmp_path, monkeypatch):
    monkeypatch.delenv("TAB5_CLE_API", raising=False)
    monkeypatch.delenv("TAB5_CONFIG_HA", raising=False)
    cle = "QkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkI="
    config = _config_ha(tmp_path, [
        {"domain": "hue", "data": {"host": "192.168.1.42"}},
        {"domain": "esphome", "data": {"host": "192.168.1.7", "device_name": "autre",
                                       "noise_psk": "AUTRE"}},
        {"domain": "esphome", "data": {"host": "192.168.1.42", "device_name": "tab5-ha-hmi",
                                       "noise_psk": cle}},
    ])
    assert cle_depuis_config_ha(config, "192.168.1.42") == cle
    assert cle_depuis_config_ha(config, "tab5-ha-hmi.local") == cle
    assert cle_depuis_config_ha(config, "192.168.1.99") is None
    # Ordre : option, puis variable d'environnement, puis HA.
    assert trouver_cle(None, "192.168.1.42", str(config)) == cle
    monkeypatch.setenv("TAB5_CLE_API", "ENV")
    assert trouver_cle(None, "192.168.1.42", str(config)) == "ENV"
    assert trouver_cle("OPTION", "192.168.1.42", str(config)) == "OPTION"


def test_sans_cle_ni_ha_rien(monkeypatch):
    monkeypatch.delenv("TAB5_CLE_API", raising=False)
    monkeypatch.delenv("TAB5_CONFIG_HA", raising=False)
    assert trouver_cle(None, "192.168.1.42", None) is None


def test_migration_lit_l_ancienne_cle(tmp_path):
    """tools/migrer_vers_3.py : la clé d'un secrets.yaml 2.x, guillemets et
    commentaire compris ; rien si la ligne manque."""
    from migrer_vers_3 import lire_ancienne_cle

    secrets = tmp_path / "secrets.yaml"
    secrets.write_text('wifi_ssid: "Maison"\n'
                       'api_encryption_key: "QUJD+/w=" # clé de la 2.x\n', encoding="utf-8")
    assert lire_ancienne_cle(secrets) == "QUJD+/w="
    secrets.write_text("wifi_ssid: Maison\n", encoding="utf-8")
    assert lire_ancienne_cle(secrets) is None
