# -*- coding: utf-8 -*-
"""Tableau de bord Home Assistant de la tablette (HomeAssistant_Config/custom_templates/tab5_dashboard.jinja).

Cette macro est rendue par Home Assistant (Outils de développement → Modèle, une ligne)
puis son résultat collé dans un tableau de bord : aucune compilation ne la relit. Ce fichier le fait de deux façons :

- à la lecture : chaque entité que le modèle cherche sur la tablette existe dans le
  firmware (nom → entity_id comme HA), chaque entité du firmware a sa carte (sauf les
  exceptions listées ici, avec leur raison), chaque entité ou automatisation de package
  citée est définie par un package ;
- au rendu : le modèle est rendu dans le bac à sable de Jinja, comme dans HA, avec une
  fausse maison (pièce « bureau » ajoutée en cours de route : préfixes mélangés comme sur
  une vraie tablette ; selects de HA dans une autre langue que le français). Le résultat
  doit être du YAML de tableau de bord dont chaque entité existe. Seules les fonctions de
  modèle que le fichier appelle sont imitées. Le job « Installation dans un HA neuf » le
  rend aussi dans un vrai HA, avec la tablette virtuelle."""
import pathlib
import re
import sys
import unicodedata

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

REPO = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools" / "installation_ha"))

import verifier_installation as verifier  # noqa: E402  (entites_du_tableau : même lecture que le job « HA neuf »)
MODELE = REPO / "HomeAssistant_Config" / "custom_templates" / "tab5_dashboard.jinja"
HA_DIR = REPO / "HomeAssistant_Config"

# Entités du firmware volontairement sans carte : (domaine, slug du nom) → raison.
SANS_CARTE = {
    ("number", "volume"): "doublon du volume du lecteur multimédia (« inconnu » tant qu'il n'a pas bougé)",
}
# Aides de l'auteur hors packages : une carte seulement si elles existent.
HORS_PACKAGES = {"input_text.tab5_annonce_vocale", "script.tab5_annonce_vocale"}

APPAREIL = "m5stack_tab5_home_assistant_hmi"
# Langues de l'écran (option du select « Langue ») traduites par la table TRADUCTIONS du modèle.
LANGUES = {"Deutsch": "de", "Nederlands": "nl", "Español": "es", "Italiano": "it", "Türkçe": "tr"}
DEFINITION_DE_T = "{%- macro t(francais, anglais) -%}"


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


def _slug(nom: str) -> str:
    """entity_id tiré d'un nom par Home Assistant (util.slugify : accents retirés,
    ponctuation en « _ »)."""
    texte = unicodedata.normalize("NFKD", nom).encode("ascii", "ignore").decode().lower()
    return re.sub(r"[^a-z0-9]+", "_", texte).strip("_")


def _texte():
    return MODELE.read_text(encoding="utf-8")


# ─────────────────────────────────────────────────────────────────────────────
# Ce que le modèle cherche, ce que le firmware et les packages définissent
# ─────────────────────────────────────────────────────────────────────────────

def _table_entites():
    """(clé, domaine, fin) de la table ENTITES du modèle."""
    bloc = re.search(r"set ENTITES = \[(.*?)\] -%\}", _texte(), re.S)
    assert bloc, "table ENTITES introuvable"
    return re.findall(r"\('(\w+)', '(\w+)', '(\w*)'\)", bloc.group(1))


# Domaine ESPHome → domaine HA (`datetime` : selon son `type`).
_DOMAINES = {"sensor": "sensor", "binary_sensor": "binary_sensor", "text_sensor": "sensor", "switch": "switch",
             "select": "select", "number": "number", "button": "button", "light": "light", "text": "text",
             "update": "update", "media_player": "media_player", "datetime": None}


