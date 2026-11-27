/**
 * tof_sensor.c - VL53L5CX central de la X-NUCLEO-53L5A1, via le BSP X-CUBE-TOF1
 * du projet ST (VL53L5A1_RANGING_SENSOR_*).
 *
 * - Alimentation / LPn / I2C_RST : déjà configurés par MX_GPIO_Init() (main.c).
 * - Mesure continue 8x8 à 15 Hz, une interruption par trame (RS_IT_DEFAULT ; l'exemple
 *   ST n'en donne que si une cible est entre ses seuils) : l'application a besoin de
 *   toutes les trames. Réglages : X-CUBE-TOF1/App/app_x-cube-tof1.c (TOF1_SensorConfig).
 * - La broche INT (PA4, EXTI4) positionne ToF_EventDetected (app_tof_pin_conf.c) :
 *   on ne lit le capteur sur l'I2C que lorsqu'une mesure est prête, avec une
 *   relecture de secours toutes les 100 ms si une interruption était manquée.
 */
#include "tof_sensor.h"
#include "main.h"
#include "app_config.h"
#include "53l5a1_ranging_sensor.h"
#include "app_x-cube-tof1.h"

extern volatile uint8_t ToF_EventDetected;    /* défini dans app_x-cube-tof1.c */

#define TOF_DEV            VL53L5A1_DEV_CENTER
#define TOF_FALLBACK_MS    100U

static RANGING_SENSOR_Result_t result;
static uint32_t last_read;

/* La configuration (profil, interruption, seuils, cible la plus proche) est faite par
 * TOF1_SensorConfig(), dans X-CUBE-TOF1/App/app_x-cube-tof1.c, avec les réglages de
 * l'exemple ST conservés là avec leurs commentaires. */
int tof_sensor_init(void)
{
    int err = (int)TOF1_SensorConfig();
    last_read = HAL_GetTick();
    return err;
}

int tof_sensor_poll(tof_frame_t *f)
{
    if (!ToF_EventDetected && (HAL_GetTick() - last_read) < TOF_FALLBACK_MS) {
        return 0;
    }
    ToF_EventDetected = 0;
    last_read = HAL_GetTick();

    /* Mode asynchrone : retourne une erreur si aucune mesure n'est prête. */
    if (VL53L5A1_RANGING_SENSOR_GetDistance(TOF_DEV, &result) != BSP_ERROR_NONE) {
        return 0;
    }
    if (result.NumberOfZones != TOF_ZONES) {
        return 0;
    }

    for (int i = 0; i < TOF_ZONES; i++) {
        const RANGING_SENSOR_ZoneResult_t *z = &result.ZoneResult[i];
        if (z->NumberOfTargets == 0U) {
            /* Le BSP ne met pas à jour Distance/Status dans ce cas : on force "pas de cible". */
            f->distance_mm[i] = 0;
            f->status[i] = 255U;
        } else {
            f->distance_mm[i] = (int16_t)z->Distance[0];
            /* Le BSP remappe les statuts ULD 5 et 9 (valides) en 0 ; les autres
             * sont transmis tels quels. On revient à la convention ULD (5 = valide)
             * utilisée par le prétraitement.                                    */
            f->status[i] = (z->Status[0] == 0U) ? 5U : (uint8_t)z->Status[0];
        }
    }
    return 1;
}
