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
firmware = _module("firmware")


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
    parametres = {const.ISSUE_PACKAGES: set(), const.ISSUE_CONFIGURATION: {"version", "signaler"},
                  const.ISSUE_REMPLACES: {"fichiers", "sauvegarde"},
                  const.ISSUE_FIRMWARE: {"version", "essais"}}
    for langue in (en, fr):
        # hassfest refuse une URL dans une traduction : elle passe par un paramètre.
        assert not re.search(r"https?://", json.dumps(langue, ensure_ascii=False))
        for cle, issue in langue["issues"].items():
            textes = json.dumps(issue, ensure_ascii=False)
            assert set(re.findall(r"\{(\w+)\}", textes)) == parametres.get(cle, {"version"}), cle


def test_constantes_lues_dans_les_packages():
    ha = REPO / "HomeAssistant_Config" / "packages"
    # Le modèle de la tablette : dans la macro partagée par les packages (HA-7).
    macros = REPO / "HomeAssistant_Config" / "custom_templates" / "tab5_tablette.jinja"
    assert f"'{const.MODELE_TABLETTE}'" in macros.read_text(encoding="utf-8")
    sante = (ha / f"{const.PACKAGE_TEMOIN}.yaml").read_text(encoding="utf-8")
    assert f"default_entity_id: {const.CAPTEUR_VERSION}" in sante
    assert installation.PACKAGE_VERSION == f"packages/{const.PACKAGE_TEMOIN}.yaml"
    assert installation.VERSION_FICHIERS.findall(sante) == ["dépôt"]
    assert installation.SIGNATURE_BLUEPRINT in (
        REPO / "HomeAssistant_Config" / installation.BLUEPRINT).read_text(encoding="utf-8")


def test_modules_purs_sans_home_assistant():
    for nom in ("installation", "messages", "const", "firmware"):
        texte = (INTEGRATION / f"{nom}.py").read_text(encoding="utf-8")
        assert not re.search(r"^\s*(from|import)\s+(homeassistant|\.)", texte, re.M), nom


def test_verificateur_de_la_ci_compilable():
    """tools/installation_ha/verifier_integration.py ne tourne que dans la CI (job « ha »,
    non requis, seulement si ses chemins changent) : une faute de syntaxe ne s'y verrait
    qu'après un poussé. Ici, sans Docker ni HA."""
    chemin = REPO / "tools" / "installation_ha" / "verifier_integration.py"
    compile(chemin.read_text(encoding="utf-8"), str(chemin), "exec")


def test_aucun_nom_local_ne_masque_un_import():
    """Une variable `messages` masquait le module `messages` dans _installer : l'installation
    s'arrêtait sur un AttributeError (CI du 07/10/2026). Le code qui parle à HA ne tourne
    que dans le conteneur de la CI : cette lecture de l'AST le voit sans HA."""
    import ast
    for fichier in sorted(INTEGRATION.glob("*.py")):
        arbre = ast.parse(fichier.read_text(encoding="utf-8"))
        importes = {(a.asname or a.name).split(".")[0] for n in arbre.body
                    if isinstance(n, (ast.Import, ast.ImportFrom)) for a in n.names}
        for fonction in ast.walk(arbre):
            if not isinstance(fonction, (ast.FunctionDef, ast.AsyncFunctionDef)):
                continue
            locaux = {n.id for n in ast.walk(fonction) if isinstance(n, ast.Name) and isinstance(n.ctx, ast.Store)}
            locaux |= {a.arg for a in fonction.args.args + fonction.args.kwonlyargs}
            assert not locaux & importes, f"{fichier.name}:{fonction.lineno} {fonction.name} : {locaux & importes}"


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


def test_premiere_installation_signale_les_fichiers_copies_a_la_main(tmp_path):
    """HA-10 (audit du 07/10/2026) : une première installation remplaçait sans rien dire des
    fichiers du Tab5 copiés à la main (archive d'une version plus ancienne, retouches)."""
    v1 = _embarques("9.9.9")
    paquets = tmp_path / "packages"
    paquets.mkdir()
    (paquets / "tab5_tv.yaml").write_bytes(v1["packages/tab5_tv.yaml"] + b"\n# retouche\n")
    (paquets / "tab5_reveil.yaml").write_bytes(v1["packages/tab5_reveil.yaml"])  # identique
    (paquets / "perso.yaml").write_bytes(b"input_boolean:\n  a: {}\n")
    plan, sauvegarde = _installer(tmp_path, v1)
    assert plan.differents == ["packages/tab5_tv.yaml"], \
        "déjà là, différent, pas posé par l'intégration : à signaler"
    assert not plan.modifies, "rien n'a été posé avant : pas « modifié depuis la dernière installation »"
    assert (sauvegarde / "packages" / "tab5_tv.yaml").read_bytes().endswith(b"# retouche\n")
    titre, texte = messages.installation(
        "fr", avant=None, version="9.9.9", ecrits=len(plan.ecrire), retires=0, identiques=1,
        sauvegarde="tab5_sauvegardes/x", modifies=[], differents=plan.differents, redemarrer=False,
        packages_absents=False, firmware="auto")
    assert "packages/tab5_tv.yaml" in texte and "Déjà là" in texte

    # Installation suivante : un fichier posé par l'intégration puis retouché est « modifié »,
    # pas « différent » (déjà dit autrement dans la notification).
    (paquets / "tab5_tv.yaml").write_bytes(v1["packages/tab5_tv.yaml"] + b"\n# encore\n")
    plan2 = installation.planifier(tmp_path, _embarques("9.9.10"), plan.installes)
    assert plan2.modifies == ["packages/tab5_tv.yaml"] and not plan2.differents


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


