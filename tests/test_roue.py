# -*- coding: utf-8 -*-
"""Roue d'actions rapides (ADR-0036, 07/10/2026) : l'appui long d'une lampe, d'un volet ou
d'une clim pose un moyeu sur la tuile et deux anneaux de boutons au-dessus — le premier
pour « Maison », les commandes, les familles de réglages et « Détails » (le popup
complet), le second pour les choix de la famille touchée. Aucun compilateur ne vérifie ce
qui suit ; ce fichier lit le C++ et le YAML, comme les autres tests statiques :

- géométrie : constantes et table des sinus de Tab5/tab5_roue.cpp = celles de
  tools/rendu/ecrans.py (qui touche « Détails » et les familles dans le rendu) ; tailles
  des widgets ; boutons des deux anneaux dans l'écran, sans chevauchement, pour toutes les
  tuiles et toutes les familles ;
- boutons par type (tableau de l'ADR) et commandes = celles du contrat (ADR-0023) et des
  popups lumière et clim, rien de nouveau ; liens « Maison » et « Détails » ;
- sous-fenêtre du registre, fermée par l'inactivité, un popup, l'écran éteint, de
  nouvelles définitions ; repeinte au changement de thème et à un état poussé ;
- widgets et pointeurs, glyphes de mdi_font_36.
"""
import math
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))
sys.path.insert(0, os.path.join(REPO, "tools", "rendu"))
sys.path.insert(0, os.path.join(REPO, "tests"))

from pathlib import Path  # noqa: E402

import ecrans  # noqa: E402
from check_tab5_code_rules import font_glyphs  # noqa: E402
from test_tuiles_firmware import _commandes_de_l_adr, _fonction  # noqa: E402

ADR = os.path.join(REPO, "docs", "decisions", "0036-quick-action-wheel.md")


def _lire(*chemin):
    with open(os.path.join(REPO, *chemin), encoding="utf-8") as f:
        return f.read()


def _roue():
    return _lire("Tab5", "tab5_roue.cpp")


def _tuiles():
    # Tuiles, popups, roue d'une tuile et leur en-tête commun (lot L7, 08/10/2026).
    return "\n".join(_lire("Tab5", f) for f in ("tab5_tuiles_priv.h", "tab5_tuiles.cpp", "tab5_tuiles_popups.cpp", "tab5_tuiles_roue.cpp"))


def _yaml(nom):
    return _lire("Tab5", "ui_components", nom)


def _const(source, nom):
    m = re.search(rf"constexpr [^=;]*?\b{nom}\b\s*(?:\[[^\]]*\])?\s*=\s*([^;]+);", source)
    assert m, f"constexpr {nom} introuvable"
    return m.group(1).strip()


def _k(nom):
    return int(_const(_roue(), nom))


# ─────────────────────────────────────────────────────────────────────────────
# Géométrie
# ─────────────────────────────────────────────────────────────────────────────

def test_constantes_du_rendu_egales_au_cpp():
    geometrie = _lire("Tab5", "tab5_geometrie.h")   # écran partagé (lot L5, 07/10/2026)
    assert (int(_const(geometrie, "kEcranL")), int(_const(geometrie, "kEcranH"))) == ecrans.ROUE_ECRAN
    for cpp, rendu in (("kRayon", ecrans.ROUE_RAYON), ("kDiametre", ecrans.ROUE_DIAMETRE),
                       ("kRayon2", ecrans.ROUE_RAYON2), ("kDiametre2", ecrans.ROUE_DIAMETRE2),
                       ("kMarge", ecrans.ROUE_MARGE), ("kPasAngle", ecrans.ROUE_PAS_ANGLE),
                       ("kPasAngle2", ecrans.ROUE_PAS_ANGLE2), ("kPivot", ecrans.ROUE_PIVOT),
                       ("kLegende2", ecrans.ROUE_LEGENDE2), ("kLegendeH", ecrans.ROUE_LEGENDE_H)):
        assert _k(cpp) == rendu, cpp
    table = tuple(int(x) for x in re.findall(r"\d+", _const(_roue(), "kSin5").strip("{}")))
    assert table == ecrans.ROUE_SIN5
    # Même règle « dessous » des deux côtés.
    assert "s.dessous = s.ya - (kRayon2 + kLegende2 + kLegendeH / 2) < 0;" in _fonction(_roue(), "roue_ouvrir")


