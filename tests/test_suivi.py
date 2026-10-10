# -*- coding: utf-8 -*-
"""Capteurs suivis (ADR-0053) : popup « Suivi » et carte « capteur » de la zone à gauche de
l'horloge.

Aucun compilateur ne relie le package `tab5_suivi.yaml`, la liste de `tab5_reglages.yaml`,
le blueprint et le firmware. Ce fichier le fait :

- contrat : l'action `tab5_maj_suivi` (payload), sa lecture (`suivis_lire`, tab5_parse.h)
  et ses plafonds face au package et à la liste (six capteurs, points de la courbe) ;
- package : ses VRAIS modèles Jinja sont rendus (bac à sable de Jinja, comme HA, imitation
  de tests/test_historique.py) sur des réponses de `recorder.get_statistics` simulées et
  comparés à un calcul Python écrit à part : 24 moyennes horaires puis la valeur actuelle,
  ramenées de 0 à 100, heures sans ligne laissées vides, variation du jour (change_pct) ou
  écart depuis la première heure, capteur illisible, liste vide, réponse manquante ;
- firmware : la carte de la zone a l'emprise du graphique, le popup son registre, sa
  flèche est dans MDI_CODE_TARGETS, la zone saute le capteur quand la liste est vide.

Ce n'est pas Home Assistant : seules les fonctions de modèle que le package appelle sont
imitées. Vérifié à part le 10/10/2026 sur le HA d'Axel : deux capteurs de cours de bourse
ont state_class measurement et un attribut change_pct (2.05 et 0.95)."""
import datetime as dt
import os
import re

import pytest
import yaml

from tests.commun import lire as _lire
from tests.test_appuis import _fonction
from tests.test_historique import EtatHA, _env as _env_historique, _rendre

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
TAB5 = os.path.join(REPO, "Tab5")
PACKAGE = os.path.join(REPO, "HomeAssistant_Config", "packages", "tab5_suivi.yaml")
REGLAGES = os.path.join(REPO, "HomeAssistant_Config", "packages", "tab5_reglages.yaml")
PARSE_H = os.path.join(TAB5, "socle", "tab5_parse.h")
SUIVI_CPP = os.path.join(TAB5, "ecran", "tab5_suivi.cpp")
GAUCHE_CPP = os.path.join(TAB5, "ecran", "tab5_zone_gauche.cpp")
API = os.path.join(TAB5, "paquets", "tab5-api-logic.yaml")
LVGL = os.path.join(TAB5, "paquets", "tab5-lvgl.yaml")
ZONE = os.path.join(TAB5, "ui_components", "suivi_zone.yaml")

UTC = dt.timezone.utc
# Un après-midi d'octobre, 14:37 UTC : l'heure de 14:00 n'est pas encore compilée.
MAINTENANT = dt.datetime(2026, 10, 9, 14, 37, 20, tzinfo=UTC)
DEBUT = dt.datetime(2026, 10, 8, 14, 0, tzinfo=UTC)
BOURSE = "sensor.bourse_un"
SERRE = "sensor.serre_temperature"


def _constante(nom):
    m = re.search(rf"constexpr int {nom} = (\d+);", _lire(PARSE_H))
    assert m, nom
    return int(m.group(1))


# ─── Contrat et plafonds ───────────────────────────────────────────────────────

def test_action_du_firmware():
    api = _lire(API)
    bloc = api.split("- service: tab5_maj_suivi\n", 1)[1].split("\n    - service:", 1)[0]
    variables = yaml.safe_load(bloc.split("variables:\n", 1)[1].split("      then:", 1)[0])
    assert list(variables) == ["payload"] and variables["payload"]["type"] == "string"
    assert "script.execute: tab5_suivi_lier" in bloc and "suivi_recu(payload);" in bloc
    assert bloc.index("tab5_suivi_lier") < bloc.index("suivi_recu(payload);"), "widgets liés avant la peinture"


