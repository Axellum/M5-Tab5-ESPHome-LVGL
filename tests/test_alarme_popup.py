# -*- coding: utf-8 -*-
"""Popup du réveil en cinq pages (09/10/2026, demande d'Axel) : ce que le compilateur ne
voit pas.

- Les pages et leurs noms en haut suivent ReveilPage (alarm_render.h), un nom par page,
  dans l'ordre, chacun posé dans g_reveil_ui par tab5_alarm_open ; le geste de page est
  branché par le mécanisme partagé avec les Réglages (tab5_pages.cpp) ; l'ouverture
  montre la page Heure.
- Chaque rouleau (rouleau.yaml) porte un ReveilRouleau distinct, est posé à la bonne
  place de g_reveil_ui.rouleau, a son cas dans le script tab5_alarm_rouleau, et ses
  bornes et son pas (kRouleaux, alarm_render.cpp) sont ceux de son entité : un rouleau
  qui proposerait une valeur que l'entité refuse ne réglerait rien.
- Tout tient dans sa carte, au pixel près (positions absolues, gabarits compris), et les
  noms des pages tiennent entre le titre et la croix.
- Le prochain rendez-vous passe par texte_ha_copier() (texte de HA) sur un label à
  hauteur fixe, « … » au-delà.
"""
import re

import yaml
from tests.commun import BaseChargeur, lire

CARTE_L, CARTE_H = 1250, 690          # ${modal_card_w} × ${modal_card_h}
CROIX_X = CARTE_L - 10 - 80            # modal_header.yaml : croix de 80 px à 10 px du bord
TITRE_FIN_MAX = 180                    # « Réveil », « Wecker », « Sveglia » : ~170 px au plus
CORPS_Y = 72                           # ${modal_body_y}


class _Chargeur(BaseChargeur):
    pass


def _include(chargeur, noeud):
    valeur = chargeur.construct_mapping(noeud, deep=True) if isinstance(noeud, yaml.MappingNode) \
        else {"file": chargeur.construct_scalar(noeud)}
    return {"!include": valeur}


_Chargeur.add_constructor("!include", _include)
_Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


def _popup():
    return yaml.load(lire("Tab5", "ui_components", "alarm_popup.yaml"), Loader=_Chargeur)


def _enum(nom):
    src = lire("Tab5", "ecran", "alarm_render.h")
    bloc = re.search(r"enum " + nom + r" : uint8_t \{(.*?)\};", src, re.S)
    assert bloc, f"enum {nom} introuvable dans alarm_render.h"
    return [m for m in re.findall(r"^\s*(REVEIL_\w+)", bloc.group(1), re.M) if not m.startswith("REVEIL_NB_")]


def _script(nom):
    texte = lire("Tab5", "paquets", "tab5-alarm.yaml")
    debut = texte.index(f"  - id: {nom}\n")
    fin = texte.find("\n  - id: ", debut + 1)
    return texte[debut:fin if fin > 0 else len(texte)]


def _includes(fichier):
    """vars de chaque include de `fichier` dans alarm_popup.yaml, dans l'ordre."""
    out = []

    def parcourir(n):
        if isinstance(n, list):
            for x in n:
                parcourir(x)
        elif isinstance(n, dict):
            inc = n.get("!include")
            if isinstance(inc, dict) and inc.get("file") == fichier:
                out.append(inc.get("vars") or {})
            for v in n.values():
                parcourir(v)

    parcourir(_popup())
    return out


# --- Pages ------------------------------------------------------------------------------

def test_un_nom_par_page_dans_l_ordre():
    pages = _enum("ReveilPage")
    assert len(pages) == 5
    onglets = _includes("reglages_onglet.yaml")
    assert [o["page"] for o in onglets] == list(range(len(pages))), "un nom par page, dans l'ordre"
    ouvrir = _script("tab5_alarm_open")
    for o, nom in zip(onglets, pages):
        assert o["afficher"] == "reveil_afficher_page", o["id"]
        assert ouvrir.count(f"u.onglet[{nom}] = id({o['id']});") == 1, o["id"]
        page = "alarm_page_" + nom.removeprefix("REVEIL_PAGE_").lower()
        assert ouvrir.count(f"u.page[{nom}] = id({page});") == 1, page


