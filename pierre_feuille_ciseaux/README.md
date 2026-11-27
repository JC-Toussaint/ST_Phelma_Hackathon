# Pierre – Feuille – Ciseaux contre le STM32

NUCLEO-L476RG + X-NUCLEO-53L5A1 (VL53L5CX, ToF 8×8) + **NanoEdge AI** (classification N-classes).

Tu montres ta main au capteur, le STM32 reconnaît **pierre / feuille / ciseaux** et joue contre toi.
Pour l'originalité, il propose aussi :

- **Un adversaire adaptatif** : un modèle de Markov apprend tes habitudes (« après pierre, il rejoue souvent
  pierre… ») et joue ce qui les bat. Il annonce ensuite ce qu'il avait prédit.
- **Une preuve qu'il ne triche pas** : avant le compte à rebours, le STM32 publie l'empreinte (FNV-1a) de son
  coup et d'un secret, puis révèle le secret après la manche. Comme il ne peut pas changer de coup après avoir
  vu le tien, l'empreinte permet de le vérifier.
- **De l'aléa tiré du bruit de mesure** du capteur ToF.

```
pierre_feuille_ciseaux/
├── NUCLEO_L476RG_TOF_PFC/     projet STM32CubeIDE = copie du projet ST NUCLEO_L476RG_TOF_VL53L5A1
│   ├── Inc/, Src/             + app, tof_sensor, tof_preproc, rps_game, rps_ai, uart_io
│   └── X-CUBE-TOF1/App/app_x-cube-tof1.c   modifié : appelle app_init() / app_loop() ; garde les
│                                           réglages du capteur de l'exemple ST (et leurs commentaires)
├── DIFFERENCES_ST.md          liste détaillée des différences avec le projet ST
├── tools/rps_logger_gui.py    collecte en interface graphique (tkinter)
├── tools/rps_logger.py        la même chose en ligne de commande
├── tools/profil_collecte.json profil de ce projet pour l'outil commun outils/collecte_nanoedge.py
└── tests/                     tests PC (make test) + CSV synthétiques (make synth)
```

---

## 1. Montage

- Enficher la X-NUCLEO-53L5A1 sur la NUCLEO-L476RG (connecteurs Arduino). Seul le **capteur central** est
  utilisé : laisser les deux satellites débranchés.
- Poser la carte **à plat, capteur vers le plafond**. On joue la main 10–20 cm au-dessus, **paume vers le
  capteur**, doigts bien écartés pour « feuille » et en V bien ouvert pour « ciseaux ».
- La main doit être **entre 4 et 20 cm** au-dessus du capteur ; tout ce qui est au-delà de 20 cm est ignoré
  (réglage `PRE_FAR_MM`, voir [REGLAGE_DISTANCE.md](REGLAGE_DISTANCE.md)).

## 2. Projet STM32CubeIDE

Le dossier `NUCLEO_L476RG_TOF_PFC/` est le projet ST `NUCLEO_L476RG_TOF_VL53L5A1`, auquel l'application
a été ajoutée (voir `DIFFERENCES_ST.md`). Le brochage, l'I2C, le BSP X-CUBE-TOF1 et la librairie NanoEdge
viennent donc du projet ST : il n'y a rien à configurer.

1. *File > Import > Existing Projects into Workspace* → sélectionner `NUCLEO_L476RG_TOF_PFC/`.
2. Choisir le mode avec le symbole `APP_MODE` : `0` = collecte (valeur par défaut), `1` = jeu. On peut le
   définir dans *Project > Properties > C/C++ Build > Settings > MCU GCC Compiler > Preprocessor*, ou
   directement dans `Inc/app_config.h`.
3. Compiler (Debug ou Release), puis flasher via le ST-LINK.

⚠️ Ne pas régénérer le code depuis le `.ioc` sans précaution (voir `DIFFERENCES_ST.md`, section 5).

## 3. Collecte des données (APP_MODE = 0)

