# -*- coding: utf-8 -*-
"""Pièces et tuiles génériques (ADR-0023), côté Home Assistant : le blueprint
« Tab5 — emplacements » décrit les pièces (tab5_maj_tuiles) et pousse l'état des tuiles
(clés tRT de tab5_maj_emplacements). Aucun compilateur ne compare ces chaînes au
contrat ; ce fichier le fait de deux façons :

- à la lecture : types, options et commandes du blueprint = tableaux de l'ADR, marqueurs
  du bloc des icônes, entrées toutes facultatives, entrées et clés 3.x conservées,
  déclencheurs des pièces, tab5_maj_tuiles jamais appelée sans le protocole 2 ;
- au rendu : les VRAIS modèles Jinja du blueprint sont rendus ici, dans le bac à sable de
  Jinja comme le fait Home Assistant, avec de fausses entités (`Etat`), un faux
  `trigger` et une fausse tablette. On vérifie les définitions, les états, le protocole,
  les tuiles poussées à chaque déclencheur (un seul chemin par tuile) et l'aiguillage
  des commandes. Ce n'est pas Home Assistant : seules les fonctions de modèle que le
  blueprint appelle sont imitées, au plus près (valeur rendue relue comme un littéral
  Python, comme `render_complex`). Les modèles ont aussi été éprouvés sur un vrai HA
  (outil ha_eval_template, 28/09/2026) et le job « Installation dans un HA neuf » les
  exécute dans un HA en conteneur."""
import ast
import datetime as dt
import math
import os
import re

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
ADR = os.path.join(REPO, "docs", "decisions", "0023-rooms-generic-tiles.md")

MAINTENANT = dt.datetime(2026, 9, 28, 12, 0, 0, tzinfo=dt.timezone.utc)


def _lire(chemin):
    with open(chemin, encoding="utf-8") as f:
        return f.read()


# ─────────────────────────────────────────────────────────────────────────────
# Lecture du blueprint (les !input restent des références, résolues plus bas)
# ─────────────────────────────────────────────────────────────────────────────

class _Entree:
    def __init__(self, nom):
        self.nom = nom


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_constructor("!input", lambda chargeur, noeud: _Entree(chargeur.construct_scalar(noeud)))


def _blueprint():
    return yaml.load(_lire(BLUEPRINT), Loader=_Chargeur)


def _entrees(bp):
    """Nom → définition de chaque entrée, sections aplaties."""
    toutes = {}
    for section in bp["blueprint"]["input"].values():
        toutes.update(section["input"])
    return toutes


def _substituer(valeur, entrees):
    if isinstance(valeur, _Entree):
        return entrees[valeur.nom]
    if isinstance(valeur, list):
        return [_substituer(v, entrees) for v in valeur]
    if isinstance(valeur, dict):
        return {k: _substituer(v, entrees) for k, v in valeur.items()}
    return valeur


def _chercher(noeud, predicat):
    """Premier dict de l'arbre (profondeur d'abord) pour lequel predicat est vrai."""
    if isinstance(noeud, dict):
        if predicat(noeud):
            return noeud
        enfants = noeud.values()
    elif isinstance(noeud, list):
        enfants = noeud
    else:
        return None
    for enfant in enfants:
        if (trouve := _chercher(enfant, predicat)) is not None:
            return trouve
    return None


# ─────────────────────────────────────────────────────────────────────────────
# Imitation de ce que le blueprint appelle dans le Jinja de Home Assistant
# ─────────────────────────────────────────────────────────────────────────────

class Etat:
    def __init__(self, entity_id, state, aire=None, age=3600, **attributes):
        self.entity_id = entity_id
        self.state = state
        self.attributes = attributes
        self.aire = aire
        self.last_changed = MAINTENANT - dt.timedelta(seconds=age)
        self.last_updated = self.last_changed
        self.name = attributes.get("friendly_name", entity_id)


class Etats:
    def __init__(self, etats):
        self.d = {e.entity_id: e for e in etats}

    def __call__(self, entity_id):
        e = self.d.get(entity_id)
        return e.state if e else "unknown"

    def __getitem__(self, entity_id):
        return self.d.get(entity_id)

    def __getattr__(self, domaine):
        """`states.weather` : les états d'un domaine (le blueprint y cherche l'unité de
        température, clé climr de l'ADR-0026)."""
        if domaine.startswith("_") or domaine == "d":
            raise AttributeError(domaine)
        return [e for e in self.d.values() if e.entity_id.split(".")[0] == domaine]

    def attr(self, entity_id, nom):
        e = self.d.get(entity_id)
        return e.attributes.get(nom) if e else None

    def aire(self, entity_id):
        e = self.d.get(entity_id)
        return e.aire if e else None


def _analyser(brut):
    """Comme Home Assistant (Template._parse_result) : un littéral Python devient sa
    valeur, une chaîne reste le texte rendu."""
    brut = brut.strip()
    try:
        valeur = ast.literal_eval(brut)
    except (ValueError, SyntaxError, TypeError, MemoryError):
        return brut
    return brut if isinstance(valeur, str) else valeur


def _est_un_nombre(valeur):
    """Filtre is_number de Home Assistant : float() réussit et le nombre est fini
    (icône solaire du bandeau, variable solaire_pourcent)."""
    try:
        x = float(valeur)
    except (TypeError, ValueError):
        return False
    return math.isfinite(x)


def _environnement(etats, tablettes):
    env = ImmutableSandboxedEnvironment(extensions=["jinja2.ext.loopcontrols"], undefined=jinja2.StrictUndefined)

    def device_attr(entity_id, nom):
        return tablettes.get(entity_id, {}).get(nom)

    env.globals.update(
        states=etats, state_attr=etats.attr, area_name=etats.aire, device_attr=device_attr,
        integration_entities=lambda domaine: list(tablettes) if domaine == "esphome" else [],
        now=lambda: MAINTENANT, device_id=lambda e: None, device_entities=lambda d: [],
    )
    env.filters.update(
        area_name=etats.aire, device_attr=device_attr, is_number=_est_un_nombre,
        regex_findall=lambda v, motif="", ignorecase=False: re.findall(motif, str(v), re.I if ignorecase else 0),
    )
    env.tests.update(
        match=lambda v, motif, ignorecase=False: bool(re.match(motif, str(v), re.I if ignorecase else 0)),
        is_state=lambda e, s: etats(e) == s,
        is_state_attr=lambda e, a, v: etats.attr(e, a) == v,
    )
    return env


def _rendre(env, valeur, contexte):
    if isinstance(valeur, str):
        if "{{" in valeur or "{%" in valeur:
            return _analyser(env.from_string(valeur).render(contexte))
        return valeur
    if isinstance(valeur, list):
        return [_rendre(env, v, contexte) for v in valeur]
    if isinstance(valeur, dict):
        return {k: _rendre(env, v, contexte) for k, v in valeur.items()}
    return valeur


def _tablette(version, connectee=True):
    return {"binary_sensor.tab5_ha_api_status": {"model": "tab5-ha-hmi", "sw_version": version, "_on": connectee}}


