#!/usr/bin/env python3
"""
collecte_coeur.py - Cœur commun de la collecte de données pour NanoEdge AI Studio (sans interface).

Toutes les cartes du dépôt envoient, en mode collecte, le même genre de texte sur la liaison série :
    # message pour l'humain
    v0,v1,...,vN-1          un signal = une ligne CSV d'entiers (une ligne = un exemple NanoEdge)
NanoEdge AI Studio (classification N classes) attend UN FICHIER PAR CLASSE, une ligne par signal :
c'est ce que produit ce module, quel que soit le projet. Ce qui change d'un projet à l'autre (noms
des classes, longueur du signal, façon de l'afficher) est décrit dans un PROFIL (fichier JSON), voir
outils/README.md.

Contenu : Profile (chargement et vérification du profil), parse_line, ClassFiles (fichiers CSV par
classe : comptage, ajout, annulation de la dernière passe), SerialSource et DemoSource (fils
d'exécution qui envoient ("connected" | "line" | "error" | "disconnected", texte) dans une file).
"""
import json
import math
import os
import random
import threading
import time

try:
    import serial
except ImportError:          # la démo et les tests fonctionnent sans pyserial
    serial = None

DISPLAY_TYPES = ("grille", "courbes", "brut")
PROFILE_KEYS = {"nom", "classes", "longueur", "affichage", "valeur_max", "par_passe", "bauds", "dossier",
                "conseil", "attente", "demo_signaux_par_s", "import"}
DISPLAY_KEYS = {"grille": {"type", "taille", "trames", "min", "miroir_gauche_droite", "miroir_haut_bas",
                           "proche", "fond", "mm_par_unite", "ecart_min"},
                "courbes": {"type", "axes", "instants", "ordre"},
                "brut": {"type"}}


class ProfileError(ValueError):
    pass