1. Compiler avec `APP_MODE=0` (valeur par défaut) et flasher la carte.
2. Sur le PC : `pip install pyserial`, puis :
   ```bash
   python tools/rps_logger_gui.py                    # interface graphique (recommandé)
   python tools/rps_logger_gui.py --demo             # pour la découvrir sans carte (données simulées, dans tools/demo/)
   python tools/rps_logger.py --port COM5 --view     # ou en ligne de commande
   python tools/rps_logger.py --port COM5            # collecte guidée : pierre, feuille, ciseaux
   ```
   Ces deux scripts lancent l'**outil de collecte commun au dépôt** (`outils/collecte_nanoedge.py`) avec
   le profil de ce projet, `tools/profil_collecte.json` : classes, 512 valeurs par signal, affichage en
   grille 8×8. Voir [outils/README.md](../outils/README.md), et [tools/README.md](tools/README.md) pour
   la façon dont ce profil se déduit du firmware, et pour `tools/augmente_pfc.py`, qui fabrique des
   variantes plausibles des signaux réels (rotation, autre main, vitesse…) et met une partie des vrais
   signaux de côté pour vérifier dans le Studio que cela aide.
   L'interface graphique (tkinter, fourni avec Python sous Windows) affiche la grille 8×8 en direct et les
   8 trames du signal, ainsi que les messages de la carte. Elle permet de choisir la classe (touches 1/2/3),
   de lancer une passe avec compte à rebours (Espace), de suivre la progression et le nombre de signaux par
   fichier, et d'**annuler la dernière passe** si elle est ratée.
   La liaison série est **USART2 à 115200 bauds uniquement**, telle que le projet ST l'initialise : le
   firmware ne la reconfigure pas. À ce débit, la carte envoie un signal toutes les 3 trames
   (`PRE_DATALOG_EVERY`, 5 signaux/s) : une ligne de 512 valeurs prend jusqu'à 180 ms, et ce rythme
   garantit qu'aucune trame du capteur n'est manquée pendant l'envoi. Chaque passe ajoute 300 signaux par classe dans `pierre.csv`, `feuille.csv` et `ciseaux.csv`,
   **dans le dossier `tools/`, à côté des scripts**, quel que soit le dossier d'où on les lance. `--out` ou
   le champ « Dossier » de l'interface permettent d'en choisir un autre.
   **Faire 2–3 passes** en variant la hauteur, la position, la rotation, la main (gauche/droite), les
   personnes et l'éclairage. À 5 signaux par seconde, 300 signaux s'enregistrent en ~60 s.
3. Le firmware n'envoie rien quand aucune main n'est détectée. Il n'y a donc pas de classe « rien » : la
   présence de la main est détectée sans IA (`PRE_MIN_HAND_ZONES`).

**Format d'un signal** : 8 trames × 64 zones = 512 entiers, la même taille que la librairie modèle du projet
ST. Chaque zone vaut 0 (fond) ou 20…100 (main,
100 = point le plus proche). C'est donc un relief **indépendant de la hauteur de la main**, et la main est
**recentrée** dans la grille avant envoi.

## 4. Générer la librairie NanoEdge AI

La librairie se génère dans **NanoEdge AI Studio**, le logiciel gratuit de ST (Windows ou Linux,
téléchargeable sur st.com). Tu y importes les fichiers CSV, il compare des centaines d'algorithmes, puis il
produit la librairie à copier dans le projet CubeIDE. Les noms d'écrans peuvent varier un peu selon la
version du Studio.

### Avant de commencer

