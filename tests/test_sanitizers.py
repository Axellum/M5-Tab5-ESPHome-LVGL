# -*- coding: utf-8 -*-
"""Outils du job sanitizers (lot B de l'audit du 30/09/2026) : lecture des rapports
ASan/UBSan dans le journal de la tablette virtuelle, et variante de compilation.

Le premier passage de l'audit comptait les fichiers d'un `log_path` que UBSan
n'utilisait pas : « 0 rapport » alors qu'il y en avait. Ces tests tiennent la lecture
du journal (rapports.py) sur des extraits réels du 30/09, et le workflow sur ses
garde-fous (témoin positif, aucun log_path)."""
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent

import rapports  # noqa: E402
import variante  # noqa: E402

# Extraits des journaux de la tablette virtuelle du 30/09/2026 (fuzz de l'audit).
UBSAN = """\x1b[0;32m[I][app:100]: Running through setup()...\x1b[0m
src/tab5_cards.cpp:347:50: runtime error: 2.56256e+32 is outside the range of representable values of type 'int'
    #0 0x5581 in popup_consigne_ui src/tab5_cards.cpp:347
    #1 0x5582 in clim_blueprint_recu(float, float) src/tab5_cards.cpp:645
    #2 0x5583 in operator() Tab5/paquets/tab5-api-logic.yaml:206
\x1b[0;33m[W][api:436]: Home Assistant event 'esphome.tab5_zones' dropped\x1b[0m
.piolibdeps/tab5-rendu/lvgl/src/misc/lv_math.c:431:13: runtime error: signed integer overflow: 30 - -2147483648 cannot be represented in type 'int'
    #0 0x5591 in lv_map .piolibdeps/tab5-rendu/lvgl/src/misc/lv_math.c:431
    #1 0x5592 in value_update .piolibdeps/tab5-rendu/lvgl/src/widgets/arc/lv_arc.c:1024
"""
ASAN = """=================================================================
==4242==ERROR: AddressSanitizer: heap-buffer-overflow on address 0x602000000011 at pc 0x55d1 bp 0x7ffc sp 0x7ffd
READ of size 1 at 0x602000000011 thread T0
    #0 0x55d1 in parse_x src/tab5_services.cpp:90
    #1 0x55d2 in main src/main.cpp:10
SUMMARY: AddressSanitizer: heap-buffer-overflow src/tab5_services.cpp:90 in parse_x
"""


def test_rapports_ubsan_et_leur_pile():
    blocs = rapports.extraire(UBSAN)
    assert len(blocs) == 2
    assert blocs[0].startswith("src/tab5_cards.cpp:347:50: runtime error")
    assert "#2 0x5583 in operator() Tab5/paquets/tab5-api-logic.yaml:206" in blocs[0]
    assert "[W][api" not in blocs[0]  # la pile s'arrête à la première ligne de journal
    assert "lv_map" in blocs[1] and "value_update" in blocs[1]


def test_rapport_asan_avec_ligne_avant_la_pile():
    (bloc,) = rapports.extraire(ASAN)
    assert "ERROR: AddressSanitizer: heap-buffer-overflow" in bloc.splitlines()[0]
    assert "#0 0x55d1 in parse_x" in bloc and "#1 0x55d2 in main" in bloc


def test_journal_sans_rapport():
    assert rapports.extraire("[I][app]: ok\n[W][api]: dropped\n") == []


def test_doublons_regroupes_malgre_pid_et_adresses():
    a = "==1==ERROR: AddressSanitizer: SEGV on unknown address 0x0000 (pc 0x1234)"
    b = "==2==ERROR: AddressSanitizer: SEGV on unknown address 0x0000 (pc 0x5678)"
    assert len(rapports.distincts([a, b])) == 1


def test_journal_lu_au_fur_et_a_mesure(tmp_path):
    chemin = tmp_path / "tablette.log"
    journal = rapports.Journal(chemin)
    assert journal.nouveaux() == []  # pas encore créé
    chemin.write_text(UBSAN, encoding="utf-8")
    assert len(journal.nouveaux()) == 2
    assert journal.nouveaux() == []  # déjà lus
    with open(chemin, "a", encoding="utf-8") as f:
        f.write(ASAN)
    (bloc,) = journal.nouveaux()
    assert "AddressSanitizer" in bloc


def test_codes_de_sortie(tmp_path):
    avec, sans = tmp_path / "avec.log", tmp_path / "sans.log"
    avec.write_text(UBSAN, encoding="utf-8")
    sans.write_text("[I][app]: ok\n", encoding="utf-8")
    for fichiers, temoin, attendu in (([avec], False, 1), ([sans], False, 0), ([avec], True, 0), ([sans], True, 1)):
        sys.argv = ["rapports.py", *(["--temoin"] if temoin else []), *map(str, fichiers)]
        assert rapports.main() == attendu, (fichiers, temoin)


