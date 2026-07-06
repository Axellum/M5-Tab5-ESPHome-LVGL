# Plan d'Implémentation : Refonte AI-First du Projet Tab5

Ce plan définit la stratégie pour rendre le projet ESPHome `00ProjetTab` parfaitement compréhensible et modifiable par des LLMs (Claude, Z-AI, Antigravity) en toute sécurité. Il est basé sur la synthèse des audits croisés (Claude 3.5 Sonnet et Zhipu GLM-4-Plus).

## User Review Required
> [!IMPORTANT]
> Avant de commencer à modifier vos fichiers YAML et C++, j'ai besoin de votre accord sur la structure des commentaires `[AI-CONTEXT]` et l'ordre des tâches.

## Synthèse des Audits (Claude + Z-AI)

Les deux intelligences artificielles ont pointé exactement les mêmes faiblesses architecturales dans le code source brut :
1. **Violation du SoC (Separation of Concerns)** : `tab5-sensors.yaml` (1677 lignes) et `tab5-api-logic.yaml` (1026 lignes) contiennent des lambdas C++ massives qui manipulent directement l'interface graphique (ex: `lv_obj_set_style_text_color`).
2. **Risque RAM/SRAM** : L'utilisation de `std::string` dans des hot-paths (curseurs, mises à jour) fragmente la mémoire. Z-AI et Claude recommandent l'usage strict de buffers statiques (`char buf[32]`) et de `strtok_r`.
3. **Bootloop et Hardcoding** : Le workaround `delay(1000)` au boot et l'utilisation de couleurs hexadécimales en dur au lieu d'un dictionnaire de tokens `UIColor`.

## Pépites Découvertes (Audit Ligne par Ligne)

La lecture manuelle des fichiers a révélé des points critiques que les LLMs "généraux" n'avaient pas soulignés avec autant de précision :

1. **La Paginations LVGL Maudite (`tab5-lvgl.yaml` - Lignes 13 à 81)** :
   - Le bloc `on_gesture` contient une **énorme lambda C++ de 70 lignes** qui gère la logique de swipe (haut/bas/gauche/droite) et la création de tableaux (`WeatherDaySlot`) directement dans le YAML. C'est une hérésie architecturale. Cette logique doit impérativement devenir une fonction `handle_swipe_gesture()` dans le C++.
2. **Le Dictionnaire de Couleurs Existe Déjà ! (`tab5_custom.h` - Ligne 107)** :
   - Z-AI nous conseillait de créer un espace de nom `UIColor`, mais **il existe déjà** ! Vous l'aviez commencé. La pépite ici, c'est qu'il suffira de faire un "Chercher/Remplacer" massif dans les YAML pour utiliser `UIColor::SUCCESS` au lieu de `0x34D399`, ce qui va nous faire gagner énormément de temps à la Phase 3.
3. **Le Piège Mortel du Boot (`tab5-ha-hmi.yaml` - Ligne 58)** :
   - Vous avez documenté une rustine vitale : un `delay(1000)` au démarrage (priorité 700) pour laisser le temps au GPIO expander I2C de s'initialiser avant l'écran DSI. Toute IA "nettoyeuse" supprimerait ce `delay()` bloquant et casserait votre écran. Il faut absolument poser un `[AI-WARNING]` blindé dessus.

## Proposed Changes

La refonte se fera en 3 phases pour ne rien casser.

### Phase 1 : Injection des [AI-CONTEXT] et [AI-WARNING] (Documentation Active)
Nous n'allons pas coder de nouvelles fonctionnalités, mais placer des "garde-fous".
- Créer un entête standardisé `[AI-CONTEXT]` dans les 6 fichiers restants.
- Placer des `[AI-WARNING]` spécifiques sur les pépites critiques (le Swipe et le Boot).

#### [MODIFY] tab5_custom.cpp
Injection du bloc recommandé par Claude pour interdire formellement aux IA de générer du code LVGL dans le YAML.

#### [MODIFY] tab5-sensors.yaml
Injection d'un avertissement en haut de fichier : "Ce fichier ne DOIT contenir AUCUNE logique de présentation. Appeler uniquement des helpers C++."

#### [MODIFY] tab5-api-logic.yaml
Injection de contrats de payload sur les gros services (comme `tab5_maj_pluie_1h`).

### Phase 2 : Refactorisation LVGL / C++ (Centralisation)
- Créer des fonctions "Helpers" dans `tab5_custom.cpp` (ex: `update_light_card_ui()`, `update_temp_ui()`).
- Nettoyer les lambdas kilométriques des fichiers YAML pour les remplacer par des appels simples d'une seule ligne.

### Phase 3 : Optimisation Mémoire et Dictionnaire de Couleurs
- Supprimer les allocations dynamiques (`std::string`) dans le parsing.
- Remplacer les hexadécimaux en dur par l'espace de nom `UIColor` recommandé par Z-AI.

