# -*- coding: utf-8 -*-
"""Chronologie du démarrage (10/10/2026, demande d'Axel).

Une tablette sur secteur n'a pas de journal série : « Tab5 Chronologie du démarrage » et
« … (suite) » publient vers Home Assistant l'instant d'étapes fixes du démarrage
(Tab5/socle/tab5_demarrage.h, testé sur PC par tools/test_tab5_socle.cpp). Rien ne compile
le câblage YAML hors tablette : ce fichier le relit.

- chaque étape de EtapeDemarrage a son nom dans kEtapesNoms, dans le même ordre, et une
  seule marque ;
- chaque texte tient sous 255 caractères (état texte de Home Assistant) au pire ;
- observer sans retarder : les marques de setup() ne publient rien ;
- la séquence on_boot de tab5-ha-hmi.yaml ([AI-WARNING-CRITICAL], ADR-0005) ne reçoit que
  des marques (accord d'Axel du 10/10/2026) : la lambda de l'attente reste seule et
  telle quelle (sa durée vient de la carte depuis le 17/10/2026, `carte_attente_ecran_ms` :
  1000 sur le Tab5, test_attente_avant_l_ecran_selon_la_carte), les trois entrées d'origine gardent leur priorité et leur rang, les
  entrées ajoutées ne font que marquer, à des priorités qu'aucune autre n'a, sauf la
  barre de chargement (accord d'Axel du 16/10/2026), appelée après la marque ;
- les capteurs sont des text_sensor de diagnostic, publiés par un seul script."""
import re

import yaml
from tests.commun import REPO, ChargeurBalisesBrutes, lire, source, sources, vue_tab5

DIAG = "tab5-sensors-diagnostics.yaml"
RACINE = "tab5-ha-hmi.yaml"
PUBLIER = "id(tab5_chronologie_publier).execute();"
# Marquées hors du YAML ou par une autre fonction que demarrage_marquer().
HORS_YAML = {"CTOR": "tab5_demarrage.cpp", "ORDO": RACINE}
# Marquées après setup() : elles publient. Les autres (setup(), premier dessin et
# rétroéclairage, qui peuvent tomber dans une attente de setup()) ne publient pas.
PUBLIENT = {"IMAGE", "TARD", "WIFI", "API", "FIN600", "CHARGEUR"}
HA_MAX = 255
# Barre de chargement (16/10/2026, accord d'Axel) : seul autre appel admis dans les entrées
# de la chronologie, toujours APRÈS leur marque (test_barre_de_chargement_dans_l_on_boot).
BARRE = re.compile(r"^(?:if \()?barre_demarrage_\w+\(")


def _yaml(chemin):
    return yaml.load(chemin.read_text(encoding="utf-8"), Loader=ChargeurBalisesBrutes)


def _texte(obj):
    return yaml.safe_dump(obj, allow_unicode=True, width=10**6)


def _entete():
    return source("tab5_demarrage.h").read_text(encoding="utf-8")


def _etapes():
    enum = re.search(r"enum class EtapeDemarrage[^{]*\{([^}]*)\}", _entete())
    assert enum, "EtapeDemarrage introuvable"
    corps = re.sub(r"//[^\n]*", "", enum.group(1))
    noms = re.findall(r"^\s*(\w+)", corps, re.M)
    assert noms[-1] == "NOMBRE"
    return noms[:-1]


def _noms():
    noms = re.search(r"kEtapesNoms\[kEtapesDemarrage\]\s*=\s*\{([^}]*)\}", _entete())
    assert noms, "kEtapesNoms introuvable"
    return re.findall(r'"([^"]+)"', noms.group(1))


def _yaml_firmware():
    # Le firmware du Tab5 : une autre carte (ADR-0061) recopie la marque de son
    # rétroéclairage dans son propre package (tests/test_carte_7b.py).
    return {c.name: c.read_text(encoding="utf-8") for c in [*vue_tab5(sources("*.yaml")), REPO / RACINE]}


