/**
 * tof_preproc.h - Prétraitement des trames 8x8 du VL53L5CX (code portable, sans HAL)
 *
 * Chaque zone est convertie en une valeur 0..100 :
 *   0        : fond / zone invalide
 *   20..100  : main, 100 = point le plus proche, 20 = PRE_DEPTH_WINDOW_MM plus loin
 * La main est ensuite recentrée dans la grille, puis PRE_FRAMES_PER_SIGNAL trames
 * consécutives sont empilées pour former un signal NanoEdge.
 */
#ifndef TOF_PREPROC_H
#define TOF_PREPROC_H

#include <stdint.h>
#include "app_config.h"

typedef struct {
    int16_t distance_mm[TOF_ZONES];
    uint8_t status[TOF_ZONES];
} tof_frame_t;

typedef struct {
    float   frames[PRE_FRAMES_PER_SIGNAL][TOF_ZONES]; /* tampon circulaire */
    uint8_t head;                                    /* prochaine case à écrire */
    uint8_t count;                                   /* nb de trames valides empilées */
    /* infos sur la dernière trame */
    uint8_t hand_present;
    uint8_t hand_zones;
    int16_t hand_min_mm;
} tof_preproc_t;

void tof_preproc_init(tof_preproc_t *pp);

/* Traite une trame. Retourne 1 si un signal complet (SIGNAL_LEN) est disponible.
 * Si la main disparaît, l'empilement est remis à zéro.                          */
int tof_preproc_push(tof_preproc_t *pp, const tof_frame_t *in);

/* Copie le signal (trame la plus ancienne d'abord) dans out[SIGNAL_LEN]. */
void tof_preproc_get_signal(const tof_preproc_t *pp, float *out);

/* Fonction élémentaire exposée pour les tests : convertit une trame en 64 valeurs.
 * Retourne le nombre de zones "main".                                           */
int tof_preproc_frame(const tof_frame_t *in, float out[TOF_ZONES], int16_t *min_mm);

#endif /* TOF_PREPROC_H */