class Passage:
    """Un passage de l'automatisation : les variables du blueprint rendues dans l'ordre,
    puis, à la demande, les conditions et les variables des actions."""

    def __init__(self, entrees, etats, trigger, tablettes=None):
        self.bp = _blueprint()
        defauts = {nom: e.get("default") for nom, e in _entrees(self.bp).items()}
        inconnues = set(entrees) - set(defauts)
        assert not inconnues, f"entrées inconnues du blueprint : {inconnues}"
        self.corps = _substituer({k: v for k, v in self.bp.items() if k != "blueprint"}, {**defauts, **entrees})
        tablettes = _tablette("3.2.0-dev (ESPHome 2026.9.0)") if tablettes is None else tablettes
        liste = list(etats) + [Etat(e, "on" if t.get("_on", True) else "off") for e, t in tablettes.items()]
        self.etats = Etats(liste)
        self.env = _environnement(self.etats, tablettes)
        self.ctx = {"trigger": trigger}
        for nom, valeur in self.corps["variables"].items():
            self.ctx[nom] = _rendre(self.env, valeur, self.ctx)

    def __getitem__(self, nom):
        return self.ctx[nom]

    def modele(self, texte):
        return _analyser(self.env.from_string(texte).render(self.ctx))

    def conditions(self):
        return all(self.modele(c["value_template"]) for c in self.corps["conditions"])

    def variables_du_bloc(self, nom):
        """Rend le bloc `variables:` des actions qui définit `nom` (et ceux d'avant)."""
        bloc = _chercher(self.corps["actions"], lambda d: nom in (d.get("variables") or {}))
        assert bloc, f"variable d'action {nom} introuvable"
        for cle, valeur in bloc["variables"].items():
            self.ctx[cle] = _rendre(self.env, valeur, self.ctx)
        return self.ctx[nom]

    def definitions(self):
        return self.variables_du_bloc("definitions")

    def etats_tuiles(self):
        """États des tuiles (tRT) ; le premier bloc des actions d'abord : les clims des
        tuiles, dans le même bloc, y lisent reglages_clims et reglages_changes."""
        if not self["tuiles_a_pousser"]:
            return ""
        self.variables_du_bloc("reglages_clims")
        return self.variables_du_bloc("etats_tuiles")

    def aiguillage(self):
        """(alias, actions) de la branche que choisit une commande de l'écran."""
        self.variables_du_bloc("t_commande")
        branches = _chercher(self.corps["actions"], lambda d: any(
            (b.get("alias") or "").startswith("Tuile : basculer") for b in d.get("choose", [])))["choose"]
        for b in branches:
            if self.modele(b["conditions"]):
                return b["alias"], b["sequence"]
        return None, []


def _defs(texte):
    """« a|b;c|d; » → [['a','b'], ['c','d']]."""
    return [e.split("|") for e in texte.split(";") if e]


def _declencheur(id_, avant=None, apres=None, entite=None):
    t = {"id": id_, "platform": "state", "from_state": avant, "to_state": apres}
    t["entity_id"] = entite or (apres or avant).entity_id
    return t


def _evenement(id_, **donnees):
    return {"id": id_, "platform": "event", "event": {"data": donnees}}


# ─────────────────────────────────────────────────────────────────────────────
# Une maison de test (identifiants inventés)
# ─────────────────────────────────────────────────────────────────────────────

def _maison():
    return [
        Etat("light.chevet", "on", "Chambre", friendly_name="Lampe Chambre",
             supported_color_modes=["color_temp", "hs"], brightness=128, rgb_color=(255, 200, 100)),
        Etat("light.plafond", "off", "Chambre", friendly_name="Plafonnier de la chambre",
             supported_color_modes=["brightness"]),
        Etat("light.guirlande", "on", friendly_name="Guirlande", supported_color_modes=["onoff"]),
        Etat("light.bureau", "on", "Bureau", friendly_name="Lampe bureau", supported_color_modes=["color_temp"],
             brightness=255, rgb_color=None),
        Etat("switch.prise_pc", "on", "Bureau", friendly_name="Prise PC"),
        Etat("media_player.tele", "playing", "Salon", friendly_name="Télé du salon"),
        Etat("cover.store", "open", "Salon", friendly_name="Store salon", current_position=45,
             supported_features=15),
        Etat("cover.volet_serre", "unknown", friendly_name="Volet serre", device_class="curtain"),
        Etat("sensor.temp_salon", "21.4", "Salon", friendly_name="Température salon",
             unit_of_measurement="°C", device_class="temperature"),
        Etat("sensor.conso", "3.2", friendly_name="Conso | voiture; électrique", unit_of_measurement="kWh/100km"),
        Etat("climate.clim", "heat_cool", "Salon", friendly_name="Clim salon", current_temperature=22.5),
        Etat("binary_sensor.porte", "off", "Salon", friendly_name="Porte d'entrée", device_class="door"),
        Etat("person.alice", "home", friendly_name="Alice"),
        Etat("lock.serrure", "locked", friendly_name="Serrure"),
        Etat("scene.soiree", "2026-09-27T20:00:00+00:00", friendly_name="Soirée"),
        Etat("script.cafe", "off", friendly_name="Café"),
        Etat("button.sonnette", "unknown", friendly_name="Sonnette"),
        Etat("valve.arrosage", "closed", friendly_name="Arrosage"),
        Etat("fan.ventilo", "off", "Chambre", friendly_name="Ventilateur chambre"),
        Etat("calendar.agenda", "off", friendly_name="Agenda"),
        Etat("input_text.volet_serre_etat", "En_mouvement"),
        Etat("script.tab5_volet_action", "off"),
        # Liste du package : le volet de l'entrée Volet, comme chez l'auteur.
        Etat("select.tab5_volet_a_course_simulee", "cover.volet_serre"),
    ]


ENTREES = {
    "piece_1_tuiles": ["light.chevet", "light.plafond", "calendar.agenda", "light.inexistante",
                       "light.guirlande", "switch.prise_pc", "cover.store", "sensor.temp_salon"],
    "piece_2_nom": "Salon",
    "piece_2_tuiles": ["media_player.tele", "cover.store", "sensor.temp_salon", "climate.clim", "binary_sensor.porte"],
    "piece_3_tuiles": ["light.chevet", "light.plafond", "fan.ventilo"],
    "piece_4_tuiles": ["person.alice", "lock.serrure", "scene.soiree", "script.cafe", "button.sonnette",
                       "valve.arrosage"],
    "piece_5_nom": "  Garage  ",
    "piece_5_tuiles": ["sensor.conso", "cover.volet_serre"],
    "personnalisation": [
        {"entite": "light.guirlande", "nom": "Sapin | Noël; 2026", "icone": "mdi:led-strip-variant",
         "comportement": "allumer_seulement"},
        {"entite": "switch.prise_pc", "icone": "mdi:icone-hors-palette", "comportement": "confirmer"},
        {"entite": "sensor.conso", "comportement": "lecture_seule"},
        {"entite": "light.guirlande", "nom": "ignoré : la première ligne compte"},
    ],
    "tv": "media_player.tele",
    "clim": "climate.clim",
    "volet": "cover.volet_serre",
    "pc": "switch.prise_pc",
    "lumiere_1": "light.chevet",
}


def _passage(trigger=None, entrees=ENTREES, etats=None, tablettes=None):
    return Passage(entrees, _maison() if etats is None else etats, trigger or _evenement("connexion"), tablettes)


def _icone(bp, mdi=None, domaine="", classe=""):
    """Ce que doit donner la palette du blueprint (ADR-0023, Icons)."""
    mdi_vers_code = bp["variables"]["icones_mdi"]
    defauts = bp["variables"]["icones_defaut"]
    defaut = defauts.get(f"{domaine}.{classe}", defauts.get(domaine, "")) if classe else defauts.get(domaine, "")
    return mdi_vers_code.get(mdi, defaut) if mdi else defaut


# ─────────────────────────────────────────────────────────────────────────────
# Tableaux de l'ADR-0023
# ─────────────────────────────────────────────────────────────────────────────

