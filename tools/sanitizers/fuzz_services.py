"""Fuzzing des services de la tablette virtuelle (contrat HA → tablette), sous ASan/UBSan.

    python tools/sanitizers/fuzz_services.py --programme <program> --resultats <dossier> [--appels 800]

Lance la tablette virtuelle (tab5-rendu-host.yaml compilé pour `host` avec ASan/UBSan :
tools/sanitizers/variante.py + pio_drapeaux.py), lui donne une clé comme
le fait la démo, puis appelle chaque service `tab5_*` connu avec des charges utiles
dérivées d'une graine valide (champs vides, délimiteurs en trop ou manquants, nombres
extrêmes, inf/nan, chaînes très longues, UTF-8 multi-octets, index hors bornes…).

Un appel de service ne renvoie rien : la tablette est « pinguée » (device_info) tous les
PING appels. Si elle ne répond plus ou si son processus est mort, les derniers appels
sont gardés comme reproducteurs, le processus est relancé et le fuzzing reprend au
service suivant. Les rapports des sanitizers sont lus dans le journal de la tablette
(tools/sanitizers/rapports.py : UBSan écrit sur la sortie d'erreur) et rattachés au
service en cours. Tirage aléatoire à graine fixe : reproductible.

Sorties : <resultats>/fuzz.json, <resultats>/fuzz.md, journaux tablette-N.log.
Code de sortie 1 si un rapport ou un plantage (job sanitizers, lot B de l'audit du
30/09/2026 : le passage de l'audit avait trouvé 4 conversions hors bornes).
"""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import random
import re
import subprocess
import sys
import time
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(RACINE / "tools" / "demo"))
sys.path.insert(0, str(RACINE / "tools" / "sanitizers"))

from demo_pusher import _donner_une_cle, _lire_cle_demo  # noqa: E402
from rapports import Journal, distincts  # noqa: E402

logger = logging.getLogger("fuzz")

PING = 10            # appels entre deux vérifications de vie
PAUSE = 0.01         # secondes entre deux appels (la boucle ESPHome doit suivre)
TAILLE_MAX = 6000    # au-delà, l'API coupe la connexion : ce n'est plus la lecture qu'on teste

