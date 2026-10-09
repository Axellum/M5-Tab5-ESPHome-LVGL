# -*- coding: utf-8 -*-
"""Popup « Réglages » (06/10/2026) : le contrat numérique entre le YAML et le C++.

Les boutons de Tab5/ui_components/reglages_popup.yaml passent `reglage` (un ReglageId de
Tab5/ecran/tab5_reglages.h, inclus par tab5_custom.h) et `valeur` (l'index d'une option) au script tab5_reglages_choisir ;
tab5_reglages_ouvrir range ces boutons dans les tableaux de ReglagesUI, dimensionnés par
REGLAGES_NB_*. Rien ne compile ce contrat : une option de plus dans un select, une langue
de plus, un bouton oublié ou un numéro faux passent la compilation, et le bouton écrit
la mauvaise entité ou reste sans surbrillance. Ce test relit :

- chaque ReglageId est appelé par le popup et traité par tab5_reglages_choisir ;
- une pastille par option, valeurs 0..n-1 dans l'ordre des options du select, autant que
  REGLAGES_NB_* (et, pour les langues, que de fichiers dans Tab5/lang/) ;
- Oui vaut 1 et Non 0 ; les flèches du thème valent −1 et +1 ;
- tab5_reglages_ouvrir pose chaque bouton à son index (Oui en 0, Non en 1) ;
- les fenêtres inscrites au registre (tab5-navigation.yaml) tiennent dans ModalRegistry::MAX ;
- quatre pages (08/10/2026) : un nom en haut et un conteneur par ReglagesPage, rangés à
  leur index, et le geste de changement de page qui ne sort pas du popup, n'est pas pris
  à un curseur et rend le lever du doigt muet."""
import pathlib
import re

import yaml
from tests.commun import ChargeurBalisesBrutes as _Chargeur, contrat, lire as _lire, source

REPO = pathlib.Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
POPUP = TAB5 / "ui_components" / "reglages_popup.yaml"
SCRIPTS = TAB5 / "paquets" / "tab5-reglages.yaml"

BOUTON = re.compile(r"file: reglages_choix_btn\.yaml, vars: \{ id: (\w+), x: \d+, y: \d+, w: \d+, "
                    r"reglage: (\d+), valeur: (-?\d+), label_text: \"([^\"]*)\" \}")
FLECHE = re.compile(r"file: bouton_pas\.yaml, .*appui: \[ script\.execute: \{ id: tab5_reglages_choisir, "
                    r"reglage: (\d+), valeur: (-?\d+) \} \]")
# Champ de ReglagesUI de chaque réglage à boutons (le thème a ses flèches et son nom).
CHAMPS = {"REGLAGE_EXTINCTION": "extinction", "REGLAGE_OKAY_NABU": "okay_nabu", "REGLAGE_TAPE": "tape",
          "REGLAGE_MODE": "mode", "REGLAGE_NUIT": "nuit", "REGLAGE_LANGUE": "langue",
          "REGLAGE_LIMITE_CHARGE": "limite", "REGLAGE_ECONOMIE": "economie",
          "REGLAGE_BATTERIE_MONTEE": "montee", "REGLAGE_MODE_CHARGE": "mode_charge",
          "REGLAGE_WIFI_ECO": "wifi_eco", "REGLAGE_ANIMATIONS": "animations"}
OUI_NON = ("REGLAGE_OKAY_NABU", "REGLAGE_TAPE", "REGLAGE_NUIT", "REGLAGE_BATTERIE_MONTEE")
ONGLET = re.compile(r"file: reglages_onglet\.yaml, vars: \{ id: (\w+), x: \d+, page: (\d+), label_text: \"([^\"]*)\" \}")
# Conteneur de chaque page (reglages_popup.yaml ; la page Système est console_sys.yaml).
PAGES = {"REGLAGES_PAGE_ECRAN": "reglages_page_ecran", "REGLAGES_PAGE_APPARENCE": "reglages_page_apparence",
         "REGLAGES_PAGE_BATTERIE": "reglages_page_batterie", "REGLAGES_PAGE_SYSTEME": "reglages_page_systeme"}


