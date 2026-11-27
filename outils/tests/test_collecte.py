#!/usr/bin/env python3
"""Tests sans carte de l'outil commun de collecte (outils/collecte_coeur.py, collecte_nanoedge.py) :
profils, analyse des lignes, fichiers par classe et annulation, signaux de démo, collecte en ligne de
commande avec une fausse carte, et (avec un affichage) l'interface graphique pour les trois types
d'affichage. Vérifie aussi les profils et les lanceurs de Pierre-Feuille-Ciseaux et de la Sentinelle.   (make test)"""
import io
import json
import os
import queue
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
sys.path.insert(0, os.path.join(HERE, ".."))
import collecte_coeur as cc  # noqa: E402
import collecte_nanoedge as cn  # noqa: E402

failures = 0


def check(cond, msg):
    global failures
    print(("  ok    : " if cond else "  ECHEC : ") + msg)
    failures += 0 if cond else 1


def raises(fn, text=""):
    try:
        fn()
    except cc.ProfileError as e:
        return text in str(e)
    return False


TMP = tempfile.mkdtemp()
GRID = {"nom": "Test grille", "classes": ["a", "b", "c"], "longueur": 128,
        "affichage": {"type": "grille", "taille": 8, "trames": 2, "min": 20}, "par_passe": 5}
CURVES = {"nom": "Test courbes", "classes": ["gauche", "droite"], "longueur": 128,
          "affichage": {"type": "courbes", "axes": ["x", "y", "d", "s"], "instants": 32, "ordre": "entrelace"}}
DIST = {"nom": "Test distances", "classes": ["normal", "anomalie"], "longueur": 128,
        "affichage": {"type": "grille", "taille": 8, "trames": 2, "proche": "petit", "fond": 100,
                      "mm_par_unite": 30, "ecart_min": 4},
        "conseil": {"normal": "ne touche à rien", "anomalie": "passe devant"}, "import": "Regular / Abnormal"}
RAW = {"nom": "Test brut", "classes": ["ok", "panne"], "longueur": 50, "affichage": {"type": "brut"}}

# --------------------------------------------------------------------------- #
print("[profils]")
p = cc.Profile(GRID, TMP)
check(p.classes == ["a", "b", "c"] and p.display == "grille" and p.out_dir == os.path.abspath(TMP),
      "profil grille lu ; dossier des CSV relatif au profil")
check(raises(lambda: cc.Profile({"longueur": 4}), "classes"), "classes absentes : erreur claire")
check(raises(lambda: cc.Profile({"classes": ["a", "a"]}), "deux fois"), "classe en double refusée")
check(raises(lambda: cc.Profile({"classes": ["a/b"]}), "fichiers"), "nom de classe inutilisable comme fichier refusé")
check(raises(lambda: cc.Profile(dict(GRID, longueur=100)), "grille"), "grille incohérente avec la longueur refusée")
check(raises(lambda: cc.Profile(dict(CURVES, longueur=100)), "courbes"), "courbes incohérentes avec la longueur refusées")
check(raises(lambda: cc.Profile(dict(GRID, affichage={"type": "camembert"})), "affichage"), "type d'affichage inconnu refusé")
bad = os.path.join(TMP, "mauvais.json")
open(bad, "w").write('{"classes": ["a",]}')
check(raises(lambda: cc.Profile.load(bad), "JSON invalide"), "JSON invalide : numéro de ligne donné")
pc = cc.Profile(CURVES)
sig = list(range(128))
check(pc.curves(sig)[0][:3] == [0, 4, 8] and pc.curves(sig)[3][-1] == 127, "courbes entrelacées : x0,y0,d0,s0,x1...")
pc2 = cc.Profile(dict(CURVES, affichage=dict(CURVES["affichage"], ordre="par_axe")))
check(pc2.curves(sig)[1][:2] == [32, 33], "courbes par axe : x0..x31, y0..y31...")
check(len(p.grid_frames(list(range(128)))) == 2, "grille : signal découpé en trames")
pw = cc.Profile(dict(GRID, mirroir_x=True, affichage=dict(GRID["affichage"], mirroir_y=True)))
check(len(pw.warnings) == 2 and "mirroir_x" in pw.warnings[0] and "mirroir_y" in pw.warnings[1]
      and "miroir_haut_bas" in pw.warnings[1] and not cc.Profile(GRID).warnings,
      "clés inconnues (fautes de frappe) signalées, avec les clés possibles ; profil correct : aucune alerte")
