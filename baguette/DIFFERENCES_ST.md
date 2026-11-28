# Différences avec les projets de base de ST

Deux projets, un par carte d'extension, construits de la même façon : `NUCLEO_L476RG_IKS01A2_BAGUETTE/` (sections 1 à 4)
et `NUCLEO_L476RG_IKS01A3_BAGUETTE/` (section 5).

Le projet `NUCLEO_L476RG_IKS01A2_BAGUETTE/` part du projet **NUCLEO-L476RG + X-NUCLEO-IKS01A2** généré par
STM32CubeIDE avec le pack **X-CUBE-MEMS1 9.6.0** et l'application **IKS01A2_DataLogTerminal** de ST. C'est le
projet zippé dans `ST-exemple/NUCLEO_L476RG_IKS01A2.zip`, essayé sur la carte avant la copie.

## 1. Renommage

| Fichier | Changement |
|---|---|
| `.project` | nom du projet : `NUCLEO_L476RG_IKS01A2_BAGUETTE` (deux projets du même nom ne peuvent pas cohabiter dans un workspace) |
| `NUCLEO_L476RG_IKS01A2.ioc` → `NUCLEO_L476RG_IKS01A2_BAGUETTE.ioc` | renommé ; `ProjectName` et `ProjectFileName` suivent |
| `.cproject` | nom de l'exécutable ; script de liaison en `${ProjDirPath}/STM32L476RGTX_FLASH.ld` (corrigé par `cubeide_cible.py`) |

## 2. Fichiers de ST modifiés

| Fichier | Changement |
|---|---|
| `MEMS/App/app_mems.c` | Dans les zones `USER CODE` de `MX_MEMS_Init()` et `MX_MEMS_Process()` : appel de `app_init()` / `app_loop()` puis `return`. L'exemple DataLogTerminal reste dans le fichier mais n'est plus appelé. Les zones `USER CODE` sont conservées par CubeMX quand il régénère le code |
| `.cproject` | Liaison avec la librairie NanoEdge : `-lneai`, cherchée dans `../Core/Src` (configurations Debug et Release), comme dans Pierre-Feuille-Ciseaux |

Aucun autre fichier de ST (`Core/`, `Drivers/`, `MEMS/Target/`) n'est modifié.

## 3. Fichiers ajoutés

| Fichier | Rôle |
|---|---|
| `Core/Inc/app_config.h` | Paramètres : mode, capteur, signal, déclenchement, reconnaissance |
| `Core/Inc/app.h`, `Core/Src/app.c` | Application : mode collecte (CSV) et mode reconnaissance |
| `Core/Inc/imu_sensor.h`, `Core/Src/imu_sensor.c` | LSM6DSL par le BSP de ST : cadence, pleine échelle, lecture sur « donnée prête » |
| `Core/Inc/wand_preproc.h`, `Core/Src/wand_preproc.c` | Découpage des gestes : tampon de pré-déclenchement, fenêtre, mise à l'échelle. Code portable, testé sur PC |
| `Core/Inc/wand_ai.h`, `Core/Src/wand_ai.c` | Adaptateur NanoEdge AI (classification) |
| `Core/Inc/uart_io.h`, `Core/Src/uart_io.c` | Sorties texte et CSV sur l'USART2 du BSP (`hcom_uart[COM1]`) |
| `Core/Inc/NanoEdgeAI.h`, `Core/Src/libneai.a` | Librairie NanoEdge AI fournie (N-Class classification, 6 axes × 256 points, classes cercle, zigzag, frappe, v, autre), entraînée sur 10 gestes par classe d'une seule personne. À remplacer par la vôtre (README, section 5) |

## 4. Régénérer le code depuis le `.ioc`

C'est possible, avec les précautions suivantes :
- vérifier que le pack reste en **X-CUBE-MEMS1 9.6.0** (les versions 10.0.0 et suivantes n'ont plus l'IKS01A2) ;
- les fichiers ajoutés dans `Core/` et les zones `USER CODE` de `app_mems.c` sont conservés ;
- après la génération, relancer `python3 cubeide_cible.py --check` à la racine du dépôt, et vérifier que
  `-lneai` et `../Core/Src` sont toujours dans *Project > Properties > C/C++ Build > Settings > MCU GCC Linker*.

## 5. Projet `NUCLEO_L476RG_IKS01A3_BAGUETTE/` (X-NUCLEO-IKS01A3)

Il part du projet **NUCLEO-L476RG + X-NUCLEO-IKS01A3** généré par STM32CubeIDE avec le pack
**X-CUBE-MEMS1 11.2.0** et l'application **IKS01A3_DataLogTerminal** : c'est le projet zippé dans
`ST-exemple/NUCLEO_L476RG_IKS01A3.zip`. Les changements sont **exactement ceux des sections 1 à 3** :

- **Renommage** : `.project`, `NUCLEO_L476RG_IKS01A3_BAGUETTE.ioc` (`ProjectName`, `ProjectFileName`),
  `.cproject` (dossiers de compilation, script de liaison en `${ProjDirPath}/STM32L476RGTX_FLASH.ld`) ;
- **`MEMS/App/app_mems.c`** : les mêmes appels `app_init()` / `app_loop()` dans les zones `USER CODE` ;
- **`.cproject`** : `-lneai` cherchée dans `../Core/Src` (Debug et Release) ;
- **fichiers ajoutés** : les mêmes que pour l'IKS01A2, **identiques octet pour octet** (vérifié par
  `make test`), sauf `Core/Inc/imu_sensor.h` et `Core/Src/imu_sensor.c`, qui pilotent le **LSM6DSO**
  (`IKS01A3_LSM6DSO_0`, fonctions `IKS01A3_MOTION_SENSOR_…`, mêmes réglages : 104 Hz, ±16 g, ±2000 °/s).

Aucun autre fichier de ST n'est modifié. Pour régénérer le code depuis le `.ioc`, mêmes précautions qu'en
section 4, avec le pack **X-CUBE-MEMS1 11.2.0** (les versions 12.0.0 et suivantes n'ont plus l'IKS01A3).

La librairie NanoEdge fournie est la même que pour l'IKS01A2 (apprise avec le LSM6DSL) : à remplacer par une
librairie apprise avec l'IKS01A3.
