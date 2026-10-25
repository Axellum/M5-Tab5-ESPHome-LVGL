"""Registre des écrans (ADR-0071) : ce qui lit les entités d'UN écran les lit pour CHAQUE écran.

25/10/2026, chez l'auteur, deux écrans connectés (le Tab5 et la Waveshare 7B, entités en
« _2 ») : les poussées partaient aux deux (ADR-0063), mais tout ce qui passait par
sensor.tab5_tablette ne suivait que le premier. La 7B ne s'allumait ni ne s'éteignait à la
présence, n'avait ni thème auto ni extinction de nuit, ses redémarrages et ses coupures
n'étaient pas signalés, et le briefing du réveil parlait sur l'autre écran. Ce fichier rend
les macros de custom_templates/tab5_tablette.jinja et les modèles des packages sur une
fausse maison à deux écrans, plus un écran hors ligne et un appareil ESPHome étranger.
"""
from __future__ import annotations

import ast
import datetime as dt
import re

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

from tests.commun import REPO, ChargeurSansBalises

HA = REPO / "HomeAssistant_Config"
MAINTENANT = dt.datetime(2026, 10, 25, 12, 0, tzinfo=dt.timezone.utc)

# Appareil → (modèle, nom, entités {entity_id: état}).
MAISON = {
    "d_tab5": ("tab5-ha-hmi", "Tab5", {
        "binary_sensor.tab5_ha_api_status": "on",
        "sensor.tab5_action_prefix": "tab5",
        "light.tab5_display_backlight": "on",
        "binary_sensor.tab5_alarm_ringing": "off",
        "switch.tab5_wake_word_active": "on",
        "switch.tab5_night_auto_theme": "off",
        "switch.tab5_night_screen_off": "off",
        "sensor.tab5_uptime": "2026-10-21T17:44:32+00:00",
        "sensor.tab5_restart_reason": "Power on",
        "number.tab5_appointments_announce_before": "15",
        "select.tab5_language": "Français",
        "assist_satellite.tab5_assist_satellite": "idle",
        "select.tab5_assistant": "preferred",
        "select.tab5_assistant_2": "preferred",
    }),
    # Second écran : nom par défaut, entités « _2 », firmware sans préfixe lisible encore.
    "d_7b": ("tab5-ha-hmi", "M5Stack Tab5 Home Assistant HMI", {
        "binary_sensor.m5stack_tab5_home_assistant_hmi_ha_api_status": "on",
        "sensor.m5stack_tab5_home_assistant_hmi_action_prefix": "unknown",
        "light.m5stack_tab5_home_assistant_hmi_display_backlight_2": "off",
        "binary_sensor.m5stack_tab5_home_assistant_hmi_alarm_ringing_2": "on",
        "switch.m5stack_tab5_home_assistant_hmi_wake_word_active_2": "on",
        "switch.m5stack_tab5_home_assistant_hmi_night_auto_theme_2": "off",
        "switch.m5stack_tab5_home_assistant_hmi_night_screen_off_2": "off",
        "sensor.m5stack_tab5_home_assistant_hmi_uptime_2": "2026-10-25T11:59:30+00:00",
        "number.m5stack_tab5_home_assistant_hmi_appointments_announce_before_2": "30",
        "select.m5stack_tab5_home_assistant_hmi_language_2": "English",
        "assist_satellite.m5stack_tab5_home_assistant_hmi_assist_satellite": "idle",
    }),
    # Un ancien écran resté dans HA, hors ligne.
    "d_ancien": ("tab5-ha-hmi", "Chambre", {
        "binary_sensor.chambre_ha_api_status": "unavailable",
        "light.chambre_display_backlight": "unavailable",
    }),
    # Un autre appareil ESPHome : jamais un écran.
    "d_autre": ("esp32-relais", "Relais", {
        "binary_sensor.relais_ha_api_status": "on",
        "light.relais_display_backlight": "on",
    }),
}
OPTIONS = {"select.tab5_assistant": ["preferred", "Assist"], "select.tab5_assistant_2": ["preferred", "Assist"]}


class _Etat:
    def __init__(self, entite, etat):
        self.entity_id, self.state = entite, etat
        self.last_changed = MAINTENANT - dt.timedelta(minutes=5)


class _Etats:
    """`states` de HA : states('x'), states.binary_sensor (liste d'états)."""

    def __init__(self, etats):
        self._etats = etats

    def __call__(self, e):
        return self._etats.get(e, "unknown")

    def __getattr__(self, domaine):
        return [_Etat(e, v) for e, v in self._etats.items() if e.startswith(domaine + ".")]


