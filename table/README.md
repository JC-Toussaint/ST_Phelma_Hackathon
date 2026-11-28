# La table qui écoute : des boutons sur une table ordinaire

NUCLEO-L476RG + **X-NUCLEO-IKS01A3** (accéléromètre LSM6DSO) + **NanoEdge AI** (classification N-classes).

La carte est collée **sous** une table, et 4 zones sont marquées sur le dessus par des gommettes. On tape du
doigt sur une zone : le STM32 reconnaît **laquelle**, rien qu'aux vibrations qui traversent le plateau. Une
note par zone, et la table devient un piano :

```
TAPE zone3 95%   (zone1 2% zone2 1% zone3 95% zone4 1% autre 1%)
TAPE - (autre chose)   (zone1 4% zone2 3% zone3 2% zone4 1% autre 90%)
```

> **Essayé sur une planche** posée sur les genoux, le capteur fixé à une extrémité : **3 zones se distinguent
> facilement**. Une **librairie NanoEdge d'essai est fournie** (`Core/Inc/NanoEdgeAI.h`, `Core/Src/libneai.a`),
> apprise sur cette planche avec 4 zones et **50 tapes par zone**, sans classe `autre`. Elle ne vaut que pour
> ce support et cette position du capteur : **le travail du hackathon** est de collecter sur votre table et de
> générer votre librairie.

Ce qui le rend intéressant :
- **Impossible à calculer simplement** : l'onde d'une tape arrive à la carte plus ou moins vite et atténuée,
  avec des rebonds sur les bords et les pieds qui dépendent de l'endroit. C'est exactement le genre de motif
  qu'un modèle apprend à partir d'exemples.
- **Une cadence élevée** : 1667 échantillons par seconde, lus dans la **FIFO** du capteur.
- **Tout le reste vient de la baguette magique** : déclenchement, pré-déclenchement, fenêtre fixe, classe
  `autre`, outil de collecte commun.

```
table/
├── NUCLEO_L476RG_IKS01A3_TABLE/    projet STM32CubeIDE = projet IKS01A3 de la baguette (DataLogTerminal de ST)
│   ├── Core/Inc, Core/Src          + app, imu_sensor (FIFO), tap_preproc, tap_ai, uart_io, app_config.h
│   │                                 + librairie NanoEdge d'essai (NanoEdgeAI.h, libneai.a)
│   └── MEMS/App/app_mems.c         modifié (zones USER CODE) : appelle app_init() / app_loop()
├── DEMARRAGE.md                    la liste des opérations, dans l'ordre
├── DIFFERENCES_ST.md               différences avec le projet de ST
├── tools/table_logger_gui.py       collecte en interface graphique (outil commun outils/)
├── tools/table_logger.py           la même chose en ligne de commande
├── tools/profil_collecte.json      profil de collecte : zone1…zone4, autre ; 3 courbes de 256 points
├── tools/piano.py                  démo : une note par zone tapée
└── tests/                          tests PC (make test) + CSV synthétiques (make synth)
```

---

## 1. Montage

- ⚠️ **Sortir les cartes de leur blister avec précaution** : les broches des connecteurs, sous les cartes,
  sont pointues et peuvent piquer les doigts. Tenir les cartes par les bords.
- Enficher la X-NUCLEO-IKS01A3 sur la NUCLEO-L476RG, puis brancher le câble USB.
- **Fixer la carte fermement sous le plateau**, à plat : adhésif double face épais, ou petit serre-joint. Une
  carte qui bouge change tout. Le câble USB pend sans toucher la table.
- Coller **4 gommettes numérotées**, espacées d'au moins 15 cm. Pour commencer, 2 zones éloignées suffisent.
- **Pas de zones symétriques** par rapport à la carte : deux zones à la même distance, de part et d'autre,
  donnent des vibrations très semblables. Pour une rangée de zones (un piano), mettre la carte sous
  **une extrémité** de la rangée, pas au milieu. C'est le montage essayé : une planche, capteur à un bout.
