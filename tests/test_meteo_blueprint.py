# -*- coding: utf-8 -*-
"""Météo choisie dans le blueprint « Tab5 — emplacements » (discussion #278, 03/10/2026 :
« I think user can be able to choose forecast easily via blueprint implementation like
room entities »).

La section « Météo » du blueprint (météo des prévisions, source de la pluie dans l'heure,
source des vigilances) ne remplace pas les listes « Tab5 · … » de
packages/tab5_meteo_sources.yaml : remplie, elle ÉCRIT son choix dans ces listes, qui
restent la seule source lue par les capteurs et les poussées. Ce fichier le prouve en
rendant les VRAIS modèles (même imitation du Jinja de HA que test_tuiles_blueprint.py et
test_meteo_sans_meteo_france.py) :

- champ vide : rien n'est écrit, à aucun déclencheur, et le passage s'arrête aux
  conditions ; la poussée lit le choix de la liste, comme avant ;
- champ rempli : à l'enregistrement et au démarrage de HA, le blueprint écrit son choix
  dans la liste ; la chaîne du package (sources météo → Tab5 Météo → poussée complète,
  pluie, vigilances) suit ce choix sans autre réglage ;
- une liste changée à la main revient au choix du blueprint (avec une notification) ;
  changée par une automatisation, elle n'est pas reprise : jamais de ping-pong.

Le job « Installation dans un HA neuf » refait le parcours dans un vrai Home Assistant
(verifier_meteo_du_blueprint dans tools/installation_ha/verifier_installation.py)."""
import yaml

from tests.test_meteo_sans_meteo_france import Etat as EtatPaquet
from tests.test_meteo_sans_meteo_france import Etats as EtatsPaquet
from tests.test_meteo_sans_meteo_france import _environnement as _env_paquet
from tests.test_meteo_sans_meteo_france import _paquet, _parcourir, _poussee
from tests.test_meteo_sans_meteo_france import _rendre as _rendre_paquet
from tests.test_tuiles_blueprint import Etat, Passage, _blueprint, _chercher, _entrees, _evenement, _maison

# Deux météos inventées : la liste est réglée sur la première, le blueprint choisit la seconde.
VILLE = "weather.ville_test"
AUTRE = "weather.autre_test"
LISTE_METEO = "input_text.tab5_meteo_previsions"
LISTE_PLUIE = "input_select.tab5_source_pluie"
LISTE_VIGILANCE = "input_select.tab5_source_vigilance"


def _options(nom):
    return _paquet("tab5_meteo_sources.yaml")["input_select"][nom]["options"]


class _Contexte:
    """Contexte d'un état de HA : user_id pour une personne (interface, API), parent_id
    pour une automatisation ou un script lancé par elle."""

    def __init__(self, user_id=None, parent_id=None):
        self.user_id = user_id
        self.parent_id = parent_id
        self.id = "ctx"


def _etat(entite, etat, contexte=None, **attributs):
    e = Etat(entite, etat, **attributs)
    e.context = contexte or _Contexte()
    return e


def _maison_meteo(meteo=VILLE, pluie="Météo-France", vigilance="Météo-France"):
    """La maison de test_tuiles_blueprint.py, deux météos et les listes du package."""
    return _maison() + [
        _etat(VILLE, "sunny", friendly_name="Ville", temperature=21.0, humidity=40, supported_features=3),
        _etat(AUTRE, "rainy", friendly_name="Autre", temperature=12.5, humidity=88, supported_features=1),
        _etat(LISTE_METEO, meteo),
        _etat(LISTE_PLUIE, pluie, options=_options("tab5_source_pluie")),
        _etat(LISTE_VIGILANCE, vigilance, options=_options("tab5_source_vigilance")),
    ]


def _liste_changee(entite, avant, apres, par_une_personne):
    contexte = _Contexte(user_id="u1") if par_une_personne else _Contexte(parent_id="p1")
    return {"id": "meteo_liste", "platform": "state", "entity_id": entite,
            "from_state": _etat(entite, avant), "to_state": _etat(entite, apres, contexte)}


RECHARGEMENT = {"id": "meteo_rechargement", "platform": "event", "event": {"data": {}}}
DEMARRAGE = {"id": "meteo_demarrage", "platform": "homeassistant", "event": "start"}
REMPLI = {"meteo_previsions": AUTRE, "meteo_pluie": "Open-Meteo", "meteo_vigilance": "Aucune"}


def _branche(p):
    """Alias de la branche du choose principal que prend ce passage (None : default)."""
    choose = _chercher(p.corps["actions"], lambda d: any(
        (b.get("alias") or "").startswith("Météo choisie dans le blueprint") for b in d.get("choose", [])))
    for b in choose["choose"]:
        if p.modele(b["conditions"]):
            return b["alias"]
    return None