def _entites_du_firmware():
    """{(domaine HA, slug du nom) : fichier} des entités publiées par le firmware, sous-capteurs
    nommés compris (debug, wifi_info). Ni la tablette virtuelle (Tab5/rendu/), ni les langues,
    ni user_entities*.yaml (identifiants réels de l'auteur, jamais lus)."""
    fichiers = [REPO / "tab5-ha-hmi.yaml"] + [
        f for f in sorted((REPO / "Tab5").rglob("*.yaml"))
        if not ({"rendu", "lang"} & set(f.relative_to(REPO / "Tab5").parts[:-1]))
        and not f.name.startswith("user_entities") and "tts_library" not in str(f)]
    entites = {}
    for fichier in fichiers:
        contenu = yaml.load(fichier.read_text(encoding="utf-8"), Loader=_Chargeur)
        if not isinstance(contenu, dict):
            continue
        for cle, liste in contenu.items():
            if cle not in _DOMAINES or not isinstance(liste, list):
                continue
            for e in (e for e in liste if isinstance(e, dict)):
                for s in [e] + [v for v in e.values() if isinstance(v, dict)]:
                    if isinstance(s.get("name"), str) and not s.get("internal"):
                        domaine = _DOMAINES[cle] or {"time": "time", "date": "date"}.get(str(e.get("type")), "datetime")
                        entites[(domaine, _slug(s["name"]))] = fichier.name
    return entites


def _packages():
    fichiers = sorted((HA_DIR / "packages").glob("*.yaml")) + sorted((HA_DIR / "optionnel").glob("*.yaml"))
    return [yaml.load(f.read_text(encoding="utf-8"), Loader=_Chargeur) or {} for f in fichiers]


def _entites_des_packages():
    """entity_id des entités définies par les packages (même lecture que test_installation_ha)."""
    definies = set()
    for paquet in _packages():
        for domaine in ("input_text", "input_select", "input_boolean", "script"):
            definies |= {f"{domaine}.{cle}" for cle in (paquet.get(domaine) or {})}
        for bloc in paquet.get("template") or []:
            for domaine in ("sensor", "binary_sensor", "select", "weather"):
                for entite in bloc.get(domaine) or []:
                    definies.add(entite.get("default_entity_id") or f"{domaine}.{_slug(entite['name'])}")
    return definies


def _automatisations_des_packages():
    """{id : alias} des automatisations des packages."""
    return {a["id"]: a.get("alias", a["id"]) for paquet in _packages() for a in (paquet.get("automation") or [])}


def test_chaque_entite_cherchee_existe_dans_le_firmware():
    """Une entité renommée dans le firmware ferait disparaître sa carte sans bruit."""
    firmware = _entites_du_firmware()
    for cle, domaine, fin in _table_entites():
        trouvees = [s for d, s in firmware if d == domaine and (not fin or s == fin or s.endswith("_" + fin))]
        if domaine == "assist_satellite":
            continue  # ajouté par l'intégration ESPHome de HA, pas par le firmware
        assert len(trouvees) == 1, f"{cle} ({domaine}, …_{fin}) : {trouvees or 'absente du firmware'}"


def test_chaque_entite_du_firmware_a_sa_carte():
    """« Tous les réglages de la tablette sont-ils sur le tableau de bord ? » : oui, sauf
    SANS_CARTE. Une entité ajoutée au firmware doit y gagner sa carte (ou sa raison)."""
    cherchees = [(d, f) for _, d, f in _table_entites()]
    oubliees = {(d, s): f for (d, s), f in _entites_du_firmware().items()
                if not any(d == dc and (not fc or s == fc or s.endswith("_" + fc)) for dc, fc in cherchees)}
    assert set(oubliees) == set(SANS_CARTE), sorted(set(oubliees) ^ set(SANS_CARTE))


def test_entites_et_automatisations_des_packages_existent():
    """Entités fixes testées par `… in present` et ids de la table IDS : un nom changé dans un
    package ferait disparaître la carte sans bruit."""
    texte = _texte()
    citees = set(re.findall(r"'((?:input_boolean|input_select|input_text|select|sensor|binary_sensor|script)"
                            r"\.tab5_\w+)'", texte))
    assert citees, "aucune entité de package citée"
    assert not citees - _entites_des_packages() - HORS_PACKAGES, sorted(citees - _entites_des_packages() - HORS_PACKAGES)
    bloc = re.search(r"set IDS = \[(.*?)\] -%\}", texte, re.S)
    assert bloc, "table IDS introuvable"
    ids = set(re.findall(r"'(\w+)'", bloc.group(1)))
    assert ids and not ids - set(_automatisations_des_packages()), sorted(ids - set(_automatisations_des_packages()))


