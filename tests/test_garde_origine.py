# -*- coding: utf-8 -*-
"""Garde d'origine des événements de la tablette (audit sécurité du 30/09/2026).

Les événements `esphome.*` n'exigent aucune option côté HA (ADR-0025) : n'importe quel
appareil ESPHome de la maison peut émettre `esphome.tab5_action` ou `esphome.tab5_journal`.
Le blueprint « Tab5 — emplacements » les acceptait de tous : un appareil voisin piraté
pilotait toutes les tuiles. `packages/tab5_evenements.yaml` vérifiait déjà le modèle de
l'appareil émetteur (`device_attr(…, 'model') == 'tab5-ha-hmi'`) ; ce fichier exige la
même garde sur CHAQUE automatisation (packages, optionnel, snippets, blueprints)
déclenchée par un événement `esphome.tab5_*`, et vérifie en la rendant, comme HA,
qu'elle :
- refuse un événement d'un autre appareil, sans device_id, ou dont le device_id est une
  entité ;
- accepte celui d'une tablette ;
- laisse passer les autres déclencheurs de la même automatisation (état, heure,
  démarrage de HA…).

Les entités de modèle (`template:` à déclencheurs) peuvent écouter un événement de la
tablette sans garde tant qu'elles ne lisent rien de `trigger` : elles se recalculent à
partir du registre, un événement forgé n'y change rien.
"""
from pathlib import Path

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment
from tests.commun import ChargeurSansBalises as _Chargeur

REPO = Path(__file__).resolve().parent.parent
HA = REPO / "HomeAssistant_Config"
DOSSIERS = ("packages", "optionnel", "snippets", "blueprints")

PREFIXE = "esphome.tab5_"
MODELE = "device_attr(d, 'model') == 'tab5-ha-hmi'"
# Depuis l'audit du 07/10/2026 (HA-7), la garde est une macro partagée : la condition
# l'importe et l'appelle, le test la rend avec le vrai fichier.
MACROS = HA / "custom_templates"
MACRO = "tab5_origine(trigger) == 'oui'"

# Appareils imités : device_attr(x, 'model'). Une entité de la tablette (« sensor.… »)
# a le modèle de son appareil dans HA : la garde doit la refuser quand même.
APPAREILS = {
    "appareil_tab5": "tab5-ha-hmi",
    "appareil_voisin": "esp32-voisin",
    "sensor.tab5_uptime": "tab5-ha-hmi",
}
ACCEPTE = {"device_id": "appareil_tab5"}
REFUSES = (
    {"device_id": "appareil_voisin"},
    {},
    {"device_id": None},
    {"device_id": ""},
    {"device_id": "sensor.tab5_uptime"},
    {"device_id": "inconnu"},
)


def _fichiers():
    for dossier in DOSSIERS:
        yield from sorted((HA / dossier).rglob("*.yaml"))


def _en_liste(valeur):
    if valeur is None:
        return []
    return valeur if isinstance(valeur, list) else [valeur]


def _declencheurs(bloc):
    return [t for t in _en_liste(bloc.get("triggers", bloc.get("trigger"))) if isinstance(t, dict)]


def _types(declencheur):
    return [e for e in _en_liste(declencheur.get("event_type")) if isinstance(e, str)]


def _de_la_tablette(declencheur):
    return any(e.startswith(PREFIXE) for e in _types(declencheur))


def _blocs(noeud, dans_template=False):
    """(bloc, est_un_modèle) pour chaque dict qui porte des déclencheurs."""
    if isinstance(noeud, dict):
        if _declencheurs(noeud):
            yield noeud, dans_template
        for cle, valeur in noeud.items():
            yield from _blocs(valeur, dans_template or cle == "template")
    elif isinstance(noeud, list):
        for valeur in noeud:
            yield from _blocs(valeur, dans_template)


def _ecoutes():
    """[(fichier, bloc, est_un_modèle)] des blocs déclenchés par un événement de la tablette."""
    trouves = []
    for chemin in _fichiers():
        doc = yaml.load(chemin.read_text(encoding="utf-8"), Loader=_Chargeur) or {}
        for bloc, modele in _blocs(doc):
            if any(_de_la_tablette(t) for t in _declencheurs(bloc)):
                trouves.append((chemin.relative_to(REPO).as_posix(), bloc, modele))
    return trouves


def _automatisations():
    return [(f, b) for f, b, modele in _ecoutes() if not modele]


