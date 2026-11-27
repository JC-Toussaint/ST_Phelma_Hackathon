# Différences avec le projet ST d'origine

**Projet de référence** : `NUCLEO_L476RG_TOF_VL53L5A1`, fourni par ST dans
`../ST-exemple/NUCLEO_L476RG_TOF_VL53L5A1.zip` (NUCLEO-L476RG + X-NUCLEO-53L5A1, X-CUBE-TOF1,
librairie NanoEdge AI modèle).
**Projet modifié** : `NUCLEO_L476RG_TOF_SENTINELLE/`, une copie complète du projet ST à laquelle
s'appliquent les changements ci-dessous.

En résumé, aucun fichier de `Src/` ni de `Inc/` fourni par ST n'est modifié : l'application est
entièrement **ajoutée**. Les seuls fichiers ST modifiés sont `X-CUBE-TOF1/App/app_x-cube-tof1.c`, qui appelle
désormais l'application à la place de l'exemple ST **tout en gardant ses réglages et leurs commentaires**,
et `app_x-cube-tof1.h` (une déclaration ajoutée). `main.c` est inchangé.

Pour vérifier :

```bash
diff -rq -x Debug -x Release NUCLEO_L476RG_TOF_VL53L5A1 sentinelle/NUCLEO_L476RG_TOF_SENTINELLE
```

---

## 1. `Inc/`

### Fichiers ajoutés

| Fichier | Rôle |
|---|---|
| `app.h` | Point d'entrée de l'application : `app_init()` et `app_loop()` |
| `app_config.h` | Tous les réglages : mode (`APP_MODE`), débit UART, signal, apprentissage, seuil, alarme, carte des changements |
| `tof_frame.h` | Type `tof_frame_t` : une trame brute de 64 distances et statuts |
| `tof_sensor.h` | Initialisation et lecture du capteur (au-dessus du BSP X-CUBE-TOF1) |
| `scene_preproc.h` | Signal « scène » (distances absolues), modèle de référence par zone, carte des changements, alarme avec hystérésis |
| `sentinel_ai.h` | Adaptateur vers la librairie NanoEdge **Anomaly detection** : `init`, `learn` et `detect` |
| `uart_io.h` | Sorties texte et CSV sur USART2 sans `printf`, reconfiguration du débit |

### Fichiers ST inchangés

`main.h`, `NanoEdgeAI.h`, `stm32l4xx_hal_conf.h`, `stm32l4xx_it.h`, `stm32l4xx_nucleo_bus.h` et
`stm32l4xx_nucleo_errno.h` sont inchangés.

> ⚠️ Le `NanoEdgeAI.h` fourni par ST est une librairie de **classification**. Le mode SENTINELLE
> (`APP_MODE=1`) a besoin d'une librairie **Anomaly detection**, qui fournit
> `neai_anomalydetection_init/learn/detect`. Tant qu'elle n'est pas générée et copiée dans le projet, seul
> le mode COLLECTE (`APP_MODE=0`) compile.

## 2. `Src/`

### Fichiers ajoutés

