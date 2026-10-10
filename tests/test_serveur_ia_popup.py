# -*- coding: utf-8 -*-
"""Popup « Serveur IA » (ADR-0059) : le tableau de bord d'un serveur de LLM local.

Aucun compilateur ne relie le package `tab5_serveur_ia.yaml`, l'action
`tab5_maj_serveur_ia` et le firmware. Ce fichier le fait :

- contrat : l'action et sa variable, le format (quatorze champs « | ») face à
  `serveur_ia_lire()` (tab5_parse.h, section 14), les plafonds de longueur ;
- package : son VRAI modèle Jinja du payload est rendu (bac à sable de Jinja, comme HA,
  imitation de tests/test_historique.py) sur des capteurs simulés et comparé à un calcul
  Python écrit à part : unités converties (Go base 1024, °F, kW, mW), arrondis,
  pourcentage de VRAM sur les valeurs lues, niveau du GPU par les deux seuils, requêtes
  en file lues dans l'attribut `en_attente` (forme de tab5_llm.yaml), valeurs inconnues
  laissées vides, rien de choisi = payload vide ; les listes ne proposent que les
  capteurs qui conviennent ;
- poussées : deux secondes au moins entre deux, tout de suite à la connexion, à
  « MAJ Écran », au changement d'une liste et au démarrage de HA ;
- firmware : rien n'est peint popup fermé, la roue ne propose le popup qu'après une
  poussée, couleurs de la palette.

Ce n'est pas Home Assistant : seules les fonctions de modèle que le package appelle sont
imitées. Non essayé sur un vrai Home Assistant ni sur la tablette quand il a été écrit."""
import math
import os
import re

import pytest
import yaml

from tests.commun import lire as _lire
from tests.test_appuis import _fonction
from tests.test_historique import EtatHA, _env as _env_historique, _rendre

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TAB5 = os.path.join(REPO, "Tab5")
PACKAGE = os.path.join(REPO, "HomeAssistant_Config", "packages", "tab5_serveur_ia.yaml")
PARSE_H = os.path.join(TAB5, "socle", "tab5_parse.h")
API = os.path.join(TAB5, "paquets", "tab5-api-logic.yaml")
CPP = os.path.join(TAB5, "ecran", "tab5_serveur_ia.cpp")

ROLES = ["etat", "modele", "tokens", "en_cours", "en_file", "vram", "vram_totale", "temperature", "ram",
         "puissance"]
CHAMPS = ["nom", "etat", "modele", "tps", "cours", "file", "vram", "vram_total", "vram_pct", "temp", "niveau",
          "ram", "puissance", "actions"]
APPAREILS = {"pc_ia": {"name": "PC IA", "name_by_user": "Serveur | maison"},
             "glances": {"name": "Glances", "name_by_user": None}}


def _paquet():
    return yaml.safe_load(_lire(PACKAGE))


# ─── Contrat ───────────────────────────────────────────────────────────────────

def test_action_du_firmware():
    api = _lire(API)
    bloc = api.split("- service: tab5_maj_serveur_ia\n", 1)[1].split("\n    - service:", 1)[0]
    variables = yaml.safe_load(bloc.split("variables:\n", 1)[1].split("      then:", 1)[0])
    assert list(variables) == ["payload"] and variables["payload"]["type"] == "string"
    assert "serveur_ia_recu(payload);" in bloc
    # La poussée ne lie pas les widgets : popup jamais ouvert = popup nul, rien n'est peint ;
    # c'est l'ouverture qui lie, avant de peindre.
    assert "tab5_serveur_ia_lier" not in bloc.split("then:", 1)[1]
    ouvrir = _lire(TAB5, "paquets", "tab5-serveur-ia.yaml").split("- id: tab5_serveur_ia_ouvrir", 1)[1]
    assert ouvrir.index("script.execute: tab5_serveur_ia_lier") < ouvrir.index("serveur_ia_ouvrir();")
    assert "ui().popup != nullptr &&" in _lire(CPP)


