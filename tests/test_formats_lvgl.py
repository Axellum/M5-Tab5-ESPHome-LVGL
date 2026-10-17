# -*- coding: utf-8 -*-
"""[AI-CONTEXT] Formats de couleur de LVGL que le firmware dessine (17/10/2026).

Une image RGB565 réduite (caméras, pochettes en COVER) ou arrondie et recolorée (vignette
de caméra appuyée) passe par lv_draw_sw_transform, qui la convertit en RGB565A8. ESPHome
n'active LV_DRAW_SW_SUPPORT_RGB565A8 que pour une `image:` RGB565 transparente ; sans
lui, LVGL écrit « Color format 0x12 is not enabled » et dessine un tampon non initialisé
(journal de husyildiz, discussion #278). Trois verrous, gardés ici :

- le drapeau dans `esphome: platformio_options: build_flags` de la tablette et du rendu
  (la seule clé que lit ESPHome 2026.9.0, le plancher, pour l'écarter de lv_conf.h) ;
- le `#error` de tab5_cameras.cpp, qui arrête le build si le drapeau se perd ;
- l'étape « Formats de couleur de LVGL » des deux tâches du rendu (rendu-host.yml), qui
  échoue si le journal du rendu contient l'avertissement.
"""
from __future__ import annotations

import yaml

from tests.commun import ChargeurSansBalises, lire, source

DRAPEAU = "-DLV_DRAW_SW_SUPPORT_RGB565A8=1"


def _esphome(fichier: str) -> dict:
    """Le seul bloc `esphome:` (le reste du fichier a des `<<: !include` que PyYAML refuse)."""
    lignes = lire(fichier).splitlines()
    debut = lignes.index("esphome:")
    fin = next(n for n in range(debut + 1, len(lignes)) if lignes[n][:1] not in ("", " ", "#"))
    return yaml.load("\n".join(lignes[debut:fin]), Loader=ChargeurSansBalises)["esphome"]


def test_drapeau_dans_platformio_options_de_la_tablette_et_du_rendu():
    for fichier in ("tab5-ha-hmi.yaml", "tab5-rendu-host.yaml"):
        bloc = _esphome(fichier)
        assert DRAPEAU in bloc.get("platformio_options", {}).get("build_flags", []), (
            f"{fichier} : {DRAPEAU} doit être sous esphome: platformio_options: build_flags "
            "(ESPHome 2026.9.0 n'écarte de lv_conf.h que ces drapeaux-là)")
        # Sous esphome: build_flags, 2026.9.0 écrirait quand même la macro à 0 dans lv_conf.h.
        assert DRAPEAU not in bloc.get("build_flags", []), f"{fichier} : drapeau au mauvais endroit"


def test_le_build_s_arrete_sans_le_drapeau():
    texte = source("tab5_cameras.cpp").read_text(encoding="utf-8")
    assert "#if !LV_DRAW_SW_SUPPORT_RGB565A8\n#error" in texte


def test_le_rendu_echoue_sur_un_format_non_active():
    flux = yaml.load(lire(".github/workflows/rendu-host.yml"), Loader=ChargeurSansBalises)
    for tache in ("rendu", "rendu-7b"):
        etapes = [e for e in flux["jobs"][tache]["steps"] if str(e.get("name", "")).startswith("Formats de couleur de LVGL")]
        assert len(etapes) == 1, f"tâche {tache} : étape « Formats de couleur de LVGL » absente"
        run = etapes[0]["run"]
        assert "is not enabled" in run and "exit 1" in run
        assert etapes[0].get("if") == "always()"

