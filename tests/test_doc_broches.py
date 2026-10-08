# -*- coding: utf-8 -*-
"""Le tableau des broches de docs/hardware.md, comparé au YAML du firmware.

Il remplace docs/images/gpio_pinout_table.png, une image générée par IA en juillet
2026 qui donnait un écran RGB parallèle 1024×600, 16 Mo de PSRAM et le même GPIO 26
pour BCLK et DOUT ; le tableau audio de hardware.md avait repris « BCLK = GPIO 26 »
(c'est GPIO 27). Chaque ligne nomme le composant (`id`, ou le bloc qui n'en a pas) et
la clé YAML qu'elle décrit ; ce test vérifie, en anglais comme en français :

- que la broche écrite est celle du YAML (GPIO de l'ESP32-P4, ou broche d'un
  expandeur PI4IOE5V6408 avec son adresse) ;
- qu'aucune broche configurée par le firmware ne manque au tableau."""
import pathlib
import re

import pytest
import yaml
from tests.commun import ChargeurSansBalises as _Chargeur, sources

REPO = pathlib.Path(__file__).resolve().parents[1]
DOC = REPO / "docs" / "hardware.md"
# Toutes les broches du firmware sont dans ces fichiers : le point d'entrée et ses
# packages (Tab5/paquets/*.yaml). Les ecran-*.yaml n'en ont pas (le modèle d'écran s'en charge).
FICHIERS = [REPO / "tab5-ha-hmi.yaml", *sources("*.yaml")]

# En-tête exact de chaque tableau : un seul par langue dans hardware.md.
ENTETES = {
    "en": "| Function | Pin | In the YAML |",
    "fr": "| Fonction | Broche | Dans le YAML |",
}


def _yaml(chemin):
    return yaml.load(chemin.read_text(encoding="utf-8"), Loader=_Chargeur) or {}


def _expandeurs():
    """id de chaque PI4IOE5V6408 → son adresse écrite « 0x43 »."""
    adresses = {}
    for chemin in FICHIERS:
        for entree in _yaml(chemin).get("pi4ioe5v6408") or []:
            adresses[entree["id"]] = f"0x{entree['address']:02x}"
    return adresses


def _broche(valeur, adresses):
    """Valeur YAML d'une broche → ("GPIO", n) ou ("PI4IOE", "0x43", n) ; None sinon."""
    if isinstance(valeur, str) and re.fullmatch(r"GPIO\d+", valeur):
        return ("GPIO", int(valeur[4:]))
    if isinstance(valeur, dict):
        if "pi4ioe5v6408" in valeur:
            return ("PI4IOE", adresses[valeur["pi4ioe5v6408"]], int(valeur["number"]))
        return _broche(valeur.get("number"), adresses)
    return None


def _broches_yaml():
    """{(composant, clé): broche} pour toute broche du firmware. Le composant est
    l'`id` le plus proche, ou le bloc de premier niveau quand il n'y en a pas."""
    adresses = _expandeurs()
    trouvees = {}

    def parcourir(noeud, composant):
        if isinstance(noeud, list):
            for element in noeud:
                parcourir(element, composant)
            return
        if not isinstance(noeud, dict):
            return
        if isinstance(noeud.get("id"), str):
            composant = noeud["id"]
        for cle, valeur in noeud.items():
            broche = _broche(valeur, adresses)
            if broche is not None:
                assert (composant, cle) not in trouvees, f"{composant} · {cle} en double"
                trouvees[(composant, cle)] = broche
            else:
                parcourir(valeur, composant)

    for chemin in FICHIERS:
        for bloc, contenu in _yaml(chemin).items():
            parcourir(contenu, bloc)
    return trouvees


def _broche_doc(texte):
    """« GPIO 27 » → ("GPIO", 27) ; « PI4IOE 0x43, P1 » → ("PI4IOE", "0x43", 1)."""
    m = re.fullmatch(r"GPIO (\d+)", texte)
    if m:
        return ("GPIO", int(m[1]))
    m = re.fullmatch(r"PI4IOE (0x[0-9a-f]{2}), P(\d)", texte)
    assert m, f"broche illisible dans hardware.md : {texte!r}"
    return ("PI4IOE", m[1], int(m[2]))


def _tableau_doc(langue):
    """{(composant, clé): broche} du tableau de la langue donnée."""
    texte = DOC.read_text(encoding="utf-8")
    entete = ENTETES[langue]
    assert texte.count(entete) == 1, f"tableau des broches ({langue}) introuvable ou en double"
    lignes = texte[texte.index(entete):].splitlines()[2:]  # sans l'en-tête ni |---|
    tableau = {}
    for ligne in lignes:
        if not ligne.startswith("|"):
            break
        _fonction, broche, source = (c.strip() for c in ligne.strip().strip("|").split("|"))
        m = re.fullmatch(r"`(\w+)` · `(\w+)`", source)
        assert m, f"colonne YAML illisible : {source!r}"
        assert (m[1], m[2]) not in tableau, f"{m[1]} · {m[2]} en double ({langue})"
        tableau[(m[1], m[2])] = _broche_doc(broche)
    return tableau


def test_le_yaml_a_des_broches():
    # Garde du parcours lui-même : un YAML devenu illisible ne doit pas donner un
    # tableau vide « conforme ». 24 broches le 29/09/2026.
    broches = _broches_yaml()
    assert len(broches) >= 20
    assert broches[("mic_bus", "i2s_bclk_pin")] == ("GPIO", 27)
    assert broches[("speaker_enable", "pin")] == ("PI4IOE", "0x43", 1)


@pytest.mark.parametrize("langue", sorted(ENTETES))
def test_chaque_ligne_est_celle_du_yaml(langue):
    broches = _broches_yaml()
    for source, broche in _tableau_doc(langue).items():
        assert source in broches, f"{langue} : {source} n'est pas une broche du YAML"
        assert broche == broches[source], (
            f"{langue} : {source} = {broche} dans hardware.md, {broches[source]} dans le YAML")


@pytest.mark.parametrize("langue", sorted(ENTETES))
def test_aucune_broche_oubliee(langue):
    oubliees = set(_broches_yaml()) - set(_tableau_doc(langue))
    assert not oubliees, f"{langue} : broches du YAML absentes de hardware.md : {sorted(oubliees)}"
