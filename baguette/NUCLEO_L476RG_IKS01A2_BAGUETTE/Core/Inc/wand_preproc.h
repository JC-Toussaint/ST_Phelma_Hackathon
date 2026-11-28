/**
 * wand_preproc.h - Baguette magique : découpage des gestes (code portable, sans HAL, testé sur PC)
 *
 * Les échantillons arrivent à cadence fixe (IMU_ODR_HZ). Un tampon circulaire garde les
 * WAND_PRETRIGGER derniers ; quand la baguette bouge franchement, la fenêtre démarre avec eux
 * (le début du geste n'est pas perdu) et se remplit jusqu'à WAND_SAMPLES échantillons.
 * Tous les gestes sont ainsi ALIGNÉS sur leur début, ce qui aide beaucoup NanoEdge.
 * Après une fenêtre, il faut un moment de calme (WAND_REST_MS) avant le geste suivant.
 */
#ifndef WAND_PREPROC_H
#define WAND_PREPROC_H

#include <stdint.h>
#include "app_config.h"

/* Un échantillon du LSM6DSL : accélération en mg, vitesse angulaire en mdps (unités du BSP de ST). */
typedef struct {
    int32_t ax, ay, az;
    int32_t gx, gy, gz;
} imu_sample_t;

typedef enum {
    WAND_ARMING = 0,   /* attend un moment de calme                         */
    WAND_ARMED,        /* prêt : attend le début d'un geste                 */
    WAND_RECORDING,    /* geste en cours d'enregistrement                   */
    WAND_READY         /* fenêtre complète, à récupérer avec wand_pre_take  */
} wand_state_t;

typedef struct {
    imu_sample_t ring[WAND_PRETRIGGER];
    uint16_t     ring_head;            /* prochaine case à écrire */
    uint16_t     ring_count;
    imu_sample_t win[WAND_SAMPLES];
    uint16_t     win_count;
    uint16_t     calm_count;           /* échantillons calmes consécutifs */
    wand_state_t state;
} wand_pre_t;

void         wand_pre_init(wand_pre_t *w);
/* Ajoute un échantillon ; retourne 1 quand une fenêtre complète est prête. */
int          wand_pre_push(wand_pre_t *w, const imu_sample_t *s);
/* Copie la fenêtre mise à l'échelle (WAND_SIGNAL_LEN valeurs entrelacées ax,ay,az,gx,gy,gz) et réarme. */
void         wand_pre_take(wand_pre_t *w, uint16_t out[WAND_SIGNAL_LEN]);
wand_state_t wand_pre_state(const wand_pre_t *w);

int          wand_is_moving(const imu_sample_t *s);   /* 1 = assez de mouvement pour déclencher */
int          wand_is_calm(const imu_sample_t *s);     /* 1 = baguette presque immobile          */
uint16_t     wand_scale_acc(int32_t mg);              /* 0..WAND_VALUE_MAX                      */
uint16_t     wand_scale_gyro(int32_t mdps);           /* 0..WAND_VALUE_MAX                      */

#endif /* WAND_PREPROC_H */
