# -*- coding: utf-8 -*-
"""tools/tab5_cle_api.py — La clé API de la tablette, sans secrets.yaml (lot 6b, ADR-0020).

Depuis la 3.0, aucune clé n'est compilée : Home Assistant la génère en ajoutant la
tablette, la lui donne et la garde. Les outils du PC qui parlent à la tablette
(tools/tab5_logs.py, tools/demo/demo_pusher.py) la cherchent, dans l'ordre :

1. l'option --cle ;
2. la variable d'environnement TAB5_CLE_API ;
3. le fichier où HA la garde, `.storage/core.config_entries`, dans le dossier de
   configuration de HA : option --config-ha, ou variable TAB5_CONFIG_HA (partage
   Samba `\\\\ip-de-ha\\config`, dossier monté…). L'entrée ESPHome retenue est celle
   dont l'hôte (ou le nom d'appareil) est celui passé à --host.

La clé n'est jamais affichée ni écrite ailleurs.
"""
from __future__ import annotations

import json
import os
from pathlib import Path

ENV_CLE = "TAB5_CLE_API"
ENV_CONFIG_HA = "TAB5_CONFIG_HA"


def cle_depuis_config_ha(config_ha: str | Path, hote: str) -> str | None:
    """Clé que HA garde pour l'appareil ESPHome `hote` (IP, nom ou nom.local).

    None si HA ne connaît pas cet appareil, ou le connaît sans clé. Lève OSError si
    le fichier est introuvable ou illisible (partage non monté, mauvais chemin).
    """
    fichier = Path(config_ha) / ".storage" / "core.config_entries"
    donnees = json.loads(fichier.read_text(encoding="utf-8"))
    for entree in donnees.get("data", {}).get("entries", []):
        if entree.get("domain") != "esphome":
            continue
        d = entree.get("data", {})
        nom = d.get("device_name")
        if hote in (d.get("host"), nom, f"{nom}.local"):
            return d.get("noise_psk") or None
    return None


def trouver_cle(cle: str | None = None, hote: str | None = None,
                config_ha: str | None = None) -> str | None:
    """Première clé trouvée dans l'ordre du docstring du module, ou None."""
    if cle:
        return cle
    if os.environ.get(ENV_CLE):
        return os.environ[ENV_CLE]
    config_ha = config_ha or os.environ.get(ENV_CONFIG_HA)
    if config_ha and hote:
        return cle_depuis_config_ha(config_ha, hote)
    return None


def ajouter_options(parser) -> None:
    """Les options communes aux outils qui parlent à la tablette."""
    parser.add_argument("--cle", help=f"clé API en base64 (sinon {ENV_CLE}, sinon lue dans HA)")
    parser.add_argument("--config-ha", help=f"dossier de configuration de HA, où il garde la clé "
                                            f"(ex. \\\\192.168.1.10\\config ; sinon {ENV_CONFIG_HA})")