check(p.display_order()[:3] == [0, 1, 2] and p.display_order(True, False)[:3] == [7, 6, 5]
      and p.display_order(False, True)[0] == 56 and p.display_order(True, True)[0] == 63,
      "miroirs d'affichage : ↔ inverse les colonnes, ↕ les lignes")
pm = cc.Profile(dict(GRID, affichage=dict(GRID["affichage"], miroir_gauche_droite=True)))
frame = [0] * 64
frame[0] = 100                                               # zone 0 : coin haut gauche du capteur
r = cn.ascii_render(pm, [0] * 64 + frame)
check(r[1].endswith("@@|") and r[1].startswith("    |  "), "rendu texte avec miroir ↔ : la zone 0 s'affiche à droite")

print("[grille en distances : proche = petit, fond, écarts]")
pd = cc.Profile(DIST)
check(not pd.warnings and pd.near == "petit" and pd.background == 100 and pd.mm_per_unit == 30 and pd.diff_min == 4,
      "options proche / fond / mm_par_unite / ecart_min lues")
check(pd.closeness(100) is None and pd.closeness(0) == 1.0 and pd.closeness(90) < pd.closeness(10),
      "proche = petit : 100 = case vide, petite valeur = proche (clair)")
check(p.closeness(0) is None and p.closeness(100) == 1.0 and cc.Profile(GRID).background == 0,
      "proche = grand (défaut) : 0 = case vide, 100 = proche, comme avant")
check(raises(lambda: cc.Profile(dict(DIST, affichage=dict(DIST["affichage"], proche="loin"))), "proche"),
      "« proche » inconnu refusé")
frame = [100] * 64
frame[3] = 0                                         # objet tout contre le capteur
check(pd.active_count(frame * 2) == 1, "zones actives = zones qui voient quelque chose")
ref = [50] * 64
cur = [50] * 64
cur[0], cur[1], cur[2] = 40, 60, 53
check(pd.diff_zones(cur, ref)[:3] == [1, -1, 0], "écarts : plus proche = +1 (apparu), plus loin = -1, sous le seuil = 0")
check(cc.Profile(dict(GRID, affichage=dict(GRID["affichage"], ecart_min=4))).diff_zones([60, 40], [50, 50]) == [1, -1],
      "écarts avec proche = grand : une valeur qui monte = plus proche")
check(pd.mean_frame([10] * 64 + [30] * 64) == [20] * 64, "trame moyenne du signal")
check(pd.advice_for("normal") == "ne touche à rien" and pd.advice_for("anomalie") == "passe devant"
      and p.advice_for("a") == "", "conseil par classe")
check(any("zzz" in w for w in cc.Profile(dict(DIST, conseil={"zzz": "x"})).warnings), "conseil pour une classe inconnue : signalé")
check(pd.import_hint == "Regular / Abnormal", "texte d'import dans le Studio")
ds = cc.demo_signal(pd, 1, 0.3)
check(max(ds) <= 51 and min(ds) < 30 and pd.active_count(ds) == 64,
      "démo en distances : un décor qui voit partout, un objet plus proche devant")
qd = queue.Queue()
dsrc = cc.DemoSource(cc.Profile(dict(DIST, attente="")), qd, rate=50, phase_s=0.3)
dsrc.start()
time.sleep(0.5)
dsrc.stop()
dsrc.join(2)
dl = [qd.get()[1] for _ in range(qd.qsize())]
check(not any("attente" in l for l in dl) and sum(1 for l in dl if l[:1].isdigit()) >= 10,
      "« attente » vide : la démo envoie des signaux en continu, sans phase d'attente")
r = cn.ascii_render(pd, frame * 2)
check(r[1] == "    |" + " " * 6 + "@@" + " " * 8 + "|", "rendu texte : case vide = espace, proche = @")