def _env(maison=MAISON, attributs=None, plus=None):
    """`attributs` : {entity_id: {attribut: valeur}} lus par state_attr ; `plus` : des états
    hors des appareils (listes de la maison, capteurs de présence, lecteurs)."""
    etats = {e: v for _, _, ents in maison.values() for e, v in ents.items()} | (plus or {})
    attributs = attributs or {}

    def device_id(e):
        return next((d for d, (_, _, ents) in maison.items() if e in ents), None)

    def device_attr(d, nom):
        d = d if d in maison else device_id(d)
        if d is None:
            return None
        modele, appele, _ = maison[d]
        return {"model": modele, "name": appele, "sw_version": "3.9.0 (ESPHome 2026.9.1)"}.get(nom)

    env = ImmutableSandboxedEnvironment(undefined=jinja2.ChainableUndefined,
                                        loader=jinja2.FileSystemLoader(str(HA / "custom_templates")))
    est = lambda e, v: etats.get(e) == v  # noqa: E731
    env.globals.update(
        device_entities=lambda d: list(maison.get(d, (None, None, {}))[2]),
        device_attr=device_attr, device_id=device_id, is_state=est,
        integration_entities=lambda domaine: list(etats),
        states=_Etats(etats),
        state_attr=lambda e, a: OPTIONS.get(e) if a == "options" else attributs.get(e, {}).get(a),
        has_value=lambda e: etats.get(e, "unknown") not in ("unknown", "unavailable"),
        expand=lambda e: [e] if e in etats else [],
    )
    env.filters.update(device_attr=device_attr, device_id=device_id, states=lambda e: etats.get(e, "unknown"))
    env.tests.update(is_state=est, match=lambda v, motif: re.match(motif, str(v)) is not None,
                     is_device_attr=lambda d, nom, v: device_attr(d, nom) == v)
    return env


def _macro(appel, maison=MAISON):
    modele = "{%- from 'tab5_tablette.jinja' import tab5_cibles, tab5_destinataires, tab5_entite, tab5_nom -%}"
    return _env(maison).from_string(modele + appel).render().strip()


def _paquet(nom):
    return yaml.load((HA / "packages" / nom).read_text(encoding="utf-8"), Loader=ChargeurSansBalises)


def _automatisation(paquet, ident):
    return next(a for a in _paquet(paquet)["automation"] if a["id"] == ident)


# ─── Macros ──────────────────────────────────────────────────────────────────

def test_cibles_les_ecrans_connectes_seulement():
    assert _macro("{{ tab5_cibles('') }}") == "d_tab5,d_7b"


def test_cibles_l_ecran_qui_demande_meme_hors_ligne():
    assert _macro("{{ tab5_cibles('d_7b') }}") == "d_7b"
    assert _macro("{{ tab5_cibles('d_ancien') }}") == "d_ancien"
    # Un appareil étranger ou une entité : tous les écrans connectés, comme tab5_destinataires.
    assert _macro("{{ tab5_cibles('d_autre') }}") == "d_tab5,d_7b"
    assert _macro("{{ tab5_cibles('light.tab5_display_backlight') }}") == "d_tab5,d_7b"


def test_destinataires_inchanges():
    """tab5_destinataires passe par tab5_cibles : mêmes préfixes qu'avant (ADR-0063), le
    préfixe illisible de la 7B retombe sur tab5_ha_hmi."""
    assert _macro("{{ tab5_destinataires('') }}") == "tab5,tab5_ha_hmi"
    assert _macro("{{ tab5_destinataires('d_7b') }}") == "tab5_ha_hmi"


