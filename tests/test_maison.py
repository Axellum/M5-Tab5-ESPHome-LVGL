# -*- coding: utf-8 -*-
"""Popup « Maison » (ADR-0037, 07/10/2026, discussion #278) : toute la maison, pièce par
pièce, comme un tableau de bord HA. Aucun compilateur ne vérifie ce que ce fichier lit
dans le C++ et le YAML :

- popup du registre (POPUP « Maison », dernier des POPUP) et option « Maison » du select
  « Aller à l'écran », à la fin, qui lance le même script ;
- chrome partagé (ADR-0009) ;
- rien de nouveau avec HA : les lignes passent par les gestes des tuiles
  (tuile_appui_piece), « Éteindre les lumières » par « Pièce : tout éteindre » ;
- « ⋯ » réservé aux tuiles qui ont un appui long (ni cap, ni bin, ni l'option r) ;
- ordre des enfants d'une ligne = celui que lit tab5_maison.cpp ; géométrie = jetons des
  popups ; glyphes dans les polices de leurs widgets ;
- colonnes dans l'ordre des pièces du blueprint (R = 0 → N − 1), cinq au plus par page
  (ADR-0066 : les widgets sont des places, colonne C et ligne K) ;
- tap sur le titre de la pièce, en mode HA et sur une page de pièce de la météo."""
import re
import subprocess
import sys
from pathlib import Path
from tests.commun import contrat, geometrie_cpp, jeton, lire as _lire, source

REPO = Path(__file__).resolve().parent.parent

from check_tab5_code_rules import MDI_CODE_TARGETS, font_glyphs  # noqa: E402
import check_tab5_modal_chrome  # noqa: E402

TAB5 = REPO / "Tab5"
UI = TAB5 / "ui_components"


def _fonction(source, nom):
    """Corps d'une fonction C++ (de sa signature à l'accolade fermante en colonne 0)."""
    m = re.search(rf"^[^\n;]*\b{nom}\([^;{{]*\)\s*{{\n(.*?)^}}", source, re.M | re.S)
    assert m, f"fonction {nom} introuvable"
    return m.group(1)


def _constexpr(source, nom):
    m = re.search(rf"constexpr int32_t {nom} = ([^;]+);", source)
    assert m, f"constexpr {nom} introuvable"
    return m.group(1).strip()


def _jeton(nom):
    return jeton(nom)


def _maison():
    return _lire("Tab5", "ecran", "tab5_maison.cpp")


def _tuiles():
    # Tuiles, popups, roue d'une tuile et leur en-tête commun (lot L7, 08/10/2026).
    return "\n".join(_lire(source(f)) for f in ("tab5_tuiles_priv.h", "tab5_tuiles.cpp", "tab5_tuiles_popups.cpp", "tab5_tuiles_roue.cpp"))


def test_popup_du_registre_dernier_des_popup():
    scripts = _lire("Tab5", "paquets", "tab5-navigation.yaml")
    ajouts = re.findall(r'ModalRegistry::add\(id\((\w+)\),\s+(?:"[^"]*"|nullptr),\s+ModalRegistry::(\w+)[,)]', scripts)
    popups = [obj for obj, genre in ajouts if genre == "POPUP"]
    assert popups[-1] == "maison_popup", "« Maison » à la fin du bloc des POPUP"
    assert re.search(r'ModalRegistry::add\(id\(maison_popup\),\s+"House",\s+ModalRegistry::POPUP,'
                     r'\s+\[\] \{ id\(tab5_maison_ouvrir\)\.execute\(\); \}\);', scripts)
    # Ceux qu'une ligne ouvre sont inscrits avant lui : visibles devant lui, ils sont nommés.
    for avant in ("light_options_popup", "volet_popup", "appareil_popup", "clim_options_popup",
                  "tv_remote_popup", "energie_popup"):
        assert popups.index(avant) < popups.index("maison_popup"), avant
    assert "- !include ../ui_components/maison_popup.yaml" in _lire("Tab5", "paquets", "tab5-lvgl.yaml")