- **Données** : au moins 200 à 300 signaux par classe, venant de 2 ou 3 passes variées. Varie les personnes, l'éclairage et la position de la main.
- **Données de test** : garde une passe à part (`--out test`, ou le dossier `tools/test/` dans
  l'interface). Elle ne doit pas servir à l'entraînement, seulement à la validation.
- **Format** : les fichiers sont déjà au bon format, une ligne par signal avec 512 entiers séparés par des
  virgules, sans en-tête.

### Étapes dans le Studio

1. **Créer le projet** : nouveau projet de type **N-Class classification**.
2. **Paramètres du projet** :
   - Cible : **STM32L476RG** (ou la carte NUCLEO-L476RG).
   - RAM et flash maximales : garder les valeurs proposées. La carte a 128 Ko de RAM et 1 Mo de flash,
     l'application n'en utilise qu'une petite partie.
   - Capteur : **Generic** avec **1 axe**. C'est la configuration de la librairie modèle du projet ST : la
     ligne de 512 valeurs est vue comme un seul signal.
   - Facultatif, si le temps le permet : refaire un projet avec **64 axes**, où chaque zone est suivie sur
     8 trames, et garder le meilleur des deux benchmarks. Le firmware accepte les deux, car il vérifie
     seulement que la librairie attend 512 valeurs.
3. **Signaux** : importer un fichier par classe (*Import from file*, séparateur virgule) : `pierre.csv`,
   `feuille.csv`, `ciseaux.csv`.
   - **Garder ces noms de classe.** Le firmware associe chaque classe à un coup par son nom
     (`pierre`/`rock`, `feuille`/`paper`, `ciseaux`/`scissors`), l'ordre d'import n'a donc pas d'importance.
   - Le Studio vérifie les fichiers : 512 valeurs par ligne, aucune valeur manquante.
4. **Benchmark** : le lancer. Il prend de quelques minutes à quelques dizaines de minutes. Viser une
   **précision équilibrée** (*balanced accuracy*) d'au moins 90 %. La matrice de confusion montre les
   classes confondues : si « ciseaux » est souvent pris pour « feuille », compléter les données de ces deux
   classes et relancer.
5. **Validation** : dans l'émulateur, importer les fichiers de **test**. Le taux de bonne reconnaissance
   doit rester proche de celui du benchmark. Utiliser des fichiers plutôt que la lecture série directe, car
   les lignes `#` du firmware peuvent la perturber.
6. **Compilation** : options à choisir pour correspondre au projet CubeIDE (vérifiées dans le `.cproject`) :
   - **Float ABI : hard**
   - **FPU : fpv4-sp-d16**

   Lancer la compilation : le Studio produit un `.zip`.

### Intégration dans le projet

1. Depuis le `.zip`, remplacer dans `NUCLEO_L476RG_TOF_PFC/` :
   - `Inc/NanoEdgeAI.h` ;
   - `Src/libneai.a` ;
   - avec une ancienne version du Studio, ajouter aussi `knowledge.h` dans `Inc/`. `rps_ai.c` gère les deux
     générations d'API.
2. Mettre `APP_MODE` à `1` dans `Inc/app_config.h`, ou définir le symbole dans les propriétés du projet.
3. Recompiler, flasher, puis jouer (section 5).

### Messages d'erreur possibles

| Message | Cause et solution |
|---|---|
| `#error La librairie NanoEdge n'attend pas SIGNAL_LEN valeurs` | La taille du signal déclarée dans le Studio n'est pas 512. Refaire le projet Studio avec 1 axe (ou 64 axes) |
| Au démarrage : `# ERREUR : init NanoEdge (classes pierre/feuille/ciseaux introuvables ?)` | Les classes ne s'appellent pas pierre, feuille et ciseaux, ou c'est encore la librairie modèle de ST (2 classes) |
| Erreur d'édition de liens « uses VFP register arguments » / float-abi | Recompiler la librairie dans le Studio avec Float ABI **hard** et FPU **fpv4-sp-d16** |
| Reconnaissance bonne dans le Studio mais mauvaise sur la carte | Conditions différentes de la collecte (hauteur, éclairage, autre personne) : refaire une passe dans ces conditions et régénérer |

💡 **Sans matériel**, `cd tests && make synth` génère des CSV **synthétiques** dans `tests/data_synth/`. Ils
permettent de vérifier le format, le benchmark, la compilation et l'intégration, mais pas d'obtenir un vrai
modèle.

## 5. Jouer (APP_MODE = 1)

Compiler avec le symbole `APP_MODE=1` et la librairie NanoEdge générée (avec la librairie modèle de ST, le
démarrage s'arrête sur « init NanoEdge »), flasher, puis ouvrir un terminal série (PuTTY, Tera Term,
`screen`, `minicom`…) à **115200 bauds**.

| Touche | Action |
|---|---|
| **B1** (bouton bleu), `espace` ou `Entrée` | Jouer une manche |
| `m` | Stratégie adaptative ↔ aléatoire |
| `r` | Remise à zéro du score |
| `d` | Affichage en direct de la reconnaissance (coup, probabilité de chaque classe, nb de zones, distance) : voir ci-dessous |
| `h` | Menu |

**Affichage en direct (`d`).** Hors manche, quand une main est devant le capteur, la carte classe le
signal toutes les 300 ms environ et affiche une ligne :

```
  [direct] pierre 87%  (pierre 87% feuille 9% ciseaux 4%)  (14 zones, 152 mm)
           ^^^^^^^^^^   ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^   ^^^^^^^^^^^^^^^^^
           coup retenu  probabilité de chaque classe        zones de main, distance mini
```

- **Coup retenu** : la classe la plus probable et sa probabilité. En manche, un vote ne compte que si
  cette probabilité atteint `GAME_MIN_CONFIDENCE` (60 %).
- **Probabilités** : celles que renvoie `neai_classification()`, une par classe, de somme ≈ 100 %. Elles
  sont dans l'**ordre des classes de la librairie NanoEdge** (l'ordre d'import des CSV dans le Studio),
  pas forcément pierre, feuille, ciseaux, et portent le nom de la classe. Elles montrent si le modèle
  hésite : `pierre 52% ciseaux 45%` signale deux classes mal séparées. C'est le moment de collecter
  plus de signaux ou de varier la hauteur et la position de la main.
- **Dans le code** : le tableau `probs` de `Src/rps_ai.c` est rempli par chaque classification ;
  `rps_ai_class_count()`, `rps_ai_class_name(k)` et `rps_ai_class_prob(k)` (déclarées dans
  `Inc/rps_ai.h`) le lisent depuis le reste du programme ; l'affichage est fait dans `Src/app.c`
  (état `ST_IDLE`). Au débogueur : point d'arrêt après `neai_classification()` dans `rps_ai.c`, et
  `probs` dans la vue *Expressions*.

Déroulement : *« Pierre… Feuille… Ciseaux… CHIFOUMI ! »* (la LED clignote), puis tu montres ton coup. Le
STM32 cumule 5 classifications concordantes (confiance ≥ 60 %) et affiche le duel, le gagnant, sa
prédiction, la vérification de l'empreinte et le score. La LED reste allumée si le STM32 a gagné.

```
     TOI            STM32
     _____       vs     ____
    |     |      vs    (____)
    |     |      vs    (____)
    |_____|      vs    (____)
   FEUILLE          PIERRE

  >>> TU GAGNES ! FEUILLE bat PIERRE
  (je pensais que tu jouerais CISEAUX ... rate.)
  Verification : coup=PIERRE secret=8f3a1c2e -> empreinte 5b0e93d1
  Score >> Toi 3 - 2 STM32   (egalites : 1)
```

## 6. Réglages (`NUCLEO_L476RG_TOF_PFC/Inc/app_config.h`)

| Paramètre | Rôle | Si… |
|---|---|---|
| `PRE_FAR_MM` (200) | Au-delà = fond (avec `MAX_THRESHOLD`, voir [REGLAGE_DISTANCE.md](REGLAGE_DISTANCE.md)) | Fond détecté comme une main → diminuer |
| `PRE_DEPTH_WINDOW_MM` (120) | Profondeur gardée derrière le point le plus proche | Avant-bras gênant → diminuer |
| `PRE_MIN_HAND_ZONES` (5) | Seuil de présence de main | Fausses détections → augmenter |
| `PRE_FRAMES_PER_SIGNAL` (8) | Trames par signal (512 valeurs) | Réponse plus vive → 4, mais il faut refaire la librairie dans Studio |
| `GAME_VOTES_NEEDED` (5) | Votes avant décision | Réponse lente → 3 |
| `GAME_MIN_CONFIDENCE` (0.6) | Confiance mini par vote | « Je n'ai pas bien vu ta main » fréquent → 0.5 |
| `GAME_EXPLORATION_PCT` (20) | % de coups aléatoires de l'IA adaptative | Trop prévisible → augmenter |
| `TOF_TARGET_CLOSEST` (1) | Cible la plus proche par zone | Main mal vue au-dessus d'un objet brillant → essayer 0 |
| `APP_UART_BAUD` (115200) | Débit du terminal, fixé : USART2 tel que `main.c` l'initialise | Ne pas changer (`#error` sinon) |
| `PRE_DATALOG_EVERY` (3) | Collecte : un signal envoyé toutes les N trames | Messages coupés ou trames manquées → 4 |

**Réglages du capteur** : ils restent ceux de l'exemple ST, avec ses commentaires, dans
`X-CUBE-TOF1/App/app_x-cube-tof1.c` (`SUCCESSIVE_MATRIX`, `MATRIX_RESOLUTION`, `MIN_THRESHOLD` /
`MAX_THRESHOLD`, `NEAI_MODE`, `Profile.*`, `ITConfig.*`). Seules différences : interruption à chaque trame
(`RS_IT_DEFAULT`) et seuils 40 / 200 mm. Les valeurs partagées avec `app_config.h` (`PRE_FRAMES_PER_SIGNAL`,
`TOF_ZONES`, `PRE_NEAR_MM` / `PRE_FAR_MM`, `TOF_FREQ_HZ`) doivent être modifiées **des deux côtés** : sinon
la compilation s'arrête sur un `#error` qui dit laquelle.

**Ciseaux confondus avec feuille ?** C'est la paire la plus difficile en 8×8. Écarter franchement les deux
doigts en V, replier les autres, et collecter davantage de données « ciseaux » sous différents angles.

## 7. Tests sur PC

```bash
cd tests
make test     # prétraitement (seuils, statuts, recentrage, fenêtre glissante) + logique de jeu
make synth    # CSV synthétiques pour essayer NanoEdge AI Studio
```
