# Tâches de Refonte AI-First

## Phase 0 : Audit et Préparation (DONE)
- [x] Compilation des fichiers du projet
- [x] Génération de l'audit via Claude 3.5 Sonnet
- [x] Génération de l'audit via Z-AI (GLM-4-Plus)
- [x] Synthèse et plan d'implémentation validés

## Phase 1 : Documentation Active (DONE)
- [x] Ajouter l'entête `[AI-CONTEXT]` dans `tab5_custom.cpp`
- [x] Ajouter l'entête `[AI-CONTEXT]` dans `tab5-sensors.yaml`
- [x] Ajouter l'entête `[AI-CONTEXT]` dans `tab5-api-logic.yaml`
- [x] Ajouter les entêtes `[AI-CONTEXT]` dans les 6 autres fichiers YAML/h
- [x] Placer un `[AI-WARNING]` sur la rustine `delay(1000)` (Boot I2C)
- [x] Placer un `[AI-WARNING]` sur la lambda du Swipe LVGL

## Phase 2 : Validation et Sauvegarde (DONE)
- [x] Valider l'état stable partiel via Git
- [x] Re-valider via Git après la fin de la Phase 1

## Phase 3 : Refactorisation LVGL / C++ (DONE)
- [x] Créer les fonctions Helpers UI dans le C++ : `update_temp_ui()` (factorise `temp_serre`/`temp_salon`) + `update_console_diagnostics_ui()` (factorise la lambda `interval: 2s` de la console diagnostic, ~100 lignes)
- [x] Nettoyer les lambdas kilométriques dans `tab5-sensors.yaml` (cf. ci-dessus)
- [x] **NOUVEAU** : Déplacer la logique du Swipe LVGL de `tab5-lvgl.yaml` vers `handle_swipe_gesture()` dans `tab5_custom.cpp` (comportement identique, `esphome compile` OK)

## Phase 4 : Optimisation Mémoire (DONE)
- [x] Supprimer `std::string` dans les boucles de parsing : `parse_and_update_heures_bulk`/`parse_and_update_jours_bulk` utilisaient `std::string s = payload;` (copie heap jusqu'à 2048 octets) → buffer stack `char[2049]`, mirroir du fix déjà appliqué à `tab5_maj_alerte_meteo_france` ; suppression des `std::string ip`/`ssid` intermédiaires dans l'interval 2s de `tab5-sensors.yaml` (`.c_str()` direct sur `.state`)
- [x] **NOUVEAU** : Appliquer le dictionnaire `UIColor` existant partout dans le code : seul `0x4A596E` correspondait exactement à une valeur déjà nommée (`UIColor::CLIM_TRACK_INACTIVE`) → remplacé dans `tab5-api-logic.yaml` (`tab5_maj_pluie_1h`) et `tab5_custom.cpp`. Les autres hex restants (icônes météo, teintes vigilance/date) n'ont pas d'équivalent existant dans `UIColor` — les y ajouter étendrait le dictionnaire plutôt que de l'appliquer ; décision documentée dans `contexte_ia/04_Projets/etat_tab5.md`, à traiter comme chantier distinct si souhaité.
