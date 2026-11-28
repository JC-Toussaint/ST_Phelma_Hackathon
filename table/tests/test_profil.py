#!/usr/bin/env python3
"""test_profil.py - Le profil de collecte, les lanceurs et piano.py de la table qui écoute sont cohérents
avec le firmware (Core/Inc/app_config.h, app.c)."""
import importlib.util
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.join(HERE, "..")
FW = os.path.join(ROOT, "NUCLEO_L476RG_IKS01A3_TABLE", "Core")
sys.path.insert(0, os.path.join(ROOT, "..", "outils"))
import collecte_coeur as cc  # noqa: E402

fails = 0


def check(cond, msg):
    global fails
    print(("  ok    : " if cond else "  ECHEC : ") + msg)
    fails += 0 if cond else 1


cfg = open(os.path.join(FW, "Inc", "app_config.h"), encoding="utf8").read()
define = {m.group(1): m.group(2) for m in re.finditer(r"#define\s+(\w+)\s+(\d+)\b", cfg)}
samples, axes, vmax = int(define["TAP_SAMPLES"]), int(define["TAP_AXES"]), int(define["TAP_VALUE_MAX"])
reject = re.search(r'#define\s+TAP_REJECT_CLASS\s+"(\w+)"', cfg).group(1)

p = cc.Profile.load(os.path.join(ROOT, "tools", "profil_collecte.json"))
check(p.length == samples * axes, f"longueur du profil = TAP_SAMPLES x TAP_AXES = {samples * axes}")
check(p.display == "courbes" and p.samples == samples and len(p.axes) == axes and p.order == "entrelace",
      "affichage : 3 courbes entrelacées de TAP_SAMPLES instants")
check(p.value_max == vmax, f"valeur_max = TAP_VALUE_MAX = {vmax}")
check(p.baud == 115200, "115200 bauds")
check(not p.warnings, "profil sans avertissement")
check(set(p.advice_by_class) == set(p.classes), "un conseil par classe")
check(reject in p.classes, f"classe « {reject} » (rejet), comme TAP_REJECT_CLASS")
check(samples & (samples - 1) == 0, "TAP_SAMPLES est une puissance de 2 (conseil de ST pour NanoEdge)")
sig = cc.demo_signal(p, 0, 0.0)
check(len(sig) == p.length and all(0 <= v <= vmax for v in sig), "signal de démo au format du profil")

app = open(os.path.join(FW, "Src", "app.c"), encoding="utf8").read()
check('"# En attente' in app and p.waiting_prefix == "En attente", "message d'attente du firmware = « attente » du profil")
check('uart_puts("TAPE ")' in app, "le firmware écrit des lignes « TAPE … », lues par piano.py")

for script in ("table_logger_gui.py", "table_logger.py"):
    r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", script), "--help"],
                       capture_output=True, text=True, timeout=30)
    check(r.returncode == 0 and "--demo" in r.stdout, f"{script} --help")

# piano.py : lecture des lignes de la carte
spec = importlib.util.spec_from_file_location("piano", os.path.join(ROOT, "tools", "piano.py"))
piano = importlib.util.module_from_spec(spec)
spec.loader.exec_module(piano)
check(piano.parse_line("TAPE zone3 95%   (zone1 2% zone2 1% zone3 95% zone4 1% autre 1%)") == ("zone3", 95),
      "piano.py : « TAPE zone3 95% … » -> zone3")
check(piano.parse_line("TAPE - (autre chose)   (zone1 2% autre 90%)") is None, "piano.py : classe autre ignorée")
check(piano.parse_line("TAPE ? pas reconnue (zone2 45%)   (…)") is None, "piano.py : zone pas reconnue ignorée")
check(piano.parse_line("# En attente d'une tape (1667 ech/s)") is None, "piano.py : messages de la carte ignorés")
check(piano.note_for("zone1")[0] == "do" and piano.note_for("zone4")[0] == "fa" and piano.note_for("autre") is None,
      "piano.py : zone1 = do … zone4 = fa ; pas de note pour une autre classe")
check(all(piano.note_for(c) for c in p.classes if c != reject), "piano.py : une note pour chaque zone du profil")
r = subprocess.run([sys.executable, os.path.join(ROOT, "tools", "piano.py"), "--help"],
                   capture_output=True, text=True, timeout=30)
check(r.returncode == 0 and "--demo" in r.stdout, "piano.py --help")

print("\n" + ("TOUS LES TESTS PASSENT" if not fails else "DES TESTS ECHOUENT") + f" ({fails} echec(s))")
sys.exit(1 if fails else 0)