| Fichier | Rôle |
|---|---|
| `app.c` | Mode COLLECTE : CSV vers NanoEdge Studio. Mode SENTINELLE : délai pour sortir du champ, apprentissage embarqué, surveillance, alarmes avec carte des changements, B1 court (ajout incrémental) et long (tout réapprendre), commandes du terminal |
| `tof_sensor.c` | Configuration du capteur déléguée à `TOF1_SensorConfig()` (`app_x-cube-tof1.c`, réglages de l'exemple ST : profil 8×8 continu à 15 Hz). Lecture sur l'interruption INT (`ToF_EventDetected`) avec relecture de secours toutes les 100 ms, conversion des statuts BSP vers la convention ULD |
| `scene_preproc.c` | Signal de 8 trames disjointes (`100 × distance / 3000 mm`), statistiques par zone (Welford), carte `+`/`-`, alarme qui demande 3 signaux consécutifs. Portable et testé sur PC (`tests/`) |
| `sentinel_ai.c` | Adaptateur NanoEdge : vérifie à la compilation la taille d'entrée (API récente `NEAI_INPUT_SIGNAL_LENGTH` ou ancienne) et n'utilise que `NEAI_OK` parmi les codes de retour |
| `uart_io.c` | `uart_puts`, `uart_put_int`, `uart_put_csv`… et `uart_io_init()`, qui ne reconfigure USART2 que si `APP_UART_BAUD` ≠ 115200 |

### Fichiers ST inchangés

`main.c`, `libneai.a`, `stm32l4xx_hal_msp.c`, `stm32l4xx_it.c`, `stm32l4xx_nucleo_bus.c`, `syscalls.c`,
`sysmem.c` et `system_stm32l4xx.c` sont inchangés.

> ⚠️ `libneai.a` est la librairie de classification modèle de ST. Il faut la remplacer, en même temps que
> `Inc/NanoEdgeAI.h`, par la librairie Anomaly detection générée.

## 3. En dehors de `Src/` et `Inc/`

| Fichier | Changement |
|---|---|
| `X-CUBE-TOF1/App/app_x-cube-tof1.c` | **Modifié.** La boucle de l'exemple ST est supprimée (boucle infinie dans `MX_53L5A1_ThresholdDetection_Process()`, buffer NanoEdge, `printf("%.1f ")`). `MX_X_CUBE_TOF1_Init()` appelle `app_init()` et `MX_X_CUBE_TOF1_Process()` appelle `app_loop()`, dans des blocs `USER CODE`. **Les réglages de l'exemple sont conservés avec leurs commentaires d'origine et s'appliquent**, comme dans Pierre-Feuille-Ciseaux : grand commentaire sur les critères d'interruption, `SUCCESSIVE_MATRIX`, `MATRIX_RESOLUTION`, `AXES`, `NEAI_BUFFER_SIZE`, `MIN_THRESHOLD`, `MAX_THRESHOLD`, `NEAI_MODE` (les 7 lignes du fichier de ST, actives dans l'original, gardées en commentaire `//#define`, puis les réglages appliqués présentés comme dans `ST-exemple/README.md`), contrôles `#error`, profil (`Profile.*`) et interruption (`ITConfig.*`), appliqués par la nouvelle fonction `TOF1_SensorConfig()`. Différences voulues : `ITConfig.Criteria = RS_IT_DEFAULT` (toutes les trames, toute la scène) au lieu de `RS_IT_IN_WINDOW`, seuils 40–3000 mm au lieu de 100–400 (la scène surveillée ; sans effet avec `RS_IT_DEFAULT`), `NEAI_MODE` = `APP_MODE`, cible la plus forte (défaut du capteur). Des `#error` vérifient que ces réglages concordent avec `Inc/app_config.h` (`SENT_FRAMES_PER_SIGNAL`, `TOF_ZONES`, `SENT_SIGNAL_LEN`, `SENT_MAX_MM`, `TOF_FREQ_HZ`). La variable `ToF_EventDetected`, nécessaire à `app_tof_pin_conf.c`, est conservée |
| `X-CUBE-TOF1/App/app_x-cube-tof1.h` | Déclaration de `TOF1_SensorConfig()` ajoutée |
| `NUCLEO_L476RG_TOF_VL53L5A1.ioc` → `NUCLEO_L476RG_TOF_SENTINELLE.ioc` | Renommé. Seuls `ProjectManager.ProjectName` et `ProjectFileName` changent : **aucun périphérique ni broche n'est modifié** |
| `NUCLEO_L476RG_TOF_VL53L5A1.launch` → `NUCLEO_L476RG_TOF_SENTINELLE.launch` | Renommé, avec le nom du projet et du `.elf` mis à jour |
| `.project`, `.cproject` | Nom du projet uniquement, pour pouvoir importer PFC et Sentinelle dans le même workspace. Chemins d'include, librairie `neai` et options de compilation inchangés |
| `Drivers/`, `X-CUBE-TOF1/Target/`, `Startup/`, `Tests/`, `*.ld`, `README.md` | Inchangés. Le `README.md` de ST décrit l'exemple d'origine, qui ne s'applique plus : voir `../README.md` |

## 4. Différences de comportement

| | Projet ST | Sentinelle |
|---|---|---|
| Type de modèle NanoEdge | Classification (modèle pré-entraîné dans la librairie) | **Anomaly detection, apprentissage sur la carte** : la librairie est livrée vierge |
| Point d'entrée | `MX_X_CUBE_TOF1_Process()` ne rend jamais la main (`while(1)` interne) | `app_loop()` est non bloquante, appelée en boucle par `main.c` |
| Déclenchement des mesures | Uniquement si une cible est entre 100 et 400 mm (interruption à seuil) | Mesure **continue** de toute la scène, puisque c'est elle qu'on apprend |
| Contenu d'un signal | 8 trames × 64 distances brutes en mm, **0 hors de 100–400 mm** | 8 trames × 64 distances **absolues** écrêtées à 3000 mm et ramenées à 0–100. Une zone sans cible vaut 100 (loin) |
| Format de collecte | Flottants `%.1f` séparés par des espaces | Entiers séparés par des virgules, et lignes `#` pour les messages. Les deux formats sont acceptés par NanoEdge Studio |
| Débit UART | 115200 bauds | 115200 bauds (inchangé, suffisant pour ~2 signaux/s) |
| Erreurs | `printf` puis `Error_Handler()` | Message `# ERREUR …` répété toutes les 2 s sur l'UART, LED LD2 qui clignote vite |
| Bouton B1 | Non utilisé | Appui court : ajouter la scène actuelle comme normale. Appui long : tout réapprendre |
| LED LD2 | Non utilisée | Clignote lentement : sortir du champ. Fixe : apprentissage. Éteinte : surveillance. Clignote vite : alarme |
| Résultat | « Class found: … » | Similarité 0–100 %, alarme avec hystérésis et carte 8×8 des zones qui ont changé |

## 5. Si vous régénérez le code depuis le `.ioc`

CubeMX ne conserve que le contenu des blocs `USER CODE`. Il recréerait l'exemple ST dans
`app_x-cube-tof1.c`, dont la boucle infinie empêcherait `app_loop()` de s'exécuter. Après une
régénération, recopiez la version de ce dépôt.
