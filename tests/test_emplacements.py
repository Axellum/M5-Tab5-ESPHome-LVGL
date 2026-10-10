# -*- coding: utf-8 -*-
"""Emplacements (lot 6a, ADR-0019) : la tablette ne connaît plus d'entité, le blueprint
« Tab5 — emplacements » fait le lien. Le contrat tient en des chaînes qu'aucun
compilateur ne compare :

- les clés poussées par le blueprint (`cles_emplacements`, plus les 4 détails de chaque
  pot) doivent être exactement celles de la table de `tab5_maj_emplacements`
  (Tab5/paquets/tab5-api-logic.yaml) : une clé inconnue est ignorée en silence ;
- chaque commande que l'écran envoie (`script.execute: tab5_action`, avec son
  emplacement et sa commande) doit avoir une branche dans le blueprint, sinon le
  bouton ne fait rien ;
- le blueprint se lit, tous ses emplacements sont facultatifs, et le firmware ne
  s'abonne plus à aucune entité de la maison.

Les pièces et tuiles (ADR-0023 : tab5_maj_tuiles, clés tRT/pR) sont vérifiées par
tests/test_tuiles_blueprint.py, qui rend les modèles du blueprint."""
import os
import re

import yaml
from tests.commun import ChargeurEntrees as _Chargeur, bloc_service, lire as _lire, source, sources

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")


def _blueprint():
    return yaml.load(_lire(BLUEPRINT), Loader=_Chargeur)


def _cles_firmware():
    bloc = bloc_service("tab5_maj_emplacements").split("emplacements_appliquer", 1)[0]
    return re.findall(r'\{"(\w+)", ', bloc)


def _commandes_firmware():
    """(emplacement, commande) de chaque script.execute tab5_action du firmware."""
    paires = set()
    fichiers = []
    fichiers += [str(p) for p in sources("*.yaml")]
    dossier = os.path.join(REPO, "Tab5", "ui_components")
    fichiers += [os.path.join(dossier, n) for n in os.listdir(dossier) if n.endswith(".yaml")]
    for chemin in fichiers:
        texte = _lire(chemin)
        # Forme en ligne : { id: tab5_action, emplacement: X, commande: Y, … }
        for emp, cmd in re.findall(r"id: tab5_action, emplacement: ([^,]+), commande: (\w+)", texte):
            paires.add((emp.strip(), cmd))
        # Forme en bloc : id / emplacement / commande sur trois lignes.
        for emp, cmd in re.findall(r"id: tab5_action\n\s+emplacement: (.+)\n\s+commande: (\w+)", texte):
            paires.add((emp.strip(), cmd))
    return paires


def test_le_blueprint_se_lit_et_tout_est_facultatif():
    bp = _blueprint()
    assert bp["blueprint"]["domain"] == "automation"
    for section in bp["blueprint"]["input"].values():
        for nom, entree in section["input"].items():
            assert "default" in entree, f"entrée {nom} obligatoire"
            if nom == "tablette":
                continue
            # Réglages de la rangée sous l'horloge (ADR-0031) : non remplis, ceux du
            # firmware (plantes en premier, 32 s ; tests/test_rangee.py compare).
            # Pareil pour le panneau Ok Nabu et le défilement au choix (lot 3, ADR-0041 ;
            # tests/test_nabu.py) : l'écran d'avant le choix. Zone à gauche de l'horloge
            # (ADR-0051, tests/test_zone_gauche.py) : le vocal au départ, comme avant.
            defauts = {"rangee_plantes": "0", "rangee_duree": 32, "nabu_ecoute": "0", "nabu_duree": 32,
                       "rangee_defilement": "auto", "nabu_defilement": "fixe",
                       "reglables_defilement": "fixe", "reglables_duree": 32,
                       "gauche_depart": "vocal", "gauche_cycle": ["vocal", "graphique", "lecteur", "capteur"]}
            if nom in defauts:
                assert entree["default"] == defauts[nom], nom
                continue
            # Appuis longs des boutons du haut (07/10/2026) : non remplis, ceux d'avant le
            # choix (tests/test_appuis.py).
            # Gestes de l'horloge et des taps (09/10/2026, tests/test_gestes.py) : pareil.
            if nom.startswith(("appui_", "geste_")):
                assert entree["default"] == "auto", f"{nom} : non rempli doit valoir \"auto\""
                continue
            if "text" in entree["selector"]:  # nom d'une pièce (ADR-0023)
                assert entree["default"] == "", f"{nom} : un nom vide doit valoir \"\""
            elif "select" in entree["selector"]:  # source météo : « liste » = celle de HA
                assert entree["default"] == "liste", f"{nom} : non rempli doit valoir \"liste\""
            elif "boolean" in entree["selector"]:  # Énergie : sens du réseau, de la batterie
                assert entree["default"] is False, f"{nom} : une case non cochée doit valoir false"
            elif "number" in entree["selector"]:  # Énergie : puissance crête, 0 = pas d'icône solaire
                assert entree["default"] == 0, f"{nom} : un nombre non rempli doit valoir 0"
            else:
                assert entree["default"] == [], f"{nom} : un emplacement vide doit valoir []"