class Profile:
    """Profil de collecte d'un projet (fichier JSON). Champs :
        nom          nom du projet (titre de la fenêtre)
        classes      liste des classes = noms des fichiers CSV (pierre -> pierre.csv)
        longueur     nombre de valeurs par signal (0 = ne pas vérifier)
        affichage    {"type": "grille", "taille": 8, "trames": 8, "min": 20}  (+ options, voir README)
                     {"type": "courbes", "axes": ["x", "y", ...], "instants": 32, "ordre": "entrelace"|"par_axe"}
                     {"type": "brut"}
        valeur_max   valeur maximale d'une mesure (100 par défaut) : échelle des couleurs et des courbes
        par_passe    signaux enregistrés par passe (300 par défaut)
        bauds        débit de la liaison série (115200 par défaut)
        dossier      dossier des CSV, relatif au fichier de profil ("." par défaut)
        conseil      texte affiché pendant la collecte (facultatif) ; ou {"classe": "texte", ...}
        import       comment importer les CSV dans NanoEdge AI Studio (facultatif, affiché à la fin)
        attente      début des messages de la carte qui se répètent sans rien apprendre ("En attente") ;
                     "" = la carte envoie des signaux en continu (la démo n'a pas de phase d'attente)
        demo_signaux_par_s   rythme des signaux simulés (--demo), 5 par défaut
    """

    def __init__(self, data, base_dir="."):
        if not isinstance(data, dict):
            raise ProfileError("le profil doit être un objet JSON")
        self.raw = data
        self.base_dir = os.path.abspath(base_dir)
        # Clés inconnues (faute de frappe, ou profil plus récent que l'outil) : signalées, pas ignorées en silence
        self.warnings = [f"clé inconnue « {k} » ignorée (clés possibles : {', '.join(sorted(PROFILE_KEYS))})"
                         for k in data if k not in PROFILE_KEYS]
        self.name = str(data.get("nom", "Collecte NanoEdge"))
        classes = data.get("classes")
        if not isinstance(classes, list) or not classes or not all(isinstance(c, str) and c for c in classes):
            raise ProfileError("« classes » : liste non vide de noms (ex. [\"pierre\", \"feuille\", \"ciseaux\"])")
        bad = [c for c in classes if any(ch in c for ch in '/\\:*?"<>|') or c.strip() != c]
        if bad:
            raise ProfileError(f"« classes » : noms inutilisables comme noms de fichiers : {bad}")
        if len(set(classes)) != len(classes):
            raise ProfileError("« classes » : un nom apparaît deux fois")
        self.classes = classes
        self.length = int(data.get("longueur", 0))
        if self.length < 0:
            raise ProfileError("« longueur » doit être positive (0 = ne pas vérifier)")
        self.value_max = float(data.get("valeur_max", 100))
        self.per_pass = int(data.get("par_passe", 300))
        self.baud = int(data.get("bauds", 115200))
        self.out_dir = os.path.normpath(os.path.join(self.base_dir, str(data.get("dossier", "."))))
        advice = data.get("conseil", "")
        if isinstance(advice, dict):
            self.advice_by_class = {str(k): str(v) for k, v in advice.items()}
            self.advice = ""
            self.warnings += [f"« conseil » : classe inconnue « {k} »" for k in advice if k not in classes]
        else:
            self.advice_by_class = {}
            self.advice = str(advice)
        self.import_hint = str(data.get("import", ""))
        self.waiting_prefix = str(data.get("attente", "En attente"))
        self.demo_rate = float(data.get("demo_signaux_par_s", 5))
        disp = data.get("affichage", {"type": "brut"})
        if not isinstance(disp, dict) or disp.get("type") not in DISPLAY_TYPES:
            raise ProfileError(f"« affichage.type » : un de {DISPLAY_TYPES}")
        self.display = disp["type"]
        self.warnings += [f"« affichage » : clé inconnue « {k} » ignorée pour le type {self.display} "
                          f"(clés possibles : {', '.join(sorted(DISPLAY_KEYS[self.display]))})"
                          for k in disp if k not in DISPLAY_KEYS[self.display]]
        if self.display == "grille":
            self.grid = int(disp.get("taille", 8))
            self.frames = int(disp.get("trames", 1))
            self.grid_min = float(disp.get("min", 0))
            # Miroirs d'AFFICHAGE seulement (les signaux enregistrés ne changent pas) : le capteur voit la
            # main par en dessous, et l'image brute paraît inversée à qui regarde la carte par au-dessus.
            # gauche_droite = symétrie par rapport à l'axe vertical (colonne x -> taille-1-x) ;
            # haut_bas      = symétrie par rapport à l'axe horizontal (ligne y -> taille-1-y).
            self.flip_x = bool(disp.get("miroir_gauche_droite", False))
            self.flip_y = bool(disp.get("miroir_haut_bas", False))
            # Sens des valeurs : « grand » = une grande valeur est proche (PFC : 100 = sommet de la main) ;
            # « petit » = une petite valeur est proche (Sentinelle : valeur = distance). Le proche est clair.
            self.near = disp.get("proche", "grand")
            if self.near not in ("grand", "petit"):
                raise ProfileError("grille : « proche » = \"grand\" (grande valeur = proche) ou \"petit\" (valeur = distance)")
            # Valeur « rien » (case vide) : 0 par défaut si proche = grand, valeur_max si proche = petit.
            self.background = float(disp.get("fond", 0 if self.near == "grand" else self.value_max))
            mm = disp.get("mm_par_unite")
            self.mm_per_unit = float(mm) if mm else None          # distances en cm affichables
            em = disp.get("ecart_min")
            self.diff_min = float(em) if em else None             # scène de référence + vue des écarts
            if self.length and self.length != self.grid * self.grid * self.frames:
                raise ProfileError(f"grille : {self.frames} trames de {self.grid}x{self.grid} = "
                                   f"{self.grid * self.grid * self.frames} valeurs, mais « longueur » = {self.length}")
        elif self.display == "courbes":
            axes = disp.get("axes")
            if not isinstance(axes, list) or not axes:
                raise ProfileError("courbes : « axes » = liste des noms des axes")
            self.axes = [str(a) for a in axes]
            self.samples = int(disp.get("instants", 0))
            self.order = disp.get("ordre", "entrelace")
            if self.order not in ("entrelace", "par_axe"):
                raise ProfileError("courbes : « ordre » = \"entrelace\" (x0,y0,x1,y1...) ou \"par_axe\" (x0,x1...,y0,y1...)")
            if self.length and self.length != self.samples * len(self.axes):
                raise ProfileError(f"courbes : {self.samples} instants x {len(self.axes)} axes = "
                                   f"{self.samples * len(self.axes)} valeurs, mais « longueur » = {self.length}")

    @classmethod
    def load(cls, path):
        try:
            with open(path, encoding="utf-8") as f:
                data = json.load(f)
        except OSError as e:
            raise ProfileError(f"profil illisible : {e}") from e
        except json.JSONDecodeError as e:
            raise ProfileError(f"{path} : JSON invalide (ligne {e.lineno}) : {e.msg}") from e
        return cls(data, os.path.dirname(os.path.abspath(path)))

    # ---- découpage d'un signal pour l'affichage ----
    def display_order(self, flip_x=None, flip_y=None):
        """Indices des zones dans l'ordre d'affichage (ligne par ligne), miroirs appliqués."""
        fx = self.flip_x if flip_x is None else flip_x
        fy = self.flip_y if flip_y is None else flip_y
        g = self.grid
        return [(g - 1 - y if fy else y) * g + (g - 1 - x if fx else x) for y in range(g) for x in range(g)]

    def grid_frames(self, values):
        """Liste des trames (listes de taille*taille valeurs), de la plus ancienne à la plus récente."""
        n = self.grid * self.grid
        return [values[i:i + n] for i in range(0, len(values) - n + 1, n)]

    def curves(self, values):
        """Liste des courbes, une par axe (listes de valeurs dans l'ordre du temps)."""
        k = len(self.axes)
        if self.order == "entrelace":
            return [values[a::k] for a in range(k)]
        t = len(values) // k
        return [values[a * t:(a + 1) * t] for a in range(k)]

    def is_background(self, v):
        """Grille : la zone ne voit rien (case vide) ?"""
        return v <= self.background if self.near == "grand" else v >= self.background

    def closeness(self, v):
        """Grille : 0 (loin) .. 1 (proche), pour les couleurs ; None pour une case vide."""
        if self.is_background(v):
            return None
        span = max(1e-9, self.value_max - self.grid_min)
        t = (v - self.grid_min) / span
        return max(0.0, min(1.0, t if self.near == "grand" else 1.0 - t))

    def active_count(self, values):
        """Grille : nombre de zones actives dans la trame la plus récente (pas vides, et au-dessus de « min »
        si proche = grand)."""
        if self.display != "grille":
            return None
        last = values[-self.grid * self.grid:]
        if self.near == "petit":
            return sum(1 for v in last if not self.is_background(v))
        return sum(1 for v in last if not self.is_background(v) and v >= self.grid_min)

    def mean_frame(self, values):
        """Grille : moyenne des trames du signal, zone par zone (moins de bruit qu'une seule trame)."""
        frames = self.grid_frames(values)
        return [sum(f[i] for f in frames) / len(frames) for i in range(self.grid * self.grid)]

    def diff_zones(self, cur, ref):
        """Grille : écart de chaque zone par rapport à une scène de référence (deux trames moyennes).
        +1 = plus proche qu'avant (quelque chose est apparu), -1 = plus loin (disparu), 0 = sous ecart_min."""
        out = []
        for v, r in zip(cur, ref):
            d = (v - r) if self.near == "grand" else (r - v)      # > 0 : plus proche
            out.append(1 if d > self.diff_min else -1 if d < -self.diff_min else 0)
        return out

    def advice_for(self, cls):
        return self.advice_by_class.get(cls, self.advice)