def test_nouvel_essai_sans_sauvegarde_identique(tmp_path):
    """HA-9 (audit du 07/10/2026) : chaque essai d'une version refusée laissait une
    sauvegarde de plus, identique, qui poussait les utiles hors des 5 gardées."""
    v1 = _embarques("9.9.9")
    plan1, _ = _installer(tmp_path, v1)
    v2 = _embarques("9.9.10")
    racine = tmp_path / installation.SAUVEGARDES
    plan2, s1 = _installer(tmp_path, v2, plan1.installes, "20261008-090000_9.9.9")
    installation.restaurer(tmp_path, plan2, s1)  # refusée : tout remis comme avant
    avant = _instantane(tmp_path)
    for nom in ("20261008-100000_9.9.9", "20261008-110000_9.9.9"):  # deux essais de plus
        plan, s = _installer(tmp_path, v2, plan1.installes, nom)
        assert s == s1, "mêmes fichiers à sauver, à l'octet près : la sauvegarde précédente resert"
        installation.restaurer(tmp_path, plan, s)
    assert [d.name for d in racine.iterdir()] == ["20261008-090000_9.9.9"]
    assert _instantane(tmp_path) == avant, "restaurer() depuis la sauvegarde réutilisée : à l'octet près"
    # Un fichier changé entre-temps : la sauvegarde ne serait plus la même, une nouvelle est faite.
    tv = tmp_path / "packages" / "tab5_tv.yaml"
    tv.write_bytes(tv.read_bytes() + b"\n# ajout\n")
    _, s3 = _installer(tmp_path, v2, plan1.installes, "20261008-120000_9.9.9")
    assert s3 != s1 and (s3 / "packages" / "tab5_tv.yaml").read_bytes().endswith(b"# ajout\n")


def test_version_refusee_pas_reessayee():
    """HA-9 : une version refusée par la vérification de la configuration n'est plus
    réessayée à chaque démarrage, tant que ni la version ni les fichiers ne changent."""
    v3 = _embarques("9.9.3")
    e3 = installation.empreinte_des_fichiers(v3)
    assert e3 == installation.empreinte_des_fichiers(dict(reversed(list(v3.items())))), "ordre sans effet"
    memoire = {"version": "9.9.2", "refusee": installation.refus("9.9.3", e3)}
    assert installation.deja_refusee(memoire, "9.9.3", e3)
    autres = dict(v3, **{"packages/tab5_tv.yaml": v3["packages/tab5_tv.yaml"] + b"\n# corrige\n"})
    assert not installation.deja_refusee(memoire, "9.9.3", installation.empreinte_des_fichiers(autres)), \
        "mêmes version, autres fichiers : nouvel essai"
    assert not installation.deja_refusee(memoire, "9.9.4", installation.empreinte_des_fichiers(_embarques("9.9.4")))
    assert not installation.deja_refusee({"version": "9.9.2"}, "9.9.3", e3), "jamais refusée"
    assert json.loads(json.dumps(memoire)) == memoire, "gardé tel quel par le Store de HA (JSON)"


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
    ordre = installation.ordre_de_chargement(domaines | {"zone"})
    assert ordre == ["input_boolean", "input_select", "input_text", "rest_command", "zone", "template",
                     "script", "automation"], "les entrées avant ce qui les lit, les automatisations en dernier"
    assert installation.packages({"packages/tab5_health.yaml": b"", "custom_templates/x.jinja": b""}) \
        == {"tab5_health"}


# ─── Firmware enchaîné ───────────────────────────────────────────────────────