@pytest.mark.parametrize("role, tab5, septb", [
    ("api", "binary_sensor.tab5_ha_api_status", "binary_sensor.m5stack_tab5_home_assistant_hmi_ha_api_status"),
    ("ecran", "light.tab5_display_backlight", "light.m5stack_tab5_home_assistant_hmi_display_backlight_2"),
    ("reveil_en_cours", "binary_sensor.tab5_alarm_ringing", "binary_sensor.m5stack_tab5_home_assistant_hmi_alarm_ringing_2"),
    ("micro", "switch.tab5_wake_word_active", "switch.m5stack_tab5_home_assistant_hmi_wake_word_active_2"),
    ("theme_nuit", "switch.tab5_night_auto_theme", "switch.m5stack_tab5_home_assistant_hmi_night_auto_theme_2"),
    ("nuit_ecran", "switch.tab5_night_screen_off", "switch.m5stack_tab5_home_assistant_hmi_night_screen_off_2"),
    ("demarrage", "sensor.tab5_uptime", "sensor.m5stack_tab5_home_assistant_hmi_uptime_2"),
    ("raison", "sensor.tab5_restart_reason", ""),
    ("rdv_avant", "number.tab5_appointments_announce_before",
     "number.m5stack_tab5_home_assistant_hmi_appointments_announce_before_2"),
    ("langue", "select.tab5_language", "select.m5stack_tab5_home_assistant_hmi_language_2"),
    ("satellite", "assist_satellite.tab5_assist_satellite", "assist_satellite.m5stack_tab5_home_assistant_hmi_assist_satellite"),
    ("pipeline", "select.tab5_assistant", ""),
    ("inconnu", "", ""),
])
def test_entite_de_chaque_ecran(role, tab5, septb):
    assert _macro(f"{{{{ tab5_entite('d_tab5', '{role}') }}}}") == tab5
    assert _macro(f"{{{{ tab5_entite('d_7b', '{role}') }}}}") == septb
    assert _macro(f"{{{{ tab5_entite('', '{role}') }}}}") == ""


def test_nom():
    assert _macro("{{ tab5_nom('d_7b') }}") == "M5Stack Tab5 Home Assistant HMI"
    assert _macro("{{ tab5_nom('') }}") == "Tab5"


# ─── Registre sensor.tab5_ecrans ─────────────────────────────────────────────

def _registre():
    for bloc in _paquet("tab5_reglages.yaml")["template"]:
        for capteur in bloc.get("sensor") or []:
            if capteur.get("unique_id") == "tab5_ecrans":
                assert "triggers" not in bloc, "modèle ordinaire : recalculé quand un écran change"
                return capteur
    raise AssertionError("sensor.tab5_ecrans introuvable dans tab5_reglages.yaml")


def test_registre():
    capteur = _registre()
    assert capteur["default_entity_id"] == "sensor.tab5_ecrans"
    env = _env()
    assert env.from_string(capteur["state"]).render().strip() == "2"
    ecrans = ast.literal_eval(env.from_string(capteur["attributes"]["ecrans"]).render().strip())
    assert [(e["appareil"], e["prefixe"], e["connectee"]) for e in ecrans] == [
        ("d_ancien", "tab5_ha_hmi", False),
        ("d_7b", "tab5_ha_hmi", True),
        ("d_tab5", "tab5", True),
    ]
    assert {e["version"] for e in ecrans} == {"3.9.0"}
    assert ast.literal_eval(env.from_string(capteur["attributes"]["hors_ligne"]).render().strip()) == ["d_ancien"]


# ─── Automatisations ─────────────────────────────────────────────────────────

def test_presence_chaque_ecran():
    auto = _automatisation("tab5_push.yaml", "tab5_screen_presence_wifi")
    env = _env()
    telephone = {"id": "allumer", "platform": "state"}
    appareils = ast.literal_eval(env.from_string(auto["variables"]["appareils"]).render(
        trigger=telephone, entres=[]).strip())
    assert appareils == ["d_tab5", "d_7b"]
    _, boucle = auto["actions"]
    variables = boucle["repeat"]["sequence"][0]["variables"]
    vus = {d: (env.from_string(variables["ecran"]).render(repeat={"item": d}).strip(),
               env.from_string(variables["reveil"]).render(repeat={"item": d}).strip()) for d in appareils}
    assert vus == {
        "d_tab5": ("light.tab5_display_backlight", "binary_sensor.tab5_alarm_ringing"),
        "d_7b": ("light.m5stack_tab5_home_assistant_hmi_display_backlight_2",
                 "binary_sensor.m5stack_tab5_home_assistant_hmi_alarm_ringing_2"),
    }
    # Chaque écran a sa garde « réveil qui sonne » : la 7B sonne, elle ne s'éteint pas.
    eteindre = boucle["repeat"]["sequence"][1]["choose"][1]["conditions"][-1]["value_template"]
    assert env.from_string(eteindre).render(reveil=vus["d_7b"][1]).strip() == "False"
    assert env.from_string(eteindre).render(reveil=vus["d_tab5"][1]).strip() == "True"