def _ecrire(p):
    """Ce que ferait la branche météo : chaque écriture appliquée à l'état de la liste."""
    for e in p["meteo_a_ecrire"]:
        p.etats.d[e["entite"]].state = e["valeur"]
    return p.etats


def _chaine(etats_bp):
    """La chaîne du package rendue sur ces états : « Tab5 · sources météo », « Tab5 Météo »,
    la source effective de la pluie et celle des vigilances, puis les variables de la
    poussée complète (packages/tab5_push.yaml)."""
    etats = EtatsPaquet([EtatPaquet(e.entity_id, e.state, **e.attributes) for e in etats_bp.d.values()
                         if e.entity_id.split(".")[0] in ("weather", "input_text", "input_select")])
    env = _env_paquet(etats)
    blocs = _paquet("tab5_meteo_sources.yaml")["template"]

    def bloc_de(unique_id):
        return next(b for b in blocs if any(c.get("unique_id") == unique_id for c in b.get("sensor", [])))

    bloc = bloc_de("tab5_sources_meteo")
    variables = {}
    for nom, modele in bloc["variables"].items():
        variables[nom] = _rendre_paquet(env, modele, variables)
    capteur = bloc["sensor"][0]
    etats.ajouter(EtatPaquet("sensor.tab5_sources_meteo", _rendre_paquet(env, capteur["state"], variables),
                             **_rendre_paquet(env, capteur["attributes"], variables)))
    capteur = next(c for c in bloc_de("tab5_meteo")["sensor"] if c.get("unique_id") == "tab5_meteo")
    etats.ajouter(EtatPaquet("sensor.tab5_meteo", _rendre_paquet(env, capteur["state"]),
                             **_rendre_paquet(env, capteur["attributes"])))
    pluie = _rendre_paquet(env, bloc_de("tab5_pluie_dans_l_heure")["variables"]["source"],
                           {"mf_pluie": etats.attr("sensor.tab5_sources_meteo", "mf_pluie") or ""})
    vigilance = next(c for c in bloc_de("tab5_vigilance")["sensor"] if c.get("unique_id") == "tab5_vigilance")
    vigilance = _rendre_paquet(env, vigilance["attributes"]["source"])
    auto, contexte = _poussee(etats, env)
    cibles = [_rendre_paquet(env, n["target"]["entity_id"], contexte) for n in _parcourir(auto["action"])
              if n.get("action") == "weather.get_forecasts"]
    actuelle = next(n for n in _parcourir(_paquet("tab5_push.yaml"))
                    if n.get("action") == "esphome.tab5_ha_hmi_tab5_maj_meteo_actuelle")
    return {"meteo": contexte["meteo"], "cibles": cibles, "actuelle": _rendre_paquet(env, actuelle["data"]),
            "pluie": pluie, "vigilance": vigilance}


# ─────────────────────────────────────────────────────────────────────────────
# Les entrées
# ─────────────────────────────────────────────────────────────────────────────

def test_section_meteo_et_options_egales_aux_listes_du_package():
    bp = _blueprint()
    section = bp["blueprint"]["input"]["meteo"]
    assert set(section["input"]) == {"meteo_previsions", "meteo_pluie", "meteo_vigilance"}
    # « français · english », et la description en deux paragraphes, comme les autres.
    assert " · " in section["name"] and "\n\nOptional." in section["description"]
    previsions = section["input"]["meteo_previsions"]
    assert previsions["default"] == []
    assert previsions["selector"]["entity"]["filter"] == [{"domain": "weather"}]
    for entree, liste in (("meteo_pluie", "tab5_source_pluie"), ("meteo_vigilance", "tab5_source_vigilance")):
        e = section["input"][entree]
        assert " · " in e["name"], entree
        options = e["selector"]["select"]["options"]
        assert [o["value"] for o in options] == ["liste"] + _options(liste), entree
        assert e["default"] == "liste", entree
        # « Celle de la liste » nomme la liste du package qu'elle laisse décider.
        assert _paquet("tab5_meteo_sources.yaml")["input_select"][liste]["name"].split(" · ")[1] in options[0]["label"]
    assert set(section["input"]) <= set(_entrees(bp))


