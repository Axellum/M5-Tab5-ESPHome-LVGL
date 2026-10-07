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
                 redemarrer: bool, packages_absents: bool, firmware: str) -> tuple[str, str]:
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


def firmware_lance(langue: str | None, version: str) -> tuple[str, str]:
    """(titre, message) quand la mise à jour de la tablette est lancée."""
    if _fr(langue):
        return ("Tab5 : mise à jour de la tablette",
                f"Les fichiers Home Assistant sont en place : la tablette passe en {version}. "
                "Elle redémarre toute seule dans quelques minutes.")
    return ("Tab5: tablet update",
            f"The Home Assistant files are in place: the tablet is updating to {version}. "
            "It restarts by itself in a few minutes.")
