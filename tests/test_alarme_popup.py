# -*- coding: utf-8 -*-
"""Popup du réveil, barre du bas : la ligne « prochain RDV » ne passe plus sous le bouton
« Tester » (audit des conteneurs du 01/10/2026).

Le label faisait 500 px de large alors que le bouton commence à 401 px : un titre de
rendez-vous de plus de ~30 caractères passait sous ce bouton translucide. Le label n'a
plus de largeur (une ligne) et alarm_render.cpp coupe le texte avec « … » à
kLargeurRdvSuivant px. Ce test refait le calcul depuis le YAML : déplacer le bouton ou
élargir la barre sans revoir la constante le fait échouer.
"""
import os
import re

import yaml

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
AIR_MIN = 8  # px entre la fin du texte et le bouton


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda *_: None)


def _lire(*chemin):
    with open(os.path.join(REPO, *chemin), encoding="utf-8") as f:
        return f.read()


def _objets(noeud):
    """Chaque widget (type, propriétés) de l'arbre, avec ses enfants directs."""
    if isinstance(noeud, list):
        for x in noeud:
            yield from _objets(x)
    elif isinstance(noeud, dict):
        for type_, props in noeud.items():
            if isinstance(props, dict):
                yield type_, props
                yield from _objets(props.get("widgets"))


def _barre_du_bas():
    arbre = yaml.load(_lire("Tab5", "ui_components", "alarm_popup.yaml"), Loader=_Chargeur)
    for _, props in _objets(arbre):
        enfants = [p for e in props.get("widgets") or [] if isinstance(e, dict)
                   for p in e.values() if isinstance(p, dict)]
        if any(p.get("id") == "lbl_alarm_rdv_next" for p in enfants):
            return props, {p.get("id"): p for p in enfants}
    raise AssertionError("barre de lbl_alarm_rdv_next introuvable dans alarm_popup.yaml")


def _constante():
    m = re.search(r"constexpr int32_t kLargeurRdvSuivant = (\d+);", _lire("Tab5", "alarm_render.cpp"))
    assert m, "kLargeurRdvSuivant introuvable dans alarm_render.cpp"
    return int(m.group(1))


def test_prochain_rdv_coupe_en_cpp():
    src = _lire("Tab5", "alarm_render.cpp")
    assert re.search(r"texte_ha_coupe\(ui\.lbl_rdv_next,", src), \
        "lbl_rdv_next doit passer par texte_ha_coupe() (une ligne, « … »)"


def test_prochain_rdv_s_arrete_avant_tester():
    barre, enfants = _barre_du_bas()
    label, tester = enfants["lbl_alarm_rdv_next"], enfants["btn_alarm_test"]
    assert barre["styles"] == "style_transparent"  # pad_all 0 : x relatifs au bord de la barre
    assert "width" not in label, "largeur fixe = retour à la ligne : le texte doit rester sur une ligne"
    assert label["align"] == "LEFT_MID" and tester["align"] == "CENTER"
    bord_tester = barre["width"] // 2 + tester["x"] - tester["width"] // 2
    fin_texte = label["x"] + _constante()
    assert fin_texte + AIR_MIN <= bord_tester, (
        f"le texte va jusqu'à {fin_texte} px, le bouton « Tester » commence à {bord_tester} px")
