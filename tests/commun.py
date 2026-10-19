# -*- coding: utf-8 -*-
"""[AI-CONTEXT] Boîte à outils commune des tests : à réutiliser avant d'en écrire une copie.

Constat OUT-3 de l'audit du 07/10/2026 : chaque fichier de test avait recopié son chargeur
YAML (25 `_Chargeur(yaml.SafeLoader)`), sa lecture de fichier (27 `_lire`), ses
`sys.path.insert` (39) et ses découpes `.split("- service: X")` de l'API du firmware.
Tout est ici, une fois :

- `REPO`, `TAB5`, `HA` : chemins (`pathlib.Path`) de la racine du dépôt, de `Tab5/` et de
  `HomeAssistant_Config/`. Le `sys.path` des outils (`tools/`, `tools/demo/`…) est posé
  une fois par `tests/conftest.py`.
- `source(nom)`, `sources(*motifs)` : un fichier du firmware par son nom seul, et les
  fichiers d'un motif, dans `Tab5/socle|ecran|jeux|paquets` (tools/tab5_sources.py).
- `lire(*chemin)` : texte UTF-8 d'un fichier, chemin relatif à la racine ou absolu.
- `fichiers_du_depot(dossier, motif)` : comme `rglob`, sans les fichiers que .gitignore
  écarte (le même jeu en local qu'en CI).
- Chargeurs YAML, tous sur le chargeur C de PyYAML (libyaml, ~10 fois plus rapide, mêmes
  objets que le chargeur Python sur les 169 YAML du dépôt, vérifié le 08/10/2026) :
  * `ChargeurSansBalises` : une balise (`!lambda`, `!include`, `!secret`, `!input`…) vaut None ;
  * `ChargeurBalisesBrutes` : une balise sur un scalaire vaut ce scalaire brut, sinon None ;
  * `ChargeurEntrees` : un `!input x` de blueprint vaut `{"!input": "x"}`.
- `jeton(nom)`, `avec_jetons(texte)` : un jeton de géométrie de
  `Tab5/paquets/tab5-ui-tokens.yaml` en entier, et un texte YAML où chaque `${jeton}` connu
  est remplacé par sa valeur (pour lire une hauteur posée par un jeton comme un nombre).
  Calculés pour une carte (`carte=`, Tab5 par défaut ; ADR-0061) : `CARTES`,
  `geometrie(carte)` (substitutions `carte_*`), `resoudre(valeur, contexte)` (une
  expression `${ … }` comme ESPHome la calcule), `taille_police(size, carte)`,
  `geometrie_cpp(carte)` (les constantes de tab5_geometrie.h pour la carte) et
  `vue_tab5(chemins)` (sans les packages propres aux autres cartes).
- `bloc_service(nom)` : le texte d'un service de `Tab5/paquets/tab5-api-logic.yaml`, de sa ligne
  `- service: nom` à la suivante (ou à la fin du bloc `api:`).
- `CacheJinja` : cache en mémoire du code compilé des modèles Jinja (`bytecode_cache=` d'un
  environnement, pour les modèles lus par un loader ; `.depuis_texte(env, texte)` pour un
  `env.from_string(texte)`). Une instance par configuration d'environnement (mêmes
  extensions, filtres et tests) : le code compilé n'est partagé qu'entre environnements
  bâtis par la même fonction.

Un test garde ses propres outils quand ils font autre chose (par exemple le `_Chargeur` du
blueprint de test_tuiles_blueprint.py, qui garde les `!input` comme objets `_Entree`).
"""
from __future__ import annotations

import re
import subprocess
from pathlib import Path

import jinja2
import yaml

REPO = Path(__file__).resolve().parent.parent
TAB5 = REPO / "Tab5"
HA = REPO / "HomeAssistant_Config"
API = TAB5 / "paquets" / "tab5-api-logic.yaml"

# Sources du firmware rangées dans Tab5/socle|ecran|jeux|paquets (08/10/2026) :
# `source("x.cpp")` trouve un fichier par son nom seul, `sources("*.cpp", …)` remplace
# un glob sur la racine de Tab5/ (qui ne trouverait plus rien). Voir tools/tab5_sources.py
# (tools/ est dans le sys.path posé par tests/conftest.py). `contrat()` : le texte de
# tab5_custom.h et des en-têtes de modules qu'il inclut (un par module depuis le 08/10/2026).
from tab5_sources import contrat, fichiers as sources, source  # noqa: E402,F401

