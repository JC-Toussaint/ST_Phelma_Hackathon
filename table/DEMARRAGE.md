# La table qui écoute pas à pas

La liste des opérations, dans l'ordre. Chaque étape renvoie à la section du [README](README.md) qui la détaille.

## 0. Avant de commencer
1. Installer **NanoEdge AI Studio**, **STM32CubeIDE** et **Python 3**, puis `pip install pyserial`.
2. Récupérer le dépôt sur le disque Windows (par exemple dans `Documents`).
3. Lire [`CUBEIDE_IMPORT.md`](../CUBEIDE_IMPORT.md).

## 1. Montage (README, section 1)
- ⚠️ Sortir les cartes de leur blister avec précaution (broches pointues) ; les tenir par les bords.
- Enficher la **X-NUCLEO-IKS01A3** sur la NUCLEO-L476RG, brancher le câble USB.
- Coller la carte **fermement sous la table**, à plat ; le câble ne touche pas le plateau.
- Coller 2 gommettes éloignées (zones 1 et 2) ; les zones 3 et 4 viendront ensuite.

## 2. Firmware (README, section 2)
1. Importer `table/NUCLEO_L476RG_IKS01A3_TABLE/` (*Existing Projects into Workspace*), configuration **Debug**.
2. Facultatif : essayer la **librairie fournie** en `APP_MODE 1` (lignes `TAPE …`, `piano.py`). Apprise sur une
   planche, capteur à une extrémité, avec 4 zones : sur votre table, elle reconnaîtra mal vos zones ; elle sert
   à vérifier que tout fonctionne.
3. Pour la collecte : `APP_MODE 0`, compiler, flasher.
4. Dans Tera Term (115200 bauds) : « # En attente d'une tape (… ech/s) » ; vérifier que la cadence est proche
   de **1667**. Puis fermer Tera Term.

## 3. Collecte (README, section 3)
1. `python table/tools/table_logger_gui.py`, choisir le port, *Connecter*.
2. Taper une fois : une courbe s'affiche (choc, puis oscillation amortie).
3. Classe `zone1`, **Espace**, 50 tapes ; puis `zone2`. Tape ratée : **Retour arrière**.
4. Premier benchmark à 2 zones (étape 4). S'il est bon : zones 3 et 4, puis `autre`.
5. ~150 tapes par classe, plusieurs personnes ; une passe par classe avec `--out test`.

## 4. Librairie NanoEdge (README, section 4)
- N-Class classification, STM32L476RG, **Generic 3 axes, 256 points** ; un fichier par classe, noms gardés.
- Benchmark, validation avec `tools/test/`, compilation Float ABI hard, FPU fpv4-sp-d16.

## 5. Jouer (README, section 5)
1. `NanoEdgeAI.h` → `Core/Inc/`, `libneai.a` → `Core/Src/`, à la place de la librairie d'essai (en garder une
   copie hors du dépôt) ; `APP_MODE 1`, *Project > Clean*, recompiler, flasher.
2. `python table/tools/piano.py` : une note par zone.

## Pièges à connaître

| Situation | Que faire |
|---|---|
| Cadence nettement sous 1667 ech/s | La carte ne vide pas la FIFO assez vite : passer à 833 Hz (README, section 8) |
| « table encore agitée » en permanence | Quelqu'un s'appuie, un ventilateur fait vibrer : calmer la table, ou monter `TAP_CALM_MG` |
| Rien ne se passe quand on tape | Tape trop douce : taper plus franchement, ou baisser `TAP_TRIGGER_MG` |
| Bon benchmark, mauvaise reconnaissance | Carte ou objets déplacés depuis la collecte : recollecter dans l'état de la démo |
| L'import refuse le projet (« already exists in the workspace ») | Un projet du même nom est déjà dans le workspace : le fermer, *Delete* sans effacer les fichiers, réimporter |
| `cannot find -lneai`, même en collecte | `libneai.a` manque dans `Core/Src` : la librairie est liée dans les deux modes |
| L'outil de collecte ou `piano.py` n'ouvre pas le port | Fermer Tera Term (un seul programme à la fois sur le port) |
