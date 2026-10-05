"""Polices d'affichage des thèmes (ADR-0029, lot 3) : Tab5/themes/_polices.yaml.

tools/police_theme.py mesure chaque police citée par un thème (fichier de Google Fonts,
fontTools) et choisit sa taille et la géométrie de l'horloge ; tools/gen_themes.py en
tire les polices compilées et la table kPolices de tab5_theme.cpp. Ce test refait la
géométrie de l'horloge depuis les métriques gardées dans le fichier, hors ligne, et
vérifie que Roboto, la police de l'état compilé, redonne les valeurs du YAML.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))

import gen_themes  # noqa: E402
import police_theme  # noqa: E402


def _mesures() -> dict:
    return gen_themes.lire_polices()


def _metriques(m: dict) -> dict:
    return {"unites_em": m["unites_em"], "ascendante": m["ascendante"], "glyphes": m["chiffres"]}


def test_roboto_redonne_la_geometrie_du_yaml():
    m = _mesures()[police_theme.REFERENCE]
    rouleau = (REPO / "Tab5" / "ui_components" / "clock_roller.yaml").read_text(encoding="utf-8")
    lvgl = (REPO / "Tab5" / "tab5-lvgl.yaml").read_text(encoding="utf-8")
    ys = {int(y) for y in re.findall(r"^\s+y: (-?\d+)\n\s+styles: \[style_police_horloge", rouleau, re.M)}
    dp = re.search(r"id: lbl_time_colon, text: \":\", align: TOP_LEFT, x: (\d+), y: (-?\d+)", lvgl)
    assert ys and dp, "le motif ne lit plus la géométrie de l'horloge"
    assert m["horloge"] == {"taille": 130, "y": ys.pop(), "x_deux_points": int(dp.group(1)),
                            "y_deux_points": int(dp.group(2))}
    assert m["date"]["taille"] == 45 and m["titre"]["taille"] == 32


def test_geometrie_de_l_horloge_recalculee_pour_chaque_police():
    for cle, m in _mesures().items():
        taille = m["horloge"]["taille"]
        assert police_theme.geometrie_horloge(_metriques(m), taille) == m["horloge"], cle
        # La plus grande taille qui tient : toutes celles au-dessus débordent du cadre.
        for t in range(taille + 1, police_theme.MAX_TAILLE["horloge"] + 1):
            assert police_theme.geometrie_horloge(_metriques(m), t) is None, f"{cle} : {t} px tiendrait"


def test_chaque_police_citee_est_mesuree():
    cites = {v for t in gen_themes.charger() for v in t.polices.values()}
    absentes = cites - set(_mesures())
    assert not absentes, f"lancer `python tools/police_theme.py` : {sorted(absentes)}"


def test_les_textes_d_affichage_sont_dans_le_jeu_mesure():
    """La couverture d'une police est relevée sur UNIVERS : un caractère hors de ce jeu
    (nouvelle traduction) serait demandé à ESPHome sans savoir s'il existe."""
    jeux = police_theme.jeux_par_role()
    hors = set("".join(jeux.values())) - set(police_theme.UNIVERS)
    assert not hors, f"élargir UNIVERS (tools/police_theme.py) : {''.join(sorted(hors))!r}"


def test_la_date_a_les_dix_chiffres():
    assert set(police_theme.CHIFFRES) <= set(police_theme.jeux_par_role()["date"])


def test_polices_generees_sans_glyphe_absent():
    """ESPHome refuse un glyphe absent du fichier : le générateur les retire (la Roboto
    du rôle les dessine)."""
    mesures = {
        "Roboto@700": _mesures()["Roboto@700"],
        "Essai Sans@700": {"manquants": "ğş", "horloge": {"taille": 120, "y": -20, "x_deux_points": 182,
                                                          "y_deux_points": 13},
                           "date": {"taille": 40}, "titre": {"taille": 30}},
    }
    # Ardoise (l'état compilé) et un thème d'essai seulement : les index des polices
    # ci-dessous ne dépendent pas du catalogue.
    ardoise = gen_themes.charger()[0]
    essai = gen_themes.Theme("essai", "Essai", 2, ardoise.modes,
                             polices={"horloge": "Essai Sans@700", "titre": "Essai Sans@700"})
    jeux = {"horloge": "0123456789:", "date": "Lun 08 Oct", "titre": "Météo ğş"}
    font_yaml, lambda_yaml, cpp = gen_themes.rendre_polices([ardoise, essai], mesures, jeux)
    texte = "\n".join(font_yaml)
    assert "id: police_essai_sans_700_120" in texte and "id: police_essai_sans_700_30" in texte
    assert "ğ" not in texte and "ş" not in texte
    assert "glyphs: '0123456789:'" in texte
    assert "police_essai_sans_700_30" in lambda_yaml[2]
    # Ardoise : les trois Roboto (0, 1, 2) ; l'essai : ses polices, la date en Roboto.
    assert cpp[-3].startswith("    {0, 1, 2, -23, 181, 10},")
    assert cpp[-2].startswith("    {3, 1, 4, -20, 182, 13},")