def _tableau_des_types():
    """{type: [domaines]} du tableau « Type | HA domains | Tap | Long press »."""
    texte = _lire(ADR).split("| Type | HA domains |", 1)[1].split("\n\n", 1)[0]
    types = {}
    for ligne in texte.splitlines()[2:]:
        colonnes = [c.strip() for c in ligne.strip("|").split("|")]
        types[colonnes[0].strip("`")] = re.findall(r"`(\w+)`", colonnes[1])
    return types


def _commandes_de_l_adr():
    texte = _lire(ADR).split("| `action` | `valeur` | For |", 1)[1].split("\n\n", 1)[0]
    commandes = set()
    for ligne in texte.splitlines()[2:]:
        premiere = ligne.strip("|").split("|")[0]
        commandes.update(c for c in re.findall(r"`(\w+)`", premiere) if c != "pR")
    return commandes


def _options_de_l_adr():
    ligne = next(l for l in _lire(ADR).splitlines() if l.startswith("Options:"))
    return re.findall(r"`([a-z])` ", ligne)


def test_types_par_domaine_egaux_au_tableau_de_l_adr():
    types = _tableau_des_types()
    assert set(types) == {"lum", "int", "vol", "med", "act", "cap", "bin", "cli"}
    attendu = {d: t for t, domaines in types.items() for d in domaines}
    assert _blueprint()["variables"]["types_par_domaine"] == attendu


def test_le_selecteur_des_pieces_suit_les_domaines_des_tuiles():
    entrees = _entrees(_blueprint())
    domaines = set(_blueprint()["variables"]["types_par_domaine"])
    for n in range(1, 6):
        selecteur = entrees[f"piece_{n}_tuiles"]["selector"]["entity"]
        assert selecteur["multiple"] is True and selecteur["reorder"] is True
        assert set(selecteur["filter"][0]["domain"]) == domaines
    champ = entrees["personnalisation"]["selector"]["object"]["fields"]["entite"]["selector"]["entity"]
    assert set(champ["filter"][0]["domain"]) == domaines


def test_options_emises_egales_a_celles_de_l_adr():
    lettres = _options_de_l_adr()
    assert lettres == ["d", "c", "o", "k", "r", "t", "m", "e"]
    bloc = _chercher(_blueprint()["actions"], lambda d: "definitions" in (d.get("variables") or {}))
    modele = bloc["variables"]["definitions"]
    emises = re.findall(r"\('([a-z])' if ", modele)
    assert emises == lettres, "chaque option de l'ADR, une fois, dans l'ordre"


def test_chaque_commande_de_l_adr_a_sa_branche():
    texte = _lire(BLUEPRINT)
    commandes = _commandes_de_l_adr()
    assert commandes == {"basculer", "allumer", "eteindre", "ouvrir", "fermer", "arreter", "lancer",
                         "luminosite", "luminosite_pct", "couleur", "position"}
    for c in commandes:
        assert re.search(rf"t_commande (== |in \[[^\]]*)'{c}'", texte), f"commande {c} sans branche de tuile"
    assert "emplacement is match('^p[0-4]$') and commande == 'eteindre'" in texte


# ─────────────────────────────────────────────────────────────────────────────
# Entrées, marqueurs, déclencheurs
# ─────────────────────────────────────────────────────────────────────────────

def test_entrees_des_pieces_et_sections():
    bp = _blueprint()
    sections = bp["blueprint"]["input"]
    noms = list(sections)
    assert noms[:5] == [f"piece_{n}" for n in range(1, 6)]
    assert "collapsed" not in sections["piece_1"], "la pièce 1 (accueil) est ouverte"
    for n in range(2, 6):
        assert sections[f"piece_{n}"]["collapsed"] is True
    assert sections["piece_1"]["name"].startswith("Pièce 1 — accueil")
    assert set(sections[f"piece_{1}"]["input"]) == {"piece_1_nom", "piece_1_tuiles"}
    assert sections["accueil_3x"]["collapsed"] is True
    assert set(sections["accueil_3x"]["input"]) == {"lumiere_1", "lumiere_2", "lumiere_3", "pc", "volet"}
    assert sections["personnaliser"]["collapsed"] is True
    champs = sections["personnaliser"]["input"]["personnalisation"]["selector"]["object"]
    assert champs["multiple"] is True
    assert set(champs["fields"]) == {"entite", "nom", "icone", "comportement"}
    valeurs = [o["value"] for o in champs["fields"]["comportement"]["selector"]["select"]["options"]]
    assert valeurs == ["normal", "allumer_seulement", "confirmer", "lecture_seule"]


def test_editeur_lisible():
    """Relecture du 07/10/2026 : toutes les sections repliées sauf la pièce 1, une phrase
    d'aide (français puis anglais) sur chaque entrée, l'Énergie en trois sections. Les
    sections ne changent pas où HA range les valeurs (à plat sous use_blueprint.input,
    Blueprint.inputs de homeassistant/components/blueprint/models.py)."""
    sections = _blueprint()["blueprint"]["input"]
    for nom, s in sections.items():
        assert s.get("collapsed") is (None if nom == "piece_1" else True), f"section {nom}"
    for nom, e in _entrees(_blueprint()).items():
        assert "\n\n" in e.get("description", ""), f"entrée {nom} : description FR puis EN"
    energie = [n for n in sections if n.startswith("energie")]
    assert energie == ["energie", "energie_reseau_maison", "energie_stockage"]
    assert list(sections["energie"]["input"]) == ["energie_solaire", "energie_solaire_autres", "energie_crete",
                                                  "energie_production", "energie_production_autres"]
    assert list(sections["energie_reseau_maison"]["input"]) == ["energie_reseau", "energie_reseau_export",
                                                               "energie_reseau_inverse", "energie_maison"]
    assert list(sections["energie_stockage"]["input"]) == ["energie_batterie", "energie_batterie_puissance",
                                                           "energie_batterie_inverse", "energie_batterie_temperature"]


def test_entrees_toutes_facultatives_et_noms_3x_conserves():
    entrees = _entrees(_blueprint())
    for nom, e in entrees.items():
        assert "default" in e, f"entrée {nom} obligatoire"
    anciennes = {"lumiere_1", "lumiere_2", "lumiere_3", "pc", "tv", "tv_telecommande", "telephone",
                 "salon_temperature", "salon_humidite", "serre_temperature", "clim", "volet",
                 "pot_1", "pot_2", "pot_3", "pot_4", "pot_5", "agenda_travail", "tablette"}
    assert anciennes <= set(entrees), f"entrées 3.x disparues : {anciennes - set(entrees)}"


def test_marqueurs_du_bloc_des_icones():
    texte = _lire(BLUEPRINT)
    debut = "  # >>> icones (généré par tools/gen_tuiles_icones.py, ne pas éditer)\n"
    fin = "  # <<< icones\n"
    assert texte.count(debut) == 1 and texte.count(fin) == 1
    bloc = texte.split(debut, 1)[1].split(fin, 1)[0]
    contenu = yaml.safe_load(bloc)
    assert set(contenu) == {"icones_mdi", "icones_defaut"}
    variables = _blueprint()["variables"]
    assert variables["icones_mdi"] == contenu["icones_mdi"] and variables["icones_defaut"] == contenu["icones_defaut"]
    # Dans `variables:` (le bloc est au niveau de ses clés).
    assert texte.index("\nvariables:\n") < texte.index(debut) < texte.index("\ntriggers:\n")
    for mdi, code in contenu["icones_mdi"].items():
        assert mdi.startswith("mdi:") and re.fullmatch(r"[a-z0-9_]{1,15}", code), (mdi, code)
    domaines = set(variables["types_par_domaine"])
    for cle, code in contenu["icones_defaut"].items():
        assert cle.split(".")[0] in domaines and re.fullmatch(r"[a-z0-9_]{1,15}", code), (cle, code)


