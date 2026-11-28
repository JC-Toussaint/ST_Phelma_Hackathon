#!/usr/bin/env python3
"""
piano.py - La table qui écoute : une note par zone tapée (démo).

Lit les lignes « TAPE zone3 95%  (…) » que la carte envoie en mode reconnaissance (APP_MODE_TABLE) et joue
une note par zone : do, ré, mi, fa, sol, la. Les lignes « TAPE - … » (classe autre) et « TAPE ? … » (zone
pas reconnue) ne jouent rien ; les messages « # … » de la carte sont affichés.

Utilisation :
    python piano.py                    # port du ST-LINK trouvé automatiquement
    python piano.py --port COM5
    python piano.py --demo             # sans carte : des tapes au hasard
    python piano.py --liste            # ports série disponibles

Fermer d'abord Tera Term ou l'outil de collecte : un seul programme à la fois sur le port.
Son : winsound (fourni avec Python sous Windows) ; ailleurs, la note est seulement affichée.
Dépendance : pip install pyserial
"""
import argparse
import random
import re
import sys
import time

# Une note par zone, dans l'ordre (zone1 = do…), en Hz. Les classes sont repérées par leur nom.
NOTES = [("do", 523), ("ré", 587), ("mi", 659), ("fa", 698), ("sol", 784), ("la", 880)]
DUREE_MS = 220

LINE = re.compile(r"^TAPE\s+(\S+)\s+(\d+)%")


def parse_line(line):
    """« TAPE zone3 95% (…) » -> ("zone3", 95) ; None pour tout le reste (autre, pas reconnue, messages)."""
    m = LINE.match(line.strip())
    if not m or m.group(1) in ("-", "?"):
        return None
    return m.group(1), int(m.group(2))


def note_for(zone):
    """« zone3 » -> ("mi", 659) ; None si le nom ne se termine pas par un numéro de zone connu."""
    m = re.search(r"(\d+)$", zone)
    if not m or not 1 <= int(m.group(1)) <= len(NOTES):
        return None
    return NOTES[int(m.group(1)) - 1]


def play(freq):
    try:
        import winsound
        winsound.Beep(freq, DUREE_MS)
    except ImportError:                     # pas Windows : pas de son, la note est affichée
        time.sleep(DUREE_MS / 1000)


def handle(line):
    hit = parse_line(line)
    if hit:
        zone, pct = hit
        note = note_for(zone)
        if note:
            print(f"{zone:8s} {pct:3d} %   ♪ {note[0]}")
            play(note[1])
        else:
            print(f"{zone:8s} {pct:3d} %   (pas de note pour cette classe)")
    elif line.startswith("#") or line.startswith("TAPE"):
        print(line)


def find_port():
    from serial.tools import list_ports
    ports = list(list_ports.comports())
    st = [p.device for p in ports if "STLink" in (p.description or "").replace("-", "")]
    return st[0] if st else (ports[0].device if len(ports) == 1 else None)


def main(argv=None):
    ap = argparse.ArgumentParser(description="La table qui écoute : une note par zone tapée")
    ap.add_argument("--port", help="port série de la carte (COM5, /dev/ttyACM0…) ; défaut : le ST-LINK")
    ap.add_argument("--demo", action="store_true", help="sans carte : des tapes au hasard")
    ap.add_argument("--liste", action="store_true", help="afficher les ports série et quitter")
    args = ap.parse_args(argv)

    if args.demo:
        print("DÉMO : tapes simulées (Ctrl+C pour arrêter)")
        try:
            while True:
                k = random.randint(1, 5)
                handle(f"TAPE zone{k} {random.randint(70, 99)}%" if k < 5 else "TAPE - (autre chose)")
                time.sleep(0.6)
        except KeyboardInterrupt:
            return 0

    try:
        import serial
        from serial.tools import list_ports
    except ImportError:
        print("Il manque pyserial : pip install pyserial")
        return 1
    if args.liste:
        for p in list_ports.comports():
            print(f"{p.device:12s} {p.description}")
        return 0
    port = args.port or find_port()
    if not port:
        print("Port introuvable : préciser --port (voir --liste)")
        return 1
    try:
        ser = serial.Serial(port, 115200, timeout=0.5)
    except serial.SerialException as e:
        print(f"Impossible d'ouvrir {port} : {e}\n(Tera Term ou l'outil de collecte l'utilise peut-être)")
        return 1
    print(f"À l'écoute sur {port} : tapez sur la table (Ctrl+C pour arrêter)")
    try:
        while True:
            raw = ser.readline()
            if raw:
                handle(raw.decode("ascii", "replace").rstrip())
    except KeyboardInterrupt:
        return 0
    finally:
        ser.close()


if __name__ == "__main__":
    sys.exit(main())