@pytest.mark.parametrize("ident", ["tab5_theme_jour_nuit", "tab5_extinction_nuit"])
def test_theme_et_nuit_cibles(ident):
    (boucle,) = _automatisation("tab5_push.yaml", ident)["actions"]
    modele = boucle["repeat"]["for_each"]
    env = _env()
    soleil = {"id": "0", "platform": "state"}
    connexion = {"id": "tab5_connected", "platform": "event", "event": {"data": {"device_id": "d_7b"}}}
    assert ast.literal_eval(env.from_string(modele).render(trigger=soleil).strip()) == ["d_tab5", "d_7b"]
    assert ast.literal_eval(env.from_string(modele).render(trigger=connexion).strip()) == ["d_7b"]


def _reboot():
    return _automatisation("tab5_health.yaml", "tab5_health_unexpected_reboot")


def test_reboot_lit_l_ecran_qui_se_connecte():
    auto = _reboot()
    assert auto["triggers"] == [{"trigger": "event", "event_type": "esphome.tab5_connected"}]
    assert auto["mode"] == "parallel", "deux écrans peuvent redémarrer ensemble"
    variables = auto["actions"][0]["variables"]
    env = _env()
    trigger = {"event": {"data": {"device_id": "d_7b"}}}
    appareil = env.from_string(variables["appareil"]).render(trigger=trigger).strip()
    assert appareil == "d_7b"
    assert env.from_string(variables["e_demarrage"]).render(appareil=appareil).strip() == \
        "sensor.m5stack_tab5_home_assistant_hmi_uptime_2"
    assert env.from_string(variables["nom"]).render(appareil=appareil).strip() == "M5Stack Tab5 Home Assistant HMI"
    # Une notification par écran.
    notif = next(a for a in auto["actions"] if a.get("action") == "script.tab5_health_notify")
    assert "{{ appareil }}" in notif["data"]["notification_id"]


def _horodatage(valeur, defaut=None):
    try:
        return dt.datetime.fromisoformat(str(valeur)).timestamp()
    except ValueError:
        return defaut


@pytest.mark.parametrize("uptime, cree_il_y_a, demarrage", [
    ("2026-10-25T11:59:30+00:00", dt.timedelta(days=4), True),    # démarrée il y a 30 s
    ("2026-10-21T17:44:32+00:00", dt.timedelta(days=4), False),   # simple reconnexion
    ("unavailable", dt.timedelta(days=4), False),                 # heure pas encore reçue
    ("2026-10-25T11:59:30+00:00", dt.timedelta(minutes=2), False),  # écran ajouté à l'instant
])
def test_reboot_seulement_un_vrai_demarrage(uptime, cree_il_y_a, demarrage):
    auto = _reboot()
    attente = next(a for a in auto["actions"] if "wait_template" in a and "e_demarrage" in a["wait_template"])
    garde = next(a for a in auto["actions"] if a.get("condition") == "template")["value_template"]
    env = _env()
    env.globals["as_timestamp"] = _horodatage
    env.globals["states"] = lambda e: uptime
    connexion = MAINTENANT.timestamp()
    fini = env.from_string(attente["wait_template"]).render(e_demarrage="x", connexion=connexion).strip() == "True"
    env.globals["device_attr"] = lambda d, nom: (MAINTENANT - cree_il_y_a).isoformat()
    passe = env.from_string(garde).render(wait={"completed": fini}, appareil="d_7b", connexion=connexion).strip()
    assert (passe == "True") is demarrage


@pytest.mark.parametrize("avant, apres, nouveaux", [
    ([], ["d_7b"], ["d_7b"]),
    (["d_ancien"], ["d_7b", "d_ancien"], ["d_7b"]),   # l'ancien écran déjà signalé : rien de plus
    (["d_7b"], [], []),                                 # retour : rien
    (None, ["d_ancien"], []),                           # démarrage de HA : pas d'alerte
])
def test_alerte_de_deconnexion_par_ecran(avant, apres, nouveaux):
    auto = _automatisation("tab5_health.yaml", "tab5_health_api_off_2min")
    assert auto["triggers"] == [{"trigger": "state", "entity_id": "sensor.tab5_ecrans",
                                 "attribute": "hors_ligne", "for": "00:02:00"}]
    attributs_avant = {} if avant is None else {"hors_ligne": avant}
    trigger = {"from_state": {"attributes": attributs_avant}, "to_state": {"attributes": {"hors_ligne": apres}}}
    rendu = _env().from_string(auto["variables"]["nouveaux"]).render(trigger=trigger).strip()
    assert ast.literal_eval(rendu) == nouveaux
    (boucle,) = auto["actions"]
    notif = boucle["repeat"]["sequence"][-1]
    assert notif["data"]["notification_id"] == "tab5_health_api_{{ repeat.item }}"


