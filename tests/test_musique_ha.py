# -*- coding: utf-8 -*-
"""Musique de toute la maison, côté Home Assistant (ADR-0067, 18/10/2026) : les pages
« Pièces » et « Bibliothèque » du popup Musique. Le firmware ne reçoit que deux chaînes
(tab5_maj_musique_pieces, tab5_maj_musique_liste) et renvoie un index et une ref
(événement esphome.tab5_musique) ; aucun compilateur ne compare ces chaînes au contrat.
Ce fichier rend les VRAIS modèles (custom_templates/tab5_musique.jinja,
packages/tab5_musique.yaml) dans le bac à sable de Jinja, avec de fausses maisons :

- la plus petite (sans Music Assistant : la liste « Tab5 · music players ») ;
- Music Assistant avec un groupe, un lecteur « groupe », la tablette enveloppée par le
  fournisseur « Home Assistant MediaPlayers », neuf lecteurs (huit gardés) ;
- des noms avec « | » et « ; », des adresses d'image à réduire, des refs à refuser.

Les scripts sont exécutés pas à pas comme HA le fait (résultat d'un modèle relu comme un
littéral, condition vraie seulement rendue « true », réponse d'une action dans une
variable) ; les actions sont relevées au lieu d'être appelées, les réponses de Music
Assistant ont la forme relevée sur le HA de l'auteur le 18/10/2026 (get_library,
get_queue, browse_media). Les mêmes macros y ont été rendues par ha_eval_template."""
import ast
import re

import jinja2
import pytest
import yaml
from jinja2.sandbox import ImmutableSandboxedEnvironment
from tests.commun import HA, REPO, CacheJinja, lire
from tests.test_tuiles_blueprint import MAINTENANT, Etat, Etats

PACKAGE = HA / "packages" / "tab5_musique.yaml"
EVENEMENTS = HA / "packages" / "tab5_evenements.yaml"
MACROS = HA / "custom_templates"
ADR = REPO / "docs" / "decisions" / "0067-hifi-music-assistant.md"

KPAYLOAD_MAX = 16 * 1024   # kPayloadMax du firmware (Tab5/socle/tab5_champs.h)
REF = re.compile(r"^[a-z0-9_-]+://[^|;\s]+$")
ENTREE_MA = "ENTREE_MA_DE_TEST"
TABLETTE_API = "binary_sensor.tab5_ha_api_status"
PREFIXE = "tab5_ha_hmi"

_JINJA = CacheJinja()
_NUMERIQUE = re.compile(r"^[+-]?(?!0\d)\d*(?:\.\d*)?$")


def _analyser(brut):
    """Template._parse_result de Home Assistant (2026.10) : le rendu relu par literal_eval,
    sauf une chaîne (le texte rendu reste), et un nombre seulement si le texte EST un
    nombre (« 1|5|… » ou « 1#… » restent du texte)."""
    try:
        valeur = ast.literal_eval(brut)
    except (ValueError, TypeError, SyntaxError, MemoryError):
        return brut
    if isinstance(valeur, (str, complex)):
        return brut
    if isinstance(valeur, bool) or not isinstance(valeur, (int, float)) or _NUMERIQUE.match(brut):
        return valeur
    return brut


def _package():
    return yaml.safe_load(lire(PACKAGE))


def _script(nom):
    return _package()["script"][nom]


# ─── Une fausse maison et le Jinja de HA, au plus près ────────────────────────

class Maison:
    """États, appareils et intégrations d'une maison de test ; `env` imite les fonctions
    de modèle de HA que les macros et le package appellent."""

    def __init__(self, etats, ma=(), modeles=None, ma_absent=False):
        self.etats = Etats(list(etats) + [Etat(TABLETTE_API, "on")])
        self.ma = [] if ma_absent else list(ma)
        self.modeles = dict(modeles or {})
        self.modeles.setdefault("dev_tablette", "tab5-ha-hmi")
        env = ImmutableSandboxedEnvironment(extensions=["jinja2.ext.loopcontrols"], undefined=jinja2.StrictUndefined,
                                            loader=jinja2.FileSystemLoader(str(MACROS)), bytecode_cache=_JINJA)
        etats_ = self.etats

        def device_attr(e, nom):
            if nom != "model":
                return None
            if e == TABLETTE_API:
                return "tab5-ha-hmi"
            return self.modeles.get(e)

        def integration_entities(domaine):
            if domaine == "music_assistant":
                return self.ma + [e.replace("media_player.", "button.") + "_favori" for e in self.ma]
            if domaine == "esphome":
                return [TABLETTE_API]
            return []

        def expand(ids):
            ids = [ids] if isinstance(ids, str) else list(ids)
            return sorted([s for s in (etats_[e] for e in set(ids)) if s is not None], key=lambda s: s.entity_id)

        def config_entry_id(e):
            return ENTREE_MA if e in integration_entities("music_assistant") else None

        env.globals.update(
            states=etats_, state_attr=etats_.attr, device_attr=device_attr, expand=expand,
            integration_entities=integration_entities, config_entry_id=config_entry_id,
            device_id=lambda e: "dev_tablette" if e == TABLETTE_API else None,
            device_entities=lambda d: [], now=lambda: MAINTENANT, is_state=lambda e, s: etats_(e) == s,
        )
        env.filters.update(
            device_attr=device_attr, config_entry_id=config_entry_id,
            bitwise_and=lambda a, b: int(a) & int(b),
            is_number=_est_un_nombre,
            regex_replace=lambda v, find="", replace="", ignorecase=False: re.sub(find, replace, str(v), flags=re.I if ignorecase else 0),
            regex_findall=lambda v, find="", ignorecase=False: re.findall(find, str(v), re.I if ignorecase else 0),
        )
        env.tests.update(
            match=lambda v, motif, ignorecase=False: bool(re.match(motif, str(v), re.I if ignorecase else 0)),
            is_state=lambda e, s: etats_(e) == s,
            is_state_attr=lambda e, a, v: etats_.attr(e, a) == v,
        )
        self.env = env

    def rendre(self, valeur, ctx):
        """Comme render_complex de HA : chaque modèle rendu puis relu comme un littéral."""
        if isinstance(valeur, str):
            if "{{" in valeur or "{%" in valeur:
                return _analyser(_JINJA.depuis_texte(self.env, valeur).render(ctx))
            return valeur
        if isinstance(valeur, list):
            return [self.rendre(v, ctx) for v in valeur]
        if isinstance(valeur, dict):
            return {k: self.rendre(v, ctx) for k, v in valeur.items()}
        return valeur

    def texte(self, modele, ctx=None):
        return _JINJA.depuis_texte(self.env, modele).render(ctx or {})

    def macro(self, appel):
        """Rend une macro de tab5_musique.jinja (« tab5_musique_pieces() »)."""
        nom = appel.split("(")[0]
        return self.texte("{%- from 'tab5_musique.jinja' import " + nom + " -%}{{ " + appel + " }}")

    def vrai(self, cond, ctx):
        """Condition de HA (condition.async_template) : vraie seulement rendue « true »."""
        if isinstance(cond, list):
            return all(self.vrai(c, ctx) for c in cond)
        if isinstance(cond, dict):
            if cond.get("condition") == "template":
                return self.vrai(cond["value_template"], ctx)
            if cond.get("condition") == "trigger":
                return ctx["trigger"]["id"] == cond["id"]
            raise AssertionError(f"condition non imitée : {cond}")
        return self.texte(cond, ctx).strip().lower() == "true"