def test_format_du_payload_partout():
    """Les quatorze champs dans le même ordre : en-tête de la lecture, package, démo."""
    attendu = "« " + "|".join(CHAMPS) + " »"
    entete = _lire(PARSE_H)
    assert attendu in entete and attendu in _lire(PACKAGE)
    assert "|".join(CHAMPS) in _lire(REPO, "Tab5", "README.md").replace("\\|", "|")
    assert re.search(r"constexpr int kServeurIaPoints = 24;", entete)
    # Plafonds : 40 caractères de nom, 80 de modèle — sous kServeurIaMax avec tout le reste.
    maximum = int(re.search(r"constexpr size_t kServeurIaMax = (\d+);", entete).group(1))
    assert "nom.t[:40]" in _lire(PACKAGE) and "modele[:80]" in _lire(PACKAGE)
    assert 40 * 4 + 80 * 4 + 12 * 12 + len("-decharger,-reveiller,-redemarrer") + 1 < maximum,         "noms en UTF-8 (4 octets au pire), douze champs courts et les trois actions"


# ─── Package : le modèle du payload ────────────────────────────────────────────

def _modele_payload():
    capteur = next(s for bloc in _paquet()["template"] if "sensor" in bloc for s in bloc["sensor"]
                   if s["unique_id"] == "tab5_poussee_serveur_ia")
    return capteur


class _Domaines:
    """`states` de HA : appel, indexation et domaines (states.sensor…)."""

    def __init__(self, base, etats):
        self.base, self.etats = base, etats

    def __call__(self, e):
        return self.base(e)

    def __getitem__(self, e):
        return self.base[e]

    def __getattr__(self, domaine):
        if domaine.startswith("_"):
            raise AttributeError(domaine)
        return [e for e in self.etats if e.entity_id.startswith(domaine + ".")]


def _env(etats):
    env = _env_historique(etats, None, APPAREILS)
    env.globals["states"] = _Domaines(env.globals["states"], etats)
    env.tests["match"] = lambda v, motif: re.match(motif, str(v)) is not None  # test match de HA
    return env


def _payload(etats, choix):
    """Le payload rendu, `choix` = {rôle: entité} (le reste sur « Aucun »)."""
    selects = [EtatHA(f"select.tab5_ia_{r}", choix.get(r, "Aucun")) for r in ROLES]
    env = _env(list(etats) + selects)
    capteur = _modele_payload()
    compte = _rendre(env, capteur["state"], {})
    payload = _rendre(env, capteur["attributes"]["payload"], {})
    assert compte == len(choix), "état = nombre de listes réglées"
    assert isinstance(payload, str), payload
    return payload


def _lire_comme_le_firmware(payload):
    """Découpe de serveur_ia_lire() : un enregistrement (le reste après « ; » ignoré),
    quatorze champs « | », nombres finis ou vides, actions parmi les trois codes."""
    if not payload:
        return None
    champs = payload.split(";")[0].split("|")
    assert len(champs) == len(CHAMPS), champs
    lu = dict(zip(CHAMPS, champs))
    for c in CHAMPS[3:13]:
        assert lu[c] == "" or math.isfinite(float(lu[c])), (c, lu[c])
    assert lu["etat"] in ("1", "0", "")
    assert lu["niveau"] in ("0", "1", "2", "")
    assert all(re.fullmatch(r"-?(decharger|reveiller|redemarrer)", a) for a in filter(None, lu["actions"].split(","))),         lu["actions"]
    for c in ("cours", "file", "vram_pct", "temp", "ram", "puissance", "niveau"):
        assert lu[c] == "" or re.fullmatch(r"-?\d+", lu[c]), (c, lu[c])
    return lu


