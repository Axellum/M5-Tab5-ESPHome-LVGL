# -*- coding: utf-8 -*-
"""Panneau « Ok Nabu » à lignes et défilement au choix (lot 3, 09/10/2026, ADR-0041,
demande d'Axel).

Le cadre « Ok Nabu » de l'accueil devient une zone à lignes comme la rangée sous l'horloge
(ADR-0031) : même modèle, même dessin, même format de clés (n… au lieu de h…), la ligne
d'écoute à la place de celle des plantes. Le tap court des heures (« auto ») passe à sa
ligne suivante (code nabu_suivant, ADR-0039). La rangée, le panneau et la tuile − / +
défilent au choix (« auto » calé sur la carte centrale, ou « fixe »), par une clé de
tab5_maj_emplacements : le contrat des services ne change pas.

Aucun compilateur ne relie le firmware, le blueprint, la démo et le rendu ; ce fichier le
fait :
- les clés et la NVS : lettre n, préférence à part, la rangée garde la sienne ;
- le tap du cadre : le mot de réveil sur la ligne d'écoute seulement ;
- le défilement : clé defil, défauts, bornes, NVS, et un blueprint sans elle ;
- la tuile − / + en auto : jamais la NVS, jamais sous le doigt ;
- la géométrie : les panneaux dans le cadre, l'arrondi intérieur du cadre dans les
  21 thèmes, les pastilles entre le cadre et la carte centrale ;
- le blueprint rendu (harnais de tests/test_tuiles_blueprint.py), la démo et le rendu."""
import math
import os
import re

import pytest
import yaml

from tests import test_tuiles_blueprint as bp
from tests.commun import lire as _lire

import ecrans  # noqa: E402
import scenarios  # noqa: E402

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


def _constante(source, nom):
    m = re.search(rf"constexpr [^=;]*?\b{nom}\b\s*=\s*([^;]+);", source)
    assert m, f"constexpr {nom} introuvable"
    return m.group(1).strip()


def _entier(source, nom):
    return int(_constante(source, nom), 0)


def _fonction(source, signature):
    return source.split(signature, 1)[1].split("\n}\n", 1)[0]


TUILES_CPP = _lire("Tab5", "ecran", "tab5_tuiles.cpp")
RANGEE_CPP = _lire("Tab5", "ecran", "tab5_rangee.cpp")
REGLABLES_CPP = _lire("Tab5", "ecran", "tab5_reglables.cpp")
ZONES_CPP = _lire("Tab5", "ecran", "tab5_zones.cpp")
INTERNAL_H = _lire("Tab5", "ecran", "tab5_internal.h")
LVGL = _lire("Tab5", "paquets", "tab5-lvgl.yaml")
TOUR = _entier(INTERNAL_H, "kTourCentralS")


def _entrees():
    return bp._entrees(bp._blueprint())


# ─────────────────────────────────────────────────────────────────────────────
# Firmware : clés, NVS, tap
# ─────────────────────────────────────────────────────────────────────────────

def test_une_lettre_et_une_preference_par_zone():
    enum = re.search(r"enum RangeeZone : uint8_t \{([^}]*)\};", INTERNAL_H)
    assert enum and re.findall(r"\w+", enum.group(1)) == ["RANGEE_HORLOGE", "RANGEE_NABU", "RANGEE_NB"]
    assert "kLettreZone[RANGEE_NB] = {'h', 'n'};" in TUILES_CPP
    magics = re.search(r"kMagicRangee\[RANGEE_NB\] = \{(0x[0-9A-F]+), (0x[0-9A-F]+)\};", TUILES_CPP).groups()
    cles = re.search(r"kPrefKeyRangee\[RANGEE_NB\] = \{(0x[0-9A-F]+), (0x[0-9A-F]+)\};", TUILES_CPP).groups()
    # La rangée garde « RAN1 » / « rang » (NVS des tablettes déjà installées) ; le panneau
    # a les siens, « NAB1 » / « nabu ».
    assert [bytes.fromhex(m[2:]).decode() for m in magics] == ["RAN1", "NAB1"]
    assert [bytes.fromhex(c[2:]).decode() for c in cles] == ["rang", "nabu"]
    # Aucune autre préférence du firmware n'a la clé « nabu » ni « defl ».
    for cle in ("0x6E616275", "0x6465666C"):
        porteurs = [f for f in _sources_cpp() if cle in _lire(*f)]
        assert len(porteurs) == 1, (cle, porteurs)