def test_micro_chaque_ecran():
    auto = _automatisation("tab5_micro_absence.yaml", "tab5_micro_absence")
    assert {"trigger": "event", "event_type": "esphome.tab5_connected", "id": "resync"} in auto["triggers"]
    couper = auto["actions"][0]["choose"][0]["sequence"][0]["repeat"]["for_each"]
    assert ast.literal_eval(_env().from_string(couper).render().strip()) == [
        "switch.tab5_wake_word_active", "switch.m5stack_tab5_home_assistant_hmi_wake_word_active_2"]


def test_briefing_sur_l_ecran_qui_sonne():
    evenements = _paquet("tab5_evenements.yaml")
    texte = yaml.safe_dump(evenements, allow_unicode=True)
    assert "appareil: '{{ trigger.event.data.device_id }}'" in texte
    script = _paquet("tab5_reveil.yaml")["script"]["tab5_reveil_annonce"]
    assert script["mode"] == "parallel"
    variables = script["sequence"][1]["variables"]
    env = _env()
    ecran = env.from_string(variables["ecran_reveil"]).render(appareil="d_7b").strip()
    assert ecran == "d_7b"
    assert env.from_string(variables["satellite"]).render(ecran_reveil=ecran).strip() == \
        "assist_satellite.m5stack_tab5_home_assistant_hmi_assist_satellite"
    assert env.from_string(variables["langue_reveil"]).render(ecran_reveil=ecran).strip() == \
        "select.m5stack_tab5_home_assistant_hmi_language_2"
    # Lancé à la main, sans appareil : le premier écran connecté.
    assert env.from_string(variables["ecran_reveil"]).render().strip() == "d_tab5"


def test_annoncer_avant_de_chaque_ecran():
    for bloc in _paquet("tab5_reglages.yaml")["template"]:
        for capteur in bloc.get("sensor") or []:
            if capteur.get("unique_id") == "tab5_rendez_vous_annoncer_avant":
                env = _env()
                assert env.from_string(capteur["state"]).render().strip() == "15 · 30"
                assert env.from_string(capteur["availability"]).render().strip() == "True"
                return
    raise AssertionError("miroir « annoncer avant » introuvable")


# ─── Garde-fou ───────────────────────────────────────────────────────────────

# Lectures de sensor.tab5_tablette gardées exprès dans tab5_reglages.yaml : sa définition,
# le miroir « Tab5 · connected » (test d'installation), « Tab5 · startup » (statistiques
# longues) et les options de la liste des pipelines (ceux de la maison).
GARDEES = {"'pipeline'", "'api'", "'demarrage'"}


def test_plus_aucun_package_ne_suit_un_seul_ecran():
    for chemin in sorted((HA / "packages").glob("*.yaml")):
        for n, ligne in enumerate(chemin.read_text(encoding="utf-8").splitlines(), 1):
            if ligne.lstrip().startswith("#") or "state_attr('sensor.tab5_tablette'" not in ligne:
                continue
            assert chemin.name == "tab5_reglages.yaml" and any(g in ligne for g in GARDEES), (
                f"{chemin.name}:{n} lit sensor.tab5_tablette, qui ne suit qu'un écran : "
                "boucler sur tab5_cibles() et lire tab5_entite() (ADR-0071)")


# ─── Connexion d'un écran (25/10/2026, 14:04) ────────────────────────────────
# La 7B redémarrée émet esphome.tab5_connected ~0,15 s avant que HA passe son « HA API
# Status » à on. L'attente « une tablette connectée » passait grâce au Tab5, la liste des
# destinataires (écrans connectés) ne contenait que lui : la poussée complète de la 7B
# partait au Tab5. Chaque poussée de connexion attend maintenant CET écran et ne sert que lui.

def _7b_pas_encore_on():
    maison = dict(MAISON)
    modele, nom, ents = maison["d_7b"]
    maison["d_7b"] = (modele, nom, {**ents, "binary_sensor.m5stack_tab5_home_assistant_hmi_ha_api_status": "off"})
    return maison


CONNEXION_7B = {"id": "tab5_connected", "platform": "event",
                "event": {"event_type": "esphome.tab5_connected", "data": {"device_id": "d_7b"}}}


def test_pret_attend_l_ecran_qui_se_connecte():
    assert _macro("{% from 'tab5_tablette.jinja' import tab5_pret %}{{ tab5_pret('d_7b') }}", _7b_pas_encore_on()) == ""
    assert _macro("{% from 'tab5_tablette.jinja' import tab5_pret %}{{ tab5_pret('d_7b') }}") == "oui"
    # Sans écran : une tablette connectée suffit, comme tab5_connectee().
    assert _macro("{% from 'tab5_tablette.jinja' import tab5_pret %}{{ tab5_pret('') }}", _7b_pas_encore_on()) == "oui"


