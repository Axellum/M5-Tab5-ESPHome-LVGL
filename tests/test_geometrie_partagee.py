# -*- coding: utf-8 -*-
"""Géométrie écrite deux fois, en YAML et en C++ (le C++ ne lit pas les substitutions
ESPHome) : ce test la tient égale des deux côtés (audit des conteneurs du 01/10/2026).

- Largeur des panneaux de la carte centrale : ${central_w} (Tab5/paquets/tab5-ui-tokens.yaml)
  et kPanneauCentralL (tab5_geometrie.h ; kLargeurPanneauCentral de tab5_central.cpp,
  réponse vocale longue).
- Colonnes du calendrier : les en-têtes « Lun »…« Dim » de calendar_popup.yaml (jetons
  cal_col_*) et les cases construites par cal_grid_build() (kCalColX0, kCalColPas, kCalColW
  de tab5_geometrie.h). Un écart décale les noms des jours de leurs cases.
- Géométrie partagée du C++ (Tab5/socle/tab5_geometrie.h, lot L5 de l'audit du 07/10/2026) :
  carte modale égale aux jetons, corps des popups Énergie et Température égal à leur
  YAML, et aucune copie locale de ces constantes ni des littéraux 1280 / 1250.
- Une taille d'écran par carte (ADR-0061) : l'écran et la carte modale du C++ (drapeaux
  -DTAB5_… de chaque carte) égaux aux jetons calculés pour cette carte, pour CHAQUE carte ;
  les défauts de l'en-tête sont ceux du Tab5. Depuis le lot 2 du portage, ce que les popups
  ont en commun (corps, graphique, demi-cartes, onglets d'en-tête, colonnes du calendrier,
  lignes des pièces, panneaux de la carte centrale) suit la carte : égal des deux côtés et
  dans la carte, carte par carte ; sur le Tab5, les valeurs au pixel d'avant.
"""
import os
import re

import yaml
import pytest
from tests.commun import (CARTES, DRAPEAUX_CARTE, ChargeurSansBalises as _Chargeur, _jetons as jetons_resolus,
                          geometrie, geometrie_cpp, jeton, lire as _lire, resoudre, source)

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
JOURS = ["Lun", "Mar", "Mer", "Jeu", "Ven", "Sam", "Dim"]


def _resolu(valeur, carte="tab5"):
    """Une valeur YAML (littéral ou `${jeton}`), calculée pour la carte."""
    v = resoudre(valeur, jetons_resolus(carte))
    return int(v) if isinstance(v, str) and re.fullmatch(r"-?\d+", v) else v


# Tab5/socle/tab5_geometrie.h : constantes entières, éventuellement calculées des précédentes.
GEOMETRIE = "tab5_geometrie.h"
GEOMETRIE_NOMS = ("kEcranL", "kEcranH", "kCarteL", "kCarteH", "kCorpsY", "kCorpsX", "kCorpsW", "kCorpsH",
                  "kCartesEcart", "kGraphiqueL", "kAxeLibelleL", "kDemiL", "kDemiX", "kPiecesMax", "kTuilesMax",
                  "kPlacesTuiles", "kAccCaseEcart",
                  "kLecteurCarteL", "kLecteurCarteH", "kOngletsFin", "kCalColPas", "kCalColW", "kCalColX0",
                  "kPieceLignePas", "kPieceLigneH", "kCentralCarteL", "kPanneauCentralL",
                  "kCorpsWTab5", "kCorpsHTab5", "kPieceLigneL")