def _est_un_nombre(v):
    try:
        float(v)
    except (TypeError, ValueError):
        return False
    return True


class Echec(Exception):
    """Une action qui échoue (Music Assistant arrêté, lecteur qui refuse)."""


class Arret(Exception):
    pass


class Script:
    """Exécute une séquence de script de HA : variables, condition, delay, stop, if,
    choose, repeat for_each, actions (relevées dans `appels`, réponse par `repondre`)."""

    def __init__(self, maison, repondre=None):
        self.m = maison
        self.repondre = repondre or (lambda nom, cible, donnees: {})
        self.appels = []

    def lancer(self, sequence, **champs):
        ctx = dict(champs)
        try:
            self._suite(sequence, ctx)
        except Arret:
            pass
        return ctx

    def _suite(self, sequence, ctx):
        for etape in sequence:
            if "variables" in etape:
                for cle, valeur in etape["variables"].items():
                    ctx[cle] = self.m.rendre(valeur, ctx)
            elif "condition" in etape:
                if not self.m.vrai(etape, ctx):
                    raise Arret
            elif "delay" in etape:
                pass
            elif "stop" in etape:
                raise Arret
            elif "if" in etape:
                self._suite(etape["then"] if self.m.vrai(etape["if"], ctx) else etape.get("else", []), ctx)
            elif "choose" in etape:
                for option in etape["choose"]:
                    if self.m.vrai(option["conditions"], ctx):
                        self._suite(option["sequence"], ctx)
                        break
                else:
                    self._suite(etape.get("default", []), ctx)
            elif "repeat" in etape:
                for item in self.m.rendre(etape["repeat"]["for_each"], ctx):
                    ctx["repeat"] = {"item": item}
                    self._suite(etape["repeat"]["sequence"], ctx)
            elif "action" in etape:
                nom = self.m.texte(etape["action"], ctx)
                cible = self.m.rendre(etape.get("target", {}), ctx).get("entity_id")
                donnees = self.m.rendre(etape.get("data", {}), ctx)
                self.appels.append((nom, cible, donnees))
                if "response_variable" in etape:
                    try:
                        ctx[etape["response_variable"]] = self.repondre(nom, cible, donnees)
                    except Echec:
                        assert etape.get("continue_on_error") is True, f"{nom} sans continue_on_error"
            else:
                raise AssertionError(f"étape non imitée : {etape}")


# ─── Les maisons ──────────────────────────────────────────────────────────────

GROUPAGE = 524288
VOLUME, MUET, ALLUMER, ETEINDRE, SOURCE = 4, 8, 128, 256, 2048
MA_FONCTIONS = 7796287          # relevé sur un lecteur Music Assistant (sans groupage)
MA_GROUPE = MA_FONCTIONS | GROUPAGE

TELE = "media_player.tele_salon"
ENCEINTE = "media_player.enceinte_cuisine"


def petite_maison(**montre):
    """Sans Music Assistant : trois lecteurs choisis (un en double, un disparu)."""
    return Maison([
        Etat("input_text.tab5_choix_lecteurs", f" {TELE}, {ENCEINTE},media_player.disparu,{TELE},sensor.pas_un_lecteur"),
        Etat("sensor.tab5_lecteur_montre", montre.get("montre", ENCEINTE)),
        Etat(TELE, "playing", friendly_name="Salon | TV; Samsung", device_class="tv",
             supported_features=VOLUME | MUET | ALLUMER | ETEINDRE | SOURCE, source_list=["HDMI 1", "Net|flix"],
             volume_level=0.35, is_volume_muted=False, media_title="Titre; long", media_artist="Artiste|X",
             group_members=[ENCEINTE, TELE]),
        Etat(ENCEINTE, "idle", friendly_name="Cuisine", device_class="speaker",
             supported_features=VOLUME | MUET | GROUPAGE, group_members=[ENCEINTE, TELE]),
        Etat("script.tab5_lecteur_montrer", "off"),
    ])


