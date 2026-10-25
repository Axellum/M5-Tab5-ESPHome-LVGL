# -*- coding: utf-8 -*-
"""Pièces et tuiles génériques (ADR-0023), côté firmware : Tab5/ecran/tab5_tuiles.cpp lit les
définitions (action tab5_maj_tuiles) et les états (clés tRT de tab5_maj_emplacements), et
envoie les commandes (événement esphome.tab5_action). Aucun compilateur ne compare ces
chaînes au contrat ; ce fichier lit le C++ et le YAML, comme les autres tests statiques :

- types, options, grammaire des clés, pièce de chaque page = tableaux de l'ADR ;
- commandes émises (appui court / long par type, « Tout éteindre ») = tableau de l'ADR ;
  commandes du mode héritage = celles de la 3.x, que le blueprint connaît toujours ;
- noms filtrés aux glyphes des polices (&latin1 de tab5-styles.yaml), 24 octets ;
- états routés avant la table des emplacements 3.x ; action tab5_maj_tuiles décrite et
  son exemple conforme à la grammaire ; définitions en NVS sous la magie « TUI1 » ;
- boutons des tuiles à leur position visuelle T (ordre inversé des horaires), widgets
  posés par tab5-tuiles.yaml sans toucher à l'on_boot ;
- version annoncée (sw_version) ≥ 3.2.0 : le blueprint parle alors le protocole des pièces."""
import os
import re
from pathlib import Path

import pytest
from tests.commun import CARTES, _jetons as jetons_resolus, constantes_cpp, lire as _lire, resoudre, source

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

from check_tab5_code_rules import font_glyphs  # noqa: E402

ADR = os.path.join(REPO, "docs", "decisions", "0023-rooms-generic-tiles.md")
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")


def _cpp():
    # Le code des tuiles : depuis le lot L7 (08/10/2026), tab5_tuiles.cpp, ses popups, sa roue
    # et l'en-tête que les trois partagent.
    return "\n".join(_lire(source(f)) for f in ("tab5_tuiles_priv.h", "tab5_tuiles.cpp", "tab5_tuiles_popups.cpp", "tab5_tuiles_roue.cpp"))


def _fonction(source, nom):
    """Corps d'une fonction C++ (de sa signature à l'accolade fermante en colonne 0)."""
    m = re.search(rf"^[^\n;]*\b{nom}\([^;{{]*\)\s*{{\n(.*?)^}}", source, re.M | re.S)
    assert m, f"fonction {nom} introuvable"
    return m.group(1)


def _constexpr(nom):
    """Valeur d'une constante de tab5_tuiles.cpp (`nom` sans ses crochets)."""
    m = re.search(rf"constexpr [^=;]*?\b{re.escape(nom)}\b\s*(?:\[[^\]]*\])?\s*= ([^;]+);", _cpp())
    assert m, f"constexpr {nom} introuvable"
    return m.group(1).strip()


def _gestes_par_type():
    """Lignes de kGestes (lot L7) : {type: (agit, commande, roue, fenêtre)}, dans l'ordre de kTypes."""
    lignes = re.findall(r'\{(true|false), (nullptr|"\w+"), (true|false), Fenetre::(\w+)\},\s*// (\w+)',
                        _constexpr("kGestes"))
    return {t: (a == "true", c.strip('"') if c != "nullptr" else None, r == "true", f) for a, c, r, f, t in lignes}


def _types_de_l_adr():
    """Types du tableau « Type | HA domains | Tap | Long press » : {type: (tap, long)}."""
    texte = _lire(ADR).split("| Type | HA domains |", 1)[1].split("\n\n", 1)[0]
    types = {}
    for ligne in texte.splitlines()[2:]:
        c = [x.strip() for x in ligne.strip("|").split("|")]
        types[c[0].strip("`")] = (c[2], c[3])
    return types


def _commandes_de_l_adr():
    texte = _lire(ADR).split("| `action` | `valeur` | For |", 1)[1].split("\n\n", 1)[0]
    commandes = set()
    for ligne in texte.splitlines()[2:]:
        premiere = ligne.strip("|").split("|")[0]
        commandes.update(c for c in re.findall(r"`(\w+)`", premiere) if c != "pR")
    return commandes


# ─────────────────────────────────────────────────────────────────────────────
# Contrat : types, options, pièces, champs
# ─────────────────────────────────────────────────────────────────────────────

def test_types_egaux_a_la_grammaire_et_au_tableau_de_l_adr():
    kTypes = re.findall(r'"(\w*)"', _constexpr("kTypes"))
    assert kTypes[0] == "", "l'indice 0 est la tuile vide"
    grammaire = re.search(r"^type\s+:= (.+)$", _lire(ADR), re.M).group(1)
    assert kTypes[1:] == [t.strip() for t in grammaire.split("|")]
    assert set(kTypes[1:]) == set(_types_de_l_adr())
    enum = re.search(r"enum class Type : uint8_t \{([^}]*)\}", _cpp()).group(1)
    assert [e.strip().lower() for e in enum.split(",")] == ["vide"] + kTypes[1:], "enum Type dans l'ordre de kTypes"


def test_options_egales_a_l_adr_bit_par_lettre():
    lettres = re.findall(r"`([a-z])` ", next(l for l in _lire(ADR).splitlines() if l.startswith("Options:")))
    assert _constexpr("kLettresOptions") == '"' + "".join(lettres) + '"'
    bits = dict(re.findall(r"OPT_([A-Z]) = (\d+)", _cpp()))
    assert {k.lower(): int(v) for k, v in bits.items()} == {l: 1 << i for i, l in enumerate(lettres)}


def test_piece_de_chaque_page_selon_l_adr():
    texte = _lire(ADR).split("| Room `R` | Page |", 1)[1].split("\n\n", 1)[0]
    page_de = {}
    for ligne in texte.splitlines()[2:]:
        r, p = (int(x) for x in re.findall(r"^\|\s*(\d)\s*\|\s*(\d)\s*\|", ligne.strip())[0])
        page_de[r] = p
    # La table vit dans le socle depuis l'ADR-0066 (piece_de_page, testée sur PC) ; les pages 5
    # et plus (une par pièce au-delà de la cinquième) : page = pièce.
    core = _lire("Tab5", "socle", "tab5_core.cpp")
    table = re.search(r"constexpr int kPieceDePage\[kPagesMeteo\] = \{([^}]*)\};", core).group(1)
    kPieceDePage = [int(x) for x in re.findall(r"\d", table)]
    assert kPieceDePage == [r for p in range(5) for r, pp in page_de.items() if pp == p]
    assert "return page < kPagesMeteo ? kPieceDePage[page] : page;" in core
    assert "kPieceDePage" not in _cpp(), "une seule table, celle du socle"
    # La tablette et le blueprint dans le même ordre que le rendu (tools/rendu/ecrans.py).
    assert "PAGE_DE_LA_PIECE" in _lire("tools", "rendu", "ecrans.py")


def test_champs_gardes_selon_l_adr():
    # kNom et kIcone : communs avec la tuile - / + (Tab5/socle/tab5_modele_ha.h, lot L5).
    modele = Path(REPO, "Tab5", "socle", "tab5_modele_ha.h").read_text(encoding="utf-8")
    assert '#include "tab5_modele_ha.h"' in _cpp() and "using namespace modele_ha;" in _cpp()
    assert re.search(r"constexpr size_t kNom = 25;", modele), "nom : 24 octets au plus, zéro final compris"
    assert re.search(r"constexpr size_t kIcone = 16;", modele), "code de palette : [a-z0-9_]{1,15}"
    assert int(_constexpr("kComplement")) >= 8, "unité d'un cap : 7 octets au plus"
    # Coupé sur une frontière de caractère : on n'ajoute un caractère que s'il tient entier.
    assert "if (k + w >= cap) break;" in _fonction(_cpp(), "copier_texte")


def test_noms_filtres_aux_glyphes_des_polices():
    """Le filtre des noms (glyphe_disponible) = les glyphes &latin1 de roboto_32_b, la
    police des onglets : un caractère gardé s'affiche, aucun affichable n'est jeté."""
    table = [int(x, 16) for x in re.findall(r"0x([0-9A-F]{4})", _constexpr("kHorsLatin1"))]
    garde = set(range(0x20, 0x7F)) | (set(range(0xA1, 0x100)) - {0xAD}) | set(table)
    corps = _fonction(_cpp(), "glyphe_disponible")
    assert "cp >= 0x20 && cp <= 0x7E" in corps and "cp >= 0xA1 && cp <= 0xFF" in corps and "0xAD" in corps
    polices = font_glyphs(Path(REPO, "Tab5", "paquets", "tab5-styles.yaml"))
    assert {ord(c) for c in polices["roboto_32_b"]} == garde


