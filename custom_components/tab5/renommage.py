# -*- coding: utf-8 -*-
"""Noms anglais des entités de la tablette dans Home Assistant : la table et le plan.

[AI-CONTEXT]
@role Les entités du Tab5 se lisent en anglais dans Home Assistant (retour de husyildiz,
      discussion #278, et de michapixel, forum HA, 24/10/2026 ; décision d'Axel le même
      jour). Le FIRMWARE GARDE SES NOMS : dans ESPHome, le nom d'une entité donne sa clé, son
      identifiant unique dans HA (aioesphomeapi.build_unique_id, `MAC/0/type/Nom`) et
      l'emplacement de sa valeur enregistrée sur la tablette (fnv1_hash_object_id(nom)).
      Le renommer ferait perdre à la tablette ses réglages (thème, réveil, limite de
      charge…) et ferait supprimer par HA les anciennes entrées du registre, historique
      orphelin. La traduction se fait donc dans le registre de HA : un identifiant d'entité
      anglais (l'historique et les statistiques suivent, recorder/entity_registry.py de
      HA) et un nom affiché anglais. __init__.py applique ce plan (réparation
      « migrer_entites », un clic) ; les fichiers HA acceptent les deux identifiants.
@contraintes Module pur (pas d'import de Home Assistant) : testé par pytest sur un PC
      (tests/test_renommage.py). ESPHOME est la seule liste des noms ; ses noms d'avant sont
      ceux du firmware (test). Un identifiant d'entité choisi par l'utilisateur (qui ne finit
      pas par le nom d'origine) et un nom qu'il a donné sont gardés.
@ai_instruction Une entité du firmware à traduire = une ligne ici, (domaine, nom dans le
      firmware) → nom anglais ; jamais changer le `name:` du firmware pour traduire.
"""
from __future__ import annotations

import re
import unicodedata
from dataclasses import dataclass

# (domaine HA, nom d'avant dans le firmware) → nom anglais. Domaine = le premier mot de
# l'identifiant d'entité ; le type d'aioesphomeapi (dans l'identifiant unique) s'en déduit
# par TYPES.
ESPHOME: dict[tuple[str, str], str] = {
    # Réveil et rendez-vous (Tab5/paquets/tab5-alarm.yaml)
    ("switch", "Réveil"): "Alarm",
    ("switch", "Réveil : sonner les jours de repos"): "Alarm: ring on days off",
    ("switch", "Réveil : volume progressif"): "Alarm: gradual volume",
    ("switch", "Réveil : annonce parlée"): "Alarm: spoken briefing",
    ("switch", "Annonce des rendez-vous"): "Appointment announcements",
    ("time", "Réveil : heure"): "Alarm: time",
    ("time", "Réveil : jamais avant"): "Alarm: never before",
    ("time", "Réveil : jamais après"): "Alarm: never after",
    ("number", "Réveil : délai avant l'ouverture"): "Alarm: delay before opening",
    ("number", "Réveil : repos mini après la fermeture"): "Alarm: minimum rest after closing",
    ("number", "Réveil : répétition"): "Alarm: snooze time",
    ("number", "Réveil : durée max de sonnerie"): "Alarm: maximum ring time",
    ("number", "Réveil : volume"): "Alarm: volume",
    ("number", "Rendez-vous : annoncer avant"): "Appointments: announce before",
    ("select", "Réveil : mode"): "Alarm: mode",
    ("select", "Réveil : sonnerie"): "Alarm: ringtone",
    ("select", "Réveil : jours"): "Alarm: days",
    ("text", "Réveil : URL de sonnerie (option)"): "Alarm: ringtone URL (optional)",
    ("sensor", "Prochain réveil"): "Next alarm",
    ("sensor", "Prochain rendez-vous"): "Next appointment",
    ("binary_sensor", "Réveil en cours"): "Alarm ringing",
    ("button", "Réveil : tester"): "Alarm: test",
    ("button", "Réveil : arrêter"): "Alarm: stop",
    ("button", "Réveil : répéter"): "Alarm: snooze",
    # Économie (tab5-economie.yaml)
    ("select", "Tab5 Économie d'énergie"): "Power saving",
    ("select", "Tab5 Animations"): "Animations",
    ("binary_sensor", "Tab5 Sur batterie"): "On battery",
    # Commandes (tab5-ha-controls.yaml)
    ("select", "Langue"): "Language",
    ("select", "Tab5 Extinction auto de l'écran"): "Screen auto-off",
    ("switch", "Tab5 Extinction auto sur batterie seulement"): "Screen auto-off on battery only",
    ("switch", "Tab5 Nuit (écran éteint)"): "Night (screen off)",
    ("switch", "Tab5 Batterie montée"): "Battery fitted",
    ("switch", "Tab5 Fond de l'horloge"): "Clock background",
    ("switch", "Tab5 Appareils sur la météo"): "Devices on the weather",
    ("switch", "Tab5 Rallumer l'écran à Okay Nabu"): "Wake screen on Okay Nabu",
    ("button", "Recharger le calendrier"): "Reload calendar",
    # Matériel (tab5-hardware.yaml, carte-waveshare-7b.yaml, tab5-imu.yaml)
    ("select", "Tab5 DAC Output"): "DAC Output",
    ("media_player", "tab5_media_player"): "Media Player",
    ("button", "Redémarrage Système"): "Restart",
    ("select", "Sens de l'écran"): "Screen direction",
    ("switch", "Tab5 Tap-to-Wake"): "Tap to wake",
    # Navigation et zones (tab5-navigation.yaml, tab5-zones.yaml)
    ("sensor", "Écran courant"): "Current screen",
    ("select", "Aller à l'écran"): "Go to screen",
    ("sensor", "Zones masquées"): "Hidden zones",
    # Diagnostics (tab5-sensors-diagnostics.yaml)
    ("switch", "Tab5 Mesure de consommation"): "Power measurement",
    ("binary_sensor", "Tab5 Batterie en charge"): "Battery charging",
    ("binary_sensor", "Tab5 Batterie détectée"): "Battery detected",
    ("binary_sensor", "Tab5 Sécurité chargeur"): "Charger safety",
    ("sensor", "Tab5 IP"): "IP",
    ("sensor", "Tab5 SSID"): "SSID",
    ("sensor", "Tab5 Raison du redémarrage"): "Restart reason",
    ("sensor", "Tab5 C6 Version"): "C6 Version",
    ("sensor", "Tab5 Préfixe des actions"): "Action prefix",
    ("sensor", "Tab5 Chronologie du démarrage"): "Boot timeline",
    ("sensor", "Tab5 Chronologie du démarrage (suite)"): "Boot timeline (continued)",
    ("sensor", "Tab5 Uptime"): "Uptime",
    ("sensor", "Tab5 WiFi Signal"): "WiFi Signal",
    ("sensor", "Tab5 Core Temp"): "Core Temp",
    ("sensor", "Tab5 Free Ram"): "Free Ram",
    ("sensor", "Tab5 Loop Time"): "Loop Time",
    ("sensor", "Tab5 Draw Max"): "Draw Max",
    ("sensor", "Tab5 Stack Free Min"): "Stack Free Min",
    ("sensor", "Tab5 Tension batterie"): "Battery voltage",
    ("sensor", "Tab5 Courant batterie"): "Battery current",
    ("sensor", "Tab5 Batterie"): "Battery",
    ("sensor", "Tab5 Consommation"): "Power draw",
    ("select", "Tab5 Limite de charge"): "Charge limit",
    ("select", "Tab5 Mode de charge"): "Charge mode",
    ("select", "Tab5 Wi-Fi éco"): "WiFi power saving",
    # Voix (tab5-sensors-domotique.yaml)
    ("switch", "Tab5 Wake Word Active"): "Wake Word Active",
    # Thèmes (tab5-themes.yaml)
    ("select", "Thème"): "Theme",
    ("select", "Clair ou sombre"): "Light or dark",
    ("switch", "Nuit (thème auto)"): "Night (auto theme)",
}