# Music Assistant : neuf lecteurs, huit gardés, triés par nom sans tenir compte de la casse.
MA = {
    "media_player.salon_ma": dict(friendly_name="Salon", supported_features=MA_GROUPE, volume_level=0.2,
                                  group_members=["media_player.salon_ma", "media_player.cuisine_ma"], state="playing",
                                  media_title="Song", media_artist="Artist"),
    "media_player.cuisine_ma": dict(friendly_name="Cuisine", supported_features=MA_GROUPE, volume_level=0.2,
                                    group_members=["media_player.salon_ma", "media_player.cuisine_ma"], state="playing",
                                    media_title="Song", media_artist="Artist"),
    "media_player.tablette_ma": dict(friendly_name="Tablette · haut-parleur", supported_features=MA_FONCTIONS,
                                     volume_level=0.2, active_queue="media_player.tablette_esphome", state="playing",
                                     media_content_id="library://radio/2", media_title="FIP",
                                     entity_picture="http://ha:8095/imageproxy/69d2?size=512&fmt=png"),
    "media_player.groupe_rdc": dict(friendly_name="Rez-de-chaussée", supported_features=MA_FONCTIONS,
                                    mass_player_type="group", group_members=["media_player.atelier_ma", "media_player.bureau_ma"]),
    "media_player.tele_ma": dict(friendly_name="Télé du salon", supported_features=MA_FONCTIONS),
    "media_player.bureau_ma": dict(friendly_name="bureau", supported_features=MA_FONCTIONS | ALLUMER | ETEINDRE,
                                   state="off"),
    "media_player.atelier_ma": dict(friendly_name="Atelier", supported_features=MA_GROUPE, is_volume_muted=False,
                                    volume_level=0.004),
    "media_player.zone_ma": dict(friendly_name="Zone 9", supported_features=MA_FONCTIONS),
    "media_player.veranda_ma": dict(friendly_name="Véranda", supported_features=MA_GROUPE, state="unavailable"),
}
ORDRE = ["media_player.atelier_ma", "media_player.bureau_ma", "media_player.cuisine_ma", "media_player.groupe_rdc",
         "media_player.salon_ma", "media_player.tablette_ma", "media_player.tele_ma", "media_player.veranda_ma"]


def maison_ma(montre="media_player.tablette_esphome", remplace=None):
    remplace = remplace or {}
    etats = [Etat("sensor.tab5_lecteur_montre", montre), Etat("script.tab5_lecteur_montrer", "off"),
             Etat("input_text.tab5_choix_lecteurs", TELE), Etat(TELE, "idle", friendly_name="Pas montré")]
    for e, a in MA.items():
        a = dict(a, **remplace.get(e, {}))
        etat = a.pop("state", "idle")
        # Comme sur le HA de l'auteur : un lecteur indisponible garde device_class speaker,
        # perd mass_player_type.
        a.setdefault("device_class", "speaker")
        if etat != "unavailable":
            a.setdefault("mass_player_type", "player")
        etats.append(Etat(e, etat, **a))
    return Maison(etats, ma=list(MA), modeles={"media_player.tablette_ma": "tab5-ha-hmi",
                                               "media_player.tele_ma": "QE55S95BATXXC"})


def _champs(pieces):
    return [p.split("|") for p in pieces.split(";")] if pieces else []


# ─── La liste des pièces ──────────────────────────────────────────────────────

def test_petite_maison_la_liste_choisie():
    m = petite_maison()
    assert m.macro("tab5_musique_ma()") == "0"
    assert m.macro("tab5_musique_entites()") == f"{TELE},{ENCEINTE},media_player.disparu"
    assert m.macro("tab5_musique_montre()") == "1"
    assert _champs(m.macro("tab5_musique_pieces()")) == [
        # nom|genre|etat|volume|muet|groupe|fonctions|titre ; « | » et « ; » remplacés.
        ["Salon / TV, Samsung", "t", "j", "35", "0", "1", "vmos", "Titre, long — Artiste/X"],
        ["Cuisine", "e", "r", "", "0", "1", "vmg", ""],
        ["", "x", "i", "", "0", "-1", "", ""],
    ]


def test_petite_maison_six_au_plus():
    lecteurs = [f"media_player.l{i}" for i in range(9)]
    m = Maison([Etat("input_text.tab5_choix_lecteurs", ",".join(lecteurs))]
               + [Etat(e, "idle", friendly_name=e) for e in lecteurs])
    assert m.macro("tab5_musique_entites()").split(",") == lecteurs[:6]


def test_music_assistant_huit_pieces_triees_par_nom():
    m = maison_ma()
    assert m.macro("tab5_musique_ma()") == "1"
    assert m.macro("tab5_musique_config()") == ENTREE_MA
    assert m.macro("tab5_musique_entites()").split(",") == ORDRE
    # La tablette montrée par son entité ESPHome : la pièce Music Assistant qui l'enveloppe.
    assert m.macro("tab5_musique_montre()") == "5"
    assert maison_ma(montre="media_player.salon_ma").macro("tab5_musique_montre()") == "4"
    # Un lecteur montré hors de Music Assistant : -1.
    assert maison_ma(montre=TELE).macro("tab5_musique_montre()") == "-1"


def test_music_assistant_pieces_groupe_et_genres():
    pieces = _champs(maison_ma().macro("tab5_musique_pieces()"))
    assert [p[0] for p in pieces] == ["Atelier", "bureau", "Cuisine", "Rez-de-chaussée", "Salon",
                                      "Tablette · haut-parleur", "Télé du salon", "Véranda"]
    assert [p[1] for p in pieces] == ["e", "e", "e", "g", "e", "a", "t", "e"]       # genre
    assert [p[2] for p in pieces] == ["r", "e", "j", "r", "j", "j", "r", "i"]       # état
    assert [p[3] for p in pieces] == ["0", "", "20", "", "20", "20", "", ""]        # volume
    # Salon mène Cuisine : les deux disent 4 ; un lecteur « groupe » de Music Assistant est
    # son propre genre, pas le meneur de ses membres.
    assert [p[5] for p in pieces] == ["-1", "-1", "4", "-1", "4", "-1", "-1", "-1"]
    assert [p[6] for p in pieces] == ["vmgt", "vmot", "vmgt", "vmt", "vmgt", "vmt", "vmt", "vmg"]
    assert pieces[4][7] == "Song — Artist" and pieces[5][7] == "FIP" and pieces[0][7] == ""


def test_noms_et_titres_coupes():
    long = "Une pièce au nom vraiment très long | avec ; tout"
    m = Maison([Etat("input_text.tab5_choix_lecteurs", TELE),
                Etat(TELE, "paused", friendly_name=long, media_title="T" * 80, media_artist="A")])
    (p,) = _champs(m.macro("tab5_musique_pieces()"))
    assert p[0] == "Une pièce au nom vraiment très long / av" and len(p[0]) == 40
    assert p[2] == "p" and len(p[7]) == 60


