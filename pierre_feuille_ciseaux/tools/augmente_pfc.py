#!/usr/bin/env python3
"""
augmente_pfc.py - Augmentation des données réelles de Pierre-Feuille-Ciseaux pour NanoEdge AI Studio.

À partir des signaux collectés sur la carte (pierre.csv, feuille.csv, ciseaux.csv : une ligne = 8 trames
8x8 = 512 valeurs prétraitées), fabrique des variantes plausibles de chaque signal :
  - rotation de la main dans le plan (même angle pour les 8 trames, petite dérive d'une trame à l'autre) ;
  - miroir gauche/droite (l'autre main) ;
  - relief plus ou moins marqué (main plus ou moins inclinée, plus ou moins épaisse) ;
  - vitesse du geste un peu différente (trames rééchantillonnées) ;
  - bruit de mesure, et quelques zones perdues (statut invalide du capteur).
Chaque variante est ensuite REMISE AUX RÈGLES DU PRÉTRAITEMENT DE LA CARTE (Src/tof_preproc.c) : dans
chaque trame, la zone la plus proche vaut 100, une zone de main vaut au moins 20, le fond 0, au moins
PRE_MIN_HAND_ZONES zones, main recentrée (PRE_MAX_SHIFT zones au plus), valeurs entières. Une variante
qui ne les respecte pas, et que la carte n'aurait donc jamais envoyée, est écartée.

Une partie des signaux réels est MISE DE CÔTÉ, intacte, pour le test (--reserve) : on vérifie ainsi sur
de vraies données, jamais vues ni augmentées, que l'augmentation aide vraiment.

    python augmente_pfc.py                       # lit tools/*.csv, écrit tools/augmente/
    python augmente_pfc.py --facteur 5 --reserve 0.2 --graine 1
    python augmente_pfc.py --entree ../mes_donnees --sortie ../mes_donnees/augmente

Résultat (importable dans NanoEdge AI Studio, un fichier par classe) :
    augmente/entrainement/<classe>.csv   signaux réels d'entraînement + leurs variantes
    augmente/test/<classe>.csv           signaux réels mis de côté, intacts

Dépendance : pip install numpy
"""
import argparse
import json
import os
import sys
import zlib

import numpy as np

HERE = os.path.dirname(os.path.abspath(__file__))

# Doit correspondre à NUCLEO_L476RG_TOF_PFC/Inc/app_config.h et Src/tof_preproc.c
GRID = 8
FRAMES = 8
ZONES = GRID * GRID
SIGNAL_LEN = ZONES * FRAMES
V_MIN = 20.0              # valeur d'une zone à PRE_DEPTH_WINDOW_MM derrière le point le plus proche
V_MAX = 100.0             # point le plus proche
MIN_HAND_ZONES = 5        # PRE_MIN_HAND_ZONES
MAX_SHIFT = 2             # PRE_MAX_SHIFT


# --------------------------------------------------------------------------- #
# Règles du prétraitement de la carte                                          #
# --------------------------------------------------------------------------- #
def recenter(img):
    """Recentrage identique à recenter() de tof_preproc.c (décalage entier, borné à MAX_SHIFT)."""
    total = img.sum()
    if total <= 0:
        return img
    ys, xs = np.mgrid[0:GRID, 0:GRID]
    c = (GRID - 1) * 0.5
    fx = c - (img * xs).sum() / total
    fy = c - (img * ys).sum() / total
    dx = int(fx + 0.5) if fx >= 0 else int(fx - 0.5)          # arrondi du C
    dy = int(fy + 0.5) if fy >= 0 else int(fy - 0.5)
    dx = max(-MAX_SHIFT, min(MAX_SHIFT, dx))
    dy = max(-MAX_SHIFT, min(MAX_SHIFT, dy))
    if dx == 0 and dy == 0:
        return img
    out = np.zeros_like(img)
    sy = slice(max(0, -dy), GRID - max(0, dy))
    sx = slice(max(0, -dx), GRID - max(0, dx))
    ty = slice(max(0, dy), GRID - max(0, -dy))
    tx = slice(max(0, dx), GRID - max(0, -dx))
    out[ty, tx] = img[sy, sx]
    return out


def conform_frame(img):
    """Ramène une trame aux règles de la carte. Retourne la trame, ou None si la carte l'aurait refusée."""
    img = np.where(img > 0, img, 0.0)
    if not (img > 0).any():
        return None
    img = np.where(img > 0, img + (V_MAX - img.max()), 0.0)   # le point le plus proche vaut 100
    img = np.where(img >= V_MIN, img, 0.0)                    # au-delà de la fenêtre de profondeur : fond
    if (img > 0).sum() < MIN_HAND_ZONES:
        return None
    img = recenter(img)
    img = np.floor(np.clip(img, 0, V_MAX) + 0.5)              # arrondi de uart_put_csv()
    if (img > 0).sum() < MIN_HAND_ZONES:                      # le recentrage a pu sortir des zones
        return None
    return img