def test_table_des_sinus():
    assert len(ecrans.ROUE_SIN5) == 90 // ecrans.ROUE_PIVOT + 1
    for k, v in enumerate(ecrans.ROUE_SIN5):
        assert v == round(math.sin(math.radians(k * ecrans.ROUE_PIVOT)) * 32767), k


def test_tailles_des_widgets_egales_au_cpp():
    """Boutons ronds (rayon = demi-diamètre), moyeu, jauge, mots : le YAML pose la taille,
    le C++ centre avec la même."""
    for fichier, cle in (("roue_bouton.yaml", "kDiametre"), ("roue_choix.yaml", "kDiametre2")):
        d = _k(cle)
        texte = _yaml(fichier)
        assert f"width: {d}" in texte and f"height: {d}" in texte and f"radius: {d // 2}" in texte, fichier
        assert "styles: style_clim_btn" in texte, "le verre des boutons des popups"
    actions = _yaml("roue_actions.yaml")
    moyeu = actions.split("id: roue_moyeu\n", 1)[1].split("widgets:", 1)[0]
    assert f"width: {_k('kMoyeu')}" in moyeu and f"radius: {_k('kMoyeu') // 2}" in moyeu
    jauge = actions.split("id: roue_jauge", 1)[1].split("- button:", 1)[0]
    assert f"width: {_k('kJauge')}" in jauge and "start_angle: 135" in jauge and "end_angle: 45" in jauge
    assert f"width: {_k('kLegendeL')}" in _yaml("roue_legende.yaml")
    assert f"width: {_k('kNomL')}" in actions.split("id: roue_nom", 1)[1]
    # Bandes : 12 px de verre autour des boutons, et de l'air entre les deux.
    assert _k("kBande") == _k("kDiametre") + 24 and _k("kBande2") == _k("kDiametre2") + 24
    assert _k("kRayon") + _k("kBande") // 2 < _k("kRayon2") - _k("kBande2") // 2
    assert _k("kJauge") // 2 < _k("kRayon") - _k("kBande") // 2


def test_premier_anneau_centre_au_dessus_de_l_ancre():
    """Sans débordement : l'éventail est symétrique autour de la verticale de l'ancre, à
    180 px (± 1 px d'arrondi), 30° entre deux boutons."""
    for n in range(2, 7):
        centres = ecrans.roue_centres(640, 572, n)
        assert len(centres) == n
        for i, (x, y) in enumerate(centres):
            a = math.radians(90 + (n - 1) * 15 - 30 * i)
            assert abs(x - (640 + 180 * math.cos(a))) <= 1 and abs(y - (572 - 180 * math.sin(a))) <= 1
            assert y < 572
        for (x1, y1), (x2, y2) in zip(centres, reversed(centres)):
            assert x1 - 640 == 640 - x2 and y1 == y2


def test_second_anneau_centre_sur_sa_famille():
    """Sans débordement, le second anneau est centré sur l'angle de sa famille, à 290 px."""
    centres = ecrans.roue_centres(640, 572, 5)
    milieu = ecrans.roue_choix_centres(640, 572, 5, 2, 5)[2]
    assert milieu == (640, 572 - 290) and centres[2] == (640, 572 - 180)
    for i in range(5):
        (xf, yf) = centres[i]
        choix = ecrans.roue_choix_centres(640, 572, 5, i, 3)
        a_f = math.atan2(572 - yf, xf - 640)
        a_c = math.atan2(572 - choix[1][1], choix[1][0] - 640)
        assert abs(a_f - a_c) < math.radians(1), i


def _ancres():
    """Centres des tuiles météo (bouton de la tuile) et des pastilles du mode HA, pour 1
    à 5 cartes centrées (switches_card.yaml : 230 px + 20 d'écart)."""
    ancres = [(140 + 250 * k, 572) for k in range(5)]
    for nb in range(1, 6):
        gauche = (1280 - (nb * 250 - 20)) // 2
        ancres += [(gauche + 250 * k + 115, 526) for k in range(nb)]
    return ancres


def _dans_l_ecran(centres, r, contexte):
    for x, y in centres:
        assert ecrans.ROUE_MARGE + r <= x <= 1280 - ecrans.ROUE_MARGE - r, contexte
        assert ecrans.ROUE_MARGE + r <= y <= 720 - ecrans.ROUE_MARGE - r, contexte