def test_option_maison_du_select_a_la_fin():
    controles = _lire("Tab5", "paquets", "tab5-navigation.yaml")
    bloc = controles.split("id: tab5_goto_screen", 1)[1].split("on_value:", 1)[0]
    options = re.findall(r'^\s*-\s*"([^"]+)"', bloc, re.M)
    # Ajoutée à la fin le 07/10/2026 : les index des autres options ne bougent pas. Celles
    # de la roue de navigation (ADR-0042), du popup Météo (ADR-0043), du lecteur de musique
    # (ADR-0050), du popup Caméras (ADR-0049), du popup Suivi (ADR-0054) et du popup Froid
    # (ADR-0055) sont venues après elle.
    # En anglais depuis le 24/10/2026 (ADR-0070), dans le même ordre (« Maison », « Lumières »…).
    assert options[12] == "House", "à la fin : les index des autres options ne bougent pas"
    # Puis celle du popup Serveur IA (ADR-0059).
    assert options[13:] == ["Lights", "Shutter", "Temperature", "Weather", "Music", "Cameras", "Tracking",
                            "Fridges", "AI server"]
    # L'index de l'option est sa valeur d'Ecran (tab5_zones.h, tests/test_appuis.py) ; le
    # select et les appuis longs passent par la routine unique tab5_ecran_ouvrir.
    enum = re.search(r"enum class Ecran : uint8_t \{(.*?)\};", contrat(), re.S).group(1)
    valeurs = re.findall(r"\b([A-Z]+),", re.sub(r"//[^\n]*", "", enum))
    assert valeurs.index("MAISON") == options.index("House")
    script = controles.split("- id: tab5_ecran_ouvrir", 1)[1].split("\ntext_sensor:", 1)[0]
    # Ouverture : celle que le registre donne à « Maison » (test plus haut).
    assert "ModalRegistry::ouvrir(target)" in script


def test_chrome_partage():
    problemes = [p for p in check_tab5_modal_chrome.scan() if p.startswith("maison")]
    assert not problemes, problemes
    popup = _lire("Tab5", "ui_components", "maison_popup.yaml")
    assert "file: modal_scrim.yaml" in popup and "file: modal_header.yaml" in popup
    assert 'title: "Maison"' in popup
    # Bouton d'en-tête sur la ligne de la barre, à gauche de la croix (comme les alertes).
    bouton = popup.split("id: btn_maison_eteindre", 1)[1].split("widgets:", 1)[0]
    assert "y: 4" in bouton and "height: 44" in bouton and "x: -100" in bouton and "hidden: true" in bouton
    assert "maison_eteindre_lumieres();" in bouton or "maison_eteindre_lumieres();" in popup


def test_widgets_poses_par_le_script():
    script = _lire("Tab5", "paquets", "tab5-maison.yaml")
    popup = _lire("Tab5", "ui_components", "maison_popup.yaml")
    # Places (ADR-0066) : colonne C, ligne K de la page affichée.
    for c in range(5):
        assert f"u.entete[{c}] = id(maison_entete_{c});" in script
        assert f'file: maison_entete.yaml, vars: {{ c: "{c}" }}' in popup
        for k in range(5):
            assert f"u.ligne[{c}][{k}] = id(maison_ligne_{c}{k});" in script
            assert f'file: maison_ligne.yaml, vars: {{ c: "{c}", k: "{k}" }}' in popup
    # Onglets des pages (ADR-0066) : un par groupe de colonnes (dix pièces : deux groupes de cinq
    # au plus, lot A) ou, avec un seul groupe, un par tranche de cinq lignes (vingt éléments :
    # quatre tranches, lot B) ; quatre boutons au plus.
    for p in range(4):
        assert f"u.onglet[{p}] = id(maison_onglet_{p});" in script
        assert f"file: pages_onglet.yaml, vars: {{ id: maison_onglet_{p}, page: {p}, choisir: maison_page }}" in popup
    entete = _lire("Tab5", "ecran", "tab5_maison.h")
    assert "constexpr int kMaisonGroupesMax = 2;" in entete and "constexpr int kMaisonTranchesMax = 4;" in entete
    assert "constexpr int kMaisonPagesMax = kMaisonGroupesMax * kMaisonTranchesMax;" in entete
    assert "lv_obj_t* onglet[kMaisonOngletsMax] = {};" in entete
    assert script.index("u.popup = id(maison_popup);") > script.index("u.ligne[4][4]"), "popup en dernier"
    for paquet in ("tab5-ha-hmi.yaml", "tab5-rendu-host.yaml"):
        texte = _lire(paquet)
        assert "tab5_maison: !include Tab5/paquets/tab5-maison.yaml" in texte, paquet
        assert "- Tab5/ecran/tab5_maison.cpp" in texte, paquet
    on_boot = _lire("tab5-ha-hmi.yaml").split("  on_boot:", 1)[1].split("\npackages:", 1)[0]
    assert "maison" not in on_boot


