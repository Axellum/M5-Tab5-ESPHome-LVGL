# Voice Assistant Pipeline

## English · [Français](#version-française)

---

## Overview

The Tab5 functions as a local voice satellite for Home Assistant. It runs wake-word detection entirely on-device, streams audio only on demand, and gives real-time visual feedback through an icon on the UI.

The pipeline involves four components working in sequence: local wake-word model → I2S audio capture → Home Assistant Voice pipeline → ES8388 DAC playback.

---

## Wake-word detection

The device runs `micro_wake_word` — ESPHome's TensorFlow Lite Micro integration. The model (`okay_nabu`) runs continuously in the background, processing audio frames from the I2S microphone. This runs on-device, in the ESP32-P4's local memory. No audio is transmitted over the network during idle listening.

When the model detects the wake phrase with sufficient confidence, it fires an internal event that ESPHome forwards to the Home Assistant Voice pipeline.

**Resource cost:** the micro_wake_word model runs at roughly 5–8% CPU on the ESP32-P4 at 400 MHz, continuously. This is acceptable, but it means the remaining ~92% must cover LVGL rendering, I2S handling, and Wi-Fi. On this hardware, that's comfortable. On an ESP32-S3, headroom would be tighter.

Wake-word detection can be toggled via a UI button (`tab5_wake_word_active` switch entity). When off, the device acts as a manual push-to-talk panel only.

---

## Audio capture

| Parameter | Value |
|-----------|-------|
| Sample rate | 16 kHz |
| Bit depth | 16-bit |
| Channels | Mono |
| Interface | I2S |
| GPIO DIN | GPIO 28 |
| GPIO BCLK | GPIO 27 |
| GPIO LRCLK | GPIO 29 |

16 kHz / 16-bit mono is the standard format for speech processing pipelines. Home Assistant's Whisper-based STT expects this format.

Once the wake-word fires, audio is streamed in real time over the existing Wi-Fi connection to the HA Voice pipeline. The pipeline handles STT (Whisper), intent processing (Conversation agent), and TTS (Piper or cloud). The result comes back as an audio stream routed to the `media_player` entity on the Tab5.

---

## Audio playback — ES8388

TTS responses play through the ES8388 DAC → amplifier → built-in speaker. The media player entity on the Tab5 is a standard ESPHome I2S media player component.

**Boot sequence:** the amplifier enable switch is activated *after* the `media_player` component has fully initialized and the HA API connection is established. Enabling the amplifier before the I2S clock is stable produces an audible pop. The `on_boot` sequence in `tab5-ha-hmi.yaml` enforces this order.

**Interaction with LVGL traffic pacing:** when TTS is playing, the I2S DMA is actively consuming CPU cycles and memory bandwidth. The traffic pacing delays (1 s between push service blocks, 150 ms within forecast loops) prevent simultaneous large payload pushes from colliding with the active audio stream.

---

## Pipeline states and visual feedback

The microphone icon in the UI changes color to reflect the current pipeline state:

| State | Icon color | Meaning |
|-------|-----------|---------|
| Idle — wake word listening | Grey | Background detection running |
| Wake word detected | Green | Actively capturing speech |
| Processing | Orange | HA pipeline analyzing the command |
| Speaking | Blue | TTS audio playing back |
| Error / not understood | Red | Pipeline returned no result |
| Wake word disabled | Dim grey | Detection switched off |

The state machine runs in `tab5_custom.cpp`. ESPHome's voice assistant component fires callbacks (`on_listening`, `on_stt_end`, `on_tts_start`, `on_end`, `on_error`) which update a `voice_state` global variable. The LVGL icon update is done inside each callback via `lv_obj_set_style_text_color`.

---

## Assistant mode selection

The device supports two assistant modes, selectable from the UI:

1. **Home Assistant mode** — uses the standard Home Assistant conversation agent (controls devices, queries sensors, triggers automations)
2. **Conversation mode** — routes to a different HA pipeline configured for free-form conversation (connected to an LLM)

The mode is stored in the `conversation_mode` global bool. A toggle button on the main screen switches between them and updates the mode icon (🏠 vs 🤖). On the HA side, the `select.m5stack_tab5_home_assistant_hmi_assistant` entity reflects and controls which pipeline is selected.

---

---

## Version Française

---

## Vue d'ensemble

Le Tab5 fonctionne comme un satellite vocal local pour Home Assistant. Il exécute la détection de wake-word entièrement sur l'appareil, stream l'audio uniquement à la demande, et donne un retour visuel en temps réel via une icône dans l'UI.

Le pipeline implique quatre composants en séquence : modèle de wake-word local → capture audio I2S → pipeline Voice de Home Assistant → lecture DAC ES8388.

