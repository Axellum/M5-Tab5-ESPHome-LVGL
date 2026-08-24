# -*- coding: utf-8 -*-
"""
Script de lecture du chemin mal placé '.qoder/wiki_publish' à la racine du dépôt.

Contexte : la tâche 'read_misplaced_file' a timeouté (120 s) lors de la
tentative 1. Cause racine : '.qoder/wiki_publish' est un DOSSIER (dépôt git
de 196 fichiers Markdown, ~3,76 Mo) et non un fichier texte. Un open() direct
sur un dossier lève IsADirectoryError, et un parcours monolithique (y compris
.git) dépasse le circuit breaker asynchrone de 120 s.

Stratégie adoptée (conforme à la logique de gestion d'erreur demandée) :
  1. Lire le fichier de contrôle '.qoder/file_exists_check.txt'.
  2. S'il contient 'YES' :
       a. Si la cible est un FICHIER  -> lecture directe avec open().
       b. Si la cible est un DOSSIER  -> on SIGNALE explicitement que la cible
          est un dossier (impossibilité technique de 'lire' un dossier comme
          un simple fichier texte), puis on PARCOURT et on CONCATÈNE le
          contenu des fichiers texte du répertoire (en excluant .git et en
          bornant la taille totale pour rester sous le timeout).
  3. S'il contient 'NO' : écrire le message de signalement demandé.
  4. Sauvegarder le résultat dans '.qoder/wiki_publish_content.txt'.

Aucun chemin absolu en constante : tout est résolu depuis __file__.
"""

import os
import sys

# ---------------------------------------------------------------------------
# Étape 0 : Résolution des chemins relatifs au dossier .qoder (via __file__)
# ---------------------------------------------------------------------------
# Le script vit dans .qoder/, donc le dossier parent est la racine du dépôt.
QODER_DIR = os.path.dirname(os.path.abspath(__file__))
CHECK_FILE = os.path.join(QODER_DIR, "file_exists_check.txt")
TARGET_PATH = os.path.join(QODER_DIR, "wiki_publish")
OUTPUT_FILE = os.path.join(QODER_DIR, "wiki_publish_content.txt")

# Bornes de sécurité pour rester bien sous le timeout de 120 s :
# - On ne lit jamais plus de MAX_FILES_FICHIERS fichiers dans un dossier.
# - On ne concatène jamais plus de MAX_TAILLE_TOTALE octets de contenu.
MAX_FILES_FICHIERS = 200
MAX_TAILLE_TOTALE = 2_000_000  # ~2 Mo de contenu concaténé maximum.


def lire_fichier_texte(chemin: str) -> str:
    """Lit un fichier texte de façon défensive (encodage + erreurs)."""
    # On tente d'abord en UTF-8 strict, puis on retombe sur 'replace'
    # pour ne jamais planter sur un octet invalide.
    try:
        with open(chemin, "r", encoding="utf-8") as f:
            return f.read()
    except UnicodeDecodeError:
        with open(chemin, "r", encoding="utf-8", errors="replace") as f:
            return f.read()