def test_ordre_des_enfants_d_une_ligne():
    """tab5_maison.cpp lit les widgets d'une ligne par leur rang (enfant()) : pastille
    (son icône), nom, état, « ⋯ »."""
    ligne = _lire("Tab5", "ui_components", "maison_ligne.yaml")
    enfants = re.findall(r"^    - (\w+):", ligne.split("\n  widgets:\n", 1)[1], re.M)
    assert enfants == ["obj", "label", "label", "button"]
    pastille = ligne.split("\n    - obj:", 1)[1].split("\n    - label:", 1)[0]
    assert 'id: "maison_icone_${c}${k}"' in pastille and "text_font: mdi_font_32" in pastille
    assert "clickable: false" in pastille
    peindre = _fonction(_maison(), "dessiner_ligne")
    assert "lv_obj_t* pastille = enfant(ligne, 0);" in peindre
    assert "ui_hidden(enfant(ligne, 3), !appui_long);" in peindre
    assert "{pastille, enfant(pastille, 0), enfant(ligne, 1), enfant(ligne, 2)}" in peindre


def test_gestes_d_une_ligne_ceux_de_la_tuile():
    ligne = _lire("Tab5", "ui_components", "maison_ligne.yaml")
    corps, plus = ligne.split("\n    - button:", 1)
    assert "on_short_click:\n    - lambda: 'maison_ligne_appui(${c}, ${k}, false);'" in corps
    assert "on_long_press:\n    - lambda: 'maison_ligne_appui(${c}, ${k}, true);'" in corps
    assert "maison_ligne_appui(${c}, ${k}, true);" in plus and "on_long_press" not in plus
    # La place dit quelle tuile elle montre (s_place) ; la roue (ADR-0036) s'ancre sur la
    # pastille de la ligne (enfant 0).
    ligne_appui = _fonction(_maison(), "maison_ligne_appui")
    # La place [C][K] de la page affichée montre la tuile s_place[C][K] (ADR-0066 : une page =
    # un groupe de colonnes × une tranche de cinq lignes).
    assert "const Place pl = s_place[c][k];" in ligne_appui and "if (pl.r < 0) return;" in ligne_appui
    assert "tuile_appui_maison(pl.r, pl.t, long_appui, ligne != nullptr ? enfant(ligne, 0) : nullptr);" in ligne_appui
    # Appui long (et « ⋯ ») : la roue de la tuile d'abord (sans son lien « Maison »), sinon
    # le répartiteur des tuiles (popup de la tuile), jamais une commande à part.
    appui = _fonction(_tuiles(), "tuile_appui_maison")
    assert "if (long_appui && tuile_roue_ouvrir(r, t, ancre, true)) return;" in appui
    assert "tuile_appui_piece(r, t, long_appui);" in appui
    assert appui.index("tuile_roue_ouvrir(") < appui.index("tuile_appui_piece(")
    for interdit in ("envoyer(", "tab5_action", "homeassistant", "popup_lumiere_ouvrir", "animate_popup_open(g_"):
        assert interdit not in _maison(), interdit


