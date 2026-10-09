# -*- coding: utf-8 -*-
"""Roue d'une clim par la température de la pièce (ADR-0047, 09/10/2026) : le toucher de la
température de la carte clim ouvre la roue d'actions rapides (ADR-0036) de la clim
qu'ouvrait le carrousel (ADR-0038), posée sur la température ; « Détails » = le carrousel
sur elle, « Clims ▸ » (au moins deux clims) passe la roue sur une autre.

Aucun compilateur ne vérifie ce qui suit ; ce fichier lit le C++, le YAML et le rendu :

- un seul code pour les boutons d'une clim (tuile cli et roue d'une clim) ;
- la clim choisie, les cas sans roue (carrousel) et sans clim (liste de la tuile − / +) ;
- les liens, la famille « Clims ▸ », les commandes aux emplacements existants ;
- la clim d'une pièce lue par la roue, le repeint à chaque poussée d'une clim ;
- les écrans du rendu hors tablette et l'ancre de la roue.
"""
import re

from tests.commun import lire as _lire, source
from tests.test_tuiles_firmware import _fonction

import ecrans  # noqa: E402 — tools/rendu, mis sur sys.path par tests/conftest.py

CLIM = _lire(source("tab5_clim.cpp"))
ROUE = _lire(source("tab5_tuiles_roue.cpp"))
INTERNE = _lire(source("tab5_internal.h"))
CARTE = _lire("Tab5", "ui_components", "climate_card.yaml")


def test_le_toucher_ouvre_la_roue_sinon_le_carrousel():
    salon = CARTE.split("id: btn_reglables_liste", 1)[1].split("\n    - ", 1)[0]
    assert "clim_temperature_ouvrir(id(btn_reglables_liste))" in salon, "l'ancre : la zone touchée"
    assert "reglables_liste_basculer();" in salon, "aucune clim : la liste de la tuile − / +"
    corps = _fonction(CLIM, "clim_temperature_ouvrir")
    assert corps.index("clim_ref_choisir(c)") < corps.index("clim_roue_ouvrir(c, ancre)") < corps.index(
        "clim_carrousel_ouvrir_sur(c)")
    # La roue exige les réglages reçus (pas de roue avant climr / crRT / crpR).
    assert "if (capacites == nullptr) return false;" in ROUE


def test_un_seul_code_de_boutons_clim():
    composer = _fonction(ROUE, "roue_composer")
    assert composer.count("auto composer_clim = ") == 1
    assert "if (!composer_clim(rt.clim, e.brut)) return 0;" in composer, "tuile cli"
    assert "composer_clim(rt.clim, clim_mode_connu(rt.clim.r, rt.clim.t))" in composer, "roue d'une clim"
    # Une tuile cli pose sa clim cible (option m : celle du blueprint).
    cli = composer.split("case Type::CLI:", 1)[1].split("break;", 1)[0]
    assert "clim_cible(d, r, t)" in cli and "rt.clim = ClimRef{" in cli


def test_premier_anneau_de_la_roue_d_une_clim():
    composer = _fonction(ROUE, "roue_composer")
    clim = composer.split("if (de_clim) {", 1)[1].split("const Def& d", 1)[0]
    # « Clims ▸ » à la place de « Maison » à partir de deux clims ; « Détails » en dernier.
    assert "if (roue_clims(rt, l, ici) >= 2) famille(RoueAction::CLIMS, RoueIcone::CLIMS);" in clim
    assert 'else ajouter(RoueAction::MAISON, RoueIcone::MAISON, RoueGenre::LIEN, false, tr("Maison"));' in clim
    assert clim.rstrip().endswith(
        'ajouter(RoueAction::REGLAGES, RoueIcone::REGLAGES, RoueGenre::LIEN, false, tr("Détails"));\n'
        '        return n;\n    }')
    # Six boutons au plus : Clims (ou Maison), Éteindre, trois familles, Détails.
    assert re.search(r"constexpr int kRoueBoutons = 6;", _lire(source("tab5_roue.h")))


