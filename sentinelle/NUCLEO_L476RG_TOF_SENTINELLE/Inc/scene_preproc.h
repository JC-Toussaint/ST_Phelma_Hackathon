/**
 * scene_preproc.h - Sentinelle : prétraitement de la scène et outils associés
 * (code portable, sans HAL)
 *
 * Contrairement à Pierre-Feuille-Ciseaux, on ne segmente rien : on garde les
 * distances ABSOLUES de toute la scène (c'est elle qu'on apprend).
 *   valeur d'une zone = 100 * min(distance, SENT_MAX_MM) / SENT_MAX_MM
 *   zone sans cible / invalide = 100 (loin)
 *
 * En plus de NanoEdge (qui dit "c'est anormal" avec un score de similarité),
 * on apprend en parallèle la moyenne et l'écart-type de chaque zone pour
 * EXPLIQUER l'alarme : une carte 8x8 qui montre où la scène a changé
 * ('+' = quelque chose est apparu / s'est rapproché, '-' = a disparu / s'est éloigné).
 */
#ifndef SCENE_PREPROC_H
#define SCENE_PREPROC_H

#include <stdint.h>
#include "app_config.h"
#include "tof_frame.h"

/* --- signal ------------------------------------------------------------- */
typedef struct {
    float   frames[SENT_FRAMES_PER_SIGNAL][TOF_ZONES];
    uint8_t count;
} scene_preproc_t;

int16_t scene_zone_mm(int16_t distance_mm, uint8_t status);     /* 0..SENT_MAX_MM */
void    scene_preproc_init(scene_preproc_t *sp);
/* Retourne 1 quand SENT_FRAMES_PER_SIGNAL trames sont réunies (fenêtres disjointes). */
int     scene_preproc_push(scene_preproc_t *sp, const tof_frame_t *f);
/* Copie le signal dans out[SENT_SIGNAL_LEN] et repart pour la fenêtre suivante. */
void    scene_preproc_take(scene_preproc_t *sp, float *out);

/* --- modèle de référence par zone (Welford) ------------------------------ */
typedef struct {
    uint32_t n;
    float    mean[TOF_ZONES];
    float    m2[TOF_ZONES];
} scene_ref_t;

void scene_ref_reset(scene_ref_t *r);
void scene_ref_add(scene_ref_t *r, const tof_frame_t *f);
/* Remplit map[64] avec '.', '+' ou '-' ; retourne le nombre de zones changées. */
int  scene_ref_diff(const scene_ref_t *r, const tof_frame_t *f, char map[TOF_ZONES]);

/* --- alarme avec hystérésis ------------------------------------------------- */
typedef enum { ALARM_NONE = 0, ALARM_RAISED, ALARM_CLEARED } alarm_event_t;

typedef struct {
    uint8_t threshold;   /* similarité < threshold => signal anormal */
    uint8_t confirm;     /* nb de signaux consécutifs pour changer d'état */
    uint8_t streak;
    uint8_t active;
} alarm_t;

void          alarm_init(alarm_t *a, uint8_t threshold, uint8_t confirm);
alarm_event_t alarm_update(alarm_t *a, uint8_t similarity);

#endif /* SCENE_PREPROC_H */
