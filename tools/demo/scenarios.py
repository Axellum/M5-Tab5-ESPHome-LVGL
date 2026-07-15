# -*- coding: utf-8 -*-
"""tools/demo/scenarios.py — Données synthétiques et constructeurs de payload pour le mode démo Tab5.

Module pur (stdlib uniquement, aucune dépendance externe) pour rester
vérifiable sans matériel ni `aioesphomeapi` installé (cf. `demo_pusher.py --dry-run`).

Le contrat exact (nombre de champs, délimiteurs, valeurs acceptées) vient de la
lecture directe de Tab5/tab5-api-logic.yaml et Tab5/tab5_custom.cpp — voir
docs/demo_mode.md pour le détail et les sources. Ne pas modifier ces règles ici
sans revérifier contre le firmware réel.
"""
from __future__ import annotations

import datetime as _dt
from dataclasses import dataclass, field

# ---------------------------------------------------------------------------
# Entités "miroir" (platform: homeassistant, Tab5/tab5-sensors-domotique.yaml).
# Clés = mêmes noms que Tab5/user_entities.example.yaml ; le script démo est
# fait pour être utilisé avec ce fichier d'exemple tel quel (pas de vrai HA).
# ---------------------------------------------------------------------------

MIRROR_ENTITIES: dict[str, str] = {
    "entity_tracker_pc": "device_tracker.your_pc",
    "entity_phone_battery": "sensor.your_phone_battery",
    "entity_light_chambre": "light.your_bedroom_light",
    "entity_light_salon": "light.your_living_room_light",
    "entity_light_bureau": "light.your_office_light",
    "entity_temp_salon": "sensor.your_living_room_temperature",
    "entity_hum_salon": "sensor.your_living_room_humidity",
    "entity_temp_chambre": "sensor.your_bedroom_temperature",
    "entity_hum_chambre": "sensor.your_bedroom_humidity",
    "entity_temp_plante": "sensor.your_plant_area_temperature",
    "entity_plante_1": "sensor.your_plant_1_moisture",
    "entity_plante_2": "sensor.your_plant_2_moisture",
    "entity_plante_3": "sensor.your_plant_3_moisture",
    "entity_plante_4": "sensor.your_plant_4_moisture",
    "entity_plante_5": "sensor.your_plant_5_moisture",
}

# entity_id -> valeur d'état envoyée (texte pour text_sensor "on"/"off"/"home",
# nombre en string pour sensor). Statique pour la session : la variation vient
# des scènes météo/planning qui tournent, pas de ces valeurs domotique.
MIRROR_STATE_VALUES: dict[str, str] = {
    MIRROR_ENTITIES["entity_tracker_pc"]: "home",
    MIRROR_ENTITIES["entity_phone_battery"]: "68",
    MIRROR_ENTITIES["entity_light_chambre"]: "off",
    MIRROR_ENTITIES["entity_light_salon"]: "on",
    MIRROR_ENTITIES["entity_light_bureau"]: "off",
    MIRROR_ENTITIES["entity_temp_salon"]: "21.4",
    MIRROR_ENTITIES["entity_hum_salon"]: "48",
    MIRROR_ENTITIES["entity_temp_chambre"]: "19.8",
    MIRROR_ENTITIES["entity_hum_chambre"]: "52",
    MIRROR_ENTITIES["entity_temp_plante"]: "22.1",
    # Un pot volontairement asséché pour illustrer le tri dynamique des slots plantes
    # (sort_and_update_moisture_slots, tab5-sensors-domotique.yaml).
    MIRROR_ENTITIES["entity_plante_1"]: "61",
    MIRROR_ENTITIES["entity_plante_2"]: "12",
    MIRROR_ENTITIES["entity_plante_3"]: "74",
    MIRROR_ENTITIES["entity_plante_4"]: "45",
    MIRROR_ENTITIES["entity_plante_5"]: "38",
}


def mirror_state_for(entity_id: str) -> str | None:
    """Valeur démo pour une entité miroir, ou None si inconnue (ignorée en silence,
    ex. si le device a été flashé avec un user_entities.yaml personnalisé)."""
    return MIRROR_STATE_VALUES.get(entity_id)


# ---------------------------------------------------------------------------
# Vigilance météo (tab5_maj_alerte_meteo_france, tab5-api-logic.yaml:242-319).
# Exactement 11 champs '|' — strtok_r fusionne les délimiteurs consécutifs, donc
# un champ vide au milieu décale tous les suivants (silencieux). On ne laisse
# donc jamais un champ vide : "Vert" par défaut pour les 9 niveaux de vigilance.
# ---------------------------------------------------------------------------

