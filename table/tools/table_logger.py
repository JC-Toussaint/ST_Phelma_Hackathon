#!/usr/bin/env python3
"""
table_logger.py - Collecte de la table qui écoute pour NanoEdge AI Studio, dans le terminal.

Lanceur de l'outil commun du dépôt (outils/collecte_nanoedge.py, mode ligne de commande) avec le
profil de ce projet (profil_collecte.json). Le firmware (APP_MODE_DATALOG) envoie une ligne CSV par
tape détectée : 256 échantillons x 3 axes = 768 entiers 0..1000.

Exemples :
    python table_logger.py --port COM5                          # collecte guidée des 5 classes
    python table_logger.py --port COM5 --view                   # juste visualiser les tapes
    python table_logger.py --port COM5 --classes zone1 --count 50

Liaison série : 115200 bauds (USART2 initialisé par le BSP de ST).
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
        argv = ["--cli"] + argv               # table_logger.py = toujours la version terminal
    sys.exit(collecte_nanoedge.main(argv, profile_path=PROFILE,
                                    description="Collecte de la table qui écoute (terminal)"))
