# Collecte NanoEdge commune à tous les projets

En mode collecte, toutes les cartes du dépôt envoient le même genre de texte sur la liaison série :

```
# En attente d'une main...          <- message pour l'humain (commence par #)
0,0,12,57,100,...,0                 <- un signal = une ligne CSV de nombres
```

NanoEdge AI Studio (classification N classes) attend **un fichier CSV par classe**, une ligne par signal.
`collecte_nanoedge.py` fait cette collecte pour n'importe quel projet. Ce qui est propre au projet est
décrit dans un **profil** JSON : noms des classes, longueur du signal, façon de l'afficher.

```
outils/
├── collecte_nanoedge.py   programme : interface graphique (défaut) ou ligne de commande
├── collecte_coeur.py      cœur sans interface : profil, lignes, fichiers par classe, sources
└── tests/                 make test : sans carte (fausse carte, trois types d'affichage)
```

## Utilisation

```bash
pip install pyserial
python outils/collecte_nanoedge.py --profil pierre_feuille_ciseaux/tools/profil_collecte.json
python outils/collecte_nanoedge.py --profil ... --demo             # données simulées, sans carte (CSV dans demo/)
python outils/collecte_nanoedge.py --profil ... --cli --port COM5  # collecte guidée dans le terminal
python outils/collecte_nanoedge.py --profil ... --view --port COM5 # visualiser seulement
```

Options : `--out DOSSIER` (relatif au dossier du profil), `--classes a b` (une partie des classes),
`--count N` (signaux par passe), `--len N` (longueur attendue, 0 = ne pas vérifier), `--baud N` (débit
autre que celui du profil).

**Interface graphique** : choix du port, signal en direct, une classe par touche (1 à 9), passe avec
compte à rebours (Espace), progression, nombre de signaux par fichier, **suppression du dernier
signal** (*Suppr. dernier*, touche Retour arrière : pendant la passe, qui continue, ou juste après, signal par
signal sans remonter avant le début de la passe), **annulation de la dernière passe**, avec la durée restante. Elle affiche aussi les messages de la carte ; le message d'attente qui se répète remplace la
ligne précédente au lieu de s'empiler. Les lignes de mauvaise longueur sont signalées : le profil ou le
mode de la carte est alors incorrect.

**Fichiers** : `<classe>.csv` dans le dossier du profil, complétés passe après passe, directement
importables dans le Studio (*Import from file*, séparateur virgule).

