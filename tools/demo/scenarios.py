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
    "entity_tracker_pc": "switch.your_pc_or_device_tracker",
    "entity_tracker_tv": "media_player.your_tv",
    "entity_phone_battery": "sensor.your_phone_battery",
    "entity_light_chambre": "light.your_bedroom_light",
    "entity_light_salon": "light.your_living_room_light",
    "entity_light_bureau": "light.your_office_light",
    "entity_temp_salon": "sensor.your_living_room_temperature",
    "entity_hum_salon": "sensor.your_living_room_humidity",
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
    MIRROR_ENTITIES["entity_tracker_pc"]: "on",
    MIRROR_ENTITIES["entity_tracker_tv"]: "off",
    MIRROR_ENTITIES["entity_phone_battery"]: "68",
    MIRROR_ENTITIES["entity_light_chambre"]: "off",
    MIRROR_ENTITIES["entity_light_salon"]: "on",
    MIRROR_ENTITIES["entity_light_bureau"]: "off",
    MIRROR_ENTITIES["entity_temp_salon"]: "21.4",
    MIRROR_ENTITIES["entity_hum_salon"]: "48",
    MIRROR_ENTITIES["entity_temp_plante"]: "22.1",
    # Un pot volontairement asséché pour illustrer le tri dynamique des slots plantes
    # (sort_and_update_moisture_slots, tab5-sensors-domotique.yaml).
    MIRROR_ENTITIES["entity_plante_1"]: "61",
    MIRROR_ENTITIES["entity_plante_2"]: "12",
    MIRROR_ENTITIES["entity_plante_3"]: "74",
    MIRROR_ENTITIES["entity_plante_4"]: "45",
    MIRROR_ENTITIES["entity_plante_5"]: "38",
}


def mirror_state_for(entity_id: str, absentes: frozenset = frozenset()) -> str | None:
    """Valeur démo pour une entité miroir, ou None si inconnue (ignorée en silence,
    ex. si le device a été flashé avec un user_entities.yaml personnalisé) ou si sa
    zone est retirée (`--maison-minimale` : une entité absente de HA n'envoie rien)."""
    for cle in absentes:
        if MIRROR_ENTITIES.get(ZONE_ENTITES.get(cle, "")) == entity_id:
            return None
    return MIRROR_STATE_VALUES.get(entity_id)


# ---------------------------------------------------------------------------
# Zones optionnelles (lot 5, Tab5/tab5-zones.yaml, ADR-0018) : la tablette envoie
# esphome.tab5_zones, HA répond tab5_maj_zones avec les clés des zones absentes.
# Clés = kCles de Tab5/tab5_zones.cpp (tests/test_demo.py vérifie la concordance).
# ---------------------------------------------------------------------------

# Clé de zone suivie par la tablette -> clé de user_entities (donc de MIRROR_ENTITIES).
ZONE_ENTITES: dict[str, str] = {
    "lumiere_1": "entity_light_chambre",
    "lumiere_2": "entity_light_salon",
    "lumiere_3": "entity_light_bureau",
    "pc": "entity_tracker_pc",
    "tv": "entity_tracker_tv",
    "telephone": "entity_phone_battery",
    "salon": "entity_temp_salon",
    "serre": "entity_temp_plante",
    "pot_1": "entity_plante_1",
    "pot_2": "entity_plante_2",
    "pot_3": "entity_plante_3",
    "pot_4": "entity_plante_4",
    "pot_5": "entity_plante_5",
}
# Zones que seul HA connaît : il les ajoute lui-même à sa réponse.
ZONES_HA = ("clim", "volet", "planning")

# `--maison-minimale` : la même maison que l'essai du 27/09/2026 sur la tablette de
# l'auteur (clim, TV, téléphone, LEDs, serre, pots 3 à 5 retirés), plus le volet et
# le planning. Reste : PC, deux lampes, salon, deux pots.
MAISON_MINIMALE: frozenset = frozenset({
    "lumiere_3", "tv", "telephone", "serre", "pot_3", "pot_4", "pot_5",
    "clim", "volet", "planning",
})


def build_zones_absentes(absentes: frozenset) -> str:
    """Réponse tab5_maj_zones : clés triées dans l'ordre de la tablette."""
    ordre = list(ZONE_ENTITES) + list(ZONES_HA)
    inconnues = set(absentes) - set(ordre)
    assert not inconnues, f"clés de zone inconnues de la tablette : {sorted(inconnues)}"
    return ",".join(c for c in ordre if c in absentes)


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


PLUIE_LIBELLES = ("Temps sec", "Pluie faible", "Pluie modérée", "Pluie forte", "Pluie très forte")


def build_pluie_1h_bulk_payload(intensites) -> str:
    """« idx|intensité;… » pour tab5_maj_pluie_1h_bulk (9 barres, un appel)."""
    assert len(intensites) == 9, f"9 intensités attendues, {len(intensites)} reçues"
    for lib in intensites:
        assert lib in PLUIE_LIBELLES, f"intensité inconnue du firmware : {lib!r}"
    payload = ";".join(f"{i}|{lib}" for i, lib in enumerate(intensites)) + ";"
    assert len(payload.encode("utf-8")) < 256, "payload pluie trop long (tampon firmware 256 octets)"
    return payload


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
            # « HH:MM-HH:MM », comme HA (tab5_push.yaml) : le réveil et le bandeau
            # planning le découpent (alarm_clock.cpp, cal_is_early_shift).
            heures_ouverture="" if est_repos else "09:00-17:30",
        ))
    return tuple(records)