# Le même nombre, jeton YAML (tab5-ui-tokens.yaml) ↔ constante C++, sur chaque carte.
JETONS_CPP = {"corps_w": "kCorpsW", "corps_h": "kCorpsH", "graphique_l": "kGraphiqueL", "demi_w": "kDemiL",
              "demi_x": "kDemiX", "onglets_fin": "kOngletsFin", "cal_col_pas": "kCalColPas",
              "cal_col_w": "kCalColW", "cal_col_x0": "kCalColX0", "piece_ligne_pas": "kPieceLignePas",
              "piece_ligne_h": "kPieceLigneH", "central_carte_w": "kCentralCarteL", "central_w": "kPanneauCentralL",
              "piece_ligne_l": "kPieceLigneL",
              # Accueil (lot 3 du portage).
              "acc_marge": "kAccMarge", "acc_col_l": "kAccColL", "acc_horloge_l": "kAccHorlogeL",
              "acc_horloge_h": "kAccHorlogeH", "acc_ligne_l": "kAccLigneL", "acc_centrale_y": "kAccCentraleY",
              "acc_tuile_l": "kAccTuileL", "acc_tuile_pas": "kAccTuilePas",
              "acc_tuile_h": "kAccTuileH", "acc_tuile_cell_y": "kAccTuileY",
              "police_pct": "kPolicePct", "hor_dx": "kHorlogeDx", "hor_dy": "kHorlogeDy"}
# Valeurs du Tab5 d'avant le lot 2 : elles ne bougent pas.
TAB5_AVANT = {"kCorpsW": 1202, "kCorpsH": 598, "kGraphiqueL": 1166, "kDemiL": 589, "kDemiX": 637,
              "kOngletsFin": 1148, "kCalColPas": 172, "kCalColW": 168, "kCalColX0": 25, "kPieceLignePas": 92,
              "kPieceLigneH": 84, "kCentralCarteL": 1240, "kPanneauCentralL": 1180,
              "kPieceLigneL": 342}
# Fichiers qui s'en servent : aucun ne doit les redéfinir.
GEOMETRIE_UTILISATEURS = ("tab5_energie.cpp", "tab5_historique.cpp", "tab5_maison.cpp", "tab5_zones.cpp",
                          "tab5_tuiles.cpp", "tab5_tuiles_popups.cpp", "tab5_tuiles_roue.cpp", "tab5_tuiles_cases.cpp",
                          "tab5_tuiles_priv.h", "tab5_roue.cpp", "tab5_clim.cpp", "tab5_lecteur.cpp",
                          "tab5_calendar.cpp", "tab5_central.cpp", "tab5_pages.cpp")


def _geometrie(carte=None):
    """Constantes de tab5_geometrie.h : défauts de l'en-tête, ou drapeaux de la carte."""
    return geometrie_cpp(carte)


def _widgets(liste):
    for entree in liste or []:
        if not isinstance(entree, dict):
            continue
        for type_, props in entree.items():
            props = props or {}
            yield type_, props
            yield from _widgets(props.get("widgets"))


@pytest.mark.parametrize("carte", CARTES)
def test_jetons_communs_egaux_au_cpp_et_dans_la_carte(carte):
    g = _geometrie(carte)
    for nom, cpp in JETONS_CPP.items():
        assert jeton(nom, carte) == g[cpp], (carte, nom, cpp)
    if carte == "tab5":
        assert {k: g[k] for k in TAB5_AVANT} == TAB5_AVANT, "Tab5 : valeurs d'avant le lot 2"
    # Corps sous la barre de titre, dans la carte ; deux demi-cartes côte à côte.
    assert g["kCorpsX"] + g["kCorpsW"] + g["kCorpsX"] == g["kCarteL"]
    assert g["kCorpsY"] + g["kCorpsH"] < g["kCarteH"]
    assert g["kDemiX"] + g["kDemiL"] <= g["kCorpsX"] + g["kCorpsW"]
    # Lignes d'une pièce : les cinq au-dessus des boutons de la pièce (88 px du bas du corps).
    assert 50 + 4 * g["kPieceLignePas"] + g["kPieceLigneH"] <= g["kCorpsH"] - 88
    assert g["kPieceLigneH"] >= 56, "une ligne garde sa pastille et ses deux textes"
    # Carte centrale : à la marge de l'accueil des bords de l'écran (20 px sur le Tab5).
    assert g["kCentralCarteL"] + 2 * g["kAccMarge"] == g["kEcranL"]


