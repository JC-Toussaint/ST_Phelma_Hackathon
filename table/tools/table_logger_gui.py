#!/usr/bin/env python3
"""
table_logger_gui.py - Collecte graphique de la table qui écoute pour NanoEdge AI Studio.

Lanceur de l'outil commun du dépôt (outils/collecte_nanoedge.py) avec le profil de ce projet
(profil_collecte.json : classes zone1 … zone4 / autre ; une tape = 256 échantillons à 1667 Hz
x 3 axes ax,ay,az entrelacés = 768 valeurs 0..1000, 500 = repos).

Utilisation :
    python table_logger_gui.py                 # carte réelle (115200 bauds)
    python table_logger_gui.py --demo          # données simulées, sans carte
    python table_logger_gui.py --out test      # autre dossier (relatif au dossier de ce script)

Raccourcis : 1 à 5 = choisir la classe, Espace = démarrer/arrêter l'enregistrement,
Retour arrière = supprimer la dernière tape enregistrée (tape ratée, pendant ou juste après la passe).
Les fichiers zone1.csv, zone2.csv... sont écrits dans ce dossier (tools/).
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
                                    description="Collecte graphique de la table qui écoute"))
