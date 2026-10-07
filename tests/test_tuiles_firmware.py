# -*- coding: utf-8 -*-
"""Pièces et tuiles génériques (ADR-0023), côté firmware : Tab5/tab5_tuiles.cpp lit les
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
import sys
from pathlib import Path

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(REPO, "tools"))

from check_tab5_code_rules import font_glyphs  # noqa: E402

ADR = os.path.join(REPO, "docs", "decisions", "0023-rooms-generic-tiles.md")
BLUEPRINT = os.path.join(REPO, "HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")


def _lire(*chemin):
    with open(os.path.join(REPO, *chemin), encoding="utf-8") as f:
        return f.read()


def _cpp():
    return _lire("Tab5", "tab5_tuiles.cpp")


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
    kPieceDePage = [int(x) for x in re.findall(r"\d", _constexpr("kPieceDePage"))]
    assert kPieceDePage == [r for p in range(5) for r, pp in page_de.items() if pp == p]
    # La tablette et le blueprint dans le même ordre que le rendu (tools/rendu/ecrans.py).
    assert "PAGE_DE_LA_PIECE" in _lire("tools", "rendu", "ecrans.py")


def test_champs_gardes_selon_l_adr():
    assert _constexpr("kNom") == "25", "nom : 24 octets au plus, zéro final compris"
    assert _constexpr("kIcone") == "16", "code de palette : [a-z0-9_]{1,15}"
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
    polices = font_glyphs(Path(REPO, "Tab5", "tab5-styles.yaml"))
    assert {ord(c) for c in polices["roboto_32_b"]} == garde


def test_magie_nvs_des_definitions():
    assert int(_constexpr("kMagic"), 16) == int.from_bytes(b"TUI1", "big")
    definir = _fonction(_cpp(), "tuiles_definir")
    # Écrites seulement si elles changent, comparées octet par octet.
    assert definir.index("memcmp") < definir.index("s_pref.save")
    # Chargées là où les zones le sont (zones_apply_ui → tuiles_appliquer_ui → charger).
    assert "tuiles_appliquer_ui();" in _fonction(_lire("Tab5", "tab5_zones.cpp"), "zones_apply_ui")


# ─────────────────────────────────────────────────────────────────────────────
# Commandes émises
# ─────────────────────────────────────────────────────────────────────────────

def test_commandes_par_type_egales_au_tableau_de_l_adr():
    cpp = _cpp()
    adr = _commandes_de_l_adr()
    types = _types_de_l_adr()
    appui = _fonction(cpp, "tuile_appui_piece")
    # Toute commande de tuile émise est dans le tableau « What the tablet sends ».
    emises = set(re.findall(r'action = [^;]*?"(\w+)"', appui)) | set(re.findall(r'\? "(\w+)" : "(\w+)"', appui)[0])
    emises |= set(re.findall(r'return "(\w+)"', _fonction(cpp, "vol_appui")))
    emises |= set(re.findall(r'"(ouvrir|fermer|arreter)"', _fonction(cpp, "vol_appui_long")))
    assert emises and emises <= adr, emises - adr
    # lum / int / med : basculer, allumer avec l'option o ; act : lancer.
    for t in ("lum", "int", "med"):
        assert "`basculer`" in types[t][0]
    assert '(d.options & OPT_O) ? "allumer" : "basculer"' in appui
    assert "`lancer`" in types["act"][0] and 'action = "lancer";' in appui
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
    agit = _fonction(cpp, "type_agit")
    assert "case Type::CLI: return (options & OPT_M) != 0 || clim_connue;" in agit
    assert "default: return false;" in agit and "OPT_R" in agit
    assert "if (d.options & OPT_T) ouvrir_popup(g_tuiles_ui.popup_tv);" in appui
    # Option k : un second appui dans les 3 s.
    assert "kConfirmationMs = 3000" in cpp and "OPT_K" in appui


def test_tout_eteindre_de_la_piece():
    assert "pR` + `eteindre`" in _lire(ADR)
    corps = _fonction(_cpp(), "popup_lumiere_tout_eteindre")
    assert "{'p', static_cast<char>('0' + s_pl.piece), '\\0'}" in corps and 'envoyer(cle, "eteindre")' in corps
    assert "popup_lumiere_tout_eteindre();" in _lire("Tab5", "ui_components", "light_popup.yaml")


def test_cles_des_commandes_de_tuile():
    cpp = _cpp()
    assert "{'t', static_cast<char>('0' + r), static_cast<char>('0' + t), '\\0'}" in _fonction(cpp, "envoyer_tuile")
    # L'événement esphome.tab5_action du script tab5_action, emplacement / action / valeur.
    tuiles_yaml = _lire("Tab5", "tab5-tuiles.yaml")
    assert "id(tab5_action).execute(std::string(e), std::string(a), std::string(v));" in tuiles_yaml
    scripts = _lire("Tab5", "tab5-scripts.yaml").split("- id: tab5_action", 1)[1].split("- id:", 1)[0]
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
    assert "u.volet_tap = []() { id(tab5_volet_tap).execute(); };" in _lire("Tab5", "tab5-tuiles.yaml")


# ─────────────────────────────────────────────────────────────────────────────
# Protocole : action, routage, version
# ─────────────────────────────────────────────────────────────────────────────

def _action(nom):
    api = _lire("Tab5", "tab5-api-logic.yaml")
    return api.split(f"- service: {nom}\n", 1)[1].split("\n    - service:", 1)[0].split("\nprovisioning:", 1)[0]


def test_action_tab5_maj_tuiles_et_son_exemple():
    bloc = _action("tab5_maj_tuiles")
    assert "tuiles_definir(payload);" in bloc
    exemple = re.search(r'example: "([^"]+)"', bloc).group(1)
    types = set(_types_de_l_adr())
    lettres = set("dcoktrm")
    for entree in filter(None, exemple.split(";")):
        champs = entree.split("|")
        if re.fullmatch(r"p[0-4]|hp|hd", champs[0]):
            assert len(champs) == 2, entree
            continue
        # Rangée sous l'horloge (ADR-0031) : la classe d'appareil en septième champ.
        if re.fullmatch(r"h[0-2][0-3]", champs[0]):
            assert len(champs) == 7 and re.fullmatch(r"[a-z0-9_]{0,15}", champs[6]), entree
            champs = champs[:6]
        assert re.fullmatch(r"[th][0-4][0-4]", champs[0]) and len(champs) == 6, entree
        assert champs[1] in types and re.fullmatch(r"[a-z0-9_]{0,15}", champs[2]), entree
        assert set(champs[3]) <= lettres, entree


def test_etats_routes_avant_les_emplacements_3x():
    corps = _fonction(_lire("Tab5", "tab5_zones.cpp"), "emplacements_appliquer")
    assert corps.index("tuiles_etat_recu(") < corps.index("for (size_t i = 0; i < n; i++)")
    recu = _fonction(_cpp(), "tuiles_etat_recu")
    # « tRT » (ou « hLI », rangée sous l'horloge, ADR-0031) : trois caractères, R et T de
    # 0 à 4 ; puis état | valeur | couleur (6 hex), lus par etat_lire pour les deux.
    assert "if (n_cle != 3) return false;" in recu and "cle[0] != 't'" in recu and "cle[0] == 'h'" in recu
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
    for fichier in ("forecast_daily.yaml", "switches_card.yaml"):
        texte = _lire("Tab5", "ui_components", fichier)
        assert re.findall(r"tuile_appui\((\d), false\)", texte) == list("01234"), fichier
        assert re.findall(r"tuile_appui\((\d), true\)", texte) == list("01234"), fichier
    # Plus aucune commande 3.x écrite en dur sur une tuile : tout passe par tuile_appui().
    assert "id: tab5_action" not in _lire("Tab5", "ui_components", "forecast_daily.yaml")
    assert "id: tab5_action" not in _lire("Tab5", "ui_components", "switches_card.yaml")


def test_widgets_poses_sans_toucher_a_l_on_boot():
    tuiles = _lire("Tab5", "tab5-tuiles.yaml")
    for t in range(5):
        assert f"u.heure_g[{t}] = id(icon_card_h{4 - t}_g);" in tuiles
        assert f"u.heure_bouton[{t}] = id(btn_h{4 - t}_action);" in tuiles
        assert f"u.carte_nom[{t}] = id(lbl_sw{t}_title);" in tuiles
        assert f"u.lum_sel[{t}] = id(btn_light_sel_{t});" in tuiles
    zones = _lire("Tab5", "tab5-zones.yaml").split("- id: tab5_zones_apply", 1)[1]
    assert zones.index("script.execute: tab5_tuiles_ui") < zones.index("zones_apply_ui();")
    on_boot = _lire("tab5-ha-hmi.yaml").split("  on_boot:", 1)[1].split("\npackages:", 1)[0]
    assert "tuile" not in on_boot and "ha_mode" not in on_boot
    assert "tab5_tuiles: !include Tab5/tab5-tuiles.yaml" in _lire("tab5-ha-hmi.yaml")


def test_cartes_du_mode_ha_facon_carte_tile():
    """Carte « tile » de HA (06/10/2026, discussion #278) : l'icône dans une pastille
    ronde de la couleur de l'état, le bouton sur la pastille, le nom dans un cadre
    cliquable (sens d'un volet), sans onglet."""
    tuiles = _lire("Tab5", "tab5-tuiles.yaml")
    carte = _lire("Tab5", "ui_components", "switches_card.yaml")
    for t in range(5):
        assert f"u.carte_pastille[{t}] = id(sw_pastille_{t});" in tuiles
        pastille = carte.split(f"id: sw_pastille_{t}\n", 1)[1].split("- button:", 1)[0]
        assert f"id: icon_sw{t}," in pastille and "clickable: false" in pastille
        bouton = carte.split(f"id: btn_sw{t}_action\n", 1)[1].split("!include", 1)[0]
        assert f"tuile_appui({t}, false);" in bouton and f"tuile_appui({t}, true);" in bouton
    assert "ui_fond(u.carte_pastille[t], v.couleur_carte);" in _fonction(_cpp(), "peindre_carte")
    titre = _lire("Tab5", "ui_components", "switch_card_title_tab.yaml")
    assert titre.split("\nobj:", 1)[1].count("widgets:") == 1, "le nom garde son cadre (parent cliquable)"


def test_mode_ha_seule_source_et_swipe_par_piece():
    assert "show_switches" not in _lire("Tab5", "tab5-globals.yaml").split("globals:", 1)[1].split("#", 1)[0]
    central = _lire("Tab5", "tab5_central.cpp")
    swipe = _fonction(central, "handle_swipe_gesture")
    # En mode HA, le swipe change de pièce et ne passe jamais par apply_forecast_page
    # (qui réaffichait le calque météo sous les cartes).
    assert swipe.index("if (ctx.ha_mode)") < swipe.index("apply_forecast_page(")
    assert "!ctx.ha_mode" in _fonction(central, "rotator_owns_card")
    assert "if (g_central_ctx.ha_mode) return;" in _lire("Tab5", "tab5-scripts.yaml")
    assert "if (e == Ecran::ACCUEIL) tuiles_mode_ha(false);" in _lire("Tab5", "tab5-ha-controls.yaml")
    assert "tuiles_mode_ha(!g_central_ctx.ha_mode);" in _lire("Tab5", "tab5-lvgl.yaml")


# ─────────────────────────────────────────────────────────────────────────────
# Popup du volet (05/10/2026, discussion #278)
# ─────────────────────────────────────────────────────────────────────────────

def test_appui_long_d_un_volet_ouvre_son_popup_sauf_avec_k():
    """L'appui long d'une tuile vol ouvre le popup du volet ; avec l'option k, l'ancien
    appui long (l'autre sens, confirmé) : le popup ne contourne jamais la confirmation.
    L'option r n'arrive pas jusque-là (type_agit), le mode héritage non plus."""
    appui = _fonction(_cpp(), "tuile_appui_piece")
    vol = appui.split("case Type::VOL:", 1)[1].split("case Type::MED:", 1)[0]
    assert "if (long_appui && !(d.options & OPT_K)) {" in vol and "popup_volet_ouvrir(r, t);" in vol
    assert vol.index("popup_volet_ouvrir(r, t);") < vol.index("action = long_appui ? vol_appui_long(e) : vol_appui(e);")
    assert appui.index("appui_heritage(t, long_appui);") < appui.index("case Type::VOL:")
    assert appui.index("if (!type_agit(") < appui.index("case Type::VOL:")
    long_adr = _types_de_l_adr()["vol"][1]
    assert "shutter popup" in long_adr and "with `k`" in long_adr


def test_commandes_du_popup_du_volet_dans_le_contrat():
    cpp = _cpp()
    adr = _commandes_de_l_adr()
    # Boutons : les commandes de tuile d'un volet, rien d'autre.
    popup = _lire("Tab5", "ui_components", "volet_popup.yaml")
    boutons = re.findall(r"file: volet_btn\.yaml, vars: \{[^}]*commande: (\w+)", popup)
    assert sorted(boutons) == ["arreter", "fermer", "ouvrir"]
    assert "popup_volet_commande(\"${commande}\");" in _lire("Tab5", "ui_components", "volet_btn.yaml")
    assert "envoyer_tuile(s_pv.piece, s_pv.tuile, action);" in _fonction(cpp, "popup_volet_commande")
    # Volet dessiné : « position » (dans le tableau de l'ADR), 0-100, à la tuile du popup,
    # et jamais sans position connue (même si elle s'est perdue pendant le geste).
    envoi = _fonction(cpp, "popup_volet_envoyer_position")
    assert 'u.envoyer(cle, "position", valeur);' in envoi and "position" in adr
    assert 'snprintf(valeur, sizeof(valeur), "%d", std::clamp(s_pv.pos, 0, 100));' in envoi
    garde = "if (!vol_position_connue(s_etats[s_pv.piece][s_pv.tuile])) return false;"
    assert garde in envoi and envoi.index(garde) < envoi.index("u.envoyer(")
    # « position » ne part que de là et du bouton « 50 % » de la roue d'actions rapides
    # (ADR-0036), gardé de la même façon ; la fonction n'a qu'un appelant : le relâcher.
    assert cpp.count('"position"') == 2
    roue = _fonction(cpp, "roue_tuile_choisir")
    assert 'if (vol_position_connue(s_etats[rt.r][rt.t]) && u.envoyer != nullptr) u.envoyer(cle, "position", valeur);' in roue
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
    assert envoi.index('u.envoyer(cle, "position", valeur);') < envoi.index("s_pv.cible = true;")
    recu = _fonction(cpp, "tuiles_etat_recu")
    assert recu.index("if (r == s_pv.piece && t == s_pv.tuile) s_pv.cible = false;") < recu.index("peindre_tuile(r, t);")
    assert cpp.count("s_pv.cible = true;") == 1 and cpp.count("s_pv.cible = false;") == 1
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
    h = int(_constexpr("kVoletFenetreH"))
    lame = int(_constexpr("kVoletLameH"))
    assert h % lame == 0
    for ident in ("volet_fenetre", "volet_tablier"):
        bloc = popup.split(f"id: {ident}\n", 1)[1].split("widgets:", 1)[0]
        assert re.search(rf"\n\s+height: {h}\n", bloc), ident
        assert "clickable: false" in bloc, ident
        assert "scrollable: false" in bloc, ident
    cadre = popup.split("id: volet_cadre\n", 1)[1].split("widgets:", 1)[0]
    assert "clickable: false" not in cadre
    # Le tablier remplit la fenêtre (même largeur), qui le rogne.
    largeurs = [re.search(r"\n\s+width: (\d+)\n", popup.split(f"id: {i}\n", 1)[1]).group(1)
                for i in ("volet_fenetre", "volet_tablier")]
    assert largeurs[0] == largeurs[1]
    # Pastille des boutons : pas cliquable, sinon elle prend l'appui du bouton.
    bouton = _lire("Tab5", "ui_components", "volet_btn.yaml")
    assert bouton.split("- obj:", 1)[1].count("clickable: false") == 1


def test_popup_du_volet_inscrit_et_branche():
    scripts = _lire("Tab5", "tab5-scripts.yaml")
    assert re.search(r'ModalRegistry::add\(id\(volet_popup\),\s+"Volet",\s+ModalRegistry::POPUP\);', scripts)
    assert "- !include ui_components/volet_popup.yaml" in _lire("Tab5", "tab5-lvgl.yaml")
    tuiles = _lire("Tab5", "tab5-tuiles.yaml")
    for champ, widget in (("vol_popup", "volet_popup"), ("vol_titre", "volet_popup_titre"),
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
    """L'appui long d'une tuile int, act, ou med sans l'option t (avec t : la télécommande)
    ouvre le popup de l'appareil ; l'appui court ne change pas. Lecture seule (option r) :
    type_agit coupe avant, comme le mode météo sans appareils."""
    appui = _fonction(_cpp(), "tuile_appui_piece")
    for type_, suivant in (("INT", "VOL"), ("ACT", "CLI")):
        bloc = _cas(appui, type_, suivant)
        assert "if (long_appui) {\n                popup_appareil_ouvrir(r, t);\n                return;" in bloc, type_
        assert bloc.index("popup_appareil_ouvrir(r, t);") < bloc.index("action = "), type_
    med = _cas(appui, "MED", "ACT")
    assert "if (d.options & OPT_T) ouvrir_popup(g_tuiles_ui.popup_tv);\n                else popup_appareil_ouvrir(r, t);" in med
    # Les appuis courts d'aujourd'hui : basculer (allumer avec o), lancer.
    assert 'action = (d.options & OPT_O) ? "allumer" : "basculer";' in _cas(appui, "INT", "VOL")
    assert 'action = "lancer";' in _cas(appui, "ACT", "CLI")
    assert appui.index("if (!type_agit(") < appui.index("case Type::INT:")
    assert "case Type::INT: case Type::ACT: return true;" in _fonction(_cpp(), "a_popup_appareil")
    assert "return (options & OPT_T) == 0;" in _fonction(_cpp(), "a_popup_appareil")
    # Le tableau de l'ADR le dit aussi.
    types = _types_de_l_adr()
    for t in ("int", "act", "med"):
        assert "device popup" in types[t][1], t
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
    assert "heritage()" in valide and "!(d.options & OPT_R)" in valide and "a_popup_appareil(" in valide
    # Popup refermé quand sa tuile ne l'a plus, repeint sinon (et au changement de thème).
    definir = _fonction(cpp, "tuiles_definir")
    assert "else animate_popup_close(g_tuiles_ui.app_popup);" in definir
    assert "if (popup_appareil_ouvert()) popup_appareil_peindre();" in _fonction(cpp, "tuiles_rejouer_theme")
    # Le popup dit l'option k et l'option o, et ce que fera l'appui.
    peindre = _fonction(cpp, "popup_appareil_peindre")
    assert "minuterie_sur(s_confirmation, r, t)" in peindre and "OPT_K" in peindre and "OPT_O" in peindre


def test_popup_d_un_appareil_inscrit_et_branche():
    scripts = _lire("Tab5", "tab5-scripts.yaml")
    assert re.search(r'ModalRegistry::add\(id\(appareil_popup\),\s+"Appareil",\s+ModalRegistry::POPUP\);', scripts)
    assert "- !include ui_components/appareil_popup.yaml" in _lire("Tab5", "tab5-lvgl.yaml")
    tuiles = _lire("Tab5", "tab5-tuiles.yaml")
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