print("[analyse des lignes]")
check(cc.parse_line("# En attente d'une main\r\n") == ("msg", "En attente d'une main"), "message de la carte")
check(cc.parse_line("1,2,3\n", 3) == ("signal", [1, 2, 3]), "signal CSV d'entiers")
check(cc.parse_line("1.5,2,3", 3) == ("signal", [1.5, 2.0, 3.0]), "signal CSV avec décimales")
check(cc.parse_line("1,2", 3)[0] == "bad", "longueur fausse signalée")
check(cc.parse_line("Class found: pierre", 3) is None and cc.parse_line("  ", 3) is None, "autre texte ignoré")

print("[fichiers par classe, annulation]")
d = os.path.join(TMP, "csv")
files = cc.ClassFiles(d, ["a", "b"])
check(files.counts() == {"a": 0, "b": 0}, "dossier absent : 0 partout")
files.start_pass("a")
for k in range(3):
    files.add([k, k + 1])
check(files.end_pass() == ("a", 3) and files.count("a") == 3, "passe de 3 signaux enregistrée")
files.start_pass("a")
files.add([9, 9])
files.end_pass()
before = open(files.path("a")).read()
check(files.undo() == ("a.csv", 1) and open(files.path("a")).read() == "0,1\n1,2\n2,3\n",
      "annulation : seule la dernière passe est retirée")
check(files.undo() is None and before.count("\n") == 4, "pas de seconde annulation")
open(files.path("b"), "w").write("1,1\n2,2")                   # fichier sans fin de ligne finale
files.start_pass("b")
files.add([3, 3])
files.end_pass()
check(open(files.path("b")).read() == "1,1\n2,2\n3,3\n", "fin de ligne ajoutée à un fichier qui n'en avait pas")
files.undo()
check(open(files.path("b")).read() == "1,1\n2,2\n" and files.count("b") == 2, "annulation après cette réparation")
files.start_pass("a")
files.end_pass()
check(files.undo_info is None, "passe vide : rien à annuler")

print("[signaux de démo]")
for prof in (cc.Profile(GRID), cc.Profile(CURVES), cc.Profile(RAW)):
    sigs = [cc.demo_signal(prof, k, 0.5) for k in range(len(prof.classes))]
    ok = all(len(s) == prof.length and all(0 <= v <= prof.value_max for v in s) for s in sigs)
    check(ok and sigs[0] != sigs[1], f"{prof.display} : bonne longueur, valeurs dans l'échelle, classes différentes")
q = queue.Queue()
src = cc.DemoSource(cc.Profile(GRID), q, rate=50, phase_s=0.3, idle_s=0.1, seed=1)
src.start()
time.sleep(1.0)
src.stop()
src.join(timeout=2)
items = []
while not q.empty():
    items.append(q.get())
lines = [cc.parse_line(x[1], 128) for x in items if x[0] == "line"]
check(any(x and x[0] == "msg" for x in lines) and sum(1 for x in lines if x and x[0] == "signal") > 10,
      "source de démo : messages d'attente puis signaux valides")

print("[collecte en ligne de commande, fausse carte]")
prof = cc.Profile(GRID)
good = ",".join(["0"] * 64 + ["50"] * 64) + "\n"
f2 = cc.ClassFiles(os.path.join(TMP, "cli"), prof.classes)


class LateQueue:
    """File qui ne reçoit les signaux qu'après le compte à rebours (comme une vraie carte)."""

    def __init__(self, lines):
        self.lines, self.armed = list(lines), False

    def get_nowait(self):
        self.armed = True
        raise queue.Empty

    def get(self, timeout=None):
        if self.armed and self.lines:
            return ("line", self.lines.pop(0))
        raise queue.Empty


out = io.StringIO()
cn.cli_record(prof, LateQueue(["# En attente\n", "9,9\n", good, good, good]), f2, ["b"], 2, 128,
              ask=lambda _: "", out=out, countdown=0)
check(f2.count("b") == 2 and "[carte] En attente" in out.getvalue() and "2 signaux ajoutés" in out.getvalue()
      and "[ligne ignorée] 2 valeurs au lieu de 128" in out.getvalue(),
      "2 signaux enregistrés ; messages affichés ; ligne de mauvaise longueur ignorée")