def _attendu(nom="", etat="", modele="", tps=None, cours=None, file=None, vram_go=None, total_go=None,
             pct=None, temp_c=None, ram=None, watts=None, actions=""):
    """Calcul indépendant : arrondis de l'ADR-0059 (0,1 pour les tokens/s et les Go,
    entier ailleurs), niveau par 80 et 90 °C, vide pour l'inconnu."""
    def un(x, n=None):
        return "" if x is None else str(round(x, n)) if n else str(int(round(x)))
    if pct is None and vram_go is not None and total_go:
        pct = vram_go / total_go * 100
    temp = None if temp_c is None else int(round(temp_c))
    niveau = "" if temp is None else str(2 if temp >= 90 else 1 if temp >= 80 else 0)
    return {"nom": nom, "etat": etat, "modele": modele, "tps": un(tps, 1), "cours": un(cours), "file": un(file),
            "vram": un(vram_go, 1), "vram_total": un(total_go, 1), "vram_pct": un(pct),
            "temp": "" if temp is None else str(temp), "niveau": niveau, "ram": un(ram), "puissance": un(watts),
            "actions": actions}


def _ollama_et_glances():
    """Les capteurs de packages/tab5_llm.yaml (Ollama) et de Glances."""
    return [
        EtatHA("binary_sensor.tab5_serveur_ia_en_ligne", "on", appareil="pc_ia", device_class="connectivity"),
        EtatHA("sensor.tab5_serveur_ia_modele", "llama3.1:8b", appareil="pc_ia"),
        EtatHA("sensor.tab5_serveur_ia_vitesse", "42.47", unit_of_measurement="tokens/s", state_class="measurement"),
        EtatHA("sensor.tab5_serveur_ia_requetes", "2", state_class="measurement", en_attente=3),
        EtatHA("sensor.tab5_serveur_ia_vram", "7.83", appareil="pc_ia", unit_of_measurement="GiB",
               device_class="data_size", state_class="measurement"),
        EtatHA("sensor.glances_gpu_memoire_totale", "16384", appareil="glances", unit_of_measurement="MiB",
               device_class="data_size", state_class="measurement"),
        EtatHA("sensor.glances_gpu_temperature", "83.4", appareil="glances", unit_of_measurement="°C",
               device_class="temperature", state_class="measurement"),
        EtatHA("sensor.glances_ram", "61.4", appareil="glances", unit_of_measurement="%", state_class="measurement"),
        EtatHA("sensor.prise_pc_ia", "0.352", unit_of_measurement="kW", device_class="power",
               state_class="measurement"),
    ]


CHOIX_COMPLET = {"etat": "binary_sensor.tab5_serveur_ia_en_ligne", "modele": "sensor.tab5_serveur_ia_modele",
                 "tokens": "sensor.tab5_serveur_ia_vitesse", "en_cours": "sensor.tab5_serveur_ia_requetes",
                 "vram": "sensor.tab5_serveur_ia_vram", "vram_totale": "sensor.glances_gpu_memoire_totale",
                 "temperature": "sensor.glances_gpu_temperature", "ram": "sensor.glances_ram",
                 "puissance": "sensor.prise_pc_ia"}


def test_serveur_complet():
    lu = _lire_comme_le_firmware(_payload(_ollama_et_glances(), CHOIX_COMPLET))
    # Nom : l'appareil du premier capteur choisi (« | » remplacé), file : l'attribut en_attente.
    assert lu == _attendu(nom="Serveur / maison", etat="1", modele="llama3.1:8b", tps=42.47, cours=2, file=3,
                          vram_go=7.83, total_go=16384 / 1024, temp_c=83.4, ram=61.4, watts=352)
    assert lu["vram_pct"] == "49" and lu["niveau"] == "1" and lu["vram_total"] == "16.0"


def test_pourcentage_sur_les_valeurs_lues():
    """7,94 / 16 = 49,6 % → 50, quand l'arrondi affiché donnerait 7,9 / 16 = 49,4 → 49 :
    le calcul part des valeurs lues."""
    etats = _ollama_et_glances()
    etats[4] = EtatHA("sensor.tab5_serveur_ia_vram", "7.94", unit_of_measurement="GiB", device_class="data_size")
    lu = _lire_comme_le_firmware(_payload(etats, CHOIX_COMPLET))
    assert lu["vram"] == "7.9" and lu["vram_pct"] == "50"