def test_declencheurs_des_pieces():
    bp = _blueprint()
    ids = [t["id"] for t in bp["triggers"]]
    assert len(ids) == len(set(ids)), "identifiants de déclencheurs en double"
    for n in range(1, 6):
        mes = [t for t in bp["triggers"] if t["id"].startswith(f"piece_{n}")]
        assert [t["id"] for t in mes] == [f"piece_{n}", f"piece_{n}_sortie", f"piece_{n}_luminosite",
                                         f"piece_{n}_couleur", f"piece_{n}_position", f"piece_{n}_consigne"]
        assert all(t["entity_id"].nom == f"piece_{n}_tuiles" for t in mes)
        visibles = mes[0]["to"]
        # « on »/« off » sans guillemets seraient des booléens, refusés par le schéma.
        assert all(isinstance(e, str) for e in visibles) and {"on", "off"} <= set(visibles)
        assert mes[1]["from"] == visibles and mes[1]["not_to"] == visibles
        # Consigne d'une clim (attribut temperature, ADR-0027) ; jamais current_temperature,
        # la température de la pièce, qui part avec les mesures.
        assert [t.get("attribute") for t in mes[2:]] == ["brightness", "rgb_color", "current_position", "temperature"]
        assert all(t["not_from"] == [None] and t["not_to"] == [None] for t in mes[2:])
    # Aucun état de mesure dans la liste : un capteur ne réveille jamais l'automatisation.
    assert not [e for e in visibles if re.fullmatch(r"-?[0-9.]+", e)]


def test_tab5_maj_tuiles_seulement_au_protocole_2():
    bp = _blueprint()
    appels = []

    def parcourir(noeud, gardes):
        if isinstance(noeud, dict):
            if "_tab5_maj_tuiles" in str(noeud.get("action", "")):
                appels.append(gardes)
            garde = [noeud["if"]] if "if" in noeud else []
            for cle, v in noeud.items():
                parcourir(v, gardes + garde if cle == "then" else gardes)
        elif isinstance(noeud, list):
            for v in noeud:
                parcourir(v, gardes)

    parcourir(bp["actions"], [])
    assert len(appels) == 1
    assert "{{ protocole == 2 }}" in appels[0]
    # Les états des tuiles (clés tRT) aussi.
    bloc = _chercher(bp["actions"], lambda d: "etats_tuiles" in str(d.get("if", "")))
    assert "protocole == 2" in bloc["if"]


# ─────────────────────────────────────────────────────────────────────────────
# Rendu : protocole
# ─────────────────────────────────────────────────────────────────────────────

@pytest.mark.parametrize("version, attendu", [
    ("3.1.0 (ESPHome 2026.9.0)", 1),
    ("3.2.0-dev (ESPHome 2026.9.0)", 2),
    ("3.2.0-rendu (ESPHome 2026.9.0)", 2),
    ("3.10.1 (ESPHome 2026.9.0)", 2),
    ("4.0.0", 2),
    ("3.1.9", 1),
    ("rendu (ESPHome 2026.9.0)", 1),
    ("", 1),
    (None, 1),
])
def test_protocole_lu_dans_la_version(version, attendu):
    assert _passage(tablettes=_tablette(version))["protocole"] == attendu


def test_protocole_sans_tablette_ou_avec_plusieurs():
    assert _passage(tablettes={})["protocole"] == 1
    deux = {**_tablette("3.2.0 (ESPHome 2026.9.0)"),
            "binary_sensor.autre_ha_api_status": {"model": "tab5-ha-hmi", "sw_version": "3.1.0 (ESPHome 2026.9.0)"}}
    assert _passage(tablettes=deux)["protocole"] == 1, "la plus ancienne décide"
    autre_modele = {**_tablette("3.2.0"), "binary_sensor.x_ha_api_status": {"model": "autre", "sw_version": "1.0.0"}}
    assert _passage(tablettes=autre_modele)["protocole"] == 2


# ─────────────────────────────────────────────────────────────────────────────
# Rendu : définitions
# ─────────────────────────────────────────────────────────────────────────────

def test_definitions_de_toutes_les_pieces():
    p = _passage()
    bp = p.bp
    defs = _defs(p.definitions())
    attendu = [
        # Pièce 1 : cinq premières entités qui existent et ont un type (agenda et entité
        # absente sautés) ; aires Chambre, Bureau, Salon : pas de nom commun.
        ["p0", ""],
        ["t00", "lum", _icone(bp, domaine="light"), "dc", "", "Lampe Chambre"],
        ["t01", "lum", _icone(bp, domaine="light"), "d", "", "Plafonnier de la chambre"],
        ["t02", "lum", _icone(bp, "mdi:led-strip-variant", "light"), "o", "", "Sapin / Noël, 2026"],
        ["t03", "int", _icone(bp, "mdi:icone-hors-palette", "switch"), "k", "", "Prise PC"],
        ["t04", "vol", _icone(bp, domaine="cover"), "", "", "Store salon"],
        # Pièce 2 : nom saisi, retiré des noms (mot entier, casse ignorée, « du » final).
        ["p1", "Salon"],
        ["t10", "med", _icone(bp, domaine="media_player"), "t", "", "Télé"],
        ["t11", "vol", _icone(bp, domaine="cover"), "", "", "Store"],
        ["t12", "cap", _icone(bp, domaine="sensor", classe="temperature"), "", "°C", "Température"],
        ["t13", "cli", _icone(bp, domaine="climate"), "m", "", "Clim"],
        ["t14", "bin", _icone(bp, domaine="binary_sensor", classe="door"), "", "door", "Porte d'entrée"],
        # Pièce 3 : sans nom, l'aire commune de ses appareils (Chambre), retirée des noms.
        ["p2", "Chambre"],
        ["t20", "lum", _icone(bp, domaine="light"), "dc", "", "Lampe"],
        ["t21", "lum", _icone(bp, domaine="light"), "d", "", "Plafonnier"],
        ["t22", "int", _icone(bp, domaine="fan"), "", "", "Ventilateur"],
        # Pièce 4 : cinq au plus (la vanne, sixième, n'y est pas).
        ["p3", ""],
        ["t30", "bin", _icone(bp, domaine="person"), "", "presence", "Alice"],
        ["t31", "bin", _icone(bp, domaine="lock"), "", "lock", "Serrure"],
        ["t32", "act", _icone(bp, domaine="scene"), "", "", "Soirée"],
        ["t33", "act", _icone(bp, domaine="script"), "", "", "Café"],
        ["t34", "act", _icone(bp, domaine="button"), "", "", "Sonnette"],
        # Pièce 5 : nom saisi nettoyé ; unité coupée à 7 octets ; « | » et « ; » remplacés.
        ["p4", "Garage"],
        ["t40", "cap", _icone(bp, domaine="sensor"), "r", "kWh/100", "Conso / voiture, électrique"],
        ["t41", "vol", _icone(bp, domaine="cover", classe="curtain"), "", "", "Volet serre"],
        # Rangée sous l'horloge (ADR-0031) : réglages par défaut, plantes en premier, 32 s ;
        # aucune ligne choisie (tests/test_rangee.py pour le reste).
        ["hp", "0"],
        ["hd", "32"],
    ]
    assert defs == attendu


