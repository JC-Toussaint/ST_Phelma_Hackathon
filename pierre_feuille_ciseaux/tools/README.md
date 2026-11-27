# Outils PC de Pierre-Feuille-Ciseaux, et comment écrire un `profil_collecte.json`

```
tools/
├── profil_collecte.json   ce que l'outil de collecte doit savoir de ce projet (expliqué ci-dessous)
├── rps_logger_gui.py      collecte en interface graphique   (lanceur de outils/collecte_nanoedge.py)
├── rps_logger.py          collecte dans le terminal         (même outil, mode --cli / --view)
├── augmente_pfc.py        augmentation des données réelles (paragraphe 5)
├── pierre.csv, feuille.csv, ciseaux.csv   les données collectées, une ligne par signal
├── demo/                  signaux simulés de --demo (ignoré par git)
└── augmente*/             données augmentées : entrainement/ et test/ (ignoré par git)
```

Les deux scripts ne contiennent presque rien. Ils lancent l'**outil de collecte commun au dépôt**
(`outils/collecte_nanoedge.py`, voir [outils/README.md](../../outils/README.md)) avec le profil de ce
projet. L'outil commun ne connaît aucun projet : **tout ce qui est propre à Pierre-Feuille-Ciseaux est
dans `profil_collecte.json`**. Ce guide explique comment ce fichier a été écrit, champ par champ, à partir
du firmware. On peut suivre la même démarche pour un autre projet.

---

## 1. Le principe

En mode collecte (`APP_MODE_DATALOG`), la carte envoie deux sortes de lignes sur la liaison série :

```
# En attente d'une main (15 trames/s, 0 zones entre 40 et 200 mm, il en faut 5)
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,57,82,61,0,0, ... ,0
```

- les lignes qui commencent par `#` sont des **messages** : l'outil les affiche ;
- les autres sont des **signaux** : une ligne CSV de nombres = un exemple pour NanoEdge.

L'outil enregistre chaque signal dans le fichier de la classe choisie (`pierre.csv`…). Pour cela, il doit
savoir :

| Question | Champ du profil |
|---|---|
| Quelles sont les classes, et donc les fichiers ? | `classes` |
| Combien de nombres compte une ligne valide ? | `longueur` |
| Comment afficher un signal pour vérifier qu'il est bon ? | `affichage`, `valeur_max` |
| À quel débit écoute la carte ? | `bauds` |
| Où ranger les fichiers ? | `dossier` |
| Quel message la carte répète-t-elle quand il n'y a rien à mesurer ? | `attente` |
| Le reste : titre, conseil, taille d'une passe, rythme de la démo | `nom`, `conseil`, `par_passe`, `demo_signaux_par_s` |

---

## 2. Le profil de ce projet, champ par champ

```json
{
  "nom": "Pierre-Feuille-Ciseaux",
  "classes": ["pierre", "feuille", "ciseaux"],
  "longueur": 512,
  "affichage": {"type": "grille", "taille": 8, "trames": 8, "min": 20, "miroir_gauche_droite": true, "miroir_haut_bas": false},
  "valeur_max": 100,
  "par_passe": 300,
  "bauds": 115200,
  "dossier": ".",
  "attente": "En attente",
  "conseil": "pendant une passe, varie la hauteur (10–20 cm), la position, la rotation et la main. Fais 2–3 passes par classe, avec plusieurs personnes.",
  "demo_signaux_par_s": 5
}
```

### `classes` : les noms des classes

```json
"classes": ["pierre", "feuille", "ciseaux"]
```

- Chaque nom devient un fichier : `pierre` → `pierre.csv`.
- Dans NanoEdge AI Studio, on importe un fichier par classe ; **le Studio garde le nom du fichier comme
  nom de classe**, et ce nom finit dans la librairie générée.
- En mode jeu, le firmware retrouve chaque coup **par ce nom**
  (`NUCLEO_L476RG_TOF_PFC/Src/rps_ai.c`, fonction `name2move`). Il accepte `pierre` ou `rock`, `feuille`
  ou `paper`, `ciseau(x)` ou `scissor(s)`, sans tenir compte des majuscules. **Avec un autre nom, la
  carte s'arrête au démarrage du jeu** sur `init NanoEdge (classes pierre/feuille/ciseaux introuvables ?)`.
