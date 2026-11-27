# Hackathon EdgeAI STMicroelectronics

Le but de ce projet est l'initiation à l'utilisation de l'intelligence artificielle embarquée sur microcontrôleur STM32. Ceci va se faire par le biais du logiciel NanoEdgeAI Studio en utilisant les données de distance d'un capteur de type Time-of-Flight (ToF).

### Liste du matériel & logiciels requis

#### Software :

-   **STM32CubeIDE**, l'environnement de développement de code embarqué sur STM32 :  [https://www.st.com/en/development-tools/stm32cubeide.html#get-software](https://www.st.com/en/development-tools/stm32cubeide.html#get-software "https://www.st.com/en/development-tools/stm32cubeide.html#get-software")
    
-   **NanoEdgeAI Studio**, outil de génération de modèle de machine learning :  [https://stm32ai.st.com/download-nanoedgeai/](https://stm32ai.st.com/download-nanoedgeai/ "https://stm32ai.st.com/download-nanoedgeai/") (remplir le formulaire, installer à réception du mail et entrer la licence indiquée dans le mail).

- Terminal type **TeraTerm** : https://github.com/TeraTermProject/teraterm/releases/tag/v5.5.1

#### Hardware :

- Carte de développement STM32 Nucleo & capteur ToF : https://www.st.com/content/st_com/en/products/evaluation-tools/product-evaluation-tools/imaging-evaluation-boards/p-nucleo-53l5a1.html

## STM32CubeIDE

Une fois le projet "NUCLEO_F401RE_TOF_VL53L5A1" importé dans STM32CubeIDE, vous devriez avoir l'arborescence suivante :

<img src="img/arborescence.png" alt="arborescence"  />

Le code principal est écrit dans le fichier "X-CUBE-TOF1/App/app_x-cube-tof1.c".
A partir de la ligne 64, des #define peuvent être édités pour modifier les paramètres du code, en particulier concernant l'acquisition des données du capteur :

```c
#define SUCCESSIVE_MATRIX 8 /* How many successive matrix we get in our neai_buffer. Possible values: power of 2 between 1 and 64 */
#define MATRIX_RESOLUTION 64 /* Possible values: 64 for 8x8 resolution, 16 for 4x4 resolution */
#define AXES 1 /* Possible value: 1 (distance data) */
#define NEAI_BUFFER_SIZE SUCCESSIVE_MATRIX * MATRIX_RESOLUTION * AXES /* The size of our NEAI buffer used for classification */
#define MIN_THRESHOLD 100 /* Minimum distance in mm between sensor & object to detect. Possible values: 40 to 4000 */
#define MAX_THRESHOLD 400 /* Maximum distance in mm between sensor & object to detect. Possible values: 40 to 4000 */
#define NEAI_MODE 0 /* Possible values: 0 for data logging mode, 1 for NanoEdgeAI classification mode */
```

Modifier en particulier : 
- SUCCESSIVE_MATRIX : mettre à 1 pour une acquisition statique (1 signal = 1 matrice de distances), choisir une puissance de 2 pour une acquisition dynamique (1 signal = plusieurs matrices de distances consécutives). La fréquence d'acquisition des matrices de données est de 15 Hz. A prendre en compte pour déterminer le nombre de matrices par signal à collecter selon la durée du phénomène à classifier.
- MIN_THRESHOLD et MAX_THRESHOLD : pour définir l'intervalle de distances à détecter. Minimum 40mm. Maximum théorique 4000mm, en cas de conditions très lumineuses, la distance maximum mesurable par le capteur peut descendre à 1500mm. L'intervalle devra être ajusté selon le projet : l'angle de vue du capteur et la zone à détecter détermineront la distance optimale devant séparer le capteur de la zone.
- NEAI_MODE : mettre à 0 pour collecter les données de distance du capteur ToF. Après intégration d'une librairie de classification NanoEdgeAI, mettre à 1 pour l'exécuter.


L'intégration d'une librairie au code nécessite l'ajout des 2 fichiers suivants:
- "NanoEdgeAI.h" dans le dossier "Core/Inc".
- "libneai.a" dans le dossier "Core/Src".

Ces fichiers sont fournis lors du téléchargement de la librairie NanoEdgeAI depuis l'onglet Deployment de NEAI Studio.

Ensuite, dans les "propriétés" du projet, il faut ajouter le chemin de la librairie précompilée "libneai.a" dans les options de compilation :

![properties](img/properties.png)

Une fois ces deux étapes effectuées, vous pouvez compiler.



Liens utiles :

- https://www.st.com/content/st_com/en/st-edge-ai-suite.html
- https://wiki.st.com/stm32mcu/wiki/Category:NanoEdgeAI
