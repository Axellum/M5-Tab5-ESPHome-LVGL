"""Gardes de la traduction de l'écran (lot 4 de l'audit « ouverture », 27/09/2026).

Le texte français du code est la clé (tab5_i18n.h) ; chaque langue est un fichier
Tab5/lang/<code>.yaml, généré en C++ par tools/gen_i18n.py. Ces tests vérifient ce qu'un
compilateur ne voit pas : une clé qui ne correspond plus à aucun texte, un %d déplacé,
un caractère absent des polices (il s'afficherait en carré vide).
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))

import gen_i18n  # noqa: E402
import i18n_keys  # noqa: E402
from check_tab5_code_rules import font_glyphs  # noqa: E402

LANGS = gen_i18n.load_languages()
RE_PRINTF = re.compile(r"%[-+#0]*\d*(?:\.\d+)?(?:hh|h|ll|l|z|j|t)?([diouxXfFeEgGcsp%])")
RE_NOM = re.compile(r"\{(\w+)\}")


def test_table_generee_a_jour():
    actuel = gen_i18n.OUT.read_text(encoding="utf-8").replace("\r\n", "\n")
    assert actuel == gen_i18n.render(LANGS), "python tools/gen_i18n.py"


def test_select_langue_dans_l_ordre_des_index():
    yaml_ctrl = (REPO / "Tab5" / "tab5-ha-controls.yaml").read_text(encoding="utf-8")
    bloc = yaml_ctrl.split("id: tab5_langue", 1)[1].split("on_value:", 1)[0]
    options = re.findall(r'^\s+- "([^"]+)"\s*$', bloc.split("options:", 1)[1], re.M)
    assert options == [l["name"] for l in LANGS], (
        "options du select « Langue » = noms des langues dans l'ordre des _index")


def test_langues_completes():
    """L'anglais couvre tout ce que l'écran affiche. Une langue en cours de traduction
    peut rester partielle (le français prend le relais) : elle ne porte pas `_statut`."""
    requis = set(i18n_keys.cles_tr()) | set(i18n_keys.textes_yaml())
    for l in LANGS[1:]:
        meta = gen_i18n.yaml.safe_load((gen_i18n.LANG_DIR / l["file"]).read_text(encoding="utf-8"))
        if meta.get("_statut") != "complet":
            continue
        manque = sorted(k for k in requis if k not in l["entries"])
        assert not manque, f"{l['file']} : {len(manque)} texte(s) sans traduction : {manque[:10]}"


def test_anglais_complet_car_langue_de_repli():
    """Les trous d'une langue partielle s'affichent en anglais : l'anglais doit donc
    rester complet, sinon ces trous retomberaient sur le français."""
    repli = [l for l in LANGS if l["code"] == gen_i18n.REPLI]
    assert repli, f"langue de repli « {gen_i18n.REPLI} » introuvable dans Tab5/lang/"
    meta = gen_i18n.yaml.safe_load((gen_i18n.LANG_DIR / repli[0]["file"]).read_text(encoding="utf-8"))
    assert meta.get("_statut") == "complet", f"{repli[0]['file']} doit porter `_statut: complet`"


def test_repli_sur_l_anglais_dans_la_table_generee():
    """Une langue partielle prend le texte anglais là où elle n'a rien (commentaire
    « repli en »), puis nullptr (→ français) si l'anglais ne l'a pas non plus.
    L'anglais lui-même ne se replie sur rien."""
    fr = {"file": "fr.yaml", "name": "Français", "code": "fr", "index": 0, "entries": {}}
    en = {"file": "en.yaml", "name": "English", "code": "en", "index": 1,
          "entries": {"Oui": "Yes", "Non": "No"}}
    es = {"file": "es.yaml", "name": "Español", "code": "es", "index": 2,
          "entries": {"Oui": "Sí", "Non": None, "Peut-être": "Quizás"}}
    texte = gen_i18n.render([fr, en, es])
    table = lambda code: texte.split(f"kI18n_{code}[] = {{", 1)[1].split("};", 1)[0]
    assert '"Sí",  // "Oui"\n' in table("es")
    assert '"No",  // "Non" (repli en)\n' in table("es")
    assert '"Quizás",  // "Peut-être"\n' in table("es")
    assert 'nullptr,  // "Peut-être"\n' in table("en")


