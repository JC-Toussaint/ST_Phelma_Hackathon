# Différences avec le projet ST d'origine

**Projet de référence** : `NUCLEO_L476RG_TOF_VL53L5A1`, fourni par ST dans
`../ST-exemple/NUCLEO_L476RG_TOF_VL53L5A1.zip` (NUCLEO-L476RG + X-NUCLEO-53L5A1, X-CUBE-TOF1,
librairie NanoEdge AI modèle).
**Projet modifié** : `NUCLEO_L476RG_TOF_PFC/`, une copie complète du projet ST à laquelle s'appliquent les
changements ci-dessous.

En résumé, aucun fichier de `Src/` ni de `Inc/` fourni par ST n'est modifié : l'application est
entièrement **ajoutée**. Les seuls fichiers ST modifiés sont `X-CUBE-TOF1/App/app_x-cube-tof1.c`, qui appelle
désormais l'application à la place de l'exemple ST **tout en gardant ses réglages et leurs commentaires**,
et `app_x-cube-tof1.h` (une déclaration ajoutée). `main.c` est inchangé.

Pour vérifier :

```bash
diff -rq -x Debug -x Release NUCLEO_L476RG_TOF_VL53L5A1 pierre_feuille_ciseaux/NUCLEO_L476RG_TOF_PFC
```

---

## 1. `Inc/`

### Fichiers ajoutés

| Fichier | Rôle |
|---|---|
| `app.h` | Point d'entrée de l'application : `app_init()` et `app_loop()` |
| `app_config.h` | Tous les réglages : mode (`APP_MODE`), débit UART, prétraitement, jeu |
| `tof_sensor.h` | Initialisation et lecture du capteur (au-dessus du BSP X-CUBE-TOF1) |
| `tof_preproc.h` | Type `tof_frame_t` et prétraitement des trames 8×8 (segmentation de la main, recentrage, empilement) |
| `rps_game.h` | Logique du jeu : arbitrage, score, adversaire adaptatif (Markov), vote, engagement anti-triche |
| `rps_ai.h` | Adaptateur vers la librairie NanoEdge (classification N-classes) |
| `uart_io.h` | Sorties texte et CSV sur USART2 (115200 bauds) sans `printf` |

### Fichiers ST inchangés

`main.h`, `NanoEdgeAI.h`, `stm32l4xx_hal_conf.h`, `stm32l4xx_it.h`, `stm32l4xx_nucleo_bus.h` et
`stm32l4xx_nucleo_errno.h` sont inchangés.

> ⚠️ `NanoEdgeAI.h` est la librairie **modèle** de ST (2 classes, 512 valeurs). Elle permet de compiler le
> mode JEU, mais il faut la **remplacer** par la librairie générée pour Pierre-Feuille-Ciseaux
> (3 classes : pierre, feuille, ciseaux). Sinon, `app_init()` s'arrête avec l'erreur « init NanoEdge ».

## 2. `Src/`

### Fichiers ajoutés

