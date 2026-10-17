# Essai du popup Musique sur la Waveshare 7B (branche essai/mesure-popup-musique, jamais mergée)

À faire **après la sortie de la release candidate, avec l'accord d'Axel** (2 OTA d'essai + 1 OTA de
retour). Mesure de la PR « perf(musique) » : ouverture du popup Musique, avant / après, la même
soirée, avec Climatisation et Énergie comme témoins. Avant le correctif (nuit du 16/10/2026,
build `3.8.1-7b-mesure-r0-3293d4e`) : Musique 110 ms (Zen Sumi) et 137 ms (Relief doux).

## 0. Avant de commencer

- Accord d'Axel ; aucune autre session ne compile, ne flashe ni n'écoute la 7B (COM8, MAC
  `E8:F6:0A:E8:BB:1A`) ; IP de la 7B lue dans HA (DHCP) ; la 7B est montée à l'envers :
  `-s carte_rotation 0` à chaque commande.
- Noter la version qui tourne (`device_info()` ou HA) et retrouver son binaire dans
  `C:\Users\axell\.tab5\elf\` : c'est elle qu'on remet à la fin.

## 1. Deux builds de mesure (PowerShell, une compilation à la fois)

```powershell
cd H:\AuxFilsDesIdees\00ProjetTab
git fetch origin
git worktree add .claude/worktrees/essai-musique-avant origin/main
git worktree add .claude/worktrees/essai-musique-apres origin/claude/perf-popup-musique-8ac40b
git worktree add .claude/worktrees/essai-musique-outils origin/essai/mesure-popup-musique
foreach ($v in "avant", "apres") {
  $wt = ".claude/worktrees/essai-musique-$v"
  python .claude/worktrees/essai-musique-outils/tools/mesure/essai_7b/instrumenter_images.py $wt
  Copy-Item Tab5\user_entities.yaml "$wt\Tab5\"
}
```

Puis, pour chaque `$v` (avant, puis apres), depuis son worktree :

```powershell
$env:ESPHOME_ESP_IDF_PREFIX = "C:\espidf"
$env:CMAKE_BUILD_PARALLEL_LEVEL = "4"
$py = "H:\AuxFilsDesIdees\.venvs\esphome-2026.9.1\Scripts\python.exe"
$s = @("-s","tab5_cle_signature","C:/Users/axell/.tab5/tab5_signature.pem","-s","tab5_carte","waveshare-7b","-s","carte_rotation","0","-s","tab5_publication","beta","-s","tab5_version","3.9.0-7b-musique-$v")
& $py -m esphome @s clean tab5-ha-hmi.yaml
& $py -m esphome @s compile tab5-ha-hmi.yaml *> "$env:TEMP\essai_musique_$v.log"
"EXIT=$LASTEXITCODE"
```

Ranger chaque binaire tout de suite (dossier de build partagé) :
`H:\.esphome_data\build\tab5-ha-hmi\build\firmware.elf` et `firmware.ota.bin` →
`C:\Users\axell\.tab5\elf\3.9.0-7b-musique-$v-<sha7>\`, empreinte SHA-256 comparée.

## 2. Flasher et mesurer (skill flasher-tab5 : OTA signée, `upload --file`)

```powershell
& $py -m esphome @s upload --file C:\Users\axell\.tab5\elf\3.9.0-7b-musique-avant-<sha7>\firmware.ota.bin --device <IP> tab5-ha-hmi.yaml
python .claude/worktrees/essai-musique-outils/tools/mesure/essai_7b/essai_musique.py --ip <IP> --nom avant
```

Vérifier la version lue sur l'appareil après l'OTA (`3.9.0-7b-musique-avant`), puis la même chose
avec `apres`. Le script vérifie la MAC, chronomètre 3 popups × (10 passes en Zen Sumi + 5 en Relief
doux) en ~12 min, remet le thème et le mode du départ, et écrit un tableau et un CSV à côté de lui.

## 3. Remettre la version de départ

OTA du binaire noté à l'étape 0, version lue sur l'appareil. Puis `git worktree remove` des trois
worktrees.

## Ce qu'on attend (non vérifié sur l'appareil)

- Musique : la même image (rendu hors tablette identique au pixel), une ouverture plus courte ;
  Climatisation et Énergie : inchangés (± 1 ms). Chiffre attendu : voir la PR (tablette virtuelle
  1024×600, run de `.github/workflows/mesure-popup.yml`).
- Le journal de chaque ouverture : `px` reste 614 400 (tout l'écran), `zones` passe de 1 à 5.
