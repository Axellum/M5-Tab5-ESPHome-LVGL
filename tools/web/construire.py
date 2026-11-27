# -*- coding: utf-8 -*-
"""tools/web/construire.py — Démo web (essai) : compile la tablette virtuelle
(tab5-rendu-host.yaml) en WebAssembly avec emscripten, puis écrit demo.json (la maison
de la démo, exporter_demo.py) pour le Home Assistant simulé de la page.

1. Les sources C++ sont celles qu'ESPHome génère pour la plateforme host :
       python -m esphome -s rendu_nom tab5-web compile --only-generate tab5-rendu-host.yaml
   (marche aussi sous Windows : seule la compilation host n'y marche pas).
2. Ce script les compile avec emcc, avec LVGL, noise-c et libsodium (les bibliothèques
   que le platformio.ini généré demande), plus Tab5/web/web_main.cpp (boucle du
   navigateur, image, toucher), puis lie tab5.js + tab5.wasm et copie la page.

Usage :
    python tools/web/construire.py --build H:/.esphome_data/build/tab5-web \\
        --lvgl <dossier lvgl 9.5.0> --noise <dossier noise-c> --sodium <dossier libsodium> \\
        --sortie <dossier> [--emcc <emcc>] [--jobs 4] [--debug]

Les objets sont gardés dans <sortie>/obj : une source plus ancienne que son objet et
compilée avec les mêmes options n'est pas recompilée.
"""
from __future__ import annotations

import argparse
import concurrent.futures as cf
import configparser
import hashlib
import shutil
import subprocess
import sys
import time
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]

# Fichiers de libsodium que la bibliothèque esphome/libsodium compile (son library.json,
# srcFilter, relatif à libsodium/src/libsodium) : un dossier = tout ce qu'il contient.
SODIUM_FILTRE = [
    "crypto_stream/salsa20", "crypto_core/salsa", "randombytes",
    "crypto_aead/chacha20poly1305/aead_chacha20poly1305.c", "crypto_hash/crypto_hash.c",
    "crypto_hash/sha256/cp/hash_sha256_cp.c", "crypto_hash/sha256/hash_sha256.c",
    "crypto_hash/sha512/cp/hash_sha512_cp.c", "crypto_onetimeauth/crypto_onetimeauth.c",
    "crypto_onetimeauth/poly1305/donna/poly1305_donna.c",
    "crypto_onetimeauth/poly1305/onetimeauth_poly1305.c", "crypto_scalarmult/crypto_scalarmult.c",
    "crypto_scalarmult/curve25519/ref10/x25519_ref10.c", "crypto_core/ed25519/ref10/ed25519_ref10.c",
    "crypto_scalarmult/curve25519/scalarmult_curve25519.c", "crypto_sign/ed25519/ref10/open.c",
    "crypto_stream/chacha20/ref/chacha20_ref.c", "crypto_stream/chacha20/stream_chacha20.c",
    "crypto_stream/crypto_stream.c", "crypto_verify/verify.c", "sodium/codecs.c", "sodium/core.c",
    "sodium/runtime.c", "sodium/utils.c",
]
SODIUM_PORT = ["version.c", "ed25519_ref10_hinit.c", "x25519_m15.c"]

# Fonctions de web_main.cpp appelées par la page.
EXPORTS = ["_main", "_web_image", "_web_largeur", "_web_hauteur", "_web_version", "_web_toucher",
           "_web_duree_max_ms", "_web_actions", "_web_action", "_malloc", "_free"]
# Envoi d'un événement ou d'une action à HA (web_main.cpp le passe à la page).
ENVOI_HA = "_ZN7esphome3api9APIServer25send_homeassistant_actionERKNS0_26HomeassistantActionRequestE"


def drapeaux_platformio(build: Path) -> list[str]:
    """build_flags du platformio.ini généré, avec les chemins rendus absolus.
    LV_CONF_PATH (chemin entre guillemets, mal transmis sous Windows) est remplacé par
    LV_CONF_INCLUDE_SIMPLE + -I src, qui inclut le même lv_conf.h."""
    ini = configparser.ConfigParser(interpolation=None, strict=False)
    ini.read(build / "platformio.ini", encoding="utf-8")
    env = next(s for s in ini.sections() if s.startswith("env:"))
    drapeaux = []
    for ligne in ini[env]["build_flags"].split("\n"):
        d = ligne.strip()
        if not d or d.startswith("-DLV_CONF_PATH"):
            continue
        if d == "-Isrc":
            d = f"-I{build / 'src'}"
        drapeaux.append(d)
    drapeaux.append("-DLV_CONF_INCLUDE_SIMPLE")
    return drapeaux


