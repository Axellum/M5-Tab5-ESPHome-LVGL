# -*- coding: utf-8 -*-
"""Tab5 — les fichiers Home Assistant du projet, installés et mis à jour en un clic par HACS.

[AI-CONTEXT]
@role Intégration distribuée par HACS (dépôt personnalisé ; hacs.json à la racine du dépôt,
      ADR-0035). Chaque release du firmware joint tab5_hacs.zip
      (tools/publication/archive_hacs.py) : ce code, plus le dossier fichiers/ = les fichiers
      de tab5_home_assistant.zip de la MÊME release. Une mise à jour = HACS remplace
      custom_components/tab5/, puis Home Assistant redémarre ; au démarrage, si la version
      embarquée n'est pas celle déjà posée, l'intégration :
        1. sauvegarde puis remplace les fichiers (installation.py) ; un fichier du Tab5 déjà
           là sans venir d'elle (copié à la main) et différent est remplacé lui aussi, et dit
           (réparation persistante « fichiers_remplaces » : lesquels, quelle sauvegarde) ;
        2. vérifie la configuration (comme « Vérifier la configuration ») et remet tout en
           place si les nouveaux fichiers y ajoutent un message — erreur OU avertissement :
           pour HA 2026.9, un domaine ou un package invalide n'est qu'un avertissement
           (résultat « valid »), et ce domaine ne se charge plus (réparation
           « configuration_invalide ») ; ces fichiers-là (version + empreinte) ne sont plus
           réessayés aux démarrages suivants, seulement par « Réinstaller » des options ou
           quand HACS en apporte d'autres ; un essai de plus ne refait pas une sauvegarde
           identique à la précédente (installation.appliquer) ;
        3. sans la ligne `packages:`, s'arrête là (réparation « packages_absents ») ; sinon
           recharge les modèles, les entrées (input_*), charge les domaines que HA n'avait
           pas encore (rest_command… sur un HA neuf), puis tout le YAML
           (homeassistant.reload_all) : pas de 2e redémarrage. Dans cet ordre, sinon une
           automatisation lit un modèle ou une entrée pas encore là ;
        4. constate l'effet : le capteur « Tab5 · HA files version »
           (packages/tab5_health.yaml) donne la nouvelle version ; sinon une réparation dit
           quoi faire (ligne `packages:` absente, ou redémarrage) ;
        5. propose l'assistant de configuration (ADR-0052) : fichiers actifs et aucune
           automatisation du blueprint tab5_emplacements → réparation « configurer_pieces »,
           dont le formulaire est l'assistant (assistant_flux.py) ; retirée dès qu'une
           automatisation du blueprint existe ;
        6. prévient (notification), puis, si l'option est cochée, lance la mise à jour de la
           tablette dès que son entité « Firmware » propose CETTE version (même release) ;
           `firmware_attendu` n'est oublié qu'une fois cette version lue sur la tablette, un
           essai sans effet est refait (firmware.py), puis la réparation « firmware_echec »
           le dit ;
        7. cherche les tableaux de bord Tab5 (mode stockage) dont le marqueur de version est
           plus ancien que les fichiers actifs et ouvre, pour chacun, la réparation
           « tableau_a_mettre_a_jour » : un clic le réécrit avec la macro (tableau.py,
           tableau_ha.py, ADR-0075), après avoir gardé l'ancien dans tab5_sauvegardes/tableaux/.
           API interne de HA, protégée : si elle manque, la notification et les étapes
           manuelles restent la voie.
@contraintes L'ordre fichiers puis firmware est le contrat du projet
      (docs/installation/updates.md) : le firmware n'est jamais lancé avant que l'étape 4 ait
      constaté la nouvelle version. Rien ici n'écrit hors de config/packages,
      custom_templates, blueprints et tab5_sauvegardes ; le tableau de bord, lui, n'est
      écrit que par la réparation que l'utilisateur valide (jamais seul) ; retirer
      l'intégration laisse les fichiers en place.
@ai_instruction La logique de fichiers reste dans installation.py (pure, testée par pytest
      sans HA) ; ici seulement ce qui parle à Home Assistant. Le test bout à bout est
      tools/installation_ha/verifier_integration.py (vrai HA en conteneur, CI
      integration-hacs.yml).
"""
from __future__ import annotations

