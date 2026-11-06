# -*- coding: utf-8 -*-
"""Les tableaux des broches de la doc, comparés au YAML du firmware.

Il remplace docs/images/gpio_pinout_table.png, une image générée par IA en juillet
2026 qui donnait un écran RGB parallèle 1024×600, 16 Mo de PSRAM et le même GPIO 26
pour BCLK et DOUT ; le tableau audio de hardware.md avait repris « BCLK = GPIO 26 »
(c'est GPIO 27). Chaque ligne nomme le composant (`id`, ou le bloc qui n'en a pas) et
la clé YAML qu'elle décrit ; ce test vérifie, en anglais comme en français, et pour
chaque carte (ADR-0061 : docs/tab5-pinout.md pour le Tab5, docs/waveshare-7b.md pour la 7B) :

- que la broche écrite est celle du YAML (GPIO de l'ESP32-P4, ou broche d'un
  expandeur PI4IOE5V6408 avec son adresse) ;
- qu'aucune broche configurée par le firmware ne manque au tableau.

Pour le Tab5 il vérifie aussi, dans docs/tab5-pinout.md, le tableau des adresses I2C écrites
dans le YAML (les autres lignes citent le brochage de M5Stack, hors YAML) et les deux
schémas SVG (docs/images/m5stack-tab5-pinout-diagram*.svg) : mêmes GPIO, mêmes broches
d'expandeur, mêmes adresses que le YAML, un texte alternatif dans chaque langue."""
import html
import pathlib
import re

import pytest
import yaml
from tests.commun import ChargeurBalisesBrutes, ChargeurSansBalises, sources, vue_tab5

REPO = pathlib.Path(__file__).resolve().parents[1]
# Carte → (page de la doc, fichiers où sont toutes ses broches). Le tableau du Tab5 a quitté
# hardware.md pour tab5-pinout.md (06/11/2026).
# Tab5 : le point d'entrée et ses packages (Tab5/paquets/*.yaml), sans ceux des autres
# cartes ; les ecran-*.yaml n'en ont pas (le modèle d'écran s'en charge).
# 7B : son seul fichier de carte. Les broches que les packages communs déclarent sur le
# matériel du Tab5 (expandeurs) y sont retirées par `!extend`/`!remove`, et l'ampli y
# reçoit sa vraie broche (GPIO53) ; ses balises sont lues brutes, pour que
# « id: !extend speaker_enable » nomme bien le composant.
CARTES = {
    "tab5": (REPO / "docs" / "tab5-pinout.md", [REPO / "tab5-ha-hmi.yaml", *vue_tab5(sources("*.yaml"))],
             ChargeurSansBalises),
    "waveshare-7b": (REPO / "docs" / "waveshare-7b.md", [REPO / "Tab5" / "paquets" / "carte-waveshare-7b.yaml"],
                     ChargeurBalisesBrutes),
}

# En-tête exact de chaque tableau : un seul par langue dans chaque page.
ENTETES = {
    "en": "| Function | Pin | In the YAML |",
    "fr": "| Fonction | Broche | Dans le YAML |",
}


def _yaml(chemin, chargeur=ChargeurSansBalises):
    return yaml.load(chemin.read_text(encoding="utf-8"), Loader=chargeur) or {}


def _expandeurs(fichiers, chargeur):
    """id de chaque PI4IOE5V6408 → son adresse écrite « 0x43 »."""
    adresses = {}
    for chemin in fichiers:
        for entree in _yaml(chemin, chargeur).get("pi4ioe5v6408") or []:
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


def _broches_yaml(carte="tab5"):
    """{(composant, clé): broche} pour toute broche du firmware de la carte. Le composant
    est l'`id` le plus proche, ou le bloc de premier niveau quand il n'y en a pas."""
    _doc, fichiers, chargeur = CARTES[carte]
    adresses = _expandeurs(fichiers, chargeur)
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

    for chemin in fichiers:
        for bloc, contenu in _yaml(chemin, chargeur).items():
            parcourir(contenu, bloc)
    return trouvees


