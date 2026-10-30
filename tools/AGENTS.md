# AGENTS.md — `tools/` (CI, tests, guards, demo, render)

Read with the root [`AGENTS.md`](../AGENTS.md). Claude Code loads this file (through the `CLAUDE.md` next to it) only once it reads or edits a file of this folder. The tools themselves are listed in [`tools/README.md`](README.md).

## CI

`esphome compile` (schema + C++ compile) is the correctness gate for the firmware itself. CI (`.github/workflows/esphome-tab5.yml`) runs on every PR and on pushes to `main`: a `python` job (pre-commit hooks, pytest, the real Go, chess, draughts and alarm engines built with g++ — chess and draughts under ASan + UBSan —, demo dry-run) and the same compile with a throwaway signing key (`openssl genrsa`, no `secrets.yaml` since lot 6b) **only when `tab5-ha-hmi.yaml`, `Tab5/` (Markdown excluded) or the workflow change**. `python`, `build` and `build-min` are required checks; `build` stays present and reports "skipped" (= success) on docs-only changes; `workflow_dispatch` forces a compile. The compile deliberately uses ESPHome `latest` (free upstream canary, [ADR-0016](../docs/decisions/0016-ci-esphome-latest-canary.md)) and keeps its ccache between runs; a `build-min` job compiles the same configuration with the ESPHome version read from `min_version:` in `tab5-ha-hmi.yaml` (required too since 2026-09-26). `main` publishes the `tab5-firmware` artifact. A non-required job, `sanitizers.yml`, compiles the virtual tablet (`tab5-rendu-host.yaml`) with ASan + UBSan and fuzzes every service of the contract (one seed each, kept equal to the declared services by `tests/test_sanitizers.py`), replays the out-of-range cases with the popups open and opens every screen; it fails on the first report read from the tablet's own output (`tools/sanitizers/`), after a positive control — read it before merging a C++ change.

## The second board: publication and render