check("|" in cn.ascii_render(prof, [50] * 128)[1] and len(cn.ascii_render(cc.Profile(CURVES), [10] * 128)) == 5,
      "rendu texte : grille, et une ligne par axe pour les courbes")

print("[dossier des CSV]")
pr = cc.Profile(GRID, TMP)
check(cn.resolve_out(pr, None) == pr.out_dir and cn.resolve_out(pr, None, demo=True) == os.path.join(pr.out_dir, "demo"),
      "défaut : dossier du profil ; en démo : son sous-dossier demo/")
check(cn.resolve_out(pr, "essai", demo=True) == os.path.join(pr.base_dir, "essai")
      and cn.resolve_out(pr, "/tmp/x") == "/tmp/x", "--out l'emporte, relatif au dossier du profil")

print("[profil et lanceurs de Pierre-Feuille-Ciseaux]")
pfc_tools = os.path.join(ROOT, "pierre_feuille_ciseaux", "tools")
pfc = cc.Profile.load(os.path.join(pfc_tools, "profil_collecte.json"))
check(pfc.classes == ["pierre", "feuille", "ciseaux"] and pfc.length == 512 and pfc.frames == 8 and pfc.baud == 115200
      and pfc.out_dir == os.path.normpath(pfc_tools), "profil PFC : 3 classes, 8 trames 8x8, 115200 bauds, CSV dans tools/")
for script in ("rps_logger.py", "rps_logger_gui.py"):
    r = subprocess.run([sys.executable, os.path.join(pfc_tools, script), "--help"], capture_output=True, text=True)
    check(r.returncode == 0 and "--demo" in r.stdout, f"{script} --help fonctionne (lanceur de l'outil commun)")
r = subprocess.run([sys.executable, os.path.join(pfc_tools, "rps_logger.py")], capture_output=True, text=True)
check(r.returncode != 0 and "--port" in r.stderr, "rps_logger.py sans --port : erreur claire, comme avant")

print("[profil et lanceurs de la Sentinelle]")
sen_tools = os.path.join(ROOT, "sentinelle", "tools")
sen = cc.Profile.load(os.path.join(sen_tools, "profil_collecte.json"))
check(not sen.warnings and sen.classes == ["normal", "anomalie"] and sen.length == 512 and sen.baud == 115200
      and sen.near == "petit" and sen.background == 100 and sen.out_dir == os.path.normpath(sen_tools),
      "profil Sentinelle : normal / anomalie, 512 valeurs, distances (100 = pas de cible), CSV dans tools/")
check(sen.mm_per_unit * sen.value_max == 3000 and sen.diff_min * sen.mm_per_unit == 120,
      "profil Sentinelle : 100 = 3000 mm (SENT_MAX_MM), écart mini 120 mm (SENT_MAP_MIN_MM)")
for script in ("sentinel_logger.py", "sentinel_logger_gui.py"):
    r = subprocess.run([sys.executable, os.path.join(sen_tools, script), "--help"], capture_output=True, text=True)
    check(r.returncode == 0 and "--demo" in r.stdout and "--baud" in r.stdout, f"{script} --help fonctionne (lanceur de l'outil commun)")
r = subprocess.run([sys.executable, os.path.join(sen_tools, "sentinel_logger.py")], capture_output=True, text=True)
check(r.returncode != 0 and "--port" in r.stderr, "sentinel_logger.py sans --port : erreur claire, comme avant")


