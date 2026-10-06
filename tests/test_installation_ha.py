# -*- coding: utf-8 -*-
"""Job « installation dans un HA neuf » (.github/workflows/installation-ha.yml) : ce qui se
vérifie sans conteneur ni tablette.

- preparer_config.py écrit une installation complète : configuration.yaml avec la ligne des
  packages de docs/installation.md, TOUS les packages publics tels quels (ADR-0024 : aucun
  placeholder, aucun `!secret`), le blueprint tel quel ; les optionnels seulement sur demande ;
- chaque entité `…tab5_…` que lisent les packages est définie par un package (listes
  « Tab5 · … », miroirs, capteurs) : son entity_id vient de son NOM, une faute ne se
  verrait qu'à l'exécution ;
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
    installes = sorted(p.name for p in (sortie / "packages").glob("*.yaml"))
    assert installes == sorted(publics + ["ci_donnees_test.yaml"])
    for source in (preparer.HA_DIR / "packages").glob("*.yaml"):
        assert (sortie / "packages" / source.name).read_bytes() == source.read_bytes(), source.name
    assert (sortie / "custom_templates" / "tab5_calendar.jinja").exists()
    copie = sortie / "blueprints" / "automation" / preparer.CHEMIN_BLUEPRINT
    assert copie.read_bytes() == preparer.BLUEPRINT.read_bytes()
    assert (sortie / "automations.yaml").read_text(encoding="utf-8").strip() == "[]"
    # Package optionnel (volet à course simulée) : pas par défaut, le volet choisi dans
    # sa liste passerait par son script (variables volet_suivi, volet_par_package).
    assert not (sortie / "packages" / "volet_serre_tracking.yaml").exists()


def test_preparer_avec_les_optionnels(tmp_path):
    sortie = tmp_path / "config"
    preparer.preparer(sortie, optionnels=True)
    for source in (preparer.HA_DIR / "optionnel").glob("*.yaml"):
        assert (sortie / "packages" / source.name).read_bytes() == source.read_bytes()


def test_installation_sans_rien_remplir(tmp_path):
    """ADR-0024 : ni placeholder ni `!secret` dans ce qu'on installe. Un `!secret` sans
    sa ligne dans secrets.yaml fait refuser à HA TOUTE sa configuration (tab5_tv.yaml,
    vu par le job le 28/09/2026) ; un placeholder ferait charger une entité « VOTRE_… »."""
    sortie = tmp_path / "config"
    preparer.preparer(sortie, optionnels=True)
    restes, secrets = [], []
    for fichier in list((sortie / "packages").glob("*.yaml")) + list((sortie / "custom_templates").iterdir()):
        for n, ligne in enumerate(fichier.read_text(encoding="utf-8").splitlines(), 1):
            code = ligne.split("#", 1)[0] if fichier.suffix == ".yaml" else ligne
            if "VOTRE_" in code:
                restes.append(f"{fichier.name}:{n}")
            if re.search(r"!secret\b", code):
                secrets.append(f"{fichier.name}:{n}")
    assert not restes, f"placeholders : {restes[:10]}"
    assert not secrets, f"!secret : {secrets[:10]}"
    assert "tab5" not in (sortie / "secrets.yaml").read_text(encoding="utf-8")


def test_donnees_de_test_et_configuration_se_lisent():
    for nom in ("donnees_test.yaml", "configuration.yaml"):
        yaml.load(_lire("tools", "installation_ha", nom), Loader=_Chargeur)


def _slug(nom: str) -> str:
    """entity_id tiré d'un nom par Home Assistant (util.slugify : accents retirés,
    ponctuation en « _ »)."""
    import unicodedata

    texte = unicodedata.normalize("NFKD", nom).encode("ascii", "ignore").decode().lower()
    return re.sub(r"[^a-z0-9]+", "_", texte).strip("_")


def _definies() -> set[str]:
    """entity_id des entités définies par les packages et les optionnels."""
    definies = set()
    fichiers = sorted((preparer.HA_DIR / "packages").glob("*.yaml")) + sorted((preparer.HA_DIR / "optionnel").glob("*.yaml"))
    for fichier in fichiers:
        paquet = yaml.load(fichier.read_text(encoding="utf-8"), Loader=_Chargeur) or {}
        for domaine in ("input_text", "input_select", "input_boolean", "script", "rest_command"):
            definies |= {f"{domaine}.{cle}" for cle in (paquet.get(domaine) or {})}
        for bloc in paquet.get("template") or []:
            for domaine in ("sensor", "binary_sensor", "select", "weather"):
                for entite in bloc.get(domaine) or []:
                    # default_entity_id fixe l'entity_id (noms bilingues, 29/09/2026) ;
                    # sans lui, HA le tire du nom.
                    fixe = entite.get("default_entity_id")
                    if fixe:
                        assert fixe.split(".", 1)[0] == domaine, fixe
                    definies.add(fixe or f"{domaine}.{_slug(entite['name'])}")
    return definies


def test_listes_bilingues_gardent_leur_entity_id():
    """Une liste « Tab5 · … » au nom bilingue (« français · english ») fixe son entity_id
    par default_entity_id : sinon, une installation neuve le tirerait du nom complet et
    les packages ne la trouveraient plus. Celles d'avant gardent l'entity_id d'avant."""
    fichiers = sorted((preparer.HA_DIR / "packages").glob("*.yaml")) + sorted((preparer.HA_DIR / "optionnel").glob("*.yaml"))
    bilingues = {}
    for fichier in fichiers:
        paquet = yaml.load(fichier.read_text(encoding="utf-8"), Loader=_Chargeur) or {}
        for bloc in paquet.get("template") or []:
            for domaine in ("sensor", "binary_sensor", "select"):
                for entite in bloc.get(domaine) or []:
                    if entite["name"].count(" · ") >= 2:
                        assert entite.get("default_entity_id"), entite["name"]
                        bilingues[entite["default_entity_id"]] = entite["name"]
    assert {"select.tab5_agenda_de_travail", "select.tab5_source_des_previsions", "select.tab5_tv_samsung",
            "select.tab5_telephone", "select.tab5_capteur_de_presence",
            "select.tab5_agenda_des_vacances_scolaires", "select.tab5_pipeline_de_discussion"} <= set(bilingues)


