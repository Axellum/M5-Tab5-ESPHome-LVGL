# -*- coding: utf-8 -*-
"""
Script de vérification : lister les fichiers SANS extension dans le dossier '.qoder/'.

Objectif :
    - Lister UNIQUEMENT le contenu non récursif du dossier '.qoder/' du dépôt 00ProjetTab.
    - Identifier les fichiers sans extension (c'est-à-dire sans point dans le nom).
    - Retourner la liste des chemins complets des fichiers sans extension trouvés.
    - Ne PAS explorer les sous-dossiers de '.qoder/'.

Usage :
    python lister_fichiers_sans_extension.py
"""

import os
import sys

# Chemin absolu du dossier '.qoder/' du dépôt 00ProjetTab (racine canonique H:\AuxFilsDesIdees)
CHEMIN_QODER = os.path.join("H:", os.sep, "AuxFilsDesIdees", "00ProjetTab", ".qoder")


def lister_fichiers_sans_extension(chemin_dossier: str) -> list:
    """
    Liste les fichiers SANS extension (sans point dans le nom) présents dans le
    dossier donné, de manière NON RÉCURSIVE.

    Args:
        chemin_dossier: chemin absolu du dossier à inspecter.

    Returns:
        Liste des chemins complets des fichiers sans extension trouvés.
        (Les sous-dossiers ne sont PAS inclus, même s'ils n'ont pas d'extension.)
    """
    # Vérifier que le dossier existe avant de le parcourir
    if not os.path.isdir(chemin_dossier):
        raise FileNotFoundError(f"Dossier introuvable : {chemin_dossier}")

    fichiers_sans_extension = []

    # os.listdir renvoie UNIQUEMENT le contenu immédiat (non récursif)
    for nom in os.listdir(chemin_dossier):
        chemin_complet = os.path.join(chemin_dossier, nom)

        # On ne retient que les FICHIERS (pas les sous-dossiers)
        if not os.path.isfile(chemin_complet):
            continue

        # Un fichier "sans extension" = son nom ne contient aucun point
        # (les fichiers cachés type '.gitignore' ont un point, donc sont exclus)
        if "." not in nom:
            fichiers_sans_extension.append(chemin_complet)

    return fichiers_sans_extension


def main() -> int:
    """Point d'entrée : liste les fichiers sans extension de '.qoder/' et affiche le résultat."""
    try:
        resultat = lister_fichiers_sans_extension(CHEMIN_QODER)
    except FileNotFoundError as exc:
        # Dossier absent : on signale clairement et on termine en erreur
        print(f"ERREUR : {exc}", file=sys.stderr)
        return 1

    # Affichage pédagogique du résultat
    print(f"Dossier inspecté (non récursif) : {CHEMIN_QODER}")
    print(f"Nombre de fichiers sans extension trouvés : {len(resultat)}")
    if resultat:
        print("Chemin(s) complet(s) :")
        for chemin in resultat:
            print(f"  - {chemin}")
    else:
        print("Aucun fichier sans extension trouvé dans .qoder/ (non récursif).")

    return 0


if __name__ == "__main__":
    sys.exit(main())
