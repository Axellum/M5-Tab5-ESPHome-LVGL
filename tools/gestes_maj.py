#!/usr/bin/env python3
"""tools/gestes_maj.py — ce qu'une release demande à l'utilisateur, par façon d'avoir installé.

[AI-CONTEXT] Audit du 09/11/2026 (mises à jour et installation). La note de la 3.9.1 disait,
en une phrase : « les fichiers de `tab5_home_assistant.zip` et `tab5_hacs.zip`, ou une mise
à jour HACS, puis Recharger toute la configuration YAML et réécrire le tableau de bord ».
Un utilisateur de l'intégration HACS y a lu « télécharger et copier à chaque fois », alors que
l'intégration fait tout cela seule et que seul le tableau de bord (facultatif) reste à lui.
Cet outil remplace la phrase écrite à la main : il lit ce qui a CHANGÉ entre deux révisions
et dit, pour chaque façon d'avoir installé, quel geste est requis, recommandé, facultatif,
inutile ou déjà fait par l'intégration. Il ne juge pas de l'importance d'un correctif : la
note garde son texte (le « pourquoi »), l'outil donne les gestes et les fichiers.

Règles (une seule liste, `besoins()`) :
- firmware : fichiers de `Tab5/` ou `tab5-ha-hmi.yaml` changés (hors documentation, sources
  d'icônes, tablette virtuelle) ;
- fichiers HA : `HomeAssistant_Config/` — `packages/`, `custom_templates/`, `blueprints/`,
  `optionnel/` (les mêmes dossiers que `tools/publication/archive_ha.py`) ;
- l'ordre vient de la matrice du contrat (`tools/contrat_api.py`) : « compatible dans les deux
  sens » rend le firmware RECOMMANDÉ (c'est lui qui apporte les corrections) et les fichiers
  HA FACULTATIFS (rien ne casse sans l'un ou l'autre) ; tout autre ordre rend REQUIS ce qui a
  changé ;
- le tableau de bord n'est jamais requis : facultatif si la macro `tab5_dashboard.jinja` a
  changé, ou si des entités ont été ajoutées aux packages (lignes `unique_id:` ajoutées ou
  package nouveau : indice, pas preuve) ; inutile sinon ;
- l'intégration recharge le YAML seule et enchaîne le firmware quand la case « Mettre ensuite
  la tablette à jour » est cochée (ADR-0035) : installer sa mise à jour (= « Installer » +
  redémarrer HA) donne donc les fichiers HA, le rechargement et le firmware ; `tab5_hacs.zip`
  n'est jamais à télécharger à la main (c'est le fichier que lit HACS).

    python tools/gestes_maj.py v3.9.0 v3.9.1          # bloc « À faire », en français
    python tools/gestes_maj.py v3.9.0 v3.9.1 --en     # le même, pour le corps de la release
    python tools/gestes_maj.py v3.9.0                 # contre la révision courante (HEAD)
    python tools/gestes_maj.py v3.9.0 v3.9.1 --json   # les faits bruts

Lecture seule : `git diff` entre deux révisions et la matrice du contrat (PyYAML). Les gestes
sont comptés comme dans `docs/installation/updates.md` : un clic, une copie ou un collage = 1.
Trois totaux par profil : le requis seul, le requis et le recommandé, tout (facultatif compris).
"""
from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
from dataclasses import asdict, dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

# Dossiers de HomeAssistant_Config/ que l'archive tab5_home_assistant.zip embarque
# (tools/publication/archive_ha.py) et extensions des fichiers concernés.
DOSSIERS_HA = ("packages/", "custom_templates/", "blueprints/", "optionnel/")
EXTENSIONS_HA = (".yaml", ".jinja")
RACINE_HA = "HomeAssistant_Config/"
MACRO = RACINE_HA + "custom_templates/tab5_dashboard.jinja"
BLUEPRINT = RACINE_HA + "blueprints/"
# Hors firmware : tablette virtuelle de la CI, sources des icônes (la police compilée, elle,
# compte), modèle de configuration utilisateur.
HORS_FIRMWARE = ("Tab5/rendu/", "Tab5/fonts/icone_meteo/", "Tab5/user_entities.example.yaml")
# Entité ajoutée à un package : indice seulement (une entité de template a un `unique_id`).
UNIQUE_ID_AJOUTE = re.compile(r"^\+\s*unique_id\s*:", re.M)

