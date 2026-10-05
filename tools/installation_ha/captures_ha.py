"""Captures de l'interface de Home Assistant pour le guide d'installation (docs/installation/).

Appelé à la fin de verifier_installation.py (option --interface), sur le Home Assistant
neuf de la CI : tablette virtuelle ajoutée, automatisation du blueprint créée, tableau de
bord enregistré. Aucune donnée personnelle : compte « CI », intégration demo,
donnees_test.yaml. Chromium (Playwright) ouvre l'interface avec le jeton du compte, comme
après une connexion, une fois par langue du site. La langue est celle du profil de
l'utilisateur (frontend/set_user_data, que le frontend relit à la connexion et préfère à
celle du navigateur) ; chaque capture vérifie qu'elle est appliquée (<html lang>, posé
par le frontend) et ce qu'elle doit montrer.

Les images (ha_<nom>_<langue>.png) vont dans l'artefact « installation-ha » du job ; elles
sont copiées à la main dans docs/images/installation/ quand l'interface change (HA_IMAGE
relevée dans .github/workflows/installation-ha.yml) ou qu'une étape du guide change.
"""

from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any

# Langues du site (ADR-0030).
LANGUES = ("en", "fr")
# Mise en page « étroite » du frontend (870 px et moins) : une colonne, encore lisible une
# fois réduite dans la page du site ; noms entiers dans la table des entités, ligne du
# modèle entière dans son éditeur (à 1280 px, l'une et l'autre sont coupées).
LARGEUR = 860
# Laisse le frontend finir de dessiner après le chargement (panneaux chargés à la demande,
# rendu du modèle par le serveur).
POSE_MS = 2000


def preferences_de_langue(langue: str) -> dict[str, str]:
    """Préférences « language » du profil (FrontendLocaleData du frontend, src/data/
    translation.ts) : la langue choisie, le reste suit la langue."""
    return {"language": langue, "number_format": "language", "time_format": "language",
            "date_format": "language", "first_weekday": "language", "time_zone": "local"}


def script_de_connexion(base: str, client_id: str, jeton: str, rafraichissement: str, langue: str) -> str:
    """Script lancé avant chaque page : les jetons du compte, rangés où le frontend les
    cherche après une connexion (localStorage.hassTokens), et la langue choisie."""
    jetons = {"access_token": jeton, "token_type": "Bearer", "expires_in": 1800,
              "hassUrl": base, "clientId": client_id, "refresh_token": rafraichissement,
              "expires": int(time.time() * 1000) + 600_000}
    return (f"localStorage.setItem('hassTokens', {json.dumps(json.dumps(jetons))});"
            f"localStorage.setItem('selectedLanguage', {json.dumps(json.dumps(langue))});")


async def ouvrir(page, url: str) -> None:
    await page.goto(url, wait_until="networkidle")
    await page.wait_for_timeout(POSE_MS)


async def textes_visibles(page, texte: str) -> int:
    """Éléments qui contiennent `texte` (les sélecteurs de Playwright traversent les
    shadow DOM du frontend)."""
    return await page.get_by_text(texte).count()


async def capture_appareil(page, base: str, cible: dict[str, Any]) -> str:
    """Étape 4 : la page de l'appareil de la tablette (Paramètres → Appareils et services)."""
    await ouvrir(page, f"{base}/config/devices/device/{cible['appareil']['id']}")
    nom = cible["appareil"].get("name_by_user") or cible["appareil"]["name"]
    if not await textes_visibles(page, nom):
        raise RuntimeError(f"nom de l'appareil « {nom} » absent de sa page")
    return f"page de l'appareil « {nom} »"


async def capture_listes(page, base: str, _cible: dict[str, Any]) -> str:
    """Étape 5 : les listes « Tab5 · … » dans la table des entités. La recherche porte aussi
    sur l'entity_id : « select.tab5_ » garde les select et les input_select des packages,
    sans les capteurs « Tab5 · » qui ne se règlent pas."""
    await ouvrir(page, f"{base}/config/entities")
    champ = page.locator("hass-tabs-subpage-data-table input").filter(visible=True).first
    await champ.fill("select.tab5_")
    await page.wait_for_timeout(POSE_MS)
    n = await textes_visibles(page, "Tab5 ·")
    if n < 10:
        raise RuntimeError(f"{n} listes « Tab5 · » affichées après la recherche")
    return f"{n} listes « Tab5 · » dans la table"


