"""Les entités de la tablette se cherchent par la fin de leur entity_id, suffixe « _N » permis.

Home Assistant ajoute « _2 » à un entity_id déjà pris par un autre appareil : un second
écran (ADR-0063), ou un écran qui en remplace un resté dans HA. Le 17/10/2026, la Waveshare
7B de l'auteur avait 19 entités en « _2 » (…_display_backlight_2, …_tab5_uptime_2,
…_wake_word_active_2) : `sensor.tab5_tablette` n'avait ni « ecran » ni « demarrage » ni
« micro », et l'allumage de l'écran selon la présence s'arrêtait à sa condition.
"""
import re

from tests.commun import REPO

HA = REPO / "HomeAssistant_Config"
# Une recherche par la fin d'un entity_id : 'domaine[.].*_fin…$', la fin pouvant être « (avant|anglais) »
# (custom_components/tab5/renommage.py, tests/test_renommage.py).
RECHERCHE = re.compile(r"'([a-z_]+)\[\.\]\.\*_(\([a-z0-9_|]+\)|[a-z0-9_]+)([^']*)\$'")
SUFFIXE = "(_[0-9]+)?"
# Météo-France : ses capteurs sont nommés par la ville, pas par un appareil du projet.
HORS_TABLETTE = {"tab5_meteo_sources.yaml"}


def _recherches():
    for chemin in sorted(HA.rglob("*")):
        if chemin.suffix not in (".yaml", ".jinja") or chemin.name in HORS_TABLETTE or "rendered" in chemin.parts:
            continue
        for n, ligne in enumerate(chemin.read_text(encoding="utf-8").splitlines(), 1):
            for m in RECHERCHE.finditer(ligne):
                yield f"{chemin.relative_to(REPO).as_posix()}:{n}", m


def test_chaque_recherche_accepte_le_suffixe_de_ha():
    trouvees = list(_recherches())
    # Les fichiers qui cherchent des entités de la tablette (17/10/2026) : la garde ne doit
    # pas passer parce qu'elle ne trouve plus rien.
    fichiers = {lieu.split(":")[0].rsplit("/", 1)[1] for lieu, _ in trouvees}
    assert {"tab5_reglages.yaml", "tab5_mesure_conso.yaml", "tab5_tablette.jinja", "tab5_push.yaml",
            "tab5_energie.yaml", "tab5_emplacements.yaml", "tab5_dashboard.jinja"} <= fichiers, fichiers
    sans = [f"{lieu} {m.group(0)}" for lieu, m in trouvees if m.group(3) != SUFFIXE]
    assert not sans, "« _2 » de HA refusé :\n" + "\n".join(sans)


def test_le_motif_accepte_le_suffixe_et_rien_d_autre():
    motif = "light[.].*_display_backlight" + SUFFIXE + "$"
    assert re.match(motif, "light.m5stack_tab5_display_backlight")
    assert re.match(motif, "light.m5stack_tab5_display_backlight_2")
    assert re.match(motif, "light.salon_display_backlight_12")
    assert not re.match(motif, "light.m5stack_tab5_display_backlight_bis")
    assert not re.match(motif, "light.m5stack_tab5_display_backlight_2_x")
