# -*- coding: utf-8 -*-
"""[AI-CONTEXT] tools/gestes_maj.py : ce qu'une mise à jour demande, profil par profil, d'après le diff
de deux révisions (lot 1 de l'audit du 09/11/2026).

Deux familles de tests :
- sur un dépôt jetable (tmp_path), version 1 → version 2, pour chaque règle du script : ce qui
  compte comme firmware / fichier Home Assistant / macro du tableau de bord, quand un geste est
  REQUIS (jamais sans preuve), FACULTATIF ou INUTILE, et que les deux profils ne se mélangent pas ;
- sur les VRAIS tags, la preuve de l'audit : `v3.9.0..v3.9.1` redonne la table du §3.3 (deux
  fichiers HA seulement, rien dans `custom_components/`, tableau de bord et fichiers HA
  facultatifs, rien de requis pour l'intégration). La CI récupère les tags (`git fetch … refs/tags/v*`,
  job `python` de esphome-tab5.yml) ; un clone local sans tag saute ces tests, la CI échoue."""
import json
import os
import subprocess
from itertools import count
from pathlib import Path

import pytest

import gestes_maj as gm
from gestes_maj import FACULTATIF, INUTILE, REQUIS, ContratLu

REPO = Path(__file__).resolve().parent.parent

PAQUET = ("template:\n  - sensor:\n      - name: A\n        default_entity_id: sensor.tab5_a\n"
          "input_boolean:\n  tab5_garde:\n    name: Garde\n")
BASE = {
    "tab5-ha-hmi.yaml": "version: 1\n",
    "Tab5/ecran/ecran.cpp": "v1\n",
    "Tab5/lang/fr.yaml": "a: b\n",
    "Tab5/README.md": "doc v1\n",
    "Tab5/fonts/f.svg": "<svg/>\n",
    "HomeAssistant_Config/packages/tab5_a.yaml": PAQUET,
    # La macro importe tab5_tablette au rendu ; l'import de tab5_alertes est dans un bloc raw
    # (évalué par Home Assistant à l'affichage, pas au collage).
    "HomeAssistant_Config/custom_templates/tab5_dashboard.jinja":
        "{%- from 'tab5_tablette.jinja' import x -%}\nmacro v1 sensor.tab5_a input_boolean.tab5_garde\n"
        "{% raw %}{% from 'tab5_alertes.jinja' import y %}{% endraw %}\n",
    "HomeAssistant_Config/custom_templates/tab5_tablette.jinja": "t v1\n",
    "HomeAssistant_Config/custom_templates/tab5_alertes.jinja": "al v1\n",
    "HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml": "bp v1\n",
    "HomeAssistant_Config/snippets/s.yaml": "s v1\n",
    "HomeAssistant_Config/AGENTS.md": "doc v1\n",
    "custom_components/tab5/__init__.py": "c v1\n",
    "hacs.json": '{"homeassistant": "2026.8.0"}\n',
}
MACRO = "HomeAssistant_Config/custom_templates/tab5_dashboard.jinja"
PACKAGE = "HomeAssistant_Config/packages/tab5_a.yaml"
BLUEPRINT = "HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml"
LIBRE = ContratLu("indifferent", False, False)


def _git(racine, *args):
    subprocess.run(["git", *args], cwd=racine, check=True, capture_output=True, encoding="utf-8")