# Graines valides : exemples du contrat (Tab5/paquets/tab5-api-logic.yaml) et de tools/demo/.
# Une par service déclaré, ni plus ni moins (tests/test_sanitizers.py le vérifie : un
# service sans graine ne serait jamais fuzzé, sans erreur). Tuiles et emplacements
# portent aussi la rangée sous l'horloge (hp, hd, hLI, ADR-0031) et la tuile − / +
# (rN, ADR-0033), dont les lecteurs ont leur propre découpage.
GRAINES: dict[str, dict[str, str]] = {
    "tab5_maj_previsions_heures_bulk": {"payload": "0|14:00|sunny|21.4|0;1|15:00|cloudy|20.8|0.2;2|16:00|rainy|19.1|1.5;"},
    "tab5_maj_previsions_jours_bulk": {"payload": "0|Auj 17|sunny|12.1|24.3|0|0|0|;1|Ven 18|rainy|11.0|19.8|1|0|0|08:00-16:00;"},
    "tab5_maj_volet_etat": {"etat_physique": "En_mouvement"},
    "tab5_maj_clim": {"target": "21.5", "current": "20.8", "mode": "cool", "preset": "boost", "fan": "auto", "swing": "off"},
    "tab5_maj_alerte_meteo_france": {"payload": "@2,1790000000|Jaune|Vert|Vert|Jaune|Vert|Vert|Vert|Vert|Vert|Vert"},
    "tab5_maj_probabilites": {"uv": "4", "gel": "0", "neige": "0"},
    "tab5_maj_meteo_actuelle": {"condition": "sunny", "temperature": "22.4", "humidite": "68"},
    "tab5_maj_pluie_1h_bulk": {"payload": "0|Temps sec;1|Pluie faible;2|Pluie modérée;"},
    "tab5_maj_info_texte": {"texte": "@ha|1|Home Assistant Core|0|0|0|", "couleur": "Blanc", "meteo_id": "meteo:orange"},
    "tab5_maj_reponse_vocale": {"texte": "Il fait 21 degrés dans le salon."},
    "tab5_assist_reponse": {"texte": "**Salon** : 21 degrés", "image_url": ""},
    "tab5_maj_alertes_ha_bulk": {"payload": "@n:6;update.home_assistant_core_update#1|Rouge|@maj:Home Assistant Core;ha:indispo#2|Orange|@indispo:3"},
    "tab5_maj_alertes_historique": {"payload": "1791381720|1791382200|0|Rouge|@maj:Home Assistant Core;1791370000|0|1791375400|Orange|@vigi:Orange"},
    "tab5_maj_planning": {"ligne1": "Auj : TRAVAIL 08:00-16:00", "ligne2": "Demain : repos"},
    "tab5_maj_rdv_prochains": {"payload": "1789552800|Dentiste~1789639200|Réunion équipe"},
    "tab5_maj_calendrier_mois": {
        "annee": "2026", "mois": "9", "codes": "01" * 31,
        "heures": "|".join(["08:00-16:00", "", "14:00-22:00"] * 10 + ["08:00-16:00"]),
        "details": "~".join(["travail|08:00-16:00", "", "ferie|Toussaint"] * 10 + [""]),
    },
    "tab5_maj_calendrier_jour": {"date": "2026-09-17", "payload": "travail|08:00-16:00;rdv|Dentiste 14:30"},
    "tab5_maj_zones": {"absentes": "clim,pot_4,pot_5"},
    "tab5_maj_emplacements": {"payload": "lumiere_1|on|180;salon|21.4|21.4;t02|on|128|FFB347;t01|open|45|;climr|16|30|0.5|°C|7|Salon;appuis|auto|rien|arcade;"
                                         "gestes|auto|reveil|appareil_suivant|auto|rangee_suivante|calendrier|"
                                         "mode_domo|auto|reglages|auto|ecoute|tv;defil|auto|fixe|auto|32;"
                                         "gauche|graphique|vocal|graphique;"
                                         "h00|21.4|21.4|;h01|on|180|FFB347;n00|612|612|;n01|off|nan|;"
                                         "r0|on|35;r1|on|128;"},
    "tab5_maj_tuiles": {"payload": "p0|Salon;t00|lum|lampadaire|d||Lampadaire;t01|vol||||Volet;t02|cap|thermometre||°C|Température;"
                                   "hp|0;hd|32;h00|cap|thermometre||°C|Salon|temperature;h01|lum|lampadaire|d||Lampadaire|;"
                                   "np|1;nd|24;n00|cap|co2||ppm|CO2|carbon_dioxide;n01|bin|porte||door|Porte|door;"
                                   "r0|son||||0|100|5|%|Volume;r1|lum|lampadaire|d|t00|0|255|25||Lampadaire;"},
    "tab5_maj_energie": {"payload": "3450|1180|-2270|78|0|24.5|°C|12.4"},
    "tab5_maj_energie_historique": {"vue": "heures", "debut": "2026-06-16",
                                    "valeurs": "0;0;0;0;0;0;0.05;0.4;1.1;1.9;2.6;3;3.1;;;;;;;;;;;"},
    # Humidité (ADR-0047) : 7e champ de l'en-tête, trois champs de plus par créneau ; avec
    # la prévision, ce que HA n'envoie jamais ensemble : tout le tracé à la fois.
    "tab5_maj_historique": {"cle": "serre", "vue": "jour", "entete": "Serre|2026-06-15T07:00|60|1485|18.2|0|62",
                            "mesures": "17.1,16.8,17.5,58,55,61;16.9,16.6,17.2;;,,,64,60,70;16.5,16.2,16.8,66,63,69",
                            "previsions": "1500,19.4;1560,20.8;1620,22.1"},
    # Lecteur de musique (ADR-0050) : la liste choisie et le lecteur montré, image relative
    # à HA (la base vient du client API).
    "tab5_maj_lecteur": {"lecteurs": "Salon|tv;Cuisine|speaker;Tablette|",
                         "etat": "1|Cuisine|speaker|playing|Bohemian Rhapsody|Queen|A Night at the Opera|Spotify|"
                                 "83|354|42|0|1|all|lspnvmar|/api/media_player_proxy/media_player.cuisine?token=x&cache=1"},
    # Caméras (ADR-0049) : la liste que le blueprint pousse à l'ouverture du popup.
    "tab5_maj_cameras": {"adresse": "http://homeassistant.local:8123",
                         "cameras": "Entrée|/api/camera_proxy/camera.entree?token=abc123;"
                                    "Jardin|/api/camera_proxy/camera.jardin?token=def456"},
    # Suivi de capteurs (ADR-0054) : une variation du jour, un écart, un capteur sans
    # courbe ni valeur, des points manquants.
    "tab5_maj_suivi": {"payload": "Tesla|382.7|USD|2.05|p|20,25,,31,40,38,52,61,58,70,74,100;"
                                  "Serre|21.4|°C|-1.5|a|90,80,70,60,50,40,30,20,10,0;"
                                  "CAC 40|7803.3301|EUR|-0.95|p|50,48,47,51,49,45;Compteur|unknown||||"},
}

