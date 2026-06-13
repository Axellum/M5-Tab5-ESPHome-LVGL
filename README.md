# M5Stack Tab5 - "Sans les mains" (AI-Powered High-Performance HMI & Local Voice Assistant)

<div align="center">
  <img src="https://img.shields.io/badge/ESPHome-Native_C++-blue.svg" alt="ESPHome Native" />
  <img src="https://img.shields.io/badge/LVGL_8.4-HMI_Graphics-green.svg" alt="LVGL" />
  <img src="https://img.shields.io/badge/HA_Failover-Double_Instance-red.svg" alt="HA Failover" />
  <img src="https://img.shields.io/badge/Local_Voice-Wyoming_&_LLM_Agent-orange.svg" alt="Local Voice" />
  <img src="https://img.shields.io/badge/100%25-AI_Generated-purple.svg" alt="AI Generated" />
  <img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="MIT License" />
</div>

<br>

---

## 📸 Aperçu du Nouveau Design Premium / UI Preview

Voici l'interface HMI fluide et optimisée conçue par l'IA et l'utilisateur, fonctionnant nativement sur l'écran tactile **M5Stack Tab5 V2** :

| Écran d'Accueil / Main Dashboard | Contrôle des Lumières / Light Control | Contrôle Climatisation / Climate Control |
| :---: | :---: | :---: |
| ![Écran Principal](docs/images/tab5_design_main.jpg) | ![Contrôle Lumière](docs/images/tab5_design_light.jpg) | ![Contrôle Climatisation](docs/images/tab5_design_climate.jpg) |

---

## 🇫🇷 Version Française

### 📖 L'histoire de ce projet
Ce dépôt est le résultat d'une expérience technologique personnelle : **un amateur en programmation peut-il concevoir une interface domotique complète, réactive à 60 FPS et intégrant un assistant vocal local intelligent en utilisant uniquement l'Intelligence Artificielle ?**

La réponse est **oui**. Ayant atteint les limites techniques d'un écran Nextion série, j'ai migré vers le **M5Stack Tab5 V2** (ESP32-P4) en laissant l'IA (Antigravity / Gemini / Claude) générer l'intégralité du code C++ custom, de l'interface LVGL et des automatisations Home Assistant. Je n'ai pas tapé une seule ligne de code moi-même !

Je partage ce projet avec la communauté domotique (HACF/GitHub) pour qu'il puisse servir d'inspiration, de boîte à outils ou d'exemples d'optimisations matérielles pour ESP32-P4 et LVGL.

---

### 🏗️ Architecture Globale & Philosophie Système
Le projet ne se limite pas à un simple écran. C'est une architecture résiliente structurée en **3 couches (3-Layers)** pour contourner les limites matérielles et garantir une fiabilité absolue.

```mermaid
graph TD
    %% Les Éléments
    Tab5[M5Stack Tab5 V2<br/>ESPHome + LVGL]
    Freebox[Instance HA Principale<br/>VM Freebox Delta - Prod]
    Deck[Instance HA Secondaire<br/>Steam Deck OLED - Secours]
    Moteur[Moteur d'Agents IA V7<br/>FastAPI/asyncio - Steam Deck]
    Wyoming[Serveur Vocal Wyoming<br/>Whisper & Piper - Steam Deck]

    %% Les Liaisons
    Tab5 -- 1. Audio brute & tactile --> Freebox
    Freebox -- 2. Délestage Audio --> Wyoming
    Wyoming -- 3. Texte transcrit --> Moteur
    Moteur -- 4. Décision & Action --> Freebox
    Freebox -- 5. Push d'état ➔ delay 150ms --> Tab5
    
    %% Failover & Sync
    Freebox ↔|Heartbeat 5 min & Sync HA| Deck
    Tab5 -.->|Fallback si Prod Hors-ligne| Deck

    %% Style
    style Tab5 fill:#1a365d,stroke:#3182ce,stroke-width:2px,color:#fff
    style Freebox fill:#2d3748,stroke:#4a5568,stroke-width:2px,color:#fff
    style Deck fill:#2c5282,stroke:#4299e1,stroke-width:2px,color:#fff
    style Moteur fill:#276749,stroke:#48bb78,stroke-width:2px,color:#fff
    style Wyoming fill:#744210,stroke:#dd6b20,stroke-width:2px,color:#fff
```