- Interdit : `/ \ : * ? " < > |`, les espaces au début ou à la fin, et deux classes de même nom. L'outil
  refuse le profil avec un message clair.
- L'ordre de la liste donne les raccourcis clavier : `1` = pierre, `2` = feuille, `3` = ciseaux.

### `longueur` : le nombre de valeurs d'un signal

```json
"longueur": 512
```

Elle se lit dans `NUCLEO_L476RG_TOF_PFC/Inc/app_config.h` :

```c
#define TOF_ZONES              (TOF_GRID * TOF_GRID)                /* 8 x 8 = 64   */
#define PRE_FRAMES_PER_SIGNAL  8
#define SIGNAL_LEN             (TOF_ZONES * PRE_FRAMES_PER_SIGNAL)  /* 512 valeurs  */
```

`app.c` envoie un signal par `uart_put_csv(nn_signal, SIGNAL_LEN)`, d'où **512**. C'est aussi la taille
qu'attend la librairie NanoEdge : projet Studio « Generic », 1 axe, longueur 512.

L'outil **ignore toute ligne qui n'a pas cette longueur** et le signale (« Ligne ignorée : 64 valeurs au
lieu de 512 »). Si ce message apparaît sans arrêt : la carte n'est pas dans le mode attendu, ou
`PRE_FRAMES_PER_SIGNAL` a changé sans que le profil suive. `0` désactive la vérification ; c'est
déconseillé, une ligne coupée finirait alors dans les données.

> **Si vous changez `PRE_FRAMES_PER_SIGNAL`**, changez aussi `longueur` et `affichage.trames` (et
> `SUCCESSIVE_MATRIX` dans `X-CUBE-TOF1/App/app_x-cube-tof1.c`, que le firmware vérifie lui-même), puis
> recréez le projet dans le Studio : une librairie attend toujours une longueur fixe.

### `affichage` : comment montrer un signal

```json
"affichage": {"type": "grille", "taille": 8, "trames": 8, "min": 20}
```

Un signal de PFC, c'est **8 trames du capteur 8×8 mises bout à bout**. La trame la plus ancienne vient en
premier ; dans une trame, les zones vont ligne par ligne, de gauche à droite. L'affichage `grille`
correspond exactement à ce format :

| Clé | Valeur | D'où elle vient |
|---|---|---|
| `type` | `"grille"` | capteur matriciel |
| `taille` | `8` | `TOF_GRID` (8×8 zones) |
| `trames` | `8` | `PRE_FRAMES_PER_SIGNAL` |
| `min` | `20` | plus petite valeur d'une zone de main : voir ci-dessous |
| `miroir_gauche_droite` | `true` | échange gauche et droite à l'écran (symétrie par rapport à l'axe vertical de la grille). Le capteur voit la main par en dessous : sans ce miroir, la grille paraît inversée pour qui regarde la carte par au-dessus |
| `miroir_haut_bas` | `false` | échange haut et bas à l'écran (symétrie par rapport à l'axe horizontal), si cela paraît inversé ; dépend de l'orientation de la carte |

Les miroirs ne changent **que l'affichage** : les signaux enregistrés restent ceux de la carte, et la
librairie NanoEdge reste valable. Les cases « miroir ↔ » et « miroir ↕ » sous la grille permettent de
trouver le bon réglage en direct : déplacer la main vers sa droite puis vers l'avant, cocher ce qu'il faut
pour que l'écran suive, puis reporter le résultat dans le profil.

L'outil vérifie que `taille × taille × trames = longueur` (8 × 8 × 8 = 512). Sinon, il refuse le profil
en donnant le calcul.

**Pourquoi `min` = 20.** Le prétraitement (`Src/tof_preproc.c`) donne 0 au fond, et à une zone de main une
valeur de **20 à 100** : 100 pour le point le plus proche, 20 pour une zone située à
`PRE_DEPTH_WINDOW_MM` derrière lui. `min` sert à deux choses :
- les couleurs de la grille vont de `min` (violet) à `valeur_max` (jaune), et 0 reste noir ;
- le compteur « N zones actives » sous la grille compte les zones entre `min` et `valeur_max`.

L'interface montre en grand la trame la plus récente et, en dessous, les 8 trames du signal en vignettes.

### `valeur_max`

```json
"valeur_max": 100
```

C'est la plus grande valeur possible d'une mesure : elle fixe l'échelle des couleurs. Ici, le
prétraitement s'arrête à 100, et `uart_put_csv()` écrête lui aussi à 0..100.