**Démo** (`--demo`) : une fausse carte simulée dans le programme. Elle alterne 2 s « rien devant le
capteur » (messages d'attente ; pas de pause si `attente` est vide) et 4 s de signaux pour chaque classe,
dans un ordre tiré au hasard, au rythme `demo_signaux_par_s`. Les signaux ont le format du profil : une
tache qui dérive, de taille différente selon la classe, pour une grille (devant un décor fixe si
`proche` = `petit`) ; des sinusoïdes pour les courbes. Ce ne sont pas de vraies
mesures, et la démo ignore la classe sélectionnée. **Sans `--out`, les fichiers de la démo vont dans le
sous-dossier `demo/`** (ignoré par git), jamais avec les vraies données ; la fenêtre le rappelle.

## Le profil

Exemple : le profil de Pierre-Feuille-Ciseaux (`pierre_feuille_ciseaux/tools/profil_collecte.json`) :

```json
{
  "nom": "Pierre-Feuille-Ciseaux",
  "classes": ["pierre", "feuille", "ciseaux"],
  "longueur": 512,
  "affichage": {"type": "grille", "taille": 8, "trames": 8, "min": 20, "miroir_gauche_droite": true, "miroir_haut_bas": false},
  "valeur_max": 100,
  "par_passe": 300,
  "bauds": 115200,
  "dossier": ".",
  "attente": "En attente",
  "conseil": "pendant une passe, varie la hauteur (10–20 cm), la position...",
  "demo_signaux_par_s": 5
}
```

| Champ | Rôle | Défaut |
|---|---|---|
| `nom` | titre de la fenêtre | « Collecte NanoEdge » |
| `classes` | noms des classes, qui sont aussi les noms des fichiers CSV | obligatoire |
| `longueur` | valeurs par signal ; les autres lignes sont ignorées et signalées | 0 (pas de vérification) |
| `affichage` | voir ci-dessous | `{"type": "brut"}` |
| `valeur_max` | plus grande valeur d'une mesure (échelle des couleurs et des courbes) | 100 |
| `par_passe` | signaux enregistrés par passe | 300 |
| `bauds` | débit de la liaison série | 115200 |
| `dossier` | dossier des CSV, relatif au fichier de profil | `.` |
| `attente` | début du message que la carte répète quand il n'y a rien à mesurer ; `""` si la carte envoie des signaux en continu | « En attente » |
| `conseil` | texte affiché pendant la collecte ; ou un texte par classe : `{"normal": "…", "anomalie": "…"}` | aucun |
| `import` | comment importer les CSV dans NanoEdge AI Studio (affiché au lancement et à la fin) | « une classe par fichier » |
| `demo_signaux_par_s` | rythme des signaux simulés (`--demo`) | 5 |

**Trois types d'affichage :**

| `affichage` | Pour | Signal attendu |
|---|---|---|
| `{"type": "grille", "taille": 8, "trames": 8, "min": 20, "miroir_gauche_droite": true}` | capteur matriciel (ToF 8×8) | `trames` × `taille`² valeurs, trame après trame. `min` : plus petite valeur d'une zone active (0 = fond). `miroir_gauche_droite` / `miroir_haut_bas` : retournent l'**affichage** seulement (voir ci-dessous) |
| `{"type": "courbes", "axes": ["x", "y", "d", "s"], "instants": 32, "ordre": "entrelace"}` | grandeurs suivies dans le temps (trajectoire d'une main, accéléromètre de la baguette…) | `instants` × nombre d'axes ; `entrelace` = x0,y0,d0,s0,x1… ; `par_axe` = x0…x31,y0… |
| `{"type": "brut"}` | tout le reste | une courbe des valeurs |

**Options de la grille :**

| Option | Rôle | Défaut |
|---|---|---|
| `taille`, `trames` | grille `taille`×`taille`, `trames` trames par signal | 8, 1 |
| `min` | plus petite valeur d'une zone active (avec `proche` = `grand`) | 0 |
| `proche` | `"grand"` : une grande valeur est proche (PFC : 100 = sommet de la main) ; `"petit"` : la valeur est une distance (Sentinelle). Le proche est toujours affiché clair | `"grand"` |
| `fond` | valeur d'une zone qui ne voit rien, affichée vide | 0 si `proche` = `grand`, `valeur_max` sinon |
| `mm_par_unite` | millimètres par unité : ajoute la case **Distances** (D, distances en cm) et « objet le plus proche » | aucun |
| `ecart_min` | ajoute **Mémoriser la référence** (R) et la vue **Écarts** (E) : zones qui ont changé de plus de `ecart_min` unités par rapport à la scène mémorisée, `+` rouge plus proche (apparu), `−` bleu plus loin (disparu). Calculé sur la trame moyenne du signal | aucun |
| `miroir_gauche_droite`, `miroir_haut_bas` | miroirs d'affichage, voir ci-dessous | `false` |

Exemple, la Sentinelle (valeur = 100 × distance / 3000 mm, 100 = pas de cible, changement à partir de
120 mm comme dans le firmware) :

```json
"affichage": {"type": "grille", "taille": 8, "trames": 8, "proche": "petit", "fond": 100,
              "mm_par_unite": 30, "ecart_min": 4}
```

La cohérence entre `longueur` et l'affichage est vérifiée au chargement, avec un message clair. Une clé
inconnue, par exemple une faute de frappe comme `mirroir_gauche_droite`, est **signalée** au lancement, dans le
terminal et dans les messages de la fenêtre, avec la liste des clés possibles : elle n'est jamais ignorée
en silence.

**Miroirs d'affichage (grille).** Un capteur tourné vers le haut voit la main **par en dessous** : son
image brute paraît inversée à qui regarde la carte par au-dessus, comme si la main était derrière la carte.
`miroir_gauche_droite` échange la gauche et la droite (symétrie par rapport à l'axe **vertical** de la
grille : la colonne x s'affiche en 7 − x) ; `miroir_haut_bas` échange le haut et le bas (symétrie par
rapport à l'axe **horizontal** : la ligne y s'affiche en 7 − y) ; les deux ensemble font un demi-tour. Les deux cases « miroir ↔ » et
« miroir ↕ » sous la grille font la même chose en direct. **Seul l'affichage change** : les signaux
enregistrés restent exactement ceux de la carte, et NanoEdge n'y voit aucune différence. Pour trouver le
bon réglage : déplacer la main vers sa droite, puis vers l'avant, et cocher ce qu'il faut pour que l'écran
suive ; reporter ensuite le résultat dans le profil.

## Ajouter un projet

1. Écrire `<projet>/tools/profil_collecte.json`. Le guide détaillé, qui déduit chaque champ du firmware,
   est l'exemple de Pierre-Feuille-Ciseaux : [pierre_feuille_ciseaux/tools/README.md](../pierre_feuille_ciseaux/tools/README.md).
2. Essayer avec `--demo`, puis avec la carte.
3. Facultatif : un lanceur de trois lignes dans le projet, comme `pierre_feuille_ciseaux/tools/rps_logger_gui.py`,
   pour garder une commande courte.

**État** : utilisé par Pierre-Feuille-Ciseaux (`rps_logger.py`, `rps_logger_gui.py`), la Sentinelle
(`sentinel_logger.py`, `sentinel_logger_gui.py`) et la Baguette magique (`wand_logger.py`,
`wand_logger_gui.py`, affichage en courbes), dont les scripts sont des lanceurs de cet outil. Un nouveau
projet n'a besoin que de son profil.
