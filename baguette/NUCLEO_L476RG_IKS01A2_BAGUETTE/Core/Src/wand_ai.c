/**
 * wand_ai.c - Adaptateur NanoEdge AI (voir wand_ai.h)
 *
 * Les classes sont désignées par leur NOM (= nom des fichiers CSV importés dans le Studio :
 * cercle.csv, zigzag.csv…) : l'ordre d'import n'a pas d'importance.
 * API NanoEdge récente (NEAI_INPUT_SIGNAL_LENGTH, neai_get_class_name), comme le projet ST du hackathon.
 */
#include "app_config.h"

#if APP_MODE == APP_MODE_WAND

#if defined(__has_include) && !__has_include("NanoEdgeAI.h")
#error "Pas de librairie NanoEdge : copier NanoEdgeAI.h dans Core/Inc et libneai.a dans Core/Src (voir README, section 5)"
#else

#include "wand_ai.h"
#include <string.h>
#include "NanoEdgeAI.h"   /* généré par NanoEdge AI Studio */

#if !defined(NEAI_NUMBER_OF_CLASSES)
#error "NanoEdgeAI.h ne vient pas d'une librairie de classification : refaire un projet N-Class classification dans le Studio"
#endif

#if (NEAI_INPUT_SIGNAL_LENGTH * NEAI_INPUT_AXIS_NUMBER) != WAND_SIGNAL_LEN
#error "La librairie NanoEdge n'attend pas WAND_SIGNAL_LEN valeurs : 6 axes x WAND_SAMPLES points dans le Studio"
#endif

static float input[WAND_SIGNAL_LEN];
static float probs[NEAI_NUMBER_OF_CLASSES];

int wand_ai_init(void)
{
    return (neai_classification_init() == NEAI_OK) ? 0 : -1;
}

int wand_ai_classify(const uint16_t *signal, float *confidence)
{
    int id = -1;
    for (int i = 0; i < WAND_SIGNAL_LEN; i++) {
        input[i] = (float)signal[i];
    }
    if (neai_classification(input, probs, &id) != NEAI_OK || id < 0 || id >= NEAI_NUMBER_OF_CLASSES) {
        memset(probs, 0, sizeof(probs));
        if (confidence) *confidence = 0.0f;
        return -1;
    }
    if (confidence) *confidence = probs[id];
    return id;
}

int wand_ai_class_count(void)
{
    return NEAI_NUMBER_OF_CLASSES;
}

const char *wand_ai_class_name(int k)
{
    return (k >= 0 && k < NEAI_NUMBER_OF_CLASSES) ? neai_get_class_name(k) : "?";
}

float wand_ai_class_prob(int k)
{
    return (k >= 0 && k < NEAI_NUMBER_OF_CLASSES) ? probs[k] : 0.0f;
}

#endif /* __has_include("NanoEdgeAI.h") */
#endif /* APP_MODE == APP_MODE_WAND */
