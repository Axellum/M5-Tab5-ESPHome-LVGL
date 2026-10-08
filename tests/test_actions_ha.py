# -*- coding: utf-8 -*-
"""Événements seulement : le firmware n'appelle plus aucune action de HA (ADR-0025, 28/09/2026).

Historique : le firmware appelait `assist_satellite.stop` à chaque interruption de la
voix ; cette action n'existe pas dans HA, et chaque appel écrivait « Action
assist_satellite.stop not found » dans le journal de HA sans que rien ne le signale côté
tablette. Surtout, chaque `homeassistant.service` exigeait l'option « Autoriser
l'appareil à effectuer des actions Home Assistant », une étape d'installation de plus
qui ouvrait TOUTES les actions de HA à la tablette.

Depuis l'ADR-0025, la tablette émet des événements `esphome.tab5_*` et
`HomeAssistant_Config/packages/tab5_evenements.yaml` les traduit en une liste blanche
d'actions. Ce fichier vérifie :
- aucun `homeassistant.service` / `homeassistant.action` dans le firmware ;
- chaque événement émis a un consommateur (package ou blueprint), et chaque événement
  `esphome.tab5_*` écouté est émis par le firmware ;
- le package n'appelle que des actions de la liste blanche, jamais un nom d'action
  calculé ; il n'écoute qu'une tablette de modèle `tab5-ha-hmi` ; il ne redémarre HA
  que sur l'événement de confirmation, émis par le seul bouton « Confirmer ».
"""
import re
from pathlib import Path

import yaml
from tests.commun import ChargeurSansBalises as _Chargeur

REPO = Path(__file__).resolve().parent.parent
PACKAGES = REPO / "HomeAssistant_Config" / "packages"
BLUEPRINTS = REPO / "HomeAssistant_Config" / "blueprints"
EVENEMENTS = PACKAGES / "tab5_evenements.yaml"

# Actions que tab5_evenements.yaml peut appeler, présentes dans HA 2026.9 (liste des
# services relevée le 28/09/2026 ; script.turn_on vérifié le même jour). En ajouter une :
# vérifier d'abord qu'elle existe (Outils de développement → Actions), puis l'écrire ici.
ACTIONS_AUTORISEES = {
    "assist_satellite.announce",
    "automation.reload",
    "automation.trigger",
    "homeassistant.restart",
    "media_player.media_stop",
    "script.turn_on",
    "select.select_option",
}

# Événements émis sans consommateur dans le projet, exprès : offerts aux automatisations
# de l'utilisateur (allumer une lampe quand le réveil sonne…).
SANS_CONSOMMATEUR = {
    "esphome.tab5_alarm_start",
    "esphome.tab5_alarm_stop",
    "esphome.tab5_alarm_snooze",
    "esphome.tab5_alarm_timeout",
}

REDEMARRAGE = "esphome.tab5_redemarrage_ha_confirme"

APPEL_ACTION = re.compile(r"homeassistant\.(?:service|action)\s*:")
EMISSION = re.compile(r"homeassistant\.event:\s*\n\s*event:\s*['\"]?(esphome\.[a-z_0-9]+)")


def _fichiers_firmware():
    tab5 = REPO / "Tab5"
    return ([REPO / "tab5-ha-hmi.yaml", REPO / "tab5-rendu-host.yaml"]
            + sorted(tab5.glob("*.yaml")) + sorted((tab5 / "ui_components").glob("*.yaml"))
            + sorted((tab5 / "rendu").glob("*.yaml")))


def _sans_commentaires(texte):
    return "\n".join(ligne.split("#", 1)[0] if ligne.lstrip().startswith("#") else ligne
                     for ligne in texte.splitlines())


def _emissions():
    """{événement: [fichiers]} des `homeassistant.event` du firmware."""
    trouves = {}
    for chemin in _fichiers_firmware():
        for evt in EMISSION.findall(_sans_commentaires(chemin.read_text(encoding="utf-8"))):
            trouves.setdefault(evt, []).append(chemin.relative_to(REPO).as_posix())
    return trouves


def _parcourir(noeud):
    """Chaque dict du document, en profondeur."""
    if isinstance(noeud, dict):
        yield noeud
        for v in noeud.values():
            yield from _parcourir(v)
    elif isinstance(noeud, list):
        for v in noeud:
            yield from _parcourir(v)


def _charger(chemin):
    return yaml.load(chemin.read_text(encoding="utf-8"), Loader=_Chargeur) or {}


def _consommateurs():
    """{événement esphome.tab5_*: [fichiers]} des déclencheurs des packages et blueprints."""
    trouves = {}
    for chemin in sorted(PACKAGES.glob("*.yaml")) + sorted(BLUEPRINTS.rglob("*.yaml")):
        for d in _parcourir(_charger(chemin)):
            evt = d.get("event_type")
            if isinstance(evt, str) and evt.startswith("esphome.tab5_"):
                trouves.setdefault(evt, []).append(chemin.relative_to(REPO).as_posix())
    return trouves


def _automatisation():
    (auto,) = [a for a in _charger(EVENEMENTS)["automation"] if a.get("id") == "tab5_evenements"]
    return auto


# ─── Firmware ────────────────────────────────────────────────────────────────

def test_le_firmware_n_appelle_aucune_action_ha():
    fautifs = []
    for chemin in _fichiers_firmware():
        for n, ligne in enumerate(chemin.read_text(encoding="utf-8").splitlines(), 1):
            if not ligne.lstrip().startswith("#") and APPEL_ACTION.search(ligne):
                fautifs.append(f"{chemin.relative_to(REPO).as_posix()}:{n}")
    assert not fautifs, ("action HA appelée par le firmware (exige l'option « actions HA ») — "
                         "émettre un événement esphome.tab5_* à la place (ADR-0025) : " + ", ".join(fautifs))


