# Pierre-Feuille-Ciseaux pas à pas

La liste des opérations, dans l'ordre, du PC vide à la première partie contre le STM32. Chaque étape
renvoie à la section du [README](README.md) qui la détaille.

Il faut prévoir à peu près :
- 30 min à 1 h pour installer les logiciels (étape 0), à faire avant si possible ;
- 30 min pour le montage et le firmware ;
- 30 min à 1 h pour la collecte ;
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
   Il faut aussi un terminal série (Tera Term, PuTTY…) pour jouer.
3. **Récupérer le dépôt sur le disque Windows**, par exemple dans `Documents`, jamais dans
   `\\wsl.localhost\…` :
   ```
   git clone https://github.com/JC-Toussaint/ST_Phelma_Hackathon.git
   ```
4. **Lire [`CUBEIDE_IMPORT.md`](../CUBEIDE_IMPORT.md)** : le seul bon menu d'import, et comment corriger
   les erreurs de CubeIDE.

## 1. Montage (README, section 1)

- Enficher la X-NUCLEO-53L5A1 sur la NUCLEO-L476RG. Seul le **capteur central** est utilisé : laisser les
  satellites débranchés.
- Poser la carte **à plat, capteur vers le plafond**, puis la brancher en USB (ST-LINK).
- La main se joue **paume vers le capteur, à 10–20 cm au-dessus**. Au-delà de 20 cm, elle est ignorée.

## 2. Firmware de collecte (README, section 2)

1. Dans CubeIDE : *File > Import > General > **Existing Projects into Workspace***, puis choisir le
   dossier `pierre_feuille_ciseaux/NUCLEO_L476RG_TOF_PFC/`. **Ne pas** cocher « Copy projects into
   workspace ».
2. Vérifier qu'en haut, la configuration est **Debug**, et non « Default ».
3. Laisser `APP_MODE 0` (collecte) dans `Inc/app_config.h`, compiler, puis flasher.

## 3. Collecte des données (README, section 3, « Utiliser l'interface de collecte »)

1. **Fermer Tera Term**, puis lancer :
   ```
   python pierre_feuille_ciseaux/tools/rps_logger_gui.py
   ```
   Pour découvrir l'interface sans carte, ajouter `--demo`.
2. Choisir le port COM du ST-LINK, puis *Connecter*.
3. **Vérifier le sens** : si la tache de la grille bouge à l'envers de la main, cocher ou décocher
   *miroir ↔* ou *miroir ↕*. Cela ne change que l'affichage.
4. Pour chaque classe, choisir la touche (`1` pierre, `2` feuille, `3` ciseaux), puis appuyer sur
   **Espace**. Une passe enregistre 300 signaux en ~60 s. Pendant la passe, faire varier la hauteur, la
   position, la rotation et la main, sans changer de geste. Une passe ratée se retire avec *Annuler la
   dernière passe*.
5. Faire **2 ou 3 passes par classe**, si possible avec plusieurs personnes.
6. **Garder une passe de chaque classe pour la validation** : relancer l'outil avec `--out test`, puis
   faire une passe par classe. Ces fichiers vont dans `tools/test/` et ne serviront pas à l'entraînement.
   ```
   python pierre_feuille_ciseaux/tools/rps_logger_gui.py --out test
   ```

Les fichiers obtenus sont `tools/pierre.csv`, `tools/feuille.csv` et `tools/ciseaux.csv`, plus ceux de
`tools/test/`.

## 4. Générer la librairie dans NanoEdge AI Studio (README, section 4)

1. Créer un projet **N-Class classification**, avec la cible **STM32L476RG** et le capteur **Generic,
   1 axe**.
2. Importer `pierre.csv`, `feuille.csv` et `ciseaux.csv` (*Import from file*, séparateur virgule). **Garder
   les noms de classe** pierre, feuille et ciseaux : le firmware s'en sert.
3. Lancer le **benchmark**. Viser une précision équilibrée (*balanced accuracy*) d'au moins 90 %. Si deux
   classes sont confondues, collecter davantage pour elles et relancer.
4. **Valider** dans l'émulateur avec les fichiers de `tools/test/`.
5. **Compiler** avec **Float ABI hard** et **FPU fpv4-sp-d16**. Le Studio produit un `.zip`.

## 5. Intégrer la librairie et jouer (README, sections 4 et 5)

1. Copier depuis le `.zip` dans `NUCLEO_L476RG_TOF_PFC/` : `Inc/NanoEdgeAI.h` et `Src/libneai.a`.
2. Passer `APP_MODE` à **1** dans `Inc/app_config.h`, recompiler, puis flasher.
3. Fermer l'outil de collecte, puis ouvrir le terminal série à **115200 bauds**.
4. Appuyer sur **B1** (bouton bleu) ou sur **Espace** pour jouer une manche.
5. La touche `d` affiche en direct la probabilité de pierre, feuille et ciseaux : pratique pour voir si
   le modèle hésite, et quelles données compléter.

---

## Pièges à connaître

| Situation | Que faire |
|---|---|
| L'outil de collecte ne peut pas ouvrir le port COM | Le terminal série ou la console de CubeIDE le garde : les fermer. Un seul programme à la fois sur le port |
| CubeIDE affiche « Default », « make not found » ou « Not able to get mcu target » | Le projet a été mal importé : suivre [`CUBEIDE_IMPORT.md`](../CUBEIDE_IMPORT.md) |
| `ERREUR : init NanoEdge (classes pierre/feuille/ciseaux introuvables ?)` au démarrage | La librairie de ST est encore là, ou les classes ne s'appellent pas pierre, feuille et ciseaux |
| Bonne reconnaissance dans le Studio, mauvaise sur la carte | Collecte faite dans d'autres conditions (hauteur, personne, éclairage) : refaire une passe dans les conditions de jeu, puis régénérer la librairie |
| Votre librairie a disparu après un `git stash` ou un `git checkout` | Ces commandes remettent la librairie de ST : **garder une copie** de votre `NanoEdgeAI.h` et de votre `libneai.a` hors du dépôt |
| Envie de régénérer le code depuis le `.ioc` | À éviter : voir [`DIFFERENCES_ST.md`](DIFFERENCES_ST.md), section 5 |
