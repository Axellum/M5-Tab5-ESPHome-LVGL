# -*- coding: utf-8 -*-
"""Capture d'un plantage sur le port série (tools/capture_serie.py).

La sortie de panique d'ESP-IDF (RISC-V, ESP32-P4) est repérée et découpée jusqu'au
redémarrage, ses adresses relevées pour addr2line, les redémarrages comptés ; le port de
la tablette n'est jamais deviné entre deux appareils Espressif."""
import sys
from pathlib import Path
from types import SimpleNamespace

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import capture_serie as cs  # noqa: E402

# Forme d'une panique d'ESP-IDF 5.x sur RISC-V, horodatée comme par la capture.
CAPTURE = """\
21:06:12.001 [I][http_request.ota:120]: Update complete
21:06:12.120 Guru Meditation Error: Core  0 panic'ed (Load access fault). Exception was unhandled.
21:06:12.121
21:06:12.121 Core  0 register dump:
21:06:12.122 MEPC    : 0x4ff0a1b2  RA      : 0x40012345  SP      : 0x4ff3c2d0  GP      : 0x4ff17a00
21:06:12.123 MTVAL   : 0x00000010  MHARTID : 0x00000000
21:06:12.124 Stack memory:
21:06:12.125 4ff3c2d0: 0x00000000 0x4001abcd 0x4ff3c300 0x40012345
21:06:12.126 ELF file SHA256: 0123456789abcdef
21:06:12.127 Rebooting...
21:06:12.300 ESP-ROM:esp32p4-eco2-20240710
21:06:12.301 rst:0xc (SW_CPU_RESET),boot:0x30f (SPI_FAST_FLASH_BOOT)
21:06:20.500 [I][app:100]: ESPHome version 2026.9.0
""".splitlines()


def test_bloc_de_panique_jusqu_au_redemarrage():
    a = cs.analyser(CAPTURE)
    assert len(a["paniques"]) == 1
    bloc = a["paniques"][0]
    assert "Guru Meditation" in bloc[0] and "Rebooting..." in bloc[-1]


def test_adresses_a_decoder_sans_doublon():
    a = cs.analyser(CAPTURE)
    assert a["adresses"] == ["0x4ff0a1b2", "0x40012345", "0x4ff3c2d0", "0x4ff17a00", "0x4001abcd",
                             "0x4ff3c300"]


def test_redemarrages_vus():
    a = cs.analyser(CAPTURE)
    assert [cs.texte_seul(l)[:8] for l in a["redemarrages"]] == ["Rebootin", "ESP-ROM:", "rst:0xc "]


def test_pas_de_panique_dans_un_demarrage_normal():
    a = cs.analyser(CAPTURE[:1] + CAPTURE[-3:])
    assert a["paniques"] == [] and a["adresses"] == []
    assert "aucune sortie de panique" in cs.resumer(a, None, None)


def _ports(monkeypatch, *ports):
    liste = [SimpleNamespace(device=d, vid=0x303A, serial_number=s) for d, s in ports]
    pytest.importorskip("serial")
    import serial.tools.list_ports as lp
    monkeypatch.setattr(lp, "comports", lambda: liste)


def test_jamais_de_port_devine_entre_deux_appareils(monkeypatch):
    _ports(monkeypatch, ("COM3", "50:78:7D:00:00:01"), ("COM6", "30:ED:A0:00:00:02"))
    with pytest.raises(SystemExit, match="préciser --mac ou --port"):
        cs.trouver_port(None, None)
    assert cs.trouver_port("30-ed-a0-00-00-02", None) == ("COM6", "30:ED:A0:00:00:02")
    with pytest.raises(SystemExit):
        cs.trouver_port(None, "COM9")