def test_nom_de_piece_par_l_aire_seulement_si_elle_est_unique():
    etats = _maison()
    # light.guirlande n'a pas d'aire : les autres (Chambre) décident.
    entrees = {"piece_1_tuiles": ["light.chevet", "light.guirlande", "fan.ventilo"]}
    assert _defs(_passage(entrees=entrees, etats=etats).definitions())[0] == ["p0", "Chambre"]
    entrees = {"piece_1_tuiles": ["light.chevet", "switch.prise_pc"]}
    assert _defs(_passage(entrees=entrees, etats=etats).definitions())[0] == ["p0", ""]
    # Une seule aire, mais sur un appareil de trois seulement : pas de nom (cas vu chez
    # l'auteur le 28/09 : la seule lampe rangée dans « Chambre » nommait tout l'accueil).
    entrees = {"piece_1_tuiles": ["light.chevet", "light.guirlande", "person.alice"]}
    assert _defs(_passage(entrees=entrees, etats=etats).definitions())[0] == ["p0", ""]


def test_nom_de_tuile_sans_la_piece():
    def nom(friendly, piece):
        etats = [Etat("light.x", "off", friendly_name=friendly, supported_color_modes=["onoff"])]
        defs = _defs(_passage(entrees={"piece_1_nom": piece, "piece_1_tuiles": ["light.x"]}, etats=etats).definitions())
        return defs[1][5]

    assert nom("Lampe du salon", "Salon") == "Lampe"
    assert nom("Salon - lampe", "Salon") == "lampe"
    assert nom("Plafonnier de l'entrée", "Entrée") == "Plafonnier"
    assert nom("Lampes", "Lampe") == "Lampes", "mot entier seulement"
    assert nom("Salon", "Salon") == "Salon", "rien ne reste : le nom de l'entité"
    assert nom("Lampe   salon  ", "salon") == "Lampe"
    assert nom("Lampe", "") == "Lampe"


def test_sans_piece_1_l_accueil_vient_des_entrees_3x():
    bp = _blueprint()
    entrees = {k: v for k, v in ENTREES.items() if k != "piece_1_tuiles"}
    entrees.update(lumiere_2="light.guirlande", lumiere_3="light.inexistante")
    p = _passage(entrees=entrees)
    tuiles = [t for t in p["tuiles"] if t["r"] == 0]
    assert [(t["cle"], t["e"], t["s"]) for t in tuiles] == [
        ("t00", "switch.prise_pc", "pc"),
        ("t01", "cover.volet_serre", "volet"),
        ("t02", "light.chevet", "lumiere_1"),
        ("t03", "light.guirlande", "lumiere_2"),
        # t04 : lumiere_3 n'existe pas, la place reste vide.
    ]
    defs = _defs(p.definitions())
    assert defs[:5] == [
        ["p0", ""],
        # Tuile PC du réglage 3.x : une icône hors palette retombe sur l'écran de la 3.1.
        ["t00", "int", "ordinateur", "k", "", "Prise PC"],
        ["t01", "vol", _icone(bp, domaine="cover", classe="curtain"), "", "", "Volet serre"],
        ["t02", "lum", _icone(bp, domaine="light"), "dc", "", "Lampe Chambre"],
        ["t03", "lum", _icone(bp, "mdi:led-strip-variant", "light"), "o", "", "Sapin / Noël, 2026"],
    ]
    # Sans PC, la TV prend la première place (option t : c'est la TV du blueprint).
    entrees.update(pc=[])
    tuiles = [t for t in _passage(entrees=entrees)["tuiles"] if t["r"] == 0]
    assert (tuiles[0]["cle"], tuiles[0]["e"], tuiles[0]["type"]) == ("t00", "media_player.tele", "med")
    # Une pièce 1 remplie : les entrées 3.x ne comptent plus pour les tuiles.
    tuiles = [t for t in _passage()["tuiles"] if t["r"] == 0]
    assert all(t["s"] == "" for t in tuiles)


def test_rien_de_choisi():
    p = _passage(entrees={})
    # Seuls les réglages de la rangée partent (leurs défauts, ceux du firmware).
    assert p["tuiles"] == [] and p["rangee"] == [] and p.definitions() == "hp|0;hd|32;"
    assert p["tuiles_a_pousser"] == []


# ─────────────────────────────────────────────────────────────────────────────
# Rendu : états et tuiles poussées (un seul chemin par tuile)
# ─────────────────────────────────────────────────────────────────────────────

def test_etats_apres_les_definitions():
    p = _passage(_evenement("connexion"))
    assert p["tuiles_a_pousser"] == [t["cle"] for t in p["tuiles"]]
    etats = {e[0]: e[1:] for e in _defs(p.etats_tuiles())}
    assert all(len(v) == 3 for v in etats.values()), "tRT|état|valeur|couleur"
    assert etats["t00"] == ["on", "128.0", "FFC864"]
    assert etats["t01"] == ["off", "nan", ""]
    assert etats["t02"] == ["on", "nan", ""]
    assert etats["t04"] == ["open", "45.0", ""]
    assert etats["t10"] == ["playing", "nan", ""]
    assert etats["t12"] == ["21.4", "21.4", ""]
    assert etats["t13"] == ["heat_cool", "22.5", ""]
    assert etats["t32"] == ["2026-09-27T20:00:00+00:00", "nan", ""]
    # Volet suivi par le package : l'état tenu par le package, pas celui du moteur.
    assert etats["t41"] == ["opening", "nan", ""]


def test_une_lampe_en_temperature_de_couleur_sans_rgb():
    etats = _maison()
    p = _passage(entrees={"piece_1_tuiles": ["light.bureau"]}, etats=etats)
    assert _defs(p.etats_tuiles()) == [["t00", "on", "255.0", ""]]


@pytest.mark.parametrize("protocole_version", ["3.1.0 (ESPHome 2026.9.0)", "3.2.0 (ESPHome 2026.9.0)"])
def test_les_definitions_partent_seulement_au_protocole_2(protocole_version):
    p = _passage(_evenement("rechargement"), tablettes=_tablette(protocole_version))
    assert p.conditions()
    # Calculées dans les deux cas (la trace les montre) ; l'envoi est gardé par protocole.
    assert p.definitions().startswith("p0|")


DEMARRAGE_HA = {"id": "demarrage_ha", "platform": "homeassistant", "event": "start"}


def _gardes_du_declencheur(noeud):
    """Les modèles des actions (`if:`, `conditions:` d'un choose) qui lisent trigger.id."""
    if isinstance(noeud, str):
        return [noeud] if "trigger.id" in noeud else []
    if isinstance(noeud, dict):
        return [g for v in noeud.values() for g in _gardes_du_declencheur(v)]
    if isinstance(noeud, list):
        return [g for v in noeud for g in _gardes_du_declencheur(v)]
    return []