REQUIS, RECOMMANDE, FACULTATIF, INUTILE, INCLUS = "requis", "recommande", "facultatif", "inutile", "inclus"

# Gestes (même décompte que docs/installation/updates.md).
G_INSTALLER = 2        # intégration : Mises à jour → Tab5 → Installer, puis redémarrer HA
G_FIRMWARE = 1         # entité « Firmware » → Installer
G_COPIER = 2           # zip : télécharger, copier dans config/
G_RECHARGER = 2        # zip : Toute la configuration YAML + reload_custom_templates
G_BLUEPRINT_URL = 1    # blueprint importé par son adresse : le réimporter
G_TABLEAU = 3          # étape 7 : modèle, copie, collage dans l'éditeur brut


@dataclass
class Changements:
    firmware: list[str] = field(default_factory=list)
    ha: list[str] = field(default_factory=list)
    macro: bool = False
    blueprint: bool = False
    entites_ajoutees: int = 0        # indice : lignes `unique_id:` ajoutées + packages nouveaux
    integration: list[str] = field(default_factory=list)


@dataclass
class Geste:
    profil: str                      # "integration" | "zip"
    cle: str
    statut: str
    nombre: int                      # gestes de l'utilisateur s'il le fait seul (0 si inutile)
    fichiers: list[str] = field(default_factory=list)


def est_firmware(chemin: str) -> bool:
    if chemin == "tab5-ha-hmi.yaml":
        return True
    if not chemin.startswith("Tab5/") or chemin.endswith((".md", ".svg")):
        return False
    return not chemin.startswith(HORS_FIRMWARE)


def est_fichier_ha(chemin: str) -> bool:
    if not chemin.startswith(RACINE_HA) or not chemin.endswith(EXTENSIONS_HA):
        return False
    return chemin[len(RACINE_HA):].startswith(DOSSIERS_HA)


def est_integration(chemin: str) -> bool:
    return chemin.startswith("custom_components/tab5/") and "__pycache__" not in chemin


def classer(statuts: dict[str, str], unique_id_ajoutes: int = 0) -> Changements:
    """`statuts` : {chemin : lettre de `git diff --name-status` (A, M, D…)}."""
    c = Changements(entites_ajoutees=unique_id_ajoutes)
    for chemin in sorted(statuts):
        if est_firmware(chemin):
            c.firmware.append(chemin)
        elif est_fichier_ha(chemin):
            c.ha.append(chemin)
            c.macro = c.macro or chemin == MACRO
            c.blueprint = c.blueprint or chemin.startswith(BLUEPRINT)
            if statuts[chemin].startswith("A") and "/packages/" in chemin:
                c.entites_ajoutees += 1
        elif est_integration(chemin):
            c.integration.append(chemin)
    return c


def besoins(c: Changements, ordre: str) -> list[Geste]:
    """Les gestes de chaque profil. `ordre` : clé de contrat_api.ETIQUETTES."""
    libre = ordre == "indifferent"
    sur_fw = INUTILE if not c.firmware else (RECOMMANDE if libre else REQUIS)
    tableau = FACULTATIF if (c.macro or c.entites_ajoutees) else INUTILE
    nb_tableau = G_TABLEAU if tableau == FACULTATIF else 0
    nb_fw = G_FIRMWARE if c.firmware else 0

    def sur(fichiers):
        return INUTILE if not fichiers else (FACULTATIF if libre else REQUIS)

    # Profil « intégration » : Paramètres → Mises à jour → Tab5. Le code de l'intégration
    # (custom_components/) n'est dans aucune archive à la main.
    fichiers_hacs = c.ha + c.integration
    g = [
        Geste("integration", "installer", sur(fichiers_hacs), G_INSTALLER if fichiers_hacs else 0, list(fichiers_hacs)),
        Geste("integration", "recharger", INCLUS if fichiers_hacs else INUTILE, 0),
        Geste("integration", "firmware", sur_fw, nb_fw, list(c.firmware)),
        Geste("integration", "tableau", tableau, nb_tableau),
    ]

    # Profil « à la main » : tab5_home_assistant.zip. Le rechargement suit la copie.
    copie = sur(c.ha)
    g += [
        Geste("zip", "copier", copie, G_COPIER if c.ha else 0, list(c.ha)),
        Geste("zip", "recharger", copie, G_RECHARGER if c.ha else 0),
    ]
    if c.blueprint:
        g.append(Geste("zip", "blueprint_url", FACULTATIF, G_BLUEPRINT_URL))
    g += [
        Geste("zip", "firmware", sur_fw, nb_fw, list(c.firmware)),
        Geste("zip", "tableau", tableau, nb_tableau),
    ]
    return g


