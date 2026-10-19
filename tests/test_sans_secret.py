# -*- coding: utf-8 -*-
"""Firmware sans secret (lot 6b, ADR-0020). Un même binaire doit pouvoir servir à tout
le monde : rien de personnel ne doit revenir dans le YAML du firmware.

- aucun `!secret` (Wi-Fi, clé API, mot de passe de l'AP) ;
- clé API fournie par HA (`encryption:` sans `key:`), fenêtre d'appairage bornée ;
- OTA sans chiffrement ni mot de passe, mais images signées (clé hors git) ;
- Wi-Fi sans identifiants, Improv par USB, AP de secours ouvert ;
- fuseau venu de HA (plus de `tab5_fuseau`), identité de projet déclarée ;
- les outils du PC trouvent la clé gardée par HA sans jamais l'afficher, et la redonnent
  à une tablette réinstallée avec effacement (tools/tab5_redonner_cle.py)."""
import json
import os
import re

import yaml
from tests.commun import BaseChargeur, lire as _lire

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

from tab5_cle_api import cle_depuis_config_ha, trouver_cle  # noqa: E402


class _Chargeur(BaseChargeur):
    """Lit les YAML ESPHome sans résoudre leurs balises (!include, !lambda, !secret…)."""


def _balise(chargeur, suffixe, noeud):
    if isinstance(noeud, yaml.MappingNode):
        return chargeur.construct_mapping(noeud)
    if isinstance(noeud, yaml.SequenceNode):
        return chargeur.construct_sequence(noeud)
    return {"!" + suffixe: chargeur.construct_scalar(noeud)}


_Chargeur.add_multi_constructor("!", _balise)


def _yaml(*chemin):
    return yaml.load(_lire(*chemin), Loader=_Chargeur)


def _fichiers_firmware():
    fichiers = [os.path.join(REPO, "tab5-ha-hmi.yaml")]
    for dossier in (os.path.join(REPO, "Tab5"), os.path.join(REPO, "Tab5", "paquets"),
                    os.path.join(REPO, "Tab5", "ui_components")):
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
    api = _yaml("Tab5", "paquets", "tab5-api-logic.yaml")
    assert api["api"]["encryption"] == {}, "une clé compilée rendrait le binaire personnel"
    assert re.fullmatch(r"\d+min", api["provisioning"]["timeout"])


def test_ota_signee_sans_chiffrement_ni_mot_de_passe():
    materiel = _yaml("Tab5", "paquets", "tab5-hardware.yaml")
    # Liste (lot 6c) : un firmware publié y ajoute `http_request` (publication-commune).
    otas = materiel["ota"] + _yaml("Tab5", "paquets", "publication-commune.yaml")["ota"]
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
    diag = _yaml("Tab5", "paquets", "tab5-sensors-diagnostics.yaml")
    wifi = diag["wifi"]
    assert not {"ssid", "password", "networks"} & wifi.keys()
    assert "password" not in wifi["ap"]
    assert "improv_serial" in diag
    assert "captive_portal" in _yaml("tab5-ha-hmi.yaml")


def test_fuseau_de_ha_garde_pour_le_demarrage():
    diag = _yaml("Tab5", "paquets", "tab5-sensors-diagnostics.yaml")
    horloges = {h["platform"]: h for h in diag["time"]}
    ha = horloges["homeassistant"]
    assert "timezone" not in ha, "un fuseau fixé empêcherait celui de HA"
    assert "fuseau_recu_de_ha" in str(ha["on_time_sync"])
    for plateforme in ("sntp", "rx8130"):
        assert "timezone" not in horloges[plateforme]
    texte = _lire("Tab5", "paquets", "tab5-sensors-diagnostics.yaml")
    assert "fuseau_restaurer();" in texte and "fuseau_memoriser();" in texte


def test_identite_de_projet():
    projet = _yaml("tab5-ha-hmi.yaml")["esphome"]["project"]
    assert projet["name"] == "axellum.tab5-ha-hmi"
    # Version du tag pour un firmware publié (lot 6c), « -dev » par défaut en local.
    defaut = re.fullmatch(r"\$\{ tab5_version \| default\('([^']+)'\) \}", projet["version"])
    assert defaut, projet["version"]
    assert re.fullmatch(r"\d+\.\d+\.\d+-dev", defaut.group(1))


def test_aucune_entite_a_regler_pour_un_binaire_publie():
    """Un binaire publié ne lit pas de user_entities.yaml (lot 6c). Depuis l'ADR-0025 il
    ne nomme plus aucune entité HA : HA retrouve celles de la tablette par l'appareil qui
    émet l'événement, et le garde-fou et l'automatisation de poussée du bouton « MAJ
    Écran » par leurs noms dans le package public tab5_push.yaml (celui-ci doit donc
    toujours les définir)."""
    assert not [k for k in (_yaml("Tab5", "paquets", "tab5-scripts.yaml").get("substitutions") or {})
                if k.startswith("entity_")]
    # Sans clé active depuis le lot 3 du portage 1024×600 (que des commentaires) : vide, accepté
    # par ESPHome (`esphome config` passe avec un user_entities.yaml sans clé).
    modele = _yaml("Tab5", "user_entities.example.yaml") or {}
    actives = sorted(k for k in modele if k.startswith("entity_"))
    assert actives == [], f"clé active inutile dans le modèle : {actives}"

    push = _yaml("HomeAssistant_Config", "packages", "tab5_push.yaml")
    assert [a for a in push["automation"] if a.get("id") == "tab5_ha_hmi_updater"]
    evenements = _lire("HomeAssistant_Config", "packages", "tab5_evenements.yaml")
    assert "selectattr('attributes.id', 'eq', 'tab5_ha_hmi_updater')" in evenements