def main() -> int:
    # -----------------------------------------------------------------------
    # Étape 1 : Lecture du fichier de contrôle (YES / NO)
    # -----------------------------------------------------------------------
    try:
        with open(CHECK_FILE, "r", encoding="utf-8") as f:
            check_value = f.read().strip().upper()
    except FileNotFoundError:
        # Fichier de contrôle absent : on traite comme 'NO' (signalement).
        check_value = "NO"

    # -----------------------------------------------------------------------
    # Étape 2 : Branchement selon la valeur du contrôle
    # -----------------------------------------------------------------------
    if check_value == "YES":
        # Le chemin est censé exister : on vérifie sa nature réelle AVANT
        # toute tentative de lecture (c'est l'erreur de la tentative 1 :
        # open() sur un dossier lève IsADirectoryError).
        if os.path.isfile(TARGET_PATH):
            # -----------------------------------------------------------------
            # Cas A : la cible est un FICHIER texte -> lecture directe.
            # -----------------------------------------------------------------
            content = lire_fichier_texte(TARGET_PATH)
            rapport = (
                "CONTENU DE .qoder/wiki_publish (fichier texte) :\n"
                + "=" * 60 + "\n" + content
            )
        elif os.path.isdir(TARGET_PATH):
            # -----------------------------------------------------------------
            # Cas B : la cible est un DOSSIER.
            #
            # Signalement explicite : il est techniquement impossible de
            # 'lire' un dossier comme un simple fichier texte avec open()
            # (IsADirectoryError). On le signale, puis on parcourt et on
            # concatène le contenu des fichiers texte du répertoire, en
            # excluant .git et en bornant la taille pour éviter le timeout.
            # -----------------------------------------------------------------
            lignes = [
                "SIGNALEMENT : .qoder/wiki_publish est un DOSSIER (répertoire),",
                "et non un fichier texte. Il est techniquement impossible de le",
                "'lire' comme un simple fichier avec open() (IsADirectoryError).",
                "On parcourt donc le répertoire et on concatène le contenu des",
                "fichiers texte qu'il contient (en excluant .git).",
                "=" * 60,
            ]

            nb_fichiers = 0
            taille_totale = 0
            contenu_total = 0
            tronque = False

            # Parcours borné : on exclut .git et on limite le nombre de
            # fichiers lus ainsi que la taille totale concaténée.
            for racine, dossiers, fichiers in os.walk(TARGET_PATH):
                # On ne descend jamais dans .git (dépôt interne, inutile ici).
                dossiers[:] = [d for d in dossiers if d != ".git"]
                for nom in fichiers:
                    # Limite de sécurité : nombre de fichiers.
                    if nb_fichiers >= MAX_FILES_FICHIERS:
                        tronque = True
                        break
                    chemin = os.path.join(racine, nom)
                    try:
                        taille = os.path.getsize(chemin)
                    except OSError:
                        taille = 0
                    taille_totale += taille
                    nb_fichiers += 1

                    # Limite de sécurité : taille totale concaténée.
                    if contenu_total + taille > MAX_TAILLE_TOTALE:
                        tronque = True
                        break

                    # Lecture défensive du fichier texte.
                    try:
                        contenu = lire_fichier_texte(chemin)
                    except (OSError, UnicodeDecodeError):
                        # Fichier illisible (binaire, permissions...) : on le
                        # signale sans planter le script.
                        relatif = os.path.relpath(chemin, TARGET_PATH)
                        lignes.append(f"\n--- [ILLISIBLE] {relatif} ---")
                        continue

                    contenu_total += len(contenu)
                    # Séparateur lisible entre chaque fichier concaténé.
                    relatif = os.path.relpath(chemin, TARGET_PATH)
                    lignes.append(f"\n--- FICHIER : {relatif} ---\n")
                    lignes.append(contenu)

            if tronque:
                lignes.append(
                    "\n... (concaténation tronquée : limite de sécurité atteinte)"
                )

            rapport = "\n".join(lignes) + (
                f"\n\nTotal : {nb_fichiers} fichiers, "
                f"{taille_totale} octets (~{taille_totale // (1024 * 1024)} Mo)."
            )
        else:
            # -----------------------------------------------------------------
            # Cas C : contradiction — le contrôle dit YES mais le chemin
            # n'existe pas au moment de la lecture.
            # -----------------------------------------------------------------
            rapport = (
                "CONTRADICTION : file_exists_check.txt = YES mais "
                ".qoder/wiki_publish n'existe pas au moment de la lecture."
            )
    else:
        # -------------------------------------------------------------------
        # Étape 3 : Cas 'NO' -> message de signalement demandé par la tâche.
        # -------------------------------------------------------------------
        rapport = (
            "FICHIER_NON_TROUVE: .qoder/wiki_publish n'existe pas à la "
            "racine du dépôt."
        )

    # -----------------------------------------------------------------------
    # Étape 4 : Sauvegarde du résultat dans wiki_publish_content.txt
    # -----------------------------------------------------------------------
    with open(OUTPUT_FILE, "w", encoding="utf-8") as f:
        f.write(rapport)

    # Message de fin sur stdout (utile pour le log de l'agent).
    print(f"[OK] Résultat écrit dans {OUTPUT_FILE} ({len(rapport)} caractères)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
