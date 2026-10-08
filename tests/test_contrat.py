# -*- coding: utf-8 -*-
"""Contrat Home Assistant ↔ tablette, phase 1 (audit du 30/09/2026, lot D) : les deux
moitiés du contrat comparées champ par champ, sans compiler ni lancer HA.

Actions (HA → tablette). Home Assistant enregistre chaque action `api: services:` du
firmware avec un schéma voluptuous : une variable déclarée est obligatoire, une clé en
trop est refusée (pas d'`extra`), et l'appel échoue (« Action … not found » pour un nom
inconnu). L'erreur ne se voit que dans le journal de HA, et elle arrête le script : les
poussées suivantes ne partent pas. Ce fichier vérifie que chaque appel d'une action de
la tablette passe exactement les variables déclarées dans Tab5/tab5-api-logic.yaml :
- packages, `optionnel/`, blueprint et snippets de HomeAssistant_Config/ ;
- le plan du rendu hors tablette (tools/rendu/ecrans.py ; actions `rendu_*` :
  Tab5/rendu/bouchons.yaml).
La démo (tools/demo/) est vérifiée par tests/test_demo.py et par `--dry-run`, qui lit le
même contrat sans PyYAML ; sa lecture est comparée ici à celle de PyYAML.

Événements (tablette → HA). Chaque champ `trigger.event.data.<x>` lu par un package ou
le blueprint pour un événement `esphome.tab5_*` doit être émis par le firmware pour CET
événement (champs `data:` des `homeassistant.event`). Un champ lu et jamais émis vaut
toujours vide, sans erreur. La lecture est attribuée aux déclencheurs possibles à cet
endroit : ceux de l'automatisation, restreints par une condition `trigger` ou un modèle
`{{ trigger.id == '…' }}` / `{{ trigger.id in [...] }}` englobant. Un champ émis et lu
nulle part doit figurer dans CHAMPS_NON_LUS, avec sa raison.

Les noms des événements eux-mêmes (émis ↔ écoutés) : tests/test_actions_ha.py. Les
comptes et tables de la documentation : tests/test_doc_comptes.py."""
import re
from pathlib import Path

import yaml

from tests.test_actions_ha import _Chargeur, _fichiers_firmware, _parcourir
from tests.commun import fichiers_du_depot

REPO = Path(__file__).resolve().parent.parent
HA = REPO / "HomeAssistant_Config"
API_LOGIC = REPO / "Tab5" / "tab5-api-logic.yaml"
BOUCHONS = REPO / "Tab5" / "rendu" / "bouchons.yaml"

import demo_pusher  # noqa: E402
import ecrans  # noqa: E402

# Champs émis par le firmware qu'aucun fichier du projet ne lit, exprès.
CHAMPS_NON_LUS = {
    # Liste des zones que la tablette suit, envoyée avec la demande : le blueprint garde
    # la sienne (`cles_zones`, comparée à celle de la tablette par tests/test_zones.py).
    ("esphome.tab5_zones", "zones"),
}
# Ajouté par l'intégration ESPHome de HA à chaque événement d'un appareil.
CHAMPS_DE_HA = {"device_id"}

# « esphome.<appareil>_<action> » : `{{ tablette }}` dans le blueprint, le nom de
# l'appareil (tab5_ha_hmi) dans les packages. Les actions de la tablette commencent
# toutes par tab5_maj_ ou tab5_assist_.
APPEL = re.compile(r"esphome\.(?:\{\{\s*\w+\s*\}\}|[a-z0-9_]+?)_(tab5_(?:maj|assist)_\w+)")
LECTURE = re.compile(r"trigger\.event\.data(?:\.(\w+)|\[\s*['\"](\w+)['\"]\s*\])")
ID_UNIQUE = re.compile(r"\{\{\s*trigger\.id\s*==\s*['\"](\w+)['\"]\s*\}\}")
ID_LISTE = re.compile(r"\{\{\s*trigger\.id\s+in\s+\[([^\]]*)\]\s*\}\}")