def parse_line(line, expected_len=0):
    """('msg', texte) pour une ligne « # ... », ('signal', [nombres]) pour une ligne CSV de la bonne
    longueur, ('bad', raison) pour une ligne CSV de mauvaise longueur, None sinon."""
    line = line.strip()
    if not line:
        return None
    if line.startswith("#"):
        return ("msg", line.lstrip("#").strip())
    parts = line.split(",")
    try:
        values = [int(v) for v in parts]
    except ValueError:
        try:
            values = [float(v) for v in parts]
        except ValueError:
            return None
    if expected_len and len(values) != expected_len:
        return ("bad", f"{len(values)} valeurs au lieu de {expected_len}")
    return ("signal", values)


class ClassFiles:
    """Un fichier CSV par classe dans un dossier : comptage, ajout par passe, annulation de la dernière passe.
    L'annulation tronque le fichier à sa taille d'avant la passe : rien d'autre n'est touché."""

    def __init__(self, out_dir, classes):
        self.out_dir = out_dir
        self.classes = list(classes)
        self.current = None          # (classe, fichier ouvert, taille avant la passe, nb ajoutés)
        self.undo_info = None        # (chemin, taille avant la passe, nb)

    def path(self, cls):
        return os.path.join(self.out_dir, f"{cls}.csv")

    def count(self, cls):
        p = self.path(cls)
        if not os.path.exists(p):
            return 0
        with open(p, "rb") as f:
            return sum(1 for line in f if line.strip())

    def counts(self):
        return {c: self.count(c) for c in self.classes}

    def start_pass(self, cls):
        if cls not in self.classes:
            raise ValueError(f"classe inconnue : {cls}")
        self.end_pass()
        os.makedirs(self.out_dir, exist_ok=True)
        p = self.path(cls)
        size = os.path.getsize(p) if os.path.exists(p) else 0
        if size:                                     # fichier sans fin de ligne finale : on la rajoute
            with open(p, "rb") as f:
                f.seek(-1, os.SEEK_END)
                if f.read(1) not in (b"\n", b"\r"):
                    with open(p, "ab") as g:
                        g.write(b"\n")
                    size += 1
        self.current = [cls, open(p, "a", newline="\n", encoding="ascii"), size, 0]

    def add(self, values):
        if not self.current:
            raise RuntimeError("aucune passe en cours")
        self.current[1].write(",".join(str(v) for v in values) + "\n")
        self.current[3] += 1
        return self.current[3]

    def end_pass(self):
        """Termine la passe en cours ; retourne (classe, nb ajoutés) ou None."""
        if not self.current:
            return None
        cls, f, size, n = self.current
        f.close()
        self.current = None
        self.undo_info = (self.path(cls), size, n) if n else None
        return cls, n

    def undo(self):
        """Retire la dernière passe terminée ; retourne (nom du fichier, nb retirés) ou None."""
        if not self.undo_info or self.current:
            return None
        p, size, n = self.undo_info
        with open(p, "r+b") as f:
            f.truncate(size)
        self.undo_info = None
        return os.path.basename(p), n


