# -*- coding: utf-8 -*-
"""Supervision d'un serveur LLM local (package `tab5_llm.yaml`, 10/10/2026).

Aucun serveur d'inférence ne tourne en CI. Ce fichier rend les VRAIS modèles Jinja du package
(bac à sable de Jinja, comme HA, imitation de tests/test_historique.py) sur des réponses
simulées d'Ollama (/api/ps), de llama.cpp (/health, /props, /metrics, /slots) et de LM Studio
(/api/v1/models), formes reprises des docs officielles lues le 10/10/2026, et compare à un
calcul Python écrit à part :

- « Aucun » ou adresse vide : aucune requête (les trois `if` sont faux, le déclencheur
  périodique arrêté par la condition), relevé « aucun », capteurs indisponibles ;
- adresse : « http:// » ajouté, chemin retiré ;
- relevé : en ligne, hors ligne (pas de réponse), erreur HTTP, chargement (503), modèles,
  VRAM, requêtes (/metrics, sinon les slots occupés) ;
- vitesse de génération : écart des compteurs entre deux relevés, gardée sans génération,
  oubliée quand les compteurs repartent de zéro ou que le serveur change ;
- capteurs visibles : disponibilité, VRAM en GiB, « aucun » sans modèle.

Ce n'est pas Home Assistant : seules les fonctions de modèle que le package appelle sont
imitées. Vérifié à part le 10/10/2026 : GET /api/ps et /api/version d'un vrai Ollama 0.30.6
(aucun modèle chargé à ce moment-là) ; llama.cpp et LM Studio non testés sur un vrai serveur."""
import os

import pytest
import yaml

from tests.commun import ChargeurSansBalises as _Chargeur, lire as _lire
from tests.test_historique import EtatHA, _env, _rendre

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
PACKAGE = os.path.join(REPO, "HomeAssistant_Config", "packages", "tab5_llm.yaml")
TYPES = ["Ollama", "llama.cpp", "LM Studio"]
RELEVE = "sensor.tab5_serveur_ia_releve"
GIB = 1024 ** 3


def _paquet():
    return yaml.load(_lire(PACKAGE), Loader=_Chargeur)


def _bloc_releve():
    return next(b for b in _paquet()["template"] if "triggers" in b)


def _bloc_visibles():
    return next(b for b in _paquet()["template"] if "triggers" not in b)


# ─── Réponses simulées (formes des docs officielles) ─────────────────────────

def _rep(status, content):
    return {"status": status, "content": content, "headers": {}}


OLLAMA_PS = _rep(200, {"models": [
    {"name": "gemma4", "model": "gemma4", "size": 6591830464, "size_vram": 5333539264,
     "expires_at": "2025-10-17T16:47:07.93355-07:00", "context_length": 4096},
    {"name": "", "model": "qwen3:8b", "size": 6000000000, "size_vram": 6000000000},
]})
OLLAMA_VIDE = _rep(200, {"models": []})       # réponse réelle du 10/10/2026, rien de chargé
METRIQUES = """# HELP llamacpp:prompt_tokens_total Number of prompt tokens processed.
# TYPE llamacpp:prompt_tokens_total counter
llamacpp:prompt_tokens_total 1200
llamacpp:tokens_predicted_total 5400
llamacpp:tokens_predicted_seconds_total 120.5
llamacpp:predicted_tokens_seconds 0
llamacpp:requests_processing 2
llamacpp:requests_deferred 1
"""
LLAMA = {
    "r_sante": _rep(200, {"status": "ok"}),
    "r_props": _rep(200, {"model_path": "../models/Meta-Llama-3.1-8B-Instruct-Q4_K_M.gguf", "total_slots": 4,
                          "default_generation_settings": {"n_ctx": 1024}}),
    "r_metriques": _rep(200, METRIQUES),
    "r_slots": _rep(200, [{"id": 0, "is_processing": True}, {"id": 1, "is_processing": False}]),
}
LMSTUDIO = _rep(200, {"models": [
    {"type": "llm", "key": "qwen/qwen3-8b", "display_name": "Qwen3 8B", "loaded_instances": [{"id": "qwen/qwen3-8b"}]},
    {"type": "llm", "key": "google/gemma-3-4b", "display_name": "", "loaded_instances": [{"id": "x"}]},
    {"type": "embedding", "key": "nomic-embed", "display_name": "Nomic", "loaded_instances": []},
]})