# ─── Poussée des pièces ───────────────────────────────────────────────────────

def _file(nom, cible, donnees):
    assert nom == "music_assistant.get_queue"
    return {cible: {"queue_id": "q", "items": 12, "current_item": {"name": "FIP"},
                    "next_item": {"name": "B - Song", "media_item": {"name": "Song", "artists": [{"name": "A"}, {"name": "B"}]}}}}


def _pousser(maison, repondre=_file):
    s = Script(maison, repondre)
    s.lancer(_script("tab5_musique_pieces_pousser")["sequence"])
    return s.appels


def test_poussee_avec_la_file_du_lecteur_montre():
    m = maison_ma()
    appels = _pousser(m)
    assert appels[0] == ("music_assistant.get_queue", "media_player.tablette_ma", {})
    assert appels[1] == (f"esphome.{PREFIXE}_tab5_maj_musique_pieces", None,
                         {"pieces": m.macro("tab5_musique_pieces()"), "contexte": "1|5|12|Song — A, B"})


def test_poussee_file_illisible_ou_sans_music_assistant():
    def panne(*_):
        raise Echec
    appels = _pousser(maison_ma(), panne)
    assert appels[-1][2]["contexte"] == "1|5|0|"
    # Sans Music Assistant : aucune action music_assistant (elle n'existe pas).
    appels = _pousser(petite_maison())
    assert [a[0] for a in appels] == [f"esphome.{PREFIXE}_tab5_maj_musique_pieces"]
    assert appels[0][2]["contexte"] == "0|1|0|"
    # Montré hors de Music Assistant : pas de file demandée.
    assert [a[0] for a in _pousser(maison_ma(montre=TELE))] == [f"esphome.{PREFIXE}_tab5_maj_musique_pieces"]


def test_poussee_toujours_envoyee_meme_vide():
    m = Maison([Etat("input_text.tab5_choix_lecteurs", "")])
    assert _pousser(m) == [(f"esphome.{PREFIXE}_tab5_maj_musique_pieces", None, {"pieces": "", "contexte": "0|-1|0|"})]


def test_poussee_rien_sans_tablette_connectee():
    m = maison_ma()
    m.etats.d[TABLETTE_API].state = "off"
    assert _pousser(m) == []


def test_miroir_suit_la_liste_poussee():
    m = maison_ma()
    capteur = _package()["template"][0]["sensor"][0]
    assert m.rendre(capteur["state"], {}) == 8
    assert m.rendre(capteur["attributes"]["signature"], {}) == "1|5|" + m.macro("tab5_musique_pieces()")
    assert str(petite_maison().rendre(capteur["attributes"]["signature"], {})).startswith("0|1|Salon / TV")
    auto = _package()["automation"][0]
    assert {t.get("entity_id") or t.get("event_type") for t in auto["triggers"]} == {
        "sensor.tab5_musique_signature", "esphome.tab5_connected"}
    poussee = _script("tab5_musique_pieces_pousser")
    assert poussee["mode"] == "restart" and poussee["sequence"][1] == {"delay": {"seconds": 1}}


# ─── Commandes : index, refs, modes ───────────────────────────────────────────

def _commande(maison, action, piece="-1", valeur="", repondre=None, appareil="dev_tablette"):
    s = Script(maison, repondre)
    ctx = s.lancer(_script("tab5_musique_commande")["sequence"],
                   commande=action, piece=_analyser(piece), valeur=_analyser(valeur), appareil=appareil)
    return s.appels, ctx


@pytest.mark.parametrize("valeur, attendu", [
    ("m|library://album/12", {"enqueue": "replace", "radio_mode": False}),
    ("s|ytmusic://playlist/VLRDCLAK5uy_k", {"enqueue": "next", "radio_mode": False}),
    ("f|library://track/3", {"enqueue": "add", "radio_mode": False}),
    ("r|library://radio/2", {"enqueue": "replace", "radio_mode": True}),
])
def test_jouer_sur_la_piece(valeur, attendu):
    appels, _ = _commande(maison_ma(), "jouer", "2", valeur)
    assert appels == [("music_assistant.play_media", "media_player.cuisine_ma",
                       dict(media_id=valeur.split("|", 1)[1], **attendu))]


def test_jouer_sur_le_lecteur_montre():
    appels, _ = _commande(maison_ma(), "jouer", "-1", "m|library://radio/2")
    assert [a[1] for a in appels] == ["media_player.tablette_ma"]


@pytest.mark.parametrize("piece, valeur", [
    ("8", "m|library://radio/2"),                 # hors liste
    ("-2", "m|library://radio/2"),
    ("abc", "m|library://radio/2"),
    ("2", "x|library://radio/2"),                 # mode inconnu
    ("2", "m|library://radio/2;rm"),              # « ; » refusé
    ("2", "m|library://track/a b"),               # espace refusé
    ("2", "m|Library://radio/2"),                 # majuscule refusée
    ("2", "m|media_player.salon_ma"),             # une entité n'est pas une ref
    ("2", "m|library://playlist/" + "x" * 150),   # plus de 160 caractères
    ("2", "m"),
])
def test_jouer_refuse(piece, valeur):
    assert _commande(maison_ma(), "jouer", piece, valeur)[0] == []


def test_jouer_rien_sans_music_assistant():
    assert _commande(petite_maison(), "jouer", "0", "m|library://radio/2")[0] == []
    # Montré hors de Music Assistant : pas de play_media sur ce lecteur-là.
    assert _commande(maison_ma(montre=TELE), "jouer", "-1", "m|library://radio/2")[0] == []


def test_meme_index_des_deux_cotes():
    """La pièce n poussée est celle que la commande n vise (une seule liste, la macro)."""
    m = maison_ma()
    noms = [p[0] for p in _champs(m.macro("tab5_musique_pieces()"))]
    for n, nom in enumerate(noms):
        appels, _ = _commande(m, "volume", str(n), "30")
        assert [a[1] for a in appels] == [ORDRE[n]]
        assert m.etats.attr(ORDRE[n], "friendly_name") == nom
    for texte in (_script("tab5_musique_pieces_pousser"), _script("tab5_musique_commande")):
        assert "import tab5_musique_entites" in yaml.safe_dump(texte, allow_unicode=True)


