/**
 * imu_sensor.c - Accéléromètre du LSM6DSO de la X-NUCLEO-IKS01A3, lu par sa FIFO (voir imu_sensor.h)
 *
 * À 1667 Hz, lire le capteur échantillon par échantillon (comme la baguette à 104 Hz) devient serré :
 * la moindre attente fait perdre des échantillons. Ici, le LSM6DSO range lui-même ses mesures dans sa FIFO
 * (mode continu : les plus anciennes sont écrasées si personne ne lit), et la carte la vide par paquets.
 * Chaque mot de la FIFO porte une étiquette (tag) : on ne garde que les mesures de l'accéléromètre.
 */
#include "imu_sensor.h"
#include "iks01a3_motion_sensors.h"
#include "iks01a3_motion_sensors_ex.h"

#define IMU  IKS01A3_LSM6DSO_0

static int overrun;

static int fifo_stream(void)
{
    /* Passer par le mode « bypass » vide la FIFO ; le mode « stream » la remet en route. */
    if (IKS01A3_MOTION_SENSOR_FIFO_Set_Mode(IMU, LSM6DSO_BYPASS_MODE) != BSP_ERROR_NONE) return -1;
    if (IKS01A3_MOTION_SENSOR_FIFO_Set_Mode(IMU, LSM6DSO_STREAM_MODE) != BSP_ERROR_NONE) return -1;
    return 0;
}

int imu_init(void)
{
    if (IKS01A3_MOTION_SENSOR_Init(IMU, MOTION_ACCELERO) != BSP_ERROR_NONE) return -1;
    /* Init laisse le capteur éteint : régler cadence et pleine échelle, la FIFO, puis l'activer. */
    if (IKS01A3_MOTION_SENSOR_SetOutputDataRate(IMU, MOTION_ACCELERO, (float)IMU_ODR_HZ) != BSP_ERROR_NONE) return -2;
    if (IKS01A3_MOTION_SENSOR_SetFullScale(IMU, MOTION_ACCELERO, IMU_ACC_FS_G) != BSP_ERROR_NONE) return -3;
    /* L'accéléromètre seul entre dans la FIFO, à sa propre cadence (BDR = batch data rate). */
    if (IKS01A3_MOTION_SENSOR_FIFO_Set_BDR(IMU, MOTION_ACCELERO, (float)IMU_ODR_HZ) != BSP_ERROR_NONE) return -4;
    if (fifo_stream() != 0) return -5;
    if (IKS01A3_MOTION_SENSOR_Enable(IMU, MOTION_ACCELERO) != BSP_ERROR_NONE) return -6;
    return 0;
}

int imu_read_batch(acc_sample_t *buf, int max)
{
    uint16_t n = 0;
    uint8_t full = 0;
    int got = 0;

    if (IKS01A3_MOTION_SENSOR_FIFO_Get_Num_Samples(IMU, &n) != BSP_ERROR_NONE) return -1;
    if (n == 0) return 0;
    if (IKS01A3_MOTION_SENSOR_FIFO_Get_Full_Status(IMU, &full) == BSP_ERROR_NONE && full) {
        overrun = 1;                     /* FIFO pleine : des échantillons ont été écrasés */
    }
    if (n > (uint16_t)max) n = (uint16_t)max;

    for (uint16_t i = 0; i < n; i++) {
        uint8_t tag = 0;
        IKS01A3_MOTION_SENSOR_Axes_t a;
        /* Lire l'étiquette puis les 6 octets de données fait avancer la FIFO d'un mot. */
        if (IKS01A3_MOTION_SENSOR_FIFO_Get_Tag(IMU, &tag) != BSP_ERROR_NONE) return -1;
        if (IKS01A3_MOTION_SENSOR_FIFO_Get_Axes(IMU, MOTION_ACCELERO, &a) != BSP_ERROR_NONE) return -1;   /* mg */
        if (tag != (uint8_t)LSM6DSO_XL_NC_TAG) {
            continue;                    /* autre chose que l'accéléromètre : ignoré */
        }
        buf[got].ax = a.x;
        buf[got].ay = a.y;
        buf[got].az = a.z;
        got++;
    }
    return got;
}

int imu_flush(void)
{
    return fifo_stream();
}

int imu_overrun(void)
{
    int o = overrun;
    overrun = 0;
    return o;
}
