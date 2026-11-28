/**
 * imu_sensor.h - LSM6DSL de la X-NUCLEO-IKS01A2, par le BSP de ST (X-CUBE-MEMS1 9.6.0)
 */
#ifndef IMU_SENSOR_H
#define IMU_SENSOR_H

#include "wand_preproc.h"

#define IMU_NAME   "LSM6DSL"            /* pour les messages de app.c */
#define IMU_BOARD  "X-NUCLEO-IKS01A2"

/* Initialise l'accéléromètre et le gyroscope à IMU_ODR_HZ, IMU_ACC_FS_G, IMU_GYRO_FS_DPS.
 * Retourne 0 si OK, sinon un code d'erreur négatif (étape qui a échoué). */
int imu_init(void);

/* Retourne 1 et remplit s quand le capteur a un NOUVEL échantillon (cadence fixe imposée par le
 * capteur), 0 sinon, -1 en cas d'erreur de lecture. À appeler aussi souvent que possible. */
int imu_read(imu_sample_t *s);

#endif /* IMU_SENSOR_H */