def _on_boot():
    return _yaml(REPO / RACINE)["esphome"]["on_boot"]


def test_noms_dans_l_ordre_de_l_enum():
    publies = _noms()
    assert [p.upper() for p in publies] == [e.replace("_", "") for e in _etapes()], (publies, _etapes())
    assert len(set(publies)) == len(publies)


def test_chaque_etape_marquee_une_fois():
    textes = _yaml_firmware()
    for etape in _etapes():
        ou = [nom for nom, t in textes.items()
              for _ in re.findall(rf"demarrage_marquer\(EtapeDemarrage::{etape}\)", t)]
        if etape in HORS_YAML:
            assert ou == [], f"{etape} : {ou}"
        else:
            assert len(ou) == 1, f"{etape} : {ou}"
    cpp = source("tab5_demarrage.cpp").read_text(encoding="utf-8")
    assert cpp.count("demarrage_marquer(EtapeDemarrage::CTOR)") == 1
    assert "const MarqueConstructeurs s_marque_constructeurs;" in cpp
    assert "esp_timer_get_time()" in cpp and "millis()" not in cpp.split("*/", 1)[1]
    assert sum(t.count("demarrage_noter_ordo(") for t in textes.values()) == 1
    assert "demarrage_noter_ordo(millis());" in textes[RACINE]
    # Une seule horloge (esp_timer, lue par l'en-tête) : plus de millis() passé en argument.
    for nom, t in textes.items():
        assert not re.search(r"demarrage_marquer\([^)]*,", t), nom


def test_textes_sous_la_limite_de_home_assistant():
    """Le pire de tools/test_tab5_socle.cpp, recompté ici : premier texte à 6 chiffres
    par étape (setup() fini avant 1 000 s), second à 10 (uint32_t)."""
    assert "constexpr size_t kChronoTexteMax = 256;" in _entete()
    noms, etapes = _noms(), _etapes()
    suite = etapes.index("ORDO")

    def longueur(partie, chiffres):
        return sum(len(n) + 1 + chiffres for n in partie) + 2 * (len(partie) - 1)

    assert longueur(noms[:suite], 6) <= HA_MAX, longueur(noms[:suite], 6)
    assert longueur(noms[suite:], 10) <= HA_MAX, longueur(noms[suite:], 10)


def test_on_boot_des_marques_seulement():
    entrees = _on_boot()
    # Les trois entrées d'origine, à leur rang et à leur priorité.
    assert [e["priority"] for e in entrees[:3]] == [700, 600, -100]
    marque = re.compile(r"^(?:if \()?demarrage_\w+\(")

    def sans_marques(actions):
        return [a for a in actions
                if not ("lambda" in a and (all(marque.match(l.strip()) for l in str(a["lambda"]).strip().splitlines())
                                           or BARRE.match(str(a["lambda"]).strip())))]

    # La lambda bloquante reste seule et telle quelle ; marques avant et après.
    p700 = entrees[0]["then"]
    assert sans_marques(p700) == [{"lambda": "delay(${carte_attente_ecran_ms});"}]
    assert str(p700[0]["lambda"]).strip() == "demarrage_marquer(EtapeDemarrage::AVANT1S);"
    assert str(p700[-1]["lambda"]).strip() == "demarrage_marquer(EtapeDemarrage::APRES1S);"
    assert str(entrees[1]["then"][0]["lambda"]).strip() == "demarrage_marquer(EtapeDemarrage::P600);"
    assert str(entrees[2]["then"][0]["lambda"]).strip() == "demarrage_marquer(EtapeDemarrage::FIN);"
    # Les entrées ajoutées : des marques seules, à des priorités qu'aucune autre n'a.
    priorites = [e["priority"] for e in entrees]
    assert len(set(priorites)) == len(priorites), priorites
    for e in entrees[3:]:
        assert sans_marques(e["then"]) == [], e
    assert entrees[3]["priority"] == max(priorites), "SETUP : le premier setup()"
    # Une marque d'on_boot = une ligne qui marque, et au plus la publication.
    for e in entrees:
        for a in e["then"]:
            if "lambda" in a and re.search(r"(?<!\w)demarrage_", str(a["lambda"])):
                for ligne in str(a["lambda"]).strip().splitlines():
                    ligne = ligne.strip()
                    assert marque.match(ligne), ligne
                    assert "id(" not in ligne or ligne.endswith(f") {PUBLIER}"), ligne