def test_noms_des_pages_entre_le_titre_et_la_croix():
    onglets = _includes("reglages_onglet.yaml")
    bords = [(o["x"], o["x"] + o.get("w", 200)) for o in onglets]
    assert bords[0][0] >= TITRE_FIN_MAX, "le premier nom passerait sur le titre"
    assert bords[-1][1] <= CROIX_X - 8, "le dernier nom passerait sur la croix"
    assert all(a[1] < b[0] for a, b in zip(bords, bords[1:])), "noms qui se chevauchent"


def test_geste_et_page_a_l_ouverture():
    src = lire("Tab5", "ecran", "alarm_render.cpp")
    preparer = src[src.index("void reveil_preparer()"):src.index("void reveil_afficher_page(")]
    assert "s_pages.afficher = reveil_afficher_page;" in preparer
    assert preparer.index("s_pages.n = REVEIL_NB_PAGES;") < preparer.index("pages_brancher(s_pages);")
    assert "pages_montrer(s_pages, page);" in src[src.index("void reveil_afficher_page("):]
    ouvrir = _script("tab5_alarm_open")
    # Pointeurs posés une fois, popup en dernier (alarm_render_settings attend popup non nul).
    assert ouvrir.index("u.lbl_rdv_next = id(lbl_alarm_rdv_next);") < ouvrir.index("u.popup = id(alarm_popup);") \
        < ouvrir.index("reveil_preparer();")
    assert ouvrir.index("script.execute: tab5_alarm_refresh") < ouvrir.index("reveil_afficher_page(REVEIL_PAGE_HEURE);") \
        < ouvrir.index("animate_popup_open(id(alarm_popup));")


# --- Rouleaux ---------------------------------------------------------------------------

def test_rouleaux_poses_et_tous_traites():
    noms = _enum("ReveilRouleau")
    rouleaux = _includes("rouleau.yaml")
    assert sorted(r["quoi"] for r in rouleaux) == list(range(len(noms))), "un rouleau par ReveilRouleau"
    ouvrir, regler = _script("tab5_alarm_open"), _script("tab5_alarm_rouleau")
    for r in rouleaux:
        nom = noms[r["quoi"]]
        assert ouvrir.count(f"u.rouleau[{nom}] = id({r['id']})->obj;") == 1, r["id"]
        assert f"case {nom}:" in regler, f"{nom} sans cas dans tab5_alarm_rouleau"


def _entite(id_):
    texte = lire("Tab5", "paquets", "tab5-alarm.yaml")
    bloc = texte[texte.index(f"id: {id_}\n"):]
    bloc = bloc[:bloc.find("\n  - ")]
    lire_ = {k: int(re.search(rf"^\s+{k}: (\d+)", bloc, re.M).group(1)) for k in ("min_value", "max_value", "step")}
    return lire_["min_value"], lire_["max_value"], lire_["step"]


def test_bornes_des_rouleaux_egales_aux_entites():
    src = lire("Tab5", "ecran", "alarm_render.cpp")
    table = dict((nom, (int(b), int(h), int(p))) for b, h, p, nom in re.findall(
        r"\{(\d+), (\d+), (\d+), FormatRouleau::\w+, (?:true|false)\},\s*// (REVEIL_ROULEAU_\w+)", src))
    assert list(table) == _enum("ReveilRouleau"), "kRouleaux : une ligne par ReveilRouleau, dans l'ordre"
    entites = {
        "REVEIL_ROULEAU_DELAI": "tab5_alarm_lead",
        "REVEIL_ROULEAU_REPOS": "tab5_alarm_rest_hours",
        "REVEIL_ROULEAU_REPETITION": "tab5_alarm_snooze_min",
        "REVEIL_ROULEAU_DUREE_MAX": "tab5_alarm_max_ring",
        "REVEIL_ROULEAU_RDV_AVANT": "tab5_rdv_lead",
    }
    for nom, entite in entites.items():
        assert table[nom] == _entite(entite), f"{nom} ≠ {entite}"
        assert f"case {nom}: nombre(id({entite}), v)" in _script("tab5_alarm_rouleau")
    # Heures (une journée), minutes, bornes du mode Ouverture : dans une journée.
    assert table["REVEIL_ROULEAU_HEURE"][:2] == (0, 23)
    assert table["REVEIL_ROULEAU_MINUTES"][:2] == (0, 59)
    assert table["REVEIL_ROULEAU_PAS_AVANT"][:2] == table["REVEIL_ROULEAU_PAS_APRES"][:2] == (0, 1439)