def test_hors_ligne_et_unites_converties():
    etats = [
        EtatHA("sensor.releve", "hors_ligne", appareil="glances"),
        EtatHA("sensor.modele", "aucun"),
        EtatHA("sensor.vitesse", "unavailable", state_class="measurement"),
        EtatHA("sensor.vram_pct", "73.6", unit_of_measurement="%", state_class="measurement"),
        EtatHA("sensor.gpu_f", "194", unit_of_measurement="°F", device_class="temperature"),
        EtatHA("sensor.ram_go", "12.5", unit_of_measurement="GiB", device_class="data_size"),
        EtatHA("sensor.puissance_mw", "185000", unit_of_measurement="mW", device_class="power"),
        EtatHA("sensor.file", "0", state_class="measurement"),
    ]
    choix = {"etat": "sensor.releve", "modele": "sensor.modele", "tokens": "sensor.vitesse", "vram": "sensor.vram_pct",
             "temperature": "sensor.gpu_f", "ram": "sensor.ram_go", "puissance": "sensor.puissance_mw",
             "en_file": "sensor.file"}
    lu = _lire_comme_le_firmware(_payload(etats, choix))
    # « aucun » n'est pas un modèle ; une RAM qui n'est pas en % reste inconnue ; un
    # capteur de VRAM en % donne le pourcentage, pas des Go.
    assert lu == _attendu(nom="Glances", etat="0", file=0, pct=73.6, temp_c=(194 - 32) * 5 / 9, watts=185)
    assert lu["niveau"] == "2" and lu["tps"] == "" and lu["ram"] == ""


@pytest.mark.parametrize("etat, attendu", [("on", "1"), ("ok", "1"), ("chargement", "1"), ("off", "0"),
                                           ("erreur", "0"), ("unavailable", ""), ("unknown", ""), ("bof", "")])
def test_etat_du_serveur(etat, attendu):
    lu = _lire_comme_le_firmware(_payload([EtatHA("binary_sensor.srv", etat)], {"etat": "binary_sensor.srv"}))
    assert lu["etat"] == attendu
    assert [lu[c] for c in CHAMPS if c != "etat"] == [""] * 13, "rien d'inventé"


def test_rien_de_choisi_ou_textes_genants():
    assert _payload(_ollama_et_glances(), {}) == "", "aucune liste : payload vide (la tablette dit où choisir)"
    long = "modèle|très;long\n" + "x" * 120
    etats = [EtatHA("sensor.modele", long, appareil="pc_ia")]
    lu = _lire_comme_le_firmware(_payload(etats, {"modele": "sensor.modele"}))
    assert lu["modele"] == long[:80].replace("|", "/").replace(";", ",").replace("\n", " ")
    assert lu["nom"] == "Serveur / maison"
    assert len(lu["modele"]) == 80


def test_valeurs_negatives_ou_illisibles():
    etats = [EtatHA("sensor.v", "-3", state_class="measurement"), EtatHA("sensor.c", "abc", state_class="measurement")]
    lu = _lire_comme_le_firmware(_payload(etats, {"tokens": "sensor.v", "en_cours": "sensor.c"}))
    assert lu["tps"] == "" and lu["cours"] == "" and lu["file"] == ""


# ─── Package : les listes ───────────────────────────────────────────────────────

def _bloc_des_listes():
    return next(b for b in _paquet()["template"] if "select" in b)


