# Idées de projets NanoEdge AI + STM32L476 : capteur ToF VL53L5CX (8×8), puis accéléromètre (IKS01A2 / IKS01A3)

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

### 13. Baguette magique (N-classes)
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

## Recommandation et réutilisation

- **Ventilateur (10)** : le plus sûr pour un hackathon, et la vitrine de l'apprentissage sur la carte.
- **Baguette magique (13)** : le plus ludique.
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

Partir de l'application **DataLogTerminal** du pack X-CUBE-MEMS1 (9.6.0 pour l'IKS01A2, 11.2.0 pour
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