# ─────────────────────────────────────────────────────────────────────────────
# Nombre de pièces (ADR-0066, lot A)
# ─────────────────────────────────────────────────────────────────────────────

# Dernière version publiée avant l'ADR-0066 : son parseur doit ignorer les clés nouvelles.
TAG_AVANT_PN = "v3.9.0-rc.5"


def _cle_lue_par_3_9(cle):
    """Miroir de lire_entree de la 3.9 (tab5_tuiles.cpp à TAG_AVANT_PN, relu par le test
    ci-dessous) : « pR » et « tRT » avec des chiffres de 0 à 4 (chiffre_0_4), « h… » et
    « n… » pour les zones à lignes ; tout le reste est ignoré."""
    if len(cle) == 2 and cle[0] == "p" and "0" <= cle[1] <= "4":
        return "piece"
    if cle[0] in "hn" and len(cle) in (2, 3):
        return "zone"
    if len(cle) == 3 and cle[0] == "t" and "0" <= cle[1] <= "4" and "0" <= cle[2] <= "4":
        return "tuile"
    return None


def test_le_firmware_3_9_ignore_les_cles_des_dix_pieces():
    """ADR-0066, compatibilité : un blueprint récent sur une tablette en 3.9 garde l'écran
    d'avant. « pn|N », les pièces 5 à 9 (« p5 », « t5x ») et les tuiles de rang 5 et plus
    (« t0a », lot B) ne sont lus ni comme une pièce, ni comme une tuile, ni comme une zone."""
    import contrat_api as ca
    if TAG_AVANT_PN not in ca.tags_publies():
        if os.environ.get("GITHUB_ACTIONS"):
            pytest.fail(f"tag {TAG_AVANT_PN} absent du clone de la CI")
        pytest.skip(f"tag {TAG_AVANT_PN} absent de ce clone (`git fetch --tags`)")
    ancien = ca.arbre(TAG_AVANT_PN).lire("Tab5/ecran/tab5_tuiles.cpp").replace("\r\n", "\n")
    # Le miroir est fidèle : les conditions de la 3.9, telles qu'elles sont écrites.
    assert "if (c < '0' || c > '4') return false;" in _fonction(ancien, "chiffre_0_4")
    lire = _fonction(ancien, "lire_entree")
    assert "if (f[0].n == 2 && f[0].p[0] == 'p' && chiffre_0_4(f[0].p[1], r)) {" in lire
    assert "if (z >= 0 && (f[0].n == 2 || f[0].n == 3)) {" in lire
    assert ("if (f[0].n != 3 || f[0].p[0] != 't' || !chiffre_0_4(f[0].p[1], r) || !chiffre_0_4(f[0].p[2], t)) return;"
            in lire)
    assert "constexpr char kLettreZone[RANGEE_NB] = {'h', 'n'};" in ancien
    nouvelles = ["pn"] + [f"p{r}" for r in range(5, 10)] + [f"t{r}{t}" for r in range(5, 10) for t in "0123456789abcdefghij"]
    nouvelles += [f"t{r}{t}" for r in range(5) for t in "56789abcdefghij"]
    assert all(_cle_lue_par_3_9(c) is None for c in nouvelles)
    # Les clés des cinq pièces d'avant restent lues : la 3.9 garde ses cinq pièces.
    assert all(_cle_lue_par_3_9(f"p{r}") == "piece" for r in range(5))
    assert all(_cle_lue_par_3_9(f"t{r}{t}") == "tuile" for r in range(5) for t in range(5))


def test_nombre_de_pieces_lu_et_borne():
    """« pn|N » : lu avant les pièces (sans lui, cinq), borné à 1 .. 10 (pieces_nombre_lire,
    tab5_core.cpp, testée sur PC) ; une pièce R ≥ N n'existe nulle part (tuile_presente) et
    ses définitions sont effacées de l'instantané."""
    lire = _fonction(_cpp(), "lire_entree")
    assert "m.pieces_n = static_cast<uint8_t>(pieces_nombre_lire(f[1].p, f[1].n));" in lire
    assert lire.index("f[0].p[1] == 'n'") < lire.index("piece_index(f[0].p[1], r)"), "pn avant le chiffre de pièce"
    assert "neuf->pieces_n = kPiecesDefaut;" in _fonction(_cpp(), "tuiles_definir")
    assert "for (int r = neuf->pieces_n; r < kPiecesMax; r++) {" in _fonction(_cpp(), "tuiles_definir")
    assert "r >= pieces_nombre()" in _fonction(_cpp(), "tuile_presente")
    assert "if (heritage() || s_m.pieces_n < 1) return kPiecesDefaut;" in _fonction(_cpp(), "pieces_nombre")
    regle = _fonction(_lire("Tab5", "socle", "tab5_core.cpp"), "pieces_nombre_lire")
    assert "return v < 1 ? 1 : (v > kPiecesMax ? kPiecesMax : v);" in regle
    # Plus de ligne « Pièce n/N » au-dessus du nom de la pièce (18/10/2026, demande d'Axel) :
    # ni le texte, ni l'étiquette qui le portait, ni ses jetons de position.
    titre = _fonction(_cpp(), "tuiles_titre_piece")
    assert "Pièce %d/%d" not in titre and 'tr("Pièce %d"), r + 1);' in titre
    for chemin in (("Tab5", "ecran", "tab5_central.cpp"), ("Tab5", "ecran", "tab5_central.h"),
                   ("Tab5", "paquets", "tab5-lvgl.yaml"), ("Tab5", "paquets", "tab5-tuiles.yaml"),
                   ("Tab5", "paquets", "tab5-ui-tokens.yaml"), ("Tab5", "socle", "tab5_geometrie.h"),
                   ("tab5-ha-hmi.yaml",), ("tab5-rendu-host.yaml",)):
        texte = _lire(*chemin)
        for mort in ("page_title_sub", "lbl_page_title_sub", "acc_titre_sous_y", "acc_titre_y2", "kAccTitreY2"):
            assert mort not in texte, (chemin, mort)
    for code in ("en", "de", "nl", "es", "it", "tr"):
        assert '"Pièce %d/%d"' not in _lire("Tab5", "lang", f"{code}.yaml"), code


def test_pages_de_piece_de_la_meteo():
    """ADR-0066 : en mode météo, les pièces 5 à N − 1 ont chacune une page après les jours
    10-14, dessinée comme en mode HA (peindre_cartes), avec le titre de la pièce ; aucune
    quand « Appareils sur la météo » est éteint ; le retour à l'accueil après 25 s les quitte
    (retour_auto_tick ne regarde que la page 2)."""
    assert "return std::max(kPagesMeteo, pieces_nombre());" in _fonction(_cpp(), "pages_meteo")
    assert "if (!s_appareils_meteo || heritage()) return kPagesMeteo;" in _fonction(_cpp(), "pages_meteo")
    assert "if (!cartes_affichees()) return;" in _fonction(_cpp(), "peindre_cartes")
    assert "if (cartes_affichees()) return;" in _fonction(_cpp(), "peindre_meteo")
    central = _lire("Tab5", "ecran", "tab5_central.cpp")
    appliquer = _fonction(central, "apply_forecast_page")
    assert "g_tuiles_ui.calque_ha" in appliquer and "tuiles_peindre_meteo();" in appliquer
    assert "pagination_afficher" not in appliquer, "les pastilles : celles des tuiles (pastilles_peindre)"
    titre = _fonction(central, "set_forecast_page_title_text")
    assert "const bool piece = ctx.ha_mode || forecast_page >= kPagesMeteo;" in titre
    assert "if (g_central_ctx.forecast_page == 2) return RetourAuto::RIEN;" in _lire("Tab5", "ecran", "tab5_anim.cpp")
    # Ce qui suit les cartes et non le mode HA (relecture du lot A) : sortie du mode HA
    # depuis le calque des cartes, ancre de la roue d'une tuile, pièce montrée (Maison,
    # roue de navigation), mention « Prévisions de … » et titre après une réponse vocale.
    assert "lv_obj_t* const avant = ctx.ha_mode ? u.calque_ha : calque_de(ctx.forecast_page);" in \
        _fonction(_cpp(), "tuiles_mode_ha")
    roue = _lire("Tab5", "ecran", "tab5_tuiles_roue.cpp")
    assert "const bool cartes = cartes_affichees();" in roue
    assert "return tuile_roue_ouvrir(r, t, entiere ? g_tuiles_ui.carte_pastille[p] : case_ancre(p, d.case_de[t]));" in roue
    assert "if (!cartes_affichees() || heritage()) return -1;" in _fonction(_cpp(), "tuiles_piece_montree")
    assert "tuiles_piece_mode_ha()" not in _lire("Tab5", "ecran", "tab5_maison.cpp")
    assert "tuiles_piece_mode_ha()" not in _lire("Tab5", "ecran", "tab5_roue_navigation.cpp")
    assert "g_central_ctx.forecast_page < kPagesMeteo &&" in _lire("Tab5", "ecran", "tab5_forecast.cpp")
    assert "if (ctx.ha_mode || ctx.forecast_page >= kPagesMeteo) {" in central
    # Dix pastilles, posées une fois ; celles de trop masquées.
    tuiles = _lire("Tab5", "paquets", "tab5-tuiles.yaml")
    lvgl = _lire("Tab5", "paquets", "tab5-lvgl.yaml")
    for i in range(10):
        assert f"u.pastilles[{i}] = id(pbar_{i});" in tuiles and f"id: pbar_{i}," in lvgl
    assert "lv_obj_t* pbars[]" not in lvgl and "lv_obj_t* pbars[]" not in _lire("Tab5", "paquets", "tab5-scripts.yaml")


