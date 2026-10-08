# -*- coding: utf-8 -*-
"""Mise à jour enchaînée de la tablette : quand la lancer, la réessayer, la dire ratée.

[AI-CONTEXT]
@role Une fois les fichiers d'une version actifs, __init__.py installe le firmware de la
      MÊME version sur chaque tablette (entité « Firmware », plateforme esphome, modèle
      tab5-ha-hmi) qui le propose. Ce module décide, sans Home Assistant ; __init__.py lit
      les états, appelle update.install et garde la mémoire (Store « tab5.fichiers » :
      `firmware_attendu`, `firmware_essais`).
@contraintes `update.install` d'ESPHome ne fait qu'envoyer la commande à la tablette (HA
      2026.9.4, components/esphome/update.py) : un OTA raté sur la tablette ne se voit qu'à
      sa version installée inchangée. Donc `firmware_attendu` n'est oublié qu'une fois la
      version installée constatée (HA-11 de l'audit du 07/10/2026 : il l'était avant
      l'appel, et rien ne réessayait) ; un essai sans effet après DELAI_ESSAI_S est refait,
      ESSAIS_MAX fois en tout, puis la réparation « firmware_echec » le dit.
      Une entité de mise à jour de la tablette qui ne propose jamais cette version (celle
      du tableau de bord ESPHome, qui suit la version d'ESPHome) n'est jamais lancée, et
      n'empêche pas de conclure.
@ai_instruction Pas d'import de homeassistant ni d'import relatif (pytest le charge seul,
      tests/test_integration_tab5.py). Dates en UTC, au format ISO dans la mémoire (JSON).
"""
from __future__ import annotations

import datetime as dt
from dataclasses import dataclass, field

# Un OTA du Tab5 (téléchargement, écriture, redémarrage) prend quelques minutes : sans
# version installée constatée après ce délai, l'essai est compté comme raté.
DELAI_ESSAI_S = 15 * 60
ESSAIS_MAX = 3


@dataclass(frozen=True)
class Tablette:
    """Ce qui compte de l'état d'une entité « Firmware »."""
    installee: str | None
    proposee: str | None
    disponible: bool  # état « on » : HA propose la mise à jour
    en_cours: bool


@dataclass
class Decision:
    fini: bool = False                                 # oublier firmware_attendu
    lancer: list[str] = field(default_factory=list)    # entités à mettre à jour maintenant
    echecs: list[str] = field(default_factory=list)    # ESSAIS_MAX essais sans effet
    attente_s: float | None = None                     # revenir voir dans … s


def _depuis(essai: dict, maintenant: dt.datetime) -> float:
    try:
        return (maintenant - dt.datetime.fromisoformat(essai["dernier"])).total_seconds()
    except (KeyError, TypeError, ValueError):
        return float("inf")


def decider(attendu: str, tablettes: dict[str, Tablette], essais: dict[str, dict],
            maintenant: dt.datetime) -> Decision:
    """`essais` : {entité : {"n": essais faits, "dernier": date ISO}} (vide au début)."""
    d = Decision()
    attentes: list[float] = []
    faites = 0
    for entite, t in sorted(tablettes.items()):
        essai = essais.get(entite)
        if t.installee == attendu:
            faites += 1
        elif t.en_cours:
            attentes.append(DELAI_ESSAI_S)  # la fin de l'OTA arrivera par son état
        elif essai and (reste := DELAI_ESSAI_S - _depuis(essai, maintenant)) > 0:
            attentes.append(reste)          # lancée il y a peu : lui laisser le temps
        elif essai and essai.get("n", 0) >= ESSAIS_MAX:
            d.echecs.append(entite)
        elif t.proposee == attendu and t.disponible:
            d.lancer.append(entite)
        # Sinon : pas (encore) proposée ; son prochain changement d'état relance la décision.
    # Fini : au moins une tablette a la version, et aucune lancée n'est encore en route.
    lancees_ouvertes = [e for e in essais if e in tablettes and tablettes[e].installee != attendu]
    d.fini = faites > 0 and not d.lancer and not lancees_ouvertes
    d.attente_s = min(attentes) if attentes else None
    return d


def noter_essai(essais: dict[str, dict], entite: str, maintenant: dt.datetime) -> int:
    """Compte un essai lancé ; renvoie son numéro."""
    essai = essais.setdefault(entite, {"n": 0})
    essai["n"] = int(essai.get("n", 0)) + 1
    essai["dernier"] = maintenant.isoformat()
    return essai["n"]
