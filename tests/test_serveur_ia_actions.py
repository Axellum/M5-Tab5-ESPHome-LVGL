# -*- coding: utf-8 -*-
"""Actions du popup « Serveur IA » (ADR-0060) : décharger, réveiller, redémarrer.

La tablette n'envoie qu'un code (événement `esphome.tab5_serveur_ia_action`) ; tout le
reste est dans `packages/tab5_llm.yaml` et `packages/tab5_evenements.yaml`. Aucun serveur ni
Home Assistant ne tourne en CI : ce fichier rend les VRAIS modèles Jinja (bac à sable de
Jinja, comme HA, imitation de tests/test_serveur_ia.py) et rejoue le script pas à pas :

- `sensor.tab5_serveur_ia_actions` (ce qui est offert, actif ou grisé) contre un calcul
  Python écrit à part, sur toutes les combinaisons de réglages ;
- `script.tab5_serveur_ia_action` : n'appelle que les quatre actions permises, jamais un nom
  d'action calculé ; rien sans un code offert ET actif à cet instant (type « Aucun »,
  bouton grisé, code inconnu ou composé) ; noms de modèles, adresse, MAC et cible lus dans
  HA, jamais dans la demande ; corps JSON correct même avec un nom de modèle piégé ; huit
  modèles au plus ; mode single, demandes de trop ignorées en silence, 5 s par action ;
- la liste « Tab5 · serveur IA, redémarrage » : ni les scripts du projet ni les boutons
  d'une tablette ;
- la branche de `tab5_evenements.yaml` : seuls les trois codes passent ;
- aucun device_id en dur.

Ce n'est pas Home Assistant : seules les fonctions de modèle que les packages appellent
sont imitées. Aucune action n'a été envoyée à un vrai serveur quand il a été écrit."""
import functools
import itertools
import json
import os
import re

import pytest
import yaml

from tests.commun import ChargeurSansBalises as _Chargeur, lire as _lire
from tests.test_historique import EtatHA, _env as _env_historique, _rendre
from tests.test_serveur_ia_popup import _Domaines

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PACKAGES = os.path.join(REPO, "HomeAssistant_Config", "packages")
LLM = os.path.join(PACKAGES, "tab5_llm.yaml")
EVENEMENTS = os.path.join(PACKAGES, "tab5_evenements.yaml")
TYPES = ["Ollama", "llama.cpp", "LM Studio"]
CODES = ["decharger", "reveiller", "redemarrer"]
RELEVE = "sensor.tab5_serveur_ia_releve"
ACTIONS = "sensor.tab5_serveur_ia_actions"
MAC = "AA:BB:CC:DD:EE:0F"
# Les seules actions que le script peut appeler (vérifiées dans HA 2026.9 : rest_command
# est le package lui-même, wake_on_lan.send_magic_packet l'intégration Wake on LAN).
PERMISES = {"rest_command.tab5_serveur_ia_envoyer", "wake_on_lan.send_magic_packet",
            "script.turn_on", "button.press"}
APPAREILS = {"tab5": {"model": "tab5-ha-hmi"}, "nas": {"model": "DS920+"}}


@functools.lru_cache(maxsize=None)
def _paquet(chemin=LLM):
    """Lu une fois (lecture seule : ne jamais modifier ce qui est renvoyé)."""
    return yaml.load(_lire(chemin), Loader=_Chargeur)


def _bloc(unique_id):
    for b in _paquet()["template"]:
        for domaine in ("sensor", "select"):
            for e in b.get(domaine, []):
                if e.get("unique_id") == unique_id:
                    return b, e
    raise AssertionError(unique_id)


def _env(etats):
    env = _env_historique(etats, None, APPAREILS)
    env.globals["states"] = _Domaines(env.globals["states"], etats)
    env.tests["match"] = lambda v, motif: re.match(motif, str(v)) is not None  # test match de HA
    env.filters["to_json"] = lambda v: json.dumps(v)                         # filtre to_json de HA
    return env


def _maison(type_ia="Ollama", releve="ok", adresse="http://ia:11434", modeles=("gemma4",), instances=(),
            mac="", cible="Aucun", existe=True, autres=()):
    etats = [EtatHA("input_select.tab5_serveur_ia_type", type_ia),
             EtatHA(RELEVE, releve, adresse=adresse, modeles=list(modeles), instances=list(instances)),
             EtatHA("input_text.tab5_serveur_ia_mac", mac),
             EtatHA("select.tab5_serveur_ia_redemarrage", cible)]
    if existe and re.match(r"(script|button)\.", cible):
        etats.append(EtatHA(cible, "off"))
    etats += list(autres)
    _, capteur = _bloc("tab5_serveur_ia_actions")
    offertes = _rendre(_env(etats), capteur["state"], {})
    return etats + [EtatHA(ACTIONS, offertes)], offertes


