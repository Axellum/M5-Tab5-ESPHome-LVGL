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
# Emplacements de la maison (lot 6a, ADR-0019) : la tablette ne connaît plus
# d'entité ; HA (le blueprint « Tab5 — emplacements ») lui pousse chaque
# emplacement par tab5_maj_emplacements, « clé|état|valeur;… ». La démo fait de
# même. Clés = table de tab5_maj_emplacements (Tab5/tab5-api-logic.yaml),
# vérifiées par tests/test_demo.py. Valeurs statiques pour la session : la
# variation vient des scènes météo qui tournent.
# ---------------------------------------------------------------------------

EMPLACEMENTS: dict[str, tuple[str, str]] = {
    # clé : (état tel que HA l'envoie, valeur affichée — luminosité 0-255, %, °C…)
    "lumiere_1": ("off", "nan"),
    "lumiere_2": ("on", "180"),
    "lumiere_3": ("off", "nan"),
    "pc": ("on", "nan"),
    "tv": ("off", "nan"),
    "telephone": ("68", "68"),
    "salon": ("21.4", "21.4"),
    "salon_hum": ("48", "48"),
    "serre": ("22.1", "22.1"),
    # Un pot volontairement asséché pour illustrer le tri dynamique des emplacements
    # plantes (sort_and_update_moisture_slots).
    "pot_1": ("61", "61"),
    "pot_2": ("12", "12"),
    "pot_3": ("74", "74"),
    "pot_4": ("45", "45"),
    "pot_5": ("38", "38"),
}
for _n, (_ec, _lux, _temp, _bat) in enumerate(
        [(350, 2400, 21.8, 90), (120, 800, 22.5, 35), (410, 5200, 23.1, 76),
         (280, 1500, 21.2, 18), (300, 3100, 22.0, 64)], start=1):
    EMPLACEMENTS[f"pot_{_n}_ec"] = (str(_ec), str(_ec))
    EMPLACEMENTS[f"pot_{_n}_lux"] = (str(_lux), str(_lux))
    EMPLACEMENTS[f"pot_{_n}_temp"] = (str(_temp), str(_temp))
    EMPLACEMENTS[f"pot_{_n}_bat"] = (str(_bat), str(_bat))


def zone_de(cle: str) -> str:
    """Zone d'une clé d'emplacement : pot_3_ec -> pot_3, salon_hum -> salon."""
    if cle.startswith("pot_"):
        return cle[:5]
    return "salon" if cle == "salon_hum" else cle


def build_emplacements_payload(absentes: frozenset = frozenset()) -> str:
    """tab5_maj_emplacements : tous les emplacements, sauf ceux d'une zone retirée
    (`--maison-minimale` : comme le blueprint, rien n'est poussé pour une case vide)."""
    payload = "".join(f"{cle}|{etat}|{valeur};" for cle, (etat, valeur) in EMPLACEMENTS.items()
                      if zone_de(cle) not in absentes)
    assert len(payload.encode("utf-8")) < 32 * 1024, "au-delà d'un message API ESPHome (32 Kio)"
    return payload


# ---------------------------------------------------------------------------
# Zones optionnelles (lot 5, Tab5/tab5-zones.yaml, ADR-0018) : la tablette envoie
# esphome.tab5_zones, HA répond tab5_maj_zones avec les clés des zones absentes.
# Clés = kCles de Tab5/tab5_zones.cpp (tests/test_demo.py vérifie la concordance).
# ---------------------------------------------------------------------------

# Zones suivies par la tablette, dans l'ordre de l'enum Zone.
ZONES_SUIVIES = ("lumiere_1", "lumiere_2", "lumiere_3", "pc", "tv", "telephone", "salon",
                 "serre", "pot_1", "pot_2", "pot_3", "pot_4", "pot_5")
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
    ordre = list(ZONES_SUIVIES) + list(ZONES_HA)
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
    nom_jour: str          # "Auj", "Mer"… : le jour seul, comme tab5_push.yaml
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
        # Le jour SEUL, sans date, comme HA (tab5_push.yaml) : la tablette le traduit
        # (ha_day_name, tab5_forecast.cpp). « Auj 16 » n'était reconnu par aucune langue
        # et restait en français sur un écran anglais (vu sur le rendu hors tablette).
        nom = "Auj" if i == 0 else JOURS_FR[(jour.weekday() + 1) % 7]
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
