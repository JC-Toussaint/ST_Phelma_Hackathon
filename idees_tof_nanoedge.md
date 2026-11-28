# Idées de projets NanoEdge AI + STM32L476 : capteur ToF VL53L5CX (8×8), accéléromètre (IKS01A2 / IKS01A3), microphone

Matériel : NUCLEO-L476RG + X-NUCLEO-53L5A1 (capteur ToF multizone VL53L5CX, 8×8 zones, ~45° de champ, 15 Hz max en 8×8).

## Pourquoi ne pas compter les doigts ?

À 15–20 cm, une zone couvre ~1,5–2 cm, soit la largeur d'un doigt : deux doigts serrés tombent dans la
même zone. Le 8×8 capte bien le **mouvement** et la **forme grossière**, pas les petits détails.
NanoEdge AI est d'ailleurs plus à l'aise avec des **signaux temporels** qu'avec une image statique fine.

NanoEdge propose 4 types de projets : **détection d'anomalie** (avec apprentissage *sur la carte*, son
point unique), **1-classe**, **N-classes** et **extrapolation** (régression).

## 🎮 Démos ludiques

### 1. Pierre – Feuille – Ciseaux contre le STM32 ✅ (réalisé : voir `pierre_feuille_ciseaux/`)
- La faible résolution suffit : poing = blob compact, feuille = grande surface plate, ciseaux = deux bandes.
- NanoEdge **N-classes** (3 classes).
- Le STM32 « joue » sa main (UART, LED), compte les points, et peut apprendre tes habitudes.

### 2. Gestes dynamiques sans contact
- Balayage gauche/droite/haut/bas, approche/recul, rotation de main.
- Signal = 8–16 trames concaténées (évolution du centroïde) → N-classes.
- Usages : diapos, musique, lampe, manette de jeu.

