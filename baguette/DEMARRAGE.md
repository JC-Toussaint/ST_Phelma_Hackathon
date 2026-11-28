# Baguette magique pas à pas

La liste des opérations, dans l'ordre, du PC vide à une baguette qui reconnaît vos gestes. Chaque étape
renvoie à la section du [README](README.md) qui la détaille.

Il faut prévoir à peu près :
- 30 min à 1 h pour installer les logiciels (étape 0), à faire avant si possible ;
- 30 min pour le montage, le firmware et l'essai de la librairie fournie ;
- 1 h à 2 h pour la collecte (5 classes, plusieurs séances) ;
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
   Il faut aussi un terminal série (Tera Term, PuTTY…) pour voir les gestes reconnus.
3. **Récupérer le dépôt sur le disque Windows**, par exemple dans `Documents`, jamais dans
   `\\wsl.localhost\…` :
   ```
   git clone https://github.com/JC-Toussaint/ST_Phelma_Hackathon.git
   ```
4. **Lire [`CUBEIDE_IMPORT.md`](../CUBEIDE_IMPORT.md)** : le seul bon menu d'import, et comment corriger
   les erreurs de CubeIDE.

## 1. Montage (README, section 1)

- ⚠️ **Sortir les cartes de leur blister avec précaution** : les broches des connecteurs, sous les cartes,
  sont pointues et peuvent piquer les doigts. Tenir les cartes par les bords.
- Enficher la X-NUCLEO-IKS01A2 sur la NUCLEO-L476RG, puis brancher le câble USB (ST-LINK).
- **Marquer un repère « haut / avant »** sur la carte et la tenir **toujours de la même façon**.
- ⚠️ **Tous les gestes se font dans l'air** : ne jamais taper la carte sur la table ou un objet, la tenir
  fermement par les bords, ne pas la faire tournoyer par le câble.

## 2. Essayer tout de suite la librairie fournie (README, sections 2 et 5)

1. Dans CubeIDE : *File > Import > General > **Existing Projects into Workspace***, puis choisir le
   dossier `baguette/NUCLEO_L476RG_IKS01A2_BAGUETTE/`. **Ne pas** cocher « Copy projects into workspace ».
2. Vérifier qu'en haut, la configuration est **Debug**, et non « Default ».
3. Passer `APP_MODE` à **1** dans `Core/Inc/app_config.h`, compiler, puis flasher.
4. Ouvrir le terminal série à **115200 bauds**. Le menu affiche les 5 classes de la librairie.
5. Immobiliser la baguette (**LED allumée = prête**), puis faire un **cercle vif** (~0,5 s) dans le sens des
   aiguilles d'une montre, en partant du haut. Une ligne `GESTE cercle …` doit s'afficher.

Cette librairie n'a appris que 10 gestes par classe, d'une seule personne : il est normal qu'elle reconnaisse
mal certains de vos gestes. C'est tout l'objet des étapes suivantes.

## 3. Firmware de collecte (README, section 2)

Remettre `APP_MODE` à **0** (collecte) dans `Core/Inc/app_config.h`, compiler, puis flasher.

## 4. Collecte des données (README, section 3)

1. **Se mettre d'accord sur les gestes** (README, « Les gestes à faire ») : même tracé, même sens, même
   tenue de la baguette pour tout le monde.
2. **Fermer Tera Term**, puis lancer :
   ```
   python baguette/tools/wand_logger_gui.py
   ```
   Pour découvrir l'interface sans carte, ajouter `--demo`.
3. Choisir le port COM du ST-LINK, puis *Connecter*.
4. Pour chaque classe (touches `1` à `5`), appuyer sur **Espace**, puis enchaîner les gestes en
   immobilisant la baguette un instant entre deux. Une passe enregistre 50 gestes. Vérifier sur les
   courbes que chaque geste est **complet** (calme à droite) et ne **plafonne** pas (ni 0 ni 1000).