NOMBRES = ["", "-1", "0", "15", "16", "31", "32", "99", "255", "256", "2147483647", "-2147483648",
           "2147483648", "4294967296", "9223372036854775807", "-9223372036854775808",
           "99999999999999999999", "1e30", "-1e30", "1e308", "1e309", "inf", "-inf", "nan", "-nan",
           "0x7fffffff", "1.5", "-0", " 12", "12abc", "+", "-", "."]
SPECIAUX = ["", " ", "|", ";", "~", "@", ",", "#", ":", "-", "||||||||", ";;;;;;;;", "~~~~", "@@@",
            "%s%s%s%s%n%n", "%x%x%x", "\n\r\t", "é°€😀", "\x00", "\x00|\x00;", "‮", "a" * 300,
            "Z" * 2100, "|" * 1100, ";" * 1100, "0|" * 700, "@" + "9" * 40]
DELIMS = "|;~,@:-"
JETON = re.compile(r"([|;~,@])")


def muter(graine: str, rng: random.Random) -> str:
    """Une mutation de la graine (1 à 3 opérations), ou une valeur spéciale."""
    if not graine or rng.random() < 0.12:
        return rng.choice(SPECIAUX + NOMBRES)
    s = graine
    for _ in range(rng.randint(1, 3)):
        parties = JETON.split(s)
        op = rng.randrange(9)
        if op == 0 and parties:  # un champ → nombre extrême
            i = rng.randrange(len(parties))
            parties[i] = rng.choice(NOMBRES)
        elif op == 1 and parties:  # un champ → spécial
            i = rng.randrange(len(parties))
            parties[i] = rng.choice(SPECIAUX)
        elif op == 2:  # retirer un délimiteur
            idx = [i for i, p in enumerate(parties) if JETON.fullmatch(p)]
            if idx:
                parties[rng.choice(idx)] = ""
        elif op == 3:  # doubler / ajouter des délimiteurs
            i = rng.randrange(len(parties) + 1)
            parties.insert(i, rng.choice(DELIMS) * rng.randint(1, 20))
        elif op == 4:  # tronquer
            s = s[: rng.randrange(len(s) + 1)]
            continue
        elif op == 5:  # répéter l'enregistrement (beaucoup de blocs)
            s = s * rng.randint(2, 60)
            continue
        elif op == 6:  # index de tête hors bornes
            s = re.sub(r"(^|;)(\d+)\|", lambda m: f"{m.group(1)}{rng.choice(['-1', '15', '16', '99', '255', '2147483648'])}|", s)
            continue
        elif op == 7:  # octets au hasard
            i = rng.randrange(len(s) + 1)
            bruit = "".join(chr(rng.choice([rng.randrange(32, 127), rng.randrange(0xA0, 0x2FF), 0x1F600, 0])) for _ in range(rng.randint(1, 12)))
            s = s[:i] + bruit + s[i:]
            continue
        else:  # échanger deux champs
            if len(parties) > 2:
                a, b = rng.randrange(len(parties)), rng.randrange(len(parties))
                parties[a], parties[b] = parties[b], parties[a]
        s = "".join(parties)
    return s[:TAILLE_MAX]