### 3. Thérémine / instrument gestuel
- Extrapolation : position/hauteur de la main → note/volume.
- N-classes : geste de « mode » (poing = changer d'instrument).

## 🧠 Ce que NanoEdge a d'unique

### 4. Sentinelle : surveiller une scène et expliquer l'alarme ✅ (réalisée : voir `sentinelle/`)
- Réalisée en **classification** `normal` / `anomalie` : le modèle est appris dans NanoEdge AI Studio sur
  des exemples de la scène (bureau, porte, étagère), puis tourne sur la carte, sans PC.
- Elle signale toute anomalie (objet déplacé, intrusion, tiroir ouvert) et l'**explique** par une carte 8×8
  de ce qui a changé.
- Variante « pour aller plus loin » : une librairie **détection d'anomalie**, avec laquelle la carte apprend
  elle-même sa scène normale et réapprend sur bouton (pas encore essayée sur la carte).

### 5. Surveillance machine / tapis roulant
- Apprentissage du flux normal d'objets → détection de bourrage, objet couché, rythme anormal.

## 🏠 Usages du quotidien

### 6. Coach de posture devant l'écran
- Capteur au-dessus de l'écran. Classes : droit / penché / trop près / affalé / absent.
- Alerte après X minutes de mauvaise posture. Très robuste en 8×8 (le corps est une grosse cible).

### 7. Comptage directionnel à une porte
- Capteur au plafond, entrée vs sortie grâce à la séquence temporelle.
- Classes : entrée / sortie / passage hésitant / 2 personnes.

### 8. Niveau de remplissage sans contact
- Capteur au-dessus d'un bac (croquettes, grains, vis) → extrapolation du volume restant,
  même avec une surface irrégulière (impossible avec un ToF 1 zone).

## 🛟 Assistance à la personne

### 9. Détecteur de chute et de situation anormale en salle de bains
- Capteur au plafond : la hauteur de la personne au-dessus du sol trahit une chute (−70 cm en < 1,5 s).
- Pré-alerte « je vais bien » de 30 s, puis alerte locale (buzzer, LED, interface PC).
- Apprend sur la carte la durée habituelle des visites : alerte si séjour anormalement long et immobile.
- Sans caméra (64 distances, aucune image) et sans cloud.

## Recommandation hackathon

- **Pierre-Feuille-Ciseaux (1)** : résultat fun et sûr, démo interactive qui attire le public.
- **+ Sentinelle (4)** : surveillance de scène avec alarme expliquée. Pour montrer l'apprentissage embarqué
  de NanoEdge, souvent un critère des jurys ST : sa variante détection d'anomalie, ou le ventilateur (10).

Les deux partagent le même socle firmware : acquisition VL53L5CX → prétraitement → CSV UART pour la collecte.

---

# Autre carte d'extension : X-NUCLEO-IKS01A2 ou IKS01A3 (capteurs de mouvement et d'environnement)

Matériel : NUCLEO-L476RG + **X-NUCLEO-IKS01A2** ou **X-NUCLEO-IKS01A3**, enfichée sur les mêmes connecteurs
Arduino que la X-NUCLEO-53L5A1. Les deux cartes se ressemblent ; seuls les capteurs changent :

| | X-NUCLEO-IKS01A2 | X-NUCLEO-IKS01A3 |
|---|---|---|
| Accéléromètre + gyroscope 6 axes | **LSM6DSL** | **LSM6DSO** |
| Autres capteurs de mouvement | LSM303AGR (accéléromètre + magnétomètre) | LIS2DW12 (accéléromètre), LIS2MDL (magnétomètre) |
| Environnement | HTS221 (humidité, température), LPS22HB (pression) | HTS221, LPS22HH (pression), STTS751 (température) |
| Pack X-CUBE-MEMS1 à utiliser | **9.6.0** (carte retirée en 10.0.0) | **11.2.0** (carte retirée en 12.0.0) |
| Fonctions du pilote | `IKS01A2_MOTION_SENSOR_...`, capteur `IKS01A2_LSM6DSL_0` | `IKS01A3_MOTION_SENSOR_...`, capteur `IKS01A3_LSM6DSO_0` |

Pour les idées ci-dessous, le **LSM6DSL** et le **LSM6DSO** jouent le même rôle (accéléromètre + gyroscope,
104 Hz, FIFO) : tout ce qui est écrit pour l'un vaut pour l'autre.

L'accéléromètre est le capteur le plus classique pour NanoEdge AI, conçu d'abord pour les **vibrations** et
les **mouvements** : des signaux temporels, son point fort.

## 🔧 Le cas d'école : maintenance prédictive

### 10. Ventilateur sous surveillance (détection d'anomalie, apprentissage sur la carte)
- Carte fixée sur un petit ventilateur (PC ou USB) ; vibrations 3 axes échantillonnées vers 1 kHz.
- La carte **apprend sur place** le ventilateur sain, puis détecte un défaut : un bout d'adhésif sur une
  pale crée un balourd, un doigt qui freine, un ventilateur posé de travers.
- Peu de données à collecter, résultat immédiat et très visuel : c'est le projet qui montre le mieux
  l'**apprentissage embarqué** de NanoEdge.

### 11. État d'une machine (N-classes)
- Même montage, plusieurs états : arrêt / vitesse lente / vitesse rapide / défaut.

### 12. Estimer une grandeur continue (extrapolation)
- Vitesse du ventilateur, inclinaison, charge d'une machine à laver miniature : la librairie sort une
  **valeur** au lieu d'une classe. C'est le seul type de projet NanoEdge qui le fait.

## 🎮 Mouvement et gestes

### 13. Baguette magique (N-classes) ✅ (réalisée : voir `baguette/`)
- Carte tenue en main ; accéléro + gyro 6 axes, ~100 Hz, fenêtres d'~1 s.
- Classes : cercle / zigzag / frappe / « V »… pour piloter un jeu ou une présentation.
- Ludique, et proche de Pierre-Feuille-Ciseaux dans la démarche.
- Plan de développement détaillé en fin de fiche (« Plan détaillé : Baguette magique »).

### 14. Reconnaissance d'activité (N-classes)
- Carte portée à la ceinture ou au poignet : assis / marche / course / escaliers.
- Classique et facile à tester sur soi.

### 15. Toc-toc secret (N-classes)
- Carte posée sur une table ou une porte : reconnaître une suite de coups frappés.
- Une serrure à code, sans clavier.

## 📦 Surveillance

### 16. Colis sous surveillance (détection d'anomalie)
- Accéléromètre + pression : chocs, chute, retournement pendant un transport.
- Proche de Sentinelle (4), avec le mouvement au lieu de la scène.

## 🖐️ Interfaces

### 20. La table qui écoute : des boutons sur une table ordinaire (N-classes)
- Carte collée **sous** une table ; 4 à 6 zones marquées sur le dessus (gommettes). On tape du doigt sur une
  zone : la carte reconnaît **laquelle**, rien qu'aux vibrations qui traversent le plateau.
- Démo : un **piano** sur la table (une note par zone, jouée par le PC), ou une télécommande de diaporama.
- L'onde d'une tape arrive à la carte plus ou moins vite et atténuée, avec des rebonds différents selon
  l'endroit : impossible à calculer simplement, mais exactement le genre de motif qu'apprend NanoEdge.
- Plan de développement détaillé plus bas (« Plan détaillé : La table qui écoute »).

## Recommandation et réutilisation

- **Ventilateur (10)** : le plus sûr pour un hackathon, et la vitrine de l'apprentissage sur la carte.
- **Baguette magique (13)** : le plus ludique.
- **La table qui écoute (20)** : le plus surprenant, en reprenant presque tout le projet de la baguette.
- **Ce qui se réutilise du dépôt** : l'outil de collecte commun `outils/collecte_nanoedge.py`, qui affiche
  déjà des **courbes à plusieurs axes** (`"affichage": {"type": "courbes", ...}` dans le profil, voir
  `outils/README.md`, « Ajouter un projet ») ; la démarche collecte (`APP_MODE 0`) → Studio → intégration
  (`APP_MODE 1`) ; la structure README / `DEMARRAGE.md` / présentation.
- **Ce qui change** : le firmware de base, à partir d'un exemple du pack **X-CUBE-MEMS1** de ST au lieu de
  X-CUBE-TOF1, **dans la version qui prend encore en charge la carte** (tableau plus haut) : les versions
  récentes du pack n'ont plus ni l'IKS01A2 ni l'IKS01A3. Le capteur doit échantillonner à **fréquence fixe** : NanoEdge attend des signaux de longueur
  et de cadence constantes.
- À vérifier : selon sa version, NanoEdge AI Studio sait générer lui-même un firmware de collecte pour
  certaines cartes et capteurs MEMS de ST, ce qui accélère le démarrage.

## Plan détaillé : Baguette magique (13)

La démarche est celle de Pierre-Feuille-Ciseaux ; ce qui change, c'est le signal : plus une image 8×8, mais
une **trajectoire dans le temps**, 6 courbes d'accéléromètre et de gyroscope. C'est le type de signal sur
lequel NanoEdge est le plus à l'aise.

### 1. Le matériel
- NUCLEO-L476RG + X-NUCLEO-IKS01A2 (ou IKS01A3) ; on utilise le **LSM6DSL** (**LSM6DSO** sur l'IKS01A3),
  accéléromètre + gyroscope 6 axes.
- La carte tenue en main **est** la baguette. Marquer un repère « haut / avant » et la tenir toujours dans
  le même sens : les axes changent si on la tourne.
- Le câble USB limite les gestes : un câble long suffit pour le hackathon (une version sans fil demanderait
  une batterie et une liaison radio, c'est un autre projet).

### 2. Concevoir le signal (le point clé)

| Choix | Valeur proposée | Pourquoi |
|---|---|---|
| Capteurs | accéléromètre ±4 g + gyroscope ±1000 °/s | Le gyroscope distingue bien cercle / zigzag, l'accéléromètre la frappe |
| Fréquence | 104 Hz, fixe | NanoEdge veut une cadence constante |
| Fenêtre | 128 échantillons ≈ 1,2 s | La durée d'un geste |
| Signal | 128 × 6 axes = **768 valeurs**, entrelacées (ax, ay, az, gx, gy, gz, ax…) | Format attendu par le Studio en « 6 axes » |
| Déclenchement | le geste commence quand le mouvement dépasse un seuil (norme du gyroscope) ; un tampon circulaire garde ~15 échantillons d'avant | Signaux **alignés sur le début du geste**, ce qui aide beaucoup NanoEdge |
| Mise à l'échelle | entiers 0..100 (50 = zéro) | Pour l'affichage de l'outil de collecte, qui attend des valeurs entre 0 et `valeur_max` |

**Classes** : `cercle`, `zigzag`, `frappe`, `v`, plus une classe **`autre`** (mouvements quelconques : se
gratter la tête, poser la baguette). Avec un seuil de confiance, comme `GAME_MIN_CONFIDENCE` dans
Pierre-Feuille-Ciseaux, elle évite de reconnaître un geste à chaque mouvement.

### 3. Le firmware, sur le modèle de Pierre-Feuille-Ciseaux

Partir de l'application **DataLogTerminal** (déjà générée et zippée dans `ST-exemple/NUCLEO_L476RG_IKS01A2.zip`
et `NUCLEO_L476RG_IKS01A3.zip`) du pack X-CUBE-MEMS1 (9.6.0 pour l'IKS01A2, 11.2.0 pour
l'IKS01A3), générée par STM32CubeIDE pour la NUCLEO-L476RG (comme Pierre-Feuille-Ciseaux part de
`ST-exemple/`), puis ajouter des fichiers sans modifier ceux de ST :

| Fichier | Rôle |
|---|---|
| `Inc/app_config.h` | `APP_MODE` (0 collecte, 1 reconnaissance), fréquence, fenêtre, seuils |
| `Src/imu_sensor.c` | Initialisation du LSM6DSL par le BSP (`IKS01A2_MOTION_SENSOR_Init(IKS01A2_LSM6DSL_0, MOTION_ACCELERO \| MOTION_GYRO)`, `..._SetOutputDataRate`, `..._SetFullScale`, `..._GetAxes` ; préfixe `IKS01A3_` et `IKS01A3_LSM6DSO_0` sur l'IKS01A3) ; lecture à cadence fixe (interruption *data ready* ou FIFO) |
| `Src/wand_preproc.c` | Tampon circulaire, déclenchement, fenêtre de 128 échantillons, mise à l'échelle : **code portable, testé sur PC** comme `tof_preproc.c` |
| `Src/wand_ai.c` | Appel NanoEdge (`neai_classification`), noms et probabilités des classes : copie adaptée de `rps_ai.c` |
| `Src/app.c` | Mode 0 : une ligne CSV par geste détecté. Mode 1 : nom du geste, probabilités en direct (comme la touche `d` de Pierre-Feuille-Ciseaux), LED |

### 4. La collecte, avec l'outil commun du dépôt

Un profil `tools/profil_collecte.json` suffit, sans nouveau code Python :

```json
{
  "nom": "Baguette magique",
  "classes": ["cercle", "zigzag", "frappe", "v", "autre"],
  "longueur": 768,
  "affichage": {"type": "courbes", "axes": ["ax", "ay", "az", "gx", "gy", "gz"], "instants": 128, "ordre": "entrelace"},
  "valeur_max": 100,
  "par_passe": 50,
  "attente": "En attente",
  "conseil": "un geste franc par signal, baguette toujours tenue dans le même sens"
}
```

L'interface affiche les 6 courbes de chaque geste : on vérifie d'un coup d'œil qu'il est complet et bien
aligné. `attente` suppose que le firmware envoie « # En attente… » quand rien ne bouge, comme
Pierre-Feuille-Ciseaux. Viser **150 à 300 gestes par classe**, faits par plusieurs personnes.

### 5. Le Studio
- Projet **N-Class classification**, cible STM32L476RG, capteur **Generic, 6 axes**, 128 points par axe.
- Benchmark, puis matrice de confusion (souvent cercle / zigzag) ; données de test à part (`--out test`).
- Compiler avec Float ABI **hard** et FPU **fpv4-sp-d16**, puis intégrer `NanoEdgeAI.h` et `libneai.a`.

### 6. Planning indicatif pour le hackathon
1. **Matin** : projet DataLogTerminal importé, lecture des 6 axes à 104 Hz affichée sur l'UART.
2. Déclenchement et fenêtre ; collecte qui marche avec l'outil commun.
3. **Midi** : collecte des 5 classes, benchmark.
4. **Après-midi** : intégration, reconnaissance en direct, réglage des seuils.
5. **Bonus** : script PC qui transforme chaque geste en touche du clavier (diapositive suivante pour la
   frappe…), ou petit jeu.

### Pièges à connaître
- **Cadence irrégulière** : lire le capteur sur son interruption ou sa FIFO, jamais dans une boucle avec
  `HAL_Delay`.
- **Orientation** : un geste fait la baguette retournée ressemble à un autre. Tenir la baguette toujours
  pareil, ou collecter aussi dans d'autres orientations.
- **Gestes trop longs ou trop lents** : ils dépassent la fenêtre de 1,2 s. Montrer le rythme attendu, ou
  allonger la fenêtre (256 échantillons, et revoir `longueur`).
- **Saturation** : une frappe franche peut dépasser ±4 g ; passer à ±8 g si les courbes plafonnent.

## Plan détaillé : La table qui écoute (20)

La démarche est celle de la baguette (déclenchement, pré-déclenchement, fenêtre fixe, classe `autre`), avec
deux différences : la carte ne bouge pas, et le signal est **une vibration très brève**, d'où une cadence
quinze fois plus élevée et une fenêtre très courte.

### 1. Le matériel
- NUCLEO-L476RG + X-NUCLEO-IKS01A2 ou IKS01A3 ; on n'utilise que l'**accéléromètre** du LSM6DSL / LSM6DSO.
- La carte est fixée **sous** le plateau, fermement : adhésif double face épais ou petit serre-joint (une
  carte qui bouge change tout). Le câble USB pend sans toucher la table.
- 4 zones pour commencer, **espacées d'au moins 15 cm**, marquées par des gommettes numérotées.
- Une table en bois ou en aggloméré transmet bien ; une table très lourde, ou sur des pieds amortissants,
  moins bien.

### 2. Concevoir le signal (le point clé)

| Choix | Valeur proposée | Pourquoi |
|---|---|---|
| Capteur | accéléromètre seul, 3 axes, ±4 g | Une tape est un choc : le gyroscope n'apporte rien ; ±4 g suffit pour une tape du doigt (passer à ±8 g si les courbes plafonnent) |
| Fréquence | **1660 Hz** (LSM6DSL), **1667 Hz** (LSM6DSO), fixe | La vibration ne dure que quelques dizaines de millisecondes : à 104 Hz, on ne verrait presque rien |
| Fenêtre | **256 échantillons ≈ 0,15 s** | Le choc et ses rebonds ; une puissance de 2, comme le conseille ST |
| Signal | 256 × 3 axes = **768 valeurs**, entrelacées (ax, ay, az, ax…) | Format « Generic, 3 axes » du Studio |
| Déclenchement | écart à la moyenne récente (la gravité, sur le repos) > seuil, par exemple 80 mg ; 32 échantillons gardés d'avant (~20 ms) | Signaux **alignés sur l'instant de la tape**, comme les gestes de la baguette |
| Mise à l'échelle | gravité retirée (moyenne d'avant le déclenchement), entiers 0..1000, 500 = zéro | Comme la baguette ; on garde l'amplitude, qui porte aussi l'information de distance |

**Classes** : `zone1` … `zone4`, plus **`autre`** : poser une tasse, un coude, un livre, taper n'importe où
ailleurs. Seuil de confiance comme `WAND_MIN_CONFIDENCE` dans la baguette.

### 3. Le firmware, à partir du projet de la baguette

Partir de `baguette/NUCLEO_L476RG_IKS01A2_BAGUETTE/` (ou `_IKS01A3_BAGUETTE/`), déjà prêt : collecte,
NanoEdge, messages sur l'UART. Ce qui change :

| Fichier | Changement |
|---|---|
| `app_config.h` | 1660 Hz, accéléromètre seul (3 axes), fenêtre de 256, pré-déclenchement de 32, seuils en mg |
| `imu_sensor.c` | Accéléromètre seul à 1660 Hz. **Commencer par la lecture actuelle** sur le bit « donnée prête » : pendant une capture, la boucle ne fait rien d'autre. Vérifier qu'aucun échantillon ne manque (256 échantillons doivent durer ~154 ms, à mesurer avec `HAL_GetTick()`). Sinon, passer à la **FIFO** du capteur, lue par paquets : ses fonctions diffèrent entre l'IKS01A2 (`IKS01A2_MOTION_SENSOR_FIFO_…`, décimation) et l'IKS01A3 (`IKS01A3_MOTION_SENSOR_FIFO_…`, BDR et étiquettes) |
| `tap_preproc.c` | La logique de `wand_preproc.c` avec 3 axes : moyenne de repos, déclenchement, pré-déclenchement, fenêtre ; **code portable, testé sur PC** |
| `app.c` | Mode 0 : une ligne CSV par tape. Mode 1 : `ZONE 3 95%  (…)`, et la LED |

Le bus I2C des deux projets tourne déjà à 1 MHz (`BUS_I2C1_FREQUENCY`) : environ 10 ko/s à lire, sans
difficulté. Pendant l'envoi d'une ligne CSV (768 valeurs, ~0,3 s à 115200 bauds), la carte ne capte rien :
c'est sans gêne, une nouvelle tape arrive rarement si vite.

### 4. La collecte, avec l'outil commun du dépôt

```json
{
  "nom": "La table qui écoute",
  "classes": ["zone1", "zone2", "zone3", "zone4", "autre"],
  "longueur": 768,
  "affichage": {"type": "courbes", "axes": ["ax", "ay", "az"], "instants": 256, "ordre": "entrelace"},
  "valeur_max": 1000,
  "par_passe": 50,
  "attente": "En attente"
}
```

**Commencer par 2 zones éloignées** : si le benchmark les sépare, ajouter les autres. Viser 100 à 200 tapes
par zone, avec plusieurs doigts, plusieurs forces, plusieurs personnes, et une passe de test à part
(`--out test`).

### 5. Le Studio
- Projet **N-Class classification**, cible STM32L476RG, capteur **Generic, 3 axes**, 256 points par axe.
- La matrice de confusion montre les zones voisines confondues : les écarter, ou collecter davantage.
- Compiler avec Float ABI **hard** et FPU **fpv4-sp-d16**, puis intégrer `NanoEdgeAI.h` et `libneai.a`.

### 6. La démo
- Un petit script PC lit les lignes `ZONE n` sur le port série et joue une note par zone (`winsound.Beep`
  sous Windows), ou simule une touche du clavier (diapositive suivante, précédente).
- Extensions : reconnaître la **façon** de taper (doigt, ongle, poing, double tape), ou estimer la **force**
  de la tape par extrapolation NanoEdge, pour des notes plus ou moins fortes.

### 7. Planning indicatif pour le hackathon
1. **Matin** : projet de la baguette passé à 1660 Hz, accéléromètre seul ; vérifier la cadence réelle.
2. Déclenchement sur une tape ; collecte de 2 zones et premier benchmark.
3. **Midi** : 4 zones + `autre`, benchmark, validation.
4. **Après-midi** : intégration, reconnaissance en direct, script du piano.
5. **Bonus** : 6 zones, façon de taper, force.

### Pièges à connaître
- **Échantillons perdus** : à 1660 Hz, la moindre attente dans la boucle (envoi UART, `HAL_Delay`) fait
  manquer des échantillons et déforme le signal. Mesurer la durée réelle d'une fenêtre ; passer à la FIFO si
  besoin.
- **Le modèle ne vaut que pour cette table et cette position de la carte** : carte décollée ou table
  déplacée, il faut recollecter. Le présenter comme un « étalonnage ».
- **Objets posés sur la table** : un ordinateur ou une pile de livres change la propagation des vibrations.
  Collecter avec la table dans l'état de la démo.
- **Tapes trop douces** : sous le seuil, rien ne se passe ; trop fortes, les courbes plafonnent. Montrer la
  tape attendue.

# Autre carte d'extension : un microphone (X-NUCLEO-CCA02M2)

Matériel : NUCLEO-L476RG + une carte de microphones MEMS numériques. Ni l'IKS01A2 ni l'IKS01A3 n'ont de
microphone.

| Carte | Microphone | Avec la NUCLEO-L476RG |
|---|---|---|
| **X-NUCLEO-CCA02M2** (recommandée) | 2 × **MP34DT06J** (numériques, sortie PDM), jusqu'à 4 | **Prise en charge par ST** : pack **X-CUBE-MEMSMIC1**, exemple *Microphones_Streaming* ; le L476 lit les micros avec son périphérique **DFSDM** |
| X-NUCLEO-IKS02A1 | **IMP34DT05** (numérique, PDM), avec un accéléromètre + gyroscope ISM330DHCX | Compatibilité du microphone avec la L476RG **non confirmée** par ST : à vérifier avant de la choisir |

NanoEdge AI marche bien sur des **sons courts et typés** (moins d'une seconde) : une sonnerie, un bip, des
coups, un bruit de machine. Il est beaucoup moins à l'aise avec la **parole** : les voix varient trop d'une
personne à l'autre pour un hackathon.

**Le point clé** : on n'envoie pas le son brut à NanoEdge. À 16 kHz, une seconde fait 16 000 valeurs, alors
que ST conseille des signaux de 16 à 4096 points par axe. Le STM32 calcule d'abord un petit
**spectrogramme** (l'énergie dans quelques dizaines de bandes de fréquence, tranche de temps par tranche de
temps), et c'est lui qui part vers NanoEdge, comme les 6 courbes de la baguette.

## 🛟 Assistance à la personne

### 17. Sons de la maison pour une personne malentendante (N-classes)
- La carte écoute la pièce et signale par une LED (ou une vibration, un message) : `sonnette`, `alarme`
  (détecteur de fumée, réveil), `frappe` (on frappe à la porte), `applaudissement` (deux claquements de
  mains pour appeler), plus une classe **`autre`** (voix, musique, pas, vaisselle) à ignorer.
- Utile, facile à montrer (on sonne, la LED s'allume) et des classes nettement différentes : son tenu,
  impulsions, bruit large bande.
- Plan de développement détaillé plus bas (« Plan détaillé : Sons de la maison »).

## 🔧 Maintenance prédictive

### 18. Une machine qui sonne faux (détection d'anomalie, apprentissage sur la carte)
- Carte posée près d'un ventilateur, d'une pompe d'aquarium ou d'une imprimante : elle **apprend seule le
  bruit normal**, puis signale un changement (obstacle dans les pales, filtre bouché, roulement usé).
- Le pendant sonore du ventilateur (10), et le même principe que la variante détection d'anomalie de la
  Sentinelle : pas de collecte préalable des défauts.

## 🎮 Démo ludique

### 19. Serrure à rythme (N-classes)
- Reconnaître un rythme de claquements de mains ou de coups sur une porte (« toc… toc-toc ») pour « ouvrir ».
- Le pendant sonore du toc-toc secret (15) : la carte n'a pas besoin de toucher la porte.

À éviter en hackathon : la reconnaissance de mots (« oui », « non », « allume »), faisable en principe mais
qui demande énormément de données pour un résultat souvent décevant.

## Plan détaillé : Sons de la maison (17)

La démarche est celle de la baguette : un **déclenchement** sur un événement (ici, un son plus fort que le
bruit de fond), une **fenêtre** de longueur fixe avec un peu d'avance gardée en mémoire, une classe
**`autre`** pour le rejet. Ce qui change, c'est le prétraitement : un spectrogramme au lieu de courbes.

### 1. Le matériel
- NUCLEO-L476RG + **X-NUCLEO-CCA02M2** ; un seul des deux microphones suffit.
- Partir de l'exemple **Microphones_Streaming** du pack **X-CUBE-MEMSMIC1** pour la NUCLEO-L476RG, qui sait
  déjà acquérir le son (DFSDM, DMA) : c'est l'équivalent de `ST-exemple/` pour ce projet. L'essayer tel
  quel d'abord (il envoie le son au PC par USB, à écouter avec Audacity).
- Pour la collecte, des sons reproductibles : sonnerie et alarme jouées par un téléphone ou une enceinte à
  volume modéré, vraies frappes et vrais claquements de mains, plusieurs personnes et plusieurs distances.

### 2. Concevoir le signal (le point clé)

| Choix | Valeur proposée | Pourquoi |
|---|---|---|
| Échantillonnage | 16 kHz, 16 bits, un microphone | Assez pour sonneries et bips (quelques kHz), et peu de calcul |
| Tranche de temps | 512 échantillons = 32 ms, FFT de 512 points | Le L476 (Cortex-M4 avec FPU) en fait 32 par seconde sans peine |
| Bandes | **32 bandes** de fréquence, plus étroites dans les graves (échelle « mel » ou logarithmique), énergie en dB | Résume chaque tranche en 32 nombres, à la manière de l'oreille |
| Fenêtre | **32 tranches ≈ 1 s** | La durée d'un son court ; une alarme continue remplit simplement la fenêtre |
| Signal | 32 tranches × 32 bandes = **1024 valeurs**, tranche après tranche | 32 points par axe, une puissance de 2 comme le conseille ST |
| Déclenchement | énergie d'une tranche > bruit de fond (moyenne glissante) + ~15 dB ; 4 tranches gardées d'avant (~130 ms) | Signaux **alignés sur le début du son**, comme les gestes de la baguette |
| Mise à l'échelle | dB ramenés entre 0 et 100, le maximum du signal à 100 | Insensible au volume et à la distance : le modèle apprend la **forme** du son |

**Classes** : `sonnette`, `alarme`, `frappe`, `applaudissement`, plus **`autre`**, et un seuil de confiance
comme `WAND_MIN_CONFIDENCE` dans la baguette.

### 3. Le firmware, sur le modèle de la baguette

Ajouter des fichiers à l'exemple de ST, sans modifier les siens :

| Fichier | Rôle |
|---|---|
| `app_config.h` | `APP_MODE` (0 collecte, 1 reconnaissance), fréquence, tailles, seuils |
| `audio_in.c` | Récupère les échantillons de l'exemple ST (demi-tampons DMA) et les range par tranches de 512 |
| `spectro.c` | FFT, énergie des 32 bandes, dB : **code portable, testé sur PC** avec des fichiers WAV |
| `sound_preproc.c` | Bruit de fond, déclenchement, pré-déclenchement, fenêtre de 32 tranches, mise à l'échelle : la logique de `wand_preproc.c` |
| `sound_ai.c` | Appel NanoEdge (`neai_classification`) : copie de `wand_ai.c` |
| `app.c` | Mode 0 : une ligne CSV par son détecté, « # En attente… » sinon. Mode 1 : `SON sonnette 92%  (…)`, LED |

### 4. La collecte, avec l'outil commun du dépôt

Un profil suffit, sans nouveau code Python. L'affichage **grille** de l'outil montre le spectrogramme comme
une image 32 × 32 (une ligne par tranche de temps, une colonne par bande) : on voit d'un coup d'œil le bip
régulier d'une alarme ou les impulsions d'une frappe.

```json
{
  "nom": "Sons de la maison",
  "classes": ["sonnette", "alarme", "frappe", "applaudissement", "autre"],
  "longueur": 1024,
  "affichage": {"type": "grille", "taille": 32, "trames": 1, "min": 10},
  "valeur_max": 100,
  "par_passe": 50,
  "attente": "En attente"
}
```

Viser **150 à 300 sons par classe**, en plusieurs séances, avec plusieurs sources (téléphones, enceintes,
vraies portes), distances et pièces. Garder une passe par classe pour la validation (`--out test`).

### 5. Le Studio
- Projet **N-Class classification**, cible STM32L476RG, capteur **Generic, 32 axes**, 32 points par axe.
- Benchmark, matrice de confusion (souvent `frappe` / `applaudissement`), validation sur les fichiers de test.
- Compiler avec Float ABI **hard** et FPU **fpv4-sp-d16**, puis intégrer `NanoEdgeAI.h` et `libneai.a`.

**Variante 18 (machine qui sonne faux)** : le même spectrogramme, mais calculé en continu (une fenêtre par
seconde, sans déclenchement), et un projet **Anomaly detection** : la carte apprend le bruit normal au
démarrage, comme la variante détection d'anomalie de la Sentinelle.

### 6. Planning indicatif pour le hackathon
1. **Matin** : exemple Microphones_Streaming de ST essayé (son écouté dans Audacity), puis niveau sonore
   affiché sur l'UART.
2. Spectrogramme et déclenchement ; collecte qui marche avec l'outil commun.
3. **Midi** : collecte des 5 classes, benchmark.
4. **Après-midi** : intégration, reconnaissance en direct, réglage des seuils.
5. **Bonus** : notification sur le PC ou le téléphone, ou une vibration pour la personne malentendante.

### Pièges à connaître
- **Conflits avec la librairie NanoEdge** : `libneai.a` contient déjà des fonctions `arm_…` de la CMSIS-DSP
  (FFT notamment). Ajouter en plus la CMSIS-DSP au projet peut provoquer des symboles en double à l'édition
  de liens : à vérifier tôt, avec une première librairie NanoEdge quelconque.
- **Sons trop longs ou trop lents** : une sonnerie à deux tons de plus d'une seconde est coupée. Allonger
  la fenêtre (64 tranches) si besoin, et revoir `longueur`.
- **Saturation** : un claquement de mains tout près du micro sature ; garder une distance minimale et
  vérifier que le spectrogramme ne « plafonne » pas.
- **Bruits parasites** : ronflement du secteur (50 Hz), ventilateur du PC, écho de la pièce. Les mettre dans
  `autre`, et collecter dans la pièce de la démonstration.
- **Vie privée** : rien n'est enregistré ni envoyé, seul un spectrogramme de 1 s est calculé sur la carte,
  puis oublié. C'est un bon argument à présenter.