@pytest.fixture
def cas(tmp_path):
    """cas(modifs, contrat=LIBRE) -> Faits de v1..v2 d'un dépôt jetable ; `modifs` = {chemin: texte,
    ou None pour supprimer}. Le contrat est injecté (contrat_api lit toujours le vrai dépôt)."""
    numeros = count()

    def construire(modifs, contrat=LIBRE, base=None):
        racine = tmp_path / f"depot{next(numeros)}"
        racine.mkdir()
        _git(racine, "init", "-q")
        _git(racine, "config", "user.email", "t@example.org")
        _git(racine, "config", "user.name", "t")
        _git(racine, "config", "commit.gpgsign", "false")
        _git(racine, "config", "core.autocrlf", "false")

        def poser(fichiers, tag):
            for chemin, texte in fichiers.items():
                cible = racine / chemin
                if texte is None:
                    cible.unlink()
                else:
                    cible.parent.mkdir(parents=True, exist_ok=True)
                    cible.write_text(texte, encoding="utf-8", newline="\n")
            _git(racine, "add", "-A")
            _git(racine, "commit", "-q", "--allow-empty", "-m", tag)
            _git(racine, "tag", tag)

        poser(base if base is not None else BASE, "v1")
        poser(modifs, "v2")
        return gm.analyser("v1", "v2", racine, contrat=lambda a, b: contrat)

    return construire


def statut(f, profil, cle):
    trouves = [g for g in gm.statuts(f)[profil] if g.cle == cle]
    assert len(trouves) == 1, (profil, cle, [g.cle for g in gm.statuts(f)[profil]])
    return trouves[0].statut


# ─── Ce qui compte comme quoi ────────────────────────────────────────────────

def test_firmware_seul_ne_demande_rien_cote_home_assistant(cas):
    f = cas({"Tab5/ecran/ecran.cpp": "v2\n", "tab5-ha-hmi.yaml": "version: 2\n",
             "Tab5/lang/fr.yaml": "a: c\n", "Tab5/README.md": "doc v2\n", "Tab5/fonts/f.svg": "<svg>v2</svg>\n",
             "HomeAssistant_Config/AGENTS.md": "doc v2\n", "HomeAssistant_Config/snippets/s.yaml": "s v2\n"})
    # lang/, *.md, *.svg, snippets/ et la documentation ne comptent pas
    assert f.firmware == ["Tab5/ecran/ecran.cpp", "tab5-ha-hmi.yaml"]
    assert f.fichiers_ha == [] and f.integration == [] and f.tableau == []
    assert statut(f, "hacs", "hacs_maj") == INUTILE
    assert statut(f, "hacs", "hacs_firmware") == FACULTATIF
    assert statut(f, "zip", "zip_copier") == INUTILE
    assert statut(f, "zip", "zip_recharger") == INUTILE
    assert statut(f, "zip", "zip_firmware") == FACULTATIF
    assert statut(f, "hacs", "tableau") == statut(f, "zip", "tableau") == INUTILE


def test_aucun_changement_utile_tout_est_inutile(cas):
    f = cas({"Tab5/README.md": "doc v2\n"})
    for profil, gestes in gm.statuts(f).items():
        assert all(g.statut == INUTILE for g in gestes), (profil, [(g.cle, g.statut) for g in gestes])


def test_un_package_modifie_est_facultatif_et_le_rechargement_suit_la_copie(cas):
    f = cas({PACKAGE: PAQUET + "# v2\n"})
    assert f.packages == ["packages/tab5_a.yaml"] and f.retires == []
    assert statut(f, "hacs", "hacs_maj") == FACULTATIF
    assert statut(f, "hacs", "hacs_recharger") == INUTILE          # l'intégration le fait
    assert statut(f, "zip", "zip_copier") == FACULTATIF
    assert statut(f, "zip", "zip_recharger") == REQUIS
    rechargement = next(g for g in gm.statuts(f)["zip"] if g.cle == "zip_recharger")
    assert rechargement.condition == "copie"                         # « si vous copiez les fichiers »
    assert "copiez" in gm.etiquette_statut(rechargement, "fr")
    assert statut(f, "zip", "zip_blueprint") == INUTILE
    assert statut(f, "hacs", "tableau") == INUTILE


def test_le_blueprint_ne_regarde_que_ceux_qui_copient_a_la_main(cas):
    f = cas({BLUEPRINT: "bp v2\n"})
    assert f.blueprint == ["blueprints/automation/tab5/tab5_emplacements.yaml"]
    assert statut(f, "zip", "zip_blueprint") == FACULTATIF
    assert statut(f, "hacs", "hacs_maj") == FACULTATIF