# ─────────────────────────────────────────────────────────────────────────────
# Fausse maison et imitation du Jinja de Home Assistant
# ─────────────────────────────────────────────────────────────────────────────

class Etat:
    def __init__(self, entity_id, state="on", **attributes):
        self.entity_id = entity_id
        self.state = state
        self.attributes = attributes
        self.domain = entity_id.split(".", 1)[0]
        self.name = attributes.get("friendly_name", entity_id)


class Etats:
    """`states` de HA : appelable (états d'une entité), itérable (tous les états) et
    `states.<domaine>`."""

    def __init__(self, etats):
        self.d = {e.entity_id: e for e in etats}

    def __call__(self, entity_id):
        e = self.d.get(entity_id)
        return e.state if e else "unknown"

    def __iter__(self):
        return iter(self.d.values())

    def __getattr__(self, domaine):
        if domaine.startswith("_") or domaine == "d":
            raise AttributeError(domaine)
        return [e for e in self.d.values() if e.domain == domaine]

    def attr(self, entity_id, nom):
        e = self.d.get(entity_id)
        return e.attributes.get(nom) if e else None


# Selects ajoutés par l'intégration ESPHome de HA : leur entity_id suit la langue de HA.
# Une langue inventée ici, pour prouver que le modèle les trouve à leurs options.
SELECTS_DE_HA = {
    "assistent": ["preferred", "Domotique", "Discussion LLM"],
    "assistent_2": ["preferred", "Domotique", "Discussion LLM"],
    "aktivierungswort": ["no_wake_word", "Okay Nabu"],
    "aktivierungswort_2": ["no_wake_word", "Okay Nabu"],
    "sprechende_erkennung": ["default", "relaxed", "aggressive"],
}


def _tablette(appareil="tab5", connectee=True, piece="bureau"):
    """États d'une tablette : les entités du firmware (celles du réveil, ajoutées après la
    pièce, prennent son préfixe), le satellite et les selects de HA."""
    etats = []
    for (domaine, slug), fichier in _entites_du_firmware().items():
        prefixe = f"{piece}_{APPAREIL}" if fichier == "tab5-alarm.yaml" else APPAREIL
        if appareil != "tab5":
            prefixe += f"_{appareil}"
        etat = "Français" if slug == "langue" else ("on" if connectee else "off") if slug == "ha_api_status" else "on"
        etats.append(Etat(f"{domaine}.{prefixe}_{slug}", etat))
    suffixe = "" if appareil == "tab5" else f"_{appareil}"
    etats.append(Etat(f"assist_satellite.{APPAREIL}{suffixe}_assist_satellite", "idle"))
    etats += [Etat(f"select.{APPAREIL}{suffixe}_{nom}", options[0], options=options)
              for nom, options in SELECTS_DE_HA.items()]
    return {appareil: etats}


def _maison(tablettes, packages=True, aides=False, langue="Français"):
    etats = [e for liste in tablettes.values() for e in liste]
    if packages:
        etats += [Etat(e, "Aucun" if e.startswith("select.") else "on") for e in sorted(_entites_des_packages())]
        etats += [Etat(f"automation.{_slug(alias)}", "on", id=id_)
                  for id_, alias in _automatisations_des_packages().items()]
        etats.append(Etat("automation.une_autre", "on", id="1790000000000"))
        # Celle du blueprint « Tab5 — emplacements », nommée par l'utilisateur.
        etats.append(Etat("automation.ecran_du_bureau", "on", id="1700000000001",
                          friendly_name="Tab5 — emplacements de l'écran"))
    if aides:
        etats += [Etat(e, "") for e in sorted(HORS_PACKAGES)]
    for e in etats:
        if e.entity_id.endswith("_langue") and e.domain == "select":
            e.state = langue
    return Etats(etats), tablettes


