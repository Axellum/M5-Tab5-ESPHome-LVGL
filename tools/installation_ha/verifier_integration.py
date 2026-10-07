# -*- coding: utf-8 -*-
"""tools/installation_ha/verifier_integration.py — L'intégration « Tab5 » (HACS) dans un vrai Home
Assistant neuf : installation, mises à jour, retour en arrière (ADR-0035).

[AI-CONTEXT]
@role Job « ha » de .github/workflows/integration-hacs.yml. Fait ce que fait un utilisateur
      avec HACS, sans HACS : HACS ne fait que télécharger l'asset tab5_hacs.zip de la
      release, le décompresser dans custom_components/tab5/ (dossier remplacé) et demander
      un redémarrage ; ici le zip est construit par tools/publication/archive_hacs.py et
      décompressé de la même façon. Scénario, sur UN Home Assistant neuf en conteneur :
        1. config/ d'une installation neuve + la ligne `packages:` du guide, zip 9.9.1,
           compte (onboarding), intégration ajoutée par son formulaire → fichiers posés
           sans redémarrage : capteur « Tab5 · version des fichiers HA » à 9.9.1,
           rest_command chargé à chaud (absent d'un HA neuf), aucune réparation ;
        2. mise à jour 9.9.2 comme HACS (dossier remplacé, redémarrage), avec un package
           modifié à la main et un package que la release ne livre plus → capteur 9.9.2,
           sauvegarde des deux, le retiré absent, la notification nomme le modifié ;
        3. mise à jour 9.9.3 dont un package casse la configuration → fichiers 9.9.2
           remis à l'octet près, capteur toujours 9.9.2, réparation
           « configuration_invalide » ;
        4. retour au zip 9.9.2 (réparation retirée), ligne `packages:` enlevée puis
           « Réinstaller » des options → réparation « packages_absents » ; ligne remise,
           redémarrage → capteur 9.9.2, réparation retirée ;
      et le journal de HA : aucune erreur de l'intégration hors celle, attendue, de l'étape 3.
      Le firmware n'est pas testé (pas de tablette ici) : seulement rapporté.
@contraintes Les fichiers de config/ écrits par HA appartiennent à root : toute écriture
      dessus passe par le conteneur (docker exec python3). Le mot de passe est tiré au
      hasard et jamais affiché.
@ai_instruction Un échec doit dire QUOI et OÙ regarder (journal de HA : artefact du job).

Usage (voir le workflow) :
    python tools/installation_ha/verifier_integration.py --dossier "$RUNNER_TEMP/ha" \\
        --image ghcr.io/home-assistant/home-assistant:2026.9.4 --journal captures/home-assistant.log
"""
from __future__ import annotations

import argparse
import asyncio
import json
import logging
import os
import secrets
import shutil
import subprocess
import sys
import tempfile
import time
import zipfile
from pathlib import Path

ICI = Path(__file__).resolve().parent
RACINE = ICI.parent.parent
sys.path.insert(0, str(RACINE / "tools" / "publication"))
sys.path.insert(0, str(ICI))

import archive_ha  # noqa: E402
import archive_hacs  # noqa: E402
from verifier_installation import HA, URL_HA, Echec, Rapport  # noqa: E402

logger = logging.getLogger("integration_hacs")

CONTENEUR = "ha-integration-tab5"
CAPTEUR = "sensor.tab5_version_des_fichiers_ha"
NOTIFICATION = "tab5_installation"
# configuration.yaml d'une installation neuve (comme tools/installation_ha/configuration.yaml,
# sans la démo ni les données de test) ; la ligne des packages est ajoutée ou retirée.
CONFIGURATION = """\
default_config:

frontend:
  themes: !include_dir_merge_named themes

automation: !include automations.yaml
script: !include scripts.yaml
scene: !include scenes.yaml

logger:
  default: warning
  logs:
    custom_components.tab5: info
"""
LIGNE_PACKAGES = "\nhomeassistant:\n  packages: !include_dir_named packages\n"
VIDES = {"automations.yaml": "[]\n", "scripts.yaml": "", "scenes.yaml": "", "secrets.yaml": ""}
MODIFIE = "packages/tab5_tv.yaml"            # modifié à la main avant la 9.9.2
RETIRE = "packages/tab5_micro_absence.yaml"  # plus livré par la 9.9.2
CASSE = "packages/tab5_casse.yaml"           # livré par la 9.9.3, refusé par check_config
CONTENU_CASSE = '# Clé qui n\'est pas un slug : check_config refuse input_boolean.\ninput_boolean:\n  "Pas Un Slug": {}\n'


# ─── Archives et conteneur ───────────────────────────────────────────────────