def sources_sodium(sodium: Path) -> list[Path]:
    base = sodium / "libsodium" / "src" / "libsodium"
    fichiers: list[Path] = []
    for f in SODIUM_FILTRE:
        p = base / f
        fichiers += sorted(p.rglob("*.c")) if p.is_dir() else [p]
    fichiers += [sodium / "port" / f for f in SODIUM_PORT]
    return fichiers


def unites(args) -> list[tuple[Path, list[str]]]:
    """(source, options propres) de chaque unité de compilation."""
    src = args.build / "src"
    lvgl_inc = [f"-I{args.lvgl}", f"-I{args.lvgl / 'src'}"]
    noise_inc = [f"-I{args.noise / 'include'}", f"-I{args.noise / 'src'}"]
    sodium_base = args.sodium / "libsodium" / "src" / "libsodium"
    sodium_inc = [f"-I{sodium_base / 'include'}", f"-I{sodium_base / 'include' / 'sodium'}",
                  f"-I{args.sodium / 'port_include'}", f"-I{sodium_base}"]
    # En-têtes publics de libsodium : sodium.h, et sodium/version.h que le portage
    # d'ESPHome range dans port_include.
    sodium_pub = [f"-I{sodium_base / 'include'}", f"-I{args.sodium / 'port_include'}"]
    commun = noise_inc + sodium_pub + lvgl_inc

    liste: list[tuple[Path, list[str]]] = []
    for f in sorted(src.rglob("*")):
        if f.suffix not in (".c", ".cpp"):
            continue
        propres = list(commun)
        if f.as_posix().endswith("components/host/core.cpp"):
            # Le main() d'ESPHome reste compilé, sous un autre nom : web_main.cpp a le sien.
            propres.append("-Dmain=esphome_hote_main_inutilise")
        liste.append((f, propres))
    liste.append((RACINE / "Tab5" / "web" / "web_main.cpp", list(commun) + [f"-I{RACINE / 'Tab5' / 'rendu'}"]))
    for f in sorted((args.lvgl / "src").rglob("*")):
        if f.suffix in (".c", ".cpp"):
            liste.append((f, [f"-I{src}"] + lvgl_inc))
    for f in sorted((args.noise / "src").rglob("*.c")):
        liste.append((f, noise_inc + sodium_pub + ["-DNOISE_USE_PROTOCOL_NAME_TABLE=0",
                                      "-DNOISE_USE_FALLBACK=0", "-DNOISE_USE_HFS=0"]))
    for f in sources_sodium(args.sodium):
        liste.append((f, sodium_inc + ["-DCONFIGURED=1", "-Wno-unused-function", "-Wno-unknown-pragmas",
                                       "-Wno-unused-variable", "-Wno-type-limits"]))
    return liste


