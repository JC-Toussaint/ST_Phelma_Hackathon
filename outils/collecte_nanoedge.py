#!/usr/bin/env python3
"""
collecte_nanoedge.py - Collecte de données pour NanoEdge AI Studio, commune à tous les projets du dépôt.

La carte (en mode collecte) envoie une ligne CSV par signal et des messages « # ... ». Ce programme
enregistre les signaux dans UN FICHIER PAR CLASSE (pierre.csv, feuille.csv...), le format qu'attend
NanoEdge AI Studio pour une classification N classes. Ce qui est propre au projet (classes, longueur
du signal, affichage) est décrit dans un profil JSON : voir outils/README.md.

Interface graphique (par défaut) :
    python outils/collecte_nanoedge.py --profil pierre_feuille_ciseaux/tools/profil_collecte.json
    python outils/collecte_nanoedge.py --profil ... --demo           # données simulées, sans carte
                                                                     #   (CSV dans le sous-dossier demo/)
  Choix du port, affichage du signal en direct, classes (touches 1..9), passes avec compte à rebours
  (Espace), progression, nombre de signaux par fichier, suppression du dernier signal (Retour arrière,
  pendant ou après une passe), annulation de la dernière passe.

Ligne de commande (sans fenêtre) :
    python outils/collecte_nanoedge.py --profil ... --cli --port COM5         # collecte guidée
    python outils/collecte_nanoedge.py --profil ... --cli --port COM5 --classes ciseaux --count 200
    python outils/collecte_nanoedge.py --profil ... --view --port COM5        # visualiser seulement

Dépendance : pip install pyserial (tkinter est fourni avec Python sous Windows)
"""
import argparse
import os
import queue
import sys
import threading
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from collecte_coeur import (ClassFiles, DemoSource, Profile, ProfileError,  # noqa: E402
                            SerialSource, parse_line, serial)

try:
    import serial.tools.list_ports  # noqa: F401
except Exception:  # noqa: BLE001
    pass

BG_CELL = "#1e2127"
BG = "#2b2f36"
FG = "#e6e6e6"
MUTED = "#9aa0a6"
ACCENT = "#4fc3f7"
REC = "#ef5350"
OKC = "#81c784"
WARN = "#ffb74d"
PALETTE = [(68, 1, 84), (59, 82, 139), (33, 145, 140), (94, 201, 98), (253, 231, 37)]   # ~viridis
AXIS_COLORS = ["#4fc3f7", "#ffb74d", "#81c784", "#f48fb1", "#ce93d8", "#fff176", "#90a4ae", "#ff8a65"]
SHADES = " .:-=+*#%@"
DIFF_PLUS = "#ef5350"    # écarts : plus proche qu'à la référence (quelque chose est apparu)
DIFF_MINUS = "#42a5f5"   # plus loin (quelque chose a disparu)
DIFF_SAME = "#3a3f47"
NO_SIGNAL_S = 0.6        # sans signal depuis ce délai : « rien devant le capteur »


def zone_color(profile, v):
    """Couleur d'une zone de grille : case vide pour le fond, sinon dégradé du loin (sombre) au proche (clair)."""
    t = profile.closeness(v)
    if t is None:
        return BG_CELL
    x = t * (len(PALETTE) - 1)
    i = min(int(x), len(PALETTE) - 2)
    f = x - i
    a, b = PALETTE[i], PALETTE[i + 1]
    return "#%02x%02x%02x" % tuple(int(a[k] + (b[k] - a[k]) * f) for k in range(3))


def list_ports():
    if serial is None:
        return []
    try:
        ports = [f"{p.device} — {p.description}" for p in serial.tools.list_ports.comports()]
    except Exception:  # noqa: BLE001
        return []
    return sorted(ports, key=lambda s: ("STLink" not in s and "STM" not in s, s))


DEMO_SUBDIR = "demo"


def resolve_out(profile, out, demo=False):
    """Dossier des CSV. --out relatif : par rapport au dossier du profil (comme le champ « dossier »).
    En démo, sans --out : sous-dossier demo/ du dossier du profil, pour ne jamais mêler des signaux
    simulés aux vraies données."""
    if not out:
        return os.path.join(profile.out_dir, DEMO_SUBDIR) if demo else profile.out_dir
    return out if os.path.isabs(out) else os.path.normpath(os.path.join(profile.base_dir, out))


# =========================================================================== #
# Ligne de commande                                                            #
# =========================================================================== #
def shade(t):
    return " " if t is None else SHADES[min(len(SHADES) - 1, 1 + int(t * (len(SHADES) - 1)))]