def archives(travail: Path) -> dict[str, Path]:
    """Les trois zips du scénario (comme ceux d'une release, archive_hacs.py)."""
    v2 = travail / "ha-9.9.2"
    shutil.copytree(archive_ha.HA_DIR, v2)
    (v2 / RETIRE).unlink()
    v3 = travail / "ha-9.9.3"
    shutil.copytree(v2, v3)
    (v3 / CASSE).write_text(CONTENU_CASSE, encoding="utf-8")
    return {
        "9.9.1": archive_hacs.construire("9.9.1", travail / "9.9.1"),
        "9.9.2": archive_hacs.construire("9.9.2", travail / "9.9.2", base=v2),
        "9.9.3": archive_hacs.construire("9.9.3", travail / "9.9.3", base=v3),
    }


def attendus(zip_: Path) -> dict[str, bytes]:
    """Fichiers que l'intégration doit poser (hors optionnels), relatifs à config/."""
    with zipfile.ZipFile(zip_) as z:
        liste = json.loads(z.read("fichiers/MANIFESTE.json"))["fichiers"]
        return {c: z.read(f"fichiers/{c}") for c in liste if not c.startswith("tab5_optionnel/")}


def docker(*args: str, verifier: bool = True) -> str:
    r = subprocess.run(["docker", *args], capture_output=True, text=True)
    if verifier and r.returncode != 0:
        raise Echec(f"docker {args[0]} : {r.stderr.strip()[:300]}")
    return r.stdout


def dans_conteneur(code: str) -> None:
    docker("exec", CONTENEUR, "python3", "-c", code)


def remplacer_integration(dossier: Path, zip_: Path) -> None:
    """Ce que fait HACS : le dossier de l'intégration remplacé par le contenu du zip."""
    shutil.copyfile(zip_, dossier / "tab5_maj.zip")
    dans_conteneur(
        "import shutil, zipfile, os\n"
        "shutil.rmtree('/config/custom_components/tab5', ignore_errors=True)\n"
        "zipfile.ZipFile('/config/tab5_maj.zip').extractall('/config/custom_components/tab5')\n"
        "os.remove('/config/tab5_maj.zip')\n")


def packages_dans_configuration(dossier: Path, oui: bool) -> None:
    (dossier / "configuration.yaml").write_text(CONFIGURATION + (LIGNE_PACKAGES if oui else ""),
                                                encoding="utf-8", newline="\n")


def preparer(dossier: Path, zip_: Path) -> None:
    if dossier.exists() and any(dossier.iterdir()):
        raise Echec(f"{dossier} n'est pas vide : un Home Assistant NEUF part d'un dossier vide")
    dossier.mkdir(parents=True, exist_ok=True)
    packages_dans_configuration(dossier, True)
    for nom, contenu in VIDES.items():
        (dossier / nom).write_text(contenu, encoding="utf-8", newline="\n")
    (dossier / "themes").mkdir()
    with zipfile.ZipFile(zip_) as z:
        z.extractall(dossier / "custom_components" / "tab5")


# ─── Lectures dans HA ────────────────────────────────────────────────────────

async def demarrer(ha: HA) -> None:
    await ha.attendre_http()
    await ha.attendre_demarrage()
    await ha.websocket()


async def redemarrer(ha: HA) -> None:
    if ha.ws is not None:
        await ha.ws.ws.close()
    docker("restart", "-t", "60", CONTENEUR)
    # Pas demarrer() : une fois l'onboarding fini, HA ne publie plus /api/onboarding (404,
    # comme dans verifier_installation.redemarrer_ha). Le jeton reste valable.
    fin = time.monotonic() + 300
    while time.monotonic() < fin:
        try:
            if (await ha.get("/api/config")).get("state") == "RUNNING":
                await ha.websocket()
                return
        except Exception:  # noqa: BLE001 — HA redémarre : connexion refusée, 502…
            pass
        await asyncio.sleep(2)
    raise Echec("Home Assistant n'est pas revenu (état RUNNING) dans les 300 s après son redémarrage")


async def notification(ha: HA, version: str, delai: float = 120.0) -> str:
    """Texte de la notification d'installation de `version` (titre qui la nomme)."""
    fin = time.monotonic() + delai
    while time.monotonic() < fin:
        for n in await ha.ws.commande("persistent_notification/get"):
            if n.get("notification_id") == NOTIFICATION and version in (n.get("title") or ""):
                return n.get("message") or ""
        await asyncio.sleep(2)
    raise Echec(f"pas de notification d'installation {version} (journal : custom_components.tab5)")


async def etat(ha: HA, entite: str) -> str | None:
    e = (await ha.etats()).get(entite)
    return e["state"] if e else None


async def attendre_etat(ha: HA, entite: str, valeur: str, delai: float = 60.0) -> str | None:
    fin = time.monotonic() + delai
    while True:
        actuel = await etat(ha, entite)
        if actuel == valeur or time.monotonic() > fin:
            return actuel
        await asyncio.sleep(2)


