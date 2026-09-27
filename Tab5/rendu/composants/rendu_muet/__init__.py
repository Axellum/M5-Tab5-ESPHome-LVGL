"""Bouchons du rendu hors tablette (tab5-rendu-host.yaml, lot 7 de l'audit « ouverture »).

Le matériel audio et l'assistant vocal du Tab5 n'existent pas sur la plateforme `host`.
Ce composant fournit des plateformes muettes (haut-parleur, micro, lecteur média) et de
quoi déclarer des actions et conditions qui ne font rien, pour que l'interface compile
telle quelle. Les composants de même nom que ceux d'ESPHome, à côté de celui-ci
(`voice_assistant`, `micro_wake_word`, `online_image`, `http_request`), ne sont chargés
que par tab5-rendu-host.yaml (external_components) : jamais dans le firmware.
"""

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@Axellum"]

rendu_muet_ns = cg.esphome_ns.namespace("rendu_muet")
RienAction = rendu_muet_ns.class_("RienAction", automation.Action)
FauxCondition = rendu_muet_ns.class_("FauxCondition", automation.Condition)
AssistantMuet = rendu_muet_ns.class_("AssistantMuet", cg.Component)
ReveilMuet = rendu_muet_ns.class_("ReveilMuet", cg.Component)

CONFIG_SCHEMA = cv.Schema({})


def schema_libre(valeur):
    """Accepte toute écriture d'une action ou d'une condition : rien, un id, un bloc."""
    if isinstance(valeur, dict):
        return valeur
    return {"valeur": valeur}


async def _rien_to_code(config, action_id, template_arg, args):
    return cg.new_Pvariable(action_id, template_arg)


def actions_muettes(*noms: str) -> None:
    """Déclare des actions qui ne font rien (ex. « voice_assistant.start »)."""
    for nom in noms:
        automation.register_action(nom, RienAction, schema_libre, synchronous=True)(_rien_to_code)


def conditions_fausses(*noms: str) -> None:
    """Déclare des conditions toujours fausses (ex. « voice_assistant.is_running »)."""
    for nom in noms:
        automation.register_condition(nom, FauxCondition, schema_libre)(_rien_to_code)


async def to_code(config):
    pass
