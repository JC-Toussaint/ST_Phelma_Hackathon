#!/usr/bin/env python3
"""test_profil.py - Le profil de collecte et les lanceurs de la baguette sont cohérents avec le firmware."""
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
sys.path.insert(0, os.path.join(ROOT, "..", "outils"))
import collecte_coeur as cc  # noqa: E402

fails = 0


def check(cond, msg):
    global fails
    print(("  ok    : " if cond else "  ECHEC : ") + msg)
    fails += 0 if cond else 1


cfg = open(os.path.join(ROOT, "NUCLEO_L476RG_IKS01A2_BAGUETTE", "Core", "Inc", "app_config.h"), encoding="utf8").read()
define = {m.group(1): m.group(2) for m in re.finditer(r"#define\s+(\w+)\s+(\d+)\b", cfg)}
samples, axes, vmax = int(define["WAND_SAMPLES"]), int(define["WAND_AXES"]), int(define["WAND_VALUE_MAX"])

p = cc.Profile.load(os.path.join(ROOT, "tools", "profil_collecte.json"))
check(p.length == samples * axes, f"longueur du profil = WAND_SAMPLES x WAND_AXES = {samples * axes}")
check(p.display == "courbes" and p.samples == samples and len(p.axes) == axes and p.order == "entrelace",
      "affichage : 6 courbes entrelacées de WAND_SAMPLES instants")
check(p.value_max == vmax, f"valeur_max = WAND_VALUE_MAX = {vmax}")
check(p.baud == 115200, "115200 bauds")
check(not p.warnings, "profil sans avertissement")
check(set(p.advice_by_class) == set(p.classes), "un conseil par classe")
check("autre" in p.classes, "classe « autre » (rejet), comme WAND_REJECT_CLASS")
sig = cc.demo_signal(p, 0, 0.0)
check(len(sig) == p.length and all(0 <= v <= vmax for v in sig), "signal de démo au format du profil")

for script in ("wand_logger_gui.py", "wand_logger.py"):
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", script), "--help"],
                       capture_output=True, text=True, timeout=30)
    check(r.returncode == 0 and "--demo" in r.stdout, f"{script} --help")

# Augmentation de données (tools/augmenter.py) sur des gestes de démo
import importlib.util  # noqa: E402
import math  # noqa: E402
import shutil  # noqa: E402
import tempfile  # noqa: E402
spec = importlib.util.spec_from_file_location("augmenter", os.path.join(ROOT, "tools", "augmenter.py"))
aug = importlib.util.module_from_spec(spec)
spec.loader.exec_module(aug)
check(aug.PRETRIGGER == int(define["WAND_PRETRIGGER"]) and aug.AXES == axes, "augmenter.py : PRETRIGGER et AXES du firmware")
tmp = tempfile.mkdtemp()
src, dst = os.path.join(tmp, "vrais"), os.path.join(tmp, "aug")
os.makedirs(src)
mid = vmax // 2


def demo_gesture(k, cls_i):
    """Geste au repos (gravité sur az) puis ~0,5 s de rotation, comme un vrai geste."""
    row = []
    for t in range(samples):
        on = 16 <= t < 16 + 52
        w = math.sin(math.pi * (t - 16) / 52) if on else 0.0
        fr = [0, 0, 31, 0, 0, 0]
        fr[3 + cls_i % 3] = int(120 * w * (1 + 0.1 * k))
        fr[cls_i % 3] += int(40 * w)
        row.extend(mid + v for v in fr)
    return row


for i, c in enumerate(p.classes):
    with open(os.path.join(src, f"{c}.csv"), "w", newline="\n") as f:
        for k in range(3):
            f.write(",".join(map(str, demo_gesture(k, i))) + "\n")
rc = aug.main(["--entree", src, "--sortie", dst, "-n", "4"])
rows = {c: aug.read_csv(os.path.join(dst, f"{c}.csv"), p.length) for c in p.classes}
check(rc == 0 and all(len(r) == 3 * 5 for r in rows.values()), "augmenter.py : 3 vrais gestes x (1 + 4 variantes) par classe")
orig = aug.read_csv(os.path.join(src, f"{p.classes[0]}.csv"), p.length)
check(rows[p.classes[0]][:3] == orig, "les vrais gestes sont recopiés tels quels en tête de fichier")
allv = [v for r in rows.values() for row in r for v in row]
check(all(v == int(v) and 0 < v < vmax for v in allv), "variantes : entiers dans l'échelle, aucune saturation")
ends = [aug.active_end(aug.to_frames(row, mid)) for r in rows.values() for row in r]
check(max(ends) < samples - aug.END_MARGIN, "aucune variante coupée en fin de fenêtre")
check(len({tuple(row) for row in rows[p.classes[0]]}) == 15, "variantes toutes différentes")
again = os.path.join(tmp, "aug2")
aug.main(["--entree", src, "--sortie", again, "-n", "4"])
check(aug.read_csv(os.path.join(again, f"{p.classes[0]}.csv"), p.length) == rows[p.classes[0]], "même graine = mêmes fichiers")
check(aug.main(["--entree", src, "--sortie", src]) == 2, "refuse d'écrire dans le dossier des vrais gestes")
shutil.rmtree(tmp, ignore_errors=True)

print("\n" + ("TOUS LES TESTS PASSENT" if not fails else "DES TESTS ECHOUENT") + f" ({fails} echec(s))")
sys.exit(1 if fails else 0)