class _ChargeurFirmware(_Chargeur):
    """Comme _Chargeur (étiquettes ESPHome ignorées), plus `<<: !include …` sauté : le
    fichier inclus n'est pas lu, et PyYAML refuse une fusion qui n'est pas un dict."""

    def flatten_mapping(self, noeud):
        noeud.value = [(k, v) for k, v in noeud.value
                       if not (k.tag == "tag:yaml.org,2002:merge" and isinstance(v, yaml.ScalarNode))]
        super().flatten_mapping(noeud)


def _charger(chemin):
    return yaml.load(chemin.read_text(encoding="utf-8"), Loader=_ChargeurFirmware)


def _relatif(chemin):
    return chemin.relative_to(REPO).as_posix()


# ─── Le contrat du firmware ──────────────────────────────────────────────────

def _actions(chemin):
    """{action: (variables…)} d'un bloc `api: services:`, dans l'ordre du fichier."""
    return {s["service"]: tuple(s.get("variables") or {}) for s in _charger(chemin)["api"]["services"]}


def contrat():
    """Actions de la tablette (tab5_*) et du rendu (rendu_*)."""
    return {**_actions(API_LOGIC), **_actions(BOUCHONS)}


def test_le_contrat_est_lu():
    # Garde du test lui-même : s'il ne trouvait plus rien, il passerait sans rien vérifier.
    actions = _actions(API_LOGIC)
    assert len(actions) > 15 and actions["tab5_maj_info_texte"] == ("texte", "couleur", "meteo_id")


def test_lecture_de_la_demo_egale_a_celle_de_pyyaml():
    """--dry-run lit le contrat ligne à ligne (aucune dépendance) : même résultat."""
    assert demo_pusher.lire_contrat(API_LOGIC) == _actions(API_LOGIC)
    assert demo_pusher.lire_contrat(BOUCHONS) == _actions(BOUCHONS)


# ─── Les appels de HA ────────────────────────────────────────────────────────

def _fichiers_ha():
    # Sans les fichiers ignorés par git (rendered/, placeholders.yaml) : en local comme en CI.
    return [p for p in fichiers_du_depot(HA, "*.yaml") if p.name != "placeholders.example.yaml"]


def appels_ha():
    """[(fichier, action, clés de data)] de chaque appel d'une action de la tablette."""
    appels = []
    for chemin in _fichiers_ha():
        for d in _parcourir(_charger(chemin)):
            nom = d.get("action", d.get("service"))
            if not (isinstance(nom, str) and nom.startswith("esphome.")):
                continue
            m = APPEL.fullmatch(nom)
            assert m, f"{_relatif(chemin)} : « {nom} », forme d'appel inconnue de ce test"
            data = d.get("data") or {}
            assert isinstance(data, dict), f"{_relatif(chemin)} : {nom}, `data:` doit être un dictionnaire"
            appels.append((_relatif(chemin), m.group(1), tuple(data)))
    return appels


def test_les_appels_de_ha_sont_trouves():
    """Garde : la lecture YAML trouve autant d'appels que le texte en montre."""
    appels = appels_ha()
    fichiers = {f for f, _, _ in appels}
    lignes = sum(len(re.findall(r"^\s*(?:- )?(?:action|service):\s*[\"']?esphome\.",
                                chemin.read_text(encoding="utf-8"), re.M))
                 for chemin in _fichiers_ha())
    # Un appel posé sous une ancre (`- &maj_clim` du blueprint, HA-8) puis repris par
    # son alias (`- *maj_clim`) compte une fois de plus.
    for chemin in _fichiers_ha():
        texte = chemin.read_text(encoding="utf-8")
        ancres = set(re.findall(r"^\s*- &(\w+)\s*\n\s*(?:action|service):\s*[\"']?esphome\.", texte, re.M))
        lignes += sum(1 for a in re.findall(r"^\s*- \*(\w+)\s*$", texte, re.M) if a in ancres)
    assert len(appels) == lignes and len(appels) > 20, (len(appels), lignes)
    assert {"HomeAssistant_Config/packages/tab5_push.yaml",
            "HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml",
            "HomeAssistant_Config/snippets/tab5_assist_reponse_exemple.yaml"} <= fichiers


