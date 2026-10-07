# -*- coding: utf-8 -*-
"""Tab5 — les fichiers Home Assistant du projet, installés et mis à jour en un clic par HACS.

[AI-CONTEXT]
@role Intégration distribuée par HACS (dépôt personnalisé ; hacs.json à la racine du dépôt,
      ADR-0035). Chaque release du firmware joint tab5_hacs.zip
      (tools/publication/archive_hacs.py) : ce code, plus le dossier fichiers/ = les fichiers
      de tab5_home_assistant.zip de la MÊME release. Une mise à jour = HACS remplace
      custom_components/tab5/, puis Home Assistant redémarre ; au démarrage, si la version
      embarquée n'est pas celle déjà posée, l'intégration :
        1. sauvegarde puis remplace les fichiers (installation.py) ;
        2. vérifie la configuration (comme « Vérifier la configuration ») et remet tout en
           place si les nouveaux fichiers la cassent (réparation « configuration_invalide ») ;
        3. charge les domaines que HA n'avait pas encore (rest_command… sur un HA neuf),
           puis recharge tout le YAML (homeassistant.reload_all) : pas de 2e redémarrage ;
        4. constate l'effet : le capteur « Tab5 · version des fichiers HA »
           (packages/tab5_health.yaml) donne la nouvelle version ; sinon une réparation dit
           quoi faire (ligne `packages:` absente, ou redémarrage) ;
        5. prévient (notification), puis, si l'option est cochée, lance la mise à jour de la
           tablette dès que son entité « Firmware » propose CETTE version (même release).
@contraintes L'ordre fichiers puis firmware est le contrat du projet
      (docs/installation/updates.md) : le firmware n'est jamais lancé avant que l'étape 4 ait
      constaté la nouvelle version. Rien ici n'écrit hors de config/packages,
      custom_templates, blueprints et tab5_sauvegardes ; retirer l'intégration laisse les
      fichiers en place.
@ai_instruction La logique de fichiers reste dans installation.py (pure, testée par pytest
      sans HA) ; ici seulement ce qui parle à Home Assistant. Le test bout à bout est
      tools/installation_ha/verifier_integration.py (vrai HA en conteneur, CI
      integration-hacs.yml).
"""
from __future__ import annotations

import asyncio
import logging
from pathlib import Path
from typing import Any

from homeassistant import config as conf_util
from homeassistant.components import persistent_notification
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import EVENT_STATE_CHANGED
from homeassistant.core import Event, EventStateChangedData, HomeAssistant, callback
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers import device_registry as dr
from homeassistant.helpers import entity_registry as er
from homeassistant.helpers import issue_registry as ir
from homeassistant.helpers.start import async_at_started
from homeassistant.helpers.storage import Store
from homeassistant.loader import async_get_integration
from homeassistant.setup import async_setup_component
from homeassistant.util import dt as dt_util

from . import installation, messages
from .const import (
    ATTENTE_CAPTEUR_S,
    CAPTEUR_VERSION,
    CONF_FIRMWARE,
    DOMAIN,
    DOSSIER_FICHIERS,
    GARDER_SAUVEGARDES,
    ISSUE_CONFIGURATION,
    ISSUE_FICHIERS_ABSENTS,
    ISSUE_PACKAGES,
    ISSUE_REDEMARRAGE,
    MODELE_TABLETTE,
    NOTIFICATION,
    PACKAGE_TEMOIN,
    STOCKAGE_CLE,
    STOCKAGE_VERSION,
)

_LOGGER = logging.getLogger(__name__)

type Tab5ConfigEntry = ConfigEntry[Gestionnaire]


async def async_setup_entry(hass: HomeAssistant, entry: Tab5ConfigEntry) -> bool:
    integration = await async_get_integration(hass, DOMAIN)
    gestionnaire = Gestionnaire(hass, entry, str(integration.version))
    await gestionnaire.async_charger()
    entry.runtime_data = gestionnaire
    entry.async_on_unload(hass.bus.async_listen(
        EVENT_STATE_CHANGED, gestionnaire.sur_changement, event_filter=_est_une_mise_a_jour))
    entry.async_on_unload(entry.add_update_listener(_options_changees))

    async def _au_demarrage(_hass: HomeAssistant) -> None:
        await gestionnaire.async_installer()

    entry.async_on_unload(async_at_started(hass, _au_demarrage))
    return True