def test_montrer():
    appels, _ = _commande(maison_ma(), "montrer", "3")
    assert appels == [("script.turn_on", "script.tab5_lecteur_montrer", {"variables": {"entite": "media_player.groupe_rdc"}})]
    assert _commande(maison_ma(), "montrer", "-1")[0] == []      # le montré l'est déjà
    assert _commande(maison_ma(), "montrer", "9")[0] == []


def test_grouper_et_separer():
    m = maison_ma(montre="media_player.salon_ma")
    assert _commande(m, "grouper", "0")[0] == [("media_player.join", "media_player.salon_ma",
                                                {"group_members": ["media_player.atelier_ma"]})]
    # Sans groupage (bureau) ou le meneur lui-même : rien.
    assert _commande(m, "grouper", "1")[0] == [] and _commande(m, "grouper", "4")[0] == []
    # Le meneur sans groupage (la tablette) : rien.
    assert _commande(maison_ma(), "grouper", "0")[0] == []
    assert _commande(m, "separer", "2")[0] == [("media_player.unjoin", "media_player.cuisine_ma", {})]
    assert _commande(m, "separer", "0")[0] == []                # pas groupé


def test_tout_grouper_tout_separer():
    m = maison_ma(montre="media_player.salon_ma")
    # Atelier sait se grouper ; Cuisine l'est déjà ; Véranda est indisponible.
    assert _commande(m, "tout_grouper")[0] == [("media_player.join", "media_player.salon_ma",
                                                {"group_members": ["media_player.atelier_ma"]})]
    assert _commande(m, "tout_separer")[0] == [("media_player.unjoin", ["media_player.cuisine_ma", "media_player.salon_ma"], {})]
    seul = maison_ma(remplace={e: {"group_members": []} for e in MA if e != "media_player.groupe_rdc"})
    assert _commande(seul, "tout_separer")[0] == []


def test_transferer():
    appels, _ = _commande(maison_ma(), "transferer", "2")
    assert appels == [
        ("music_assistant.transfer_queue", "media_player.cuisine_ma", {"source_player": "media_player.tablette_ma"}),
        ("script.turn_on", "script.tab5_lecteur_montrer", {"variables": {"entite": "media_player.cuisine_ma"}}),
    ]
    assert _commande(maison_ma(), "transferer", "5")[0] == []          # vers lui-même
    assert _commande(maison_ma(montre=TELE), "transferer", "2")[0] == []
    assert _commande(petite_maison(), "transferer", "0")[0] == []


@pytest.mark.parametrize("valeur, niveau", [("57", 0.57), ("0", 0.0), ("150", 1.0), ("33.3", 0.33)])
def test_volume(valeur, niveau):
    assert _commande(maison_ma(), "volume", "0", valeur)[0] == [("media_player.volume_set", "media_player.atelier_ma",
                                                                 {"volume_level": niveau})]


def test_volume_muet_marche():
    assert _commande(maison_ma(), "volume", "0", "abc")[0] == []
    assert _commande(maison_ma(), "muet", "0")[0] == [("media_player.volume_mute", "media_player.atelier_ma",
                                                       {"is_volume_muted": True})]
    muet = maison_ma(remplace={"media_player.atelier_ma": {"is_volume_muted": True}})
    assert _commande(muet, "muet", "0")[0][0][2] == {"is_volume_muted": False}
    assert _commande(maison_ma(), "marche", "1")[0] == [("media_player.turn_on", "media_player.bureau_ma", {})]
    allume = maison_ma(remplace={"media_player.bureau_ma": {"state": "playing"}})
    assert _commande(allume, "marche", "1")[0] == [("media_player.turn_off", "media_player.bureau_ma", {})]
    assert _commande(maison_ma(), "marche", "0")[0] == []          # ne sait ni s'allumer ni s'éteindre


def test_ouvrir_pousse_les_pieces():
    assert _commande(maison_ma(), "ouvrir")[0] == [("script.turn_on", "script.tab5_musique_pieces_pousser", {})]


def test_action_inconnue_ne_fait_rien():
    for action in ("chercher", "", "media_player.turn_off", "lecture"):
        assert _commande(maison_ma(), action, "0", "1")[0] == []


# ─── Listes : bibliothèque, détail, sources ───────────────────────────────────

def _envoi(appels):
    """(contexte, elements) de la liste envoyée par script.tab5_musique_liste_envoyer."""
    (envoi,) = [a for a in appels if a[:2] == ("script.turn_on", "script.tab5_musique_liste_envoyer")]
    v = envoi[2]["variables"]
    assert v["appareil"] == "dev_tablette"
    return v["contexte"], v["elements"]


def _radios(n, debut=0):
    return [{"media_type": "radio", "uri": f"library://radio/{debut + i}", "name": f"Radio {debut + i}", "version": "",
             "image": f"http://ha:8095/imageproxy/r{i}?size=0", "favorite": True, "explicit": None} for i in range(n)]


def _bibliotheque(par_genre):
    def repondre(nom, cible, d):
        assert nom == "music_assistant.get_library" and d["config_entry_id"] == ENTREE_MA
        if d["media_type"] not in par_genre:
            raise Echec
        items = par_genre[d["media_type"]][d.get("offset", 0):d.get("offset", 0) + d["limit"]]
        return {"items": items, "limit": d["limit"], "offset": d.get("offset", 0), "order_by": "name",
                "media_type": d["media_type"]}
    return repondre


