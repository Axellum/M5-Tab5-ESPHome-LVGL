# -*- coding: utf-8 -*-
"""`tools/gestes_maj.py` : ce qu'une release demande, par façon d'avoir installé (audit du
09/11/2026, lot 1). Cas construits (les règles) puis le cas réel qui a motivé l'outil :
v3.9.0 → v3.9.1, dont la note a été lue « télécharger et copier à chaque fois » alors que
l'intégration HACS fait tout seule et que seul le tableau de bord (facultatif) reste à
l'utilisateur.

Tags : comme `test_contrat_versions.py`, la CI les récupère ; un clone local sans tag saute
le cas réel, la CI échoue."""
import json
import os

import pytest

import contrat_api as ca
import gestes_maj as gm

MACRO = "HomeAssistant_Config/custom_templates/tab5_dashboard.jinja"
PACKAGE = "HomeAssistant_Config/packages/tab5_meteo_sources.yaml"
FIRMWARE = "Tab5/ecran/tab5_assist.cpp"


def _par(gestes, profil, cle):
    return next(x for x in gestes if x.profil == profil and x.cle == cle)


# ─── Classement des fichiers ─────────────────────────────────────────────────

def test_classer_range_chaque_fichier():
    c = gm.classer({
        FIRMWARE: "M", "tab5-ha-hmi.yaml": "M", "Tab5/lang/de.yaml": "M",
        MACRO: "M", PACKAGE: "M",
        "custom_components/tab5/__init__.py": "M",
        "Tab5/AGENTS.md": "M", "Tab5/rendu/ecrans.cpp": "M", "Tab5/user_entities.example.yaml": "M",
        "HomeAssistant_Config/AGENTS.md": "M", "docs/faq.md": "M", "CHANGELOG.md": "M",
    })
    assert c.firmware == ["Tab5/ecran/tab5_assist.cpp", "Tab5/lang/de.yaml", "tab5-ha-hmi.yaml"]
    assert c.ha == [MACRO, PACKAGE]
    assert c.integration == ["custom_components/tab5/__init__.py"]
    assert c.macro and not c.blueprint


def test_classer_blueprint_et_entites_nouvelles():
    c = gm.classer({"HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml": "M",
                    "HomeAssistant_Config/packages/tab5_nouveau.yaml": "A"}, unique_id_ajoutes=2)
    assert c.blueprint and not c.macro
    assert c.entites_ajoutees == 3          # 2 lignes unique_id: + 1 package nouveau


# ─── Les gestes selon l'ordre du contrat ─────────────────────────────────────

def test_compatible_dans_les_deux_sens_rien_n_est_requis():
    c = gm.classer({FIRMWARE: "M", MACRO: "M", PACKAGE: "M"})
    g = gm.besoins(c, "indifferent")
    assert _par(g, "integration", "installer").statut == gm.FACULTATIF
    assert _par(g, "integration", "recharger").statut == gm.INCLUS
    assert _par(g, "integration", "firmware").statut == gm.RECOMMANDE
    assert _par(g, "integration", "tableau").statut == gm.FACULTATIF      # la macro a changé
    assert _par(g, "zip", "copier").statut == gm.FACULTATIF
    assert _par(g, "zip", "recharger").statut == gm.FACULTATIF            # il suit la copie
    assert gm.totaux(g, "integration") == (0, 1, 5)
    assert gm.totaux(g, "zip") == (0, 1, 8)


def test_un_ordre_impose_rend_requis_ce_qui_a_change():
    c = gm.classer({FIRMWARE: "M", PACKAGE: "M"})
    g = gm.besoins(c, "ha")
    assert _par(g, "integration", "installer").statut == gm.REQUIS
    assert _par(g, "integration", "firmware").statut == gm.REQUIS
    assert _par(g, "zip", "copier").statut == gm.REQUIS
    assert _par(g, "zip", "recharger").statut == gm.REQUIS
    assert _par(g, "integration", "tableau").statut == gm.INUTILE         # ni macro ni entité
    # L'intégration enchaîne le firmware : Installer + redémarrer, rien de plus.
    assert gm.totaux(g, "integration") == (2, 2, 2)
    # À la main : copier (2) + recharger (2) + firmware (1).
    assert gm.totaux(g, "zip") == (5, 5, 5)


def test_firmware_seul_pas_de_fichiers_ha():
    c = gm.classer({FIRMWARE: "M"})
    g = gm.besoins(c, "indifferent")
    assert _par(g, "integration", "installer").statut == gm.INUTILE
    assert _par(g, "integration", "recharger").statut == gm.INUTILE
    assert _par(g, "zip", "copier").statut == gm.INUTILE
    assert gm.totaux(g, "integration") == (0, 1, 1)
    assert gm.totaux(g, "zip") == (0, 1, 1)
    assert "Aucun fichier Home Assistant ne change" in gm.rendre(c, "indifferent", "v3.9.0")


def test_code_de_l_integration_seul_ne_touche_pas_le_zip():
    c = gm.classer({"custom_components/tab5/__init__.py": "M"})
    g = gm.besoins(c, "firmware")
    assert _par(g, "integration", "installer").statut == gm.REQUIS
    assert _par(g, "zip", "copier").statut == gm.INUTILE                 # custom_components n'est pas dans le zip


def test_blueprint_importe_par_son_adresse():
    c = gm.classer({"HomeAssistant_Config/blueprints/automation/tab5/tab5_emplacements.yaml": "M"})
    g = gm.besoins(c, "indifferent")
    assert _par(g, "zip", "blueprint_url").statut == gm.FACULTATIF
    assert all(x.cle != "blueprint_url" for x in gm.besoins(gm.classer({PACKAGE: "M"}), "indifferent"))