import asyncio
import logging
from collections.abc import Callable
from pathlib import Path
from typing import Any

from homeassistant import config as conf_util
from homeassistant.components import persistent_notification
from homeassistant.config_entries import ConfigEntry
from homeassistant.const import EVENT_STATE_CHANGED
from homeassistant.core import Event, EventStateChangedData, HomeAssistant, callback
from homeassistant.exceptions import HomeAssistantError
from homeassistant.helpers import check_config
from homeassistant.helpers import device_registry as dr
from homeassistant.helpers import entity_registry as er
from homeassistant.helpers import issue_registry as ir
from homeassistant.helpers import restore_state
from homeassistant.helpers.event import async_call_later
from homeassistant.helpers.start import async_at_started
from homeassistant.helpers.storage import Store
from homeassistant.loader import async_get_integration
from homeassistant.setup import async_setup_component
from homeassistant.util import dt as dt_util

from . import assistant_flux, firmware, installation, messages, renommage, tableau, tableau_ha
from .const import (
    ATTENTE_CAPTEUR_S,
    CAPTEUR_VERSION,
    CONF_FIRMWARE,
    DOMAIN,
    DOSSIER_FICHIERS,
    GARDER_SAUVEGARDES,
    ISSUE_ASSISTANT,
    ISSUE_CONFIGURATION,
    ISSUE_FICHIERS_ABSENTS,
    ISSUE_FIRMWARE,
    ISSUE_MIGRATION,
    ISSUE_PACKAGES,
    ISSUE_REDEMARRAGE,
    ISSUE_REMPLACES,
    ISSUE_TABLEAU,
    MODELE_TABLETTE,
    NOTIFICATION,
    PACKAGE_TEMOIN,
    SEPARATEUR_TABLEAU,
    STOCKAGE_CLE,
    STOCKAGE_VERSION,
    URL_SIGNALER,
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
    entry.async_on_unload(gestionnaire.arreter)

    async def _au_demarrage(_hass: HomeAssistant) -> None:
        await gestionnaire.async_options_au_demarrage()
        await gestionnaire.async_installer()
        gestionnaire.proposer_migration()

    entry.async_on_unload(async_at_started(hass, _au_demarrage))

    # Un tableau enregistré (par l'utilisateur, ou par la réparation) : son marqueur a pu changer,
    # la réparation « tableau_a_mettre_a_jour » suit. Le nom de l'événement est un texte : une
    # constante de HA qui changerait de module ne casserait pas l'intégration.
    @callback
    def _tableau_enregistre(_event: Event) -> None:
        hass.async_create_task(gestionnaire.async_evaluer_tableaux())

    entry.async_on_unload(hass.bus.async_listen(tableau_ha.EVENEMENT_TABLEAU_ENREGISTRE, _tableau_enregistre))
    # Une entité ESPHome créée (le firmware anglais vient de parler) : migration à proposer.
    # @callback : sinon HA l'appellerait hors de la boucle, et le registre ne se lit qu'ici.
    @callback
    def _entite_ajoutee(_event: Event[er.EventEntityRegistryUpdatedData]) -> None:
        gestionnaire.proposer_migration()

    entry.async_on_unload(hass.bus.async_listen(
        er.EVENT_ENTITY_REGISTRY_UPDATED, _entite_ajoutee, event_filter=_entite_creee))
    return True


async def async_unload_entry(hass: HomeAssistant, entry: Tab5ConfigEntry) -> bool:
    return True


async def async_remove_entry(hass: HomeAssistant, entry: ConfigEntry) -> None:
    """Intégration retirée : les fichiers restent (la configuration de l'utilisateur), sa
    mémoire part, et une nouvelle installation repartira comme la première fois."""
    await Store(hass, STOCKAGE_VERSION, STOCKAGE_CLE).async_remove()
    for cle in (ISSUE_FICHIERS_ABSENTS, ISSUE_CONFIGURATION, ISSUE_PACKAGES, ISSUE_REDEMARRAGE,
                ISSUE_REMPLACES, ISSUE_FIRMWARE, ISSUE_ASSISTANT, ISSUE_MIGRATION):
        ir.async_delete_issue(hass, DOMAIN, cle)
    # Une réparation par tableau de bord : identifiants construits (ISSUE_TABLEAU + adresse).
    for domaine, cle in list(ir.async_get(hass).issues):
        if domaine == DOMAIN and cle.startswith(ISSUE_TABLEAU + SEPARATEUR_TABLEAU):
            ir.async_delete_issue(hass, DOMAIN, cle)


async def _options_changees(hass: HomeAssistant, entry: Tab5ConfigEntry) -> None:
    await entry.runtime_data.async_verifier_firmware(rafraichir=True)


@callback
def _est_une_mise_a_jour(donnees: EventStateChangedData) -> bool:
    return donnees["entity_id"].startswith("update.")


@callback
def _entite_creee(donnees: er.EventEntityRegistryUpdatedData) -> bool:
    return donnees["action"] == "create"


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
        self._minuterie: Callable[[], None] | None = None
        self._api_tableau_signalee = False

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
                try:
                    await self._installer(forcer)
                except Exception:  # noqa: BLE001 — au journal de l'intégration, pas « never retrieved »
                    _LOGGER.exception("Tab5 : installation des fichiers %s interrompue", self.version)
            elif self._version_active():
                # Déjà posés : un redémarrage (ligne `packages:` ajoutée, réparation
                # « redémarrer ») les a rendus actifs, ou une version précédente refusée a
                # laissé place à celle-ci : les réparations n'ont plus lieu d'être.
                for cle in (ISSUE_PACKAGES, ISSUE_REDEMARRAGE, ISSUE_CONFIGURATION, ISSUE_FICHIERS_ABSENTS):
                    ir.async_delete_issue(self.hass, DOMAIN, cle)
        try:
            await self._proposer_assistant()
        except Exception:  # noqa: BLE001 — une proposition ratée n'empêche pas le firmware
            _LOGGER.exception("Tab5 : proposition de l'assistant de configuration impossible")
        await self.async_evaluer_tableaux()
        await self.async_verifier_firmware(rafraichir=True)

    async def _proposer_assistant(self) -> None:
        """Fichiers actifs (le blueprint est là) et aucune automatisation du blueprint :
        la réparation « configurer_pieces » propose l'assistant. Une automatisation du
        blueprint, quelle qu'elle soit, la retire : rien n'est jamais proposé par-dessus."""
        if not self._version_active():
            return
        if await assistant_flux.automatisations_tab5(self.hass, self.config):
            ir.async_delete_issue(self.hass, DOMAIN, ISSUE_ASSISTANT)
            return
        self._probleme(ISSUE_ASSISTANT, ir.IssueSeverity.WARNING, {}, reparable=True)

    def _version_active(self) -> bool:
        etat = self.hass.states.get(CAPTEUR_VERSION)
        return etat is not None and etat.state == self.version

    async def _installer(self, forcer: bool = False) -> None:
        hass, executer = self.hass, self.hass.async_add_executor_job
        embarques = await executer(installation.lire_embarques, self.dossier)
        if not embarques:
            self._probleme(ISSUE_FICHIERS_ABSENTS, ir.IssueSeverity.ERROR, {"version": self.version})
            return
        ir.async_delete_issue(hass, DOMAIN, ISSUE_FICHIERS_ABSENTS)
        empreinte = await executer(installation.empreinte_des_fichiers, embarques)
        if not forcer and installation.deja_refusee(self.donnees, self.version, empreinte):
            # Déjà refusés tels quels : un nouvel essai à chaque démarrage referait la même
            # vérification, le même retour en arrière et une sauvegarde de plus (HA-9). La
            # réparation, non persistante, est recréée ; « Réinstaller » réessaie.
            _LOGGER.warning("Fichiers Tab5 %s déjà refusés par la vérification de la configuration : "
                            "pas de nouvel essai (options de l'intégration : « Réinstaller »)", self.version)
            self._probleme(ISSUE_CONFIGURATION, ir.IssueSeverity.ERROR,
                           {"version": self.version, "signaler": URL_SIGNALER})
            return

        plan = await executer(installation.planifier, self.config, embarques,
                              self.donnees.get("fichiers", {}))
        avant = await executer(installation.version_installee, self.config)
        _, constats_avant = await self._verifier_configuration()
        nom = installation.etiquette(dt_util.now(), avant)
        try:
            sauvegarde = await executer(installation.appliquer, self.config, plan, nom)
        except OSError as err:  # appliquer() a déjà tout remis comme avant
            _LOGGER.error("Fichiers Tab5 %s : écriture impossible dans %s (%s), rien n'a changé",
                          self.version, self.config, err)
            self._probleme(ISSUE_CONFIGURATION, ir.IssueSeverity.ERROR,
                           {"version": self.version, "signaler": URL_SIGNALER})
            return
        relatif = sauvegarde.relative_to(self.config).as_posix() if sauvegarde else None

        # 2. La configuration, comme « Vérifier la configuration » : un message nouveau
        # vient de ces fichiers, tout est remis comme avant.
        bloquant, constats = await self._verifier_configuration()
        if nouveaux := sorted(constats - constats_avant):
            await executer(installation.restaurer, self.config, plan, sauvegarde)
            await executer(installation.nettoyer_sauvegardes, self.config, GARDER_SAUVEGARDES)
            _LOGGER.error("Fichiers Tab5 %s refusés par la vérification de la configuration, "
                          "anciens fichiers remis : %s", self.version, " | ".join(nouveaux))
            self.donnees["refusee"] = installation.refus(self.version, empreinte)
            await self._sauver()
            self._probleme(ISSUE_CONFIGURATION, ir.IssueSeverity.ERROR,
                           {"version": self.version, "signaler": URL_SIGNALER})
            return
        self.donnees.pop("refusee", None)
        ir.async_delete_issue(hass, DOMAIN, ISSUE_CONFIGURATION)
        _LOGGER.info("Fichiers Tab5 %s posés (%d écrits, %d retirés, %d identiques), sauvegarde : %s, "
                     "vérification de la configuration : %d message(s), aucun nouveau",
                     self.version, len(plan.ecrire), len(plan.retirer), len(plan.identiques), relatif,
                     len(constats))
        if plan.differents:
            # Des fichiers du Tab5 copiés à la main (avant la première installation, le plus
            # souvent) étaient différents : remplacés, copie dans la sauvegarde. Persistante :
            # la notification, elle, ne survit pas à un redémarrage.
            _LOGGER.warning("Fichiers Tab5 déjà présents et différents, remplacés (copie dans %s) : %s",
                            relatif, ", ".join(plan.differents))
            self._probleme(ISSUE_REMPLACES, ir.IssueSeverity.WARNING,
                           {"fichiers": ", ".join(f"`{c}`" for c in plan.differents),
                            "sauvegarde": f"`{relatif}`"},
                           reparable=True, persistant=True)

        # 3. Sans la ligne `packages:`, rien à recharger (et rest_command.reload lève alors
        # un KeyError, HA 2026.9). Sinon : rendus actifs sans redémarrage.
        config_yaml = await self._config_yaml()
        charges = ((config_yaml or {}).get("homeassistant") or {}).get("packages") or {}
        packages_absents = config_yaml is not None and PACKAGE_TEMOIN not in charges
        # Configuration déjà invalide avant (erreur bloquante) : reload_all refuserait.
        redemarrer = bloquant
        if not packages_absents and not bloquant:
            choisies = self._options_choisies()
            redemarrer = await self._recharger(installation.domaines(plan.contenus), config_yaml)
            await self._options_gardees(choisies)

        # 4. L'effet : HA lit la nouvelle version des fichiers.
        if not packages_absents and not await self._attendre_version():
            redemarrer = True
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
            self.donnees.pop("firmware_essais", None)  # nouvelle version : compteurs à zéro
        await self._sauver()
        await executer(installation.nettoyer_sauvegardes, self.config, GARDER_SAUVEGARDES)

        suite_firmware = "non" if packages_absents else ("auto" if self.firmware_auto else "manuel")
        titre, message = messages.installation(
            hass.config.language, avant=avant, version=self.version, ecrits=len(plan.ecrire),
            retires=len(plan.retirer), identiques=len(plan.identiques), sauvegarde=relatif,
            modifies=plan.modifies, differents=plan.differents, redemarrer=redemarrer, packages_absents=packages_absents,
            firmware=suite_firmware)
        persistent_notification.async_create(hass, message, titre, NOTIFICATION)

    async def _verifier_configuration(self) -> tuple[bool, frozenset[str]]:
        """(erreur bloquante, tous les messages) de « Vérifier la configuration ». HA ne
        compte comme erreur qu'un fichier illisible : la configuration invalide d'un domaine
        ou d'un package n'y est qu'un avertissement (helpers/check_config.py, 2026.9), alors
        que ce domaine ne se charge plus. Les deux comptent ici."""
        res = await check_config.async_check_ha_config_file(self.hass)
        return bool(res.errors), frozenset(f"{e.domain or '-'} : {e.message}"
                                           for e in (*res.errors, *res.warnings))

    async def _recharger(self, domaines: set[str], config_yaml: dict[str, Any] | None) -> bool:
        """Rend les fichiers posés actifs sans redémarrage ; True s'il en faut un. Dans
        l'ordre : les modèles (custom_templates, lus par les automatisations), les entrées
        (input_*), les domaines que HA n'avait pas encore, puis tout le YAML."""
        hass, redemarrer = self.hass, False
        try:
            await hass.services.async_call("homeassistant", "reload_custom_templates", blocking=True)
            for domaine in installation.ordre_de_chargement(domaines):
                if domaine not in hass.config.components:
                    if config_yaml is None or not await async_setup_component(hass, domaine, config_yaml):
                        _LOGGER.warning("Tab5 : %s pas chargé sans redémarrage", domaine)
                        redemarrer = True
                elif domaine.startswith("input_") and hass.services.has_service(domaine, "reload"):
                    await hass.services.async_call(domaine, "reload", blocking=True)
            await hass.services.async_call("homeassistant", "reload_all", blocking=True)
        # Un rechargement de HA peut lever autre chose que HomeAssistantError (KeyError de
        # rest_command.reload, 2026.9) : les fichiers sont posés, un redémarrage finira.
        except Exception as err:  # noqa: BLE001
            _LOGGER.warning("Tab5 : rechargement du YAML interrompu (%r)", err)
            redemarrer = True
        return redemarrer

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
                  reparable: bool = False, persistant: bool = False) -> None:
        ir.async_create_issue(self.hass, DOMAIN, cle, is_fixable=reparable, is_persistent=persistant,
                              severity=gravite, translation_key=cle, translation_placeholders=valeurs)

    # ── Tableau de bord (tableau.py, tableau_ha.py, ADR-0075) ───────────────

    @staticmethod
    def _id_tableau(url_path: str | None) -> str:
        return f"{ISSUE_TABLEAU}{SEPARATEUR_TABLEAU}{url_path or 'defaut'}"

    async def async_evaluer_tableaux(self) -> None:
        """Une réparation par tableau de bord (mode stockage) dont le marqueur est plus ancien
        que les fichiers actifs ; retirée dès que le tableau est à jour (réécrit par la
        réparation, ou collé à la main). Ne lève jamais : une vérification ratée ne doit rien
        casser, et sans cette API la notification et les étapes manuelles restent la voie."""
        try:
            await self._evaluer_tableaux()
        except Exception:  # noqa: BLE001
            _LOGGER.exception("Tab5 : vérification du tableau de bord impossible")

    async def _evaluer_tableaux(self) -> None:
        if not self._version_active():
            return  # fichiers pas encore actifs (chez l'auteur : « dépôt ») : rien à comparer
        try:
            trouves = await tableau_ha.tableaux_tab5(self.hass)
        except tableau.TableauRefuse as err:
            if not self._api_tableau_signalee:
                self._api_tableau_signalee = True
                _LOGGER.warning("Tab5 : tableaux de bord pas lus (%s) : mise à jour en un clic "
                                "indisponible, étapes manuelles de docs/installation/dashboard.md", err)
            return
        en_retard = {self._id_tableau(c.url_path): c for c in trouves
                     if tableau.en_retard(c.marqueur, self.version)}
        for cle, cible in en_retard.items():
            ir.async_create_issue(
                self.hass, DOMAIN, cle, is_fixable=True, is_persistent=False,
                severity=ir.IssueSeverity.WARNING, translation_key=ISSUE_TABLEAU,
                translation_placeholders={"tableau": cible.titre, "avant": cible.marqueur.version or "?",
                                          "version": self.version},
                data={"url_path": cible.url_path})
        for domaine, cle in list(ir.async_get(self.hass).issues):
            if domaine == DOMAIN and cle.startswith(ISSUE_TABLEAU + SEPARATEUR_TABLEAU) and cle not in en_retard:
                ir.async_delete_issue(self.hass, DOMAIN, cle)

    async def async_mettre_a_jour_tableau(self, url_path: str | None) -> None:
        """La correction « Mettre à jour le tableau de bord » : rend la macro avec les
        arguments du marqueur, vérifie le rendu, garde l'ancien tableau dans
        tab5_sauvegardes/tableaux/, enregistre, relit. TableauRefuse (code = clé de
        traduction) AVANT d'écrire quoi que ce soit si une des étapes ne va pas."""
        async with self._verrou:
            if not self._version_active():
                raise tableau.TableauRefuse(tableau.ERREUR_VERSION, "fichiers pas encore actifs")
            trouves = await tableau_ha.tableaux_tab5(self.hass)
            cible = next((c for c in trouves if c.url_path == url_path), None)
            if cible is None:
                raise tableau.TableauRefuse(tableau.ERREUR_INTROUVABLE,
                                            f"tableau {url_path!r} sans marqueur ou disparu")
            texte = await tableau_ha.rendre(self.hass, cible.marqueur)
            nouveau = await tableau_ha.lire_yaml(self.hass, texte)
            tableau.valider_rendu(nouveau, self.version, cible.marqueur)
            sauvegarde = await self.hass.async_add_executor_job(
                tableau.sauvegarder, self.config, url_path, cible.config, cible.marqueur.version,
                dt_util.now())
            await tableau_ha.enregistrer(cible, nouveau)
            relu = await tableau_ha.tableaux_tab5(self.hass)
            apres = next((c for c in relu if c.url_path == url_path), None)
            if apres is None or tableau.en_retard(apres.marqueur, self.version):
                # Enregistré mais HA ne le voit pas à jour : l'ancien est dans la sauvegarde.
                raise tableau.TableauRefuse(tableau.ERREUR_API, "HA ne voit pas le tableau enregistré à jour")
            _LOGGER.info("Tab5 : tableau %r réécrit (marqueur %s → %s), ancien gardé dans %s",
                         url_path or "défaut", cible.marqueur.version or "?", self.version,
                         sauvegarde.relative_to(self.config).as_posix())
        await self.async_evaluer_tableaux()

    # ── Noms anglais (renommage.py) ─────────────────────────────────────────

    def _plan_migration(self) -> list[renommage.Renommage]:
        """Plan pour les entités ESPHome de chaque tablette (trouvée par son modèle, comme
        pour le firmware)."""
        appareils = dr.async_get(self.hass)
        entrees: list[tuple[str, str, str | None]] = []
        for e in er.async_get(self.hass).entities.values():
            if e.platform != "esphome" or not e.device_id:
                continue
            appareil = appareils.async_get(e.device_id)
            if appareil is not None and appareil.model == MODELE_TABLETTE:
                entrees.append((e.entity_id, e.unique_id, e.name))
        # Rien de faisable (identifiants anglais tous pris, nom déjà donné) : pas proposé,
        # sinon la réparation reviendrait sans fin.
        registre = er.async_get(self.hass)
        return [r for r in renommage.planifier(entrees)
                if r.nom or any(registre.async_get(c) is None for c in r.nouveaux_entity_ids)]

    def _options_choisies(self) -> dict[str, str]:
        """Option choisie de chaque liste des packages dont les options ont changé de langue."""
        return {eid: etat.state for eid in renommage.OPTIONS_PAQUETS
                if (etat := self.hass.states.get(eid)) is not None}

    async def _options_gardees(self, choisies: dict[str, str]) -> None:
        """Remet l'option choisie avant, traduite : sans elle, Home Assistant reprend la
        première option d'une liste dont l'option choisie n'existe plus."""
        for eid, avant in choisies.items():
            apres = renommage.option_anglaise(eid, avant)
            etat = self.hass.states.get(eid)
            if not apres or etat is None or etat.state == apres or apres not in (etat.attributes.get("options") or []):
                continue
            await self.hass.services.async_call("input_select", "select_option",
                                                {"entity_id": eid, "option": apres}, blocking=True)
            _LOGGER.info("Tab5 : %s gardé « %s » → « %s »", eid, avant, apres)

    async def async_options_au_demarrage(self) -> None:
        """Fichiers posés puis Home Assistant redémarré (ou installés à la main) : l'option
        d'avant n'existait plus, la liste a repris la première. La dernière option connue
        avant l'arrêt est dans les états restaurés de HA."""
        try:
            restaures = restore_state.async_get(self.hass).last_states
        except Exception:  # noqa: BLE001 — sans états restaurés, rien à garder
            return
        await self._options_gardees({eid: r.state.state for eid in renommage.OPTIONS_PAQUETS
                                     if (r := restaures.get(eid)) is not None})

    @callback
    def proposer_migration(self) -> None:
        if plan := self._plan_migration():
            self._probleme(ISSUE_MIGRATION, ir.IssueSeverity.WARNING,
                           {"nombre": str(len(plan))}, reparable=True)
        else:
            ir.async_delete_issue(self.hass, DOMAIN, ISSUE_MIGRATION)

    async def async_migrer(self) -> int:
        """Applique le plan dans le registre : identifiant d'entité anglais (l'historique et
        les statistiques suivent) et nom affiché anglais. Un identifiant déjà pris par une
        autre entité n'est pas touché (le nom l'est quand même)."""
        registre = er.async_get(self.hass)
        faits = 0
        for r in self._plan_migration():
            changements: dict[str, Any] = {}
            libre = next((c for c in r.nouveaux_entity_ids if registre.async_get(c) is None), None)
            if libre:
                changements["new_entity_id"] = libre
            if r.nom:
                changements["name"] = r.nom
            if changements:
                registre.async_update_entity(r.entity_id, **changements)
                faits += 1
        _LOGGER.info("Tab5 : %d entités passées à leur nom anglais", faits)
        self.proposer_migration()
        return faits

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
        fichiers, et seulement une fois ces fichiers actifs ; la réessaie si elle reste sans
        effet (firmware.py). Sinon attend (sur_changement, minuterie)."""
        attendu = self.donnees.get("firmware_attendu")
        if not self.firmware_auto:
            ir.async_delete_issue(self.hass, DOMAIN, ISSUE_FIRMWARE)
        if not attendu or not self.firmware_auto or attendu != self.version or not self._version_active():
            return
        entites = self._entites_firmware()
        tablettes = {}
        for entite in entites:
            if (etat := self.hass.states.get(entite)) is not None:
                tablettes[entite] = firmware.Tablette(
                    installee=etat.attributes.get("installed_version"),
                    proposee=etat.attributes.get("latest_version"), disponible=etat.state == "on",
                    en_cours=bool(etat.attributes.get("in_progress")))
        essais: dict[str, dict] = self.donnees.setdefault("firmware_essais", {})
        maintenant = dt_util.utcnow()
        decision = firmware.decider(attendu, tablettes, essais, maintenant)

        if decision.fini:
            # Succès constaté (version installée lue sur l'entité) : seulement maintenant.
            _LOGGER.info("Tab5 : tablette en %s, mise à jour enchaînée terminée", attendu)
            self.donnees.pop("firmware_attendu", None)
            self.donnees.pop("firmware_essais", None)
            await self._sauver()
            ir.async_delete_issue(self.hass, DOMAIN, ISSUE_FIRMWARE)
            self._programmer(None)
            return

        # Les essais sont comptés avant tout await : un autre appel (sur_changement) voit
        # l'essai en cours et ne relance pas la même tablette.
        numeros = {entite: firmware.noter_essai(essais, entite, maintenant) for entite in decision.lancer}
        if numeros:
            await self._sauver()
        for entite, numero in numeros.items():
            _LOGGER.info("Tab5 : fichiers %s actifs, mise à jour de %s lancée (essai %d/%d)",
                         attendu, entite, numero, firmware.ESSAIS_MAX)
            try:
                # ESPHome ne fait qu'envoyer la commande : l'appel rend la main tout de suite,
                # et ne lève que si la tablette ne la reçoit pas (déconnectée…).
                async with asyncio.timeout(60):
                    await self.hass.services.async_call("update", "install", {"entity_id": entite},
                                                        blocking=True)
            except Exception as err:  # noqa: BLE001 — l'essai compte, la minuterie réessaiera
                _LOGGER.warning("Tab5 : mise à jour de %s pas lancée (%r), nouvel essai dans %d min",
                                entite, err, firmware.DELAI_ESSAI_S // 60)
                continue
            titre, message = messages.firmware_lance(self.hass.config.language, attendu)
            persistent_notification.async_create(self.hass, message, titre, f"{NOTIFICATION}_firmware")

        if decision.echecs:
            _LOGGER.warning("Tab5 : %s toujours pas en %s après %d essais, à installer à la main",
                            ", ".join(decision.echecs), attendu, firmware.ESSAIS_MAX)
            self._probleme(ISSUE_FIRMWARE, ir.IssueSeverity.WARNING,
                           {"version": attendu, "essais": str(firmware.ESSAIS_MAX)})
        else:
            ir.async_delete_issue(self.hass, DOMAIN, ISSUE_FIRMWARE)
        # Revenir voir après un essai, ou à la fin du délai d'un essai en cours.
        self._programmer(firmware.DELAI_ESSAI_S if numeros else decision.attente_s)

        if rafraichir and entites:
            # Le manifeste n'est relu que toutes les 6 h : demander une vérification tout de
            # suite (ESPHome : commande CHECK) ; la réponse arrive par sur_changement.
            await self.hass.services.async_call("homeassistant", "update_entity",
                                                {"entity_id": entites}, blocking=False)

    def _programmer(self, delai_s: float | None) -> None:
        """Une seule minuterie : la prochaine vérification du firmware (None : aucune)."""
        if self._minuterie is not None:
            self._minuterie()
            self._minuterie = None
        if delai_s is not None:
            self._minuterie = async_call_later(self.hass, delai_s + 5, self._a_l_heure)

    @callback
    def _a_l_heure(self, _maintenant: Any) -> None:
        self._minuterie = None
        self.hass.async_create_task(self.async_verifier_firmware())

    @callback
    def arreter(self) -> None:
        self._programmer(None)