def test_parcourir_une_categorie_par_pages():
    rep = _bibliotheque({"radio": _radios(25)})
    appels, _ = _commande(maison_ma(), "parcourir", "-1", "rad|1", rep)
    assert appels[0] == ("music_assistant.get_library", None,
                         {"config_entry_id": ENTREE_MA, "media_type": "radio", "limit": 11, "offset": 10})
    contexte, elements = _envoi(appels)
    assert contexte == "rad|Radios|1|1|0"
    lignes = [e.split("|") for e in elements.split(";")]
    assert len(lignes) == 10
    assert lignes[0] == ["Radio 10", "", "d", "library://radio/10", "http://ha:8095/imageproxy/r10?size=160"]
    # Dernière page : pas de suite.
    assert _envoi(_commande(maison_ma(), "parcourir", "-1", "rad|2", rep)[0])[0] == "rad|Radios|2|0|0"


@pytest.mark.parametrize("vue, genre, titre", [
    ("pl", "playlist", "Playlists"), ("alb", "album", "Albums"), ("art", "artist", "Artistes"),
    ("rad", "radio", "Radios"), ("pod", "podcast", "Podcasts"),
])
def test_parcourir_chaque_categorie(vue, genre, titre):
    appels, _ = _commande(maison_ma(), "parcourir", "-1", f"{vue}|0", _bibliotheque({genre: []}))
    assert appels[0][2]["media_type"] == genre
    assert _envoi(appels) == (f"{vue}|{titre}|0|0|0", "")


def test_parcourir_les_favoris_de_tous_les_genres():
    albums = [{"media_type": "album", "uri": "library://album/1", "name": "Album | un", "version": "Live",
               "artists": [{"name": "A; B"}, {"name": "C"}],
               "image": "https://lh3.googleusercontent.com/x=w544-h544-l90-rj"}]
    rep = _bibliotheque({"playlist": [], "album": albums, "artist": [], "track": [], "radio": _radios(12),
                         "podcast": [], "audiobook": []})
    appels, _ = _commande(maison_ma(), "parcourir", "-1", "fav|0", rep)
    demandes = [a[2] for a in appels if a[0] == "music_assistant.get_library"]
    assert [d["media_type"] for d in demandes] == ["playlist", "album", "artist", "track", "radio", "podcast", "audiobook"]
    assert all(d["favorite"] is True and d["limit"] == 11 for d in demandes)
    contexte, elements = _envoi(appels)
    assert contexte == "fav|Favoris|0|1|0"
    lignes = [e.split("|") for e in elements.split(";")]
    assert lignes[0] == ["Album / un (Live)", "A, B, C", "a", "library://album/1",
                         "https://lh3.googleusercontent.com/x=w160-h160-l90-rj"]
    assert len(lignes) == 10 and lignes[-1][0] == "Radio 8"
    # Page 1 : les radios suivantes, sans suite ; chaque genre lu jusqu'à 21.
    appels, _ = _commande(maison_ma(), "parcourir", "-1", "fav|1", rep)
    assert {a[2]["limit"] for a in appels if a[0] == "music_assistant.get_library"} == {21}
    contexte, elements = _envoi(appels)
    assert contexte == "fav|Favoris|1|0|0" and [e.split("|")[0] for e in elements.split(";")] == ["Radio 9", "Radio 10", "Radio 11"]


def test_parcourir_un_genre_en_panne_n_arrete_pas_les_favoris():
    rep = _bibliotheque({"radio": _radios(2)})          # tous les autres genres échouent
    contexte, elements = _envoi(_commande(maison_ma(), "parcourir", "-1", "fav|0", rep)[0])
    assert contexte == "fav|Favoris|0|0|0" and elements.count("library://radio/") == 2


def test_parcourir_music_assistant_muet_ou_absent():
    def panne(*_):
        raise Echec
    assert _envoi(_commande(maison_ma(), "parcourir", "-1", "rad|0", panne)[0]) == (
        "err|Music Assistant ne répond pas.|0|0|0", "")
    assert _envoi(_commande(maison_ma(), "parcourir", "-1", "fav|0", panne)[0]) == (
        "err|Music Assistant ne répond pas.|0|0|0", "")
    appels, _ = _commande(petite_maison(), "parcourir", "-1", "rad|0")
    assert [a[0] for a in appels] == ["script.turn_on"]       # aucune action music_assistant
    assert _envoi(appels) == ("err|Music Assistant n'est pas installé.|0|0|0", "")
    assert _commande(maison_ma(), "parcourir", "-1", "tout|0")[0] == []


def test_parcourir_les_elements_injouables_ne_partent_pas():
    items = _radios(3) + [{"media_type": "track", "uri": "filesystem_local://track/Mon Disque/a.flac", "name": "Espace"},
                          {"media_type": "track", "uri": "spotify://track/abc;x", "name": "Point-virgule"}]
    contexte, elements = _envoi(_commande(maison_ma(), "parcourir", "-1", "rad|0", _bibliotheque({"radio": items}))[0])
    assert [e.split("|")[3] for e in elements.split(";")] == ["library://radio/0", "library://radio/1", "library://radio/2"]


def test_recents():
    m = maison_ma()
    recents = "\n".join(f"Radio {i}||d|library://radio/{i}|" for i in range(15))
    m.etats.d["sensor.tab5_musique_recente"] = Etat("sensor.tab5_musique_recente", "Radio 0", elements=recents)
    appels, _ = _commande(m, "parcourir", "-1", "rec|1")
    assert [a[0] for a in appels] == ["script.turn_on"]
    contexte, elements = _envoi(appels)
    assert contexte == "rec|Récents|1|0|0" and elements.split(";")[0] == "Radio 10||d|library://radio/10|"
    assert _envoi(_commande(maison_ma(), "parcourir", "-1", "rec|0")[0]) == ("rec|Récents|0|0|0", "")


class BrowseMedia:
    """Ce que browse_media met dans la variable d'un script : un objet à attributs, pas un
    dictionnaire (vérifié sur le HA de l'auteur le 18/10/2026 : `is mapping` faux,
    `| string` = « <BrowseMedia …> »)."""

    def __init__(self, **attributs):
        self.__dict__.update(attributs)

    def __str__(self):
        return f"<BrowseMedia {self.title} ({self.media_class})>"


