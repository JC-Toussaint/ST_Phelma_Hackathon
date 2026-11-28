/**
 * app_config.h - Baguette magique : paramètres de l'application
 *
 * NUCLEO-L476RG + X-NUCLEO-IKS01A2 (LSM6DSL : accéléromètre + gyroscope 6 axes).
 * Un geste dans l'air (cercle, zigzag, frappe, V…) = un signal de WAND_SAMPLES
 * échantillons x 6 axes, classé par une librairie NanoEdge AI (N-class classification).
 */
#ifndef APP_CONFIG_H
#define APP_CONFIG_H

/* ------------------------------------------------------------------------- */
/* Mode de fonctionnement                                                    */
/* ------------------------------------------------------------------------- */
#define APP_MODE_DATALOG   0   /* Envoie chaque geste en CSV sur l'UART (collecte NanoEdge)          */
#define APP_MODE_WAND      1   /* Reconnaît les gestes avec la librairie NanoEdge générée            */

#ifndef APP_MODE
#define APP_MODE           APP_MODE_DATALOG
#endif

/* Liaison série : USART2 (port COM virtuel du ST-LINK), initialisée par le BSP de ST à 115200 bauds. */
#define APP_UART_BAUD      115200

/* ------------------------------------------------------------------------- */
/* Capteur LSM6DSL                                                           */
/* ------------------------------------------------------------------------- */
#define IMU_ODR_HZ         104     /* cadence fixe, imposée par le capteur (NanoEdge en a besoin)  */
#define IMU_ACC_FS_G       16      /* pleine échelle accéléromètre : 2, 4, 8 ou 16 g (les frappes dépassent 8 g) */
#define IMU_GYRO_FS_DPS    2000    /* pleine échelle gyroscope : 125, 245, 500, 1000 ou 2000 °/s (frappes > 1000) */

/* ------------------------------------------------------------------------- */
/* Signal (doit correspondre au projet NanoEdge AI Studio et au profil de collecte)  */
/* ------------------------------------------------------------------------- */
#define WAND_AXES          6       /* ax, ay, az, gx, gy, gz, entrelacés                           */
#define WAND_SAMPLES       256     /* échantillons par geste : 256 / 104 Hz ≈ 2,5 s (le Z entier)  */
#define WAND_SIGNAL_LEN    (WAND_SAMPLES * WAND_AXES)   /* 1536 valeurs                          */
#define WAND_PRETRIGGER    16      /* échantillons gardés AVANT le déclenchement (~150 ms)         */

/* Mise à l'échelle : entiers 0..WAND_VALUE_MAX, WAND_VALUE_MAX / 2 = zéro.
 * Avec ±16 g et ±2000 °/s : 1 unité = 32 mg et 4 °/s ; les valeurs hors échelle sont écrêtées. */
#define WAND_VALUE_MAX     1000
#define WAND_ACC_MG_PER_UNIT    ((IMU_ACC_FS_G * 1000) / (WAND_VALUE_MAX / 2))
#define WAND_GYRO_MDPS_PER_UNIT ((IMU_GYRO_FS_DPS * 1000) / (WAND_VALUE_MAX / 2))

/* Déclenchement : un geste commence quand la baguette bouge franchement... */
#define WAND_TRIGGER_DPS   150     /* norme du gyroscope (°/s)                                     */
#define WAND_TRIGGER_MG    600     /* ou écart de la norme de l'accélération à 1 g (mg)            */
/* ...et on ne réarme qu'après un moment de calme (pas de geste à cheval sur deux fenêtres). */
#define WAND_CALM_DPS      40      /* gyroscope sous ce seuil...                                   */
#define WAND_CALM_MG       150     /* ...et accélération à moins de 150 mg de 1 g...               */
#define WAND_REST_MS       300     /* ...pendant au moins ce temps                                 */

/* ------------------------------------------------------------------------- */
/* Reconnaissance (APP_MODE_WAND)                                            */
/* ------------------------------------------------------------------------- */
#define WAND_MIN_CONFIDENCE 0.60f  /* probabilité mini pour accepter un geste                      */
#define WAND_REJECT_CLASS   "autre" /* classe « mouvement quelconque » : jamais annoncée comme geste */

#endif /* APP_CONFIG_H */