def test_magie_nvs_des_definitions():
    # « TU » + la taille du modèle (ADR-0066, lots A et B : la même ligne) : une mise en page
    # nouvelle change la magie d'elle-même (« TUI1 » jusqu'à la 3.9, cinq pièces de cinq
    # tuiles). Dix pièces de vingt éléments : 8 + 10 × 25 + 200 × 59 octets.
    assert _constexpr("kMagic") == "0x54550000u | static_cast<uint32_t>(sizeof(Modele))"
    assert 'static_assert(sizeof(Modele) < 0x10000, "la taille du modèle tient dans la magie");' in _cpp()
    modele = re.search(r"struct Modele \{(.*?)\};", _cpp(), re.S).group(1)
    assert "char pieces[kPiecesMax][modele_ha::kNom];" in modele and "Def tuiles[kPiecesMax][kTuilesMax];" in modele
    assert "uint8_t pieces_n;" in modele and "uint8_t reserve[2];" in modele, "pn pris sur la réserve"
    # Le bourrage final écrit (memcmp de tuiles_definir) : 8 + 250 + 11 800 = 12 058, plus 2.
    assert "uint8_t fin[4 - kModeleUtile % 4];" in modele
    assert 'static_assert(sizeof(Modele) == kModeleUtile + sizeof(Modele::fin), "Modele sans bourrage implicite");' \
        in _cpp()
    # Relu dans le tas (le modèle fait ~12 Ko, la pile de la boucle 8 Ko).
    charger = _fonction(_cpp(), "charger")
    assert "std::unique_ptr<Modele> m(new Modele());" in charger and "Modele m{};" not in charger
    definir = _fonction(_cpp(), "tuiles_definir")
    # Écrites seulement si elles changent, comparées octet par octet.
    assert definir.index("memcmp") < definir.index("s_pref.save")
    # Chargées là où les zones le sont (zones_apply_ui → tuiles_appliquer_ui → charger).
    assert "tuiles_appliquer_ui();" in _fonction(_lire("Tab5", "ecran", "tab5_zones.cpp"), "zones_apply_ui")


def test_mode_heritage_borne_a_ses_cinq_emplacements():
    # Le mode héritage (aucune définition reçue : premier démarrage après la mise à jour, blueprint
    # 3.x) n'a que les cinq emplacements de la 3.x ; zone_tuile_absente() ne connaît pas les
    # éléments 5 à 19 : sans la borne, ils seraient présents et liraient s_h.lum[t - 2] et
    # kHeritageLumieres[t - 2] hors de leurs tableaux (relecture du lot B, ADR-0066).
    assert _constexpr("kTuilesHeritage") == "5"
    assert "bool lum[3] = {};" in _cpp() and "kHeritageLumieres[3]" in _cpp()
    presente = _fonction(_cpp(), "tuile_presente")
    assert "if (heritage()) return r == 0 && t < kTuilesHeritage && !zone_tuile_absente(t);" in presente


# ─────────────────────────────────────────────────────────────────────────────
# Commandes émises
# ─────────────────────────────────────────────────────────────────────────────

def test_commandes_par_type_egales_au_tableau_de_l_adr():
    cpp = _cpp()
    adr = _commandes_de_l_adr()
    types = _types_de_l_adr()
    appui = _fonction(cpp, "tuile_appui_piece")
    gestes = _fonction(cpp, "gestes")
    table = _gestes_par_type()
    # Une ligne par type de kTypes, dans le même ordre (lot L7 : une seule table).
    assert list(table) == ["vide"] + re.findall(r'"(\w+)"', _constexpr("kTypes")), list(table)
    # Toute commande de tuile émise est dans le tableau « What the tablet sends ».
    emises = {c for _, c, _, _ in table.values() if c} | set(re.findall(r'\? "(\w+)" :', gestes))
    emises |= set(re.findall(r'return "(\w+)"', _fonction(cpp, "vol_appui")))
    emises |= set(re.findall(r'"(ouvrir|fermer|arreter)"', _fonction(cpp, "vol_appui_long")))
    assert emises and emises <= adr, emises - adr
    # lum / int / med : basculer, allumer avec l'option o ; act : lancer.
    for t in ("lum", "int", "med"):
        assert "`basculer`" in types[t][0] and table[t][1] == "basculer", t
    assert '(d.options & OPT_O)) ? "allumer" : l.commande;' in gestes
    assert "`lancer`" in types["act"][0] and table["act"][1] == "lancer"
    # La commande de la table part ; sans commande, la fenêtre (clim, capteur) ; un volet :
    # son sens.
    assert ("const char* action = type == Type::VOL ? (long_appui ? vol_appui_long(e) : vol_appui(e)) : "
            "g.commande;") in appui
    assert appui.index("if (action == nullptr) {") < appui.index("envoyer_tuile(r, t, action);")
    # vol : en mouvement arrêter (pause), sinon le sens choisi par le titre (au départ,
    # ouvert → fermer) ; long : l'autre (mise à jour du 28/09, retour de la 3.1).
    assert all(f"`{c}`" in types["vol"][0] for c in ("arreter", "fermer", "ouvrir"))
    vol = _fonction(cpp, "vol_appui")
    assert 'if (vol_mouvement(e.brut)) return "arreter";' in vol
    assert 'return vol_sens(e) == SENS_FERMER ? "fermer" : "ouvrir";' in vol
    assert 'return vol_sens(e) == SENS_FERMER ? "ouvrir" : "fermer";' in _fonction(cpp, "vol_appui_long")
    assert 'return est(e.brut, "open") ? SENS_FERMER : SENS_OUVRIR;' in _fonction(cpp, "vol_sens")
    # Le titre de chaque tuile (jours, heures, cartes HA) bascule le sens.
    titres = _fonction(cpp, "tuiles_brancher_titres")
    assert "u.jour_titre[t], u.heure_titre[t], u.carte_nom[t]" in titres and "LV_EVENT_SHORT_CLICKED" in titres
    # cap / bin : lecture seule (aucun bouton) ; cli : popup avec m, ou sur sa propre clim
    # quand la tablette en a les réglages (ADR-0027) ; med : télécommande avec t.
    assert "(type == Type::CLI && ((d.options & OPT_M) || clim_connue))" in gestes
    assert "(type == Type::CAP && (d.options & OPT_E))" in gestes
    assert not table["cap"][0] and not table["bin"][0] and not table["cli"][0] and not table["vide"][0]
    assert "if (d.type >= kNbTypes || (d.options & OPT_R)) return g;" in gestes
    assert "if (type == Type::MED && (d.options & OPT_T)) g.fenetre = Fenetre::TELECOMMANDE;" in gestes
    assert "case Fenetre::TELECOMMANDE:\n            telecommande_ouvrir(0);" in _fonction(cpp, "ouvrir_fenetre")
    # Option k : un second appui dans les 3 s.
    assert "kConfirmationMs = 3000" in cpp and "OPT_K" in appui


def test_tout_eteindre_de_la_piece():
    """« Tout éteindre » du popup lumière et « Éteindre les lumières » du popup Maison
    (ADR-0037) : la même commande pR / eteindre, par la même fonction."""
    assert "pR` + `eteindre`" in _lire(ADR)
    cpp = _cpp()
    corps = _fonction(cpp, "tuiles_piece_eteindre")
    assert 'envoyer(piece_cle(r).s, "eteindre");' in corps
    # Clés « pR » et « tRT » : une seule écriture, tab5_modele_ha.h (lot L7).
    modele = _lire("Tab5", "socle", "tab5_modele_ha.h")
    assert "c.s[0] = 'p';" in _fonction(modele, "piece_cle") and "c.s[1] = static_cast<char>('0' + r);" in _fonction(
        modele, "piece_cle")
    assert "void popup_lumiere_tout_eteindre() { tuiles_piece_eteindre(s_pl.piece); }" in cpp
    assert "popup_lumiere_tout_eteindre();" in _lire("Tab5", "ui_components", "light_popup.yaml")


