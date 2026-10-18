# -*- coding: utf-8 -*-
"""Playlists favorites du popup Musique (ADR-0068, 18/10/2026) : l'auteur touche une de ses
playlists YouTube / YouTube Music sur la tablette, elle part sur son Apple TV. Le firmware
ne connaît que des NOMS (clé « playlists|nom|… » de tab5_maj_emplacements) et renvoie un
RANG (événement esphome.tab5_lecteur, action « playlist ») ; le blueprint « Tab5 —
emplacements » garde les liens et lance script.tab5_lecteur_playlist
(packages/tab5_lecteur.yaml). Aucun compilateur ne compare ces chaînes ; ce fichier le fait :

- à la lecture : nombre de playlists, nom de la clé et action égaux entre le firmware et le
  blueprint ; la page par la seule brique de pages du popup Musique (ADR-0046, ADR-0067) ; aucun lien
  ni aucune entité vers la tablette ;
- au rendu (les vrais modèles du blueprint et du package, tests/test_tuiles_blueprint.py) :
  les lignes lues, la clé poussée, l'aiguillage de l'événement et le lien de YouTube
  traduit en adresse youtube:// de l'appli tvOS."""
import re

import pytest
import yaml
from tests import test_tuiles_blueprint as tb
from tests.commun import lire, source
from tests.test_telecommandes import _executer
from tests.test_tuiles_blueprint import Etat, Etats, Passage, _chercher, _evenement, _rendre

POPUP = ("Tab5", "ui_components", "lecteur_popup.yaml")
PACKAGE = ("HomeAssistant_Config", "packages", "tab5_lecteur.yaml")
BRANCHE = "La tablette ouvre le lecteur de musique depuis une tuile"

LIGNES = "\n".join([
    "Jazz du dimanche | https://music.youtube.com/playlist?list=PLjazz_01-x&si=abc",
    "",
    "# une ligne de commentaire",
    "Sans lien |",
    "https://www.youtube.com/watch?v=Vid_01&list=PLsanstitre",
    "Rock | des années 80 | https://youtu.be/Court01?list=PLrock&si=z",
    "Point; virgule | https://m.youtube.com/playlist?list=PLpv",
])


# ─── Lecture : firmware = blueprint ───────────────────────────────────────────

def test_nombre_et_cle_egaux_au_firmware():
    h = lire(source("tab5_parse.h"))
    assert re.search(r"constexpr int kPlaylistsMax = (\d+);", h).group(1) == "8"
    assert "ns.l | count < 8" in tb._blueprint()["variables"]["playlists"]
    assert 'constexpr char kClePlaylists[] = "playlists";' in lire(source("tab5_zones.cpp"))
    assert tb._blueprint()["variables"]["playlists_pousse"].startswith("playlists|")
    popup = lire(*POPUP)
    assert [int(n) for n in re.findall(r'file: lecteur_playlist\.yaml, vars: \{ n: "(\d)" \}', popup)] == list(range(8))
    assert "constexpr int kLecteurPlaylists = 8;" in lire(source("tab5_lecteur.h"))


def test_l_ecran_envoie_le_rang_jamais_un_lien():
    cpp = lire(source("tab5_lecteur.cpp"))
    assert 'ui().envoyer("playlist", "-1", v);' in cpp
    # Le blueprint lit cette action et ce champ.
    assert "(trigger.event.data.action | default('') | string) == 'playlist'" in lire(tb.BLUEPRINT)
    assert "trigger.event.data.valeur" in lire(tb.BLUEPRINT)


def test_pages_par_la_brique_commune():
    """Une seule brique de pages pour le popup Musique (ADR-0067) : Lecture, Playlists,
    Pièces, Bibliothèque. La page Playlists existe dès une playlist ; le lecteur n'a ni
    glissement ni noms de pages à lui, il peint la grille et demande le retour à Lecture."""
    musique = lire(source("tab5_musique.cpp"))
    assert "case Page::PLAYLISTS: return lecteur_playlists_nb() > 0;" in musique
    assert musique.count("pages_brancher(&s_pages);") == 1
    assert "constexpr uint32_t kRetourMs = 600;" in musique
    assert "void musique_retour_lecture() { retour_programmer(Page::PLAYLISTS, Page::LECTURE); }" in musique
    cpp = lire(source("tab5_lecteur.cpp"))
    assert "pages_brancher(" not in cpp and "PagesPopup" not in cpp
    assert "LV_EVENT_GESTURE" not in cpp and "lv_anim" not in cpp
    assert "constexpr uint32_t kPlaylistEnCoursMs = 30u * 1000u;" in cpp
    playlist = cpp[cpp.index("void lecteur_playlist(int i)"):]
    assert "musique_retour_lecture();" in playlist[:playlist.index("\n}\n")]
    assert "lecteur_onglet" not in lire(*POPUP)
    calques = lire("Tab5", "ui_components", "musique_calques.yaml")
    for i in range(4):
        assert f"file: pages_onglet.yaml, vars: {{ id: musique_onglet_{i}, page: {i}, choisir: musique_onglet }}" in calques


