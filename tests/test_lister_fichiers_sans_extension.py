# -*- coding: utf-8 -*-
"""
Tests unitaires du script 'lister_fichiers_sans_extension.py'.

Vérifie que la fonction 'lister_fichiers_sans_extension' :
    - liste uniquement le contenu NON RÉCURSIF du dossier,
    - retient uniquement les FICHIERS (pas les sous-dossiers),
    - identifie correctement les fichiers SANS extension (sans point dans le nom),
    - exclut les fichiers AVEC extension (avec un point),
    - lève FileNotFoundError si le dossier n'existe pas.

Les chemins sont résolus depuis __file__ (convention projet) : aucun chemin
absolu en dur type 'H:\\' ou '/home/deck/'.
"""

import os
import tempfile
import unittest

# Import du module à tester, résolu relativement à ce fichier de test
# (le script est dans scripts/, le test dans tests/ -> même racine dépôt)
import sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "scripts"))

from lister_fichiers_sans_extension import lister_fichiers_sans_extension


class TestListerFichiersSansExtension(unittest.TestCase):
    """Tests de la fonction lister_fichiers_sans_extension."""

    def setUp(self):
        """Crée un dossier temporaire avec un jeu de fichiers contrôlé."""
        # Dossier temporaire isolé pour chaque test
        self.tmpdir = tempfile.mkdtemp()

    def tearDown(self):
        """Nettoie le dossier temporaire après chaque test."""
        import shutil
        shutil.rmtree(self.tmpdir, ignore_errors=True)

    def _creer(self, nom: str, contenu: str = "x") -> str:
        """Crée un fichier dans le dossier temporaire et renvoie son chemin."""
        chemin = os.path.join(self.tmpdir, nom)
        with open(chemin, "w", encoding="utf-8") as f:
            f.write(contenu)
        return chemin

    def test_fichier_sans_extension_detecte(self):
        """Un fichier sans point dans le nom doit être retenu."""
        self._creer("wiki_publish")
        resultat = lister_fichiers_sans_extension(self.tmpdir)
        self.assertEqual(len(resultat), 1)
        self.assertEqual(resultat[0], os.path.join(self.tmpdir, "wiki_publish"))

    def test_fichier_avec_extension_exclu(self):
        """Un fichier avec un point (extension) doit être exclu."""
        self._creer("rapport.md")
        resultat = lister_fichiers_sans_extension(self.tmpdir)
        self.assertEqual(resultat, [])

    def test_fichier_cache_avec_point_exclu(self):
        """Un fichier caché type '.gitignore' a un point -> exclu."""
        self._creer(".gitignore")
        resultat = lister_fichiers_sans_extension(self.tmpdir)
        self.assertEqual(resultat, [])

    def test_sous_dossier_sans_extension_exclu(self):
        """Un SOUS-DOSSIER sans extension ne doit PAS être retenu (non récursif, fichiers seuls)."""
        os.makedirs(os.path.join(self.tmpdir, "wiki_publish"))
        resultat = lister_fichiers_sans_extension(self.tmpdir)
        self.assertEqual(resultat, [])

    def test_non_recursif(self):
        """Les fichiers des sous-dossiers ne doivent PAS apparaître (non récursif)."""
        sous = os.path.join(self.tmpdir, "sous")
        os.makedirs(sous)
        with open(os.path.join(sous, "fichier_sans_ext"), "w", encoding="utf-8") as f:
            f.write("x")
        resultat = lister_fichiers_sans_extension(self.tmpdir)
        self.assertEqual(resultat, [])

    def test_melange(self):
        """Mélange : seul le fichier sans extension est retenu."""
        self._creer("wiki_publish")      # sans extension -> retenu
        self._creer("Home.md")           # avec extension -> exclu
        self._creer("_Sidebar.md")        # avec extension -> exclu
        os.makedirs(os.path.join(self.tmpdir, "sous_dossier"))  # dossier -> exclu
        resultat = lister_fichiers_sans_extension(self.tmpdir)
        self.assertEqual(len(resultat), 1)
        self.assertEqual(resultat[0], os.path.join(self.tmpdir, "wiki_publish"))

    def test_dossier_inexistant_leve_erreur(self):
        """Un dossier inexistant doit lever FileNotFoundError."""
        chemin_inexistant = os.path.join(self.tmpdir, "n_existe_pas")
        with self.assertRaises(FileNotFoundError):
            lister_fichiers_sans_extension(chemin_inexistant)


if __name__ == "__main__":
    unittest.main()