ALERTE_CHAMPS = (
    "phrase_pluie", "globale", "vent", "inondation", "orages",
    "pluie_inondation", "neige_verglas", "grand_froid",
    "vagues_submersion", "canicule", "avalanches",
)
ALERTE_BUF_OCTETS = 1024  # char buf[1024] — tab5-api-logic.yaml:251 (1023 octets utiles)


def build_alerte_payload(**champs: str) -> str:
    """Construit le payload 11 champs de tab5_maj_alerte_meteo_france."""
    valeurs = []
    for nom in ALERTE_CHAMPS:
        defaut = "" if nom == "phrase_pluie" else "Vert"
        valeurs.append(str(champs.get(nom, defaut)) or defaut)
    payload = "|".join(valeurs)
    taille = len(payload.encode("utf-8"))
    assert taille < ALERTE_BUF_OCTETS, f"payload alerte trop long ({taille} octets >= {ALERTE_BUF_OCTETS})"
    return payload


# ---------------------------------------------------------------------------
# Prévisions horaires / journalières (bulk) : enregistrements séparés par ';',
# champs séparés par '|'. Rejet total et silencieux côté firmware au-delà de
# 2048 octets (tab5_custom.cpp:238-241/283-286) — d'où le pacing en plusieurs
# appels côté prod (repris ici, cf. demo_pusher.py).
# ---------------------------------------------------------------------------

BULK_MAX_OCTETS = 2048
JOURS_FR = ("Dim", "Lun", "Mar", "Mer", "Jeu", "Ven", "Sam")


@dataclass
class HeureForecast:
    idx: int          # 0-14
    heure_texte: str  # "14:00"
    condition: str    # cf. update_meteo_icon() : sunny/cloudy/partlycloudy/rainy/pouring/...
    temp: float
    pluvio: float


@dataclass
class JourForecast:
    idx: int              # 0-14
    nom_jour: str          # "Auj 15", "Mer 16"...
    condition: str
    tmin: float
    tmax: float
    est_repos: bool
    est_dimanche: bool
    est_passe: bool = False
    heures_ouverture: str = ""


def build_heures_bulk_payload(records) -> str:
    parts = [f"{r.idx}|{r.heure_texte}|{r.condition}|{r.temp}|{r.pluvio}" for r in records]
    payload = ";".join(parts) + ";"
    taille = len(payload.encode("utf-8"))
    assert taille <= BULK_MAX_OCTETS, f"payload heures trop long ({taille} octets, rejeté par le firmware)"
    return payload


def build_jours_bulk_payload(records) -> str:
    parts = []
    for r in records:
        parts.append("|".join([
            str(r.idx), r.nom_jour, r.condition, str(r.tmin), str(r.tmax),
            "1" if r.est_repos else "0",
            "1" if r.est_dimanche else "0",
            "1" if r.est_passe else "0",
            r.heures_ouverture,
        ]))
    payload = ";".join(parts) + ";"
    taille = len(payload.encode("utf-8"))
    assert taille <= BULK_MAX_OCTETS, f"payload jours trop long ({taille} octets, rejeté par le firmware)"
    return payload


def _heures_depuis_maintenant(condition: str, temp_base: float, pluvio_base: float) -> tuple:
    """15 tranches horaires (idx 0-14), variation légère autour d'une base."""
    maintenant = _dt.datetime.now()
    records = []
    for i in range(15):
        heure = maintenant + _dt.timedelta(hours=i)
        variation = (i % 5) - 2  # petite oscillation +/-2°C sur la journée
        records.append(HeureForecast(
            idx=i,
            heure_texte=heure.strftime("%H:00"),
            condition=condition,
            temp=round(temp_base + variation, 1),
            pluvio=pluvio_base,
        ))
    return tuple(records)


def _jours_depuis_aujourdhui(condition: str, tmax_base: float, tmin_base: float,
                              jours_repos_supplementaires: frozenset = frozenset()) -> tuple:
    """15 jours (idx 0-14) à partir d'aujourd'hui, week-ends marqués repos par défaut."""
    aujourdhui = _dt.datetime.now()
    records = []
    for i in range(15):
        jour = aujourdhui + _dt.timedelta(days=i)
        est_dimanche = jour.weekday() == 6
        est_weekend = jour.weekday() >= 5
        est_repos = est_weekend or i in jours_repos_supplementaires
        nom = f"Auj {jour.strftime('%d')}" if i == 0 else f"{JOURS_FR[(jour.weekday() + 1) % 7]} {jour.strftime('%d')}"
        variation = (i % 4) - 1
        records.append(JourForecast(
            idx=i,
            nom_jour=nom,
            condition=condition,
            tmin=round(tmin_base + variation, 1),
            tmax=round(tmax_base + variation, 1),
            est_repos=est_repos,
            est_dimanche=est_dimanche,
            heures_ouverture="" if est_repos else "09h00 - 17h30",
        ))
    return tuple(records)


