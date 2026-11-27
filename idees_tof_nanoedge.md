# Idées de projets : VL53L5CX (8×8) + NanoEdge AI + STM32L476

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

### 4. Sentinelle qui apprend sa scène sur place ✅ (réalisée : voir `sentinelle/`)
- Grâce à l'**apprentissage embarqué** (anomalie), la carte apprend en quelques minutes ce qui est
  « normal » devant elle (bureau, porte, étagère), sans PC.
- Elle signale ensuite toute anomalie : objet déplacé, intrusion, tiroir ouvert. Réapprentissage sur bouton.

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
- **+ Sentinelle (4)** pour montrer l'apprentissage embarqué de NanoEdge, souvent un critère des jurys ST.

Les deux partagent le même socle firmware : acquisition VL53L5CX → prétraitement → CSV UART pour la collecte.