def _rendre(maison, noter=None, **variables):
    """Rend la macro dans la maison donnée. `noter(français, anglais)`, s'il est donné, reçoit
    chaque appel de t() (la ligne de définition de t() est complétée à la volée)."""
    etats, tablettes = maison
    appareils = {e.entity_id: nom for nom, liste in tablettes.items() for e in liste}
    modeles = {**{e: "tab5-ha-hmi" for e in appareils}, "binary_sensor.autre_esp_ha_api_status": "esp32-autre"}
    texte = _texte()
    if noter:
        assert texte.count(DEFINITION_DE_T) == 1, "définition de t() changée : mettre DEFINITION_DE_T à jour"
        texte = texte.replace(DEFINITION_DE_T, DEFINITION_DE_T + "{{ noter(francais, anglais) }}")
    # custom_templates/ de HA : la macro s'importe par son nom de fichier.
    env = ImmutableSandboxedEnvironment(extensions=["jinja2.ext.loopcontrols", "jinja2.ext.do"],
                                        undefined=jinja2.StrictUndefined,
                                        loader=jinja2.FunctionLoader(lambda nom: texte if nom == MODELE.name else None))
    env.globals["noter"] = noter or (lambda *_: "")
    env.globals.update(
        states=etats, state_attr=etats.attr, is_state=lambda e, s: etats(e) == s,
        integration_entities=lambda domaine: list(modeles) if domaine == "esphome" else [],
        device_attr=lambda e, nom: modeles.get(e) if nom == "model" else None,
        device_id=lambda e: appareils.get(e),
        config_entry_id=lambda e: f"entree_{appareils[e]}" if e in appareils else None,
        device_entities=lambda appareil: [e for e, a in appareils.items() if a == appareil],
    )
    env.tests.update(match=lambda v, motif, ignorecase=False: bool(re.match(motif, str(v), re.I if ignorecase else 0)))
    if not variables:
        return env.from_string(verifier.APPEL_TABLEAU).render()  # la ligne de la doc, telle quelle
    arguments = ", ".join(f"{nom}={valeur!r}" for nom, valeur in variables.items())
    return env.from_string(verifier.APPEL_TABLEAU.replace("tab5_dashboard()", f"tab5_dashboard({arguments})")).render()


def _toutes_les_cartes(tableau):
    return [c for v in tableau["views"] for s in v["sections"] for c in s["cards"]]


def _cartes(noeud):
    if isinstance(noeud, dict):
        return (1 if "type" in noeud and "cards" not in noeud and "sections" not in noeud else 0) + sum(
            _cartes(v) for v in noeud.values())
    if isinstance(noeud, list):
        return sum(_cartes(v) for v in noeud)
    return 0


