# -*- coding: utf-8 -*-
"""Intégration « Tab5 » pour HACS : les fichiers HA installés en un clic (ADR-0035).

- custom_components/tab5/installation.py (pur, sans Home Assistant) : plan, sauvegarde,
  écriture, retrait, restauration à l'octet près, optionnels, copies du blueprint
  importées par URL, fichiers de l'utilisateur jamais touchés ;
- tools/publication/archive_hacs.py : le zip que HACS décompresse dans
  custom_components/tab5/ (code à la racine, manifest versionné, fichiers/ = les mêmes
  octets que tab5_home_assistant.zip) ;
- cohérence : manifest.json, hacs.json, traductions, constantes lues dans les packages.
Le comportement dans un vrai Home Assistant (rechargement, réparations, notification) est
vérifié par tools/installation_ha/verifier_integration.py (CI integration-hacs.yml)."""
import importlib.util
import json
import re
import sys
import zipfile
from pathlib import Path

import pytest
import yaml

REPO = Path(__file__).resolve().parent.parent
INTEGRATION = REPO / "custom_components" / "tab5"
sys.path.insert(0, str(REPO / "tools" / "publication"))

import archive_ha  # noqa: E402
import archive_hacs  # noqa: E402


def _module(nom: str):
    """Un module pur de l'intégration, chargé par son chemin (son __init__.py importe HA)."""
    spec = importlib.util.spec_from_file_location(f"tab5_{nom}", INTEGRATION / f"{nom}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module  # @dataclass relit son module dans sys.modules
    spec.loader.exec_module(module)
    return module


installation = _module("installation")
messages = _module("messages")
const = _module("const")


def _embarques(version="9.9.9", base=archive_ha.HA_DIR) -> dict[str, bytes]:
    return dict(archive_ha.entrees(version, base))


def _instantane(config: Path) -> dict[str, bytes]:
    return {f.relative_to(config).as_posix(): f.read_bytes()
            for f in sorted(config.rglob("*")) if f.is_file()}


def _installer(config: Path, embarques: dict[str, bytes], precedents=None, nom="20261007-120000_x"):
    plan = installation.planifier(config, embarques, precedents or {})
    sauvegarde = installation.appliquer(config, plan, nom)
    return plan, sauvegarde


# ─── Manifestes ──────────────────────────────────────────────────────────────

def test_manifest_de_l_integration():
    texte = (INTEGRATION / "manifest.json").read_text(encoding="utf-8")
    m = json.loads(texte)
    cles = list(m)
    assert cles[:2] == ["domain", "name"] and cles[2:] == sorted(cles[2:]), \
        "hassfest : domain, name, puis les autres clés dans l'ordre alphabétique"
    assert m["domain"] == const.DOMAIN == INTEGRATION.name
    assert m["version"] == "0.0.0", "la version vient de la release (archive_hacs.py), pas du dépôt"
    assert m["config_flow"] is True and m["single_config_entry"] is True
    assert m["requirements"] == [] and m["iot_class"] and m["codeowners"]


def test_hacs_json():
    hacs = json.loads((REPO / "hacs.json").read_text(encoding="utf-8"))
    autorisees = {"name", "content_in_root", "country", "filename", "hacs", "hide_default_branch",
                  "homeassistant", "persistent_directory", "render_readme", "zip_release"}
    assert set(hacs) <= autorisees, "clé inconnue : HACS refuse le dépôt (PREVENT_EXTRA)"
    assert hacs["zip_release"] is True and hacs["filename"] == archive_hacs.NOM
    assert hacs["hide_default_branch"] is True, "la branche par défaut n'a pas les fichiers HA"
    blueprint = (REPO / "HomeAssistant_Config" / installation.BLUEPRINT).read_text(encoding="utf-8")
    plancher = re.search(r"min_version:\s*(\S+)", blueprint).group(1)
    assert hacs["homeassistant"] == plancher, "même plancher de HA que le blueprint"


def test_traductions_completes():
    en = json.loads((INTEGRATION / "translations" / "en.json").read_text(encoding="utf-8"))
    fr = json.loads((INTEGRATION / "translations" / "fr.json").read_text(encoding="utf-8"))

    def cles(d, prefixe=""):
        return {prefixe + k for k in d} | {c for k, v in d.items() if isinstance(v, dict)
                                           for c in cles(v, prefixe + k + ".")}
    assert cles(en) == cles(fr), "en.json et fr.json : mêmes clés"
    assert set(en["config"]["step"]["user"]["data"]) == {const.CONF_FIRMWARE}
    assert set(en["options"]["step"]["init"]["data"]) == {const.CONF_FIRMWARE, const.CONF_REINSTALLER}
    issues = {v for k, v in vars(const).items() if k.startswith("ISSUE_")}
    assert set(en["issues"]) == issues, "une réparation par constante ISSUE_*"
    assert set(en["issues"][const.ISSUE_REDEMARRAGE]["fix_flow"]["error"]) == {const.ISSUE_CONFIGURATION}
    parametres = {const.ISSUE_PACKAGES: set(), const.ISSUE_CONFIGURATION: {"version", "signaler"}}
    for langue in (en, fr):
        # hassfest refuse une URL dans une traduction : elle passe par un paramètre.
        assert not re.search(r"https?://", json.dumps(langue, ensure_ascii=False))
        for cle, issue in langue["issues"].items():
            textes = json.dumps(issue, ensure_ascii=False)
            assert set(re.findall(r"\{(\w+)\}", textes)) == parametres.get(cle, {"version"}), cle


def test_constantes_lues_dans_les_packages():
    ha = REPO / "HomeAssistant_Config" / "packages"
    assert f"'{const.MODELE_TABLETTE}'" in (ha / "tab5_evenements.yaml").read_text(encoding="utf-8")
    sante = (ha / f"{const.PACKAGE_TEMOIN}.yaml").read_text(encoding="utf-8")
    assert f"default_entity_id: {const.CAPTEUR_VERSION}" in sante
    assert installation.PACKAGE_VERSION == f"packages/{const.PACKAGE_TEMOIN}.yaml"
    assert installation.VERSION_FICHIERS.findall(sante) == ["dépôt"]
    assert installation.SIGNATURE_BLUEPRINT in (
        REPO / "HomeAssistant_Config" / installation.BLUEPRINT).read_text(encoding="utf-8")


def test_modules_purs_sans_home_assistant():
    for nom in ("installation", "messages", "const"):
        texte = (INTEGRATION / f"{nom}.py").read_text(encoding="utf-8")
        assert not re.search(r"^\s*(from|import)\s+(homeassistant|\.)", texte, re.M), nom


# ─── Installation ────────────────────────────────────────────────────────────

def test_premiere_installation(tmp_path):
    (tmp_path / "packages").mkdir()
    (tmp_path / "packages" / "perso.yaml").write_text("input_boolean:\n  a: {}\n", encoding="utf-8")
    embarques = _embarques()
    plan, sauvegarde = _installer(tmp_path, embarques)
    assert sauvegarde is None, "rien à sauvegarder sur un config/ sans fichiers Tab5"
    hors_optionnels = {c for c in embarques if not c.startswith(installation.OPTIONNEL)}
    assert set(plan.ecrire) == hors_optionnels and not plan.retirer and not plan.modifies
    for c in hors_optionnels:
        assert (tmp_path / c).read_bytes() == embarques[c]
    assert not (tmp_path / "packages" / "volet_serre_tracking.yaml").exists(), \
        "un optionnel n'est posé que s'il est déjà là"
    assert (tmp_path / "packages" / "perso.yaml").read_text(encoding="utf-8").startswith("input_boolean")
    assert installation.version_installee(tmp_path) == "9.9.9"
    assert not list(tmp_path.rglob("*" + installation.TEMPORAIRE))


def test_mise_a_jour_modifie_retire_puis_restaure(tmp_path):
    v1 = _embarques("9.9.9")
    plan1, _ = _installer(tmp_path, v1)
    # À la main : un package modifié, l'optionnel copié, une copie du blueprint importée
    # par son URL, et un blueprint homonyme d'un autre auteur.
    tv = tmp_path / "packages" / "tab5_tv.yaml"
    tv.write_bytes(tv.read_bytes() + b"\n# ajout perso\n")
    optionnel = "tab5_optionnel/volet_serre_tracking.yaml"
    (tmp_path / "packages" / "volet_serre_tracking.yaml").write_bytes(b"# ancienne copie\n")
    importe = tmp_path / "blueprints" / "automation" / "Axellum" / "tab5_emplacements.yaml"
    importe.parent.mkdir(parents=True)
    importe.write_bytes(b"blueprint:\n  source_url: https://github.com/Axellum/M5-Tab5-ESPHome-LVGL/blob/x\n")
    autre = tmp_path / "blueprints" / "automation" / "quelquun" / "tab5_emplacements.yaml"
    autre.parent.mkdir(parents=True)
    autre.write_bytes(b"blueprint:\n  name: autre\n")

    # Version 2 : un fichier de moins (retiré par la release), les autres versionnés.
    v2 = _embarques("9.9.10")
    del v2["packages/tab5_micro_absence.yaml"]
    avant = _instantane(tmp_path)
    plan2, sauvegarde = _installer(tmp_path, v2, plan1.installes, "20261008-090000_9.9.9")

    assert plan2.modifies == ["packages/tab5_tv.yaml"]
    assert plan2.retirer == ["packages/tab5_micro_absence.yaml"]
    assert "packages/volet_serre_tracking.yaml" in plan2.ecrire
    assert (tmp_path / "packages" / "volet_serre_tracking.yaml").read_bytes() == v2[optionnel]
    assert importe.read_bytes() == v2[installation.BLUEPRINT], "copie importée par URL mise à jour"
    assert autre.read_bytes() == b"blueprint:\n  name: autre\n", "blueprint d'un autre : intact"
    assert not (tmp_path / "packages" / "tab5_micro_absence.yaml").exists()
    assert installation.version_installee(tmp_path) == "9.9.10"
    assert sauvegarde == tmp_path / installation.SAUVEGARDES / "20261008-090000_9.9.9"
    assert (sauvegarde / "packages" / "tab5_tv.yaml").read_bytes().endswith(b"# ajout perso\n")
    assert (sauvegarde / "packages" / "tab5_micro_absence.yaml").is_file()

    installation.restaurer(tmp_path, plan2, sauvegarde)
    apres = {c: d for c, d in _instantane(tmp_path).items()
             if not c.startswith(installation.SAUVEGARDES + "/")}
    assert apres == avant, "restaurer() remet config/ à l'octet près"


def test_ecriture_impossible_rien_a_moitie(tmp_path):
    v1 = _embarques("9.9.9")
    plan1, _ = _installer(tmp_path, v1)
    # Une cible qui ne peut pas être écrite (un dossier à sa place), au milieu de la liste.
    bloque = tmp_path / "packages" / "tab5_reveil.yaml"
    bloque.unlink()
    bloque.mkdir()
    avant = _instantane(tmp_path)
    plan2 = installation.planifier(tmp_path, _embarques("9.9.10"), plan1.installes)
    with pytest.raises(OSError):
        installation.appliquer(tmp_path, plan2, "20261009-000000_9.9.9")
    apres = {c: d for c, d in _instantane(tmp_path).items()
             if not c.startswith(installation.SAUVEGARDES + "/")}
    assert apres == avant, "échec au milieu : les fichiers déjà écrits sont remis"
    assert installation.version_installee(tmp_path) == "9.9.9"


def test_meme_version_rien_a_ecrire(tmp_path):
    v1 = _embarques()
    plan1, _ = _installer(tmp_path, v1)
    plan2 = installation.planifier(tmp_path, v1, plan1.installes)
    assert plan2.vide and not plan2.modifies and len(plan2.identiques) == len(plan1.ecrire)


def test_optionnel_retire_par_l_utilisateur_pas_remis(tmp_path):
    (tmp_path / "packages").mkdir()
    (tmp_path / "packages" / "volet_serre_tracking.yaml").write_bytes(b"# copie\n")
    plan1, _ = _installer(tmp_path, _embarques())
    assert "packages/volet_serre_tracking.yaml" in plan1.installes
    (tmp_path / "packages" / "volet_serre_tracking.yaml").unlink()
    plan2 = installation.planifier(tmp_path, _embarques("9.9.10"), plan1.installes)
    assert "packages/volet_serre_tracking.yaml" not in plan2.contenus
    assert "packages/volet_serre_tracking.yaml" not in plan2.retirer


def test_nettoyer_garde_les_plus_recentes(tmp_path):
    for i in range(8):
        (tmp_path / installation.SAUVEGARDES / f"2026100{i}-000000_3.{i}.0").mkdir(parents=True)
    retires = installation.nettoyer_sauvegardes(tmp_path, 5)
    assert retires == ["20261000-000000_3.0.0", "20261001-000000_3.1.0", "20261002-000000_3.2.0"]
    assert len(list((tmp_path / installation.SAUVEGARDES).iterdir())) == 5


def test_chemins_refuses(tmp_path):
    for mauvais in ("../x.yaml", "/etc/x", "packages", "autre/x.yaml", "packages/../../x", "packages\\x"):
        assert not installation.chemin_sur(mauvais), mauvais
    assert installation.chemin_sur("packages/tab5_push.yaml")
    (tmp_path / installation.MANIFESTE).write_text('{"fichiers": ["../secrets.yaml"]}', encoding="utf-8")
    with pytest.raises(ValueError):
        installation.lire_embarques(tmp_path)
    assert installation.lire_embarques(tmp_path / "absent") == {}


def test_etiquette_et_domaines():
    import datetime as dt
    assert installation.etiquette(dt.datetime(2026, 10, 7, 9, 5, 3), "3.7.0-rc.4") == "20261007-090503_3.7.0-rc.4"
    assert installation.etiquette(dt.datetime(2026, 10, 7), None) == "20261007-000000_inconnue"
    assert installation.etiquette(dt.datetime(2026, 10, 7), "dépôt/x") == "20261007-000000_d_p_t_x"
    domaines = installation.domaines({f"packages/{c}": d for c, d in
                                      ((p.name, p.read_bytes()) for p in
                                       (REPO / "HomeAssistant_Config" / "packages").glob("*.yaml"))})
    assert domaines == {"automation", "script", "template", "input_text", "input_select",
                        "input_boolean", "rest_command"}, \
        "nouveau domaine dans un package : vérifier qu'il se charge à chaud (verifier_integration.py)"
    assert installation.packages({"packages/tab5_health.yaml": b"", "custom_templates/x.jinja": b""}) \
        == {"tab5_health"}


# ─── Archive HACS ────────────────────────────────────────────────────────────

def test_archive_hacs(tmp_path):
    archive = archive_hacs.construire("9.9.9", tmp_path)
    ha = archive_ha.construire("9.9.9", tmp_path / "ha")
    with zipfile.ZipFile(archive) as z, zipfile.ZipFile(ha) as zha:
        noms = z.namelist()
        assert "manifest.json" in noms and "__init__.py" in noms, "code à la RACINE du zip (HACS)"
        assert not any(n.startswith("custom_components/") or "__pycache__" in n for n in noms)
        assert json.loads(z.read("manifest.json"))["version"] == "9.9.9"
        manifeste = json.loads(z.read("fichiers/MANIFESTE.json"))
        attendus = [n for n in zha.namelist() if n != archive_ha.LISEZMOI]
        assert manifeste == {"version": "9.9.9", "fichiers": attendus}
        for n in attendus:
            assert z.read(f"fichiers/{n}") == zha.read(n), f"{n} : mêmes octets que tab5_home_assistant.zip"
        for f in INTEGRATION.rglob("*"):
            relatif = f.relative_to(INTEGRATION).as_posix()
            if f.is_file() and "__pycache__" not in relatif and relatif != "manifest.json":
                assert z.read(relatif) == f.read_bytes(), relatif
    # Ce que l'intégration lira, une fois décompressé par HACS.
    with zipfile.ZipFile(archive) as z:
        z.extractall(tmp_path / "custom_components" / "tab5")
    lus = installation.lire_embarques(tmp_path / "custom_components" / "tab5" / "fichiers")
    assert lus == _embarques("9.9.9")
    assert archive_hacs.construire("9.9.9", tmp_path / "bis").read_bytes() == archive.read_bytes(), \
        "archive reproductible"


def test_archive_hacs_ancien_tag(tmp_path, monkeypatch):
    monkeypatch.setattr(sys, "argv", ["archive_hacs.py", "--version", "3.6.0", "--sortie", str(tmp_path),
                                      "--integration", str(tmp_path / "absent")])
    assert archive_hacs.main() == 3 and not (tmp_path / archive_hacs.NOM).exists()


# ─── Notifications ───────────────────────────────────────────────────────────

@pytest.mark.parametrize("langue,mot", [("fr", "remplacé"), ("fr-FR", "remplacé"), ("en", "replaced"),
                                        ("tr", "replaced"), (None, "replaced")])
def test_messages(langue, mot):
    titre, texte = messages.installation(
        langue, avant="3.7.0", version="3.8.0", ecrits=12, retires=1, identiques=2,
        sauvegarde="tab5_sauvegardes/x", modifies=["packages/tab5_tv.yaml"], redemarrer=True,
        packages_absents=False, firmware="auto")
    assert "3.8.0" in titre and mot in texte and "tab5_sauvegardes/x" in texte and "tab5_tv" in texte
    titre, texte = messages.firmware_lance(langue, "3.8.0")
    assert "3.8.0" in texte


def test_yaml_des_packages_lisible_par_le_plan():
    # Les clés de premier niveau lues par regex sont bien celles du YAML.
    for p in (REPO / "HomeAssistant_Config" / "packages").glob("*.yaml"):
        donnees = yaml.load(p.read_text(encoding="utf-8"), Loader=yaml.BaseLoader) or {}
        assert set(donnees) == installation.domaines({f"packages/{p.name}": p.read_bytes()}), p.name
