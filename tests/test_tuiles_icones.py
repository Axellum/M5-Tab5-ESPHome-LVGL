"""Palette des icônes des tuiles de pièce (ADR-0023, « Icons: one palette, generated »).

Tab5/tuiles_icones.yaml est la source unique ; tools/gen_tuiles_icones.py en tire la table
C++ (Tab5/socle/tab5_tuiles_icones.h), les glyphes de trois polices MDI (Tab5/paquets/tab5-styles.yaml)
et les deux tables du blueprint. Ce qu'aucun compilateur ne voit :
- une partie générée périmée (la tablette montrerait une icône vide, HA un autre code) ;
- un glyphe absent d'une des trois polices (carré vide) ou du TTF, un point de code
  qui ne porte pas le nom annoncé (icône fausse) ;
- un défaut qui pointe vers un code inexistant, un nom `mdi:` représenté deux fois ;
- l'API que le firmware appelle (`tuile_icone(code, actif, type)`) qui change.
"""
from __future__ import annotations

import fnmatch
import re
import shutil
import subprocess
from pathlib import Path

import pytest
import yaml
from tests.commun import ChargeurEntrees as _Chargeur

REPO = Path(__file__).resolve().parent.parent

import check_tab5_code_rules  # noqa: E402
import gen_tuiles_icones as gen  # noqa: E402

PAL = gen.charger()

# Domaines que les tuiles acceptent (table des types de l'ADR-0023) : chacun a une icône
# par défaut, sinon une tuile de ce domaine sans icône choisie n'aurait rien à montrer.
DOMAINES_ADR = {
    "lum": ["light"],
    "int": ["switch", "input_boolean", "fan", "humidifier", "automation"],
    "vol": ["cover", "valve"],
    "med": ["media_player"],
    "act": ["scene", "script", "button", "input_button"],
    "cap": ["sensor", "number", "input_number"],
    "bin": ["binary_sensor", "device_tracker", "person", "lock"],
    "cli": ["climate"],
}


def test_check_propre(capsys):
    assert gen.main(["--check"]) == 0, capsys.readouterr().out


SORTIES = ("ENTETE", "STYLES", "BLUEPRINT", "DOC")   # les quatre parties générées


def _copie(tmp_path, monkeypatch, eol: str) -> list[Path]:
    """Copie des quatre sorties (fins de ligne `eol`), le générateur pointé dessus."""
    copies = []
    for nom in SORTIES:
        src = getattr(gen, nom)
        dst = tmp_path / src.name
        texte = src.read_bytes().decode("utf-8").replace("\r\n", "\n")
        dst.write_bytes(texte.replace("\n", eol).encode("utf-8"))
        monkeypatch.setattr(gen, nom, dst)
        copies.append(dst)
    monkeypatch.setattr(gen, "REPO", tmp_path)
    return copies


def _perimer(entete: Path, styles: Path, blueprint: Path, doc: Path) -> None:
    """Une ligne générée retirée de chaque sortie, sans toucher aux fins de ligne."""
    for f, motif in ((entete, b'    {"lustre",'), (styles, b'"\\U000F1793"'), (blueprint, b'"mdi:chandelier"'),
                     (doc, b"| `lustre` |")):
        brut = f.read_bytes()
        lignes = brut.split(b"\n")
        k = next(i for i, l in enumerate(lignes) if motif in l)
        f.write_bytes(b"\n".join(lignes[:k] + lignes[k + 1:]))


def test_check_detecte_et_n_ecrit_rien(tmp_path, monkeypatch, capsys):
    copies = _copie(tmp_path, monkeypatch, "\r\n")
    _perimer(*copies)
    avant = [c.read_bytes() for c in copies]
    assert gen.main(["--check"]) == 1
    sortie = capsys.readouterr().out
    for c in copies:
        assert c.name in sortie, sortie
    assert [c.read_bytes() for c in copies] == avant, "--check ne doit rien écrire"