def donnees(graine: dict[str, str], noms: list[str], rng: random.Random) -> dict[str, str]:
    """Charge utile d'un appel : un ou plusieurs arguments mutés, les autres gardés valides."""
    sortie = {}
    cible = rng.choice(noms) if noms else None
    for nom in noms:
        valeur = graine.get(nom, "")
        if nom == cible or rng.random() < 0.3:
            valeur = muter(valeur, rng)
        if "url" in nom:
            # Jamais de vraie requête réseau depuis le runner : port fermé en local.
            valeur = "" if rng.random() < 0.3 else "http://127.0.0.1:9/" + re.sub(r"[^\w./%-]", "_", valeur)[:300]
        sortie[nom] = valeur
    return sortie


class Tablette:
    """Le processus de la tablette virtuelle, relancé après un plantage."""

    def __init__(self, programme: Path, dossier: Path):
        self.programme, self.dossier, self.n, self.proc = programme, dossier, 0, None
        self.journal: Journal | None = None

    def demarrer(self) -> None:
        self.n += 1
        chemin = self.dossier / f"tablette-{self.n}.log"
        sortie = open(chemin, "w", encoding="utf-8", errors="replace")  # noqa: SIM115 — vit avec le processus
        self.proc = subprocess.Popen([str(self.programme)], stdout=sortie, stderr=subprocess.STDOUT)
        self.journal = Journal(chemin)
        logger.info("Tablette lancée (n°%d, pid %d)", self.n, self.proc.pid)

    def rapports(self) -> list[str]:
        """Rapports ASan/UBSan écrits dans son journal depuis la lecture précédente."""
        return self.journal.nouveaux() if self.journal else []

    def vivante(self) -> bool:
        return self.proc is not None and self.proc.poll() is None

    def arreter(self) -> int | None:
        if self.proc is None:
            return None
        if self.proc.poll() is None:
            self.proc.terminate()
            try:
                self.proc.wait(10)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()
        return self.proc.returncode


async def connecter(hote: str, cle: str | None):
    from aioesphomeapi import APIClient

    for _ in range(60):
        try:
            if cle is None:
                cle = _lire_cle_demo() or await _donner_une_cle(hote)
            client = APIClient(hote, 6053, "", noise_psk=cle, client_info="Tab5 fuzz sanitizers")
            await asyncio.wait_for(client.connect(login=True), 10)
            return client, cle
        except SystemExit:
            raise
        except Exception as exc:  # port pas encore ouvert, tablette qui redémarre
            logger.debug("connexion : %s", exc)
            await asyncio.sleep(1)
    raise RuntimeError("tablette injoignable après 60 s")


async def vivante(client) -> bool:
    try:
        await asyncio.wait_for(client.device_info(), 5)
        return True
    except Exception:
        return False


