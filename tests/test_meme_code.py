# -*- coding: utf-8 -*-
"""Même code qu'une image publiée (tools/publication/meme_code.py).

Une recompilation d'une version publiée ne diffère que par l'heure de compilation, les
empreintes et la signature SBv2 (dernier secteur de 4 096 octets, magie 0xE7) : son ELF
décode alors les plantages de l'image publiée. Au-delà, ce n'est pas le même code."""
import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools" / "publication"))

import meme_code as mc  # noqa: E402


def _image(code: bytes, graine: int) -> bytes:
    """Image signée factice : code rembourré à 4 096, puis un secteur de signature."""
    corps = code + b"\xff" * (-len(code) % 4096)
    signature = bytes([0xE7, 0x02, 0, 0]) + bytes((graine + i) % 256 for i in range(4092))
    return corps + signature


CODE = bytes(range(256)) * 64   # 16 Ko


def test_signature_et_heure_de_compilation_ignorees():
    b = bytearray(CODE)
    b[100:125] = b"2026-09-28 10:00:00 +0200"   # heure de compilation (ESPHome)
    b[5000:5032] = os.urandom(32)                # empreinte de l'ELF (esp_app_desc_t)
    ok, message = mc.meme_code(_image(CODE, 1), _image(bytes(b), 2))
    assert ok, message


def test_taille_differente_pas_le_meme_code():
    ok, message = mc.meme_code(_image(CODE, 1), _image(CODE + b"\x00" * 8192, 1))
    assert not ok and "tailles" in message


def test_code_deplace_pas_le_meme_code():
    decale = CODE[1:] + CODE[:1]
    ok, message = mc.meme_code(_image(CODE, 1), _image(decale, 1))
    assert not ok and "pas le même code" in message


def test_image_non_signee_refusee():
    with pytest.raises(ValueError):
        mc.meme_code(CODE, _image(CODE, 1))