def test_popup_lumiere_revalide_aux_nouvelles_definitions():
    """UI-1 (audit du 07/10/2026) : comme les popups volet et appareil, le popup lumière
    est revalidé quand HA renvoie les définitions — pages et lignes recalculées sur la même
    pièce (ADR-0046 : la première qui a des lumières si elle n'en a plus) et repeintes,
    refermé s'il n'y a plus de lumière dans la maison, oublié s'il est fermé."""
    cpp = _cpp()
    # tuiles_definir → popups_revalider (tab5_tuiles_popups.cpp, lot L7) : volet, appareil, lumière.
    assert "popups_revalider();" in _fonction(cpp, "tuiles_definir")
    assert "popup_lumiere_revalider();" in _fonction(cpp, "popups_revalider")
    assert "popup_volet_revalider();" in _fonction(cpp, "popups_revalider")
    corps = _fonction(cpp, "popup_lumiere_revalider")
    assert "s_pl = PopupLumiere{};" in corps
    # Mêmes lignes qu'à l'ouverture (popup_lumiere_lignes) ; refermé sans lumière, repeint
    # sinon (popup_tuile_revalider, commun aux popups, lot L7).
    assert ("popup_tuile_revalider(s_pl, popup_lumiere_lignes(s_pl.piece, choisie), popup_lumiere_peindre);"
            in corps)
    assert "popup_lumiere_lignes(r, t)" in _fonction(cpp, "popup_lumiere_ouvrir")
    lignes = _fonction(cpp, "popup_lumiere_lignes")
    assert "pieces_calculer(s_pl, est_lumiere, r, t)" in lignes and "s_pl = PopupLumiere{};" in lignes
    # La clé des commandes suit la lampe choisie (mode héritage → clés tRT).
    assert "lumiere_cle(s_pl.piece, s_pl.tuile, *u.lum_cle);" in lignes
    calcul = _fonction(cpp, "pieces_calculer")
    assert "p.tuile = p.tuiles[p.choix];" in calcul and "p.piece = p.pieces[p.page];" in calcul
    revalider = _fonction(cpp, "popup_tuile_revalider")
    assert "if (!P::ouvert()) return;" in revalider
    assert "if (valide) peindre();\n    else animate_popup_close(P::conteneur());" in revalider
    volet = _fonction(cpp, "popup_volet_revalider")
    assert "pieces_calculer(s_pv, est_volet, r, t)" in volet and "animate_popup_close(PopupVolet::conteneur());" in volet


def test_les_trois_popups_d_une_tuile_partagent_popup_tuile():
    """UI-4 (audit du 07/10/2026, lot L7) : pièce, tuile, « ouvert ? », ouverture et
    revalidation des popups lumière, volet et appareil écrits une fois (PopupTuile) ; les
    pages par pièce des popups Lumières et Volets une fois aussi (PopupPieces, ADR-0046)."""
    cpp = _cpp()
    assert "struct PopupPieces : PopupTuile<C>" in cpp
    for struct, base in (("PopupLumiere", "PopupPieces<&TuilesUI::lum_popup>"),
                         ("PopupVolet", "PopupPieces<&TuilesUI::vol_popup>"),
                         ("PopupAppareil", "PopupTuile<&TuilesUI::app_popup>")):
        assert f"struct {struct} : {base}" in cpp, struct
    # Plus de « ouvert ? » ni de bornes écrits à la main, popup par popup.
    for ancien in ("bool popup_ouvert()", "bool popup_volet_ouvert()", "bool popup_appareil_ouvert()",
                   "r >= kPieces || t < 0 || t >= kTuiles || heritage()"):
        assert ancien not in cpp, ancien
    assert cpp.count("!lv_obj_has_flag(p, LV_OBJ_FLAG_HIDDEN)") == 1
    assert "pieces_calculer(s_pv, est_volet, r, t)" in _fonction(cpp, "popup_volet_montrer")
    assert ("void popup_appareil_ouvrir(int r, int t) { popup_tuile_ouvrir(s_pa, r, t, popup_appareil_peindre); }"
            in cpp)
    ouvrir = _fonction(cpp, "popup_tuile_ouvrir")
    assert ouvrir.index("p = P{};") < ouvrir.index("peindre();") < ouvrir.index("animate_popup_open(P::conteneur());")
    # Un état : la ligne de la tuile (si la page la montre), puis la partie droite.
    etat = _fonction(cpp, "popup_volet_etat")
    assert "ligne_de_la_tuile(s_pv, g_tuiles_ui.vol_ligne, t);" in etat
    assert "if (s_pv.montre(r, t)) popup_volet_peindre();" in etat
    assert "ligne_de_la_tuile(s_pl, g_tuiles_ui.lum_ligne, t);" in _fonction(cpp, "popup_lumiere_etat")
    assert "if (s_pa.montre(r, t)) popup_appareil_peindre();" in _fonction(cpp, "popup_appareil_etat")


def _lum_pct(v):
    """Miroir de lum_pct (Tab5/socle/tab5_core.cpp) : 0-255 → %, arrondi, borné à 1..100."""
    if v != v or v in (float("inf"), float("-inf")):
        return -1
    b = min(max(v, 0.0), 255.0)
    x = b * 100.0 / 255.0
    return min(max(int(x + 0.5), 1), 100)  # std::lround : demi vers le haut (x ≥ 0)


def test_luminosite_en_pourcent_une_seule_formule():
    """CPP-3 (audit du 07/10/2026) : carte, popup lumière (arc compris) et roue donnent le
    même % pour la même lampe — une seule fonction, lum_pct, arrondie ; une lampe allumée
    n'affiche jamais 0 %. Avant : 127 → 50 % sur la carte, 49 % dans le popup."""
    core = _lire("Tab5", "socle", "tab5_core.cpp")
    corps = _fonction(core, "lum_pct")
    assert "std::lround(b * 100.0f / 255.0f)" in corps and "pct < 1 ? 1" in corps and "std::isfinite(v)" in corps
    assert "int lum_pct(float v);" in _lire("Tab5", "socle", "tab5_core.h")
    cpp = _cpp()
    assert "100.0f / 255.0f" not in cpp and "* 100 / 255" not in cpp, "une autre formule que lum_pct"
    assert cpp.count("lum_pct(") >= 4  # carte, popup, choix et jauge de la roue
    popup = _lire("Tab5", "ui_components", "light_popup.yaml")
    assert "255.0f" not in popup and "x > 0 ? lum_pct(x) : 0" in popup
    assert [_lum_pct(v) for v in (0.0, 1.0, 127.0, 128.0, 254.0, 255.0, 300.0, -5.0)] == [1, 1, 50, 50, 100, 100, 100, 1]
    assert _lum_pct(float("nan")) == -1


def test_cles_des_commandes_de_tuile():
    cpp = _cpp()
    assert "envoyer(tuile_cle(r, t).s, action);" in _fonction(cpp, "envoyer_tuile")
    cle = _fonction(_lire("Tab5", "socle", "tab5_modele_ha.h"), "tuile_cle")
    assert ("c.s[0] = 't';\n    c.s[1] = static_cast<char>('0' + r);\n    c.s[2] = tuile_car(t);"
            in cle.replace("\r\n", "\n"))
    # T = 0 … 19 (ADR-0066) : « 0 » … « 9 » puis « a » … « j », lu par tuile_index partout où
    # une clé de tuile arrive (états, définitions, clims, lien de la tuile − / +).
    modele = _lire("Tab5", "socle", "tab5_modele_ha.h")
    assert 'constexpr char kCarsTuile[] = "0123456789abcdefghij";' in modele
    assert "!tuile_index(cle[2], t) || t >= kTuilesMax" in _fonction(cpp, "tuiles_etat_recu")
    assert "!tuile_index(f[0].p[2], t)" in _fonction(cpp, "lire_entree")
    assert "modele_ha::tuile_index(cle[3], t) || t >= kTuilesMax" in _lire(source("tab5_clim.cpp"))
    assert "tuile_index(f[3].p[2], lien_t) && lien_t < kTuilesMax" in _lire(source("tab5_reglables.cpp"))
    # Écrite une seule fois : plus de clé « tRT » montée à la main dans le C++ des tuiles et des clims.
    for fichier in ("tab5_tuiles.cpp", "tab5_tuiles_popups.cpp", "tab5_tuiles_roue.cpp", "tab5_clim.cpp"):
        assert "static_cast<char>('0' + " not in _lire(source(fichier)), fichier
    # L'événement esphome.tab5_action du script tab5_action, emplacement / action / valeur.
    tuiles_yaml = _lire("Tab5", "paquets", "tab5-tuiles.yaml")
    assert "id(tab5_action).execute(std::string(e), std::string(a), std::string(v));" in tuiles_yaml
    scripts = _lire("Tab5", "paquets", "tab5-scripts.yaml").split("- id: tab5_action", 1)[1].split("- id:", 1)[0]
    assert re.findall(r"^\s+(\w+): string$", scripts, re.M) == ["emplacement", "commande", "valeur"]


