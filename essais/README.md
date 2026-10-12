# Essais d'autres écrans (hors firmware Tab5)

Configurations ESPHome de **démarrage** pour deux cartes Waveshare, préparées le 12/10/2026 avant leur réception.
Elles n'utilisent rien de l'interface du Tab5 et ne sont pas publiées : elles servent à vérifier le matériel, carte en main,
avant d'éventuellement porter l'interface. Aucun secret : le Wi-Fi se donne par Improv (port USB, ESPHome Web) ou par le
point d'accès de secours.

| Fichier | Carte | Ce qu'il montre |
|---|---|---|
| `waveshare-p4-7b.yaml` | ESP32-P4-WIFI6-Touch-LCD-7B (7", 1024×600) | page de test : coordonnées du toucher et quatre coins, niveau du micro, bouton « Bip », luminosité ; Wi-Fi par le C6 |
| `waveshare-c6-amoled.yaml` | ESP32-C6-Touch-AMOLED-2.16 (480×480, sans PSRAM) | « HAUT » en haut, coordonnées du toucher, « Okay Nabu » ; capteurs « Mémoire libre » et « Plus grand bloc » |

Documents des cartes (schémas, BSP, fiches) : `H:\AuxFilsDesIdees\contexte_ia\02_Hardware\docs_constructeurs\` (hors dépôt).

## Premier branchement : quoi regarder

Flash par USB : `python -m esphome run essais/<fichier>.yaml`, puis lire le journal.

**7B**
1. Révision du P4 dans les premières lignes (`chip revision: v3.x` attendu). En `v1.x` : reflasher la variante
   `-s p4_echantillon "true" -s nom essai-7b-rev1`.
2. `ESP-Hosted link` établi, puis Wi-Fi connecté : valide les broches SDIO déduites (CLK 18, CMD 19, D0-D3 14-17, reset 54).
3. Scan I2C : GT911 (0x5D ou 0x14), ES8311 (0x18), ES7210 (0x40).
4. Écran : texte net, couleurs justes (carré rouge en haut à gauche). Toucher les quatre coins : x ≈ 0 / 1023, y ≈ 0 / 599.
5. « Micro », parler : la barre bouge. « Bip » : quatre notes dans le haut-parleur (PH2.0). Curseur : la luminosité suit.

**C6 AMOLED**
1. Pas de `AXP2101 muet sur 0x34` dans le journal ; scan I2C : AXP2101 (0x34), CST9220, ES8311.
2. L'écran s'allume ; « HAUT » est-il en haut ? Sinon noter le sens (miroir ou rotation à régler).
3. Toucher : les coordonnées suivent le doigt. Dire « Okay Nabu » : le texte du bas change.
4. Relever « Mémoire libre » et « Plus grand bloc » après une minute : c'est ce qui dit si un petit satellite tient.
