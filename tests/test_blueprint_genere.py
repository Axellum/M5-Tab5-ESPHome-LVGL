"""Déclencheurs générés du blueprint « Tab5 — emplacements » (audit du 07/10/2026, HA-8).

tools/gen_blueprint_emplacements.py écrit les déclencheurs des 5 pièces (6 chacune) et
des 3 lignes de la rangée (2 chacune) entre deux paires de marqueurs. Ce qu'aucun
chargement de HA ne voit : une pièce oubliée, un attribut recopié de travers, une
partie générée retouchée à la main puis écrasée au prochain passage du script.
"""
from __future__ import annotations

import sys
from pathlib import Path

import yaml

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))

import gen_blueprint_emplacements as gen  # noqa: E402


class _Chargeur(yaml.SafeLoader):
    pass


_Chargeur.add_constructor("!input", lambda l, n: {"!input": l.construct_scalar(n)})


def _blueprint() -> dict:
    return yaml.load(gen.BLUEPRINT.read_text(encoding="utf-8"), Loader=_Chargeur)


def test_le_blueprint_commite_est_la_sortie_du_generateur(capsys):
    assert gen.main(["--check"]) == 0, capsys.readouterr().out


def test_check_echoue_sur_une_partie_retouchee_sans_rien_ecrire(monkeypatch, tmp_path):
    copie = tmp_path / "tab5_emplacements.yaml"
    texte = gen.BLUEPRINT.read_bytes().replace(b"id: piece_3_couleur", b"id: piece_3_teinte")
    copie.write_bytes(texte)
    monkeypatch.setattr(gen, "BLUEPRINT", copie)
    monkeypatch.setattr(gen, "REPO", tmp_path)
    assert gen.main(["--check"]) == 1
    assert copie.read_bytes() == texte, "--check ne doit rien écrire"
    assert gen.main([]) == 0
    assert gen.main(["--check"]) == 0
    # Fins de ligne du fichier gardées, le reste à l'octet près.
    assert copie.read_bytes() == gen.BLUEPRINT.read_bytes()


def test_un_seul_jeu_de_marqueurs_exige(monkeypatch, tmp_path):
    copie = tmp_path / "tab5_emplacements.yaml"
    copie.write_bytes(gen.BLUEPRINT.read_bytes().replace(gen.MARQUES_RANGEE[1].encode("utf-8"), b"# fin"))
    monkeypatch.setattr(gen, "BLUEPRINT", copie)
    assert gen.main(["--check"]) == 1


def test_chaque_piece_et_chaque_ligne_ont_tous_leurs_declencheurs():
    declencheurs = _blueprint()["triggers"]
    ids = [t.get("id") for t in declencheurs]
    assert len(ids) == len(set(ids)), "id de déclencheur en double"
    par_id = {t["id"]: t for t in declencheurs}
    entrees = _blueprint()["blueprint"]["input"]
    noms = {n for section in entrees.values() for n in (section.get("input") or {})}
    for n in range(1, gen.PIECES + 1):
        assert f"piece_{n}_tuiles" in noms
        attendus = [f"piece_{n}", f"piece_{n}_sortie"] + [f"piece_{n}_{s}" for s, _ in gen.ATTRIBUTS_PIECE]
        for ident in attendus:
            assert par_id[ident]["entity_id"] == {"!input": f"piece_{n}_tuiles"}, ident
        for suffixe, attribut in gen.ATTRIBUTS_PIECE:
            assert par_id[f"piece_{n}_{suffixe}"]["attribute"] == attribut
    for n in range(1, gen.LIGNES_RANGEE + 1):
        assert f"rangee_ligne_{n}" in noms
        for ident in (f"rangee_{n}", f"rangee_{n}_sortie"):
            assert par_id[ident]["entity_id"] == {"!input": f"rangee_ligne_{n}"}, ident
    # « on » et « off » restent des textes (YAML 1.1 en ferait des booléens).
    assert {"on", "off"} <= set(par_id["piece_1"]["to"])
    assert par_id["rangee_3"]["to"] == par_id["piece_1"]["to"]


def test_une_seule_liste_des_declenchements_qui_poussent_tout():
    """La liste « connexion, rechargement, MAJ Écran, démarrage de HA » n'existe qu'une
    fois (variable tout_pousser) ; recopiée, un oubli dans une copie ne poussait qu'une
    partie de l'écran (le démarrage de HA ajouté le 28/09 a dû l'être 9 fois)."""
    texte = gen.BLUEPRINT.read_text(encoding="utf-8")
    assert texte.count("'connexion', 'rechargement'") == 1
    assert _blueprint()["variables"]["tout_pousser"] == (
        "{{ trigger.id in ['connexion', 'rechargement', 'maj_ecran', 'demarrage_ha'] }}")
    assert texte.count("tout_pousser") >= 9


def test_l_etat_de_la_clim_part_avec_la_meme_action_dans_les_deux_branches():
    """Ancre &maj_clim : le changement de la clim et la poussée complète envoient les
    mêmes champs à tab5_maj_clim."""
    texte = gen.BLUEPRINT.read_text(encoding="utf-8")
    assert texte.count("_tab5_maj_clim\"") == 1
    assert texte.count("- &maj_clim\n") == 1 and texte.count("- *maj_clim\n") == 1