def test_commandes_du_mode_heritage_connues_du_blueprint():
    """Tant qu'aucune définition n'est arrivée, les tuiles envoient les commandes 3.x :
    le blueprint (3.x ou 3.2) doit toujours avoir leur branche."""
    cpp = _cpp()
    paires = set(re.findall(r'envoyer\("(\w+)", "(\w+)"\)', cpp))
    paires |= {(l, "basculer") for l in re.findall(r'"(lumiere_\d)"', _constexpr("kHeritageLumieres"))}
    assert paires == {("pc", "basculer"), ("lumieres", "eteindre"), ("lumiere_1", "basculer"),
                      ("lumiere_2", "basculer"), ("lumiere_3", "basculer")}
    bp = _lire(BLUEPRINT)
    for emp, cmd in paires:
        assert f"'{cmd}'" in bp, cmd
        assert f"emplacement == '{emp}'" in bp or (
            emp.startswith("lumiere_") and "emplacement.startswith('lumiere_')" in bp), emp
    # Le volet 3.x garde son script (volet / arreter, ouvrir, fermer, tab5-scripts.yaml).
    assert "u.volet_tap = []() { id(tab5_volet_tap).execute(); };" in _lire("Tab5", "paquets", "tab5-tuiles.yaml")


# ─────────────────────────────────────────────────────────────────────────────
# Protocole : action, routage, version
# ─────────────────────────────────────────────────────────────────────────────

def _action(nom):
    api = _lire("Tab5", "paquets", "tab5-api-logic.yaml")
    return api.split(f"- service: {nom}\n", 1)[1].split("\n    - service:", 1)[0].split("\nprovisioning:", 1)[0]


def test_action_tab5_maj_tuiles_et_son_exemple():
    bloc = _action("tab5_maj_tuiles")
    assert "tuiles_definir(payload);" in bloc
    exemple = re.search(r'example: "([^"]+)"', bloc).group(1)
    types = set(_types_de_l_adr())
    lettres = set("dcoktrm")
    for entree in filter(None, exemple.split(";")):
        champs = entree.split("|")
        if re.fullmatch(r"p[0-4]|hp|hd|np|nd", champs[0]):
            assert len(champs) == 2, entree
            continue
        # Rangée sous l'horloge (ADR-0031) et panneau Ok Nabu (ADR-0041) : la classe
        # d'appareil en septième champ.
        if re.fullmatch(r"[hn][0-2][0-3]", champs[0]):
            assert len(champs) == 7 and re.fullmatch(r"[a-z0-9_]{0,15}", champs[6]), entree
            champs = champs[:6]
        assert re.fullmatch(r"t[0-4][0-9a-j]|[hn][0-4][0-4]", champs[0]) and len(champs) == 6, entree
        assert champs[1] in types and re.fullmatch(r"[a-z0-9_]{0,15}", champs[2]), entree
        assert set(champs[3]) <= lettres, entree


def test_etats_routes_avant_les_emplacements_3x():
    corps = _fonction(_lire("Tab5", "ecran", "tab5_zones.cpp"), "emplacements_appliquer")
    assert corps.index("tuiles_etat_recu(") < corps.index("for (size_t i = 0; i < n; i++)")
    recu = _fonction(_cpp(), "tuiles_etat_recu")
    # « tRT » (ou « hLI » / « nLI », rangée sous l'horloge et panneau Ok Nabu, ADR-0031 et
    # 0041) : trois caractères, R et T de 0 à 4 ; puis état | valeur | couleur (6 hex), lus
    # par etat_lire pour les deux.
    assert "if (n_cle != 3) return false;" in recu and "cle[0] != 't'" in recu
    assert "zone_de_lettre(cle[0])" in recu
    assert recu.count("etat_lire(") == 2
    lire = _fonction(_cpp(), "etat_lire")
    assert "decouper(reste, n_reste, f, 3)" in lire and "f[2].n == 6" in lire
    # Découpage champ par champ, champs vides compris (pas de strtok).
    assert "strtok" not in _cpp()


def test_version_annoncee_au_blueprint():
    """Le blueprint lit sw_version : à partir de 3.2.0, il envoie tab5_maj_tuiles."""
    version = re.search(r"default\('([^']+)'\)", _lire("tab5-ha-hmi.yaml").split("project:", 1)[1]).group(1)
    rendu = re.search(r"^\s+version: (\S+)$", _lire("tab5-rendu-host.yaml"), re.M).group(1)
    for v in (version, rendu):
        assert tuple(int(x) for x in re.match(r"(\d+)\.(\d+)\.(\d+)", v).groups()) >= (3, 2, 0), v


# ─────────────────────────────────────────────────────────────────────────────
# Écran : boutons, widgets, mode HA
# ─────────────────────────────────────────────────────────────────────────────

def test_boutons_des_tuiles_a_leur_position_visuelle():
    horaires = _lire("Tab5", "ui_components", "forecast_hourly.yaml")
    inclus = re.findall(r'idx: "(\d)", tuile: "(\d)"', horaires)
    assert len(inclus) == 5 and all(int(t) == 4 - int(i) for i, t in inclus), "tuile T = objet h(4−T)"
    carte = _lire("Tab5", "ui_components", "forecast_hour_card.yaml")
    assert "tuile_appui(${tuile}, false);" in carte and "tuile_appui(${tuile}, true);" in carte
    # Tuiles journalières et cartes du mode HA (08/10/2026, audit YML-3) : un gabarit dont le
    # bouton envoie sa propre tuile (n), inclus dans l'ordre des tuiles 0 à 4.
    for gabarit in ("forecast_day_body.yaml", "switch_card.yaml"):
        texte = _lire("Tab5", "ui_components", gabarit)
        assert "tuile_appui(${n}, false);" in texte and "tuile_appui(${n}, true);" in texte, gabarit
    jours = re.findall(r'file: forecast_day_(?:card|body)\.yaml, vars: \{ n: "(\d)"',
                       _lire("Tab5", "ui_components", "forecast_daily.yaml"))
    assert jours == list("01234"), jours
    cartes = re.findall(r'file: switch_card\.yaml, vars: \{ n: "(\d)"',
                        _lire("Tab5", "ui_components", "switches_card.yaml"))
    assert cartes == list("01234"), cartes
    # Plus aucune commande 3.x écrite en dur sur une tuile : tout passe par tuile_appui().
    assert "id: tab5_action" not in _lire("Tab5", "ui_components", "forecast_daily.yaml")
    assert "id: tab5_action" not in _lire("Tab5", "ui_components", "switches_card.yaml")


def test_widgets_poses_sans_toucher_a_l_on_boot():
    tuiles = _lire("Tab5", "paquets", "tab5-tuiles.yaml")
    for t in range(5):
        assert f"u.heure_g[{t}] = id(icon_card_h{4 - t}_g);" in tuiles
        assert f"u.heure_bouton[{t}] = id(btn_h{4 - t}_action);" in tuiles
        assert f"u.carte_nom[{t}] = id(lbl_sw{t}_title);" in tuiles
        assert f"u.lum_ligne[{t}] = id(btn_light_sel_{t});" in tuiles
        assert f"u.vol_ligne[{t}] = id(volet_ligne_{t});" in tuiles
        assert f"u.lum_onglet[{t}] = id(lum_onglet_{t});" in tuiles and f"u.vol_onglet[{t}] = id(vol_onglet_{t});" in tuiles
    zones = _lire("Tab5", "paquets", "tab5-zones.yaml").split("- id: tab5_zones_apply", 1)[1]
    assert zones.index("script.execute: tab5_tuiles_ui") < zones.index("zones_apply_ui();")
    on_boot = _lire("tab5-ha-hmi.yaml").split("  on_boot:", 1)[1].split("\npackages:", 1)[0]
    assert "tuile" not in on_boot and "ha_mode" not in on_boot
    assert "tab5_tuiles: !include Tab5/paquets/tab5-tuiles.yaml" in _lire("tab5-ha-hmi.yaml")