def test_appareil_de_la_connexion():
    modele = "{% from 'tab5_tablette.jinja' import tab5_appareil_connexion %}{{ tab5_appareil_connexion(trigger) }}"
    env = _env()
    assert env.from_string(modele).render(trigger=CONNEXION_7B) == "d_7b"
    autre = {"platform": "event", "event": {"event_type": "esphome.tab5_maj_ecran", "data": {"device_id": "d_7b"}}}
    assert env.from_string(modele).render(trigger=autre) == ""
    assert env.from_string(modele).render(trigger={"platform": "time_pattern"}) == ""


def test_poussee_complete_de_l_ecran_qui_se_connecte():
    auto = _automatisation("tab5_push.yaml", "tab5_ha_hmi_updater")
    env = _env(_7b_pas_encore_on())
    appareil = env.from_string(auto["actions"][0]["variables"]["appareil"]).render(trigger=CONNEXION_7B).strip()
    assert appareil == "d_7b"
    attente = auto["actions"][1]["then"][0]["wait_template"]
    assert env.from_string(attente).render(appareil=appareil).strip() == "False", "la 7B pas encore on : on attend"
    assert _env().from_string(attente).render(appareil=appareil).strip() == "True"
    # Les envois : la 7B seule, même avant son « on » (avant : le Tab5 seul).
    envoi = next(a for a in auto["actions"] if "if" in a and "repeat" in str(a.get("then")))
    for_each = envoi["then"][0]["repeat"]["for_each"]
    assert ast.literal_eval(env.from_string(for_each).render(appareil=appareil).strip()) == ["tab5_ha_hmi"]
    # Autres déclencheurs : chaque écran connecté, comme avant.
    assert ast.literal_eval(_env().from_string(for_each).render(appareil="").strip()) == ["tab5", "tab5_ha_hmi"]
    # Les scripts appelés reçoivent l'écran.
    appels = [a for a in auto["actions"] if str(a.get("action", "")).startswith("script.tab5_push_")]
    appels += [b for a in auto["actions"] if "then" in a for b in a["then"]
               if str(b.get("action", "")).startswith("script.tab5_push_")]
    assert len(appels) == 4 and all(a["data"]["appareil"] == "{{ appareil }}" for a in appels)


@pytest.mark.parametrize("paquet", ["tab5_push.yaml", "tab5_reveil.yaml", "tab5_lecteur.yaml", "tab5_suivi.yaml",
                                    "tab5_froid.yaml", "tab5_serveur_ia.yaml", "tab5_musique.yaml"])
def test_chaque_poussee_de_connexion_attend_son_ecran(paquet):
    for auto in _paquet(paquet)["automation"]:
        if not any(t.get("event_type") == "esphome.tab5_connected" for t in auto["triggers"]):
            continue
        texte = yaml.safe_dump(auto["actions"], allow_unicode=True)
        if "esphome." not in texte and "script." not in texte:
            continue  # rien n'est envoyé (thème, nuit, santé : ils ciblent déjà l'écran)
        if "tab5_cibles(" in texte or "tab5_entite(" in texte:
            continue
        assert "tab5_appareil_connexion(trigger)" in texte and "tab5_pret(" in yaml.safe_dump(
            _paquet(paquet), allow_unicode=True), f"{paquet} : {auto['id']} n'attend pas l'écran qui se connecte"


# ─── Réglages par écran (ADR-0072, 25/10/2026) ───────────────────────────────
# Capteur de présence, extinction de nuit et lecteurs de musique : la valeur propre d'un
# écran (sensor.tab5_reglages_ecrans), sinon celle de la maison.

ECRANS = {"sensor.tab5_ecrans": {"ecrans": [{"appareil": "d_ancien"}, {"appareil": "d_7b"}, {"appareil": "d_tab5"}]}}
MAISON_REGLEE = {
    "select.tab5_capteur_de_presence": "binary_sensor.salon",
    "select.tab5_extinction_de_nuit": "00:00-06:00",
    "input_text.tab5_choix_lecteurs": "media_player.salon, media_player.tv",
    "binary_sensor.salon": "on",
    "binary_sensor.chambre": "off",
    "media_player.salon": "idle",
    "media_player.tv": "playing",
    "media_player.chambre": "idle",
    "media_player.cuisine": "idle",
    "input_text.tab5_lecteur_actif": "",
}
TABLETTE = ("{%- from 'tab5_tablette.jinja' import tab5_reglage, tab5_reglage_propre, tab5_presences, tab5_nuits,"
            " tab5_ecrans, tab5_ecran_a_regler, tab5_lecteurs_tous, tab5_lecteur_montre -%}")