def _entete():
    return contrat()


def _reglages():
    """{nom: numéro} de l'enum ReglageId."""
    texte = _entete()
    bloc = texte[texte.index("enum ReglageId"):]
    bloc = bloc[:bloc.index("};")]
    trouves = {nom: int(n) for nom, n in re.findall(r"(REGLAGE_\w+) = (\d+)", bloc)}
    assert len(trouves) >= 7, "le motif ne reconnaît plus l'enum ReglageId"
    return trouves


def _nb(nom):
    m = re.search(rf"constexpr int {nom} = (\d+);", _entete())
    assert m, f"{nom} introuvable dans tab5_custom.h et ses en-têtes"
    return int(m.group(1))


def _boutons():
    """{nom du réglage: [(id, valeur, libellé)]}, dans l'ordre du popup."""
    noms = {n: nom for nom, n in _reglages().items()}
    rangees = {}
    for ident, reglage, valeur, libelle in BOUTON.findall(_lire(POPUP)):
        rangees.setdefault(noms[int(reglage)], []).append((ident, int(valeur), libelle))
    assert sum(len(b) for b in rangees.values()) >= 20, "le motif ne reconnaît plus les boutons du popup"
    return rangees


def _options(fichier, ident):
    selects = yaml.load(_lire(source(fichier)), Loader=_Chargeur)["select"]
    return next(s for s in selects if s.get("id") == ident)["options"]


def test_chaque_reglage_est_appele_et_traite():
    reglages = _reglages()
    appeles = set(_boutons()) | {next(n for n, v in reglages.items() if v == int(r))
                                 for r, _ in FLECHE.findall(_lire(POPUP))}
    assert appeles == set(reglages), f"réglages sans bouton : {sorted(set(reglages) - appeles)}"
    script = _lire(SCRIPTS)
    for nom in reglages:
        assert f"case {nom}:" in script, f"tab5_reglages_choisir ne traite pas {nom}"


def test_une_pastille_par_option_dans_l_ordre_du_select():
    boutons = _boutons()
    for nom, fichier, select, nb in (
            ("REGLAGE_EXTINCTION", "tab5-ha-controls.yaml", "tab5_extinction_auto", "REGLAGES_NB_EXTINCTION"),
            ("REGLAGE_MODE", "tab5-themes.yaml", "tab5_theme_mode", "REGLAGES_NB_MODES"),
            ("REGLAGE_LIMITE_CHARGE", "tab5-sensors-diagnostics.yaml", "tab5_limite_charge",
             "REGLAGES_NB_LIMITES"),
            ("REGLAGE_ECONOMIE", "tab5-economie.yaml", "tab5_economie", "REGLAGES_NB_ECONOMIE"),
            ("REGLAGE_MODE_CHARGE", "tab5-sensors-diagnostics.yaml", "tab5_mode_charge",
             "REGLAGES_NB_MODES_CHARGE"),
            ("REGLAGE_WIFI_ECO", "tab5-sensors-diagnostics.yaml", "tab5_wifi_eco", "REGLAGES_NB_WIFI_ECO"),
            ("REGLAGE_ANIMATIONS", "tab5-economie.yaml", "tab5_animations", "REGLAGES_NB_ANIMATIONS")):
        options = _options(fichier, select)
        assert [libelle for _, _, libelle in boutons[nom]] == options, f"{nom} : libellés ≠ options de {select}"
        assert [valeur for _, valeur, _ in boutons[nom]] == list(range(len(options))), nom
        assert _nb(nb) == len(options), f"{nb} ≠ {len(options)} options de {select}"