def _navigateur(titre, enfants):
    def repondre(nom, cible, d):
        assert nom == "media_player.browse_media"
        return {cible: BrowseMedia(title=titre, media_class="album", media_content_id=d["media_content_id"],
                                   thumbnail=None, children=[BrowseMedia(**e) for e in enfants])}
    return repondre


def test_detail_d_un_album():
    enfants = [{"title": f"Queen - Titre {i}", "media_class": "track", "media_content_type": "music",
                "media_content_id": f"library://track/{i}", "can_play": True, "can_expand": False,
                "thumbnail": "http://ha:8095/imageproxy/t?size=256"} for i in range(13)]
    appels, _ = _commande(maison_ma(), "detail", "-1", "library://album/7|1", _navigateur("News; of the world", enfants))
    assert appels[0] == ("media_player.browse_media", "media_player.tablette_ma",
                         {"media_content_id": "library://album/7", "media_content_type": "album"})
    contexte, elements = _envoi(appels)
    assert contexte == "det|News, of the world|1|0|1"
    assert elements.split(";") == [f"Titre {i}|Queen|t|library://track/{i}|http://ha:8095/imageproxy/t?size=160"
                                   for i in (10, 11, 12)]


def test_detail_refuse_et_lecteur_de_parcours():
    # Pas un contenant, ref mal formée : rien.
    for valeur in ("library://radio/2|0", "library://album/7;x|0", "media_player.salon_ma|0", "|0"):
        assert _commande(maison_ma(), "detail", "-1", valeur)[0] == []
    # Montré hors de Music Assistant : le premier lecteur Music Assistant disponible parcourt.
    appels, _ = _commande(maison_ma(montre=TELE), "detail", "-1", "ytmusic://playlist/PL1|0", _navigateur("P", []))
    assert appels[0][1] == "media_player.atelier_ma" and appels[0][2]["media_content_type"] == "playlist"
    # Sans Music Assistant : l'erreur, sans browse_media.
    appels, _ = _commande(petite_maison(), "detail", "-1", "library://album/7|0")
    assert _envoi(appels)[0] == "err|Music Assistant n'est pas installé.|0|0|0"
    def panne(*_):
        raise Echec
    assert _envoi(_commande(maison_ma(), "detail", "-1", "library://artist/1|0", panne)[0])[0] == \
        "err|Music Assistant ne répond pas.|0|0|0"


def test_sources_et_source():
    appels, _ = _commande(petite_maison(), "sources", "0", "0")
    assert _envoi(appels) == ("src|Salon / TV, Samsung|0|0|1", "HDMI 1||s|0|;Net/flix||s|1|")
    assert _envoi(_commande(maison_ma(), "sources", "0", "0")[0]) == ("src|Atelier|0|0|1", "")
    assert _commande(petite_maison(), "source", "0", "1")[0] == [("media_player.select_source", TELE, {"source": "Net|flix"})]
    for valeur in ("2", "-1", "a", "1|2", ""):
        assert _commande(petite_maison(), "source", "0", valeur)[0] == []


def test_envoi_de_la_liste_a_la_tablette_qui_demande():
    m = maison_ma()
    s = Script(m)
    s.lancer(_script("tab5_musique_liste_envoyer")["sequence"], appareil="dev_tablette",
             contexte="rad|Radios|0|0|0", elements="FIP||d|library://radio/2|")
    assert s.appels == [(f"esphome.{PREFIXE}_tab5_maj_musique_liste", None,
                         {"contexte": "rad|Radios|0|0|0", "elements": "FIP||d|library://radio/2|"})]


# ─── Images, textes, tailles ──────────────────────────────────────────────────

@pytest.mark.parametrize("url, attendu", [
    ("http://ha:8095/imageproxy/55fa?size=0", "http://ha:8095/imageproxy/55fa?size=160"),
    ("http://ha:8095/imageproxy/69d?size=512&fmt=png", "http://ha:8095/imageproxy/69d?size=160&fmt=png"),
    ("http://ha:8095/imageproxy/69d", "http://ha:8095/imageproxy/69d?size=160"),
    ("https://lh3.googleusercontent.com/abc=w544-h544-l90-rj", "https://lh3.googleusercontent.com/abc=w160-h160-l90-rj"),
    ("https://www.nova.fr/x.png?fit=180%2c180&#038;quality=75", "https://www.nova.fr/x.png?fit=180%2c180&#038%3Bquality=75"),
    ("https://a.b/c d|e.png", "https://a.b/c%20d%7Ce.png"),
    ("https://upload.wikimedia.org/x/1024px-FIP.png", "https://upload.wikimedia.org/x/1024px-FIP.png"),
    ("data:image/png;base64,xx", ""), ("/api/media_player_proxy/x", ""), ("", ""),
    ("https://a.b/" + "x" * 520, ""),
])
def test_images_petites(url, attendu):
    assert maison_ma().macro(f"tab5_musique_image({url!r})") == attendu


def test_le_regex_des_refs_est_celui_de_l_adr():
    adr = lire(ADR)
    assert "^[a-z0-9_-]+://[^|;\\s]+$" in adr
    texte = lire(PACKAGE) + lire(MACROS / "tab5_musique.jinja")
    assert texte.count("[a-z0-9_-]+://") >= 4 and "| length <= 160" in texte


def test_payloads_sous_16_kio_au_pire():
    """Noms et titres au maximum, en caractères de 3 octets, refs de 160, images de 512."""
    lourd = "€" * 80
    etats = []
    ma = []
    for i in range(9):
        e = f"media_player.p{i}"
        ma.append(e)
        etats.append(Etat(e, "playing", friendly_name=f"{i}{lourd}", mass_player_type="player",
                          supported_features=MA_GROUPE | SOURCE | ALLUMER, source_list=["a"], volume_level=1.0,
                          is_volume_muted=True, media_title=lourd, media_artist=lourd,
                          group_members=[f"media_player.p{j}" for j in range(9)]))
    m = Maison(etats + [Etat("sensor.tab5_lecteur_montre", "media_player.p0")], ma=ma)
    appels = _pousser(m, lambda nom, cible, d: {cible: {"items": 99999, "next_item": {"name": lourd * 3}}})
    donnees = appels[-1][2]
    assert len(_champs(donnees["pieces"])) == 8
    assert len(donnees["pieces"].encode()) + len(donnees["contexte"].encode()) < KPAYLOAD_MAX
    items = [{"media_type": "album", "uri": "library://album/" + "9" * 144, "name": lourd, "version": lourd,
              "artists": [{"name": lourd}] * 5, "image": "https://lh3.googleusercontent.com/" + "y" * 470}
             for _ in range(11)]
    assert len(items[0]["uri"]) == 160
    contexte, elements = _envoi(_commande(m, "parcourir", "-1", "alb|0", _bibliotheque({"album": items}))[0])
    assert len(elements.split(";")) == 10 and all(len(e.split("|")[4]) > 400 for e in elements.split(";"))
    assert len(elements.encode()) + len(contexte.encode()) < KPAYLOAD_MAX


