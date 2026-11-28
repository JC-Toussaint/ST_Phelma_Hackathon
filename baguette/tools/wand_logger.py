#!/usr/bin/env python3
"""
wand_logger.py - Collecte de la baguette magique pour NanoEdge AI Studio, dans le terminal.

Lanceur de l'outil commun du dépôt (outils/collecte_nanoedge.py, mode ligne de commande) avec le
profil de ce projet (profil_collecte.json). Le firmware (APP_MODE_DATALOG) envoie une ligne CSV par
geste détecté : 256 échantillons x 6 axes = 1536 entiers 0..1000.

Exemples :
    python wand_logger.py --port COM5                          # collecte guidée des 5 classes
    python wand_logger.py --port COM5 --view                   # juste visualiser les gestes
    python wand_logger.py --port COM5 --classes cercle --count 50

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
        argv = ["--cli"] + argv               # wand_logger.py = toujours la version terminal
    sys.exit(collecte_nanoedge.main(argv, profile_path=PROFILE,
                                    description="Collecte de la baguette magique (terminal)"))
