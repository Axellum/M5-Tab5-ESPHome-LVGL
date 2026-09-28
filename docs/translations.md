# Screen language & translations

## English · [Français](#version-française)

---

The Tab5 screen speaks **French** (the source language), **English**, **German**, **Dutch**, **Spanish** or **Italian**. Everything the screen shows or the tablet says follows the chosen language: popups, cards, dates, the alarm clock and its spoken reminders, and the eight games. Two exceptions: the **quiz questions** of Trial Poursuite stay French (more than 720 of them), and the **console names** (Fil d'Or, Roi Noir…) are proper names, kept as they are.

The German, Dutch, Spanish and Italian translations were made with an AI (Claude), from the French and English texts, and no native speaker has reviewed them yet. Corrections are welcome: an issue, or a pull request on `Tab5/lang/de.yaml`, `nl.yaml`, `es.yaml` or `it.yaml`.

## Choosing the language

- **From Home Assistant:** the tablet exposes a select entity **« Langue »** (Configuration). Pick `Français`, `English`, `Deutsch`, `Nederlands`, `Español` or `Italiano`: the tablet restarts and comes back in the new language. It remembers the choice.
- **Spoken sentences** (alarm reminders, « shutter stopped ») follow the screen language, but the voice that reads them is the one of the tablet's voice pipeline in Home Assistant: give it a voice of the same language.
- **Default of a first boot:** `tab5_langue: English` in `Tab5/user_entities.yaml` (the native name of the language). Without that line, French.

What does **not** change: entity names, select options and the states Home Assistant reads (for example « Heure fixe », « Écran courant »). Renaming them would break your history and automations, so they stay as they are in every language. Since lot 4c, Home Assistant no longer sends sentences: it sends **codes** (rain level and start time, update and error counts…), and the tablet writes the sentence in its own language — « Averses dans 12 mn » or “Showers in 12 min”, counted down on the tablet's clock. Forecast day labels are translated when shown. What stays as HA sends it: your own data (calendar event titles, update names, sensor names).

## How it works

It works like gettext: **the French text written in the code is the key.**

- In C++ and YAML lambdas, a displayed text goes through `tr("Calendrier")`, which returns `Calendar` in English and the French text itself in French. In a language that is not complete yet, a missing text shows in English (French if English lacks it too): `tools/gen_i18n.py` writes that fallback into the generated tables.
- Texts laid out by the YAML (`text: "Calendrier"`) keep their French text; `i18n_apply_boot()` translates them once at the end of the setup, before the first frame.
- A word with two meanings gets a context: `tr_ctx("mardi", "M")` → `T`, `tr_ctx("mercredi", "M")` → `W`, `tr_ctx("echecs", "Dame")` → `Queen` but `tr_ctx("dames", "Dame")` → `King`.
- Word order that changes with the language goes through a template: `tr_fill("{jour} {quantieme} {mois}", …)` → `{jour}, {mois} {quantieme}` in English.
- A text kept in a table (`static const char* const kModes[] = {…}`) is marked `tr_noop("Joueur contre Tab")` and translated where it is shown, `tr(kModes[i])`. `tr_noop()` translates nothing; it only tells `tools/i18n_keys.py` that the text is a key.

Each language is one file, `Tab5/lang/<code>.yaml`: a flat mapping `"French text": "translation"`. `tools/gen_i18n.py` turns them into `Tab5/tab5_i18n_data.h` (generated, committed).

## Adding a language

1. Copy `Tab5/lang/en.yaml` to `Tab5/lang/<code>.yaml` (for example `pt.yaml`).
2. Change `_langue` (the native name, as shown in the select: `Português`), `_code` (`pt`) and `_index` (**the next free number**: the tablet stores the index, so existing languages never move). Remove `_statut: complet` until the translation is complete: missing texts then show in English, so a partial language is usable (English stays complete for that reason).
3. Translate the right-hand side of each line. Keep the `%d`/`%s` in the same order and the `{names}` as they are.
4. Add the native name at the **end** of the `options:` of the select « Langue » (`Tab5/tab5-ha-controls.yaml`).
5. Run:

   ```bash
   python tools/gen_i18n.py        # regenerates Tab5/tab5_i18n_data.h
   python tools/i18n_keys.py       # lists the texts still missing, per language
   python -m pytest tests/test_i18n.py
   ```

**Characters:** the screen fonts carry Latin-1 plus the Windows-1252 punctuation (`&latin1` in `Tab5/tab5-styles.yaml`). That covers English, German, Spanish, Italian, Portuguese, Dutch and the Nordic languages. Polish, Czech, Turkish, Cyrillic or Greek need that glyph set extended first (it costs flash on every text font); `tests/test_i18n.py` refuses a translation whose characters the fonts don't have — they would show as empty boxes.

## Rules for contributors

- A new text **on the screen** is written in French in the code, wrapped in `tr()` (C++, lambdas) or laid out by the YAML, and gets its line in each complete language file. `tests/test_i18n.py` fails if a complete language (English, German, Dutch, Spanish, Italian) misses it. Keep the literal on the same line as `tr(`: the key finder reads one line at a time.
- The French text itself must use only characters the fonts carry: the tests check French too.
- Never translate what Home Assistant reads: entity `name:`, select options, text-sensor states, payload codes (`En_mouvement`, `Rouge`…). A HA value shown on screen is translated **at display time**: `tr(state.c_str())`.
- A key that no longer matches any text of the code fails the tests: change the key when you change the French text.

---

## Version Française

L'écran du Tab5 parle **français** (la langue source), **anglais**, **allemand**, **néerlandais**, **espagnol** ou **italien**. Tout ce que l'écran affiche ou que la tablette dit suit la langue choisie : popups, cartes, dates, le réveil et ses rappels parlés, et les huit jeux. Deux exceptions : les **questions du quiz** de Trial Poursuite restent en français (plus de 720), et les **noms des consoles** (Fil d'Or, Roi Noir…) sont des noms propres, gardés tels quels.

Les traductions allemande, néerlandaise, espagnole et italienne ont été faites par une IA (Claude), à partir des textes français et anglais, et aucune personne dont c'est la langue ne les a encore relues. Les corrections sont bienvenues : une issue, ou une pull request sur `Tab5/lang/de.yaml`, `nl.yaml`, `es.yaml` ou `it.yaml`.

## Choisir la langue

- **Depuis Home Assistant :** la tablette expose un select **« Langue »** (Configuration). Choisissez `Français`, `English`, `Deutsch`, `Nederlands`, `Español` ou `Italiano` : la tablette redémarre et revient dans la nouvelle langue. Elle garde ce choix.
- **Les phrases parlées** (rappels du réveil, « Volet arrêté ») suivent la langue de l'écran, mais la voix qui les lit est celle du pipeline vocal de la tablette dans Home Assistant : donnez-lui une voix de la même langue.
- **Au premier démarrage :** `tab5_langue: English` dans `Tab5/user_entities.yaml` (le nom de la langue dans la langue elle-même). Sans cette ligne, le français.

Ce qui **ne change pas** : les noms d'entités, les options de select et les états que lit Home Assistant (par exemple « Heure fixe », « Écran courant »). Les renommer casserait votre historique et vos automatisations : ils restent tels quels dans toutes les langues. Depuis le lot 4c, Home Assistant n'envoie plus de phrases mais des **codes** (niveau et heure de début de la pluie, nombre de mises à jour et d'erreurs…), et la tablette écrit la phrase dans sa langue — « Averses dans 12 mn » ou “Showers in 12 min”, décomptée avec sa propre horloge. Les libellés des jours des prévisions sont traduits à l'affichage. Ce qui reste tel que HA l'envoie : vos propres données (titres des événements du calendrier, noms des mises à jour et des capteurs).

## Comment ça marche

Comme gettext : **le texte français écrit dans le code est la clé.**

- En C++ et dans les lambdas YAML, un texte affiché passe par `tr("Calendrier")`, qui rend `Calendar` en anglais, et le texte français lui-même en français. Dans une langue pas encore complète, un texte manquant s'affiche en anglais (en français si l'anglais ne l'a pas non plus) : `tools/gen_i18n.py` écrit ce repli dans les tables générées.
- Les textes posés par le YAML (`text: "Calendrier"`) gardent leur texte français ; `i18n_apply_boot()` les traduit une fois en fin de setup, avant la première image.
- Un mot à deux sens reçoit un contexte : `tr_ctx("mardi", "M")` → `T`, `tr_ctx("mercredi", "M")` → `W`, `tr_ctx("echecs", "Dame")` → `Queen` mais `tr_ctx("dames", "Dame")` → `King`.
- Un ordre des mots qui change avec la langue passe par un modèle : `tr_fill("{jour} {quantieme} {mois}", …)` → `{jour}, {mois} {quantieme}` en anglais.
- Un texte rangé dans une table (`static const char* const kModes[] = {…}`) est marqué `tr_noop("Joueur contre Tab")` et traduit là où il s'affiche, `tr(kModes[i])`. `tr_noop()` ne traduit rien : il signale seulement à `tools/i18n_keys.py` que le texte est une clé.

Chaque langue est un fichier, `Tab5/lang/<code>.yaml` : un mapping plat `"texte français": "traduction"`. `tools/gen_i18n.py` en fait `Tab5/tab5_i18n_data.h` (généré, versionné).

## Ajouter une langue

1. Copiez `Tab5/lang/en.yaml` en `Tab5/lang/<code>.yaml` (par exemple `pt.yaml`).
2. Changez `_langue` (le nom dans la langue elle-même, tel qu'affiché dans le select : `Português`), `_code` (`pt`) et `_index` (**le numéro suivant** : la tablette mémorise l'index, les langues existantes ne bougent donc jamais). Retirez `_statut: complet` tant que la traduction n'est pas finie : les textes manquants s'affichent alors en anglais, donc une langue partielle est utilisable (c'est pourquoi l'anglais doit rester complet).
3. Traduisez la partie droite de chaque ligne. Gardez les `%d`/`%s` dans le même ordre et les `{noms}` tels quels.
4. Ajoutez le nom natif à la **fin** des `options:` du select « Langue » (`Tab5/tab5-ha-controls.yaml`).
5. Lancez :

   ```bash
   python tools/gen_i18n.py        # régénère Tab5/tab5_i18n_data.h
   python tools/i18n_keys.py       # liste les textes qui manquent encore, par langue
   python -m pytest tests/test_i18n.py
   ```

**Caractères :** les polices de l'écran portent le Latin-1 plus la ponctuation Windows-1252 (`&latin1` dans `Tab5/tab5-styles.yaml`). Ça couvre l'anglais, l'allemand, l'espagnol, l'italien, le portugais, le néerlandais et les langues nordiques. Le polonais, le tchèque, le turc, le cyrillique ou le grec demandent d'abord d'étendre ce jeu de glyphes (ça coûte de la flash sur chaque police de texte) ; `tests/test_i18n.py` refuse une traduction dont les polices n'ont pas les caractères — ils s'afficheraient en carrés vides.

## Règles pour contribuer

- Un nouveau texte **à l'écran** s'écrit en français dans le code, passe par `tr()` (C++, lambdas) ou est posé par le YAML, et reçoit sa ligne dans chaque fichier de langue complet. `tests/test_i18n.py` échoue si une langue complète (anglais, allemand, néerlandais, espagnol, italien) ne l'a pas. Gardez le littéral sur la même ligne que `tr(` : l'outil qui relève les clés lit ligne par ligne.
- Le texte français lui-même ne doit utiliser que des caractères des polices : les tests vérifient aussi le français.
- Ne jamais traduire ce que lit Home Assistant : `name:` d'entité, options de select, états de text_sensor, codes des payloads (`En_mouvement`, `Rouge`…). Une valeur HA affichée à l'écran se traduit **au moment de l'affichage** : `tr(etat.c_str())`.
- Une clé qui ne correspond plus à aucun texte du code fait échouer les tests : changez la clé quand vous changez le texte français.
