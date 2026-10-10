# -*- coding: utf-8 -*-
"""Assistant de configuration : les formulaires, côté Home Assistant (ADR-0052, ADR-0053).

[AI-CONTEXT]
@role Les étapes de l'assistant, partagées par deux portes d'entrée :
      - la réparation « configurer_pieces » (repairs.py), ouverte par __init__.py quand les
        fichiers sont actifs et qu'aucune automatisation du blueprint tab5_emplacements
        n'existe : c'est la proposition faite à la première installation ;
      - les options de l'intégration (config_flow.py), case « Assistant » : relançable.
      Étapes : `pieces` (jusqu'à 5 pièces de HA, pré-choisies par assistant.classer_zones),
      `piece` une fois par pièce choisie (nom, tuiles, température, humidité, clim,
      pré-remplis par assistant.proposer_piece), `maison` (les entrées qui ne dépendent pas
      d'une pièce, assistant.proposer_maison ; celles d'une automatisation existante
      d'abord), `agendas` (les listes « Tab5 · … » de packages/tab5_reglages.yaml,
      assistant.proposer_liste ; sautée si le package n'est pas chargé), `recapitulatif`
      (ce qui sera écrit et où), puis l'écriture (assistant.ajouter / remplacer_pieces /
      ecrire_si_inchange), `automation.reload` et le constat (l'automatisation est-elle
      chargée ?), puis `select.select_option` sur chaque liste changée, comme le ferait
      l'utilisateur, et le constat (le select a-t-il pris la valeur ?). Une notification
      dit le résultat, ou donne le YAML à coller quand automations.yaml n'est pas utilisable.
@contraintes Ce module ne décide de rien : toute règle (quelles entités, quelles entrées,
      écrire ou pas) est dans assistant.py, pur et testé sans HA. Rien n'est écrit ni
      réglé avant la validation du récapitulatif ; une automatisation déjà là ne change que
      si la case « mettre à jour » est cochée ; une liste n'est réglée que si sa valeur
      diffère de l'actuelle. Pas de pièce utilisable : l'assistant le dit (abandon
      « aucune_piece ») et n'invente rien.
"""
from __future__ import annotations

import asyncio
import logging
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

import voluptuous as vol

from homeassistant.components import persistent_notification
from homeassistant.components.automation import automations_with_blueprint
from homeassistant.core import HomeAssistant
from homeassistant.helpers import area_registry as ar
from homeassistant.helpers import device_registry as dr
from homeassistant.helpers import entity_registry as er
from homeassistant.helpers import issue_registry as ir
from homeassistant.helpers.selector import (
    AreaSelector,
    BooleanSelector,
    EntitySelector,
    SelectSelector,
    TextSelector,
)
from homeassistant.util import dt as dt_util

from . import assistant, installation, messages
from .const import DOMAIN, ISSUE_ASSISTANT, MODELE_TABLETTE, NOTIFICATION

_LOGGER = logging.getLogger(__name__)

NOTIFICATION_ASSISTANT = f"{NOTIFICATION}_assistant"
# Champs des formulaires (translations/*.json, « data »).
CHAMP_PIECES = "pieces"
CHAMP_MAJ = "mettre_a_jour"
# Les constats après automation.reload et select.select_option : quelques secondes au plus.
ATTENTE_CHARGEE_S = 10
ATTENTE_LISTE_S = 5


async def automatisations_tab5(hass: HomeAssistant, config: Path) -> set[str]:
    """Ids des automatisations chargées par HA depuis un blueprint tab5_emplacements (le
    nôtre ou une copie importée par son URL). Une automatisation sans id compte quand même
    (« ?entity_id »), pour ne jamais en créer une seconde."""
    copies = await hass.async_add_executor_job(installation.copies_du_blueprint, config)
    chemins = {assistant.BLUEPRINT_CHEMIN} | {c.removeprefix("blueprints/automation/") for c in copies}
    ids: set[str] = set()
    for chemin in chemins:
        for entite in automations_with_blueprint(hass, chemin):
            etat = hass.states.get(entite)
            id_ = etat.attributes.get("id") if etat is not None else None
            ids.add(str(id_) if id_ is not None else f"?{entite}")
    return ids