def test_dix_listes_et_leurs_memoires():
    paquet = _paquet()
    bloc = _bloc_des_listes()
    selects = {s["unique_id"]: s for s in bloc["select"]}
    assert list(selects) == [f"tab5_ia_{r}" for r in ROLES]
    memoires = [f"input_text.tab5_choix_ia_{r}" for r in ROLES]
    assert sorted(paquet["input_text"]) == sorted(m.split(".")[1] for m in memoires)
    assert bloc["triggers"][-1]["entity_id"] == memoires
    for r, s in selects.items():
        assert s["default_entity_id"] == f"select.{r}"
        assert s["name"].startswith("Tab5 · serveur IA, ") and " · AI server, " in s["name"]
        memoire = f"input_text.tab5_choix_ia_{r[len('tab5_ia_'):]}"
        assert s["select_option"][0]["target"]["entity_id"] == memoire
        assert memoire in s["state"] and "'Aucun'" in s["state"] and "['Aucun']" in s["options"]
    immediate = next(a for a in paquet["automation"] if a["id"] == "tab5_serveur_ia_immediat")
    assert immediate["triggers"][0]["entity_id"] == memoires


def _options(etats, role, memoire="Aucun"):
    bloc = _bloc_des_listes()
    etats = list(etats) + [EtatHA(f"input_text.tab5_choix_ia_{role}", memoire)]
    env = _env(etats)
    ctx = {nom: _rendre(env, v, {}) for nom, v in bloc["variables"].items()}
    select = next(s for s in bloc["select"] if s["unique_id"] == f"tab5_ia_{role}")
    return _rendre(env, select["options"], ctx), _rendre(env, select["state"], ctx)


def test_chaque_liste_ne_propose_que_ce_qui_convient():
    etats = _ollama_et_glances() + [EtatHA("sensor.salon_humidite", "55", unit_of_measurement="%",
                                           device_class="humidity", state_class="measurement")]
    options = {r: _options(etats, r)[0] for r in ROLES}
    assert options["etat"] == ["binary_sensor.tab5_serveur_ia_en_ligne", "sensor.tab5_serveur_ia_modele",
                               "sensor.tab5_serveur_ia_requetes", "Aucun"]
    assert "sensor.tab5_serveur_ia_modele" in options["modele"] and "sensor.glances_ram" not in options["modele"]
    assert "sensor.tab5_serveur_ia_vitesse" in options["tokens"] and "binary_sensor" not in str(options["tokens"])
    assert options["vram_totale"] == ["sensor.tab5_serveur_ia_vram", "sensor.glances_gpu_memoire_totale", "Aucun"]
    assert "sensor.glances_ram" in options["vram"], "une VRAM utilisée en % est permise"
    assert options["temperature"] == ["sensor.glances_gpu_temperature", "Aucun"]
    assert options["ram"] == ["sensor.glances_ram", "sensor.salon_humidite", "Aucun"]
    assert options["puissance"] == ["sensor.prise_pc_ia", "Aucun"]
    assert all(o[-1] == "Aucun" for o in options.values())


def test_un_choix_disparu_reste_dans_sa_liste():
    """Une entité choisie puis supprimée reste proposée (sinon la liste refuserait son état)."""
    options, etat = _options(_ollama_et_glances(), "temperature", "sensor.gpu_parti")
    assert etat == "sensor.gpu_parti" and "sensor.gpu_parti" in options
    options, etat = _options(_ollama_et_glances(), "temperature", "n'importe quoi")
    assert etat == "Aucun"


# ─── Poussées ──────────────────────────────────────────────────────────────────

def test_deux_secondes_au_moins_entre_deux_poussees():
    autos = {a["id"]: a for a in _paquet()["automation"]}
    lente = autos["tab5_serveur_ia"]
    assert lente["mode"] == "single" and lente["max_exceeded"] == "silent"
    assert lente["triggers"] == [{"trigger": "state", "entity_id": "sensor.tab5_poussee_serveur_ia",
                                  "attribute": "payload"}]
    boucle = lente["actions"][0]["repeat"]
    assert boucle["sequence"][0]["target"]["entity_id"] == "script.tab5_serveur_ia_pousser"
    assert boucle["sequence"][1] == {"delay": {"seconds": 2}}
    assert ".total_seconds() >= 2" in boucle["until"][0]["value_template"]
    immediate = autos["tab5_serveur_ia_immediat"]
    evenements = {t.get("event_type") for t in immediate["triggers"]}
    assert {"esphome.tab5_connected", "esphome.tab5_maj_ecran"} <= evenements
    assert {"trigger": "homeassistant", "event": "start"} in immediate["triggers"]
    assert "tab5_origine(trigger) == 'oui'" in immediate["conditions"][0]["value_template"]