def test_plus_reserve_aux_tuiles_a_appui_long():
    gestes = _fonction(_tuiles(), "tuile_gestes")
    # gestes() (table kGestes, lot L7) écarte l'option r (aucun appui) et les cap / bin sans action.
    assert "agit = gestes(d, type == Type::CLI && clim_tuile_connue(r, t)).agit;" in gestes
    assert "appui_long = agit && type != Type::CAP && type != Type::BIN;" in gestes
    assert "if (d.type >= kNbTypes || (d.options & OPT_R)) return g;" in _fonction(_tuiles(), "gestes")
    table = re.search(r"constexpr GesteType kGestes\[\] = \{(.*?)\n\};", _tuiles(), re.S).group(1)
    agissent = re.findall(r"\{true, [^}]*\},\s*// (\w+)", table)
    assert agissent == ["lum", "int", "vol", "med", "act"], agissent
    # La ligne n'est pressable que si son toucher fait quelque chose.
    assert "cliquable(ligne, agit);" in _fonction(_maison(), "dessiner_ligne")
    # Gestes lus une fois par ligne : disposer() les passe au dessin.
    disposer = _fonction(_maison(), "disposer")
    assert disposer.count("tuile_gestes(") == 1 and "dessiner_ligne(ligne, r, t, agit, appui_long);" in disposer


def test_eteindre_les_lumieres_piece_par_piece():
    corps = _fonction(_maison(), "maison_eteindre_lumieres")
    assert "if (tuiles_piece_a_lumieres(r)) tuiles_piece_eteindre(r);" in corps
    assert "est_lumiere(r, t)" in _fonction(_tuiles(), "tuiles_piece_a_lumieres")
    # Bouton masqué sans lumière.
    assert "ui_hidden(u.eteindre, !lumieres);" in _fonction(_maison(), "disposer")


def test_colonnes_dans_l_ordre_des_pieces_et_geometrie():
    cpp = _maison()
    disposer = _fonction(cpp, "disposer")
    assert "for (int r = 0; r < tuiles_pieces_nombre(); r++)" in disposer, "Pièce 1 → N (R), pas l'ordre des pages"
    assert "kPieceDePage" not in cpp
    # Cinq colonnes au plus par page, réparties (sept pièces : 4 + 3), même largeur partout.
    assert "s_rep = pages_repartir(s_nb, kMaisonColonnes);" in disposer
    assert ("s_colonne = s_rep.par_page > 0 ? (kCarteL - 2 * kMarge - (s_rep.par_page - 1) * kEcart) / s_rep.par_page : 0;"
            in disposer)
    assert "ui_hidden(u.vide, s_nb > 0);" in disposer
    # Carte et corps : tab5_geometrie.h, partagé (lot L5 ; tests/test_geometrie_partagee.py).
    assert '#include "tab5_geometrie.h"' in cpp and "constexpr int32_t kCarteL" not in cpp
    geometrie = geometrie_cpp()
    assert geometrie["kCarteL"] == _jeton("modal_card_w")
    assert geometrie["kCarteH"] == _jeton("modal_card_h")
    assert geometrie["kCorpsY"] == _jeton("modal_body_y")
    ligne = _lire("Tab5", "ui_components", "maison_ligne.yaml")
    # Hauteur du YAML = celle du Tab5 ; disposer() la ramène à la carte (ADR-0061).
    assert f"height: {_constexpr(cpp, 'kLigneHTab5')}" in ligne.split("widgets:", 1)[0]
    assert "hauteur(ligne, kLigneH);" in disposer
    assert re.search(rf"x: {_constexpr(cpp, 'kTexteX')}, y: -15", ligne)
    plus = ligne.split("\n    - button:", 1)[1]
    assert "width: 36" in plus and "x: -12" in plus and "kPlusPlace = 36 + 12 + 8;" in cpp


