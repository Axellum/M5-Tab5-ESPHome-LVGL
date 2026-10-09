"""Graines du harnais libFuzzer des parseurs (tools/fuzz/fuzz_parse.cpp, lot F de l'audit
du 30/09/2026).

Chaque graine = le chiffre qui choisit le parseur, puis le payload que le fuzz de la
tablette virtuelle envoie au même service (tools/sanitizers/fuzz_services.py, tenu égal au
contrat par tests/test_sanitizers.py) : une seule source de payloads valides.

    python tools/fuzz/graines.py <dossier>      # écrit une graine par parseur
"""

from __future__ import annotations

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "sanitizers"))

import fuzz_services  # noqa: E402

# (premier octet, service, variable) : dans l'ordre de kParseurs (fuzz_parse.cpp) ;
# tests/test_fuzz_parse.py tient les deux listes égales.
FAMILLES = [
    ("0", "tab5_maj_previsions_heures_bulk", "payload"),
    ("1", "tab5_maj_previsions_jours_bulk", "payload"),
    ("2", "tab5_maj_alerte_meteo_france", "payload"),
    ("3", "tab5_maj_alertes_ha_bulk", "payload"),
    ("4", "tab5_maj_alertes_historique", "payload"),
    ("5", "tab5_maj_info_texte", "texte"),
    ("6", "tab5_maj_pluie_1h_bulk", "payload"),
    ("7", "tab5_maj_calendrier_mois", "heures"),
    ("8", "tab5_maj_calendrier_jour", "payload"),
]


def graines() -> dict[str, bytes]:
    """Nom de fichier → contenu de chaque graine."""
    return {f"{sel}_{service}": sel.encode() + fuzz_services.GRAINES[service][variable].encode("utf-8")
            for sel, service, variable in FAMILLES}


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    dossier = Path(sys.argv[1])
    dossier.mkdir(parents=True, exist_ok=True)
    for nom, contenu in graines().items():
        (dossier / nom).write_bytes(contenu)
    print(f"{len(FAMILLES)} graine(s) dans {dossier}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