@pytest.mark.parametrize("carte", CARTES)
def test_mise_en_page_des_popups_ramenee_au_corps(carte):
    """Lot 4 du portage : un littéral de la mise en page d'un popup (dessinée pour le corps du
    Tab5, 1202 × 598) est ramené au corps de la carte, en YAML par l'expression Jinja
    `${ v * (corps_w | int) // 1202 }` et en C++ par largeur_carte() / hauteur_carte() :
    les deux donnent le même nombre sur chaque carte, et sur le Tab5 le nombre lui-même."""
    g = _geometrie(carte)
    assert (g["kCorpsWTab5"], g["kCorpsHTab5"]) == (1202, 598)
    entete = _lire(source(GEOMETRIE))
    assert "v * kCorpsW / kCorpsWTab5" in entete and "v * kCorpsH / kCorpsHTab5" in entete
    jetons = jetons_resolus(carte)
    for v in (0, 1, 22, 56, 386, 456, 598, 1202):
        l = v * g["kCorpsW"] // g["kCorpsWTab5"]
        h = v * g["kCorpsH"] // g["kCorpsHTab5"]
        assert resoudre(f"${{ {v} * (corps_w | int) // 1202 }}", jetons) == l
        assert resoudre(f"${{ {v} * (corps_h | int) // 598 }}", jetons) == h
        assert resoudre(f"${{ 0 - {v} * (corps_h | int) // 598 }}", jetons) == -h
        if carte == "tab5":
            assert l == h == v
        assert l <= v and h <= v, "jamais plus grand que sur le Tab5"


def test_largeur_des_panneaux_centraux():
    assert "kLargeurPanneauCentral = kPanneauCentralL;" in _lire(source("tab5_central.cpp"))
    # Plus de 1180 écrit en clair dans le YAML : tout passe par le jeton.
    # Bouton des panneaux : central_bouton.yaml depuis le 08/10/2026 (audit YML-4).
    for chemin in (("Tab5", "paquets", "tab5-lvgl.yaml"), ("Tab5", "ui_components", "central_bouton.yaml")):
        texte = _lire(*chemin)
        assert "${central_w}" in texte, "/".join(chemin)
        assert not re.search(r"^\s*width: (1180|1240)\s*$", texte, re.M), "/".join(chemin)


def _largeur_roboto_22(texte):
    """Largeur d'un texte en roboto_22 (Tab5/fonts/roboto_700.ttf, 22 px, qui ne rétrécit pas
    avec la carte) : la somme des avances, comme lv_text_get_size sans crénage."""
    from fontTools.ttLib import TTFont
    police = TTFont(os.path.join(REPO, "Tab5", "fonts", "roboto_700.ttf"))
    cmap, hmtx = police.getBestCmap(), police["hmtx"]
    return round(sum(hmtx[cmap[ord(c)]][0] for c in texte) * 22 / police["head"].unitsPerEm)


@pytest.mark.parametrize("carte", CARTES)
def test_tout_ouvrir_tout_fermer_dans_leurs_boutons(carte):
    """Volets : « Tout ouvrir » et « Tout fermer » gardent 8 px d'air de chaque côté dans leur
    bouton, les deux boutons côte à côte dans la carte de gauche (sur la 7B, le mot touchait
    les bords d'un bouton de 121 px jusqu'au 18/10/2026). Sur le Tab5 : 163 px à x 22, 16 entre."""
    j = jetons_resolus(carte)
    l, x, e = (int(j[n]) for n in ("volet_tout_l", "volet_tout_x", "volet_tout_ecart"))
    for mot in ("Tout ouvrir", "Tout fermer"):
        assert _largeur_roboto_22(mot) + 16 <= l, (mot, l, carte)
    assert e >= 10 and x >= 8 and x + 2 * l + e <= 386 * int(j["corps_w"]) // 1202 - 8, (x, l, e, carte)
    if carte == "tab5":
        assert (l, x, e) == (163, 22, 16)