# ---------------------------------------------------------------------------
# Scène : un jeu complet de valeurs pour les 10 services tab5_maj_*.
# ---------------------------------------------------------------------------

@dataclass
class Scene:
    nom: str
    meteo_condition: str
    meteo_temperature: float
    meteo_humidite: float
    probabilites: dict          # uv, gel, neige (str -> int)
    pluie_1h: tuple             # 9 intensités (index 0..8 = 0,5,10,15,20,25,35,45,55 min)
    alerte: dict                # champs -> valeur (cf. ALERTE_CHAMPS), absents = "Vert"
    heures: tuple                # 15 HeureForecast
    jours: tuple                 # 15 JourForecast
    clim: dict                  # target, current, mode, preset, fan, swing (toutes en str)
    volet_etat: str
    planning: tuple              # (ligne1, ligne2)
    info_texte: tuple            # (texte, couleur) — couleur "Orange"/"Rouge" affiche la
                                  # bannière de vigilance fixe du firmware, texte est alors ignoré


SCENES: tuple = (
    Scene(
        nom="Journée ensoleillée",
        meteo_condition="sunny",
        meteo_temperature=27.0,
        meteo_humidite=38.0,
        probabilites={"uv": 6, "gel": 0, "neige": 0},
        pluie_1h=("Temps sec",) * 9,
        alerte={"phrase_pluie": "Pas de pluie prévue"},
        heures=_heures_depuis_maintenant("sunny", 26.0, 0.0),
        jours=_jours_depuis_aujourdhui("sunny", 28.0, 16.0),
        clim={"target": "22.0", "current": "23.5", "mode": "cool", "preset": "eco", "fan": "auto", "swing": "off"},
        volet_etat="Ouvert",
        planning=("Auj. : 09h00-17h30", "Dem. : Repos"),
        info_texte=("Auj. : 09h00-17h30\nDem. : Repos\nApr-dem. : 09h00-17h30", "Blanc"),
    ),
    Scene(
        nom="Pluie + alerte orange",
        meteo_condition="rainy",
        meteo_temperature=14.0,
        meteo_humidite=82.0,
        probabilites={"uv": 1, "gel": 0, "neige": 0},
        pluie_1h=("Pluie faible", "Pluie modérée", "Pluie modérée", "Pluie forte",
                   "Pluie forte", "Pluie modérée", "Pluie faible", "Temps sec", "Temps sec"),
        alerte={
            "phrase_pluie": "Pluie dans 10 mn",
            "globale": "Orange",
            "pluie_inondation": "Orange",
            "orages": "Jaune",
        },
        heures=_heures_depuis_maintenant("lightning-rainy", 13.0, 3.5),
        jours=_jours_depuis_aujourdhui("rainy", 15.0, 10.0),
        clim={"target": "21.0", "current": "20.0", "mode": "heat", "preset": "none", "fan": "low", "swing": "off"},
        volet_etat="En_mouvement",
        planning=("Auj. : Repos", "Dem. : 09h00-17h30"),
        # couleur "Orange" -> bannière de vigilance fixe du firmware (texte ignoré, cf. update_info_text_ui)
        info_texte=("", "Orange"),
    ),
    Scene(
        nom="Jour de repos, plantes à surveiller",
        meteo_condition="partlycloudy",
        meteo_temperature=19.0,
        meteo_humidite=55.0,
        probabilites={"uv": 3, "gel": 0, "neige": 0},
        pluie_1h=("Temps sec",) * 7 + ("Pluie faible", "Pluie faible"),
        alerte={"phrase_pluie": "Averses possibles"},
        heures=_heures_depuis_maintenant("partlycloudy", 18.0, 0.5),
        jours=_jours_depuis_aujourdhui("partlycloudy", 20.0, 12.0, jours_repos_supplementaires=frozenset({0})),
        clim={"target": "20.0", "current": "19.5", "mode": "fan_only", "preset": "none", "fan": "quiet", "swing": "vertical"},
        volet_etat="Ferme",
        planning=("Auj. : Repos", "Dem. : 09h00-17h30"),
        info_texte=("Auj. : Repos\nDem. : 09h00-17h30\nApr-dem. : Repos", "Blanc"),
    ),
)