def test_liens_et_clims():
    choisir = _fonction(ROUE, "roue_tuile_choisir")
    details = choisir.split("case RoueAction::REGLAGES:", 1)[1].split("return;", 1)[0]
    assert "if (roue_de_clim(rt)) clim_carrousel_ouvrir_sur(rt.clim);" in details
    assert "else tuile_ouvrir_popup(rt.r, rt.t);" in details
    choix = _fonction(ROUE, "roue_tuile_choisir_choix")
    clims = choix.split("if (rt.action[i] == RoueAction::CLIMS) {", 1)[1].split("return;\n    }", 1)[0]
    assert "if (!clim_roue_ouvrir(l[j], rt.ancre)) clim_carrousel_ouvrir_sur(l[j]);" in clims
    # Les clims de « Clims ▸ » : la liste unique du carrousel, au plus kRoueChoix, celle de
    # la roue comprise et marquée.
    liste = _fonction(ROUE, "roue_clims")
    assert "clims_enumerer(l, kClimPastilles)" in liste and "kRoueChoix" in liste
    famille = _fonction(ROUE, "roue_choix").split("case RoueAction::CLIMS: {", 1)[1].split("break;", 1)[0]
    assert 'choix(nullptr, "", k == ici)' in famille, "rien n'est envoyé, la clim de la roue marquée"
    assert "x.icone = RoueIcone::ETEINDRE;" in famille and "tete.consigne" in famille and "x.legende" in famille


def test_commandes_aux_emplacements_existants():
    emplacement = _fonction(ROUE, "clim_emplacement")
    assert "c.t < 0 ? clim_piece_cle(c.r) : tuile_cle(c.r, c.t)" in emplacement
    assert 'char s[8] = "clim";' in ROUE
    assert "const bool est_clim = vise_une_clim(rt);" in _fonction(ROUE, "roue_tuile_choisir_choix")
    # Les mêmes emplacements que le popup (clim_affichee_cle : « clim », « tRT », « cpR »).
    assert 'return vue_tuile() ? s_vue_cle : "clim";' in _fonction(CLIM, "clim_affichee_cle")


def test_la_clim_d_une_piece_est_lue_par_la_roue():
    assert "ClimTuile* ref_clim(int r, int t) { return t < 0 ? piece_clim(r, false) : tuile_clim(r, t, false); }" in CLIM
    for f in ("clim_capacites_connues", "clim_mode_connu"):
        assert "ref_clim(r, t)" in _fonction(CLIM, f), f
    assert "const ClimTuile* ct = ref_clim(r, t);" in _fonction(CLIM, "clim_roue")
    tete = _fonction(CLIM, "clim_tete")
    # Nom comme le titre du popup, couleur comme l'icône de la tuile − / +.
    assert "tuiles_piece_titre(r, out.nom, sizeof(out.nom))" in tete and 'tr("Climatisation")' in tete
    assert "out.couleur = couleur_icone_carte(mode);" in tete
    assert "void clim_tete(int r, int t, ClimTete& out);" in INTERNE


def test_une_clim_poussee_repeint_la_roue():
    for f in ("clim_blueprint_recu", "clim_reglages_recu", "clim_tuile_recu", "clim_tuile_oublier",
              "clim_piece_oublier"):
        assert "roue_clim_changee();" in _fonction(CLIM, f), f
    assert "if (roue_actions_ouverte() && vise_une_clim(s_rt)) roue_tuile_rejouer();" in _fonction(
        ROUE, "roue_clim_changee")


def test_glyphe_de_clims():
    roue = _lire(source("tab5_roue.cpp"))
    assert 'case RoueIcone::CLIMS: return "\\U000F001B";' in roue
    assert re.search(r'- "\\U000F001B"  # air-conditioner', _lire("Tab5", "paquets", "tab5-styles.yaml"))