def totaux(gestes: list[Geste], profil: str) -> tuple[int, int, int]:
    """(le requis seul, requis + recommandé, tout). Avec l'intégration, installer sa mise à
    jour enchaîne le firmware (case « Mettre ensuite la tablette à jour ») : pas de geste de plus."""
    du_profil = [x for x in gestes if x.profil == profil]

    def somme(statuts):
        choisis = [x for x in du_profil if x.statut in statuts]
        total = sum(x.nombre for x in choisis)
        cles = {x.cle for x in choisis}
        if profil == "integration" and {"installer", "firmware"} <= cles:
            total -= next(x.nombre for x in choisis if x.cle == "firmware")
        return total

    return (somme({REQUIS}), somme({REQUIS, RECOMMANDE}), somme({REQUIS, RECOMMANDE, FACULTATIF}))


# ── Texte ───────────────────────────────────────────────────────────────────

TEXTES = {
    "fr": {
        "titre": "**À faire en mettant à jour depuis la {depuis}** (contrat HA ↔ firmware : {ordre}).",
        "integration": "*Avec l'intégration* (« Tab5 — fichiers HA · HA files » dans Paramètres → Mises à jour) :",
        "zip": "*À la main* (`tab5_home_assistant.zip`) :",
        REQUIS: "requis", RECOMMANDE: "recommandé", FACULTATIF: "facultatif", INCLUS: "fait par l'intégration",
        "installer": "Mises à jour → Tab5 → Installer, puis redémarrer Home Assistant",
        "recharger_i": "Recharger le YAML",
        "recharger_z": "Recharger : Outils de développement → YAML → Toute la configuration YAML, puis l'action `homeassistant.reload_custom_templates` (ou redémarrer Home Assistant)",
        "firmware_i": "Firmware : enchaîné par l'intégration si « Mettre ensuite la tablette à jour » est coché ; sinon entité « Firmware » de la tablette → Installer",
        "firmware_z": "Firmware : entité « Firmware » de la tablette → Installer",
        "copier": "Télécharger `tab5_home_assistant.zip` et copier ses dossiers dans `config/`",
        "blueprint_url": "Blueprint importé depuis son adresse : l'importer de nouveau",
        "tableau": "Tableau de bord (étape 7 du guide)",
        "apporte": "apporte", "fichiers_n": "{n} fichier(s) de firmware",
        "rien_i": "Aucun fichier Home Assistant ne change dans cette version.",
        "total": "Gestes : {m} pour le requis, {r} avec le recommandé, {t} avec tout (facultatif compris).",
        "note_hacs": "`tab5_hacs.zip` n'est pas à télécharger : c'est le fichier que lit HACS.",
        "ordre_inconnu": "ordre non déterminé",
        "fin": "Le détail des étapes : `docs/installation/updates.md`.",
    },
    "en": {
        "titre": "**To do when upgrading from {depuis}** (HA ↔ firmware contract: {ordre}).",
        "integration": "*With the integration* (« Tab5 — fichiers HA · HA files » in Settings → Updates):",
        "zip": "*By hand* (`tab5_home_assistant.zip`):",
        REQUIS: "required", RECOMMANDE: "recommended", FACULTATIF: "optional", INCLUS: "done by the integration",
        "installer": "Updates → Tab5 → Install, then restart Home Assistant",
        "recharger_i": "Reload the YAML",
        "recharger_z": "Reload: Developer tools → YAML → All YAML configuration, then the action `homeassistant.reload_custom_templates` (or restart Home Assistant)",
        "firmware_i": "Firmware: chained by the integration if « Then update the tablet » is ticked; otherwise the tablet's « Firmware » entity → Install",
        "firmware_z": "Firmware: the tablet's « Firmware » entity → Install",
        "copier": "Download `tab5_home_assistant.zip` and copy its folders into `config/`",
        "blueprint_url": "Blueprint imported from its address: import it again",
        "tableau": "Dashboard (step 7 of the guide)",
        "apporte": "brings", "fichiers_n": "{n} firmware file(s)",
        "rien_i": "No Home Assistant file changes in this version.",
        "total": "Gestures: {m} for what is required, {r} with what is recommended, {t} for everything (optional included).",
        "note_hacs": "`tab5_hacs.zip` is not for you to download: it is the file HACS reads.",
        "ordre_inconnu": "order not determined",
        "fin": "The steps in detail: `docs/installation/updates.md`.",
    },
}