# Options des listes des packages passées en anglais (décision d'Axel du 24/10/2026 : pour
# les entités des packages, noms et options en anglais, identifiants gardés). Home Assistant
# ne garde l'option choisie que si elle existe encore : l'intégration la traduit au
# rechargement des fichiers et au démarrage (option_anglaise, Gestionnaire._options_gardees).
_OUI_NON = {"Oui": "Yes", "Non": "No"}
OPTIONS_PAQUETS: dict[str, dict[str, str]] = {
    "input_select.tab5_alertes_maj": {"Toutes": "All", "Home Assistant seulement": "Home Assistant only",
                                      "Aucune": "None"},
    "input_select.tab5_alertes_vigilance": {"Jaune": "Yellow", "Rouge": "Red", "Aucune": "None"},
    "input_select.tab5_alertes_problemes": _OUI_NON,
    "input_select.tab5_alertes_indisponibles": _OUI_NON,
    "input_select.tab5_alertes_etiquette": _OUI_NON,
    "input_select.tab5_alertes_froid": _OUI_NON,
    "input_select.tab5_alertes_piles": {"Aucune": "None"},
    "input_select.tab5_serveur_ia_type": {"Aucun": "None"},
    "input_select.tab5_source_pluie": {"Aucune": "None"},
    "input_select.tab5_source_vigilance": {"Aucune": "None"},
}


def option_anglaise(entity_id: str, option: str) -> str | None:
    """Option anglaise d'une option d'avant d'une liste des packages, ou None (déjà
    anglaise, nom propre, liste hors table)."""
    return OPTIONS_PAQUETS.get(entity_id, {}).get(option)


# Domaine HA → type d'entité d'aioesphomeapi dans l'identifiant unique (_TYPE_TO_NAME) :
# seuls les domaines de la table. Un capteur texte est un « sensor » dans HA mais un
# « text_sensor » dans l'identifiant ; un texte, « text_info » (lu dans le registre de
# l'auteur le 24/10/2026 : `MAC/0/text_info/Réveil : URL de sonnerie (option)`).
TYPES: dict[str, tuple[str, ...]] = {
    "switch": ("switch",),
    "time": ("time",),
    "number": ("number",),
    "select": ("select",),
    "text": ("text_info",),
    "sensor": ("sensor", "text_sensor"),
    "binary_sensor": ("binary_sensor",),
    "button": ("button",),
    "media_player": ("media_player",),
}