def test_repeint_seulement_affiche():
    tuiles = _tuiles()
    assert "maison_tuile_changee(r, t);" in _fonction(tuiles, "peindre_tuile")
    assert "maison_definitions_changees();" in _fonction(tuiles, "tuiles_appliquer_ui")
    cpp = _maison()
    assert _fonction(cpp, "maison_tuile_changee").lstrip().startswith("if (!affiche()")
    assert "if (affiche()) disposer();" in _fonction(cpp, "maison_definitions_changees")
    # Même dessin que les cartes du mode HA.
    assert "peindre_vue_sur(v, w, largeur);" in _fonction(tuiles, "tuile_peindre_ligne")


def test_tap_du_titre_de_la_piece_en_mode_ha():
    lvgl = _lire("Tab5", "paquets", "tab5-lvgl.yaml")
    bouton = lvgl.split("id: btn_page_title_tap", 1)[1].split("# Info Wrapper", 1)[0]
    assert "lambda: 'return maison_titre_appui_valide();'" in bouton
    assert "script.execute: tab5_maison_ouvrir" in bouton
    # Appui long : la roue de navigation (ADR-0042), comme les panneaux de la carte.
    assert bouton.split("on_long_press:", 1)[1].strip().startswith("- lambda: 'roue_navigation_ouvrir();'")
    valide = _fonction(_maison(), "maison_titre_appui_valide")
    assert "if (!g_central_ctx.ha_mode && g_central_ctx.forecast_page < kPagesMeteo) return false;" in valide
    # Garde commune « appui au bout d'un glissement » (lot L10) : tab5_internal.h.
    assert "return !ui_appui_glisse();" in valide
    garde = _lire("Tab5", "ecran", "tab5_internal.h").split("inline bool ui_appui_glisse()", 1)[1].split("\n}", 1)[0]
    assert "lv_indev_get_press_moved" in garde and "lv_indev_get_gesture_dir" in garde


def test_glyphes_dans_les_polices():
    polices = font_glyphs(TAB5 / "paquets" / "tab5-styles.yaml")
    assert chr(0xF01D8) in polices["mdi_font_26"], "« ⋯ » (dots-horizontal)"
    assert chr(0xF02DC) in polices["mdi_font_32"], "home, icône de la barre de titre"
    ligne = _lire("Tab5", "ui_components", "maison_ligne.yaml")
    assert '"\\U000F01D8", align: CENTER, text_font: mdi_font_26' in ligne
    assert "maison_icone_*" in MDI_CODE_TARGETS[("tab5_tuiles_icones.h", "")]
    assert "maison_icone_*" in MDI_CODE_TARGETS[("tab5_tuiles.cpp", "heritage_glyphe_carte")]


def test_textes_traduits():
    for code in ("en", "de", "nl", "es", "it", "tr"):
        langue = _lire("Tab5", "lang", f"{code}.yaml")
        for cle in ("Maison", "Éteindre les lumières", "Aucun appareil", "Pièce %d"):
            assert f'"{cle}":' in langue, (code, cle)
    assert 'tr("Pièce %d")' in _fonction(_tuiles(), "tuiles_piece_titre")


