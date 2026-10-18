# -*- coding: utf-8 -*-
"""Popup télécommande à plusieurs télécommandes (ADR-0056, 10/10/2026) : la TV Samsung,
une Apple TV et un Freebox Player chez l'auteur. Le firmware ne nomme aucune entité : le
blueprint « Tab5 — emplacements » pousse la liste (clé « telecommandes|écran|nom|… » de
tab5_maj_emplacements) et traduit les touches de la page montrée (emplacement « tv »,
« tv1 »…) pour la bonne télécommande. Aucun compilateur ne compare ces chaînes ; ce fichier
le fait :

- à la lecture : codes d'écran, emplacements et nombre de télécommandes égaux entre
  Tab5/socle/tab5_parse.* et le blueprint ; chaque touche de l'écran traduite pour un
  boîtier, sauf celles que le popup cache sur un boîtier ; le glissement par la brique
  commune des popups à pages (ADR-0046) ;
- au rendu (les vrais modèles du blueprint, tests/test_tuiles_blueprint.py) : la clé
  poussée à la connexion, les noms sans la pièce, et l'aiguillage de chaque commande."""
import re

import pytest
from tests import test_tuiles_blueprint as tb
from tests.commun import CARTES, _jetons as jetons_resolus, lire, resoudre, source
from tests.test_tuiles_blueprint import Etat, Passage, _chercher, _evenement, _rendre

POPUP = ("Tab5", "ui_components", "tv_remote_popup.yaml")

SAMSUNG = "remote.tele_samsung"
APPLE_TV = "remote.boitier_apple"
FREEBOX = "remote.boitier_freebox"

# Trois appareils comme ceux de l'auteur (identifiants inventés ; noms construits comme ceux\n# que HA lui donne, lus le 10/10/2026 par ha_eval_template).
APPAREILS = {
    SAMSUNG: {"id": "d_samsung", "name": "Samsung TV", "name_by_user": "Salon · TV Samsung (télécommande)",
              "entites": [SAMSUNG, "media_player.tele_samsung"]},
    APPLE_TV: {"id": "d_apple", "name": "Apple TV", "name_by_user": "Salon · Apple TV",
               "entites": [APPLE_TV, "media_player.boitier_apple"]},
    FREEBOX: {"id": "d_freebox", "name": "Freebox Player", "name_by_user": "Salon · Freebox Player (Apple TV 3)",
              "entites": [FREEBOX, "media_player.boitier_freebox"]},
}
BOITIERS = [APPLE_TV, FREEBOX, "media_player.boitier_apple", "media_player.boitier_freebox"]


def _maison(lecteur_apple="playing"):
    return [
        Etat(SAMSUNG, "on", "Salon", friendly_name="Salon · TV Samsung (télécommande)"),
        Etat(APPLE_TV, "on", "Salon", friendly_name="Salon · Apple TV"),
        Etat(FREEBOX, "on", "Salon", friendly_name="Salon · Freebox Player (Apple TV 3)"),
        Etat("media_player.tele_samsung", "on", "Salon"),
        Etat("media_player.boitier_apple", lecteur_apple, "Salon"),
        Etat("media_player.boitier_freebox", "idle", "Salon"),
        Etat("media_player.tele", "on", "Salon", friendly_name="TV"),
        Etat("script.tab5_tv_app", "off"),
    ]


@pytest.fixture(autouse=True)
def _appareils(monkeypatch):
    """Les fonctions d'appareil et d'intégration que le blueprint appelle pour les
    télécommandes, ajoutées à l'imitation de test_tuiles_blueprint."""
    original = tb._environnement
    par_id = {a["id"]: a for a in APPAREILS.values()}

    def environnement(etats, tablettes):
        env = original(etats, tablettes)
        integration = env.globals["integration_entities"]
        attribut = env.globals["device_attr"]
        env.globals.update(
            integration_entities=lambda d: list(BOITIERS) if d == "apple_tv" else integration(d),
            device_id=lambda e: APPAREILS.get(e, {}).get("id"),
            device_attr=lambda dev, nom: par_id[dev].get(nom) if dev in par_id else attribut(dev, nom),
            device_entities=lambda dev: list(par_id.get(dev, {}).get("entites", [])),
        )
        return env

    monkeypatch.setattr(tb, "_environnement", environnement)


ENTREES = {"tv": "media_player.tele", "tv_telecommande": SAMSUNG,
           "telecommandes_autres": [APPLE_TV, FREEBOX, SAMSUNG, "remote.inconnue", "remote.cinquieme"]}


# ─── Lecture : firmware = blueprint ───────────────────────────────────────────

def _bp():
    return tb._blueprint()