@dataclass
class Inventaire:
    """Ce que les registres de HA disent de la maison, pour assistant.py."""
    zones: list[assistant.Zone] = field(default_factory=list)
    appareils: dict[str, assistant.Appareil] = field(default_factory=dict)
    entites: list[assistant.Entite] = field(default_factory=list)
    parzone: dict[str, list[assistant.Entite]] = field(default_factory=dict)
    noms: dict[str, str] = field(default_factory=dict)


def inventaire(hass: HomeAssistant) -> Inventaire:
    inv = Inventaire()
    inv.zones = [assistant.Zone(a.id, a.name) for a in ar.async_get(hass).async_list_areas()]
    inv.appareils = {d.id: assistant.Appareil(d.id, d.area_id, d.model)
                     for d in dr.async_get(hass).devices.values()}
    for e in er.async_get(hass).entities.values():
        etat = hass.states.get(e.entity_id)
        nom = etat.name if etat is not None else (e.name or e.original_name or e.entity_id)
        inv.noms[e.entity_id] = nom
        inv.entites.append(assistant.Entite(
            entity_id=e.entity_id, nom=nom, zone=e.area_id, appareil=e.device_id,
            classe=e.device_class or e.original_device_class, desactivee=e.disabled, cachee=e.hidden,
            categorie=str(e.entity_category) if e.entity_category else None, presente=etat is not None,
            plateforme=e.platform))
    # Les entités hors registre (YAML sans unique_id) ont un nom aussi, pour les listes.
    for etat in hass.states.async_all():
        inv.noms.setdefault(etat.entity_id, etat.name)
    inv.parzone = assistant.par_zone(inv.entites, inv.appareils, MODELE_TABLETTE)
    return inv


def nom_esphome(hass: HomeAssistant) -> str | None:
    """Le nom ESPHome de la tablette (donnée « device_name » de son entrée ESPHome), s'il
    n'y en a qu'une."""
    tablettes = [d for d in dr.async_get(hass).devices.values() if d.model == MODELE_TABLETTE]
    if len(tablettes) != 1:
        return None
    for entry_id in tablettes[0].config_entries:
        entree = hass.config_entries.async_get_entry(entry_id)
        if entree is not None and entree.domain == "esphome" and entree.data.get("device_name"):
            return str(entree.data["device_name"])
    return None


def pipeline_prefere(hass: HomeAssistant) -> str | None:
    """Le nom du pipeline préféré de HA (Paramètres → Assistants vocaux), ou None."""
    try:
        from homeassistant.components.assist_pipeline import async_get_pipeline
        return async_get_pipeline(hass).name
    except Exception:  # noqa: BLE001 — assist_pipeline absent ou sans pipeline : pas de proposition
        return None


def _champ(cle: str, valeur: Any) -> vol.Optional:
    """Un champ facultatif pré-rempli (suggested_value : l'utilisateur peut le vider)."""
    if valeur in (None, "", [], ()):
        return vol.Optional(cle)
    return vol.Optional(cle, description={"suggested_value": list(valeur) if isinstance(valeur, tuple) else valeur})


def _capteur(classe: str) -> EntitySelector:
    return EntitySelector({"filter": [{"domain": "sensor", "device_class": classe}]})


def _selecteur(nature: str) -> Any:
    """Le sélecteur d'une entrée de CHAMPS_MAISON d'après sa nature."""
    if nature == "bool":
        return BooleanSelector()
    if nature == "texte":
        return TextSelector()
    domaine, _, classe = nature.partition(":")
    filtre = {"domain": domaine} | ({"device_class": classe} if classe else {})
    return EntitySelector({"filter": [filtre]})