def _broche_doc(texte):
    """« GPIO 27 » → ("GPIO", 27) ; « PI4IOE 0x43, P1 » → ("PI4IOE", "0x43", 1)."""
    m = re.fullmatch(r"GPIO (\d+)", texte)
    if m:
        return ("GPIO", int(m[1]))
    m = re.fullmatch(r"PI4IOE (0x[0-9a-f]{2}), P(\d)", texte)
    assert m, f"broche illisible dans la doc : {texte!r}"
    return ("PI4IOE", m[1], int(m[2]))


def _tableau_doc(langue, carte="tab5"):
    """{(composant, clé): broche} du tableau de la langue donnée, dans la page de la carte."""
    doc, _fichiers, _chargeur = CARTES[carte]
    texte = doc.read_text(encoding="utf-8")
    entete = ENTETES[langue]
    assert texte.count(entete) == 1, f"{doc.name} : tableau des broches ({langue}) introuvable ou en double"
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
    # tableau vide « conforme ». 24 broches le 29/09/2026 sur le Tab5, 17 sur la 7B le 16/10.
    broches = _broches_yaml()
    assert len(broches) >= 20
    assert broches[("mic_bus", "i2s_bclk_pin")] == ("GPIO", 27)
    assert broches[("speaker_enable", "pin")] == ("PI4IOE", "0x43", 1)
    broches_7b = _broches_yaml("waveshare-7b")
    assert len(broches_7b) >= 15
    assert broches_7b[("speaker_enable", "pin")] == ("GPIO", 53)
    assert broches_7b[("touch", "reset_pin")] == ("GPIO", 23)


@pytest.mark.parametrize("carte", sorted(CARTES))
@pytest.mark.parametrize("langue", sorted(ENTETES))
def test_chaque_ligne_est_celle_du_yaml(langue, carte):
    broches = _broches_yaml(carte)
    for source, broche in _tableau_doc(langue, carte).items():
        assert source in broches, f"{carte}, {langue} : {source} n'est pas une broche du YAML"
        assert broche == broches[source], (
            f"{carte}, {langue} : {source} = {broche} dans la doc, {broches[source]} dans le YAML")


@pytest.mark.parametrize("carte", sorted(CARTES))
@pytest.mark.parametrize("langue", sorted(ENTETES))
def test_aucune_broche_oubliee(langue, carte):
    oubliees = set(_broches_yaml(carte)) - set(_tableau_doc(langue, carte))
    assert not oubliees, f"{carte}, {langue} : broches du YAML absentes de la doc : {sorted(oubliees)}"


# --- Adresses I2C et schémas du Tab5 (docs/tab5-pinout.md) ------------------------------

ENTETES_ADRESSES = {
    "en": "| Chip | Address | Source |",
    "fr": "| Puce | Adresse | Source |",
}
# Schéma de chaque langue, dans docs/images/ : un seul modèle, deux fichiers.
SCHEMAS = {
    "en": "m5stack-tab5-pinout-diagram.svg",
    "fr": "m5stack-tab5-pinout-diagram-fr.svg",
}


def _adresses_yaml():
    """{id: « 0x43 »} de chaque composant du Tab5 qui écrit `address:` dans le YAML."""
    _doc, fichiers, chargeur = CARTES["tab5"]
    trouvees = {}

    def parcourir(noeud):
        if isinstance(noeud, list):
            for element in noeud:
                parcourir(element)
        elif isinstance(noeud, dict):
            if isinstance(noeud.get("id"), str) and isinstance(noeud.get("address"), int):
                trouvees[noeud["id"]] = f"0x{noeud['address']:02x}"
            for valeur in noeud.values():
                parcourir(valeur)

    for chemin in fichiers:
        for contenu in _yaml(chemin, chargeur).values():
            parcourir(contenu)
    return trouvees


