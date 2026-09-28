# -*- coding: utf-8 -*-
"""Job « installation dans un HA neuf » (.github/workflows/installation-ha.yml) : ce qui se
vérifie sans conteneur ni tablette.

- preparer_config.py écrit une installation complète : configuration.yaml avec la ligne des
  packages de docs/installation.md, TOUS les packages publics rendus sans qu'il reste un
  placeholder (placeholders_ci.yaml doit suivre les packages), le blueprint tel quel ;
- les entrées données au blueprint par verifier_installation.py en sont bien des entrées, et la
  chaîne « Zones masquées » attendue est celle que la tablette écrira ;
- la tablette virtuelle porte le nom de la vraie (préfixe des actions des packages) ;
- les fonctions pures de verifier_installation.py (clé, traces, horodatages)."""
import os
import re
import sys

import yaml

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(REPO, "tools", "installation_ha"))

import preparer_config as preparer  # noqa: E402
import verifier_installation as verifier  # noqa: E402


def _lire(*chemin):
    with open(os.path.join(REPO, *chemin), encoding="utf-8") as f:
        return f.read()


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


def test_preparer_ecrit_une_installation_complete(tmp_path):
    sortie = tmp_path / "config"
    preparer.preparer(sortie)
    config = (sortie / "configuration.yaml").read_text(encoding="utf-8")
    assert "packages: !include_dir_named packages" in config  # docs/installation.md, étape 4
    assert "automation: !include automations.yaml" in config  # l'API des automatisations écrit là
    publics = sorted(p.name for p in (preparer.HA_DIR / "packages").glob("*.yaml"))
    rendus = sorted(p.name for p in (sortie / "packages").glob("*.yaml"))
    assert rendus == sorted(publics + ["ci_donnees_test.yaml"])
    assert (sortie / "custom_templates" / "tab5_calendar.jinja").exists()
    copie = sortie / "blueprints" / "automation" / preparer.CHEMIN_BLUEPRINT
    assert copie.read_bytes() == preparer.BLUEPRINT.read_bytes()
    assert (sortie / "automations.yaml").read_text(encoding="utf-8").strip() == "[]"


def test_placeholders_ci_couvrent_tous_les_packages(tmp_path):
    """Un placeholder ajouté à un package sans valeur dans placeholders_ci.yaml ferait
    charger à HA une entité « VOTRE_… » : le job testerait une installation ratée."""
    sortie = tmp_path / "config"
    preparer.preparer(sortie)
    restes = []
    for fichier in list((sortie / "packages").glob("*.yaml")) + list((sortie / "custom_templates").iterdir()):
        for n, ligne in enumerate(fichier.read_text(encoding="utf-8").splitlines(), 1):
            code = ligne.split("#", 1)[0] if fichier.suffix == ".yaml" else ligne
            if "VOTRE_" in code:
                restes.append(f"{fichier.name}:{n}")
    assert not restes, f"placeholders sans valeur dans placeholders_ci.yaml : {restes[:10]}"


def test_chaque_secret_des_packages_est_dans_secrets_yaml(tmp_path):
    """Un `!secret` sans sa ligne dans secrets.yaml fait refuser à HA TOUTE sa
    configuration (tab5_tv.yaml, vu par le job le 28/09/2026)."""
    sortie = tmp_path / "config"
    preparer.preparer(sortie)
    secrets = yaml.safe_load((sortie / "secrets.yaml").read_text(encoding="utf-8"))
    demandes = set()
    for fichier in (sortie / "packages").glob("*.yaml"):
        for ligne in fichier.read_text(encoding="utf-8").splitlines():
            demandes.update(re.findall(r"!secret\s+(\w+)", ligne.split("#", 1)[0]))
    assert demandes, "plus aucun !secret : retirer SECRETS de preparer_config.py"
    assert demandes <= set(secrets), demandes - set(secrets)


def test_donnees_de_test_et_configuration_se_lisent():
    for nom in ("donnees_test.yaml", "configuration.yaml", "placeholders_ci.yaml"):
        yaml.load(_lire("tools", "installation_ha", nom), Loader=_Chargeur)


