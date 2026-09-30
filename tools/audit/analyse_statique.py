"""Audit qualité : analyse statique du C++ du projet (avertissements GCC, clang-tidy, cppcheck, clang-format).

    python tools/audit/analyse_statique.py --build .esphome/build/tab5-rendu \
        --journal build.log --resultats resultats/

S'appuie sur la tablette virtuelle déjà compilée (tab5-rendu-host.yaml, plateforme
`host`, compilée par GCC du runner avec des avertissements en plus via
AUDIT_CCFLAGS) : mêmes sources, mêmes définitions, vrais en-têtes d'ESPHome et de LVGL.

1. Avertissements GCC du journal de compilation, gardés s'ils tombent dans un fichier
   du projet (Tab5/*.cpp, Tab5/*.h — copiés à plat dans src/ par ESPHome).
2. clang-tidy sur chaque .cpp du projet, avec la base de compilation de PlatformIO
   (`pio run -t compiledb`) réduite à nos fichiers.
3. cppcheck sur la même base.
4. clang-format : combien de lignes changeraient avec quelques styles candidats
   (mesure seulement, rien n'est réécrit dans le dépôt).

Sorties : analyse.json, analyse.md, et les journaux bruts de chaque outil.
Outil d'audit : ne fait jamais échouer le job sur un constat.
"""

from __future__ import annotations

import argparse
import collections
import concurrent.futures
import difflib
import json
import os
import re
import shlex
import subprocess
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]
TAB5 = RACINE / "Tab5"
# Fichiers générés : ni relus ni reformatés.
GENERES = {"tab5_i18n_data.h", "tab5_tuiles_icones.h"}

CHECKS = ",".join([
    "-*",
    "bugprone-*", "-bugprone-easily-swappable-parameters", "-bugprone-narrowing-conversions",
    "-bugprone-reserved-identifier", "-bugprone-macro-parentheses",
    "clang-analyzer-*", "-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling",
    "cert-*", "-cert-err58-cpp", "-cert-dcl37-c", "-cert-dcl51-cpp", "-cert-err33-c",
    "concurrency-*", "performance-*", "-performance-avoid-endl", "portability-*",
    "misc-*", "-misc-non-private-member-variables-in-classes", "-misc-include-cleaner",
    "-misc-use-anonymous-namespace", "-misc-no-recursion", "-misc-const-correctness",
    "-misc-use-internal-linkage",
    "cppcoreguidelines-pro-type-member-init", "cppcoreguidelines-init-variables",
    "cppcoreguidelines-slicing", "cppcoreguidelines-virtual-class-destructor",
])

STYLES = {
    "LLVM, retrait 4, 120 colonnes": "{BasedOnStyle: LLVM, IndentWidth: 4, ColumnLimit: 120}",
    "Google, retrait 4, 120 colonnes": "{BasedOnStyle: Google, IndentWidth: 4, ColumnLimit: 120}",
    "WebKit, 120 colonnes": "{BasedOnStyle: WebKit, ColumnLimit: 120}",
    "ESPHome (Google, retrait 2, 120 colonnes)": "{BasedOnStyle: Google, IndentWidth: 2, ColumnLimit: 120}",
    "LLVM, retrait 4, 120 colonnes, sans réordonner ni tasser": (
        "{BasedOnStyle: LLVM, IndentWidth: 4, ColumnLimit: 120, SortIncludes: Never, "
        "AllowShortFunctionsOnASingleLine: All, AllowShortIfStatementsOnASingleLine: AllIfsAndElse, "
        "AllowShortLoopsOnASingleLine: true, AlignConsecutiveAssignments: Consecutive, "
        "AlignConsecutiveDeclarations: Consecutive, AlignTrailingComments: true, ReflowComments: false}"),
}

ANSI = re.compile(r"\x1b\[[0-9;]*[A-Za-z]")  # couleurs de PlatformIO dans le journal
DIAG = re.compile(r"^(?P<fichier>[^:\n]+):(?P<ligne>\d+):(?P<col>\d+): (?P<niveau>warning|error|note|style|performance|portability|information): (?P<msg>.*?)(?: \[(?P<id>[^\]]+)\])?$")