def code_pluie(niveau: int, dans_min: int | None) -> str:
    """Code « @niveau,début » du lot 4c (tab5_central.cpp) : la tablette compose la
    phrase dans sa langue et décompte les minutes. niveau -1 pas de données, 0 sec,
    1 à 4 faible à très forte ; début = epoch UTC, 0 s'il pleut déjà (dans_min None).
    Calculé à l'envoi : un epoch figé à l'import vieillirait pendant la démo."""
    assert -1 <= niveau <= 5, f"niveau de pluie hors contrat : {niveau}"
    if dans_min is None:
        return f"@{niveau},0"
    debut = _dt.datetime.now(_dt.timezone.utc) + _dt.timedelta(minutes=dans_min)
    return f"@{niveau},{int(debut.timestamp())}"


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
    pluie: tuple                # (niveau, dans_min) -> code_pluie(), 1er champ de l'alerte
    alerte: dict                # vigilances (cf. ALERTE_CHAMPS sauf phrase_pluie), absents = "Vert"
    heures: tuple                # 15 HeureForecast
    jours: tuple                 # 15 JourForecast
    clim: dict                  # target, current, mode, preset, fan, swing (toutes en str)
    volet_etat: str
    info_texte: tuple            # (texte, couleur, meteo_id) — texte = code du lot 4c
                                  # « @ha|nb MAJ|titre|nb erreurs|nb indispo|jaune 0/1|vigilance »,
                                  # composé par la tablette dans sa langue (compose_info_code,
                                  # tab5_central.cpp) ; vigilance « orange »/« rouge » affiche la
                                  # bannière de vigilance. meteo_id = identifiant de dismiss (tap sur
                                  # le bandeau) ; vide = bandeau non masquable. Les 3 champs sont
                                  # OBLIGATOIRES : le service tab5_maj_info_texte déclare 3 variables
                                  # et aioesphomeapi lève un KeyError si l'une manque.


SCENES: tuple = (
    Scene(
        nom="Journée ensoleillée",
        meteo_condition="sunny",
        meteo_temperature=27.0,
        meteo_humidite=38.0,
        probabilites={"uv": 6, "gel": 0, "neige": 0},
        pluie_1h=("Temps sec",) * 9,
        pluie=(0, None),
        alerte={},
        heures=_heures_depuis_maintenant("sunny", 26.0, 0.0),
        jours=_jours_depuis_aujourdhui("sunny", 28.0, 16.0),
        clim={"target": "22.0", "current": "23.5", "mode": "cool", "preset": "eco", "fan": "auto", "swing": "off"},
        volet_etat="Ouvert",
        # Rien à signaler côté HA : pas de bandeau info.
        info_texte=("@ha|0||0|0|0|", "Blanc", ""),
    ),
    Scene(
        nom="Pluie + alerte orange",
        meteo_condition="rainy",
        meteo_temperature=14.0,
        meteo_humidite=82.0,
        probabilites={"uv": 1, "gel": 0, "neige": 0},
        pluie_1h=("Pluie faible", "Pluie modérée", "Pluie modérée", "Pluie forte",
                   "Pluie forte", "Pluie modérée", "Pluie faible", "Temps sec", "Temps sec"),
        pluie=(2, 10),  # « Pluie modérée dans 10 mn », décomptée par la tablette
        alerte={
            "globale": "Orange",
            "pluie_inondation": "Orange",
            "orages": "Jaune",
        },
        heures=_heures_depuis_maintenant("lightning-rainy", 13.0, 3.5),
        jours=_jours_depuis_aujourdhui("rainy", 15.0, 10.0),
        clim={"target": "21.0", "current": "20.0", "mode": "heat", "preset": "none", "fan": "low", "swing": "off"},
        volet_etat="En_mouvement",
        # vigilance « orange » -> bannière de vigilance du firmware ; meteo_id non vide :
        # un tap sur le bandeau le masque jusqu'au prochain id différent — c'est la
        # démo de la fonction « tap pour masquer ».
        info_texte=("@ha|0||0|0|0|orange", "Orange", "meteo:orange"),
    ),
    Scene(
        nom="Jour de repos, plantes à surveiller",
        meteo_condition="partlycloudy",
        meteo_temperature=19.0,
        meteo_humidite=55.0,
        probabilites={"uv": 3, "gel": 0, "neige": 0},
        pluie_1h=("Temps sec",) * 7 + ("Pluie faible", "Pluie faible"),
        pluie=(1, 45),
        alerte={},
        heures=_heures_depuis_maintenant("partlycloudy", 18.0, 0.5),
        jours=_jours_depuis_aujourdhui("partlycloudy", 20.0, 12.0, jours_repos_supplementaires=frozenset({0})),
        clim={"target": "20.0", "current": "19.5", "mode": "fan_only", "preset": "none", "fan": "quiet", "swing": "vertical"},
        volet_etat="Ferme",
        # Une mise à jour et deux entités indisponibles : bandeau « 1 MAJ · … · 2 indispo ».
        info_texte=("@ha|1|Home Assistant Core 2026.10.0|0|2|0|", "Orange", ""),
    ),
)