def _sources_cpp():
    dossier = os.path.join(REPO, "Tab5")
    for racine, _, fichiers in os.walk(dossier):
        for f in fichiers:
            if f.endswith((".cpp", ".h")):
                yield tuple(os.path.relpath(os.path.join(racine, f), REPO).split(os.sep))


def test_les_cles_n_vont_au_panneau():
    zone = _fonction(TUILES_CPP, "int zone_de_lettre(char c)")
    assert "kLettreZone[z] == c" in zone
    lire = _fonction(TUILES_CPP, "void lire_entree(")
    # « pR » (pièce) avant la lettre de zone : « np » / « nd » ne sont pas des pièces.
    assert lire.index("f[0].p[0] == 'p'") < lire.index("zone_de_lettre(f[0].p[0])")
    recu = _fonction(TUILES_CPP, "bool tuiles_etat_recu(")
    assert "rangee_element_change(z, r, t);" in recu


def test_sans_cle_n_l_ecoute_seule():
    """Un instantané sans clé n… (blueprint d'avant le lot 3) : la ligne d'écoute en
    premier, aucune ligne de capteurs — l'écran d'avant."""
    vide = _fonction(TUILES_CPP, "void rangee_vide(")
    # Valeur initialisée : plantes (la place de la ligne spéciale) à 0, la première.
    assert "g = ModeleRangee{};" in vide and "int8_t plantes;" in TUILES_CPP
    assert "g.tours = kToursDefaut;" in vide
    assert "speciale_disponible(const Defileur& d) { return d.z == RANGEE_NABU" in RANGEE_CPP


def test_le_tap_du_cadre_ne_bascule_l_ecoute_que_sur_sa_ligne():
    bouton = LVGL.split("id: btn_ok_nabu", 1)[1].split("\n        - ", 1)[0]
    clic = bouton.split("on_short_click:", 1)[1].split("widgets:", 1)[0]
    assert "lambda: 'return nabu_ecoute_affichee();'" in clic
    assert clic.index("nabu_ecoute_affichee") < clic.index("switch.toggle: tab5_wake_word_active")
    affichee = _fonction(RANGEE_CPP, "bool nabu_ecoute_affichee()")
    # Avant le premier dessin, le cadre montre l'écoute (le tap la bascule, comme avant).
    assert "if (!d.pret) return true;" in affichee
    assert "d.ordre[d.courante] == kSpeciale" in affichee
    # Le geste « Écoute » reste, quelle que soit la ligne (ADR-0039).
    nav = _lire("Tab5", "paquets", "tab5-navigation.yaml")
    assert "nabu_suivant();" in nav.split("GesteAction::NABU_SUIVANTE)", 1)[1].split("}", 1)[0]


def test_panneaux_dans_le_cadre_et_non_cliquables():
    bouton = LVGL.split("id: btn_ok_nabu", 1)[1].split("\n        # Pastilles", 1)[0]
    w, h = (int(re.search(rf"^            {c}: (\d+)$", bouton, re.M).group(1)) for c in ("width", "height"))
    assert (w, h) == (405, 90)
    assert "pad_all: 0" in bouton and "scrollable: false" in bouton
    ecoute = bouton.split("id: nabu_ecoute", 1)[1].split("- label:", 1)[0]
    assert "width: 401" in ecoute and "height: 88" in ecoute and "clickable: false" in ecoute
    for ident, el in (("nabu_a", "na"), ("nabu_b", "nb")):
        assert (f'file: ../ui_components/rangee_panneau.yaml, vars: {{ id: {ident}, el: "{el}", '
                f'hauteur: "88" }}') in bouton
    panneau = _lire("Tab5", "ui_components", "rangee_panneau.yaml")
    assert "width: 401" in panneau and "clickable: false" in panneau
    assert "clickable: false" in _lire("Tab5", "ui_components", "rangee_element.yaml")
    # 401 = kLargeur du dessin (tab5_rangee.cpp) : la place mesurée est celle du panneau.
    assert _entier(RANGEE_CPP, "kLargeur") == 401
    # tab5-rangee.yaml pose les widgets du panneau : le cadre est la zone et le toucher.
    ui = _lire("Tab5", "paquets", "tab5-rangee.yaml")
    assert "n.zone = id(btn_ok_nabu);" in ui and "n.panneau_special = id(nabu_ecoute);" in ui
    assert "n.toucher" not in ui
    for p in ("a", "b"):
        for i in range(4):
            assert f"id(rangee_el_n{p}{i})" in ui and f"id(rangee_icone_n{p}{i})" in ui
            assert f"id(rangee_texte_n{p}{i})" in ui