def slug(texte: str) -> str:
    """Comme homeassistant.util.slugify pour nos noms : accents retirés, minuscules, « _ »."""
    ascii_ = unicodedata.normalize("NFKD", texte).encode("ascii", "ignore").decode("ascii")
    return re.sub(r"[^a-z0-9]+", "_", ascii_.lower()).strip("_")


def objet_esphome(nom: str) -> str:
    """object_id d'ESPHome (identifiant unique version 1) : snake_case puis sanitize."""
    return re.sub(r"[^a-zA-Z0-9_-]", "_", nom.lower().replace(" ", "_"))


def nom_firmware(domaine: str, uid: str) -> str | None:
    """Nom de l'entité dans le firmware, lu dans son identifiant unique, s'il est dans la table.

    Version 3 `MAC/sous-appareil/type/Nom` (HA 2026.10, registre de l'auteur le
    24/10/2026) ; versions 1 `MAC-type-objet` et 2 `MAC-type-Nom` d'un HA plus ancien, avec
    « @id » pour un sous-appareil. Le type doit aller avec le domaine : deux entités de types
    différents peuvent porter le même nom."""
    v3 = re.fullmatch(r"[^/]+/[^/]+/([a-z_]+)/(.+)", uid)
    if v3:
        type_, nom = v3.groups()
        return nom if type_ in TYPES.get(domaine, ()) and (domaine, nom) in ESPHOME else None
    v1 = re.fullmatch(r".+?-([a-z_]+)-(.+?)(@[^@]*)?", uid)
    if not v1 or v1.group(1) not in TYPES.get(domaine, ()):
        return None
    reste = v1.group(2)
    for dom, ancien in ESPHOME:
        if dom == domaine and reste in (ancien, objet_esphome(ancien)):
            return ancien
    return None


def nouveaux_entity_ids(entity_id: str, ancien: str, neuf: str) -> tuple[str, ...]:
    """Identifiants possibles avec le nom anglais, dans l'ordre de préférence ; () s'il ne
    finit pas par le nom d'origine (choisi par l'utilisateur : gardé) ou ne change pas.

    Home Assistant le compose « domaine.<appareil>_<nom> », puis « _2 »… quand il est déjà
    pris. Seule la fin change, le préfixe de l'appareil reste ; le « _2 » tombe si
    l'identifiant sans lui est libre (la 7B de l'auteur en avait 19, hérités d'un écran
    retiré), sinon il reste (un second écran)."""
    domaine, objet = entity_id.split(".", 1)
    # Le préfixe de l'appareil n'est jamais vide (has_entity_name) : sans lui,
    # « sensor.tab5_uptime » déjà migré finirait par « tab5_uptime », le nom d'avant de
    # « Tab5 Uptime », et partirait vers « sensor.uptime » (vu par le banc de HA, 24/10/2026).
    m = re.fullmatch(r"(.+?_)" + re.escape(slug(ancien)) + r"(_[0-9]+)?", objet)
    if not m:
        return ()
    base = f"{domaine}.{m.group(1)}{slug(neuf)}"
    candidats = (base, base + m.group(2)) if m.group(2) else (base,)
    return () if entity_id in candidats else candidats


def nouvel_entity_id(entity_id: str, ancien: str, neuf: str) -> str | None:
    """Le premier identifiant de nouveaux_entity_ids, ou None."""
    candidats = nouveaux_entity_ids(entity_id, ancien, neuf)
    return candidats[0] if candidats else None


@dataclass(frozen=True)
class Renommage:
    entity_id: str
    nouveaux_entity_ids: tuple[str, ...]  # par préférence ; () : l'identifiant ne change pas
    nom: str | None  # nom affiché anglais ; None : déjà là, ou un nom donné par l'utilisateur


def planifier(entrees: list[tuple[str, str, str | None]]) -> list[Renommage]:
    """Plan pour les entrées ESPHome d'une tablette : [(entity_id, id unique, nom du
    registre), …]. Le nom du registre (None quand l'utilisateur n'en a pas donné) prend le
    nom anglais ; un nom donné par l'utilisateur reste. Une entrée déjà migrée ne donne
    rien : la réparation disparaît une fois tout fait. Les identifiants sans « _N » passent
    d'abord : ils prennent l'identifiant anglais nu avant un second écran."""
    plan: list[Renommage] = []
    for eid, uid, nom_registre in sorted(entrees, key=lambda e: (bool(re.search(r"_[0-9]+$", e[0])), e[0])):
        domaine = eid.split(".", 1)[0]
        ancien = nom_firmware(domaine, uid)
        if ancien is None:
            continue
        neuf = ESPHOME[(domaine, ancien)]
        cibles = nouveaux_entity_ids(eid, ancien, neuf)
        nom = neuf if nom_registre is None else None
        if cibles or nom:
            plan.append(Renommage(eid, cibles, nom))
    return plan