# ─── Récents ──────────────────────────────────────────────────────────────────

def test_memoire_des_recents():
    m = maison_ma()
    capteur = _package()["template"][1]
    assert capteur["triggers"] == [{"trigger": "state", "entity_id": "sensor.tab5_musique_signature"}]
    attr = capteur["sensor"][0]["attributes"]["elements"]
    avant = "\n".join(f"Vieux {i}||t|library://track/{i}|" for i in range(25)) + "\nFIP ancien||d|library://radio/2|"
    lignes = m.rendre(attr, {"this": Etat("sensor.tab5_musique_recente", "x", elements=avant)}).split("\n")
    # Ce qui joue d'abord (Salon et Cuisine jouent la même chose : une fois), puis la mémoire.
    assert lignes[:2] == ["FIP||d|library://radio/2|http://ha:8095/imageproxy/69d2?size=160&fmt=png",
                          "Vieux 0||t|library://track/0|"]
    assert len(lignes) == 20 and "FIP ancien" not in "\n".join(lignes)
    # Un premier passage : `this` sans attribut.
    assert str(m.rendre(attr, {"this": Etat("sensor.tab5_musique_recente", "unknown")})).startswith("FIP||d|")
    assert m.rendre(capteur["sensor"][0]["state"], {"this": Etat("sensor.tab5_musique_recente", "unknown")}) == "FIP"
    # Une radio : son nom (l'album de Music Assistant), pas le morceau du moment.
    radio = maison_ma(remplace={"media_player.tablette_ma": {"media_title": "Un morceau", "media_album_name": "FIP"}})
    assert str(radio.rendre(attr, {"this": Etat("s.x", "x")})).startswith("FIP||d|")
    # Sans Music Assistant : la mémoire reste ce qu'elle était.
    assert petite_maison().rendre(attr, {"this": Etat("s.x", "x", elements="A||d|library://radio/1|")}) == \
        "A||d|library://radio/1|"


# ─── Le package dans son ensemble ─────────────────────────────────────────────

def _actions(noeud):
    if isinstance(noeud, dict):
        if "action" in noeud:
            yield noeud
        for v in noeud.values():
            yield from _actions(v)
    elif isinstance(noeud, list):
        for v in noeud:
            yield from _actions(v)


def test_chaque_action_du_lecteur_peut_echouer_sans_arreter():
    for nom, script in _package()["script"].items():
        for a in _actions(script["sequence"]):
            if a["action"].split(".")[0] in ("media_player", "music_assistant") or a["action"].startswith("esphome."):
                assert a.get("continue_on_error") is True, f"{nom} : {a['action']}"


def test_une_branche_par_action_de_l_adr():
    table = re.search(r"\*\*Event `esphome\.tab5_musique`\*\*.*?\n\n(\|.*?)\n\n", lire(ADR), re.S).group(1)
    actions = set()
    for ligne in table.splitlines()[2:]:
        actions |= set(re.findall(r"`([a-z_]+)`", ligne.split("|")[1]))
    assert actions == {"ouvrir", "parcourir", "detail", "jouer", "montrer", "grouper", "separer", "tout_grouper",
                       "tout_separer", "transferer", "volume", "muet", "marche", "sources", "source"}
    (choix,) = [e["choose"] for e in _script("tab5_musique_commande")["sequence"] if "choose" in e]
    conditions = " ".join(o["conditions"] for o in choix)
    for action in actions:
        assert f"a == '{action}'" in conditions, action


def test_en_tete_et_installation():
    texte = lire(PACKAGE)
    assert texte.startswith("# ====") and "[AI-CONTEXT]" in texte and "@payload_contract" in texte
    assert "tab5_musique.jinja" in texte and "reload_custom_templates" in lire(MACROS / "tab5_musique.jinja")
    # Rien de la maison de l'auteur : Music Assistant trouvé, jamais écrit.
    assert "config_entry_id: \"{{ config }}\"" in texte and not re.search(r"\b01[A-Z0-9]{24}\b", texte)


def test_branche_de_l_evenement():
    auto = next(a for a in yaml.safe_load(lire(EVENEMENTS))["automation"] if a["id"] == "tab5_evenements")
    assert {"trigger": "event", "event_type": "esphome.tab5_musique", "id": "musique"} in auto["triggers"]
    (choix,) = [a["choose"] for a in auto["actions"] if "choose" in a]
    (branche,) = [b for b in choix if b["conditions"] == [{"condition": "trigger", "id": "musique"}]]
    m = maison_ma()
    m.etats.d["script.tab5_musique_commande"] = Etat("script.tab5_musique_commande", "off")
    s = Script(m)
    trigger = {"id": "musique", "event": {"data": {"action": "jouer", "piece": "2", "valeur": "m|library://radio/2",
                                                   "device_id": "dev_tablette"}}}
    s.lancer(branche["sequence"], trigger=trigger)
    assert s.appels == [("script.turn_on", "script.tab5_musique_commande", {"variables": {
        "commande": "jouer", "piece": 2, "valeur": "m|library://radio/2", "appareil": "dev_tablette"}})]
    # Sans le package : rien.
    s = Script(maison_ma())
    s.lancer(branche["sequence"], trigger=trigger)
    assert s.appels == []
