/**
 * tap_preproc.c - La table qui écoute : découpage des tapes (voir tap_preproc.h)
 */
#include "tap_preproc.h"
#include <string.h>

#define REST_SAMPLES  ((uint16_t)((TAP_REST_MS * IMU_ODR_HZ + 999) / 1000))

static int32_t absdiff(int32_t a, int32_t b)
{
    return (a > b) ? a - b : b - a;
}

static int32_t rest_mg(const tap_pre_t *t, int k)
{
    return t->rest[k] >> TAP_REST_SHIFT;
}

int32_t tap_deviation(const tap_pre_t *t, const acc_sample_t *s)
{
    int32_t d = absdiff(s->ax, rest_mg(t, 0));
    int32_t e = absdiff(s->ay, rest_mg(t, 1));
    int32_t f = absdiff(s->az, rest_mg(t, 2));
    if (e > d) d = e;
    if (f > d) d = f;
    return d;
}

/* Moyenne glissante exponentielle du repos (initialisée sur le premier échantillon). */
static void rest_update(tap_pre_t *t, const acc_sample_t *s)
{
    const int32_t v[3] = {s->ax, s->ay, s->az};
    for (int k = 0; k < 3; k++) {
        if (!t->rest_valid) {
            t->rest[k] = v[k] * (1 << TAP_REST_SHIFT);
        } else {
            t->rest[k] += v[k] - rest_mg(t, k);
        }
    }
    t->rest_valid = 1;
}

uint16_t tap_scale(int32_t mg)
{
    int32_t u = TAP_VALUE_MAX / 2 + mg / TAP_MG_PER_UNIT;
    if (u < 0) u = 0;
    if (u > TAP_VALUE_MAX) u = TAP_VALUE_MAX;
    return (uint16_t)u;
}

void tap_pre_init(tap_pre_t *t)
{
    memset(t, 0, sizeof(*t));
    t->state = TAP_ARMING;
}

tap_state_t tap_pre_state(const tap_pre_t *t)
{
    return t->state;
}

void tap_pre_rearm(tap_pre_t *t)
{
    t->win_count = 0;
    t->ring_count = 0;
    t->ring_head = 0;
    t->calm_count = 0;
    t->state = TAP_ARMING;
}

static void ring_put(tap_pre_t *t, const acc_sample_t *s)
{
    t->ring[t->ring_head] = *s;
    t->ring_head = (uint16_t)((t->ring_head + 1u) % TAP_PRETRIGGER);
    if (t->ring_count < TAP_PRETRIGGER) {
        t->ring_count++;
    }
}

/* Démarre la fenêtre avec le contenu du tampon circulaire, du plus ancien au plus récent. */
static void start_window(tap_pre_t *t)
{
    uint16_t first = (uint16_t)((t->ring_head + TAP_PRETRIGGER - t->ring_count) % TAP_PRETRIGGER);
    t->base.ax = rest_mg(t, 0);
    t->base.ay = rest_mg(t, 1);
    t->base.az = rest_mg(t, 2);
    t->win_count = 0;
    for (uint16_t i = 0; i < t->ring_count; i++) {
        t->win[t->win_count++] = t->ring[(first + i) % TAP_PRETRIGGER];
    }
    t->state = TAP_RECORDING;
}

int tap_pre_push(tap_pre_t *t, const acc_sample_t *s)
{
    switch (t->state) {
    case TAP_ARMING:
        if (!t->rest_valid) {
            rest_update(t, s);
        }
        t->calm_count = (tap_deviation(t, s) < TAP_CALM_MG) ? (uint16_t)(t->calm_count + 1u) : 0u;
        rest_update(t, s);
        ring_put(t, s);
        if (t->calm_count >= REST_SAMPLES) {
            t->state = TAP_ARMED;
        }
        return 0;

    case TAP_ARMED:
        if (tap_deviation(t, s) > TAP_TRIGGER_MG) {
            start_window(t);                 /* pré-déclenchement ... */
            t->win[t->win_count++] = *s;     /* ... puis l'échantillon qui a déclenché */
            if (t->win_count >= TAP_SAMPLES) {
                t->state = TAP_READY;
                return 1;
            }
        } else {
            rest_update(t, s);               /* le repos suit les dérives lentes (pas les tapes) */
            ring_put(t, s);
        }
        return 0;

    case TAP_RECORDING:
        t->win[t->win_count++] = *s;
        if (t->win_count >= TAP_SAMPLES) {
            t->state = TAP_READY;
            return 1;
        }
        return 0;

    case TAP_READY:
    default:
        return 0;                            /* fenêtre pas encore récupérée : on ignore */
    }
}

void tap_pre_take(tap_pre_t *t, uint16_t out[TAP_SIGNAL_LEN])
{
    for (int i = 0; i < TAP_SAMPLES; i++) {
        const acc_sample_t *s = &t->win[i];
        uint16_t *o = &out[i * TAP_AXES];
        o[0] = tap_scale(s->ax - t->base.ax);
        o[1] = tap_scale(s->ay - t->base.ay);
        o[2] = tap_scale(s->az - t->base.az);
    }
    tap_pre_rearm(t);
}