def test_pages_de_pieces_par_la_brique():
    """Plus de cinq pièces (ADR-0066) : des pages de cinq colonnes au plus, au glissement de la
    brique commune (ADR-0046) et par les onglets à côté du titre, la page de la pièce du mode
    HA d'abord ; un état repeint la place qui montre sa tuile, sur la page affichée."""
    cpp = _maison()
    ouvrir = _fonction(cpp, "maison_ouvrir")
    assert "pages_brancher(&s_pages);" in ouvrir and "tuiles_piece_montree()" in ouvrir
    assert "LV_EVENT_GESTURE" not in cpp, "le geste : la brique (tab5_pages.cpp)"
    onglets = _fonction(cpp, "peindre_onglets")
    # Une page = un groupe de colonnes (lot A) × une tranche de cinq lignes (lot B) : onglets
    # par groupe (« Pièces a à b », « n/m » gardé entier sur le groupe affiché), ou, avec un
    # seul groupe, par tranche (« 1–5 ») ; aucun sous deux pages.
    assert 'tr("Pièces %d à %d")' in onglets and '"%d\\xE2\\x80\\x93%d"' in onglets
    assert "choix_peindre(u.onglet, n, par_tranche ? s_page : g_affiche);" in onglets
    assert "const int voulus = s_nb_pages >= 2 ? (par_tranche ? s_nb_pages : s_rep.pages) : 0;" in onglets
    assert "texte_ha_coupe(lv_obj_get_child(b, 0), nom, w - 2 * kOngletMarge, sfx);" in onglets
    disposer = _fonction(cpp, "disposer")
    assert "s_tranches[g] = std::max(1, std::min(kMaisonTranchesMax, (lignes + kMaisonLignes - 1) / kMaisonLignes));" \
        in disposer
    assert "for (int i = premiere_ligne; occupee && i < nb_lues[col] && k < kMaisonLignes; i++, k++) {" in disposer
    # Les onglets s'arrêtent avant « Éteindre les lumières » (320 px à 100 px du bord droit).
    assert "constexpr int32_t kOngletsFinMaison = kCarteL - 100 - 320 - 10;" in cpp
    # Le bouton garde ses 320 px : sur une carte plus étroite, les onglets commencent plus tôt
    # (jusqu'à kOngletsDebutTab5) au lieu de rétrécir — « Pièces 1 à 4 » entier sur la 7B.
    assert "constexpr int32_t kOngletsXMin = kOngletsDebutTab5;" in cpp
    assert "std::min<int32_t>(200, (kOngletsFinMaison - kOngletsXMin - (n - 1) * kOngletEcart) / n)" in onglets
    assert "std::min<int32_t>(kOngletsX, kOngletsFinMaison - n * w - (n - 1) * kOngletEcart);" in onglets
    for carte_l, x_attendu, w_attendu in ((1250, 318, 200), (994, 196, 179)):  # Tab5, 7B (kCarteL)
        fin_onglets = carte_l - 102  # onglet_x() de tab5_geometrie.h
        zone = fin_onglets - 196
        x318 = fin_onglets - (830 if zone >= 952 else (830 * zone + 476) // 952)
        fin, n = carte_l - 100 - 320 - 10, 2
        w = min(200, (fin - 196 - (n - 1) * 10) // n)
        assert (min(x318, fin - n * w - (n - 1) * 10), w) == (x_attendu, w_attendu)
    assert "s_place[c][k].r != r || s_place[c][k].t != t" in _fonction(cpp, "peindre_ligne")
    for code in ("en", "de", "nl", "es", "it", "tr"):
        assert '"Pièces %d à %d":' in _lire("Tab5", "lang", f"{code}.yaml"), code


def test_roue_devant_le_popup_maison():
    # La roue est montée avant les popups (tab5-lvgl.yaml) : roue_ouvrir la ramène au premier
    # plan, sinon elle s'ouvrirait derrière le popup Maison. Pas à un repeint (état poussé,
    # thème) : la sonnerie du réveil, au-dessus de tout, resterait dessous.
    roue = _lire("Tab5", "ecran", "tab5_roue.cpp")
    ouvrir = _fonction(roue, "roue_ouvrir")
    assert "const bool repeinte = garder && ouverte();" in ouvrir
    assert "if (!repeinte) lv_obj_move_to_index(u.fond, -1);" in ouvrir
    assert ouvrir.index("lv_obj_move_to_index(u.fond, -1);") < ouvrir.index("ui_hidden(u.fond, false);")


if __name__ == "__main__":
    sys.exit(subprocess.run([sys.executable, "-m", "pytest", "-q", __file__], check=False).returncode)