| Fichier | Rôle |
|---|---|
| `app.c` | Machine à états. Mode COLLECTE : CSV vers NanoEdge Studio. Mode JEU : compte à rebours, capture, vote, duel, score, commandes B1 et terminal |
| `tof_sensor.c` | Configuration du capteur déléguée à `TOF1_SensorConfig()` (`app_x-cube-tof1.c`, réglages de l'exemple ST). Lecture sur l'interruption INT (`ToF_EventDetected`) avec relecture de secours toutes les 100 ms, conversion des statuts BSP vers la convention ULD |
| `tof_preproc.c` | Prétraitement : fond supprimé, relief 0–100 relatif au point le plus proche, recentrage de la main, fenêtre glissante de 8 trames |
| `rps_game.c` | Logique du jeu, portable et testée sur PC (`tests/`) |
| `rps_ai.c` | Adaptateur NanoEdge : API récente du projet ST (`neai_classification_init(void)`, identifiants à partir de 0, `neai_get_class_name()`) ou ancienne API. Associe les classes aux coups par leur nom |
| `uart_io.c` | `uart_puts`, `uart_put_int`, `uart_put_csv`… ; `uart_io_init()` ne fait rien (USART2 reste à 115200 bauds, `#error` si `APP_UART_BAUD` diffère) |

### Fichiers ST inchangés

`main.c`, `libneai.a`, `stm32l4xx_hal_msp.c`, `stm32l4xx_it.c`, `stm32l4xx_nucleo_bus.c`, `syscalls.c`,
`sysmem.c` et `system_stm32l4xx.c` sont inchangés.

> ⚠️ `libneai.a` est la librairie modèle de ST, à remplacer en même temps que `Inc/NanoEdgeAI.h`.

## 3. En dehors de `Src/` et `Inc/`

| Fichier | Changement |
|---|---|
| `X-CUBE-TOF1/App/app_x-cube-tof1.c` | **Modifié.** La boucle de l'exemple ST est supprimée (boucle infinie dans `MX_53L5A1_ThresholdDetection_Process()`, buffer NanoEdge, `printf("%.1f ")`). `MX_X_CUBE_TOF1_Init()` appelle `app_init()` et `MX_X_CUBE_TOF1_Process()` appelle `app_loop()`, dans des blocs `USER CODE`. **Les réglages de l'exemple sont conservés avec leurs commentaires d'origine et s'appliquent** : grand commentaire sur les critères d'interruption, `SUCCESSIVE_MATRIX`, `MATRIX_RESOLUTION`, `AXES`, `NEAI_BUFFER_SIZE`, `MIN_THRESHOLD`, `MAX_THRESHOLD`, `NEAI_MODE` (les 7 lignes du fichier de ST, actives dans l'original, gardées en commentaire `//#define`, puis les réglages appliqués présentés comme dans `ST-exemple/README.md`), contrôles `#error`, profil (`Profile.*`) et interruption (`ITConfig.*`), appliqués par la nouvelle fonction `TOF1_SensorConfig()`. Différences voulues : `ITConfig.Criteria = RS_IT_DEFAULT` (toutes les trames) au lieu de `RS_IT_IN_WINDOW`, `MIN_THRESHOLD` 40 mm au lieu de 100, `NEAI_MODE` = `APP_MODE`, cible la plus proche. Des `#error` vérifient que ces réglages concordent avec `Inc/app_config.h` (`PRE_FRAMES_PER_SIGNAL`, `TOF_ZONES`, `SIGNAL_LEN`, `PRE_NEAR_MM`/`PRE_FAR_MM`, `TOF_FREQ_HZ`). La variable `ToF_EventDetected`, nécessaire à `app_tof_pin_conf.c`, est conservée |
| `X-CUBE-TOF1/App/app_x-cube-tof1.h` | Déclaration de `TOF1_SensorConfig()` ajoutée |
| `NUCLEO_L476RG_TOF_VL53L5A1.ioc` → `NUCLEO_L476RG_TOF_PFC.ioc` | Renommé. Seuls `ProjectManager.ProjectName` et `ProjectFileName` changent : **aucun périphérique ni broche n'est modifié** |
| `NUCLEO_L476RG_TOF_VL53L5A1.launch` → `NUCLEO_L476RG_TOF_PFC.launch` | Renommé, avec le nom du projet et du `.elf` mis à jour |
| `.project`, `.cproject` | Nom du projet uniquement, pour pouvoir importer PFC et Sentinelle dans le même workspace. Chemins d'include, librairie `neai` et options de compilation inchangés |
| `Drivers/`, `X-CUBE-TOF1/Target/`, `Startup/`, `Tests/`, `*.ld`, `README.md` | Inchangés. Le `README.md` de ST décrit l'exemple d'origine (`NEAI_MODE`, `SUCCESSIVE_MATRIX`…), qui ne s'applique plus : voir `../README.md` |

## 4. Différences de comportement

| | Projet ST | Pierre-Feuille-Ciseaux |
|---|---|---|
| Point d'entrée | `MX_X_CUBE_TOF1_Process()` ne rend jamais la main (`while(1)` interne) | `app_loop()` est non bloquante, appelée en boucle par `main.c` |
| Déclenchement des mesures | Uniquement si une cible est entre 100 et 400 mm (interruption à seuil du capteur) | Mesure continue (les seuils sont désactivés). La présence de la main est décidée par le prétraitement (≥ 5 zones) |
| Choix de la cible par zone | Plus forte (défaut) | **Plus proche** (`TOF_TARGET_CLOSEST`), c'est-à-dire la main plutôt que le plafond |
| Contenu d'un signal | 8 trames × 64 distances **brutes en mm**, 0 hors fenêtre | 8 trames × 64 valeurs **prétraitées 0–100** : relief relatif au point le plus proche, main recentrée |
| Construction des signaux | Blocs disjoints de 8 trames | **Fenêtre glissante** : un nouveau signal à chaque trame (15 signaux/s) ; en collecte, un sur 3 est envoyé (5 signaux/s, voir ci-dessous) |
| Format de collecte | Flottants `%.1f` séparés par des espaces | Entiers séparés par des virgules, et lignes `#` pour les messages (dont `# En attente d'une main…` chaque seconde sans main). Les deux formats sont acceptés par NanoEdge Studio |
| Débit UART | 115200 bauds | **115200 bauds aussi** (USART2, non reconfiguré). Une ligne de collecte (512 valeurs) prenant jusqu'à 180 ms, le firmware n'envoie qu'un signal toutes les 3 trames (`PRE_DATALOG_EVERY`), pour ne manquer aucune trame du capteur |
| Erreurs | `printf` puis `Error_Handler()` | Message `# ERREUR …` répété toutes les 2 s sur l'UART, LED LD2 qui clignote vite |
| Utilisation de NanoEdge | Affiche « Class found: … » toutes les 500 ms | Jeu complet : 5 votes concordants avec une confiance ≥ 60 % |
| Bouton B1, LED LD2 | Non utilisés | B1 lance une manche. LD2 sert au compte à rebours et au résultat |

## 5. Si vous régénérez le code depuis le `.ioc`

CubeMX ne conserve que le contenu des blocs `USER CODE`. Il recréerait l'exemple ST dans
`app_x-cube-tof1.c`, dont la boucle infinie empêcherait `app_loop()` de s'exécuter. Après une
régénération, recopiez la version de ce dépôt.