def test_ecrans_du_rendu():
    # L'ancre : le centre de btn_reglables_liste (carte en 855, 110 ; zone x 4, y 22, 192 × 64).
    zone = CARTE.split("id: btn_reglables_liste", 1)[1].split("on_short_click", 1)[0]
    x, y, w, h = (int(re.search(rf"\n\s+{k}: (\d+)", zone).group(1)) for k in ("x", "y", "width", "height"))
    assert (855 + x + w // 2, 110 + y + h // 2) == ecrans.SALON == (955, 164)
    assert ecrans.roue_dessous(164), "la température est trop haute pour y poser la roue"
    # L'ancre basse de clim_ancre_basse() : mêmes constantes que le rendu.
    ancre = {k: int(re.search(rf"constexpr int32_t {k} = (\d+);", ROUE).group(1))
             for k in ("kClimAncreY", "kClimAncreXMin", "kClimAncreXMax")}
    assert ecrans.ROUE_CLIM_ANCRE == (min(max(955, ancre["kClimAncreXMin"]), ancre["kClimAncreXMax"]),
                                      ancre["kClimAncreY"])
    # Sur tout ce domaine, les deux anneaux et leurs mots tiennent au-dessus, sans pivot,
    # six familles de six choix comprises (la géométrie de disposer(), recopiée par le rendu).
    for xa in (ancre["kClimAncreXMin"], ancre["kClimAncreXMax"]):
        assert _roue_entiere(xa, ancre["kClimAncreY"]), xa
    assert "rt.ancre = clim_ancre_basse(ancre);" in _fonction(ROUE, "clim_roue_ouvrir")
    assert "if (zone == s_clim_ancre) return zone;" in _fonction(ROUE, "clim_ancre_basse")
    centres = ecrans.roue_centres(*ecrans.ROUE_CLIM_ANCRE, ecrans.ROUE_CLIM_TEMPERATURE)
    assert (ecrans.ROUE_CLIM_DETAILS.x, ecrans.ROUE_CLIM_DETAILS.y) == centres[-1]
    assert (ecrans.ROUE_CLIM_CLIMS.x, ecrans.ROUE_CLIM_CLIMS.y) == centres[ecrans.ROUE_CLIMS]
    # La clim du Bureau de la démo a ses trois familles (modes, consigne connue, Silence) :
    # avec « Clims ▸ » et « Détails », six boutons.
    reglages = ecrans.PIECES[ecrans.PIECE_CLIMAT].climat.reglages.split("|")
    etat = ecrans.PIECES[ecrans.PIECE_CLIMAT].climat.etat.split("|")
    assert set(reglages[4]) & set("hcdf") and set(reglages[4]) & set("ebqsw") and etat[0] not in ("", "nan")
    par_nom = {e.nom: e for e in ecrans.ECRANS}
    for nom in ("roue-clim-temperature", "roue-clim-temperature-clims", "climatisation-carrousel-mode-ha"):
        assert ecrans.Toucher(*ecrans.SALON) in par_nom[nom].etapes, nom
    assert par_nom["climatisation-carrousel-mode-ha"].etapes[-1] == ecrans.ROUE_CLIM_DETAILS
    assert par_nom["roue-clim-temperature-clims"].etapes[-1] == ecrans.ROUE_CLIM_CLIMS
    meteo = par_nom["roue-clim-temperature-meteo"].etapes
    assert meteo == (ecrans.CLIM_CAPACITES, ecrans.Toucher(*ecrans.SALON))


def _roue_entiere(xa: int, ya: int, n: int = 6, m: int = 6) -> bool:
    """Premier anneau de n boutons et second de m choix pour chaque famille : au-dessus de
    l'ancre, sans pivot, mots des choix (150 px, centrés à kRayon2 + kLegende2) dans l'écran."""
    e = ecrans
    largeur, hauteur = e.ROUE_ECRAN
    if e.roue_dessous(ya):
        return False
    p = e._roue_disposer(xa, ya, n, e.ROUE_RAYON, e.ROUE_PAS_ANGLE, 90, e.ROUE_DIAMETRE // 2, False)
    if p[0][2] != 90 + (n - 1) * e.ROUE_PAS_ANGLE // 2:
        return False
    for _, _, a in p:
        q = e._roue_disposer(xa, ya, m, e.ROUE_RAYON2, e.ROUE_PAS_ANGLE2, a, e.ROUE_DIAMETRE2 // 2, False)
        if q[0][2] != a + (m - 1) * e.ROUE_PAS_ANGLE2 // 2:
            return False
        for _, _, b in q:
            d = e.ROUE_RAYON2 + e.ROUE_LEGENDE2
            lx = xa + e._roue_echelle(d, e._roue_sin5(b + 90))
            ly = ya - e._roue_echelle(d, e._roue_sin5(b))
            if not (75 <= lx <= largeur - 75 and e.ROUE_LEGENDE_H // 2 <= ly <= hauteur - e.ROUE_LEGENDE_H // 2):
                return False
    return True


def test_adr():
    texte = _lire("docs", "decisions", "0047-climate-wheel-from-temperature.md")
    assert "clim_roue_ouvrir" in texte and "« Clims ▸ »" in texte and "« Détails »" in texte
    assert "0047-climate-wheel-from-temperature.md" in _lire("docs", "decisions", "0038-climate-carousel.md")
