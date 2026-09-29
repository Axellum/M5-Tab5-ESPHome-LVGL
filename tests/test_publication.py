# -*- coding: utf-8 -*-
"""Publication des firmwares et page de flashage (lot 6c, ADR-0022).

- tools/publication/preparer.py : binaires renommés par révision d'écran, manifeste
  réécrit et contrôlé ;
- tools/publication/pages.py : canaux stable et bêta choisis parmi les releases 3.x,
  site reconstruit depuis leurs fichiers ;
- cohérence entre le workflow, les révisions d'écran, la page et les packages de
  publication (mise à jour dans les seuls firmwares publiés) ;
- tools/publication/archive_ha.py : l'archive Home Assistant de la release
  (tab5_home_assistant.zip, ADR-0024), jointe par le workflow."""
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
import archive_ha  # noqa: E402


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


# Fichiers qu'une release 3.x porte une fois publication.yml passé (écrits par preparer.py,
# vérifié dans test_site_assemble_depuis_les_fichiers_des_releases).
FICHIERS = [f for e in pages.ECRANS
            for f in (f"manifest-{e}.json", f"tab5-ha-hmi-{e}.factory.bin", f"tab5-ha-hmi-{e}.ota.bin")]


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


def test_release_sans_ses_fichiers_ignoree():
    """28/09/2026 : #219 mergée pendant la compilation de v3.1.0 ; le site l'a prise pour
    la stable et a échoué (« no assets to download »). Sans ses fichiers, une release
    est ignorée : les canaux restent sur la précédente."""
    releases = [
        dict(_rel("v3.0.1", date="2026-09-28T09:00:00Z"), assets=FICHIERS),
        dict(_rel("v3.1.0", date="2026-09-28T11:29:00Z"), assets=[]),
    ]
    assert pages.choisir(releases) == {"stable": "v3.0.1", "beta": "v3.0.1"}
    incompletes = {
        "une révision encore en cours": FICHIERS[:4],
        # gh release upload envoie en parallèle : les manifestes peuvent précéder les binaires.
        "manifestes sans leurs binaires": [f for f in FICHIERS if f.startswith("manifest-")],
        "sans l'écran de référence": [f for f in FICHIERS if "st7123" not in f],
    }
    for cas, fichiers in incompletes.items():
        releases[1]["assets"] = fichiers
        assert pages.choisir(releases) == {"stable": "v3.0.1", "beta": "v3.0.1"}, cas
    # Tout joint : servie. Un autre écran absent (révision ajoutée à ECRANS après la
    # release) ne l'écarte pas, sinon ajouter un écran viderait les canaux.
    for fichiers in (FICHIERS, [f for f in FICHIERS if "ili9881c" not in f]):
        releases[1]["assets"] = fichiers
        assert pages.choisir(releases) == {"stable": "v3.1.0", "beta": "v3.1.0"}


def test_site_lit_les_fichiers_des_releases():
    """Le déploiement du site lit l'API des releases, avec leurs fichiers déjà envoyés
    (état « uploaded ») : c'est ce qui permet à choisir() d'écarter une release encore
    en compilation."""
    site = (REPO / ".github" / "workflows" / "site.yml").read_text(encoding="utf-8")
    assert 'gh api "repos/$GITHUB_REPOSITORY/releases?per_page=100"' in site
    assert 'assets: [.assets[] | select(.state == "uploaded") | .name]' in site


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
        # Les fichiers que choisir() attend sont ceux que preparer.py écrit.
        assert sorted(p.name for p in (tmp_path / "assets" / tag).iterdir()) == sorted(FICHIERS)
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
    page = (REPO / "web" / "install" / "index.html").read_text(encoding="utf-8")
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
    """La page de flashage est dans install/ ; canaux et versions.json à la racine du site."""
    page = (REPO / "web" / "install" / "index.html").read_text(encoding="utf-8")
    assert re.search(r"esp-web-tools@\d+\.\d+\.\d+/", page), "version d'ESP Web Tools figée"
    assert "`../${canal}/${ecran}/manifest.json`" in page
    assert 'fetch("../versions.json"' in page


# --- Site : vitrine, images et référencement (révision de l'ADR-0022, 28/09/2026) ---

# Les fichiers google*.html sont ceux de vérification de Google Search Console, pas des pages.
PAGES_WEB = sorted(p for p in (REPO / "web").rglob("*.html") if not p.name.startswith("google"))


def _url_de_page(page: Path) -> str:
    return pages.SITE + page.relative_to(REPO / "web").as_posix().removesuffix("index.html")


def test_site_meme_adresse_que_les_firmwares():
    source = _yaml("Tab5", "publication-commune.yaml")["update"][0]["source"]
    assert source.startswith(pages.SITE)