### `bauds`

```json
"bauds": 115200
```

C'est le débit d'USART2, tel que l'initialise `main.c` du projet ST (`APP_UART_BAUD` dans `app_config.h`,
contrôlé par un `#error`). Un débit faux donne des caractères illisibles : aucune ligne n'est alors
reconnue, ni message ni signal.

### `dossier`

```json
"dossier": "."
```

C'est le dossier des CSV, **relatif au fichier de profil**. `"."` désigne donc `tools/`, à côté des
scripts, quel que soit le dossier d'où on les lance. On peut en mettre un autre (`"donnees"`), ou le
choisir au lancement avec `--out` ou dans le champ « Dossier » de l'interface.

En démo (`--demo`), les fichiers vont dans le sous-dossier `demo/`. Ainsi, des signaux simulés ne se
mêlent jamais aux vraies données.

### `attente`

```json
"attente": "En attente"
```

Sans main, la carte répète chaque seconde `# En attente d'une main (...)`. Les messages qui commencent par
ce texte **remplacent** la ligne précédente dans la fenêtre, au lieu de remplir l'historique. Si votre
firmware répète un autre message, mettez son début ici.

### `par_passe`

```json
"par_passe": 300
```

C'est le nombre de signaux enregistrés par passe, modifiable aussi dans l'interface (« Signaux par
passe ») ou avec `--count`. La carte envoie 5 signaux par seconde (`PRE_DATALOG_EVERY` = 3 trames à
15 Hz) : 300 signaux prennent donc environ **60 s**.

### `conseil`, `nom`, `demo_signaux_par_s`

- `conseil` : le texte affiché sous les boutons pendant la collecte. On y rappelle comment varier les
  exemples (hauteur, position, rotation, personnes).
- `nom` : le titre de la fenêtre.
- `demo_signaux_par_s` : le rythme des signaux simulés de `--demo`. Il vaut 5, comme la carte, pour que la
  démo donne une idée juste de la durée d'une passe.

---

## 3. Vérifier un profil

1. **Lecture** : lancer `python rps_logger_gui.py --demo`. Si le profil a un défaut, l'outil s'arrête avec
   un message qui nomme le champ :

   | Message | Correction |
   |---|---|
   | `JSON invalide (ligne 5)` | virgule en trop ou manquante, guillemet oublié |
   | `« classes » : liste non vide de noms` | `classes` absent ou vide |
   | `« classes » : un nom apparaît deux fois` | supprimer le doublon |
   | `grille : 8 trames de 8x8 = 512 valeurs, mais « longueur » = 500` | accorder `longueur` et `affichage` |
   | `« affichage.type » : un de ('grille', 'courbes', 'brut')` | type mal orthographié |