async def capture_blueprint(page, base: str, cible: dict[str, Any]) -> str:
    """Étape 6 : l'automatisation créée depuis le blueprint, dans son éditeur."""
    await ouvrir(page, f"{base}/config/automation/edit/{cible['automatisation']}")
    if not await textes_visibles(page, cible["alias"]):
        raise RuntimeError(f"nom de l'automatisation « {cible['alias']} » absent de l'éditeur")
    return f"éditeur de « {cible['alias']} »"


async def capture_modele(page, base: str, cible: dict[str, Any]) -> str:
    """Étape 7 : la ligne du tableau de bord tapée dans Outils de développement → Modèle,
    et son résultat. insert_text d'un bloc : l'éditeur ne ferme pas les accolades tout
    seul (il ne le fait que pour un caractère tapé)."""
    await ouvrir(page, f"{base}/developer-tools/template")
    editeur = page.locator(".cm-content").first
    await editeur.click()
    await page.keyboard.press("Control+A")
    await page.keyboard.insert_text(cible["modele"])
    await page.keyboard.press("Home")  # la ligne vue depuis son début
    await page.wait_for_timeout(POSE_MS * 2)
    tape = (await editeur.inner_text()).strip()
    if tape != cible["modele"]:
        raise RuntimeError(f"l'éditeur contient {tape[:200]!r}")
    if not await textes_visibles(page, "tab5-reglages"):
        raise RuntimeError("le résultat du modèle ne montre pas le tableau de bord (vue tab5-reglages)")
    return "ligne du tableau de bord et son résultat"


# (nom du fichier, hauteur de la fenêtre, capture)
CAPTURES = (
    ("appareil", 1100, capture_appareil),
    ("listes", 1150, capture_listes),
    ("blueprint", 1300, capture_blueprint),
    ("modele", 1000, capture_modele),
)


async def appareil_de(ws, entite: str) -> dict[str, Any]:
    """L'appareil (registre de HA) qui porte l'entité `entite`."""
    entree = await ws.commande("config/entity_registry/get", entity_id=entite)
    for appareil in await ws.commande("config/device_registry/list"):
        if appareil["id"] == entree.get("device_id"):
            return appareil
    raise RuntimeError(f"aucun appareil pour {entite}")


async def capturer_interface(ha, rapport, dossier: Path, *, client_id: str, entite_tablette: str,
                             automatisation: str, alias: str, modele: str) -> None:
    """Les captures de CAPTURES dans chaque langue de LANGUES, rangées dans `dossier`.
    Un échec est rapporté (le job échoue) sans arrêter les autres captures."""
    from playwright.async_api import async_playwright

    dossier.mkdir(parents=True, exist_ok=True)
    cible = {"appareil": await appareil_de(ha.ws, entite_tablette), "automatisation": automatisation,
             "alias": alias, "modele": modele}
    async with async_playwright() as p:
        navigateur = await p.chromium.launch()
        try:
            for langue in LANGUES:
                await ha.ws.commande("frontend/set_user_data", key="language",
                                     value=preferences_de_langue(langue))
                contexte = await navigateur.new_context(viewport={"width": LARGEUR, "height": 1000},
                                                        locale=langue, device_scale_factor=1)
                await contexte.add_init_script(script_de_connexion(
                    ha.base, client_id, ha.jeton, ha.rafraichissement, langue))
                page = await contexte.new_page()
                for nom, hauteur, capture in CAPTURES:
                    fichier = dossier / f"ha_{nom}_{langue}.png"
                    try:
                        await page.set_viewport_size({"width": LARGEUR, "height": hauteur})
                        constat = await capture(page, ha.base, cible)
                        langue_page = await page.evaluate("document.documentElement.lang")
                        if langue_page != langue:
                            raise RuntimeError(f"page en « {langue_page} »")
                        await page.screenshot(path=str(fichier))
                        rapport.ok(f"interface de HA, {fichier.name} : {constat}")
                    except Exception as exc:  # noqa: BLE001 — une capture ratée n'arrête pas les autres
                        await page.screenshot(path=str(dossier / f"echec_{nom}_{langue}.png"))
                        rapport.echec(f"interface de HA, {fichier.name} : {type(exc).__name__}: {exc}")
                await contexte.close()
        finally:
            await navigateur.close()