5. **Geste raté** : **Retour arrière** le retire, et la passe continue.
6. Faire **3 à 6 passes par classe, en plusieurs séances**, avec plusieurs personnes si possible, en variant
   la taille et la vitesse des gestes (jamais le sens). Viser **~150 gestes par classe**, `autre` compris :
   se gratter la tête, poser et reprendre la baguette, gestes hésitants…
7. **Garder une passe de chaque classe pour la validation** : relancer l'outil avec `--out test`. Ces
   fichiers vont dans `tools/test/` et ne serviront pas à l'entraînement.
   ```
   python baguette/tools/wand_logger_gui.py --out test
   ```

Les fichiers obtenus sont `tools/cercle.csv`, `tools/zigzag.csv`, `tools/frappe.csv`, `tools/v.csv` et
`tools/autre.csv`, plus ceux de `tools/test/`. Facultatif : `python baguette/tools/augmenter.py` en fait des
variantes dans `tools/augmente/` (README, « Augmenter les données »), sans remplacer la collecte.

## 5. Générer la librairie dans NanoEdge AI Studio (README, section 4)

1. Créer un projet **N-Class classification**, avec la cible **STM32L476RG** et le capteur **Generic,
   6 axes** (ni *Accelerometer*, ni *Multi-sensor*), 256 points par axe.
2. Importer les 5 fichiers (*Import from file*, séparateur virgule). Le Studio doit annoncer **256 points ×
   6 axes**. **Garder les noms de classe** : le firmware cherche `autre` pour les mouvements à ignorer.
3. Lancer le **benchmark**. Viser une précision équilibrée (*balanced accuracy*) d'au moins 90 %, et regarder
   la matrice de confusion.
4. **Valider** dans l'émulateur avec les fichiers de `tools/test/` : c'est le meilleur indicateur du
   comportement réel.
5. **Compiler** avec **Float ABI hard** et **FPU fpv4-sp-d16**. Le Studio produit un `.zip`.

## 6. Intégrer votre librairie (README, section 5)

1. Copier depuis le `.zip` dans `NUCLEO_L476RG_IKS01A2_BAGUETTE/` : `NanoEdgeAI.h` dans `Core/Inc/` et
   `libneai.a` dans `Core/Src/`, à la place de la librairie fournie.
2. Passer `APP_MODE` à **1**, faire *Project > Clean*, recompiler, puis flasher.
3. Fermer l'outil de collecte, ouvrir le terminal série à **115200 bauds**, et faire des gestes.

---

## Pièges à connaître

| Situation | Que faire |
|---|---|
| L'outil de collecte ne peut pas ouvrir le port COM | Le terminal série ou la console de CubeIDE le garde : les fermer. Un seul programme à la fois sur le port |
| CubeIDE affiche « Default », « make not found » ou « Not able to get mcu target » | Le projet a été mal importé : suivre [`CUBEIDE_IMPORT.md`](../CUBEIDE_IMPORT.md) |
| `ERREUR : init LSM6DSL` au démarrage | La X-NUCLEO-IKS01A2 est mal enfoncée (les secousses la font sortir) : la renfoncer |
| Rien ne s'affiche quand on fait un geste | Immobiliser la baguette un instant avant le geste (en reconnaissance : attendre que la LED s'allume), puis faire un geste plus franc |
| `GESTE ? pas reconnu (…)` ou mauvais geste reconnu | Le geste ne ressemble pas assez aux exemples appris : collecter des gestes comme ceux-là (vitesse, taille, tenue), puis régénérer la librairie |
| Benchmark parfait, reconnaissance médiocre sur la carte | Une seule séance de collecte, ou des gestes coupés ou saturés : voir le « Piège » du README, section 4 |
| Votre librairie a disparu après un `git stash` ou un `git checkout` | Ces commandes remettent la librairie fournie : **garder une copie** de votre `NanoEdgeAI.h` et de votre `libneai.a` hors du dépôt |
| Envie de régénérer le code depuis le `.ioc` | À éviter : voir [`DIFFERENCES_ST.md`](DIFFERENCES_ST.md), section 4 |
