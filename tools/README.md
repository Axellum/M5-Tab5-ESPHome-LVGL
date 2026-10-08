# Outils du dépôt (`tools/`)

Une ligne par outil, tirée de son en-tête (à lire avant de s'en servir : usage, options, pièges). Tout se lance depuis la racine du dépôt. `python -m pytest` joue déjà les tests et les garde-fous : il n'y a rien à lancer à la main pour vérifier un changement, sauf ce que dit la colonne « Quand ».

## Ce qui réécrit un fichier dès qu'on le lance

Ces outils écrivent dans le dépôt **sans option** : à lancer exprès, puis relire le diff. Leur option `--check` (quand elle existe) ne fait que comparer, et `pytest` la joue.

| Outil | Réécrit | Quand |
|---|---|---|
| `gen_i18n.py` | `Tab5/tab5_i18n_data.h`, depuis `Tab5/lang/*.yaml` | après un nouveau `tr("…")` ou une traduction (`--check` : à jour ?) |
| `gen_themes.py` | `Tab5/tab5_themes_data.h` et les parties générées de `Tab5/tab5_tokens.h`, `Tab5/tab5-themes.yaml` et `Tab5/tab5_theme.cpp`, depuis `Tab5/themes/*.yaml` | après un thème ou un rôle de couleur (`--check`) |
| `gen_tuiles_icones.py` | `Tab5/tab5_tuiles_icones.h`, les glyphes de trois polices MDI de `Tab5/tab5-styles.yaml`, deux tables du blueprint | après `Tab5/tuiles_icones.yaml` (`--check`) |
| `police_theme.py` | `Tab5/themes/_polices.yaml` (télécharge et mesure les polices Google Fonts des thèmes) | après une police de thème, avant `gen_themes.py` |
| `cartographie_counts.py --write` | les comptes de lignes de `CARTOGRAPHIE_TAB5.md` | quand `pytest` le demande (sans `--write` : vérifie seulement) |
| `render_ha_config.py` | `HomeAssistant_Config/rendered/` (gitignoré) | copie déployable ; `--check` = garde-fou de fuite, n'écrit rien |
| `make_chess_font.py` | `Tab5/ChessPieces.ttf` (12 glyphes) | seulement si les pièces d'échecs changent |
| `rendu/maj_references.py` | les PNG de `docs/images/rendu/` (télécharge les captures d'un run) | quand un changement d'écran voulu doit devenir la référence |
| `site/images_notice.py` | les images WebP de la notice (`docs/images/notice/`), depuis les captures d'un run du rendu | après un changement d'écran montré par la notice |

## Garde-fous (joués par `pytest` ou par les hooks pre-commit)

| Outil | Vérifie |
|---|---|
| `check_tab5_code_rules.py` | les règles de code du firmware (`snprintf`, pas de `lv_*` dans le contrat, pas d'entité HA en dur, glyphes MDI, couleurs par la palette…) |
| `check_tab5_modal_chrome.py` | chaque popup utilise le chrome partagé (ADR-0009) |
| `check_tab5_registry.py` | une seule liste des consoles et des fenêtres modales (ADR-0013) |
| `check_marble_rooms.py` | les 6 salles de « Fil d'Or » restent traversables |
| `check_lode_levels.py` | les 10 niveaux de « Coureur d'Or » restent jouables |
| `check_arkanoid_levels.py` | les 8 niveaux d'« Arcanoïde » complets, aucune brique emmurée |
| `check_trivia_questions.py` | la banque de « Trial Poursuite » : ni entrée manquante, ni question vide ou en double |
| `cartographie_counts.py` | les comptes de lignes de `CARTOGRAPHIE_TAB5.md`, à 20 % près |
| `verifier_secrets_config.py` | aucun secret en clair dans un fichier suivi (hook pre-commit) |
| `i18n_keys.py` | relève les clés de `tr()` du firmware ; lu par `tests/test_i18n.py`, lancé seul il liste les clés manquantes de chaque langue |

## Tests des moteurs (sans la tablette)

| Outil | Vérifie |
|---|---|
| `test_go_engine.cpp` | règles du Go contre le vrai `go_engine.cpp` (g++, job `python` de la CI) ; seul test du Go depuis le 08/10/2026 |
| `test_chess_engine.cpp` | le vrai `chess_ai.cpp` contre la suite perft et une recherche courte, sous ASan + UBSan (g++, job `python`) |
| `test_draughts_engine.cpp` | le `Draughts::Engine` de `draughts_game.cpp` contre les perft 10×10 et 8×8 et les règles, sous ASan + UBSan (g++, job `python`) |
| `test_chess_perft.py` | miroir Python de `test_chess_engine.cpp`, pour un poste sans g++ |
| `test_draughts_engine.py` | miroir Python de `test_draughts_engine.cpp`, pour un poste sans g++ |
| `test_alarm_clock.cpp` | le vrai moteur du réveil, `tab5_core` et `tab5_economie.h`, horloge simulée (g++, job `python` de la CI) |
| `test_tab5_socle.cpp` | le socle commun `tab5_champs` + `tab5_core` (payloads, dates, heures, géométrie, tuiles) (g++, job `python` de la CI) |

Le test C++ fait foi. Un miroir Python ne prouve le C++ que s'il est tenu à jour à chaque changement du C++ ; `tests/test_moteurs_hote.py` tient ses perft égaux à ceux du test C++ et vérifie que la CI compile et lance chaque `test_*.cpp`.

## Parler à la tablette depuis le PC

| Outil | Rôle |
|---|---|
| `tab5_cle_api.py` | trouve la clé API de la tablette (gardée par Home Assistant depuis la 3.0) sans jamais l'afficher ; utilisé par les deux suivants |
| `tab5_logs.py` | les journaux de la tablette, comme `esphome logs`, avec cette clé |
| `capture_serie.py` | écoute le port série USB sans réinitialiser la puce, repère un plantage et décode la pile avec l'ELF (`--elf`) |
| `improv_serie.py` | règle le Wi-Fi d'une tablette par Improv sur l'USB |
| `migrer_vers_3.py` | passe une tablette 2.x à la 3.0 par le réseau (seul outil qui lit encore un ancien `secrets.yaml`) |

## Sous-dossiers

| Dossier | Rôle |
|---|---|
| `ci/` | `pip_reessai.sh` : `pip install` avec réessais, utilisé par tous les workflows |
| `demo/` | mode démo : `demo_pusher.py` pousse des données synthétiques à une tablette sans HA ; `--dry-run` vérifie les payloads contre le contrat sans matériel ; `scenarios.py` (données, module pur) |
| `hote/` | compiler un moteur sur PC : `esphome.h` minimal (journal, `millis()`), `extraire_moteur_dames.py` (bloc `Draughts::Engine` de `draughts_game.cpp`, sans LVGL) |
| `installation_ha/` | job « installation dans un HA neuf » : `preparer_config.py` (dossier `config/` d'un HA neuf), `verifier_installation.py` (installation et vérifications), `verifier_integration.py` (intégration HACS, ADR-0035), `captures_ha.py` (images du guide d'installation) |
| `publication/` | release : `preparer.py` (binaires d'une révision d'écran), `archive_ha.py` (`tab5_home_assistant.zip`), `archive_hacs.py` (`tab5_hacs.zip`), `pages.py` (site GitHub Pages, manifestes de mise à jour), `meme_code.py` (une recompilation a-t-elle le même code qu'une image publiée ?) |
| `rendu/` | rendu hors tablette (ADR-0021) : `capturer.py` (captures des scènes et des écrans), `ecrans.py` (plan des écrans), `comparer.py` (comparaison aux références), `maj_references.py` (voir plus haut) |
| `sanitizers/` | tablette virtuelle sous ASan + UBSan : `variante.py` et `pio_drapeaux.py` (compilation), `fuzz_services.py` (fuzz des services), `cibles_ub.py` (cas ciblés, fenêtres ouvertes), `rapports.py` (lecture des rapports), `temoin.cpp` (témoin positif) |
| `site/` | site de documentation (ADR-0030) : `construire.py` (MkDocs, en/ et fr/), `menu.yml` (seule liste des pages publiées), `images_notice.py` (voir plus haut) |
