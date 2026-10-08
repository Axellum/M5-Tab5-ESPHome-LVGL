"""Polices d'affichage des thèmes (ADR-0029, lot 3) : Tab5/themes/_polices.yaml.

tools/police_theme.py mesure chaque police citée par un thème (fichier de Google Fonts,
fontTools) et choisit sa taille et la géométrie de l'horloge ; tools/gen_themes.py en
tire les polices compilées et la table kPolices de tab5_theme.cpp. Ce test refait la
géométrie de l'horloge depuis les métriques gardées dans le fichier, hors ligne, et
vérifie que Roboto, la police de l'état compilé, redonne les valeurs du YAML, et que
chaque police laisse les mêmes marges dans la tuile (05/10/2026).
"""
from __future__ import annotations

import math
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
    date = re.search(r"id: lbl_date, text: \"\", align: TOP_MID, y: (\d+)", lvgl)
    cadre_y = re.search(r"^  y: (\d+)\n  width: 75\n", rouleau, re.M)
    assert ys and dp and date and cadre_y, "le motif ne lit plus la géométrie de l'horloge"
    assert int(cadre_y.group(1)) == police_theme.CADRE_Y, "CADRE_Y ≠ y du cadre de clock_roller.yaml"
    # Le YAML est l'état compilé du thème Ardoise : géométrie de Roboto remontée du jambage
    # de sa date (gen_themes.py fait de même pour chaque thème).
    geo = police_theme.avec_jambage(m["horloge"], m["date"]["jambage"])
    assert geo == {"taille": 130, "y": ys.pop(), "cadre_y": police_theme.CADRE_Y, "dx": 0,
                   "x_deux_points": int(dp.group(1)), "y_deux_points": int(dp.group(2))}
    assert (m["date"]["taille"], m["date"]["y"]) == (45, int(date.group(1))) and m["titre"]["taille"] == 32
    # kCadreX de tab5_theme.cpp (généré depuis X_CADRES) repose ces x : ce sont ceux du YAML.
    xs = tuple(int(x) for x in re.findall(r"file: ui_components/clock_roller\.yaml, vars: \{ d: \w+, x: (\d+) \}", lvgl))
    assert xs == police_theme.X_CADRES, f"X_CADRES {police_theme.X_CADRES} ≠ x des rouleaux de tab5-lvgl.yaml {xs}"