def test_firmware_garde_l_attente_et_reessaie():
    """HA-11 (audit du 07/10/2026) : `firmware_attendu` était oublié AVANT update.install,
    et rien ne réessayait si l'OTA n'aboutissait pas. Ici, une tablette dont l'OTA rate :
    l'attente reste jusqu'à la version constatée, chaque essai est refait après le délai,
    ESSAIS_MAX fois, puis l'échec est dit."""
    import datetime as dt
    t0 = dt.datetime(2026, 10, 8, 12, 0, tzinfo=dt.timezone.utc)
    minute = dt.timedelta(minutes=1)
    ent = "update.tab5_firmware"
    proposee = firmware.Tablette(installee="3.7.0", proposee="3.8.0", disponible=True, en_cours=False)
    essais: dict = {}

    d = firmware.decider("3.8.0", {ent: proposee}, essais, t0)
    assert d.lancer == [ent] and not d.fini and not d.echecs
    instant = t0
    for essai in range(1, firmware.ESSAIS_MAX + 1):
        assert firmware.noter_essai(essais, ent, instant) == essai
        en_cours = firmware.Tablette("3.7.0", "3.8.0", True, True)
        d = firmware.decider("3.8.0", {ent: en_cours}, essais, instant + minute)
        assert not d.fini and not d.lancer and d.attente_s, "OTA en cours : attendre, sans oublier"
        # L'OTA retombe sans effet (version installée inchangée).
        d = firmware.decider("3.8.0", {ent: proposee}, essais, instant + 5 * minute)
        assert not d.fini, "lancée mais pas constatée : firmware_attendu reste"
        assert not d.lancer and 0 < d.attente_s <= firmware.DELAI_ESSAI_S, "trop tôt pour réessayer"
        instant += dt.timedelta(seconds=firmware.DELAI_ESSAI_S + 1)
        d = firmware.decider("3.8.0", {ent: proposee}, essais, instant)
        if essai < firmware.ESSAIS_MAX:
            assert d.lancer == [ent], f"essai {essai} sans effet : nouvel essai"
        else:
            assert not d.lancer and d.echecs == [ent] and not d.fini, "plus d'essai : échec dit"
    assert json.loads(json.dumps(essais)) == essais, "gardé par le Store de HA (JSON)"

    # Installée à la main ensuite : constatée, fini.
    faite = firmware.Tablette("3.8.0", "3.8.0", False, False)
    d = firmware.decider("3.8.0", {ent: faite}, essais, instant + minute)
    assert d.fini and not d.echecs and not d.lancer


def test_firmware_succes_constate_seulement():
    import datetime as dt
    t0 = dt.datetime(2026, 10, 8, 12, 0, tzinfo=dt.timezone.utc)
    ent, tableau = "update.tab5_firmware", "update.tab5_firmware_esphome"
    # L'entité du tableau de bord ESPHome suit la version d'ESPHome : jamais lancée,
    # n'empêche pas de conclure.
    autre = firmware.Tablette("2026.9.1", "2026.9.2", True, False)
    essais: dict = {}
    d = firmware.decider("3.8.0", {ent: firmware.Tablette("3.7.0", "3.8.0", True, False), tableau: autre},
                         essais, t0)
    assert d.lancer == [ent]
    firmware.noter_essai(essais, ent, t0)
    d = firmware.decider("3.8.0", {ent: firmware.Tablette("3.8.0", "3.8.0", False, False), tableau: autre},
                         essais, t0 + dt.timedelta(minutes=4))
    assert d.fini and not d.lancer, "version lue sur la tablette : succès constaté"
    # Pas encore proposée (manifeste pas relu) : on attend, sans minuterie ni échec.
    d = firmware.decider("3.8.0", {ent: firmware.Tablette("3.7.0", "3.7.0", False, False)}, {}, t0)
    assert not d.fini and not d.lancer and not d.echecs and d.attente_s is None
    # Aucune tablette trouvée : rien de fini.
    assert not firmware.decider("3.8.0", {}, {}, t0).fini


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
        packages_absents=False, firmware="auto", differents=["packages/tab5_reveil.yaml"])
    assert "3.8.0" in titre and mot in texte and "tab5_sauvegardes/x" in texte and "tab5_tv" in texte
    assert "tab5_reveil" in texte
    titre, texte = messages.firmware_lance(langue, "3.8.0")
    assert "3.8.0" in texte


def test_yaml_des_packages_lisible_par_le_plan():
    # Les clés de premier niveau lues par regex sont bien celles du YAML.
    for p in (REPO / "HomeAssistant_Config" / "packages").glob("*.yaml"):
        donnees = yaml.load(p.read_text(encoding="utf-8"), Loader=yaml.BaseLoader) or {}
        assert set(donnees) == installation.domaines({f"packages/{p.name}": p.read_bytes()}), p.name
