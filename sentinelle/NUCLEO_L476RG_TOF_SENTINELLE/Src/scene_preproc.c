/**
 * scene_preproc.c - Sentinelle : prétraitement de la scène (voir scene_preproc.h)
 */
#include "scene_preproc.h"
#include <string.h>
#include <math.h>

int16_t scene_zone_mm(int16_t d, uint8_t status)
{
    /* 5 = valide ; 6, 9 = valides avec avertissement. Le reste (dont 255 = pas
     * de cible) est considéré comme "loin", ce qui est stable d'une trame à l'autre. */
    if (!(status == 5U || status == 6U || status == 9U) || d <= 0) {
        return SENT_MAX_MM;
    }
    return (d > SENT_MAX_MM) ? (int16_t)SENT_MAX_MM : d;
}

void scene_preproc_init(scene_preproc_t *sp)
{
    memset(sp, 0, sizeof(*sp));
}

int scene_preproc_push(scene_preproc_t *sp, const tof_frame_t *f)
{
    if (sp->count >= SENT_FRAMES_PER_SIGNAL) {
        return 1;   /* signal déjà prêt et pas encore récupéré */
    }
    float *dst = sp->frames[sp->count];
    for (int i = 0; i < TOF_ZONES; i++) {
        dst[i] = 100.0f * (float)scene_zone_mm(f->distance_mm[i], f->status[i]) / (float)SENT_MAX_MM;
    }
    sp->count++;
    return sp->count == SENT_FRAMES_PER_SIGNAL;
}

void scene_preproc_take(scene_preproc_t *sp, float *out)
{
    memcpy(out, sp->frames, sizeof(sp->frames));
    sp->count = 0;
}

void scene_ref_reset(scene_ref_t *r)
{
    memset(r, 0, sizeof(*r));
}

void scene_ref_add(scene_ref_t *r, const tof_frame_t *f)
{
    r->n++;
    for (int i = 0; i < TOF_ZONES; i++) {
        float x = (float)scene_zone_mm(f->distance_mm[i], f->status[i]);
        float delta = x - r->mean[i];
        r->mean[i] += delta / (float)r->n;
        r->m2[i]   += delta * (x - r->mean[i]);
    }
}

int scene_ref_diff(const scene_ref_t *r, const tof_frame_t *f, char map[TOF_ZONES])
{
    int changed = 0;
    for (int i = 0; i < TOF_ZONES; i++) {
        map[i] = '.';
        if (r->n < 2) {
            continue;
        }
        float sd  = sqrtf(r->m2[i] / (float)(r->n - 1));
        float tol = SENT_MAP_SIGMAS * sd;
        if (tol < (float)SENT_MAP_MIN_MM) {
            tol = (float)SENT_MAP_MIN_MM;
        }
        float diff = (float)scene_zone_mm(f->distance_mm[i], f->status[i]) - r->mean[i];
        if (diff < -tol) {
            map[i] = '+';   /* plus proche qu'avant : quelque chose est apparu */
            changed++;
        } else if (diff > tol) {
            map[i] = '-';   /* plus loin qu'avant : quelque chose a disparu */
            changed++;
        }
    }
    return changed;
}

void alarm_init(alarm_t *a, uint8_t threshold, uint8_t confirm)
{
    a->threshold = threshold;
    a->confirm   = confirm ? confirm : 1U;
    a->streak    = 0;
    a->active    = 0;
}

alarm_event_t alarm_update(alarm_t *a, uint8_t similarity)
{
    int abnormal = similarity < a->threshold;
    /* streak compte les signaux consécutifs qui contredisent l'état actuel */
    if (abnormal != (int)a->active) {
        if (++a->streak >= a->confirm) {
            a->active = (uint8_t)abnormal;
            a->streak = 0;
            return abnormal ? ALARM_RAISED : ALARM_CLEARED;
        }
    } else {
        a->streak = 0;
    }
    return ALARM_NONE;
}
