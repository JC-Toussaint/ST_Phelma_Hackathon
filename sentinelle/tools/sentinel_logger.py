#!/usr/bin/env python3
"""
sentinel_logger.py - Collecte de la Sentinelle pour le BENCHMARK de NanoEdge AI Studio, dans le terminal.

Lanceur de l'outil commun du dépôt (outils/collecte_nanoedge.py, mode ligne de commande) avec le
profil de ce projet (profil_collecte.json). Le firmware (APP_MODE_DATALOG) envoie une ligne CSV par
signal : 8 trames x 64 zones = 512 entiers 0..100 (100 * distance / SENT_MAX_MM).
Rappel : ces données servent uniquement à choisir le bon algorithme dans le Studio ; la
« connaissance » réelle est apprise ensuite SUR LA CARTE, à l'endroit surveillé.

Exemples :
    python sentinel_logger.py --port COM5 --view               # visualiser la scène
    python sentinel_logger.py --port COM5                      # normal puis anomalie
    python sentinel_logger.py --port COM5 --classes normal --count 200

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
        argv = ["--cli"] + argv               # sentinel_logger.py = toujours la version terminal
    sys.exit(collecte_nanoedge.main(argv, profile_path=PROFILE,
                                    description="Collecte de la Sentinelle (terminal)"))
