# -*- coding: utf-8 -*-
"""Contrat Home Assistant ↔ tablette, phase 2 (audit du 30/09/2026, lot E) : la
compatibilité entre versions prouvée par le code, plus seulement annoncée.

- L'instantané `contrat/contrat.yaml` (actions et variables, événements et champs) est
  celui que donne le code (`python tools/contrat_api.py --write` sinon).
- Semver contre le dernier tag publié : ce qui a changé depuis ce tag (lu dans SON code par
  `git cat-file`) exige une `version:` de contrat plus haute — majeure si un fichier HA
  d'avant peut casser, mineure pour un ajout pur (règle dans tools/contrat_api.py).
- Matrice N-1 : les fichiers HA du dépôt avec le firmware du tag précédent, et le firmware
  du dépôt avec les fichiers HA de ce tag (dernier tag, dernier tag stable). Sans étiquette
  dans `[Unreleased]` du CHANGELOG, les deux sens doivent passer ; une étiquette
  « **Contrat HA ↔ firmware** : … (depuis vX) » annonce l'ordre, et la matrice doit le
  permettre (pareil pour celles des sections de version).
- Les règles elles-mêmes (semver, appels qui ne peuvent pas partir) sur des cas construits.

Tags : la CI les récupère (`git fetch --depth=1 … refs/tags/v*`, job `python` de
esphome-tab5.yml) ; un clone local sans tag saute ces tests, la CI échoue.

Ce que la matrice ne voit pas : le contenu des payloads (clés d'une chaîne « a|b|c »), et
une garde à l'exécution autre que le déclencheur ou le `protocole` du blueprint (un appel
lancé seulement quand un écran du nouveau firmware est ouvert compte comme possible : la
matrice se trompe alors du côté prudent)."""
import os
import re
from pathlib import Path

import pytest

import contrat_api as ca

REPO = Path(__file__).resolve().parent.parent
CHANGELOGS = [REPO / "CHANGELOG.md", *sorted((REPO / "docs" / "changelog").glob("CHANGELOG-*.md"))]
SECTION = re.compile(r"^## \[([^\]]+)\]", re.M)


def _tag(tag):
    """Le tag s'il est dans ce clone ; sinon saute (local) ou échoue (CI)."""
    if tag is not None and tag in ca.tags_publies():
        return tag
    if os.environ.get("GITHUB_ACTIONS"):
        pytest.fail(f"tag {tag or 'v*'} absent du clone de la CI : l'étape « Tags publiés » du job "
                    "`python` (esphome-tab5.yml) doit les récupérer")
    pytest.skip(f"tag {tag or 'v*'} absent de ce clone (`git fetch --tags`)")


# ─── Instantané ──────────────────────────────────────────────────────────────

def test_instantane_a_jour():
    texte = (REPO / ca.INSTANTANE).read_text(encoding="utf-8").replace("\r\n", "\n")
    version, lu = ca.lire_instantane(texte)
    code = ca.contrat_firmware()
    diffs = [t for _, t in ca.differences(lu, code)]
    assert texte == ca.texte_instantane(code, version), (
        f"{ca.INSTANTANE} ne correspond plus au code ({'; '.join(diffs) or 'mise en forme'}) : "
        "`python tools/contrat_api.py --write`, puis `--semver` dit s'il faut monter `version:`")


def test_instantane_lu_entier():
    # Garde : s'il ne trouvait plus rien, les comparaisons passeraient sans rien vérifier.
    c = ca.contrat_firmware()
    assert len(c.services) > 15 and c.services["tab5_maj_info_texte"] == {
        "texte": "string", "couleur": "string", "meteo_id": "string"}
    assert c.evenements["esphome.tab5_action"] == {"emplacement", "action", "valeur"}


# ─── Semver contre le dernier tag ────────────────────────────────────────────

def test_semver_contre_le_dernier_tag():
    tag = _tag(ca.tag_precedent())
    base = ca.version_a(tag)
    version = ca.lire_instantane((REPO / ca.INSTANTANE).read_text(encoding="utf-8"))[0]
    diffs = ca.differences(ca.contrat_firmware(tag), ca.contrat_firmware())
    ecart = ca.ecart_semver(base, version, diffs)
    assert ecart is None, (
        f"contrat {base} au tag {tag}, {version} ici : {ecart}.\n"
        + "\n".join(f"  [{n}] {t}" for n, t in diffs)
        + f"\nMonter `version:` dans {ca.INSTANTANE} (au moins {ca.version_minimale(base, diffs)}), "
          "et dire dans le CHANGELOG l'ordre de mise à jour (`python tools/contrat_api.py --matrice`).")


def test_le_contrat_d_un_tag_se_lit():
    """Le code d'un tag (autre disposition de Tab5/ avant le 08/10/2026) se lit comme celui
    du dépôt : sans cela, le semver comparerait à un contrat vide."""
    tag = _tag(ca.tag_precedent(stable=True))
    c = ca.contrat_firmware(tag)
    assert len(c.services) > 15 and "esphome.tab5_action" in c.evenements, tag
    assert len(ca.appelants_ha(tag).appels) > 20, tag


