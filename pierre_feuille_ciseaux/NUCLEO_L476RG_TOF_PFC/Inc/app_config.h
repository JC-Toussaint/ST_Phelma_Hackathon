/**
 * app_config.h - Configuration du projet Pierre-Feuille-Ciseaux (VL53L5CX + NanoEdge AI)
 *
 * Cible : NUCLEO-L476RG + X-NUCLEO-53L5A1 (capteur central)
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ------------------------------------------------------------------------- */
/* Mode de fonctionnement                                                    */
/* ------------------------------------------------------------------------- */
#define APP_MODE_DATALOG   0   /* Envoie les signaux prétraités en CSV sur l'UART (collecte NanoEdge) */
#define APP_MODE_GAME      1   /* Joue à Pierre-Feuille-Ciseaux avec la librairie NanoEdge générée     */

#ifndef APP_MODE
#define APP_MODE           APP_MODE_DATALOG
#endif

/* ------------------------------------------------------------------------- */
/* Capteur                                                                   */
/* ------------------------------------------------------------------------- */
#define TOF_GRID           8                      /* 8x8 zones                     */
#define TOF_ZONES          (TOF_GRID * TOF_GRID)  /* 64                            */
#define TOF_FREQ_HZ        15                     /* max en 8x8                    */

#define TOF_TARGET_CLOSEST 1                      /* 1 = la cible la plus proche (la main) plutôt que la plus forte */
/* Brochage (PWR_EN=PB0, LPn=PB4, I2C_RST=PB3, INT=PA4) : géré par main.c / .ioc
 * du projet ST et par le BSP X-CUBE-TOF1, rien à configurer ici.               */

/* Liaison série : USART2 (port COM virtuel du ST-LINK) à 115200 bauds UNIQUEMENT,
 * tel que main.c (projet ST) l'initialise. Le firmware ne le reconfigure plus.  */
#define APP_UART_BAUD      115200

/* ------------------------------------------------------------------------- */
/* Prétraitement                                                             */
/* ------------------------------------------------------------------------- */
#define PRE_NEAR_MM            40    /* en dessous : trop près, ignoré                      */
#define PRE_FAR_MM             200   /* au-delà : considéré comme fond (plafond, mur...). Voir REGLAGE_DISTANCE.md */
#define PRE_DEPTH_WINDOW_MM    120   /* profondeur gardée derrière le point le plus proche  */
#define PRE_MIN_HAND_ZONES     5     /* nb mini de zones "main" pour dire qu'une main est là */
#define PRE_RECENTER           1     /* 1 = recentre la main dans la grille (robustesse)    */
#define PRE_MAX_SHIFT          2     /* décalage max du recentrage (zones)                  */

/* Un signal NanoEdge = PRE_FRAMES_PER_SIGNAL trames 8x8 consécutives (comme
 * SUCCESSIVE_MATRIX = 8 dans le projet ST) => 512 valeurs, la taille attendue
 * par la librairie NanoEdge modèle du projet ST.                              */
#define PRE_FRAMES_PER_SIGNAL  8
#define SIGNAL_LEN             (TOF_ZONES * PRE_FRAMES_PER_SIGNAL)   /* 512 valeurs */

/* Mode COLLECTE : un signal envoyé toutes les PRE_DATALOG_EVERY trames (fenêtre glissante).
 * Une ligne fait 1 à 2 Ko (512 entiers 0..100) ; à 115200 bauds (11,5 Ko/s) elle prend jusqu'à
 * 180 ms, et l'envoi est bloquant. Avec 3 trames (200 ms à 15 Hz), aucune trame du capteur n'est
 * manquée : les 8 trames d'un signal restent consécutives, comme en mode JEU. 5 signaux/s.      */
#define PRE_DATALOG_EVERY      3

/* ------------------------------------------------------------------------- */
/* Jeu                                                                       */
/* ------------------------------------------------------------------------- */
#define GAME_COUNTDOWN_STEP_MS  700   /* "Pierre..." "Feuille..." "Ciseaux..." "CHIFOUMI !" */
#define GAME_CAPTURE_MS         2500  /* fenêtre pour montrer son geste                      */
#define GAME_VOTES_NEEDED       5     /* nb de classifications concordantes à cumuler        */
#define GAME_MIN_CONFIDENCE     0.60f /* proba mini NanoEdge pour qu'un vote compte          */
#define GAME_EXPLORATION_PCT    20    /* stratégie adaptative : % de coups aléatoires        */

/* Ancienne API NanoEdge uniquement (NanoEdgeAI.h sans NEAI_INPUT_SIGNAL_LENGTH) :
 * type de l'identifiant de classe de neai_classification() (int ou uint16_t). */
#define NEAI_ID_TYPE            int

#endif /* APP_CONFIG_H */