def test_la_macro_du_tableau_et_ses_imports_au_rendu_seulement(cas):
    f = cas({MACRO: BASE[MACRO] + "{# v2 #}\n"})
    assert f.tableau == ["custom_templates/tab5_dashboard.jinja"]
    assert statut(f, "hacs", "tableau") == statut(f, "zip", "tableau") == FACULTATIF
    # modèle importé au rendu : la macro change de sortie
    f = cas({"HomeAssistant_Config/custom_templates/tab5_tablette.jinja": "t v2\n"})
    assert f.tableau == ["custom_templates/tab5_tablette.jinja"]
    assert statut(f, "zip", "tableau") == FACULTATIF
    # modèle importé dans un bloc raw : évalué à l'affichage, rien à refaire
    f = cas({"HomeAssistant_Config/custom_templates/tab5_alertes.jinja": "al v2\n"})
    assert f.tableau == [] and f.modeles == ["custom_templates/tab5_alertes.jinja"]
    assert statut(f, "zip", "tableau") == INUTILE
    assert statut(f, "zip", "zip_recharger") == REQUIS             # mais le fichier, lui, se copie


def test_le_code_de_l_integration_ne_concerne_que_le_profil_hacs(cas):
    f = cas({"custom_components/tab5/__init__.py": "c v2\n", "custom_components/tab5/neuf.py": "n\n"})
    assert f.integration == ["custom_components/tab5/__init__.py", "custom_components/tab5/neuf.py"]
    assert statut(f, "hacs", "hacs_maj") == FACULTATIF
    assert next(g for g in gm.statuts(f)["hacs"] if g.cle == "hacs_maj").raison_cle == "hacs_maj_code"
    assert statut(f, "zip", "zip_copier") == INUTILE               # le zip n'a pas d'intégration


def test_entites_ajoutees_retirees_et_citees(cas):
    f = cas({PACKAGE: PAQUET.replace("input_boolean:", "input_number:").replace("tab5_garde", "tab5_autre")})
    assert f.entites_retirees == ["input_boolean.tab5_garde"]
    assert f.entites_ajoutees == ["input_number.tab5_autre"]
    assert f.entites_citees == ["input_boolean.tab5_garde"]        # la macro de BASE la cite
    g = next(g for g in gm.statuts(f)["hacs"] if g.cle == "tableau")
    assert g.raison_cle == "tableau_citees"
    assert g.statut == FACULTATIF                                   # jamais REQUIS : une carte en erreur
    # retirée mais pas citée par l'ancienne macro : à vérifier
    f = cas({PACKAGE: PAQUET.replace("input_boolean:\n  tab5_garde:\n    name: Garde\n", "")},
            base={**BASE, MACRO: BASE[MACRO].replace("input_boolean.tab5_garde", "")})
    assert f.entites_retirees == ["input_boolean.tab5_garde"] and f.entites_citees == []
    assert next(g for g in gm.statuts(f)["zip"] if g.cle == "tableau").raison_cle == "tableau_retirees"
    # ajoutée sans que la macro change
    f = cas({PACKAGE: PAQUET + "input_select:\n  tab5_choix:\n    options: [a]\n"})
    assert f.entites_ajoutees == ["input_select.tab5_choix"] and f.tableau == []
    assert next(g for g in gm.statuts(f)["hacs"] if g.cle == "tableau").raison_cle == "tableau_entites"


