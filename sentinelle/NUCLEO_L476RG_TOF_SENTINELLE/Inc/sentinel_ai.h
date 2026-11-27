/**
 * sentinel_ai.h - Adaptateur vers la librairie NanoEdge AI
 *
 * "Anomaly detection" : l'apprentissage se fait SUR LA CARTE ; la librairie générée
 * par le Studio ne contient que l'algorithme choisi par le benchmark, aucune connaissance.
 * "N-Class classification" (classes normal et anomalie) : le modèle est appris dans
 * le Studio et figé ; similarity = probabilité de la classe normal.
 * Fichiers à remplacer dans le projet : Inc/NanoEdgeAI.h et Src/libneai.a
 */
#ifndef SENTINEL_AI_H
#define SENTINEL_AI_H

#include <stdint.h>

int      sent_ai_init(void);                                      /* (ré)initialise : oublie tout. 0 = OK */
void     sent_ai_learn(const float *signal);                      /* un appel = un signal appris          */
int      sent_ai_detect(const float *signal, uint8_t *similarity); /* 0 = OK ; similarity 0..100          */
uint32_t sent_ai_min_learn(void);                                 /* nb mini d'apprentissages conseillé (0 = inconnu) */

int      sent_ai_is_trained(void);                               /* 1 = modèle figé (classification)      */

#endif /* SENTINEL_AI_H */
