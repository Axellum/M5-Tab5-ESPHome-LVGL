# -*- coding: utf-8 -*-
"""Popup à pages (ADR-0046, 09/10/2026) : la brique commune des popups à plusieurs pages
(Tab5/ecran/tab5_pages.cpp) — Réglages, Lumières, Volets, Température et Réveil — lue dans
le vrai C++.

On vérifie :
- le geste : arrêté au popup (GESTURE_BUBBLE retiré), ignoré quand il part d'un curseur,
  d'un arc ou d'un rouleau, sans effet sous deux pages, lever du doigt muet, page voisine en boucle ;
  branché sans doublon (le rappel retiré avant d'être ajouté) ;
- la géométrie des onglets : quatre pages tombent sur les noms des Réglages, cinq tiennent
  entre le titre et la croix ;
- un seul geste de page dans le firmware hors carrousel des clims (à brancher plus tard) :
  plus aucune copie dans les Réglages."""
import re

from tests.commun import TAB5, lire

PAGES = TAB5 / "ecran" / "tab5_pages.cpp"


def _cpp() -> str:
    return lire(PAGES).replace("\r\n", "\n")


def _corps(texte: str, signature: str) -> str:
    debut = texte.index(signature)
    i = texte.index("{", debut)
    profondeur = 0
    for j in range(i, len(texte)):
        profondeur += {"{": 1, "}": -1}.get(texte[j], 0)
        if profondeur == 0:
            return texte[i:j + 1]
    raise AssertionError(signature)


def _constante(nom: str) -> int:
    return int(re.search(rf"constexpr int32_t {nom} = (-?\d+);", _cpp()).group(1))


def test_geste_arrete_au_popup_et_branche_une_fois():
    brancher = _corps(_cpp(), "void pages_brancher(")
    assert "lv_obj_remove_flag(p->popup, LV_OBJ_FLAG_GESTURE_BUBBLE);" in brancher
    retire = "lv_obj_remove_event_cb_with_user_data(p->popup, geste_rappel, p);"
    ajoute = "lv_obj_add_event_cb(p->popup, geste_rappel, LV_EVENT_GESTURE, p);"
    assert retire in brancher and ajoute in brancher
    assert brancher.index(retire) < brancher.index(ajoute), "sans doublon : retiré avant d'être ajouté"


def test_geste_curseur_arc_et_lever_du_doigt():
    geste = _corps(_cpp(), "void geste_rappel(")
    assert "dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT" in geste
    # Un curseur ou un arc glissé de côté règle sa valeur ; sous deux pages, rien ; le
    # lever du doigt qui suit ne déclenche rien. Dans cet ordre.
    assert "lv_obj_check_type(o, &lv_slider_class) || lv_obj_check_type(o, &lv_arc_class)" in geste
    # Un rouleau (lv_roller, popup Réveil) se glisse de haut en bas ; en biais, LVGL y voit
    # aussi un geste gauche / droite : c'est son réglage (09/10/2026).
    assert "if (lv_obj_check_type(o, &lv_roller_class)) return;" in geste
    assert "if (n < 2) return;" in geste
    assert (geste.index("lv_arc_class") < geste.index("lv_roller_class") < geste.index("if (n < 2) return;")
            < geste.index("lv_indev_wait_release(indev);") < geste.index("p->afficher("))
    assert "reglages_page_voisine(p->courante(), n, dir == LV_DIR_LEFT)" in geste
    # Transitions instantanées : aucune animation dans la brique.
    assert "lv_anim" not in _cpp()


def _x_onglets(n: int) -> list[int]:
    debut, fin = _constante("kOngletsDebut"), _constante("kOngletsFin")
    ecart, largeur = _constante("kOngletEcart"), _constante("kOngletL")
    w = min(largeur, (fin - debut - (n - 1) * ecart) // n)
    return [fin - (n - i) * w - (n - 1 - i) * ecart for i in range(n)], w


def test_onglets_a_la_place_des_noms_des_reglages():
    """Quatre pages : les x des quatre noms des Réglages (reglages_popup.yaml) ; cinq
    pièces : entre la fin du titre (x 318) et la croix (8 px avant elle)."""
    popup = lire(TAB5 / "ui_components" / "reglages_popup.yaml")
    reglages = [int(x) for x in re.findall(r"file: reglages_onglet\.yaml, vars: \{ id: \w+, x: (\d+),", popup)]
    xs, w = _x_onglets(4)
    assert xs == reglages and w == 200
    # Largeur par défaut de reglages_onglet.yaml (le Réveil passe la sienne, `w`).
    largeur = int(re.search(r"\n  width: \$\{ w \| default\((\d+)\) \}\n",
                            lire(TAB5 / "ui_components" / "reglages_onglet.yaml")).group(1))
    assert largeur == w
    xs, w = _x_onglets(5)
    assert xs[0] >= _constante("kOngletsDebut") and xs[-1] + w == _constante("kOngletsFin")
    assert w >= 150, "cinq noms de pièce lisibles"
    # La croix (modal_header.yaml) : 80 px à 10 px du bord droit de la carte de 1250 px.
    assert _constante("kOngletsFin") <= 1250 - 10 - 80 - 8


def test_reglages_par_la_brique():
    cpp = lire(TAB5 / "ecran" / "tab5_reglages.cpp")
    assert "pages_brancher(&s_pages);" in cpp
    assert "constexpr auto peindre_choix = &choix_peindre;" in cpp
    # Le geste n'est écrit qu'une fois (le carrousel des clims, ADR-0038, a encore le sien :
    # à brancher sur la brique par une PR à part).
    for f in (TAB5 / "ecran").glob("*.cpp"):
        if f.name in ("tab5_pages.cpp", "tab5_clim.cpp"):
            continue
        assert not re.search(r"lv_obj_add_event_cb\([^;]*LV_EVENT_GESTURE", lire(f)), f.name


def test_temperature_par_la_brique():
    """Popup Température (ADR-0047) : son geste et la couleur de l'onglet affiché par la
    brique ; la place de ses onglets (après le titre, jusqu'à sept) reste la sienne."""
    cpp = lire(TAB5 / "ecran" / "tab5_historique.cpp").replace("\r\n", "\n")
    assert "pages_brancher(&s_pages);" in cpp
    assert "PagesPopup s_pages{nullptr, nombre_pages, rang_courant, afficher_page};" in cpp
    assert "choix_peindre(u.onglet, n, rang_courant());" in cpp
    assert "void peindre_onglet(" not in cpp, "la couleur de l'onglet : choix_peindre"


def test_reveil_par_la_brique():
    """Popup Réveil (09/10/2026) : son geste et la couleur du nom de la page affichée par la
    brique ; la place de ses noms reste celle du YAML (alarm_popup.yaml), pas pages_onglets."""
    cpp = lire(TAB5 / "ecran" / "alarm_render.cpp").replace("\r\n", "\n")
    assert "PagesPopup s_pages{nullptr, nombre_pages, page_courante, reveil_afficher_page};" in cpp
    assert "s_pages.popup = u.popup;\n    pages_brancher(&s_pages);" in cpp
    afficher = _corps(cpp, "void reveil_afficher_page(")
    assert "choix_peindre(u.onglet, REVEIL_NB_PAGES, page);" in afficher
    assert "pages_onglets(" not in cpp