def test_entrees_du_blueprint_et_zones_attendues():
    blueprint = yaml.load(_lire("HomeAssistant_Config", "blueprints", "automation", "tab5",
                                "tab5_emplacements.yaml"), Loader=_Chargeur)
    entrees = {}
    for section in blueprint["blueprint"]["input"].values():
        entrees.update(section.get("input", {}))
    choisies = verifier.entrees_blueprint()
    assert set(choisies) <= set(entrees), set(choisies) - set(entrees)
    # Zones de l'écran (cles_zones du blueprint) : une zone est absente si son entrée
    # n'est pas choisie. Nom de l'entrée → clé de zone.
    vers_zone = {"salon_temperature": "salon", "serre_temperature": "serre", "agenda_travail": "planning"}
    presentes = {vers_zone.get(e, e) for e in choisies}
    kcles = re.search(r"kCles\[kNbZones\] = \{(.*?)\};", _lire("Tab5", "tab5_zones.cpp"), re.S).group(1)
    ordre = re.findall(r'"(\w+)"', kcles)
    assert ordre == blueprint["variables"]["cles_zones"]
    assert verifier.ZONES_ABSENTES == ", ".join(z for z in ordre if z not in presentes)


def test_la_tablette_virtuelle_porte_le_nom_de_la_vraie():
    tablette = _lire("tab5-ha-hmi.yaml")
    nom = re.search(r"^  name: (\S+)$", tablette, re.M).group(1)
    affiche = re.search(r"^  friendly_name: (.+)$", tablette, re.M).group(1).strip()
    workflow = _lire(".github", "workflows", "installation-ha.yml")
    assert f'-s rendu_nom {nom} -s rendu_nom_affiche "{affiche}"' in workflow
    assert f".esphome/build/{nom} " in workflow
    assert "name: ${rendu_nom}" in _lire("tab5-rendu-host.yaml")
    assert verifier.PREFIXE_ACTIONS == nom.replace("-", "_")
    assert verifier.PREFIXE_ENTITES == re.sub(r"\W+", "_", affiche.lower())
    # Le préfixe écrit en dur dans les packages et par défaut dans le blueprint.
    assert f"esphome.{verifier.PREFIXE_ACTIONS}_tab5_maj_" in _lire("HomeAssistant_Config", "packages", "tab5_push.yaml")
    assert f"default: {verifier.PREFIXE_ACTIONS}" in _lire(
        "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")


def test_memes_chemins_sur_main_et_en_pr():
    workflow = yaml.safe_load(_lire(".github", "workflows", "installation-ha.yml"))
    declencheurs = workflow.get("on") or workflow[True]  # « on » lu comme un booléen
    assert declencheurs["push"]["paths"] == declencheurs["pull_request"]["paths"]
    assert "tools/installation_ha/**" in declencheurs["pull_request"]["paths"]


def test_fonctions_pures():
    assert verifier.longueur_cle("") == 0
    assert verifier.longueur_cle("pas de la base64 !") == 0
    assert verifier.longueur_cle("A" * 43 + "=") == 32
    contenu = {"data": {"entries": [{"entry_id": "a", "domain": "demo"}, {"entry_id": "b", "domain": "esphome"}]}}
    assert verifier.entree_esphome(contenu, "b")["domain"] == "esphome"
    assert verifier.entree_esphome(contenu, "c") is None
    assert verifier.horodatage("2026-09-28T08:00:00+00:00") == 1790582400.0
    assert verifier.horodatage(None) == 0.0
    trace = {"trace": {"action/0": [{"path": "action/0"}],
                       "action/1": [{"path": "action/1", "error": "Action esphome.x not found"}]}}
    assert verifier.erreurs_de_trace(trace) == ["action/1 : Action esphome.x not found"]
    assert verifier.erreurs_de_trace({"error": "boum", "trace": {}}) == ["passage : boum"]



def test_journal_de_ha():
    journal = (
        "s6-rc: info: service legacy-services successfully started\n"
        "\x1b[31m2026-09-28 11:17:49.794 ERROR (MainThread) [homeassistant.components.script.tab5_push_alertes] "
        "Tab5 — pousser: Error executing script. Service not found for call_service at pos 1\x1b[0m\n"
        "2026-09-28 11:17:50.000 WARNING (MainThread) [homeassistant.components.automation] Error evaluating condition:\n"
        "  In 'state' condition: unknown entity binary_sensor.m5stack_tab5_home_assistant_hmi_ha_api_status\n"
        "2026-09-28 11:17:51.000 WARNING (MainThread) [homeassistant.helpers.translation] Invalid domain demo.weather\n"
    )
    entrees = verifier.lignes_du_journal(journal)
    assert [e["niveau"] for e in entrees] == ["ERROR", "WARNING", "WARNING"]
    assert entrees[0]["quand"] == 1790587069.794  # 09:17:49.794 UTC (heure d'été de Paris)
    assert "unknown entity" in entrees[1]["message"]
    assert [verifier.concerne_le_tab5(e) for e in entrees] == [True, True, False]