@pytest.mark.parametrize("carte", CARTES)
def test_raccourcis_de_luminosite_dans_leurs_boutons(carte):
    """Lumières : « 100 % » garde 3 px d'air de chaque côté dans son bouton (il touchait les
    bords d'un bouton de 62 px sur la 7B jusqu'au 18/10/2026), les quatre boutons dans la carte
    du milieu (398 px ramenés au corps). Sur le Tab5 : 80 px au pas de 90 depuis x 22."""
    j = jetons_resolus(carte)
    l, pas, x = (int(j[n]) for n in ("lum_niveau_l", "lum_niveau_pas", "lum_niveau_x"))
    assert _largeur_roboto_22("100 %") + 6 <= l, (l, carte)
    assert pas - l >= 6 and x >= 8 and x + 3 * pas + l <= 398 * int(j["corps_w"]) // 1202 - 8, (x, l, pas, carte)
    if carte == "tab5":
        assert (l, pas, x) == (80, 90, 22)


@pytest.mark.parametrize("carte", CARTES)
def test_en_tetes_du_calendrier_sur_les_colonnes_des_cases(carte):
    g = _geometrie(carte)
    x0, pas, w = g["kCalColX0"], g["kCalColPas"], g["kCalColW"]
    popup = yaml.load(_lire("Tab5", "ui_components", "calendar_popup.yaml"), Loader=_Chargeur)
    en_tetes = {props["text"]: props for type_, props in _widgets([popup])
                if type_ == "label" and props.get("text") in JOURS}
    assert sorted(en_tetes, key=JOURS.index) == JOURS
    for c, jour in enumerate(JOURS):
        props = en_tetes[jour]
        assert props["align"] == "TOP_LEFT", jour
        assert _resolu(props["x"], carte) == x0 + c * pas, f"« {jour} » décalé de sa colonne ({carte})"
        assert _resolu(props["width"], carte) == w, f"« {jour} » plus large ou plus étroit que sa case ({carte})"


@pytest.mark.parametrize("carte", CARTES)
def test_en_tete_du_calendrier_sans_chevauchement(carte):
    """‹, le mois, › et « Aujourd'hui » côte à côte sur la ligne du titre, sans se couvrir :
    sur la 7B, › était caché sous « Aujourd'hui » (rendu du 18/10/2026). Sur le Tab5 : 278
    et 420, comme avant."""
    g = _geometrie(carte)
    popup = yaml.load(_lire("Tab5", "ui_components", "calendar_popup.yaml"), Loader=_Chargeur)
    par_id = {p.get("id"): p for _t, p in _widgets([popup]) if p.get("id")}
    milieu = g["kCarteL"] // 2

    def boite(nom):
        p = par_id[nom]
        x, w = _resolu(p["x"], carte), _resolu(p["width"], carte)
        if p["align"] == "TOP_MID":
            return milieu + x - w // 2, milieu + x - w // 2 + w
        assert p["align"] == "TOP_RIGHT", nom
        return g["kCarteL"] + x - w, g["kCarteL"] + x

    ordre = [boite(n) for n in ("btn_cal_prev", "lbl_cal_month", "btn_cal_next", "btn_cal_today")]
    for (g1, d1), (g2, d2) in zip(ordre, ordre[1:]):
        assert d1 + 16 <= g2, f"en-tête du calendrier : {ordre} ({carte})"
    assert ordre[0][0] >= 200, f"‹ trop près du titre « Calendrier » ({carte})"
    if carte == "tab5":
        assert (jeton("cal_nav_x"), jeton("cal_mois_l")) == (278, 420)


