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
           un blueprint du Tab5 copié à la main (ancien, différent), compte (onboarding),
           intégration ajoutée par son formulaire → fichiers posés sans redémarrage :
           capteur « Tab5 · version des fichiers HA » à 9.9.1, rest_command chargé à chaud
           (absent d'un HA neuf), le blueprint remplacé et dit (réparation persistante
           « fichiers_remplaces », copie dans la sauvegarde), validée comme l'interface ;
           l'assistant de configuration proposé (réparation « configurer_pieces ») ;
        1 bis. l'assistant (ADR-0052) : deux pièces créées (entités de modèle et un
           thermostat, une lumière cachée), la réparation suivie comme l'interface, valeurs
           proposées acceptées → automatisation du blueprint écrite dans automations.yaml,
           chargée, réparation retirée ; relancé depuis les options sans cocher « mettre à
           jour » → automations.yaml à l'octet près ; puis en la cochant, un nom changé →
           seules les pièces changent, une sauvegarde dans tab5_sauvegardes/automatisations/ ;
        2. mise à jour 9.9.2 comme HACS (dossier remplacé, redémarrage), avec un package
           modifié à la main et un package que la release ne livre plus → capteur 9.9.2,
           sauvegarde des deux, le retiré absent, la notification nomme le modifié ;
        3. mise à jour 9.9.3 dont un package casse la configuration → fichiers 9.9.2
           remis à l'octet près, capteur toujours 9.9.2, réparation
           « configuration_invalide » ; redémarrage avec la même 9.9.3 → pas de nouvel
           essai (réparation recréée, aucune sauvegarde de plus) ; « Réinstaller » des
           options → nouvel essai, refusé, sans sauvegarde identique de plus ;
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

# Des appareils pour l'assistant de configuration (étape 1 bis) : rangés dans deux pièces
# par le scénario (registre des entités), comme l'utilisateur le ferait.
template:
  - light:
      - name: Plafonnier CI
        unique_id: ci_plafonnier
        default_entity_id: light.ci_plafonnier
        turn_on: [{event: ci_lumiere}]
        turn_off: [{event: ci_lumiere}]
      - name: Applique CI
        unique_id: ci_applique
        default_entity_id: light.ci_applique
        turn_on: [{event: ci_lumiere}]
        turn_off: [{event: ci_lumiere}]
      - name: Lumière cachée CI
        unique_id: ci_cachee
        default_entity_id: light.ci_cachee
        turn_on: [{event: ci_lumiere}]
        turn_off: [{event: ci_lumiere}]
      - name: Chevet CI
        unique_id: ci_chevet
        default_entity_id: light.ci_chevet
        turn_on: [{event: ci_lumiere}]
        turn_off: [{event: ci_lumiere}]
  - switch:
      - name: Prise CI
        unique_id: ci_prise
        default_entity_id: switch.ci_prise
        turn_on: [{event: ci_prise}]
        turn_off: [{event: ci_prise}]
  - sensor:
      - name: Température salon CI
        unique_id: ci_temperature_salon
        default_entity_id: sensor.ci_temperature_salon
        state: "21.5"
        unit_of_measurement: "°C"
        device_class: temperature
        state_class: measurement
      - name: Humidité salon CI
        unique_id: ci_humidite_salon
        default_entity_id: sensor.ci_humidite_salon
        state: "48"
        unit_of_measurement: "%"
        device_class: humidity
        state_class: measurement
      - name: Température chambre CI
        unique_id: ci_temperature_chambre
        default_entity_id: sensor.ci_temperature_chambre
        state: "19"
        unit_of_measurement: "°C"
        device_class: temperature
        state_class: measurement

climate:
  - platform: generic_thermostat
    name: CI clim
    unique_id: ci_clim
    heater: switch.ci_prise
    target_sensor: sensor.ci_temperature_salon
