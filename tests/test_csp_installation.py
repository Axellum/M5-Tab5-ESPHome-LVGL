# -*- coding: utf-8 -*-
"""Page de flashage (web/install/index.html) : la politique de sécurité du contenu (CSP) ne doit
pas se périmer en silence (lot J de l'audit du 30/09/2026).

La CSP autorise le script de la page par son empreinte et ESP Web Tools par son dossier de
jsDelivr. Modifier le script sans recalculer l'empreinte, ou changer la version d'ESP Web Tools
dans une seule des trois places (script, CSP, SRI), casserait la page sans erreur de build :
ces tests le font échouer avant la fusion.
"""
from __future__ import annotations

import base64
import hashlib
import re
from pathlib import Path

PAGE = Path(__file__).resolve().parent.parent / "web" / "install" / "index.html"


def _page() -> str:
    # Le navigateur lit le texte du script avec des fins de ligne LF, quel que soit le fichier.
    return PAGE.read_text(encoding="utf-8").replace("\r\n", "\n")


def _csp(page: str) -> str:
    m = re.search(r'<meta http-equiv="Content-Security-Policy" content="([^"]+)"', page)
    assert m, "la page n'a plus de balise Content-Security-Policy"
    return m.group(1)


def test_empreinte_du_script_de_la_page():
    page = _page()
    scripts = re.findall(r"<script>(.*?)</script>", page, re.S)
    assert len(scripts) == 1, "un seul script en ligne : un deuxième exigerait sa propre empreinte"
    attendue = "sha256-" + base64.b64encode(hashlib.sha256(scripts[0].encode("utf-8")).digest()).decode()
    assert f"'{attendue}'" in _csp(page), (
        f"le script de la page a changé : remplacer l'empreinte de la CSP par '{attendue}'")


def test_esp_web_tools_meme_version_partout():
    page = _page()
    csp = _csp(page)
    src = re.search(r'<script type="module" src="(https://cdn\.jsdelivr\.net/npm/esp-web-tools@([\d.]+)/[^"]+)"([^>]*)>', page)
    assert src, "le script d'ESP Web Tools n'est plus chargé depuis jsDelivr"
    url, version, reste = src.groups()
    assert f"https://cdn.jsdelivr.net/npm/esp-web-tools@{version}/" in csp, \
        "la CSP doit nommer le dossier de la version chargée d'ESP Web Tools"
    assert 'integrity="sha384-' in reste and 'crossorigin="anonymous"' in reste, \
        "le script d'entrée perd son empreinte SRI (la recalculer à chaque changement de version)"


def test_pas_d_autre_source_de_script_ni_gestionnaire_en_ligne():
    page = _page()
    csp = _csp(page)
    scripts_externes = re.findall(r'<script[^>]+src="([^"]+)"', page)
    assert all(u.startswith("https://cdn.jsdelivr.net/npm/esp-web-tools@") for u in scripts_externes), scripts_externes
    # un gestionnaire en ligne (onclick=…) serait bloqué par la CSP : la page cesserait de répondre
    assert not re.search(r"<[^>]+\son[a-z]+=", page), "gestionnaire d'événement en ligne"
    for directive in ("object-src 'none'", "base-uri 'none'", "form-action 'none'"):
        assert directive in csp
    assert "unsafe-inline" not in csp.split("script-src", 1)[1].split(";", 1)[0]