# ─── Calcul attendu, écrit à part ────────────────────────────────────────────

def _attendu(type_ia, adresse_ok, r):
    vide = dict(etat="aucun", raison="", modeles=[], vram=None, memoire=None, tokens=None, secondes=None,
                requetes=None, attente=None, slots=None)
    if type_ia not in TYPES:
        return dict(vide, raison="type Aucun")
    if not adresse_ok:
        return dict(vide, raison="adresse vide")
    if type_ia == "Ollama":
        ps = r.get("r_ps")
        if ps is None:
            return dict(vide, etat="hors_ligne", raison="pas de réponse")
        if ps["status"] != 200:
            return dict(vide, etat="erreur", raison=f"HTTP {ps['status']} sur /api/ps")
        ms = ps["content"]["models"]
        return dict(vide, etat="ok", modeles=[m["name"] or m["model"] for m in ms],
                    vram=sum(m["size_vram"] for m in ms), memoire=sum(m["size"] for m in ms))
    if type_ia == "LM Studio":
        rm = r.get("r_modeles")
        if rm is None:
            return dict(vide, etat="hors_ligne", raison="pas de réponse")
        if rm["status"] != 200:
            return dict(vide, etat="erreur", raison=f"HTTP {rm['status']} sur /api/v1/models (LM Studio 0.4 et plus)")
        return dict(vide, etat="ok", modeles=[m["display_name"] or m["key"] for m in rm["content"]["models"]
                                               if m["loaded_instances"]])
    sante = r.get("r_sante")
    if sante is None:
        return dict(vide, etat="hors_ligne", raison="pas de réponse")
    if sante["status"] not in (200, 503):
        return dict(vide, etat="erreur", raison=f"HTTP {sante['status']} sur /health")
    out = dict(vide, etat="ok" if sante["status"] == 200 else "chargement")
    props = r.get("r_props")
    if props and props["status"] == 200:
        nom = os.path.basename(props["content"]["model_path"].replace("\\", "/"))
        out["modeles"] = [nom[:-5] if nom.lower().endswith(".gguf") else nom]
        out["slots"] = props["content"]["total_slots"]
    met = r.get("r_metriques")
    if met and met["status"] == 200:
        valeurs = dict(l.split()[:2] for l in met["content"].splitlines() if l and not l.startswith("#"))
        out["tokens"] = float(valeurs["llamacpp:tokens_predicted_total"])
        out["secondes"] = float(valeurs["llamacpp:tokens_predicted_seconds_total"])
        out["requetes"] = int(float(valeurs["llamacpp:requests_processing"]))
        out["attente"] = int(float(valeurs["llamacpp:requests_deferred"]))
    sl = r.get("r_slots")
    if out["requetes"] is None and sl and sl["status"] == 200:
        out["requetes"] = sum(1 for s in sl["content"] if s.get("is_processing"))
    return out


# ─── Rendu des modèles du package ────────────────────────────────────────────

class Releve:
    """Un passage du capteur à déclencheurs : variables, `if` des actions, `releve`, attributs."""

    def __init__(self, type_ia, adresse, reponses=None, avant=None, plateforme="time_pattern"):
        etats = [EtatHA("input_select.tab5_serveur_ia_type", type_ia),
                 EtatHA("input_text.tab5_serveur_ia_adresse", adresse)]
        self.env = _env(etats, None)
        bloc = _bloc_releve()
        ctx = {"trigger": {"platform": plateforme}}
        self.passe = _rendre(self.env, bloc["conditions"][0]["value_template"], ctx) is True
        for cle, valeur in bloc["variables"].items():
            ctx[cle] = _rendre(self.env, valeur, ctx)
        self.vars = dict(ctx)
        actions = bloc["actions"]
        self.chemins = []
        for a in actions:
            if "if" in a and _rendre(self.env, a["if"][0]["value_template"], ctx) is True:
                self.chemins += [x["data"]["chemin"] for x in a["then"]]
        ctx.update({k: v for k, v in (reponses or {}).items() if v is not None})
        ctx["releve"] = _rendre(self.env, actions[-1]["variables"]["releve"], ctx)
        if avant is not None:
            ctx["this"] = EtatHA(RELEVE, "ok", **avant)
        capteur = bloc["sensor"][0]
        self.etat = _rendre(self.env, capteur["state"], ctx)
        self.attributs = {k: _rendre(self.env, v, ctx) for k, v in capteur["attributes"].items()}
        self.releve = ctx["releve"]