# Chargeur C de PyYAML s'il est compilé (roues officielles), sinon le chargeur Python.
BaseChargeur = getattr(yaml, "CSafeLoader", yaml.SafeLoader)


def lire(*chemin) -> str:
    """Texte UTF-8 d'un fichier ; `lire("Tab5", "x.cpp")` ou `lire(chemin_absolu)`."""
    return REPO.joinpath(*chemin).read_text(encoding="utf-8")


class ChargeurSansBalises(BaseChargeur):
    """YAML ESPHome ou HA sans résoudre ses balises : chacune vaut None."""


ChargeurSansBalises.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


class ChargeurBalisesBrutes(BaseChargeur):
    """Balises ESPHome (!lambda, !extend, !include…) : leur valeur brute suffit."""


ChargeurBalisesBrutes.add_multi_constructor(
    "!", lambda chargeur, suffixe, noeud: chargeur.construct_scalar(noeud) if isinstance(noeud, yaml.ScalarNode) else None)


class ChargeurEntrees(BaseChargeur):
    """Blueprint HA : un `!input x` vaut `{"!input": "x"}`."""


ChargeurEntrees.add_constructor("!input", lambda chargeur, noeud: {"!input": chargeur.construct_scalar(noeud)})


def fichiers_du_depot(dossier: Path, motif: str = "*") -> list[Path]:
    """Fichiers de `dossier` qui correspondent à `motif`, SANS ceux que .gitignore écarte
    (HomeAssistant_Config/rendered/, placeholders.yaml…) : en local, le même jeu qu'en CI,
    où ils n'existent pas (constat HA-12 de l'audit du 07/10/2026). Un fichier neuf pas
    encore ajouté à git compte. Sans git : tout le dossier (rglob)."""
    try:
        sortie = subprocess.run(["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard", "--", str(dossier)],
                                cwd=REPO, capture_output=True, check=True).stdout
    except (OSError, subprocess.CalledProcessError):
        return sorted(dossier.rglob(motif))
    chemins = {REPO / p for p in sortie.decode("utf-8").split("\0") if p}
    return sorted(p for p in chemins if p.match(motif) and p.is_file())


def charger(*chemin, chargeur=ChargeurSansBalises):
    """Un YAML du dépôt (chemin comme `lire`), lu par `chargeur`."""
    return yaml.load(lire(*chemin), Loader=chargeur)


# Cartes (ADR-0061) : `tab5_carte` de tab5-ha-hmi.yaml, une Tab5/paquets/carte-<carte>.yaml
# et une Tab5/paquets/geometrie-<carte>.yaml chacune. La première est le défaut.
CARTES = ("tab5", "waveshare-7b")
_EXPRESSION = re.compile(r"\$\{([^}]*)\}")
_JINJA = jinja2.Environment()


def geometrie(carte: str = "tab5") -> dict[str, str]:
    """Substitutions `carte_*` d'une carte (Tab5/paquets/geometrie-<carte>.yaml)."""
    return {k: str(v) for k, v in charger("Tab5", "paquets", f"geometrie-{carte}.yaml")["substitutions"].items()}


def resoudre(valeur, contexte: dict):
    """Une valeur YAML comme ESPHome la substitue : chaque `${expression}` est évaluée par
    Jinja dans `contexte` (substitutions déjà connues) ; une valeur faite d'une seule
    expression garde son type (int pour `${ carte_ecran_l | int - 30 }`). Une variable
    inconnue laisse le texte tel quel."""
    if not isinstance(valeur, str):
        return valeur
    def evaluer(expression: str):
        return _JINJA.compile_expression(expression.strip(), undefined_to_none=False)(**contexte)

    seule = _EXPRESSION.fullmatch(valeur.strip())
    try:
        if seule:
            return evaluer(seule.group(1))
        return _EXPRESSION.sub(lambda m: str(evaluer(m.group(1))), valeur)
    except jinja2.exceptions.UndefinedError:
        return valeur


def _jetons(carte: str = "tab5") -> dict[str, str]:
    """Jetons de géométrie résolus pour la carte, avec ses substitutions `carte_*`, en texte
    comme ESPHome les voit (`| int` les convertit dans les expressions)."""
    contexte = dict(geometrie(carte))
    for k, v in charger("Tab5", "paquets", "tab5-ui-tokens.yaml")["substitutions"].items():
        contexte[k] = str(resoudre(v, contexte))
    return contexte