def nos_fichiers() -> tuple[set[str], set[str]]:
    cpp = {p.name for p in TAB5.glob("*.cpp")}
    h = {p.name for p in TAB5.glob("*.h")}
    return cpp, h


def a_nous(chemin: str, noms: set[str]) -> bool:
    p = chemin.replace("\\", "/")
    return Path(p).name in noms and "/esphome/" not in p and "/.platformio/" not in p and "/lvgl/" not in p


def lire_diags(texte: str, noms: set[str]) -> list[dict]:
    vus, sortie = set(), []
    for ligne in ANSI.sub("", texte).splitlines():
        m = DIAG.match(ligne.strip())
        if not m or m["niveau"] == "note" or not a_nous(m["fichier"], noms):
            continue
        cle = (Path(m["fichier"]).name, int(m["ligne"]), m["id"] or "", m["msg"])
        if cle in vus:
            continue
        vus.add(cle)
        sortie.append({"fichier": cle[0], "ligne": cle[1], "id": cle[2] or "?", "niveau": m["niveau"], "message": m["msg"]})
    return sorted(sortie, key=lambda d: (d["fichier"], d["ligne"]))


def compter(diags: list[dict]) -> dict:
    return {
        "total": len(diags),
        "par_id": collections.Counter(d["id"] for d in diags).most_common(),
        "par_fichier": collections.Counter(d["fichier"] for d in diags).most_common(),
    }


def base_compilation(build: Path, cpp: set[str], sortie: Path) -> Path | None:
    subprocess.run(["platformio", "run", "-d", str(build), "-t", "compiledb"], check=False,
                   stdout=subprocess.DEVNULL, stderr=subprocess.STDOUT)
    source = build / "compile_commands.json"
    if not source.exists():
        print("compile_commands.json absent : clang-tidy et cppcheck sautés")
        return None
    entrees = [e for e in json.loads(source.read_text()) if a_nous(e["file"], cpp)]
    for e in entrees:  # les commandes de GCC en liste, pour pouvoir retirer un drapeau refusé par clang
        if "command" in e:
            e["arguments"] = shlex.split(e.pop("command"))
        # PlatformIO écrit « src/x.cpp » relatif à `directory` : clang-tidy, lancé depuis
        # la racine du dépôt, ne le trouvait pas (1er run du 30/09 : 0 fichier analysé).
        e["file"] = str(Path(e["directory"], e["file"]).resolve())
    dossier = sortie / "cc"
    dossier.mkdir(parents=True, exist_ok=True)
    (dossier / "compile_commands.json").write_text(json.dumps(entrees, indent=1))
    print(f"Base de compilation : {len(entrees)} fichiers du projet")
    return dossier


def clang_tidy(dossier_cc: Path, cpp: set[str], h: set[str], sortie: Path) -> list[dict]:
    base = dossier_cc / "compile_commands.json"
    entrees = json.loads(base.read_text())
    extra = ["--extra-arg=-Wno-unknown-warning-option", "--extra-arg=-Wno-ignored-optimization-argument",
             "--extra-arg=-Wno-unused-command-line-argument"]

    def un(fichier: str) -> str:
        r = subprocess.run(["clang-tidy", "-p", str(dossier_cc), f"--checks={CHECKS}",
                            r"--header-filter=.*/src/[A-Za-z0-9_]+\.h$", "--quiet", *extra, fichier],
                           capture_output=True, text=True, errors="replace")
        return r.stdout + r.stderr

    brut = ""
    for essai in range(3):
        with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
            textes = list(ex.map(un, [e["file"] for e in entrees]))
        brut = "\n".join(textes)
        refuses = set(re.findall(r"error: unknown argument: '([^']+)'", brut))
        if not refuses or essai == 2:
            break
        print(f"clang refuse {sorted(refuses)} : retirés, nouvel essai")
        for e in entrees:
            e["arguments"] = [a for a in e["arguments"] if a not in refuses]
        base.write_text(json.dumps(entrees, indent=1))
    (sortie / "clang-tidy.log").write_text(brut, encoding="utf-8")
    return lire_diags(brut, cpp | h)