# ─── Ce qui est offert, calcul écrit à part ──────────────────────────────────

def _attendu(type_ia, releve, adresse, modeles, instances, mac, cible, existe):
    if type_ia not in TYPES:
        return "aucune"
    out = []
    if type_ia in ("Ollama", "LM Studio") and adresse:
        charges = modeles if type_ia == "Ollama" else instances
        out.append(("" if releve == "ok" and charges else "-") + "decharger")
    if re.fullmatch(r"([0-9A-Fa-f]{2}[:-]){5}[0-9A-Fa-f]{2}", mac.strip()):
        out.append(("-" if releve in ("ok", "chargement") else "") + "reveiller")
    if re.fullmatch(r"(script|button)\.[a-z0-9_]+", cible) and not cible.startswith("script.tab5_") and existe:
        out.append("redemarrer")
    return ",".join(out) or "aucune"


CAS = list(itertools.product(
    ["Aucun", *TYPES],
    ["ok", "chargement", "hors_ligne", "erreur", "aucun"],
    ["http://ia:1", ""],
    [(), ("gemma4", "qwen3:8b")],
    ["", MAC, " aa-bb-cc-dd-ee-0f ", "AA:BB:CC:DD:EE", "unknown"],
    [("Aucun", False), ("script.relancer_ollama", True), ("button.ia_restart", True), ("script.disparu", False),
     ("script.tab5_push_meteo", True), ("script.a,script.b", True)],
))


@pytest.mark.parametrize("type_ia,releve,adresse,charges,mac,cible", CAS)
def test_actions_offertes_contre_calcul_independant(type_ia, releve, adresse, charges, mac, cible):
    cible, existe = cible
    _, offertes = _maison(type_ia, releve, adresse, modeles=charges, instances=charges, mac=mac,
                          cible=cible, existe=existe)
    assert offertes == _attendu(type_ia, releve, adresse, charges, charges, mac, cible, existe)
    # Ce que lit la tablette : seulement les trois codes, « - » devant pour grisé.
    if offertes != "aucune":
        assert all(re.fullmatch(r"-?(decharger|reveiller|redemarrer)", c) for c in offertes.split(","))


def test_llama_cpp_ne_decharge_jamais():
    _, offertes = _maison("llama.cpp", modeles=("m",), mac=MAC, cible="script.x")
    assert "decharger" not in offertes
    assert offertes == "-reveiller,redemarrer"


# ─── Le script, rejoué pas à pas ─────────────────────────────────────────────

def _script():
    return _paquet()["script"]["tab5_serveur_ia_action"]


class Execution:
    """Une exécution de script.tab5_serveur_ia_action : actions appelées (données rendues),
    attentes, événements. Une condition fausse arrête tout, comme dans HA."""

    def __init__(self, etats, code):
        self.env = _env(etats)
        self.appels, self.attente, self.evenements = [], 0, []
        self.complet = self._suite(_script()["sequence"], {"code": code})

    def _vrai(self, modele, ctx):
        return _rendre(self.env, modele, ctx) is True

    def _suite(self, pas_liste, ctx):
        for pas in pas_liste:
            if "variables" in pas:
                for cle, valeur in pas["variables"].items():
                    ctx[cle] = _rendre(self.env, valeur, ctx)
            elif "condition" in pas:
                if not self._vrai(pas["value_template"], ctx):
                    return False
            elif "choose" in pas:
                for option in pas["choose"]:
                    if all(self._vrai(c["value_template"], ctx) for c in option["conditions"]):
                        if not self._suite(option["sequence"], dict(ctx)):
                            return False
                        break
            elif "repeat" in pas:
                for item in _rendre(self.env, pas["repeat"]["for_each"], ctx):
                    if not self._suite(pas["repeat"]["sequence"], dict(ctx, repeat={"item": item})):
                        return False
            elif "if" in pas:
                branche = "then" if all(self._vrai(c["value_template"], ctx) for c in pas["if"]) else "else"
                if not self._suite(pas.get(branche, []), dict(ctx)):
                    return False
            elif "action" in pas:
                self.appels.append((pas["action"], _rendre(self.env, pas.get("data", {}), ctx),
                                    _rendre(self.env, pas.get("target", {}), ctx)))
            elif "delay" in pas:
                self.attente += pas["delay"]["seconds"]
            elif "event" in pas:
                self.evenements.append(pas["event"])
            else:
                raise AssertionError(f"pas inconnu du rejeu : {pas}")
        return True

    def corps_json(self, i):
        """Le payload du rest_command tel que HA l'enverrait (`{{ corps | to_json }}`)."""
        modele = _paquet()["rest_command"]["tab5_serveur_ia_envoyer"]["payload"]
        return json.loads(self.env.from_string(modele).render(corps=self.appels[i][1]["corps"]))


