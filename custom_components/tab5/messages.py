# -*- coding: utf-8 -*-
"""Textes des notifications de l'intégration « Tab5 », en français ou en anglais.

[AI-CONTEXT]
@role Une notification persistante n'a pas de traduction dans Home Assistant (au contraire
      des formulaires et des réparations, translations/*.json) : le texte est choisi ici
      d'après la langue du serveur (hass.config.language). Module pur, sans import de
      homeassistant ni import relatif (tests/test_integration_tab5.py).
@ai_instruction Phrases courtes, accents et « » en français ; jamais de code ni d'état brut
      à l'utilisateur, sauf un chemin de fichier entre accents graves.
"""
from __future__ import annotations


def _fr(langue: str | None) -> bool:
    return (langue or "").lower().startswith("fr")


def installation(langue: str | None, *, avant: str | None, version: str, ecrits: int,
                 retires: int, identiques: int, sauvegarde: str | None, modifies: list[str],
                 redemarrer: bool, packages_absents: bool, firmware: str,
                 differents: list[str] | None = None) -> tuple[str, str]:
    """(titre, message) après une installation. `firmware` : « auto » (lancée dès que
    proposée), « manuel » (option décochée) ou « non » (fichiers pas actifs : rien)."""
    fr = _fr(langue)
    lignes = []
    if fr:
        titre = f"Tab5 : fichiers Home Assistant {version}"
        if avant:
            lignes.append(f"Fichiers du Tab5 passés de la {avant} à la {version} : {ecrits} remplacé(s), "
                          f"{retires} retiré(s), {identiques} inchangé(s).")
        else:
            lignes.append(f"Fichiers du Tab5 installés en {version} : {ecrits} fichier(s) posé(s).")
        if sauvegarde:
            lignes.append(f"Les anciens sont gardés dans `{sauvegarde}`.")
        if modifies:
            lignes.append("Modifiés à la main depuis la dernière installation, remplacés quand même "
                          f"(copie dans la sauvegarde) : {', '.join(modifies)}.")
        if differents:
            lignes.append("Déjà là et différents (copiés à la main ?), remplacés quand même "
                          f"(copie dans la sauvegarde) : {', '.join(differents)}.")
        if packages_absents:
            lignes.append("Home Assistant ne charge pas encore les packages : voir Paramètres → Réparations.")
        elif redemarrer:
            lignes.append("Redémarrez Home Assistant pour finir : voir Paramètres → Réparations.")
        if firmware == "auto":
            lignes.append(f"La tablette se mettra à jour toute seule dès que son entité « Firmware » "
                          f"proposera la {version}.")
        elif firmware == "manuel":
            lignes.append("Mettez ensuite la tablette à jour : Paramètres → Mises à jour, entité « Firmware ».")
        lignes.append("Tableau de bord « Tab5 » : si cette version ajoute des entités, refaites ses "
                      "étapes 2 et 3.")
    else:
        titre = f"Tab5: Home Assistant files {version}"
        if avant:
            lignes.append(f"Tab5 files updated from {avant} to {version}: {ecrits} replaced, "
                          f"{retires} removed, {identiques} unchanged.")
        else:
            lignes.append(f"Tab5 files installed, version {version}: {ecrits} file(s) added.")
        if sauvegarde:
            lignes.append(f"The old ones are kept in `{sauvegarde}`.")
        if modifies:
            lignes.append("Edited by hand since the last installation, replaced anyway "
                          f"(a copy is in the backup): {', '.join(modifies)}.")
        if differents:
            lignes.append("Already there and different (copied by hand?), replaced anyway "
                          f"(a copy is in the backup): {', '.join(differents)}.")
        if packages_absents:
            lignes.append("Home Assistant does not load the packages yet: see Settings → Repairs.")
        elif redemarrer:
            lignes.append("Restart Home Assistant to finish: see Settings → Repairs.")
        if firmware == "auto":
            lignes.append(f"The tablet will update by itself as soon as its « Firmware » entity "
                          f"offers {version}.")
        elif firmware == "manuel":
            lignes.append("Then update the tablet: Settings → Updates, « Firmware » entity.")
        lignes.append("« Tab5 » dashboard: if this version adds entities, do its items 2 and 3 again.")
    return titre, "\n\n".join(lignes)


