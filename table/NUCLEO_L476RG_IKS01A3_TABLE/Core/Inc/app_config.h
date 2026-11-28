/**
 * app_config.h - La table qui écoute : paramètres de l'application
 *
 * NUCLEO-L476RG + X-NUCLEO-IKS01A3, carte collée SOUS une table. On tape du doigt sur une zone marquée sur
 * le plateau : l'accéléromètre du LSM6DSO capte la vibration, et une librairie NanoEdge AI (N-class
 * classification) reconnaît la zone. Une tape = un signal de TAP_SAMPLES échantillons x 3 axes.
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ------------------------------------------------------------------------- */
/* Mode de fonctionnement                                                    */
/* ------------------------------------------------------------------------- */
#define APP_MODE_DATALOG   0   /* Envoie chaque tape en CSV sur l'UART (collecte NanoEdge)           */
#define APP_MODE_TABLE     1   /* Reconnaît la zone tapée avec la librairie NanoEdge générée          */

#ifndef APP_MODE
#define APP_MODE           APP_MODE_DATALOG
#endif

/* Liaison série : USART2 (port COM virtuel du ST-LINK), initialisée par le BSP de ST à 115200 bauds. */
#define APP_UART_BAUD      115200

/* ------------------------------------------------------------------------- */
/* Capteur : accéléromètre du LSM6DSO seul, lu par sa FIFO                    */
/* ------------------------------------------------------------------------- */
#define IMU_ODR_HZ         1667    /* cadence fixe du LSM6DSO (valeurs possibles : ... 833, 1667, 3333) */
#define IMU_ACC_FS_G       4       /* pleine échelle : 2, 4, 8 ou 16 g ; 8 si les courbes plafonnent    */
#define IMU_FIFO_BATCH     64      /* échantillons lus au plus par appel (la FIFO en garde bien plus)   */

/* ------------------------------------------------------------------------- */
/* Signal (doit correspondre au projet NanoEdge AI Studio et au profil de collecte)  */
/* ------------------------------------------------------------------------- */
#define TAP_AXES           3       /* ax, ay, az, entrelacés                                         */
#define TAP_SAMPLES        256     /* échantillons par tape : 256 / 1667 Hz ≈ 154 ms                 */
#define TAP_SIGNAL_LEN     (TAP_SAMPLES * TAP_AXES)   /* 768 valeurs                                 */
#define TAP_PRETRIGGER     32      /* échantillons gardés AVANT le déclenchement (~19 ms)            */

/* Mise à l'échelle : la gravité (moyenne au repos) est retirée, puis entiers 0..TAP_VALUE_MAX,
 * TAP_VALUE_MAX / 2 = zéro. Avec ±4 g : 1 unité = 8 mg ; les valeurs hors échelle sont écrêtées. */
#define TAP_VALUE_MAX      1000
#define TAP_MG_PER_UNIT    ((IMU_ACC_FS_G * 1000) / (TAP_VALUE_MAX / 2))

/* Déclenchement : une tape commence quand l'accélération s'écarte franchement du repos... */
#define TAP_TRIGGER_MG     80      /* écart à la moyenne de repos, sur un axe au moins (mg)          */
/* ...et on ne réarme qu'après un moment de calme (pas de tape à cheval sur deux fenêtres). */
#define TAP_CALM_MG        25      /* écart sous ce seuil sur les 3 axes...                          */
#define TAP_REST_MS        200     /* ...pendant au moins ce temps                                   */
/* Moyenne de repos : moyenne glissante exponentielle, 1/2^TAP_REST_SHIFT par échantillon
 * (8 -> constante de temps de 256 échantillons, ~150 ms) : elle suit la gravité et les dérives lentes. */
#define TAP_REST_SHIFT     8

/* ------------------------------------------------------------------------- */
/* Reconnaissance (APP_MODE_TABLE)                                           */
/* ------------------------------------------------------------------------- */
#define TAP_MIN_CONFIDENCE 0.60f   /* probabilité mini pour accepter une zone                         */
#define TAP_REJECT_CLASS   "autre" /* classe « autre chose qu'une tape sur une zone » : jamais annoncée */

#endif /* APP_CONFIG_H */
