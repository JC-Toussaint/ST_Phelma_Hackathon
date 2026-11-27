/**
 * tof_frame.h - Une trame brute du VL53L5CX (8x8 distances + statuts)
 */
#ifndef TOF_FRAME_H
#define TOF_FRAME_H

#include <stdint.h>
#include "app_config.h"

typedef struct {
    int16_t distance_mm[TOF_ZONES];
    uint8_t status[TOF_ZONES];
} tof_frame_t;

#endif /* TOF_FRAME_H */