def jeton(nom: str, carte: str = "tab5") -> int:
    """Valeur entière d'un jeton de géométrie (`Tab5/paquets/tab5-ui-tokens.yaml`), calculé
    pour la carte (Tab5 par défaut : les valeurs au pixel de la mise en page)."""
    return int(_jetons(carte)[nom])


def avec_jetons(texte: str, carte: str = "tab5") -> str:
    """`texte` où chaque `${jeton}` de `tab5-ui-tokens.yaml` (et chaque `${carte_…}`) est
    remplacé par sa valeur, comme ESPHome le fait (les autres substitutions restent telles
    quelles)."""
    valeurs = _jetons(carte)
    return re.sub(r"\$\{(\w+)\}", lambda m: valeurs.get(m.group(1), m.group(0)), texte)


# Drapeau de compilation de tab5_geometrie.h → substitution de la carte qui le donne
# (build_flags de tab5-ha-hmi.yaml et de tab5-rendu-host.yaml, ADR-0061) ; police_pct est un
# jeton de tab5-ui-tokens.yaml calculé de carte_police_echelle (lot 3 du portage).
DRAPEAUX_CARTE = {"TAB5_ECRAN_L": "carte_ecran_l", "TAB5_ECRAN_H": "carte_ecran_h", "TAB5_ROTATION": "carte_rotation",
                  "TAB5_POLICE_PCT": "police_pct"}


def geometrie_cpp(carte: str | None = None) -> dict[str, int]:
    """Constantes entières de Tab5/socle/tab5_geometrie.h (`kEcranL`, `kCarteL`…), chacune
    calculée des précédentes comme le compilateur le fait. `carte=None` : avec les
    `#define` par défaut de l'en-tête (tests compilés sur PC) ; sinon avec les drapeaux
    `-DTAB5_…` que la carte passe (ses substitutions `carte_*`)."""
    texte = lire("Tab5", "socle", "tab5_geometrie.h")
    macros = {n: int(v) for n, v in re.findall(r"#define\s+(TAB5_\w+)\s+(-?\d+)", texte)}
    if carte is not None:
        g = _jetons(carte)
        macros.update({m: int(g[s]) for m, s in DRAPEAUX_CARTE.items()})
    valeurs = _constexpr_evaluer(texte, dict(macros))
    return {k: v for k, v in valeurs.items() if k not in macros}


