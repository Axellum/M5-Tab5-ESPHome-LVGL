# -*- coding: utf-8 -*-
"""Carrousel des clims (ADR-0038, 09/10/2026) : le popup clim a une page par clim que la
tablette connaît, le toucher de la température de la pièce l'ouvre, et la liste de la
tuile − / + passe à l'appui long sur la valeur entre − et +.

Le contrat tient en des chaînes qu'aucun compilateur ne compare :

- une seule liste des clims (clims_enumerer) : la clim du blueprint d'abord, puis les
  clims de tuile reçues, et c'est elle que lisent l'ouverture, le geste et les pastilles ;
- les pastilles : autant dans le YAML que kClimPastilles, posées par tab5_clim_ui,
  sous les trois cartes et dans la carte modale ;
- les gestes de la carte clim de l'accueil, et les appuis du rendu hors tablette qui les
  rejouent."""
import re

from tests.commun import avec_jetons, jeton, lire as _lire

import ecrans  # noqa: E402 — tools/rendu, mis sur sys.path par tests/conftest.py

CLIM = _lire("Tab5", "ecran", "tab5_clim.cpp")
ENTETE = _lire("Tab5", "ecran", "tab5_clim.h")
POPUP = _lire("Tab5", "ui_components", "climate_popup.yaml")
CARTE = avec_jetons(_lire("Tab5", "ui_components", "climate_card.yaml"))


def _corps(source, signature):
    debut = source.index(signature)
    return source[debut:source.index("\n}\n", debut)]


def _bloc_yaml(texte, ident, fin):
    return texte.split(f"id: {ident}", 1)[1].split(fin, 1)[0]


def _nombre(bloc, cle):
    m = re.search(rf"\n\s+{cle}: (-?\d+)", bloc)
    assert m, cle
    return int(m.group(1))


def test_une_seule_liste_des_clims():
    corps = _corps(CLIM, "int clims_enumerer(ClimRef* out, int max) {")
    # La clim du blueprint d'abord, sauf zone absente ; puis les tuiles dont les réglages
    # sont reçus, pièce par pièce, tuile par tuile.
    assert corps.index("zone_absente(Zone::CLIM)") < corps.index("reglages.recu")
    assert "for (int r = 0; r < kPieces; r++)" in corps and "for (int t = 0; t < kTuiles; t++)" in corps
    # Un doublon (même nom non vide) n'a pas sa page.
    assert "meme_nom(noms[i], nom)" in corps
    assert "a[0] != '\\0'" in _corps(CLIM, "bool meme_nom(")
    # Ouverture, geste et pastilles lisent cette liste, rien d'autre.
    for f in ("void carrousel_pastilles() {", "void carrousel_geste(lv_event_t* /*e*/) {",
              "bool clim_ref_choisir(ClimRef& out) {"):
        assert "clims_enumerer(l, kClimPastilles);" in _corps(CLIM, f), f
    # Déclarée pour les autres unités (une clim de plus, ailleurs : une ligne ici).
    assert "int clims_enumerer(ClimRef* out, int max);" in _lire("Tab5", "ecran", "tab5_internal.h")
    assert "clims_enumerer()" in _lire("AGENTS.md")


def test_geste_comme_les_pages_des_reglages():
    geste = _corps(CLIM, "void carrousel_geste(lv_event_t* /*e*/) {")
    assert "dir != LV_DIR_LEFT && dir != LV_DIR_RIGHT" in geste
    # L'arc glissé de côté règle la consigne ; une seule clim : rien.
    assert "lv_obj_check_type(o, &lv_arc_class)" in geste
    assert geste.index("if (n < 2) return;") < geste.index("lv_indev_wait_release(indev);")
    assert "reglages_page_voisine(carrousel_rang(l, n), n, dir == LV_DIR_LEFT)" in geste
    preparer = _corps(CLIM, "void clim_carrousel_preparer() {")
    assert "lv_obj_remove_flag(p, LV_OBJ_FLAG_GESTURE_BUBBLE);" in preparer
    assert "lv_obj_add_event_cb(p, carrousel_geste, LV_EVENT_GESTURE, nullptr);" in preparer
    # tab5_clim_ui est relancé par tab5_zones_apply à chaque réponse de HA : un seul rappel
    # (sinon un geste sautait une page par rappel, vu dans le rendu du 09/10/2026).
    assert preparer.index("lv_obj_remove_event_cb(p, carrousel_geste);") < preparer.index("lv_obj_add_event_cb(")
    assert "- script.execute: tab5_clim_ui" in _lire("Tab5", "paquets", "tab5-zones.yaml")
    scripts = _lire("Tab5", "paquets", "tab5-scripts.yaml")
    assert "clim_carrousel_preparer();" in scripts.split("- id: tab5_clim_ui", 1)[1].split("\n  - id: ", 1)[0]
    # Changement de page instantané : aucune animation dans le carrousel.
    for f in ("void carrousel_geste(", "void carrousel_pastilles(", "bool clim_carrousel_ouvrir_sur("):
        assert "lv_anim" not in _corps(CLIM, f), f


