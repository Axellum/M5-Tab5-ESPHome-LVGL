# -*- coding: utf-8 -*-
"""Tableau de bord Tab5 — ce qui parle à Home Assistant : trouver les tableaux qui portent le
marqueur de la macro, rendre la macro, enregistrer le résultat (ADR-0075).

[AI-CONTEXT]
@role Les trois gestes dont la mise à jour en un clic a besoin, chacun protégé :
        1. tableaux_tab5 : les tableaux en MODE STOCKAGE dont la configuration porte le
           marqueur (tableau.lire_marqueur). Lu par hass.data[LOVELACE_DATA].dashboards ;
        2. rendre / lire_yaml : la macro rendue par Template, comme Outils de développement →
           Modèle, puis lue comme YAML (le texte rendu est du YAML, pas du JSON) ;
        3. enregistrer : LovelaceStorage.async_save, ce que fait le websocket
           `lovelace/config/save` du frontend (lovelace/websocket.py :
           `await config.async_save(msg["config"])`, identique en 2026.8.3, 2026.10.4 et 2026.11.0).
@contraintes API INTERNE de Home Assistant, non publique : LOVELACE_DATA, `.dashboards`, `.mode`,
      `.async_load`, `.async_save`. Elle peut changer à une version mensuelle. Tout accès est
      dans cette seule unité et lève TableauRefuse(ERREUR_API) si elle manque ou ne répond plus
      comme prévu : l'appelant retombe alors sur la notification et les étapes manuelles
      (docs/installation/dashboard.md), sans planter. Aucune création de tableau : la
      collection qui les crée n'est pas exposée, cette étape reste manuelle, une fois.
@ai_instruction Le tableau de bord d'un utilisateur est SA configuration : ici on ne l'écrit
      jamais sans que Gestionnaire.async_mettre_a_jour_tableau ait sauvegardé l'ancien.
"""
from __future__ import annotations

import json
import logging
import time
from dataclasses import dataclass
from typing import Any

from homeassistant.core import HomeAssistant
from homeassistant.helpers.template import Template
from homeassistant.util.yaml import parse_yaml

from . import tableau
from .tableau import ERREUR_API, ERREUR_RENDU, TableauRefuse

_LOGGER = logging.getLogger(__name__)

# Événement que HA publie à chaque enregistrement d'un tableau (EVENT_LOVELACE_UPDATED : défini
# dans lovelace/const.py en 2026.8, dans homeassistant/const.py en 2026.11 — la valeur, elle,
# n'a pas bougé : le texte évite un import qui casserait d'une version à l'autre).
EVENEMENT_TABLEAU_ENREGISTRE = "lovelace_updated"


@dataclass
class Tableau:
    url_path: str | None   # None : le tableau par défaut
    titre: str
    config: dict[str, Any]
    marqueur: tableau.Marqueur
    objet: Any             # LovelaceStorage (API interne de HA) : seulement pour enregistrer


def _tableaux_de_ha(hass: HomeAssistant) -> dict[str | None, Any]:
    try:
        from homeassistant.components.lovelace.const import LOVELACE_DATA
        return dict(hass.data[LOVELACE_DATA].dashboards)
    except Exception as err:  # noqa: BLE001 — import, clé ou attribut : l'API a changé ou Lovelace n'est pas là
        raise TableauRefuse(ERREUR_API, f"hass.data[LOVELACE_DATA].dashboards illisible ({err!r})") from err


async def tableaux_tab5(hass: HomeAssistant) -> list[Tableau]:
    """Les tableaux en mode stockage qui portent le marqueur de la macro. TableauRefuse(api) si
    Lovelace ne répond pas comme prévu (tous les tableaux en erreur, ou aucun accessible)."""
    trouves: list[Tableau] = []
    inattendus: list[str] = []
    tous = _tableaux_de_ha(hass)
    for url_path, objet in tous.items():
        try:
            if getattr(objet, "mode", None) != "storage":
                continue
            config = await objet.async_load(False)
        except Exception as err:  # noqa: BLE001
            # ConfigNotFound : un tableau sans configuration (auto-généré), normal. Le reste
            # (signature changée, attribut disparu) est compté : il dit que l'API a bougé.
            if type(err).__name__ != "ConfigNotFound":
                inattendus.append(f"{url_path}: {err!r}")
            continue
        marqueur = tableau.lire_marqueur(config)
        if marqueur is not None:
            titre = (getattr(objet, "config", None) or {}).get("title") or url_path or "Overview"
            trouves.append(Tableau(url_path, str(titre), config, marqueur, objet))
    if inattendus and not trouves:
        raise TableauRefuse(ERREUR_API, "lecture impossible : " + " | ".join(inattendus[:3]))
    return trouves


async def rendre(hass: HomeAssistant, marqueur: tableau.Marqueur) -> str:
    """La macro du marqueur, rendue avec ses arguments (le texte YAML du tableau)."""
    source, variables = tableau.source_modele(marqueur)
    debut = time.monotonic()
    try:
        texte = Template(source, hass).async_render(variables, parse_result=False)
    except Exception as err:  # noqa: BLE001 — TemplateError, macro absente (TemplateNotFound)…
        raise TableauRefuse(ERREUR_RENDU, f"rendu de {marqueur.macro} impossible ({err})") from err
    _LOGGER.info("Tab5 : %s rendu en %.2f s (%d caractères)", marqueur.macro, time.monotonic() - debut, len(texte))
    return str(texte)


async def lire_yaml(hass: HomeAssistant, texte: str) -> Any:
    """Le YAML rendu, en types simples (dict, list, str…) : ce que le frontend enverrait à
    `lovelace/config/save`. HA lit un YAML 1.1 (PyYAML) comme js-yaml côté navigateur pour tout
    ce que la macro écrit (aucun `on`, `off`, `yes`, `no` sans guillemets : test_tableau_de_bord)."""
    def _lire() -> Any:
        return json.loads(json.dumps(parse_yaml(texte)))

    try:
        return await hass.async_add_executor_job(_lire)
    except Exception as err:  # noqa: BLE001 — HomeAssistantError du chargeur, JSON impossible
        raise TableauRefuse(ERREUR_RENDU, f"le rendu n'est pas du YAML lisible ({err})") from err


async def enregistrer(cible: Tableau, config: dict[str, Any]) -> None:
    """Enregistre `config` comme le fait le frontend. L'appelant a déjà sauvegardé l'ancien."""
    try:
        await cible.objet.async_save(config)
    except Exception as err:  # noqa: BLE001 — recovery_mode, signature changée…
        raise TableauRefuse(ERREUR_API, f"async_save a échoué ({err!r})") from err