@pytest.mark.parametrize("avant, apres, niveau", [
    ({"a": {"x": "string"}}, {"a": {"x": "string"}, "b": {}}, ca.MINEURE),            # action ajoutée
    ({"a": {"x": "string"}, "b": {}}, {"a": {"x": "string"}}, ca.MAJEURE),            # action retirée
    ({"a": {"x": "string"}}, {"a": {"x": "string", "y": "string"}}, ca.MAJEURE),      # variable ajoutée
    ({"a": {"x": "string", "y": "string"}}, {"a": {"x": "string"}}, ca.MAJEURE),      # variable retirée
    ({"a": {"x": "string"}}, {"a": {"x": "int"}}, ca.MAJEURE),                        # type changé
])
def test_regle_semver_des_actions(avant, apres, niveau):
    diffs = ca.differences(ca.Contrat(avant, {}), ca.Contrat(apres, {}))
    assert {n for n, _ in diffs} == {niveau}


@pytest.mark.parametrize("avant, apres, niveau", [
    ({"e": {"a"}}, {"e": {"a"}, "f": set()}, ca.MINEURE),     # événement ajouté
    ({"e": {"a"}, "f": set()}, {"e": {"a"}}, ca.MAJEURE),     # événement plus émis
    ({"e": {"a"}}, {"e": {"a", "b"}}, ca.MINEURE),            # champ ajouté
    ({"e": {"a", "b"}}, {"e": {"a"}}, ca.MAJEURE),            # champ plus émis
])
def test_regle_semver_des_evenements(avant, apres, niveau):
    gel = lambda d: {k: frozenset(v) for k, v in d.items()}  # noqa: E731
    diffs = ca.differences(ca.Contrat({}, gel(avant)), ca.Contrat({}, gel(apres)))
    assert {n for n, _ in diffs} == {niveau}


def test_version_exigee():
    majeure, mineure = [(ca.MAJEURE, "x")], [(ca.MINEURE, "x")]
    assert ca.ecart_semver("1.2.3", "1.2.3", []) is None
    assert ca.ecart_semver("1.2.3", "1.2.2", []) is not None          # jamais plus bas
    assert ca.ecart_semver("1.2.3", "1.2.9", mineure) is not None     # un correctif ne suffit pas
    assert ca.ecart_semver("1.2.3", "1.3.0", mineure) is None
    assert ca.ecart_semver("1.2.3", "1.9.0", majeure + mineure) is not None
    assert ca.ecart_semver("1.2.3", "2.0.0", majeure + mineure) is None
    assert ca.version_minimale("1.2.3", majeure) == "2.0.0"
    assert ca.version_minimale("1.2.3", mineure) == "1.3.0"


def test_ordre_des_tags():
    tags = ["v3.8.0-rc.2", "v3.7.0", "v3.8.0-rc.10", "v3.8.0", "v3.7.0-rc.5"]
    assert sorted(tags, key=ca.cle_tag) == ["v3.7.0-rc.5", "v3.7.0", "v3.8.0-rc.2", "v3.8.0-rc.10", "v3.8.0"]


# ─── Appels qui peuvent partir (cas construits) ──────────────────────────────

class _ArbreMemoire:
    """Ce qu'appels_detail lit d'un Arbre, en mémoire."""

    def __init__(self, fichiers):
        self._f = fichiers
        self.chemins = sorted(fichiers)

    def precharger(self, chemins):
        pass

    def lire(self, chemin):
        return self._f[chemin]


PACKAGE = """
automation:
  - id: sur_evenement
    triggers:
      - trigger: event
        event_type: esphome.tab5_nouveau
    actions:
      - action: esphome.tab5_ha_hmi_tab5_maj_a
        data: {payload: x}
      - action: script.pousser_b
  - id: sur_etat
    triggers:
      - trigger: state
        entity_id: sensor.x
        id: etat
      - trigger: event
        event_type: esphome.tab5_nouveau
        id: nouveau
    actions:
      - if:
          - condition: trigger
            id: nouveau
        then:
          - action: esphome.tab5_ha_hmi_tab5_maj_c
            data: {payload: x}
script:
  pousser_b:
    sequence:
      - action: esphome.tab5_ha_hmi_tab5_maj_b
        data: {payload: x}
  libre:
    sequence:
      - action: esphome.tab5_ha_hmi_tab5_maj_d
        data: {payload: x}
"""
BLUEPRINT = """
variables:
  protocole: >-
    {%- set p = 2 if m and (m[0][0] | int(0)) * 1000000 >= 3002000 else 1 -%}
triggers:
  - trigger: state
    entity_id: sensor.y
actions:
  - if: "{{ protocole == 2 and x != '' }}"
    then:
      - action: "esphome.{{ tablette }}_tab5_maj_e"
        data: {payload: x}
  - if: "{{ protocole == 2 and x or y }}"
    then:
      - action: "esphome.{{ tablette }}_tab5_maj_f"
        data: {payload: x}
"""


