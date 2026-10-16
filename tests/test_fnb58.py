# -*- coding: utf-8 -*-
"""Décodage des trames du FNB58 (tools/fnb58.py), sans le matériel ni le paquet hidapi."""
import fnb58


def trame(v=5.0, a=0.39, crc_juste=True):
    corps = bytearray([0x04])
    for _ in range(4):
        e = bytearray(15)
        e[0:4] = round(v * 100000).to_bytes(4, "little")
        e[4:8] = round(a * 100000).to_bytes(4, "little")
        e[8:10] = (3163).to_bytes(2, "little")
        e[10:12] = (10).to_bytes(2, "little")
        e[13:15] = (312).to_bytes(2, "little")
        corps += e
    corps += bytes(62 - len(corps))
    crc = fnb58.crc8(bytes(corps))
    return bytes([0xAA]) + bytes(corps) + bytes([crc if crc_juste else crc ^ 0xFF])


def test_trame_de_mesures():
    ech, ok = fnb58.decoder(trame())
    assert ok
    assert len(ech) == 4
    v, a, dp, dn, t = ech[0]
    assert (v, a, dp, dn, t) == (5.0, 0.39, 3.163, 0.01, 31.2)


def test_crc_faux_signale():
    ech, ok = fnb58.decoder(trame(crc_juste=False))
    assert ech is not None and not ok


def test_autre_trame_ignoree():
    assert fnb58.decoder(bytes([0xAA, 0x03]) + bytes(62)) == (None, False)
    assert fnb58.decoder(b"") == (None, False)


def test_module_importable_sans_hidapi():
    # hid n'est importé qu'à l'ouverture : la CI n'a pas besoin du paquet hidapi.
    assert "hid" not in fnb58.__dict__
