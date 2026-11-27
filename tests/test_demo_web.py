# -*- coding: utf-8 -*-
"""Démo web (ADR-0086) : la tablette virtuelle en WebAssembly et son Home Assistant simulé.

La compilation et l'essai dans Chromium tournent dans .github/workflows/demo-web.yml (il
faut emscripten). Ici, ce qui se vérifie sans eux :
- la page n'appelle que des fonctions que le firmware exporte (une fonction absente
  casse la page sans erreur de build) ;
- demo.json ne dépend pas du jour de l'export, et sa maison vit à l'instant des captures
  du rendu de la CI (rendu-host.yml) ;
- le site pose la démo de la release stable sous demo/, et rien sans son archive.
"""
from __future__ import annotations

import json
import re
import subprocess
import sys
import zipfile
from pathlib import Path

import pytest

import pages
from tests.commun import REPO

PAGE = REPO / "web" / "demo" / "index.html"
WEB_MAIN = REPO / "Tab5" / "web" / "web_main.cpp"
CONSTRUIRE = REPO / "tools" / "web" / "construire.py"
EXPORTER = REPO / "tools" / "web" / "exporter_demo.py"


def _exports() -> list[str]:
    texte = CONSTRUIRE.read_text(encoding="utf-8")
    bloc = re.search(r"^EXPORTS = \[(.*?)\]", texte, re.S | re.M)
    assert bloc, "EXPORTS introuvable dans construire.py"
    return re.findall(r'"(_\w+)"', bloc.group(1))


def test_la_page_appelle_des_fonctions_exportees():
    exports = set(_exports())
    appelees = set(re.findall(r"Module\.(_web_\w+)", PAGE.read_text(encoding="utf-8")))
    assert appelees, "aucune fonction du firmware trouvée dans la page"
    assert appelees <= exports, f"appelées par la page sans être exportées (construire.py) : {appelees - exports}"
    # « ccall('web_action', …) » passe aussi par un export.
    for nom in re.findall(r"ccall\('(\w+)'", PAGE.read_text(encoding="utf-8")):
        assert f"_{nom}" in exports


def test_chaque_export_existe_dans_le_firmware_web():
    texte = WEB_MAIN.read_text(encoding="utf-8")
    definies = set(re.findall(r"EMSCRIPTEN_KEEPALIVE\s+[\w:<>*\s]+?\b(web_\w+)\s*\(", texte))
    for nom in _exports():
        if nom in ("_main", "_malloc", "_free"):
            continue
        assert nom[1:] in definies, f"{nom} exporté par construire.py, absent de web_main.cpp"


def test_les_enveloppes_du_lien_existent():
    """Chaque --wrap=X de construire.py a son __wrap_X dans web_main.cpp (sinon : lien en échec
    seulement en CI, ou symbole d'origine appelé sans rien dire)."""
    enveloppes = re.findall(r"--wrap=(\w+)", CONSTRUIRE.read_text(encoding="utf-8"))
    texte = WEB_MAIN.read_text(encoding="utf-8")
    assert len(enveloppes) >= 4
    for nom in enveloppes:
        assert f"__wrap_{nom}(" in texte, nom
    # L'envoi vers HA, enveloppé par son nom C++ décoré (ENVOI_HA, « --wrap={ENVOI_HA} »).
    envoi = re.search(r'^ENVOI_HA = "(\w+)"', CONSTRUIRE.read_text(encoding="utf-8"), re.M)
    assert envoi and f"__wrap_{envoi.group(1)}(" in texte


def test_l_horloge_de_la_demo_est_celle_du_rendu():
    """demo.json et les captures du rendu montrent le même moment (même maison, mêmes heures)."""
    rendu = (REPO / ".github" / "workflows" / "rendu-host.yml").read_text(encoding="utf-8")
    depart = re.search(r'DEPART: "([\d-]+ [\d:]+)"', rendu)
    assert depart and "TZ: Europe/Paris" in rendu
    exporteur = EXPORTER.read_text(encoding="utf-8")
    demo = re.search(r'DEPART = _dt\.datetime\.fromisoformat\("([^"]+)"\)', exporteur)
    assert demo, "DEPART introuvable dans exporter_demo.py"
    demo = demo.group(1)
    assert demo.replace("T", " ").startswith(depart.group(1)), (demo, depart.group(1))
    # Heure de Paris le 16 juin : heure d'été.
    assert demo.endswith("+02:00")


