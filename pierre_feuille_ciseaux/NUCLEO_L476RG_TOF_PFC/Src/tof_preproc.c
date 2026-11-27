/**
 * tof_preproc.c - Prétraitement des trames 8x8 (voir tof_preproc.h)
 */
#include "tof_preproc.h"
#include <string.h>

/* Statuts VL53L5CX considérés comme fiables : 5 = valide, 6 et 9 = valides
 * avec avertissement (wrap-around non vérifié / pulse large).               */
static int status_ok(uint8_t s)
{
    return (s == 5U) || (s == 6U) || (s == 9U);
}

static void recenter(float img[TOF_ZONES])
{
    float sum = 0.0f, sx = 0.0f, sy = 0.0f;
    for (int y = 0; y < TOF_GRID; y++) {
        for (int x = 0; x < TOF_GRID; x++) {
            float v = img[y * TOF_GRID + x];
            sum += v;
            sx  += v * (float)x;
            sy  += v * (float)y;
        }
    }
    if (sum <= 0.0f) {
        return;
    }
    const float c = (TOF_GRID - 1) * 0.5f;          /* centre = 3.5 */
    float fx = c - sx / sum;
    float fy = c - sy / sum;
    int dx = (int)(fx >= 0.0f ? fx + 0.5f : fx - 0.5f);
    int dy = (int)(fy >= 0.0f ? fy + 0.5f : fy - 0.5f);
    if (dx >  PRE_MAX_SHIFT) dx =  PRE_MAX_SHIFT;
    if (dx < -PRE_MAX_SHIFT) dx = -PRE_MAX_SHIFT;
    if (dy >  PRE_MAX_SHIFT) dy =  PRE_MAX_SHIFT;
    if (dy < -PRE_MAX_SHIFT) dy = -PRE_MAX_SHIFT;
    if (dx == 0 && dy == 0) {
        return;
    }

    float tmp[TOF_ZONES];
    memset(tmp, 0, sizeof(tmp));
    for (int y = 0; y < TOF_GRID; y++) {
        int ny = y + dy;
        if (ny < 0 || ny >= TOF_GRID) continue;
        for (int x = 0; x < TOF_GRID; x++) {
            int nx = x + dx;
            if (nx < 0 || nx >= TOF_GRID) continue;
            tmp[ny * TOF_GRID + nx] = img[y * TOF_GRID + x];
        }
    }
    memcpy(img, tmp, sizeof(tmp));
}

int tof_preproc_frame(const tof_frame_t *in, float out[TOF_ZONES], int16_t *min_mm)
{
    int16_t dmin = INT16_MAX;

    for (int i = 0; i < TOF_ZONES; i++) {
        int16_t d = in->distance_mm[i];
        if (status_ok(in->status[i]) && d >= PRE_NEAR_MM && d < PRE_FAR_MM && d < dmin) {
            dmin = d;
        }
    }

    int n = 0;
    for (int i = 0; i < TOF_ZONES; i++) {
        int16_t d = in->distance_mm[i];
        out[i] = 0.0f;
        if (dmin == INT16_MAX || !status_ok(in->status[i]) || d < PRE_NEAR_MM || d >= PRE_FAR_MM) {
            continue;
        }
        int rel = d - dmin;
        if (rel > PRE_DEPTH_WINDOW_MM) {
            continue;   /* trop loin derrière la main : fond, avant-bras lointain... */
        }
        out[i] = 20.0f + 80.0f * (1.0f - (float)rel / (float)PRE_DEPTH_WINDOW_MM);
        n++;
    }

    if (min_mm) {
        *min_mm = (dmin == INT16_MAX) ? 0 : dmin;
    }

#if PRE_RECENTER
    if (n >= PRE_MIN_HAND_ZONES) {
        recenter(out);
    }
#endif
    return n;
}

void tof_preproc_init(tof_preproc_t *pp)
{
    memset(pp, 0, sizeof(*pp));
}

int tof_preproc_push(tof_preproc_t *pp, const tof_frame_t *in)
{
    float img[TOF_ZONES];
    int16_t dmin;
    int n = tof_preproc_frame(in, img, &dmin);

    pp->hand_zones  = (uint8_t)n;
    pp->hand_min_mm = dmin;
    pp->hand_present = (n >= PRE_MIN_HAND_ZONES) ? 1U : 0U;

    if (!pp->hand_present) {
        pp->count = 0;
        pp->head  = 0;
        return 0;
    }

    memcpy(pp->frames[pp->head], img, sizeof(img));
    pp->head = (uint8_t)((pp->head + 1U) % PRE_FRAMES_PER_SIGNAL);
    if (pp->count < PRE_FRAMES_PER_SIGNAL) {
        pp->count++;
    }
    return pp->count == PRE_FRAMES_PER_SIGNAL;
}

void tof_preproc_get_signal(const tof_preproc_t *pp, float *out)
{
    /* Quand le tampon est plein, head pointe sur la trame la plus ancienne. */
    for (int f = 0; f < PRE_FRAMES_PER_SIGNAL; f++) {
        int idx = (pp->head + f) % PRE_FRAMES_PER_SIGNAL;
        memcpy(&out[f * TOF_ZONES], pp->frames[idx], sizeof(pp->frames[idx]));
    }
}
