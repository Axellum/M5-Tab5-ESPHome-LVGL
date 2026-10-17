"""Branche essai/mesure-invalidation (JAMAIS MERGÉE) : LVGL 9.5.0 modifié dans le build de la tablette virtuelle.

    python tools/mesure/patch_lvgl.py avant|caches|sans

Modifie SUR PLACE src/core/lv_obj_pos.c de la bibliothèque LVGL téléchargée par
PlatformIO (.esphome/build/**/lvgl/src/core/lv_obj_pos.c, premier build fait) ; une copie
d'origine est gardée à côté (.orig) et chaque mode repart d'elle :
- avant  : LVGL tel quel ;
- caches : blur_walk_cb saute un objet caché et ses enfants (un objet HIDDEN ne se dessine
           pas : il n'a pas de flou à redessiner) — la borne haute d'un remède qui sortirait
           les popups fermés du parcours ;
- sans   : plus de parcours du tout après une invalidation (obj_invalidate_area_internal) —
           ce que fait LVGL 9.6.0 tant qu'aucun objet n'a de flou ni d'ombre portée
           (lv_obj_invalidate_expand_blur, blur_obj_cnt == 0).
Chaque ancre doit se trouver exactement une fois : sinon le script s'arrête.
"""

from __future__ import annotations

import shutil
import sys
from pathlib import Path

RACINE = Path(__file__).resolve().parents[2]

ANCRE_CACHES = "    if(obj == blur_data->requester_obj) return LV_OBJ_TREE_WALK_SKIP_CHILDREN;\n"
AJOUT_CACHES = "    if(lv_obj_has_flag(obj, LV_OBJ_FLAG_HIDDEN)) return LV_OBJ_TREE_WALK_SKIP_CHILDREN; /*essai*/\n"
PARCOURS = [
    "    lv_obj_tree_walk(disp->act_scr, blur_walk_cb, &blur_walk_data);\n",
    "    if(disp->prev_scr) lv_obj_tree_walk(disp->prev_scr, blur_walk_cb, &blur_walk_data);\n",
    "    lv_obj_tree_walk(disp->sys_layer, blur_walk_cb, &blur_walk_data);\n",
    "    lv_obj_tree_walk(disp->top_layer, blur_walk_cb, &blur_walk_data);\n",
    "    lv_obj_tree_walk(disp->bottom_layer, blur_walk_cb, &blur_walk_data);\n",
]


def source_lvgl() -> Path:
    trouves = sorted(p for p in (RACINE / ".esphome" / "build").rglob("lv_obj_pos.c")
                     if p.parent.name == "core" and p.parent.parent.name == "src")
    if len(trouves) != 1:
        raise SystemExit(f"lv_obj_pos.c de LVGL trouvé {len(trouves)} fois (1 attendu) : {trouves}")
    return trouves[0]


def une_fois(texte: str, ancre: str) -> None:
    if texte.count(ancre) != 1:
        raise SystemExit(f"Ancre « {ancre.strip()} » trouvée {texte.count(ancre)} fois (1 attendue)")


def main() -> int:
    if len(sys.argv) != 2 or sys.argv[1] not in ("avant", "caches", "sans"):
        raise SystemExit(__doc__)
    mode = sys.argv[1]
    src = source_lvgl()
    orig = src.with_suffix(".c.orig")
    if not orig.exists():
        shutil.copy2(src, orig)
    texte = orig.read_text(encoding="utf-8")
    if mode == "caches":
        une_fois(texte, ANCRE_CACHES)
        texte = texte.replace(ANCRE_CACHES, ANCRE_CACHES + AJOUT_CACHES)
    elif mode == "sans":
        for ligne in PARCOURS:
            une_fois(texte, ligne)
            texte = texte.replace(ligne, "    /*essai : parcours retiré*/\n")
        texte = texte.replace("    blur_walk_data.inv_area = &area_tmp;\n",
                              "    blur_walk_data.inv_area = &area_tmp;\n    (void) blur_walk_data;\n", 1)
    src.write_text(texte, encoding="utf-8")
    print(f"{src} : mode « {mode} »")
    return 0


if __name__ == "__main__":
    sys.exit(main())