def test_script_de_poussee():
    script = _paquet()["script"]["tab5_serveur_ia_pousser"]
    seq = script["sequence"]
    assert seq[0]["condition"] == "template" and "tab5_connectee() == 'oui'" in seq[0]["value_template"]
    assert seq[1]["action"] == "esphome.tab5_ha_hmi_tab5_maj_serveur_ia" and seq[1]["continue_on_error"] is True
    assert list(seq[1]["data"]) == ["payload"]
    assert "state_attr('sensor.tab5_poussee_serveur_ia', 'payload')" in seq[1]["data"]["payload"]


def test_seuils_a_un_seul_endroit():
    paquet = _lire(PACKAGE)
    assert paquet.count("set attention, critique = 80, 90") == 1
    assert not re.search(r"\b(80|90)\b", _lire(CPP)), "aucun seuil dans la tablette"


# ─── Firmware ──────────────────────────────────────────────────────────────────

def test_rien_n_est_peint_popup_ferme():
    cpp = _lire(CPP)
    recu = _fonction(cpp, "void serveur_ia_recu(")
    assert "if (popup_ouvert()) peindre_popup();" in recu
    sans_garde = recu.replace("if (popup_ouvert()) peindre_popup();", "")
    assert not re.search(r"\b(lv_\w+|ui_\w+)\(", sans_garde), "aucune écriture LVGL hors du popup ouvert"
    assert "return s_present;" in _fonction(cpp, "bool serveur_ia_disponible()")
    assert "if (e == Ecran::SERVEUR_IA) return serveur_ia_disponible();" in \
        _lire(TAB5, "ecran", "tab5_roue_navigation.cpp")
    assert "serveur_ia_rejouer_theme();" in _lire(TAB5, "ecran", "tab5_theme.cpp")


def test_couleurs_de_la_palette():
    cpp = _lire(CPP)
    for role in ("UIColor.SUCCESS", "UIColor.WARNING", "UIColor.ERROR", "UIColor.INACTIVE", "UIColor.ACCENT"):
        assert role in cpp, role
    assert not re.search(r"lv_color_hex\(|0x[0-9A-Fa-f]{6}", cpp), "aucune couleur en dur (règle 1)"


# ─── Actions (ADR-0060) : le 14e champ ─────────────────────────────────────────

@pytest.mark.parametrize("etat_capteur, attendu", [
    ("decharger,-reveiller,redemarrer", "decharger,-reveiller,redemarrer"),
    ("aucune", ""), ("unknown", ""), ("unavailable", ""),
    # Seuls les trois codes passent, grisés ou non : rien d'autre n'arrive à la tablette.
    ("decharger,rm -rf /,reveiller|x,-redemarrer;y,-redemarrer", "decharger,-redemarrer"),
])
def test_actions_dans_le_payload(etat_capteur, attendu):
    etats = _ollama_et_glances() + [EtatHA("sensor.tab5_serveur_ia_actions", etat_capteur)]
    lu = _lire_comme_le_firmware(_payload(etats, CHOIX_COMPLET))
    assert lu["actions"] == attendu
    assert lu["modele"] == "llama3.1:8b", "les autres champs ne bougent pas"


def test_actions_sans_le_capteur():
    """packages/tab5_llm.yaml absent (capteurs venus d'ailleurs) : aucun bouton."""
    lu = _lire_comme_le_firmware(_payload(_ollama_et_glances(), CHOIX_COMPLET))
    assert lu["actions"] == ""