@pytest.mark.parametrize("eol", ["\r\n", "\n"])
def test_ecriture_garde_les_fins_de_ligne(tmp_path, monkeypatch, eol):
    """Checkout Windows (CRLF) ou Linux (LF) : même contenu, fins de ligne du fichier."""
    depot = [getattr(gen, nom) for nom in SORTIES]
    copies = _copie(tmp_path, monkeypatch, eol)
    _perimer(*copies)
    assert gen.main([]) == 0
    for c, d in zip(copies, depot):
        brut = c.read_bytes().decode("utf-8")
        if eol == "\n":
            assert "\r" not in brut, f"{c.name} : CRLF écrit dans un fichier en LF"
        else:
            assert brut.count("\n") == brut.count("\r\n"), f"{c.name} : fins de ligne mélangées"
        # Le résultat, normalisé, est celui du dépôt : même sortie sous Windows et Linux.
        assert brut.replace("\r\n", "\n") == d.read_bytes().decode("utf-8").replace("\r\n", "\n")
    assert gen.main(["--check"]) == 0


def test_codes_des_defauts_existent():
    codes = set(PAL.par_code())
    assert PAL.repli in codes
    assert set(PAL.defauts_types()) == set(gen.TYPES)
    assert set(PAL.defauts_types().values()) <= codes
    assert set(PAL.icones_defaut().values()) <= codes
    assert set(PAL.icones_mdi().values()) <= codes


def test_chaque_domaine_des_tuiles_a_un_defaut():
    defauts = PAL.icones_defaut()
    tous = {d for ds in DOMAINES_ADR.values() for d in ds}
    manquants = sorted(tous - set(defauts))
    assert not manquants, f"domaine(s) sans icône par défaut : {manquants}"
    hors = sorted(k for k in defauts if k.split(".")[0] not in tous)
    assert not hors, f"défaut(s) pour un domaine qu'aucune tuile n'accepte : {hors}"


def test_codes_au_format_du_contrat():
    for code in PAL.par_code():
        assert re.fullmatch(r"[a-z0-9_]{1,15}", code), code
    # Les codes de l'amorce, que la pièce 1 construite depuis la 3.x réutilise.
    assert {"lit", "canape", "led", "ordinateur"} <= set(PAL.par_code())


def test_noms_mdi_uniques(tmp_path):
    total = sum(len(i.mdi) for i in PAL.icones)
    assert len(PAL.icones_mdi()) == total, "un nom mdi: est représenté par deux codes"
    source = gen.SOURCE.read_text(encoding="utf-8")
    double = source.replace("mdi: [chandelier]", "mdi: [chandelier, lamp]", 1)
    assert double != source
    (tmp_path / "p.yaml").write_text(double, encoding="utf-8")
    with pytest.raises(gen.ErreurPalette, match="mdi:lamp"):
        gen.charger(tmp_path / "p.yaml")


def test_glyphes_dans_les_trois_polices():
    polices = check_tab5_code_rules.font_glyphs(gen.STYLES)
    for police in gen.POLICES:
        manquants = sorted(f"U+{cp:05X} {nom}" for cp, (nom, _) in PAL.glyphes().items()
                           if chr(cp) not in polices.get(police, set()))
        assert not manquants, f"{police} : {manquants}"


def test_glyphes_et_noms_dans_le_ttf():
    """Chaque point de code existe dans le TTF sous le nom annoncé ; chaque nom `mdi:`
    est un nom de MDI 7.4.47. (Contre meta.json : `gen_tuiles_icones.py --meta`.)"""
    pytest.importorskip("fontTools")
    assert gen.problemes_ttf(PAL) == []


def test_regle_7_couvre_les_trois_polices():
    """MDI_CODE_TARGETS rattache la table aux widgets qui l'affichent : leurs polices
    doivent être exactement celles où le générateur écrit les glyphes."""
    motifs = check_tab5_code_rules.MDI_CODE_TARGETS[("tab5_tuiles_icones.h", "")]
    sources = [p for p in check_tab5_code_rules.firmware_sources() if p.name != "tab5-styles.yaml"]
    widgets = check_tab5_code_rules.widget_font_map(sources)
    polices = set()
    for motif in motifs:
        trouves = {w: f for w, f in widgets.items() if fnmatch.fnmatchcase(w, motif)}
        assert trouves, f"aucun widget {motif}"
        polices |= set(trouves.values())
    assert polices == set(gen.POLICES)