def compiler(emcc: str, source: Path, objet: Path, options: list[str]) -> tuple[Path, float, str]:
    """Compile une unité, sauf si l'objet est à jour (même empreinte d'options)."""
    empreinte = hashlib.sha1(" ".join(options).encode()).hexdigest()[:12]
    marque = objet.with_suffix(".opt")
    if (objet.exists() and marque.exists() and marque.read_text() == empreinte
            and objet.stat().st_mtime >= source.stat().st_mtime):
        return source, 0.0, ""
    objet.parent.mkdir(parents=True, exist_ok=True)
    debut = time.monotonic()
    proc = subprocess.run([emcc, *options, "-c", str(source), "-o", str(objet)],
                          capture_output=True, text=True, encoding="utf-8", errors="replace")
    if proc.returncode != 0:
        return source, -1.0, proc.stderr[-4000:]
    marque.write_text(empreinte)
    return source, time.monotonic() - debut, ""


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--build", type=Path, required=True)
    ap.add_argument("--lvgl", type=Path, required=True)
    ap.add_argument("--noise", type=Path, required=True)
    ap.add_argument("--sodium", type=Path, required=True)
    ap.add_argument("--sortie", type=Path, required=True)
    ap.add_argument("--emcc", default="emcc")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--debug", action="store_true", help="-O1 et assertions d'emscripten")
    args = ap.parse_args()

    emcc = args.emcc
    empp = str(Path(emcc).with_name(Path(emcc).name.replace("emcc", "em++"))) if "emcc" in Path(emcc).name else "em++"
    opt = ["-O1", "-g1"] if args.debug else ["-O2"]
    base = opt + drapeaux_platformio(args.build) + ["-Wno-everything"]
    obj = args.sortie / "obj"
    taches = []
    for source, propres in unites(args):
        try:
            rel = source.relative_to(args.build / "src")
            cle = Path("esphome") / rel
        except ValueError:
            cle = Path("ext") / hashlib.sha1(str(source.parent).encode()).hexdigest()[:8] / source.name
        objet = obj / cle.with_suffix(cle.suffix + ".o")
        options = base + propres
        if source.suffix == ".c":
            options = [o for o in options if not o.startswith("-std=gnu++")] + ["-std=gnu11"]
            outil = emcc
        else:
            # esphome/core/string_ref.h appelle std::strtod et std::equal sans inclure
            # <cstdlib> ni <algorithm> : la libstdc++ de gcc les apporte en passant, pas la
            # libc++ d'emscripten.
            options = options + ["-include", "cstdlib", "-include", "algorithm"]
            outil = empp
        taches.append((outil, source, objet, options))

    print(f"{len(taches)} unités, {args.jobs} à la fois", flush=True)
    debut = time.monotonic()
    erreurs = 0
    lentes = []
    with cf.ThreadPoolExecutor(args.jobs) as pool:
        futurs = [pool.submit(compiler, *t) for t in taches]
        for i, fut in enumerate(cf.as_completed(futurs), 1):
            source, duree, err = fut.result()
            if duree < 0:
                erreurs += 1
                print(f"ÉCHEC {source}\n{err}", flush=True)
            elif duree > 20:
                lentes.append((duree, source.name))
            if i % 100 == 0:
                print(f"  {i}/{len(taches)} ({time.monotonic() - debut:.0f} s)", flush=True)
    print(f"compilation : {time.monotonic() - debut:.0f} s, {erreurs} échec(s)", flush=True)
    for d, n in sorted(lentes, reverse=True)[:5]:
        print(f"  lent : {n} {d:.0f} s")
    if erreurs:
        return 1

    args.sortie.mkdir(parents=True, exist_ok=True)
    liens = opt + [
        "-sALLOW_MEMORY_GROWTH=1", "-sINITIAL_MEMORY=134217728", "-sSTACK_SIZE=4194304",
        "-sENVIRONMENT=web", f"-sEXPORTED_FUNCTIONS={','.join(EXPORTS)}",
        "-sEXPORTED_RUNTIME_METHODS=HEAPU8,ccall,cwrap,UTF8ToString",
        "-Wl,--wrap=socket", "-Wl,--wrap=nanosleep", f"-Wl,--wrap={ENVOI_HA}",
    ]
    if args.debug:
        liens += ["-sASSERTIONS=1"]
    rsp = args.sortie / "objets.rsp"
    rsp.write_text("\n".join(f'"{t[2].as_posix()}"' for t in taches), encoding="utf-8")
    debut = time.monotonic()
    proc = subprocess.run([empp, f"@{rsp}", *liens, "-o", str(args.sortie / "tab5.js")],
                          capture_output=True, text=True, encoding="utf-8", errors="replace")
    if proc.returncode != 0:
        print("ÉCHEC du lien\n" + proc.stderr[-6000:])
        return 1
    shutil.copy2(Path(__file__).with_name("index.html"), args.sortie / "index.html")
    taille = (args.sortie / "tab5.wasm").stat().st_size
    print(f"lien : {time.monotonic() - debut:.0f} s ; tab5.wasm = {taille / 1e6:.1f} Mo")
    # La maison de la démo, que le Home Assistant simulé de la page pousse (étape 1).
    return subprocess.run([sys.executable, str(Path(__file__).with_name("exporter_demo.py")),
                           "--sortie", str(args.sortie / "demo.json")]).returncode


if __name__ == "__main__":
    sys.exit(main())