def test_deux_anneaux_dans_l_ecran_et_sans_chevauchement():
    r1, r2 = ecrans.ROUE_DIAMETRE // 2, ecrans.ROUE_DIAMETRE2 // 2
    for xa, ya in _ancres() + [(104, 412), (640, 60), (40, 40), (1240, 700)]:
        for n in range(2, 7):
            premier = ecrans.roue_centres(xa, ya, n)
            _dans_l_ecran(premier, r1, (xa, ya, n))
            for famille in range(n):
                for m in range(2, 7):
                    second = ecrans.roue_choix_centres(xa, ya, n, famille, m)
                    _dans_l_ecran(second, r2, (xa, ya, n, famille, m))
                    if (xa, ya) not in _ancres():
                        continue
                    for i in range(m - 1):
                        (x1, y1), (x2, y2) = second[i], second[i + 1]
                        assert math.hypot(x2 - x1, y2 - y1) >= ecrans.ROUE_DIAMETRE2, (xa, ya, n, famille, i)
                    for (x1, y1) in premier:
                        for (x2, y2) in second:
                            assert math.hypot(x2 - x1, y2 - y1) >= r1 + r2, (xa, ya, n, famille, m)
            if (xa, ya) in _ancres():
                for i in range(n - 1):
                    (x1, y1), (x2, y2) = premier[i], premier[i + 1]
                    assert math.hypot(x2 - x1, y2 - y1) >= ecrans.ROUE_DIAMETRE, (xa, ya, n, i)


def test_ancre_haute_roue_en_dessous():
    """Les mots du second anneau tiennent au-dessus dès y = 364 ; plus haut, la roue
    passe sous l'ancre (une ligne du haut du popup Maison)."""
    seuil = ecrans.ROUE_RAYON2 + ecrans.ROUE_LEGENDE2 + ecrans.ROUE_LEGENDE_H // 2
    assert not ecrans.roue_dessous(seuil) and ecrans.roue_dessous(seuil - 1)
    # Toute ancre a ses deux anneaux entiers d'un côté ou de l'autre (les mots d'une ancre
    # entre 357 et 363 sont ramenés dans l'écran).
    bord = ecrans.ROUE_RAYON2 + ecrans.ROUE_DIAMETRE2 // 2 + ecrans.ROUE_MARGE
    assert seuil - 1 + bord <= 720 and seuil - bord >= 0
    assert all(y > 100 for _, y in ecrans.roue_centres(640, 100, 5))
    assert all(y > 100 for _, y in ecrans.roue_choix_centres(640, 100, 5, 2, 5))
    assert all(y < 526 for _, y in ecrans.roue_centres(640, 526, 6))


def test_le_rendu_touche_reglages_et_les_familles():
    """Les écrans des popups de tuile ouvrent la roue puis touchent son « Détails » ; ceux
    de la roue déplient une famille."""
    par_nom = {e.nom: e for e in ecrans.ECRANS}

    def touche(nom, centre):
        return any(isinstance(e, ecrans.Toucher) and (e.x, e.y) == centre for e in par_nom[nom].etapes)

    chambre, salon, volet = ecrans.TUILES["chambre"], ecrans.TUILES["salon"], ecrans.TUILE_VOLET
    for nom, tuile, cle in (("lumieres-chambre", chambre, "chambre"), ("lumieres-salon", salon, "salon"),
                            ("volet", volet, "volet"), ("volet-sans-position", volet, "volet-sans-position"),
                            ("volet-glisse", volet, "volet")):
        assert touche(nom, ecrans.roue_centres(*tuile, ecrans.ROUE_BOUTONS[cle])[-1]), nom
    for nom, tuile, cle, i in (("roue-lampe", chambre, "chambre", ecrans.ROUE_LUMINOSITE),
                               ("roue-lampe-couleurs", chambre, "chambre", ecrans.ROUE_COULEURS),
                               ("roue-volet", volet, "volet", ecrans.ROUE_POSITION),
                               ("roue-clim", chambre, "clim", ecrans.ROUE_MODE)):
        assert touche(nom, ecrans.roue_centres(*tuile, ecrans.ROUE_BOUTONS[cle])[i]), nom
        # Un toucher replie le second anneau, le suivant ferme la roue.
        assert list(par_nom[nom].fermer[:2]) == [ecrans.ROUE_FERMER] * 2, nom
    assert "maison-roue" in par_nom
    assert ecrans.ECRANS[-1].nom == "roue-clim", "climr reçu ne s'efface pas : dernière capture"


# ─────────────────────────────────────────────────────────────────────────────
# Boutons et commandes
# ─────────────────────────────────────────────────────────────────────────────