# ─── Le texte ────────────────────────────────────────────────────────────────

def test_texte_dans_les_deux_langues():
    c = gm.classer({FIRMWARE: "M", MACRO: "M", PACKAGE: "M"})
    fr = gm.rendre(c, "indifferent", "v3.9.0", "fr")
    en = gm.rendre(c, "indifferent", "v3.9.0", "en")
    assert "depuis la 3.9.0" in fr and "compatible dans les deux sens" in fr
    assert "upgrading from 3.9.0" in en and "compatible both ways" in en
    for texte in (fr, en):
        assert "`tab5_hacs.zip`" in texte                                # jamais à télécharger à la main
        assert "tab5_dashboard.jinja" in texte and "tab5_meteo_sources.yaml" in texte
    assert set(gm.TEXTES["fr"]) == set(gm.TEXTES["en"])


def test_rechargement_a_la_main_est_requis_des_qu_on_copie():
    # Audit §3.3 : « à la main : oui, après la copie ». Facultatif tant que la copie l'est.
    c = gm.classer({FIRMWARE: "M", PACKAGE: "M"})
    libre = gm.rendre(c, "indifferent", "v3.9.0", "fr")
    ligne = next(x for x in libre.splitlines() if "reload_custom_templates" in x)
    assert ligne.endswith("— requis si vous copiez les fichiers.")
    assert "required if you copy the files" in gm.rendre(c, "indifferent", "v3.9.0", "en")
    impose = gm.rendre(c, "ha", "v3.9.0", "fr")
    assert next(x for x in impose.splitlines() if "reload_custom_templates" in x).endswith("— requis.")


def test_les_traductions_ne_comptent_pas_dans_le_nombre_de_fichiers():
    c = gm.classer({FIRMWARE: "M", "Tab5/ecran/tab5_zones.cpp": "M", "Tab5/lang/de.yaml": "M", "Tab5/lang/nl.yaml": "M"})
    assert "(2 fichier(s) de firmware et les traductions)" in gm.rendre(c, "indifferent", "v3.9.0", "fr")
    assert "(2 firmware file(s) and the translations)" in gm.rendre(c, "indifferent", "v3.9.0", "en")
    seules = gm.classer({"Tab5/lang/de.yaml": "M"})
    assert seules.firmware == ["Tab5/lang/de.yaml"]                    # c'est bien du firmware
    assert "(traductions seulement)" in gm.rendre(seules, "indifferent", "v3.9.0", "fr")
    assert "fichier(s) de firmware" not in gm.rendre(seules, "indifferent", "v3.9.0", "fr")


def test_les_deux_formes_de_revisions():
    assert gm.reviser("v3.9.0", None) == ("v3.9.0", "HEAD")
    assert gm.reviser("v3.9.0", "v3.9.1") == ("v3.9.0", "v3.9.1")
    assert gm.reviser("v3.9.0..v3.9.1", None) == ("v3.9.0", "v3.9.1")
    assert gm.reviser("v3.9.0..0123abc", None) == ("v3.9.0", "0123abc")
    for mauvais in (("v3.9.0..v3.9.1", "v3.9.2"), ("v3.9.0..", None), ("..v3.9.1", None), ("a..b..c", None)):
        with pytest.raises(SystemExit):
            gm.reviser(*mauvais)


# ─── Le cas réel qui a motivé l'outil ────────────────────────────────────────

def _tag(tag):
    if tag in ca.tags_publies():
        return tag
    if os.environ.get("GITHUB_ACTIONS"):
        pytest.fail(f"tag {tag} absent du clone de la CI : l'étape « Tags publiés » du job `python` "
                    "(esphome-tab5.yml) doit les récupérer")
    pytest.skip(f"tag {tag} absent de ce clone (`git fetch --tags`)")


def test_v3_9_0_vers_v3_9_1(capsys):
    """Le tableau du §3.3 de l'audit : firmware recommandé, fichiers HA et tableau de bord
    facultatifs, rechargement fait par l'intégration, zip HACS jamais à la main."""
    depuis, courant = _tag("v3.9.0"), _tag("v3.9.1")
    assert gm.main([depuis, courant, "--json"]) == 0
    faits = json.loads(capsys.readouterr().out)
    assert faits["ordre"] == "indifferent"
    assert faits["changements"]["ha"] == [MACRO, PACKAGE]
    assert faits["changements"]["integration"] == []                     # aucun changement dans custom_components/
    assert faits["changements"]["macro"] is True and faits["changements"]["blueprint"] is False
    assert len(faits["changements"]["firmware"]) > 10
    statuts = {(x["profil"], x["cle"]): x["statut"] for x in faits["gestes"]}
    assert statuts == {
        ("integration", "installer"): gm.FACULTATIF, ("integration", "recharger"): gm.INCLUS,
        ("integration", "firmware"): gm.RECOMMANDE, ("integration", "tableau"): gm.FACULTATIF,
        ("zip", "copier"): gm.FACULTATIF, ("zip", "recharger"): gm.FACULTATIF,
        ("zip", "firmware"): gm.RECOMMANDE, ("zip", "tableau"): gm.FACULTATIF,
    }
    assert gm.main([depuis, courant]) == 0
    texte = capsys.readouterr().out
    assert "Gestes : 0 pour le requis, 1 avec le recommandé, 5 avec tout" in texte    # intégration
    assert "Gestes : 0 pour le requis, 1 avec le recommandé, 8 avec tout" in texte    # à la main
    assert "`tab5_hacs.zip` n'est pas à télécharger" in texte
    assert "(15 fichier(s) de firmware et les traductions)" in texte
    # La forme de git donne le même texte.
    assert gm.main([f"{depuis}..{courant}"]) == 0
    assert capsys.readouterr().out == texte