@pytest.mark.parametrize("carte", CARTES)
def test_reglages_derniere_commande_au_dessus_du_bas(carte):
    """Réglages : chaque bouton de choix (reglages_choix_btn.yaml) reste 40 px sous le titre de
    sa rangée (reglages_rangee.yaml, fixe: 46), et le plus bas garde 16 px au-dessus du bas
    du corps (3 sur la 7B avant le 18/10/2026). Sur le Tab5, les y d'avant."""
    texte = _lire("Tab5", "ui_components", "reglages_popup.yaml")
    rangees = re.findall(r"file: reglages_rangee\.yaml, vars: \{([^}]*)\}", texte)
    assert rangees and all("fixe: 46," in r for r in rangees)
    titres = {int(y) for y in re.findall(r"file: reglages_rangee\.yaml, vars: \{[^}]*\by: (\d+),", texte)}
    boutons = [int(y) for y in re.findall(r"file: reglages_choix_btn\.yaml, vars: \{[^}]*\by: (\d+),", texte)]
    assert boutons and all(y - 40 in titres for y in boutons)
    jetons = jetons_resolus(carte)
    rangee = yaml.load(_lire("Tab5", "ui_components", "reglages_rangee.yaml"), Loader=_Chargeur)["obj"]
    bouton = yaml.load(_lire("Tab5", "ui_components", "reglages_choix_btn.yaml"), Loader=_Chargeur)["button"]

    def y_rangee(y):
        return int(resoudre(rangee["y"], dict(jetons, y=str(y), fixe="46")))

    def y_bouton(y):
        return int(resoudre(bouton["y"], dict(jetons, y=str(y))))

    for y in boutons:
        assert y_bouton(y) == y_rangee(y - 40) + 40, (y, carte)
    bas = max(y_bouton(y) for y in boutons) + int(resoudre(bouton["height"], jetons))
    assert bas <= int(jetons["corps_h"]) - 16, f"dernier bouton des Réglages à {bas} ({carte})"
    if carte == "tab5":
        assert [y_bouton(y) for y in boutons] == boutons and jeton("reglages_h") == 552


@pytest.mark.parametrize("carte", CARTES)
def test_grille_du_calendrier_centree_dans_la_carte(carte):
    g = _geometrie(carte)
    x0, pas, w = g["kCalColX0"], g["kCalColPas"], g["kCalColW"]
    assert pas > w, "les cases se chevauchent"
    droite = g["kCarteL"] - (x0 + 6 * pas + w)
    assert abs(droite - x0) <= 1, f"grille décentrée : {x0} px à gauche, {droite} px à droite ({carte})"
    if carte == "tab5":
        assert droite == x0


# ─── Géométrie partagée du C++ (lot L5) ──────────────────────────────────────