def _composer():
    return _fonction(_tuiles(), "roue_composer")


def test_types_et_options_de_la_roue():
    corps = _composer()
    lum = corps.split("case Type::LUM:", 1)[1].split("case Type::VOL:", 1)[0]
    vol = corps.split("case Type::VOL:", 1)[1].split("case Type::CLI:", 1)[0]
    cli = corps.split("case Type::CLI:", 1)[1].split("default:", 1)[0]
    assert "if (d.options & OPT_R) return 0;" in corps
    # Lampe : toutes, sauf à confirmer ; une bascule et les luminosités avec un variateur,
    # Allumer et Éteindre sans ; blancs et couleurs avec l'option c ; jamais Éteindre avec o.
    assert "if (d.options & OPT_K) return 0;" in lum
    assert "famille(RoueAction::LUMINOSITE, RoueIcone::LUMINOSITE);" in lum
    assert "if (!(d.options & OPT_O)) commande(RoueAction::ETEINDRE" in lum
    assert "if (allumee && !(d.options & OPT_O)) commande(RoueAction::ETEINDRE" in lum
    assert "if (d.options & OPT_C) {" in lum and "famille(RoueAction::COULEURS" in lum
    assert "if (d.options & OPT_K) return 0;" in vol
    for action in ("OUVRIR", "ARRETER", "FERMER"):
        assert f"commande(RoueAction::{action}," in vol
    assert "if (vol_position_connue(e)) famille(RoueAction::POSITION" in vol
    assert "if (capacites == nullptr) return 0;" in cli
    for f in ("MODE", "CONSIGNE", "OPTIONS"):
        assert f"famille(RoueAction::{f}, RoueIcone::{f});" in cli
    # « Maison » d'abord (sauf depuis lui), « Détails » en dernier, une commande au moins.
    debut = corps.split("switch (static_cast<Type>(d.type))", 1)[0]
    assert ('if (!depuis_maison) ajouter(RoueAction::MAISON, RoueIcone::MAISON, RoueGenre::LIEN, false, '
            'tr("Maison"));') in debut
    fin = corps.rsplit("if (n == premiere) return 0;", 1)[1]
    assert 'ajouter(RoueAction::REGLAGES, RoueIcone::REGLAGES, RoueGenre::LIEN, false, tr("Détails"));' in fin
    # Au plus : Maison + 4 + Détails.
    assert int(_const(_lire("Tab5", "tab5_custom.h"), "kRoueBoutons")) == 6


def test_choix_des_familles():
    t = _tuiles()
    assert re.findall(r"\d+", _const(t, "kRoueLuminosites")) == ["10", "25", "50", "75", "100"]
    assert re.findall(r"\d+", _const(t, "kRouePositions")) == ["25", "50", "75"]
    blancs = re.findall(r'\{"(\w+)", tr_noop\("([^"]+)"\)\}', _const(t, "kRoueBlancs"))
    assert blancs == [("warmwhite", "Chaud"), ("navajowhite", "Crème"), ("white", "Froid")]
    couleurs = re.findall(r'\{"(\w+)", nullptr\}', _const(t, "kRoueCouleurs"))
    assert couleurs == ["red", "orange", "gold", "green", "blue", "purple"]
    # Une seule liste de teintes (UI-8, lot L7) : lampe_teinte(), pour le popup lumière et
    # la roue — et les mêmes valeurs qu'avant, une à une (aucun changement à l'écran).
    teintes = {n: int(h, 16) for n, h in re.findall(r'\{"(\w+)", 0x([0-9A-F]{6})\}', _const(t, "kLampeTeintes"))}
    assert teintes == {
        "warmwhite": 0xFFC864, "navajowhite": 0xFFDEAD, "white": 0xFFFFFF, "gold": 0xFFD700,
        "orange": 0xFFA500, "orangered": 0xFF4500, "red": 0xFF2020, "deeppink": 0xFF1493,
        "magenta": 0xFF00FF, "blueviolet": 0x8A2BE2, "purple": 0xA855F7, "blue": 0x3B82F6,
        "cyan": 0x00E5FF, "springgreen": 0x00FF7F, "green": 0x22C55E}
    assert "x.pastille = lampe_teinte(p[k].nom);" in t
    # Le popup lumière : chaque pastille par son nom seul, dans l'ordre de la table.
    popup = _yaml("light_popup.yaml")
    assert "icon_color" not in popup and re.search(r"0x[0-9A-Fa-f]{6}", popup.split("COULEURS", 1)[1]) is None
    assert re.findall(r'color_name: "(\w+)"', popup) == list(teintes)
    assert re.findall(r'color_name: "(\w+)", name: "([^"]+)"', popup) == blancs
    for gabarit, propriete in (("light_color_preset_btn.yaml", "bg_color"), ("light_white_btn.yaml", "text_color")):
        assert (f"{propriete}: !lambda 'return lv_color_hex(lampe_teinte(\"${{color_name}}\"));'"
                in _yaml(gabarit)), gabarit
        assert "icon_color" not in _yaml(gabarit).split("button:", 1)[1], gabarit
    assert set(n for n, _ in blancs) | set(couleurs) <= set(teintes)
    assert int(_const(_lire("Tab5", "tab5_custom.h"), "kRoueChoix")) >= len(couleurs)