def test_ouverture_sur_la_piece_affichee():
    # La clim visée par la température : celle de la pièce affichée en mode HA, sinon la
    # première ; aucune : faux (la liste de la tuile − / +).
    choisir = _corps(CLIM, "bool clim_ref_choisir(ClimRef& out) {")
    assert "if (n == 0) return false;" in choisir
    assert "tuiles_piece_mode_ha()" in choisir and "l[k].piece == piece" in choisir
    assert "out = l[i];" in choisir
    # Le carrousel ouvert sur une clim : sa page, puis le popup.
    ouvrir = _corps(CLIM, "bool clim_carrousel_ouvrir_sur(const ClimRef& c) {")
    assert ouvrir.index("clim_ref_afficher(c)") < ouvrir.index("animate_popup_open(g_clim_ui.popup);")
    # Le toucher de la température (ADR-0048) : la roue, sinon le carrousel sur elle.
    temperature = _corps(CLIM, "bool clim_temperature_ouvrir(lv_obj_t* ancre) {")
    assert temperature.index("if (!clim_ref_choisir(c)) return false;") < temperature.index(
        "if (clim_roue_ouvrir(c, ancre)) return true;") < temperature.index("return clim_carrousel_ouvrir_sur(c);")
    piece = _corps(_lire("Tab5", "ecran", "tab5_tuiles.cpp"), "int tuiles_piece_mode_ha() {")
    assert "if (!g_central_ctx.ha_mode || heritage()) return -1;" in piece


def test_pastilles_du_popup():
    n = int(re.search(r"constexpr int kClimPastilles = (\d+);", ENTETE).group(1))
    ids = re.findall(r"id: clim_pastille_(\d+),", POPUP)
    assert ids == [str(i) for i in range(n)]
    scripts = _lire("Tab5", "paquets", "tab5-scripts.yaml")
    assert re.findall(r"u\.pastilles\[(\d+)\] = id\(clim_pastille_(\d+)\);", scripts) == [(i, i) for i in ids]
    assert "u.pastilles_cadre = id(clim_pastilles);" in scripts
    # Rangée flex sous les trois cartes, dans la carte modale (contenu : 690 − 2 × 2 de
    # bordure), masquée au démarrage (une seule clim).
    cadre = _bloc_yaml(POPUP, "clim_pastilles", "widgets:")
    assert "align: BOTTOM_MID" in cadre and "hidden: true" in cadre
    tokens = _lire("Tab5", "paquets", "tab5-ui-tokens.yaml")
    carte_h = int(re.search(r'modal_card_h: "(\d+)"', tokens).group(1))
    corps_y = int(re.search(r'modal_body_y: "(\d+)"', tokens).group(1))
    hauteurs = {int(h) for h in re.findall(r"y: \$\{modal_body_y\}\n\s+width: \d+\n\s+height: (\d+)", POPUP)}
    assert len(hauteurs) == 1
    bas_cartes = corps_y + hauteurs.pop()
    contenu = carte_h - 2 * 2
    bas = contenu + _nombre(cadre, "y")
    assert bas_cartes < bas - 4 and bas <= contenu, (bas_cartes, bas, contenu)
    # Couleur par la palette (règle 1) ; largeurs de pagination_afficher (16, 30).
    assert POPUP.count("styles: style_bg_dim, bg_opa:") >= n
    assert "pagination_afficher(u.pastilles, kClimPastilles, carrousel_rang(l, n))" in CLIM


def test_gestes_de_la_carte_clim():
    salon = _bloc_yaml(CARTE, "btn_reglables_liste", "\n    - ")
    assert ("on_short_click:\n          - lambda: 'if (!clim_temperature_ouvrir(id(btn_reglables_liste))) "
            "reglables_liste_basculer();'") in salon
    # L'appui long : l'historique (ADR-0032), du salon ou de la pièce affichée (ADR-0040).
    assert "accueil_historique_cle(false);" in salon
    assert "id(tab5_historique_ouvrir).execute(std::string(cle));" in salon
    # La liste de la tuile − / + : appui long sur la valeur entre − et +.
    valeur = _bloc_yaml(CARTE, "btn_clim_target_click", "\n          - button:")
    assert "on_long_press:\n                - lambda: 'reglables_liste_basculer();'" in valeur


def test_le_rendu_touche_la_valeur_et_rejoue_le_carrousel():
    # btn_clim_target_click : centré dans climate_controls_zone (405 × cadre_bas_h, en bas de la
    # carte de 198 posée en 855, 110).
    valeur = _bloc_yaml(CARTE, "btn_clim_target_click", "on_short_click")
    w, h = _nombre(valeur, "width"), _nombre(valeur, "height")
    tuile = jeton("cadre_bas_h")
    x0, y0 = 855 + (405 - w) / 2, 110 + 198 - tuile + (tuile - h) / 2
    x, y = ecrans.CONSIGNE_CLIM
    assert x0 < x < x0 + w and y0 < y < y0 + h
    noms = {e.nom: e for e in ecrans.ECRANS}
    assert ecrans.Long(*ecrans.CONSIGNE_CLIM) in noms["accueil-tuile-liste"].etapes
    for nom in ("climatisation-carrousel", "climatisation-carrousel-page-2", "climatisation-carrousel-page-3",
                "climatisation-carrousel-mode-ha"):
        assert noms[nom].etapes[:len(ecrans.CLIMS_DE_TUILES)] == ecrans.CLIMS_DE_TUILES, nom
        # Les clims de tuile oubliées ensuite : les définitions de la démo repoussées.
        assert noms[nom].fermer[-len(ecrans.MAISON_DE_LA_DEMO):] == ecrans.MAISON_DE_LA_DEMO, nom
    assert ecrans.CARROUSEL_SUIVANTE.dans_popup and ecrans.CARROUSEL_SUIVANTE.x2 < ecrans.CARROUSEL_SUIVANTE.x1
    # Les deux clims de tuile sont des tuiles cli sans l'option m.
    bureau = ecrans.BUREAU_AVEC_CLIMS[3].tuiles
    assert all(bureau[t].type == "cli" and "m" not in bureau[t].options for t in (2, 3))
