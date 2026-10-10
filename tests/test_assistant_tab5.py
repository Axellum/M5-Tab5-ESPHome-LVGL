# -*- coding: utf-8 -*-
"""Assistant de configuration de l'intégration « Tab5 » (ADR-0052), sans Home Assistant.

custom_components/tab5/assistant.py : ce que l'assistant propose d'après les registres de
HA (pièces, tuiles, température, humidité, clim), les entrées du blueprint qu'il construit,
et automations.yaml (ajout, mise à jour des seules pièces, sauvegarde, refus d'écrire quand
le fichier n'est pas sûr). Les formulaires eux-mêmes (assistant_flux.py) tournent dans un
vrai Home Assistant : tools/installation_ha/verifier_integration.py (CI integration-hacs.yml).
"""
import importlib.util
import json
import re
import sys
from pathlib import Path

import pytest
import yaml

from tests.commun import ChargeurEntrees, lire

REPO = Path(__file__).resolve().parent.parent
INTEGRATION = REPO / "custom_components" / "tab5"


def _module(nom: str):
    spec = importlib.util.spec_from_file_location(f"tab5_{nom}", INTEGRATION / f"{nom}.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module  # @dataclass relit son module dans sys.modules
    spec.loader.exec_module(module)
    return module


assistant = _module("assistant")
installation = _module("installation")
messages = _module("messages")
const = _module("const")

A = assistant
TABLETTE = const.MODELE_TABLETTE


def _blueprint() -> dict:
    return yaml.load(lire("HomeAssistant_Config", installation.BLUEPRINT), Loader=ChargeurEntrees)


def _entrees_du_blueprint() -> dict:
    """{clé d'entrée : sa définition}, sections aplaties."""
    plat = {}
    for cle, valeur in _blueprint()["blueprint"]["input"].items():
        if isinstance(valeur, dict) and "input" in valeur:
            plat.update(valeur["input"])
        else:
            plat[cle] = valeur
    return plat


# ─── Le blueprint et l'assistant parlent des mêmes entrées ───────────────────

def test_entrees_des_pieces_dans_le_blueprint():
    entrees = _entrees_du_blueprint()
    for n in range(1, A.MAX_PIECES + 1):
        for champ in A.CHAMPS_PIECE:
            assert f"piece_{n}_{champ}" in entrees, f"piece_{n}_{champ} absente du blueprint"
        assert A.ENTREE_PIECE.match(f"piece_{n}_tuiles")
    assert not any(A.ENTREE_PIECE.match(c) for c in entrees if not c.startswith("piece_")), \
        "seules les entrées des pièces sont remplacées par une mise à jour"
    filtre = entrees["piece_1_tuiles"]["selector"]["entity"]["filter"][0]["domain"]
    assert tuple(filtre) == A.DOMAINES_TUILES, "même liste de domaines que le sélecteur des tuiles"
    assert set(A.DOMAINES_AUTO) <= set(A.DOMAINES_TUILES)
    for champ, (domaine, classe) in {"temperature": ("sensor", "temperature"), "humidite": ("sensor", "humidity"),
                                     "clim": ("climate", None)}.items():
        f = entrees[f"piece_1_{champ}"]["selector"]["entity"]["filter"][0]
        assert f["domain"] == domaine and f.get("device_class") == classe, champ
    assert installation.BLUEPRINT == f"blueprints/automation/{A.BLUEPRINT_CHEMIN}"
    assert installation.BLUEPRINT.endswith(A.BLUEPRINT_NOM)


# ─── Propositions ────────────────────────────────────────────────────────────

ZONES = [A.Zone("salon", "Salon"), A.Zone("chambre", "Chambre"), A.Zone("garage", "Garage"),
         A.Zone("bureau", "Bureau"), A.Zone("cave", "Cave")]
APPAREILS = {
    "lampe": A.Appareil("lampe", "salon"),
    "tablette": A.Appareil("tablette", "salon", TABLETTE),
    "thermo": A.Appareil("thermo", "chambre"),
}


def _maison() -> list:
    E = A.Entite
    return [
        # Salon : 7 tuiles possibles (5 retenues, lumières d'abord), clim, deux températures.
        E("media_player.tv", "TV", zone="salon"),
        E("switch.prise", "Prise", zone="salon"),
        E("light.plafond", "Plafond", appareil="lampe"),          # pièce de son appareil
        E("light.applique", "Applique", zone="salon"),
        E("cover.volet", "Volet", zone="salon"),
        E("fan.ventilo", "Ventilateur", zone="salon"),
        E("light.zz_lampadaire", "Lampadaire", zone="salon"),
        E("climate.salon", "Clim salon", zone="salon"),
        E("sensor.t_salon", "Température salon", zone="salon", classe="temperature"),
        E("sensor.a_temp", "A temp", zone="salon", classe="temperature"),
        E("sensor.h_salon", "Humidité salon", zone="salon", classe="humidity"),
        # Jamais proposées.
        E("light.desactivee", "Désactivée", zone="salon", desactivee=True),
        E("light.cachee", "Cachée", zone="salon", cachee=True),
        E("switch.config", "Réglage", zone="salon", categorie="config"),
        E("sensor.diag", "Diag", zone="salon", classe="temperature", categorie="diagnostic"),
        E("light.absente", "Absente", zone="salon", presente=False),
        E("light.ecran_tab5", "Écran", appareil="tablette"),              # la tablette elle-même
        E("sensor.tab5_temp", "Tab5 temp", appareil="tablette", classe="temperature"),
        E("scene.soir", "Soir", zone="salon"),                            # pas proposée d'office
        # Chambre : la pièce de l'entité prime sur celle de son appareil.
        E("light.chevet", "Chevet", zone="chambre"),
        E("sensor.t_chambre", "Température chambre", appareil="thermo", classe="temperature"),
        E("light.deplacee", "Déplacée", zone="garage", appareil="thermo"),
        # Bureau : seulement une température. Cave : rien d'utile. Sans pièce : ignorée.
        E("sensor.t_bureau", "Température bureau", zone="bureau", classe="temperature"),
        E("sensor.puissance", "Puissance", zone="cave", classe="power"),
        E("light.sans_piece", "Sans pièce"),
    ]


def test_proposition_par_piece():
    parzone = A.par_zone(_maison(), APPAREILS, TABLETTE)
    salon = A.proposer_piece(ZONES[0], parzone["salon"])
    assert salon.nom == "Salon"
    assert salon.tuiles == ("light.applique", "light.zz_lampadaire", "light.plafond", "cover.volet", "switch.prise"), \
        "lumières puis volets puis interrupteurs (puis ventilateurs, lecteurs), par nom, cinq au plus"
    assert salon.temperature == "sensor.a_temp", "la première par nom"
    assert salon.humidite == "sensor.h_salon" and salon.clim == "climate.salon"
    utiles = {e.entity_id for e in parzone["salon"]}
    for jamais in ("light.desactivee", "light.cachee", "switch.config", "sensor.diag", "light.absente",
                   "light.ecran_tab5", "sensor.tab5_temp"):
        assert jamais not in utiles, jamais
    chambre = A.proposer_piece(ZONES[1], parzone["chambre"])
    assert chambre.tuiles == ("light.chevet",) and chambre.temperature == "sensor.t_chambre"
    assert chambre.clim is None and chambre.humidite is None
    assert [e.entity_id for e in parzone["garage"]] == ["light.deplacee"], "sa pièce à elle, pas celle de l'appareil"
    assert "light.sans_piece" not in {e.entity_id for liste in parzone.values() for e in liste}


def test_classement_des_pieces():
    parzone = A.par_zone(_maison(), APPAREILS, TABLETTE)
    classees = [z.id for z in A.classer_zones(ZONES, parzone)]
    assert classees == ["salon", "chambre", "garage", "bureau"], \
        "les plus pilotables d'abord, une pièce sans rien à montrer (cave) écartée"
    assert A.classer_zones(ZONES, {}) == [], "aucune entité dans une pièce : rien à proposer"
    assert A.classer_zones([], parzone) == []


# ─── Entrées du blueprint ────────────────────────────────────────────────────

def test_entrees_blueprint():
    pieces = [A.Piece("salon", " Salon ", ("light.a", "cover.b"), "sensor.t", None, "climate.c"),
              A.Piece("chambre", "", (), None, "sensor.h", None)]
    assert A.entrees_blueprint(pieces) == {
        "piece_1_nom": "Salon", "piece_1_tuiles": ["light.a", "cover.b"], "piece_1_temperature": "sensor.t",
        "piece_1_clim": "climate.c", "piece_2_humidite": "sensor.h"}
    with pytest.raises(ValueError):
        A.entrees_blueprint([A.Piece()] * 6)
    with pytest.raises(ValueError):
        A.entrees_blueprint([A.Piece(tuiles=tuple(f"light.{i}" for i in range(6)))])
    assert all(A.ENTREE_PIECE.match(c) for c in A.entrees_blueprint(pieces))


def test_fusionner_ne_remplace_que_les_pieces():
    anciennes = {"piece_1_nom": "Séjour", "piece_4_tuiles": ["light.x"], "piece_5_clim": "climate.y",
                 "meteo_entite": "weather.maison", "gestes": ["auto"], "piece_10_nom": "pas une pièce"}
    nouvelles = {"piece_1_nom": "Salon", "piece_2_tuiles": ["light.a"]}
    assert A.fusionner(anciennes, nouvelles) == {
        "piece_1_nom": "Salon", "piece_2_tuiles": ["light.a"], "meteo_entite": "weather.maison",
        "gestes": ["auto"], "piece_10_nom": "pas une pièce"}
    assert A.fusionner(None, nouvelles) == nouvelles


# ─── automations.yaml ────────────────────────────────────────────────────────

CONF_HA = "default_config:\n\nautomation: !include automations.yaml\nscript: !include scripts.yaml\n"
AUTRE = {"id": "1", "alias": "Lumière du soir", "triggers": [{"trigger": "sun", "event": "sunset"}],
         "actions": [{"action": "light.turn_on", "target": {"entity_id": "light.a"}}]}


def _config(tmp_path: Path, automations: str, configuration: str = CONF_HA) -> Path:
    tmp_path.mkdir(parents=True, exist_ok=True)
    (tmp_path / "configuration.yaml").write_text(configuration, encoding="utf-8")
    (tmp_path / A.FICHIER).write_text(automations, encoding="utf-8")
    return tmp_path


def test_inclusion_d_automations_yaml():
    assert A.inclut_automations(CONF_HA)
    assert A.inclut_automations("automation ui: !include automations.yaml  # éditeur\n")
    assert not A.inclut_automations("automation: !include_dir_merge_list automations/\n")
    assert not A.inclut_automations("# automation: !include automations.yaml\n")
    assert not A.inclut_automations("homeassistant:\n  automation: !include automations.yaml\n")
    assert not A.inclut_automations("automation: !include mes_automations.yaml\n")


def test_lire_refuse_ce_qui_n_est_pas_sur():
    assert A.lire("") == [] and A.lire("[]\n") == [] and A.lire("# rien\n") == []
    for texte in ("- id: '1'\n  token: !secret jeton\n", "- !include une.yaml\n", "a: 1\n", "- 1\n- 2\n",
                  "- id: [pas fermé\n"):
        with pytest.raises(A.FichierInutilisable):
            A.lire(texte)


def test_du_blueprint_et_nouvel_id():
    liste = [AUTRE, {"id": "2", "use_blueprint": {"path": "tab5/tab5_emplacements.yaml"}},
             {"id": "3", "use_blueprint": {"path": "Axellum/tab5_emplacements.yaml"}},
             {"id": "4", "use_blueprint": {"path": "autre/motion_light.yaml"}}]
    assert [a["id"] for a in A.du_blueprint(liste)] == ["2", "3"], "le nôtre et une copie importée par URL"
    assert A.nouvel_id(liste, 7) == "7"
    assert A.nouvel_id([{"id": "7"}, {"id": 8}], 7) == "9", "jamais un id déjà pris"


def test_ajouter_garde_le_fichier_tel_quel(tmp_path):
    nouvelle = A.automatisation("42", {"piece_1_nom": "Séjour"}, "fr")
    assert nouvelle["use_blueprint"]["path"] == A.BLUEPRINT_CHEMIN
    assert nouvelle["alias"] == "Tab5 — emplacements de l'écran"
    assert A.automatisation("42", {}, "en")["alias"] == "Tab5 — screen slots"
    existant = "# mes automatisations\n" + A.vers_yaml([AUTRE]) + "# fin\n"
    resultat = A.ajouter(existant, nouvelle)
    assert resultat.startswith(existant), "le texte d'avant (commentaires compris) n'est pas touché"
    assert A.lire(resultat) == [AUTRE, nouvelle]
    assert "Séjour" in resultat, "accents gardés, comme l'éditeur de HA"
    assert A.lire(A.ajouter(existant.replace("\n", "\r\n"), nouvelle)) == [AUTRE, nouvelle], "fichier en CRLF"
    # Vide (« [] » d'un HA neuf, ou rien) : la liste devient la nôtre.
    assert A.lire(A.ajouter("[]\n", nouvelle)) == [nouvelle]
    assert A.ajouter("# vide\n[]\n", nouvelle).startswith("# vide\n")
    assert A.lire(A.ajouter("", nouvelle)) == [nouvelle]
    # Liste en ligne : réécrite comme le fait HA, rien de perdu.
    en_ligne = json.dumps([AUTRE])
    assert A.lire(A.ajouter(en_ligne, nouvelle)) == [AUTRE, nouvelle]


def test_situation(tmp_path):
    s = A.situation(_config(tmp_path / "vide", "[]\n"), set())
    assert s.action == "creer" and s.automatisations == []

    nous = {"id": "77", "alias": "Tablette", "use_blueprint": {"path": "tab5/tab5_emplacements.yaml",
                                                                 "input": {"piece_1_nom": "Séjour"}}}
    une = _config(tmp_path / "une", A.vers_yaml([AUTRE, nous]))
    s = A.situation(une, {"77"})
    assert s.action == "mettre_a_jour" and s.existante == nous and s.index == 1
    assert A.situation(une, set()).action == "mettre_a_jour", "écrite mais pas chargée (blueprint absent)"
    assert A.situation(une, {"77", "?automation.ailleurs"}).action == "ailleurs"
    assert A.situation(une, {"autre_id"}).action == "ailleurs", "chargée depuis un autre fichier (package)"

    deux = _config(tmp_path / "deux", A.vers_yaml([nous, {**nous, "id": "78"}]))
    assert A.situation(deux, {"77", "78"}).action == "plusieurs"

    sans = _config(tmp_path / "sans", "[]\n", "default_config:\nautomation: !include_dir_merge_list auto/\n")
    s = A.situation(sans, set())
    assert s.action == "fichier" and "configuration.yaml" in s.raison

    secret = _config(tmp_path / "secret", "- id: '1'\n  x: !secret y\n")
    s = A.situation(secret, set())
    assert s.action == "fichier" and A.FICHIER in s.raison

    (tmp_path / "absent").mkdir()
    (tmp_path / "absent" / "configuration.yaml").write_text(CONF_HA, encoding="utf-8")
    assert A.situation(tmp_path / "absent", set()).action == "creer", "automations.yaml absent : créé"


def test_remplacer_pieces_garde_le_reste():
    nous = {"id": "77", "alias": "Tablette", "description": "à moi",
            "use_blueprint": {"path": "tab5/tab5_emplacements.yaml",
                              "input": {"piece_1_nom": "Séjour", "piece_3_tuiles": ["light.x"],
                                        "meteo_entite": "weather.maison"}}}
    texte = A.vers_yaml([AUTRE, nous])
    resultat = A.lire(A.remplacer_pieces(texte, 1, {"piece_1_nom": "Salon"}))
    assert resultat[0] == AUTRE, "les autres automatisations à l'identique"
    assert resultat[1]["alias"] == "Tablette" and resultat[1]["description"] == "à moi"
    assert resultat[1]["use_blueprint"]["input"] == {"piece_1_nom": "Salon", "meteo_entite": "weather.maison"}
    with pytest.raises(A.FichierInutilisable):  # l'index ne désigne plus une du blueprint
        A.remplacer_pieces(texte, 0, {})
    with pytest.raises(A.FichierInutilisable):
        A.remplacer_pieces(texte, 5, {})


def test_ecrire_sauvegarde_et_refuse_un_fichier_change(tmp_path):
    lu = A.vers_yaml([AUTRE])
    _config(tmp_path, lu)
    (tmp_path / installation.SAUVEGARDES / "20261007-120000_3.7.0").mkdir(parents=True)
    nouveau = A.ajouter(lu, A.automatisation("42", {}, "fr"))
    sauvegarde = A.ecrire_si_inchange(tmp_path, lu, nouveau, "20261010-120000")
    assert sauvegarde == tmp_path / A.SAUVEGARDES / f"20261010-120000_{A.FICHIER}"
    assert sauvegarde.read_text(encoding="utf-8") == lu
    assert (tmp_path / A.FICHIER).read_text(encoding="utf-8") == nouveau
    assert not list(tmp_path.glob("*" + A.TEMPORAIRE))
    # Changé entre la lecture et l'écriture : rien n'est écrit.
    with pytest.raises(A.FichierInutilisable):
        A.ecrire_si_inchange(tmp_path, lu, "[]\n", "20261010-120001")
    assert (tmp_path / A.FICHIER).read_text(encoding="utf-8") == nouveau
    # GARDER sauvegardes au plus ; même étiquette : suffixée.
    for i in range(A.GARDER + 2):
        actuel = (tmp_path / A.FICHIER).read_text(encoding="utf-8")
        A.ecrire_si_inchange(tmp_path, actuel, actuel, "20261011-000000")
    assert len(list((tmp_path / A.SAUVEGARDES).iterdir())) == A.GARDER
    # Les sauvegardes d'installation ne voient pas ce sous-dossier.
    assert installation.derniere_sauvegarde(tmp_path).name == "20261007-120000_3.7.0"
    assert installation.nettoyer_sauvegardes(tmp_path, 0) == ["20261007-120000_3.7.0"]
    assert (tmp_path / A.SAUVEGARDES).is_dir(), "nettoyer_sauvegardes ne touche pas aux automatisations"


def test_ecrire_un_fichier_absent(tmp_path):
    (tmp_path / "configuration.yaml").write_text(CONF_HA, encoding="utf-8")
    nouveau = A.ajouter("", A.automatisation("1", {}, "fr"))
    assert A.ecrire_si_inchange(tmp_path, "", nouveau, "20261010-120000") is None
    assert A.lire((tmp_path / A.FICHIER).read_text(encoding="utf-8"))[0]["id"] == "1"


# ─── Textes ──────────────────────────────────────────────────────────────────

def test_resume_et_messages():
    pieces = [A.Piece("salon", "Salon", ("light.a", "light.b"), "sensor.t", None, None), A.Piece()]
    noms = {"light.a": "Plafond", "light.b": "Applique", "sensor.t": "Thermomètre"}
    fr = A.resume(pieces, noms, "fr")
    assert "**Pièce 1 · Salon** : Plafond, Applique ; température : Thermomètre ; humidité : —" in fr
    assert "**Pièce 2 · Pièce 2**" in fr and "light.a" not in fr, "des noms, pas des entity_id"
    assert "**Room 1 · Salon**: Plafond, Applique; temperature: Thermomètre" in A.resume(pieces, noms, "en")
    for langue in ("fr", "en"):
        for action in ("creer", "mettre_a_jour", "ailleurs", "plusieurs", "fichier"):
            texte = messages.assistant_action(langue, action, "Ma tablette", "une raison")
            assert texte and "{" not in texte
        assert "Ma tablette" in messages.assistant_action(langue, "mettre_a_jour", "Ma tablette")
        titre, texte = messages.assistant_resultat(langue, "a_coller", yaml_a_coller="- id: '1'\n", raison="r")
        assert "```yaml\n- id: '1'\n```" in texte and titre.startswith("Tab5")
        for resultat in ("cree", "mis_a_jour", "non_chargee", "rien"):
            assert messages.assistant_resultat(langue, resultat, entite="automation.x", sauvegarde="s")[1]
    assert "Rien n'a changé" in messages.assistant_resultat("fr", "rien")[1]


def test_formulaires_et_traductions():
    """Chaque champ des formulaires de l'assistant a son libellé, et la réparation
    « configurer_pieces » dit la même chose que les options (mêmes étapes, mêmes textes)."""
    flux = (INTEGRATION / "assistant_flux.py").read_text(encoding="utf-8")
    champs_piece = set(re.findall(r'_champ\("(\w+)"', flux))
    for langue in ("fr", "en"):
        t = json.loads((INTEGRATION / "translations" / f"{langue}.json").read_text(encoding="utf-8"))
        options = t["options"]
        reparation = t["issues"][const.ISSUE_ASSISTANT]["fix_flow"]
        for etape in ("pieces", "piece", "recapitulatif"):
            assert reparation["step"][etape] == options["step"][etape], (langue, etape)
        assert reparation["abort"] == options["abort"] and reparation["error"] == options["error"]
        assert set(options["step"]["piece"]["data"]) == champs_piece
        assert set(options["step"]["pieces"]["data"]) == {"pieces"}
        assert set(options["step"]["recapitulatif"]["data"]) == {"mettre_a_jour"}
        assert set(options["abort"]) == set(re.findall(r'async_abort\(reason="(\w+)"\)', flux))
        assert set(options["error"]) == set(re.findall(r'erreurs\[[\w"]+\] = "(\w+)"', flux))
    fr = json.loads((INTEGRATION / "translations" / "fr.json").read_text(encoding="utf-8"))
    assert "pièce" in fr["options"]["step"]["piece"]["title"] and "récapitulatif" in fr["options"]["step"]["recapitulatif"]["title"]
