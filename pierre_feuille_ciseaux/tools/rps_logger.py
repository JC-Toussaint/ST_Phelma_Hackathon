#!/usr/bin/env python3
"""
rps_logger.py - Collecte Pierre-Feuille-Ciseaux pour NanoEdge AI Studio, dans le terminal.

Lanceur de l'outil commun du dépôt (outils/collecte_nanoedge.py, mode ligne de commande) avec le
profil de ce projet (profil_collecte.json). Le firmware (APP_MODE_DATALOG) envoie une ligne CSV par
signal : 8 trames x 64 zones = 512 entiers 0..100. Chaque classe va dans son fichier (pierre.csv...),
directement importable dans NanoEdge AI Studio (projet N-Class classification).

Exemples :
    python rps_logger.py --port COM5                 # collecte guidée des 3 classes
    python rps_logger.py --port /dev/ttyACM0 --view  # juste visualiser le capteur
    python rps_logger.py --port COM5 --classes ciseaux --count 200   # compléter une classe

Liaison série : 115200 bauds uniquement (USART2 du projet ST, non reconfiguré).
Dépendance : pip install pyserial
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "..", "outils"))
import collecte_nanoedge  # noqa: E402

PROFILE = os.path.join(HERE, "profil_collecte.json")

if __name__ == "__main__":
    argv = sys.argv[1:]
    if "--view" not in argv and "--cli" not in argv:
        argv = ["--cli"] + argv               # rps_logger.py = toujours la version terminal
    sys.exit(collecte_nanoedge.main(argv, profile_path=PROFILE,
                                    description="Collecte Pierre-Feuille-Ciseaux (terminal)"))