---

## Détection du wake-word

L'appareil fait tourner `micro_wake_word` — l'intégration TensorFlow Lite Micro d'ESPHome. Le modèle (`okay_nabu`) tourne en continu en arrière-plan, traitant les frames audio venant du microphone I2S. Ça tourne sur l'appareil, dans la mémoire locale de l'ESP32-P4. Aucun audio n'est transmis sur le réseau pendant l'écoute passive.

Quand le modèle détecte la phrase d'activation avec une confiance suffisante, il déclenche un événement interne qu'ESPHome transmet au pipeline Voice de Home Assistant.

**Coût ressources :** le modèle micro_wake_word tourne à environ 5–8% CPU sur l'ESP32-P4 à 400 MHz, en continu. C'est acceptable, mais ça signifie que les ~92% restants doivent couvrir le rendu LVGL, la gestion I2S et le Wi-Fi. Sur ce matériel, c'est confortable. Sur un ESP32-S3, la marge serait plus serrée.

La détection du wake-word peut être basculée via un bouton UI (switch `tab5_wake_word_active`). Quand désactivé, l'appareil fonctionne uniquement en mode push-to-talk manuel.

---

## Capture audio

| Paramètre | Valeur |
|-----------|--------|
| Taux d'échantillonnage | 16 kHz |
| Profondeur de bits | 16-bit |
| Canaux | Mono |
| Interface | I2S |
| GPIO DIN | GPIO 28 |
| GPIO BCLK | GPIO 27 |
| GPIO LRCLK | GPIO 29 |

16 kHz / 16-bit mono est le format standard pour les pipelines de traitement vocal. Le STT basé sur Whisper de Home Assistant attend ce format.

Une fois le wake-word déclenché, l'audio est streamé en temps réel via la connexion Wi-Fi existante vers le pipeline Voice HA. Le pipeline gère le STT (Whisper), le traitement d'intention (agent Conversation), et le TTS (Piper ou cloud). Le résultat revient comme flux audio routé vers l'entité `media_player` du Tab5.

---

## Lecture audio — ES8388

Les réponses TTS sont jouées via le DAC ES8388 → amplificateur → haut-parleur intégré. L'entité media player du Tab5 est un composant media player I2S ESPHome standard.

**Séquence de boot :** le switch d'activation de l'amplificateur est activé *après* que le composant `media_player` s'est complètement initialisé et que la connexion API HA est établie. Activer l'ampli avant que l'horloge I2S soit stable produit un pop audible. La séquence `on_boot` dans `tab5-ha-hmi.yaml` enforce cet ordre.

**Interaction avec le traffic pacing LVGL :** quand le TTS joue, le DMA I2S consomme activement des cycles CPU et de la bande passante mémoire. Les délais de traffic pacing (1 s entre les blocs de service push, 150 ms dans les boucles de prévisions) empêchent les push de gros payloads simultanés d'entrer en collision avec le flux audio actif.

---

## États du pipeline et retour visuel

L'icône microphone dans l'UI change de couleur pour refléter l'état courant du pipeline :

| État | Couleur icône | Signification |
|------|--------------|---------------|
| Veille — écoute wake word | Gris | Détection en arrière-plan |
| Wake word détecté | Vert | Capture vocale active |
| Traitement | Orange | Pipeline HA analyse la commande |
| Synthèse | Bleu | Audio TTS en lecture |
| Erreur / non compris | Rouge | Pipeline n'a retourné aucun résultat |
| Wake word désactivé | Gris sombre | Détection coupée |

La machine d'états tourne dans `tab5_custom.cpp`. Les callbacks du composant assistant vocal ESPHome (`on_listening`, `on_stt_end`, `on_tts_start`, `on_end`, `on_error`) mettent à jour une variable globale `voice_state`. La mise à jour de l'icône LVGL se fait dans chaque callback via `lv_obj_set_style_text_color`.

---

## Sélection du mode assistant

L'appareil supporte deux modes assistant, sélectionnables depuis l'UI :

1. **Mode Home Assistant** — utilise l'agent de conversation standard de Home Assistant (contrôle les appareils, interroge les capteurs, déclenche les automations)
2. **Mode Conversation** — route vers un pipeline HA différent configuré pour la conversation libre (connecté à un LLM)

Le mode est stocké dans la globale booléenne `conversation_mode`. Un bouton toggle sur l'écran principal bascule entre les deux et met à jour l'icône de mode (🏠 vs 🤖). Côté HA, l'entité `select.m5stack_tab5_home_assistant_hmi_assistant` reflète et contrôle quel pipeline est sélectionné.