def test_aucun_ne_lance_aucune_requete():
    for type_ia, adresse in (("Aucun", "http://h:11434"), ("Ollama", ""), ("llama.cpp", "unknown")):
        r = Releve(type_ia, adresse)
        assert not r.passe, "le déclencheur périodique s'arrête à la condition"
        assert r.chemins == [] and r.etat == "aucun"
        assert r.releve == _attendu(type_ia, False, {})
    # Un changement de réglage passe toujours la condition (le relevé dit « aucun »).
    assert Releve("Aucun", "", plateforme="state").passe
    assert Releve("Ollama", "h:1", plateforme="time_pattern").passe


@pytest.mark.parametrize("saisie,adresse", [
    ("192.168.1.5:11434", "http://192.168.1.5:11434"),
    (" http://serveur.local:8080/ ", "http://serveur.local:8080"),
    ("https://ia.maison:1234/v1/models", "https://ia.maison:1234"),
    ("http://", ""), ("", ""),
])
def test_adresse(saisie, adresse):
    assert Releve("Ollama", saisie).vars["adresse"] == adresse


def test_requetes_par_type():
    assert Releve("Ollama", "h:11434").chemins == ["/api/ps"]
    assert Releve("llama.cpp", "h:8080").chemins == ["/health", "/props", "/metrics", "/slots"]
    assert Releve("LM Studio", "h:1234").chemins == ["/api/v1/models"]


@pytest.mark.parametrize("type_ia,reponses", [
    ("Ollama", {"r_ps": OLLAMA_PS}),
    ("Ollama", {"r_ps": OLLAMA_VIDE}),
    ("Ollama", {}),
    ("Ollama", {"r_ps": _rep(404, "404 page not found")}),
    ("llama.cpp", LLAMA),
    ("llama.cpp", dict(LLAMA, r_metriques=_rep(501, {"error": {"code": 501}}))),
    ("llama.cpp", dict(LLAMA, r_sante=_rep(503, {"error": {"code": 503, "message": "Loading model"}}))),
    ("llama.cpp", {}),
    ("llama.cpp", {"r_sante": _rep(401, {"error": "Unauthorized"})}),
    ("LM Studio", {"r_modeles": LMSTUDIO}),
    ("LM Studio", {"r_modeles": _rep(404, {"error": "Unexpected endpoint"})}),
    ("LM Studio", {}),
])
def test_releve_contre_calcul_independant(type_ia, reponses):
    r = Releve(type_ia, "h:1", reponses)
    attendu = _attendu(type_ia, True, reponses)
    assert r.releve == attendu
    assert r.etat == attendu["etat"]
    assert r.attributs["vram_octets"] == attendu["vram"] and r.attributs["modeles"] == attendu["modeles"]


def test_vitesse_de_generation():
    precedent = dict(type="llama.cpp", adresse="http://h:1", tokens_total=5000.0, secondes_total=100.5, debit=31.0)
    # 400 tokens en 20 s depuis le relevé d'avant.
    assert Releve("llama.cpp", "h:1", LLAMA, avant=precedent).attributs["debit"] == 20.0
    # Rien de généré : la vitesse d'avant reste.
    rien = dict(precedent, tokens_total=5400.0, secondes_total=120.5)
    assert Releve("llama.cpp", "h:1", LLAMA, avant=rien).attributs["debit"] == 31.0
    # Compteurs repartis de zéro, autre serveur, premier relevé, pas de /metrics : on attend.
    assert Releve("llama.cpp", "h:1", LLAMA, avant=dict(precedent, tokens_total=9e9)).attributs["debit"] is None
    assert Releve("llama.cpp", "h:2", LLAMA, avant=precedent).attributs["debit"] is None
    assert Releve("llama.cpp", "h:1", LLAMA, avant={}).attributs["debit"] is None
    sans = dict(LLAMA, r_metriques=_rep(501, ""))
    assert Releve("llama.cpp", "h:1", sans, avant=precedent).attributs["debit"] is None