def check_signal(sig):
    """Vrai si le signal respecte toutes les règles de la carte (utilisé par les tests)."""
    frames = np.asarray(sig, dtype=float).reshape(FRAMES, GRID, GRID)
    for f in frames:
        act = f[f > 0]
        if act.size < MIN_HAND_ZONES or act.max() != V_MAX or act.min() < V_MIN:
            return False
        if not np.array_equal(f, np.floor(f)):
            return False
    return True


# --------------------------------------------------------------------------- #
# Transformations                                                              #
# --------------------------------------------------------------------------- #
SUB = 6                   # chaque zone est découpée en SUB x SUB points pour la rotation


def rotate(img, angle_deg):
    """Rotation dans le plan autour du centre de la main, en conservant les surfaces : chaque zone de sortie
    est découpée en SUB x SUB points, tournés un à un vers la grille d'origine. Comme une zone du capteur
    mesure sur toute sa surface, une zone tournée est « main » si au moins la moitié de sa surface tombe sur
    la main ; sa valeur est alors la moyenne de ce qu'elle couvre. Le fond n'est jamais mélangé à la main."""
    total = img.sum()
    if total <= 0 or abs(angle_deg) < 1e-6:
        return img.copy()
    ys, xs = np.mgrid[0:GRID, 0:GRID]
    cx, cy = (img * xs).sum() / total, (img * ys).sum() / total
    a = np.deg2rad(angle_deg)
    sub = (np.arange(GRID * SUB) + 0.5) / SUB - 0.5          # centres des points, en unités de zone
    py, px = np.meshgrid(sub, sub, indexing="ij")
    sx = cx + (px - cx) * np.cos(a) + (py - cy) * np.sin(a)  # rotation inverse : d'où vient chaque point
    sy = cy - (px - cx) * np.sin(a) + (py - cy) * np.cos(a)
    xi, yi = np.rint(sx).astype(int), np.rint(sy).astype(int)
    inside = (xi >= 0) & (xi < GRID) & (yi >= 0) & (yi < GRID)
    src = np.zeros(px.shape)
    src[inside] = img[yi[inside], xi[inside]]
    blocks = src.reshape(GRID, SUB, GRID, SUB)
    hand = (blocks > 0).mean(axis=(1, 3))                    # part de la zone couverte par la main
    vsum = blocks.sum(axis=(1, 3))
    vcnt = (blocks > 0).sum(axis=(1, 3))
    return np.where(hand >= 0.5, vsum / np.maximum(vcnt, 1), 0.0)


def relief(img, k):
    """Relief plus (k > 1) ou moins (k < 1) marqué : distance au point le plus proche multipliée par k."""
    return np.where(img > 0, V_MAX - (V_MAX - img) * k, 0.0)


def resample_time(frames, speed):
    """Geste un peu plus rapide (speed > 1) ou plus lent : les 8 trames sont reprises autour du milieu."""
    c = (FRAMES - 1) / 2
    idx = np.clip(np.rint(c + (np.arange(FRAMES) - c) * speed), 0, FRAMES - 1).astype(int)
    return frames[idx]


def augment(sig, rng, params):
    """Une variante d'un signal (512 valeurs), ou None si elle ne respecte pas les règles de la carte."""
    frames = np.asarray(sig, dtype=float).reshape(FRAMES, GRID, GRID)
    frames = resample_time(frames, rng.uniform(1 - params["vitesse"], 1 + params["vitesse"]))
    mirror = rng.random() < params["miroir"]
    angle = rng.uniform(-params["rotation"], params["rotation"])
    k = np.exp(rng.uniform(-np.log(1 + params["relief"]), np.log(1 + params["relief"])))
    out = []
    for f in frames:
        img = f[:, ::-1] if mirror else f
        img = rotate(img, angle + rng.normal(0, params["derive"]))
        img = relief(img, k)
        hand = img > 0
        img = img + hand * rng.normal(0, params["bruit"], img.shape)
        img = np.where(hand & (rng.random(img.shape) < params["perte"]), 0.0, img)
        img = conform_frame(img)
        if img is None:
            return None
        out.append(img)
    return np.concatenate([f.ravel() for f in out]).astype(int).tolist()


DEFAULTS = {"rotation": 25.0, "derive": 3.0, "miroir": 0.5, "relief": 0.25, "vitesse": 0.25,
            "bruit": 3.0, "perte": 0.02}


# --------------------------------------------------------------------------- #
def load_class(path):
    """Signaux d'un fichier CSV ; les lignes qui n'ont pas 512 valeurs sont comptées et écartées."""
    sigs, bad = [], 0
    with open(path) as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            try:
                v = [float(x) for x in line.split(",")]
            except ValueError:
                bad += 1
                continue
            if len(v) != SIGNAL_LEN:
                bad += 1
                continue
            sigs.append(v)
    return sigs, bad