def test_classer_le_journal():
    """Erreur Tab5 : fautive avant comme après la connexion (défaut 3 du 28/09/2026 :
    les poussées attendent la tablette), sauf « Not connected » pendant une déconnexion
    voulue ; avertissements : rapportés."""
    def e(quand, niveau, message):
        return {"quand": quand, "niveau": niveau, "logger": "homeassistant.components.script.tab5_x",
                "message": message}
    entrees = [
        e(10, "ERROR", "Action esphome.tab5_ha_hmi_y not found"),           # avant : fautive
        e(20, "WARNING", "Can't connect to ESPHome API for tab5-ha-hmi"),    # avant : rapportée
        e(120, "ERROR", "Failed … tab5_maj_rdv_prochains: Not connected to tab5-ha-hmi"),  # pendant : rapportée
        e(130, "WARNING", "Already running"),                                # avertissement : rapporté
        e(200, "ERROR", "Failed … Not connected to tab5-ha-hmi"),            # hors fenêtre : fautive
        e(210, "ERROR", "Error rendering"),                                  # fautive
        {"quand": 220, "niveau": "ERROR", "logger": "homeassistant.helpers.translation", "message": "demo"},
    ]
    fautives, groupes, autres = verifier.classer_journal(entrees, connexion=100, deconnexions=[(115, 125)])
    assert len(fautives) == 3
    assert "(avant la connexion)" in fautives[0] and "not found" in fautives[0]
    assert "Not connected" in fautives[1] and "Error rendering" in fautives[2]
    assert sorted(m for m, *_ in groupes) == ["après la connexion", "avant la connexion",
                                              "pendant une déconnexion voulue de la tablette"]
    assert autres == 1


def test_la_tablette_virtuelle_porte_le_modele_de_la_vraie():
    """Les packages et le blueprint reconnaissent la tablette par le modèle de l'appareil
    (bloc `project:`), pas par son nom : la tablette virtuelle doit porter le même."""
    projet = re.search(r"^  project:\n    name: (\S+)", _lire("tab5-ha-hmi.yaml"), re.M).group(1)
    assert re.search(r"^  project:\n    name: " + re.escape(projet) + "$", _lire("tab5-rendu-host.yaml"), re.M)
    modele = projet.split(".", 1)[1]
    assert modele == "tab5-ha-hmi"


def test_chaque_poussee_attend_la_tablette():
    """Défauts 2, 3 et 5 du 28/09/2026 : chaque script qui appelle une action de la
    tablette commence par la garde « tablette connectée » (trouvée par son modèle), et
    le blueprint pousse aussi au rechargement des automatisations (défaut 4)."""
    garde = "select('eq', 'tab5-ha-hmi')"
    for nom in ("tab5_push", "tab5_calendar", "tab5_reveil"):
        paquet = yaml.load(_lire("HomeAssistant_Config", "packages", f"{nom}.yaml"), Loader=_Chargeur)
        for script, corps in (paquet.get("script") or {}).items():
            if "esphome.tab5_ha_hmi_" not in yaml.dump(corps, allow_unicode=True):
                continue
            premiere = corps["sequence"][0]
            assert premiere.get("condition") == "template" and garde in premiere["value_template"], \
                f"{nom}.yaml, script {script} : la garde « tablette connectée » doit ouvrir la séquence"
    push = _lire("HomeAssistant_Config", "packages", "tab5_push.yaml")
    assert "entity_id: binary_sensor.m5stack_tab5_home_assistant_hmi_ha_api_status" not in push
    reveil = _lire("HomeAssistant_Config", "packages", "tab5_reveil.yaml")
    assert "not_to: [unavailable, unknown]" in reveil
    blueprint = _lire("HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
    assert "event_type: automation_reloaded" in blueprint and garde in blueprint
