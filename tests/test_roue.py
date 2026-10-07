# -*- coding: utf-8 -*-
"""Roue d'actions rapides (ADR-0036, 07/10/2026) : l'appui long d'une lampe à variateur,
d'un volet ou d'une clim ouvre une roue de boutons au-dessus de la tuile, dont « ⋯ » ouvre
le popup d'avant. Aucun compilateur ne vérifie ce qui suit ; ce fichier lit le C++ et le
YAML, comme les autres tests statiques :

- géométrie : constantes et table des sinus de Tab5/tab5_roue.cpp = celles de
  tools/rendu/ecrans.py (qui touche « ⋯ » dans le rendu) ; boutons dans l'écran, sans
  chevauchement, pour toutes les tuiles ;
- boutons par type (tableau de l'ADR) et commandes = celles du contrat (ADR-0023) et du
  popup clim, rien de nouveau ;
- sous-fenêtre du registre, fermée par l'inactivité, un popup, l'écran éteint, de
  nouvelles définitions ; repeinte au changement de thème ;
- six boutons dans le YAML, leurs pointeurs posés, glyphes de mdi_font_36.
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
    return _lire("Tab5", "tab5_tuiles.cpp")


def _const(source, nom):
    m = re.search(rf"constexpr [^=;]*?\b{nom}\b\s*(?:\[[^\]]*\])?\s*=\s*([^;]+);", source)
    assert m, f"constexpr {nom} introuvable"
    return m.group(1).strip()


# ─────────────────────────────────────────────────────────────────────────────
# Géométrie
# ─────────────────────────────────────────────────────────────────────────────

def test_constantes_du_rendu_egales_au_cpp():
    cpp = _roue()
    assert int(_const(cpp, "kEcranL")) == ecrans.ROUE_ECRAN[0]
    assert int(_const(cpp, "kEcranH")) == ecrans.ROUE_ECRAN[1]
    assert int(_const(cpp, "kRayon")) == ecrans.ROUE_RAYON
    assert int(_const(cpp, "kDiametre")) == ecrans.ROUE_DIAMETRE
    assert int(_const(cpp, "kMarge")) == ecrans.ROUE_MARGE
    assert int(_const(cpp, "kPasAngle")) == ecrans.ROUE_PAS_ANGLE
    assert int(_const(cpp, "kPivot")) == ecrans.ROUE_PIVOT
    table = tuple(int(x) for x in re.findall(r"\d+", _const(cpp, "kSin5").strip("{}")))
    assert table == ecrans.ROUE_SIN5


def test_table_des_sinus():
    assert len(ecrans.ROUE_SIN5) == 90 // ecrans.ROUE_PIVOT + 1
    for k, v in enumerate(ecrans.ROUE_SIN5):
        assert v == round(math.sin(math.radians(k * ecrans.ROUE_PIVOT)) * 32767), k


def test_halo_autour_de_l_arc():
    """Halo : rayon de l'arc + demi-bouton + 8 px, le même dans le C++ et le YAML."""
    halo = int(_const(_roue(), "kHalo"))
    assert halo == ecrans.ROUE_RAYON + ecrans.ROUE_DIAMETRE // 2 + 8
    yaml = _lire("Tab5", "ui_components", "roue_actions.yaml")
    bloc = yaml.split("id: roue_halo", 1)[1].split("- !include", 1)[0]
    assert f"width: {2 * halo}" in bloc and f"height: {2 * halo}" in bloc and f"radius: {halo}" in bloc
    assert "clickable: false" in bloc, "un toucher sur le halo doit fermer la roue"
    assert "shadow_width: 0" in bloc


def test_arc_centre_au_dessus_de_l_ancre():
    """Sans débordement : l'éventail est symétrique autour de la verticale de l'ancre,
    à 200 px de l'ancre (± 1 px d'arrondi), 30° entre deux boutons."""
    for n in (4, 5, 6):
        centres = ecrans.roue_centres(640, 572, n)
        assert len(centres) == n
        for i, (x, y) in enumerate(centres):
            a = math.radians(90 + (n - 1) * 15 - 30 * i)
            assert abs(x - (640 + 200 * math.cos(a))) <= 1 and abs(y - (572 - 200 * math.sin(a))) <= 1
            assert y < 572
        for (x1, y1), (x2, y2) in zip(centres, reversed(centres)):
            assert x1 - 640 == 640 - x2 and y1 == y2


