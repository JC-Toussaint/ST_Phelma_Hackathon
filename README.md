# Hackathon ST × Phelma 2026 — ToF multizone, accéléromètre + NanoEdge AI

Quatre objets intelligents construits avec l'**IA embarquée NanoEdge AI** de STMicroelectronics : deux avec
un **capteur de distance ToF 8×8**, deux avec un **accéléromètre**. Sans caméra, sans cloud, tout
se calcule dans le microcontrôleur.

**Matériel** : carte **NUCLEO-L476RG** (STM32L476, Cortex-M4 80 MHz) + une carte d'extension :
**X-NUCLEO-53L5A1** (capteur ToF **VL53L5CX**, 64 zones, 15 mesures/s) pour Pierre-Feuille-Ciseaux et la
Sentinelle, **X-NUCLEO-IKS01A2** (accéléromètre + gyroscope **LSM6DSL**) ou **X-NUCLEO-IKS01A3** (**LSM6DSO**)
pour la Baguette magique, **X-NUCLEO-IKS01A3** pour la Table qui écoute.

| Projet | Idée | IA NanoEdge | Outils PC |
|---|---|---|---|
| [**Pierre-Feuille-Ciseaux**](pierre_feuille_ciseaux/README.md) | Jouer contre le STM32 avec la main au-dessus du capteur | Classification (pierre, feuille, ciseaux) | Collecte (graphique et ligne de commande), augmentation des données |
| [**Sentinelle**](sentinelle/README.md) | Surveiller une scène et signaler tout ce qui n'est pas « normal », en montrant où la scène a changé | Classification (normal, anomalie) | Collecte (graphique et ligne de commande) |
| [**Baguette magique**](baguette/README.md) | Reconnaître des gestes dans l'air avec la carte tenue comme une baguette | Classification (cercle, zigzag, frappe, V, autre), librairie d'essai fournie | Collecte (graphique et ligne de commande), augmentation des données |
| [**La table qui écoute**](table/README.md) | Taper sur une table ordinaire : la carte, collée dessous, reconnaît la zone tapée | Classification (zone1…zone4, autre), librairie d'essai fournie | Collecte (graphique et ligne de commande), piano |

Les projets partent des exemples de ST ([`ST-exemple/`](ST-exemple/README.md)) : le projet ToF fourni pour
le hackathon, et l'application DataLogTerminal de X-CUBE-MEMS1 pour la baguette et la table.
La fiche [`idees_tof_nanoedge.md`](idees_tof_nanoedge.md) rassemble d'autres idées de projets avec ce
capteur, avec les cartes accéléromètre X-NUCLEO-IKS01A2 ou IKS01A3, et avec un microphone.

---

## Les quatre projets en bref

### ✊✋✌️ Pierre-Feuille-Ciseaux
Le capteur, posé à plat, regarde la main. Le STM32 reconnaît le geste et joue sa propre main :
- un **adversaire adaptatif** apprend tes habitudes et joue ce qui les bat ;
- une **empreinte publiée avant la partie** prouve qu'il ne triche pas ;
- un mode « direct » affiche la probabilité de chaque classe, pour voir quand le modèle hésite ;
- **pas à pas** pour une première fois : [`pierre_feuille_ciseaux/DEMARRAGE.md`](pierre_feuille_ciseaux/DEMARRAGE.md).