async def fuzz(args) -> dict:
    rng = random.Random(args.graine)
    tablette = Tablette(Path(args.programme), Path(args.resultats))
    tablette.demarrer()
    await asyncio.sleep(3)
    client, cle = await connecter(args.hote, None)
    _, services = await client.list_entities_services()
    par_nom = {s.name: s for s in services}
    resultat = {
        "graine": args.graine, "appels_par_service": args.appels,
        "services_exposes": sorted(par_nom), "non_fuzzes": sorted(set(par_nom) - set(GRAINES)),
        "absents": sorted(set(GRAINES) - set(par_nom)), "services": {},
    }
    for nom in sorted(set(GRAINES) & set(par_nom)):
        info = par_nom[nom]
        noms = [a.name for a in info.args]
        stats = {"appels": 0, "plantages": [], "rapports": [], "coupures": 0}
        resultat["services"][nom] = stats
        recents: list[dict] = []
        debut = time.monotonic()
        logger.info("Service %s (%s)", nom, ", ".join(noms))
        for i in range(args.appels):
            data = donnees(GRAINES[nom], noms, rng)
            recents = (recents + [data])[-PING * 2:]
            try:
                await client.execute_service(info, data)
                stats["appels"] += 1
            except Exception as exc:
                logger.warning("%s : envoi refusé (%s)", nom, exc)
            await asyncio.sleep(PAUSE)
            if (i + 1) % PING and i + 1 != args.appels:
                continue
            ok = await vivante(client)
            textes = tablette.rapports()
            for texte in textes:
                stats["rapports"].append({"texte": texte, "derniers_appels": recents[-PING:]})
            if ok and tablette.vivante():
                continue
            # Plus de réponse : plantage ou simple coupure de connexion ?
            await asyncio.sleep(2)
            if tablette.vivante():
                stats["coupures"] += 1
                logger.warning("%s : connexion perdue, tablette vivante — reconnexion", nom)
                try:
                    await client.disconnect()
                except Exception:
                    pass
                client, cle = await connecter(args.hote, cle)
                continue
            code = tablette.arreter()
            textes = tablette.rapports()
            stats["plantages"].append({"code": code, "journal": f"tablette-{tablette.n}.log",
                                       "rapports": textes, "derniers_appels": recents})
            logger.error("%s : la tablette s'est arrêtée (code %s) — relance", nom, code)
            tablette.demarrer()
            await asyncio.sleep(3)
            client, cle = await connecter(args.hote, cle)
            if len(stats["plantages"]) >= 3:
                logger.error("%s : 3 plantages, service suivant", nom)
                break
        stats["secondes"] = round(time.monotonic() - debut, 1)
    try:
        await client.disconnect()
    except Exception:
        pass
    resultat["code_sortie_finale"] = tablette.arreter()
    fin = tablette.rapports()  # écrits après le dernier « ping »
    if fin:
        resultat["services"]["(fin)"] = {"appels": 0, "plantages": [], "coupures": 0,
                                         "rapports": [{"texte": t, "derniers_appels": []} for t in fin]}
    return resultat


def textes_des_rapports(resultat: dict) -> list[str]:
    textes = [r["texte"] for s in resultat["services"].values() for r in s["rapports"]]
    return textes + [t for s in resultat["services"].values() for pl in s["plantages"] for t in pl["rapports"]]


def en_echec(resultat: dict) -> bool:
    return any(s["rapports"] or s["plantages"] for s in resultat["services"].values())


def resume(resultat: dict) -> str:
    lignes = ["## Fuzzing des services (tablette virtuelle)", "",
              f"Graine {resultat['graine']}, {resultat['appels_par_service']} appels par service.", "",
              "| Service | Appels | Plantages | Rapports sanitizer | Coupures |", "|---|---|---|---|---|"]
    for nom, s in resultat["services"].items():
        lignes.append(f"| `{nom}` | {s['appels']} | {len(s['plantages'])} | {len(s['rapports'])} | {s['coupures']} |")
    if resultat["non_fuzzes"]:
        lignes += ["", "Services exposés non fuzzés (pas de graine) : " + ", ".join(f"`{n}`" for n in resultat["non_fuzzes"])]
    if resultat["absents"]:
        lignes += ["", "Services attendus absents : " + ", ".join(f"`{n}`" for n in resultat["absents"])]
    lignes += ["", f"Code de sortie de la tablette à la fin : {resultat['code_sortie_finale']}"]
    uniques = distincts(textes_des_rapports(resultat))
    if uniques:
        lignes += ["", f"Rapports distincts : {len(uniques)}"]
        for texte in uniques[:20]:
            lignes += ["", "```", texte, "```"]
    return "\n".join(lignes) + "\n"


def main() -> int:
    logging.basicConfig(level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--programme", required=True, help="exécutable de la tablette virtuelle")
    p.add_argument("--resultats", required=True, help="dossier des résultats")
    p.add_argument("--hote", default="127.0.0.1")
    p.add_argument("--appels", type=int, default=800)
    p.add_argument("--graine", type=int, default=20260930)
    args = p.parse_args()
    Path(args.resultats).mkdir(parents=True, exist_ok=True)
    resultat = asyncio.run(fuzz(args))
    (Path(args.resultats) / "fuzz.json").write_text(json.dumps(resultat, ensure_ascii=False, indent=1), encoding="utf-8")
    texte = resume(resultat)
    (Path(args.resultats) / "fuzz.md").write_text(texte, encoding="utf-8")
    print(texte)
    return 1 if en_echec(resultat) else 0


if __name__ == "__main__":
    sys.exit(main())
