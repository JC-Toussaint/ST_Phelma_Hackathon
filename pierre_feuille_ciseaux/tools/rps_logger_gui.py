#!/usr/bin/env python3
"""
rps_logger_gui.py - Collecte graphique Pierre-Feuille-Ciseaux pour NanoEdge AI Studio.

Lanceur de l'outil commun du dépôt (outils/collecte_nanoedge.py) avec le profil de ce projet
(profil_collecte.json : classes pierre / feuille / ciseaux, signaux de 8 trames 8x8 = 512 valeurs).

Utilisation :
    python rps_logger_gui.py                 # carte réelle (115200 bauds)
    python rps_logger_gui.py --demo          # données simulées, sans carte
    python rps_logger_gui.py --out essai2    # autre dossier (relatif au dossier de ce script)

Raccourcis : 1/2/3 = choisir la classe, Espace = démarrer/arrêter l'enregistrement.
Les fichiers pierre.csv, feuille.csv, ciseaux.csv sont écrits dans ce dossier (tools/).
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
                                    description="Collecte graphique Pierre-Feuille-Ciseaux"))
