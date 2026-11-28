/**
 * imu_sensor.c - LSM6DSL de la X-NUCLEO-IKS01A2 (voir imu_sensor.h)
 *
 * Cadence fixe sans broche d'interruption : on interroge le bit « donnée prête » (DRDY) du gyroscope ;
 * c'est le capteur qui fixe le rythme (IMU_ODR_HZ). Accéléromètre et gyroscope tournent à la même
 * cadence : on lit les deux à chaque nouvel échantillon.
 */
#include "imu_sensor.h"
#include "iks01a2_motion_sensors.h"
#include "iks01a2_motion_sensors_ex.h"

#define IMU  IKS01A2_LSM6DSL_0

int imu_init(void)
{
    if (IKS01A2_MOTION_SENSOR_Init(IMU, MOTION_ACCELERO | MOTION_GYRO) != BSP_ERROR_NONE) return -1;
    /* Init laisse le capteur éteint : régler cadence et pleine échelle, puis l'activer. */
    if (IKS01A2_MOTION_SENSOR_SetOutputDataRate(IMU, MOTION_ACCELERO, (float)IMU_ODR_HZ) != BSP_ERROR_NONE) return -2;
    if (IKS01A2_MOTION_SENSOR_SetOutputDataRate(IMU, MOTION_GYRO, (float)IMU_ODR_HZ) != BSP_ERROR_NONE) return -3;
    if (IKS01A2_MOTION_SENSOR_SetFullScale(IMU, MOTION_ACCELERO, IMU_ACC_FS_G) != BSP_ERROR_NONE) return -4;
    if (IKS01A2_MOTION_SENSOR_SetFullScale(IMU, MOTION_GYRO, IMU_GYRO_FS_DPS) != BSP_ERROR_NONE) return -5;
    if (IKS01A2_MOTION_SENSOR_Enable(IMU, MOTION_ACCELERO) != BSP_ERROR_NONE) return -6;
    if (IKS01A2_MOTION_SENSOR_Enable(IMU, MOTION_GYRO) != BSP_ERROR_NONE) return -7;
    return 0;
}

int imu_read(imu_sample_t *s)
{
    uint8_t ready = 0;
    IKS01A2_MOTION_SENSOR_Axes_t a, g;

    if (IKS01A2_MOTION_SENSOR_Get_DRDY_Status(IMU, MOTION_GYRO, &ready) != BSP_ERROR_NONE) return -1;
    if (!ready) return 0;
    if (IKS01A2_MOTION_SENSOR_GetAxes(IMU, MOTION_GYRO, &g) != BSP_ERROR_NONE) return -1;       /* mdps */
    if (IKS01A2_MOTION_SENSOR_GetAxes(IMU, MOTION_ACCELERO, &a) != BSP_ERROR_NONE) return -1;   /* mg   */
    s->ax = a.x; s->ay = a.y; s->az = a.z;
    s->gx = g.x; s->gy = g.y; s->gz = g.z;
    return 1;
}