def _ancres():
    """Centres des tuiles météo (bouton de la tuile) et des pastilles du mode HA, pour 1
    à 5 cartes centrées (switches_card.yaml : 230 px + 20 d'écart)."""
    ancres = [(140 + 250 * k, 572) for k in range(5)]
    for nb in range(1, 6):
        gauche = (1280 - (nb * 250 - 20)) // 2
        ancres += [(gauche + 250 * k + 115, 526) for k in range(nb)]
    return ancres


def test_boutons_dans_l_ecran_et_sans_chevauchement():
    r = ecrans.ROUE_DIAMETRE // 2
    for xa, ya in _ancres() + [(640, 60), (40, 40), (1240, 700)]:
        for n in (4, 5, 6):
            centres = ecrans.roue_centres(xa, ya, n)
            for x, y in centres:
                assert ecrans.ROUE_MARGE + r <= x <= 1280 - ecrans.ROUE_MARGE - r, (xa, ya, n)
                assert ecrans.ROUE_MARGE + r <= y <= 720 - ecrans.ROUE_MARGE - r, (xa, ya, n)
            if (xa, ya) in _ancres():
                for i in range(n - 1):
                    (x1, y1), (x2, y2) = centres[i], centres[i + 1]
                    assert math.hypot(x2 - x1, y2 - y1) >= ecrans.ROUE_DIAMETRE, (xa, ya, n, i)


def test_ancre_haute_arc_en_dessous():
    centres = ecrans.roue_centres(640, 100, 5)
    assert all(y > 100 for _, y in centres)


def test_le_rendu_touche_le_dernier_bouton():
    """Les écrans des popups de tuile ouvrent la roue puis touchent son « ⋯ »."""
    par_nom = {e.nom: e for e in ecrans.ECRANS}
    for nom, tuile, n in (("lumieres-chambre", ecrans.TUILES["chambre"], 5),
                          ("lumieres-salon", ecrans.TUILES["salon"], 5),
                          ("volet", ecrans.TUILE_VOLET, 5),
                          ("volet-sans-position", ecrans.TUILE_VOLET, 4),
                          ("volet-glisse", ecrans.TUILE_VOLET, 5)):
        etapes = par_nom[nom].etapes
        plus = ecrans.roue_centres(*tuile, n)[-1]
        assert any(isinstance(e, ecrans.Toucher) and (e.x, e.y) == plus for e in etapes), nom
    assert {"roue-lampe", "roue-volet", "roue-clim"} <= set(par_nom)
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
    assert "if (!(d.options & OPT_D) || (d.options & OPT_K)) return 0;" in lum
    assert "if (!(d.options & OPT_O)) ajouter(RoueAction::ETEINDRE" in lum
    assert "if (d.options & OPT_K) return 0;" in vol
    for action in ("OUVRIR", "ARRETER", "FERMER"):
        assert f"ajouter(RoueAction::{action}," in vol
    assert "if (vol_position_connue(e))" in vol
    assert "if (capacites == nullptr) return 0;" in cli
    # Moins de trois commandes : pas de roue ; « ⋯ » toujours le dernier.
    assert _const(_tuiles(), "kRoueMinActions") == "3"
    fin = corps.rsplit("if (n < kRoueMinActions) return 0;", 1)[1]
    assert "ajouter(RoueAction::PLUS," in fin
    assert re.findall(r"\d+", _const(_tuiles(), "kRoueLuminosites")) == ["10", "50", "100"]
    assert _const(_tuiles(), "kRouePosition") == "50"


def test_modes_de_la_clim_ceux_du_popup():
    """Lettres de capacités (ADR-0026) → modes envoyés : ceux des boutons du popup clim,
    dans l'ordre chaud, froid, sec, ventilation ; pas d'« auto » (ni lettre ni bouton)."""
    modes = re.findall(r"\{'(\w)', \"(\w+)\", RoueIcone::(\w+)\}", _const(_tuiles(), "kRoueModesClim"))
    assert modes == [("h", "heat", "CHAUFFER"), ("c", "cool", "REFROIDIR"),
                     ("d", "dry", "SECHER"), ("f", "fan_only", "VENTILER")]
    popup = _lire("Tab5", "ui_components", "climate_popup.yaml")
    assert {m for _, m, _ in modes} == set(re.findall(r"hvac_mode: \"(\w+)\"", popup))