def test_un_fichier_retire_est_dit_aux_deux_profils(cas):
    f = cas({"HomeAssistant_Config/packages/tab5_a.yaml": None,
             "HomeAssistant_Config/packages/tab5_b.yaml": PAQUET.replace("tab5_a", "tab5_b")})
    assert f.retires == ["packages/tab5_a.yaml"]
    assert "packages/tab5_b.yaml" in f.packages
    fr_hacs = gm.raison(f, next(g for g in gm.statuts(f)["hacs"] if g.cle == "hacs_maj"), "fr")
    fr_zip = gm.raison(f, next(g for g in gm.statuts(f)["zip"] if g.cle == "zip_copier"), "fr")
    assert "retire seule" in fr_hacs and "packages/tab5_a.yaml" in fr_hacs
    assert "À supprimer vous-même" in fr_zip and "packages/tab5_a.yaml" in fr_zip


def test_un_firmware_deplace_compte_une_fois(cas):
    f = cas({"Tab5/ecran/ecran.cpp": None, "Tab5/ecran/ecran_renomme.cpp": "v1\n"})
    assert f.firmware == ["Tab5/ecran/ecran_renomme.cpp"]


# ─── « Requis » seulement avec une preuve ────────────────────────────────────

def test_requis_vient_du_contrat_et_dit_de_quel_geste_il_depend(cas):
    ha = {PACKAGE: PAQUET + "# v2\n", "Tab5/ecran/ecran.cpp": "v2\n"}
    # le nouveau firmware casse avec les anciens fichiers : les fichiers d'abord
    f = cas(ha, ContratLu("ha", True, False, ["action x absente"]))
    for profil, cle in (("hacs", "hacs_maj"), ("zip", "zip_copier")):
        g = next(g for g in gm.statuts(f)[profil] if g.cle == cle)
        assert (g.statut, g.condition) == (REQUIS, "firmware")
    assert statut(f, "hacs", "hacs_firmware") == FACULTATIF
    # les nouveaux fichiers cassent avec l'ancien firmware : le firmware avec les fichiers
    f = cas(ha, ContratLu("firmware", False, True))
    for profil, cle in (("hacs", "hacs_firmware"), ("zip", "zip_firmware")):
        assert statut(f, profil, cle) == REQUIS
    assert statut(f, "hacs", "hacs_maj") == FACULTATIF
    # …mais sans fichier HA changé, il n'y a rien à casser
    f = cas({"Tab5/ecran/ecran.cpp": "v2\n"}, ContratLu("firmware", False, True))
    assert statut(f, "hacs", "hacs_firmware") == FACULTATIF


def test_sans_contrat_lisible_rien_n_est_declare_requis(cas):
    f = cas({PACKAGE: PAQUET + "# v2\n", "Tab5/ecran/ecran.cpp": "v2\n"}, contrat=None)
    assert f.contrat is None
    requis = [(p, g.cle) for p, gs in gm.statuts(f).items() for g in gs if g.statut == REQUIS]
    assert requis == [("zip", "zip_recharger")]                     # la seule dépendance, à la copie
    assert "non lu" in gm.table_faits(f, "fr")


def test_version_minimale_de_home_assistant(cas):
    f = cas({"hacs.json": '{"homeassistant": "2026.10.0"}\n'})
    assert f.ha_minimum == ("2026.8.0", "2026.10.0") and f.ha_min_releve
    for profil in ("hacs", "zip"):
        assert statut(f, profil, "prerequis_ha") == REQUIS
    # 2026.10 contre 2026.9 : comparaison numérique, pas de texte
    f = cas({"hacs.json": '{"homeassistant": "2026.10.0"}\n'}, base={**BASE, "hacs.json": '{"homeassistant": "2026.9.0"}\n'})
    assert f.ha_min_releve
    # le fichier naît (3.7.0) ou baisse : pas un relèvement
    f = cas({"hacs.json": '{"homeassistant": "2026.8.0"}\n'}, base={k: v for k, v in BASE.items() if k != "hacs.json"})
    assert f.ha_minimum == (None, "2026.8.0") and not f.ha_min_releve
    f = cas({"hacs.json": '{"homeassistant": "2026.7.0"}\n'})
    assert not f.ha_min_releve


