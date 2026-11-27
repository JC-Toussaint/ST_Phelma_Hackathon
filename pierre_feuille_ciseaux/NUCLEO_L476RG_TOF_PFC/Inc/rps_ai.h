/**
 * rps_ai.h - Adaptateur vers la librairie NanoEdge AI (N-class classification)
 *
 * La librairie générée par NanoEdge AI Studio remplace les fichiers modèles du projet ST :
 *   Inc/NanoEdgeAI.h et Src/libneai.a (+ Inc/knowledge.h avec les anciennes versions du Studio).
 * (voir README).
 */
#ifndef RPS_AI_H
#define RPS_AI_H

#include "rps_game.h"

/* Retourne 0 si OK. Vérifie aussi que les noms de classes sont reconnus. */
int        rps_ai_init(void);

/* Classe un signal de SIGNAL_LEN valeurs. confidence (0..1) peut être NULL. */
rps_move_t rps_ai_classify(const float *signal, float *confidence);

/* Résultat complet de la dernière classification : une probabilité par classe (0..1), dans l'ordre des
 * classes de la librairie NanoEdge (pas forcément pierre, feuille, ciseaux), avec leur nom.
 * k va de 0 à rps_ai_class_count() - 1. Probabilités à 0 si la dernière classification a échoué. */
int         rps_ai_class_count(void);
const char *rps_ai_class_name(int k);
float       rps_ai_class_prob(int k);

#endif /* RPS_AI_H */