# --------------------------------------------------------------------------- #
# Sources                                                                      #
# --------------------------------------------------------------------------- #
class SerialSource(threading.Thread):
    def __init__(self, port, baud, out_q):
        super().__init__(daemon=True)
        self.port, self.baud, self.q = port, baud, out_q
        self.stop_evt = threading.Event()

    def run(self):
        if serial is None:
            self.q.put(("error", "Module « pyserial » manquant : pip install pyserial"))
            return
        ser = None
        # Windows : « Accès refusé » tant qu'un programme garde le port ouvert ; on réessaie ~2 s.
        for attempt in range(10):
            try:
                ser = serial.Serial(self.port, self.baud, timeout=0.2)
                break
            except serial.SerialException as e:
                text = str(e)
                busy = any(s in text for s in ("PermissionError", "Acc", "denied", "busy"))
                if not busy or attempt == 9 or self.stop_evt.is_set():
                    msg = f"Ouverture de {self.port} impossible : {e}"
                    if busy:
                        msg += ("\n→ Le port est déjà utilisé par un autre programme : ferme Tera Term, une autre "
                                "fenêtre de collecte ou la console série de STM32CubeIDE, puis reconnecte.")
                    self.q.put(("error", msg))
                    return
                time.sleep(0.2)
        self.q.put(("connected", f"{self.port} @ {self.baud} bauds"))
        try:
            while not self.stop_evt.is_set():
                raw = ser.readline()
                if raw:
                    self.q.put(("line", raw.decode("ascii", errors="ignore")))
        except Exception as e:  # noqa: BLE001
            self.q.put(("error", f"Liaison série perdue : {e}"))
        finally:
            ser.close()
            self.q.put(("disconnected", ""))

    def stop(self):
        self.stop_evt.set()