# ─── Les deux profils ne se mélangent pas ────────────────────────────────────

SCENARIOS = [
    {"Tab5/ecran/ecran.cpp": "v2\n"},
    {PACKAGE: PAQUET + "# v2\n", MACRO: BASE[MACRO] + "{# v2 #}\n", BLUEPRINT: "bp v2\n",
     "custom_components/tab5/__init__.py": "c v2\n", "Tab5/ecran/ecran.cpp": "v2\n"},
    {"HomeAssistant_Config/packages/tab5_a.yaml": None, "hacs.json": '{"homeassistant": "2026.10.0"}\n'},
    {},
]


@pytest.mark.parametrize("langue", gm.LANGUES)
@pytest.mark.parametrize("modifs", SCENARIOS)
def test_chaque_profil_parle_de_son_propre_chemin(cas, modifs, langue):
    f = cas(modifs, ContratLu("ha", True, True, ["x"]))
    for g in gm.statuts(f)["hacs"]:
        texte = (gm.libelle(g, langue) + " " + gm.raison(f, g, langue)).lower()
        assert "zip" not in texte, (g.cle, texte)
    for g in gm.statuts(f)["zip"]:
        texte = (gm.libelle(g, langue) + " " + gm.raison(f, g, langue)).lower()
        assert "intégration" not in texte and "integration" not in texte, (g.cle, texte)
        if g.cle != "zip_hacs":                                    # la seule ligne qui nomme HACS : pour l'écarter
            assert "hacs" not in texte, (g.cle, texte)
    # le zip de HACS n'est jamais un geste à faire
    assert statut(f, "zip", "zip_hacs") == INUTILE


@pytest.mark.parametrize("langue", gm.LANGUES)
@pytest.mark.parametrize("modifs", SCENARIOS)
def test_les_sorties_se_rendent_et_le_json_se_relit(cas, modifs, langue):
    f = cas(modifs)
    sortie = gm.rendu(f, langue)
    assert gm.TEXTES[langue]["profil_hacs"] in sortie and gm.TEXTES[langue]["profil_zip"] in sortie
    assert "{" not in sortie, "champ de format non remplacé"
    notes = gm.notes(f, langue)
    # EN : un titre `## Upgrading from …` (notes de release) ; FR : la ligne en gras du CHANGELOG
    assert notes.startswith("## Upgrading from 1" if langue == "en" else "**À faire en mettant à jour depuis la 1** :")
    assert notes.index(gm.TEXTES[langue]["notes_hacs"]) < notes.index(gm.TEXTES[langue]["notes_zip"])
    donnees = json.loads(gm.en_json(f))
    assert set(donnees["profils"]) == {"hacs", "zip"}
    assert donnees["faits"]["depuis"] == "v1"


def test_plage_de_revisions():
    assert gm.lire_plage(["v3.9.0..v3.9.1"]) == ("v3.9.0", "v3.9.1")
    assert gm.lire_plage(["v3.9.0", "v3.9.1"]) == ("v3.9.0", "v3.9.1")
    assert gm.lire_plage(["v3.9.0"]) == ("v3.9.0", "HEAD")           # avant de poser le tag d'une release
    for mauvais in (["a..b..c"], ["..v3.9.1"], ["a", "b", "c"], []):
        with pytest.raises(SystemExit):
            gm.lire_plage(mauvais)
    with pytest.raises(SystemExit):
        gm.analyser("tag-qui-n-existe-pas", "HEAD", REPO)