def _regle(reglages, plus=None):
    attributs = dict(ECRANS, **{"sensor.tab5_reglages_ecrans": {"reglages": reglages}})
    return _env(attributs=attributs, plus=MAISON_REGLEE | (plus or {}))


def _rend(env, appel, **contexte):
    return env.from_string(TABLETTE + appel).render(**contexte).strip()


def test_reglage_d_un_ecran_sinon_la_maison():
    env = _regle({"d_7b": {"presence": "binary_sensor.chambre", "nuit": "Not set", "lecteurs": "media_player.chambre"}})
    assert _rend(env, "{{ tab5_reglage('d_7b', 'presence') }}") == "binary_sensor.chambre"
    assert _rend(env, "{{ tab5_reglage('d_tab5', 'presence') }}") == "binary_sensor.salon"
    assert _rend(env, "{{ tab5_reglage('d_7b', 'nuit') }}") == "Not set"
    assert _rend(env, "{{ tab5_reglage('d_tab5', 'nuit') }}") == "00:00-06:00"
    assert _rend(env, "{{ tab5_reglage('d_7b', 'lecteurs') }}") == "media_player.chambre"
    assert _rend(env, "{{ tab5_reglage('d_tab5', 'lecteurs') }}") == "media_player.salon,media_player.tv"
    assert _rend(env, "{{ tab5_reglage('', 'lecteurs') }}") == "media_player.salon,media_player.tv"
    assert _rend(env, "{{ tab5_reglage_propre('d_tab5', 'nuit') }}") == ""
    # Lecteurs « Not set » : aucun, pas ceux de la maison.
    env = _regle({"d_7b": {"lecteurs": "Not set"}})
    assert _rend(env, "{{ tab5_reglage('d_7b', 'lecteurs') }}") == ""
    # Rien de gardé (premier démarrage) : la maison partout.
    env = _regle(None)
    assert _rend(env, "{{ tab5_reglage('d_7b', 'presence') }}") == "binary_sensor.salon"


def test_presences_par_ecran():
    env = _regle({"d_7b": {"presence": "binary_sensor.chambre"}, "d_ancien": {"presence": "Not set"}})
    assert _rend(env, "{{ tab5_presences('on') }}") == "d_tab5"
    assert _rend(env, "{{ tab5_presences('off') }}") == "d_7b"
    # Sans réglage propre, chaque écran suit le capteur de la maison (comme avant).
    env = _regle({})
    assert _rend(env, "{{ tab5_presences('on') }}") == "d_ancien,d_7b,d_tab5"
    assert _rend(env, "{{ tab5_presences('off') }}") == ""


@pytest.mark.parametrize("h, attendu", [("03:00", "d_ancien,d_tab5"), ("23:30", "d_7b"), ("12:00", "")])
def test_nuits_par_ecran(h, attendu):
    env = _regle({"d_7b": {"nuit": "23:00-01:00"}})
    assert _rend(env, "{{ tab5_nuits(h) }}", h=h) == attendu


def test_ecran_a_regler():
    env = _regle({}, {"input_text.tab5_choix_ecran_a_regler": "d_tab5"})
    assert _rend(env, "{{ tab5_ecran_a_regler() }}") == "d_tab5"
    env = _regle({}, {"input_text.tab5_choix_ecran_a_regler": "d_disparu"})
    assert _rend(env, "{{ tab5_ecran_a_regler() }}") == "d_ancien", "le premier écran connu"


@pytest.mark.parametrize("actif, ecran, montre", [
    ("", "d_7b", "media_player.chambre"),            # rien de choisi : le premier de SA liste
    ("", "d_tab5", "media_player.tv"),               # le premier de sa liste qui joue
    ("media_player.salon", "d_tab5", "media_player.salon"),
    ("media_player.salon", "d_7b", "media_player.chambre"),   # choisi ailleurs, pas dans sa liste
    ("media_player.cuisine", "d_7b", "media_player.cuisine"),  # d'une tuile, dans aucune liste
    ("media_player.chambre", "", "media_player.chambre"),     # la maison : le choisi, quel qu'il soit
])
def test_lecteur_montre_par_ecran(actif, ecran, montre):
    env = _regle({"d_7b": {"lecteurs": "media_player.chambre"}}, {"input_text.tab5_lecteur_actif": actif})
    assert _rend(env, "{{ tab5_lecteur_montre(ecran) }}", ecran=ecran) == montre
    assert _rend(env, "{{ tab5_lecteurs_tous() }}") == "media_player.salon,media_player.tv,media_player.chambre"