def test_declencheurs_de_la_meteo():
    bp = _blueprint()
    meteo = [t for t in bp["triggers"] if t["id"].startswith("meteo_")]
    assert [t["id"] for t in meteo] == bp["variables"]["meteo_declencheurs"]
    par_id = {t["id"]: t for t in meteo}
    assert par_id["meteo_rechargement"] == {"trigger": "event", "event_type": "automation_reloaded",
                                            "id": "meteo_rechargement"}
    assert par_id["meteo_demarrage"] == {"trigger": "homeassistant", "event": "start", "id": "meteo_demarrage"}
    # L'état seulement (`to: ~`), des trois listes que lisent les capteurs du package.
    assert par_id["meteo_liste"]["to"] is None
    assert par_id["meteo_liste"]["entity_id"] == [LISTE_METEO, LISTE_PLUIE, LISTE_VIGILANCE]
    lues = yaml.safe_dump(_paquet("tab5_meteo_sources.yaml"))
    for liste in par_id["meteo_liste"]["entity_id"]:
        assert liste in lues, liste


# ─────────────────────────────────────────────────────────────────────────────
# Champ vide : comportement inchangé
# ─────────────────────────────────────────────────────────────────────────────

def test_champ_vide_rien_n_est_ecrit():
    for declencheur in (RECHARGEMENT, DEMARRAGE, _liste_changee(LISTE_METEO, VILLE, AUTRE, True),
                        _liste_changee(LISTE_PLUIE, "Météo-France", "DWD", True)):
        p = Passage({}, _maison_meteo(), declencheur)
        assert p["meteo_a_ecrire"] == [], declencheur["id"]
        assert not p.conditions(), f"{declencheur['id']} : le passage devait s'arrêter aux conditions"
    # « liste » explicite = vide.
    p = Passage({"meteo_pluie": "liste", "meteo_vigilance": "liste", "meteo_previsions": []},
                _maison_meteo(), RECHARGEMENT)
    assert p["meteo_a_ecrire"] == []


def test_champ_vide_la_poussee_lit_la_liste():
    p = Passage({}, _maison_meteo(meteo=VILLE, pluie="DWD", vigilance="CAP Alerts"), RECHARGEMENT)
    chaine = _chaine(_ecrire(p))
    assert chaine["meteo"] == VILLE
    assert chaine["cibles"] and set(chaine["cibles"]) == {VILLE}
    assert chaine["actuelle"]["condition"] == "sunny"
    assert (chaine["pluie"], chaine["vigilance"]) == ("DWD", "CAP Alerts")


def test_les_autres_declencheurs_n_ecrivent_jamais():
    """Connexion, mesures, demande des zones : la météo n'y est pas écrite, même remplie."""
    for declencheur in (_evenement("connexion"), {"id": "mesures", "platform": "time_pattern"},
                        {"id": "demarrage_ha", "platform": "homeassistant", "event": "start"}):
        p = Passage(REMPLI, _maison_meteo(), declencheur)
        assert p["meteo_a_ecrire"] == [], declencheur["id"]
        assert _branche(p) != "Météo choisie dans le blueprint : l'écrire dans les listes « Tab5 · … »"


# ─────────────────────────────────────────────────────────────────────────────
# Champ rempli : écrit dans la liste, et la poussée suit
# ─────────────────────────────────────────────────────────────────────────────

def test_champ_rempli_ecrit_a_l_enregistrement_et_au_demarrage():
    for declencheur in (RECHARGEMENT, DEMARRAGE):
        p = Passage(REMPLI, _maison_meteo(), declencheur)
        assert p["meteo_a_ecrire"] == [{"entite": LISTE_METEO, "valeur": AUTRE},
                                       {"entite": LISTE_PLUIE, "valeur": "Open-Meteo"},
                                       {"entite": LISTE_VIGILANCE, "valeur": "Aucune"}], declencheur["id"]
        assert p.conditions(), declencheur["id"]
        assert _branche(p) == "Météo choisie dans le blueprint : l'écrire dans les listes « Tab5 · … »"


def test_champ_rempli_meme_sans_tablette_connectee():
    """Les listes suivent le blueprint même tablette éteinte (les autres déclencheurs de HA
    s'arrêtent alors à la garde « tablette connectée »)."""
    from tests.test_tuiles_blueprint import _tablette

    hors_ligne = _tablette("3.2.0 (ESPHome 2026.9.0)", connectee=False)
    assert Passage(REMPLI, _maison_meteo(), RECHARGEMENT, hors_ligne).conditions()
    assert not Passage(REMPLI, _maison_meteo(), {"id": "rechargement", "platform": "event",
                                                 "event": {"data": {}}}, hors_ligne).conditions()


def test_champ_rempli_la_poussee_suit_le_choix():
    avant = _chaine(Passage({}, _maison_meteo(), RECHARGEMENT).etats)
    assert avant["meteo"] == VILLE
    p = Passage(REMPLI, _maison_meteo(), RECHARGEMENT)
    chaine = _chaine(_ecrire(p))
    assert chaine["meteo"] == AUTRE
    assert chaine["cibles"] and set(chaine["cibles"]) == {AUTRE}
    assert chaine["actuelle"] == {"condition": "rainy", "temperature": 12.5, "humidite": 88.0}
    assert (chaine["pluie"], chaine["vigilance"]) == ("Open-Meteo", "Aucune")


