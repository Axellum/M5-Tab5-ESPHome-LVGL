# -*- coding: utf-8 -*-
"""Tableau de bord Tab5 mis à jour en un clic (ADR-0075) : le marqueur de la macro et la logique pure
de custom_components/tab5/tableau.py.

Le test bout à bout — un vrai Home Assistant, un tableau créé par le websocket, une réparation
validée, le tableau relu — est tools/installation_ha/verifier_integration.py (CI integration-hacs.yml,
étapes 1 ter et 2 bis). Ici, sans Home Assistant : la VRAIE macro est rendue dans le bac à sable de Jinja avec la
fausse maison de tests/test_tableau_de_bord.py, et son marqueur est relu par le module de
l'intégration, pour que les deux ne divergent pas."""
import datetime as dt
import importlib.util
import json
import sys
from pathlib import Path

import pytest
import yaml

from tests.test_tableau_de_bord import APPEL_ECRANS, _deux_ecrans, _maison, _rendre, _tablette

INTEGRATION = Path(__file__).resolve().parent.parent / "custom_components" / "tab5"


def _module(nom: str):
    """Un module pur de l'intégration, chargé par son chemin (son __init__.py importe HA)."""
    spec = importlib.util.spec_from_file_location(f"tab5_{nom}", INTEGRATION / f"{nom}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module  # @dataclass relit son module dans sys.modules
    spec.loader.exec_module(module)
    return module


tableau = _module("tableau")
Marqueur, TableauRefuse = tableau.Marqueur, tableau.TableauRefuse

CAPTEUR_VERSION = "sensor.tab5_version_des_fichiers_ha"


def _maison_en(version: str, tablettes=None):
    maison = _maison(tablettes or _tablette())
    maison[0].d[CAPTEUR_VERSION].state = version
    return maison


# ─── Le marqueur écrit par la vraie macro ────────────────────────────────────

def test_la_macro_ecrit_son_marqueur_a_la_racine():
    rendu = _rendre(_maison_en("3.9.1"))
    tableau_ha = yaml.safe_load(rendu)
    assert list(tableau_ha)[:2] == [tableau.CLE_MARQUEUR, "views"], "le marqueur précède les vues"
    marqueur = tableau.lire_marqueur(tableau_ha)
    assert marqueur == Marqueur("3.9.1", "tab5_dashboard", {
        "adresse": "dashboard-tab5", "langue": "", "appareil": "", "chemin": "tab5", "titre": ""})


@pytest.mark.parametrize("etat", ["dépôt", "unknown", "unavailable", "on", ""])
def test_sans_numero_de_version_le_marqueur_la_laisse_vide(etat):
    """Chez l'auteur le capteur vaut « dépôt » : pas de version inventée."""
    marqueur = tableau.lire_marqueur(yaml.safe_load(_rendre(_maison_en(etat))))
    assert marqueur is not None and marqueur.version == ""


def test_les_arguments_de_l_appel_sont_gardes_et_rejoues_a_l_identique():
    """Le point central : le marqueur contient de quoi refaire EXACTEMENT le même appel."""
    maison = _maison_en("3.9.1")
    rendu = _rendre(maison, langue="English", adresse="dashboard-salon", titre="Salon", chemin="salon")
    marqueur = tableau.lire_marqueur(yaml.safe_load(rendu))
    assert marqueur.args == {"adresse": "dashboard-salon", "langue": "English", "appareil": "",
                             "chemin": "salon", "titre": "Salon"}
    source, variables = tableau.source_modele(marqueur)
    assert _rendre(maison, appel=source, **variables) == rendu


def test_un_ecran_ou_plusieurs_la_macro_ecrans_se_rejoue_aussi():
    for tablettes, nombre in ((_tablette(), 3), (_deux_ecrans(), 6)):
        maison = _maison_en("3.9.1", tablettes)
        rendu = _rendre(maison, appel=APPEL_ECRANS)
        marqueur = tableau.lire_marqueur(yaml.safe_load(rendu))
        assert marqueur.macro == "tab5_dashboard_ecrans" and marqueur.args == {"adresse": "dashboard-tab5", "langue": ""}
        assert len(yaml.safe_load(rendu)["views"]) == nombre
        source, variables = tableau.source_modele(marqueur)
        assert _rendre(maison, appel=source, **variables) == rendu


def test_les_vues_d_un_ecran_d_un_jeu_multiple_n_ont_pas_de_marqueur_a_elles():
    """vues_seules : les vues d'un écran sont des morceaux du tableau d'un autre appel, pas un
    tableau (sinon la clé serait écrite au milieu des vues)."""
    rendu = _rendre(_maison_en("3.9.1", _deux_ecrans()), appel=APPEL_ECRANS)
    assert rendu.count("tab5_dashboard:") == 1


def test_sans_tablette_aucun_marqueur_ni_tableau():
    rendu = _rendre(_maison({}))
    assert rendu.startswith("# Tablette introuvable") and tableau.lire_marqueur(yaml.safe_load(rendu)) is None


def test_le_marqueur_resiste_a_une_valeur_a_caracteres_speciaux():
    """Guillemets, deux-points, accents, <, > et & dans un argument : du JSON, donc du YAML valide,
    relu tel quel. (Le titre d'une vue, lui, est écrit entre guillemets par la macro : un « " » y
    casserait le tableau, limite connue de @ai_warning, sans rapport avec le marqueur.)"""
    titre = "Salon \"A\": l'étage <1> & co é \\ fin"
    appel = ("{% from 'tab5_dashboard.jinja' import tab5_marqueur %}"
             "{{ tab5_marqueur('tab5_dashboard', 'dashboard-tab5', '', '', 'tab5', titre) }}")
    ligne = _rendre(_maison_en("3.9.1"), appel=appel, titre=titre)
    marqueur = tableau.lire_marqueur(yaml.safe_load(ligne))
    assert marqueur is not None and marqueur.args["titre"] == titre and len(ligne.splitlines()) == 1


def test_la_macro_de_remplacement_de_la_ci_prend_le_pas_et_garde_le_vrai_marqueur(monkeypatch):
    """Le scénario de la CI (verifier_integration.py, étapes 1 ter et 2 bis) ajoute à la fin du modèle une
    macro tab5_dashboard() de REMPLACEMENT, faute de tablette dans son HA. Hors HA, ici : elle prend le pas
    sur la vraie, y compris pour tab5_dashboard_ecrans() qui l'appelle, écrit le vrai marqueur, se rejoue à
    l'identique par ce qu'en lit l'intégration, et sa carte dit la version des fichiers livrés."""
    import verifier_integration as ci
    from tests import test_tableau_de_bord as base
    reel = base._texte()
    monkeypatch.setattr(base, "_texte", lambda: reel + ci.surcharge_tableau("9.9.1"))
    maison = _maison({})  # pas de tablette, comme dans le HA du scénario
    maison[0].d[CAPTEUR_VERSION].state = "9.9.1"
    for adresse, appel in ci.TABLEAUX.items():
        rendu = _rendre(maison, appel=appel)
        config = yaml.safe_load(rendu)
        marqueur = tableau.lire_marqueur(config)
        assert marqueur is not None and marqueur.version == "9.9.1" and marqueur.args["adresse"] == adresse
        assert "modele de test 9.9.1" in json.dumps(config), "la macro de remplacement, pas la vraie"
        source, variables = tableau.source_modele(marqueur)
        assert _rendre(maison, appel=source, **variables) == rendu
    ecrans = tableau.lire_marqueur(yaml.safe_load(_rendre(maison, appel=ci.TABLEAUX["dashboard-ecrans"])))
    assert ecrans.macro == "tab5_dashboard_ecrans"


def test_la_macro_de_remplacement_est_posee_a_la_fin_du_modele_de_chaque_zip(tmp_path):
    import verifier_integration as ci
    fichier = tmp_path / "custom_templates" / "tab5_dashboard.jinja"
    fichier.parent.mkdir()
    fichier.write_bytes("{# modele #}\n".encode("utf-8"))
    ci.poser_modele_de_test(tmp_path, "9.9.2")
    texte = fichier.read_text(encoding="utf-8")
    assert texte.startswith("{# modele #}\n") and texte.endswith(ci.surcharge_tableau("9.9.2"))
    assert "@version@" not in texte and "modele de test 9.9.2" in texte


# ─── Lire un marqueur ────────────────────────────────────────────────────────

def test_lire_marqueur_refuse_ce_qui_n_est_pas_celui_de_la_macro():
    assert tableau.lire_marqueur(None) is None
    assert tableau.lire_marqueur({"views": []}) is None
    assert tableau.lire_marqueur({tableau.CLE_MARQUEUR: "3.9.1"}) is None
    assert tableau.lire_marqueur({tableau.CLE_MARQUEUR: {"macro": "autre", "version": "3.9.1"}}) is None
    assert tableau.lire_marqueur({tableau.CLE_MARQUEUR: {"macro": "__import__('os')"}}) is None


def test_lire_marqueur_ignore_les_cles_hors_liste_et_les_valeurs_hors_texte():
    brut = {tableau.CLE_MARQUEUR: {"macro": "tab5_dashboard_ecrans", "version": 3.9, "adresse": "dashboard-x",
                                   "langue": 7, "appareil": "ne doit pas passer", "inconnue": "x"}}
    assert tableau.lire_marqueur(brut) == Marqueur("", "tab5_dashboard_ecrans", {"adresse": "dashboard-x"})


# ─── Versions ────────────────────────────────────────────────────────────────

@pytest.mark.parametrize("plus_ancienne, plus_recente", [
    ("3.9.0", "3.9.1"), ("3.9.1", "3.10.0"), ("3.9.9", "4.0.0"), ("3.9.0-rc.7", "3.9.0-rc.8"),
    ("3.9.0-rc.8", "3.9.0"), ("3.9.0-beta.2", "3.9.0-rc.1"), ("9.9.1", "9.9.2"), ("v3.8", "3.9.0")])
def test_ordre_des_versions(plus_ancienne, plus_recente):
    assert tableau.cle_version(plus_ancienne) < tableau.cle_version(plus_recente)


@pytest.mark.parametrize("texte", ["dépôt", "unknown", "unavailable", "", None, "3", "abc.def"])
def test_pas_une_version(texte):
    assert tableau.cle_version(texte) is None


def test_en_retard():
    m = lambda v: Marqueur(v, "tab5_dashboard")  # noqa: E731
    assert tableau.en_retard(m("3.9.0"), "3.9.1")
    assert not tableau.en_retard(m("3.9.1"), "3.9.1")
    assert not tableau.en_retard(m("3.9.2"), "3.9.1"), "un tableau plus récent que les fichiers n'est pas à mettre à jour"
    assert tableau.en_retard(m(""), "3.9.1"), "sans version lisible : un rendu l'écrit à jour"
    assert not tableau.en_retard(m(""), "dépôt"), "fichiers sans version (auteur) : jamais proposé"
    assert not tableau.en_retard(m("3.9.0"), "")


# ─── L'appel de la macro ─────────────────────────────────────────────────────

def test_source_modele_ne_met_aucune_valeur_dans_le_texte():
    marqueur = Marqueur("3.9.0", "tab5_dashboard", {"adresse": "x'}}{{ system }}", "titre": "{% raw %}"})
    source, variables = tableau.source_modele(marqueur)
    assert "system" not in source and "raw" not in source
    assert source == ("{% from 'tab5_dashboard.jinja' import tab5_dashboard %}"
                      "{{ tab5_dashboard(adresse=args['adresse'], titre=args['titre']) }}")
    assert variables == {"args": marqueur.args}


def test_source_modele_refuse_une_macro_inconnue():
    with pytest.raises(ValueError):
        tableau.source_modele(Marqueur("3.9.0", "autre"))


# ─── Refuser un rendu avant d'écraser ────────────────────────────────────────

def _rendu_valide(version="3.9.1", macro="tab5_dashboard"):
    return {tableau.CLE_MARQUEUR: {"macro": macro, "version": version}, "views": [{"title": "Tab5"}]}


def test_un_rendu_valide_passe():
    tableau.valider_rendu(_rendu_valide(), "3.9.1", Marqueur("3.9.0", "tab5_dashboard"))


@pytest.mark.parametrize("rendu, code", [
    (None, tableau.ERREUR_RENDU),                                  # « # Tablette introuvable » : YAML vide
    ("# texte", tableau.ERREUR_RENDU),
    ({"views": []}, tableau.ERREUR_RENDU),
    ({"views": [{}]}, tableau.ERREUR_RENDU),                       # un modèle d'avant le marqueur
    (_rendu_valide(macro="tab5_dashboard_ecrans"), tableau.ERREUR_RENDU),
    (_rendu_valide(version="3.9.0"), tableau.ERREUR_VERSION),      # le capteur n'a pas suivi
    (_rendu_valide(version=""), tableau.ERREUR_VERSION),
])
def test_un_rendu_inutilisable_est_refuse(rendu, code):
    with pytest.raises(TableauRefuse) as erreur:
        tableau.valider_rendu(rendu, "3.9.1", Marqueur("3.9.0", "tab5_dashboard"))
    assert erreur.value.code == code


# ─── Sauvegarde de l'ancien tableau ──────────────────────────────────────────

MAINTENANT = dt.datetime(2026, 11, 9, 15, 30, 12)


def test_sauvegarde_du_tableau(tmp_path):
    ancien = {"title": "Tab5", "views": [{"title": "Accueil é"}], tableau.CLE_MARQUEUR: {"macro": "tab5_dashboard"}}
    fichier = tableau.sauvegarder(tmp_path, "dashboard-tab5", ancien, "3.9.0", MAINTENANT)
    assert fichier == tmp_path / "tab5_sauvegardes" / "tableaux" / "20261109-153012_dashboard-tab5_3.9.0.json"
    assert json.loads(fichier.read_text(encoding="utf-8")) == ancien
    assert "é" in fichier.read_text(encoding="utf-8"), "lisible tel quel"
    assert [p.name for p in fichier.parent.iterdir()] == [fichier.name], "aucun fichier temporaire laissé"


def test_deux_sauvegardes_dans_la_meme_seconde_ne_s_ecrasent_pas(tmp_path):
    a = tableau.sauvegarder(tmp_path, "dashboard-tab5", {"n": 1}, "3.9.0", MAINTENANT)
    b = tableau.sauvegarder(tmp_path, "dashboard-tab5", {"n": 2}, "3.9.0", MAINTENANT)
    assert a != b and json.loads(a.read_text(encoding="utf-8")) == {"n": 1}
    assert json.loads(b.read_text(encoding="utf-8")) == {"n": 2}


def test_les_plus_anciennes_sauvegardes_partent_mais_pas_celles_d_un_autre_tableau(tmp_path):
    autre = tableau.sauvegarder(tmp_path, "dashboard-autre", {"x": 1}, "3.9.0", MAINTENANT)
    gardees = []
    for n in range(tableau.GARDER + 3):
        gardees.append(tableau.sauvegarder(tmp_path, "dashboard-tab5", {"n": n}, "3.9.0",
                                           MAINTENANT + dt.timedelta(seconds=n)))
    restantes = {p.name for p in (tmp_path / tableau.SAUVEGARDES).iterdir()}
    assert restantes == {autre.name} | {p.name for p in gardees[-tableau.GARDER:]}


def test_tableau_par_defaut_et_version_inconnue(tmp_path):
    fichier = tableau.sauvegarder(tmp_path, None, {}, "", MAINTENANT)
    assert fichier.name == "20261109-153012_defaut_inconnue.json"