def test_codes_d_ecran_egaux_au_firmware():
    h = lire(source("tab5_parse.h"))
    codes = re.search(r"kTelecommandeEcranCodes\[[^\]]*\] = \{([^}]*)\}", h).group(1)
    assert re.findall(r'"(\w+)"', codes) == ["tv", "boitier"]
    enum = re.search(r"enum class TelecommandeEcran : uint8_t \{([^}]*)\};", h).group(1)
    assert re.findall(r"^\s*(\w+)", enum, re.M) == ["TV", "BOITIER", "NB"]
    texte = lire(tb.BLUEPRINT)
    assert "('boitier' if e in boitiers else 'tv')" in texte


def test_emplacements_et_nombre_egaux_au_firmware():
    cpp = lire(source("tab5_parse.cpp"))
    table = re.search(r"const char\* telecommande_emplacement\(int i\) \{.*?\{([^}]*)\}", cpp, re.S).group(1)
    firmware = re.findall(r'"(\w+)"', table)
    assert firmware == _bp()["variables"]["telecommande_emplacements"] == ["tv", "tv1", "tv2", "tv3"]
    assert re.search(r"constexpr int kTelecommandesMax = (\d+);", lire(source("tab5_parse.h"))).group(1) == "4"
    assert "ns.l | length < 4" in _bp()["variables"]["telecommandes"]
    # Un onglet par télécommande.
    popup = lire(*POPUP)
    assert [int(n) for n in re.findall(r"id: tv_onglet_(\d)", popup)] == [0, 1, 2, 3]
    assert "constexpr int kTelecommandeOnglets = 4;" in lire(source("tab5_telecommande.h"))


def _touches_firmware():
    touches = set()
    for nom in ("tv_remote_popup.yaml", "tv_touche.yaml", "tv_pad_btn.yaml", "tv_transport_btn.yaml"):
        touches |= set(re.findall(r"\bKEY_[A-Z]+\b", lire("Tab5", "ui_components", nom)))
    return touches


def test_chaque_touche_traduite_pour_un_boitier():
    table = _bp()["variables"]["telecommande_apple_tv"]
    sans = _touches_firmware() - set(table)
    # Pas de muet ni de source chez pyatv : ces deux touches sont cachées sur un boîtier.
    assert sans == {"KEY_MUTE", "KEY_SOURCE"}, sans
    assert set(table) <= _touches_firmware(), "une traduction qu'aucune touche n'envoie"
    popup = lire(*POPUP)
    # La ligne de l'include (ses vars contiennent des `${ … }` depuis le lot 4 du portage).
    source_ = popup.split("id: tv_touche_source,", 1)[1].split("\n", 1)[0]
    assert 'touche: "KEY_SOURCE"' in source_
    muet = popup.split("id: tv_touche_muet\n", 1)[1].split("- label:", 1)[0]
    assert 'touche: "KEY_MUTE"' in muet
    peindre = lire(source("tab5_telecommande.cpp")).split("void peindre() {", 1)[1].split("\n}", 1)[0]
    assert "ui_hidden(u.source, !tv);" in peindre and "ui_hidden(u.muet, !tv);" in peindre


def test_glissement_par_la_brique_des_pages():
    cpp = lire(source("tab5_telecommande.cpp"))
    assert "pages_brancher(&s_pages);" in cpp and "pages_onglets(u.onglet," in cpp
    assert "LV_EVENT_GESTURE" not in cpp
    for ident in ("tv_onglet_0", "tv_onglet_3"):
        assert f"id: {ident}" in lire(*POPUP)
    assert "choisir: telecommande_page" in lire(*POPUP)


def test_toutes_les_touches_portent_l_emplacement_de_la_page():
    scripts = lire(source("tab5-scripts.yaml")).split("- id: tab5_tv_key", 1)[1].split("\n  - id:", 1)[0]
    assert "emplacement: !lambda 'return telecommande_cle();'" in scripts
    assert "emplacement: !lambda 'return telecommande_cle();'" in lire("Tab5", "ui_components", "tv_app_btn.yaml")
    popup = lire(*POPUP)
    assert "emplacement: tv," not in popup and "emplacement: !lambda 'return telecommande_cle();', commande: alimentation" in popup


# ─── Rendu : clé poussée ──────────────────────────────────────────────────────

def _payload(entrees, declencheur="connexion", etats=None):
    p = Passage(entrees, _maison() if etats is None else etats, _evenement(declencheur))
    return p, p.variables_du_bloc("payload")


def test_cle_poussee_a_la_connexion():
    p, payload = _payload(ENTREES)
    # Sans doublon, quatre au plus, dans l'ordre ; une entité inconnue garde sa place, sans
    # nom (le firmware écrit « Télécommande 4 », jamais l'identifiant).
    assert p["telecommandes"] == [SAMSUNG, APPLE_TV, FREEBOX, "remote.inconnue"]
    cle = [e for e in payload.split(";") if e.startswith("telecommandes|")]
    assert cle == ["telecommandes|tv|TV Samsung|boitier|Apple TV|boitier|Freebox Player|tv|"]