def _ligne(t: dict, x: Geste) -> str | None:
    if x.statut == INUTILE:
        return None
    if x.cle == "recharger":
        texte = t["recharger_i"] if x.profil == "integration" else t["recharger_z"]
    elif x.cle == "firmware":
        texte = t["firmware_i"] if x.profil == "integration" else t["firmware_z"]
    else:
        texte = t[x.cle]
    detail = ""
    if x.cle == "firmware":
        detail = f" ({t['fichiers_n'].format(n=len(x.fichiers))})"
    elif x.fichiers:
        noms = ", ".join(f"`{f.removeprefix(RACINE_HA)}`" for f in x.fichiers[:6])
        reste = f" +{len(x.fichiers) - 6}" if len(x.fichiers) > 6 else ""
        detail = f" ({t['apporte']} {noms}{reste})"
    return f"- {texte}{detail} — {t[x.statut]}."


def rendre(c: Changements, ordre: str, depuis: str, langue: str = "fr") -> str:
    from contrat_api import ETIQUETTES  # import tardif : PyYAML n'est utile qu'ici
    t = TEXTES[langue]
    libelle = ETIQUETTES[langue].get(ordre, t["ordre_inconnu"])
    gestes = besoins(c, ordre)
    sortie = [t["titre"].format(depuis=depuis.lstrip("v"), ordre=libelle), ""]
    for profil in ("integration", "zip"):
        sortie.append(t[profil])
        sortie.extend(ligne for x in gestes if x.profil == profil and (ligne := _ligne(t, x)))
        if profil == "integration" and not (c.ha or c.integration):
            sortie.append(f"- {t['rien_i']}")
        m, r, tout = totaux(gestes, profil)
        sortie += [f"- {t['total'].format(m=m, r=r, t=tout)}", ""]
    sortie += [t["note_hacs"], t["fin"]]
    return "\n".join(sortie) + "\n"


# ── Git ─────────────────────────────────────────────────────────────────────

def _git(*args: str) -> str:
    r = subprocess.run(["git", *args], cwd=REPO, capture_output=True, text=True,
                       encoding="utf-8", errors="replace")
    if r.returncode != 0:
        raise SystemExit(f"git {' '.join(args)} : {r.stderr.strip() or 'échec'}")
    return r.stdout


def statuts_du_diff(depuis: str, courant: str) -> dict[str, str]:
    sortie = _git("diff", "--name-status", "--no-renames", f"{depuis}..{courant}")
    statuts = {}
    for ligne in sortie.splitlines():
        lettre, _, chemin = ligne.partition("\t")
        if chemin:
            statuts[chemin.strip()] = lettre
    return statuts


def unique_id_ajoutes(depuis: str, courant: str) -> int:
    diff = _git("diff", "-U0", "--no-renames", f"{depuis}..{courant}", "--", RACINE_HA + "packages")
    return len(UNIQUE_ID_AJOUTE.findall(diff))


def main(argv=None) -> int:
    p = argparse.ArgumentParser(description=(__doc__ or "").split("\n")[0])
    p.add_argument("depuis", help="révision de départ (tag publié, ex. v3.9.0)")
    p.add_argument("courant", nargs="?", default="HEAD", help="révision d'arrivée (défaut : HEAD)")
    p.add_argument("--en", action="store_true", help="en anglais (corps de la release GitHub)")
    p.add_argument("--json", action="store_true", help="les faits bruts, en JSON")
    args = p.parse_args(argv)
    reconfigurer = getattr(sys.stdout, "reconfigure", None)
    if reconfigurer:
        reconfigurer(encoding="utf-8")
    import contrat_api  # même dossier que ce script
    c = classer(statuts_du_diff(args.depuis, args.courant), unique_id_ajoutes(args.depuis, args.courant))
    ordre = contrat_api.matrice(args.depuis, args.courant).ordre
    if args.json:
        print(json.dumps({"depuis": args.depuis, "courant": args.courant, "ordre": ordre,
                          "changements": asdict(c), "gestes": [asdict(x) for x in besoins(c, ordre)]},
                         ensure_ascii=False, indent=2))
        return 0
    print(rendre(c, ordre, args.depuis, "en" if args.en else "fr"), end="")
    return 0


if __name__ == "__main__":
    sys.exit(main())