def test_le_script_de_commande_ignore_la_playlist():
    script = yaml.safe_load(lire(*PACKAGE))["script"]["tab5_lecteur_commande"]
    texte = yaml.safe_dump(script["sequence"], allow_unicode=True)
    assert "playlist" not in texte, "le lien est au blueprint : aucune branche « playlist » ici"


# ─── Rendu du blueprint : lignes, clé poussée, aiguillage ────────────────────

def _passage(lignes=LIGNES, declencheur=None):
    return Passage({"playlists": lignes}, [], declencheur or _evenement("connexion"))


def test_lignes_lues():
    assert _passage()["playlists"] == [
        {"nom": "Jazz du dimanche", "lien": "https://music.youtube.com/playlist?list=PLjazz_01-x&si=abc"},
        {"nom": "Playlist 2", "lien": "https://www.youtube.com/watch?v=Vid_01&list=PLsanstitre"},
        {"nom": "Rock / des années 80", "lien": "https://youtu.be/Court01?list=PLrock&si=z"},
        {"nom": "Point, virgule", "lien": "https://m.youtube.com/playlist?list=PLpv"},
    ]


def test_huit_au_plus():
    lignes = "\n".join(f"P{i} | https://youtube.com/playlist?list=L{i}" for i in range(12))
    assert [p["nom"] for p in _passage(lignes)["playlists"]] == [f"P{i}" for i in range(8)]


def test_cle_poussee_avec_tous_les_etats():
    p = _passage()
    payload = p.variables_du_bloc("payload")
    cle = [e for e in payload.split(";") if e.startswith("playlists|")]
    assert cle == ["playlists|Jazz du dimanche|Playlist 2|Rock / des années 80|Point, virgule"]
    assert "youtu" not in payload and "list=" not in payload, "aucun lien vers la tablette"


def test_vide_efface_la_page():
    assert "playlists|;" in _passage("").variables_du_bloc("payload")


def _aiguiller(valeur, action="playlist", etats=(), lecteur="-1"):
    """(alias, actions envoyées) de la branche que l'événement esphome.tab5_lecteur prend."""
    p = Passage({"playlists": LIGNES}, list(etats),
                _evenement("lecteur", action=action, lecteur=lecteur, valeur=valeur))
    branches = _chercher(p.corps["actions"], lambda d: any(
        b.get("alias") == BRANCHE for b in d.get("choose", [])))["choose"]
    for b in branches:
        if p.modele(b["conditions"]):
            return b["alias"], _executer(p, b["sequence"])
    return None, []


SCRIPT = Etat("script.tab5_lecteur_playlist", "off")
LANCER = ("script.turn_on", "script.tab5_lecteur_playlist",
          {"variables": {"lien": "https://youtu.be/Court01?list=PLrock&si=z"}})


def test_playlist_lancee_par_son_rang():
    # Sans le package (script absent) : rien ne part.
    assert _aiguiller("2") == (BRANCHE, [])
    assert _aiguiller("2", etats=[SCRIPT]) == (BRANCHE, [LANCER])


@pytest.mark.parametrize("valeur", ["4", "-1", "abc", ""])
def test_rang_hors_liste_ne_lance_rien(valeur):
    assert _aiguiller(valeur, etats=[SCRIPT]) == (BRANCHE, [])


def test_ouvrir_ne_lance_aucune_playlist():
    # « ouvrir » porte la clé d'une tuile (« tRT ») ou rien, jamais « -1 ».
    assert _aiguiller("2", action="ouvrir", etats=[SCRIPT], lecteur="t00") == (BRANCHE, [])


# ─── Rendu du package : le lien devient l'adresse de l'appli YouTube ─────────

APPLE = "media_player.salon_apple"
TELE = "media_player.tele_samsung"


def _variables_du_script(lien, montre=APPLE, choix=f"{TELE},{APPLE}", sources=("Netflix", "YouTube")):
    etats = Etats([
        Etat("input_text.tab5_choix_lecteurs", choix),
        Etat("sensor.tab5_lecteur_montre", montre),
        Etat(APPLE, "off", source_list=list(sources)),
        Etat(TELE, "playing", source_list=["YouTube"]),
    ])
    env = tb._environnement(etats, {})
    env.globals.update(
        integration_entities=lambda d: [APPLE, "remote.salon_apple"] if d == "apple_tv" else [],
        device_id=lambda e: "d_apple" if e == APPLE else None,
        device_entities=lambda d: [APPLE, "remote.salon_apple"] if d == "d_apple" else [],
    )
    script = yaml.safe_load(lire(*PACKAGE))["script"]["tab5_lecteur_playlist"]
    assert script["mode"] == "restart"
    ctx = {"lien": lien}
    for cle, valeur in script["sequence"][0]["variables"].items():
        ctx[cle] = _rendre(env, valeur, ctx)
    return ctx