# ─────────────────────────────────────────────────────────────────────────────
# Géométrie : l'arrondi du cadre dans les 21 thèmes, les pastilles
# ─────────────────────────────────────────────────────────────────────────────

def _formes_du_cadre():
    """{fichier du thème: (rayon, bordure)} de style_clim_btn_page (tab5_theme.cpp) : la
    valeur de base, ou celle du thème en sombre (le clair a les mêmes formes)."""
    import gen_themes
    theme = _lire("Tab5", "ecran", "tab5_theme.cpp")
    base = {}
    par_theme = {}
    for prop, valeur, suite in re.findall(
            r"\{8, LV_STYLE_(RADIUS|BORDER_WIDTH), FORME_NOMBRE, (\d+)\},  // style_clim_btn_page(.*)$",
            theme, re.M):
        m = re.fullmatch(r" (\w+) \((sombre|clair)\)", suite)
        if not suite:
            base[prop] = int(valeur)
        elif m:
            par_theme.setdefault((m.group(1), m.group(2)), {})[prop] = int(valeur)
    assert set(base) == {"RADIUS", "BORDER_WIDTH"}
    formes = {}
    for t in gen_themes.charger():
        for mode in ("sombre", "clair"):
            f = {**base, **par_theme.get((t.fichier, mode), {})}
            formes[(t.fichier, mode)] = (f["RADIUS"], f["BORDER_WIDTH"])
    assert len(formes) == 42
    return formes