# --------------------------------------------------------------------------- #
def gui_tests():
    try:
        import tkinter as tk
        tk.Tk().destroy()
    except Exception:  # noqa: BLE001
        print("[interface] pas d'affichage : tests ignorés")
        return
    print("[interface graphique]")
    from tkinter import messagebox
    messagebox.askyesno = lambda *a, **k: True
    for data in (GRID, DIST, CURVES, RAW):
        prof = cc.Profile(data)
        outd = os.path.join(TMP, "gui_" + prof.display)
        app = cn.run_gui(prof, outd, demo=False)
        app.update()
        q = app.q
        app.source = type("S", (), {"stop": lambda s: None, "is_alive": lambda s: False})()
        app.target.set(3)
        app.cls.set(prof.classes[1])
        app.begin_record()
        sig = cc.demo_signal(prof, 1, 0.2)
        for _ in range(5):
            q.put(("line", ",".join(map(str, sig)) + "\n"))
        q.put(("line", "1,2,3\n"))
        q.put(("line", f"# {prof.waiting_prefix} (1)\n"))
        q.put(("line", f"# {prof.waiting_prefix} (2)\n"))
        t0 = time.monotonic()
        while app.state != "idle" and time.monotonic() - t0 < 15:
            app.update()
            time.sleep(0.02)
        n = app.files.count(prof.classes[1])
        log = app.log.get("1.0", "end")
        name = prof.name
        check(n == 3 and app.state == "idle", f"{name} : passe de 3 signaux enregistrée, arrêt automatique")
        check("(2)" in log and "(1)" not in log and "Ligne ignorée" in log,
              f"{name} : message d'attente remplacé, ligne de mauvaise longueur signalée")
        app.undo_last()
        check(app.files.count(prof.classes[1]) == 0, f"{name} : annulation de la passe")
        if data is DIST:
            v = app.view
            scene = [100] * 64
            scene[9] = 20                                    # objet à 60 cm
            v.update(scene * 2)
            check(v.canvas.itemcget(v.cells[0], "fill") == cn.BG_CELL and v.canvas.itemcget(v.cells[9], "fill") != cn.BG_CELL,
                  "distances : pas de cible = case vide, objet = couleur")
            check("60 cm" in v.info_text(), "objet le plus proche annoncé en cm")
            v.toggle(v.show_dist)
            check(v.canvas.itemcget(v.labels[9], "text") == "60" and v.canvas.itemcget(v.labels[0], "text") == "",
                  "case Distances (D) : distance en cm dans chaque zone qui voit quelque chose")
            app.set_reference()
            moved = list(scene)
            moved[9], moved[20] = 100, 30                    # l'objet a disparu, un autre est apparu à 90 cm
            v.update(moved * 2)
            check(v.canvas.itemcget(v.cells[9], "fill") == cn.DIFF_MINUS and v.canvas.itemcget(v.cells[20], "fill") == cn.DIFF_PLUS
                  and v.canvas.itemcget(v.cells[0], "fill") == cn.DIFF_SAME, "référence (R) + écarts : − disparu, + apparu")
            check(v.canvas.itemcget(v.labels[20], "text") == "+210" and "2 zone(s)" in v.info_text()
                  and "120 mm" in v.info_text(), "écarts en cm et nombre de zones changées, seuil en mm")
            check("Scène de référence" in app.log.get("1.0", "end") and "Regular / Abnormal" in log,
                  "journal : référence mémorisée, texte d'import")
            app.cls.set("anomalie")
            check("passe devant" in app.advice_lbl.cget("text"), "le conseil suit la classe choisie")
        if data is GRID:
            hot = [0] * 64
            hot[0] = 100
            app.view.update([0] * 64 + hot)
            left = app.view.canvas.itemcget(app.view.cells[0], "fill")
            app.view.flip_x.set(True)
            app.view.redraw()
            right = app.view.canvas.itemcget(app.view.cells[7], "fill")
            moved = app.view.canvas.itemcget(app.view.cells[0], "fill")
            check(left != cn.BG_CELL and right == left and moved == cn.BG_CELL,
                  "case miroir ↔ : la zone 0 passe de gauche à droite à l'écran")
            app.files.start_pass(prof.classes[0])
            app.files.add([0] * 64 + hot)
            app.files.end_pass()
            saved = open(app.files.path(prof.classes[0])).read().split(",")
            check(saved[64] == "100", "miroir d'affichage : le signal enregistré reste celui de la carte")
        app.source = None
        app.on_close()


gui_tests()
print("\n" + ("TOUS LES TESTS PASSENT" if not failures else "TESTS EN ECHEC") + f" ({failures} echec(s))")
sys.exit(1 if failures else 0)
