"""Script PlatformIO (pre) du job sanitizers : drapeaux ajoutés au compilateur ET à l'éditeur de liens.

Les `build_flags` d'ESPHome ne vont qu'au compilateur (sauf `-Wl,…`) : un
`-fsanitize=address` y compilerait le code instrumenté sans lier la bibliothèque
d'ASan. Ce script lit deux variables d'environnement et les ajoute à tout le build
(code du projet, cœur d'ESPHome, LVGL) :

    TAB5_SAN_CCFLAGS    drapeaux du compilateur (ex. « -fsanitize=address,undefined -g »)
    TAB5_SAN_LINKFLAGS  drapeaux de l'édition de liens (ex. « -fsanitize=address,undefined »)

Branché par tools/sanitizers/variante.py (`esphome: platformio_options: extra_scripts`).
Repris de l'audit du 30/09/2026 (lot B), où il a trouvé les conversions float → int
hors bornes corrigées au lot A.
"""

import os
import shlex

Import("env")  # noqa: F821 — fourni par PlatformIO (SCons)

cc = shlex.split(os.environ.get("TAB5_SAN_CCFLAGS", ""))
ld = shlex.split(os.environ.get("TAB5_SAN_LINKFLAGS", ""))
env.Append(CCFLAGS=cc, LINKFLAGS=ld)  # noqa: F821
print(f"[sanitizers] CCFLAGS += {cc} ; LINKFLAGS += {ld}")
