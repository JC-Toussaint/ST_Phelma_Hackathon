# Sentinelle : une carte qui surveille une scène et explique ses alarmes

NUCLEO-L476RG + X-NUCLEO-53L5A1 (VL53L5CX, ToF 8×8) + **NanoEdge AI** (classification N-classes :
`normal` / `anomalie`).

> 🚀 **Première fois ?** Suivre [`DEMARRAGE.md`](DEMARRAGE.md) : toutes les opérations dans l'ordre, de
> l'installation des logiciels à la surveillance.

Tu poses la carte face à un bureau, une étagère ou une porte. Avec la collecte, tu montres à NanoEdge AI
Studio des exemples de scène **normale** et des exemples d'**anomalies** : quelqu'un qui passe, un objet
ajouté, retiré ou déplacé. Le Studio en tire un modèle de classification, qui tourne ensuite **sur le STM32,
sans PC** : la carte surveille la scène dès la mise sous tension et signale tout ce qui n'est pas normal :

```
[3:42] !!! ALARME #2 : la scene a change (proba normal 12%)
    Carte des changements (+ apparu/rapproche, - disparu/eloigne) : 3 zone(s)
      . . . . . . . .
      . . . . . . . .
      . . . . . . . .
      . . . . . . . .
      . . . . . . . .
      . . . . . . - -
      . . . . . . - .
      . . . . . . . .
    Si c'est normal desormais : recollecter et regenerer la librairie.
[3:51] Retour a la normale apres 9 s (proba normal mini 8%)
```

Ce qui la rend originale :

- **IA embarquée, sans caméra** : le modèle NanoEdge calcule sur le STM32 la **probabilité que la scène
  soit normale**, à partir de 64 distances seulement : aucune image n'est prise.