def test_modes_de_la_clim_ceux_du_popup():
    """Lettres de capacités (ADR-0026) → modes envoyés : ceux des boutons du popup clim,
    dans l'ordre chaud, froid, sec, ventilation ; pas d'« auto » (ni lettre ni bouton)."""
    modes = re.findall(r"\{'(\w)', \"(\w+)\", RoueIcone::(\w+)\}", _const(_tuiles(), "kRoueModesClim"))
    assert modes == [("h", "heat", "CHAUFFER"), ("c", "cool", "REFROIDIR"),
                     ("d", "dry", "SECHER"), ("f", "fan_only", "VENTILER")]
    popup = _yaml("climate_popup.yaml")
    assert {m for _, m, _ in modes} == set(re.findall(r"hvac_mode: \"(\w+)\"", popup))
    # « Chaud » d'une clim : sa traduction propre (« Heat », pas « Warm »).
    assert 'case \'h\': return tr_ctx("clim", "Chaud");' in _fonction(_tuiles(), "roue_legende_mode")


def test_commandes_envoyees_du_contrat():
    t = _tuiles()
    choisir = _fonction(t, "roue_tuile_choisir")
    envoyees = set(re.findall(r'envoyer_tuile\(rt\.r, rt\.t, "(\w+)"\)', choisir))
    envoyees |= set(re.findall(r'u\.envoyer\([\w.()]+, "(\w+)"', choisir))
    envoyees |= set(re.findall(r'choix\("(\w+)",', _fonction(t, "roue_choix")))
    bascules = _fonction(_lire("Tab5", "tab5_clim.cpp"), "clim_roue_bascules")
    envoyees |= set(re.findall(r"\{'\w', on, \"(\w+)\",", bascules))
    assert envoyees == {"allumer", "eteindre", "ouvrir", "arreter", "fermer", "luminosite_pct", "couleur",
                        "position", "mode", "consigne", "preset", "ventilation", "oscillation"}
    # Lampes et volets : le tableau de l'ADR-0023 ; clim : les commandes de son popup.
    assert envoyees - {"mode", "consigne", "preset", "ventilation", "oscillation"} <= _commandes_de_l_adr()
    popup = _yaml("climate_popup.yaml") + _yaml("climate_hvac_mode_btn.yaml") + _yaml(
        "climate_preset_toggle_btn.yaml") + _lire("Tab5", "tab5-scripts.yaml")
    for c in ("mode", "eteindre", "consigne", "preset", "ventilation", "oscillation"):
        assert f"commande: {c}" in popup, c
    # Clim du blueprint (option m) : l'emplacement « clim », comme clim_affichee_cle().
    assert 'u.envoyer(clim.emplacement(), "eteindre", "");' in choisir
    assert 'u.envoyer(est_clim ? clim.emplacement() : cle.s, env[j].commande, env[j].valeur);' in _fonction(
        t, "roue_tuile_choisir_choix")
    assert "if (d.options & OPT_M) return c;" in _fonction(t, "clim_cible")
    assert 'const char* emplacement() const { return r < 0 ? "clim" : cle.s; }' in t
    # Liens : le popup de la tuile ; le popup Maison par la routine unique des écrans.
    reglages = choisir.split("case RoueAction::REGLAGES:", 1)[1].split("return;", 1)[0]
    assert "tuile_ouvrir_popup(rt.r, rt.t);" in reglages
    maison = choisir.split("case RoueAction::MAISON:", 1)[1].split("return;", 1)[0]
    assert "g_roue_ui.ouvrir_ecran(static_cast<int>(Ecran::MAISON));" in maison
    assert "roue.ouvrir_ecran = [](int e) { id(tab5_ecran_ouvrir).execute(e); };" in _lire("Tab5", "tab5-tuiles.yaml")