---

### 💡 Les 3 Piliers du Projet : Ce qui le rend intéressant et unique

#### 1. L'Écran Tactile M5Stack Tab5 V2 (ESPHome & LVGL)
* 🚀 **Fluidité Graphique Absolue (60 FPS) :** L'allocation de mémoire du frame buffer de LVGL est poussée à 100% (~1.8 Mo) directement dans la PSRAM de l'ESP32-P4. La commutation de page se fait instantanément en arrière-plan puis est transmise au bus MIPI-DSI 16 bits en mode DMA (Direct Memory Access).
* 💾 **Optimisation CPU et RAM Extrême :**
  * **Abolition de la transparence Alpha** sur les PNG ( little-endian complexe de l'ESP32-P4). Les images météo subissent un pré-calcul (*pre-baking*) sur fond opaque uni via un script Python local avant d'être téléversées.
  * **Rendu vectoriel ultra-léger :** Utilisation intensive de la police vectorielle *Material Design Icons (MDI)* avec antialiasing matériel désactivé (`bpp: 1`) pour un calcul CPU quasi instantané par l'ESP32.
  * **Radius et ombres proscrits** sur les objets transparents pour économiser les calculs de fondu du processeur.
* ⚡ **Architecture Push (Passivité HMI) :** L'écran ne requête jamais Home Assistant. C'est HA qui pousse les changements d'états (météo, clim, planning, volets) via des appels de services ESPHome, en espaçant les paquets de 150 ms pour ne pas saturer le buffer réseau de l'écran.

#### 2. La Haute Disponibilité par Double Instance Home Assistant (Failover)
* **Le Problème :** La VM Home Assistant OS hébergée sur la Freebox Delta a des ressources CPU/RAM critiques et ne peut pas faire tourner d'IA ou de vocal lourd sans planter.
* **La Solution Coexistence/Secours :**
  * Une instance secondaire de Home Assistant tourne sous Podman sur un **Steam Deck OLED** (machine puissante).
  * **Heartbeat & Failover :** Le Steam Deck ping la Freebox Delta toutes les 30 secondes. Si elle est hors-ligne pendant 5 minutes, le Deck bascule son état logique `is_primary_active` à `on` et prend le relais des automatisations physiques.
  * **Boot Délesté en 20s (contre 120s) :** Le script de synchronisation `sync_ha.sh` copie la configuration de la Freebox vers le Deck. Immédiatement après, un script de post-traitement `post_sync.py` désactive à chaud les intégrations lourdes ou physiques (freebox, onedrive, bluetooth local, esphome de production) pour que l'instance de secours démarre en 20 secondes en cas de crise.

#### 3. Le Vocal Local Déporté (Wyoming) & Moteur d'Agents IA
* **Délestage Réseau Vocal :** Le traitement de la voix (STT Whisper & TTS Piper) s'exécute localement sur le Steam Deck. La Freebox Delta n'est qu'un simple relais réseau pour les flux audio du Tab5.
* **Moteur d'Agents IA :** Un serveur multi-agents asynchrone (FastAPI / Python asyncio) tourne sur le Steam Deck, interfacé en tant que "Conversation Engine" de Home Assistant sous l'intégration `moteur_ia_conversation`.
* **Routage Intelligent :** L'assistant vocal propose deux modes commutables sur l'écran :
  * **Mode Domotique (Assist local) :** Traite les commandes physiques de la maison en local de façon déterministe.
  * **Mode Discussion (Moteur V7) :** Envoie la voix à notre moteur d'agents qui orchestre la cascade de modèles IA (Mistral, Gemini, DeepSeek R1 local) pour tenir une discussion et répondre intelligemment sur le haut-parleur de l'écran.

---

### 📚 Documentation Détaillée
Pour reproduire tout ou partie de l'installation, explorez les sous-dossiers du wiki :
* [⚙️ Matériel & Câblage Tab5](docs/hardware.md) — Spécifications, branchements GPIO et bus I2C.
* [🏗️ Structure & Code ESPHome](docs/architecture.md) — Détails des lambdas C++, packages et inclusions LVGL.
* [💻 Intégration double instance HA & Failover](contexte_ia/03_Software/06_DOUBLE_INSTANCE_HA.md) — Stratégie réseau et scripts d'automatisation.

---

## 🇺🇸 English Version

### 📖 The Story Behind
This repository is the result of a personal tech experiment: **can a complete beginner build a fully functional, high-performance smart home dashboard with a local AI voice assistant using only Artificial Intelligence?**

The answer is **yes**. After reaching the limits of a serial Nextion screen, I migrated to the **M5Stack Tab5 V2** (ESP32-P4) and let an AI (Antigravity / Gemini / Claude) generate all the custom C++ code, the LVGL interface, and the Home Assistant automations. I didn't write a single line of code myself!

I'm sharing it here for the smart home community to use as a toolkit, an inspiration source, or for hardware optimization examples on ESP32-P4 and LVGL.

---

### 🏗️ Global Architecture & System Philosophy
This project is not just a screen. It's a resilient architecture structured into **3 layers** to bypass hardware limits and ensure absolute reliability.

1. **Hardware Layer (M5Stack Tab5 V2):** Rendered natively in C++ using ESPHome and LVGL.
2. **Software Layer (HA Failover Cluster):** Actively synchronized double instances (Freebox Delta VM ↔ Steam Deck OLED Podman HA Core).
3. **Logic/API Layer (Local AI & Voice):** Local Wyoming Whisper/Piper services coupled with an asyncio multi-agent engine running on the Steam Deck.

---

### 💡 Key Features: What Makes It Unique

#### 1. M5Stack Tab5 V2 Touchscreen (ESPHome & LVGL)
* 🚀 **Smooth 60 FPS Graphics:** LVGL frame buffer is allocated 100% (~1.8 MB) in the ESP32-P4 PSRAM. Page transitions are computed in the background and pushed to the MIPI-DSI 16-bit bus using DMA.
* 💾 **Extreme CPU/RAM Optimizations:**
  * **Zero Alpha Transparency** on PNGs (due to ESP32-P4 little-endian rendering overhead). Weather images are pre-baked on solid color backgrounds using a Python script.
  * **Vector Fonts:** Uses Material Design Icons (MDI) vector fonts with antialiasing disabled (`bpp: 1`) for near-zero rendering time.
  * **No complex radius/shadows** on transparent objects.
* ⚡ **Event-Driven Push Architecture:** The screen is passive. Home Assistant pushes weather, climate, and schedule updates to the screen, pacing updates 150 ms apart to prevent network buffer overflows.

#### 2. High Availability via Double HA Instance (Failover)
* **The Problem:** The primary HA OS on the Freebox VM has very low resources (2 cores, 4 GB RAM) and cannot run local voice/LLMs.
* **The Solution:**
  * A secondary HA instance runs on a **Steam Deck OLED** (powerful CPU/GPU).
  * **Heartbeat & Failover:** The Deck pings the Freebox Delta. If it is offline for 5 minutes, it activates the `is_primary_active` flag to take over automations.
  * **20s Fast Boot (down from 120s):** The `sync_ha.sh` script syncs configs. Immediately after, a `post_sync.py` script disables heavy/conflicting integrations (local bluetooth, onedrive, production esphome) on the backup instance so it boots up in just 20 seconds.

#### 3. Offloaded Local Voice (Wyoming) & AI Agent Engine
* **Voice Offloading:** Voice processing (STT Whisper & TTS Piper) runs on the Steam Deck. The Freebox VM only relays raw audio packages.
* **AI Agent Engine:** A multi-agent FastAPI server runs on the Steam Deck, configured as a custom "Conversation Engine" in HA.
* **Smart Routing:** The screen allows switching between:
  * **Home Automation Mode:** Local HA Assist for fast, deterministic control.
  * **Discussion Mode:** Relays voice to the multi-agent engine (V7) to chat with LLMs (Mistral, Gemini, DeepSeek R1) and outputs the response to the screen speaker.

---

*Project born on the French Home Assistant Community forum (HACF). Feel free to submit pull requests or suggest improvements!*
