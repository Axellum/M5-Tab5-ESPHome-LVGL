# -*- coding: utf-8 -*-
"""Lecture des payloads de HA (Tab5/socle/tab5_parse.h, lot F de l'audit du 30/09/2026) :
chaque parseur est testé ET fuzzé.

Le poste de dev n'a ni g++ ni clang : tools/test_parse.cpp tourne dans le job `python` de la
CI, le harnais libFuzzer (tools/fuzz/fuzz_parse.cpp) dans le job `fuzz-parseurs` de
sanitizers.yml. Ces tests tiennent, sans compilateur, ce qui les rend probants : aucune
fonction de tab5_parse.h oubliée par l'un ou l'autre, des graines pour chaque parseur du
harnais (tirées du contrat, tools/fuzz/graines.py), un témoin positif dans le job.
"""
import re

import graines
from tests.commun import lire

ENTETE = ("Tab5", "socle", "tab5_parse.h")
DECLARATION = re.compile(r"^(?!//|#|constexpr|struct|enum|class|using|inline|extern|\s)[\w:<>]+[\s*&]+(\w+)\(", re.M)


def _fonctions():
    noms = DECLARATION.findall(lire(*ENTETE))
    assert noms, "aucune fonction lue dans tab5_parse.h"
    return sorted(set(noms))


def test_chaque_parseur_est_teste_et_fuzze():
    test = lire("tools", "test_parse.cpp")
    fuzz = lire("tools", "fuzz", "fuzz_parse.cpp")
    for nom in _fonctions():
        assert re.search(rf"\b{nom}\(", test), f"{nom} : aucun test dans tools/test_parse.cpp"
        assert re.search(rf"\b{nom}\(", fuzz), f"{nom} : jamais appelé par tools/fuzz/fuzz_parse.cpp"


def test_graines_dans_l_ordre_du_harnais():
    fuzz = lire("tools", "fuzz", "fuzz_parse.cpp")
    bloc = fuzz.split("kParseurs[] = {", 1)[1].split("};", 1)[0]
    harnais = re.findall(r"//\s*'(\d)'\s+(tab5_\w+)", bloc)
    assert len(harnais) == bloc.count(",") >= 2, "un parseur du harnais sans son commentaire « 'k' service »"
    assert harnais == [(sel, service) for sel, service, _ in graines.FAMILLES]
    assert [sel for sel, _ in harnais] == [str(i) for i in range(len(harnais))], "sélecteurs 0, 1, 2… dans l'ordre"


def test_graines_tirees_du_contrat():
    import fuzz_services

    for (nom, contenu), (sel, service, variable) in zip(graines.graines().items(), graines.FAMILLES):
        assert nom == f"{sel}_{service}"
        assert contenu == sel.encode() + fuzz_services.GRAINES[service][variable].encode("utf-8")


def test_le_job_fuzz_a_son_temoin():
    wf = lire(".github", "workflows", "sanitizers.yml")
    job = wf.split("\n  fuzz-parseurs:\n", 1)[1]
    assert "-fsanitize=fuzzer,address,undefined" in job and "-fno-sanitize-recover=all" in job
    assert "tools/fuzz/fuzz_parse.cpp" in job and "tools/fuzz/graines.py" in job
    # La détection est prouvée à chaque run : le témoin doit produire un rapport.
    assert "-DTAB5_FUZZ_TEMOIN" in job and "tools/fuzz/temoin.txt" in job
    assert "tools/sanitizers/rapports.py --temoin" in job
    assert "'tools/fuzz/**'" in wf, "le job ne se lance pas quand le harnais change"
    assert lire("tools", "fuzz", "temoin.txt").startswith("0TEMOIN")