def test_demarrage_de_ha_rejoue_la_connexion_perdue():
    """HA 2026.9.4 (28/09/2026) : la tablette s'est reconnectée avant que les
    automatisations soient actives, son tab5_connected était perdu et rien ne partait
    jusqu'au rechargement manuel. Le démarrage de HA fait ce qu'aurait fait la
    connexion : définitions, tous les états, clim, volet. Pas les zones : la tablette
    les demande avec la première poussée des prévisions (tab5-api-logic.yaml), qui
    vient d'une automatisation active."""
    bp = _blueprint()
    assert [t for t in bp["triggers"] if t.get("id") == "demarrage_ha"] == [
        {"trigger": "homeassistant", "event": "start", "id": "demarrage_ha"}]
    gardes = _gardes_du_declencheur(bp["actions"])
    assert len(gardes) >= 8, gardes
    for g in gardes + [bp["variables"]["cles"], bp["variables"]["tuiles_a_pousser"]]:
        if "'connexion'" in g:
            assert "'demarrage_ha'" in g, f"le démarrage de HA manque dans : {g}"
    # Si la tablette demandait ses zones à la connexion, la demande serait perdue elle
    # aussi, et le démarrage devrait y répondre comme un rechargement.
    api = _lire(os.path.join(REPO, "Tab5", "tab5-api-logic.yaml"))
    jours = api.split("- service: tab5_maj_previsions_jours_bulk", 1)[1].split("- service:", 1)[0]
    assert "id(tab5_zones_demande).execute()" in jours
    demarrage, connexion = _passage(DEMARRAGE_HA), _passage(_evenement("connexion"))
    assert demarrage.conditions()
    for nom in ("cles", "redefinir", "tuiles_a_pousser"):
        assert demarrage[nom] == connexion[nom], nom
    assert demarrage["tuiles_a_pousser"] == [t["cle"] for t in demarrage["tuiles"]]
    assert demarrage.definitions() == connexion.definitions() != ""
    assert demarrage.etats_tuiles() == connexion.etats_tuiles() != ""
    for p in (demarrage, connexion):
        p.variables_du_bloc("volet")
    assert [demarrage.modele(g) for g in gardes] == [connexion.modele(g) for g in gardes]


def test_demarrage_de_ha_sans_tablette_connectee_ne_pousse_rien():
    """Tablette pas encore reconnectée quand HA démarre : arrêt à la garde « tablette
    connectée » (sinon « Not connected ») ; son tab5_connected, émis plus tard, sera
    entendu, les automatisations étant alors actives."""
    hors_ligne = _tablette("3.2.0 (ESPHome 2026.9.0)", connectee=False)
    assert not _passage(DEMARRAGE_HA, tablettes=hors_ligne).conditions()


def _changement(entite, avant, apres, id_):
    return _declencheur(id_, avant, apres, entite)


def _maison_avec(*nouveaux):
    """La maison de test, dans l'état d'après le changement (ce que lit `states`)."""
    remplaces = {e.entity_id: e for e in nouveaux}
    return [remplaces.get(e.entity_id, e) for e in _maison()]


def test_un_changement_visible_pousse_sa_tuile_une_fois():
    base = {e.entity_id: e for e in _maison()}
    chevet = base["light.chevet"]
    eteinte = Etat("light.chevet", "off", "Chambre", **{**chevet.attributes, "brightness": None, "rgb_color": None})
    # Pièce 1 : le chevet est t00 ; le même dans la pièce 3 (t20) part par piece_3.
    p = _passage(_changement("light.chevet", chevet, eteinte, "piece_1"), etats=_maison_avec(eteinte))
    assert p["tuiles_a_pousser"] == ["t00"] and p.conditions()
    assert _defs(p.etats_tuiles()) == [["t00", "off", "nan", ""]]
    assert _passage(_changement("light.chevet", chevet, eteinte, "piece_3"))["tuiles_a_pousser"] == ["t20"]
    # Luminosité seule.
    plus = Etat("light.chevet", "on", "Chambre", **{**chevet.attributes, "brightness": 200})
    assert _passage(_changement("light.chevet", chevet, plus, "piece_1_luminosite"))["tuiles_a_pousser"] == ["t00"]
    # Couleur seule.
    rouge = Etat("light.chevet", "on", "Chambre", **{**chevet.attributes, "rgb_color": (255, 0, 0)})
    p = _passage(_changement("light.chevet", chevet, rouge, "piece_1_couleur"), etats=_maison_avec(rouge))
    assert _defs(p.etats_tuiles()) == [["t00", "on", "128.0", "FF0000"]]
    # Attribut invisible (température de couleur seule) : rien, et le passage s'arrête.
    tiede = Etat("light.chevet", "on", "Chambre", **{**chevet.attributes, "color_temp_kelvin": 3000})
    p = _passage(_changement("light.chevet", chevet, tiede, "piece_1"))
    assert p["tuiles_a_pousser"] == [] and not p.conditions()


def test_un_capteur_ne_part_qu_avec_les_mesures():
    base = {e.entity_id: e for e in _maison()}
    temp = base["sensor.temp_salon"]
    indispo = Etat("sensor.temp_salon", "unavailable", "Salon", **temp.attributes)
    p = _passage(_changement("sensor.temp_salon", temp, indispo, "piece_2"))
    assert p["tuiles_a_pousser"] == [] and not p.conditions()
    # Passage « mesures » : les capteurs changés depuis moins de 310 s, et les clims
    # dont un attribut (température de la pièce) a changé.
    etats = _maison()
    for e in etats:
        if e.entity_id in ("sensor.temp_salon", "climate.clim"):
            e.last_changed = e.last_updated = MAINTENANT - dt.timedelta(seconds=100)
    p = _passage({"id": "mesures", "platform": "time_pattern"}, etats=etats)
    assert sorted(p["tuiles_a_pousser"]) == ["t12", "t13"]
    # Rien de changé : rien.
    assert _passage({"id": "mesures", "platform": "time_pattern"})["tuiles_a_pousser"] == []


def test_chaque_type_a_son_chemin_de_poussee():
    """Un capteur (cap) ne part qu'avec les mesures lentes ; les autres types partent à
    chaque changement visible ; une clim part à son changement de mode et sa
    température de pièce avec les mesures."""
    variables = _blueprint()["variables"]
    types = set(variables["types_par_domaine"].values())
    immediats, mesures = set(variables["types_immediats"]), set(variables["types_mesures"])
    assert "cap" not in immediats and "cap" in mesures
    assert immediats | mesures == types
    assert immediats & mesures == {"cli"}


def test_une_entite_qui_apparait_redefinit_tout():
    nouvelle = Etat("light.chevet", "on", "Chambre", friendly_name="Lampe Chambre", supported_color_modes=["hs"])
    p = _passage(_declencheur("piece_1", None, nouvelle, "light.chevet"))
    assert p["redefinir"] is True and p.conditions()
    assert p["tuiles_a_pousser"] == [t["cle"] for t in p["tuiles"]]
    assert p.definitions().startswith("p0|")
    # Au protocole 1, rien.
    p = _passage(_declencheur("piece_1", None, nouvelle, "light.chevet"), tablettes=_tablette("3.1.0"))
    assert p["redefinir"] is False and not p.conditions()


def test_protocole_1_aucune_tuile_ne_reveille_l_automatisation():
    base = {e.entity_id: e for e in _maison()}
    chevet = base["light.chevet"]
    eteinte = Etat("light.chevet", "off", "Chambre", **chevet.attributes)
    p = _passage(_changement("light.chevet", chevet, eteinte, "piece_1"), tablettes=_tablette("3.1.0"))
    assert not p.conditions()


def test_volet_suivi_par_le_package():
    p = _passage(_declencheur("volet_suivi", Etat("input_text.volet_serre_etat", "Ferme"),
                              Etat("input_text.volet_serre_etat", "En_mouvement")))
    assert p["volet_suivi"] == "cover.volet_serre"
    assert p["tuiles_a_pousser"] == ["t41"]


def _liste_du_package(choix):
    """La maison de test, la liste « Tab5 · volet à course simulée » sur `choix`."""
    return _maison_avec(Etat("select.tab5_volet_a_course_simulee", choix))


def _etat_de_tuile(p, cle):
    return {e[0]: e[1:] for e in _defs(p.etats_tuiles())}[cle]


