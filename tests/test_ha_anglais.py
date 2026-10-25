# -*- coding: utf-8 -*-
"""Ce que Home Assistant lit de la tablette est en anglais (ADR-0070, décision d'Axel du
24/10/2026) : options des menus (select) et états texte, dans toutes les langues de l'écran.

Le firmware garde les `name:` de ses entités (le nom donne la clé, l'identifiant unique dans
HA et l'emplacement de la valeur gardée : l'intégration traduit les noms côté HA). Seul le
texte des options et des états change ; une tablette restaure un select par son INDEX
(template_select.cpp d'ESPHome, `pref.load(&index)`), donc l'ordre des options ne bouge
jamais et le choix enregistré reste. Ce test relit :

- aucune option de select du firmware n'est en français (pas de lettre accentuée), sauf
  « Langue », où chaque langue garde son nom natif ;
- le firmware lit et publie ces selects par leur index, jamais par leur texte ;
- « Prochain réveil » se calcule sous la garde I18nPourHa (tr() en anglais le temps du calcul) ;
- les fichiers HA qui comparent un état de la tablette acceptent l'ancien texte ET le nouveau.
"""
from __future__ import annotations

import re

import yaml

from tests.commun import HA, ChargeurBalisesBrutes, lire, sources

# Options sans lettre accentuée : ASCII, plus le tiret cadratin de la position de repos.
PERMIS = re.compile(r"^[\x20-\x7e—]+$")


def _selects():
    for chemin in sources("*.yaml"):
        contenu = yaml.load(chemin.read_text(encoding="utf-8"), Loader=ChargeurBalisesBrutes)
        if not isinstance(contenu, dict):
            continue
        for s in contenu.get("select") or []:
            if isinstance(s, dict) and s.get("options"):
                yield chemin.name, s


def test_options_des_selects_en_anglais():
    vus = 0
    for fichier, s in _selects():
        if s.get("id") == "tab5_langue":
            continue  # les langues sous leur nom natif (« Français », « Türkçe »…)
        vus += 1
        francais = [o for o in s["options"] if not PERMIS.match(str(o))]
        assert not francais, f"{fichier} « {s.get('name')} » : option(s) pas en anglais {francais}"
        initiale = s.get("initial_option")
        assert initiale is None or initiale in s["options"], (fichier, s.get("name"), initiale)
    assert vus >= 12, "le relevé ne trouve plus les selects du firmware"


def test_les_selects_se_lisent_par_l_index():
    """Le texte d'une option est celui de HA : le firmware ne le compare jamais."""
    for chemin in sources("*.yaml", "*.cpp", "*.h"):
        texte = re.sub(r"(?m)^\s*(#|//).*$", "", chemin.read_text(encoding="utf-8"))
        assert ".current_option()" not in texte, f"{chemin.name} : lire un select par active_index()"
    alarme = lire("Tab5", "paquets", "tab5-alarm.yaml")
    assert "id(tab5_alarm_days).publish_state((size_t) alarm_days_preset_index(id(alarm_days_mask)));" in alarme
    assert "alarm_days_preset_name" not in lire("Tab5", "socle", "alarm_clock.h")


def test_prochain_reveil_en_anglais_pour_ha():
    alarme = lire("Tab5", "paquets", "tab5-alarm.yaml")
    bloc = alarme.split("id: tab5_alarm_next_sensor", 1)[1].split("- platform:", 1)[0]
    garde = bloc.index("const I18nPourHa en;")
    assert garde < bloc.index("alarm_next_label(") and garde < bloc.index('tr("Heure non synchronisée")')
    entete = lire("Tab5", "socle", "tab5_i18n.h")
    assert "class I18nPourHa" in entete and "I18nPourHa(const I18nPourHa&) = delete;" in entete


def test_etats_courts_en_anglais():
    navigation = lire("Tab5", "paquets", "tab5-navigation.yaml")
    assert 'std::string("Game · ") + game : std::string("Arcade")' in navigation
    assert 'std::string(modal) : std::string("Home")' in navigation
    assert 'return t.empty() ? std::string("none") : t;' in lire("Tab5", "ecran", "tab5_zones.cpp")
    for nom in re.findall(r'ModalRegistry::add\(id\(\w+\),\s+"([^"]+)"', navigation):
        assert PERMIS.match(nom), f"libellé du registre (état « Écran courant ») pas en anglais : {nom}"


def test_ha_accepte_l_ancien_et_le_nouveau_texte():
    """Firmware d'avant ou d'après le 24/10/2026 : chaque comparaison côté HA prend les deux."""
    push = (HA / "packages" / "tab5_push.yaml").read_text(encoding="utf-8")
    assert "select('is_state', ['Alerts', 'Alertes'])" in push
    energie = (HA / "packages" / "tab5_energie.yaml").read_text(encoding="utf-8")
    assert energie.count("states(ecran) not in ['Energy', 'Énergie']") == 2
    assert "!= 'Énergie'" not in energie
    # Le test de consommation choisit une option par son index, lue sur la tablette.
    conso = (HA / "packages" / "tab5_mesure_conso.yaml").read_text(encoding="utf-8")
    assert "state_attr(e_extinction, 'options')" in conso and '"Jamais"' not in conso