---

## Verification Plan

### Manual Verification
- Compilation ESPHome en local sans erreur (`esphome compile tab5-ha-hmi.yaml`).
- Aucun changement visuel sur l'écran tactile (répression stricte à iso-fonctionnalité).
- L'utilisation de la RAM statique (`free_heap`) ne doit pas s'aggraver.

## 1. La Cartographie Globale (Ce qui manque)
Nous avons déjà les états des projets (`etat_tab5.md`, `etat_moteur_agents.md`) et GitHub est fonctionnel. Il manque le **liant** :
*   **Cartographie d'Architecture (Le Graphe)** : Un document maître `CARTOGRAPHIE_SYSTEME.md` (avec des diagrammes Mermaid) expliquant comment l'IHM du Tab5 parle à Home Assistant, qui déclenche le Moteur d'Agents Python.
*   **Matrice de Dépendances** : Si je modifie le fichier X, quels sont les fichiers Y et Z que je risque de casser ?
*   **Historique Décisionnel (ADR - Architecture Decision Records)** : Pourquoi on a choisi SQLite WAL et pas Postgres ? (Essentiel pour éviter que l'IA ne propose de tout réécrire en Postgres).

## 2. Commenter le code "Pour les LLMs" (La Méthode)
Un humain lit du code pour comprendre *comment* ça marche. Un LLM a besoin de savoir *dans quel but* et *quelles sont les limites*.

Voici les 4 règles d'or que je vous propose d'appliquer sur tout votre code :

### A. L'En-tête de Contexte (AI Header)
Au tout début de chaque fichier (`.py`, `.yaml`, `.h`), on ajoutera un bloc standardisé :
```python
# ==========================================
# [AI-CONTEXT] ROLE : Gère le routage des LLMs (LM Studio vs Cloud)
# [AI-CONTEXT] DEPENDENCIES : config.yaml, provider_api.py
# [AI-CONTEXT] CRITICAL : Ne JAMAIS utiliser de requêtes synchrones ici (boucle asyncio).
# [AI-CONTEXT] REF_DOC : contexte_ia/03_Software/moteur.md
# ==========================================
```
*Pourquoi ?* Cela donne immédiatement à l'IA la portée (scope) de son action avant même qu'elle ne lise la première ligne de code.

### B. Le "Negative Prompting" dans le code
Les LLMs obéissent très bien aux interdictions. Au lieu de juste commenter ce que fait une fonction, on commente ce qu'il ne faut **pas** faire.
```cpp
// AI-WARNING: Ne pas utiliser delay() ici, cela fait crasher le Watchdog de l'ESP32. Utilisez set_timeout().
```

### C. Le typage strict absolu
Pour Python (`moteur_agents`), le typage n'est pas optionnel pour une IA, c'est son système de guidage.
```python
# Mauvais pour l'IA
def traiter_donnees(data):

# Parfait pour l'IA
def traiter_donnees(data: dict[str, Any]) -> AgentResponse:
```

### D. Le "Chain of Thought" en commentaire
Expliquer la logique métier complexe étape par étape dans le code.

## 3. Plan d'Exécution (Par quel LLM et par où commencer ?)

### Quel LLM utiliser pour ce chantier ?
*   **Pour analyser et cartographier tout le projet** : **Claude 3.5 Sonnet** (via votre Claude Desktop) ou **Gemini 1.5 Pro**. Leurs fenêtres de contexte gigantesques permettent de lire 100 fichiers d'un coup et de tracer les liens.
*   **Pour insérer les commentaires dans le code fichier par fichier** : **Qwen 2.5 Coder** (en local via LM Studio) ou **Antigravity** (moi-même). On fera des passes rapides fichier par fichier pour ajouter les En-têtes `[AI-CONTEXT]`.

### Par quoi commencer ? (L'Ordre d'attaque)
1.  **Étape 1 (Théorique) :** Je rédige la `CARTOGRAPHIE_SYSTEME.md` avec vous pour avoir la vue d'ensemble.
2.  **Étape 2 (Le Cerveau) :** On refactorise les commentaires du `moteur_agents/core/` (Python), car c'est là qu'il y a le plus de logique algorithmique.
3.  **Étape 3 (Le Front) :** On attaque le code C++ (ESPHome du Tab5) en ajoutant les `AI-WARNING` liés au matériel (Watchdog, RAM).
4.  **Étape 4 (La Glue) :** On commente les automatisations YAML de Home Assistant.

## Plan de Vérification
*   Une fois un composant commenté (ex: `moteur_agents/core`), on demandera à un agent local (ex: Mistral Nemo) de lire le fichier et de nous résumer ses contraintes. S'il ne se trompe pas, le formatage est validé !