def test_plus_aucune_entite_ha_nommee_par_le_firmware():
    """Les entités de la tablette et du bouton « MAJ Écran » sont retrouvées par HA
    (device_entities, id de l'automatisation) : plus de substitution `entity_*`."""
    fautifs = [chemin.relative_to(REPO).as_posix() for chemin in _fichiers_firmware()
               if "${entity_" in _sans_commentaires(chemin.read_text(encoding="utf-8"))]
    assert not fautifs, fautifs
    substitutions = (_charger(REPO / "Tab5" / "tab5-scripts.yaml").get("substitutions") or {})
    assert not [k for k in substitutions if k.startswith("entity_")]


# ─── Émetteurs et consommateurs ──────────────────────────────────────────────

def test_le_firmware_emet_des_evenements():
    # Garde du test lui-même : s'il ne trouvait plus rien, il passerait sans rien vérifier.
    emis = _emissions()
    assert len(emis) >= 15, sorted(emis)
    assert {"esphome.tab5_calendrier_mois", "esphome.tab5_maj_ecran", REDEMARRAGE} <= emis.keys()


def test_chaque_evenement_emis_a_un_consommateur():
    consommes = _consommateurs()
    orphelins = {evt: ou for evt, ou in _emissions().items()
                 if evt not in consommes and evt not in SANS_CONSOMMATEUR}
    assert not orphelins, ("événement émis par le firmware, écouté par aucun package ni "
                           f"blueprint : {orphelins}")


def test_chaque_evenement_ecoute_est_emis():
    emis = _emissions()
    fantomes = {evt: ou for evt, ou in _consommateurs().items() if evt not in emis}
    assert not fantomes, f"événement écouté que le firmware n'émet jamais : {fantomes}"
    # La liste des exceptions ne doit pas vieillir : chacune est encore émise.
    assert SANS_CONSOMMATEUR <= emis.keys()


# ─── Le package tab5_evenements.yaml ─────────────────────────────────────────

def test_le_package_n_ecoute_que_la_tablette():
    conditions = _automatisation()["conditions"]
    assert len(conditions) == 1 and conditions[0]["condition"] == "template"
    modele = conditions[0]["value_template"]
    assert "device_attr(d, 'model') == 'tab5-ha-hmi'" in modele
    assert "trigger.event.data.device_id" in modele and "'.' not in d" in modele


def test_le_package_n_appelle_que_la_liste_blanche():
    auto = _automatisation()
    appels = [d["action"] for d in _parcourir(auto["actions"]) if "action" in d]
    assert appels and not [d for d in _parcourir(auto["actions"]) if "service" in d]
    for action in appels:
        assert "{" not in action, f"nom d'action calculé : {action}"
        assert action in ACTIONS_AUTORISEES, f"{action} : hors de la liste blanche (ADR-0025)"
    # Les scripts lancés existent dans les packages du projet.
    scripts = {f"script.{nom}" for chemin in PACKAGES.glob("*.yaml")
               for nom in (_charger(chemin).get("script") or {})}
    for d in _parcourir(auto["actions"]):
        if d.get("action") == "script.turn_on":
            cible = d["target"]["entity_id"]
            assert cible in scripts, f"{cible} : aucun package ne définit ce script"


def test_une_branche_par_evenement():
    auto = _automatisation()
    declencheurs = {t["event_type"]: t["id"] for t in auto["triggers"]}
    assert all(t["trigger"] == "event" for t in auto["triggers"])
    for evt, ident in declencheurs.items():
        assert evt == "esphome.tab5_" + ident, f"id {ident} : le nom de l'événement sans « esphome.tab5_ »"
    (choix,) = [a["choose"] for a in auto["actions"] if "choose" in a]
    branches = [o["conditions"] for o in choix]
    assert all(len(c) == 1 and c[0]["condition"] == "trigger" for c in branches)
    ids = [c[0]["id"] for c in branches]
    assert sorted(ids) == sorted(declencheurs.values()), "chaque déclencheur a une branche et une seule"


def test_redemarrage_seulement_sur_la_confirmation():
    """homeassistant.restart : une seule branche, celle de l'événement de confirmation,
    lui-même émis une seule fois, par le bouton « Confirmer » de la console."""
    (choix,) = [a["choose"] for a in _automatisation()["actions"] if "choose" in a]
    avec_redemarrage = [o for o in choix
                        if any(d.get("action") == "homeassistant.restart" for d in _parcourir(o["sequence"]))]
    assert len(avec_redemarrage) == 1
    assert avec_redemarrage[0]["conditions"] == [{"condition": "trigger", "id": "redemarrage_ha_confirme"}]
    # Nulle part ailleurs dans les packages.
    for chemin in PACKAGES.glob("*.yaml"):
        if chemin != EVENEMENTS:
            assert "homeassistant.restart" not in _sans_commentaires(chemin.read_text(encoding="utf-8")), chemin.name

    assert _emissions()[REDEMARRAGE] == ["Tab5/ui_components/console_sys.yaml"]
    console = _sans_commentaires((REPO / "Tab5" / "ui_components" / "console_sys.yaml").read_text(encoding="utf-8"))
    apres = console.split("event: " + REDEMARRAGE, 1)[1]
    libelle = re.search(r'text:\s*"([^"]+)"', apres).group(1)
    assert libelle == "Confirmer", f"l'événement de redémarrage doit partir du bouton « Confirmer », pas « {libelle} »"
    # Et ce bouton est dans l'écran de confirmation, pas sur la carte GESTION.
    avant = console.split("event: " + REDEMARRAGE, 1)[0]
    assert avant.rfind("id: overlay_confirm_ha") > avant.rfind("id: btn_restart_ha")
