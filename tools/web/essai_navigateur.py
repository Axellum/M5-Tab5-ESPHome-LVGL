#!/usr/bin/env python3
"""Essai de la démo web dans un vrai navigateur (Chromium de Playwright), sans réseau.

[AI-CONTEXT]
Rôle : prouver que la démo construite par tools/web/construire.py MARCHE, pas seulement
qu'elle compile (.github/workflows/demo-web.yml l'appelle avant de joindre l'archive à
une release). La page web/demo/index.html est servie avec tab5.js, tab5.wasm et
demo.json sous demo/, comme sur le site ; le script vérifie :
  - tab5.js, tab5.wasm et demo.json chargés (200), aucune erreur JavaScript ;
  - les actions de l'API lues dans le firmware (web_actions), la maison poussée sans
    action refusée ni absente (journal de la page) ;
  - l'horloge de la tablette sur celle de la démo (demo.json, « horloge ») ;
  - une lampe basculée depuis l'écran (événement tab5_action) change d'état dans le
    Home Assistant simulé et son état repart vers l'écran sans refus ;
  - une image dessinée (des dizaines de couleurs, pas un écran uni).
Il écrit une capture de la page (--capture). Code de sortie 1 au premier échec.

Usage : python tools/web/essai_navigateur.py --dossier <sortie de construire.py> [--capture demo.png]
Il faut Playwright et son Chromium (python -m playwright install chromium).
"""
from __future__ import annotations

import argparse
import functools
import http.server
import shutil
import sys
import tempfile
import threading
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
PAGE = RACINE / "web" / "demo" / "index.html"
FICHIERS = ("tab5.js", "tab5.wasm", "demo.json")


class _Silencieux(http.server.SimpleHTTPRequestHandler):
    extensions_map = {**http.server.SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm"}

    def log_message(self, format, *args) -> None:  # noqa: A002 (nom de la méthode de base)
        pass  # pas de ligne par requête


def _serveur(dossier: Path) -> tuple[http.server.ThreadingHTTPServer, str]:
    gestionnaire = functools.partial(_Silencieux, directory=str(dossier))
    serveur = http.server.ThreadingHTTPServer(("127.0.0.1", 0), gestionnaire)
    threading.Thread(target=serveur.serve_forever, daemon=True).start()
    return serveur, f"http://127.0.0.1:{serveur.server_address[1]}/demo/"


def essayer(dossier: Path, capture: Path | None) -> list[str]:
    from playwright.sync_api import sync_playwright

    echecs: list[str] = []
    with tempfile.TemporaryDirectory() as tmp:
        site = Path(tmp)
        (site / "demo").mkdir()
        shutil.copyfile(PAGE, site / "demo" / "index.html")
        for nom in FICHIERS:
            shutil.copyfile(dossier / nom, site / "demo" / nom)
        serveur, adresse = _serveur(site)
        try:
            with sync_playwright() as p:
                navigateur = p.chromium.launch()
                page = navigateur.new_page(viewport={"width": 1600, "height": 900}, locale="fr-FR")
                erreurs: list[str] = []
                statuts: dict[str, int] = {}
                page.on("pageerror", lambda e: erreurs.append(str(e)))
                page.on("response", lambda r: statuts.__setitem__(r.url.rsplit("/", 1)[-1], r.status))
                page.goto(adresse)
                page.wait_for_function("window.fauxHa && Object.keys(fauxHa.actions).length > 0", timeout=90_000)
                page.wait_for_timeout(3000)  # la première scène : une action toutes les 15 ms
                for nom in FICHIERS:
                    if statuts.get(nom) != 200:
                        echecs.append(f"{nom} : HTTP {statuts.get(nom)}")
                etat = page.evaluate("""() => {
                    const journal = [...document.querySelectorAll('#journal li')].map((l) => l.textContent);
                    return {
                        actions: Object.keys(fauxHa.actions).length,
                        refus: journal.filter((l) => /refusée|refused|absente|missing/.test(l)),
                        ecart: Module._web_maintenant() - Date.parse(demo.horloge) / 1000,
                        lampe: Object.keys(fauxHa.tuiles).find((c) => fauxHa.tuiles[c].type === 'lum'
                                                                     && fauxHa.tuiles[c].etat !== 'unavailable'),
                    };
                }""")
                print(f"{etat['actions']} actions, horloge à {etat['ecart']:.0f} s du départ de la démo")
                if etat["actions"] < 30:
                    echecs.append(f"seulement {etat['actions']} actions lues dans le firmware")
                if not 0 <= etat["ecart"] < 300:
                    echecs.append(f"horloge de la tablette à {etat['ecart']:.0f} s de celle de la démo")
                if not etat["lampe"]:
                    echecs.append("aucune lampe dans la maison de la démo")
                else:
                    cle = etat["lampe"]
                    avant = page.evaluate(f"fauxHa.tuiles['{cle}'].etat")
                    page.evaluate(f"""fauxHa.recevoir({{event: true, service: 'esphome.tab5_action',
                                      data: {{emplacement: '{cle}', action: 'basculer', valeur: ''}}}})""")
                    page.wait_for_timeout(1000)
                    apres = page.evaluate(f"fauxHa.tuiles['{cle}'].etat")
                    print(f"lampe {cle} : {avant} → {apres}")
                    if apres == avant:
                        echecs.append(f"lampe {cle} basculée depuis l'écran : toujours « {avant} »")
                    etat["refus"] += page.evaluate("""() => [...document.querySelectorAll('#journal li')]
                        .map((l) => l.textContent).filter((l) => /refusée|refused|absente|missing/.test(l))""")
                for ligne in sorted(set(etat["refus"])):
                    echecs.append(f"journal : {ligne}")
                couleurs = page.evaluate("""() => {
                    const c = document.getElementById('ecran');
                    const d = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
                    const vues = new Set();
                    for (let i = 0; i < d.length; i += 4 * 97) vues.add((d[i] << 16) | (d[i + 1] << 8) | d[i + 2]);
                    return vues.size;
                }""")
                print(f"{couleurs} couleurs à l'écran")
                if couleurs < 20:  # RGB565 et thème sombre : ~90 à l'accueil
                    echecs.append(f"écran presque uni ({couleurs} couleurs) : rien de dessiné ?")
                echecs += [f"erreur JavaScript : {e}" for e in erreurs]
                if capture is not None:
                    capture.parent.mkdir(parents=True, exist_ok=True)
                    page.screenshot(path=str(capture), full_page=True)
                    print(f"capture : {capture}")
                navigateur.close()
        finally:
            serveur.shutdown()
    return echecs


def main() -> int:
    parser = argparse.ArgumentParser(description="Essaie la démo web dans Chromium (Playwright).")
    parser.add_argument("--dossier", type=Path, required=True, help="sortie de tools/web/construire.py")
    parser.add_argument("--capture", type=Path, help="capture de la page (PNG)")
    args = parser.parse_args()
    manquants = [n for n in FICHIERS if not (args.dossier / n).is_file()]
    if manquants:
        print(f"absent de {args.dossier} : {', '.join(manquants)}")
        return 1
    echecs = essayer(args.dossier, args.capture)
    for e in echecs:
        print(f"ÉCHEC {e}")
    print("démo web : OK" if not echecs else f"démo web : {len(echecs)} échec(s)")
    return 1 if echecs else 0


if __name__ == "__main__":
    sys.exit(main())