class AssistantFlux:
    """Mixin des étapes de l'assistant ; la classe qui l'hérite fournit _assistant_fin()."""

    hass: HomeAssistant

    async def _assistant_fin(self) -> Any:
        raise NotImplementedError

    async def _situation(self) -> assistant.Situation:
        config = Path(self.hass.config.config_dir)
        ids = await automatisations_tab5(self.hass, config)
        return await self.hass.async_add_executor_job(assistant.situation, config, ids)

    async def async_step_pieces(self, user_input: dict[str, Any] | None = None) -> Any:
        inv = self._inv = inventaire(self.hass)
        classees = assistant.classer_zones(inv.zones, inv.parzone)
        if not classees:
            return self.async_abort(reason="aucune_piece")  # type: ignore[attr-defined]
        self._zones = {z.id: z for z in inv.zones}
        self._maison: dict[str, Any] = {}
        self._changees: dict[str, str] = {}
        self._proposees: set[str] = set()
        erreurs: dict[str, str] = {}
        if user_input is not None:
            choisies = [z for z in dict.fromkeys(user_input.get(CHAMP_PIECES) or []) if z in self._zones]
            if not choisies:
                erreurs[CHAMP_PIECES] = "aucune_piece_choisie"
            elif len(choisies) > assistant.MAX_PIECES:
                erreurs[CHAMP_PIECES] = "trop_de_pieces"
            else:
                self._choisies = choisies
                self._pieces: list[assistant.Piece] = []
                return await self.async_step_piece()
        selecteur = AreaSelector({"multiple": True, "reorder": True,
                                  "entity": [{"domain": list(assistant.DOMAINES_TUILES)}]})
        schema = vol.Schema({_champ(CHAMP_PIECES, [z.id for z in classees[:assistant.MAX_PIECES]]): selecteur})
        return self.async_show_form(  # type: ignore[attr-defined]
            step_id="pieces", data_schema=schema, errors=erreurs,
            description_placeholders={"nombre": str(len(classees)), "max": str(assistant.MAX_PIECES)})

    async def async_step_piece(self, user_input: dict[str, Any] | None = None) -> Any:
        n = len(self._pieces)
        zone = self._zones[self._choisies[n]]
        erreurs: dict[str, str] = {}
        if user_input is not None:
            tuiles = tuple(dict.fromkeys(user_input.get("tuiles") or []))
            if len(tuiles) > assistant.MAX_TUILES:
                erreurs["tuiles"] = "trop_de_tuiles"
            else:
                self._pieces.append(assistant.Piece(
                    zone=zone.id, nom=str(user_input.get("nom") or "").strip(), tuiles=tuiles,
                    temperature=user_input.get("temperature") or None,
                    humidite=user_input.get("humidite") or None, clim=user_input.get("clim") or None))
                if len(self._pieces) < len(self._choisies):
                    return await self.async_step_piece()
                return await self.async_step_maison()
        p = assistant.proposer_piece(zone, self._inv.parzone.get(zone.id, []))
        schema = vol.Schema({
            _champ("nom", p.nom): TextSelector(),
            _champ("tuiles", p.tuiles): EntitySelector({
                "multiple": True, "reorder": True, "filter": [{"domain": list(assistant.DOMAINES_TUILES)}]}),
            _champ("temperature", p.temperature): _capteur("temperature"),
            _champ("humidite", p.humidite): _capteur("humidity"),
            _champ("clim", p.clim): EntitySelector({"filter": [{"domain": "climate"}]}),
        })
        return self.async_show_form(  # type: ignore[attr-defined]
            step_id="piece", data_schema=schema, errors=erreurs,
            description_placeholders={"numero": str(n + 1), "total": str(len(self._choisies)),
                                      "zone": zone.nom, "max": str(assistant.MAX_TUILES)})

    async def async_step_maison(self, user_input: dict[str, Any] | None = None) -> Any:
        if user_input is not None:
            self._maison = assistant.entrees_maison(user_input, self._existantes)
            return await self.async_step_agendas()
        # Une automatisation existante : ses valeurs d'abord (rien ne change sans le dire).
        situation = await self._situation()
        self._existantes: dict[str, Any] = {}
        if situation.action == "mettre_a_jour" and situation.existante:
            self._existantes = (situation.existante.get("use_blueprint") or {}).get("input") or {}
        existantes = self._existantes
        valeurs = assistant.proposer_maison(self._inv.entites, self._inv.appareils, MODELE_TABLETTE,
                                            self._pieces, nom_esphome(self.hass))
        valeurs.update({c: existantes[c] for c, _ in assistant.CHAMPS_MAISON if c in existantes})
        champs: dict[Any, Any] = {}
        for cle, nature in assistant.CHAMPS_MAISON:
            if nature == "bool":
                champs[vol.Optional(cle, default=bool(valeurs.get(cle, False)))] = _selecteur(nature)
            else:
                champs[_champ(cle, valeurs.get(cle))] = _selecteur(nature)
        return self.async_show_form(  # type: ignore[attr-defined]
            step_id="maison", data_schema=vol.Schema(champs))

    def _listes(self) -> list[tuple[assistant.Liste, list[str], str]]:
        """(liste, ses options, sa valeur actuelle) des listes que HA connaît."""
        trouvees = []
        for liste in assistant.LISTES:
            etat = self.hass.states.get(liste.select)
            if etat is not None and isinstance(etat.attributes.get("options"), list):
                trouvees.append((liste, [str(o) for o in etat.attributes["options"]], etat.state))
        return trouvees

    async def async_step_agendas(self, user_input: dict[str, Any] | None = None) -> Any:
        listes = self._listes()
        if not listes:  # package tab5_reglages pas chargé : rien à régler ici
            return await self.async_step_recapitulatif()
        if user_input is not None:
            actuels = {liste.cle: actuel for liste, _, actuel in listes}
            # Un champ vidé garde la valeur actuelle : vider n'efface rien.
            choix = {liste.cle: str(user_input.get(liste.cle) or actuel) for liste, _, actuel in listes}
            self._changees = assistant.changements(choix, actuels)
            self._proposees = {c for c in self._proposees if self._changees.get(c) == self._propositions.get(c)}
            return await self.async_step_recapitulatif()
        infos = assistant.Infos(noms=self._inv.noms,
                                plateformes={e.entity_id: e.plateforme or "" for e in self._inv.entites},
                                classes={e.entity_id: e.classe or "" for e in self._inv.entites},
                                pipeline_prefere=pipeline_prefere(self.hass))
        champs: dict[Any, Any] = {}
        self._propositions: dict[str, str] = {}
        self._proposees = set()
        for liste, options, actuel in listes:
            valeur, proposee = assistant.proposer_liste(liste, options, actuel, infos)
            if proposee:
                self._propositions[liste.cle] = valeur
                self._proposees.add(liste.cle)
            fr = (self.hass.config.language or "").lower().startswith("fr")
            libelles = [{"value": o, "label": (o if fr else "None") if o == assistant.AUCUN
                         else f"{self._inv.noms[o]} ({o})" if self._inv.noms.get(o, o) != o else o}
                        for o in options]
            champs[_champ(liste.cle, valeur)] = SelectSelector({"options": libelles, "mode": "dropdown"})
        return self.async_show_form(  # type: ignore[attr-defined]
            step_id="agendas", data_schema=vol.Schema(champs))

    async def async_step_recapitulatif(self, user_input: dict[str, Any] | None = None) -> Any:
        config = Path(self.hass.config.config_dir)
        situation = await self._situation()
        langue = self.hass.config.language
        if user_input is not None:
            await self._appliquer(config, situation, bool(user_input.get(CHAMP_MAJ)))
            return await self._assistant_fin()
        champs: dict[Any, Any] = {}
        if situation.action == "mettre_a_jour":
            champs[vol.Required(CHAMP_MAJ, default=False)] = BooleanSelector()
        existante = situation.existante or {}
        alias = str(existante.get("alias") or existante.get("id") or "")
        noms = self._inv.noms
        resume = "\n".join((assistant.resume(self._pieces, noms, langue),
                            assistant.resume_maison(self._maison, noms, langue),
                            assistant.resume_listes(self._changees, self._proposees, noms, langue)))
        return self.async_show_form(  # type: ignore[attr-defined]
            step_id="recapitulatif", data_schema=vol.Schema(champs),
            description_placeholders={
                "resume": resume,
                "action": messages.assistant_action(langue, situation.action, alias, situation.raison)})

    async def _appliquer(self, config: Path, situation: assistant.Situation, mettre_a_jour: bool) -> None:
        hass, langue = self.hass, self.hass.config.language
        entrees = {**assistant.entrees_blueprint(self._pieces), **self._maison}
        resultat, raison, a_coller, nouveau, id_ = "rien", situation.raison, "", None, None
        try:
            if situation.action == "creer":
                id_ = assistant.nouvel_id(situation.automatisations, int(time.time() * 1000))
                nouveau = assistant.ajouter(situation.texte, assistant.automatisation(id_, entrees, langue))
            elif situation.action == "mettre_a_jour" and mettre_a_jour and situation.index is not None:
                id_ = str(situation.automatisations[situation.index].get("id"))
                nouveau = assistant.remplacer_pieces(situation.texte, situation.index, entrees)
            elif situation.action == "fichier":
                resultat = "a_coller"
        except assistant.FichierInutilisable as err:
            # Une mise à jour impossible ne devient pas une seconde automatisation à coller.
            resultat, raison = ("a_coller" if situation.action == "creer" else "rien"), str(err)
            _LOGGER.warning("Tab5 : assistant, %s pas écrit (%s)", assistant.FICHIER, err)
        if resultat == "a_coller":
            id_ = assistant.nouvel_id(situation.automatisations, int(time.time() * 1000))
            a_coller = assistant.vers_yaml([assistant.automatisation(id_, entrees, langue)])

        sauvegarde, entite = None, None
        if nouveau is not None:
            etiquette = dt_util.now().strftime("%Y%m%d-%H%M%S")
            try:
                chemin = await hass.async_add_executor_job(
                    assistant.ecrire_si_inchange, config, situation.texte, nouveau, etiquette)
            except (assistant.FichierInutilisable, OSError) as err:
                _LOGGER.warning("Tab5 : assistant, %s pas écrit (%s)", assistant.FICHIER, err)
                raison = str(err)
                if situation.action == "creer":
                    resultat = "a_coller"
                    a_coller = assistant.vers_yaml([assistant.automatisation(id_ or "tab5", entrees, langue)])
            else:
                sauvegarde = chemin.relative_to(config).as_posix() if chemin else None
                await hass.services.async_call("automation", "reload", blocking=True)
                entite = await self._attendre_chargee(id_)
                resultat = ("cree" if situation.action == "creer" else "mis_a_jour") if entite else "non_chargee"
                _LOGGER.info("Tab5 : assistant, automatisation %s %s (%s), sauvegarde : %s",
                             id_, resultat, entite, sauvegarde)
                if entite:
                    ir.async_delete_issue(hass, DOMAIN, ISSUE_ASSISTANT)
        reglees, ratees = await self._regler_listes()
        titre, message = messages.assistant_resultat(langue, resultat, entite=entite, sauvegarde=sauvegarde,
                                                     yaml_a_coller=a_coller, raison=raison,
                                                     listes=reglees, listes_ratees=ratees)
        persistent_notification.async_create(hass, message, titre, NOTIFICATION_ASSISTANT)

    async def _regler_listes(self) -> tuple[list[str], list[str]]:
        """Chaque liste changée, par select.select_option (comme l'utilisateur), puis le
        constat : le select montre-t-il la valeur ? (réglées, ratées), en libellés."""
        langue = self.hass.config.language
        fr = (langue or "").lower().startswith("fr")
        selects = {liste.cle: liste.select for liste in assistant.LISTES}
        reglees, ratees = [], []
        for cle, valeur in self._changees.items():
            libelle = assistant.LIBELLES.get(cle, (cle, cle))[0 if fr else 1]
            texte = f"{libelle} → {self._inv.noms.get(valeur, valeur)}"
            try:
                await self.hass.services.async_call("select", "select_option",
                                                    {"entity_id": selects[cle], "option": valeur}, blocking=True)
            except Exception as err:  # noqa: BLE001 — dit dans la notification, les autres continuent
                _LOGGER.warning("Tab5 : assistant, liste %s pas réglée (%r)", selects[cle], err)
                ratees.append(texte)
                continue
            for _ in range(ATTENTE_LISTE_S * 2):
                etat = self.hass.states.get(selects[cle])
                if etat is not None and etat.state == valeur:
                    break
                await asyncio.sleep(0.5)
            (reglees if etat is not None and etat.state == valeur else ratees).append(texte)
        return reglees, ratees

    async def _attendre_chargee(self, id_: str | None) -> str | None:
        """L'entité de l'automatisation `id_`, une fois chargée par HA (sinon None)."""
        for _ in range(ATTENTE_CHARGEE_S * 2):
            for etat in self.hass.states.async_all("automation"):
                if str(etat.attributes.get("id")) == id_ and etat.state != "unavailable":
                    return etat.entity_id
            await asyncio.sleep(0.5)
        return None
