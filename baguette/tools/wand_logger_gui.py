#!/usr/bin/env python3
"""
wand_logger_gui.py - Collecte graphique de la baguette magique pour NanoEdge AI Studio.

Lanceur de l'outil commun du dépôt (outils/collecte_nanoedge.py) avec le profil de ce projet
(profil_collecte.json : classes cercle / zigzag / frappe / v / autre ; un geste = 256 échantillons
x 6 axes ax,ay,az,gx,gy,gz entrelacés = 1536 valeurs 0..1000, 500 = zéro).

Utilisation :
    python wand_logger_gui.py                 # carte réelle (115200 bauds)
    python wand_logger_gui.py --demo          # données simulées, sans carte
    python wand_logger_gui.py --out test      # autre dossier (relatif au dossier de ce script)

Raccourcis : 1 à 5 = choisir la classe, Espace = démarrer/arrêter l'enregistrement,
Retour arrière = supprimer le dernier geste enregistré (geste raté, pendant ou juste après la passe).
Les fichiers cercle.csv, zigzag.csv... sont écrits dans ce dossier (tools/).
Dépendance : pip install pyserial  (tkinter est fourni avec Python sous Windows)
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "outils"))
import collecte_nanoedge  # noqa: E402

PROFILE = os.path.join(HERE, "profil_collecte.json")

if __name__ == "__main__":
    sys.exit(collecte_nanoedge.main(sys.argv[1:], profile_path=PROFILE,
                                    description="Collecte graphique de la baguette magique"))
