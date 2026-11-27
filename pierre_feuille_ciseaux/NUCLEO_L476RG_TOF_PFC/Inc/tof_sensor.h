/**
 * tof_sensor.h - Initialisation et lecture du VL53L5CX (via le BSP X-CUBE-TOF1 du projet ST)
 */
#ifndef TOF_SENSOR_H
#define TOF_SENSOR_H

#include <stdint.h>
#include "tof_preproc.h"

/* Retourne 0 si OK, sinon un code d'erreur (affiché sur l'UART). */
int tof_sensor_init(void);

/* Retourne 1 si une nouvelle trame a été lue dans *f, 0 sinon (non bloquant). */
int tof_sensor_poll(tof_frame_t *f);

#endif /* TOF_SENSOR_H */