@pytest.fixture(scope="module")
def demo_json(tmp_path_factory) -> dict:
    # Processus neuf : l'exporteur fige datetime.now() avant d'importer la démo.
    sortie = tmp_path_factory.mktemp("demo") / "demo.json"
    subprocess.run([sys.executable, str(EXPORTER), "--sortie", str(sortie)], check=True,
                   capture_output=True, cwd=REPO)
    return json.loads(sortie.read_text(encoding="utf-8"))


def test_demo_json_vit_a_l_heure_de_la_demo(demo_json):
    assert demo_json["horloge"] == "2026-06-16T07:45:00+02:00"
    assert {"zones", "scenes", "energie", "historique", "maison"} <= set(demo_json)
    heures = [d["payload"] for s in demo_json["scenes"][:1] for n, d in s["appels"]
              if n == "tab5_maj_previsions_heures_bulk"]
    assert heures and heures[0].startswith("0|07:00|"), heures[:1]
    jours = [d["payload"] for n, d in demo_json["scenes"][0]["appels"] if n == "tab5_maj_previsions_jours_bulk"]
    assert jours and jours[0].startswith("0|Auj|") and "|Mer|" in jours[0]  # un mardi


def _site(tmp_path: Path, zip_contenu: dict[str, bytes] | None) -> Path:
    web = tmp_path / "web"
    (web / "demo").mkdir(parents=True)
    (web / "demo" / "index.html").write_text("<html>démo</html>", encoding="utf-8")
    assets = tmp_path / "assets"
    source = assets / "v3.10.0"
    source.mkdir(parents=True)
    manifeste = {"version": "3.10.0", "builds": [{"parts": [{"path": "f.bin"}], "ota": {"path": "o.bin"}}]}
    (source / "manifest-st7123.json").write_text(json.dumps(manifeste), encoding="utf-8")
    (source / "f.bin").write_bytes(b"f")
    (source / "o.bin").write_bytes(b"o")
    if zip_contenu is not None:
        with zipfile.ZipFile(source / pages.DEMO_ARCHIVE, "w") as z:
            for nom, octets in zip_contenu.items():
                z.writestr(nom, octets)
    sortie = tmp_path / "site"
    pages.assembler(web, assets, "v3.10.0", None, sortie)
    return sortie


def test_le_site_pose_la_demo_de_la_stable(tmp_path):
    site = _site(tmp_path, {"tab5.js": b"js", "tab5.wasm": b"\0asm", "demo.json": b"{}"})
    assert sorted(p.name for p in (site / "demo").iterdir()) == ["demo.json", "index.html", "tab5.js", "tab5.wasm"]
    assert (site / "demo" / "tab5.wasm").read_bytes() == b"\0asm"
    assert "demo/" in (site / "sitemap.xml").read_text(encoding="utf-8")


def test_sans_archive_pas_de_demo(tmp_path):
    site = _site(tmp_path, None)
    assert not (site / "demo").exists()
    assert "demo/" not in (site / "sitemap.xml").read_text(encoding="utf-8")


def test_archive_inattendue_refusee(tmp_path):
    with pytest.raises(SystemExit):
        _site(tmp_path, {"tab5.js": b"js", "tab5.wasm": b"\0asm"})


def test_la_publication_joint_la_demo_et_le_site_la_prend():
    publication = (REPO / ".github" / "workflows" / "publication.yml").read_text(encoding="utf-8")
    assert "uses: ./.github/workflows/demo-web.yml" in publication
    site = (REPO / ".github" / "workflows" / "site.yml").read_text(encoding="utf-8")
    assert f"--pattern '{pages.DEMO_ARCHIVE}'" in site
    demo = (REPO / ".github" / "workflows" / "demo-web.yml").read_text(encoding="utf-8")
    assert pages.DEMO_ARCHIVE in demo
    for nom in pages.DEMO_FICHIERS:
        assert nom in demo