def assistant_action(langue: str | None, action: str, alias: str = "", raison: str = "") -> str:
    """Ce que fera la validation du récapitulatif de l'assistant (assistant.Situation.action)."""
    if _fr(langue):
        textes = {
            "creer": "Valider crée l'automatisation « Tab5 — emplacements de l'écran » (pièces et maison) dans "
                     "`automations.yaml` (sauvegardé avant dans `tab5_sauvegardes/automatisations/`), "
                     "puis recharge les automatisations. Tout reste modifiable ensuite dans l'automatisation.",
            "mettre_a_jour": f"Une automatisation du blueprint existe déjà : « {alias} ». Elle ne change que "
                             "si vous cochez la case ci-dessous : ses pièces sont alors remplacées par "
                             "celles-ci, ses entrées de la maison par celles remplies ici, ses autres "
                             "réglages restent (`automations.yaml` sauvegardé avant). "
                             "Sans la case, elle ne change pas.",
            "ailleurs": "Une automatisation du blueprint existe déjà, hors de `automations.yaml` : "
                        "l'assistant n'y touche pas.",
            "plusieurs": "Plusieurs automatisations du blueprint existent déjà : l'assistant ne choisit pas "
                         "laquelle changer : il n'y touche pas.",
            "fichier": f"L'assistant n'écrira pas `automations.yaml` ({raison}). Valider met le YAML à "
                       "coller dans une notification.",
        }
    else:
        textes = {
            "creer": "Submitting creates the automation « Tab5 — screen slots » (rooms and home) in `automations.yaml` "
                     "(saved first in `tab5_sauvegardes/automatisations/`), then reloads the automations. "
                     "Everything stays editable in the automation afterwards.",
            "mettre_a_jour": f"An automation of the blueprint already exists: « {alias} ». It only changes if "
                             "you tick the box below: its rooms are then replaced by these ones, its home inputs by "
                             "the ones filled in here, its other settings stay (`automations.yaml` saved "
                             "first). Without the box, it does not change.",
            "ailleurs": "An automation of the blueprint already exists outside `automations.yaml`: the "
                        "assistant leaves it alone.",
            "plusieurs": "Several automations of the blueprint already exist: the assistant does not pick "
                         "which one to change: it leaves them alone.",
            "fichier": f"The assistant will not write `automations.yaml` ({raison}). Submitting puts the YAML "
                       "to paste in a notification.",
        }
    return textes[action]


def assistant_resultat(langue: str | None, resultat: str, *, entite: str | None = None,
                       sauvegarde: str | None = None, yaml_a_coller: str = "",
                       raison: str = "", listes: list[str] | None = None,
                       listes_ratees: list[str] | None = None) -> tuple[str, str]:
    """(titre, message) à la fin de l'assistant. `resultat` : « cree », « mis_a_jour »,
    « non_chargee » (écrite, mais HA ne l'a pas chargée), « rien » ou « a_coller ».
    `listes` / `listes_ratees` : les listes « Tab5 · … » réglées ou pas (« libellé → choix »)."""
    fr = _fr(langue)
    titre = "Tab5 : assistant de configuration" if fr else "Tab5: setup assistant"
    lignes = []
    if resultat in ("cree", "mis_a_jour", "non_chargee"):
        if fr:
            lignes.append({"cree": "Automatisation des emplacements créée",
                           "mis_a_jour": "Pièces de l'automatisation des emplacements remplacées",
                           "non_chargee": "Automatisation écrite dans `automations.yaml`, mais Home Assistant "
                                          "ne l'a pas chargée : voir Paramètres → Système → Journaux"}[resultat]
                          + (f" (`{entite}`)." if entite else "."))
            if sauvegarde:
                lignes.append(f"L'ancien `automations.yaml` est gardé dans `{sauvegarde}`.")
            lignes.append("La tablette reçoit ses pièces à sa prochaine connexion. Tout se change ensuite dans "
                          "l'automatisation (Paramètres → Automatisations et scènes).")
        else:
            lignes.append({"cree": "Screen slots automation created",
                           "mis_a_jour": "Rooms of the screen slots automation replaced",
                           "non_chargee": "Automation written to `automations.yaml`, but Home Assistant did "
                                          "not load it: see Settings → System → Logs"}[resultat]
                          + (f" (`{entite}`)." if entite else "."))
            if sauvegarde:
                lignes.append(f"The old `automations.yaml` is kept in `{sauvegarde}`.")
            lignes.append("The tablet gets its rooms at its next connection. Change anything afterwards in the "
                          "automation (Settings → Automations & scenes).")
    elif resultat == "a_coller":
        if fr:
            lignes.append(f"`automations.yaml` n'a pas été écrit ({raison}). Ajoutez cette automatisation à "
                          "votre fichier des automatisations, puis rechargez les automatisations :")
        else:
            lignes.append(f"`automations.yaml` was not written ({raison}). Add this automation to your "
                          "automations file, then reload the automations:")
        lignes.append(f"```yaml\n{yaml_a_coller.rstrip()}\n```")
    else:
        lignes.append(("Automatisation inchangée" if fr else "Automation unchanged") + (f" ({raison})." if raison else "."))
    if listes:
        lignes.append(("Listes « Tab5 · … » réglées : " if fr else "« Tab5 · … » lists set: ") + " ; ".join(listes) + ".")
    if listes_ratees:
        lignes.append(("Pas réglées (à faire dans Paramètres → Appareils et services → Entités) : " if fr
                       else "Not set (do it in Settings → Devices & services → Entities): ")
                      + " ; ".join(listes_ratees) + ".")
    return titre, "\n\n".join(lignes)


def firmware_lance(langue: str | None, version: str) -> tuple[str, str]:
    """(titre, message) quand la mise à jour de la tablette est lancée."""
    if _fr(langue):
        return ("Tab5 : mise à jour de la tablette",
                f"Les fichiers Home Assistant sont en place : la tablette passe en {version}. "
                "Elle redémarre toute seule dans quelques minutes.")
    return ("Tab5: tablet update",
            f"The Home Assistant files are in place: the tablet is updating to {version}. "
            "It restarts by itself in a few minutes.")
