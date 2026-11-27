/**
 * rps_ai.c - Adaptateur NanoEdge AI (voir rps_ai.h)
 *
 * Les classes sont associées aux coups par leur NOM (= nom des fichiers CSV
 * importés dans NanoEdge AI Studio : pierre.csv, feuille.csv, ciseaux.csv),
 * pas par leur ordre : l'ordre d'import n'a donc pas d'importance.
 *
 * Deux générations d'API NanoEdge sont gérées :
 *  - API récente (celle du projet ST) : NEAI_INPUT_SIGNAL_LENGTH, init sans
 *    argument, ids 0..N-1, neai_get_class_name() ;
 *  - ancienne API : DATA_INPUT_USER/AXIS_NUMBER, knowledge.h, ids 1..N, id2class[].
 */
#include "app_config.h"

#if APP_MODE == APP_MODE_GAME

#include "rps_ai.h"
#include <string.h>
#include <ctype.h>
#include "NanoEdgeAI.h"   /* généré par NanoEdge AI Studio */

#if defined(NEAI_INPUT_SIGNAL_LENGTH)            /* --- API récente --- */
#define RPS_NEAI_LEN        (NEAI_INPUT_SIGNAL_LENGTH * NEAI_INPUT_AXIS_NUMBER)
#define RPS_NEAI_CLASSES    NEAI_NUMBER_OF_CLASSES
#define RPS_NEAI_FIRST_ID   0
#define RPS_NEAI_ID_T       int
#define RPS_NEAI_INIT()     neai_classification_init()
#define RPS_NEAI_NAME(id)   neai_get_class_name(id)
#else                                            /* --- ancienne API --- */
#include "knowledge.h"
#define RPS_NEAI_LEN        (DATA_INPUT_USER * AXIS_NUMBER)
#define RPS_NEAI_CLASSES    CLASS_NUMBER
#define RPS_NEAI_FIRST_ID   1
#define RPS_NEAI_ID_T       NEAI_ID_TYPE
#define RPS_NEAI_INIT()     neai_classification_init(knowledge)
#define RPS_NEAI_NAME(id)   id2class[id]
#endif

#if RPS_NEAI_LEN != SIGNAL_LEN
#error "La librairie NanoEdge n'attend pas SIGNAL_LEN valeurs : vérifier PRE_FRAMES_PER_SIGNAL et le projet Studio."
#endif

static rps_move_t id2move[RPS_NEAI_CLASSES + 1];
static float      input[SIGNAL_LEN];
static float      probs[RPS_NEAI_CLASSES];

/* Cherche "needle" (en minuscules) dans "hay" sans tenir compte de la casse. */
static int contains_nocase(const char *hay, const char *needle)
{
    size_t n = strlen(needle);
    for (; hay && *hay; hay++) {
        size_t i = 0;
        while (i < n && hay[i] && tolower((unsigned char)hay[i]) == needle[i]) {
            i++;
        }
        if (i == n) {
            return 1;
        }
    }
    return 0;
}

static rps_move_t name2move(const char *name)
{
    if (contains_nocase(name, "pierre") || contains_nocase(name, "rock"))     return RPS_PIERRE;
    if (contains_nocase(name, "feuille") || contains_nocase(name, "paper"))   return RPS_FEUILLE;
    if (contains_nocase(name, "ciseau") || contains_nocase(name, "scissor"))  return RPS_CISEAUX;
    return RPS_NONE;
}

int rps_ai_init(void)
{
    if (RPS_NEAI_INIT() != NEAI_OK) {
        return -1;
    }
    int found = 0;
    for (int k = 0; k < RPS_NEAI_CLASSES; k++) {
        id2move[k] = name2move(RPS_NEAI_NAME(k + RPS_NEAI_FIRST_ID));
        if (id2move[k] != RPS_NONE) {
            found++;
        }
    }
    return (found >= 3) ? 0 : -2;
}

rps_move_t rps_ai_classify(const float *signal, float *confidence)
{
    RPS_NEAI_ID_T id = 0;
    int k;
    memcpy(input, signal, sizeof(input));  /* l'API NanoEdge prend un float* non const */
    if (neai_classification(input, probs, &id) != NEAI_OK) {
        memset(probs, 0, sizeof(probs));
        if (confidence) *confidence = 0.0f;
        return RPS_NONE;
    }
    k = (int)id - RPS_NEAI_FIRST_ID;        /* index 0..N-1 */
    if (k < 0 || k >= RPS_NEAI_CLASSES) {
        if (confidence) *confidence = 0.0f;
        return RPS_NONE;
    }
    if (confidence) {
        *confidence = probs[k];
    }
    return id2move[k];
}

int rps_ai_class_count(void)
{
    return RPS_NEAI_CLASSES;
}

const char *rps_ai_class_name(int k)
{
    if (k < 0 || k >= RPS_NEAI_CLASSES) {
        return "?";
    }
    return RPS_NEAI_NAME(k + RPS_NEAI_FIRST_ID);
}

float rps_ai_class_prob(int k)
{
    return (k >= 0 && k < RPS_NEAI_CLASSES) ? probs[k] : 0.0f;
}

#endif /* APP_MODE == APP_MODE_GAME */
