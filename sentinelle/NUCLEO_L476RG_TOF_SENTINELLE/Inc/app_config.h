/**
 * app_config.h - Configuration du projet Sentinelle (VL53L5CX + NanoEdge AI, apprentissage embarqué)
 *
 * Cible : NUCLEO-L476RG + X-NUCLEO-53L5A1 (capteur central)
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ------------------------------------------------------------------------- */
/* Mode de fonctionnement                                                    */
/* ------------------------------------------------------------------------- */
#define APP_MODE_DATALOG   0   /* Envoie les signaux "scène" en CSV sur l'UART (collecte pour le benchmark) */
#define APP_MODE_SENTINEL  1   /* Apprentissage sur place + surveillance (librairie NanoEdge générée)       */

#ifndef APP_MODE
#define APP_MODE           APP_MODE_DATALOG
#endif

/* ------------------------------------------------------------------------- */
/* Capteur                                                                   */
/* ------------------------------------------------------------------------- */
#define TOF_GRID           8
#define TOF_ZONES          (TOF_GRID * TOF_GRID)
#define TOF_FREQ_HZ        15

#define TOF_TARGET_CLOSEST 0                      /* 0 = cible la plus forte (défaut du capteur) */
/* Brochage (PWR_EN=PB0, LPn=PB4, I2C_RST=PB3, INT=PA4) : géré par main.c / .ioc
 * du projet ST et par le BSP X-CUBE-TOF1, rien à configurer ici.               */

/* Liaison série : 115200 bauds comme le projet ST (suffisant : ~2 signaux/s). */
#define APP_UART_BAUD      115200

/* ------------------------------------------------------------------------- */
/* Signal                                                                    */
/* ------------------------------------------------------------------------- */
#define SENT_MAX_MM             3000  /* distances écrêtées ; "pas de cible" = SENT_MAX_MM */
/* Un signal = SENT_FRAMES_PER_SIGNAL trames 8x8 consécutives, SANS recouvrement
 * (comme SUCCESSIVE_MATRIX = 8 dans le projet ST) => 512 valeurs.             */
#define SENT_FRAMES_PER_SIGNAL  8
#define SENT_SIGNAL_LEN         (TOF_ZONES * SENT_FRAMES_PER_SIGNAL)   /* 512 valeurs */

/* ------------------------------------------------------------------------- */
/* Apprentissage et surveillance                                             */
/* ------------------------------------------------------------------------- */
#define SENT_LEARN_SIGNALS      40    /* signaux appris par session (~21 s à 15 Hz)               */
#define SENT_LEARN_DELAY_MS     5000  /* délai pour sortir du champ avant l'apprentissage         */
#define SENT_THRESHOLD          90    /* similarité (0..100) en dessous : signal anormal          */
#define SENT_CONFIRM            3     /* signaux consécutifs pour lever / retirer l'alarme        */
#define SENT_LONG_PRESS_MS      1500  /* B1 : appui long = tout réapprendre, court = ajouter      */

/* Carte des changements (explication de l'alarme) */
#define SENT_MAP_MIN_MM         120   /* écart mini par rapport à la scène apprise                 */
#define SENT_MAP_SIGMAS         4.0f  /* ... et en nombre d'écarts-types appris                     */

#endif /* APP_CONFIG_H */