# --- Au pixel près ----------------------------------------------------------------------

GABARITS = {  # gabarit : (vars de largeur, de hauteur, valeurs fixes)
    "rouleau.yaml": ("w", "h", None),
    "choix_btn.yaml": ("w", "h", (None, 60)),
    "alarm_day_chip.yaml": (None, None, (160, 70)),
    "reglages_rangee.yaml": ("w", None, (None, 40)),
}


def _boite(widget):
    """(x, y, l, h) d'un enfant posé en TOP_LEFT, gabarits compris ; None sinon."""
    inc = widget.get("!include") if isinstance(widget, dict) else None
    if inc:
        regle = GABARITS.get(inc.get("file"))
        if regle is None:
            return None
        v = inc.get("vars") or {}
        lv, hv, fixe = regle
        largeur = v.get(lv) if lv else fixe[0]
        hauteur = v.get(hv) if hv and hv in v else (fixe[1] if fixe else None)
        x = 17 if inc["file"] == "reglages_rangee.yaml" else v.get("x")
        return int(x), int(v["y"]), int(largeur), int(hauteur)
    (_, props), = widget.items()
    if not isinstance(props, dict) or props.get("align") != "TOP_LEFT":
        return None
    if not all(isinstance(props.get(k), int) for k in ("x", "y", "width", "height")):
        return None
    return props["x"], props["y"], props["width"], props["height"]


def test_tout_tient_dans_sa_carte():
    cartes = 0
    for page in _popup()["obj"]["widgets"][1]["obj"]["widgets"]:
        props = page.get("obj") if isinstance(page, dict) else None
        if not props or not str(props.get("id", "")).startswith("alarm_page_"):
            continue
        assert props["y"] == "${modal_body_y}" and CORPS_Y + props["height"] <= CARTE_H - 20, props["id"]
        for carte in props["widgets"]:
            c = carte["obj"]
            assert c["x"] + c["width"] <= CARTE_L - 24 and c["height"] == props["height"], props["id"]
            cartes += 1
            for enfant in c["widgets"]:
                boite = _boite(enfant)
                if boite is None:
                    continue
                x, y, largeur, hauteur = boite
                assert x >= 16 and y >= 14, (props["id"], enfant)
                assert x + largeur <= c["width"] - 16 and y + hauteur <= c["height"] - 14, (props["id"], enfant)
    assert cartes == 8, "Heure, Sonnerie, Annonces : deux cartes ; Jours, Ouverture : une"


# --- Prochain rendez-vous ---------------------------------------------------------------

def test_prochain_rdv_sur_plusieurs_lignes():
    src = lire("Tab5", "ecran", "alarm_render.cpp")
    assert re.search(r"texte_ha_copier\(txt, sizeof\(txt\), n\.c_str\(\), n\.size\(\)\);", src), \
        "le titre d'un rendez-vous vient de HA : texte_ha_copier()"
    popup = lire("Tab5", "ui_components", "alarm_popup.yaml")
    ligne = next(l for l in popup.splitlines() if "id: lbl_alarm_rdv_next" in l)
    assert "height:" in ligne and "long_mode: DOT" in ligne, "hauteur fixe, « … » au-delà"