def _actions_du_script(noeud):
    if isinstance(noeud, dict):
        for cle, valeur in noeud.items():
            if cle == "action":
                yield valeur
            yield from _actions_du_script(valeur)
    elif isinstance(noeud, list):
        for v in noeud:
            yield from _actions_du_script(v)


def test_script_n_appelle_que_les_actions_permises():
    actions = list(_actions_du_script(_script()["sequence"]))
    assert set(actions) == PERMISES
    assert all("{" not in a for a in actions), "jamais un nom d'action calculé"


def test_script_une_action_par_cinq_secondes():
    s = _script()
    assert s["mode"] == "single" and s["max_exceeded"] == "silent"
    assert set(s["fields"]) == {"code"}, "la demande ne porte qu'un code"
    assert s["fields"]["code"]["selector"]["select"]["options"] == CODES
    attente = sum(p["delay"]["seconds"] for p in s["sequence"] if "delay" in p)
    assert attente == 5
    # Le relevé repart tout de suite après l'action.
    assert [p["event"] for p in s["sequence"] if "event" in p] == ["tab5_serveur_ia_relever"]
    releve = next(b for b in _paquet()["template"] if "triggers" in b)
    assert {"trigger": "event", "event_type": "tab5_serveur_ia_relever"} in releve["triggers"]


def test_decharger_ollama():
    modeles = ["gemma4", 'piege"},{"x": 1', "qwen3:8b"]
    etats, _ = _maison("Ollama", modeles=modeles)
    e = Execution(etats, "decharger")
    assert e.complet and [a[0] for a in e.appels] == ["rest_command.tab5_serveur_ia_envoyer"] * 3
    for i, nom in enumerate(modeles):
        assert e.appels[i][1]["adresse"] == "http://ia:11434"
        assert e.appels[i][1]["chemin"] == "/api/generate"
        assert e.corps_json(i) == {"model": nom, "keep_alive": 0}, "corps JSON, nom gardé tel quel"
    assert e.attente == 5 and e.evenements == ["tab5_serveur_ia_relever"]


def test_decharger_lm_studio():
    etats, _ = _maison("LM Studio", adresse="http://ia:1234", modeles=("Qwen3 8B",),
                       instances=("qwen/qwen3-8b", "qwen/qwen3-8b:2"))
    e = Execution(etats, "decharger")
    assert [a[1]["chemin"] for a in e.appels] == ["/api/v1/models/unload"] * 2
    assert [e.corps_json(i) for i in range(2)] == [{"instance_id": "qwen/qwen3-8b"},
                                                   {"instance_id": "qwen/qwen3-8b:2"}]


def test_decharger_huit_modeles_au_plus():
    etats, _ = _maison("Ollama", modeles=[f"m{i}" for i in range(12)])
    assert len(Execution(etats, "decharger").appels) == 8


def test_reveiller():
    etats, offertes = _maison("Ollama", releve="hors_ligne", mac=" aa-bb-cc-dd-ee-0f ")
    assert "reveiller" in offertes.split(",")
    e = Execution(etats, "reveiller")
    assert e.appels == [("wake_on_lan.send_magic_packet", {"mac": "aa-bb-cc-dd-ee-0f"}, {})]


@pytest.mark.parametrize("cible,action", [("script.relancer_ollama", "script.turn_on"),
                                          ("button.ia_restart", "button.press")])
def test_redemarrer(cible, action):
    etats, _ = _maison("LM Studio", cible=cible)
    e = Execution(etats, "redemarrer")
    assert e.appels == [(action, {}, {"entity_id": cible})]