class _Ceci:
    def __init__(self, reglages):
        self.attributes = {} if reglages is None else {"reglages": reglages}


def _stockage():
    for bloc in _paquet("tab5_reglages.yaml")["template"]:
        for capteur in bloc.get("sensor") or []:
            if capteur.get("unique_id") == "tab5_reglages_ecrans":
                assert bloc["triggers"] == [{"trigger": "event", "event_type": "tab5_reglage_ecran"}], \
                    "à déclencheurs : HA restaure son état et ses attributs au redémarrage"
                return capteur
    raise AssertionError("sensor.tab5_reglages_ecrans introuvable")


def test_stockage_des_reglages():
    modele = _stockage()["attributes"]["reglages"]
    env = _env()
    env.globals["dict"] = dict

    def evenement(avant, appareil, cle, valeur):
        trigger = {"event": {"data": {"appareil": appareil, "cle": cle, "valeur": valeur}}}
        return ast.literal_eval(env.from_string(modele).render(this=_Ceci(avant), trigger=trigger).strip())

    r = evenement(None, "d_7b", "presence", "binary_sensor.chambre")
    assert r == {"d_7b": {"presence": "binary_sensor.chambre"}}
    r = evenement(r, "d_7b", "nuit", "23:00-06:00")
    r = evenement(r, "d_tab5", "lecteurs", "media_player.tv")
    assert r == {"d_7b": {"presence": "binary_sensor.chambre", "nuit": "23:00-06:00"},
                 "d_tab5": {"lecteurs": "media_player.tv"}}
    # Valeur vide : la clé part (l'écran reprend la maison), un écran sans clé aussi.
    r = evenement(r, "d_tab5", "lecteurs", "")
    assert r == {"d_7b": {"presence": "binary_sensor.chambre", "nuit": "23:00-06:00"}}
    # Ni un appareil étranger, ni une clé inconnue, ni une entité à la place d'un appareil.
    assert evenement(r, "d_autre", "nuit", "Not set") == r
    assert evenement(r, "d_7b", "theme", "x") == r
    assert evenement(r, "light.tab5_display_backlight", "nuit", "Not set") == r
    # Un écran retiré de HA disparaît au changement suivant.
    assert evenement({"d_parti": {"nuit": "Not set"}}, "d_7b", "nuit", "Not set") == {"d_7b": {"nuit": "Not set"}}


def test_presence_allume_et_attend_par_ecran():
    auto = _automatisation("tab5_push.yaml", "tab5_screen_presence_wifi")
    assert {"trigger": "state", "entity_id": "sensor.tab5_presences_ecrans", "attribute": "absents",
            "id": "absence"} in auto["triggers"]
    env = _env()
    avant = {"attributes": {"presents": ["d_tab5"], "absents": ["d_7b", "d_ancien"]}}
    apres = {"attributes": {"presents": ["d_tab5", "d_7b"], "absents": ["d_ancien"]}}
    trigger = {"id": "presence", "platform": "state", "from_state": avant, "to_state": apres}
    entres = ast.literal_eval(env.from_string(auto["variables"]["entres"]).render(trigger=trigger).strip())
    assert entres == ["d_7b"]
    appareils = env.from_string(auto["variables"]["appareils"]).render(trigger=trigger, entres=entres).strip()
    assert ast.literal_eval(appareils) == ["d_7b"]
    # Le Tab5 se vide : il entre dans les absents, l'attente de 15 min part pour lui seul.
    trigger = {"id": "absence", "platform": "state", "from_state": apres,
               "to_state": {"attributes": {"presents": [], "absents": ["d_ancien", "d_tab5"]}}}
    entres = ast.literal_eval(env.from_string(auto["variables"]["entres"]).render(trigger=trigger).strip())
    assert entres == ["d_tab5"]
    absence = auto["actions"][0]
    assert absence["then"][0]["repeat"]["sequence"][0]["target"] == {"entity_id": "script.tab5_ecran_absence"}
    script = _paquet("tab5_push.yaml")["script"]["tab5_ecran_absence"]
    assert script["mode"] == "parallel" and script["sequence"][0]["timeout"] == "00:15:00"
