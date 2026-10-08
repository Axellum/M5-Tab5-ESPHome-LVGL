# -*- coding: utf-8 -*-
"""Premier démarrage après une installation par l'USB (28/09/2026, 3.0.1).

Le flash par la page d'installation (mode téléchargement, flash effacée) finit par un
reset du chien de garde RTC : « other watchdogs ». Le firmware le reconnaît (marque NVS
absente, Tab5/ecran/tab5_journal.cpp) et le capteur « Tab5 Raison du redémarrage » publie
alors « First boot after install (…) », que la garde « reboot inattendu » de
packages/tab5_health.yaml laisse passer. Ce test tient les deux moitiés du contrat."""
import re
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
JOURNAL = REPO / "Tab5" / "ecran" / "tab5_journal.cpp"
DIAGNOSTICS = REPO / "Tab5" / "paquets" / "tab5-sensors-diagnostics.yaml"
SANTE = REPO / "HomeAssistant_Config" / "packages" / "tab5_health.yaml"


def _prefixe_firmware():
    texte = JOURNAL.read_text(encoding="utf-8")
    m = re.search(r'kRaisonInstallationHa = "([^"]+)"', texte)
    assert m, "kRaisonInstallationHa introuvable dans tab5_journal.cpp"
    return m.group(1)


def test_la_garde_reconnait_le_premier_demarrage():
    prefixe = _prefixe_firmware()
    assert f"raison is match('{prefixe}')" in SANTE.read_text(encoding="utf-8")


def test_le_capteur_de_raison_passe_par_le_filtre():
    texte = DIAGNOSTICS.read_text(encoding="utf-8")
    bloc = texte.split("platform: debug", 1)[1].split("\n  - platform:", 1)[0]
    assert "reset_reason:" in bloc
    assert "journal_raison_ha(x)" in bloc


def test_seul_le_chien_de_garde_rtc_est_excuse():
    # Une panique ou un chien de garde de tâche doivent toujours alerter, même au
    # premier démarrage : l'excuse ne porte que sur ESP_RST_WDT.
    texte = JOURNAL.read_text(encoding="utf-8")
    assert "s_installation = s_neuve && r == ESP_RST_WDT;" in texte
    assert ("if (reset_anormal(r, s_rapport_plantage) && !s_installation) "
            "s_j.anomalie |= kPlantage;") in texte
