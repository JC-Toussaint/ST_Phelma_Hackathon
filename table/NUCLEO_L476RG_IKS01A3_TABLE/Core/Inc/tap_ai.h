/**
 * tap_ai.h - Adaptateur vers la librairie NanoEdge AI (N-class classification)
 *
 * La librairie générée par NanoEdge AI Studio se copie dans le projet :
 *   Core/Inc/NanoEdgeAI.h et Core/Src/libneai.a   (voir README).
 * Le dépôt fournit une librairie d'essai (4 zones, apprise sur une planche) : à remplacer par la vôtre.
 */
#ifndef TAP_AI_H
#define TAP_AI_H

#include <stdint.h>

/* Retourne 0 si OK, -1 si l'initialisation NanoEdge échoue, -2 si la taille du signal ne convient pas. */
int         tap_ai_init(void);

/* Classe un signal de TAP_SIGNAL_LEN valeurs. Retourne l'indice de la classe la plus probable
 * (0 .. tap_ai_class_count() - 1), -1 en cas d'erreur ; *confidence = sa probabilité (0..1). */
int         tap_ai_classify(const uint16_t *signal, float *confidence);

/* Résultat complet de la dernière classification, dans l'ordre des classes de la librairie. */
int         tap_ai_class_count(void);
const char *tap_ai_class_name(int k);
float       tap_ai_class_prob(int k);

#endif /* TAP_AI_H */
