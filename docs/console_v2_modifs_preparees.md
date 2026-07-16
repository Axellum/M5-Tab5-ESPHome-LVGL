# Console Système v2 — modifications d'accompagnement (✅ TOUTES APPLIQUÉES le 16/07/2026)

> Refonte de `Tab5/ui_components/console_sys.yaml` (2026-07-16). Ce document
> listait les modifications préparées hors popup ; sur validation d'Axel, elles
> ont **toutes été appliquées** le jour même. Conservé comme trace de conception.
> Vérification : `esphome config` valide + `esphome clean` + `compile` SUCCESS.

## 1. ✅ État connexion HA dans la carte RÉSEAU — APPLIQUÉ

`Tab5/tab5-sensors-diagnostics.yaml`, interval `2s`, dans le bloc
`if (is_console_layer_visible(...))` : `lbl_sys_ha_val` affiche
`Connecte` (vert) / `Hors ligne` (rouge) d'après `status_ha`.
Après un « Redémarrer HA » confirmé, la popup écrit `Redemarrage...` (orange) ;
ce snippet reprend la main au tick suivant (rouge pendant la coupure, vert au
retour de l'API).

## 2. ✅ Rafraîchissement immédiat de l'état HA à l'ouverture — APPLIQUÉ

`Tab5/tab5-lvgl.yaml`, `btn_control_console` → `on_short_click` : même bloc
qu'au §1 ajouté en fin de lambda (évite le `--` pendant jusqu'à 2 s).

## 3. ✅ Nettoyage du global `reboot_armed` — APPLIQUÉ

`Tab5/tab5-globals.yaml` : bloc `reboot_armed` supprimé (plus aucun
lecteur/écrivain — l'armement passe par la visibilité de
`overlay_confirm_reboot`). Commentaire d'en-tête reformulé (l'exemple
historique du bug reste cité). Table des globals de `Tab5/README.md` à jour.

## 4. ✅ Migration des entity_id vers `user_entities.yaml` — APPLIQUÉ

`entity_primary_active` (= `input_boolean.is_primary_active`) et
`entity_push_automation` (= `automation.maj_ecran_tab5_esphome_push`) ajoutés
dans `Tab5/user_entities.yaml` **et** `Tab5/user_entities.example.yaml` ;
`console_sys.yaml` référence `${entity_primary_active}` /
`${entity_push_automation}`.

## 5. ✅ Docs resynchronisées — APPLIQUÉ

`Tab5/README.md` (ui_components, table globals, règle n°3), `docs/screens.md`
(EN+FR), `docs/architecture.md` (EN+FR), `docs/debugging.md`,
`CARTOGRAPHIE_TAB5.md` (copie repo + copie `contexte_ia/02_Hardware/`),
`CHANGELOG.md` (entrée 2026-07-16).

## Vérifications avant flash (toujours valables)

1. `esphome clean` obligatoire (`!include` modifié) puis compile/OTA depuis
   `00ProjetTab/` (source canonique — jamais le partage HA, cf. #T198).
2. Option « Autoriser les appels de service » de l'intégration ESPHome dans HA :
   déjà active (fix du 04/07) — requise pour les 4 boutons GESTION.
3. Entités HA utilisées par « MAJ Écran » : `input_boolean.is_primary_active`
   et `automation.maj_ecran_tab5_esphome_push` (vérifiées dans
   `HomeAssistant_Config/automations_tab5.yaml`, alias « MAJ Ecran Tab5
   ESPHome Push »).
4. Services HA appelés : `homeassistant.restart`, `automation.reload`,
   `automation.trigger`, `input_boolean.turn_on` (natifs, rien à créer).