def test_entites_tab5_lues_sont_definies():
    """Les packages se lisent entre eux par entity_id (listes « Tab5 · … », miroirs de
    tab5_reglages.yaml, capteurs météo). Celui d'un modèle vient de son NOM : une faute
    (« select.tab5_agenda_travail » pour « Tab5 · agenda de travail ») ne se verrait
    qu'à l'exécution, en silence."""
    definies = _definies()
    fichiers = sorted((preparer.HA_DIR / "packages").glob("*.yaml")) + sorted((preparer.HA_DIR / "optionnel").glob("*.yaml"))
    lues = set()
    for fichier in fichiers:
        # Hors commentaires YAML (ils citent d'anciens noms, pour mémoire).
        texte = "\n".join(ligne for ligne in fichier.read_text(encoding="utf-8").splitlines()
                          if not ligne.lstrip().startswith("#"))
        lues |= set(re.findall(r"\b((?:sensor|binary_sensor|select|input_text|input_select|input_boolean)\.tab5_[a-z0-9_]+)", texte))
    assert lues, "aucune entité tab5_ lue"
    assert not lues - definies, sorted(lues - definies)
    assert set(verifier.SOURCES) <= definies
    assert {e for e, _ in verifier.DEDUITS} <= definies


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
    # « discussion » n'est pas une entrée (29/09/2026) : absente quand la liste « Tab5 ·
    # pipeline de discussion » vaut « Aucun », présente sans cette liste. Tant qu'aucun
    # package ne la définit, le HA neuf du job ne l'a pas : la zone reste. Une fois la
    # liste livrée, un HA neuf n'a choisi aucun pipeline (« Aucun ») : ZONES_ABSENTES de
    # verifier_installation.py doit alors finir par « discussion ».
    if "select.tab5_pipeline_de_discussion" not in _definies():
        presentes.add("discussion")
    kcles = re.search(r"kCles\[kNbZones\] = \{(.*?)\};", _lire("Tab5", "tab5_zones.cpp"), re.S).group(1)
    ordre = re.findall(r'"(\w+)"', kcles)
    assert ordre == blueprint["variables"]["cles_zones"]
    assert verifier.ZONES_ABSENTES == ", ".join(z for z in ordre if z not in presentes)