def test_plafonds_du_package_et_de_la_liste():
    """Six capteurs au plus partout : la lecture, les cartes du popup, la liste de HA (un
    septième ne s'ajoute pas) et la poussée. 24 heures et la valeur actuelle : 25 points,
    sous le plafond de la lecture."""
    assert _constante("kSuivisMax") == 6
    assert re.search(r"constexpr int kSuiviCartes = 6;", _lire(TAB5, "ecran", "tab5_suivi.h"))
    reglages = _lire(REGLAGES)
    assert "entite.startswith('sensor.') and entite not in choisis\n                         and choisis | count < 6" \
        in reglages.replace("\r\n", "\n")
    paquet = _lire(PACKAGE)
    assert paquet.count("| unique | list)[:6]") == 2, "signature et poussée : les six premiers seulement"
    assert "range(24)" in paquet and 24 + 1 <= _constante("kSuiviPointsMax")
    assert all(f"suivi_carte.yaml, vars: {{ n: \"{i}\" }}" in _lire(TAB5, "ui_components", "suivi_popup.yaml")
               for i in range(6))


def test_liste_de_ha():
    """« Tab5 · capteurs suivis » : entity_id fixé, mémoire déclarée et déclencheur de la
    liste, seulement les capteurs qui ont des statistiques."""
    paquet = yaml.safe_load(_lire(REGLAGES).replace("!", "!!str "))
    assert "tab5_choix_suivis" in paquet["input_text"]
    bloc = paquet["template"][0]
    assert "input_text.tab5_choix_suivis" in bloc["triggers"][-1]["entity_id"]
    select = next(s for s in bloc["select"] if s["unique_id"] == "tab5_capteurs_suivis")
    assert select["name"] == "Tab5 · capteurs suivis · tracked sensors"
    assert select["default_entity_id"] == "select.tab5_capteurs_suivis"
    assert "selectattr('attributes.state_class', 'eq', 'measurement')" in bloc["variables"]["suivis_ha"]


# ─── Package : rendu des vrais modèles ─────────────────────────────────────────

def _script():
    return yaml.safe_load(_lire(PACKAGE))["script"]["tab5_suivi_pousser"]


def _env(etats):
    env = _env_historique(etats, MAINTENANT.astimezone(dt.timezone(dt.timedelta(hours=2))))
    env.globals["utcnow"] = lambda: MAINTENANT
    env.tests["match"] = lambda v, motif: re.match(motif, str(v)) is not None  # test match de HA
    return env


def _pousser(etats, choix, stats=None):
    """Une exécution de script.tab5_suivi_pousser : stats = réponse de get_statistics
    (None : l'action a échoué, continue_on_error, sa variable n'existe pas)."""
    etats = list(etats) + [EtatHA("input_text.tab5_choix_suivis", choix)]
    env = _env(etats)
    seq = _script()["sequence"]
    assert seq[0]["condition"] == "template" and "tab5_connectee() == 'oui'" in seq[0]["value_template"]
    ctx = {}
    for nom, v in seq[1]["variables"].items():
        ctx[nom] = _rendre(env, v, ctx)
    assert seq[2]["action"] == "recorder.get_statistics" and seq[2]["continue_on_error"] is True
    demande = _rendre(env, seq[2]["data"], ctx)
    assert isinstance(demande, dict), demande
    if stats is not None:
        ctx["stats"] = stats
    for nom, v in seq[3]["variables"].items():
        ctx[nom] = _rendre(env, v, ctx)
    pousse = seq[4]
    assert pousse["action"] == "esphome.tab5_ha_hmi_tab5_maj_suivi" and pousse["continue_on_error"] is True
    return demande, str(_rendre(env, pousse["data"]["payload"], ctx))


def _lignes(entite, valeur, sauf=()):
    """Lignes horaires de HA depuis DEBUT (24 heures compilées), au format de la réponse."""
    return {entite: [{"start": (DEBUT + dt.timedelta(hours=h)).isoformat(),
                      "end": (DEBUT + dt.timedelta(hours=h + 1)).isoformat(), "mean": valeur(h)}
                     for h in range(24) if h not in sauf]}


def _attendus(moyennes, actuel):
    """Calcul indépendant : 24 moyennes (None = sans ligne) puis la valeur actuelle."""
    serie = list(moyennes) + [actuel]
    connus = [x for x in serie if x is not None]
    if len(connus) < 2:
        return ""
    bas, haut = min(connus), max(connus)
    return ",".join("" if x is None else str(round((x - bas) / (haut - bas) * 100) if haut > bas else 50)
                    for x in serie)