- Pour essayer chez soi, une **planche** (étagère, plan de travail en aggloméré) posée sur les genoux ou sur
  une table suffit.
- Une table en bois ou en aggloméré transmet bien ; une table très lourde, ou sur des pieds amortissants,
  moins bien.

## 2. Projet STM32CubeIDE

1. *File > Import > General > Existing Projects into Workspace* → `NUCLEO_L476RG_IKS01A3_TABLE/` (voir
   [`CUBEIDE_IMPORT.md`](../CUBEIDE_IMPORT.md)).
2. `APP_MODE` dans `Core/Inc/app_config.h` : `0` = collecte (par défaut), `1` = reconnaissance (avec la
   librairie d'essai fournie, ou la vôtre, section 5).
3. Compiler, puis flasher via le ST-LINK.

Au démarrage, le terminal (115200 bauds) affiche « # La table qui ecoute : mode COLLECTE », puis une fois par
seconde « # En attente d'une tape (1667 ech/s) ».

**Premier contrôle** : le nombre d'échantillons par seconde doit être proche de **1667**. S'il est nettement
plus bas, la carte ne vide pas la FIFO assez vite (section 8).

## 3. Collecte des données (APP_MODE = 0)

```bash
python tools/table_logger_gui.py                  # interface graphique (recommandé)
python tools/table_logger_gui.py --demo           # pour la découvrir sans carte (données simulées)
python tools/table_logger.py --port COM5          # en ligne de commande
```

- **Comment taper** : une tape franche du bout du doigt au centre de la gommette, puis attendre que la LED
  change d'état (la ligne est partie) et que la table soit au calme (0,2 s).
- **L'interface** affiche les 3 courbes de chaque tape : un choc net juste après le début (les 32 premiers
  échantillons sont ceux d'avant la tape), puis une oscillation qui s'amortit.
- **Commencer par 2 zones éloignées** (`zone1`, `zone2`) et lancer un premier benchmark : si le Studio les
  sépare, ajouter les autres.
- **`autre`** : tout ce qui ne doit pas jouer de note : poser une tasse, un livre, un coude, taper hors des
  gommettes, frotter la table.
- **Tape ratée** : Retour arrière (*Suppr. dernier*) la retire.
- Viser **100 à 200 tapes par zone**, avec plusieurs doigts, plusieurs forces, plusieurs personnes, en
  plusieurs séances, et une passe de chaque classe à part pour la validation (`--out test`).

**Format d'un signal** : 256 échantillons × 3 axes = **768 entiers**, entrelacés `ax0, ay0, az0, ax1…`, à
**1667 Hz** (≈ 154 ms). La gravité (moyenne au repos) est retirée ; chaque valeur va de 0 à 1000,
**500 = repos**, 1 unité = 8 mg (±4 g).

💡 **Sans matériel**, `cd tests && make synth` génère des CSV **synthétiques** dans `tests/data_synth/`, passés
par le vrai découpage du firmware : pour essayer le parcours du Studio, pas pour obtenir un vrai modèle.

## 4. Générer la librairie NanoEdge AI

1. Projet **N-Class classification**, cible **STM32L476RG**, capteur **Generic, 3 axes**, 256 points par axe.
2. Importer un fichier par classe ; **garder les noms** (`zone1`… pour `piano.py`, `autre` pour le firmware).
3. Benchmark : la matrice de confusion montre les zones voisines confondues. Les écarter, ou collecter davantage.
4. Validation dans l'émulateur avec les fichiers de test.
5. Compilation avec **Float ABI hard** et **FPU fpv4-sp-d16** : le Studio produit un `.zip`.

## 5. Intégrer la librairie et jouer (APP_MODE = 1)

1. Depuis le `.zip` : `NanoEdgeAI.h` → `Core/Inc/`, `libneai.a` → `Core/Src/`, à la place de la librairie
   d'essai fournie. **En garder une copie** hors du dépôt : un `git checkout` ou un `git stash` remet celle
   du dépôt.
   La librairie est liée **dans les deux modes** (`-lneai` dans le `.cproject`, quel que soit `APP_MODE`) :
   `libneai.a` doit toujours être présent, même pour la collecte.
2. `APP_MODE` à `1`, *Project > Clean*, recompiler, flasher.
3. Table au calme : **LED allumée = prête**. Une tape sur une zone : une ligne `TAPE …`, la LED clignote.
4. **Le piano** : fermer Tera Term, puis `python tools/piano.py` (une note par zone : do, ré, mi, fa).

| Ligne affichée | Signification |
|---|---|
| `TAPE zone3 95%  (…)` | Zone reconnue (probabilité ≥ `TAP_MIN_CONFIDENCE`, 60 %) |
| `TAPE ? pas reconnue (zone2 45%)  (…)` | Le modèle hésite |
| `TAPE - (autre chose)  (…)` | Classe `autre` : ignorée |

Touches du terminal : `h` menu, `r` cadence mesurée.

| Message | Cause et solution |
|---|---|
| `#error "Pas de librairie NanoEdge…"` | `NanoEdgeAI.h` manque dans `Core/Inc` |
| `#error … n'attend pas TAP_SIGNAL_LEN valeurs` | Le projet Studio n'a pas 3 axes × 256 points |
| `undefined reference to neai_…` | `libneai.a` et `NanoEdgeAI.h` ne viennent pas du même `.zip` : copier les deux |
| `cannot find -lneai` (même en collecte) | `libneai.a` manque dans `Core/Src` : le remettre (`git checkout -- Core/Src/libneai.a`) |
| `ERREUR : init LSM6DSO` | Carte d'extension mal enfoncée, ou ce n'est pas une IKS01A3 |

## 6. Réglages (`Core/Inc/app_config.h`)

| Paramètre | Défaut | Rôle | Si… |
|---|---|---|---|
| `TAP_TRIGGER_MG` | 80 | Écart au repos qui déclenche une tape | Tapes douces ignorées → 50 ; déclenchements intempestifs → 120 |
| `TAP_CALM_MG` / `TAP_REST_MS` | 25 / 200 | Calme exigé avant la tape suivante | « table encore agitée » en permanence → `TAP_CALM_MG` à 40 |
| `TAP_MIN_CONFIDENCE` | 0.60 | Probabilité mini pour accepter une zone | Trop de « pas reconnue » → 0.5 |
| `IMU_ACC_FS_G` | 4 | Pleine échelle | Courbes qui plafonnent → 8 (recollecter) |
| `TAP_SAMPLES`, `TAP_PRETRIGGER`, `IMU_ODR_HZ` | 256, 32, 1667 | Forme du signal | Changer = recollecter, profil et Studio aussi |

## 7. Tests sur PC

```bash
cd tests
make test     # découpage des tapes (repos, armement, déclenchement, dérive lente), profil, piano.py, outil commun
make synth    # CSV synthétiques des 5 classes pour essayer NanoEdge AI Studio
```

## 8. État et limites

- **Essayé** sur une planche posée sur les genoux, capteur fixé à une extrémité : collecte, librairie
  apprise sur 4 zones (50 tapes chacune), reconnaissance ; **3 zones se distinguent facilement**.
- **Librairie fournie sans classe `autre`** : toute tape est attribuée à une zone. Collecter une classe
  `autre` (tasse posée, coude, tape hors des gommettes) pour que la table ignore le reste.
- **Cadence** : chaque échantillon demande plusieurs échanges I2C (étiquette, données). Si la cadence affichée
  reste nettement sous 1667 ech/s, la carte ne suit pas : passer à 833 Hz (`IMU_ODR_HZ`, fenêtre de 128
  échantillons, profil et Studio).
- **Le modèle ne vaut que pour cette table et cette position de la carte** : carte décollée, table déplacée
  ou objets lourds posés dessus, il faut recollecter. Le présenter comme un « étalonnage ».
- **Une seule carte** : la FIFO est propre au LSM6DSO ; une version IKS01A2 demanderait de réécrire
  `imu_sensor.c` (fonctions FIFO différentes du LSM6DSL).