@pytest.mark.parametrize("lien, url", [
    ("https://music.youtube.com/playlist?list=PLjazz_01-x&si=abc", "youtube://www.youtube.com/watch?list=PLjazz_01-x"),
    ("https://www.youtube.com/watch?v=Vid_01&list=PLx&index=3", "youtube://www.youtube.com/watch?v=Vid_01&list=PLx"),
    ("https://youtu.be/Court01?list=PLrock&si=z", "youtube://www.youtube.com/watch?v=Court01&list=PLrock"),
    ("https://m.youtube.com/playlist?si=q&list=PLpv", "youtube://www.youtube.com/watch?list=PLpv"),
    (" https://music.youtube.com/watch?v=Abc&list=RDCLAK5uy_k ", "youtube://www.youtube.com/watch?v=Abc&list=RDCLAK5uy_k"),
])
def test_adresse_de_l_appli_youtube(lien, url):
    assert _variables_du_script(lien)["url"] == url


def test_lien_sans_list_refuse():
    ctx = _variables_du_script("https://www.youtube.com/watch?v=Vid_01")
    assert ctx["id_liste"] == ""
    script = yaml.safe_load(lire(*PACKAGE))["script"]["tab5_lecteur_playlist"]
    assert script["sequence"][1]["if"] == "{{ id_liste == '' }}"
    assert script["sequence"][1]["then"][-1] == {"stop": "Lien sans list="}


def test_apple_tv_choisie():
    # Le lecteur montré est une Apple TV avec YouTube : c'est elle, et sa télécommande.
    ctx = _variables_du_script("https://youtu.be/a?list=L")
    assert (ctx["cible"], ctx["telecommande"]) == (APPLE, "remote.salon_apple")
    # Le lecteur montré est la TV (pas de l'intégration apple_tv) : la première de la liste qui convient.
    assert _variables_du_script("https://youtu.be/a?list=L", montre=TELE)["cible"] == APPLE
    # Apple TV sans l'appli YouTube : aucune cible (notification, arrêt).
    ctx = _variables_du_script("https://youtu.be/a?list=L", sources=("Netflix",))
    assert (ctx["cible"], ctx["telecommande"]) == ("", "")


def test_sequence_calee_sur_l_apple_tv():
    """La séquence mesurée le 18/10/2026 (ADR-0068) : allumer si en veille (≤ 10 s, sinon un
    second réveil et ≤ 15 s : une Apple TV qui vient de s'endormir ignore le premier), le
    lien, ≤ 6 s pour jouer, sinon « select » puis ≤ 5 s, sinon media_play ; tout en
    continue_on_error."""
    seq = yaml.safe_load(lire(*PACKAGE))["script"]["tab5_lecteur_playlist"]["sequence"]
    actions = [e.get("action") for e in seq if "action" in e]
    assert actions.index("input_text.set_value") < actions.index("media_player.play_media")
    allumer = next(e for e in seq if "if" in e and "standby" in e["if"])
    assert allumer["then"][0]["action"] == "media_player.turn_on" and allumer["then"][1]["timeout"] == "00:00:10"
    second = allumer["then"][2]
    assert second["if"] == "{{ not wait.completed }}"
    assert second["then"][0]["action"] == "media_player.turn_on" and second["then"][1]["timeout"] == "00:00:15"
    jouer = next(e for e in seq if e.get("action") == "media_player.play_media")
    assert jouer["data"] == {"media_content_type": "url", "media_content_id": "{{ url }}"}
    attente = seq[seq.index(jouer) + 1]
    assert attente["timeout"] == "00:00:06" and "last_changed" in attente["wait_template"]
    secours = seq[seq.index(jouer) + 2]
    assert secours["if"] == "{{ not wait.completed }}"
    texte = yaml.safe_dump(secours, allow_unicode=True)
    assert "command: select" in texte and "00:00:05" in texte and "media_player.media_play" in texte
    # Chaque action sur le lecteur ou sa télécommande peut échouer sans arrêter la suite.
    def actions_de_lecteur(noeud):
        if isinstance(noeud, dict):
            if str(noeud.get("action", "")).startswith(("media_player.", "remote.")):
                yield noeud
            for v in noeud.values():
                yield from actions_de_lecteur(v)
        elif isinstance(noeud, list):
            for v in noeud:
                yield from actions_de_lecteur(v)
    trouvees = list(actions_de_lecteur(seq))
    # turn_on ×2 (le second si le premier réveil est ignoré), play_media, select, media_play.
    assert len(trouvees) == 5 and all(a.get("continue_on_error") is True for a in trouvees)