def test_pieces_du_job_et_protocole():
    """Pièces (ADR-0023) : le job configure au moins deux pièces, la pièce 1 vide (accueil
    depuis les entrées 3.x), et lit le protocole comme le blueprint."""
    remplies = [k for k, v in verifier.PIECES.items() if k.endswith("_tuiles") and v]
    assert len(remplies) >= 2 and "piece_1_tuiles" not in verifier.PIECES
    assert any(len(v) > 5 for k, v in verifier.PIECES.items() if k.endswith("_tuiles")), "la limite de 5 est éprouvée"
    blueprint = yaml.load(_lire("HomeAssistant_Config", "blueprints", "automation", "tab5",
                                "tab5_emplacements.yaml"), Loader=_Chargeur)
    assert verifier.TYPES_PAR_DOMAINE == blueprint["variables"]["types_par_domaine"]
    assert verifier.icones_du_blueprint(_lire("HomeAssistant_Config", "blueprints", "automation", "tab5",
                                              "tab5_emplacements.yaml")) == blueprint["variables"]["icones_mdi"]
    for version, attendu in (("3.1.0 (ESPHome 2026.9.0)", 1), ("3.2.0-rendu (ESPHome 2026.9.0)", 2),
                             ("rendu (ESPHome 2026.9.0)", 1), (None, 1), ("3.10.0", 2)):
        assert verifier.protocole_de(version) == attendu, version
    cles = [c for c, _ in verifier.tuiles_attendues()]
    assert cles[:5] == ["t00", "t01", "t02", "t03", "t04"] and "t15" not in cles


def test_ce_que_le_job_attend_est_ce_que_calcule_le_blueprint():
    """Le blueprint, rendu ici (tests/test_tuiles_blueprint.py) avec les entrées du job
    et des états imitant l'intégration demo, donne exactement ce que
    verifier_installation.py attend : un écart se voit avant la CI."""
    sys.path.insert(0, os.path.join(REPO, "tests"))
    import test_tuiles_blueprint as tuiles

    attributs = {
        "light.kitchen_lights": {"supported_color_modes": ["color_temp", "hs"]},
        "light.office_rgbw_lights": {"supported_color_modes": ["rgbw"]},
        "light.bed_light": {"supported_color_modes": ["color_temp", "hs"]},
        "light.ceiling_lights": {"supported_color_modes": ["color_temp", "hs"]},
        "sensor.outside_temperature": {"unit_of_measurement": "°C", "device_class": "temperature"},
        "binary_sensor.movement_backyard": {"device_class": "motion"},
    }
    etats = []
    for entite in verifier.entites_de_test():
        nom = entite.split(".", 1)[1].replace("_", " ").title()
        etats.append(tuiles.Etat(entite, "on", friendly_name=nom, **attributs.get(entite, {})))
    passage = tuiles.Passage(verifier.entrees_blueprint(), etats, {"id": "connexion"},
                             tuiles._tablette("rendu (ESPHome 2026.9.0)"))
    assert passage["protocole"] == 1
    definitions = passage.definitions()
    icones = verifier.icones_du_blueprint(_lire("HomeAssistant_Config", "blueprints", "automation", "tab5",
                                                "tab5_emplacements.yaml"))
    assert verifier.juger_definitions(definitions, icones) == []
    etats_tuiles = verifier.entrees_de(passage.etats_tuiles())
    assert [e[0] for e in etats_tuiles] == [c for c, _ in verifier.tuiles_attendues()]
    # Clim de tuile (ADR-0027) : ses clés cr/ce, pas celles de la clim du blueprint.
    reglages, clims = passage["reglages_tuiles"], passage["etats_clims"]
    assert verifier.juger_clims(reglages, clims) == []
    assert (verifier.CLIM_DE_TUILE, "cli") in verifier.tuiles_attendues()
    assert verifier.EMPLACEMENTS["clim"] not in [t["e"] for t in passage["tuiles_clim"]]
    # Et le juge voit un écart.
    assert verifier.juger_definitions(definitions.replace("|Lights;", "|Kitchen Lights;"), icones)
    assert verifier.juger_clims(reglages.replace("cr24", "cr14"), clims)
    assert verifier.juger_clims(reglages, "")
    assert verifier.juger_clims(reglages.replace("|°C|", "|C|"), clims)


def test_trace_variables_et_appels():
    trace = {"trace": {
        "trigger/0": [{"path": "trigger/0", "changed_variables": {"tuiles": [], "protocole": 1}}],
        "action/2/then/0": [{"path": "action/2/then/0", "changed_variables": {"definitions": "p0|;"}}],
        "action/1/default/0/then/0": [{"path": "x", "result": {"params": {
            "domain": "esphome", "service": "tab5_ha_hmi_tab5_maj_emplacements",
            "service_data": {"payload": "tv|on|nan;"}, "target": {}}, "running_script": False}}],
    }}
    assert verifier.variables_de_trace(trace) == {"tuiles": [], "protocole": 1, "definitions": "p0|;"}
    assert [a["service"] for a in verifier.appels_de_trace(trace)] == ["tab5_ha_hmi_tab5_maj_emplacements"]
    assert verifier.entrees_de("p0|Salon;t00|lum||dc||Lampe;") == [["p0", "Salon"], ["t00", "lum", "", "dc", "", "Lampe"]]


