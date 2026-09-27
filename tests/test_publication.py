# -*- coding: utf-8 -*-
"""Publication des firmwares et page de flashage (lot 6c, ADR-0022).

- tools/publication/preparer.py : binaires renommés par révision d'écran, manifeste
  réécrit et contrôlé ;
- tools/publication/pages.py : canaux stable et bêta choisis parmi les releases 3.x,
  site reconstruit depuis leurs fichiers ;
- cohérence entre le workflow, les révisions d'écran, la page et les packages de
  publication (mise à jour dans les seuls firmwares publiés)."""
import hashlib
import json
import os
import re
import sys
from pathlib import Path

import pytest
import yaml

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools" / "publication"))

import preparer  # noqa: E402
import pages  # noqa: E402


class _Chargeur(yaml.SafeLoader):
    """YAML ESPHome sans résoudre ses balises (!include, !lambda…)."""


_Chargeur.add_multi_constructor(
    "!", lambda c, s, n: c.construct_mapping(n) if isinstance(n, yaml.MappingNode) else {"!" + s: c.construct_scalar(n)})


def _yaml(*chemin):
    return yaml.load((REPO.joinpath(*chemin)).read_text(encoding="utf-8"), Loader=_Chargeur)


def _build_action(dossier: Path, version="3.0.0", name="axellum.tab5-ha-hmi", puce="ESP32-P4") -> Path:
    """Sortie factice de esphome/build-action (complete-manifest)."""
    dossier.mkdir(parents=True)
    usine, ota = dossier / "tab5-ha-hmi-esp32p4.factory.bin", dossier / "tab5-ha-hmi-esp32p4.ota.bin"
    usine.write_bytes(b"usine" * 100)
    ota.write_bytes(b"ota" * 100)

    def emp(f):
        d = f.read_bytes()
        return {"md5": hashlib.md5(d).hexdigest(), "sha256": hashlib.sha256(d).hexdigest()}

    manifeste = {"name": name, "version": version, "home_assistant_domain": "esphome",
                 "new_install_prompt_erase": False,
                 "builds": [{"chipFamily": puce,
                             "ota": {"path": ota.name, **emp(ota), "summary": "3.0"},
                             "parts": [{"path": usine.name, "offset": 0, **emp(usine)}]}]}
    (dossier / "manifest.json").write_text(json.dumps(manifeste), encoding="utf-8")
    return dossier


def test_preparer_renomme_par_ecran_et_reecrit_le_manifeste(tmp_path):
    source = _build_action(tmp_path / "tab5-ha-hmi-esp32p4")
    chemin = preparer.preparer(source, "st7121", "3.0.0", tmp_path / "publie")
    m = json.loads(chemin.read_text(encoding="utf-8"))
    assert chemin.name == "manifest-st7121.json"
    assert m["builds"][0]["parts"][0]["path"] == "tab5-ha-hmi-st7121.factory.bin"
    assert m["builds"][0]["ota"]["path"] == "tab5-ha-hmi-st7121.ota.bin"
    assert m["new_install_prompt_erase"] is True
    for f in ("tab5-ha-hmi-st7121.factory.bin", "tab5-ha-hmi-st7121.ota.bin"):
        assert (tmp_path / "publie" / f).is_file()


@pytest.mark.parametrize("defaut", [{"version": "3.0.1"}, {"name": "autre.projet"}, {"puce": "ESP32-S3"}])
def test_preparer_refuse_un_manifeste_inattendu(tmp_path, defaut):
    source = _build_action(tmp_path / "b", **defaut)
    with pytest.raises(SystemExit):
        preparer.preparer(source, "st7123", "3.0.0", tmp_path / "publie")


def test_preparer_refuse_un_binaire_modifie(tmp_path):
    source = _build_action(tmp_path / "b")
    (source / "tab5-ha-hmi-esp32p4.ota.bin").write_bytes(b"autre chose")
    with pytest.raises(SystemExit):
        preparer.preparer(source, "st7123", "3.0.0", tmp_path / "publie")


def _rel(tag, pre=False, date="2026-10-01T00:00:00Z", brouillon=False):
    return {"tagName": tag, "isPrerelease": pre, "isDraft": brouillon, "publishedAt": date}


def test_canaux_stable_et_beta():
    releases = [
        _rel("v2.2.0", date="2026-09-27T09:00:00Z"),                   # avant les binaires
        _rel("v3.0.0-rc.1", pre=True, date="2026-09-28T00:00:00Z"),
        _rel("v3.0.0", date="2026-10-01T00:00:00Z"),
        _rel("v3.1.0-rc.1", pre=True, date="2026-10-10T00:00:00Z"),
        _rel("v3.2.0", date="2026-10-20T00:00:00Z", brouillon=True),   # brouillon : ignoré
        _rel("essai", date="2026-10-21T00:00:00Z"),                     # tag hors format
    ]
    assert pages.choisir(releases) == {"stable": "v3.0.0", "beta": "v3.1.0-rc.1"}
    # Une stable plus récente que la bêta : les deux canaux la servent.
    releases.append(_rel("v3.1.0", date="2026-10-15T00:00:00Z"))
    assert pages.choisir(releases) == {"stable": "v3.1.0", "beta": "v3.1.0"}