def test_chaque_appel_de_ha_passe_exactement_les_variables():
    actions = contrat()
    ecarts = []
    for fichier, nom, cles in appels_ha():
        if nom not in actions:
            ecarts.append(f"{fichier} : {nom} n'existe pas dans le firmware (« Action … not found »)")
        elif ecart := demo_pusher.ecart_de_contrat(actions[nom], cles):
            ecarts.append(f"{fichier} : {nom}, {ecart} (variables : {list(actions[nom])})")
    assert not ecarts, "HA refuserait ces appels :\n" + "\n".join(ecarts)


# ─── Le rendu hors tablette ──────────────────────────────────────────────────

def test_chaque_appel_du_rendu_passe_exactement_les_variables():
    actions = contrat()
    vus = 0
    for ecran in ecrans.ECRANS:
        for etape in ecran.etapes + ecran.fermer:
            if isinstance(etape, ecrans.Service):
                vus += 1
                assert etape.nom in actions, (ecran.nom, etape.nom)
                cles = [cle for cle, _ in etape.donnees]
                assert len(cles) == len(set(cles)), (ecran.nom, etape.nom, "clé répétée")
                ecart = demo_pusher.ecart_de_contrat(actions[etape.nom], cles)
                assert ecart is None, f"{ecran.nom} : {etape.nom}, {ecart}"
    assert vus > 5


# ─── Les événements ──────────────────────────────────────────────────────────

def champs_emis():
    """{événement: champs} des `homeassistant.event` du firmware. Un même événement émis
    à plusieurs endroits porte partout les mêmes champs."""
    emis, ou = {}, {}
    for chemin in _fichiers_firmware():
        for d in _parcourir(_charger(chemin)):
            evt = d.get("homeassistant.event")
            if not isinstance(evt, dict):
                continue
            nom = evt["event"]
            assert "variables" not in evt, f"{_relatif(chemin)} : {nom}, champs en `variables:` non lus ici"
            champs = frozenset(evt.get("data") or {}) | frozenset(evt.get("data_template") or {})
            if nom in emis:
                assert emis[nom] == champs, (f"{nom} : {sorted(champs)} dans {_relatif(chemin)}, "
                                             f"{sorted(emis[nom])} dans {ou[nom]}")
            emis[nom], ou[nom] = champs, _relatif(chemin)
    return emis


def _cle(declencheur, rang):
    """Un déclencheur sans id a pour id son rang (comme dans HA)."""
    return str(declencheur.get("id", rang))


def _ids_restreints(conditions):
    """Ids des déclencheurs auxquels ces conditions (liées par ET) limitent l'exécution,
    ou None si elles ne disent rien du déclencheur."""
    if isinstance(conditions, str):
        if m := ID_UNIQUE.fullmatch(conditions.strip()):
            return {m.group(1)}
        if m := ID_LISTE.fullmatch(conditions.strip()):
            return set(re.findall(r"['\"](\w+)['\"]", m.group(1)))
        return None
    if isinstance(conditions, dict):
        if conditions.get("condition") == "trigger":
            ids = conditions["id"]
            return {str(i) for i in ids} if isinstance(ids, list) else {str(ids)}
        if conditions.get("condition") == "template":
            return _ids_restreints(conditions.get("value_template"))
        return None
    if isinstance(conditions, list):
        restreints = None
        for c in conditions:
            if (ids := _ids_restreints(c)) is not None:
                restreints = ids if restreints is None else restreints & ids
        return restreints
    return None


