# -*- coding: utf-8 -*-
"""Popup Caméras (ADR-0049, ADR-0056), côté Home Assistant : la réponse du blueprint
« Tab5 — emplacements » à l'événement esphome.tab5_cameras, rendue avec le VRAI modèle
Jinja dans le bac à sable de tests/test_tuiles_blueprint.py (fausses entités, aire de
chaque caméra, état « unavailable »).

Le format « nom|image|pièce|depuis » est lu par cameras_lire() (Tab5/socle/tab5_parse.cpp,
cas dans tools/test_parse.cpp) ; un firmware d'avant lit « nom|image » et ignore la suite,
ce que vérifie test_lisible_par_un_firmware_d_avant."""
import datetime as dt

from tests.test_tuiles_blueprint import MAINTENANT, Etat, Passage, _chercher, _evenement

ALIAS = "La tablette demande ses caméras"


def _reponse(etats, cameras, adresse=""):
    """Les données de l'action tab5_maj_cameras, rendues comme HA les enverrait."""
    p = Passage({"cameras_liste": cameras, "cameras_adresse": adresse}, etats, _evenement("cameras"))
    # as_timestamp de HA : un datetime → secondes Unix (float), `defaut` sinon.
    p.env.globals["as_timestamp"] = lambda v, defaut=None: v.timestamp() if isinstance(v, dt.datetime) else defaut
    p.env.tests["list"] = lambda v: isinstance(v, list)   # test « is list » de HA
    branche = _chercher(p.corps["actions"], lambda d: d.get("alias") == ALIAS)
    assert branche, f"branche « {ALIAS} » introuvable"
    action = branche["sequence"][0]
    assert action["action"].endswith("_tab5_maj_cameras")
    return {k: str(p.modele(v)) for k, v in action["data"].items()}


def _cam(eid, nom, aire=None, image=True, etat="idle", age=3600):
    attrs = {"friendly_name": nom}
    if image:
        attrs["entity_picture"] = f"/api/camera_proxy/{eid}?token=t"
    return Etat(eid, etat, aire=aire, age=age, **attrs)


def test_piece_et_hors_ligne():
    etats = [
        _cam("camera.entree", "Entrée", aire="Entrée"),
        _cam("camera.jardin", "Jardin", aire="Jardin"),
        _cam("camera.cour", "Cour"),                                       # sans pièce
        _cam("camera.portail", "Portail", aire="Jardin", image=False, etat="unavailable", age=600),
        _cam("camera.vieille", "Vieille", aire="Jardin", image=False),     # ni image ni hors ligne
    ]
    r = _reponse(etats, [e.entity_id for e in etats], adresse="https://ha.example.com")
    depuis = int((MAINTENANT - dt.timedelta(seconds=600)).timestamp())
    assert r["adresse"] == "https://ha.example.com"
    assert r["cameras"].split(";") == [
        "Entrée|/api/camera_proxy/camera.entree?token=t|Entrée|",
        "Jardin|/api/camera_proxy/camera.jardin?token=t|Jardin|",
        "Cour|/api/camera_proxy/camera.cour?token=t||",
        f"Portail||Jardin|{depuis}",
    ]


def test_indisponible_avec_image_garde_son_image():
    """HA peut garder entity_picture sur une caméra indisponible : l'image part aussi (un
    firmware d'avant essaie comme avant) ; le nouveau voit « depuis » et ne la demande pas."""
    etats = [_cam("camera.a", "A", aire="Salon", etat="unavailable", age=60)]
    r = _reponse(etats, ["camera.a"])
    depuis = int((MAINTENANT - dt.timedelta(seconds=60)).timestamp())
    assert r["cameras"] == f"A|/api/camera_proxy/camera.a?token=t|Salon|{depuis}"


def test_separateurs_retires_des_noms_et_des_pieces():
    etats = [_cam("camera.a", "Porte | côté; rue", aire="Rez | jardin; sud")]
    assert _reponse(etats, ["camera.a"])["cameras"] == \
        "Porte / côté, rue|/api/camera_proxy/camera.a?token=t|Rez / jardin, sud|"


def test_seize_au_plus():
    etats = [_cam(f"camera.c{i}", f"C{i}", aire=f"P{i % 3}") for i in range(20)]
    enregistrements = _reponse(etats, [e.entity_id for e in etats])["cameras"].split(";")
    assert len(enregistrements) == 16
    assert enregistrements[-1].startswith("C15|")


def test_section_vide_reponse_vide():
    assert _reponse([], [])["cameras"] == ""


def test_lisible_par_un_firmware_d_avant():
    """Un firmware 1.2.0 d'avant ce lot lisait « nom|image » (champ_suivant deux fois) et
    sautait un enregistrement sans image : les deux premiers champs suffisent."""
    etats = [_cam("camera.a", "A", aire="Salon"),
             _cam("camera.b", "B", aire="Salon", image=False, etat="unavailable")]
    lus = []
    for e in _reponse(etats, ["camera.a", "camera.b"])["cameras"].split(";"):
        nom, image = (e.split("|") + ["", ""])[:2]
        if image:
            lus.append((nom, image))
    assert lus == [("A", "/api/camera_proxy/camera.a?token=t")]