def test_barre_de_chargement_dans_l_on_boot():
    """La barre s'ouvre à 500 (dalle prête), avance à 399 puis 249, se ferme au début de
    -100 : avant la première image de LVGL, qui suit setup(). Chaque appel après la marque."""
    par_priorite = {e["priority"]: [str(a.get("lambda", "")).strip() for a in e["then"]] for e in _on_boot()}
    appels = {p: [l for l in ls if BARRE.match(l)] for p, ls in par_priorite.items()}
    assert {p: len(a) for p, a in appels.items() if a} == {500: 1, 399: 1, 249: 1, -100: 1}, appels
    assert appels[500][0].startswith("if (barre_demarrage_ouvrir(id(tab5_display))) {")
    assert appels[399] == ["barre_demarrage_etape(1);"]
    assert appels[249] == ["barre_demarrage_etape(2);"]
    assert par_priorite[-100][:2] == ["demarrage_marquer(EtapeDemarrage::FIN);", "barre_demarrage_fermer();"]
    for p in (500, 399, 249):
        assert par_priorite[p][0].startswith("demarrage_marquer("), p
    # Deux étapes de plus que l'ouverture : autant que de jalons (Tab5/socle/tab5_barre.h).
    jalons = re.search(r"kJalons\[\]\s*=\s*\{(.*?)\n\};", source("tab5_barre.h").read_text(encoding="utf-8"), re.S)
    assert jalons and len(re.findall(r"^\s*\{", jalons.group(1), re.M)) == 3


def test_marques_de_setup_sans_publication():
    lignes = [l for t in _yaml_firmware().values() for l in t.splitlines()
              if "demarrage_marquer(EtapeDemarrage::" in l]
    for etape in _etapes():
        if etape in HORS_YAML:
            continue
        ligne = next(l for l in lignes if f"demarrage_marquer(EtapeDemarrage::{etape})" in l)
        if etape in PUBLIENT:
            assert ligne.strip().strip("'").endswith(PUBLIER), (etape, ligne)
        else:
            assert "publier" not in ligne, (etape, ligne)
    diag = _yaml(source(DIAG))
    usb = next(s for s in diag["switch"] if s.get("id") == "usb_5v_power")
    assert usb["restore_mode"] == "ALWAYS_ON"
    assert "demarrage_marquer(EtapeDemarrage::EXPANDEUR)" in _texte(usb["on_turn_on"])
    assert "demarrage_marquer(EtapeDemarrage::DESSIN)" in _texte(diag["lvgl"]["on_draw_start"])
    retro = next(o for o in _yaml(source("tab5-hardware.yaml"))["output"] if o.get("id") == "backlight_plafonne")
    assert "if (state > 0.0f) demarrage_marquer(EtapeDemarrage::RETRO);" in _texte(retro["write_action"])


def test_style_repere_en_premier():
    """« objets » : le premier style créé par ESPHome, avant les widgets, posé sur aucun."""
    lvgl = _yaml(source("tab5-styles.yaml"))["lvgl"]
    premier = lvgl["style_definitions"][0]
    assert premier["id"] == "style_repere_demarrage"
    assert "demarrage_marquer(EtapeDemarrage::OBJETS);" in str(premier["text_letter_space"])
    corpus = "\n".join(t for nom, t in _yaml_firmware().items() if nom != "tab5-styles.yaml")
    assert "style_repere_demarrage" not in corpus


