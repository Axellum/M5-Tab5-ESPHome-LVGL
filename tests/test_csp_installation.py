# -*- coding: utf-8 -*-
"""Page de flashage (web/install/index.html) : la politique de sécurité du contenu (CSP) ne doit
pas se périmer en silence (lot J de l'audit du 30/09/2026).

La CSP autorise le script de la page par son empreinte et ESP Web Tools par son dossier de
jsDelivr. Modifier le script sans recalculer l'empreinte, ou changer la version d'ESP Web Tools
dans une seule des trois places (script, CSP, SRI), casserait la page sans erreur de build :
ces tests le font échouer avant la fusion. La page est lue par un vrai analyseur HTML, pas par
une expression régulière (casse des balises, attributs dans un autre ordre).
"""
from __future__ import annotations

import base64
import hashlib
import re
from html.parser import HTMLParser
from pathlib import Path

PAGE = Path(__file__).resolve().parent.parent / "web" / "install" / "index.html"
DOSSIER_JSDELIVR = "https://cdn.jsdelivr.net/npm/esp-web-tools@"


class _Page(HTMLParser):
    def __init__(self) -> None:
        super().__init__(convert_charrefs=False)
        self.csp: list[str] = []
        self.scripts: list[dict] = []  # {"attrs": {...}, "texte": "..."}
        self.gestionnaires: list[str] = []
        self._dans_script = False

    def handle_starttag(self, tag, attrs):
        attrs = {k.lower(): (v or "") for k, v in attrs}
        self.gestionnaires += [k for k in attrs if k.startswith("on")]
        if tag == "meta" and attrs.get("http-equiv", "").lower() == "content-security-policy":
            self.csp.append(attrs.get("content", ""))
        if tag == "script":
            self.scripts.append({"attrs": attrs, "texte": ""})
            self._dans_script = True

    def handle_endtag(self, tag):
        if tag == "script":
            self._dans_script = False

    def handle_data(self, data):
        if self._dans_script:
            self.scripts[-1]["texte"] += data


def _page() -> _Page:
    # Le navigateur lit le texte du script avec des fins de ligne LF, quel que soit le fichier.
    p = _Page()
    p.feed(PAGE.read_text(encoding="utf-8").replace("\r\n", "\n"))
    return p


def _csp(p: _Page) -> str:
    assert len(p.csp) == 1, "la page doit avoir une seule balise Content-Security-Policy"
    return p.csp[0]


def _empreinte(texte: str) -> str:
    return "sha256-" + base64.b64encode(hashlib.sha256(texte.encode("utf-8")).digest()).decode()


def test_empreinte_du_script_de_la_page():
    p = _page()
    en_ligne = [s for s in p.scripts if "src" not in s["attrs"]]
    assert len(en_ligne) == 1, "un seul script en ligne : un deuxième exigerait sa propre empreinte"
    attendue = _empreinte(en_ligne[0]["texte"])
    assert f"'{attendue}'" in _csp(p), (
        f"le script de la page a changé : remplacer l'empreinte de la CSP par '{attendue}'")


def test_esp_web_tools_meme_version_partout():
    p = _page()
    externes = [s["attrs"] for s in p.scripts if "src" in s["attrs"]]
    assert len(externes) == 1 and externes[0]["src"].startswith(DOSSIER_JSDELIVR), externes
    attrs = externes[0]
    version = re.match(re.escape(DOSSIER_JSDELIVR) + r"([\d.]+)/", attrs["src"])
    assert version, "le script d'ESP Web Tools n'est plus chargé depuis le dossier d'une version précise"
    assert f"{DOSSIER_JSDELIVR}{version.group(1)}/" in _csp(p), \
        "la CSP doit nommer le dossier de la version chargée d'ESP Web Tools"
    assert attrs.get("integrity", "").startswith("sha384-") and attrs.get("crossorigin") == "anonymous", \
        "le script d'entrée perd son empreinte SRI (la recalculer à chaque changement de version)"


def test_directives_strictes_et_pas_de_gestionnaire_en_ligne():
    p = _page()
    csp = _csp(p)
    # un gestionnaire en ligne (onclick=…) serait bloqué par la CSP : la page cesserait de répondre
    assert not p.gestionnaires, f"gestionnaire d'événement en ligne : {p.gestionnaires}"
    for directive in ("object-src 'none'", "base-uri 'none'", "form-action 'none'"):
        assert directive in csp
    assert "unsafe-inline" not in csp.split("script-src", 1)[1].split(";", 1)[0]