def test_meme_liste_que_l_archive():
    """Les fichiers HA comptés sont ceux de l'archive publiée (archive_ha.CONTENU), pas une
    seconde liste."""
    import archive_ha
    fichiers = archive_ha.fichiers()
    assert fichiers
    for source, chemin in fichiers:
        assert gm.classer(source.relative_to(archive_ha.RACINE).as_posix())[1] == chemin
    suivis = subprocess.run(["git", "ls-files", "HomeAssistant_Config"], cwd=REPO, capture_output=True,
                            encoding="utf-8", check=True).stdout.split()
    comptes = {gm.classer(c)[1] for c in suivis if gm.classer(c)}
    assert comptes == {chemin for _, chemin in fichiers}
    assert gm.classer("HomeAssistant_Config/snippets/tab5_assist_reponse_exemple.yaml") is None
    assert gm.classer("HomeAssistant_Config/packages/sous/dossier.yaml") is None
    assert gm.classer("HomeAssistant_Config/README.md") is None


# ─── La preuve de l'audit sur les vrais tags ─────────────────────────────────

def _tag(tag):
    """Le tag s'il est dans ce clone ; sinon saute (local) ou échoue (CI)."""
    try:
        subprocess.run(["git", "rev-parse", "--verify", "--quiet", f"{tag}^{{commit}}"], cwd=REPO,
                       check=True, capture_output=True)
        return tag
    except subprocess.CalledProcessError:
        if os.environ.get("GITHUB_ACTIONS"):
            pytest.fail(f"tag {tag} absent du clone de la CI : l'étape « Tags publiés » du job `python` "
                        "(esphome-tab5.yml) doit les récupérer")
        pytest.skip(f"tag {tag} absent de ce clone (`git fetch --tags`)")


def test_3_9_1_redonne_la_table_du_paragraphe_3_3_de_l_audit():
    """audit_maj_installation_tab5_2026-11-09.md §3.3 : côté HA, 2 fichiers utiles ; aucun changement
    dans custom_components/ ; firmware, fichiers HA et tableau de bord facultatifs ; le rechargement
    du YAML se fait tout seul avec l'intégration, et après la copie à la main ; tab5_hacs.zip :
    jamais à la main."""
    f = gm.analyser(_tag("v3.9.0"), _tag("v3.9.1"), REPO)
    assert sorted(f.fichiers_ha) == ["custom_templates/tab5_dashboard.jinja", "packages/tab5_meteo_sources.yaml"]
    assert f.integration == [] and f.blueprint == [] and f.retires == []
    assert f.tableau == ["custom_templates/tab5_dashboard.jinja"]
    assert f.entites_ajoutees == [] and f.entites_retirees == [] and f.entites_lues
    assert f.ha_minimum == ("2026.8.0", "2026.8.0") and not f.ha_min_releve
    assert f.contrat is not None and f.contrat.ordre == "indifferent"
    assert len(f.firmware) == 15
    statuts = {p: {g.cle: g.statut for g in gs} for p, gs in gm.statuts(f).items()}
    assert statuts["hacs"] == {"hacs_maj": FACULTATIF, "hacs_firmware": FACULTATIF,
                               "hacs_recharger": INUTILE, "tableau": FACULTATIF}
    assert statuts["zip"] == {"zip_copier": FACULTATIF, "zip_recharger": REQUIS, "zip_blueprint": INUTILE,
                              "zip_firmware": FACULTATIF, "tableau": FACULTATIF, "zip_hacs": INUTILE}
    assert REQUIS not in statuts["hacs"].values()                    # avec l'intégration, rien n'est requis


def test_3_8_1_ne_change_aucun_fichier_home_assistant():
    f = gm.analyser(_tag("v3.8.0"), _tag("v3.8.1"), REPO)
    assert f.fichiers_ha == [] and f.integration == [] and f.tableau == []
    assert statut(f, "hacs", "hacs_maj") == INUTILE
    assert statut(f, "hacs", "hacs_firmware") == FACULTATIF
    assert statut(f, "zip", "zip_copier") == INUTILE


def test_3_9_0_change_le_code_de_l_integration_et_le_tableau():
    f = gm.analyser(_tag("v3.8.1"), _tag("v3.9.0"), REPO)
    assert f.integration and f.tableau and f.fichiers_ha
    assert statut(f, "hacs", "hacs_maj") == FACULTATIF