def _encre_metriques(m: dict) -> dict:
    """Encre des chiffres d'après les métriques (mêmes arrondis que geometrie_horloge)."""
    t = m["horloge"]["taille"]
    e = t / m["unites_em"]
    asc = math.ceil(m["ascendante"] * e)
    g = m["chiffres"]
    x0 = {c: (police_theme.CADRE_L - round(g[c][0] * e)) // 2 for c in police_theme.CHIFFRES}
    return {"haut": min(asc - math.ceil(g[c][4] * e) for c in police_theme.CHIFFRES),
            "gauche": min(x0[c] + math.floor(g[c][1] * e) for c in police_theme.CHIFFRES),
            "droite": max(x0[c] + math.ceil(g[c][2] * e) for c in police_theme.CHIFFRES)}


def test_l_encre_visible_est_dans_celle_des_metriques():
    """encre_visible() (FreeType, bpp 2) ne peut que rogner l'encre des métriques : au plus
    une rangée ou une colonne de pixels trop pâles (± 1 px d'arrondi de FreeType)."""
    for cle, m in _mesures().items():
        v, f = m["encre"], _encre_metriques(m)
        assert len(v["gauche"]) == len(v["droite"]) == 10, cle
        assert -1 <= v["haut"] - f["haut"] <= 2, cle
        assert -1 <= min(v["gauche"]) - f["gauche"] <= 2 and -1 <= f["droite"] - max(v["droite"]) <= 2, cle


def test_marges_egales_dans_la_tuile_horloge():
    """Demande d'Axel (05/10/2026) : la même marge en haut (encre visible des chiffres) et
    en bas (ligne de base de la date), l'encre de HH:MM centrée entre la gauche et la
    droite, quelle que soit la police. Tuile de 401 × 210 à bordure de 1 px :
    theme_polices() retranche la bordure réelle du thème. Vérifié sur la galerie de la CI
    (17 thèmes mesurables : haut prévu = haut mesuré, au pixel)."""
    marge, x_cadres = police_theme.MARGE, police_theme.X_CADRES
    for cle, m in _mesures().items():
        h = m["horloge"]
        v = m["encre"]
        assert 1 + h["cadre_y"] + h["y"] + v["haut"] == marge, f"{cle} : encre des chiffres pas à {marge} px du haut"
        assert 1 + police_theme.DATE_BASE == 210 - marge, "ligne de base de la date pas à MARGE px du bas"
        # « 00:00 » : marges gauche et droite à 3 px près ; en moyenne sur les heures, dx est
        # le meilleur décalage (un px de plus ou de moins écarte davantage).
        a_gauche = 1 + x_cadres[0] + h["dx"] + v["gauche"][0]
        a_droite = 401 - (1 + x_cadres[3] + h["dx"] + v["droite"][0])
        assert abs(a_droite - a_gauche) <= 3, f"{cle} : « 00:00 » pas centré ({a_gauche} / {a_droite} px)"
        e = police_theme.ecart_horizontal
        assert e(v, h["dx"]) <= min(e(v, h["dx"] - 1), e(v, h["dx"] + 1)), cle
        # Le chiffre qui arrive traverse tout le cadre : il reste au-dessus de l'encre de
        # la date (au plus 40 px au-dessus de sa ligne de base, à 45 px).
        assert 0 <= h["cadre_y"] and h["cadre_y"] + police_theme.CADRE_H <= police_theme.DATE_BASE - 40, cle


def test_horloge_a_la_marge_des_jambages_de_la_date_dans_chaque_theme():
    """Demande d'Axel (05/10/2026) : la marge du haut de l'horloge (encre visible des
    chiffres) égale celle du bas des jambages de la date (g, j, p, q, y) au bas de la
    tuile ; la date ne bouge pas, l'horloge monte d'autant. Lu dans la table générée
    (tab5_theme.cpp), avec la police d'heure et la police de date de chaque thème, qui ne
    sont pas toujours les mêmes."""
    cpp = (REPO / "Tab5" / "tab5_theme.cpp").read_text(encoding="utf-8")
    rangees = re.findall(r"^    \{(-?\d+(?:, -?\d+){8})\},  // (\w+)$", cpp, re.M)
    themes = {t.fichier: t for t in gen_themes.charger()}
    assert [f for _, f in rangees] == list(themes), "table kPolices pas dans l'ordre des thèmes"
    mesures = _mesures()
    for valeurs, fichier in rangees:
        _, _, _, y, _, y_dp, _, cadre_y, _ = (int(v) for v in valeurs.split(", "))
        polices = themes[fichier].polices
        heure = mesures[polices.get("horloge", police_theme.REFERENCE)]
        date = mesures[polices.get("date", police_theme.REFERENCE)]
        haut = 1 + cadre_y + y + heure["encre"]["haut"]
        bas = 210 - (1 + police_theme.DATE_BASE + date["date"]["jambage"])
        assert haut == bas, f"{fichier} : {haut} px au-dessus de l'heure, {bas} px sous les jambages de la date"
        assert y_dp == cadre_y + y, f"{fichier} : « : » pas à la hauteur des chiffres"
        # Le cadre peut mordre sur la bordure (Pacifico : jambages de 20 px), pas en sortir.
        assert cadre_y >= -1, fichier


def test_ligne_de_base_de_la_date_au_meme_endroit_pour_chaque_police():
    # Marges égales dans la tuile horloge (05/10/2026) : quelle que soit l'ascendante de
    # la police (de 40 à 53 px à 45 px), la ligne de base reste à DATE_BASE.
    for cle, m in _mesures().items():
        d = m["date"]
        assert d["y"] == police_theme.y_date(_metriques(m), d["taille"]), cle


def test_geometrie_de_l_horloge_recalculee_pour_chaque_police():
    for cle, m in _mesures().items():
        taille = m["horloge"]["taille"]
        assert police_theme.geometrie_horloge(_metriques(m), taille, m["encre"]) == m["horloge"], cle
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


def test_la_police_de_date_couvre_les_textes_de_l_accueil():
    """Températures, consigne, « Ok Nabu », carte centrale et titres des prévisions sont
    dans la police de la date depuis le 05/10/2026 : l'ASCII et « ° » au moins, dans
    chaque police de thème (ce qui manque au fichier est dessiné par roboto_45_b)."""
    assert set(police_theme.JEU_TEXTE_BASE) <= set(police_theme.jeux_par_role()["date"])


def test_polices_generees_sans_glyphe_absent():
    """ESPHome refuse un glyphe absent du fichier : le générateur les retire (la Roboto
    du rôle les dessine)."""
    mesures = {
        "Roboto@700": _mesures()["Roboto@700"],
        "Essai Sans@700": {"manquants": "ğş", "horloge": {"taille": 120, "y": -20, "cadre_y": 25, "dx": 1,
                                                          "x_deux_points": 182, "y_deux_points": 13},
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
    assert lambda_yaml[-1].endswith(", horloge, id(lbl_date));")
    # Ardoise : les trois Roboto (0, 1, 2) ; l'essai : ses polices, la date en Roboto (son y).
    # Les cadres et le « : » remontent du jambage de la date (Roboto 45 : 9 px).
    assert cpp[-3].startswith("    {0, 1, 2, -23, 181, -6, 135, 17, 0},")
    assert cpp[-2].startswith("    {3, 1, 4, -20, 182, 4, 135, 16, 1},")
    assert "static constexpr int16_t kCadreX[] = {27, 102, 222, 297};" in cpp


# --- Polices figées dans le dépôt (audit du 07/10/2026, DO-15) -----------------------
# Avant le 08/10/2026, le firmware compilait `gfonts://Famille@graisse` : ESPHome
# redemandait la police à Google Fonts chaque jour, et une nouvelle version changeait le
# firmware en silence. Les fichiers vivent maintenant dans Tab5/fonts/.

FONTS = REPO / "Tab5" / "fonts"
RE_FILE = re.compile(r"^\s*-?\s*file:\s*\"?([^\"\s#]+)\"?", re.M)


def _fichiers_police() -> list[str]:
    """Chaque `file:` de police des YAML du firmware et du rendu hors tablette."""
    sources = list((REPO / "Tab5").glob("*.yaml")) + list((REPO / "Tab5" / "ui_components").glob("*.yaml"))
    sources += [REPO / "tab5-ha-hmi.yaml", REPO / "tab5-rendu-host.yaml"]
    return [m.group(1) for p in sources for m in RE_FILE.finditer(p.read_text(encoding="utf-8"))
            if m.group(1).lower().endswith((".ttf", ".otf")) or "://" in m.group(1)]


def test_aucune_police_telechargee_a_la_compilation():
    fichiers = _fichiers_police()
    assert len(fichiers) > 40, "le motif ne lit plus les `file:` des polices"
    distantes = sorted({f for f in fichiers if "://" in f})
    assert not distantes, f"police téléchargée à chaque compilation (la figer dans Tab5/fonts/) : {distantes}"
    absentes = sorted({f for f in fichiers if not (REPO / f).is_file()})
    assert not absentes, absentes


def test_empreinte_des_polices_figees():
    """Le fichier compilé est celui que tools/police_theme.py a mesuré (sha256 de
    _polices.yaml) : une police remplacée sans nouvelle mesure échoue ici."""
    import hashlib
    for cle, m in _mesures().items():
        chemin = police_theme.fichier(*cle.rsplit("@", 1))
        assert chemin.is_file(), f"{cle} : {chemin.relative_to(REPO)} absent"
        assert hashlib.sha256(chemin.read_bytes()).hexdigest()[:16] == m["sha256"], \
            f"{cle} : {chemin.name} n'est plus le fichier mesuré (relancer tools/police_theme.py)"


def test_chaque_police_figee_sert_et_garde_sa_licence():
    from urllib.parse import urlparse

    from fontTools.ttLib import TTFont
    utilises = {Path(f).name for f in _fichiers_police()}
    licence = (FONTS / "OFL.txt").read_text(encoding="utf-8")
    assert "SIL OPEN FONT LICENSE Version 1.1" in licence
    for ttf in sorted(FONTS.glob("*.ttf")):
        assert ttf.name in utilises, f"{ttf.name} : compilé nulle part (poids mort dans le dépôt)"
        nom = TTFont(ttf)["name"]
        url = urlparse(nom.getDebugName(14) or "")
        assert (url.hostname, url.path.rstrip("/")) in {("scripts.sil.org", "/OFL"), ("openfontlicense.org", "")}, \
            f"{ttf.name} : licence {url.geturl()!r} (SIL OFL attendue)"
        assert nom.getDebugName(0).strip() in licence, f"{ttf.name} : copyright absent de Tab5/fonts/OFL.txt"