def test_bascules_et_consignes_comme_le_popup():
    """Options de la clim : mêmes « actif » et mêmes valeurs que les bascules du popup ;
    consignes : deux pas de chaque côté, dans les bornes, envoyées comme le popup."""
    cards = _lire("Tab5", "tab5_clim.cpp")
    bascules = _fonction(cards, "clim_roue_bascules")
    for attendu in ("{'e', on, \"preset\", on ? \"none\" : \"away\"}",
                    "{'b', on, \"preset\", on ? \"none\" : \"boost\"}",
                    "{'q', on, \"ventilation\", on ? \"auto\" : \"quiet\"}",
                    "{'s', on, \"oscillation\", on ? \"stop\" : \"swing\"}",
                    "{'w', on, \"oscillation\", on ? \"stop\" : \"windnice\"}"):
        assert attendu in bascules, attendu
    assert 'p = clim_preset_actif(p, bouton) ? std::string("none") : std::string(bouton);' in cards
    assert 'f = clim_silence_actif(f) ? std::string("auto") : std::string("quiet");' in cards
    assert 's = clim_oscillation_actif(s) ? std::string("stop") : std::string("swing");' in cards
    assert 's = (s == "windnice") ? std::string("stop") : std::string("windnice");' in cards
    consignes = _fonction(cards, "clim_roue_consignes")
    assert "for (int k = -2; k <= 2; k++)" in consignes and "g.min - 0.001f" in consignes
    assert 'choix("consigne", clim_consigne_texte(valeurs[k]).c_str(), k == courant);' in _fonction(
        _tuiles(), "roue_choix")


def test_appui_long_ouvre_la_roue_puis_le_popup():
    appui = _fonction(_tuiles(), "tuile_appui_piece")
    # La roue d'abord, sinon la fenêtre du type (table kGestes, lot L7) : lum, vol, cli.
    table = re.search(r"constexpr GesteType kGestes\[\] = \{(.*?)\n\};", _tuiles(), re.S).group(1)
    assert re.findall(r"\{\w+, [^,]+, true, Fenetre::(\w+)\},\s*// (\w+)", table) == [
        ("LUMIERE", "lum"), ("VOLET", "vol"), ("CLIM", "cli")]
    long_ = appui.split("if (long_appui) {", 1)[1].split("\n    }\n", 1)[0]
    assert long_.index("if (g.roue && roue_de_la_tuile(r, t)) return;") < long_.index("ouvrir_fenetre(g.fenetre, d, r, t);")
    # Le toucher court ne passe jamais par la roue.
    assert appui.count("roue_de_la_tuile(") == 1
    # Depuis le popup Maison : sans le lien « Maison ».
    assert "if (long_appui && tuile_roue_ouvrir(r, t, ancre, true)) return;" in _fonction(
        _tuiles(), "tuile_appui_maison")


def test_ouverture_avec_ancre_pour_d_autres_vues():
    """tuile_roue_ouvrir(r, t, ancre, depuis_maison) : une autre vue (popup Maison) ouvre la
    roue d'une tuile autour de son propre widget ; faux = pas de roue, l'appelant ouvre le
    popup."""
    assert "bool tuile_roue_ouvrir(int r, int t, lv_obj_t* ancre, bool depuis_maison = false);" in _lire(
        "Tab5", "tab5_internal.h")
    corps = _fonction(_tuiles(), "roue_de_la_tuile")
    assert "g_tuiles_ui.carte_pastille[t]" in corps and "widgets_meteo(t, g, d, b);" in corps


def test_toucher_deplie_replie_ferme():
    """Une famille se déplie (une autre se replie) ; un choix ou une commande ferme la roue
    avant d'agir ; le voile et le moyeu replient, puis ferment."""
    roue = _roue()
    choisir = _fonction(roue, "roue_actions_choisir")
    assert "s.famille = s.famille == n ? -1 : n;" in choisir
    assert choisir.index("roue_actions_fermer();") < choisir.index("if (choisir != nullptr) choisir(n);")
    toucher = _fonction(roue, "roue_choix_toucher")
    assert toucher.index("roue_actions_fermer();") < toucher.index("choisir(famille, n);")
    replier = _fonction(roue, "roue_replier_ou_fermer")
    assert "s.famille = -1;" in replier and "roue_actions_fermer();" in replier
    actions = _yaml("roue_actions.yaml")
    assert 'close_lambda: "roue_replier_ou_fermer();"' in actions, "le voile"
    moyeu = actions.split("id: roue_moyeu\n", 1)[1].split("widgets:", 1)[0]
    assert "- lambda: 'roue_replier_ou_fermer();'" in moyeu, "le moyeu"
    assert "lambda: 'roue_actions_choisir(${n});'" in _yaml("roue_bouton.yaml")
    assert "lambda: 'roue_choix_toucher(${n});'" in _yaml("roue_choix.yaml")