def _lire_comme_le_firmware(payload):
    """Découpe de suivis_lire() : « ; » entre capteurs, six champs « | », points 0-100."""
    capteurs = [c.split("|") for c in payload.split(";")] if payload else []
    for c in capteurs:
        assert len(c) == 6, c
        nom, valeur, unite, variation, genre, points = c
        assert genre in ("p", "a", "")
        assert (variation == "") == (genre == ""), c
        if points:
            pts = points.split(",")
            assert len(pts) <= _constante("kSuiviPointsMax")
            assert all(p == "" or 0 <= int(p) <= 100 for p in pts), points
    return capteurs


def test_demande_au_recorder():
    demande, _ = _pousser([EtatHA(BOURSE, "382.7", friendly_name="Action")], f"{BOURSE}, sensor.absent,input_text.x")
    assert demande["statistic_ids"] == [BOURSE, "sensor.absent"], "les sensor.* de la liste seulement"
    assert demande["period"] == "hour" and demande["types"] == ["mean"]
    assert dt.datetime.fromisoformat(demande["start_time"]) == DEBUT, "l'heure ronde moins 24 h, en UTC"


def test_cours_de_bourse_variation_du_jour():
    courbe = lambda h: 370 + h * 0.5
    stats = {"statistics": _lignes(BOURSE, courbe)}
    etats = [EtatHA(BOURSE, "382.7", friendly_name="Action | test", unit_of_measurement="USD", change_pct=2.05)]
    _, payload = _pousser(etats, BOURSE, stats)
    (c,) = _lire_comme_le_firmware(payload)
    assert c[:5] == ["Action / test", "382.7", "USD", "2.05", "p"]
    assert c[5] == _attendus([courbe(h) for h in range(24)], 382.7)
    assert c[5].split(",")[0] == "0" and c[5].split(",")[-1] == "100"


def test_temperature_ecart_et_heures_manquantes():
    courbe = lambda h: 18.0 + (h % 6) * 0.7
    stats = {"statistics": _lignes(SERRE, courbe, sauf=(0, 1, 7))}
    etats = [EtatHA(SERRE, "21.4", friendly_name="Serre", unit_of_measurement="°C")]
    _, payload = _pousser(etats, SERRE, stats)
    (c,) = _lire_comme_le_firmware(payload)
    moyennes = [None if h in (0, 1, 7) else courbe(h) for h in range(24)]
    assert c[:3] == ["Serre", "21.4", "°C"] and c[4] == "a"
    # Écart depuis la PREMIÈRE heure qui a une ligne (la troisième ici).
    assert float(c[3]) == pytest.approx(21.4 - courbe(2), abs=1e-4)
    assert c[5] == _attendus(moyennes, 21.4)
    assert c[5].split(",")[:2] == ["", ""] and c[5].split(",")[7] == ""


def test_courbe_plate_et_capteur_illisible():
    stats = {"statistics": {**_lignes(SERRE, lambda h: 20.0), **_lignes(BOURSE, lambda h: 5.0)}}
    etats = [EtatHA(SERRE, "20", friendly_name="Plat", unit_of_measurement="°C"),
             EtatHA(BOURSE, "unavailable", friendly_name=None)]
    _, payload = _pousser(etats, f"{SERRE},{BOURSE}", stats)
    plat, illisible = _lire_comme_le_firmware(payload)
    assert plat[5] == ",".join(["50"] * 25) and plat[3:5] == ["0.0", "a"]
    # Illisible : ni valeur ni écart, la courbe des 24 h seule ; aucun nom (l'écran écrit
    # « Capteur », jamais l'entity_id).
    assert illisible[:5] == ["", "", "", "", ""] and illisible[5] == _attendus([5.0] * 24, None)
    assert "sensor." not in payload


def test_sans_statistiques_ni_liste():
    etats = [EtatHA(SERRE, "21.4", friendly_name="Serre", unit_of_measurement="°C")]
    # Réponse manquante (action en échec) : la valeur sans courbe ni écart.
    _, payload = _pousser(etats, SERRE, None)
    assert _lire_comme_le_firmware(payload) == [["Serre", "21.4", "°C", "", "", ""]]
    # Liste vide : payload vide (l'écran dit où choisir, la zone saute le capteur).
    _, payload = _pousser(etats, "", {"statistics": {}})
    assert payload == ""


def test_six_capteurs_au_plus():
    entites = [f"sensor.c{i}" for i in range(8)]
    etats = [EtatHA(e, str(i), friendly_name=f"C{i}") for i, e in enumerate(entites)]
    demande, payload = _pousser(etats, ",".join(entites), {"statistics": {}})
    assert len(demande["statistic_ids"]) == 6
    assert [c[0] for c in _lire_comme_le_firmware(payload)] == [f"C{i}" for i in range(6)]


