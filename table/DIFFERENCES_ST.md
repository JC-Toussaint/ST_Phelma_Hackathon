# Différences avec le projet de base de ST

`NUCLEO_L476RG_IKS01A3_TABLE/` est une copie du projet `baguette/NUCLEO_L476RG_IKS01A3_BAGUETTE/`, lui-même
construit sur le projet **NUCLEO-L476RG + X-NUCLEO-IKS01A3** généré par STM32CubeIDE avec le pack
**X-CUBE-MEMS1 11.2.0** et l'application **IKS01A3_DataLogTerminal** (`ST-exemple/NUCLEO_L476RG_IKS01A3.zip`).
Les modifications des fichiers de ST sont donc **les mêmes que pour la baguette** (voir
`baguette/DIFFERENCES_ST.md`, sections 1, 2 et 5) :

| Fichier | Changement |
|---|---|
| `.project`, `NUCLEO_L476RG_IKS01A3_TABLE.ioc`, `.cproject` | nom du projet ; script de liaison en `${ProjDirPath}/STM32L476RGTX_FLASH.ld` |
| `.cproject` | liaison avec la librairie NanoEdge : `-lneai`, cherchée dans `../Core/Src` (Debug et Release) |
| `MEMS/App/app_mems.c` | dans les zones `USER CODE` de `MX_MEMS_Init()` et `MX_MEMS_Process()` : `app_init()` / `app_loop()` puis `return` |

Aucun autre fichier de ST n'est modifié.

## Fichiers ajoutés

| Fichier | Rôle |
|---|---|
| `Core/Inc/app_config.h` | Paramètres : mode, capteur, signal, déclenchement, reconnaissance |
| `Core/Inc/app.h`, `Core/Src/app.c` | Application : collecte (CSV) et reconnaissance, cadence mesurée |
| `Core/Inc/imu_sensor.h`, `Core/Src/imu_sensor.c` | Accéléromètre du LSM6DSO à 1667 Hz, **FIFO** en mode continu lue par paquets (fonctions `IKS01A3_MOTION_SENSOR_FIFO_…` du BSP) |
| `Core/Inc/tap_preproc.h`, `Core/Src/tap_preproc.c` | Découpage des tapes : moyenne de repos, déclenchement, pré-déclenchement, fenêtre, mise à l'échelle. Code portable, testé sur PC |
| `Core/Inc/tap_ai.h`, `Core/Src/tap_ai.c` | Adaptateur NanoEdge AI (classification) : copie de `wand_ai` de la baguette |
| `Core/Inc/uart_io.h`, `Core/Src/uart_io.c` | Sorties texte et CSV sur l'USART2 du BSP : copie de la baguette |
| `Core/Inc/NanoEdgeAI.h`, `Core/Src/libneai.a` | Librairie NanoEdge AI d'essai (N-Class classification, 3 axes × 256 points, classes zone1 à zone4), apprise sur une planche, capteur à une extrémité, 50 tapes par zone. Liée dans les deux modes (`-lneai`). À remplacer par la vôtre (README, section 5) |

## Régénérer le code depuis le `.ioc`

Mêmes précautions que pour la baguette : garder le pack **X-CUBE-MEMS1 11.2.0**, vérifier ensuite
`-lneai` et `../Core/Src` dans *MCU GCC Linker*, et relancer `python3 cubeide_cible.py --check`.