def test_une_pastille_par_langue():
    langues = _options("tab5-ha-controls.yaml", "tab5_langue")
    fichiers = sorted((TAB5 / "lang").glob("*.yaml"))
    assert len(langues) == len(fichiers), "select « Langue » ≠ fichiers de Tab5/lang/"
    assert _nb("REGLAGES_NB_LANGUES") == len(langues)
    pastilles = _boutons()["REGLAGE_LANGUE"]
    assert [valeur for _, valeur, _ in pastilles] == list(range(len(langues)))
    # Le nom natif est écrit par reglages_preparer() : aucun texte dans le YAML.
    assert all(libelle == "" for _, _, libelle in pastilles)


def test_oui_vaut_1_non_vaut_0_et_les_fleches_du_theme():
    boutons = _boutons()
    for nom in OUI_NON:
        assert [(libelle, valeur) for _, valeur, libelle in boutons[nom]] == [("Oui", 1), ("Non", 0)], nom
    fleches = FLECHE.findall(_lire(POPUP))
    assert sorted(int(v) for _, v in fleches) == [-1, 1]
    assert {int(r) for r, _ in fleches} == {_reglages()["REGLAGE_THEME"]}


def test_ouvrir_pose_chaque_bouton_a_son_index():
    script = _lire(SCRIPTS)
    for nom, boutons in _boutons().items():
        champ = CHAMPS[nom]
        for ident, valeur, libelle in boutons:
            index = (0 if libelle == "Oui" else 1) if nom in OUI_NON else valeur
            ligne = f"u.{champ}[{index}] = id({ident});"
            assert script.count(ligne) == 1, f"tab5_reglages_ouvrir : « {ligne} » attendu une fois"
        assert len(re.findall(rf"u\.{champ}\[\d+\] = ", script)) == len(boutons), champ


def test_registre_des_fenetres_assez_grand():
    inscrites = _lire(TAB5 / "paquets" / "tab5-navigation.yaml").count("ModalRegistry::add(")
    m = re.search(r"constexpr int MAX = (\d+);", _lire(TAB5 / "ecran" / "tab5_registry.h"))
    assert m and inscrites > 10, "les motifs ne reconnaissent plus le registre"
    assert inscrites <= int(m.group(1)), f"{inscrites} fenêtres pour ModalRegistry::MAX = {m.group(1)}"



def _pages():
    """{nom: numéro} de l'enum ReglagesPage (sans REGLAGES_NB_PAGES)."""
    texte = _entete()
    bloc = texte[texte.index("enum ReglagesPage"):]
    bloc = bloc[:bloc.index("};")]
    return {nom: int(n) for nom, n in re.findall(r"(REGLAGES_PAGE_\w+) = (\d+)", bloc)}


def _corps(texte, signature):
    """Corps d'une fonction C++ jusqu'à l'accolade fermante en début de ligne."""
    debut = texte.index(signature)
    return texte[debut:texte.index("\n}\n", debut)]


def test_un_nom_et_un_conteneur_par_page():
    pages = _pages()
    nb = re.search(r"REGLAGES_NB_PAGES = (\d+),", _entete())
    assert nb and len(pages) == int(nb.group(1)) == 4, pages
    popup = _lire(POPUP)
    onglets = ONGLET.findall(popup)
    assert [int(p) for _, p, _ in onglets] == sorted(pages.values()), "un nom par page, dans l'ordre"
    script = _lire(SCRIPTS)
    systeme = _lire(TAB5 / "ui_components" / "console_sys.yaml")
    for (ident, page, _), (nom, n) in zip(onglets, sorted(pages.items(), key=lambda kv: kv[1])):
        assert int(page) == n
        assert script.count(f"u.onglet[{nom}] = id({ident});") == 1, ident
        conteneur = PAGES[nom]
        assert f"id: {conteneur}" in popup + systeme, conteneur
        assert script.count(f"u.page[{nom}] = id({conteneur});") == 1, conteneur
    # La page Système (l'ancienne console) est dans le popup, plus à part.
    assert "!include console_sys.yaml" in popup
    assert "console_sys.yaml" not in _lire(TAB5 / "paquets" / "tab5-lvgl.yaml")
    # Engrenage : un tap « auto » ouvre la page Écran (code reglages de kGestesAuto ; les
    # deux gestes passent par tab5_ecran_ouvrir et le registre, tests/test_gestes.py,
    # tests/test_appuis.py).
    zones = _lire(TAB5 / "ecran" / "tab5_zones.cpp").replace("\r\n", "\n")
    auto = zones.split("kGestesAuto[GESTE_NB] = {", 1)[1].split("};", 1)[0]
    assert re.search(r'"reglages",\s+nullptr,\s+// engrenage', auto)