def test_premiere_pre_release_sans_stable():
    releases = [_rel("v2.2.0"), _rel("v3.0.0-rc.1", pre=True, date="2026-10-02T00:00:00Z")]
    assert pages.choisir(releases) == {"stable": None, "beta": "v3.0.0-rc.1"}


def test_site_assemble_depuis_les_fichiers_des_releases(tmp_path):
    web = tmp_path / "web"
    web.mkdir()
    (web / "index.html").write_text("page", encoding="utf-8")
    for tag, version in (("v3.0.0", "3.0.0"), ("v3.1.0-rc.1", "3.1.0-rc.1")):
        for ecran in preparer.ECRANS:
            source = _build_action(tmp_path / "builds" / tag / ecran, version=version)
            preparer.preparer(source, ecran, version, tmp_path / "assets" / tag)
    versions = pages.assembler(web, tmp_path / "assets", "v3.0.0", "v3.1.0-rc.1", tmp_path / "site")
    assert versions["stable"] == {"tag": "v3.0.0", "version": "3.0.0", "ecrans": list(preparer.ECRANS)}
    assert versions["beta"]["version"] == "3.1.0-rc.1"
    for canal in ("stable", "beta"):
        for ecran in preparer.ECRANS:
            m = json.loads((tmp_path / "site" / canal / ecran / "manifest.json").read_text(encoding="utf-8"))
            assert (tmp_path / "site" / canal / ecran / m["builds"][0]["ota"]["path"]).is_file()
    assert (tmp_path / "site" / "index.html").is_file()
    assert json.loads((tmp_path / "site" / "versions.json").read_text(encoding="utf-8")) == versions


def test_memes_revisions_partout():
    """Révisions d'écran : fichiers Tab5/ecran-*.yaml = outils = matrice = page."""
    fichiers = sorted(p.stem.removeprefix("ecran-") for p in (REPO / "Tab5").glob("ecran-*.yaml"))
    assert sorted(preparer.ECRANS) == fichiers == sorted(pages.ECRANS)
    flux = _yaml(".github", "workflows", "publication.yml")
    assert sorted(flux["jobs"]["firmware"]["strategy"]["matrix"]["ecran"]) == fichiers
    page = (REPO / "web" / "index.html").read_text(encoding="utf-8")
    assert sorted(re.findall(r'name="ecran" value="([a-z0-9]+)"', page)) == fichiers


def test_workflow_signe_avec_la_cle_du_projet():
    texte = (REPO / ".github" / "workflows" / "publication.yml").read_text(encoding="utf-8")
    flux = yaml.safe_load(texte)
    assert "openssl genrsa" not in texte, "jamais de clé jetable pour une release"
    assert "secrets.TAB5_CLE_SIGNATURE" in texte
    assert re.fullmatch(r"[0-9a-f]{64}", flux["env"]["EMPREINTE_CLE"])
    assert "verify-signature" in texte and "rm -f tab5_signature.pem" in texte
    etapes = flux["jobs"]["firmware"]["steps"]
    build = next(e for e in etapes if str(e.get("uses", "")).startswith("esphome/build-action"))
    subs = build["with"]["substitutions"]
    for cle in ("tab5_ecran=", "tab5_publication=", "tab5_version="):
        assert cle in subs
    assert build["with"]["complete-manifest"] is True
    assert build["with"]["version"] == "${{ env.ESPHOME_PUBLICATION }}"


def test_esphome_des_releases_pas_sous_le_plancher():
    flux = yaml.safe_load((REPO / ".github" / "workflows" / "publication.yml").read_text(encoding="utf-8"))
    fige = tuple(int(x) for x in flux["env"]["ESPHOME_PUBLICATION"].split("."))
    plancher = re.search(r"min_version:\s*([0-9.]+)", (REPO / "tab5-ha-hmi.yaml").read_text(encoding="utf-8"))
    assert fige >= tuple(int(x) for x in plancher.group(1).split("."))


def test_mise_a_jour_seulement_dans_les_firmwares_publies():
    assert _yaml("Tab5", "publication-locale.yaml") == {}
    for canal in ("stable", "beta"):
        inclus = _yaml("Tab5", f"publication-{canal}.yaml")["packages"]["maj"]
        assert inclus["file"] == "publication-commune.yaml" and inclus["vars"] == {"canal": canal}
    commune = _yaml("Tab5", "publication-commune.yaml")
    source = commune["update"][0]["source"]
    assert source.startswith("https://axellum.github.io/M5-Tab5-ESPHome-LVGL/${canal}/")
    assert source.endswith("/manifest.json") and "tab5_ecran" in source
    point = (REPO / "tab5-ha-hmi.yaml").read_text(encoding="utf-8")
    assert "!include Tab5/publication-${ tab5_publication | default('locale') }.yaml" in point


def test_page_suit_le_manifeste_du_site():
    page = (REPO / "web" / "index.html").read_text(encoding="utf-8")
    assert re.search(r"esp-web-tools@\d+\.\d+\.\d+/", page), "version d'ESP Web Tools figée"
    assert "`${canal}/${ecran}/manifest.json`" in page
    assert 'fetch("versions.json"' in page
