# -*- coding: utf-8 -*-
"""Bouton d'alimentation : un redémarrage, pas un plantage (06/10/2026, discussion #278).

Un appui court sur le bouton d'alimentation redémarre la tablette avec la raison
ESP_RST_WDT, sans rapport de plantage `esp32.crash`. Le journal des démarrages
(Tab5/tab5_journal.cpp) le classait « plantage (chien de garde) » et HA envoyait une
alerte sur le téléphone ; le capteur « Tab5 Raison du redémarrage » affichait la source
du dernier redémarrage DEMANDÉ (« Reboot request from esphome.ota »), périmée.

Ce fichier lit le vrai code (tab5_journal.cpp n'est pas compilable sur PC : il dépend
d'ESPHome) et rend la vraie garde « reboot inattendu » de packages/tab5_health.yaml :
- ESP_RST_WDT seul n'est pas une anomalie ; panique, chiens de garde de tâche et
  d'interruption, baisse de tension, micro-coupure, blocage du CPU le restent ;
- un rapport de plantage (lu par `crash_handler_has_data()` : le logger l'écrit avant que
  le déclencheur `on_message` existe) fait d'ESP_RST_WDT un plantage, et ses lignes,
  rejouées dans le journal, posent l'anomalie ;
- le filtre du capteur remplace le texte d'ESPHome pour ESP_RST_WDT, par le libellé du
  bouton sans rapport, par un libellé de plantage avec ;
- la garde HA laisse passer le libellé du bouton et notifie celui du plantage."""
import re
from pathlib import Path

import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment

REPO = Path(__file__).resolve().parent.parent
JOURNAL = REPO / "Tab5" / "tab5_journal.cpp"
SANTE = REPO / "HomeAssistant_Config" / "packages" / "tab5_health.yaml"

# Raisons d'esp_reset_reason() qui doivent alerter à elles seules.
ANORMALES = {
    "ESP_RST_PANIC",
    "ESP_RST_INT_WDT",
    "ESP_RST_TASK_WDT",
    "ESP_RST_BROWNOUT",
    "ESP_RST_PWR_GLITCH",
    "ESP_RST_CPU_LOCKUP",
}


def _code():
    return JOURNAL.read_text(encoding="utf-8")


def _fonction(texte, signature):
    """Corps de la fonction qui commence par `signature` (accolades équilibrées)."""
    debut = texte.index(signature)
    ouvre = texte.index("{", debut)
    profondeur = 0
    for i in range(ouvre, len(texte)):
        if texte[i] == "{":
            profondeur += 1
        elif texte[i] == "}":
            profondeur -= 1
            if profondeur == 0:
                return texte[ouvre + 1:i]
    raise AssertionError(f"fin de {signature!r} introuvable")


def _constante(nom):
    m = re.search(rf'{nom} = "([^"]+)"', _code())
    assert m, f"{nom} introuvable dans tab5_journal.cpp"
    return m.group(1)


def _anormales_du_code():
    corps = _fonction(_code(), "bool raison_anormale(esp_reset_reason_t r)")
    return set(re.findall(r"r == (ESP_RST_\w+)", corps))


def _anomalie(raison, rapport_plantage):
    """Évalue `reset_anormal()` du vrai code : son expression C++ traduite en Python, avec
    `raison_anormale(r)` lue elle aussi dans le code."""
    corps = _fonction(_code(), "bool reset_anormal(esp_reset_reason_t r, bool rapport_plantage)")
    m = re.fullmatch(r"\s*return (.+?);\s*", corps, re.S)
    assert m, "reset_anormal() n'est plus une seule expression"
    expr = " ".join(m.group(1).split())
    expr = expr.replace("raison_anormale(r)", "(r in anormales)")
    expr = re.sub(r"\b(ESP_RST_\w+)\b", r"'\1'", expr)
    expr = expr.replace("||", " or ").replace("&&", " and ")
    return bool(eval(expr, {"__builtins__": {}},  # noqa: S307 (expression lue dans le dépôt)
                     {"r": raison, "rapport_plantage": rapport_plantage,
                      "anormales": _anormales_du_code()}))


def test_le_chien_de_garde_seul_n_est_pas_une_anomalie():
    assert _anormales_du_code() == ANORMALES
    assert not _anomalie("ESP_RST_WDT", rapport_plantage=False)


