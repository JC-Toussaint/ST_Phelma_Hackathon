#!/usr/bin/env python3
"""
sentinel_logger_gui.py - Collecte graphique de la Sentinelle pour le BENCHMARK de NanoEdge AI Studio.

Lanceur de l'outil commun du dépôt (outils/collecte_nanoedge.py) avec le profil de ce projet
(profil_collecte.json : classes normal / anomalie, signaux de 8 trames 8x8 = 512 valeurs,
valeur = 100 * distance / 3000 mm). Rappel : ces données servent uniquement à choisir l'algorithme
dans le Studio (projet "Anomaly detection") ; la connaissance réelle est apprise ensuite SUR LA CARTE.

  - scène 8x8 en direct (proche = clair, loin = sombre, case vide = pas de cible), distances en cm (D) ;
  - « scène de référence » (R) + vue des ÉCARTS (E) : + apparu / − disparu, au-delà de 120 mm comme la
    carte des changements du firmware ; on vérifie que les anomalies jouées sont visibles par le capteur ;
  - enregistrement guidé normal / anomalie, annulation de la dernière passe.

Utilisation :
    python sentinel_logger_gui.py            # carte réelle (115200 bauds)
    python sentinel_logger_gui.py --demo     # données simulées, sans carte (CSV dans demo/)

Raccourcis : 1/2 = classe, Espace = démarrer/arrêter, R = mémoriser la référence, E = écarts, D = distances.
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
                                    description="Collecte graphique de la Sentinelle"))
