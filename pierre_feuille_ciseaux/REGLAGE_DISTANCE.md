# Changer la distance maximale de la main (Pierre-Feuille-Ciseaux)

Réglage actuel : **40 à 200 mm**. La main doit être **entre 4 et 20 cm** au-dessus du capteur ; tout
ce qui est plus loin compte comme du **fond** (plafond, mur, personne qui passe) et n'est jamais pris
pour la main.

| Date | Distance maximale | Raison |
|---|---|---|
| origine | 400 mm | réglage de l'exemple ST |
| 2026-11-26 | 800 mm | essai à plus grande hauteur |
| 2026-11-26 | **200 mm** | main tenue près du capteur : le fond et les passants sont ignorés |

## Où modifier

Deux valeurs **doivent rester égales**. Sinon, la compilation s'arrête sur un `#error` qui le dit
(`MIN_THRESHOLD / MAX_THRESHOLD doivent etre egaux a PRE_NEAR_MM / PRE_FAR_MM`).

| Fichier | Réglage | Rôle |
|---|---|---|
| `NUCLEO_L476RG_TOF_PFC/Inc/app_config.h` | `#define PRE_FAR_MM 200` | prétraitement (`Src/tof_preproc.c`) : au-delà, la zone est du fond |
| `NUCLEO_L476RG_TOF_PFC/X-CUBE-TOF1/App/app_x-cube-tof1.c` | `#define MAX_THRESHOLD 200` | réglage hérité de l'exemple ST ; donne `ITConfig.HighThreshold` |

La distance minimale se change de la même façon : `PRE_NEAR_MM` et `MIN_THRESHOLD`, 40 mm aujourd'hui.
Le capteur ne mesure pas en dessous de 40 mm.

Un autre endroit, sans effet sur la carte :

| Fichier | Quoi | Pourquoi |
|---|---|---|
| `tools/rps_logger_gui.py`, classe `DemoSource` | texte « entre 40 et 200 mm » | message affiché en mode `--demo` seulement |

Les tests sur PC (`tests/test_host.c`) suivent le réglage tout seuls. Leurs mains simulées sont placées au
milieu de la plage (`HAND_MM`), et leur plafond à 2500 mm. Il n'y a rien à y changer, sauf si la distance
maximale dépasse 2500 mm.

Après le changement : recompiler, flasher, puis lancer `cd tests && make test`.

## À quoi s'attendre selon la distance

Le capteur voit environ 45° × 45° en 8 × 8 zones : une zone couvre à peu près **0,1 × la distance**, et
toute la grille environ **0,8 × la distance**.

| Hauteur de la main | Champ du capteur | Taille d'une zone | Zones couvertes par une main (~10 × 18 cm) |
|---|---|---|---|
| 100 mm | ~8 cm | ~1 cm | toute la grille : on ne voit qu'une partie de la main |
| 200 mm | ~16 cm | ~2 cm | presque toute la grille |
| 400 mm | ~33 cm | ~4 cm | 10 à 15 |
| 800 mm | ~66 cm | ~8 cm | 3 à 5 |
| 1500 mm | ~1,2 m | ~15 cm | 1 à 2 : pierre, feuille et ciseaux ne se distinguent plus |

### Avec 200 mm (réglage actuel)

- **Avantage** : tout ce qui est à plus de 20 cm est ignoré (plafond, visage, personnes autour). Il n'y a
  plus de fausse détection quand personne ne joue.
- **La main remplit presque tout le champ** : à 20 cm, la grille couvre environ 16 × 16 cm. Tenir la main
  plutôt vers **15 à 20 cm**. Plus près, on ne voit qu'une partie de la main, surtout pour « feuille ».
- **Le poignet et l'avant-bras** sont aussi à moins de 20 cm. Ils sont écartés s'ils sont à plus de
  `PRE_DEPTH_WINDOW_MM` (12 cm) derrière le point le plus proche ; sinon, présenter la main bien à plat
  au-dessus du capteur, poignet relevé.
- **Librairie NanoEdge** : une librairie entraînée avec des mains vers 20–30 cm reconnaîtra mal des mains
  tenues à 10–20 cm. Collecter à cette hauteur, puis réentraîner.

### Avec une distance plus grande

- **Main lointaine peu vue** : le firmware exige `PRE_MIN_HAND_ZONES` zones (5) pour dire « main
  présente ». Vers 800 mm, une main en couvre 3 à 5 : essayer 3 (`app_config.h`).
- **Objets proches pris pour une main** : sans main, tout objet plus proche que la distance maximale
  déclenche des signaux.
- `PRE_DEPTH_WINDOW_MM` (120 mm) ne dépend pas de la distance : il garde ce qui se trouve à moins de 12 cm
  derrière le point le plus proche.