### 🛡️ Sentinelle
Posée face à un bureau, une étagère ou une porte, la carte reconnaît une scène **normale** d'une
**anomalie** (quelqu'un qui passe, un objet ajouté, retiré ou déplacé), grâce à un modèle appris dans
NanoEdge AI Studio sur des exemples des deux classes :
- l'**alarme est expliquée** par une carte 8×8 qui montre ce qui est apparu ou a disparu ;
- elle surveille **seule, dès la mise sous tension**, sans PC ;
- **pas à pas** pour une première fois : [`sentinelle/DEMARRAGE.md`](sentinelle/DEMARRAGE.md).

### 🪄 Baguette magique
La carte, tenue en main, devient une baguette : le STM32 reconnaît un **cercle**, un **Z**, une **frappe**
ou un **V** tracés dans l'air, à partir de 6 courbes (accélération et rotation sur 3 axes) :
- des **signaux dans le temps**, le point fort de NanoEdge, alignés sur le début du geste ;
- une classe **autre** pour que les mouvements quelconques ne soient pas pris pour des gestes ;
- une **librairie d'essai fournie** (10 gestes par classe) pour essayer tout de suite, avant de collecter
  les vôtres ;
- **pas à pas** pour une première fois : [`baguette/DEMARRAGE.md`](baguette/DEMARRAGE.md).

### 🎹 La table qui écoute
La carte est collée **sous** une table ; on tape du doigt sur des gommettes, et le STM32 reconnaît la zone
tapée, rien qu'aux vibrations qui traversent le plateau. Une note par zone, et la table devient un piano :
- un signal **très bref** (≈ 0,15 s) à **1667 échantillons par seconde**, lus dans la FIFO du capteur ;
- une position impossible à calculer simplement, mais que NanoEdge apprend à partir d'exemples ;
- essayée sur une planche, capteur à une extrémité : 3 zones se distinguent facilement ;
- **pas à pas** pour une première fois : [`table/DEMARRAGE.md`](table/DEMARRAGE.md).

---

## Organisation du dépôt

```
.
├── ST-exemple/                 projets de référence (zip : capteur ToF, IKS01A2, IKS01A3) et README de ST
├── pierre_feuille_ciseaux/     ┐  un dossier par projet, chacun avec :
├── sentinelle/                 │    NUCLEO_L476RG_TOF_<NOM>/  projet STM32CubeIDE complet
├── baguette/                   │      (NUCLEO_L476RG_IKS01A2_BAGUETTE/ et _IKS01A3_BAGUETTE/ pour la baguette)
├── table/                      ┘      (NUCLEO_L476RG_IKS01A3_TABLE/ pour la table)
│                                    tools/                    outils PC en Python (tkinter)
│                                    tests/                    tests sur PC (make test)
│                                    README.md                 guide complet du projet
│                                    DIFFERENCES_ST.md         différences avec le projet ST d'origine
├── outils/                     collecte NanoEdge commune (collecte_nanoedge.py + un profil JSON par projet)
├── cubeide_cible.py            remet la cible MCU des projets CubeIDE (« Not able to get mcu target »)
├── CUBEIDE_IMPORT.md           importer un projet dans CubeIDE sans le casser, et corriger les erreurs
└── idees_tof_nanoedge.md       idées de projets : capteur ToF, accéléromètre (IKS01A2 / IKS01A3), microphone
```

**Base commune** : chaque projet ToF est une **copie du projet fourni par ST**
`NUCLEO_L476RG_TOF_VL53L5A1`, renommée pour que les projets puissent cohabiter dans le même workspace (la
baguette part de même de `NUCLEO_L476RG_IKS01A2.zip` et `NUCLEO_L476RG_IKS01A3.zip` : voir son `DIFFERENCES_ST.md`).
- **Aucun fichier `Src/` ou `Inc/` de ST n'est modifié** : l'application est ajoutée dans de nouveaux
  fichiers.
- **Seul `X-CUBE-TOF1/App/app_x-cube-tof1.c` change vraiment** : il appelle `app_init()` et `app_loop()` à
  la place de l'exemple, et **garde les réglages du capteur de ST avec leurs commentaires**
  (`app_x-cube-tof1.h` y gagne une déclaration).
- Le détail figure dans le `DIFFERENCES_ST.md` de chaque projet.

Le projet ST d'origine est fourni **zippé** dans `ST-exemple/NUCLEO_L476RG_TOF_VL53L5A1.zip`, avec le
`README.md` de ST pour le hackathon. Décompressé à la racine du dépôt, il est ignoré par git (voir
`.gitignore`). Ce README parle du projet « NUCLEO_F401RE_TOF_VL53L5A1 » et renvoie à des images absentes
(`img/`) : le projet fourni est bien celui de la NUCLEO-L476RG. Ses réglages sont **actifs** dans
`app_x-cube-tof1.c` (avec `NEAI_MODE 1`), alors que le README les montre avec `NEAI_MODE 0`.

Pour un projet avec un accéléromètre, `ST-exemple/` contient aussi l'application **DataLogTerminal** du pack
X-CUBE-MEMS1, générée pour la NUCLEO-L476RG : `NUCLEO_L476RG_IKS01A2.zip` (X-NUCLEO-IKS01A2, pack 9.6.0)
et `NUCLEO_L476RG_IKS01A3.zip` (X-NUCLEO-IKS01A3, pack 11.2.0), bases des deux projets de la baguette. Voir
`idees_tof_nanoedge.md`.

## Démarrage rapide

0. **Logiciels à installer**, si possible **avant** le hackathon (téléchargements et installations
   prennent du temps) :
   - **NanoEdge AI Studio**, le logiciel gratuit de ST qui génère la librairie d'IA (Windows ou Linux) ;
   - **STM32CubeIDE**, pour compiler et flasher le firmware ;
   - **Python 3**, puis `pip install pyserial`, pour les outils de collecte ;
   - un terminal série (Tera Term, PuTTY…).

   Les deux logiciels de ST se téléchargent sur st.com, en général avec un compte myST gratuit : le créer
   à l'avance. Lancer NanoEdge AI Studio une fois après l'installation, pour vérifier qu'il démarre.
   Cloner ensuite ce dépôt **sur le disque Windows** (par exemple dans `Documents`).
1. **Matériel** : enficher la carte d'extension du projet (X-NUCLEO-53L5A1, ou X-NUCLEO-IKS01A2 / IKS01A3 pour la
   baguette, X-NUCLEO-IKS01A3 pour la table) sur la NUCLEO-L476RG, puis brancher le câble USB (ST-LINK).
   ⚠️ **Sortir les cartes de leur blister avec précaution** : les broches des connecteurs, sous les cartes,
   sont pointues et peuvent piquer les doigts. Tenir les cartes par les bords.