def demo_signal(profile, cls_index, t, rng=random):
    """Signal simulé, de forme différente pour chaque classe, adapté au type d'affichage du profil."""
    vmax = profile.value_max
    if profile.display == "grille":
        g = profile.grid
        frames = []
        for k in range(profile.frames):
            tk = t + k * 0.07
            cx = g / 2 - 0.5 + math.sin(tk * 1.3) * g / 8
            cy = g / 2 - 0.5 + math.cos(tk * 0.9) * g / 10
            rx = 1.0 + 0.9 * cls_index                    # chaque classe : une tache de forme différente
            ry = 1.0 + 0.9 * ((cls_index * 2) % 3)
            img = []
            for y in range(g):
                for x in range(g):
                    d = ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2
                    v = 0.0
                    if d <= 1.0:
                        lo = max(profile.grid_min, 0.2 * vmax)
                        v = lo + (vmax - lo) * (1.0 - d) + rng.uniform(-0.06, 0.06) * vmax
                    if profile.near == "petit":               # valeur = distance : décor fixe, objet devant
                        wall = vmax * (0.5 if y < g * 3 // 4 else 0.3 + 0.03 * (g - 1 - y))
                        v = wall * (1 - min(1.0, v / vmax) * 0.7) + rng.uniform(-0.01, 0.01) * vmax
                    img.append(int(max(0, min(vmax, v))))
            frames.append(img)
        return [v for fr in frames for v in fr]
    if profile.display == "courbes":
        n_axes, n = len(profile.axes), profile.samples
        curves = []
        for a in range(n_axes):
            phase = cls_index * 1.7 + a * 0.9
            freq = 1 + (cls_index + a) % 3
            curves.append([int(max(0, min(vmax, vmax / 2 + vmax / 2.5 * math.sin(2 * math.pi * freq * i / n + phase)
                                          + rng.uniform(-0.04, 0.04) * vmax))) for i in range(n)])
        if profile.order == "entrelace":
            return [curves[a][i] for i in range(n) for a in range(n_axes)]
        return [v for c in curves for v in c]
    n = profile.length or 64
    return [int(max(0, min(vmax, vmax / 2 + vmax / 3 * math.sin(i * (cls_index + 1) * 2 * math.pi / n + t)
                           + rng.uniform(-0.05, 0.05) * vmax))) for i in range(n)]


class DemoSource(threading.Thread):
    """Simule une carte : alternance « rien devant le capteur » (messages d'attente) et signaux de chaque
    classe, au rythme du profil. Permet de découvrir l'interface et de tester sans matériel."""

    def __init__(self, profile, out_q, rate=None, phase_s=4.0, idle_s=2.0, seed=None):
        super().__init__(daemon=True)
        self.p, self.q = profile, out_q
        self.rate = rate or profile.demo_rate
        self.phase_s, self.idle_s = phase_s, idle_s
        self.rng = random.Random(seed)
        self.stop_evt = threading.Event()

    def run(self):
        self.q.put(("connected", "DÉMO (données simulées)"))
        self.q.put(("line", f"# {self.p.name} : mode COLLECTE (demo)\n"))
        while not self.stop_evt.is_set():
            order = list(range(len(self.p.classes)))
            self.rng.shuffle(order)
            for k in ([None] if self.p.waiting_prefix else []) + order:
                t_end = time.monotonic() + (self.idle_s if k is None else self.phase_s)
                last_msg = 0.0
                while time.monotonic() < t_end and not self.stop_evt.is_set():
                    now = time.monotonic()
                    if k is None:
                        if now - last_msg >= 1.0:
                            self.q.put(("line", f"# {self.p.waiting_prefix} (demo : rien devant le capteur)\n"))
                            last_msg = now
                        time.sleep(0.05)
                    else:
                        sig = demo_signal(self.p, k, now, self.rng)
                        self.q.put(("line", ",".join(str(v) for v in sig) + "\n"))
                        time.sleep(1.0 / self.rate)
                if self.stop_evt.is_set():
                    break
        self.q.put(("disconnected", ""))

    def stop(self):
        self.stop_evt.set()