def _visibles(etat, **attributs):
    env = _env([EtatHA(RELEVE, etat, **attributs)], None)
    bloc = _bloc_visibles()
    out = {}
    for domaine in ("binary_sensor", "sensor"):
        for e in bloc[domaine]:
            dispo = _rendre(env, e["availability"], {})
            out[e["default_entity_id"]] = _rendre(env, e["state"], {}) if dispo is True else "unavailable"
    return out


def test_capteurs_visibles():
    ollama = _visibles("ok", modeles=["gemma4", "qwen3:8b"], vram_octets=5333539264 + 6000000000,
                       requetes=None, debit=None)
    assert ollama["binary_sensor.tab5_serveur_ia_en_ligne"] is True
    assert ollama["sensor.tab5_serveur_ia_modele"] == "gemma4, qwen3:8b"
    assert ollama["sensor.tab5_serveur_ia_vram"] == round((5333539264 + 6000000000) / GIB, 2)
    assert ollama["sensor.tab5_serveur_ia_vitesse"] == "unavailable"
    assert ollama["sensor.tab5_serveur_ia_requetes"] == "unavailable"
    llama = _visibles("ok", modeles=[], vram_octets=None, requetes=2, debit=45.3)
    assert llama["sensor.tab5_serveur_ia_modele"] == "aucun" and llama["sensor.tab5_serveur_ia_vram"] == "unavailable"
    assert llama["sensor.tab5_serveur_ia_vitesse"] == 45.3 and llama["sensor.tab5_serveur_ia_requetes"] == 2
    eteint = _visibles("hors_ligne", modeles=[], vram_octets=None)
    assert eteint["binary_sensor.tab5_serveur_ia_en_ligne"] is False
    assert all(v == "unavailable" for k, v in eteint.items() if k.startswith("sensor."))
    assert all(v == "unavailable" for v in _visibles("aucun").values())


def test_capteurs_proposes_au_suivi():
    """La liste « Tab5 · capteurs suivis » ne propose que les capteurs à state_class
    measurement (tab5_reglages.yaml) : les trois mesures en ont un, les textes non."""
    bloc = _bloc_visibles()
    mesures = {e["default_entity_id"] for e in bloc["sensor"] if e.get("state_class") == "measurement"}
    assert mesures == {"sensor.tab5_serveur_ia_vram", "sensor.tab5_serveur_ia_vitesse",
                       "sensor.tab5_serveur_ia_requetes"}
    assert "selectattr('attributes.state_class', 'eq', 'measurement')" in _lire(
        "HomeAssistant_Config", "packages", "tab5_reglages.yaml")


def test_structure():
    """Les options du type sont celles que lisent les modèles ; chaque requête rattrape une
    panne (continue_on_error) ; le relevé n'a pas d'horodatage (une ligne du recorder par
    changement, pas toutes les 30 s) ; aucune adresse écrite dans le package."""
    paquet = _paquet()
    assert paquet["input_select"]["tab5_serveur_ia_type"]["options"] == ["Aucun"] + TYPES
    assert "initial" not in paquet["input_text"]["tab5_serveur_ia_adresse"]
    assert paquet["rest_command"]["tab5_serveur_ia"]["timeout"] <= 7
    bloc = _bloc_releve()
    appels = [x for a in bloc["actions"] if "if" in a for x in a["then"]]
    assert appels and all(x["action"] == "rest_command.tab5_serveur_ia" and x["continue_on_error"] is True
                          and x["response_variable"] for x in appels)
    assert not any("now()" in str(v) for v in bloc["sensor"][0]["attributes"].values())
    code = "\n".join(l for l in _lire(PACKAGE).splitlines() if not l.lstrip().startswith("#"))
    assert "11434" not in code and "192.168." not in code