2. **Firmware** : dans STM32CubeIDE, *File > Import > Existing Projects into Workspace*, puis choisir le
   dossier `NUCLEO_L476RG_<…>/` du projet. Régler le mode dans `app_config.h` (`Inc/`, ou `Core/Inc/` pour
   la baguette et la table), compiler et flasher.

   > 📄 **Lire [`CUBEIDE_IMPORT.md`](CUBEIDE_IMPORT.md)** avant le premier import : le seul menu à utiliser,
   > un workspace par dépôt, et la correction de chaque erreur d'import.
   >
   > ⚠️ **« Not able to get mcu target »** au débogage, ou **« Program "make" not found »** : CubeIDE a
   > régénéré le `.cproject` à l'import et perdu la cible MCU. Importer **toujours** par *Existing
   > Projects into Workspace* (jamais *New > Makefile Project with Existing Code*). Si l'erreur apparaît :
   > clic droit sur le projet > *Close Project*, puis `python3 cubeide_cible.py` à la racine du dépôt,
   > puis *Open Project*. Le script remet la puce, le FPU, le script de liaison et la compilation
   > parallèle dans tous les projets.
   >
   > Sous Windows, compiler depuis un clone sur le disque Windows (par exemple dans `Documents`), pas
   > depuis `\\wsl.localhost` : CubeIDE gère mal ces chemins réseau.
3. **PC** : `pip install pyserial`, puis lancer l'outil du projet. Les outils graphiques ont un mode
   **`--demo`** qui fonctionne **sans carte** :

   ```bash
   python pierre_feuille_ciseaux/tools/rps_logger_gui.py --demo
   python sentinelle/tools/sentinel_logger_gui.py --demo
   python baguette/tools/wand_logger_gui.py --demo
   python table/tools/table_logger_gui.py --demo
   ```

