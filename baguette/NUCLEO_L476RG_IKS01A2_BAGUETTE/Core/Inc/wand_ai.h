/**
 * wand_ai.h - Adaptateur vers la librairie NanoEdge AI (N-class classification)
 *
 * La librairie générée par NanoEdge AI Studio se copie dans le projet :
 *   Core/Inc/NanoEdgeAI.h et Core/Src/libneai.a   (voir README).
 * Le libneai.a fourni dans le dépôt ne contient aucune fonction NanoEdge : il permet de compiler le mode collecte.
 */
#ifndef WAND_AI_H
#define WAND_AI_H

#include <stdint.h>

/* Retourne 0 si OK, -1 si l'initialisation NanoEdge échoue, -2 si la taille du signal ne convient pas. */
int         wand_ai_init(void);

/* Classe un signal de WAND_SIGNAL_LEN valeurs. Retourne l'indice de la classe la plus probable
 * (0 .. wand_ai_class_count() - 1), -1 en cas d'erreur ; *confidence = sa probabilité (0..1). */
int         wand_ai_classify(const uint16_t *signal, float *confidence);

/* Résultat complet de la dernière classification, dans l'ordre des classes de la librairie. */
int         wand_ai_class_count(void);
const char *wand_ai_class_name(int k);
float       wand_ai_class_prob(int k);

#endif /* WAND_AI_H */