def test_variante_branche_le_script_sous_esphome():
    source = "substitutions:\n  a: b\nesphome:\n  name: tab5-rendu\n"
    texte = variante.variante(source, Path("/depot/tools/sanitizers/pio_drapeaux.py"))
    assert "esphome:\n  platformio_options:\n    extra_scripts:\n      - pre:/depot/tools/sanitizers/pio_drapeaux.py\n  name: tab5-rendu\n" in texte
    # Sur le vrai fichier : le bloc existe, sinon le job compilerait sans sanitizers.
    assert "extra_scripts" in variante.variante(variante.SOURCE.read_text(encoding="utf-8"))


def test_cas_cibles_au_contrat_du_firmware():
    # _appeler (tools/demo) ignore, avec un simple log, un appel dont les variables
    # diffèrent de celles du firmware : un cas périmé ne serait jamais envoyé et le
    # job resterait vert sans rien avoir essayé. Le fuzzing, lui, lit les variables
    # sur la tablette.
    sys.path[:0] = [str(REPO / "tools" / "demo"), str(REPO / "tools" / "rendu")]
    import cibles_ub
    import demo_pusher

    contrat = demo_pusher.lire_contrat()
    for nom, service, donnees, _ecran in cibles_ub.CAS:
        assert demo_pusher.ecart_de_contrat(contrat[service], donnees) is None, nom


def test_le_workflow_garde_le_temoin_et_lit_le_journal():
    wf = (REPO / ".github" / "workflows" / "sanitizers.yml").read_text(encoding="utf-8")
    assert "tools/sanitizers/rapports.py --temoin" in wf  # la détection est prouvée à chaque run
    assert "log_path=" not in wf  # UBSan l'ignorerait : tout passe par le journal
    for script in ("fuzz_services.py", "cibles_ub.py", "variante.py"):
        assert f"tools/sanitizers/{script}" in wf


def _ecarts_graines(graines, contrat):
    """Ce qui sépare les graines du fuzz des services déclarés ; [] si tout va."""
    import demo_pusher

    ecarts = [f"{s} : service déclaré sans graine (jamais fuzzé)" for s in sorted(set(contrat) - set(graines))]
    ecarts += [f"{s} : graine d'un service disparu" for s in sorted(set(graines) - set(contrat))]
    for s in sorted(set(graines) & set(contrat)):
        if ecart := demo_pusher.ecart_de_contrat(contrat[s], graines[s]):
            ecarts.append(f"{s} : {ecart}")
    return ecarts


def test_graines_du_fuzz_egales_au_contrat():
    """OUT-1 (audit du 07/10/2026) : le fuzz n'appelait ni tab5_maj_energie ni
    tab5_maj_energie_historique, sans rien dire (un service sans graine est sauté). Chaque
    service déclaré a sa graine, avec exactement ses variables ; aucune graine orpheline."""
    sys.path[:0] = [str(REPO / "tools" / "demo")]
    import demo_pusher
    import fuzz_services

    contrat = demo_pusher.lire_contrat()
    assert len(contrat) >= 23, "le contrat n'est plus lu"
    assert _ecarts_graines(fuzz_services.GRAINES, contrat) == []
    # Falsifiabilité : une graine retirée, une en trop, une variable en moins se voient.
    sans = {k: v for k, v in fuzz_services.GRAINES.items() if k != "tab5_maj_energie"}
    assert _ecarts_graines(sans, contrat) == ["tab5_maj_energie : service déclaré sans graine (jamais fuzzé)"]
    assert _ecarts_graines({**fuzz_services.GRAINES, "tab5_maj_disparu": {}}, contrat) == [
        "tab5_maj_disparu : graine d'un service disparu"]
    moins = {**fuzz_services.GRAINES, "tab5_maj_energie_historique": {"vue": "jours", "debut": ""}}
    assert _ecarts_graines(moins, contrat) == ["tab5_maj_energie_historique : argument(s) manquant(s) ['valeurs']"]


def test_graines_des_tuiles_et_emplacements_couvrent_leurs_cles():
    """Les lecteurs de la rangée sous l'horloge (hp, hd, hLI) et de la tuile − / + (rN)
    découpent leurs propres champs : ils sont dans les graines, définitions et états."""
    import re

    import fuzz_services

    cles = lambda payload: {e.split("|", 1)[0] for e in payload.split(";") if e}  # noqa: E731
    tuiles = cles(fuzz_services.GRAINES["tab5_maj_tuiles"]["payload"])
    etats = cles(fuzz_services.GRAINES["tab5_maj_emplacements"]["payload"])
    assert {"hp", "hd"} <= tuiles
    for ensemble in (tuiles, etats):
        assert any(re.fullmatch(r"h[0-2][0-3]", c) for c in ensemble), ensemble
        assert any(re.fullmatch(r"r[0-7]", c) for c in ensemble), ensemble