async def reparations(ha: HA) -> set[str]:
    resultat = await ha.ws.commande("repairs/list_issues")
    return {i["issue_id"] for i in resultat.get("issues", []) if i.get("domain") == "tab5"}


async def attendre_reparations(ha: HA, presentes: set[str], absentes: set[str], delai: float = 60.0) -> set[str]:
    fin = time.monotonic() + delai
    while True:
        actuelles = await reparations(ha)
        if (presentes <= actuelles and not (absentes & actuelles)) or time.monotonic() > fin:
            return actuelles
        await asyncio.sleep(2)


def comparer(rapport: Rapport, dossier: Path, fichiers: dict[str, bytes], quoi: str) -> None:
    differents = [c for c, d in fichiers.items()
                  if not (dossier / c).is_file() or (dossier / c).read_bytes() != d]
    rapport.verifier(not differents, f"{quoi} : {len(fichiers)} fichiers identiques à ceux du zip",
                     f"différents ou absents : {differents[:5]}")


# ─── Le scénario ─────────────────────────────────────────────────────────────

async def scenario(args, rapport: Rapport) -> None:
    import aiohttp

    dossier: Path = args.dossier.resolve()
    with tempfile.TemporaryDirectory() as tmp:
        zips = archives(Path(tmp))
        v1, v2 = attendus(zips["9.9.1"]), attendus(zips["9.9.2"])
        preparer(dossier, zips["9.9.1"])
        docker("rm", "-f", CONTENEUR, verifier=False)
        docker("run", "-d", "--name", CONTENEUR, "--network", "host", "-e", "TZ=Europe/Paris",
               "-v", f"{dossier}:/config", args.image)

        async with aiohttp.ClientSession() as session:
            ha = HA(session, URL_HA, CONTENEUR)
            await ha.attendre_http()
            await ha.onboarding(secrets.token_urlsafe(18))
            await ha.terminer_onboarding()
            await ha.attendre_demarrage()
            await ha.websocket()

            # ── 1. Première installation, sans redémarrage ──
            services_avant = {s["domain"] for s in await ha.get("/api/services")}
            rapport.info(f"HA neuf : rest_command {'déjà' if 'rest_command' in services_avant else 'pas'} chargé")
            flux = await ha.flux("/api/config/config_entries/flow", "tab5")
            if flux.get("type") != "create_entry":
                raise Echec(f"formulaire de l'intégration : {flux}")
            entree = flux["result"]["entry_id"]
            rapport.ok("intégration ajoutée par son formulaire (option firmware cochée par défaut)")
            texte = await notification(ha, "9.9.1")
            rapport.info(f"notification : {texte.splitlines()[0]}")
            rapport.verifier(await attendre_etat(ha, CAPTEUR, "9.9.1") == "9.9.1",
                             "1. fichiers actifs sans redémarrage (capteur à 9.9.1)")
            comparer(rapport, dossier, v1, "1. config/")
            services = {(s["domain"], n) for s in await ha.get("/api/services") for n in s["services"]}
            rapport.verifier(("rest_command", "tab5_pluie") in services,
                             "1. rest_command.tab5_pluie chargé à chaud")
            rapport.verifier(not await reparations(ha), "1. aucune réparation")
            manifeste = await ha.ws.commande("manifest/get", integration="tab5")
            rapport.verifier(manifeste.get("version") == "9.9.1", "1. version de l'intégration = 9.9.1",
                             str(manifeste.get("version")))
            memoire = await ha.storage("tab5.fichiers") or {}
            rapport.verifier((memoire.get("data") or {}).get("version") == "9.9.1",
                             "1. version posée gardée (.storage/tab5.fichiers)")

            # ── 2. Mise à jour 9.9.2 comme HACS ──
            dans_conteneur(f"open('/config/{MODIFIE}', 'a').write('\\n# ajout perso\\n')")
            remplacer_integration(dossier, zips["9.9.2"])
            await redemarrer(ha)
            texte = await notification(ha, "9.9.2")
            rapport.verifier(await attendre_etat(ha, CAPTEUR, "9.9.2") == "9.9.2",
                             "2. mise à jour : capteur à 9.9.2")
            comparer(rapport, dossier, v2, "2. config/")
            rapport.verifier(not (dossier / RETIRE).exists(), f"2. {RETIRE} retiré (plus livré)")
            sauvegardes = sorted((dossier / "tab5_sauvegardes").glob("*_9.9.1"))
            rapport.verifier(len(sauvegardes) == 1, "2. une sauvegarde nommée d'après la 9.9.1",
                             str([p.name for p in sauvegardes]))
            if sauvegardes:
                s = sauvegardes[0]
                rapport.verifier((s / MODIFIE).is_file() and (s / MODIFIE).read_bytes().endswith(b"# ajout perso\n"),
                                 f"2. {MODIFIE} modifié à la main : gardé dans la sauvegarde")
                rapport.verifier((s / RETIRE).read_bytes() == v1[RETIRE], f"2. {RETIRE} gardé dans la sauvegarde")
            rapport.verifier("tab5_tv.yaml" in texte, "2. la notification nomme le fichier modifié à la main",
                             texte[:300])

            # ── 3. Mise à jour 9.9.3 qui casse la configuration ──
            remplacer_integration(dossier, zips["9.9.3"])
            await redemarrer(ha)
            presentes = await attendre_reparations(ha, {"configuration_invalide"}, set(), 120)
            rapport.verifier("configuration_invalide" in presentes,
                             "3. configuration cassée : réparation « configuration_invalide »", str(presentes))
            rapport.verifier(await etat(ha, CAPTEUR) == "9.9.2", "3. capteur toujours à 9.9.2")
            comparer(rapport, dossier, v2, "3. config/ remis comme avant")
            rapport.verifier(not (dossier / CASSE).exists(), f"3. {CASSE} retiré par le retour en arrière")

            # ── 4. Retour à 9.9.2, puis packages absents ──
            remplacer_integration(dossier, zips["9.9.2"])
            await redemarrer(ha)
            restes = await attendre_reparations(ha, set(), {"configuration_invalide"})
            rapport.verifier("configuration_invalide" not in restes,
                             "4. version déjà posée : réparation « configuration_invalide » retirée", str(restes))
            packages_dans_configuration(dossier, False)
            options = await ha.flux("/api/config/config_entries/options/flow", entree,
                                    {"mettre_a_jour_tablette": True, "reinstaller": True})
            rapport.verifier(options.get("type") == "create_entry", "4. options : « Réinstaller » validé",
                             str(options)[:200])
            presentes = await attendre_reparations(ha, {"packages_absents"}, set(), 90)
            rapport.verifier("packages_absents" in presentes,
                             "4. sans la ligne `packages:` : réparation « packages_absents »", str(presentes))
            packages_dans_configuration(dossier, True)
            await redemarrer(ha)
            rapport.verifier(await attendre_etat(ha, CAPTEUR, "9.9.2") == "9.9.2",
                             "4. ligne remise, redémarrage : capteur à 9.9.2")
            restes = await attendre_reparations(ha, set(), {"packages_absents"})
            rapport.verifier("packages_absents" not in restes, "4. réparation « packages_absents » retirée",
                             str(restes))
            rapport.info("firmware : pas de tablette dans ce HA, la mise à jour enchaînée n'est pas exercée")