def test_api_inchangee():
    h = gen.ENTETE.read_text(encoding="utf-8")
    assert re.search(r"struct TuileIcone \{\s*const char \*code;[^\n]*\n\s*const char \*eteint;[^\n]*\n"
                     r"\s*const char \*allume;[^\n]*\n\};", h), "struct TuileIcone {code, eteint, allume}"
    assert "inline const char *tuile_icone(const char *code, bool actif, const char *type) {" in h
    assert "namespace tuiles_icones {" in h
    assert "inline constexpr TuileIcone kPalette[] = {" in h
    assert "inline const char *defaut_du_type(const char *type) {" in h
    # La table vient avant toute fonction (règle 7 : fonction vide dans MDI_CODE_TARGETS).
    assert h.index("kPalette[] = {") < h.index("(const char *")


def test_api_compilee_et_executee(tmp_path):
    """La table et `tuile_icone()` en vrai C++ (g++ en CI ; sauté sans compilateur hôte)."""
    gpp = shutil.which("g++")
    if gpp is None:
        pytest.skip("g++ absent (le job python de la CI l'a)")
    amp = PAL.par_code()["ampoule"]
    defauts = PAL.defauts_types()
    rep = PAL.par_code()[PAL.repli]
    vol = PAL.par_code()[defauts["vol"]]
    src = tmp_path / "t.cpp"
    src.write_text(
        f'#include "{gen.ENTETE.as_posix()}"\n'
        "#include <cstdio>\n#include <cstring>\n"
        "static int ko = 0;\n"
        "static void eq(const char *a, const char *b, int n) { if (std::strcmp(a, b)) { std::printf(\"KO %d\\n\", n); ko++; } }\n"
        "int main() {\n"
        f'  eq(tuile_icone("ampoule", false, "int"), "{amp.eteint.c}", 1);\n'
        f'  eq(tuile_icone("ampoule", true, "int"), "{amp.allume.c}", 2);\n'
        f'  eq(tuile_icone("inconnu", true, "vol"), "{vol.allume.c}", 3);\n'
        f'  eq(tuile_icone("", false, "vol"), "{vol.eteint.c}", 4);\n'
        f'  eq(tuile_icone(nullptr, false, "vol"), "{vol.eteint.c}", 5);\n'
        f'  eq(tuile_icone(nullptr, true, "zzz"), "{rep.allume.c}", 6);\n'
        f'  eq(tuile_icone(nullptr, false, nullptr), "{rep.eteint.c}", 7);\n'
        f'  eq(tuiles_icones::defaut_du_type("cli"), "{defauts["cli"]}", 8);\n'
        "  return ko;\n}\n",
        encoding="utf-8",
    )
    exe = tmp_path / "t.exe"
    r = subprocess.run([gpp, "-std=c++17", "-Wall", "-Werror", str(src), "-o", str(exe)],
                       capture_output=True, text=True)
    assert r.returncode == 0, r.stderr
    r = subprocess.run([str(exe)], capture_output=True, text=True)
    assert r.returncode == 0, r.stdout


def test_bloc_du_blueprint_est_celui_du_generateur():
    bp = yaml.load(gen.BLUEPRINT.read_text(encoding="utf-8"), Loader=_Chargeur)
    variables = bp["variables"]
    assert variables["icones_mdi"] == PAL.icones_mdi()
    assert variables["icones_defaut"] == PAL.icones_defaut()
    assert all(isinstance(v, str) for v in [*variables["icones_mdi"].values(), *variables["icones_defaut"].values()])
    texte = gen.BLUEPRINT.read_text(encoding="utf-8")
    for marque in gen.MARQUES_BLUEPRINT:
        assert re.search(rf"^  {re.escape(marque)}$", texte, re.M), f"marqueur « {marque} » absent de variables:"