def test_garde_fou_is_primary_active_retire():
    """Le garde-fou input_boolean.is_primary_active (vestige de la bascule entre deux
    instances HA, ADR-0008) est retiré depuis la 3.8 (audit du 07/10/2026) : resté à `off`,
    il figeait l'écran sans erreur. Aucun fichier HA ne doit le définir, le tester ni le
    rallumer ; seul un commentaire d'historique peut encore le nommer."""
    from pathlib import Path
    racine = Path(REPO) / "HomeAssistant_Config"
    fichiers = [f for motif in ("packages/*.yaml", "optionnel/*.yaml", "blueprints/**/*.yaml",
                                "custom_templates/*.jinja", "snippets/**/*.yaml")
                for f in sorted(racine.glob(motif))]
    assert fichiers
    fautifs = []
    for f in fichiers:
        for n, ligne in enumerate(f.read_text(encoding="utf-8").splitlines(), 1):
            code = ligne.split("#", 1)[0] if f.suffix == ".yaml" else ligne
            if "is_primary_active" in code or "force_primary_active_on_boot" in code:
                fautifs.append(f"{f.relative_to(racine)}:{n}")
    assert not fautifs, fautifs
    push = _yaml("HomeAssistant_Config", "packages", "tab5_push.yaml")
    assert "input_boolean" not in push
    assert not [a for a in push["automation"] if a.get("id") == "force_primary_active_on_boot"]


def test_ci_sans_secrets_factices():
    ci = _lire(".github", "workflows", "esphome-tab5.yml")
    assert "secrets.yaml" not in ci.replace("plus de secrets.yaml", "")
    assert "api_encryption_key" not in ci
    # build, build-min, build-revisions et build-carte-7b (ADR-0061) : une clé jetable chacun.
    assert ci.count("openssl genrsa -out tab5_signature.pem 3072") == 4


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


class _FausseTablette:
    """Une tablette pour tools/tab5_redonner_cle.py : sans clé (effacée) ou avec, fenêtre
    d'appairage ouverte ou fermée, comme l'API d'ESPHome 2026.9 (api_connection.cpp)."""

    def __init__(self, cle=None, fenetre=True):
        self.cle, self.fenetre, self.cles_recues = cle, fenetre, []

    def client(self):
        from aioesphomeapi import (ZERO_NOISE_PSK, APIConnectionError, ConnectionClosedEvent,
                                   DisconnectReason, InvalidEncryptionKeyAPIError)
        tablette = self

        class Client:
            def __init__(self, hote, port, mot_de_passe, noise_psk=None, client_info=""):
                self.psk, self.rappels = noise_psk, []

            def add_connection_closed_callback(self, rappel):
                self.rappels.append(rappel)

            async def connect(self, login=True):
                if tablette.cle is not None:
                    if self.psk != tablette.cle:
                        raise InvalidEncryptionKeyAPIError("Handshake MAC failure")
                    return
                if self.psk not in (None, ZERO_NOISE_PSK):
                    raise InvalidEncryptionKeyAPIError("Handshake MAC failure")
                if not tablette.fenetre:
                    for rappel in self.rappels:
                        rappel(ConnectionClosedEvent(reason=DisconnectReason.PROVISIONING_CLOSED))
                    raise APIConnectionError("Disconnect request")

            async def device_info(self):
                from types import SimpleNamespace
                return SimpleNamespace(api_encryption_provisionable=tablette.cle is None)

            async def noise_encryption_set_key(self, cle):
                tablette.cles_recues.append(cle)
                tablette.cle = cle.decode()
                return True

            async def disconnect(self, force=False):
                pass

        return Client


def _redonner(monkeypatch, tablette, cle):
    import asyncio

    import aioesphomeapi
    import tab5_redonner_cle as outil

    monkeypatch.setattr(aioesphomeapi, "APIClient", tablette.client())
    monkeypatch.setattr(outil, "PAUSE_NOUVELLE_CLE_S", 0)
    return asyncio.run(outil.redonner_cle("192.168.1.42", cle))


def test_cle_de_ha_redonnee_a_une_tablette_effacee(monkeypatch):
    """tools/tab5_redonner_cle.py : la clé de HA, par la clé nulle, seulement à une
    tablette sans clé et dans sa fenêtre ; rien de changé sinon."""
    cle = "QkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkI="
    effacee = _FausseTablette()
    assert _redonner(monkeypatch, effacee, cle) == "donnee"
    assert effacee.cles_recues == [cle.encode()]
    # Relancé : la tablette a déjà la clé de HA, rien n'est renvoyé.
    assert _redonner(monkeypatch, effacee, cle) == "deja" and len(effacee.cles_recues) == 1
    autre = _FausseTablette(cle="AUTRE")
    assert _redonner(monkeypatch, autre, cle) == "autre_cle" and autre.cle == "AUTRE"
    fermee = _FausseTablette(fenetre=False)
    assert _redonner(monkeypatch, fermee, cle) == "fenetre_fermee" and fermee.cle is None


def test_redonner_la_cle_ne_l_affiche_jamais(monkeypatch, capsys):
    import tab5_redonner_cle as outil

    cle = "QkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkJCQkI="

    async def faux(hote, c, port):
        return "donnee"

    monkeypatch.setattr(outil, "redonner_cle", faux)
    monkeypatch.setattr("sys.argv", ["tab5_redonner_cle.py", "--host", "192.168.1.42", "--cle", cle])
    assert outil.main() == 0
    sortie = capsys.readouterr()
    assert cle not in sortie.out + sortie.err and "Recharger" in sortie.out
    assert all(cle not in message for _, message in outil.MESSAGES.values())
