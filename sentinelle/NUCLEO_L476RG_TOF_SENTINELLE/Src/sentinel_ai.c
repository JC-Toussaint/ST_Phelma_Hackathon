/**
 * sentinel_ai.c - Adaptateur NanoEdge AI (voir sentinel_ai.h)
 *
 * Deux types de librairie acceptés :
 *  - "Anomaly detection" (conseillé) : apprentissage sur la carte ;
 *  - "N-Class classification" avec les classes normal et anomalie : modèle appris
 *    dans le Studio, similarité = probabilité de "normal".
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

#if defined(NEAI_INPUT_SIGNAL_LENGTH)
#define SENT_NEAI_LEN  (NEAI_INPUT_SIGNAL_LENGTH * NEAI_INPUT_AXIS_NUMBER)
#else
#define SENT_NEAI_LEN  (DATA_INPUT_USER * AXIS_NUMBER)
#endif

#if SENT_NEAI_LEN != SENT_SIGNAL_LEN
#error "La librairie NanoEdge n'attend pas SENT_SIGNAL_LEN valeurs : vérifier SENT_FRAMES_PER_SIGNAL et le projet Studio."
#endif

static float input[SENT_SIGNAL_LEN];

#if defined(NEAI_NUMBER_OF_CLASSES)
/* ------------------------------------------------------------------------
 * Librairie de CLASSIFICATION (projet Studio "N-Class classification" avec les
 * classes normal et anomalie) : le modèle est appris dans le Studio, pas sur la
 * carte. La similarité est la probabilité de la classe "normal", en %.
 * app.c ne lance alors aucune phase d'apprentissage (voir sent_ai_is_trained).
 * ------------------------------------------------------------------------ */
#include <ctype.h>

static float probs[NEAI_NUMBER_OF_CLASSES];
static int   normal_id = 0;

static int is_normal_name(const char *n)
{
    static const char ref[] = "normal";
    if (n == NULL) return 0;
    for (int i = 0; ref[i]; i++) {
        if (tolower((unsigned char)n[i]) != ref[i]) return 0;
    }
    return n[6] == '\0';
}

int sent_ai_init(void)
{
    if (neai_classification_init() != NEAI_OK) return -1;
    normal_id = -1;
    for (int k = 0; k < NEAI_NUMBER_OF_CLASSES; k++) {
        if (is_normal_name(neai_get_class_name(k))) normal_id = k;
    }
    return (normal_id < 0) ? -2 : 0;    /* -2 : pas de classe "normal" dans la librairie */
}

void sent_ai_learn(const float *signal)
{
    (void)signal;                        /* modèle figé : rien à apprendre */
}

int sent_ai_detect(const float *signal, uint8_t *similarity)
{
    int id = 0;
    memcpy(input, signal, sizeof(input));
    if (neai_classification(input, probs, &id) != NEAI_OK) return -1;
    float p = probs[normal_id] * 100.0f + 0.5f;
    *similarity = (uint8_t)((p > 100.0f) ? 100.0f : (p < 0.0f ? 0.0f : p));
    return 0;
}

uint32_t sent_ai_min_learn(void)
{
    return 0;
}

int sent_ai_is_trained(void)
{
    return 1;
}

#else
/* ------------------------------------------------------------------------
 * Librairie de DÉTECTION D'ANOMALIE (projet Studio "Anomaly detection") :
 * apprentissage sur la carte.
 * ------------------------------------------------------------------------ */
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

int sent_ai_is_trained(void)
{
    return 0;
}
#endif /* NEAI_NUMBER_OF_CLASSES */

#endif /* APP_MODE == APP_MODE_SENTINEL */