@pytest.mark.parametrize("langue", ["Français", "English", *LANGUES])
def test_rendu_complet(langue):
    table = _traductions()

    def tr(francais, anglais):
        return francais if langue == "Français" else anglais if langue == "English" else table[francais][LANGUES[langue]]

    maison = _maison(_tablette(), aides=True, langue=langue)
    sortie = _rendre(maison, adresse="ma-tablette")
    tableau = yaml.safe_load(sortie)
    assert [v["path"] for v in tableau["views"]] == ["tab5", "tab5-reglages", "tab5-sante"]
    references = verifier.entites_du_tableau(tableau)
    etats = maison[0]
    assert not {e for e in references if e not in etats.d}, "entités citées absentes de la maison"
    # Chaque entité de la tablette a sa carte, sauf SANS_CARTE.
    tablette = {e.entity_id for e in maison[1]["tab5"]}
    sans = {e for e in tablette if any(e.startswith(f"{d}.") and e.endswith(f"_{s}") for d, s in SANS_CARTE)}
    assert tablette - sans <= references, sorted(tablette - sans - references)
    # Chaque liste « Tab5 · … » des packages a sa carte (même règle que le job « HA neuf »).
    listes = {e for e in etats.d if verifier.LISTES_TAB5.match(e)}
    assert len(listes) >= 10 and listes <= references, sorted(listes - references)
    # Les selects de HA sont à leur place : le premier pipeline n'est pas le second.
    reglages = yaml.safe_dump(tableau["views"][1], allow_unicode=True)
    assert reglages.index("_assistent\n") < reglages.index("_aktivierungswort\n") < reglages.index("_assistent_2\n")
    # Rien n'est resté du Jinja du modèle (hors Markdown « En bref », rendu par la carte).
    sans_markdown = re.sub(r"content: \|-\n(?:\s{14}.*\n|\n)*", "", sortie)
    assert "{{" not in sans_markdown and "{%" not in sans_markdown and "None" not in sortie
    chemins = re.findall(r"navigation_path: (\S+)", sortie) + re.findall(r"back_path: (\S+)", sortie)
    assert chemins and all(c.startswith(("/ma-tablette/", "/config/")) for c in chemins), chemins
    titres = [v["title"] for v in tableau["views"]]
    assert titres == ["Tab5", tr("Réglages Tab5", "Tab5 settings"), tr("Santé Tab5", "Tab5 health")]
    assert _cartes(tableau) > 120
    # L'automatisation du blueprint : sa tuile, et le lien vers son éditeur.
    assert "automation.ecran_du_bureau" in references and "/config/automation/edit/1700000000001" in chemins
    # Réglages : la page de la tablette et sa connexion ESPHome, l'énergie solaire et ses liens.
    assert {"/config/devices/device/tab5", "/config/integrations/integration/esphome#config_entry=entree_tab5",
            "/config/energy", "/config/voice-assistants/assistants"} <= set(chemins), chemins
    reglages = yaml.safe_dump(tableau["views"][1], allow_unicode=True)
    assert tr("Énergie solaire", "Solar energy") in reglages
    assert tr("Puissance crête des panneaux", "Panel peak power") in reglages
    assert "tab5_energie" not in reglages, "package présent : pas d'avertissement"
    # Santé : guide par symptôme, liens de HA, « En bref » avec les gardes de santé.
    sante = sortie[sortie.index("path: tab5-sante"):]
    assert {"/config/logs", "/config/repairs"} <= set(chemins), chemins
    assert tr("Quand quelque chose cloche", "When something is wrong") in sante
    # Cinq gardes depuis la 3.8 (la garde de is_primary_active est retirée avec lui).
    assert re.search(r"\['automation\.[^']+'(, 'automation\.[^']+'){4}\] \| select\('is_state', 'off'\)", sante)
    # Chaque tuile ou raccourci écrit sa largeur, sauf une tuile à commande en ligne
    # (12 colonnes au minimum) : sans elle, le frontend lui donne 6 colonnes sur 12.
    sans_largeur = [c.get("entity") or c.get("label") for c in _toutes_les_cartes(tableau)
                    if c["type"] in ("tile", "shortcut") and "grid_options" not in c
                    and not (c.get("features_position") == "inline" and c.get("features"))]
    assert not sans_largeur, sans_largeur


def test_langue_forcee_et_adresse_par_defaut():
    sortie = _rendre(_maison(_tablette()), langue="English")
    assert "title: \"Tab5 settings\"" in sortie and "/dashboard-tab5/tab5-reglages" in sortie


def test_tablette_seule_sans_package():
    """Sans les packages, il reste les cartes de la tablette, sans entité absente."""
    maison = _maison(_tablette(), packages=False)
    tableau = yaml.safe_load(_rendre(maison))
    references = verifier.entites_du_tableau(tableau)
    assert references and not {e for e in references if e not in maison[0].d}
    assert not any("tab5_" in e and APPAREIL not in e for e in references), "carte d'un package absent"
    assert "annonce_vocale" not in str(tableau)
    assert "/config/blueprint/dashboard" in str(tableau), "sans automatisation du blueprint, le lien va aux blueprints"
    assert "Package tab5_energie absent" in str(tableau), "sans le package, la section Énergie le dit"
    assert "pas encore créés" in str(tableau), "« En bref » dit que les emplacements manquent"


def _ancre_github(titre: str) -> str:
    """Ancre d'un titre Markdown sur GitHub : minuscules, ponctuation retirée (lettres
    accentuées gardées), espaces en tirets."""
    return re.sub(r"[^\w\- ]", "", titre.strip().lower()).replace(" ", "-")


def test_liens_de_la_doc_vers_des_titres_existants():
    """Chaque lien du tableau de bord vers docs/ vise un fichier et un titre qui existent,
    en français comme en anglais : un titre renommé casserait le lien sans bruit."""
    liens = re.findall(r"doc\('([^']+)', '([^']*)', '([^']*)'\)", _texte())
    assert len(liens) >= 8 and {"installation.md", "troubleshooting.md", "performance.md"} <= {f for f, *_ in liens}, liens
    manquantes = []
    for fichier, *ancres_du_lien in liens:
        doc = (REPO / "docs" / fichier).read_text(encoding="utf-8")
        ancres = {_ancre_github(m) for m in re.findall(r"^#{1,4} (.+)$", doc, re.M)}
        manquantes += [f"{fichier}#{a}" for a in ancres_du_lien if a and a not in ancres]
    assert not manquantes, manquantes


