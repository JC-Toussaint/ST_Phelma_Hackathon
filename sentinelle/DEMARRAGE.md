# Sentinelle pas à pas

La liste des opérations, dans l'ordre, du PC vide à la carte qui surveille sa scène. Chaque étape renvoie
à la section du [README](README.md) qui la détaille.

Il faut prévoir à peu près :
- 30 min à 1 h pour installer les logiciels (étape 0), à faire avant si possible ;
- 30 min pour le montage et le firmware ;
- 30 min à 1 h pour la collecte, car une passe dure ~2 min 30 ;
- de quelques minutes à une demi-heure pour le benchmark dans le Studio.

---

## 0. Avant de commencer

1. **Télécharger et installer NanoEdge AI Studio**, si possible **avant** le hackathon :
   - le logiciel est gratuit, sur le site de ST (st.com, chercher « NanoEdge AI Studio »), pour Windows ou
     Linux ;
   - le téléchargement demande en général un **compte myST**, gratuit lui aussi : le créer à l'avance ;
   - lancer le Studio une fois après l'installation pour vérifier qu'il démarre.
2. **Installer STM32CubeIDE** (st.com, avec le même compte), **Python 3**, puis :
   ```
   pip install pyserial
   ```
   Un terminal série (Tera Term, PuTTY…) est utile pour suivre la surveillance.
3. **Récupérer le dépôt sur le disque Windows**, par exemple dans `Documents`, jamais dans
   `\\wsl.localhost\…` :
   ```
   git clone https://github.com/JC-Toussaint/ST_Phelma_Hackathon.git
   ```
4. **Lire [`CUBEIDE_IMPORT.md`](../CUBEIDE_IMPORT.md)** : le seul bon menu d'import, et comment corriger
   les erreurs de CubeIDE.

## 1. Montage et installation (README, section 1)

- ⚠️ **Sortir les cartes de leur blister avec précaution** : les broches des connecteurs, sous les cartes,
  sont pointues et peuvent piquer les doigts. Tenir les cartes par les bords.
- Enficher la X-NUCLEO-53L5A1 sur la NUCLEO-L476RG, puis la brancher en USB (ST-LINK).
- Poser la carte **face à la scène** (bureau, étagère, porte), à 0,5–3 m, et **la fixer** : si elle bouge,
  toute la scène change.

## 2. Firmware de collecte (README, section 2)

1. Dans CubeIDE : *File > Import > General > **Existing Projects into Workspace***, puis choisir le
   dossier `sentinelle/NUCLEO_L476RG_TOF_SENTINELLE/`. **Ne pas** cocher « Copy projects into workspace ».
2. Vérifier qu'en haut, la configuration est **Debug**, et non « Default ».
3. Laisser `APP_MODE 0` (collecte) dans `Inc/app_config.h`, compiler, puis flasher.

## 3. Collecte des données (README, section 4)

1. **Fermer Tera Term**, puis lancer :
   ```
   python sentinelle/tools/sentinel_logger_gui.py
   ```
   Pour découvrir l'interface sans carte, ajouter `--demo`.
2. Choisir le port COM du ST-LINK, puis *Connecter* : la grille s'anime (~2 signaux/s).
3. Scène normale, personne devant : appuyer sur **R** (mémoriser la référence). Passer devant le capteur
   et déplacer un objet : la vue **Écarts** doit montrer des `+` / `−` au bon endroit.
4. Classe **`1` normal**, **Espace**, puis **sortir du champ** pendant le compte à rebours. Ne rien
   toucher pendant ~2 min 30.
5. Classe **`2` anomalie**, **Espace**, puis provoquer des changements variés pendant toute la passe :
   passer devant, ajouter, retirer ou déplacer des objets, ouvrir un tiroir.
6. Faire **2 ou 3 passes par classe** (lumière allumée ou éteinte, autres heures). Une passe ratée se
   retire avec *Annuler la dernière passe*.
7. **Garder une passe de chaque classe pour la validation** : relancer l'outil avec `--out test`, puis
   faire une passe par classe :
   ```
   python sentinelle/tools/sentinel_logger_gui.py --out test
   ```

Les fichiers obtenus sont `tools/normal.csv` et `tools/anomalie.csv`, plus ceux de `tools/test/`.

## 4. Générer la librairie dans NanoEdge AI Studio (README, section 5)

1. Créer un projet **Anomaly detection**, avec la cible **STM32L476RG** et le capteur **Generic, 1 axe**.
2. Importer `normal.csv` en *Regular signals* et `anomalie.csv` en *Abnormal signals* (*Import from file*,
   séparateur virgule).
3. Lancer le **benchmark**. Viser une précision équilibrée (*balanced accuracy*) d'au moins 90 %. Sinon,
   ajouter des passes « normal » plus variées et relancer.
4. **Valider** dans l'émulateur : apprentissage avec des signaux normaux, puis détection sur les fichiers
   de `tools/test/`. Noter le nombre minimal d'apprentissages conseillé : s'il dépasse 40, le reporter
   dans `SENT_LEARN_SIGNALS` (`Inc/app_config.h`).
5. **Compiler** avec **Float ABI hard** et **FPU fpv4-sp-d16**. Le Studio produit un `.zip`.

Une librairie **N-Class classification** avec les classes `normal` et `anomalie` marche aussi, mais le
modèle est alors figé : la carte n'apprend plus sur place (README, fin de la section 5).

## 5. Intégrer la librairie et surveiller (README, section 6)

1. Copier depuis le `.zip` dans `NUCLEO_L476RG_TOF_SENTINELLE/` : `Inc/NanoEdgeAI.h` et `Src/libneai.a`.
2. Passer `APP_MODE` à **1** dans `Inc/app_config.h`, recompiler, puis flasher.
3. Au démarrage, la LED clignote lentement : **sortir du champ**. Elle reste allumée ~21 s pendant
   l'apprentissage, puis s'éteint : la surveillance commence.
4. Facultatif : terminal série à **115200 bauds**, pour lire les alarmes et la carte des changements
   (`v` similarité en continu, `c` carte, `h` menu).
5. Un changement désormais normal : **B1 appui court**, pour l'ajouter à la normale. Carte déplacée :
   **B1 appui long**, pour tout réapprendre.

---

## Pièges à connaître

| Situation | Que faire |
|---|---|
| L'outil de collecte ne peut pas ouvrir le port COM | Le terminal série ou la console de CubeIDE le garde : les fermer. Un seul programme à la fois sur le port |
| CubeIDE affiche « Default », « make not found » ou « Not able to get mcu target » | Le projet a été mal importé : suivre [`CUBEIDE_IMPORT.md`](../CUBEIDE_IMPORT.md) |
| `ERREUR : init NanoEdge (pas de classe "normal" dans la librairie) (code -2)` au démarrage | La librairie de ST est encore là : générer et copier la vôtre |
| Fausses alarmes à répétition | La carte a bougé, ou l'apprentissage s'est fait avec quelqu'un dans le champ : refixer la carte, puis B1 appui long |
| Votre librairie a disparu après un `git stash` ou un `git checkout` | Ces commandes remettent la librairie de ST : **garder une copie** de votre `NanoEdgeAI.h` et de votre `libneai.a` hors du dépôt |
| Envie de régénérer le code depuis le `.ioc` | À éviter : voir [`DIFFERENCES_ST.md`](DIFFERENCES_ST.md), section 5 |