"""
LIGNE_PACKAGES = "\nhomeassistant:\n  packages: !include_dir_named packages\n"
VIDES = {"automations.yaml": "[]\n", "scripts.yaml": "", "scenes.yaml": "", "secrets.yaml": ""}
MODIFIE = "packages/tab5_tv.yaml"            # modifié à la main avant la 9.9.2
RETIRE = "packages/tab5_micro_absence.yaml"  # plus livré par la 9.9.2
CASSE = "packages/tab5_casse.yaml"           # livré par la 9.9.3, refusé par check_config
# Copié à la main avant la première installation, différent de celui de la release (HA-10).
COPIE_MAIN = "blueprints/automation/tab5/tab5_emplacements.yaml"
MARQUE_MAIN = b"\n# copie a la main\n"
# Un `initial` qui n'est pas un booléen : HA 2026.9.4 ne charge plus input_boolean, et
# « Vérifier la configuration » ne le signale qu'en AVERTISSEMENT (contre-épreuve du
# 07/10/2026 ci-dessous) — le cas qu'une vérification des seules erreurs laissait passer.
CONTENU_CASSE = "input_boolean:\n  tab5_casse:\n    initial: peut-etre\n"
# L'assistant (étape 1 bis) : les pièces créées, leurs entités, et ce qu'il doit proposer.
PIECES_CI = {
    "Salon CI": ["light.ci_plafonnier", "light.ci_applique", "light.ci_cachee", "switch.ci_prise",
                 "sensor.ci_temperature_salon", "sensor.ci_humidite_salon", "climate.ci_clim"],
    "Chambre CI": ["light.ci_chevet", "sensor.ci_temperature_chambre"],
}
CACHEE = "light.ci_cachee"
PIECES_ATTENDUES = {
    "piece_1_nom": "Salon CI",
    "piece_1_tuiles": ["light.ci_applique", "light.ci_plafonnier", "switch.ci_prise"],
    "piece_1_temperature": "sensor.ci_temperature_salon",
    "piece_1_humidite": "sensor.ci_humidite_salon",
    "piece_1_clim": "climate.ci_clim",
    "piece_2_nom": "Chambre CI",
    "piece_2_tuiles": ["light.ci_chevet"],
    "piece_2_temperature": "sensor.ci_temperature_chambre",
}
BLUEPRINT_ASSISTANT = "tab5/tab5_emplacements.yaml"
# Ce que « Vérifier la configuration » dit de quelques casses, écrit au journal du scénario.
# Le 07/10/2026 (HA 2026.9.4) : clé pas un slug = rien (valide) ; initial pas un booléen et
# domaine pas un dictionnaire = avertissement ; YAML illisible = erreur.
ESSAIS_CASSES = {
    "clé pas un slug": 'input_boolean:\n  "Pas Un Slug": {}\n',
    "initial pas un booléen": CONTENU_CASSE,
    "domaine pas un dictionnaire": "input_boolean: 42\n",
    "YAML illisible": "input_boolean: [pas fermé\n",
}


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
        copie = dossier / COPIE_MAIN
        copie.parent.mkdir(parents=True)
        copie.write_bytes(z.read(f"fichiers/{COPIE_MAIN}") + MARQUE_MAIN)


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


def suggestions(etape: dict) -> dict:
    """Les valeurs pré-remplies d'un formulaire (suggested_value), comme les montre l'interface."""
    return {c["name"]: c["description"]["suggested_value"] for c in etape.get("data_schema") or []
            if "suggested_value" in (c.get("description") or {})}


async def suivre(ha: HA, chemin: str, etape: dict, saisies: list[dict | None]) -> list[dict]:
    """Un flux à plusieurs étapes : `saisies[i]` répond à la i-ème étape (None : accepter les
    valeurs pré-remplies). Renvoie toutes les étapes vues, la dernière comprise."""
    vues = [etape]
    for saisie in saisies:
        if etape.get("type") != "form":
            break
        valeurs = suggestions(etape) if saisie is None else saisie
        etape = await ha.post(f"{chemin}/{etape['flow_id']}", valeurs)
        vues.append(etape)
    return vues


def automatisations_tab5(dossier: Path) -> list[dict]:
    import yaml
    liste = yaml.safe_load((dossier / "automations.yaml").read_text(encoding="utf-8")) or []
    return [a for a in liste if (a.get("use_blueprint") or {}).get("path", "").endswith("tab5_emplacements.yaml")]


async def ranger_dans_des_pieces(ha: HA, rapport: Rapport) -> dict[str, str]:
    """Les pièces de PIECES_CI créées, leurs entités rangées dedans ; la lumière CACHEE cachée."""
    etats = await ha.etats()
    absentes = [e for liste in PIECES_CI.values() for e in liste if e not in etats]
    if absentes:
        raise Echec(f"entités de l'assistant absentes (configuration.yaml du scénario) : {absentes}")
    zones = {}
    for nom, entites in PIECES_CI.items():
        zone = await ha.ws.commande("config/area_registry/create", name=nom)
        zones[nom] = zone["area_id"]
        for e in entites:
            champs = {"hidden_by": "user"} if e == CACHEE else {}
            await ha.ws.commande("config/entity_registry/update", entity_id=e, area_id=zone["area_id"], **champs)
    rapport.ok(f"1 bis. pièces créées et entités rangées : {zones}")
    return zones


async def assistant(ha: HA, dossier: Path, entree: str, rapport: Rapport) -> None:
    """Étape 1 bis : l'assistant de configuration, par la réparation puis par les options."""
    zones = await ranger_dans_des_pieces(ha, rapport)
    rapport.verifier(not automatisations_tab5(dossier), "1 bis. aucune automatisation du blueprint avant")

    # Par la réparation « configurer_pieces », valeurs proposées acceptées.
    reparation = "/api/repairs/issues/fix"
    etape = await ha.post(reparation, {"handler": "tab5", "issue_id": "configurer_pieces"})
    rapport.verifier(etape.get("step_id") == "pieces", "1 bis. la réparation ouvre l'assistant (pièces)",
                     str(etape)[:300])
    proposees = suggestions(etape).get("pieces")
    rapport.verifier(proposees == [zones["Salon CI"], zones["Chambre CI"]],
                     "1 bis. pièces proposées : les deux qui ont des appareils, la mieux équipée d'abord "
                     "(pas les pièces vides de l'onboarding)", str(proposees))
    vues = await suivre(ha, reparation, etape, [None, None, None, {}])
    formulaires = [v for v in vues if v.get("type") == "form"]
    rapport.verifier([v.get("step_id") for v in formulaires] == ["pieces", "piece", "piece", "recapitulatif"],
                     "1 bis. étapes : pièces, pièce ×2, récapitulatif", str([v.get("step_id") for v in vues]))
    if len(formulaires) >= 2:
        rapport.verifier(CACHEE not in json.dumps(formulaires[1]), "1 bis. la lumière cachée n'est pas proposée")
    recap = json.dumps(formulaires[-1].get("description_placeholders") or {}, ensure_ascii=False)
    rapport.verifier("Salon CI" in recap and "Applique CI" in recap and "light.ci_applique" not in recap,
                     "1 bis. le récapitulatif nomme les pièces et les appareils (pas leurs entity_id)", recap[:300])
    rapport.verifier(vues[-1].get("type") == "create_entry", "1 bis. assistant validé", str(vues[-1])[:300])
    nos = automatisations_tab5(dossier)
    rapport.verifier(len(nos) == 1, "1 bis. une automatisation du blueprint dans automations.yaml", str(nos)[:300])
    if nos:
        a = nos[0]
        rapport.verifier(a["use_blueprint"]["path"] == BLUEPRINT_ASSISTANT
                         and a["use_blueprint"].get("input") == PIECES_ATTENDUES,
                         "1 bis. entrées du blueprint = les pièces proposées", str(a)[:400])
        etat_a = next((e for e in (await ha.etats()).values() if e["entity_id"].startswith("automation.")
                       and str(e["attributes"].get("id")) == str(a["id"])), None)
        rapport.verifier(etat_a is not None and etat_a["state"] == "on",
                         "1 bis. automatisation chargée et active", str(etat_a)[:200])
    restes = await attendre_reparations(ha, set(), {"configurer_pieces"})
    rapport.verifier("configurer_pieces" not in restes, "1 bis. réparation « configurer_pieces » retirée", str(restes))
    notes = {n.get("notification_id") for n in await ha.ws.commande("persistent_notification/get")}
    rapport.verifier(f"{NOTIFICATION}_assistant" in notes, "1 bis. notification du résultat")

    # Relancé depuis les options, sans cocher « mettre à jour » : rien ne change.
    avant = (dossier / "automations.yaml").read_bytes()
    options = "/api/config/config_entries/options/flow"
    etape = await ha.post(options, {"handler": entree, "show_advanced_options": False})
    vues = await suivre(ha, options, etape, [
        {"mettre_a_jour_tablette": True, "reinstaller": False, "assistant": True}, None, None, None, None])
    recap = [v for v in vues if v.get("step_id") == "recapitulatif"]
    champs = [c["name"] for c in (recap[0].get("data_schema") or [])] if recap else []
    rapport.verifier(champs == ["mettre_a_jour"], "1 bis. options : l'automatisation existante est vue, case proposée",
                     str(recap)[:300])
    rapport.verifier(vues[-1].get("type") == "create_entry", "1 bis. options : assistant validé sans la case",
                     str(vues[-1])[:300])
    rapport.verifier((dossier / "automations.yaml").read_bytes() == avant,
                     "1 bis. sans la case : automations.yaml à l'octet près")
    sauvegardes = dossier / "tab5_sauvegardes" / "automatisations"
    deja = sorted(sauvegardes.glob("*")) if sauvegardes.is_dir() else []

    # La case cochée, un nom changé : seules les pièces changent, sauvegarde d'abord.
    etape = await ha.post(options, {"handler": entree, "show_advanced_options": False})
    etape = await ha.post(f"{options}/{etape['flow_id']}",
                          {"mettre_a_jour_tablette": True, "reinstaller": False, "assistant": True})
    etape = await ha.post(f"{options}/{etape['flow_id']}", {"pieces": [zones["Salon CI"]]})
    salon = {**suggestions(etape), "nom": "Séjour CI"}
    vues = await suivre(ha, options, etape, [salon, {"mettre_a_jour": True}])
    rapport.verifier(vues[-1].get("type") == "create_entry", "1 bis. options : mise à jour validée", str(vues[-1])[:300])
    nos = automatisations_tab5(dossier)
    attendu = {k: v for k, v in PIECES_ATTENDUES.items() if k.startswith("piece_1_")} | {"piece_1_nom": "Séjour CI"}
    rapport.verifier(len(nos) == 1 and nos[0]["use_blueprint"].get("input") == attendu,
                     "1 bis. mise à jour : la même automatisation, ses pièces remplacées (pièce 2 retirée)",
                     str(nos)[:400])
    nouvelles = sorted(sauvegardes.glob("*")) if sauvegardes.is_dir() else []
    rapport.verifier(len(nouvelles) == len(deja) + 1 and nouvelles[-1].read_bytes() == avant,
                     "1 bis. mise à jour : l'ancien automations.yaml sauvegardé", str([p.name for p in nouvelles]))


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


