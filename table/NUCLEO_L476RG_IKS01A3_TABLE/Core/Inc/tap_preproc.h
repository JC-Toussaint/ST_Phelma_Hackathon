/**
 * tap_preproc.h - La table qui écoute : découpage des tapes (code portable, sans HAL, testé sur PC)
 *
 * Les échantillons arrivent à cadence fixe (IMU_ODR_HZ). Une moyenne glissante suit le repos (la gravité,
 * la carte étant immobile sous la table). Un tampon circulaire garde les TAP_PRETRIGGER derniers
 * échantillons ; quand l'accélération s'écarte franchement du repos, la fenêtre démarre avec eux et se
 * remplit jusqu'à TAP_SAMPLES. Toutes les tapes sont ainsi ALIGNÉES sur leur début.
 * Après une fenêtre, il faut un moment de calme (TAP_REST_MS) avant la tape suivante.
 * Même logique que wand_preproc.c de la baguette, avec l'accéléromètre seul.
 */
#ifndef TAP_PREPROC_H
#define TAP_PREPROC_H

#include <stdint.h>
#include "app_config.h"

/* Un échantillon de l'accéléromètre, en mg (unité du BSP de ST). */
typedef struct {
    int32_t ax, ay, az;
} acc_sample_t;

typedef enum {
    TAP_ARMING = 0,    /* attend un moment de calme                         */
    TAP_ARMED,         /* prêt : attend une tape                            */
    TAP_RECORDING,     /* tape en cours d'enregistrement                    */
    TAP_READY          /* fenêtre complète, à récupérer avec tap_pre_take   */
} tap_state_t;

typedef struct {
    acc_sample_t ring[TAP_PRETRIGGER];
    uint16_t     ring_head;            /* prochaine case à écrire */
    uint16_t     ring_count;
    acc_sample_t win[TAP_SAMPLES];
    uint16_t     win_count;
    uint16_t     calm_count;           /* échantillons calmes consécutifs */
    int32_t      rest[3];              /* moyenne de repos, en mg << TAP_REST_SHIFT */
    acc_sample_t base;                 /* repos figé au déclenchement, retiré de la fenêtre */
    uint8_t      rest_valid;           /* 0 tant que la moyenne n'a pas été initialisée */
    tap_state_t  state;
} tap_pre_t;

void        tap_pre_init(tap_pre_t *t);
/* Ajoute un échantillon ; retourne 1 quand une fenêtre complète est prête. */
int         tap_pre_push(tap_pre_t *t, const acc_sample_t *s);
/* Copie la fenêtre mise à l'échelle (TAP_SIGNAL_LEN valeurs entrelacées ax,ay,az), repos retiré, et réarme. */
void        tap_pre_take(tap_pre_t *t, uint16_t out[TAP_SIGNAL_LEN]);
/* Abandonne une tape en cours et repart en attente de calme (après une perte d'échantillons). */
void        tap_pre_rearm(tap_pre_t *t);
tap_state_t tap_pre_state(const tap_pre_t *t);

/* Écart maximal (mg, sur un axe) de s à la moyenne de repos actuelle. */
int32_t     tap_deviation(const tap_pre_t *t, const acc_sample_t *s);
uint16_t    tap_scale(int32_t mg);     /* écart en mg -> 0..TAP_VALUE_MAX */

#endif /* TAP_PREPROC_H */