def test_cartes_du_mode_ha_facon_carte_tile():
    """Carte « tile » de HA (06/10/2026, discussion #278) : l'icône dans une pastille
    ronde de la couleur de l'état, le bouton sur la pastille, le nom dans un cadre
    cliquable (sens d'un volet), sans onglet."""
    tuiles = _lire("Tab5", "paquets", "tab5-tuiles.yaml")
    # Une carte = switch_card.yaml, incluse pour n = 0 à 4 (08/10/2026, audit YML-3) : la
    # place n (ADR-0066 : l'élément entier de cette place, tuile_appui → element_de_place).
    carte = _lire("Tab5", "ui_components", "switch_card.yaml")
    for t in range(5):
        assert f"u.carte_pastille[{t}] = id(sw_pastille_{t});" in tuiles
    pastille = carte.split('id: "sw_pastille_${n}"\n', 1)[1].split("- button:", 1)[0]
    assert 'id: "icon_sw${n}",' in pastille and "clickable: false" in pastille
    bouton = carte.split('id: "btn_sw${n}_action"\n', 1)[1].split("!include", 1)[0]
    assert "tuile_appui(${n}, false);" in bouton and "tuile_appui(${n}, true);" in bouton
    # Dessin partagé avec les lignes du popup Maison (ADR-0037) : peindre_vue_sur.
    assert "ui_fond(w.pastille, v.couleur_carte);" in _fonction(_cpp(), "peindre_vue_sur")
    carte = "peindre_vue_sur(v, {u.carte_pastille[p], u.carte_icone[p], u.carte_nom[p], u.carte_etat[p]},"
    assert carte in _fonction(_cpp(), "peindre_carte")
    assert "const int t = element_de_place(place);" in _fonction(_cpp(), "tuile_appui")
    titre = _lire("Tab5", "ui_components", "switch_card_title_tab.yaml")
    assert titre.split("\nobj:", 1)[1].count("widgets:") == 1, "le nom garde son cadre (parent cliquable)"


def test_mode_ha_seule_source_et_swipe_par_piece():
    assert "show_switches" not in _lire("Tab5", "paquets", "tab5-globals.yaml").split("globals:", 1)[1].split("#", 1)[0]
    central = _lire("Tab5", "ecran", "tab5_central.cpp")
    swipe = _fonction(central, "handle_swipe_gesture")
    # En mode HA, le swipe change de pièce et ne passe jamais par apply_forecast_page
    # (qui réaffichait le calque météo sous les cartes).
    assert swipe.index("if (ctx.ha_mode)") < swipe.index("apply_forecast_page(")
    assert "!ctx.ha_mode" in _fonction(central, "rotator_owns_card")
    assert "if (g_central_ctx.ha_mode) return RetourAuto::RIEN;" in _lire("Tab5", "ecran", "tab5_anim.cpp")
    assert "if (e == Ecran::ACCUEIL) tuiles_mode_ha(false);" in _lire("Tab5", "paquets", "tab5-navigation.yaml")
    # Le tap du bouton maison (action mode_domo, au choix depuis le 09/10/2026) : script
    # tab5_geste (tests/test_gestes.py).
    assert "tuiles_mode_ha(!g_central_ctx.ha_mode);" in _lire("Tab5", "paquets", "tab5-navigation.yaml")


def test_rouleau_des_pieces_en_mode_ha():
    """Mode HA (18/10/2026, demande d'Axel) : le swipe passe à la pièce suivante ou précédente,
    en rouleau (la 5 revient à la 1, pièces vides comprises) ; les pastilles suivent l'ordre
    des pièces. La météo garde son ordre et son bouclage volontaire ([AI-WARNING])."""
    swipe = _fonction(_cpp(), "tuiles_swipe_ha")
    # Les N pièces choisies dans HA (ADR-0066), en rouleau.
    assert "reglages_page_voisine(piece_courante(), pieces_nombre(), gauche)" in swipe
    assert "forecast_page_suivante" not in swipe and "piece_non_vide" not in swipe
    # Pastilles : la pièce en mode HA (une par pièce), la page sinon (une par page de la météo) ;
    # repeintes à l'entrée et à la sortie.
    pastilles = _fonction(_cpp(), "pastilles_peindre")
    assert "ctx.ha_mode ? piece_courante() : ctx.forecast_page" in pastilles
    assert "const int n = ctx.ha_mode ? pieces_nombre() : pages_meteo();" in pastilles
    assert "pastilles_peindre();" in _fonction(_cpp(), "aller_piece_ha")
    # La pièce du mode HA a son propre index ; forecast_page suit sa page.
    assert "if (g_central_ctx.ha_mode) return s_piece_ha;" in _fonction(_cpp(), "piece_courante")
    assert "g_central_ctx.forecast_page = page_de_piece(r);" in _fonction(_cpp(), "aller_piece_ha")
    mode = _fonction(_cpp(), "tuiles_mode_ha")
    assert "aller_piece_ha(piece_de_page(cible));" in mode
    assert mode.index("ctx.ha_mode = false;") < mode.index("pastilles_peindre();")
    # La météo : droite 4→3→2→1→0→2 inchangée ; gauche 0→1→2→3→4→2, ou →5→…→N−1→2 avec les
    # pages de pièce (ADR-0066, décision d'Axel du 18/10/2026 : le côté gauche seul).
    central = _lire("Tab5", "ecran", "tab5_central.cpp")
    meteo = _fonction(central, "forecast_page_suivante")
    assert "return meteo_page_suivante(page, gauche, tuiles_pages_meteo());" in meteo
    regle = _fonction(_lire("Tab5", "socle", "tab5_core.cpp"), "meteo_page_suivante")
    assert "if (gauche) return page >= pages - 1 ? 2 : page + 1;" in regle and "return page <= 0 ? 2 : page - 1;" in regle
    assert "forecast_page_suivante(old_page, dir == LV_DIR_LEFT)" in _fonction(central, "handle_swipe_gesture")


# ─────────────────────────────────────────────────────────────────────────────
# Popup du volet (05/10/2026, discussion #278)
# ─────────────────────────────────────────────────────────────────────────────

def test_appui_long_d_un_volet_ouvre_son_popup_sauf_avec_k():
    """L'appui long d'une tuile vol ouvre le popup du volet ; avec l'option k, l'ancien
    appui long (l'autre sens, confirmé) : le popup ne contourne jamais la confirmation.
    L'option r n'arrive pas jusque-là (gestes), le mode héritage non plus."""
    cpp = _cpp()
    appui = _fonction(cpp, "tuile_appui_piece")
    gestes = _fonction(cpp, "gestes")
    assert _gestes_par_type()["vol"] == (True, None, True, "VOLET")
    # Option k : ni roue ni popup ; l'appui long garde l'autre sens, confirmé.
    assert "const bool confirme = (d.options & OPT_K) && type != Type::CLI;" in gestes
    assert "g.roue = l.roue && !confirme;" in gestes
    assert "if (type == Type::VOL && confirme) g.fenetre = Fenetre::AUCUNE;" in gestes
    assert (appui.index("if (g.roue && roue_de_la_tuile(r, t)) return;")
            < appui.index("ouvrir_fenetre(g.fenetre, d, r, t);") < appui.index("vol_appui_long(e)"))
    assert "case Fenetre::VOLET:\n            popup_volet_ouvrir(r, t);" in _fonction(cpp, "ouvrir_fenetre")
    assert appui.index("appui_heritage(t, long_appui);") < appui.index("gestes(d,")
    assert appui.index("if (!g.agit) return;") < appui.index("if (long_appui) {")
    long_adr = _types_de_l_adr()["vol"][1]
    assert "shutter popup" in long_adr and "with `k`" in long_adr