# ─── Automatisations ───────────────────────────────────────────────────────────

def test_poussees_limitees_et_immediates():
    autos = {a["id"]: a for a in yaml.safe_load(_lire(PACKAGE))["automation"]}
    lente = autos["tab5_suivi"]
    assert lente["mode"] == "single" and lente["max_exceeded"] == "silent"
    assert lente["triggers"] == [{"trigger": "state", "entity_id": "sensor.tab5_signature_du_suivi"}]
    assert {"delay": {"minutes": 5}} in lente["actions"]
    immediate = autos["tab5_suivi_immediat"]
    evenements = {t.get("event_type") for t in immediate["triggers"]}
    assert {"esphome.tab5_connected", "esphome.tab5_maj_ecran"} <= evenements
    assert {"trigger": "homeassistant", "event": "start"} in immediate["triggers"]
    assert "tab5_origine(trigger) == 'oui'" in immediate["conditions"][0]["value_template"]


# ─── Firmware ──────────────────────────────────────────────────────────────────

def test_zone_meme_emprise_que_le_graphique():
    zone = _lire(ZONE).split("widgets:", 1)[0]
    lvgl = _lire(LVGL)
    graphique = lvgl[lvgl.index("id: zone_graphique\n"):].split("on_short_click:", 1)[0]
    for cle in ("align", "x", "y", "width", "height", "pad_all", "hidden", "styles"):
        v = re.search(rf"^\s+{cle}: (.+)$", graphique, re.M).group(1)
        assert re.search(rf"^  {cle}: {re.escape(v)}$", zone, re.M), cle
    assert "id(tab5_ecran_ouvrir).execute((int) Ecran::SUIVI);" in _lire(ZONE) and "ui_appui_glisse()" in _lire(ZONE)
    assert lvgl.index("lecteur_zone.yaml") < lvgl.index("suivi_zone.yaml")


def test_capteur_saute_sans_capteur_suivi():
    gauche = _lire(GAUCHE_CPP)
    disponible = _fonction(gauche, "bool disponible(ZoneGauche z)")
    assert "case ZoneGauche::CAPTEUR: return g_zone_gauche_ui.capteur != nullptr && suivi_zone_disponible();" \
        in disponible
    appliquer = _fonction(gauche, "void zone_gauche_appliquer(")
    assert "ui_hidden(u.capteur, z != ZoneGauche::CAPTEUR);" in appliquer
    assert "suivi_zone_montrer(z == ZoneGauche::CAPTEUR);" in appliquer
    cpp = _lire(SUIVI_CPP)
    assert "return !(s_recu && s_n == 0);" in _fonction(cpp, "bool suivi_zone_disponible()")
    assert "if (suivi_zone_disponible() != zone_avant) zone_gauche_appliquer();" in _fonction(cpp, "void suivi_recu(")
    # Peinte seulement montrée, sinon marquée « sale ».
    peindre = _fonction(cpp, "void peindre_zone()")
    assert "if (!s_zone_montre)" in peindre and "s_zone_sale = true;" in peindre
    zones = _lire(TAB5, "paquets", "tab5-zones.yaml").split("- id: tab5_zones_apply", 1)[1]
    assert zones.index("script.execute: tab5_suivi_lier") < zones.index("zone_gauche_appliquer();")
    assert "g.capteur = id(zone_suivi);" in zones


def test_couleurs_de_la_palette_et_fleche():
    cpp = _lire(SUIVI_CPP)
    for role in ("UIColor.SUCCESS", "UIColor.ERROR", "UIColor.TEXT_DIM", "UIColor.ACCENT"):
        assert role in cpp, role
    assert not re.search(r"lv_color_hex\(|0x[0-9A-Fa-f]{6}", cpp), "aucune couleur en dur (règle 1)"
    regles = _lire(REPO, "tools", "check_tab5_code_rules.py")
    assert '("tab5_suivi.cpp", "glyphe_tendance"): ("suivi_fleche_*", "suivi_zone_fleche")' in regles
    assert "suivi_rejouer_theme();" in _lire(TAB5, "ecran", "tab5_theme.cpp")
    # Générique : ni « bourse » ni nom de valeur dans le code de l'écran.
    assert not re.search(r"\b(bourse|stock|tesla|cac)\b", cpp, re.I)