def test_geste_de_page_reste_dans_le_popup():
    """Le mécanisme des pages est partagé avec le popup du réveil depuis le 09/10/2026
    (tab5_pages.cpp) : le popup Réglages le branche, et le geste montre la page voisine
    par reglages_afficher_page (confirmations refermées, page Batterie peinte)."""
    cpp = _lire(TAB5 / "ecran" / "tab5_reglages.cpp").replace("\r\n", "\n")
    preparer = _corps(cpp, "void reglages_preparer()")
    assert "s_pages.afficher = reglages_afficher_page;" in preparer
    assert preparer.index("s_pages.n = REGLAGES_NB_PAGES;") < preparer.index("pages_brancher(s_pages);")
    afficher = _corps(cpp, "void reglages_afficher_page(")
    assert afficher.index("fermer_confirmations();") < afficher.index("pages_montrer(s_pages, page);")
    pages = _lire(TAB5 / "ecran" / "tab5_pages.cpp").replace("\r\n", "\n")
    brancher = _corps(pages, "void pages_brancher(")
    # Le geste s'arrête au popup : page_main ne change ni les prévisions ni la pièce.
    assert "lv_obj_remove_flag(p.popup, LV_OBJ_FLAG_GESTURE_BUBBLE);" in brancher
    assert "lv_obj_add_event_cb(p.popup, geste_rappel, LV_EVENT_GESTURE, &p);" in brancher
    geste = _corps(pages, "void geste_rappel(")
    assert "dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT" in geste
    # Un curseur ou un rouleau glissé n'est pas un changement de page ; le lever du doigt
    # qui suit le geste ne déclenche rien (ni tap, ni appui long). Dans cet ordre.
    assert "lv_obj_check_type(o, &lv_slider_class)" in geste
    assert "lv_obj_check_type(o, &lv_roller_class)" in geste
    assert (geste.index("lv_slider_class") < geste.index("lv_roller_class")
            < geste.index("lv_indev_wait_release(indev);") < geste.index("p->afficher(page);"))


def test_gardes_de_la_console_par_la_page_systeme():
    """La console et la page Batterie ne coûtent rien tant qu'elles ne sont pas affichées
    (garde #T222)."""
    diag = _lire(TAB5 / "paquets" / "tab5-sensors-diagnostics.yaml")
    assert diag.count("if (!reglages_page_visible(REGLAGES_PAGE_SYSTEME)) return;") == 3
    assert "if (reglages_page_visible(REGLAGES_PAGE_SYSTEME)) {" in diag
    assert "reglages_batterie_peindre();" in diag
    assert "if (reglages_page_visible(REGLAGES_PAGE_SYSTEME))" in _lire(TAB5 / "paquets" / "tab5-themes.yaml")
    cpp = _lire(TAB5 / "ecran" / "tab5_reglages.cpp").replace("\r\n", "\n")
    assert "reglages_page_visible(REGLAGES_PAGE_BATTERIE)" in _corps(cpp, "void reglages_batterie_peindre()")


def test_bouchon_du_rendu_comme_le_vrai_select():
    for ident in ("tab5_limite_charge", "tab5_mode_charge", "tab5_wifi_eco"):
        vrai = _options("tab5-sensors-diagnostics.yaml", ident)
        assert _options("rendu/bouchons.yaml", ident) == vrai, ident