def _lectures(noeud, possibles, sortie):
    """(champ, ids des déclencheurs possibles) de chaque lecture de trigger.event.data."""
    if isinstance(noeud, str):
        for a, b in LECTURE.findall(noeud):
            sortie.append((a or b, possibles))
    elif isinstance(noeud, list):
        for v in noeud:
            _lectures(v, possibles, sortie)
    elif isinstance(noeud, dict):
        # Option d'un choose (conditions + sequence) ou if/then : la branche ne s'exécute
        # que pour les déclencheurs admis ; le else, pour les autres.
        garde = noeud.get("conditions", noeud.get("if"))
        ids = _ids_restreints(garde) if garde is not None else None
        dedans = possibles if ids is None else possibles & ids
        for cle, v in noeud.items():
            _lectures(v, possibles if cle == "else" else dedans, sortie)


def _automatisations(noeud):
    """Chaque dict qui a une liste de déclencheurs : automatisation, blueprint, entité de
    modèle à déclencheurs."""
    if isinstance(noeud, dict):
        declencheurs = noeud.get("triggers", noeud.get("trigger"))
        if isinstance(declencheurs, list):
            yield noeud, declencheurs
            return
        for v in noeud.values():
            yield from _automatisations(v)
    elif isinstance(noeud, list):
        for v in noeud:
            yield from _automatisations(v)


def champs_lus():
    """{événement esphome.tab5_*: {champ: [fichiers]}} lus par les fichiers de HA."""
    lus = {}
    for chemin in _fichiers_ha():
        for auto, declencheurs in _automatisations(_charger(chemin)):
            evenements = {_cle(t, i): t.get("event_type") for i, t in enumerate(declencheurs)
                          if (t.get("trigger") or t.get("platform")) == "event"}
            sortie = []
            _lectures({k: v for k, v in auto.items() if k not in ("triggers", "trigger")},
                      {_cle(t, i) for i, t in enumerate(declencheurs)}, sortie)
            for champ, possibles in sortie:
                for cle in possibles:
                    evt = evenements.get(cle)
                    if isinstance(evt, str) and evt.startswith("esphome.tab5_"):
                        lus.setdefault(evt, {}).setdefault(champ, []).append(_relatif(chemin))
    return lus


def test_les_champs_sont_trouves():
    emis, lus = champs_emis(), champs_lus()
    assert emis["esphome.tab5_action"] == {"emplacement", "action", "valeur"}
    assert emis["esphome.tab5_connected"] == frozenset()
    assert {"emplacement", "action", "valeur"} <= lus["esphome.tab5_action"].keys()
    assert sum(len(c) for c in lus.values()) >= 12, lus


def test_chaque_champ_lu_est_emis():
    emis = champs_emis()
    fantomes = []
    for evt, champs in sorted(champs_lus().items()):
        for champ, fichiers in sorted(champs.items()):
            if champ in CHAMPS_DE_HA:
                continue
            if evt not in emis:
                fantomes.append(f"{evt} (jamais émis) : {champ}, lu par {sorted(set(fichiers))}")
            elif champ not in emis[evt]:
                fantomes.append(f"{evt} : « {champ} » lu par {sorted(set(fichiers))}, "
                                f"le firmware émet {sorted(emis[evt])}")
    assert not fantomes, "champ lu mais jamais émis (vaut toujours vide) :\n" + "\n".join(fantomes)


def test_chaque_champ_emis_est_lu():
    lus = champs_lus()
    orphelins = sorted((evt, champ) for evt, champs in champs_emis().items() for champ in champs
                       if champ not in lus.get(evt, {}) and (evt, champ) not in CHAMPS_NON_LUS)
    assert not orphelins, ("champ émis par le firmware que rien ne lit : le lire, ou l'ajouter à "
                           f"CHAMPS_NON_LUS avec sa raison : {orphelins}")
    # La liste des exceptions ne vieillit pas : chacune est encore émise, et toujours pas lue.
    emis = champs_emis()
    for evt, champ in CHAMPS_NON_LUS:
        assert champ in emis.get(evt, ()), (evt, champ, "n'est plus émis : le retirer de CHAMPS_NON_LUS")
        assert champ not in lus.get(evt, {}), (evt, champ, "est lu maintenant : le retirer de CHAMPS_NON_LUS")