def essais_refuses() -> int:
    """Refus de la 9.9.3 écrits au journal de HA depuis la création du conteneur."""
    r = subprocess.run(["docker", "logs", CONTENEUR], capture_output=True, text=True)
    return sum("Fichiers Tab5 9.9.3 refusés par la vérification" in l
               for l in (r.stdout + r.stderr).splitlines())


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
            presentes = await attendre_reparations(ha, {"fichiers_remplaces", "configurer_pieces"}, set())
            rapport.verifier(presentes == {"fichiers_remplaces", "configurer_pieces"},
                             "1. deux réparations : « fichiers_remplaces » (blueprint copié à la main) et "
                             "« configurer_pieces » (assistant proposé)", str(presentes))
            rapport.verifier("tab5_emplacements.yaml" in texte, "1. la notification nomme le blueprint remplacé",
                             texte[:300])
            premieres = sorted((dossier / "tab5_sauvegardes").glob("*"))
            rapport.verifier(len(premieres) == 1 and (premieres[0] / COPIE_MAIN).is_file()
                             and (premieres[0] / COPIE_MAIN).read_bytes().endswith(MARQUE_MAIN),
                             "1. la copie à la main gardée dans la sauvegarde", str([p.name for p in premieres]))
            # Validée comme dans Paramètres → Réparations : le formulaire cite la sauvegarde.
            etape = await ha.post("/api/repairs/issues/fix", {"handler": "tab5", "issue_id": "fichiers_remplaces"})
            cites = json.dumps(etape.get("description_placeholders") or {}, ensure_ascii=False)
            rapport.verifier(etape.get("type") == "form" and bool(premieres) and premieres[0].name in cites
                             and COPIE_MAIN in cites, "1. la réparation cite le fichier et la sauvegarde",
                             str(etape)[:300])
            if etape.get("flow_id"):
                fin = await ha.post(f"/api/repairs/issues/fix/{etape['flow_id']}", {})
                rapport.verifier(fin.get("type") == "create_entry", "1. réparation validée", str(fin)[:200])
            restes = await attendre_reparations(ha, set(), {"fichiers_remplaces"})
            rapport.verifier(restes == {"configurer_pieces"}, "1. reste l'assistant proposé", str(restes))
            manifeste = await ha.ws.commande("manifest/get", integration="tab5")
            rapport.verifier(manifeste.get("version") == "9.9.1", "1. version de l'intégration = 9.9.1",
                             str(manifeste.get("version")))
            memoire = await ha.storage("tab5.fichiers") or {}
            rapport.verifier((memoire.get("data") or {}).get("version") == "9.9.1",
                             "1. version posée gardée (.storage/tab5.fichiers)")

            # ── 1 bis. Assistant de configuration ──
            await assistant(ha, dossier, entree, rapport)

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
            # Contre-épreuve : « Vérifier la configuration » (la même fonction que
            # l'intégration) voit-elle ces contenus ? Sinon l'étape 3 ne prouve rien.
            for nom, contenu in ESSAIS_CASSES.items():
                dans_conteneur(f"open('/config/{CASSE}', 'w').write({contenu!r})")
                verif = await ha.post("/api/config/core/check_config") or {}
                rapport.info(f"3. contre-épreuve « {nom} » : {verif.get('result')}, "
                             f"erreurs={str(verif.get('errors'))[:160]!r}, "
                             f"avertissements={str(verif.get('warnings'))[:160]!r}")
                if contenu == CONTENU_CASSE:
                    rapport.verifier(bool(verif.get("errors") or verif.get("warnings")),
                                     "3. contre-épreuve : la vérification signale le package de la 9.9.3",
                                     str(verif)[:200])
            dans_conteneur(f"import os; os.remove('/config/{CASSE}')")
            remplacer_integration(dossier, zips["9.9.3"])
            await redemarrer(ha)
            presentes = await attendre_reparations(ha, {"configuration_invalide"}, set(), 120)
            rapport.verifier("configuration_invalide" in presentes,
                             "3. configuration cassée : réparation « configuration_invalide »", str(presentes))
            rapport.verifier(await etat(ha, CAPTEUR) == "9.9.2", "3. capteur toujours à 9.9.2")
            comparer(rapport, dossier, v2, "3. config/ remis comme avant")
            rapport.verifier(not (dossier / CASSE).exists(), f"3. {CASSE} retiré par le retour en arrière")
            memoire = ((await ha.storage("tab5.fichiers") or {}).get("data") or {})
            rapport.verifier((memoire.get("refusee") or {}).get("version") == "9.9.3",
                             "3. refus de la 9.9.3 gardé (.storage/tab5.fichiers)", str(memoire.get("refusee")))
            sauvegardes = sorted(p.name for p in (dossier / "tab5_sauvegardes").iterdir())
            # Même 9.9.3 au redémarrage : plus de nouvel essai (HA-9). La réparation, non
            # persistante, n'est revenue que si l'intégration l'a recréée.
            await redemarrer(ha)
            presentes = await attendre_reparations(ha, {"configuration_invalide"}, set(), 120)
            rapport.verifier("configuration_invalide" in presentes,
                             "3. redémarrage, même 9.9.3 : réparation recréée", str(presentes))
            rapport.verifier(essais_refuses() == 1, "3. redémarrage, même 9.9.3 : pas de nouvel essai",
                             f"{essais_refuses()} refus au journal")
            rapport.verifier(sorted(p.name for p in (dossier / "tab5_sauvegardes").iterdir()) == sauvegardes,
                             "3. redémarrage, même 9.9.3 : aucune sauvegarde de plus", str(sauvegardes))
            comparer(rapport, dossier, v2, "3. config/ toujours en 9.9.2")
            # « Réinstaller » : l'action explicite réessaie ; même refus, même sauvegarde.
            options = await ha.flux("/api/config/config_entries/options/flow", entree,
                                    {"mettre_a_jour_tablette": True, "reinstaller": True})
            rapport.verifier(options.get("type") == "create_entry", "3. options : « Réinstaller » validé",
                             str(options)[:200])
            fin = time.monotonic() + 120
            while essais_refuses() < 2 and time.monotonic() < fin:
                await asyncio.sleep(2)
            rapport.verifier(essais_refuses() == 2, "3. « Réinstaller » : nouvel essai, refusé de nouveau",
                             f"{essais_refuses()} refus au journal")
            rapport.verifier(sorted(p.name for p in (dossier / "tab5_sauvegardes").iterdir()) == sauvegardes,
                             "3. nouvel essai : pas de sauvegarde identique de plus", str(sauvegardes))
            comparer(rapport, dossier, v2, "3. config/ remis comme avant (2e essai)")

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