def _adresses_doc(langue):
    """{id: adresse} des lignes du tableau des adresses dont la source est « `id` · `address` »."""
    doc = CARTES["tab5"][0]
    texte = doc.read_text(encoding="utf-8")
    entete = ENTETES_ADRESSES[langue]
    assert texte.count(entete) == 1, f"{doc.name} : tableau des adresses ({langue}) introuvable ou en double"
    adresses = {}
    for ligne in texte[texte.index(entete):].splitlines()[2:]:
        if not ligne.startswith("|"):
            break
        _puce, adresse, source = (c.strip() for c in ligne.strip().strip("|").split("|"))
        m = re.match(r"`(\w+)` · `address`", source)
        if m:
            assert m[1] not in adresses, f"{m[1]} en double ({langue})"
            adresses[m[1]] = adresse
    return adresses


def test_le_yaml_a_des_adresses():
    # Garde du parcours : les quatre adresses écrites dans le YAML du Tab5 (29/09/2026).
    assert _adresses_yaml() == {"pi4ioe1": "0x43", "pi4ioe2": "0x44", "tab5_imu": "0x68", "ina226_batterie": "0x41"}


@pytest.mark.parametrize("langue", sorted(ENTETES_ADRESSES))
def test_les_adresses_i2c_sont_celles_du_yaml(langue):
    assert _adresses_doc(langue) == _adresses_yaml()


def _textes_svg(langue):
    brut = (REPO / "docs" / "images" / SCHEMAS[langue]).read_text(encoding="utf-8")
    textes = re.findall(r"<text[^>]*>(.*?)</text>", brut, flags=re.S)
    return html.unescape("\n".join(re.sub(r"<[^>]+>", "", t) for t in textes))


@pytest.mark.parametrize("langue", sorted(SCHEMAS))
def test_le_schema_est_celui_du_yaml(langue):
    """Les GPIO, les broches d'expandeur et les adresses écrits dans le schéma sont
    exactement ceux du YAML : ni une broche inventée, ni une broche oubliée."""
    texte = _textes_svg(langue)
    broches = _broches_yaml()
    gpio_yaml = {b[1] for b in broches.values() if b[0] == "GPIO"}
    gpio_svg = {int(n) for liste in re.findall(r"GPIO (\d+(?: · \d+)*)", texte) for n in liste.split(" · ")}
    assert gpio_svg == gpio_yaml, f"{langue} : GPIO du schéma {sorted(gpio_svg)}, du YAML {sorted(gpio_yaml)}"
    expandeur_yaml = {(b[1], b[2]) for b in broches.values() if b[0] == "PI4IOE"}
    expandeur_svg = {(a, int(n)) for a, n in re.findall(r"(0x[0-9a-f]{2}) P(\d)", texte)}
    assert expandeur_svg == expandeur_yaml, f"{langue} : expandeurs du schéma {sorted(expandeur_svg)}, du YAML {sorted(expandeur_yaml)}"
    adresses_svg = set(re.findall(r"0x[0-9a-f]{2}", texte))
    assert adresses_svg == set(_adresses_yaml().values()), f"{langue} : adresses du schéma {sorted(adresses_svg)}"


@pytest.mark.parametrize("langue", sorted(SCHEMAS))
def test_le_schema_est_accessible_et_place_dans_la_page(langue):
    """Le SVG a un titre et une description (lecteurs d'écran), et la page l'appelle avec un
    texte alternatif, dans la partie de la bonne langue."""
    brut = (REPO / "docs" / "images" / SCHEMAS[langue]).read_text(encoding="utf-8")
    assert 'role="img"' in brut and f'xml:lang="{langue}"' in brut
    assert re.search(r'<title id="titre">[^<]{20,}</title>', brut) and re.search(r'<desc id="desc">[^<]{100,}</desc>', brut)
    page = CARTES["tab5"][0].read_text(encoding="utf-8")
    anglais, francais = page.split("## Version Française")
    partie = anglais if langue == "en" else francais
    m = re.search(r"!\[([^\]]*)\]\(images/" + re.escape(SCHEMAS[langue]) + r"\)", partie)
    assert m, f"{SCHEMAS[langue]} n'est pas dans la partie « {langue} » de tab5-pinout.md"
    assert len(m[1]) >= 60, f"texte alternatif trop court : {m[1]!r}"
