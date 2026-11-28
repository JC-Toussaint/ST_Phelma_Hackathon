#!/usr/bin/env python3
"""
sentinel_logger_gui.py - Collecte graphique de la Sentinelle pour NanoEdge AI Studio.

Lanceur de l'outil commun du dépôt (outils/collecte_nanoedge.py) avec le profil de ce projet
(profil_collecte.json : classes normal / anomalie, signaux de 8 trames 8x8 = 512 valeurs,
valeur = 100 * distance / 3000 mm). Dans le Studio, projet "N-Class classification" : le modèle
apprend à partir de ces fichiers à distinguer la scène normale d'une anomalie. Les collecter sur la
scène qui sera surveillée.

  - scène 8x8 en direct (proche = clair, loin = sombre, case vide = pas de cible), distances en cm (D) ;
  - « scène de référence » (R) + vue des ÉCARTS (E) : + apparu / − disparu, au-delà de 120 mm comme la
    carte des changements du firmware ; on vérifie que les anomalies jouées sont visibles par le capteur ;
  - enregistrement guidé normal / anomalie, annulation de la dernière passe.

Utilisation :
    python sentinel_logger_gui.py            # carte réelle (115200 bauds)
    python sentinel_logger_gui.py --demo     # données simulées, sans carte (CSV dans demo/)

Raccourcis : 1/2 = classe, Espace = démarrer/arrêter, Retour arrière = supprimer le dernier signal,
R = mémoriser la référence, E = écarts, D = distances.
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