def test_deja_a_jour_rien_a_ecrire():
    p = Passage(REMPLI, _maison_meteo(meteo=AUTRE, pluie="Open-Meteo", vigilance="Aucune"), RECHARGEMENT)
    assert p["meteo_a_ecrire"] == []
    assert not p.conditions()


def test_seulement_les_champs_remplis():
    p = Passage({"meteo_vigilance": "DWD"}, _maison_meteo(), DEMARRAGE)
    assert p["meteo_a_ecrire"] == [{"entite": LISTE_VIGILANCE, "valeur": "DWD"}]


def test_meteo_absente_ou_package_absent_rien_d_ecrit():
    # Météo choisie qui n'existe pas (encore) : la liste et son repli restent.
    p = Passage({"meteo_previsions": "weather.disparue"}, _maison_meteo(), RECHARGEMENT)
    assert p["meteo_a_ecrire"] == []
    # Sans le package : aucune liste, rien à écrire (et pas d'action vers une entité absente).
    sans = [e for e in _maison_meteo() if not e.entity_id.startswith(("input_text.tab5_", "input_select.tab5_"))]
    p = Passage(REMPLI, sans, RECHARGEMENT)
    assert p["meteo_a_ecrire"] == []
    # Option que la liste n'a pas (package plus ancien) : rien.
    etats = _maison_meteo()
    next(e for e in etats if e.entity_id == LISTE_PLUIE).attributes["options"] = ["Météo-France", "Aucune"]
    p = Passage({"meteo_pluie": "Open-Meteo"}, etats, RECHARGEMENT)
    assert p["meteo_a_ecrire"] == []


# ─────────────────────────────────────────────────────────────────────────────
# Qui prime : le blueprint rempli ; une liste changée à la main y revient
# ─────────────────────────────────────────────────────────────────────────────

def test_liste_changee_a_la_main_revient_au_blueprint_avec_une_notification():
    declencheur = _liste_changee(LISTE_METEO, AUTRE, VILLE, par_une_personne=True)
    p = Passage(REMPLI, _maison_meteo(meteo=VILLE, pluie="Open-Meteo", vigilance="Aucune"), declencheur)
    assert p["meteo_a_ecrire"] == [{"entite": LISTE_METEO, "valeur": AUTRE}]
    assert p.conditions()
    assert _branche(p) == "Météo choisie dans le blueprint : l'écrire dans les listes « Tab5 · … »"
    branche = _chercher(p.corps["actions"], lambda d: (d.get("alias") or "").startswith("Météo choisie"))
    notification = next(n for n in branche["sequence"] if "if" in n)
    assert p.modele(notification["if"]) is True
    assert notification["then"][0]["action"] == "persistent_notification.create"
    assert notification["then"][0]["data"]["notification_id"] == "tab5_meteo_blueprint"


def test_pas_de_notification_a_l_enregistrement():
    p = Passage(REMPLI, _maison_meteo(), RECHARGEMENT)
    branche = _chercher(p.corps["actions"], lambda d: (d.get("alias") or "").startswith("Météo choisie"))
    notification = next(n for n in branche["sequence"] if "if" in n)
    assert p.modele(notification["if"]) is False


def test_pas_de_ping_pong_entre_deux_automatisations():
    """L'écriture d'une automatisation (celle-ci, ou celle d'une autre tablette réglée
    autrement) n'a pas de user_id : elle n'est jamais reprise."""
    declencheur = _liste_changee(LISTE_METEO, AUTRE, VILLE, par_une_personne=False)
    p = Passage(REMPLI, _maison_meteo(meteo=VILLE, pluie="Open-Meteo", vigilance="Aucune"), declencheur)
    assert p["meteo_a_ecrire"] == []
    assert not p.conditions()
    # Sa propre écriture, relue : déjà à jour, rien non plus.
    declencheur = _liste_changee(LISTE_METEO, VILLE, AUTRE, par_une_personne=True)
    p = Passage(REMPLI, _maison_meteo(meteo=AUTRE, pluie="Open-Meteo", vigilance="Aucune"), declencheur)
    assert p["meteo_a_ecrire"] == []


def test_liste_supprimee_rien():
    declencheur = _liste_changee(LISTE_METEO, VILLE, AUTRE, par_une_personne=True)
    declencheur["to_state"] = None
    p = Passage(REMPLI, _maison_meteo(), declencheur)
    assert p["meteo_a_ecrire"] == []