# ─────────────────────────────────────────────────────────────────────────────
# Sous-fenêtre, fermetures, thème
# ─────────────────────────────────────────────────────────────────────────────

def test_sous_fenetre_et_fermetures():
    scripts = _lire("Tab5", "tab5-scripts.yaml")
    ligne = re.search(r"ModalRegistry::add\(id\(roue_actions\),\s*nullptr,\s*ModalRegistry::SUBWINDOW\);", scripts)
    assert ligne, "la roue est une sous-fenêtre du registre (ADR-0013)"
    assert "if (idle >= UIIdle::POPUP_MS && roue_actions_ouverte()) roue_actions_fermer();" in scripts
    assert "roue_actions_fermer();" in _fonction(_lire("Tab5", "tab5_anim.cpp"), "animate_popup_open")
    assert "roue_actions_fermer();" in _fonction(_tuiles(), "tuiles_definir")
    hardware = _lire("Tab5", "tab5-hardware.yaml")
    eteint = hardware.split("on_turn_off:", 1)[1].split("lvgl.pause", 1)[0]
    assert "roue_actions_fermer();" in eteint
    assert "roue_rejouer_theme();" in _fonction(_lire("Tab5", "tab5_theme.cpp"), "theme_rejouer_ui")
    # Console système (sans animate_popup_open) et retour automatique à la page météo.
    console = scripts.split("- id: tab5_console_ouvrir", 1)[1].split("- id:", 1)[0]
    assert "roue_actions_fermer();" in console
    retour = scripts.split("if (idle < UIIdle::FORECAST_MS) return;", 1)[1]
    assert retour.index("roue_actions_fermer();") < retour.index("reset_forecast_to_main_page(")


def test_un_etat_pousse_repeint_la_roue():
    """Un état de la tuile poussé par HA repeint la roue ouverte sur elle (boutons et
    choix courants, moyeu), la famille dépliée gardée."""
    assert "roue_tuile_etat(r, t);" in _fonction(_tuiles(), "peindre_tuile")
    corps = _fonction(_tuiles(), "roue_tuile_etat")
    assert "roue_actions_ouverte() && s_rt.r == r && s_rt.t == t" in corps and "roue_tuile_rejouer();" in corps
    assert "if (!roue_tuile_peindre(rt, true)) roue_actions_fermer();" in _fonction(_tuiles(), "roue_tuile_rejouer")
    assert "b[s.famille].icone == s.bouton[s.famille].icone" in _fonction(_roue(), "roue_ouvrir")


def test_un_bouton_n_est_repeint_que_si_son_aspect_change():
    """UI-7 (audit du 07/10/2026, lot L7) : chaque bouton garde le dernier aspect posé
    (Pose) ; aspect() et aspect_pastille() ne reposent leurs propriétés que s'il change —
    aspect, couleur, ou couleur de la palette lue (thème)."""
    roue = _roue()
    reposer = _fonction(roue, "reposer")
    assert reposer.index("if (pose == posee) return false;") < reposer.index("lv_obj_remove_local_style_prop(")
    # Les propriétés ne sont retirées qu'à cet endroit.
    assert roue.count("lv_obj_remove_local_style_prop(") == 1
    aspect = _fonction(roue, "aspect")
    assert "pose.haut = UIColor.GLASS_HI;" in aspect and "pose.bas = UIColor.GLASS_LO;" in aspect
    assert aspect.index("if (!reposer(b, posee, pose)) return;") < aspect.index("lv_obj_set_style_")
    pastille = _fonction(roue, "aspect_pastille")
    assert "pose.haut = courant ? UIColor.TEXT_PRIMARY : UIColor.GLASS_RIM;" in pastille
    assert pastille.index("if (!reposer(b, posee, pose)) return;") < pastille.index("lv_obj_set_style_")
    # Un Pose par bouton du premier anneau et par choix du second.
    assert "aspect(u.bouton[i], s_pose_bouton[i], a, s.couleur);" in roue
    assert "aspect_pastille(u.choix[j], s_pose_choix[j], k.pastille, k.courant);" in roue
    assert "aspect(u.choix[j], s_pose_choix[j], " in roue