| Projet | Mode par défaut (`APP_MODE`) | Autre mode | Débit série |
|---|---|---|---|
| Pierre-Feuille-Ciseaux | `0` collecte | `1` jeu (librairie NanoEdge) | 115200 bauds |
| Sentinelle | `0` collecte | `1` sentinelle (librairie NanoEdge) | 115200 bauds |
| Baguette magique | `0` collecte | `1` reconnaissance (librairie d'essai fournie, ou la vôtre) | 115200 bauds |
| La table qui écoute | `0` collecte | `1` reconnaissance (librairie d'essai fournie, ou la vôtre) | 115200 bauds |

## Démarche NanoEdge AI (commune aux quatre projets)

1. **Collecter** des exemples avec le firmware en mode `0` et l'outil graphique du projet : un fichier CSV
   par classe, enregistré à côté du script. L'outil est commun aux quatre projets
   ([`outils/`](outils/README.md)) ; chaque projet le décrit par un profil JSON.
2. **Générer la librairie** dans **NanoEdge AI Studio**, le logiciel gratuit de ST :
   - type de projet : *N-Class classification* ;
   - cible : STM32L476RG ;
   - lancer le benchmark, puis compiler avec **Float ABI hard** et **FPU fpv4-sp-d16**.
3. **Intégrer** : remplacer `NanoEdgeAI.h` et `libneai.a` dans le projet CubeIDE, puis passer en mode
   NanoEdge. Pour les deux projets ToF, le dépôt contient la librairie modèle de ST : tant qu'elle n'est pas
   remplacée par la vôtre, le mode NanoEdge s'arrête sur « init NanoEdge ». La baguette et la table sont
   fournies avec une librairie d'essai qui fonctionne, à remplacer par celle de vos propres données.

Les étapes détaillées, les noms de classes à respecter et les messages d'erreur possibles figurent dans la
section « Générer la librairie NanoEdge AI » de chaque README. `cd <projet>/tests && make synth` génère des
CSV synthétiques pour essayer le parcours du Studio sans matériel.

## Tests

Le code portable de chaque projet (prétraitement, logique de jeu ou de surveillance) et l'outil de
collecte sont testés sur PC :

```bash
cd <projet>/tests && make test
```

> Les firmwares ont été compilés sur PC avec les en-têtes réels du projet ST, et les outils PC ont été
> testés en mode démo. Les quatre projets ont été essayés sur la carte : Pierre-Feuille-Ciseaux (collecte,
> jeu, probabilités en direct) et la Sentinelle (collecte, surveillance avec une librairie de classification
> `normal` / `anomalie`), la Baguette magique (collecte, reconnaissance avec la librairie d'essai) et la Table qui écoute (sur une
> planche, 3 zones distinguées facilement). La
> variante Anomaly detection de la Sentinelle (apprentissage sur la carte,
> « Pour aller plus loin ») reste à valider sur la carte.

## À savoir

- **Port série** : le terminal (Tera Term…) et les outils PC ne peuvent pas ouvrir le port COM en même
  temps. Il faut en fermer un avant d'utiliser l'autre.
- **Régénération CubeMX** : ne pas régénérer le code depuis le `.ioc` sans précaution (voir la section 5 des
  `DIFFERENCES_ST.md`).
- **Votre librairie NanoEdge** (`NanoEdgeAI.h`, `libneai.a`) n'existe que dans votre copie : gardez-en une
  sauvegarde hors du dépôt, car un `git stash` ou un `git checkout` peut la remplacer par la librairie
  modèle.

## Licence

Le code écrit pour ce dépôt (application, outils Python, tests, documentation) est sous **licence MIT**
(voir [`LICENSE`](LICENSE)). Les fichiers de STMicroelectronics (HAL, BSP, X-CUBE-TOF1, pilote VL53L5CX,
code généré, projet de `ST-exemple/`), CMSIS et la librairie NanoEdge AI gardent leurs propres licences,
indiquées dans leurs en-têtes ou les fichiers `LICENSE` de leurs dossiers.