def test_images_du_site():
    """Chaque image citée par une page est dans IMAGES, chaque image d'IMAGES sert, et
    son fichier existe dans docs/images/."""
    citees = set()
    for page in PAGES_WEB:
        texte = page.read_text(encoding="utf-8")
        citees |= {s.rsplit("images/", 1)[1] for s in pages.IMAGE_DE_PAGE.findall(texte)}
        citees |= set(re.findall(r'content="' + re.escape(pages.SITE) + r'images/([^"]+)"', texte))
    assert citees == set(pages.IMAGES)
    for fichier in pages.IMAGES.values():
        assert (REPO / "docs" / "images" / fichier).is_file(), fichier


@pytest.mark.parametrize("page", PAGES_WEB, ids=lambda p: p.relative_to(REPO / "web").as_posix())
def test_page_referencable(page):
    """Titre, description, adresse canonique, image de partage ; texte alternatif partout."""
    texte = page.read_text(encoding="utf-8")
    titre = re.search(r"<title>([^<]+)</title>", texte)
    assert titre and "M5Stack Tab5" in titre.group(1) and "Home Assistant" in titre.group(1)
    description = re.search(r'<meta name="description" content="([^"]+)"', texte)
    assert description and 70 <= len(description.group(1)) <= 300
    assert f'<link rel="canonical" href="{_url_de_page(page)}">' in texte
    assert f'<meta property="og:url" content="{_url_de_page(page)}">' in texte
    assert re.search(r'<meta property="og:image" content="' + re.escape(pages.SITE) + r'images/', texte)
    for img in re.findall(r"<img\b[^>]*>", texte):
        alt = re.search(r'alt="([^"]*)"', img)
        assert alt and len(alt.group(1)) >= 20, img
        assert re.search(r'width="\d+" height="\d+"', img), img


def test_plan_du_site_avec_les_images(tmp_path):
    pages.assembler(REPO / "web", tmp_path / "assets", None, None, tmp_path / "site", REPO / "docs" / "images")
    plan = (tmp_path / "site" / "sitemap.xml").read_text(encoding="utf-8")
    locs = re.findall(r"<loc>([^<]+)</loc>", plan)
    assert pages.SITE in locs and pages.SITE + "install/" in locs
    assert not any("google" in url for url in locs), "fichier de vérification hors du plan"
    images = re.findall(r"<image:loc>([^<]+)</image:loc>", plan)
    assert pages.SITE + "images/m5stack-tab5-home-assistant-wall-screen.jpg" in images
    for url in images:
        assert (tmp_path / "site" / url.removeprefix(pages.SITE)).is_file(), url
    assert json.loads((tmp_path / "site" / "versions.json").read_text(encoding="utf-8")) == {"stable": None, "beta": None}


def test_site_deploye_sans_compiler():
    """site.yml déploie le site seul (push, à la main) et après une publication ; il ne
    compile rien et ne touche pas aux fichiers des releases."""
    texte = (REPO / ".github" / "workflows" / "site.yml").read_text(encoding="utf-8")
    flux = yaml.safe_load(texte)
    declencheurs = flux[True]  # « on: » lu comme un booléen par YAML 1.1
    assert "workflow_call" in declencheurs and "workflow_dispatch" in declencheurs
    for chemin in ("web/**", "docs/images/**", "tools/publication/pages.py"):
        assert chemin in declencheurs["push"]["paths"]
    assert "build-action" not in texte and "release upload" not in texte
    assert "--images docs/images" in texte
    job = flux["jobs"]["pages"]
    assert job["environment"]["name"] == "github-pages"
    publication = yaml.safe_load((REPO / ".github" / "workflows" / "publication.yml").read_text(encoding="utf-8"))
    appel = publication["jobs"]["pages"]
    assert appel["uses"] == "./.github/workflows/site.yml" and appel["needs"] == "release"
    assert appel["permissions"] == job["permissions"]


def test_site_refuse_un_commit_deja_deploye():
    """GitHub Pages garde un déploiement par commit : une release sur le commit qu'un push
    venait de déployer (v3.2.0-rc.1, 28/09) s'est dite déployée, et le site est resté celui
    du push. Une version qui n'est pas un commit est refusée (404, #231). site.yml échoue
    donc avant de déployer un commit qui a déjà un déploiement réussi."""
    flux = yaml.safe_load((REPO / ".github" / "workflows" / "site.yml").read_text(encoding="utf-8"))
    job = flux["jobs"]["pages"]
    etapes = job["steps"]
    rang = {str(e.get("uses") or e.get("name")).split("@")[0]: i for i, e in enumerate(etapes)}
    garde = etapes[rang["Commit pas encore déployé"]]["run"]
    assert 'pages/deployments/$GITHUB_SHA' in garde and "succeed" in garde and "exit 1" in garde
    assert rang["Commit pas encore déployé"] < rang["actions/upload-pages-artifact"] < rang["actions/deploy-pages"]
    deploiement = etapes[rang["actions/deploy-pages"]]
    assert deploiement["id"] == "deploiement" and "with" not in deploiement
    assert job["environment"]["url"] == "${{ steps.deploiement.outputs.page_url }}"


# --- Archive Home Assistant de la release (ADR-0024, 28/09/2026) ---

