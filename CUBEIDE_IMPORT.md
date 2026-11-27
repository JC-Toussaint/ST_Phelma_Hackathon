# Importer un projet dans STM32CubeIDE sans le casser

STM32CubeIDE (bâti sur Eclipse) **réécrit** les fichiers qui décrivent un projet, `.project` et
`.cproject`, au lieu de seulement les lire. S'il ne reconnaît pas un dossier comme « projet STM32 », il ne
s'arrête pas sur une erreur : il le transforme en projet Makefile générique et enregistre ce résultat sur
le disque. Les symptômes ci-dessous en découlent presque tous.

## La bonne façon de faire

1. **Un workspace par dépôt.** Les projets de deux copies du dépôt portent les mêmes noms, et un
   workspace ne peut ouvrir qu'un projet par nom. Pour changer : *File > Switch Workspace > Other…*
2. **Compiler depuis un clone sur le disque Windows** (par exemple `C:\Users\<vous>\Documents\…`), jamais
   depuis `\\wsl.localhost\…` : CubeIDE et `make` gèrent mal ces chemins réseau.
3. **Importer par un seul menu** : *File > Import > General > **Existing Projects into Workspace***
   - *Select root directory* : le dossier du dépôt (ou celui d'un projet) ;
   - **ne pas** cocher « Copy projects into workspace ».
4. **Vérifier** qu'en haut, la configuration proposée est **Debug** (ou Release), et non « Default ».
5. **Contrôler les `.cproject`** : `python3 cubeide_cible.py --check` à la racine du dépôt, ou `make test`
   dans le dossier `tests/` d'un projet, qui fait ce contrôle.

**À éviter** : *File > Open Projects from File System…* et *New > Makefile Project with Existing Code*.
Ces menus devinent la nature du projet, se trompent, et écrasent le `.cproject`.

## Symptômes et corrections

### « Program "make" not found in PATH », configuration « Default », « No ToolChain »

**Cause** : le `.cproject` a été remplacé par celui d'un projet Makefile générique. `.project` est modifié
et `.settings/language.settings.xml` est souvent supprimé.

`git status` dans le dépôt montre ces fichiers comme modifiés (`M`) ou supprimés (`D`).
`cubeide_cible.py --check` affiche : `ECHEC … pas de chaîne d'outils « MCU ARM GCC »`.

**Correction** :
1. Dans CubeIDE : clic droit sur le projet > *Delete*, **sans** cocher « Delete project contents on disk ».
2. Remettre les fichiers du dépôt, dans un terminal à la racine du dépôt :
   ```
   git restore <projet>/NUCLEO_L476RG_TOF_<NOM>
   ```
   Cette commande remet **tous** les fichiers modifiés de ce dossier : mettez d'abord de côté ce que vous
   voulez garder, en particulier votre librairie NanoEdge (`Inc/NanoEdgeAI.h`, `Src/libneai.a`) et vos
   réglages de `Inc/app_config.h`. Pour ne remettre que les fichiers du projet :
   ```
   git restore <projet>/NUCLEO_L476RG_TOF_<NOM>/.cproject <projet>/NUCLEO_L476RG_TOF_<NOM>/.project <projet>/NUCLEO_L476RG_TOF_<NOM>/.settings
   ```
3. Réimporter par *Existing Projects into Workspace*, après avoir vérifié qu'aucun projet du même nom
   n'est ouvert dans ce workspace.

### « Not able to get mcu target » au débogage ou au flash

**Cause** : à l'import, CubeIDE a vidé les options *MCU*, *CPU*, *Core*, *FPU* et *Float ABI*. La
compilation passe, mais le débogage échoue.

**Correction** :
1. Dans CubeIDE : clic droit sur le projet > *Close Project* (sinon CubeIDE réécrit le fichier).
2. À la racine du dépôt : `python3 cubeide_cible.py` (ou `python cubeide_cible.py` sous Windows).
3. Clic droit > *Open Project*, puis déboguer.

### « Cannot find the specified linker script », ou chemin `\\WSL.LOCALHOST\…` tronqué

**Cause** : le script de liaison est désigné par un chemin relatif au workspace, ou le projet est compilé
depuis `\\wsl.localhost`.

**Correction** : `python3 cubeide_cible.py` (projet fermé, comme ci-dessus) remplace ce chemin par
`${ProjDirPath}/STM32L476RGTX_FLASH.ld`. Compiler depuis un clone sur le disque Windows.

### L'import refuse le projet, ou n'en importe qu'une partie

**Cause** : un projet du même nom est déjà dans le workspace, souvent celui d'une autre copie du dépôt.
L'import échoue alors en silence, ou retombe sur un import générique qui casse le projet.

**Correction** : utiliser un workspace propre à ce dépôt, ou fermer puis supprimer (*Delete*, sans effacer
les fichiers) l'autre projet du même nom avant d'importer.

### Après une compilation, `git status` montre `language.settings.xml` ou `*.launch` modifiés

**Cause** : CubeIDE y enregistre l'état de la machine (options détectées, fenêtres de débogage).

**Correction** : rien à corriger ; ne pas commiter ces fichiers. Pour revenir à la version du dépôt :
`git restore <le fichier>`.

## Pourquoi `cubeide_cible.py`

Ce script remet dans chaque `.cproject` la puce (STM32L476RGTx pour les projets NUCLEO-L476RG), le FPU,
la Float ABI, le chemin du script de liaison et la compilation parallèle, sans toucher au reste du fichier.
Il ne répare pas un `.cproject` remplacé par un projet générique : celui-là se remet avec `git restore`,
comme indiqué plus haut. `--check` ne modifie rien et signale ce qui est à corriger.
