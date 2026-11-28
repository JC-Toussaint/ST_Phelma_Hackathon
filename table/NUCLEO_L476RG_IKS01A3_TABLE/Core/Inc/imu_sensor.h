/**
 * imu_sensor.h - Accéléromètre du LSM6DSO (X-NUCLEO-IKS01A3), lu par sa FIFO, par le BSP de ST
 * (X-CUBE-MEMS1 11.2.0)
 */
#ifndef IMU_SENSOR_H
#define IMU_SENSOR_H

#include "tap_preproc.h"

#define IMU_NAME   "LSM6DSO"            /* pour les messages de app.c */
#define IMU_BOARD  "X-NUCLEO-IKS01A3"

/* Initialise l'accéléromètre à IMU_ODR_HZ et IMU_ACC_FS_G, FIFO en mode continu (accéléromètre seul).
 * Retourne 0 si OK, sinon un code d'erreur négatif (étape qui a échoué). */
int imu_init(void);

/* Vide la FIFO dans buf (au plus max échantillons, dans l'ordre d'arrivée). Retourne le nombre
 * d'échantillons lus (0 si rien de nouveau), ou -1 en cas d'erreur. À appeler aussi souvent que possible. */
int imu_read_batch(acc_sample_t *buf, int max);

/* Jette tout le contenu de la FIFO (après un long envoi sur l'UART, par exemple). 0 si OK. */
int imu_flush(void);

/* 1 si la FIFO a débordé depuis le dernier appel (des échantillons ont été perdus), puis remet à zéro. */
int imu_overrun(void);

#endif /* IMU_SENSOR_H */