@pytest.mark.parametrize("reglages,code", [
    (dict(type_ia="Aucun", mac=MAC, cible="script.x", releve="aucun"), "reveiller"),
    (dict(type_ia="Aucun", cible="script.x"), "redemarrer"),
    (dict(type_ia="Ollama", modeles=()), "decharger"),                 # grisé : rien de chargé
    (dict(type_ia="Ollama", releve="hors_ligne"), "decharger"),        # grisé : serveur éteint
    (dict(type_ia="llama.cpp", modeles=("m",)), "decharger"),          # pas pris en charge
    (dict(type_ia="Ollama", releve="ok", mac=MAC), "reveiller"),       # grisé : déjà en ligne
    (dict(type_ia="Ollama", mac=""), "reveiller"),                     # pas de MAC
    (dict(type_ia="Ollama", cible="script.disparu", existe=False), "redemarrer"),
    (dict(type_ia="Ollama", cible="Aucun"), "redemarrer"),
    (dict(type_ia="Ollama", cible="script.tab5_push_meteo"), "redemarrer"),   # écrit à la main
    (dict(type_ia="Ollama", cible="script.a,script.b"), "redemarrer"),
    (dict(type_ia="Ollama", mac=MAC, releve="hors_ligne", cible="script.x"), "reveiller,redemarrer"),
    (dict(type_ia="Ollama", cible="script.x"), "script.x"),
    (dict(type_ia="Ollama", cible="script.x"), "-redemarrer"),
    (dict(type_ia="Ollama", cible="script.x"), " REDEMARRER"),
    (dict(type_ia="Ollama"), ""),
])
def test_rien_sans_action_offerte_et_active(reglages, code):
    etats, _ = _maison(**reglages)
    e = Execution(etats, code)
    assert not e.complet and e.appels == [] and e.attente == 0


def test_select_redemarrage_ni_projet_ni_tablette():
    autres = [EtatHA("script.relancer_ollama", "off"), EtatHA("script.tab5_push_meteo", "off"),
              EtatHA("button.ia_restart", "unknown", appareil="nas"),
              EtatHA("button.tab5_redemarrer", "unknown", appareil="tab5"),
              EtatHA("light.salon", "on")]
    bloc, select = _bloc("tab5_serveur_ia_redemarrage")
    for memoire, etat in (("", "Aucun"), ("button.ia_restart", "button.ia_restart"),
                          ("script.oublie", "script.oublie"), ("light.salon", "Aucun"),
                          ("script.tab5_push_meteo", "Aucun"), ("script.a,script.b", "Aucun")):
        etats = autres + [EtatHA("input_text.tab5_choix_ia_redemarrage", memoire)]
        env = _env(etats)
        ctx = {k: _rendre(env, v, {}) for k, v in bloc["variables"].items()}
        options = _rendre(env, select["options"], ctx)
        assert options[-1] == "Aucun"
        assert {"script.relancer_ollama", "button.ia_restart"} <= set(options)
        assert not {"script.tab5_push_meteo", "button.tab5_redemarrer", "light.salon"} & set(options)
        assert _rendre(env, select["state"], ctx) == etat
        assert etat in options, "le choix gardé reste dans la liste, même disparu"


# ─── La branche de tab5_evenements.yaml ──────────────────────────────────────

def _branche():
    for bloc in _paquet(EVENEMENTS)["automation"]:
        for pas in bloc["actions"]:
            for option in pas.get("choose", []):
                if option["conditions"] == [{"condition": "trigger", "id": "serveur_ia_action"}]:
                    return bloc, option
    raise AssertionError("branche serveur_ia_action absente")


@pytest.mark.parametrize("demande,passe", [
    ("decharger", True), ("reveiller", True), ("redemarrer", True),
    ("", False), ("Decharger", False), ("decharger,redemarrer", False), ("script.turn_on", False),
    ("rm -rf /", False), (None, False),
])
def test_branche_evenement(demande, passe):
    bloc, option = _branche()
    assert {"trigger": "event", "event_type": "esphome.tab5_serveur_ia_action",
            "id": "serveur_ia_action"} in bloc["triggers"]
    env = _env([EtatHA("script.tab5_serveur_ia_action", "off")])
    donnees = {} if demande is None else {"action": demande}
    ctx = {"trigger": {"event": {"data": donnees}}}
    variables, si = option["sequence"]
    for cle, valeur in variables["variables"].items():
        ctx[cle] = _rendre(env, valeur, ctx)
    assert (_rendre(env, si["if"], ctx) is True) is passe
    assert si["then"] == [{"action": "script.turn_on", "target": {"entity_id": "script.tab5_serveur_ia_action"},
                           "data": {"variables": {"code": "{{ demande }}"}}}]
    # Sans le package tab5_llm.yaml (script absent) : rien.
    assert _rendre(_env([]), si["if"], ctx) is False


def test_aucun_device_id_en_dur():
    def cles(noeud):
        if isinstance(noeud, dict):
            for k, v in noeud.items():
                yield k
                yield from cles(v)
        elif isinstance(noeud, list):
            for v in noeud:
                yield from cles(v)
    assert "device_id" not in set(cles(_paquet())), "aucun device_id en dur (ADR-0024)"
    _, option = _branche()
    assert "device_id" not in set(cles(option))