def test_geometrie_partagee_egale_aux_jetons_et_aux_popups():
    g = _geometrie()
    assert set(GEOMETRIE_NOMS) <= set(g), set(GEOMETRIE_NOMS) - set(g)
    assert g == _geometrie("tab5"), "défauts de tab5_geometrie.h ≠ Tab5/paquets/geometrie-tab5.yaml"
    # Corps des popups à cartes et zone de leur graphique, comme leur YAML.
    for popup, zone in (("energie_popup.yaml", "energie_zone"), ("historique_popup.yaml", "historique_zone")):
        widgets = list(_widgets([yaml.load(_lire("Tab5", "ui_components", popup), Loader=_Chargeur)]))
        corps = [p for _t, p in widgets if p.get("x") == g["kCorpsX"] and _resolu(p.get("width")) == g["kCorpsW"]]
        assert corps, f"{popup} : pas de corps x {g['kCorpsX']}, largeur {g['kCorpsW']}"
        zones = [p for _t, p in widgets if p.get("id") == zone]
        assert len(zones) == 1 and _resolu(zones[0].get("width")) == g["kGraphiqueL"], f"{popup} : {zone}"
    # Carte du popup Musique (ADR-0050) : égale à ses jetons, centrée, plus petite que la
    # carte modale, et le popup la prend par ses jetons.
    assert (g["kLecteurCarteL"], g["kLecteurCarteH"]) == (jeton("lecteur_card_w"), jeton("lecteur_card_h"))
    assert g["kLecteurCarteL"] < g["kCarteL"] and g["kLecteurCarteH"] < g["kCarteH"]
    popup = _lire("Tab5", "ui_components", "lecteur_popup.yaml")
    assert "width: ${lecteur_card_w}" in popup and "height: ${lecteur_card_h}" in popup
    # Pièces : dix au plus depuis l'ADR-0066 (lot A), cinq sans « pn », cinq pages de météo ;
    # éléments d'une pièce : vingt (lot B), sur cinq places coupées en deux ou en quatre ;
    # moitiés de 230 × 135 et quarts de 113 × 135 sur le Tab5 (disposition::case_rect, 4 px
    # d'écart).
    assert g["kPiecesMax"] == 10 and g["kPiecesDefaut"] == g["kPagesMeteo"] == g["kPlacesTuiles"] == 5
    assert g["kTuilesMax"] == 20
    assert g["kAccCaseEcart"] == 4
    assert ((g["kAccTuileH"] - g["kAccCaseEcart"]) // 2, (g["kAccTuileL"] - g["kAccCaseEcart"]) // 2) == (135, 113)


@pytest.mark.parametrize("carte", CARTES)
def test_ecran_et_carte_modale_egaux_aux_jetons_de_chaque_carte(carte):
    g = _geometrie(carte)
    assert (g["kEcranL"], g["kEcranH"]) == (int(geometrie(carte)["carte_ecran_l"]), int(geometrie(carte)["carte_ecran_h"]))
    assert (g["kCarteL"], g["kCarteH"], g["kCorpsY"]) == (
        jeton("modal_card_w", carte), jeton("modal_card_h", carte), jeton("modal_body_y", carte))
    # La carte est centrée dans l'écran : 15 px de chaque côté (commentaire des jetons).
    assert (g["kEcranL"] - g["kCarteL"]) // 2 == (g["kEcranH"] - g["kCarteH"]) // 2 == 15
    # La grille du calendrier suit la hauteur de la carte.
    assert jeton("cal_grid_h", carte) == jeton("modal_card_h", carte) - 162


def test_drapeaux_de_la_carte_dans_les_deux_configurations():
    """Le C++ ne voit la carte que par ses -DTAB5_… : les deux configurations (tablette et
    rendu hors tablette) les passent, chacun égal à sa substitution `carte_*`."""
    for config in ("tab5-ha-hmi.yaml", "tab5-rendu-host.yaml"):
        texte = _lire(config)
        for drapeau, substitution in DRAPEAUX_CARTE.items():
            assert texte.count(f"-D{drapeau}=") == 1 and f"- -D{drapeau}=${{{substitution}}}" in texte, (config, drapeau)
    entete = _lire(source(GEOMETRIE))
    for drapeau in DRAPEAUX_CARTE:
        assert re.search(rf"#ifndef {drapeau}\s+#define {drapeau} -?\d+\s+#endif", entete), drapeau


def test_aucune_copie_locale_de_la_geometrie():
    for fichier in GEOMETRIE_UTILISATEURS:
        cpp = _lire(source(fichier))
        assert '#include "tab5_geometrie.h"' in cpp, fichier
        for nom in GEOMETRIE_NOMS:
            assert not re.search(r"constexpr\s+\w+\s+%s\s*=" % nom, cpp), f"{nom} redéfini dans {fichier}"
        sans_blocs = re.sub(r"/\*.*?\*/", "", cpp, flags=re.S)
        code = "\n".join(l.split("//", 1)[0] for l in sans_blocs.splitlines())
        for litteral in ("1280", "1250", "1202", "1166"):
            assert not re.search(r"\b%s\b" % litteral, code), f"{litteral} en dur dans {fichier}"


# ─── Accueil à la taille de la carte (lot 3 du portage 1024×600) ─────────────────────────

# Valeurs du Tab5 d'avant le lot 3 : chaque jeton de l'accueil les redonne au pixel.
ACCUEIL_TAB5 = {
    "acc_marge": 20, "acc_gout": 15, "acc_bouton_l": 125, "acc_col_l": 405, "acc_droite_x": 855, "acc_discu_x": 300, "acc_sys_x": 995, "acc_tv_x": 1135,
    "acc_horloge_l": 401, "acc_horloge_x": 440, "acc_milieu": 640, "acc_haut_y": 20, "acc_bouton_h": 90,
    "acc_horloge_h": 210, "acc_bandeau_y": 10, "ligne_zone_h": 70, "cadre_bas_h": 72, "acc_cadre_bas": 308,
    "acc_cadre_y": 236, "acc_ligne_l": 401, "acc_rangee_y": 244, "acc_pastilles_y": 319, "acc_zone_y": 42,
    "acc_zone_h": 184, "acc_vocal_h": 230, "acc_micro_x": 162, "acc_micro_y": 78, "acc_micro_l": 120,
    "acc_domo_y": 90, "acc_clim_y": 110, "acc_clim_h": 198, "acc_temp_x": 14, "acc_temp_ecart": 8, "acc_temp_y": -36,
    "acc_temp_zone_y": 31, "acc_temp_zone_h": 64, "acc_serre_x": -6, "acc_serre_l": 200, "acc_salon_x": 4,
    "acc_salon_l": 192, "acc_pas_x": 13, "acc_pas_l": 52, "acc_pas_rayon": 12, "acc_consigne_l": 200, "acc_reglables_x": 740, "acc_reglables_l": 520, "acc_reglable_h": 52,
    "acc_mini_vignette": 56, "acc_mini_texte_x": 76, "acc_mini_titre_y": 10, "acc_mini_artiste_y": 38,
    "acc_lect_btn": 52, "acc_lect_lecture": 60, "central_carte_w": 1240, "central_w": 1180,
    "acc_centrale_y": 333, "acc_centrale_h": 84, "acc_centrale_bouton_h": 80, "acc_pluie_h": 50,
    "acc_pluie_5mn_l": 25, "acc_pluie_10mn_l": 55, "acc_pluie_ecart": 15,
    "acc_pluie_pad": 15, "acc_pluie_pad_val": 10,
    "acc_pagination_x": 480, "acc_pagination_y": 420, "acc_pagination_l": 310, "acc_perimees_x": 800,
    "acc_perimees_y": 416, "acc_perimees_l": 455, "acc_perimees_h": 26, "acc_tuiles_y": 430, "acc_tuiles_h": 290,
    "acc_tuile_l": 230, "acc_tuile_pas": 250, "acc_tuile_x0": 25, "acc_tuile_x1": 275, "acc_tuile_x2": 525,
    "acc_tuile_x3": 775, "acc_tuile_x4": 1025, "acc_tuile_h": 275, "acc_tuile_cell_y": 5, "acc_t_onglet_y": 7,
    "acc_t_onglet_l": 130, "acc_t_onglet_h": 46, "acc_t_temp_l": 140, "acc_t_bas_h": 51, "acc_t_epaule_x": 10,
    "acc_t_epaule_y": 9, "acc_t_bouton_l": 140, "acc_t_bouton_l4": 130, "acc_t_bouton_h": 130, "acc_t_max_x": -25,
    "acc_t_min_x": 35, "acc_t_texte_y": -7, "acc_t_volet_h": 60, "acc_h_carte_y": 35, "acc_h_carte_h": 205,
    "acc_h_bas_l": 210, "acc_h_goutte_x": 5, "acc_h_goutte_y": -14, "acc_h_pluie_x": 34, "acc_h_temp_x": -5,
    "acc_sw_pastille_y": 26, "acc_sw_pastille": 130, "acc_sw_rayon": 65, "acc_sw_nom_y": 166, "acc_sw_nom_l": 214, "acc_sw_nom_h": 40,
    "acc_sw_etat_y": 210, "police_pct": 100, "hor_dx": 0, "hor_dy": 0, "hor_cadre_y": 17, "hor_cadre_l": 75,
    "hor_cadre_h": 104, "hor_label_y": -23, "hor_h10_x": 27, "hor_h1_x": 102, "hor_m10_x": 222, "hor_m1_x": 297,
    "hor_dp_x": 181, "hor_dp_y": -6, "hor_date_y": 135, "hor_coupe_y": 158, "hor_heures_l": 200,
    "hor_minutes_l": 201, "hor_chiffres_h": 138, "hor_date_h": 72, "hor_froid_x": 806,
}


def test_accueil_du_tab5_au_pixel():
    """Sur le Tab5, chaque jeton de l'accueil redonne la valeur d'avant le lot 3, et la liste
    ci-dessus les connaît tous (un jeton acc_* ou hor_* de plus = sa valeur du Tab5 ici)."""
    jetons = jetons_resolus("tab5")
    assert {k: int(jetons[k]) for k in ACCUEIL_TAB5} == ACCUEIL_TAB5
    nouveaux = {k for k in jetons if k.startswith(("acc_", "hor_"))} - set(ACCUEIL_TAB5)
    assert not nouveaux, f"jetons de l'accueil sans valeur du Tab5 dans ACCUEIL_TAB5 : {sorted(nouveaux)}"


@pytest.mark.parametrize("carte", CARTES)
def test_accueil_tient_dans_l_ecran_de_chaque_carte(carte):
    """La grille de l'accueil tient dans l'écran de la carte, sans chevauchement : colonnes
    et horloge côte à côte, rangées du haut vers le bas, cinq tuiles entières."""
    j = {k: int(v) for k, v in jetons_resolus(carte).items() if re.fullmatch(r"-?\d+", str(v))}
    l, h = j["carte_ecran_l"], j["carte_ecran_h"]
    # Horizontal : colonne gauche | horloge | colonne droite, les marges aux bords.
    assert j["acc_marge"] + j["acc_col_l"] < j["acc_horloge_x"]
    assert j["acc_horloge_x"] + j["acc_horloge_l"] < j["acc_droite_x"]
    assert j["acc_droite_x"] + j["acc_col_l"] + j["acc_marge"] == l
    assert j["acc_discu_x"] + j["acc_bouton_l"] == j["acc_marge"] + j["acc_col_l"]
    assert j["acc_ligne_l"] <= j["acc_horloge_l"] and j["acc_ligne_l"] <= j["acc_col_l"] - 4
    # Vertical : boutons du haut, cadres du bas (Ok Nabu, − / +) alignés, carte centrale,
    # tuiles jusqu'au bas de l'écran.
    assert j["acc_haut_y"] + j["acc_bouton_h"] <= j["acc_clim_y"]
    assert j["acc_clim_y"] + j["acc_clim_h"] == j["acc_cadre_y"] + j["cadre_bas_h"] == j["acc_cadre_bas"]
    assert j["acc_zone_y"] + j["acc_zone_h"] < j["acc_cadre_y"]
    assert j["acc_haut_y"] + j["acc_horloge_h"] <= j["acc_rangee_y"]
    assert j["acc_rangee_y"] + j["ligne_zone_h"] < j["acc_pastilles_y"] < j["acc_centrale_y"]
    assert j["acc_centrale_y"] + j["acc_centrale_h"] <= j["acc_pagination_y"] < j["acc_tuiles_y"] + j["acc_tuile_cell_y"]
    assert j["acc_tuiles_y"] + j["acc_tuiles_h"] == h
    assert j["acc_tuile_cell_y"] + j["acc_tuile_h"] <= j["acc_tuiles_h"]
    # Cinq tuiles entières, centrées, sans se toucher.
    assert j["acc_tuile_pas"] > j["acc_tuile_l"]
    assert j["acc_tuile_x0"] >= 0 and j["acc_tuile_x4"] + j["acc_tuile_l"] <= l
    assert abs((l - j["acc_tuile_x4"] - j["acc_tuile_l"]) - j["acc_tuile_x0"]) <= 1
    # Horloge : les quatre cadres et le « : » dans la tuile, sans se chevaucher ; la coupe des
    # zones tactiles dans la tuile.
    interieur = j["acc_horloge_l"] - 2
    xs = [j["hor_h10_x"], j["hor_h1_x"], j["hor_m10_x"], j["hor_m1_x"]]
    assert xs[0] >= 0 and xs[3] + j["hor_cadre_l"] <= interieur
    assert xs[0] + j["hor_cadre_l"] <= xs[1] and xs[1] + j["hor_cadre_l"] <= j["hor_dp_x"] < xs[2]
    assert j["hor_cadre_y"] >= 0 and j["hor_cadre_y"] + j["hor_cadre_h"] < j["hor_coupe_y"] - j["acc_haut_y"]
    assert j["acc_haut_y"] < j["hor_coupe_y"] < j["acc_haut_y"] + j["acc_horloge_h"]
    assert j["hor_froid_x"] + 34 <= j["acc_horloge_x"] + j["acc_horloge_l"]