async def async_unload_entry(hass: HomeAssistant, entry: Tab5ConfigEntry) -> bool:
    return True


async def async_remove_entry(hass: HomeAssistant, entry: ConfigEntry) -> None:
    """Intégration retirée : les fichiers restent (la configuration de l'utilisateur), sa
    mémoire part, et une nouvelle installation repartira comme la première fois."""
    await Store(hass, STOCKAGE_VERSION, STOCKAGE_CLE).async_remove()
    for cle in (ISSUE_FICHIERS_ABSENTS, ISSUE_CONFIGURATION, ISSUE_PACKAGES, ISSUE_REDEMARRAGE):
        ir.async_delete_issue(hass, DOMAIN, cle)


async def _options_changees(hass: HomeAssistant, entry: Tab5ConfigEntry) -> None:
    await entry.runtime_data.async_verifier_firmware(rafraichir=True)


@callback
def _est_une_mise_a_jour(donnees: EventStateChangedData) -> bool:
    return donnees["entity_id"].startswith("update.")


class Gestionnaire:
    """Installe les fichiers d'une version, une seule fois, puis suit le firmware."""

    def __init__(self, hass: HomeAssistant, entry: Tab5ConfigEntry, version: str) -> None:
        self.hass = hass
        self.entry = entry
        self.version = version
        self.config = Path(hass.config.config_dir)
        self.dossier = Path(__file__).parent / DOSSIER_FICHIERS
        self._store: Store[dict[str, Any]] = Store(hass, STOCKAGE_VERSION, STOCKAGE_CLE)
        self.donnees: dict[str, Any] = {}
        self._verrou = asyncio.Lock()

    async def async_charger(self) -> None:
        self.donnees = await self._store.async_load() or {}

    async def _sauver(self) -> None:
        await self._store.async_save(self.donnees)

    @property
    def firmware_auto(self) -> bool:
        return bool(self.entry.options.get(CONF_FIRMWARE, True))

    # ── Fichiers ────────────────────────────────────────────────────────────

    async def async_installer(self, forcer: bool = False) -> None:
        async with self._verrou:
            if forcer or self.donnees.get("version") != self.version:
                await self._installer()
            elif self._version_active():
                # Déjà posés : un redémarrage (ligne `packages:` ajoutée, réparation
                # « redémarrer ») les a rendus actifs, ou une version précédente refusée a
                # laissé place à celle-ci : les réparations n'ont plus lieu d'être.
                for cle in (ISSUE_PACKAGES, ISSUE_REDEMARRAGE, ISSUE_CONFIGURATION, ISSUE_FICHIERS_ABSENTS):
                    ir.async_delete_issue(self.hass, DOMAIN, cle)
        await self.async_verifier_firmware(rafraichir=True)

    def _version_active(self) -> bool:
        etat = self.hass.states.get(CAPTEUR_VERSION)
        return etat is not None and etat.state == self.version

    async def _installer(self) -> None:
        hass, executer = self.hass, self.hass.async_add_executor_job
        embarques = await executer(installation.lire_embarques, self.dossier)
        if not embarques:
            self._probleme(ISSUE_FICHIERS_ABSENTS, ir.IssueSeverity.ERROR, {"version": self.version})
            return
        ir.async_delete_issue(hass, DOMAIN, ISSUE_FICHIERS_ABSENTS)

        plan = await executer(installation.planifier, self.config, embarques,
                              self.donnees.get("fichiers", {}))
        avant = await executer(installation.version_installee, self.config)
        erreurs_avant = await conf_util.async_check_ha_config_file(hass)
        nom = installation.etiquette(dt_util.now(), avant)
        try:
            sauvegarde = await executer(installation.appliquer, self.config, plan, nom)
        except OSError as err:  # appliquer() a déjà tout remis comme avant
            _LOGGER.error("Fichiers Tab5 %s : écriture impossible dans %s (%s), rien n'a changé",
                          self.version, self.config, err)
            self._probleme(ISSUE_CONFIGURATION, ir.IssueSeverity.ERROR, {"version": self.version})
            return
        relatif = sauvegarde.relative_to(self.config).as_posix() if sauvegarde else None

        # 2. La configuration, comme « Vérifier la configuration » : une erreur nouvelle
        # vient de ces fichiers, tout est remis comme avant.
        erreurs = await conf_util.async_check_ha_config_file(hass)
        if erreurs and erreurs != erreurs_avant:
            await executer(installation.restaurer, self.config, plan, sauvegarde)
            await executer(installation.nettoyer_sauvegardes, self.config, GARDER_SAUVEGARDES)
            _LOGGER.error("Fichiers Tab5 %s refusés par la vérification de la configuration, "
                          "anciens fichiers remis : %s", self.version, erreurs)
            self._probleme(ISSUE_CONFIGURATION, ir.IssueSeverity.ERROR, {"version": self.version})
            return
        ir.async_delete_issue(hass, DOMAIN, ISSUE_CONFIGURATION)
        _LOGGER.info("Fichiers Tab5 %s posés (%d écrits, %d retirés, %d identiques), sauvegarde : %s",
                     self.version, len(plan.ecrire), len(plan.retirer), len(plan.identiques), relatif)

        # 3. Domaines absents (HA neuf), puis tout le YAML rechargé.
        redemarrer = bool(erreurs)  # configuration déjà invalide avant : reload_all refuserait
        manquants = sorted(installation.domaines(plan.contenus) - hass.config.components)
        if manquants and not erreurs:
            config_yaml = await self._config_yaml()
            for domaine in manquants:
                if config_yaml is None or not await async_setup_component(hass, domaine, config_yaml):
                    _LOGGER.warning("Tab5 : %s pas chargé sans redémarrage", domaine)
                    redemarrer = True
        if not erreurs:
            try:
                await hass.services.async_call("homeassistant", "reload_all", blocking=True)
            except HomeAssistantError as err:
                _LOGGER.warning("Tab5 : rechargement du YAML refusé (%s)", err)
                redemarrer = True

        # 4. L'effet : HA lit la nouvelle version des fichiers.
        actifs = await self._attendre_version()
        packages_absents = False
        if not actifs:
            config_yaml = await self._config_yaml() or {}
            charges = (config_yaml.get("homeassistant") or {}).get("packages") or {}
            packages_absents = PACKAGE_TEMOIN not in charges
            redemarrer = redemarrer or not packages_absents
        if packages_absents:
            self._probleme(ISSUE_PACKAGES, ir.IssueSeverity.ERROR, {})
        else:
            ir.async_delete_issue(hass, DOMAIN, ISSUE_PACKAGES)
        if redemarrer and not packages_absents:
            self._probleme(ISSUE_REDEMARRAGE, ir.IssueSeverity.WARNING, {"version": self.version},
                           reparable=True)
        else:
            ir.async_delete_issue(hass, DOMAIN, ISSUE_REDEMARRAGE)

        self.donnees.update(version=self.version, fichiers=plan.installes,
                            date=dt_util.now().isoformat(), sauvegarde=relatif)
        if self.firmware_auto:
            # Attendu même si les fichiers ne sont pas encore actifs : le firmware ne part
            # qu'une fois le capteur à cette version (async_verifier_firmware), par exemple
            # après le redémarrage demandé par une réparation.
            self.donnees["firmware_attendu"] = self.version
        await self._sauver()
        await executer(installation.nettoyer_sauvegardes, self.config, GARDER_SAUVEGARDES)

        firmware = "non" if packages_absents else ("auto" if self.firmware_auto else "manuel")
        titre, message = messages.installation(
            hass.config.language, avant=avant, version=self.version, ecrits=len(plan.ecrire),
            retires=len(plan.retirer), identiques=len(plan.identiques), sauvegarde=relatif,
            modifies=plan.modifies, redemarrer=redemarrer, packages_absents=packages_absents,
            firmware=firmware)
        persistent_notification.async_create(hass, message, titre, NOTIFICATION)

    async def _config_yaml(self) -> dict[str, Any] | None:
        """configuration.yaml lu comme au démarrage, packages fusionnés ; None si illisible."""
        try:
            return await conf_util.async_hass_config_yaml(self.hass)
        except HomeAssistantError as err:
            _LOGGER.warning("Tab5 : configuration illisible (%s)", err)
            return None

    async def _attendre_version(self) -> bool:
        """Le capteur des packages donne-t-il la version embarquée ? (quelques secondes :
        les entités de template se recréent après le rechargement)."""
        for _ in range(ATTENTE_CAPTEUR_S * 2):
            if self._version_active():
                return True
            await asyncio.sleep(0.5)
        etat = self.hass.states.get(CAPTEUR_VERSION)
        _LOGGER.warning("Tab5 : %s vaut %s, %s attendu", CAPTEUR_VERSION,
                        etat.state if etat else "absent", self.version)
        return False

    def _probleme(self, cle: str, gravite: ir.IssueSeverity, valeurs: dict[str, str],
                  reparable: bool = False) -> None:
        ir.async_create_issue(self.hass, DOMAIN, cle, is_fixable=reparable, severity=gravite,
                              translation_key=cle, translation_placeholders=valeurs)

    # ── Firmware ────────────────────────────────────────────────────────────

    def _entites_firmware(self) -> list[str]:
        """Entités de mise à jour ESPHome de chaque tablette (modèle tab5-ha-hmi)."""
        appareils = dr.async_get(self.hass)
        entites = []
        for e in er.async_get(self.hass).entities.values():
            if e.domain != "update" or e.platform != "esphome" or e.disabled or not e.device_id:
                continue
            appareil = appareils.async_get(e.device_id)
            if appareil is not None and appareil.model == MODELE_TABLETTE:
                entites.append(e.entity_id)
        return entites

    @callback
    def sur_changement(self, event: Event[EventStateChangedData]) -> None:
        if self.donnees.get("firmware_attendu") and event.data["entity_id"] in self._entites_firmware():
            self.hass.async_create_task(self.async_verifier_firmware())

    async def async_verifier_firmware(self, rafraichir: bool = False) -> None:
        """Lance la mise à jour de la tablette quand son entité propose la version des
        fichiers, et seulement une fois ces fichiers actifs. Sinon attend (sur_changement)."""
        attendu = self.donnees.get("firmware_attendu")
        if not attendu or not self.firmware_auto or attendu != self.version or not self._version_active():
            return
        entites = self._entites_firmware()
        for entite in entites:
            etat = self.hass.states.get(entite)
            if etat is None:
                continue
            installee = etat.attributes.get("installed_version")
            derniere = etat.attributes.get("latest_version")
            if installee == attendu:
                self.donnees.pop("firmware_attendu", None)
                await self._sauver()
                return
            if derniere == attendu and etat.state == "on" and not etat.attributes.get("in_progress"):
                self.donnees.pop("firmware_attendu", None)  # avant tout await : une seule fois
                await self._sauver()
                _LOGGER.info("Tab5 : fichiers %s actifs, mise à jour de %s lancée", attendu, entite)
                await self.hass.services.async_call("update", "install", {"entity_id": entite},
                                                    blocking=False)
                titre, message = messages.firmware_lance(self.hass.config.language, attendu)
                persistent_notification.async_create(self.hass, message, titre, f"{NOTIFICATION}_firmware")
                return
        if rafraichir and entites:
            # Le manifeste n'est relu que toutes les 6 h : demander une vérification tout de
            # suite (ESPHome : commande CHECK) ; la réponse arrive par sur_changement.
            await self.hass.services.async_call("homeassistant", "update_entity",
                                                {"entity_id": entites}, blocking=False)