def cppcheck(dossier_cc: Path, cpp: set[str], h: set[str], sortie: Path) -> list[dict]:
    r = subprocess.run([
        "cppcheck", f"--project={dossier_cc / 'compile_commands.json'}",
        "--enable=warning,performance,portability,style", "--inconclusive", "--std=c++20",
        f"-j{os.cpu_count() or 4}", "--suppress=missingIncludeSystem", "--suppress=missingInclude",
        "--suppress=unmatchedSuppression", "--suppress=*:*/.platformio/*", "--suppress=*:*/esphome/*",
        "--suppress=*:*/lvgl/*", "--template={file}:{line}:{column}: {severity}: {message} [{id}]",
    ], capture_output=True, text=True, errors="replace")
    brut = r.stdout + r.stderr
    (sortie / "cppcheck.log").write_text(brut, encoding="utf-8")
    return lire_diags(brut.replace(": error: ", ": warning: "), cpp | h)


def formatage() -> dict:
    fichiers = sorted(p for p in list(TAB5.glob("*.cpp")) + list(TAB5.glob("*.h")) if p.name not in GENERES)
    total = sum(len(p.read_text(encoding="utf-8", errors="replace").splitlines()) for p in fichiers)
    res = {"fichiers": len(fichiers), "lignes": total, "styles": {}}
    for nom, style in STYLES.items():
        changees, par_fichier = 0, {}
        for p in fichiers:
            avant = p.read_text(encoding="utf-8", errors="replace").splitlines()
            r = subprocess.run(["clang-format", f"--style={style}", str(p)], capture_output=True, text=True, errors="replace")
            if r.returncode:
                continue
            apres = r.stdout.splitlines()
            n = sum(1 for op in difflib.ndiff(avant, apres) if op.startswith("- "))
            changees += n
            par_fichier[p.name] = n
        res["styles"][nom] = {"lignes_changees": changees, "pourcent": round(100 * changees / max(total, 1), 1),
                              "plus_touches": sorted(par_fichier.items(), key=lambda kv: -kv[1])[:8]}
    return res


def resume(r: dict) -> str:
    L = ["## Analyse statique du C++", ""]
    for outil in ("gcc", "clang_tidy", "cppcheck"):
        if outil not in r:
            L.append(f"- {outil} : non lancé")
            continue
        c = r[outil]["compte"]
        L.append(f"### {outil} : {c['total']} constats dans nos fichiers")
        L.append("")
        L.append("Par règle : " + ", ".join(f"`{k}` {v}" for k, v in c["par_id"][:25]))
        L.append("")
        L.append("Par fichier : " + ", ".join(f"{k} {v}" for k, v in c["par_fichier"][:15]))
        L.append("")
    if "formatage" in r:
        f = r["formatage"]
        L += [f"### clang-format ({f['fichiers']} fichiers, {f['lignes']} lignes, générés exclus)", "",
              "| Style | Lignes changées | % |", "|---|---|---|"]
        for nom, s in f["styles"].items():
            L.append(f"| {nom} | {s['lignes_changees']} | {s['pourcent']} |")
    return "\n".join(L) + "\n"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--build", required=True, type=Path)
    p.add_argument("--journal", required=True, type=Path, help="journal de `esphome compile` (avertissements GCC)")
    p.add_argument("--resultats", required=True, type=Path)
    args = p.parse_args()
    args.resultats.mkdir(parents=True, exist_ok=True)
    cpp, h = nos_fichiers()
    r: dict = {}
    if args.journal.exists():
        d = lire_diags(args.journal.read_text(encoding="utf-8", errors="replace"), cpp | h)
        r["gcc"] = {"compte": compter(d), "constats": d}
    cc = base_compilation(args.build, cpp, args.resultats)
    if cc:
        d = clang_tidy(cc, cpp, h, args.resultats)
        r["clang_tidy"] = {"compte": compter(d), "constats": d}
        d = cppcheck(cc, cpp, h, args.resultats)
        r["cppcheck"] = {"compte": compter(d), "constats": d}
    r["formatage"] = formatage()
    (args.resultats / "analyse.json").write_text(json.dumps(r, ensure_ascii=False, indent=1), encoding="utf-8")
    texte = resume(r)
    (args.resultats / "analyse.md").write_text(texte, encoding="utf-8")
    print(texte)
    return 0


if __name__ == "__main__":
    sys.exit(main())