def _litteraux_du_code() -> set[str]:
    lits: set[str] = set()
    for f in i18n_keys.fichiers_source(inclure_jeux=True):
        texte = f.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(i18n_keys.RE_LIT, texte):
            lits.add(i18n_keys.c_decode(m.group(1)))
        # Littéraux C adjacents recollés ("parl\xC3\xA9""e") : même mécanique que tr().
        # Par séquences entières, puis chaque sous-suite contiguë : un appariement deux
        # à deux se décalait quand un "" précédait la paire (vu au lot 4b).
        for m in re.finditer(r'"(?:[^"\\]|\\.)*"(?:\s*"(?:[^"\\]|\\.)*")+', texte):
            parts = [i18n_keys.c_decode(p) for p in re.findall(i18n_keys.RE_LIT, m.group(0))]
            for i in range(len(parts)):
                for j in range(i + 2, len(parts) + 1):
                    lits.add("".join(parts[i:j]))
    return lits


def test_aucune_cle_orpheline():
    lits = _litteraux_du_code()
    for l in LANGS[1:]:
        orphelines = []
        for k in l["entries"]:
            ctx, txt = gen_i18n.split_key(k)
            if txt not in lits or (ctx and ctx not in lits):
                orphelines.append(k)
        assert not orphelines, (
            f"{l['file']} : clé(s) sans texte correspondant dans le code "
            f"(faute de frappe ou texte modifié ?) : {orphelines[:10]}")


def test_formats_et_noms_conserves():
    for l in LANGS[1:]:
        for k, v in l["entries"].items():
            if v is None:
                continue
            txt = gen_i18n.split_key(k)[1]
            assert RE_PRINTF.findall(txt) == RE_PRINTF.findall(v), (
                f"{l['file']} : spécificateurs printf différents pour {k!r}")
            assert sorted(RE_NOM.findall(txt)) == sorted(RE_NOM.findall(v)), (
                f"{l['file']} : {{noms}} différents pour {k!r}")


def test_traductions_couvertes_par_les_polices():
    """Toutes les polices de texte portent le jeu `&latin1` (tab5-styles.yaml) : une
    traduction hors Latin-1 s'afficherait en carrés vides. Une langue qui en a besoin
    (polonais, tchèque…) doit d'abord étendre ce jeu de glyphes."""
    glyphes = font_glyphs(REPO / "Tab5" / "tab5-styles.yaml")
    latin1 = glyphes.get("roboto_32_b")
    assert latin1, "jeu de glyphes de roboto_32_b introuvable"
    for l in LANGS[1:]:
        for k, v in l["entries"].items():
            manquants = sorted(set(v or "") - latin1 - {"\n"})
            assert not manquants, f"{l['file']} : {k!r} utilise {''.join(manquants)!r}, absent des polices"


def test_textes_francais_couverts_par_les_polices():
    """Même garde côté français : un texte de l'écran (clé tr() ou texte YAML) dont un
    caractère manque aux polices ne peut pas s'afficher. Trouvé au lot 4b : le « → » du
    pied de page de l'arcade, absent du jeu &latin1."""
    latin1 = font_glyphs(REPO / "Tab5" / "tab5-styles.yaml").get("roboto_32_b")
    fichiers = i18n_keys.fichiers_source()
    cles = set(i18n_keys.cles_tr(fichiers)) | set(i18n_keys.textes_yaml(fichiers))
    fautifs = {k: "".join(sorted(set(gen_i18n.split_key(k)[1]) - latin1 - {"\n"})) for k in cles}
    fautifs = {k: v for k, v in fautifs.items() if v}
    assert not fautifs, f"caractères absents des polices dans des textes affichés : {fautifs}"


def test_traduction_au_demarrage_branchee():
    entree = (REPO / "tab5-ha-hmi.yaml").read_text(encoding="utf-8")
    bloc = entree.split("- priority: -100", 1)[1]
    avant_delai = bloc.split("- delay: 2s", 1)[0]
    assert "i18n_apply_boot(" in avant_delai and "id(page_main)->obj" in avant_delai, (
        "on_boot -100 doit appeler i18n_apply_boot() avant son delay (traduction des textes du YAML)")