def test_inclus_entre_les_cartes_et_les_popups():
    lvgl = _lire("Tab5", "tab5-lvgl.yaml")
    i = lvgl.index("ui_components/roue_actions.yaml")
    assert lvgl.index("ui_components/switches_card.yaml") < i < lvgl.index("ui_components/climate_popup.yaml")


# ─────────────────────────────────────────────────────────────────────────────
# Widgets et glyphes
# ─────────────────────────────────────────────────────────────────────────────

def test_widgets_et_leurs_pointeurs():
    custom = _lire("Tab5", "tab5_custom.h")
    n = int(_const(custom, "kRoueBoutons"))
    m = int(_const(custom, "kRoueChoix"))
    yaml = _yaml("roue_actions.yaml")
    assert re.findall(r"file: roue_bouton\.yaml, vars: \{ n: (\d) \}", yaml) == [str(i) for i in range(n)]
    assert re.findall(r"file: roue_choix\.yaml, vars: \{ n: (\d) \}", yaml) == [str(j) for j in range(m)]
    legendes = re.findall(r"file: roue_legende\.yaml, vars: \{ id: (\w+) \}", yaml)
    assert legendes == [f"roue_choix_{j}_legende" for j in range(m)] + ["roue_lien_0", "roue_lien_1"]
    tuiles = _lire("Tab5", "tab5-tuiles.yaml")
    attendus = [("fond", "roue_actions"), ("bande[0]", "roue_bande_0"), ("bande[1]", "roue_bande_1"),
                ("jauge", "roue_jauge"), ("moyeu", "roue_moyeu"), ("moyeu_icone", "roue_moyeu_icone"),
                ("moyeu_valeur", "roue_moyeu_valeur"), ("nom", "roue_nom"),
                ("lien_legende[0]", "roue_lien_0"), ("lien_legende[1]", "roue_lien_1")]
    for i in range(n):
        attendus += [(f"bouton[{i}]", f"roue_bouton_{i}"), (f"icone[{i}]", f"roue_bouton_{i}_icone"),
                     (f"point[{i}]", f"roue_bouton_{i}_point")]
    for j in range(m):
        attendus += [(f"choix[{j}]", f"roue_choix_{j}"), (f"choix_icone[{j}]", f"roue_choix_{j}_icone"),
                     (f"choix_texte[{j}]", f"roue_choix_{j}_texte"),
                     (f"choix_legende[{j}]", f"roue_choix_{j}_legende")]
    for champ, wid in attendus:
        assert f"roue.{champ} = id({wid});" in tuiles, champ
    assert "roue_brancher();" in tuiles
    # Le voile est celui des popups (ADR-0009), à 60 % : les tuiles restent visibles
    # dessous ; premier enfant, sous tout le reste.
    premier = yaml.split("widgets:", 1)[1].split("\n    - ", 2)[1]
    assert premier.startswith('!include { file: modal_scrim.yaml, vars: { scrim_opa: "60",')


def test_glyphes_de_mdi_font_36():
    glyphes = set(re.findall(r'return "\\U000(F[0-9A-F]{4})";', _fonction(_roue(), "glyphe_roue")))
    police = font_glyphs(Path(REPO) / "Tab5" / "tab5-styles.yaml")["mdi_font_36"]
    assert {f"{ord(c):05X}" for c in police} == glyphes
    for fichier in ("roue_bouton.yaml", "roue_choix.yaml"):
        assert "text_font: mdi_font_36" in _yaml(fichier)
    # Une icône par valeur de RoueIcone (AUCUNE exceptée).
    enum = re.search(r"enum class RoueIcone : uint8_t \{([^}]*)\}", _lire("Tab5", "tab5_internal.h")).group(1)
    valeurs = [v.strip() for v in enum.split(",") if v.strip()]
    assert valeurs[0] == "AUCUNE"
    for v in valeurs[1:]:
        assert f"case RoueIcone::{v}: return" in _fonction(_roue(), "glyphe_roue"), v


def test_adr_presente():
    texte = _lire(ADR)
    assert "discussions/278" in texte
    assert "tuile_roue_ouvrir" in texte
    assert "« Détails »" in texte and "Maison" in texte