def test_commandes_du_popup_du_volet_dans_le_contrat():
    cpp = _cpp()
    adr = _commandes_de_l_adr()
    # Boutons : les commandes de tuile d'un volet, rien d'autre.
    popup = _lire("Tab5", "ui_components", "volet_popup.yaml")
    boutons = re.findall(r"file: volet_btn\.yaml, vars: \{[^\n]*?appel: (\w+), commande: (\w+)", popup)
    assert sorted(c for a, c in boutons if a == "popup_volet_commande") == ["arreter", "fermer", "ouvrir"]
    # Tout ouvrir / Tout fermer (ADR-0046) : la commande à chaque volet de la pièce (toutes ses
    # pages depuis l'ADR-0066).
    assert sorted(c for a, c in boutons if a == "popup_volets_tout") == ["fermer", "ouvrir"]
    assert len(boutons) == 5
    assert "${appel}(\"${commande}\");" in _lire("Tab5", "ui_components", "volet_btn.yaml")
    tout = _fonction(cpp, "popup_volets_tout")
    assert "if (est_volet(s_pv.piece, t)) envoyer_tuile(s_pv.piece, t, action);" in tout
    assert "envoyer_tuile(s_pv.piece, s_pv.tuile, action);" in _fonction(cpp, "popup_volet_commande")
    # Volet dessiné : « position » (dans le tableau de l'ADR), 0-100, à la tuile du popup,
    # et jamais sans position connue (même si elle s'est perdue pendant le geste).
    envoi = _fonction(cpp, "popup_volet_envoyer_position")
    assert 'u.envoyer(tuile_cle(s_pv.piece, s_pv.tuile).s, "position", valeur);' in envoi and "position" in adr
    assert 'snprintf(valeur, sizeof(valeur), "%d", std::clamp(s_pv.pos, 0, 100));' in envoi
    garde = "if (!vol_position_connue(s_etats[s_pv.piece][s_pv.tuile])) return false;"
    assert garde in envoi and envoi.index(garde) < envoi.index("u.envoyer(")
    # « position » ne part que de là et des positions de la roue d'actions rapides
    # (ADR-0036, 25 / 50 / 75 %), proposées seulement quand la position est connue et
    # recalculées au toucher ; la fonction n'a qu'un appelant : le relâcher.
    assert cpp.count('"position"') == 2
    roue = _fonction(cpp, "roue_choix").split("case RoueAction::POSITION:", 1)[1].split("case RoueAction::MODE:", 1)[0]
    assert "if (!vol_position_connue(e)) break;" in roue and 'choix("position", nombre, position == p);' in roue
    assert cpp.count("popup_volet_envoyer_position()") == 2, "définition + le relâcher, rien d'autre"


def test_le_volet_dessine_n_envoie_qu_au_relacher():
    """Glisser ne fait que dessiner ; un toucher (sous le seuil) et un volet sans position
    connue n'envoient rien ; le relâcher (ou un doigt perdu) envoie une fois."""
    rappel = _fonction(_cpp(), "volet_cadre_rappel")
    appui = rappel.split("case LV_EVENT_PRESSED:", 1)[1].split("break;", 1)[0]
    glisse = rappel.split("case LV_EVENT_PRESSING:", 1)[1].split("case LV_EVENT_RELEASED:", 1)[0]
    relache = rappel.split("case LV_EVENT_PRESS_LOST:", 1)[1].split("default:", 1)[0]
    assert "envoyer" not in glisse, "le glissement ne doit rien envoyer"
    # Saisi seulement si la position est connue ; rien ne glisse sinon.
    assert "s_pv.glisse = false;" in appui
    assert "s_pv.saisi = popup_volet_valide() && vol_position_connue(s_etats[s_pv.piece][s_pv.tuile]);" in appui
    assert glisse.index("if (!s_pv.saisi) break;") < glisse.index("s_pv.glisse = true;")
    # Un toucher n'arme pas : il faut dépasser le seuil.
    assert "if (!s_pv.glisse && std::abs(dy) < kVoletSeuilGlisse) break;" in glisse
    assert int(_constexpr("kVoletSeuilGlisse")) >= 8
    # Le dessin et le nombre suivent le doigt, bornés à 0-100 ; vers le bas, ça ferme.
    assert "std::clamp(s_pv.pos_appui - static_cast<int>(dy * 100 / kVoletFenetreH), 0, 100)" in glisse
    assert "popup_volet_dessiner(pos, false);" in glisse and "popup_volet_nombre(pos);" in glisse
    # Relâcher : une fois, après un vrai glissement seulement ; sinon, retour à l'état de HA.
    assert "const bool envoyer = s_pv.saisi && s_pv.glisse;" in relache
    assert relache.index("s_pv.saisi = false;") < relache.index("popup_volet_envoyer_position()")
    assert "if (envoyer && popup_volet_envoyer_position()) break;" in relache
    assert "popup_volet_peindre();" in relache
    # Les quatre événements, sur le cadre ; un geste sur le volet ne remonte pas à la page.
    branche = _fonction(_cpp(), "tuiles_brancher_popup_volet")
    assert "{LV_EVENT_PRESSED, LV_EVENT_PRESSING, LV_EVENT_RELEASED, LV_EVENT_PRESS_LOST}" in branche
    assert "lv_obj_add_event_cb(cadre, volet_cadre_rappel, code, nullptr);" in branche
    assert "lv_obj_remove_flag(cadre, LV_OBJ_FLAG_GESTURE_BUBBLE);" in branche
    assert "lames_construire(g_tuiles_ui.vol_tablier);" in branche


def test_le_volet_dessine_suit_la_position_de_ha():
    """Chaque état poussé redessine le tablier tout de suite (pas d'animation), jamais sous
    le doigt ; sans position connue, le dessin montre l'état et le nombre disparaît."""
    cpp = _cpp()
    peindre = _fonction(cpp, "popup_volet_peindre")
    # Jamais sous le doigt, ni entre le relâcher et le prochain état de HA (un repeint de
    # thème gardait la position de HA à chaud et celle du doigt à froid : rendu « clair »).
    assert "if (!s_pv.saisi && !s_pv.cible) s_pv.pos = vol_position_dessin(e, s_pv.estompe);" in peindre
    assert "popup_volet_dessiner(s_pv.pos, s_pv.estompe);" in peindre
    envoi = _fonction(cpp, "popup_volet_envoyer_position")
    assert envoi.index('"position", valeur);') < envoi.index("s_pv.cible = true;")
    recu = _fonction(cpp, "tuiles_etat_recu")
    assert recu.index("popup_volet_etat_pousse(r, t);") < recu.index("peindre_tuile(r, t);")
    assert "if (r == s_pv.piece && t == s_pv.tuile) s_pv.cible = false;" in _fonction(cpp, "popup_volet_etat_pousse")
    # Oubliée aussi quand un autre volet est choisi (volet_doigt_oublier, ADR-0046).
    assert cpp.count("s_pv.cible = true;") == 1 and cpp.count("s_pv.cible = false;") == 2
    assert "s_pv.cible = false;" in _fonction(cpp, "volet_doigt_oublier")
    assert "ui_hidden(u.vol_position, !connue);" in peindre
    dessiner = _fonction(cpp, "popup_volet_dessiner")
    assert "ui_y(g_tuiles_ui.vol_tablier, -(std::clamp(pos, 0, 100) * kVoletFenetreH) / 100);" in dessiner
    assert "lv_anim" not in dessiner and "lv_anim" not in peindre, "transitions instantanées"
    # Position inconnue : fermé en bas, ouvert en haut, le reste à mi-hauteur estompé.
    dessin = _fonction(cpp, "vol_position_dessin")
    assert "if (vol_position_connue(e)) return tab5_float_vers_int(e.valeur, 0, 100, 0);" in dessin
    assert 'if (e.recu && est(e.brut, "closed")) return 0;' in dessin
    assert 'if (e.recu && est(e.brut, "open") && !(e.valeur < 0.0f)) return 100;' in dessin
    assert "estompe = true;" in dessin and _constexpr("kVoletMilieu") == "50"
    connue = _fonction(cpp, "vol_position_connue")
    assert "!std::isnan(e.valeur) && e.valeur >= 0.0f && e.valeur <= 100.0f" in connue


def test_geometrie_du_volet_dessine():
    """La fenêtre et le tablier du YAML ont la hauteur du C++ ; les lames la couvrent ;
    le toucher arrive au cadre (fenêtre et tablier non cliquables)."""
    popup = _lire("Tab5", "ui_components", "volet_popup.yaml")
    # Mise en page ramenée au corps de chaque carte (lot 4 du portage) : le C++ et le YAML
    # calculés pour la carte ; sur le Tab5, les valeurs au pixel d'avant (456, 38).
    for carte in CARTES:
        c = constantes_cpp(_cpp(), carte)
        h, lame = c["kVoletFenetreH"], c["kVoletLameH"]
        assert h % lame == 0 and h // lame == 12, carte
        if carte == "tab5":
            assert (h, lame) == (456, 38)
        for ident in ("volet_fenetre", "volet_tablier"):
            bloc = popup.split(f"id: {ident}\n", 1)[1].split("widgets:", 1)[0]
            haut = re.search(r"\n\s+height: (.+)\n", bloc).group(1).strip()
            assert resoudre(haut, jetons_resolus(carte)) == h, (carte, ident)
    for ident in ("volet_fenetre", "volet_tablier"):
        bloc = popup.split(f"id: {ident}\n", 1)[1].split("widgets:", 1)[0]
        assert "clickable: false" in bloc, ident
        assert "scrollable: false" in bloc, ident
    cadre = popup.split("id: volet_cadre\n", 1)[1].split("widgets:", 1)[0]
    assert "clickable: false" not in cadre
    # Le tablier remplit la fenêtre (même largeur), qui le rogne.
    largeurs = [re.search(r"\n\s+width: (.+)\n", popup.split(f"id: {i}\n", 1)[1]).group(1).strip()
                for i in ("volet_fenetre", "volet_tablier")]
    assert largeurs[0] == largeurs[1]
    # Pastille des boutons : pas cliquable, sinon elle prend l'appui du bouton.
    bouton = _lire("Tab5", "ui_components", "volet_btn.yaml")
    assert bouton.split("- obj:", 1)[1].count("clickable: false") == 1