def journal(rapport: Rapport, chemin: Path | None) -> None:
    r = subprocess.run(["docker", "logs", CONTENEUR], capture_output=True, text=True)
    texte = r.stdout + r.stderr
    if chemin:
        chemin.parent.mkdir(parents=True, exist_ok=True)
        chemin.write_text(texte, encoding="utf-8")
    erreurs = [l for l in texte.splitlines()
               if "custom_components.tab5" in l and ("ERROR" in l or "Traceback" in l)
               and "refusés par la vérification de la configuration" not in l]
    autres = [l for l in texte.splitlines() if "Error setting up entry" in l and "tab5" in l.lower()]
    # Ce qui trahit un chargement dans le désordre (vu dans la CI du 07/10/2026 : une
    # automatisation lisait tab5_alertes.jinja avant le rechargement des modèles) ou une
    # exception qui échappe à l'intégration.
    autres += [l for l in texte.splitlines()
               if "TemplateNotFound" in l or "Task exception was never retrieved" in l]
    rapport.verifier(not erreurs and not autres, "journal de HA : aucune erreur de l'intégration",
                     " | ".join((erreurs + autres)[:3]))


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    parser = argparse.ArgumentParser(description=__doc__.split("\n", 1)[0])
    parser.add_argument("--dossier", type=Path, required=True, help="config/ du HA (vide ou absent)")
    parser.add_argument("--image", required=True, help="image de Home Assistant")
    parser.add_argument("--journal", type=Path, help="où écrire le journal de HA")
    args = parser.parse_args()
    rapport = Rapport()
    try:
        asyncio.run(scenario(args, rapport))
    except Echec as e:
        rapport.echec(str(e))
    finally:
        journal(rapport, args.journal)
        docker("rm", "-f", CONTENEUR, verifier=False)
    resume = rapport.resume().replace("Installation dans un Home Assistant neuf", "Intégration Tab5 (HACS)")
    print(resume)
    if sommaire := os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(sommaire, "a", encoding="utf-8") as f:
            f.write(resume)
    return 0 if rapport.reussi else 1


if __name__ == "__main__":
    sys.exit(main())