def ascii_render(profile, values, info=""):
    """Représentation texte du signal (dernière trame pour une grille, une ligne par axe sinon)."""
    vmax = profile.value_max
    lines = []
    if profile.display == "grille":
        g = profile.grid
        last = values[-g * g:]
        frame = [last[i] for i in profile.display_order()]        # miroirs d'affichage du profil
        lines.append("    +" + "-" * (2 * g) + "+")
        for y in range(g):
            row = "".join(shade(profile.closeness(v)) * 2 for v in frame[y * g:(y + 1) * g])
            lines.append("    |" + row + "|")
        lines.append("    +" + "-" * (2 * g) + "+")
    else:
        curves = profile.curves(values) if profile.display == "courbes" else [values]
        names = profile.axes if profile.display == "courbes" else ["signal"]
        for name, c in zip(names, curves):
            step = max(1, len(c) // 60)
            spark = "".join(SHADES[min(len(SHADES) - 1, int(max(0, v) * len(SHADES) / (vmax + 1)))]
                            for v in c[::step])
            lines.append(f"    {name[:8]:>8} |{spark}|")
    lines.append(f"    {info:<60}")
    return lines


class TermView:
    """Affichage texte rafraîchi sur place."""

    def __init__(self, profile, out=sys.stdout):
        self.p, self.out, self.drawn = profile, out, 0

    def show(self, values, info=""):
        lines = ascii_render(self.p, values, info)
        text = (f"\x1b[{self.drawn}F" if self.drawn else "") + "\n".join(lines) + "\n"
        self.out.write(text)
        self.out.flush()
        self.drawn = len(lines)

    def reset(self):
        self.drawn = 0


def read_signal(src_q, expected_len, timeout=1.0, out=sys.stdout):
    """Attend un signal dans la file d'une source ; affiche les messages de la carte. None = délai dépassé."""
    t_end = time.monotonic() + timeout
    while time.monotonic() < t_end:
        try:
            kind, payload = src_q.get(timeout=max(0.01, t_end - time.monotonic()))
        except queue.Empty:
            return None
        if kind == "error":
            raise RuntimeError(payload)
        if kind != "line":
            continue
        parsed = parse_line(payload, expected_len)
        if parsed and parsed[0] == "msg":
            out.write(f"\n[carte] {parsed[1]}\n")
        elif parsed and parsed[0] == "bad":
            out.write(f"\n[ligne ignorée] {parsed[1]} (profil ou mode de la carte ?)\n")
        elif parsed and parsed[0] == "signal":
            return parsed[1]
    return None


def cli_view(profile, src_q, expected_len, stop=None, out=sys.stdout):
    out.write("Visualisation (Ctrl+C pour quitter).\n\n")
    view = TermView(profile, out)
    n, t0 = 0, time.monotonic()
    while stop is None or not stop.is_set():
        v = read_signal(src_q, expected_len, out=out)
        if v is None:
            continue
        n += 1
        view.show(v, f"{len(v)} valeurs/signal, {n / max(1e-3, time.monotonic() - t0):4.1f} signaux/s")


def cli_record(profile, src_q, files, classes, count, expected_len, ask=input, out=sys.stdout, countdown=3):
    for name in classes:
        out.write(f"\n=== Classe « {name.upper()} » ({files.count(name)} signaux déjà dans {files.path(name)}) ===\n")
        if profile.advice_for(name):
            out.write(profile.advice_for(name) + "\n")
        ask("Prends la pose puis appuie sur Entrée...")
        for s in range(countdown, 0, -1):
            out.write(f"  {s}...")
            out.flush()
            time.sleep(1)
        out.write("  GO\n")
        while True:                       # vide ce qui est arrivé pendant le compte à rebours
            try:
                src_q.get_nowait()
            except queue.Empty:
                break
        view = TermView(profile, out)
        files.start_pass(name)
        got = 0
        try:
            while got < count:
                v = read_signal(src_q, expected_len, out=out)
                if v is None:
                    view.reset()
                    out.write("  (aucun signal : rien devant le capteur ?)\n")
                    continue
                got = files.add(v)
                view.show(v, f"{name} : {got}/{count}")
        finally:
            files.end_pass()
        out.write(f"  -> {got} signaux ajoutés à {files.path(name)}\n")


# =========================================================================== #
# Interface graphique                                                          #
# =========================================================================== #
def run_gui(profile, out_dir, demo=False, port=None):
    import tkinter as tk
    from tkinter import filedialog, messagebox, ttk

    class GridView:
        """Grille de la trame la plus récente + vignettes des trames du signal. Les cases « Miroir »
        retournent l'AFFICHAGE seulement : les signaux enregistrés restent ceux de la carte."""

        def __init__(self, parent, p):
            self.p = p
            self.last = None
            self.flip_x = tk.BooleanVar(value=p.flip_x)
            self.flip_y = tk.BooleanVar(value=p.flip_y)
            self.reference = None                         # trame moyenne mémorisée (R)
            self.show_diff = tk.BooleanVar(value=False)
            self.show_dist = tk.BooleanVar(value=False)
            self.cell = max(20, min(48, 368 // p.grid))
            size = self.cell * p.grid
            self.canvas = tk.Canvas(parent, width=size, height=size, bg=BG, highlightthickness=0)
            self.canvas.pack()
            self.cells = [self.canvas.create_rectangle(x * self.cell + 2, y * self.cell + 2,
                                                       (x + 1) * self.cell - 2, (y + 1) * self.cell - 2,
                                                       fill=BG_CELL, outline="")
                          for y in range(p.grid) for x in range(p.grid)]
            self.labels = [self.canvas.create_text((x + 0.5) * self.cell, (y + 0.5) * self.cell, text="",
                                                   font=("Segoe UI", 9, "bold"))
                           for y in range(p.grid) for x in range(p.grid)]
            self.thumb_cells = []
            if p.frames > 1:
                ttk.Label(parent, text=f"Les {p.frames} trames du signal (de la plus ancienne à la plus récente)",
                          style="Muted.TLabel").pack(pady=(8, 2))
                c = max(3, min(6, 380 // (p.frames * p.grid)))
                tw = p.grid * c + 4
                self.thumbs = tk.Canvas(parent, width=p.frames * (tw + 4), height=tw, bg=BG, highlightthickness=0)
                self.thumbs.pack()
                for f in range(p.frames):
                    ox = f * (tw + 4)
                    self.thumb_cells.append([self.thumbs.create_rectangle(
                        ox + 2 + (i % p.grid) * c, 2 + (i // p.grid) * c,
                        ox + 1 + (i % p.grid + 1) * c, 1 + (i // p.grid + 1) * c, fill=BG_CELL, outline="")
                        for i in range(p.grid * p.grid)])

            row = ttk.Frame(parent)
            row.pack(pady=(4, 0))
            ttk.Label(row, text="Affichage :", style="Muted.TLabel").pack(side="left")
            ttk.Checkbutton(row, text="miroir ↔", variable=self.flip_x, command=self.redraw).pack(side="left", padx=6)
            ttk.Checkbutton(row, text="miroir ↕", variable=self.flip_y, command=self.redraw).pack(side="left")
            if p.mm_per_unit or p.diff_min:
                row2 = ttk.Frame(parent)
                row2.pack(pady=(4, 0))
                if p.diff_min:
                    ttk.Button(row2, text="Mémoriser la référence (R)", command=self.set_reference).pack(side="left")
                    ttk.Checkbutton(row2, text="Écarts (E)", variable=self.show_diff,
                                    command=self.redraw).pack(side="left", padx=8)
                if p.mm_per_unit:
                    ttk.Checkbutton(row2, text="Distances (D)", variable=self.show_dist,
                                    command=self.redraw).pack(side="left")

        def set_reference(self):
            """Mémorise la scène actuelle (trame moyenne du dernier signal) ; la vue Écarts montre ce qui change."""
            if self.last is None or not self.p.diff_min:
                return False
            self.reference = self.p.mean_frame(self.last)
            self.show_diff.set(True)
            self.redraw()
            return True

        def toggle(self, var):
            var.set(not var.get())
            self.redraw()

        def cm(self, units):
            return units * self.p.mm_per_unit / 10

        def info_text(self):
            """Ligne d'information sous la grille (None = celle de l'application)."""
            if self.last is None:
                return None
            p = self.p
            if self.show_diff.get() and self.reference is not None:
                changed = sum(1 for d in p.diff_zones(p.mean_frame(self.last), self.reference) if d)
                seuil = f"{p.diff_min * p.mm_per_unit:.0f} mm" if p.mm_per_unit else f"{p.diff_min:g}"
                return (f"Écarts / référence : {changed} zone(s) changée(s)  "
                        f"(rouge + apparu, bleu − disparu, seuil {seuil})")
            if p.mm_per_unit:
                vals = [v for v in self.last[-p.grid * p.grid:] if not p.is_background(v)]
                if not vals:
                    return "Aucun objet vu"
                near = min(vals) if p.near == "petit" else max(vals)
                return (f"Objet le plus proche : {self.cm(near):.0f} cm"
                        + ("   ·   R = mémoriser la référence" if p.diff_min and self.reference is None else ""))
            return None
        def redraw(self):
            if self.last is not None:
                self.update(self.last)

        def update(self, values):
            self.last = values
            p = self.p
            order = p.display_order(self.flip_x.get(), self.flip_y.get())
            frames = p.grid_frames(values)
            diff = None
            if self.show_diff.get() and self.reference is not None:
                cur = p.mean_frame(values)
                diff = p.diff_zones(cur, self.reference)
            dist = self.show_dist.get() and p.mm_per_unit
            for i, k in enumerate(order):
                v = frames[-1][k]
                label = ""
                if diff is not None:
                    color = DIFF_PLUS if diff[k] > 0 else DIFF_MINUS if diff[k] < 0 else DIFF_SAME
                    if diff[k]:
                        label = "+" if diff[k] > 0 else "−"
                        if dist:
                            label += f"{abs(self.cm(cur[k] - self.reference[k])):.0f}"
                    fg = "white"
                else:
                    color = zone_color(p, v)
                    if dist and not p.is_background(v):
                        label = f"{self.cm(v):.0f}"
                    t = p.closeness(v)
                    fg = "black" if t is not None and t > 0.65 else "white"
                self.canvas.itemconfigure(self.cells[i], fill=color)
                self.canvas.itemconfigure(self.labels[i], text=label, fill=fg)
            for f, row in enumerate(self.thumb_cells):
                if f < len(frames):
                    for i, k in enumerate(order):
                        self.thumbs.itemconfigure(row[i], fill=zone_color(p, frames[f][k]))

    class PlotView:
        """Courbes (une par axe) ou signal brut."""

        W, H = 380, 260

        def __init__(self, parent, p):
            self.p = p
            self.canvas = tk.Canvas(parent, width=self.W, height=self.H, bg=BG_CELL, highlightthickness=0)
            self.canvas.pack()
            names = p.axes if p.display == "courbes" else ["signal"]
            leg = ttk.Frame(parent)
            leg.pack(pady=(4, 0))
            for k, name in enumerate(names):
                tk.Label(leg, text="━ " + name, fg=AXIS_COLORS[k % len(AXIS_COLORS)], bg=BG,
                         font=("Segoe UI", 9)).pack(side="left", padx=5)

        def update(self, values):
            c = self.canvas
            c.delete("curve")
            curves = self.p.curves(values) if self.p.display == "courbes" else [values]
            for k, cur in enumerate(curves):
                if len(cur) < 2:
                    continue
                pts = []
                for i, v in enumerate(cur):
                    pts += [6 + i * (self.W - 12) / (len(cur) - 1),
                            self.H - 6 - max(0.0, min(1.0, v / self.p.value_max)) * (self.H - 12)]
                c.create_line(*pts, fill=AXIS_COLORS[k % len(AXIS_COLORS)], width=2, tags="curve")

    class App(tk.Tk):
        def __init__(self):
            super().__init__()
            self.title(f"{profile.name} · collecte NanoEdge")
            self.configure(bg=BG)
            self.minsize(900, 600)
            self.q = queue.Queue()
            self.source = None
            self.out_dir = tk.StringVar(value=out_dir)
            self.files = ClassFiles(out_dir, profile.classes)
            self.cls = tk.StringVar(value=profile.classes[0])
            self.target = tk.IntVar(value=profile.per_pass)
            self.state = "idle"            # idle | countdown | recording
            self.last_signal_t = 0.0
            self.rate_n, self.rate_t, self.rate = 0, time.monotonic(), 0.0
            self.bad_lines = 0
            self._last_wait = False
            self._style()
            self._build()
            self._bind_keys()
            self.refresh_ports()
            self.refresh_counts()
            self._poll_id = self.after(30, self.poll)
            self.add_log(f"Les fichiers CSV seront enregistrés dans : {self.out_dir.get()}", "info")
            for w in profile.warnings:
                self.add_log(f"Profil : {w}", "warn")
            if profile.import_hint:
                self.add_log(f"Dans NanoEdge AI Studio : {profile.import_hint}", "info")
            if demo:
                self.title(f"{profile.name} · collecte NanoEdge · DÉMO")
                self.add_log("DÉMO : signaux simulés, sans rapport avec la classe choisie. Ils sont enregistrés "
                             "à part (dossier demo/), jamais avec vos vraies données.", "warn")
            self.protocol("WM_DELETE_WINDOW", self.on_close)
            if demo:
                self.connect()

        def _style(self):
            s = ttk.Style(self)
            s.theme_use("clam")
            s.configure(".", background=BG, foreground=FG, fieldbackground="#3a3f47")
            s.configure("TLabelframe", background=BG, foreground=MUTED, bordercolor="#444a53")
            s.configure("TLabelframe.Label", background=BG, foreground=MUTED)
            s.configure("TButton", background="#3a3f47", foreground=FG, bordercolor="#444a53", padding=6)
            s.map("TButton", background=[("active", "#4a505a"), ("disabled", "#30343b")],
                  foreground=[("disabled", "#6b7078")])
            s.configure("Accent.TButton", background="#1f6f8b")
            s.map("Accent.TButton", background=[("active", "#2a87a8")])
            s.configure("Rec.TButton", background="#8e2b2b")
            s.map("Rec.TButton", background=[("active", "#a83636")])
            s.configure("TRadiobutton", background=BG, foreground=FG, font=("Segoe UI", 11),
                        indicatorbackground="#3a3f47", indicatorforeground=ACCENT)
            s.map("TRadiobutton", background=[("active", BG)],
                  indicatorbackground=[("selected", ACCENT), ("!selected", "#3a3f47")],
                  foreground=[("selected", ACCENT)])
            s.map("TCombobox", fieldbackground=[("readonly", "#3a3f47")], foreground=[("readonly", FG)],
                  selectbackground=[("readonly", "#3a3f47")], selectforeground=[("readonly", FG)])
            self.option_add("*TCombobox*Listbox.background", "#3a3f47")
            self.option_add("*TCombobox*Listbox.foreground", FG)
            s.configure("Muted.TLabel", foreground=MUTED)
            s.configure("Big.TLabel", font=("Segoe UI", 13, "bold"))
            s.configure("Horizontal.TProgressbar", background=ACCENT, troughcolor="#3a3f47", bordercolor="#444a53")

        def _build(self):
            top = ttk.Frame(self, padding=(12, 10))
            top.pack(fill="x")
            ttk.Label(top, text="Port").pack(side="left")
            self.port_cb = ttk.Combobox(top, width=28, state="readonly")
            self.port_cb.pack(side="left", padx=(6, 4))
            ttk.Button(top, text="↻", width=3, command=self.refresh_ports).pack(side="left")
            ttk.Label(top, text=f"{profile.baud} bauds", style="Muted.TLabel").pack(side="left", padx=(14, 0))
            self.conn_btn = ttk.Button(top, text="Connecter", style="Accent.TButton", command=self.toggle_connect)
            self.conn_btn.pack(side="left", padx=10)
            self.conn_lbl = ttk.Label(top, text="● déconnecté", style="Muted.TLabel")
            self.conn_lbl.pack(side="left")

            body = ttk.Frame(self, padding=(12, 0, 12, 8))
            body.pack(fill="both", expand=True)
            left = ttk.Frame(body)
            left.pack(side="left", fill="y")
            self.view = GridView(left, profile) if profile.display == "grille" else PlotView(left, profile)
            self.overlay = tk.Label(left, text="", font=("Segoe UI", 40, "bold"), bg=BG, fg=WARN)
            self.overlay.pack()
            self.info = ttk.Label(left, text="—", style="Muted.TLabel", wraplength=380, justify="center")
            self.info.pack(pady=(2, 2))

            right = ttk.Frame(body, padding=(16, 0, 0, 0))
            right.pack(side="left", fill="both", expand=True)
            rec = ttk.LabelFrame(right, text=" Enregistrement ", padding=10)
            rec.pack(fill="x")
            self.count_lbls = {}
            for i, c in enumerate(profile.classes):
                key = f"{i + 1}  " if i < 9 else "   "
                ttk.Radiobutton(rec, text=f"{key}{c}", value=c, variable=self.cls).grid(
                    row=i, column=0, sticky="w", pady=2)
                lbl = ttk.Label(rec, text="0 signaux", style="Muted.TLabel")
                lbl.grid(row=i, column=1, sticky="e", padx=(20, 0))
                self.count_lbls[c] = lbl
            rec.columnconfigure(1, weight=1)
            r = len(profile.classes)

            row = ttk.Frame(rec)
            row.grid(row=r, column=0, columnspan=2, sticky="we", pady=(10, 4))
            ttk.Label(row, text="Signaux par passe").pack(side="left")
            ttk.Spinbox(row, from_=1, to=100000, increment=50, width=7, textvariable=self.target).pack(side="left", padx=6)
            drow = ttk.Frame(rec)
            drow.grid(row=r + 1, column=0, columnspan=2, sticky="we", pady=4)
            ttk.Label(drow, text="Dossier").pack(side="left")
            ttk.Entry(drow, textvariable=self.out_dir).pack(side="left", fill="x", expand=True, padx=6)
            ttk.Button(drow, text="…", width=3, command=self.choose_dir).pack(side="left")
            brow = ttk.Frame(rec)
            brow.grid(row=r + 2, column=0, columnspan=2, sticky="we", pady=(8, 4))
            self.rec_btn = ttk.Button(brow, text="●  Enregistrer  (Espace)", style="Rec.TButton", command=self.toggle_record)
            self.rec_btn.pack(side="left", fill="x", expand=True)
            self.del_btn = ttk.Button(brow, text="Suppr. dernier  (⌫)", command=self.remove_last, state="disabled")
            self.del_btn.pack(side="left", padx=(8, 0))
            self.undo_btn = ttk.Button(brow, text="Annuler la dernière passe", command=self.undo_last, state="disabled")
            self.undo_btn.pack(side="left", padx=(8, 0))
            self.progress = ttk.Progressbar(rec, maximum=100)
            self.progress.grid(row=r + 3, column=0, columnspan=2, sticky="we", pady=(6, 2))
            self.rec_lbl = ttk.Label(rec, text="Prêt.", style="Big.TLabel")
            self.rec_lbl.grid(row=r + 4, column=0, columnspan=2, sticky="w")
            self.advice_lbl = ttk.Label(rec, text="", style="Muted.TLabel", justify="left", wraplength=420)
            self.advice_lbl.grid(row=r + 5, column=0, columnspan=2, sticky="w", pady=(8, 0))
            self.cls.trace_add("write", lambda *a: self.show_advice())
            self.show_advice()

            msgs = ttk.LabelFrame(right, text=" Messages de la carte ", padding=8)
            msgs.pack(fill="both", expand=True, pady=(12, 0))
            self.log = tk.Text(msgs, height=8, bg=BG_CELL, fg=FG, insertbackground=FG, relief="flat",
                               font=("Consolas", 10), wrap="word")
            self.log.pack(fill="both", expand=True)
            self.log.tag_configure("err", foreground=REC)
            self.log.tag_configure("info", foreground=ACCENT)
            self.log.tag_configure("warn", foreground=WARN)
            self.log.configure(state="disabled")
            self.status = ttk.Label(self, text="", style="Muted.TLabel", padding=(12, 4))
            self.status.pack(fill="x", side="bottom")

        def show_advice(self):
            a = profile.advice_for(self.cls.get())
            self.advice_lbl.configure(text=("Astuce : " + a) if a else "")

        def _typing(self):
            w = self.focus_get()
            return w is not None and w.winfo_class() in ("TEntry", "TSpinbox", "TCombobox", "Entry", "Spinbox", "Text")

        def _bind_keys(self):
            for cls in ("TButton", "TCheckbutton", "TRadiobutton"):   # Espace ne « clique » pas un bouton
                self.bind_class(cls, "<Key-space>", lambda e: None)
            self.bind("<space>", lambda e: None if self._typing() else self.toggle_record())
            self.bind("<BackSpace>", lambda e: None if self._typing() else self.remove_last())
            for i, c in enumerate(profile.classes[:9]):
                self.bind(str(i + 1), lambda e, c=c: self.state == "idle" and not self._typing() and self.cls.set(c))
            if profile.display == "grille" and profile.diff_min:
                self.bind("r", lambda e: None if self._typing() else self.set_reference())
                self.bind("e", lambda e: None if self._typing() else self.view.toggle(self.view.show_diff))
            if profile.display == "grille" and profile.mm_per_unit:
                self.bind("d", lambda e: None if self._typing() else self.view.toggle(self.view.show_dist))

        def set_reference(self):
            if self.view.set_reference():
                self.add_log("Scène de référence mémorisée : la vue Écarts montre ce qui change.", "info")

        # ---- connexion ----
        def refresh_ports(self):
            ports = list_ports()
            self.port_cb["values"] = ports
            pick = next((p for p in ports if port and p.split(" — ")[0] == port), None)
            if pick:
                self.port_cb.set(pick)
            elif ports and not self.port_cb.get():
                self.port_cb.set(ports[0])

        def toggle_connect(self):
            self.disconnect() if self.source else self.connect()

        def connect(self):
            if demo:
                self.source = DemoSource(profile, self.q)
            else:
                if serial is None:
                    messagebox.showerror("pyserial manquant", "Installe-le avec :  pip install pyserial")
                    return
                sel = self.port_cb.get()
                if not sel:
                    messagebox.showwarning("Port", "Aucun port série trouvé. Branche la carte puis clique sur ↻.")
                    return
                self.source = SerialSource(sel.split(" — ")[0], profile.baud, self.q)
            self.source.start()
            self.conn_btn.configure(text="Déconnecter")

        def disconnect(self):
            if self.state != "idle":
                self.stop_record()
            if self.source:
                self.source.stop()
                if self.source.is_alive():
                    self.source.join(timeout=1.5)          # libère le port avant toute reconnexion
                self.source = None
            self.conn_btn.configure(text="Connecter")
            self.conn_lbl.configure(text="● déconnecté", foreground=MUTED)

        # ---- réception ----
        def poll(self):
            try:
                for _ in range(200):
                    kind, payload = self.q.get_nowait()
                    if kind == "line":
                        parsed = parse_line(payload, expected_len)
                        if not parsed:
                            continue
                        if parsed[0] == "signal":
                            self.on_signal(parsed[1])
                        elif parsed[0] == "bad":
                            self.bad_lines += 1
                            if self.bad_lines in (1, 10, 100):
                                self.add_log(f"Ligne ignorée : {parsed[1]} (profil ou mode de la carte ?)", "warn")
                        else:
                            self.add_log(parsed[1], "err" if "ERREUR" in parsed[1] else None)
                    elif kind == "connected":
                        self.conn_lbl.configure(text=f"● {payload}", foreground=OKC)
                        self.add_log(f"Connecté : {payload}", "info")
                    elif kind == "error":
                        self.add_log(payload, "err")
                        self.disconnect()
            except queue.Empty:
                pass
            now = time.monotonic()
            if now - self.rate_t >= 1.0:
                self.rate = self.rate_n / (now - self.rate_t)
                self.rate_n, self.rate_t = 0, now
            if self.source and now - self.last_signal_t > NO_SIGNAL_S:
                self.info.configure(text="Aucun signal (rien devant le capteur ?)")
                if self.state == "recording":
                    self.rec_lbl.configure(text=f"{self.cls.get().upper()} : {self.files.current[3]}/{self.get_target()}"
                                                "  — aucun signal ?")
            self.status.configure(text=f"{self.rate:4.1f} signaux/s   ·   dossier : {self.out_dir.get()}")
            self._poll_id = self.after(30, self.poll)

        def on_signal(self, values):
            self.last_signal_t = time.monotonic()
            self.rate_n += 1
            self.view.update(values)
            n = profile.active_count(values)
            text = self.view.info_text() if profile.display == "grille" else None
            self.info.configure(text=text or f"Signal reçu : {len(values)} valeurs"
                                + (f", {n} zones actives" if n is not None else ""))
            if self.state == "recording":
                got = self.files.add(values)
                self.del_btn.configure(state="normal")
                target = self.get_target()
                self.progress["value"] = 100 * got / target
                left = f"   (reste ~{int((target - got) / self.rate)} s)" if self.rate > 0.1 else ""
                self.rec_lbl.configure(text=f"{self.cls.get().upper()} : {got}/{target}{left}")
                if got >= target:
                    self.stop_record(done=True)

        def get_target(self):
            try:
                return max(1, int(self.target.get()))
            except (tk.TclError, ValueError):
                return profile.per_pass

        def add_log(self, text, tag=None):
            self.log.configure(state="normal")
            waiting = bool(profile.waiting_prefix) and text.startswith(profile.waiting_prefix)
            if waiting and self._last_wait:              # message répété : on remplace la ligne
                self.log.delete("end-2l linestart", "end-1c")
            self.log.insert("end-1c", text + "\n", tag or ())
            self._last_wait = waiting
            if int(self.log.index("end-1c").split(".")[0]) > 500:
                self.log.delete("1.0", "100.0")
            self.log.see("end")
            self.log.configure(state="disabled")

        # ---- enregistrement ----
        def refresh_counts(self):
            self.files.out_dir = self.out_dir.get()
            for c, n in self.files.counts().items():
                self.count_lbls[c].configure(text=f"{n} signaux")

        def choose_dir(self):
            d = filedialog.askdirectory(initialdir=self.out_dir.get())
            if d and self.state == "idle":
                self.out_dir.set(d)
                self.files.undo_info = None
                self.files.starts = []
                self.undo_btn.configure(state="disabled")
                self.del_btn.configure(state="disabled")
                self.refresh_counts()

        def toggle_record(self):
            self.start_countdown() if self.state == "idle" else self.stop_record()

        def start_countdown(self):
            if not self.source:
                messagebox.showinfo("Pas connecté", "Connecte d'abord la carte (ou lance avec --demo).")
                return
            self.state = "countdown"
            self.rec_btn.configure(text="■  Arrêter  (Espace)")
            self.rec_lbl.configure(text=f"Prends la pose « {self.cls.get()} »…", foreground=FG)
            self._countdown(3)

        def _countdown(self, n):
            if self.state != "countdown":
                return
            if n > 0:
                self.overlay.configure(text=str(n))
                self.after(1000, self._countdown, n - 1)
            else:
                self.overlay.configure(text="")
                self.begin_record()

        def begin_record(self):
            self.files.out_dir = self.out_dir.get()
            self.files.start_pass(self.cls.get())
            self.progress["value"] = 0
            self.del_btn.configure(state="disabled")
            self.state = "recording"
            self.rec_lbl.configure(text=f"{self.cls.get().upper()} : 0/{self.get_target()}", foreground=REC)

        def stop_record(self, done=False):
            res = self.files.end_pass() if self.state == "recording" else None
            if res:
                cls, n = res
                self.undo_btn.configure(state="normal" if n else "disabled")
                msg = f"{'Terminé' if done else 'Arrêté'} : {n} signaux ajoutés à {cls}.csv"
                self.rec_lbl.configure(text=msg, foreground=FG)
                self.add_log(msg, "info")
                if done:
                    self.bell()
            else:
                self.rec_lbl.configure(text="Annulé.", foreground=FG)
            self.overlay.configure(text="")
            self.state = "idle"
            self.rec_btn.configure(text="●  Enregistrer  (Espace)")
            self.refresh_counts()

        def undo_last(self):
            if self.state != "idle" or not self.files.undo_info:
                return
            path, _, n = self.files.undo_info
            if messagebox.askyesno("Annuler", f"Supprimer les {n} derniers signaux de {os.path.basename(path)} ?"):
                res = self.files.undo()
                if res:
                    self.add_log(f"Passe annulée : {res[1]} signaux retirés de {res[0]}", "info")
                self.undo_btn.configure(state="disabled")
                self.del_btn.configure(state="disabled")
                self.refresh_counts()

        def remove_last(self):
            """Retire le dernier signal enregistré, pendant la passe (elle continue) ou juste après."""
            if self.state == "countdown":
                return
            res = self.files.remove_last()
            if not res:
                return
            cls, left = res
            self.add_log(f"Dernier signal retiré de {cls}.csv", "info")
            if self.state == "recording":
                self.progress["value"] = 100 * left / self.get_target()
                self.rec_lbl.configure(text=f"{cls.upper()} : {left}/{self.get_target()}  (dernier retiré)")
            else:
                self.rec_lbl.configure(text=f"Dernier signal retiré : {left} restant(s) de cette passe dans {cls}.csv",
                                       foreground=FG)
                self.undo_btn.configure(state="normal" if left else "disabled")
                self.refresh_counts()
            if not left:
                self.del_btn.configure(state="disabled")

        def on_close(self):
            self.disconnect()
            self.after_cancel(self._poll_id)              # sinon Tk appelle poll après la fermeture
            self.destroy()

    expected_len = profile.length
    return App()


# =========================================================================== #
def build_parser(description=None):
    ap = argparse.ArgumentParser(description=description or "Collecte de données pour NanoEdge AI Studio",
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--profil", help="profil JSON du projet (voir outils/README.md)")
    ap.add_argument("--demo", action="store_true", help="données simulées, sans carte")
    ap.add_argument("--port", help="port série (COM5, /dev/ttyACM0...)")
    ap.add_argument("--out", help="dossier des CSV (défaut : celui du profil, ou son sous-dossier demo/ avec --demo ; "
                                  "un chemin relatif part du dossier du profil)")
    ap.add_argument("--cli", action="store_true", help="collecte guidée dans le terminal, sans fenêtre")
    ap.add_argument("--view", action="store_true", help="visualiser dans le terminal, sans enregistrer")
    ap.add_argument("--classes", nargs="+", help="classes à enregistrer (défaut : toutes celles du profil)")
    ap.add_argument("--count", type=int, help="signaux par classe et par passe (défaut : celui du profil)")
    ap.add_argument("--len", type=int, help="valeurs attendues par signal (défaut : celui du profil ; 0 = ne pas vérifier)")
    ap.add_argument("--baud", type=int, help="débit de la liaison série (défaut : celui du profil)")
    return ap


def main(argv=None, profile_path=None, description=None):
    ap = build_parser(description)
    args = ap.parse_args(argv)
    path = args.profil or profile_path
    if not path:
        ap.error("--profil est obligatoire")
    try:
        profile = Profile.load(path)
    except ProfileError as e:
        print(f"Profil {path} : {e}")
        return 2
    for w in profile.warnings:
        print(f"Profil {path} : ATTENTION, {w}")
    if args.len is not None:
        profile.length = args.len
    if args.baud:
        profile.baud = args.baud
    out_dir = resolve_out(profile, args.out, args.demo)
    if args.demo:
        print(f"DÉMO : signaux simulés, enregistrés dans {out_dir} (pas avec vos vraies données)")

    if not (args.cli or args.view):
        run_gui(profile, out_dir, demo=args.demo, port=args.port).mainloop()
        return 0

    if not args.demo and not args.port:
        ap.error("--port est obligatoire en ligne de commande (ou --demo)")
    q = queue.Queue()
    src = DemoSource(profile, q) if args.demo else SerialSource(args.port, profile.baud, q)
    src.start()
    try:
        if args.view:
            cli_view(profile, q, profile.length)
            return 0
        classes = args.classes or profile.classes
        unknown = [c for c in classes if c not in profile.classes]
        if unknown:
            print(f"Classes inconnues du profil : {unknown} (profil : {profile.classes})")
            return 2
        files = ClassFiles(out_dir, profile.classes)
        cli_record(profile, q, files, classes, args.count or profile.per_pass, profile.length)
        print(f"\nFichiers dans : {out_dir}")
        print("Terminé. Importe les fichiers CSV dans NanoEdge AI Studio : "
              + (profile.import_hint or "une classe par fichier."))
        print("Conseil : refais 2-3 passes (autre personne, autre éclairage) pour plus de robustesse.")
    except KeyboardInterrupt:
        print("\nInterrompu.")
    except RuntimeError as e:
        print(f"\nERREUR : {e}")
        return 1
    finally:
        src.stop()
    return 0


if __name__ == "__main__":
    sys.exit(main())