def _nom(fichier, bloc):
    return f"{fichier} ({bloc.get('id') or bloc.get('alias') or 'blueprint'})"


def _gardes(bloc):
    conditions = _en_liste(bloc.get("conditions", bloc.get("condition")))
    return [c["value_template"] for c in conditions
            if isinstance(c, dict) and c.get("condition") == "template"
            and (MODELE in str(c.get("value_template", "")) or MACRO in str(c.get("value_template", "")))]


def _rendre(garde, trigger):
    """La garde rendue comme HA : bac à sable, variable non définie = erreur (un
    déclencheur d'état n'a pas de `trigger.event`), résultat lu comme un booléen."""
    env = ImmutableSandboxedEnvironment(undefined=jinja2.StrictUndefined,
                                        loader=jinja2.FileSystemLoader(str(MACROS)))
    env.globals["device_attr"] = lambda d, nom: APPAREILS.get(d) if nom == "model" else None
    texte = env.from_string(garde).render(trigger=trigger).strip().lower()
    return texte in ("true", "yes", "on", "enable", "1")


def _trigger(declencheur, indice, donnees=None):
    """Variable `trigger` d'un déclenchement : id donné, sinon l'indice (comme HA)."""
    t = {"id": str(declencheur.get("id", indice)),
         "platform": declencheur.get("trigger", declencheur.get("platform")), "idx": str(indice)}
    if t["platform"] == "event":
        t["event"] = {"event_type": _types(declencheur)[0], "data": dict(donnees or {})}
    return t


# ─── Le test lui-même ne doit pas passer à vide ──────────────────────────────

def test_les_automatisations_de_la_tablette_sont_trouvees():
    fichiers = {f for f, _ in _automatisations()}
    assert {
        "HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml",
        "HomeAssistant_Config/packages/tab5_evenements.yaml",
        "HomeAssistant_Config/packages/tab5_health.yaml",
        "HomeAssistant_Config/packages/tab5_push.yaml",
        "HomeAssistant_Config/packages/tab5_reveil.yaml",
    } <= fichiers, sorted(fichiers)


# ─── La garde ────────────────────────────────────────────────────────────────

def _cas():
    return [pytest.param(b, id=_nom(f, b)) for f, b in _automatisations()]


@pytest.mark.parametrize("bloc", _cas())
def test_une_garde_d_origine(bloc):
    gardes = _gardes(bloc)
    assert len(gardes) == 1, ("automatisation déclenchée par un événement esphome.tab5_* sans "
                              f"garde d'origine (condition « {MODELE} », comme tab5_evenements.yaml)")
    garde = gardes[0]
    if MACRO in garde:
        assert "import tab5_origine" in garde, garde
        garde = (MACROS / "tab5_tablette.jinja").read_text(encoding="utf-8")
    assert "trigger.event.data.device_id" in garde and "'.' not in d" in garde, garde


@pytest.mark.parametrize("bloc", _cas())
def test_la_garde_refuse_un_autre_appareil(bloc):
    (garde,) = _gardes(bloc)
    for i, d in enumerate(_declencheurs(bloc)):
        if _de_la_tablette(d):
            assert _rendre(garde, _trigger(d, i, ACCEPTE)), f"{_types(d)} d'une tablette refusé"
            for donnees in REFUSES:
                assert not _rendre(garde, _trigger(d, i, donnees)), f"{_types(d)} accepté avec {donnees}"


@pytest.mark.parametrize("bloc", _cas())
def test_la_garde_laisse_passer_les_autres_declencheurs(bloc):
    (garde,) = _gardes(bloc)
    for i, d in enumerate(_declencheurs(bloc)):
        if not _de_la_tablette(d):
            assert _rendre(garde, _trigger(d, i)), f"déclencheur {d} bloqué par la garde d'origine"


# ─── Les entités de modèle ───────────────────────────────────────────────────

def test_un_modele_qui_ecoute_la_tablette_ne_lit_pas_l_evenement():
    fautifs = [f for f, bloc, modele in _ecoutes() if modele and "trigger" in yaml.safe_dump(
        {k: v for k, v in bloc.items() if k not in ("triggers", "trigger")}, allow_unicode=True)]
    assert not fautifs, ("entité de modèle déclenchée par un événement esphome.tab5_* qui lit "
                         f"`trigger` : ajouter la garde d'origine ({fautifs})")