def test_passage_de_la_meteo_reconnu_a_son_declencheur():
    """Le blueprint a deux déclencheurs sur automation_reloaded : rechargement (poussée,
    doit aboutir) et meteo_rechargement (section « Météo », arrêté aux conditions quand il
    n'y a rien à écrire). Seul le second est excusé par juger_passages ; les identifiants
    sont ceux du blueprint."""
    def trace(id_):
        return {"trace": {"trigger/0": [{"path": "trigger/0", "changed_variables": {
            "trigger": {"id": id_, "platform": "event", "description": "event 'automation_reloaded'"}}}]}}

    assert verifier.declencheur_meteo(trace("meteo_rechargement"))
    assert not verifier.declencheur_meteo(trace("rechargement"))
    assert not verifier.declencheur_meteo({"trace": {}})
    blueprint = yaml.load(_lire("HomeAssistant_Config", "blueprints", "automation", "tab5",
                                "tab5_emplacements.yaml"), Loader=_Chargeur)
    ids = [t["id"] for t in blueprint["triggers"]]
    assert {i for i in ids if i.startswith("meteo_")} == set(blueprint["variables"]["meteo_declencheurs"])
    assert "rechargement" in ids and not any(verifier.declencheur_meteo(trace(i)) for i in ids
                                             if i not in blueprint["variables"]["meteo_declencheurs"])


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


def test_plus_d_option_actions_ha():
    """ADR-0025 : « Autoriser l'appareil à effectuer des actions Home Assistant » n'est plus
    une étape. La CI ne la coche plus (et vérifie qu'elle reste décochée) ; le guide ne la
    donne plus comme étape ; les demandes de la tablette sont exercées de bout en bout,
    et leurs fichiers relancent le job."""
    verif = _lire("tools", "installation_ha", "verifier_installation.py")
    assert '"allow_service_calls": True' not in verif and "verifier_option_decochee(" in verif
    assert "demandes_de_la_tablette(ha, ws, rapport)" in verif
    guide = _lire("docs", "installation.md") + "".join(
        _lire("docs", "installation", f) for f in sorted(os.listdir(os.path.join(REPO, "docs", "installation"))))
    for etape in ("**Allow Home Assistant actions**", "**Autoriser les actions Home Assistant**"):
        assert etape not in guide, etape
    # Les événements écoutés par le vérificateur sont bien émis par le firmware.
    for evt in (verifier.EVT_MOIS, verifier.EVT_MAJ_ECRAN, verifier.EVT_REDEMARRAGE):
        assert f"event: {evt}" in "".join(_lire(*c) for c in (
            ("Tab5", "tab5-calendar.yaml"), ("Tab5", "ui_components", "console_sys.yaml")))
    assert f"id: {verifier.ID_EVENEMENTS}" in _lire("HomeAssistant_Config", "packages", "tab5_evenements.yaml")
    assert verifier.SELECT_ECRAN.endswith("_aller_a_l_ecran")
    assert "name: \"Aller à l'écran\"" in _lire("Tab5", "tab5-ha-controls.yaml")
    chemins = yaml.safe_load(_lire(".github", "workflows", "installation-ha.yml"))
    chemins = (chemins.get("on") or chemins[True])["pull_request"]["paths"]
    for fichier in ("Tab5/tab5-alarm.yaml", "Tab5/tab5-assist.yaml", "Tab5/tab5-calendar.yaml",
                    "Tab5/tab5-ha-controls.yaml", "Tab5/ui_components/console_sys.yaml"):
        assert fichier in chemins, fichier


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


def test_calendrier_chaque_demande_a_sa_reponse():
    """La tablette demande d'affilée le mois affiché et ses voisins (pré-chargement
    M-1 / M+1, Tab5/tab5-calendar.yaml) : en `mode: restart`, une seule des trois
    demandes aboutissait (job « HA neuf », 28/09/2026). Les deux scripts appelés par
    le popup calendrier sont en file, assez longue pour ces trois demandes."""
    assert "M-1, M+1" in _lire("Tab5", "tab5-calendar.yaml"), "pré-chargement des mois voisins disparu ?"
    paquet = yaml.load(_lire("HomeAssistant_Config", "packages", "tab5_calendar.yaml"), Loader=_Chargeur)
    for script in ("tab5_calendrier_mois", "tab5_calendrier_jour"):
        corps = paquet["script"][script]
        assert corps["mode"] == "queued", f"{script} : mode {corps['mode']}"
        assert corps.get("max", 10) >= 3, f"{script} : max {corps.get('max')}"