def test_les_vraies_anomalies_alertent_toujours():
    for raison in sorted(ANORMALES):
        assert _anomalie(raison, rapport_plantage=False), raison
    assert _anomalie("ESP_RST_WDT", rapport_plantage=True)


def test_un_rapport_de_plantage_est_lu_et_pose_l_anomalie():
    code = _code()
    # Le logger écrit le rapport avant que le déclencheur on_message existe : le journal le
    # lit à l'ouverture, par ESPHome, et le classement en dépend.
    ouvrir = _fonction(code, "void ouvrir_session()")
    assert "s_rapport_plantage = esphome::esp32::crash_handler_has_data();" in ouvrir
    assert ("if (reset_anormal(r, s_rapport_plantage) && !s_installation) "
            "s_j.anomalie |= kPlantage;") in ouvrir
    # Ses lignes, rejouées hors du chemin du logger, posent aussi l'anomalie.
    message = _fonction(code, "void journal_log_message(")
    assert "if (crash && !s_rejeu_en_cours) return;" in message
    assert "if (crash) s_j.anomalie |= kPlantage;" in message
    assert "rejouer_rapport();" not in message  # ses ESP_LOGE repasseraient par le logger
    rejeu = _fonction(code, "void rejouer_rapport()")
    assert "esphome::esp32::crash_handler_log();" in rejeu
    assert "reset_anormal(s_raison, true)" in rejeu
    for fonction in ("void journal_tick()", "bool journal_has_report()"):
        assert "rejouer_rapport();" in _fonction(code, fonction), fonction
    # Effacé seulement une fois livré à HA avec le journal.
    livre = _fonction(code, "void journal_mark_delivered()")
    assert "if (s_rapport_dans_journal) {" in livre
    assert "crash_handler_clear();" in livre


def test_le_texte_du_journal_distingue_bouton_et_plantage():
    corps = _fonction(_code(), "void texte_demarrage(char* buf, size_t taille, const char* prefixe)")
    assert "s_raison == ESP_RST_WDT" in corps
    assert "s_rapport_plantage ? raison_texte(s_raison) : kTexteBouton" in corps
    assert "bouton d'alimentation" in _constante("kTexteBouton")
    # La raison envoyée à HA (événement esphome.tab5_journal) passe par le même texte.
    assert "texte_demarrage(" in _fonction(_code(), "std::string journal_reset_reason()")


def test_le_filtre_du_capteur_remplace_le_texte_perime():
    corps = _fonction(_code(), "std::string journal_raison_ha(const std::string& raison)")
    # Premier démarrage après installation d'abord, puis chien de garde : le texte
    # d'ESPHome (« Reboot request from … », périmé) n'est jamais rendu pour ESP_RST_WDT.
    assert corps.index("s_installation") < corps.index("ESP_RST_WDT")
    assert "if (s_raison != ESP_RST_WDT) return raison;" in corps
    apres = corps.split("if (s_raison != ESP_RST_WDT) return raison;", 1)[1]
    assert "return raison" not in apres
    assert '"Crash, other watchdogs" : kRaisonBoutonHa' in apres


def _demande(raison):
    """Rend la variable `demande` de la garde « reboot inattendu » comme HA."""
    sante = yaml.safe_load(SANTE.read_text(encoding="utf-8"))
    auto = next(a for a in sante["automation"] if a["id"] == "tab5_health_unexpected_reboot")
    variables = next(e["variables"] for e in auto["actions"] if "variables" in e)
    env = ImmutableSandboxedEnvironment()
    env.tests["match"] = lambda valeur, motif: re.match(motif, str(valeur)) is not None
    rendu = env.from_string(variables["demande"]).render(
        wait={"completed": True}, raison=raison)
    return rendu.strip() == "True"


def test_la_garde_ha_laisse_passer_le_bouton():
    libelle = f"{_constante('kRaisonBoutonHa')} (rst 0x10)"
    assert _demande(libelle)


def test_la_garde_ha_notifie_les_plantages():
    for raison in ("Crash, other watchdogs (rst 0x10)", "other watchdogs",
                   "exception/panic", "task watchdog", "interrupt watchdog", "brownout"):
        assert not _demande(raison), raison
    # Ce qui était déjà « demandé » le reste.
    for raison in ("Reboot request from esphome.ota", "software via esp_restart",
                   "USB peripheral", "First boot after install (other watchdogs)"):
        assert _demande(raison), raison