def test_popup_du_volet_inscrit_et_branche():
    scripts = _lire("Tab5", "paquets", "tab5-navigation.yaml")
    # Ouvert aussi sans tuile touchée (« Aller à l'écran », roue de navigation, ADR-0042).
    assert re.search(r'ModalRegistry::add\(id\(volet_popup\),\s+"Shutter",\s+ModalRegistry::POPUP,'
                     r'\s+\[\] \{ tuiles_ecran_ouvrir\(Ecran::VOLET\); \}\);', scripts)
    assert "- !include ../ui_components/volet_popup.yaml" in _lire("Tab5", "paquets", "tab5-lvgl.yaml")
    tuiles = _lire("Tab5", "paquets", "tab5-tuiles.yaml")
    for champ, widget in (("vol_popup", "volet_popup"), ("vol_nom", "volet_nom"),
                          ("vol_position", "volet_position"), ("vol_nombre", "volet_nombre"),
                          ("vol_etat", "volet_etat"), ("vol_cadre", "volet_cadre"),
                          ("vol_tablier", "volet_tablier")):
        assert f"u.{champ} = id({widget});" in tuiles
    for pose in ("u.vol_cadre = id(volet_cadre);", "u.vol_tablier = id(volet_tablier);"):
        assert tuiles.index(pose) < tuiles.index("tuiles_brancher_popup_volet();")
    # Mis à jour en direct : chaque tuile repeinte repeint le popup s'il la montre.
    assert "popup_volet_etat(r, t);" in _fonction(_cpp(), "peindre_tuile")


# ─────────────────────────────────────────────────────────────────────────────
# Popup d'un appareil (06/10/2026, discussion #278)
# ─────────────────────────────────────────────────────────────────────────────

def _cas(appui, type_, suivant):
    return appui.split(f"case Type::{type_}:", 1)[1].split(f"case Type::{suivant}:", 1)[0]


def test_appui_long_d_un_appareil_ouvre_son_popup():
    """L'appui long d'une tuile int ou act ouvre le popup de l'appareil ; celui d'une med
    sans l'option t, le lecteur de musique (ADR-0050, 10/10/2026 ; avec t : la
    télécommande) ; l'appui court ne change pas. Lecture seule (option r) : gestes coupe
    avant, comme le mode météo sans appareils."""
    cpp = _cpp()
    appui = _fonction(cpp, "tuile_appui_piece")
    table = _gestes_par_type()
    for t in ("int", "act"):
        assert table[t][2:] == (False, "APPAREIL"), t
    assert table["med"][2:] == (False, "LECTEUR")
    assert "case Fenetre::APPAREIL:\n            popup_appareil_ouvrir(r, t);" in _fonction(cpp, "ouvrir_fenetre")
    lecteur = _fonction(cpp, "ouvrir_fenetre").split("case Fenetre::LECTEUR:", 1)[1].split("return true;", 1)[0]
    assert "g_tuiles_ui.lecteur_ouvrir(modele_ha::tuile_cle(r, t).s)" in lecteur
    # Le − / + n'ouvre pas le lecteur : ses trois ouvertures sont celles de la demande.
    assert "f == Fenetre::LECTEUR) return false;" in _fonction(cpp, "tuile_ouvrir_popup")
    # med avec l'option t : la télécommande à la place.
    assert "if (type == Type::MED && (d.options & OPT_T)) g.fenetre = Fenetre::TELECOMMANDE;" in _fonction(cpp, "gestes")
    # Les appuis courts d'aujourd'hui : basculer (allumer avec o), lancer.
    assert (table["int"][1], table["med"][1], table["act"][1]) == ("basculer", "basculer", "lancer")
    assert appui.index("if (!g.agit) return;") < appui.index("if (long_appui) {")
    assert ("return s_pa.en_grille() && gestes(s_m.tuiles[s_pa.piece][s_pa.tuile], false).fenetre == "
            "Fenetre::APPAREIL;") in _fonction(cpp, "popup_appareil_valide")
    # Le tableau de l'ADR le dit aussi.
    types = _types_de_l_adr()
    for t in ("int", "act"):
        assert "device popup" in types[t][1], t
    assert "music player popup" in types["med"][1]
    # Mode météo sans appareils : aucun appui, popup compris.
    assert "if (!g_central_ctx.ha_mode && !s_appareils_meteo) return;" in _fonction(_cpp(), "tuile_appui")


def test_le_bouton_du_popup_fait_le_toucher_de_la_tuile():
    """Le grand bouton passe par le même chemin que le toucher de la tuile : même
    commande, même confirmation (option k), même « OK » — jamais une commande à part."""
    cpp = _cpp()
    corps = _fonction(cpp, "popup_appareil_appui")
    assert "if (!popup_appareil_valide()) return;" in corps
    assert "tuile_appui_piece(s_pa.piece, s_pa.tuile, false);" in corps
    assert "envoyer" not in corps, "le popup n'envoie rien lui-même"
    assert "popup_appareil_appui();" in _lire("Tab5", "ui_components", "appareil_popup.yaml")
    # Ni lecture seule, ni mode héritage, ni type sans popup (définitions changées popup ouvert).
    valide = _fonction(cpp, "popup_appareil_valide")
    assert "s_pa.en_grille() && gestes(" in valide
    assert "tuile >= 0 && tuile < kTuilesMax && !heritage(); }" in cpp
    assert "(d.options & OPT_R)) return g;" in _fonction(cpp, "gestes")
    # Popup refermé quand sa tuile ne l'a plus, repeint sinon (et au changement de thème).
    assert "popups_revalider();" in _fonction(cpp, "tuiles_definir")
    revalider = _fonction(cpp, "popups_revalider")
    assert "popup_tuile_revalider(s_pa, popup_appareil_valide(), popup_appareil_peindre);" in revalider
    assert "popup_volet_revalider();" in revalider
    assert "popups_rejouer_theme();" in _fonction(cpp, "tuiles_rejouer_theme")
    assert "if (s_pa.ouvert()) popup_appareil_peindre();" in _fonction(cpp, "popups_rejouer_theme")
    # Le popup dit l'option k et l'option o, et ce que fera l'appui.
    peindre = _fonction(cpp, "popup_appareil_peindre")
    assert "minuterie_sur(s_confirmation, r, t)" in peindre and "OPT_K" in peindre and "OPT_O" in peindre


def test_popup_d_un_appareil_inscrit_et_branche():
    scripts = _lire("Tab5", "paquets", "tab5-navigation.yaml")
    assert re.search(r'ModalRegistry::add\(id\(appareil_popup\),\s+"Device",\s+ModalRegistry::POPUP\);', scripts)
    assert "- !include ../ui_components/appareil_popup.yaml" in _lire("Tab5", "paquets", "tab5-lvgl.yaml")
    tuiles = _lire("Tab5", "paquets", "tab5-tuiles.yaml")
    popup = _lire("Tab5", "ui_components", "appareil_popup.yaml")
    for champ, widget in (("app_popup", "appareil_popup"), ("app_titre", "appareil_popup_titre"),
                          ("app_pastille", "appareil_pastille"), ("app_icone", "appareil_icone"),
                          ("app_etat", "appareil_etat"), ("app_piece", "appareil_piece"),
                          ("app_options", "appareil_options"), ("app_remplissage", "appareil_remplissage"),
                          ("app_commande_icone", "appareil_commande_icone"), ("app_action", "appareil_action")):
        assert f"u.{champ} = id({widget});" in tuiles
        assert re.search(rf"\bid: {widget}\b|title_id: \"{widget}\"", popup), widget
    # Le remplissage ne doit pas prendre l'appui du bouton.
    remplissage = popup.split("id: appareil_remplissage", 1)[1].split("widgets:", 1)[0]
    assert "clickable: false" in remplissage
    assert "popup_appareil_etat(r, t);" in _fonction(_cpp(), "peindre_tuile")