def test_package_sans_volet_choisi_ne_rend_muet_aucun_volet():
    """Discussion #278 (06/10/2026) : le package copié, sa liste laissée sur « Aucun ». Son
    script s'arrête alors sans rien commander : le blueprint ne doit pas lui confier le
    volet de l'entrée Volet (avant, la seule présence du script suffisait)."""
    etats = _liste_du_package("Aucun")
    for emplacement, action, attendu in (("t41", "ouvrir", ("cover.open_cover", "cover.volet_serre")),
                                         ("t41", "arreter", ("cover.stop_cover", "cover.volet_serre")),
                                         ("volet", "fermer", ("cover.close_cover", "cover.volet_serre"))):
        p = _passage(_evenement("action", emplacement=emplacement, action=action), etats=etats)
        assert p["volet_suivi"] == "" and p["volet_par_package"] is False
        p.variables_du_bloc("volet")  # l'entrée Volet, lue par la branche 3.x
        alias, sequence = p.aiguillage()
        assert alias is not None and _action_rendue(p, sequence) == attendu, (emplacement, action)
    # Son état est le vrai, plus celui que tient le package (« En_mouvement » ici).
    assert _etat_de_tuile(_passage(etats=etats), "t41")[0] == "unknown"


def test_le_package_suit_le_volet_de_sa_liste():
    """Liste sur un autre volet que l'entrée Volet : c'est lui que suit le package, et
    l'entrée Volet est commandée directement."""
    etats = _liste_du_package("cover.store")
    p = _passage(_evenement("action", emplacement="t04", action="ouvrir"), etats=etats)
    assert p["volet_suivi"] == "cover.store" and p["volet_par_package"] is False
    assert _action_rendue(p, p.aiguillage()[1]) == ("script.turn_on", "script.tab5_volet_action")
    p = _passage(_evenement("action", emplacement="volet", action="ouvrir"), etats=etats)
    p.variables_du_bloc("volet")
    assert _action_rendue(p, p.aiguillage()[1]) == ("cover.open_cover", "cover.volet_serre")
    # Package absent (pas de script) : la liste ne compte plus.
    sans_script = [e for e in etats if e.entity_id != "script.tab5_volet_action"]
    assert _passage(etats=sans_script)["volet_suivi"] == ""


def test_volet_choisi_dans_la_liste_repousse_ses_tuiles():
    """Le volet suivi dépend de la liste : la changer repousse les tuiles de l'ancien et
    du nouveau volet, chacune avec la bonne source d'état."""
    aucun = Etat("select.tab5_volet_a_course_simulee", "Aucun")
    serre = Etat("select.tab5_volet_a_course_simulee", "cover.volet_serre")
    store = Etat("select.tab5_volet_a_course_simulee", "cover.store")
    # Choisi : sa tuile prend l'état tenu par le package (« En_mouvement »).
    p = _passage(_declencheur("volet_choisi", aucun, serre))
    assert p["tuiles_a_pousser"] == ["t41"] and p.conditions()
    assert _etat_de_tuile(p, "t41")[0] == "opening"
    # Remis à « Aucun » : son état réel revient, tuile et entrée Volet (3.x).
    p = _passage(_declencheur("volet_choisi", serre, aucun), etats=_liste_du_package("Aucun"))
    assert p["tuiles_a_pousser"] == ["t41"] and p.conditions()
    assert _etat_de_tuile(p, "t41")[0] == "unknown"
    p.variables_du_bloc("volet")
    branche = _chercher(p.corps["actions"],
                        lambda d: d.get("alias") == "Volet qui signale sa course : son état a changé")
    assert p.modele(branche["conditions"])
    # D'un volet à l'autre : les tuiles des deux (le store est dans les pièces 1 et 2).
    p = _passage(_declencheur("volet_choisi", serre, store), etats=_liste_du_package("cover.store"))
    assert sorted(p["tuiles_a_pousser"]) == ["t04", "t11", "t41"]


def test_accueil_3x_les_declencheurs_3x_poussent_ses_tuiles():
    entrees = {k: v for k, v in ENTREES.items() if k != "piece_1_tuiles"}
    base = {e.entity_id: e for e in _maison()}
    chevet = base["light.chevet"]
    eteinte = Etat("light.chevet", "off", "Chambre", **chevet.attributes)
    p = _passage(_changement("light.chevet", chevet, eteinte, "lumiere_1"), entrees=entrees)
    assert p["cles"] == ["lumiere_1"] and p["tuiles_a_pousser"] == ["t02"]
    # Pièce 1 remplie : le déclencheur 3.x ne pousse que sa clé 3.x.
    p = _passage(_changement("light.chevet", chevet, eteinte, "lumiere_1"))
    assert p["cles"] == ["lumiere_1"] and p["tuiles_a_pousser"] == []


# ─────────────────────────────────────────────────────────────────────────────
# Rendu : commandes de l'écran
# ─────────────────────────────────────────────────────────────────────────────

def _commande(emplacement, action, valeur="", entrees=ENTREES):
    return _passage(_evenement("action", emplacement=emplacement, action=action, valeur=valeur), entrees=entrees)


def _action_rendue(p, sequence):
    etape = sequence[0]
    if "choose" in etape:  # volet : package ou entité
        for b in etape["choose"]:
            if p.modele(b["conditions"]):
                choisie = b["sequence"][0]
                cible = choisie.get("target", {}).get("entity_id")
                return p.modele(choisie["action"]), (p.modele(cible) if cible else None)
        etape = etape["default"][0]
    if "variables" in etape:
        for cle, valeur in etape["variables"].items():
            p.ctx[cle] = _rendre(p.env, valeur, p.ctx)
        return _action_rendue(p, sequence[1:])
    if "if" in etape:
        etape = etape["then"][0]
    cible = etape.get("target", {}).get("entity_id")
    return p.modele(etape["action"]), (p.modele(cible) if cible else None)


@pytest.mark.parametrize("emplacement, action, attendu", [
    ("t00", "basculer", ("light.toggle", "light.chevet")),
    ("t01", "eteindre", ("light.turn_off", "light.plafond")),
    ("t00", "luminosite", ("light.turn_on", "light.chevet")),
    ("t00", "couleur", ("light.turn_on", "light.chevet")),
    ("t04", "fermer", ("cover.close_cover", "cover.store")),
    ("t04", "arreter", ("cover.stop_cover", "cover.store")),
    ("t10", "basculer", ("media_player.toggle", "media_player.tele")),
    ("t22", "allumer", ("fan.turn_on", "fan.ventilo")),
    ("t32", "lancer", ("scene.turn_on", "scene.soiree")),
    ("t33", "lancer", ("script.turn_on", "script.cafe")),
    ("t34", "lancer", ("button.press", "button.sonnette")),
    # Volet suivi par le package : son script, lancé sans l'attendre (script.turn_on),
    # sinon « arrêter » attend la fin de la course.
    ("t41", "ouvrir", ("script.turn_on", "script.tab5_volet_action")),
    ("t41", "arreter", ("script.turn_on", "script.tab5_volet_action")),
    # Allumer seulement : basculer allume, éteindre ne fait rien.
    ("t02", "basculer", ("light.turn_on", "light.guirlande")),
    ("t02", "eteindre", None),
    # Lecture seule, commande hors type, tuile vide : rien.
    ("t40", "basculer", None),
    ("t31", "basculer", None),
    ("t12", "ouvrir", None),
    ("t44", "basculer", None),
    ("t0", "basculer", None),
])
def test_aiguillage_par_domaine(emplacement, action, attendu):
    p = _commande(emplacement, action, "128")
    alias, sequence = p.aiguillage()
    if attendu is None:
        assert alias is None, f"{emplacement}/{action} ne devrait rien commander ({alias})"
        return
    assert alias is not None, f"{emplacement}/{action} sans branche"
    assert _action_rendue(p, sequence) == attendu


