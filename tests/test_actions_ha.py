# -*- coding: utf-8 -*-
"""Actions Home Assistant appelées par le firmware (28/09/2026).

Le firmware appelait `assist_satellite.stop` à chaque interruption de la voix : cette
action n'existe pas dans HA (le domaine n'a que announce, start_conversation,
ask_question), et chaque appel écrivait « Action assist_satellite.stop not found » dans
le journal de HA. Rien ne le signalait côté tablette : `homeassistant.service` n'attend
pas de réponse.

Ici : chaque `homeassistant.service` / `homeassistant.action` du firmware appelle soit
une action de HA vérifiée (liste ci-dessous, relevée dans la liste des services de HA
2026.9 le 28/09/2026), soit un script défini par un package du projet."""
import re
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent

# Actions de HA appelées par le firmware, présentes dans HA 2026.9 (liste des services,
# 28/09/2026). En ajouter une : vérifier d'abord qu'elle existe (Outils de
# développement → Actions), puis l'écrire ici.
ACTIONS_HA_VERIFIEES = {
    "assist_satellite.announce",
    "automation.reload",
    "automation.trigger",
    "homeassistant.restart",
    "input_boolean.turn_on",
    "media_player.media_stop",
    "select.select_option",
}

APPEL = re.compile(r"homeassistant\.(?:service|action):\s*\n\s*(?:service|action):\s*([a-z_]+\.[a-z_0-9]+)")


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_multi_constructor("!", lambda chargeur, suffixe, noeud: None)


def _actions_du_firmware():
    trouvees = {}
    fichiers = sorted((REPO / "Tab5").glob("*.yaml")) + sorted((REPO / "Tab5" / "ui_components").glob("*.yaml"))
    for chemin in fichiers:
        for action in APPEL.findall(chemin.read_text(encoding="utf-8")):
            trouvees.setdefault(action, []).append(chemin.relative_to(REPO).as_posix())
    return trouvees


def _scripts_des_packages():
    scripts = set()
    for chemin in (REPO / "HomeAssistant_Config" / "packages").glob("*.yaml"):
        paquet = yaml.load(chemin.read_text(encoding="utf-8"), Loader=_Chargeur) or {}
        scripts |= {f"script.{nom}" for nom in (paquet.get("script") or {})}
    return scripts


def test_le_firmware_appelle_des_actions_ha():
    # Garde du test lui-même : s'il ne trouvait plus rien, il passerait sans rien vérifier.
    assert len(_actions_du_firmware()) >= 8


def test_chaque_action_appelee_existe():
    scripts = _scripts_des_packages()
    for action, ou in sorted(_actions_du_firmware().items()):
        if action.startswith("script."):
            assert action in scripts, f"{action} ({', '.join(ou)}) : aucun package ne définit ce script"
        else:
            assert action in ACTIONS_HA_VERIFIEES, (
                f"{action} ({', '.join(ou)}) : action non vérifiée dans HA — "
                "`assist_satellite.stop`, par exemple, n'existe pas")