**Second board, the Waveshare ESP32-P4 7B** (1024×600, supported since 3.9.0, [ADR-0061](../docs/decisions/0061-board-and-screen-size.md), [`docs/waveshare-7b.md`](../docs/waveshare-7b.md)): `python -m esphome -s tab5_carte waveshare-7b compile tab5-ha-hmi.yaml`, ESPHome **2026.9.1** at least (a `static_assert` in `Tab5/ecran/carte_7b_version.h` stops an older one). Its firmware is published with each release since 3.9.0-rc.6 (variant `waveshare-7b` of `.github/workflows/publication.yml`, built with `ESPHOME_PUBLICATION_CARTES`, the Tab5 keeping `ESPHOME_PUBLICATION`) and installs from the install page; a published firmware reads its updates from `<channel>/waveshare-7b/`, never from a Tab5 folder (`Tab5/paquets/publication-commune.yaml`, `tests/test_publication.py`). Its off-device render is the `rendu-1024x600` artifact (job `rendu-7b`): its JSON counts, per screen, the objects cut by or outside the screen (compare with the Tab5's own counts: some popups clip on both). The games module (`tab5_jeux`, absent by default on the 7B): [`Tab5/jeux/AGENTS.md`](../Tab5/jeux/AGENTS.md).

## Tests and guards

There is no unit test suite *for the HMI*, but three game engines have host tests (Go, chess, draughts), and eight content guards read the real C++/YAML (modal chrome per ADR-0009, single registry per ADR-0013, code rules, Marble rooms traversable, Lode levels playable, Arkanoid levels complete and winnable, Trivia question bank complete, `CARTOGRAPHIE_TAB5.md` line counts). Everything runs on a plain PC with no toolchain, in one `pytest` (config in `pyproject.toml`, deps in `requirements-dev.txt`):

```bash
pip install -r requirements-dev.txt # une fois : pytest, numpy (garde-fou Marble), aioesphomeapi, fonttools, pyserial, pre-commit, yamllint
python -m pytest                    # tout : tests/ (outils + garde-fous) + tools/ (miroirs des moteurs d'échecs et de dames)
pre-commit install                  # une fois : hooks yamllint / BOM / secrets / placeholders HA avant chaque commit
pre-commit run --all-files          # les mêmes garde-fous sur tout le dépôt (la CI les rejoue)
python tools/test_chess_perft.py    # générateur d'échecs contre la suite perft standard
python tools/test_draughts_engine.py # générateur de dames (10×10 et 8×8) contre les perft de référence
python tools/render_ha_config.py --check   # ni identifiant HA réel ni placeholder dans un fichier public (local : lit placeholders.yaml)
python tools/check_tab5_modal_chrome.py    # ADR-0009 : chrome modal partagé sur chaque popup
python tools/check_marble_rooms.py         # les 6 salles de Fil d'Or restent traversables
python tools/check_lode_levels.py          # les 10 niveaux de Coureur d'Or restent jouables
python tools/check_arkanoid_levels.py      # les 8 niveaux d'Arcanoïde complets, aucune brique emmurée
python tools/check_trivia_questions.py     # banque de Trial Poursuite : ni entrée manquante, ni question vide ou en double
python tools/check_tab5_registry.py        # ADR-0013 : registre unique consoles/modales
python tools/check_tab5_code_rules.py      # règles de code (snprintf, pas de lv_* dans le contrat, pas d'entité HA en dur, couleurs par la palette…)
python tools/cartographie_counts.py        # comptes de lignes de la cartographie (--write pour les recalculer)
```

The reference tests are C++, compiled against the real engines by the CI `python` job with g++ on every PR (the dev box has no host g++): `tools/test_go_engine.cpp` (`go_engine.cpp`, the only Go test since 2026-10-08), `tools/test_chess_engine.cpp` (`chess_ai.cpp`, perft suite) and `tools/test_draughts_engine.cpp` (`draughts_engine.cpp`, the pure `Draughts::Engine` module, built as is, perft and rules), the last two under ASan + UBSan; `tools/hote/esphome.h` is the minimal ESPHome header the chess test needs (the draughts engine needs none). The chess and draughts **Python mirrors** stay for a PC without g++: a change to the C++ must be mirrored there, and `tests/test_moteurs_hote.py` keeps their perft values equal to the C++ tests and checks that CI builds and runs every `tools/test_*.cpp`. Shared test helpers live in `tests/commun.py` (paths, `lire()`, `sources()` / `source()` to find a firmware file wherever it is filed (`tools/tab5_sources.py`, used by the tools too), YAML loaders, `bloc_service()`, a session Jinja cache) and `tests/conftest.py` puts the tool folders on `sys.path` once: do not add a `sys.path.insert` to a test file. `tools/test_alarm_clock.cpp` does the same for the **alarm engine** (`alarm_clock.cpp` + `tab5_core.cpp`, both free of ESPHome/LVGL): simulated clock through `tab5_time_source`, Europe/Paris DST, calendar anchor day. Keep those two files free of `esphome.h`/`lv_*` — the render lives in `alarm_render.*`.

## The demo dry-run

`tools/demo/demo_pusher.py --dry-run` plays every demo scene against the firmware contract without any hardware: the **18 push services** the demo calls get exactly the `variables:` declared in `Tab5/paquets/tab5-api-logic.yaml` (read by the script), and their payloads follow the formats asserted in `tools/demo/scenarios.py` — cheap check after touching `tab5-api-logic.yaml`. The 14 other services (`tab5_maj_alertes_ha_bulk`, `tab5_maj_alertes_historique`, `tab5_maj_calendrier_mois`, `tab5_maj_calendrier_jour`, `tab5_maj_rdv_prochains`, `tab5_assist_reponse`, `tab5_maj_reponse_vocale`, `tab5_maj_lecteur`, `tab5_maj_musique_pieces`, `tab5_maj_musique_liste`, `tab5_maj_cameras`, `tab5_maj_suivi`, `tab5_maj_froid`, `tab5_maj_planning`) are out of its scope by design (they need live HA entities, a calendar, media players, Music Assistant, a camera, the recorder's statistics or a voice pipeline). `tab5_maj_planning` is obsolete since 2026-09-08 and has no caller: none in this repository (packages, blueprint, snippets, demo), none in the author's live Home Assistant (read-only search of its `config/` share on 2026-10-08: comments and `.bak` files only). The tablet derives the planning banner from the daily forecast; a text pushed through the old service is replaced at the next `tab5_maj_previsions_jours_bulk` or theme change (`planning_rejouer_theme()`). It stays declared, and fuzzed by `tools/sanitizers/fuzz_services.py`, until a future major version removes it. `tests/test_contrat.py` checks the other callers (HA packages, blueprint, snippets, off-device render) the same way, and the fields of the `esphome.tab5_*` events.

## The documentation site

See [`docs/README.md`](../docs/README.md) for the documentation by task (installation, user manual, screens, voice assistant, troubleshooting, architecture, hardware, decisions): it is an index, not a complete list. Every published page is in the website's menu, [`tools/site/menu.yml`](site/menu.yml) (the only list of pages; [ADR-0030](../docs/decisions/0030-documentation-site.md)), with a search; its home page is the root `README.md`. The pages kept off the site on purpose are listed in `HORS_SITE` (`tests/test_site_doc.py`). The `AGENTS.md` / `CLAUDE.md` files of the folders are outside `docs/`, so the site ignores them.

## Talking to the device

Do not leave more than a couple of ESPHome CLI processes running against the device at once — the API only has 8 connection slots; a past session's leaked `esphome` processes silently starved the real device of connections.

## Secrets and the API key

Since 3.0 (lot 6b, [ADR-0020](../docs/decisions/0020-no-secret-firmware-signed-ota.md)) the firmware reads no `!secret` at all — a `secrets.yaml` left from 2.x is only used once, by `tools/migrer_vers_3.py`, to send the first 3.0 firmware with the old API key. Never add a `!secret` back (`tests/test_sans_secret.py` fails). The API key lives in Home Assistant: tools that talk to the tablet get it through `tools/tab5_cle_api.py` and never print it.