def _dans_l_arrondi(x, y, w, h, rayon, bordure):
    """Le point (x, y) est-il dans l'intérieur du cadre w × h (bordure retirée), aux coins
    arrondis de `rayon` (LVGL borne le rayon à la moitié du petit côté) ?"""
    r = max(0, min(rayon, min(w, h) // 2) - bordure)
    gx, gy, dx, dy = bordure, bordure, w - bordure, h - bordure
    if not (gx <= x <= dx and gy <= y <= dy):
        return False
    cx = min(max(x, gx + r), dx - r)
    cy = min(max(y, gy + r), dy - r)
    return math.hypot(x - cx, y - cy) <= r


def test_les_elements_restent_dans_l_arrondi_du_cadre_dans_chaque_theme():
    """Boîte des éléments au pire, dans le cadre de 405 × 90 :
    - icônes seules (70 px, quatre) : écart SPACE_EVENLY de (401 − 4 × 70) / 5 ;
    - avec valeurs : le premier élément à kMargeCadre du bord du panneau, une ligne de
      texte de 45 px comptée 54 px de haut (interligne), l'icône de 45 px.
    Chaque coin de ces boîtes reste dans l'arrondi intérieur du cadre (bordure retirée),
    dans les 21 thèmes et les deux modes (Capsule : gélule de rayon 45, bordure de 4 px)."""
    w, h, panneau_w, panneau_h = 405, 90, 401, 88
    marge = _entier(RANGEE_CPP, "kMargeCadre")
    assert "{RANGEE_NABU, Defilement::NABU, g_nabu_ui, kMargeCadre}" in RANGEE_CPP
    assert "kLargeur - (n + 1) * d.marge" in RANGEE_CPP
    x0 = (w - panneau_w) // 2
    y0 = (h - panneau_h) // 2
    ecart_icones = (panneau_w - 4 * 70) / 5
    boites = [
        (x0 + max(marge, ecart_icones), y0 + (panneau_h - 70) / 2, 70, 70),   # icône seule, à gauche
        (x0 + marge, (h - 45) / 2, 45, 45),                                   # icône d'une valeur
        (w - x0 - marge - 60, (h - 54) / 2, 60, 54),                          # texte, à droite
    ]
    for (fichier, mode), (rayon, bordure) in _formes_du_cadre().items():
        for bx, by, bw, bh in boites:
            for px, py in ((bx, by), (bx + bw, by), (bx, by + bh), (bx + bw, by + bh)):
                assert _dans_l_arrondi(px, py, w, h, rayon, bordure), (fichier, mode, (px, py))


def test_pastilles_entre_le_cadre_et_la_carte_centrale():
    gabarit = _lire("Tab5", "ui_components", "rangee_pastilles.yaml")
    y = int(re.search(r"^  y: (\d+)$", gabarit, re.M).group(1))
    hauteur = max(int(v) for v in re.findall(r"height: (\d+), radius", gabarit))
    bouton = LVGL.split("id: btn_ok_nabu", 1)[1]
    bas_du_cadre = 218 + 90
    assert re.search(r"^            y: 218$", bouton, re.M) and re.search(r"^            height: 90$", bouton, re.M)
    carte = LVGL.split("id: central_card", 1)[1]
    haut_de_la_carte = int(re.search(r"^            y: (\d+)$", carte, re.M).group(1))
    assert bas_du_cadre < y and y + hauteur < haut_de_la_carte
    # Le même gabarit pour les deux zones, centré sous chacune.
    assert ('rangee_pastilles.yaml, vars: { nom: nabu, align: TOP_LEFT, x: "20", largeur: "405" }') in LVGL
    assert ('rangee_pastilles.yaml, vars: { nom: rangee, align: TOP_MID, x: "0", largeur: SIZE_CONTENT }') in LVGL
    assert "clickable: false" in gabarit


# ─────────────────────────────────────────────────────────────────────────────
# Défilement au choix : firmware
# ─────────────────────────────────────────────────────────────────────────────

def test_defauts_et_bornes_du_defilement_egaux_au_blueprint():
    entrees = _entrees()
    assert "kAutoDefaut[kNbDefil] = {true, false, false};" in RANGEE_CPP
    assert [entrees[n]["default"] for n in ("rangee_defilement", "nabu_defilement", "reglables_defilement")] == \
        ["auto", "fixe", "fixe"]
    for nom in ("rangee_defilement", "nabu_defilement", "reglables_defilement"):
        valeurs = [o["value"] for o in entrees[nom]["selector"]["select"]["options"]]
        assert valeurs == ["auto", "fixe"]
    assert 'kCleAuto[] = "auto";' in RANGEE_CPP and 'kCleFixe[] = "fixe";' in RANGEE_CPP
    duree = entrees["reglables_duree"]
    nombre = duree["selector"]["number"]
    assert duree["default"] == TOUR * _entier(RANGEE_CPP, "kToursClimDefaut")
    assert nombre["min"] == nombre["step"] == TOUR and nombre["max"] == TOUR * _entier(RANGEE_CPP, "kToursClimMax")
    assert "(s + kTourCentralS / 2) / kTourCentralS" in _fonction(RANGEE_CPP, "void defilement_recu(")
    # Le panneau : mêmes bornes que la rangée (même modèle).
    for nom in ("min", "max", "step"):
        assert entrees["nabu_duree"]["selector"]["number"][nom] == entrees["rangee_duree"]["selector"]["number"][nom]
    assert entrees["nabu_duree"]["default"] == entrees["rangee_duree"]["default"]
    assert entrees["nabu_ecoute"]["default"] == "0"
    assert [o["value"] for o in entrees["nabu_ecoute"]["selector"]["select"]["options"]] == ["0", "1", "2", "masquee"]


def test_cle_defil_lue_avec_les_gestes():
    assert 'kCleDefilement[] = "defil";' in ZONES_CPP
    appliquer = _fonction(ZONES_CPP, "int emplacements_appliquer(")
    assert "defilement_recu(" in appliquer
    fin = _fonction(ZONES_CPP, "void gestes_fin_payload(")
    # Un blueprint 3.8 envoie gestes sans defil seulement s'il est d'avant le lot 3 : les
    # défauts reviennent (l'écran d'avant) ; un payload sans gestes ne touche à rien.
    assert "(s_appuis_vue || s_gestes_vue) && !s_defil_vue" in fin and "defilement_defaut();" in fin
    assert "s_defil_vue = false;" in fin


def test_nvs_du_defilement():
    assert re.search(r"struct SauvegardeDefil \{\s+uint32_t magic;\s+uint8_t auto_\[kNbDefil\];\s+"
                     r"uint8_t tours_clim;\s+\};", RANGEE_CPP)
    assert bytes.fromhex(_constante(RANGEE_CPP, "kMagicDefil")[2:]).decode() == "DEF1"
    assert bytes.fromhex(_constante(RANGEE_CPP, "kPrefKeyDefil")[2:]).decode() == "defl"
    garder = _fonction(RANGEE_CPP, "void defil_garder(")
    # Écrit seulement si quelque chose change (la clé part à chaque connexion).
    assert garder.index("return;") < garder.index("s_defil.pref.save(&s);")
    assert "std::memset(&s, 0, sizeof(s));" in garder


def test_un_geste_n_est_pas_ecrase_aussitot():
    """En auto, une zone change toutes les N tours ; un geste remet son compte à zéro."""
    suivante = _fonction(RANGEE_CPP, "void suivante(Defileur& d)")
    assert "d.tours = 0;" in suivante
    tour = _fonction(RANGEE_CPP, "void tour(Defileur& d)")
    assert "defilement_auto(d.defilement)" in tour and "rangee_tours(d.z)" in tour
    # Tuile − / + : chaque geste remet le compte (pas, liste, choix).
    for signature in ("void reglables_pas(int sens)", "void reglables_liste_basculer()", "void reglables_choisir(int ligne)"):
        assert "s_tours = 0;" in _fonction(REGLABLES_CPP, signature), signature


def test_la_tuile_en_auto_ne_touche_ni_la_nvs_ni_l_appareil_qu_on_regle():
    tour = _fonction(REGLABLES_CPP, "void reglables_tour()")
    garde = tour.split("{", 1)[1].split("s_tours = 0;", 1)[0]
    for condition in ("!defilement_auto(Defilement::CLIM)", "!tuile_visible()", "liste_ouverte()",
                      "s_attente.cle[0] != '\\0'"):
        assert condition in garde, condition
    assert "save(" not in tour and "envoyer" not in tour
    assert "l[j].sorte == Sorte::TABLETTE" in tour, "le volume de la tablette se choisit à la main"
    assert "if (relais < 2) return;" in tour
    # Le choix d'un geste reste en NVS : écrit seulement s'il diffère de celui gardé.
    choisir = _fonction(REGLABLES_CPP, "void reglables_choisir(int ligne)")
    assert "if (id != s_choix_sauve)" in choisir and "s_pref_choix.save(&c);" in choisir
    tour_rangee = _fonction(RANGEE_CPP, "void rangee_tour()")
    assert "reglables_tour();" in tour_rangee


# ─────────────────────────────────────────────────────────────────────────────
# Blueprint rendu
# ─────────────────────────────────────────────────────────────────────────────

NABU = {
    # Ligne 1 vide ; ligne 2 : une scène (action, sautée), un capteur, une porte.
    "nabu_ligne_2": ["scene.soiree", "sensor.temp_salon", "binary_sensor.porte"],
    "nabu_ligne_3": ["switch.prise_pc"],
    "nabu_ecoute": "1",
    "nabu_duree": 24,
}


def _passage(trigger=None, entrees=None, etats=None):
    return bp._passage(trigger, {**bp.ENTREES, **NABU} if entrees is None else entrees,
                       bp._maison() if etats is None else etats)


def test_definitions_du_panneau_au_format_de_la_rangee():
    p = _passage()
    defs = bp._defs(p.definitions())
    nabu = [d for d in defs if d[0][0] == "n"]
    icone = bp._icone
    assert nabu == [
        ["np", "1"],
        ["nd", "24"],
        ["n10", "cap", icone(p.bp, domaine="sensor", classe="temperature"), "", "°C", "Température salon",
         "temperature"],
        ["n11", "bin", icone(p.bp, domaine="binary_sensor", classe="door"), "", "door", "Porte d'entrée", "door"],
        ["n20", "int", icone(p.bp, "mdi:icone-hors-palette", "switch"), "k", "", "Prise PC", ""],
    ]
    # Après la rangée, avant la tuile − / + ; la rangée ne change pas.
    cles = [d[0] for d in defs]
    assert cles.index("hd") < cles.index("np")
    sans = bp._defs(bp._passage(None, bp.ENTREES, bp._maison()).definitions())
    assert [d for d in defs if d[0][0] != "n"] == [d for d in sans if d[0][0] != "n"]


@pytest.mark.parametrize("ecoute, duree, attendu", [
    ("0", 32, ["np|0", "nd|32"]),
    ("2", 8, ["np|2", "nd|8"]),
    ("masquee", 120, ["np|-", "nd|120"]),
    ("n'importe quoi", 32.0, ["np|-", "nd|32"]),
])
def test_reglages_du_panneau(ecoute, duree, attendu):
    p = _passage(entrees={"nabu_ecoute": ecoute, "nabu_duree": duree})
    assert p.definitions().split(";")[2:4] == attendu


def test_etats_et_declencheurs_du_panneau():
    p = _passage(bp._evenement("connexion"))
    assert p["tuiles_a_pousser"][-3:] == ["n10", "n11", "n20"]
    etats = {e[0]: e[1:] for e in bp._defs(p.etats_tuiles())}
    assert etats["n10"] == ["21.4", "21.4", ""] and etats["n11"] == ["off", "nan", ""]
    triggers = bp._blueprint()["triggers"]
    visibles = next(t for t in triggers if t["id"] == "piece_1")["to"]
    for n in (1, 2, 3):
        mes = [t for t in triggers if t["id"].startswith(f"nabu_{n}")]
        assert [t["id"] for t in mes] == [f"nabu_{n}", f"nabu_{n}_sortie"]
        assert all(t["entity_id"].nom == f"nabu_ligne_{n}" for t in mes)
        assert mes[0]["to"] == visibles and mes[1]["from"] == visibles and mes[1]["not_to"] == visibles
    # Un changement pousse son élément seulement, celui du panneau (la porte est aussi
    # dans une pièce, t14 : pas poussée par nabu_2).
    base = {e.entity_id: e for e in bp._maison()}
    porte = base["binary_sensor.porte"]
    ouverte = bp.Etat("binary_sensor.porte", "on", "Salon", **porte.attributes)
    etats = [ouverte if e.entity_id == "binary_sensor.porte" else e for e in bp._maison()]
    p = _passage(bp._declencheur("nabu_2", porte, ouverte), etats=etats)
    assert p["tuiles_a_pousser"] == ["n11"]
    assert bp._defs(p.etats_tuiles()) == [["n11", "on", "nan", ""]]
    # Une entité qui apparaît : tout est redéfini.
    p = _passage(bp._declencheur("nabu_3", None, base["switch.prise_pc"]))
    assert p["redefinir"] is True and "n20" in p["tuiles_a_pousser"]


def test_aucune_commande_sur_le_panneau():
    for action in ("basculer", "allumer", "eteindre"):
        p = bp._passage(bp._evenement("action", emplacement="n20", action=action, valeur=""),
                        {**bp.ENTREES, **NABU}, bp._maison())
        assert p.aiguillage() == (None, []), action


def _payload(declencheur, **entrees):
    return bp.Passage(entrees, [], bp._evenement(declencheur)).variables_du_bloc("payload")


def test_cle_defil_du_blueprint():
    p = _payload("connexion")
    assert "defil|auto|fixe|fixe|32;" in p
    assert p.index("gestes|") < p.index("defil|"), "après les gestes : le firmware lit defil avant la fin"
    choix = _payload("connexion", rangee_defilement="fixe", nabu_defilement="auto",
                     reglables_defilement="auto", reglables_duree=40)
    assert "defil|fixe|auto|auto|40;" in choix
    # Une valeur inconnue part vide : le firmware prend le défaut de la zone.
    assert "defil|||auto|32;" in _payload("connexion", rangee_defilement="AUTO", nabu_defilement=None,
                                         reglables_defilement="auto")
    for declencheur in ("zones", "action"):
        assert "defil|" not in _payload(declencheur)


def test_tap_des_heures_dans_le_blueprint():
    entrees = _entrees()
    options = [o["value"] for o in entrees["geste_heures_court"]["selector"]["select"]["options"]]
    # Après les actions du lot A ; Lumières et Volets (ADR-0046) sont venus après.
    assert options[-3:] == ["nabu_suivant", "lumieres", "volets"]
    assert "panneau Ok Nabu" in entrees["geste_heures_court"]["description"]


# ─────────────────────────────────────────────────────────────────────────────
# Démo et rendu
# ─────────────────────────────────────────────────────────────────────────────

def test_la_demo_garde_le_panneau_d_avant():
    assert scenarios.build_rangee_payload(scenarios.NABU) == "np|0;nd|32;"
    payload = scenarios.build_rangee_payload(scenarios.NABU_TROIS_LIGNES)
    entrees = [e.split("|") for e in payload.split(";") if e]
    assert entrees[:2] == [["np", "0"], ["nd", "32"]]
    assert all(re.fullmatch(r"n[01][0-3]", e[0]) and len(e) == 7 for e in entrees[2:])
    assert scenarios.build_rangee_payload(scenarios.NABU_UNE_LIGNE).startswith("np|-;nd|32;n00|")


def test_le_rendu_capture_le_panneau():
    par_nom = {e.nom: e for e in ecrans.ECRANS}
    heures = ecrans.Toucher(*ecrans.HEURES)
    for n in (1, 2, 3):
        ecran = par_nom[f"accueil-nabu-ligne-{n}"]
        assert [e for e in ecran.etapes if e == heures] == [heures] * (n - 1)
        assert ecran.fermer == ecrans.NABU_DE_LA_DEMO
    assert "accueil-nabu-une-ligne" in par_nom
    themes = [o.split('"')[1] for o in re.findall(r'^      - "[^"]+"$', _lire("Tab5", "paquets", "tab5-themes.yaml"), re.M)]
    for nom in ("accueil-nabu-police-large", "accueil-nabu-gelule"):
        ecran = par_nom[nom]
        choix = [e for e in ecran.etapes if isinstance(e, ecrans.Choisir)]
        assert len(choix) == 1 and choix[0].select == "Thème" and choix[0].option in themes
        assert ecran.fermer[-1] == ecrans.Choisir("Thème", ecrans.THEME_PAR_DEFAUT)
    assert ecrans.THEME_PAR_DEFAUT == re.search(r'initial_option: "([^"]+)"',
                                                _lire("Tab5", "paquets", "tab5-themes.yaml")).group(1)
    capturer = _lire("tools", "rendu", "capturer.py")
    assert "isinstance(etape, Choisir)" in capturer
    # Le blueprint de la démo d'avant n'a aucune ligne n… : celui de fermer non plus.
    assert "n00|" not in dict(ecrans.NABU_DE_LA_DEMO[0].donnees)["payload"]


def test_le_blueprint_est_valide_en_yaml():
    texte = _lire("HomeAssistant_Config", "blueprints", "automation", "tab5", "tab5_emplacements.yaml")
    section = yaml.load(texte.replace("!input", "!!str"), Loader=yaml.SafeLoader)["blueprint"]["input"]["panneau_nabu"]
    assert section["collapsed"] is True and " · " in section["name"]
    assert list(section["input"]) == ["nabu_ligne_1", "nabu_ligne_2", "nabu_ligne_3", "nabu_ecoute",
                                      "nabu_defilement", "nabu_duree"]