def _possibles(emis, version):
    a = _ArbreMemoire({"HomeAssistant_Config/packages/p.yaml": PACKAGE,
                       "HomeAssistant_Config/blueprints/b.yaml": BLUEPRINT})
    detail = ca.appels_detail(a)
    scripts = {}
    for x in detail:
        if x.action.startswith("script."):
            scripts.setdefault(x.action, []).append(x)
    ha = ca.Appelants([x for x in detail if not x.action.startswith("script.")], scripts, {}, {},
                      ca.seuil_protocole(a))
    c = ca.Contrat({}, {e: frozenset() for e in emis})
    return {x.action for x in ha.appels if ca.appel_possible(x, c, version, ha)}


def test_appel_seulement_sur_un_evenement_que_le_firmware_n_emet_pas():
    # a (déclencheur), b (script lancé de là), c (branche de ce déclencheur) dorment avec un
    # firmware qui n'émet pas esphome.tab5_nouveau ; d (script que rien ne lance) peut partir.
    assert _possibles(set(), None) == {"tab5_maj_d", "tab5_maj_e", "tab5_maj_f"}
    assert _possibles({"esphome.tab5_nouveau"}, None) >= {"tab5_maj_a", "tab5_maj_b", "tab5_maj_c"}


def test_appel_garde_par_le_protocole_du_blueprint():
    # e : `protocole == 2 and …` = firmware 3.2.0 ou plus ; f : `… or …` n'est pas une garde.
    assert "tab5_maj_e" not in _possibles(set(), 3_001_000)
    assert {"tab5_maj_e", "tab5_maj_f"} <= _possibles(set(), 3_002_000)
    assert "tab5_maj_f" in _possibles(set(), 3_001_000)


# ─── Matrice N-1 et étiquettes du CHANGELOG ──────────────────────────────────

def _etiquettes():
    """[(fichier, section, ordre, depuis)] des lignes « **Contrat HA ↔ firmware** : … »."""
    trouvees = []
    for chemin in CHANGELOGS:
        texte = chemin.read_text(encoding="utf-8")
        reperes = list(SECTION.finditer(texte))
        for i, m in enumerate(reperes):
            fin = reperes[i + 1].start() if i + 1 < len(reperes) else len(texte)
            for ligne in texte[m.end():fin].splitlines():
                if not ligne.startswith("**Contrat HA ↔ firmware**"):
                    continue
                e = ca.ETIQUETTE.match(ligne)
                assert e, f"{chemin.name} [{m.group(1)}] : « {ligne} », forme attendue : {ca.ETIQUETTE.pattern}"
                ordres = {v: k for k, v in ca.ETIQUETTES["fr"].items()}
                assert e["etiquette"] in ordres, (
                    f"{chemin.name} [{m.group(1)}] : « {e['etiquette']} », attendu l'une de {sorted(ordres)}")
                trouvees.append((chemin.name, m.group(1), ordres[e["etiquette"]], e["depuis"]))
    return trouvees


def _courant(section):
    """Révision d'une section : son tag, ou la copie de travail pour [Unreleased] et pour la
    section d'une version pas encore taguée (PR de release)."""
    if section == "Unreleased":
        return None
    tag = f"v{section}"
    return tag if tag in ca.tags_publies() else None


def _explication(m: ca.Matrice, ordre: str) -> str:
    return (f"« {ca.ETIQUETTES['fr'][ordre]} » contredit la matrice ; elle conseille "
            f"« {ca.ETIQUETTES['fr'][m.ordre]} » :\n{ca.tableau(m)}")


def test_etiquettes_du_changelog_selon_la_matrice():
    for fichier, section, ordre, depuis in _etiquettes():
        m = ca.matrice(_tag(depuis), _courant(section))
        assert m.sur(ordre), f"{fichier} [{section}] : " + _explication(m, ordre)


@pytest.mark.parametrize("stable", [True, False], ids=["dernier-stable", "dernier-tag"])
def test_matrice_n_1_du_depot(stable):
    """Firmware du tag précédent × fichiers HA du dépôt, et l'inverse. Sans étiquette dans
    [Unreleased] pour ce tag, les deux sens doivent passer (ordre indifférent)."""
    tag = _tag(ca.tag_precedent(stable=stable))
    m = ca.matrice(tag, None)
    # Chaque version avec ses propres fichiers : rien ne casse (phase 1 pour le dépôt ; ici,
    # garde de la lecture d'un tag).
    assert m.fw_precedent_ha_precedent.niveau < ca.CASSE, m.fw_precedent_ha_precedent.casse
    assert m.fw_courant_ha_courant.niveau < ca.CASSE, m.fw_courant_ha_courant.casse
    annonces = [o for f, s, o, d in _etiquettes() if s == "Unreleased" and d == tag]
    ordre = annonces[0] if annonces else "indifferent"
    assert m.sur(ordre), (
        f"[Unreleased] contre {tag} : " + _explication(m, ordre)
        + "\nRendre l'appel impossible avec l'ancien firmware (déclencheur sur un événement qu'il "
          "n'émet pas, garde `protocole`), ou annoncer l'ordre dans [Unreleased] du CHANGELOG : "
          + ca.etiquette(m))