2. **Démo** : la grille doit s'animer, les vignettes défiler, et les fichiers aller dans `tools/demo/`.
3. **Carte** (en `APP_MODE_DATALOG`) : `python rps_logger.py --port COM5 --view`, puis passer la main. La
   grille doit suivre la main. Aucun message `[ligne ignorée] …` ne doit apparaître (« Ligne ignorée… »
   dans l'interface) : sinon, `longueur` est fausse.

---

## 4. Écrire le profil d'un autre projet

La même démarche s'applique à tout firmware qui envoie des lignes `#` et des lignes CSV :

1. **Classes** : les noms que le firmware attend de la librairie NanoEdge. Les chercher dans son
   adaptateur NanoEdge, comme `rps_ai.c` ici.
2. **Longueur** : la taille passée à la fonction qui envoie le CSV (ici `uart_put_csv(…, SIGNAL_LEN)`).
3. **Affichage**, selon la nature du signal :

   ```json
   {"type": "grille", "taille": 8, "trames": 1, "min": 1}
   ```
   pour une seule trame 8×8 (64 valeurs) ;

   ```json
   {"type": "courbes", "axes": ["x", "y", "distance", "surface"], "instants": 32, "ordre": "entrelace"}
   ```
   pour 4 grandeurs suivies sur 32 instants (128 valeurs), envoyées `x0,y0,d0,s0,x1,y1…`.
   Avec `"ordre": "par_axe"`, l'envoi est `x0…x31,y0…y31…` ;

   ```json
   {"type": "brut"}
   ```
   pour tout le reste : les valeurs sont tracées telles quelles.

4. **Débit** : celui de l'UART du firmware.
5. Vérifier comme au paragraphe 3, puis, si l'on veut une commande courte, copier `rps_logger_gui.py` dans
   le projet : il suffit d'y changer le chemin du profil.

La référence complète des champs et des affichages est dans [outils/README.md](../../outils/README.md).

---

## 5. Augmenter les données réelles : `augmente_pfc.py`

Collecter coûte du temps : 300 signaux par classe et par passe. `augmente_pfc.py` fabrique, à partir de
chaque signal réel, des **variantes plausibles**, pour que NanoEdge voie plus de situations (autre main,
main tournée, geste plus lent…) sans collecte supplémentaire.

```bash
pip install numpy
python augmente_pfc.py                                  # lit tools/<classe>.csv
python augmente_pfc.py --facteur 5 --reserve 0.2 --graine 1
python augmente_pfc.py --entree D:/donnees_pfc          # données rangées ailleurs
```

**Ce qu'il fait à chaque signal** (8 trames 8×8) :

| Variation | Ce qu'elle imite | Réglage par défaut |
|---|---|---|
| rotation dans le plan, même angle pour les 8 trames, petite dérive d'une trame à l'autre | main tournée | ±25°, dérive 3° |
| miroir gauche/droite | l'autre main | 1 fois sur 2 |
| relief plus ou moins marqué | main plus ou moins inclinée ou épaisse | ±25 % |
| trames rééchantillonnées | geste un peu plus rapide ou plus lent | ±25 % |
| bruit, zones perdues | bruit de mesure, statut invalide d'une zone | 3 points, 2 % des zones |

La rotation **conserve les surfaces** : chaque zone est découpée en 6 × 6 points tournés un à un, comme une
zone du capteur qui mesure sur toute sa surface.

**Chaque variante est remise aux règles de la carte**, celles de `Src/tof_preproc.c` : dans chaque trame,
la zone la plus proche vaut 100, une zone de main vaut au moins 20 et le fond 0 ; il faut au moins
`PRE_MIN_HAND_ZONES` zones ; la main est recentrée comme le fait le firmware ; les valeurs sont entières.
Une variante que la carte n'aurait jamais envoyée est écartée. Les variantes ont donc exactement les
propriétés des vrais signaux.

**Ce qu'il écrit**, sans jamais modifier les données réelles :

```
augmente/
├── entrainement/<classe>.csv   signaux réels (80 %) + leurs variantes (5 par signal), mélangés
└── test/<classe>.csv           signaux réels mis de côté (20 %), intacts, jamais augmentés
```

Le partage est fait **avant** l'augmentation. Aucune variante d'un signal de test ne se retrouve donc dans
l'entraînement : le test mesure vraiment la reconnaissance de signaux jamais vus. Avec la même `--graine`,
on obtient les mêmes fichiers.

### Vérifier que l'augmentation aide, dans NanoEdge AI Studio

Préparer les deux jeux, **avec la même graine** (par défaut 1) :

```bash
python augmente_pfc.py --facteur 0 --sortie augmente_ref   # réels d'entraînement seuls (référence)
python augmente_pfc.py                                     # réels d'entraînement + variantes
```

Les deux commandes mettent de côté **exactement les mêmes signaux de test** : le partage ne dépend que de
la graine et de la classe, pas du nombre de variantes.

1. **Projet A (référence)** : importer `augmente_ref/entrainement/<classe>.csv`, lancer le Benchmark.
2. **Projet B** : importer `augmente/entrainement/<classe>.csv` (réels + variantes), lancer le Benchmark.
3. Dans l'**Emulator** de chaque projet, importer les mêmes `augmente/test/<classe>.csv` et comparer le
   taux de bonne reconnaissance. C'est ce chiffre, sur de vrais signaux jamais vus, qui compte ; le score
   du Benchmark, lui, est calculé sur des données qui contiennent des variantes.
4. Si B fait mieux, générer la librairie de B. Sinon, réduire les variations (`--facteur 2`, ou les
   réglages cachés `--rotation 15 --relief 0.15`), ou collecter plus de vraies données.

**Limites** : l'augmentation ne crée pas de situation absente des données réelles, comme une hauteur
jamais collectée ou une autre lumière ambiante. Elle multiplie les variations de ce qui a été vu. Pour les
conditions nouvelles, il faut collecter.