def test_sans_tablette():
    """Pas de tablette : une seule ligne de commentaire, pas un tableau de bord vide."""
    sortie = _rendre(_maison({}))
    assert sortie.startswith("# Tablette introuvable") and yaml.safe_load(sortie) is None


def test_deux_tablettes_celle_qui_est_connectee():
    """Deux tablettes du même modèle : celle dont la liaison avec HA est active."""
    tablettes = {**_tablette("tab5", connectee=False), **_tablette("deux", connectee=True)}
    tableau = yaml.safe_load(_rendre(_maison(tablettes)))
    references = verifier.entites_du_tableau(tableau)
    assert any(e.startswith("binary_sensor.") and "_deux_" in e for e in references)
    assert not any(APPAREIL in e and "_deux_" not in e for e in references)


# ─────────────────────────────────────────────────────────────────────────────
# Langues : allemand, néerlandais, espagnol, italien et turc par la table TRADUCTIONS
# ─────────────────────────────────────────────────────────────────────────────

def _traductions():
    """Table TRADUCTIONS du modèle (fin du fichier), lue sur le module sans appeler la macro."""
    env = jinja2.Environment(loader=jinja2.FunctionLoader(lambda nom: _texte()))
    return env.get_template(MODELE.name).module.TRADUCTIONS


def _textes_de_t():
    """{français : anglais} de chaque appel de t(), dans des maisons qui passent par toutes les
    branches : complète, sans package, sans tablette, deux tablettes."""
    vus = {}

    def noter(francais, anglais):
        vus[francais] = anglais
        return ""

    for maison in (_maison(_tablette(), aides=True), _maison(_tablette(), packages=False), _maison({}),
                   _maison({**_tablette("tab5", connectee=False), **_tablette("deux")})):
        _rendre(maison, noter=noter, adresse="ma-tablette")
    return vus


def test_chaque_texte_a_ses_cinq_traductions():
    """Un texte ajouté avec t() sans son entrée dans TRADUCTIONS s'afficherait en anglais ; une
    entrée dont le français a changé ne servirait plus."""
    textes, table = _textes_de_t(), _traductions()
    # Un appel littéral jamais rendu par les fausses maisons échapperait au contrôle.
    corps = _texte()[:_texte().index("{%- set TRADUCTIONS")]
    litteraux = {a or b for a, b in re.findall(r"""\bt\((?:'([^']*)'|"([^"]*)"),""", corps)}
    assert litteraux <= set(textes), sorted(litteraux - set(textes))
    incompletes = sorted(f for f in textes if set(table.get(f, {})) != set(LANGUES.values()))
    assert not incompletes, incompletes
    orphelines = sorted(set(table) - set(textes))
    assert not orphelines, orphelines


def test_traductions_sures_dans_le_yaml_et_le_jinja():
    """Certaines traductions finissent dans une chaîne Jinja entre apostrophes (cases « En bref »)
    ou dans du YAML entre guillemets ; d'autres sont des morceaux de phrase collés à leur voisin :
    leurs espaces de début et de fin suivent l'anglais."""
    textes = _textes_de_t()
    erreurs = []
    for francais, par_langue in _traductions().items():
        anglais = textes[francais]
        forme = (anglais.startswith(" "), anglais.endswith(" "), anglais == "")
        for code, v in par_langue.items():
            if any(c in v for c in ("'", '"', "|", "{{", "{%", "\n")):
                erreurs.append(f"{code} : caractère interdit dans {v!r}")
            if (v.startswith(" "), v.endswith(" "), v == "") != forme:
                erreurs.append(f"{code} : espaces de {v!r} ≠ anglais {anglais!r}")
    assert not erreurs, erreurs


@pytest.mark.parametrize("langue", ["Klingon", "unknown"])
def test_langue_inconnue(langue):
    """Une langue sans traduction donne l'anglais ; un select pas encore connu, le français."""
    sortie = _rendre(_maison(_tablette(), langue=langue))
    assert ('"Tab5 settings"' if langue == "Klingon" else '"Réglages Tab5"') in sortie
