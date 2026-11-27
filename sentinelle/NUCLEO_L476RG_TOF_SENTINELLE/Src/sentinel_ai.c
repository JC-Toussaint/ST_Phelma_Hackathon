/**
 * sentinel_ai.c - Adaptateur NanoEdge AI "Anomaly detection" (voir sentinel_ai.h)
 *
 * Seul NEAI_OK est utilisé parmi les codes de retour : les autres valeurs de
 * l'énumération neai_state varient selon les versions du Studio.
 * Taille d'entrée : API récente (NEAI_INPUT_SIGNAL_LENGTH, comme dans le projet
 * ST) ou ancienne API (DATA_INPUT_USER x AXIS_NUMBER).
 */
#include "app_config.h"

#if APP_MODE == APP_MODE_SENTINEL

#include "sentinel_ai.h"
#include <string.h>
#include "NanoEdgeAI.h"   /* généré par NanoEdge AI Studio (projet Anomaly detection) */

/* Une librairie de détection d'anomalie n'a pas de classes : NEAI_NUMBER_OF_CLASSES
 * signale une librairie de classification (par exemple la librairie modèle de ST
 * fournie dans le dépôt), qui n'a pas les fonctions neai_anomalydetection_*. */
#if defined(NEAI_NUMBER_OF_CLASSES)
#error "NanoEdgeAI.h vient d'une librairie de CLASSIFICATION (librairie modele de ST ?) : generer dans NanoEdge AI Studio un projet 'Anomaly detection', puis remplacer Inc/NanoEdgeAI.h et Src/libneai.a (voir sentinelle/README.md)"
#endif

#if defined(NEAI_INPUT_SIGNAL_LENGTH)
#define SENT_NEAI_LEN  (NEAI_INPUT_SIGNAL_LENGTH * NEAI_INPUT_AXIS_NUMBER)
#else
#define SENT_NEAI_LEN  (DATA_INPUT_USER * AXIS_NUMBER)
#endif

#if SENT_NEAI_LEN != SENT_SIGNAL_LEN
#error "La librairie NanoEdge n'attend pas SENT_SIGNAL_LEN valeurs : vérifier SENT_FRAMES_PER_SIGNAL et le projet Studio."
#endif

static float input[SENT_SIGNAL_LEN];

int sent_ai_init(void)
{
    return (neai_anomalydetection_init() == NEAI_OK) ? 0 : -1;
}

void sent_ai_learn(const float *signal)
{
    memcpy(input, signal, sizeof(input));       /* l'API prend un float* non const */
    /* Renvoie NEAI_LEARNING_IN_PROGRESS / NEAI_LEARNING_DONE... selon la version : ignoré. */
    (void)neai_anomalydetection_learn(input);
}

int sent_ai_detect(const float *signal, uint8_t *similarity)
{
    memcpy(input, signal, sizeof(input));
    return (neai_anomalydetection_detect(input, similarity) == NEAI_OK) ? 0 : -1;
}

uint32_t sent_ai_min_learn(void)
{
#ifdef MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING
    return MINIMUM_ITERATION_CALLS_FOR_EFFICIENT_LEARNING;
#else
    return 0;   /* sinon : reporter dans SENT_LEARN_SIGNALS la valeur conseillée par le Studio */
#endif
}

#endif /* APP_MODE == APP_MODE_SENTINEL */