def test_archive_ha_arborescence_de_config(tmp_path):
    """tab5_home_assistant.zip = l'arborescence de config/ : packages, custom_templates,
    blueprint, les optionnels à part (tab5_optionnel/, pas chargés par HA) et le LISEZMOI.
    Octet pour octet les fichiers du dépôt, qui n'ont plus de placeholder, sauf la ligne
    de la version des fichiers (« dépôt » → version de la release)."""
    import zipfile

    archive = archive_ha.construire("3.2.0", tmp_path)
    assert archive.name == "tab5_home_assistant.zip"
    with zipfile.ZipFile(archive) as z:
        noms = z.namelist()
        attendus = [chemin for _, chemin in archive_ha.fichiers()] + [archive_ha.LISEZMOI]
        assert noms == attendus
        for source, chemin in archive_ha.fichiers():
            attendu = source.read_bytes()
            if chemin == "packages/tab5_health.yaml":
                attendu = attendu.replace('"dépôt"  # >>>'.encode("utf-8"), b'"3.2.0"  # >>>')
            assert z.read(chemin) == attendu, chemin
        lisezmoi = z.read(archive_ha.LISEZMOI).decode("utf-8")
    ha = REPO / "HomeAssistant_Config"
    assert {f"packages/{p.name}" for p in (ha / "packages").glob("*.yaml")} <= set(noms)
    assert "custom_templates/tab5_calendar.jinja" in noms
    assert "blueprints/automation/tab5/tab5_emplacements.yaml" in noms
    assert "tab5_optionnel/volet_serre_tracking.yaml" in noms
    assert not any(n.startswith("packages/volet") for n in noms)
    assert "3.2.0" in lisezmoi and "packages: !include_dir_named packages" in lisezmoi
    assert archive_ha.placeholders() == []


def test_archive_ha_porte_sa_version(tmp_path):
    """Garde (f) de tab5_health.yaml : dans l'archive, « Tab5 · version des fichiers HA »
    vaut la version de la release ; dans le dépôt, « dépôt » (jamais comparé). Une seule
    ligne porte le marqueur."""
    import zipfile

    sante = (REPO / "HomeAssistant_Config" / "packages" / "tab5_health.yaml").read_text(encoding="utf-8")
    assert len(archive_ha.MARQUEUR_VERSION.findall(sante)) == 1
    marques = sum(len(archive_ha.MARQUEUR_VERSION.findall(p.read_text(encoding="utf-8")))
                  for p in (REPO / "HomeAssistant_Config" / "packages").glob("*.yaml"))
    assert marques == 1
    archive = archive_ha.construire("3.3.0-rc.1", tmp_path)
    with zipfile.ZipFile(archive) as z:
        texte = z.read("packages/tab5_health.yaml").decode("utf-8")
    assert "state: \"3.3.0-rc.1\"  # >>> version de l'archive" in texte
    assert '"dépôt"' not in texte


def test_archive_ha_reproductible(tmp_path):
    a = archive_ha.construire("3.2.0", tmp_path / "a").read_bytes()
    b = archive_ha.construire("3.2.0", tmp_path / "b").read_bytes()
    assert a == b


def test_archive_ha_refuse_un_ancien_tag(tmp_path, monkeypatch, capsys):
    """Un tag d'avant l'ADR-0024 (placeholders) : code 3, rien d'écrit, le workflow
    n'envoie pas d'archive sans échouer."""
    base = tmp_path / "HomeAssistant_Config"
    (base / "packages").mkdir(parents=True)
    (base / "packages" / "tab5_push.yaml").write_text("x: weather.VOTRE_VILLE\n", encoding="utf-8")
    monkeypatch.setattr(sys, "argv", ["archive_ha.py", "--version", "3.1.0", "--base", str(base),
                                      "--sortie", str(tmp_path / "sortie")])
    assert archive_ha.main() == 3
    assert not (tmp_path / "sortie").exists()
    assert "::warning::" in capsys.readouterr().out
    with pytest.raises(SystemExit):
        archive_ha.construire("v3.2", tmp_path)


def test_workflow_joint_l_archive_ha():
    """Job home-assistant : outil du commit du workflow, fichiers du tag, archive jointe à la
    release ; le site ne télécharge toujours que manifestes et binaires."""
    flux = yaml.safe_load((REPO / ".github" / "workflows" / "publication.yml").read_text(encoding="utf-8"))
    job = flux["jobs"]["home-assistant"]
    assert job["needs"] == "preparation" and job["permissions"] == {"contents": "write"}
    texte = yaml.dump(job, allow_unicode=True)
    assert "outils-workflow/tools/publication/archive_ha.py" in texte
    assert "--base tag/HomeAssistant_Config" in texte
    assert "gh release upload \"$TAG\" publie-ha/tab5_home_assistant.zip" in texte
    assert "home-assistant" not in str(flux["jobs"]["release"].get("needs"))
    site = (REPO / ".github" / "workflows" / "site.yml").read_text(encoding="utf-8")
    assert "--pattern 'manifest-*.json' --pattern 'tab5-ha-hmi-*.bin'" in site