def _constexpr_evaluer(texte: str, valeurs: dict[str, int]) -> dict[str, int]:
    """Les `constexpr int… NOM = expression;` de `texte`, dans l'ordre, chacune calculée des
    précédentes et de `valeurs` comme le compilateur le fait ; `valeurs` complété."""
    # largeur_carte(), hauteur_carte(), taille_carte() de tab5_geometrie.h (lot 4 du portage) :
    # une longueur du Tab5 ramenée au corps de la carte, arrondie en dessous comme en C++.
    def _ramene(corps: str, corps_tab5: str):
        def f(v: int) -> int:
            return -f(-v) if v < 0 else v * valeurs[corps] // valeurs[corps_tab5]
        return f
    fonctions = {"largeur_carte": _ramene("kCorpsW", "kCorpsWTab5"), "hauteur_carte": _ramene("kCorpsH", "kCorpsHTab5")}
    fonctions["taille_carte"] = lambda v: min(fonctions["largeur_carte"](v), fonctions["hauteur_carte"](v))
    # police_px() : une longueur qui suit les polices ramenées (kPolicePct, arrondie au plus proche).
    fonctions["police_px"] = lambda v: ((v * valeurs["kPolicePct"] + 50) // 100 if v >= 0
                                        else -((-v * valeurs["kPolicePct"] + 50) // 100))
    declarations = []
    for nom, expr in re.findall(r"constexpr\s+int(?:32_t)?\s+(\w+)\s*=\s*([^;]+);", texte):
        # Plusieurs constantes sur une ligne (`kNomX = 12, kNomL = largeur_carte(230)`).
        morceaux = re.split(r",\s*(\w+)\s*=(?!=)\s*", expr)
        declarations.append((nom, morceaux[0]))
        declarations += list(zip(morceaux[1::2], morceaux[2::2]))
    for nom, expr in declarations:
        # Un `a ? b : c` (un seul, le minimum d'une valeur du Tab5 et d'une autre) en Python.
        ternaire = re.fullmatch(r"([^?]+)\?([^:]+):(.+)", expr.strip())
        if ternaire:
            expr = "({1}) if ({0}) else ({2})".format(*ternaire.groups())
        # Division entière, comme en C++ (valeurs positives : tronquer = arrondir en dessous).
        expr = expr.replace("/", "//")
        try:
            valeurs[nom] = int(eval(expr, {"__builtins__": {}}, {**valeurs, **fonctions}))   # expressions du fichier
        except Exception:   # noqa: BLE001 — une constante d'un .cpp qui ne se calcule pas ici
            continue
    return valeurs


def constantes_cpp(texte: str, carte: str | None = None) -> dict[str, int]:
    """Constantes entières d'un .cpp de l'écran (`kVoletFenetreH`…), calculées sur la
    géométrie de la carte (tab5_geometrie.h, comme `geometrie_cpp`) : celles qui ne s'y
    calculent pas sont absentes. Mise en page des popups ramenée au corps (lot 4)."""
    return {k: v for k, v in _constexpr_evaluer(texte, dict(geometrie_cpp(carte))).items()}


def vue_tab5(chemins) -> list[Path]:
    """`chemins` sans les packages des autres cartes (`carte-<x>.yaml`, `geometrie-<x>.yaml`
    et `tab5-imu-absente.yaml`, ADR-0061) : le firmware tel que le Tab5 le compile, pour les
    gardes qui comptent une marque ou une broche par firmware."""
    autres = {f"{p}-{c}.yaml" for c in CARTES[1:] for p in ("carte", "geometrie")} | {"tab5-imu-absente.yaml"}
    return [c for c in chemins if Path(c).name not in autres]


def taille_police(valeur, carte: str = "tab5") -> int:
    """`size:` d'une police (Tab5/paquets/tab5-styles.yaml, tab5-themes.yaml) : un nombre, ou
    l'expression de la carte `${ (130 * (carte_police_echelle | float)) | round | int }`
    (ADR-0061), calculée pour `carte`."""
    return int(resoudre(valeur, geometrie(carte)))


_SERVICE = re.compile(r"^([ \t]*)- service: (\S+)[ \t]*$", re.M)


def bloc_service(nom: str, texte: str | None = None) -> str:
    """Texte d'un service de l'API du firmware, ligne `- service: nom` comprise, jusqu'au
    service suivant ou à la première clé de premier niveau qui suit (`provisioning:`…).
    AssertionError si le service n'existe pas."""
    texte = lire(API) if texte is None else texte
    debut = next((m for m in _SERVICE.finditer(texte) if m.group(2) == nom), None)
    assert debut, f"service {nom} introuvable dans {API.name}"
    suite = texte[debut.end():]
    fins = [m.start() for m in _SERVICE.finditer(suite) if m.group(1) == debut.group(1)]
    fins += [m.start() for m in re.finditer(r"^[^\s#]", suite, re.M)]
    return texte[debut.start():debut.end() + min(fins, default=len(suite))]


class CacheJinja(jinja2.BytecodeCache):
    """Code compilé des modèles, en mémoire, partagé par les environnements d'une même
    configuration. Clé : nom du modèle ET somme du source (un source modifié se recompile)."""

    def __init__(self):
        self._codes = {}

    def load_bytecode(self, bucket):
        code = self._codes.get((bucket.key, bucket.checksum))
        if code is not None:
            bucket.code = code

    def dump_bytecode(self, bucket):
        self._codes[(bucket.key, bucket.checksum)] = bucket.code

    def depuis_texte(self, env, texte: str):
        """Comme `env.from_string(texte)`, sans recompiler un texte déjà vu."""
        code = self._codes.get(("texte", texte))
        if code is None:
            code = self._codes[("texte", texte)] = env.compile(texte)
        return env.template_class.from_code(env, code, env.make_globals(None))


def envoi_tablettes(etape):
    """L'envoi d'une étape mise en boucle sur les tablettes (ADR-0063, plusieurs écrans) :
    `repeat: for_each` dont la séquence pose `tablette` puis appelle `esphome.{{ tablette }}_…` ;
    renvoie cette action. Toute autre étape est rendue telle quelle."""
    boucle = etape.get("repeat") if isinstance(etape, dict) else None
    if isinstance(boucle, dict) and "for_each" in boucle:
        for e in boucle.get("sequence") or []:
            if isinstance(e, dict) and str(e.get("action", "")).startswith("esphome."):
                return e
    return etape