def test_commandes_envoyees_du_contrat():
    corps = _fonction(_tuiles(), "roue_tuile_choisir")
    envoyees = set(re.findall(r'envoyer_tuile\(rt\.r, rt\.t, "(\w+)"\)', corps))
    envoyees |= set(re.findall(r'u\.envoyer\(\w+, "(\w+)"', corps))
    assert envoyees == {"eteindre", "ouvrir", "arreter", "fermer", "luminosite_pct", "position", "mode"}
    # Lampes et volets : le tableau de l'ADR-0023 ; clim : les commandes de son popup.
    assert envoyees - {"mode"} <= _commandes_de_l_adr()
    popup = _lire("Tab5", "ui_components", "climate_popup.yaml") + _lire(
        "Tab5", "ui_components", "climate_hvac_mode_btn.yaml")
    assert "commande: mode" in popup and "commande: eteindre" in popup
    # Clim du blueprint (option m) : l'emplacement « clim », comme clim_affichee_cle().
    assert 'const char* cle_clim = (d.options & OPT_M) ? "clim" : cle;' in corps
    # « ⋯ » : le popup de la tuile.
    assert "tuile_ouvrir_popup(rt.r, rt.t);" in corps.split("case RoueAction::PLUS:", 1)[1].split("return;", 1)[0]


def test_appui_long_ouvre_la_roue_puis_le_popup():
    appui = _fonction(_tuiles(), "tuile_appui_piece")
    assert "if (!roue_de_la_tuile(r, t)) popup_lumiere_ouvrir(r, t);" in appui
    assert "if (!roue_de_la_tuile(r, t)) popup_volet_ouvrir(r, t);" in appui
    assert "if (long_appui && roue_de_la_tuile(r, t)) return;" in appui
    # Le toucher court ne passe jamais par la roue.
    assert appui.count("roue_de_la_tuile(") == 3


def test_ouverture_avec_ancre_pour_d_autres_vues():
    """tuile_roue_ouvrir(r, t, ancre) : une autre vue (popup Maison) ouvre la roue d'une
    tuile autour de son propre widget ; faux = pas de roue, l'appelant ouvre le popup."""
    assert "bool tuile_roue_ouvrir(int r, int t, lv_obj_t* ancre);" in _lire("Tab5", "tab5_internal.h")
    corps = _fonction(_tuiles(), "roue_de_la_tuile")
    assert "g_tuiles_ui.carte_pastille[t]" in corps and "widgets_meteo(t, g, d, b);" in corps


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
    """Un état de la tuile poussé par HA repeint la roue ouverte sur elle (bouton courant)."""
    assert "roue_tuile_etat(r, t);" in _fonction(_tuiles(), "peindre_tuile")
    corps = _fonction(_tuiles(), "roue_tuile_etat")
    assert "roue_actions_ouverte() && s_rt.r == r && s_rt.t == t" in corps and "roue_tuile_rejouer();" in corps


def test_inclus_entre_les_cartes_et_les_popups():
    lvgl = _lire("Tab5", "tab5-lvgl.yaml")
    i = lvgl.index("ui_components/roue_actions.yaml")
    assert lvgl.index("ui_components/switches_card.yaml") < i < lvgl.index("ui_components/climate_popup.yaml")


# ─────────────────────────────────────────────────────────────────────────────
# Widgets et glyphes
# ─────────────────────────────────────────────────────────────────────────────

def test_six_boutons_et_leurs_pointeurs():
    n = int(_const(_lire("Tab5", "tab5_custom.h"), "kRoueBoutons"))
    yaml = _lire("Tab5", "ui_components", "roue_actions.yaml")
    assert re.findall(r"file: roue_bouton\.yaml, vars: \{ n: (\d) \}", yaml) == [str(i) for i in range(n)]
    tuiles = _lire("Tab5", "tab5-tuiles.yaml")
    for i in range(n):
        for champ, wid in (("bouton", f"roue_bouton_{i}"), ("icone", f"roue_bouton_{i}_icone"),
                           ("texte", f"roue_bouton_{i}_texte")):
            assert f"roue.{champ}[{i}] = id({wid});" in tuiles
    assert "roue_brancher();" in tuiles


def test_glyphes_de_mdi_font_36():
    glyphes = set(re.findall(r'return "\\U000(F[0-9A-F]{4})";', _fonction(_roue(), "glyphe_roue")))
    police = font_glyphs(Path(REPO) / "Tab5" / "tab5-styles.yaml")["mdi_font_36"]
    assert {f"{ord(c):05X}" for c in police} == glyphes
    assert "text_font: mdi_font_36" in _lire("Tab5", "ui_components", "roue_bouton.yaml")


def test_adr_presente():
    texte = _lire(ADR)
    assert "discussions/278" in texte
    assert "tuile_roue_ouvrir" in texte