def test_cles_poussees_egales_a_la_table_du_firmware():
    bp = _blueprint()
    cles = []
    for c in bp["variables"]["cles_emplacements"]:
        cles.append(c)
        if c.startswith("pot_"):
            cles += [f"{c}_{d}" for d in ("ec", "lux", "temp", "bat")]
    assert sorted(cles) == sorted(_cles_firmware())


def test_chaque_commande_de_l_ecran_a_sa_branche():
    texte = _lire(BLUEPRINT)
    paires = _commandes_firmware()
    assert len(paires) >= 15, f"trop peu de commandes trouvées : {sorted(paires)}"
    for emp, cmd in paires:
        assert f"commande == '{cmd}'" in texte or f"commande in [" in texte and f"'{cmd}'" in texte, \
            f"commande {cmd!r} ({emp}) sans branche dans le blueprint"
        if "current_light_slot" in emp:
            assert "emplacement.startswith('lumiere_')" in texte
        elif "clim_affichee_cle()" in emp or "clim_tuile_attente_cle()" in emp:
            # Popup clim (ADR-0027) : « clim » ou la tuile tRT d'une clim, traduites par les
            # mêmes branches « Clim : … » (tests/test_clim.py).
            assert f"clim_commande == '{cmd}'" in texte, f"commande {cmd!r} du popup clim sans branche"
        elif "telecommande_cle()" in emp:
            # Popup télécommande (ADR-0056) : « tv », « tv1 »…, résolus en rang par les
            # branches « Télécommande : … » (tests/test_telecommandes.py).
            assert f"tc_rang >= 0 and commande == '{cmd}'" in texte, \
                f"commande {cmd!r} de la télécommande sans branche dans le blueprint"
        elif emp.startswith("!lambda"):
            # Clé calculée à l'exécution : une tuile tRT ou une pièce pR (ADR-0023),
            # aiguillée par le domaine de l'entité de la tuile (tests/test_tuiles_blueprint.py).
            assert re.search(rf"t_commande (== |in \[[^\]]*)'{cmd}'", texte) or (
                cmd == "eteindre" and "emplacement is match('^p[0-4]$')" in texte), \
                f"commande {cmd!r} d'une tuile ({emp}) sans branche dans le blueprint"
        else:
            assert f"emplacement == '{emp}'" in texte or (
                emp.startswith("lumiere_") and "emplacement.startswith('lumiere_')" in texte), \
                f"emplacement {emp!r} ({cmd}) sans branche dans le blueprint"


def test_plus_aucun_abonnement_a_une_entite_de_la_maison():
    for nom in ("tab5-sensors-domotique.yaml", "pot_sensors.yaml"):
        assert "platform: homeassistant\n" not in _lire(source(nom)), nom


def test_chaque_emplacement_a_un_seul_chemin_de_poussee():
    """Mesures groupées (28/09/2026) : un emplacement part soit à son déclencheur d'état
    (lumières, PC, TV), soit au passage « mesures » toutes les 5 min (températures,
    humidité, pots, batterie) ; jamais les deux, jamais aucun."""
    bp = _blueprint()
    variables = bp["variables"]
    etats = {t["id"] for t in bp["triggers"] if t["trigger"] == "state"}
    mesures = set(variables["cles_mesures"])
    emplacements = set(variables["cles_emplacements"])
    assert not etats & mesures, f"poussées deux fois : {sorted(etats & mesures)}"
    assert emplacements <= etats | mesures, f"jamais poussées : {sorted(emplacements - etats - mesures)}"
    assert mesures <= emplacements
    passage = [t for t in bp["triggers"] if t.get("id") == "mesures"]
    assert len(passage) == 1 and passage[0]["trigger"] == "time_pattern"
    periode = int(passage[0]["minutes"].lstrip("/")) * 60
    # La fenêtre couvre la période (rien de perdu entre deux passages), sans en couvrir deux.
    assert periode < variables["fenetre_mesures"] < 2 * periode
