#!/usr/bin/env python3
"""tools/publication/meme_code.py — Une recompilation a-t-elle le même code qu'une image publiée ?

[AI-CONTEXT] Pour décoder la pile d'appels d'un plantage, il faut l'ELF du firmware qui a
planté. Avant que le workflow de publication garde les ELF (artefacts `elf-<révision>`),
seuls les binaires étaient publiés : on recompile alors la même version
(`publication.yml`, entrée `elf_seulement`) et on vérifie ici que le code est le même.

Deux compilations ne sont jamais identiques à l'octet : ESPHome écrit l'heure de
compilation (ESPHOME_BUILD_TIME, writer.py), ESP-IDF l'heure, la date et l'empreinte de
l'ELF (esp_app_desc_t), l'image finit par son empreinte, et la signature SBv2 (dernier
secteur de 4 096 octets, magie 0xE7) change à chaque signature (RSA-PSS). Même taille et
quelques dizaines d'octets différents hors signature : même code, adresses identiques,
l'ELF recompilé décode les plantages de l'image publiée.

    python tools/publication/meme_code.py publie.ota.bin recompile.ota.bin
"""
from __future__ import annotations

import sys
from pathlib import Path

SECTEUR_SIGNATURE = 4096
MAGIE_SIGNATURE = 0xE7
# Heure et date (ESPHome, ESP-IDF), empreintes de l'ELF et de l'image, somme : ~130 octets.
OCTETS_TOLERES = 512


def sans_signature(image: bytes) -> bytes:
    if len(image) % SECTEUR_SIGNATURE or image[-SECTEUR_SIGNATURE] != MAGIE_SIGNATURE:
        raise ValueError("pas une image signée SBv2 (dernier secteur de 4 096 octets, magie 0xE7)")
    return image[:-SECTEUR_SIGNATURE]


def plages(a: bytes, b: bytes) -> list[tuple[int, int]]:
    """Plages [début, fin) des octets qui diffèrent."""
    out: list[tuple[int, int]] = []
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            if out and out[-1][1] >= i - 8:
                out[-1] = (out[-1][0], i + 1)
            else:
                out.append((i, i + 1))
    return out


def meme_code(publie: bytes, recompile: bytes) -> tuple[bool, str]:
    a, b = sans_signature(publie), sans_signature(recompile)
    if len(a) != len(b):
        return False, f"tailles différentes : {len(a)} contre {len(b)} octets (hors signature)"
    diff = plages(a, b)
    n = sum(f - d for d, f in diff)
    detail = ", ".join(f"0x{d:06x}+{f - d}" for d, f in diff[:12])
    if n > OCTETS_TOLERES:
        return False, f"{n} octets différents en {len(diff)} plages ({detail}…) : pas le même code"
    return True, f"même code : {n} octets différents en {len(diff)} plages ({detail})"


def main() -> int:
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    ok, message = meme_code(Path(sys.argv[1]).read_bytes(), Path(sys.argv[2]).read_bytes())
    print(message)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
