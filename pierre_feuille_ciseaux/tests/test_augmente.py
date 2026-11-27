#!/usr/bin/env python3
"""Tests de tools/augmente_pfc.py (augmentation des données réelles) : transformations, respect des règles
du prétraitement de la carte, partage entraînement / test, reproductibilité, données réelles jamais
modifiées. Les « données réelles » sont ici les signaux synthétiques de make synth.   (make test)"""
import io
import os
import shutil
import sys
import tempfile

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(HERE, "..", "tools"))
import augmente_pfc as a  # noqa: E402

failures = 0


def check(cond, msg):
    global failures
    print(("  ok    : " if cond else "  ECHEC : ") + msg)
    failures += 0 if cond else 1


print("[règles du prétraitement de la carte]")
img = np.zeros((8, 8))
img[0:3, 0:3] = 60
img[0, 0] = 100
r = a.recenter(img.copy())
ys, xs = np.nonzero(r)
check(r.sum() == img.sum() and ys.min() == 2 and xs.min() == 2, "recentrage : décalage de 2 zones au plus vers le centre")
centered = np.zeros((8, 8))
centered[3:5, 3:5] = 100
check(np.array_equal(a.recenter(centered.copy()), centered), "main déjà centrée : rien ne bouge")
f = np.zeros((8, 8))
f[3:6, 3:6] = [[70, 80, 70], [80, 90, 80], [5, 80, 70]]
c = a.conform_frame(f)
check(c is not None and c.max() == 100 and c[c > 0].min() >= 20 and (c > 0).sum() == 8,
      "trame remise aux règles : max 100, zones < 20 renvoyées au fond")
few = np.zeros((8, 8))
few[3, 3:6] = 100
check(a.conform_frame(few) is None, f"moins de {a.MIN_HAND_ZONES} zones : trame refusée, comme sur la carte")

print("[transformations]")
sq = np.zeros((8, 8))
sq[2:6, 2:6] = 80
sq[3:5, 3:5] = 100
check(np.array_equal(a.rotate(sq, 90), sq), "rotation de 90° d'un carré centré : identique")
L = np.zeros((8, 8))
L[1:6, 3:5] = 100
L[4:6, 3:7] = 100
check(np.array_equal(a.rotate(L, 0), L), "rotation nulle : identique")
rot = a.rotate(L, 90)
check(abs(int((rot > 0).sum()) - int((L > 0).sum())) <= 2 and not np.array_equal(rot > 0, L > 0),
      "rotation de 90° d'une forme en L : surface conservée à 2 zones près, forme tournée")
synth_real = np.loadtxt(os.path.join(HERE, "data_synth", "feuille.csv"), delimiter=",")[:20].reshape(-1, 8, 8)
ratio = np.mean([(a.rotate(fr, 25) > 0).sum() / (fr > 0).sum() for fr in synth_real])
check(0.85 <= ratio <= 1.15, f"rotation de 25° de vraies trames : surface conservée ({ratio:.2f})")
rel = a.relief(np.array([[100.0, 60.0, 0.0]]), 1.5)
check(np.allclose(rel, [[100, 40, 0]]), "relief x1,5 : la zone à 40 du sommet passe à 60 en dessous, le fond reste 0")
fr = np.arange(8)[:, None, None] * np.ones((8, 8, 8))
check(list(a.resample_time(fr, 1.0)[:, 0, 0]) == list(range(8)) and a.resample_time(fr, 1.25)[0, 0, 0] == 0
      and a.resample_time(fr, 0.75)[0, 0, 0] == 1, "vitesse : trames reprises autour du milieu")

print("[variantes]")
synth = os.path.join(HERE, "data_synth")
real = np.loadtxt(os.path.join(synth, "ciseaux.csv"), delimiter=",")
rng = np.random.default_rng(0)
vs = [a.augment(s, rng, a.DEFAULTS) for s in real[:40] for _ in range(3)]
ok = [v for v in vs if v is not None]
check(len(ok) > 0.8 * len(vs), f"variantes acceptées : {len(ok)}/{len(vs)}")
check(all(len(v) == a.SIGNAL_LEN and a.check_signal(v) for v in ok), "toutes les variantes respectent les règles de la carte")
check(sum(1 for v in ok if not any(np.array_equal(v, s) for s in real)) == len(ok), "aucune variante n'est une copie d'un signal réel")
check(a.check_signal(real[0]) and not a.check_signal([5] * 512), "contrôle des règles : signal réel accepté, signal impossible refusé")

print("[fichiers, partage, reproductibilité]")
tmp = tempfile.mkdtemp()
src = os.path.join(tmp, "reel")
shutil.copytree(synth, src)
with open(os.path.join(src, "pierre.csv"), "a") as f:
    f.write("1,2,3\n")                                          # ligne de mauvaise longueur
before = {n: open(os.path.join(src, n)).read() for n in os.listdir(src)}
out1, out2 = os.path.join(tmp, "o1"), os.path.join(tmp, "o2")
rep = a.run(src, out1, ["pierre", "feuille", "ciseaux", "absent"], 2, 0.25, 7, a.DEFAULTS, out=io.StringIO())
a.run(src, out2, ["pierre", "feuille", "ciseaux"], 2, 0.25, 7, a.DEFAULTS, out=io.StringIO())
p = rep["pierre"]
n_train = sum(1 for _ in open(os.path.join(out1, "entrainement", "pierre.csv")))
n_test = sum(1 for _ in open(os.path.join(out1, "test", "pierre.csv")))
check(p["reels"] == 300 and p["ecartes"] == 1 and p["test"] == 75 and n_test == 75,
      "25 % des signaux réels mis de côté pour le test ; ligne de mauvaise longueur écartée")
check(n_train == p["entrainement"] + p["variantes"] and p["variantes"] == 2 * p["entrainement"],
      "entraînement = réels restants + 2 variantes chacun")
test_rows = {l for l in open(os.path.join(out1, "test", "pierre.csv"))}
train_rows = {l for l in open(os.path.join(out1, "entrainement", "pierre.csv"))}
check(not (test_rows & train_rows), "aucun signal de test dans l'entraînement, ni sous forme de variante identique")
check(all(open(os.path.join(out1, d, c + ".csv")).read() == open(os.path.join(out2, d, c + ".csv")).read()
          for d in ("entrainement", "test") for c in ("pierre", "feuille", "ciseaux")),
      "même graine : mêmes fichiers")
check("absent" not in rep, "classe sans fichier : ignorée")
out0 = os.path.join(tmp, "o0")
a.run(src, out0, ["feuille", "pierre", "ciseaux"], 0, 0.25, 7, a.DEFAULTS, out=io.StringIO())
check(all(open(os.path.join(out0, "test", c + ".csv")).read() == open(os.path.join(out1, "test", c + ".csv")).read()
          for c in ("pierre", "feuille", "ciseaux")),
      "même graine : mêmes signaux de test avec --facteur 0 ou 2, quel que soit l'ordre des classes")
check(all(open(os.path.join(src, n)).read() == before[n] for n in before), "données réelles jamais modifiées")
try:
    a.main(["--entree", src, "--sortie", src])
    refused = False
except SystemExit as e:
    refused = e.code != 0
check(refused, "sortie = entrée : refusé")

print("\n" + ("TOUS LES TESTS PASSENT" if not failures else "TESTS EN ECHEC") + f" ({failures} echec(s))")
sys.exit(1 if failures else 0)
