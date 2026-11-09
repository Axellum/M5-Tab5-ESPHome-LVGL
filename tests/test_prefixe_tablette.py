"""Préfixe des actions d'une tablette (ADR-0063, `tab5_prefixe` et `tab5_destinataires`
de custom_templates/tab5_tablette.jinja), rendu avec le vrai fichier.

Le 16/10/2026, juste après un redémarrage, la tablette s'est connectée avant d'avoir
publié son préfixe : le capteur valait « unknown », qui passe le filtre des noms, et
toute la poussée de connexion est partie vers esphome.unknown_… (« Action not found »).
Un état illisible retombe sur le nom par défaut, comme un firmware sans le capteur.
"""
import re

import jinja2
import pytest
from jinja2.sandbox import ImmutableSandboxedEnvironment

from tests.commun import REPO

MACROS = REPO / "HomeAssistant_Config" / "custom_templates"
CAPTEUR = "sensor.salon_tab5_prefixe_des_actions"


def _rendre(appel, etat_capteur, avec_capteur=True, suffixe=""):
    api, capteur = "binary_sensor.salon_ha_api_status" + suffixe, CAPTEUR + suffixe
    entites = {"appareil_tab5": [api] + ([capteur] if avec_capteur else [])}
    etats = {api: "on", capteur: etat_capteur}
    env = ImmutableSandboxedEnvironment(undefined=jinja2.StrictUndefined,
                                        loader=jinja2.FileSystemLoader(str(MACROS)))
    def device_id(e):
        return next((d for d, liste in entites.items() if e in liste), None)

    env.globals.update(
        device_entities=lambda d: entites.get(d, []),
        # comme HA : un device_id ou l'entity_id d'une de ses entités
        device_attr=lambda d, nom: "tab5-ha-hmi" if nom == "model" and (d in entites or device_id(d)) else None,
        device_id=device_id,
        integration_entities=lambda domaine: [e for l in entites.values() for e in l],
    )
    env.filters["states"] = lambda e: etats.get(e, "unknown")
    env.tests["is_state"] = lambda e, v: etats.get(e) == v
    env.globals["is_state"] = lambda e, v: etats.get(e) == v  # tab5_pret l'appelle en fonction
    env.tests["match"] = lambda v, motif: re.match(motif, str(v)) is not None  # comme HA
    modele = "{%- from 'tab5_tablette.jinja' import tab5_prefixe, tab5_destinataires -%}" + appel
    return env.from_string(modele).render().strip()


@pytest.mark.parametrize("etat", ["unknown", "unavailable", "", "None", "Salon-7B"])
def test_un_prefixe_illisible_retombe_sur_le_nom_par_defaut(etat):
    assert _rendre("{{ tab5_prefixe('appareil_tab5') }}", etat) == "tab5_ha_hmi"
    assert _rendre("{{ tab5_destinataires('') }}", etat) == "tab5_ha_hmi"


def test_un_prefixe_lisible_est_pris():
    assert _rendre("{{ tab5_prefixe('appareil_tab5') }}", "salon_7b") == "salon_7b"
    assert _rendre("{{ tab5_destinataires('appareil_tab5') }}", "salon_7b") == "salon_7b"


def test_entites_suffixees_par_ha():
    """Entity_id déjà pris par un autre appareil : HA ajoute « _2 » (Waveshare 7B de
    l'auteur, 17/10/2026). La tablette reste trouvée, son préfixe aussi."""
    assert _rendre("{{ tab5_prefixe('appareil_tab5') }}", "salon_7b", suffixe="_2") == "salon_7b"
    assert _rendre("{{ tab5_destinataires('') }}", "salon_7b", suffixe="_2") == "salon_7b"


def test_un_firmware_sans_capteur_garde_le_nom_par_defaut():
    assert _rendre("{{ tab5_prefixe('appareil_tab5') }}", "", avec_capteur=False) == "tab5_ha_hmi"


# 09/11/2026 : l'écran est « prêt » quand son API est `on` ET son préfixe publié. Le préfixe
# arrive ~30 ms après l'API ; une poussée calculée entre les deux partait vers « tab5_ha_hmi ».
PRET = ("{%- from 'tab5_tablette.jinja' import tab5_pret, tab5_prefixe_publie -%}"
        "{{ tab5_pret('appareil_tab5') }}|{{ tab5_prefixe_publie('appareil_tab5') }}")


@pytest.mark.parametrize("etat", ["unknown", "unavailable"])
def test_un_ecran_sans_prefixe_publie_n_est_pas_pret(etat):
    assert _rendre(PRET, etat) == "|"


@pytest.mark.parametrize("etat", ["tab5", "salon_7b", "Salon-7B", ""])
def test_un_ecran_au_prefixe_publie_est_pret(etat):
    """Même un préfixe illisible : tab5_prefixe retombe alors sur le nom par défaut, comme avant."""
    assert _rendre(PRET, etat) == "oui|oui"


def test_un_firmware_sans_capteur_est_pret_des_que_son_api_est_on():
    assert _rendre(PRET, "", avec_capteur=False) == "oui|oui"


def test_ecran_pret_entites_suffixees():
    assert _rendre(PRET, "unavailable", suffixe="_2") == "|"
    assert _rendre(PRET, "tab5", suffixe="_2") == "oui|oui"