def test_sans_telecommande_la_cle_vide_efface_les_pages():
    _, payload = _payload({"tv": "media_player.tele"})
    assert "telecommandes|;" in payload


def test_nom_sans_separateur_du_payload():
    APPAREILS[APPLE_TV]["name_by_user"] = "Apple | TV; bureau"
    try:
        _, payload = _payload({"tv_telecommande": APPLE_TV})
    finally:
        APPAREILS[APPLE_TV]["name_by_user"] = "Salon · Apple TV"
    assert "telecommandes|boitier|Apple / TV, bureau;" in payload


def test_cle_seulement_avec_tous_les_etats():
    p = Passage(ENTREES, _maison(), {"id": "mesures", "platform": "time_pattern"})
    assert p["tout_pousser"] is False
    assert "telecommandes|" not in p.variables_du_bloc("payload")


# ─── Rendu : aiguillage des commandes ─────────────────────────────────────────

def _executer(p, sequence):
    """(action, cible, données) de la séquence : variables, if, choose, actions."""
    envoyees = []
    for etape in sequence:
        if "variables" in etape:
            for cle, valeur in etape["variables"].items():
                p.ctx[cle] = _rendre(p.env, valeur, p.ctx)
        elif "if" in etape:
            envoyees += _executer(p, etape["then"] if p.modele(etape["if"]) else etape.get("else", []))
        elif "choose" in etape:
            for option in etape["choose"]:
                if p.modele(option["conditions"]):
                    envoyees += _executer(p, option["sequence"])
                    break
        elif "action" in etape:
            cible = _rendre(p.env, etape.get("target", {}), p.ctx).get("entity_id")
            envoyees.append((p.modele(etape["action"]), cible, _rendre(p.env, etape.get("data", {}), p.ctx)))
    return envoyees


def _commande(emplacement, action, valeur="", entrees=ENTREES, etats=None):
    p = Passage(entrees, _maison() if etats is None else etats,
                _evenement("action", emplacement=emplacement, action=action, valeur=valeur))
    alias, sequence = p.aiguillage()
    return alias, _executer(p, sequence)


@pytest.mark.parametrize("emplacement, touche, attendu", [
    ("tv", "KEY_ENTER", (SAMSUNG, "KEY_ENTER")),          # TV : le code tel quel
    ("tv", "KEY_SOURCE", (SAMSUNG, "KEY_SOURCE")),
    ("tv1", "KEY_ENTER", (APPLE_TV, "select")),           # boîtier : traduit
    ("tv1", "KEY_RETURN", (APPLE_TV, "menu")),
    ("tv2", "KEY_MENU", (FREEBOX, "top_menu")),
    ("tv2", "KEY_PLAYPAUSE", (FREEBOX, "play_pause")),
    ("tv2", "KEY_REWIND", (FREEBOX, "skip_backward")),
    ("tv1", "KEY_VOLUP", (APPLE_TV, "volume_up")),
])
def test_touche_vers_la_bonne_telecommande(emplacement, touche, attendu):
    alias, envoyees = _commande(emplacement, "touche", touche)
    assert alias == "Télécommande : touche"
    assert envoyees == [("remote.send_command", attendu[0], {"command": attendu[1]})]


def test_touche_sans_equivalent_ou_sans_telecommande_n_envoie_rien():
    assert _commande("tv1", "touche", "KEY_MUTE") == ("Télécommande : touche", [])
    alias, envoyees = _commande("tv3", "touche", "KEY_ENTER")   # quatrième : entité inconnue de HA
    assert envoyees == [] and alias != "Télécommande : touche"
    alias, envoyees = _commande("tv", "touche", "KEY_ENTER", entrees={"tv": "media_player.tele"})
    assert envoyees == [] and alias != "Télécommande : touche"


@pytest.mark.parametrize("etat, commande", [("playing", "turn_off"), ("standby", "turn_on"), ("off", "turn_on")])
def test_alimentation_d_un_boitier_selon_son_lecteur(etat, commande):
    alias, envoyees = _commande("tv1", "alimentation", etats=_maison(lecteur_apple=etat))
    assert alias == "Télécommande : marche / arrêt"
    assert envoyees == [("remote.send_command", APPLE_TV, {"command": commande})]


def test_alimentation_de_la_tv_comme_avant():
    assert _commande("tv", "alimentation")[1] == [("remote.toggle", SAMSUNG, {})]
    # Rien de choisi pour les télécommandes (blueprint d'avant) : la TV du bouton « TV ».
    envoyees = _commande("tv", "alimentation", entrees={"tv": "media_player.tele"})[1]
    assert envoyees == [("media_player.toggle", "media_player.tele", {})]


