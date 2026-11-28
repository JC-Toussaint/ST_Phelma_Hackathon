#!/usr/bin/env python3
"""
augmenter.py - Variantes réalistes des gestes collectés (augmentation de données) pour NanoEdge AI Studio.

Chaque vrai geste (une ligne des CSV cercle.csv, zigzag.csv...) donne plusieurs variantes :
  - vitesse   : geste étiré ou comprimé dans le temps ; la vitesse de rotation suit (÷ facteur) et
                l'accélération propre aussi (÷ facteur²), la gravité ne change pas ;
  - amplitude : geste plus grand ou plus petit (accélération propre et rotation) ;
  - rotation  : baguette tenue un peu de travers (les 3 axes accéléro ET gyro tournent ensemble :
                le SENS du geste ne change pas) ;
  - décalage  : départ avancé ou retardé de quelques échantillons ;
  - bruit     : léger bruit de mesure.
Une variante n'est jamais étirée au point d'être coupée en fin de fenêtre, et rien ne sature.

Les fichiers écrits (originaux + variantes, mêmes noms) vont dans un dossier À PART, jamais avec les
vrais gestes : tools/augmente/ par défaut. On les importe dans le Studio comme d'habitude.

    python augmenter.py                         # tools/*.csv -> tools/augmente/, 10 variantes par geste
    python augmenter.py -n 5 --force 1          # moins de variantes, déformations plus fortes
    python augmenter.py --entree Backup_Mesures --sortie essai_aug

Force 0,5 par défaut : sur 50 vrais gestes (10 par classe), un test « geste laissé de côté » reconnaît
alors tous les gestes et garde 8 « autre » sur 10 ; à force 1, la classe cercle déformée « avale » des
« autre » (5 sur 10 seulement). Plus de déformation n'est donc pas toujours mieux.

ATTENTION : les variantes restent des copies déformées des mêmes gestes. Elles ne remplacent pas une
collecte variée (plusieurs séances), et le score du Studio devient trop optimiste (des variantes d'un même
geste tombent dans l'entraînement ET dans le test) : vérifier sur la carte, ou dans le Studio avec des
fichiers de test faits uniquement de vrais gestes (collectés à part avec --out test).

Sans dépendance (Python seul).
"""
import argparse
import json
import math
import os
import random
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROFILE = os.path.join(HERE, "profil_collecte.json")

PRETRIGGER = 16          # = WAND_PRETRIGGER (app_config.h) : échantillons d'avant le déclenchement
AXES = 6                 # ax, ay, az, gx, gy, gz entrelacés
ACTIVE_FRAC = 0.25       # « en mouvement » = norme gyro > 25 % de son maximum dans le geste
END_MARGIN = 12          # la fin du mouvement d'une variante reste à 12 échantillons (~0,1 s) du bord
NOISE = 1.0              # écart-type du bruit, en unités (1 unité = 32 mg ou 4 °/s à ±16 g / ±2000 °/s)

# Déformations maximales à --force 1 (proportionnelles à --force ; 0,5 par défaut)
SPEED_MAX = 0.45         # facteur de durée tiré dans [1/(1+0,45), 1+0,45] ≈ [0,69 ; 1,45]
AMPL_MAX = 0.25          # amplitude ×[0,75 ; 1,25]
ROT_MAX_DEG = 12.0       # rotation de la baguette jusqu'à 12°
SHIFT_MAX = 4            # décalage du départ de ±4 échantillons


def load_profile(path):
    with open(path, encoding="utf8") as f:
        p = json.load(f)
    return p["classes"], int(p["longueur"]), int(p.get("valeur_max", 1000))


def read_csv(path, length):
    rows = []
    with open(path, encoding="ascii") as f:
        for k, line in enumerate(f, 1):
            line = line.strip()
            if not line:
                continue
            vals = [float(v) for v in line.split(",")]
            if len(vals) != length:
                raise ValueError(f"{os.path.basename(path)} ligne {k} : {len(vals)} valeurs au lieu de {length}")
            rows.append(vals)
    return rows


