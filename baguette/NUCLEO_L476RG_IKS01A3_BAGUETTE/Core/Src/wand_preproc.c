/**
 * wand_preproc.c - Baguette magique : découpage des gestes (voir wand_preproc.h)
 */
#include "wand_preproc.h"
#include <string.h>

#define REST_SAMPLES  ((uint16_t)((WAND_REST_MS * IMU_ODR_HZ + 999) / 1000))

static int64_t sq(int32_t v)
{
    return (int64_t)v * (int64_t)v;
}

/* Norme² de l'accélération comparée à (1 g ± marge)² : pas de racine carrée. */
static int acc_far_from_1g(const imu_sample_t *s, int32_t margin_mg)
{
    int64_t a2 = sq(s->ax) + sq(s->ay) + sq(s->az);
    int64_t hi = sq(1000 + margin_mg);
    int64_t lo = (margin_mg < 1000) ? sq(1000 - margin_mg) : 0;
    return a2 > hi || a2 < lo;
}

static int64_t gyro_norm2(const imu_sample_t *s)
{
    return sq(s->gx) + sq(s->gy) + sq(s->gz);
}

int wand_is_moving(const imu_sample_t *s)
{
    return gyro_norm2(s) > sq(WAND_TRIGGER_DPS * 1000) || acc_far_from_1g(s, WAND_TRIGGER_MG);
}

int wand_is_calm(const imu_sample_t *s)
{
    return gyro_norm2(s) < sq(WAND_CALM_DPS * 1000) && !acc_far_from_1g(s, WAND_CALM_MG);
}

static uint16_t scale(int32_t v, int32_t per_unit)
{
    int32_t u = WAND_VALUE_MAX / 2 + v / per_unit;
    if (u < 0) u = 0;
    if (u > WAND_VALUE_MAX) u = WAND_VALUE_MAX;
    return (uint16_t)u;
}

uint16_t wand_scale_acc(int32_t mg)
{
    return scale(mg, WAND_ACC_MG_PER_UNIT);
}

uint16_t wand_scale_gyro(int32_t mdps)
{
    return scale(mdps, WAND_GYRO_MDPS_PER_UNIT);
}

void wand_pre_init(wand_pre_t *w)
{
    memset(w, 0, sizeof(*w));
    w->state = WAND_ARMING;
}

wand_state_t wand_pre_state(const wand_pre_t *w)
{
    return w->state;
}

static void ring_put(wand_pre_t *w, const imu_sample_t *s)
{
    w->ring[w->ring_head] = *s;
    w->ring_head = (uint16_t)((w->ring_head + 1u) % WAND_PRETRIGGER);
    if (w->ring_count < WAND_PRETRIGGER) {
        w->ring_count++;
    }
}

/* Démarre la fenêtre avec le contenu du tampon circulaire, du plus ancien au plus récent. */
static void start_window(wand_pre_t *w)
{
    uint16_t first = (uint16_t)((w->ring_head + WAND_PRETRIGGER - w->ring_count) % WAND_PRETRIGGER);
    w->win_count = 0;
    for (uint16_t i = 0; i < w->ring_count; i++) {
        w->win[w->win_count++] = w->ring[(first + i) % WAND_PRETRIGGER];
    }
    w->state = WAND_RECORDING;
}

int wand_pre_push(wand_pre_t *w, const imu_sample_t *s)
{
    switch (w->state) {
    case WAND_ARMING:
        w->calm_count = wand_is_calm(s) ? (uint16_t)(w->calm_count + 1u) : 0u;
        ring_put(w, s);
        if (w->calm_count >= REST_SAMPLES) {
            w->state = WAND_ARMED;
        }
        return 0;

    case WAND_ARMED:
        if (wand_is_moving(s)) {
            start_window(w);                 /* pré-déclenchement ... */
            w->win[w->win_count++] = *s;     /* ... puis l'échantillon qui a déclenché */
            if (w->win_count >= WAND_SAMPLES) {
                w->state = WAND_READY;
                return 1;
            }
        } else {
            ring_put(w, s);
        }
        return 0;

    case WAND_RECORDING:
        w->win[w->win_count++] = *s;
        if (w->win_count >= WAND_SAMPLES) {
            w->state = WAND_READY;
            return 1;
        }
        return 0;

    case WAND_READY:
    default:
        return 0;                            /* fenêtre pas encore récupérée : on ignore */
    }
}

void wand_pre_take(wand_pre_t *w, uint16_t out[WAND_SIGNAL_LEN])
{
    for (int i = 0; i < WAND_SAMPLES; i++) {
        const imu_sample_t *s = &w->win[i];
        uint16_t *o = &out[i * WAND_AXES];
        o[0] = wand_scale_acc(s->ax);
        o[1] = wand_scale_acc(s->ay);
        o[2] = wand_scale_acc(s->az);
        o[3] = wand_scale_gyro(s->gx);
        o[4] = wand_scale_gyro(s->gy);
        o[5] = wand_scale_gyro(s->gz);
    }
    w->win_count = 0;
    w->ring_count = 0;
    w->ring_head = 0;
    w->calm_count = 0;
    w->state = WAND_ARMING;
}
