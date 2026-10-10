# -*- coding: utf-8 -*-
"""Popup Caméras (ADR-0049, ADR-0057) : le YAML a autant de puces, de pastilles et de cases
de mosaïque que le C++ en attend.

tab5_cameras.cpp ne nomme ni les puces, ni les pastilles, ni les cases : cameras_ouvrir()
lit les enfants de leur conteneur, dans l'ordre. Un `!include` de moins laisse un pointeur
nul (une puce ou une case jamais montrée), un de plus un widget jamais peint. Les
static_assert du C++ ne comparent que ses constantes entre elles ; ce test relie le YAML."""
import re

from tests.commun import lire, source


def _constante(nom: str) -> int:
    m = re.search(rf"constexpr int {nom} = (\d+);", source("tab5_cameras.h").read_text(encoding="utf-8"))
    assert m, f"{nom} introuvable dans tab5_cameras.h"
    return int(m.group(1))


POPUP = lire("Tab5", "ui_components", "cameras_popup.yaml")


def _includes(fichier: str) -> list[int]:
    return [int(n) for n in re.findall(rf"file: {re.escape(fichier)}, vars: \{{ n: (\d+) \}}", POPUP)]


def test_puces():
    assert _includes("cameras_puce.yaml") == list(range(_constante("kCamerasPuces")))


def test_cases_de_la_mosaique():
    assert _includes("cameras_vignette.yaml") == list(range(_constante("kCamerasVignettes")))


def test_pastilles():
    bloc = POPUP.split("id: cameras_pastilles", 1)[1]
    assert len(re.findall(r"^\s+- obj: \{ width: \d+, height: 4,", bloc, re.M)) == _constante("kCamerasPastilles")


def test_une_pastille_par_camera():
    entete = source("tab5_parse.h").read_text(encoding="utf-8")
    m = re.search(r"constexpr int kCamerasMax = (\d+);", entete)
    assert m, "kCamerasMax introuvable dans tab5_parse.h"
    assert int(m.group(1)) == _constante("kCamerasPastilles")