- **Alarme expliquée** : l'IA dit **s'il y a** une anomalie ; une carte 8×8, calculée sans IA, montre
  **où** la scène a changé et **dans quel sens** (`+` objet apparu ou quelqu'un devant, `-` objet disparu).
- **Alarme avec hystérésis** : il faut 3 signaux anormaux d'affilée pour lever l'alarme, et 3 normaux pour la
  retirer. Il n'y a donc pas de fausse alerte sur une mesure bruitée.
- **Autonome** : une fois la librairie intégrée, la carte surveille seule ; la LED clignote en cas d'alarme,
  et le terminal série, facultatif, détaille chaque alarme.

```
sentinelle/
├── NUCLEO_L476RG_TOF_SENTINELLE/   projet STM32CubeIDE = copie du projet ST NUCLEO_L476RG_TOF_VL53L5A1
│   ├── Inc/, Src/                  + app, tof_sensor, scene_preproc, sentinel_ai, uart_io
│   └── X-CUBE-TOF1/App/app_x-cube-tof1.c   modifié : appelle app_init() / app_loop() ; garde les
│                                           réglages du capteur de l'exemple ST (et leurs commentaires)
├── DEMARRAGE.md                    les opérations dans l'ordre, pour une première fois
├── DIFFERENCES_ST.md               liste détaillée des différences avec le projet ST
├── tools/sentinel_logger_gui.py    collecte en interface graphique (outil commun outils/, tkinter)
├── tools/sentinel_logger.py        la même chose en ligne de commande
├── tools/profil_collecte.json      profil de collecte : classes, format du signal, affichage
├── img/collecte_gui.png            capture de l'interface de collecte
├── sentinelle.pptx                 présentation du projet (diapositives tirées de ce README)
└── tests/                          tests PC (make test) + CSV synthétiques (make synth)
```

---

## 1. Montage et installation

- ⚠️ **Sortir les cartes de leur blister avec précaution** : les broches des connecteurs, sous les cartes,
  sont pointues et peuvent piquer les doigts. Tenir les cartes par les bords.
- Enficher la X-NUCLEO-53L5A1 sur la NUCLEO-L476RG (connecteurs Arduino), puis brancher le câble USB
  (ST-LINK). Il alimente la carte ; le terminal série reste facultatif en surveillance.
- **Poser la carte face à la scène** à surveiller (bureau, étagère, porte), capteur tourné vers elle, à
  **0,5–3 m**. Au-delà de 3 m (`SENT_MAX_MM`), tout est compté comme « loin ».
- **Fixer la carte** (adhésif, support, pile de livres), **à la même place pour la collecte et pour la
  surveillance** : le modèle apprend la scène vue depuis cet endroit. Si la carte bouge, toute la scène
  change et l'alarme se déclenche.
- Éviter le **soleil direct** sur la scène ou sur le capteur : il dégrade les mesures.

## 2. Projet STM32CubeIDE

Le dossier `NUCLEO_L476RG_TOF_SENTINELLE/` est le projet ST `NUCLEO_L476RG_TOF_VL53L5A1`, auquel
l'application a été ajoutée (voir `DIFFERENCES_ST.md`). Le brochage, l'I2C, le BSP X-CUBE-TOF1 et l'UART à
115200 bauds viennent du projet ST : il n'y a rien à configurer.

1. *File > Import > Existing Projects into Workspace* → sélectionner `NUCLEO_L476RG_TOF_SENTINELLE/`. Il
   peut cohabiter avec `NUCLEO_L476RG_TOF_PFC` dans le même workspace.
2. Choisir le mode avec le symbole `APP_MODE` : `0` = collecte (valeur par défaut), `1` = sentinelle,
   dans `Inc/app_config.h`.
3. Compiler, puis flasher via le ST-LINK.

⚠️ Le mode `1` demande votre librairie NanoEdge (section 5) : la librairie modèle fournie par ST n'a pas
de classe `normal` et s'arrête sur « init NanoEdge ».
⚠️ Ne pas régénérer le code depuis le `.ioc` sans précaution (voir `DIFFERENCES_ST.md`, section 5).

## 3. Comment la carte décide

1. **Un signal** = 8 trames 8×8 consécutives (~0,5 s), soit 512 distances.
2. **NanoEdge** classe le signal et renvoie la probabilité de chaque classe. La carte garde celle de
   `normal` : c'est la **« proba normal »** affichée (0–100 %).
3. **Alarme** : sous le seuil (90 % par défaut) pendant 3 signaux d'affilée, l'alarme se lève ; au-dessus
   pendant 3 signaux, elle retombe.
4. **Carte des changements** : au démarrage, la carte mémorise pendant 1 s une image de référence de la
   scène (moyenne et écart-type de chaque zone). À chaque alarme, elle compare la scène actuelle à cette
   référence et affiche les zones qui ont changé. **Cette partie n'utilise pas l'IA.**

Conséquences pratiques :
- le modèle ne connaît que ce que la collecte lui a montré : **collecter sur la scène qui sera surveillée**,
  depuis la même place, et mettre dans « normal » toutes les variations acceptables (lumière, chaise un peu
  déplacée…), sinon elles déclencheront une alarme ;
- **au démarrage, la scène doit être normale**, personne devant, puisque l'image de référence est prise à
  ce moment. Pour la reprendre : bouton RESET (noir).

## 4. Collecte (APP_MODE = 0)

1. Compiler avec `APP_MODE=0` (valeur par défaut), flasher, puis installer la carte face à la scène, à sa
   place définitive (section 1).
2. Sur le PC : `pip install pyserial`, puis :
   ```bash
   python tools/sentinel_logger_gui.py                  # interface graphique (recommandé)
   python tools/sentinel_logger_gui.py --demo           # pour la découvrir sans carte (données simulées)
   python tools/sentinel_logger.py --port COM5 --view   # ou en ligne de commande
   python tools/sentinel_logger.py --port COM5          # normal.csv puis anomalie.csv
   ```
   Ces deux scripts lancent l'outil de collecte commun du dépôt ([outils/](../outils/README.md)) avec le
   profil `tools/profil_collecte.json`. Le mode d'emploi de l'interface est juste après, dans « Utiliser
   l'interface de collecte ».
3. La carte envoie un signal **en continu**, même quand rien ne bouge : **~2 signaux/s** (un toutes les
   8 trames), à 115200 bauds. Une passe de 300 signaux dure donc **~2 min 30**.
4. Les fichiers `normal.csv` et `anomalie.csv` sont enregistrés **dans le dossier `tools/`, à côté des
   scripts**. `--out` ou le champ « Dossier » de l'interface permettent d'en choisir un autre.
   - **normal** : scène immobile, **personne dans le champ** (lancer la passe, puis sortir du champ
     pendant le compte à rebours). Plusieurs passes : lumière allumée ou éteinte, heures différentes,
     petites variations acceptables (chaise un peu déplacée…).
   - **anomalie** : tout ce qui doit déclencher l'alarme : passer devant, retirer, ajouter ou déplacer des
     objets, ouvrir un tiroir ou une porte… le plus varié possible.
   - Viser des **nombres de signaux proches** pour les deux classes.

**Format d'un signal** : 8 trames × 64 zones = 512 entiers, la même taille que la librairie modèle du projet
ST. Chaque zone vaut `100 × distance / 3000 mm` (1 unité = 30 mm) ; **100** = 3 m ou plus, ou pas de cible.
Contrairement à Pierre-Feuille-Ciseaux, rien n'est recentré : on garde les **distances absolues de toute la
scène**, puisque c'est elle qu'on surveille.

### Utiliser l'interface de collecte (`sentinel_logger_gui.py`)

![Interface de collecte en mode --demo : vue Écarts pendant une passe « anomalie »](img/collecte_gui.png)

*Capture en mode `--demo` : la référence a été mémorisée (R), la vue Écarts montre l'objet simulé qui est
apparu (+ rouge) ou a quitté une zone (− bleu), pendant une passe « anomalie ».*

L'interface est la même que pour Pierre-Feuille-Ciseaux (voir son
[README](../pierre_feuille_ciseaux/README.md#utiliser-linterface-de-collecte-rps_logger_guipy)), avec des
outils en plus pour une scène :

| Zone | Contenu |
|---|---|
| **Grille 8×8** | La dernière trame : **proche = clair, loin = sombre**, case vide = pas de cible. Toute la scène est visible, pas seulement un objet |
| **Distances (D)** | Écrit dans chaque case la distance en cm ; sous la grille, la distance de l'objet le plus proche |
| **Mémoriser la référence (R)** | Enregistre la scène actuelle (trame moyenne du dernier signal) comme référence et active la vue Écarts |
| **Écarts (E)** | Zones qui ont changé de plus de 120 mm par rapport à la référence : `+` rouge = apparu ou rapproché, `−` bleu = disparu ou éloigné, comme la carte des changements du firmware |
| **Enregistrement** | Touche `1` normal, `2` anomalie ; l'astuce sous la barre change avec la classe |
| **Messages de la carte** | « Sentinelle : mode COLLECTE », puis rien d'autre : la carte n'envoie que des signaux |

**Déroulement d'une collecte**

1. Flasher en `APP_MODE 0`, **fermer Tera Term**, lancer `python tools/sentinel_logger_gui.py`, choisir le
   port, puis *Connecter* : la grille s'anime aussitôt (~2 signaux/s).
2. Scène normale, personne devant : appuyer sur **R** pour mémoriser la référence. La vue Écarts doit
   montrer très peu de zones changées.
3. **Vérifier que les anomalies sont vues** : passer devant, déplacer un objet. Les `+` / `−` doivent
   apparaître au bon endroit ; un objet trop petit ou trop loin ne sera pas vu non plus par le modèle.
4. Choisir `1` (normal), appuyer sur **Espace** et **sortir du champ** pendant le compte à rebours. Ne
   rien toucher pendant ~2 min 30.
5. Choisir `2` (anomalie), **Espace**, puis provoquer des changements variés pendant toute la passe.
6. Recommencer : **2 ou 3 passes par classe**, dans des conditions différentes. Une passe ratée (quelqu'un
   est passé pendant « normal ») se retire avec **Annuler la dernière passe**.

**Problèmes fréquents** : ceux de Pierre-Feuille-Ciseaux (port occupé, liste des ports vide, touches sans
effet quand le curseur est dans un champ), plus :

| Symptôme | Cause et solution |
|---|---|
| Beaucoup de zones changées sans que rien ne bouge | Référence mémorisée alors que quelqu'un était dans le champ, ou carte qui a bougé : la refixer, puis **R** |
| Grille presque vide (peu de cases colorées) | Rien à moins de 3 m : la scène est trop loin (ou le capteur mal orienté) : rapprocher la carte |
| « Ligne ignorée : … (profil ou mode de la carte ?) » | La carte n'est pas en mode collecte (`APP_MODE 1`) ou un autre projet est flashé |

## 5. Générer la librairie NanoEdge AI

La librairie se génère dans **NanoEdge AI Studio**, le logiciel gratuit de ST (Windows ou Linux,
téléchargeable sur st.com). Tu y importes les fichiers CSV, il compare des centaines d'algorithmes, puis il
produit la librairie à copier dans le projet CubeIDE. Les noms d'écrans peuvent varier un peu selon la
version du Studio.

### Avant de commencer

- **Données** : au moins 200 à 300 signaux par classe, venant de 2 ou 3 passes variées, **sur la scène qui
  sera surveillée**. Pour « normal » : lumière allumée/éteinte, heures différentes. Pour « anomalie » :
  passages, objets ajoutés, retirés ou déplacés, tiroirs, portes.
- **Données de test** : garde une passe de chaque classe à part, pour la validation : relancer l'outil avec
  `--out test` (les fichiers vont dans `tools/test/`).
- **Format** : les fichiers sont déjà au bon format, une ligne par signal avec 512 entiers séparés par des
  virgules, sans en-tête.

### Étapes dans le Studio

1. **Créer le projet** : nouveau projet de type **N-Class classification**.
2. **Paramètres du projet** :
   - Cible : **STM32L476RG** (ou la carte NUCLEO-L476RG).
   - RAM et flash maximales : garder les valeurs proposées.
   - Capteur : **Generic** avec **1 axe**, comme la librairie modèle du projet ST. Facultatif : essayer aussi
     **64 axes** et garder le meilleur benchmark. Le firmware accepte les deux, car il vérifie seulement
     qu'il y a 512 valeurs.
3. **Signaux** : importer un fichier par classe (*Import from file*, séparateur virgule) : `normal.csv`,
   `anomalie.csv`. **Garder le nom de classe `normal`** : le firmware cherche cette classe pour calculer la
   « proba normal ».
4. **Benchmark** : le lancer, puis viser une **précision équilibrée** (*balanced accuracy*) d'au moins
   90 %. La matrice de confusion montre les erreurs : des signaux normaux classés « anomalie » donneront de
   fausses alarmes ; ajouter des passes « normal » plus variées et relancer.
5. **Validation** : dans l'émulateur, importer les fichiers de **test**. Le taux de bonne reconnaissance
   doit rester proche de celui du benchmark.
6. **Compilation** : options à choisir pour correspondre au projet CubeIDE (vérifiées dans le `.cproject`) :
   - **Float ABI : hard**
   - **FPU : fpv4-sp-d16**

   Lancer la compilation : le Studio produit un `.zip`.

### Intégration dans le projet

1. Depuis le `.zip`, remplacer dans `NUCLEO_L476RG_TOF_SENTINELLE/` :
   - `Inc/NanoEdgeAI.h` ;
   - `Src/libneai.a`.
2. Mettre `APP_MODE` à `1` dans `Inc/app_config.h`.
3. Recompiler, flasher, puis mettre la carte sous tension **à sa place, scène normale** (section 6).

### Messages d'erreur possibles

| Message | Cause et solution |
|---|---|
| `#error La librairie NanoEdge n'attend pas SENT_SIGNAL_LEN valeurs` | La taille du signal déclarée dans le Studio n'est pas 512. Refaire le projet Studio avec 1 axe (ou 64 axes) |
| `ERREUR : init NanoEdge (pas de classe "normal" dans la librairie) (code -2)` | La classe ne s'appelle pas `normal`, ou c'est encore la librairie modèle de ST. Refaire le projet Studio avec `normal.csv` et `anomalie.csv` |
| Erreur d'édition de liens « uses VFP register arguments » / float-abi | Recompiler la librairie dans le Studio avec Float ABI **hard** et FPU **fpv4-sp-d16** |
| Fausses alarmes fréquentes sur la carte | Conditions absentes de la collecte (lumière, objet déplacé), ou carte qui a bougé : refaire des passes « normal » dans ces conditions et régénérer, ou baisser le seuil avec `-` |

💡 **Sans matériel**, `cd tests && make synth` génère `normal.csv` et `anomalie.csv` **synthétiques** dans
`tests/data_synth/`. Ils permettent de vérifier le format, le benchmark, la compilation et l'intégration,
mais pas d'obtenir un vrai modèle.

## 6. Utilisation (APP_MODE = 1)

Compiler avec `APP_MODE=1`, flasher, puis mettre la carte sous tension à sa place, **scène normale et
personne devant** : l'image de référence de la carte des changements est prise pendant la première
seconde. La surveillance commence aussitôt. Le terminal série (**115200 bauds**, comme dans le projet ST)
est **facultatif** : la LED suffit.

| LED LD2 | État |
|---|---|
| Éteinte | Surveillance |
| Clignote vite | **ALARME** |

| Commande (terminal) | Action |
|---|---|
| `+` / `-` | Seuil d'alarme ± 5 % (90 % par défaut) |
| `v` | Probabilité de « normal » en continu : `proba normal  97% [###################-]` |
| `c` | Afficher la carte des changements maintenant |
| `h` | Menu |
| Bouton RESET (noir) | Redémarrer : nouvelle image de référence de la scène |

Une alarme sur une situation désormais normale ne se corrige pas sur la carte : le modèle est figé. Il faut
recollecter dans cette situation (classe « normal ») et régénérer la librairie.

**Idée de démo** : installer la sentinelle devant un stand avec 3 objets, collecter « normal » avec les
3 objets en place et « anomalie » en retirant, ajoutant ou déplaçant des objets et en passant devant. En
surveillance : on retire un objet → alarme et `-` à sa place ; on le remet → retour à la normale.

## 7. Réglages pour améliorer la réponse (`NUCLEO_L476RG_TOF_SENTINELLE/Inc/app_config.h`)

Les leviers se rangent en trois familles, selon ce qu'il faut refaire après un changement.

### Réglages de décision : sans recollecter

Ils agissent **après** NanoEdge, sur la façon dont la carte utilise la proba « normal ». Recompiler suffit :
la librairie reste la même. Ce sont les premiers à essayer.

| Paramètre | Défaut | Rôle | Si… |
|---|---|---|---|
| `SENT_THRESHOLD` | 90 | Proba « normal » sous laquelle un signal est anormal ; aussi en direct avec `+` / `-` | Fausses alarmes → diminuer ; anomalies manquées → augmenter |
| `SENT_CONFIRM` | 3 | Signaux consécutifs (~0,5 s chacun) pour lever ou retirer l'alarme | Alarme trop nerveuse → 4 ou 5 ; trop lente → 2 |
| `SENT_MAP_MIN_MM` / `SENT_MAP_SIGMAS` | 120 mm / 4 | Sensibilité de la carte des changements (sans IA) | Carte trop bavarde → augmenter |

### Réglages du signal : recollecter et régénérer la librairie

Ils changent les données envoyées à NanoEdge : une librairie entraînée sur l'ancien signal ne marche plus,
ou mal. Après un changement : recollecter les deux classes, refaire le benchmark, remplacer la librairie.

| Paramètre | Défaut | Rôle | Si… |
|---|---|---|---|
| `SENT_MAX_MM` | 3000 mm | Distance d'écrêtage : au-delà, tout est « loin » | Grande pièce → 4000 |
| `SENT_FRAMES_PER_SIGNAL` | 8 | Trames par signal (512 valeurs) | Réponse plus vive → 4 (256 valeurs : aussi `longueur` dans `tools/profil_collecte.json` et dans le Studio) |
| `TOF_TARGET_CLOSEST` | 0 | Cible la plus forte par zone (défaut du capteur) ; 1 = la plus proche | Objets fins devant un mur mal vus → essayer 1 |

⚠️ `SENT_MAX_MM`, `SENT_FRAMES_PER_SIGNAL` et `TOF_FREQ_HZ` existent aussi dans
`X-CUBE-TOF1/App/app_x-cube-tof1.c` (`MAX_THRESHOLD`, `SUCCESSIVE_MATRIX`, `Profile.Frequency`) : les
modifier **des deux côtés**, sinon un `#error` dit laquelle.

### Les données et le Studio : souvent le levier le plus efficace

- **Collecter sur la scène surveillée**, carte à sa place définitive.
- **« normal » plus varié** : lumière allumée ou éteinte, autres heures, petites variations acceptables.
  Tout ce qui manque dans « normal » déclenchera une alarme.
- **« anomalie » plus variée** : passages, objets ajoutés, retirés ou déplacés, tiroirs, portes.
- **Classes équilibrées**, et **données de test à part** (`--out test`) pour la validation.
- **Matrice de confusion** du benchmark : des normaux classés « anomalie » donnent de fausses alarmes, des
  anomalies classées « normal » des alarmes manquées.
- **1 axe ou 64 axes** dans le Studio : essayer les deux et garder le meilleur benchmark.
- **Diagnostic sur la carte** : la touche `v` affiche la proba « normal » en continu ; la regarder sur la
  scène normale, puis pendant une anomalie, pour choisir `SENT_THRESHOLD`.

`SENT_LEARN_SIGNALS` et `SENT_LEARN_DELAY_MS` ne servent qu'au mode détection d'anomalie (section 9).

## 8. Limites connues

- **Modèle figé** : il ne connaît que la scène et les situations de la collecte. Carte déplacée, scène
  réaménagée : recollecter et régénérer la librairie.
- **Orientation de la carte des changements** : selon le sens de montage, la grille peut apparaître en
  miroir par rapport à ce que tu vois. Repère-toi une fois en passant la main d'un côté.
- **Lumière** : un soleil direct très fort dégrade les mesures du VL53L5CX. Collecter « normal » sous
  différents éclairages.

## 9. Pour aller plus loin : détection d'anomalie avec apprentissage sur la carte

NanoEdge propose aussi des librairies **Anomaly detection**, que le firmware accepte sans modification : il
reconnaît le type de librairie à la compilation. La logique est alors différente :

- la librairie générée ne contient que l'**algorithme**, vierge ; les CSV `normal` / `anomalie` servent
  seulement au benchmark, pour le choisir ;
- c'est la **carte qui apprend sur place** la scène normale : au démarrage, la LED clignote lentement (5 s
  pour sortir du champ), puis reste allumée ~21 s pendant l'apprentissage ;
- la valeur affichée est une **similarité** à la normale apprise ;
- **B1 appui court** (ou `a`) ajoute la situation actuelle à la normale ; **B1 appui long** (ou `l`) efface
  tout et réapprend, par exemple après avoir déplacé la carte.

| | N-Class classification (ce projet) | Anomaly detection |
|---|---|---|
| Ce que contient la librairie | Le modèle appris sur vos CSV | L'algorithme seul, vierge |
| Au démarrage | **Surveillance immédiate** | 5 s pour sortir du champ, puis ~21 s d'apprentissage |
| Valeur affichée | `proba normal` | `similarite` à la normale apprise |
| B1, `l`, `a` | Sans effet | Apprendre ou réapprendre |
| Carte déplacée | Recollecter et régénérer la librairie | B1 long : la carte réapprend |

C'est **le même firmware** : `Src/sentinel_ai.c` regarde le `NanoEdgeAI.h` présent. S'il définit des classes
(`NEAI_NUMBER_OF_CLASSES`), c'est une librairie de classification ; sinon, le firmware appelle les fonctions
de détection d'anomalie (`neai_anomalydetection_init`, `_learn`, `_detect`). Seule la librairie change.

### Passer en détection d'anomalie

1. **Sauvegarder votre librairie de classification** (`Inc/NanoEdgeAI.h`, `Src/libneai.a`) hors du dépôt,
   pour pouvoir y revenir.
2. **Dans NanoEdge AI Studio**, créer un nouveau projet **Anomaly detection** : cible **STM32L476RG**,
   capteur **Generic, 1 axe**. Les mêmes CSV conviennent :
   - *Regular signals* ← `normal.csv` ;
   - *Abnormal signals* ← `anomalie.csv`.

   Ils servent seulement au benchmark, pour choisir l'algorithme : la scène peut être différente de celle
   qui sera surveillée, tant qu'elle est du même genre.
3. **Benchmark** : viser une précision équilibrée d'au moins 90 %, comme en classification.
4. **Validation**, dans l'émulateur : faire un apprentissage avec des signaux normaux, puis une détection
   sur les fichiers de test. Le Studio indique le **nombre minimal d'apprentissages conseillé** : le nombre
   de signaux normaux que la librairie doit apprendre avant de détecter correctement (voir ci-dessous).
5. **Compilation** avec **Float ABI hard** et **FPU fpv4-sp-d16**, puis remplacer `Inc/NanoEdgeAI.h` et
   `Src/libneai.a` par ceux du `.zip`. Il n'y a pas de `knowledge.h`.
6. Dans CubeIDE : **Project > Clean**, puis recompiler en `APP_MODE 1` et flasher.

Pour revenir à la classification : remettre la librairie sauvegardée, puis *Clean* et recompiler.

### Le nombre minimal d'apprentissages conseillé

- **Un apprentissage = un signal** de 8 trames, soit ~0,53 s à 15 mesures/s.
- La carte apprend **`SENT_LEARN_SIGNALS` signaux** (40 par défaut, ~21 s) à chaque démarrage ou appui long
  sur B1, et autant à chaque appui court.
- **Si le Studio conseille plus de 40**, mettre cette valeur (ou un peu plus) dans `SENT_LEARN_SIGNALS`
  (`Inc/app_config.h`). Exemple : 100 apprentissages → ~53 s d'apprentissage au démarrage.
- Si la librairie fournit ce nombre (`MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING` dans `NanoEdgeAI.h`),
  le firmware le lit et prévient à la fin de l'apprentissage s'il n'est pas atteint :
  `Attention : NanoEdge conseille au moins N apprentissages ([a] pour completer)`. Un appui court sur B1
  (ou `a`) ajoute alors `SENT_LEARN_SIGNALS` signaux de plus.
- Trop peu d'apprentissages donne des **fausses alarmes** ; pendant l'apprentissage, la scène doit rester
  normale et personne ne doit passer devant.

### Utilisation

| LED LD2 | État |
|---|---|
| Clignote lentement (1 Hz) | Sors du champ : l'apprentissage commence dans 5 s |
| Allumée fixe | Apprentissage en cours, ne touche à rien |
| Éteinte | Surveillance |
| Clignote vite | **ALARME** |

Le terminal affiche « SENTINELLE - apprentissage embarque », puis une **similarité** au lieu de « proba
normal ». Les commandes `+`, `-`, `v`, `c` et `h` sont les mêmes ; B1 et les touches `a` / `l` servent à
apprendre (tableau plus haut). La connaissance apprise est **perdue à chaque redémarrage** : la carte
réapprend au démarrage.

⚠️ **Ce mode n'a pas encore été essayé sur la carte**, seulement sur PC avec une librairie simulée. Si une
version récente du Studio a changé la forme de ces fonctions (un paramètre de plus à
`neai_anomalydetection_init`, par exemple), la compilation s'arrêtera dans `Src/sentinel_ai.c` : il faudra
adapter l'appel à ce que déclare le `NanoEdgeAI.h` généré.

## 10. Tests sur PC

```bash
cd tests
make test     # signal, carte des changements (objet retiré, personne), alarme avec hystérésis
make synth    # CSV synthétiques pour essayer NanoEdge AI Studio
```