def test_appli_depuis_une_page():
    alias, envoyees = _commande("tv", "appli", "netflix")
    assert alias.startswith("Télécommande : application")
    assert envoyees == [("script.tab5_tv_app", None, {"app": "netflix"})]


def test_une_branche_par_commande_du_popup():
    bp = _bp()
    branches = _chercher(bp["actions"], lambda d: any(
        (b.get("alias") or "").startswith("Télécommande : touche") for b in d.get("choose", [])))["choose"]
    alias = [b["alias"] for b in branches if b.get("alias", "").startswith("Télécommande")]
    assert alias == ["Télécommande : marche / arrêt", "Télécommande : touche",
                     "Télécommande : application (script tab5_tv_app, package tab5_tv.yaml)"]
    assert not any((b.get("alias") or "").startswith("TV : ") for b in branches)


@pytest.mark.parametrize("carte", CARTES)
def test_marche_arret_dans_sa_touche(carte):
    """« Marche / Arrêt » (142 px en roboto_22, qui ne rétrécit pas) : taille au contenu sur
    le Tab5, comme avant le portage ; sur une carte plus petite, il va à la ligne dans la
    touche (lot 4, ADR-0061), sans qu'un de ses deux morceaux (« Marche / » 87 px) déborde."""
    jetons = jetons_resolus(carte)
    texte = lire(*POPUP).replace("\r\n", "\n")
    bouton = re.search(r"# ── Colonne des commandes.*?\n\s+width: ([^\n]+)\n", texte, re.S)
    mot = re.search(r'text: "Marche / Arrêt"\n\s+align: LEFT_MID\n\s+x: "(.+)"\n\s+width: "(.+)"\n', texte)
    assert bouton and mot
    touche_l = int(resoudre(bouton.group(1), jetons))
    x, largeur = int(resoudre(mot.group(1), jetons)), resoudre(mot.group(2), jetons)
    if carte == "tab5":
        assert (touche_l, x, largeur) == (290, 82, "SIZE_CONTENT")
        assert x + 142 <= touche_l - 42
    else:
        assert 87 <= largeur and x + largeur <= touche_l - 42


def _ombre_max(style):
    """Plus grande marge de dessin qu'un des thèmes donne à `style` : demi-largeur + 1,
    étalement et plus grand décalage, comme lv_obj_calculate_ext_draw_size() de LVGL 9.5."""
    import gen_themes
    compiles = gen_themes.styles_compiles()
    plus = 0
    for theme in gen_themes.charger():
        for formes in gen_themes.formes_resolues(theme, compiles).values():
            f = {k: v[1] for k, v in formes.get(style, {}).items()}
            if f.get("shadow_width"):
                plus = max(plus, f["shadow_width"] // 2 + 1 + f.get("shadow_spread", 0)
                           + max(abs(f.get("shadow_ofs_x", 0)), abs(f.get("shadow_ofs_y", 0))))
    return plus


@pytest.mark.parametrize("carte", CARTES)
def test_ombres_des_touches_du_bord_pas_coupees(carte):
    """Le volume (à droite), Menu et Accueil (en bas), la dernière touche de la rangée basse :
    LVGL 9.5 coupe leur ombre au bord de leur conteneur, même en overflow_visible (coupée net
    sur le Tab5 comme sur la 7B jusqu'au 18/10/2026). Les conteneurs ont des marges
    intérieures, au moins l'ombre la plus large d'un thème, et restent dans la carte."""
    jetons = jetons_resolus(carte)
    bouton, carte_l = int(jetons["ombre_bouton_max"]), int(jetons["ombre_carte_max"])
    assert bouton >= _ombre_max("style_clim_btn") and carte_l >= _ombre_max("style_meteo_card")
    texte = lire(*POPUP).replace("\r\n", "\n")
    zone = texte.split("# ═══ ZONE PRINCIPALE", 1)[1].split("widgets:", 1)[0]
    assert "pad_left: ${ombre_carte_max}\n" in zone and "pad_right: ${ombre_carte_max}\n" in zone
    assert "pad_bottom: ${ombre_bouton_max}\n" in zone
    largeur = int(resoudre(re.search(r"width: (.+)\n", zone).group(1), jetons))
    assert largeur - 2 * carte_l == 1170 * int(jetons["corps_w"]) // 1202  # touches à leur place
    assert largeur <= int(jetons["modal_card_w"]) - 4
    for rangee in ("tv_rangee_applis", "tv_rangee_lecture"):
        bloc = texte.split(f"id: {rangee}\n", 1)[1].split("widgets:", 1)[0]
        for cote in ("left", "right", "bottom"):
            assert f"pad_{cote}: ${{ombre_bouton_max}}\n" in bloc, (rangee, cote)
        assert int(resoudre(re.search(r"y: (.+)\n", bloc).group(1), jetons)) <= -2  # dans la carte