def write_csv(path, sigs):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", newline="\n") as f:
        for s in sigs:
            f.write(",".join(str(int(round(v))) for v in s) + "\n")


def classes_from_profile():
    with open(os.path.join(HERE, "profil_collecte.json"), encoding="utf-8") as f:
        return json.load(f)["classes"]


def run(in_dir, out_dir, classes, factor, reserve, seed, params, out=sys.stdout):
    report = {}
    for cls in classes:
        # Deux tirages indépendants par classe : le partage entraînement / test ne dépend que de la graine
        # et de la classe. Avec la même graine, --facteur 0 et --facteur 5 mettent donc de côté les MÊMES
        # signaux, ce qui permet de comparer honnêtement deux librairies sur le même test.
        key = zlib.crc32(cls.encode())
        split_rng = np.random.default_rng([seed, key, 0])
        rng = np.random.default_rng([seed, key, 1])
        src = os.path.join(in_dir, f"{cls}.csv")
        if not os.path.exists(src):
            out.write(f"  {cls:<8} : {src} absent, classe ignorée\n")
            continue
        sigs, bad = load_class(src)
        if not sigs:
            out.write(f"  {cls:<8} : aucun signal de {SIGNAL_LEN} valeurs dans {src}\n")
            continue
        order = split_rng.permutation(len(sigs))
        n_test = int(round(len(sigs) * reserve)) if len(sigs) > 1 else 0
        test = [sigs[i] for i in order[:n_test]]
        train = [sigs[i] for i in order[n_test:]]
        gen, rejected = [], 0
        for s in train:
            made = 0
            for _ in range(factor * 4):                     # quelques essais par variante voulue
                v = augment(s, rng, params)
                if v is None:
                    rejected += 1
                    continue
                gen.append(v)
                made += 1
                if made == factor:
                    break
        mixed = train + gen
        mixed = [mixed[i] for i in rng.permutation(len(mixed))]
        write_csv(os.path.join(out_dir, "entrainement", f"{cls}.csv"), mixed)
        write_csv(os.path.join(out_dir, "test", f"{cls}.csv"), test)
        z_real = np.mean([(np.asarray(s).reshape(FRAMES, ZONES) > 0).sum(1).mean() for s in train])
        z_gen = np.mean([(np.asarray(s).reshape(FRAMES, ZONES) > 0).sum(1).mean() for s in gen]) if gen else 0
        report[cls] = {"reels": len(sigs), "ecartes": bad, "entrainement": len(train), "test": len(test),
                       "variantes": len(gen), "refusees": rejected}
        out.write(f"  {cls:<8} : {len(sigs)} réels ({bad} lignes écartées) -> {len(train)} entraînement + "
                  f"{len(gen)} variantes, {len(test)} mis de côté pour le test ; zones de main par trame : "
                  f"{z_real:.1f} (réels) / {z_gen:.1f} (variantes)\n")
    return report


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--entree", default=HERE, help="dossier des CSV réels (défaut : ce dossier, tools/)")
    ap.add_argument("--sortie", help="dossier de sortie (défaut : <entrée>/augmente)")
    ap.add_argument("--classes", nargs="+", help="classes (défaut : celles de profil_collecte.json)")
    ap.add_argument("--facteur", type=int, default=5, help="variantes par signal réel (défaut 5)")
    ap.add_argument("--reserve", type=float, default=0.2,
                    help="part des signaux réels mise de côté, intacte, pour le test (défaut 0.2)")
    ap.add_argument("--graine", type=int, default=1, help="graine du hasard : même graine = mêmes fichiers")
    for k, v in DEFAULTS.items():
        ap.add_argument(f"--{k}", type=float, default=v, help=argparse.SUPPRESS)
    args = ap.parse_args(argv)
    if not 0 <= args.reserve < 1 or args.facteur < 0:
        ap.error("--reserve entre 0 et 1 (exclu), --facteur positif")
    in_dir = os.path.abspath(args.entree)
    out_dir = os.path.abspath(args.sortie or os.path.join(in_dir, "augmente"))
    if os.path.normpath(out_dir) == os.path.normpath(in_dir):
        ap.error("--sortie doit être différent de --entree : les données réelles ne sont jamais modifiées")
    params = {k: getattr(args, k) for k in DEFAULTS}
    print(f"Données réelles : {in_dir}")
    report = run(in_dir, out_dir, args.classes or classes_from_profile(), args.facteur, args.reserve,
                 args.graine, params)
    if not report:
        print("Aucune donnée trouvée : collecter d'abord avec rps_logger_gui.py (fichiers pierre.csv...).")
        return 1
    print(f"\nRésultat : {out_dir}")
    print("  entrainement/<classe>.csv : à importer dans NanoEdge AI Studio (Signals)")
    print("  test/<classe>.csv         : vrais signaux mis de côté, pour la validation (Emulator)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