def test_lvgl_prepare_avant_les_widgets():
    """La même lambda, et elle seule, prépare LVGL avant les widgets (16/10/2026,
    [AI-WARNING] de tab5_demarrage.cpp) : sans cet appel, setup() redevient quadratique
    en nombre de labels."""
    lvgl = _yaml(source("tab5-styles.yaml"))["lvgl"]
    lambda_ = str(lvgl["style_definitions"][0]["text_letter_space"])
    # Après la marque « objets » : la chronologie reste comparable à celle d'avant.
    assert lambda_.index("demarrage_marquer(EtapeDemarrage::OBJETS);") < lambda_.index("demarrage_preparer_lvgl();")
    appels = [nom for nom, t in _yaml_firmware().items() if "demarrage_preparer_lvgl" in t]
    assert appels == ["tab5-styles.yaml"]
    cpp = source("tab5_demarrage.cpp").read_text(encoding="utf-8")
    corps = cpp[cpp.index("void demarrage_preparer_lvgl() {"):]
    assert cpp.count("void demarrage_preparer_lvgl() {") == 1
    assert "lv_display_set_resolution(disp, kEcranL, kEcranH);" in corps
    # Le vidage est demandé par lvgl_vidage_abonnes(), partagé avec la bascule de thème
    # (17/10/2026) : compté, branché à la première demande, retiré à la dernière fin.
    assert "lvgl_vidage_abonnes(true);" in corps
    vidage = cpp[cpp.index("void lvgl_vidage_abonnes(bool actif) {"):]
    assert "lv_display_add_event_cb(disp, vider_abonnements, LV_EVENT_REFR_REQUEST, disp);" in vidage
    assert "lv_display_remove_event_cb_with_user_data(disp, vider_abonnements, disp);" in vidage
    # Rendu à la première image : rien ne change ensuite (sauf une bascule de thème en cours).
    assert "lv_display_add_event_cb(disp, fin_du_vidage, LV_EVENT_REFR_START, disp);" in corps
    fin = cpp[cpp.index("void fin_du_vidage(lv_event_t* e) {"):]
    assert "lvgl_vidage_abonnes(false);" in fin[:fin.index("\n}\n")]


def test_capteurs_et_script():
    diag = _yaml(source(DIAG))
    capteurs = {t.get("id"): t for t in diag["text_sensor"]}
    for cid, nom in (("sys_chronologie", "Tab5 Chronologie du démarrage"),
                     ("sys_chronologie_suite", "Tab5 Chronologie du démarrage (suite)")):
        capteur = capteurs[cid]
        assert capteur["platform"] == "template"
        assert capteur["name"] == nom
        assert capteur["entity_category"] == "diagnostic"
        assert capteur["update_interval"] == "never"
    script = next(s for s in diag["script"] if s["id"] == "tab5_chronologie_publier")
    code = script["then"][0]["lambda"]
    assert "char buf[kChronoTexteMax];" in code
    assert "demarrage_texte(PartieChrono::SETUP, buf, sizeof(buf));" in code
    assert "demarrage_texte(PartieChrono::SUITE, buf, sizeof(buf));" in code
    textes = _yaml_firmware()
    for cid in ("sys_chronologie", "sys_chronologie_suite"):
        assert f"id({cid}).publish_state(buf);" in code
        n = sum(t.count(f"id({cid}).publish_state(") for t in textes.values())
        assert n == 1, f"un seul endroit publie {cid}"
    status = next(b for b in diag["binary_sensor"] if b.get("id") == "status_ha")
    assert f"if (x && demarrage_marquer(EtapeDemarrage::API)) {PUBLIER}" in _texte(status["on_state"])
    assert lire(RACINE).count("demarrage_noter_ordo(millis());") == 1