def to_frames(row, mid):
    """Ligne entrelacée -> liste de [ax, ay, az, gx, gy, gz] centrés (0 = zéro physique)."""
    return [[row[t * AXES + i] - mid for i in range(AXES)] for t in range(len(row) // AXES)]


def to_row(frames, mid, vmax):
    out = []
    for fr in frames:
        out.extend(min(vmax, max(0, int(round(v + mid)))) for v in fr)
    return out


def gyro_norm(fr):
    return math.sqrt(fr[3] ** 2 + fr[4] ** 2 + fr[5] ** 2)


def active_end(frames):
    """Dernier échantillon « en mouvement » (norme gyro > ACTIVE_FRAC du maximum)."""
    norms = [gyro_norm(fr) for fr in frames]
    peak = max(norms) or 1.0
    last = PRETRIGGER
    for t, n in enumerate(norms):
        if n > ACTIVE_FRAC * peak:
            last = t
    return last


def gravity(frames):
    """Gravité vue par la baguette : moyenne des échantillons d'avant le déclenchement (baguette quasi immobile)."""
    pre = frames[:PRETRIGGER]
    return [sum(fr[i] for fr in pre) / len(pre) for i in range(3)]


def sample(frames, x):
    """Interpolation linéaire au temps x (en échantillons) ; au-delà des bords : moyenne des 8 bords."""
    n = len(frames)
    if x <= 0:
        return list(frames[0])
    if x >= n - 1:
        tail = frames[-8:]
        return [sum(fr[i] for fr in tail) / len(tail) for i in range(AXES)]
    k = int(x)
    a = x - k
    return [frames[k][i] * (1 - a) + frames[k + 1][i] * a for i in range(AXES)]


def rotation_matrix(axis, angle):
    x, y, z = axis
    c, s, C = math.cos(angle), math.sin(angle), 1 - math.cos(angle)
    return [[c + x * x * C, x * y * C - z * s, x * z * C + y * s],
            [y * x * C + z * s, c + y * y * C, y * z * C - x * s],
            [z * x * C - y * s, z * y * C + x * s, c + z * z * C]]


def rotate(m, v):
    return [m[0][0] * v[0] + m[0][1] * v[1] + m[0][2] * v[2],
            m[1][0] * v[0] + m[1][1] * v[1] + m[1][2] * v[2],
            m[2][0] * v[0] + m[2][1] * v[1] + m[2][2] * v[2]]


def random_axis(rng):
    while True:
        v = [rng.gauss(0, 1) for _ in range(3)]
        n = math.sqrt(sum(c * c for c in v))
        if n > 1e-6:
            return [c / n for c in v]


def variant(frames, rng, force, half_range):
    """Une variante d'un geste (frames centrés). half_range = demi-échelle (500) : limite de saturation."""
    n = len(frames)
    g = gravity(frames)
    end = active_end(frames)

    # Vitesse : facteur de durée s (s > 1 = plus lent), plafonné pour ne pas couper la fin du geste
    span = 1 + SPEED_MAX * force
    s = math.exp(rng.uniform(-math.log(span), math.log(span)))
    s_cap = (n - 1 - END_MARGIN - PRETRIGGER) / max(1, end - PRETRIGGER)
    if s > s_cap:
        s = max(1.0, s_cap) if end < n - 1 - END_MARGIN else 1.0   # geste déjà coupé (autre) : pas d'étirement
    shift = rng.randint(-SHIFT_MAX, SHIFT_MAX) if force > 0 else 0
    ampl = 1 + rng.uniform(-AMPL_MAX, AMPL_MAX) * force
    rot = rotation_matrix(random_axis(rng), math.radians(rng.uniform(0, ROT_MAX_DEG * force)))

    def build(amp):
        out = []
        for t in range(n):
            u = t - shift
            x = u if u < PRETRIGGER else PRETRIGGER + (u - PRETRIGGER) / s
            fr = sample(frames, x)
            dyn = [(fr[i] - g[i]) * amp / (s * s) for i in range(3)]       # accélération propre
            acc = rotate(rot, [g[i] + dyn[i] for i in range(3)])
            gyr = rotate(rot, [fr[3 + i] * amp / s for i in range(3)])
            out.append(acc + gyr)
        return out

    limit = half_range - 2 - 4 * NOISE                                   # jamais de saturation inventée
    out = build(ampl)
    for _ in range(20):
        if max(abs(v) for fr in out for v in fr) <= limit:
            break
        ampl *= 0.9                                                      # geste trop violent : moins ample
        out = build(ampl)
    return [[v + rng.gauss(0, NOISE) for v in fr] for fr in out]


def main(argv=None):
    ap = argparse.ArgumentParser(description="Variantes réalistes des gestes de la baguette (augmentation de données)")
    ap.add_argument("--entree", default=".", help="dossier des vrais CSV, relatif à tools/ (défaut : tools/)")
    ap.add_argument("--sortie", default="augmente", help="dossier des CSV augmentés, relatif à tools/ (défaut : augmente)")
    ap.add_argument("-n", "--variantes", type=int, default=10, help="variantes par vrai geste (défaut : 10)")
    ap.add_argument("--force", type=float, default=0.5, help="force des déformations, 0 à 1.5 (défaut : 0.5)")
    ap.add_argument("--classes", nargs="+", help="seulement ces classes (défaut : toutes celles du profil)")
    ap.add_argument("--sans-originaux", action="store_true", help="n'écrire que les variantes")
    ap.add_argument("--graine", type=int, default=1, help="graine du hasard (même graine = mêmes fichiers)")
    args = ap.parse_args(argv)

    classes, length, vmax = load_profile(PROFILE)
    mid = vmax / 2
    src = os.path.normpath(os.path.join(HERE, args.entree))
    dst = os.path.normpath(os.path.join(HERE, args.sortie))
    if src == dst:
        print("ERREUR : le dossier de sortie doit être différent de celui des vrais gestes.")
        return 2
    if not 0 <= args.force <= 1.5 or args.variantes < 0:
        print("ERREUR : --force entre 0 et 1.5, --variantes >= 0.")
        return 2
    rng = random.Random(args.graine)
    os.makedirs(dst, exist_ok=True)

    done = 0
    for cls in args.classes or classes:
        path = os.path.join(src, f"{cls}.csv")
        if not os.path.exists(path):
            print(f"  {cls:8s} : pas de fichier {path} (ignoré)")
            continue
        rows = read_csv(path, length)
        out = [] if args.sans_originaux else [to_row(to_frames(r, mid), mid, vmax) for r in rows]
        for r in rows:
            frames = to_frames(r, mid)
            out.extend(to_row(variant(frames, rng, args.force, mid), mid, vmax) for _ in range(args.variantes))
        with open(os.path.join(dst, f"{cls}.csv"), "w", newline="\n", encoding="ascii") as f:
            for row in out:
                f.write(",".join(map(str, row)) + "\n")
        print(f"  {cls:8s} : {len(rows)} vrais gestes -> {len(out)} lignes")
        done += 1
    if not done:
        print(f"ERREUR : aucun CSV de classe dans {src}")
        return 1
    print(f"Fichiers écrits dans {dst}")
    print("Rappel : le score du Studio est trop optimiste avec des variantes ; vérifier sur la carte.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