def test_tuile_pc_de_l_accueil_3x_garde_pc_et_tv():
    entrees = {k: v for k, v in ENTREES.items() if k != "piece_1_tuiles"}
    alias, _ = _commande("t00", "basculer", entrees=entrees).aiguillage()
    assert alias.startswith("PC : la TV allumée s'éteint")
    # Le même interrupteur placé dans une pièce : une tuile comme une autre.
    alias, _ = _commande("t03", "basculer").aiguillage()
    assert alias.startswith("Tuile : basculer")
    # La clé 3.x `pc` garde sa branche.
    assert _commande("pc", "basculer").aiguillage()[0].startswith("PC : ")


def test_piece_tout_eteindre():
    p = _commande("p0", "eteindre")
    alias, sequence = p.aiguillage()
    assert alias.startswith("Pièce : tout éteindre")
    lampes = _rendre(p.env, sequence[0]["variables"]["lampes"], p.ctx)
    # La guirlande est « allumer seulement » : jamais éteinte depuis l'écran.
    assert lampes == ["light.chevet", "light.plafond"]
    p = _commande("p3", "eteindre")
    assert _rendre(p.env, p.aiguillage()[1][0]["variables"]["lampes"], p.ctx) == []


def _avec_comportement(entite, comportement, **autres):
    """ENTREES avec `entite` personnalisée en tête (la première ligne compte)."""
    return {**ENTREES, **autres,
            "personnalisation": [{"entite": entite, "comportement": comportement}] + ENTREES["personnalisation"]}


def _lampes(p):
    alias, sequence = p.aiguillage()
    return alias, _rendre(p.env, sequence[0]["variables"]["lampes"], p.ctx)


@pytest.mark.parametrize("comportement, attendu", [
    ("normal", ["light.chevet", "light.plafond"]),
    # Audit du 07/10/2026 (UI-2) : « Confirmer » n'est jamais contourné par un seul toucher
    # (ADR-0036), même par « Éteindre les lumières » de toute la pièce.
    ("confirmer", ["light.chevet"]),
    ("allumer_seulement", ["light.chevet"]),
    ("lecture_seule", ["light.chevet"]),
])
def test_piece_tout_eteindre_epargne_les_lampes_protegees(comportement, attendu):
    alias, lampes = _lampes(_commande("p0", "eteindre", entrees=_avec_comportement("light.plafond", comportement)))
    assert alias.startswith("Pièce : tout éteindre")
    assert lampes == attendu


@pytest.mark.parametrize("comportement, attendu", [
    ("normal", ["light.chevet", "light.plafond"]),
    ("confirmer", ["light.chevet"]),
    ("allumer_seulement", ["light.chevet"]),
    ("lecture_seule", ["light.chevet"]),
])
def test_tout_eteindre_des_lumieres_3x_epargne_les_lampes_protegees(comportement, attendu):
    """Mode héritage (lumieres / eteindre) : les mêmes lampes épargnées."""
    entrees = _avec_comportement("light.plafond", comportement, lumiere_2="light.plafond")
    alias, lampes = _lampes(_commande("lumieres", "eteindre", entrees=entrees))
    assert alias == "Tout éteindre"
    assert lampes == attendu



# ─────────────────────────────────────────────────────────────────────────────
# Popup du volet (05/10/2026, discussion #278) : « position » au relâcher du curseur
# ─────────────────────────────────────────────────────────────────────────────

def _position(emplacement, valeur, entrees=ENTREES, etats=None):
    """(action, entité, position) de la commande « position », ou None si rien ne part."""
    p = _passage(_evenement("action", emplacement=emplacement, action="position", valeur=valeur),
                 entrees=entrees, etats=etats)
    alias, sequence = p.aiguillage()
    if alias is None:
        return None
    assert alias.startswith("Tuile : position"), alias
    action, cible = _action_rendue(p, sequence)
    return action, cible, p.modele(sequence[0]["data"]["position"])


@pytest.mark.parametrize("valeur, attendu", [("45", 45), ("0", 0), ("100", 100), ("128", 100)])
def test_position_du_volet_bornee_et_sur_l_entite_de_la_tuile(valeur, attendu):
    assert _position("t04", valeur) == ("cover.set_cover_position", "cover.store", attendu)


@pytest.mark.parametrize("valeur", ["-5", "abc", "", "45.5", "1000", "4 5"])
def test_position_qui_n_est_pas_un_nombre_ne_ferme_rien(valeur):
    """Pas de « 0 » par défaut : une valeur illisible fermerait le volet."""
    assert _position("t04", valeur) is None


def test_position_d_une_vanne():
    etats = [e for e in _maison() if e.entity_id != "valve.arrosage"]
    etats.append(Etat("valve.arrosage", "open", friendly_name="Arrosage", current_position=30, supported_features=7))
    assert _position("t00", "60", entrees={"piece_1_tuiles": ["valve.arrosage"]}, etats=etats) == (
        "valve.set_valve_position", "valve.arrosage", 60)


def test_position_jamais_hors_de_la_liste_blanche():
    # Une lumière, un capteur, une tuile vide, une clé 3.x, un emplacement inventé : rien.
    for emplacement in ("t00", "t12", "t44", "volet", "t9", "cover.store"):
        assert _position(emplacement, "50") is None, emplacement
    # Le volet à course simulée du package : pas de position réglable.
    assert _position("t41", "50") is None
    # Un store sans SET_POSITION (ouvrir, fermer, arrêter seulement) : rien, plutôt
    # qu'une erreur « does not support this service » qui arrêterait l'automatisation.
    etats = [e for e in _maison() if e.entity_id != "cover.store"]
    etats.append(Etat("cover.store", "open", "Salon", friendly_name="Store salon", supported_features=11))
    assert _position("t04", "50", etats=etats) is None
    # Lecture seule : rien (la tablette n'envoie déjà rien, ceci en est la garde côté HA).
    perso = ENTREES["personnalisation"] + [{"entite": "cover.store", "comportement": "lecture_seule"}]
    assert _position("t04", "50", entrees={**ENTREES, "personnalisation": perso}) is None


def test_position_derriere_la_garde_d_origine():
    """La branche est dans « Commande d'un bouton de l'écran », que seule une tablette
    déclenche (trigger action, garde du modèle tab5-ha-hmi)."""
    bp = _blueprint()
    garde = bp["conditions"][0]["value_template"]
    assert "'action'" in garde and "device_attr(d, 'model') == 'tab5-ha-hmi'" in garde
    commande = _chercher(bp["actions"], lambda d: d.get("alias") == "Commande d'un bouton de l'écran")
    assert commande and "trigger.id == 'action'" in commande["conditions"]
    assert _chercher(commande, lambda d: (d.get("alias") or "").startswith("Tuile : position"))


def test_volet_simule_sans_position_hors_de_la_course():
    """Le volet du package n'a pas de vraie position : nan au bout de la course (la
    tablette ne lui donne pas de curseur ; sa flèche repart dans l'autre sens comme à
    100 / 0), -1 pour « Partiel » (arrêté en route, le sens choisi reste)."""
    for etat_package, attendu in (("Ouvert", ["open", "nan", ""]), ("Ferme", ["closed", "nan", ""]),
                                  ("Partiel", ["open", "-1", ""]), ("En_mouvement", ["opening", "nan", ""])):
        etats = [e for e in _maison() if e.entity_id != "input_text.volet_serre_etat"]
        etats.append(Etat("input_text.volet_serre_etat", etat_package))
        p = _passage(_evenement("connexion"), etats=etats)
        assert {e[0]: e[1:] for e in _defs(p.etats_tuiles())}["t41"] == attendu, etat_package
