# Inventaire des entités ESPHome - Tab5

Cet inventaire recense les entités déclarées dans la configuration ESPHome du dépôt Tab5 et exposées à Home Assistant.

| ID | Type | Fichier | Note |
| :--- | :--- | :--- | :--- |
| `speaker_enable` | Switch (GPIO) | `tab5-sensors-domotique.yaml` | Activation haut-parleur |
| `tab5_wake_word_active` | Switch (Template) | `tab5-sensors-domotique.yaml` | État du Wake Word "Ok Nabu" |
| `headphone_detect` | Binary Sensor (GPIO) | `tab5-sensors-domotique.yaml` | Détection casque |
| `pc_status` | Text Sensor (HA) | `tab5-sensors-domotique.yaml` | Statut PC |
| `tv_status` | Text Sensor (HA) | `tab5-sensors-domotique.yaml` | Statut TV |
| `light_chambre_state` | Text Sensor (HA) | `tab5-sensors-domotique.yaml` | État lumière chambre |
| `light_salon_state` | Text Sensor (HA) | `tab5-sensors-domotique.yaml` | État lumière salon |
| `light_led_state` | Text Sensor (HA) | `tab5-sensors-domotique.yaml` | État lumière bureau/LED |
| `light_chambre_brightness` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Luminosité chambre |
| `light_salon_brightness` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Luminosité salon |
| `light_led_brightness` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Luminosité bureau/LED |
| `phone_battery` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Batterie téléphone |
| `temp_serre` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Température serre |
| `temp_salon` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Température salon |
| `temp_chambre` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Température chambre |
| `hum_salon` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Humidité salon |
| `hum_chambre` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Humidité chambre |
| `moisture_1` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Humidité plante 1 |
| `moisture_2` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Humidité plante 2 |
| `moisture_3` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Humidité plante 3 |
| `moisture_4` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Humidité plante 4 |
| `moisture_5` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Humidité plante 5 |
| `pot1_ec` | Sensor (HA) | `tab5-sensors-domotique.yaml` | EC plante 1 |
| `pot1_lux` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Lux plante 1 |
| `pot1_temp` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Temp plante 1 |
| `pot1_bat` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Batterie plante 1 |
| `pot2_ec` | Sensor (HA) | `tab5-sensors-domotique.yaml` | EC plante 2 |
| `pot2_lux` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Lux plante 2 |
| `pot2_temp` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Temp plante 2 |
| `pot2_bat` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Batterie plante 2 |
| `pot3_ec` | Sensor (HA) | `tab5-sensors-domotique.yaml` | EC plante 3 |
| `pot3_lux` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Lux plante 3 |
| `pot3_temp` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Temp plante 3 |
| `pot3_bat` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Batterie plante 3 |
| `pot4_ec` | Sensor (HA) | `tab5-sensors-domotique.yaml` | EC plante 4 |
| `pot4_lux` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Lux plante 4 |
| `pot4_temp` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Temp plante 4 |
| `pot4_bat` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Batterie plante 4 |
| `pot5_ec` | Sensor (HA) | `tab5-sensors-domotique.yaml` | EC plante 5 |
| `pot5_lux` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Lux plante 5 |
| `pot5_temp` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Temp plante 5 |
| `pot5_bat` | Sensor (HA) | `tab5-sensors-domotique.yaml` | Batterie plante 5 |
| `wifi_power` | Switch (GPIO) | `tab5-sensors-diagnostics.yaml` | Alimentation WiFi |
| `usb_5v_power` | Switch (GPIO) | `tab5-sensors-diagnostics.yaml` | Alimentation USB 5V |
| `wifi_antenna_int_ext` | Switch (GPIO) | `tab5-sensors-diagnostics.yaml` | Antenne WiFi (interne) |
| `external_5v_power` | Switch (GPIO) | `tab5-sensors-diagnostics.yaml` | Alimentation 5V externe |
| `status_ha` | Binary Sensor (Status) | `tab5-sensors-diagnostics.yaml` | Statut API HA |
| `sys_wifi_ip` | Text Sensor (WiFi) | `tab5-sensors-diagnostics.yaml` | Adresse IP Tab5 |
| `sys_wifi_ssid` | Text Sensor (WiFi) | `tab5-sensors-diagnostics.yaml` | SSID Tab5 |
| `sys_uptime` | Sensor (Uptime) | `tab5-sensors-diagnostics.yaml` | Uptime Tab5 |
| `sys_wifi_rssi` | Sensor (WiFi) | `tab5-sensors-diagnostics.yaml` | Signal WiFi RSSI |
| `sys_core_temp` | Sensor (Internal) | `tab5-sensors-diagnostics.yaml` | Température coeur |
| `sys_free_heap` | Sensor (Debug) | `tab5-sensors-diagnostics.yaml` | RAM libre |
| `sys_loop_time` | Sensor (Debug) | `tab5-sensors-diagnostics.yaml` | Temps de boucle |
| `wifi_antenna_select` | Select (Template) | `tab5-sensors-diagnostics.yaml` | Choix Antenne WiFi |
| `tab5_volume_pct` | Number (Template) | `tab5-ha-controls.yaml` | Volume système (%) |
| `tab5_screen_name` | Text Sensor (Template) | `tab5-ha-controls.yaml` | Nom de l'écran courant |
| `tab5_goto_screen` | Select (Template) | `tab5-ha-controls.yaml` | Commande navigation écran |
| `tab5_reload_calendar` | Button (Template) | `tab5-ha-controls.yaml` | Forcer recharge calendrier |
| `tab5_alarm_enabled` | Switch (Template) | `tab5-alarm.yaml` | Activation Réveil |
| `tab5_alarm_repos_fixe` | Switch (Template) | `tab5-alarm.yaml` | Réveil jours repos |
| `tab5_alarm_crescendo` | Switch (Template) | `tab5-alarm.yaml` | Volume progressif |
| `tab5_alarm_tts` | Switch (Template) | `tab5-alarm.yaml` | Annonce parlée |
| `tab5_alarm_rdv_on` | Switch (Template) | `tab5-alarm.yaml` | Annonce rendez-vous |
| `tab5_alarm_time` | Datetime (Template) | `tab5-alarm.yaml` | Heure du réveil |
| `tab5_alarm_early` | Datetime (Template) | `tab5-alarm.yaml` | Heure limite avant |
| `tab5_alarm_late` | Datetime (Template) | `tab5-alarm.yaml` | Heure limite après |
| `tab5_alarm_lead` | Number (Template) | `tab5-alarm.yaml` | Délai avant ouverture |
| `tab5_alarm_rest_hours` | Number (Template) | `tab5-alarm.yaml` | Repos mini après fermeture |
| `tab5_alarm_snooze_min` | Number (Template) | `tab5-alarm.yaml` | Délai répétition (Snooze) |
| `tab5_alarm_max_ring` | Number (Template) | `tab5-alarm.yaml` | Durée max sonnerie |
| `tab5_alarm_volume` | Number (Template) | `tab5-alarm.yaml` | Volume du réveil |
| `tab5_rdv_lead` | Number (Template) | `tab5-alarm.yaml` | Délai annonce RDV |
| `tab5_alarm_mode` | Select (Template) | `tab5-alarm.yaml` | Mode de déclenchement réveil |
| `tab5_alarm_melody` | Select (Template) | `tab5-alarm.yaml` | Mélodie du réveil |
| `tab5_alarm_days` | Select (Template) | `tab5-alarm.yaml` | Jours de sonnerie |
| `imu_pitch` | Sensor (Motion) | `tab5-imu.yaml` | Inclinaison Pitch |
| `imu_roll` | Sensor (Motion) | `tab5-imu.yaml` | Inclinaison Roll |
| `imu_temp` | Sensor (BMI270) | `tab5-imu.yaml` | Température IMU |
| `tab5_tap_to_wake` | Switch (Template) | `tab5-imu.yaml` | Activation Tap-to-Wake |
| `btn_restart` | Button (Restart) | `tab5-hardware.yaml` | Redémarrage système |
| `tab5_dac_output_template` | Select (Template) | `tab5-hardware.yaml` | Sortie DAC (Line1/2/Both) |

### Tâches filles
- Aucune anomalie de nommage ou doublon majeure détectée.
