/**
 * rps_game.h - Logique Pierre-Feuille-Ciseaux (code portable, sans HAL)
 *
 * - arbitrage et score
 * - adversaire STM32 : aléatoire ou ADAPTATIF (modèle de Markov d'ordre 1 sur
 *   les coups du joueur : il apprend tes habitudes et joue ce qui les bat)
 * - vote majoritaire sur plusieurs classifications NanoEdge
 * - "engagement" : le STM32 publie une empreinte de son coup AVANT de voir le
 *   tien, puis révèle le secret : on peut vérifier qu'il ne triche pas.
 */
#ifndef RPS_GAME_H
#define RPS_GAME_H

#include <stdint.h>

typedef enum {
    RPS_NONE = 0,
    RPS_PIERRE,
    RPS_FEUILLE,
    RPS_CISEAUX,
    RPS_NB_MOVES = 3
} rps_move_t;

typedef enum {
    RPS_DRAW = 0,
    RPS_PLAYER_WINS,
    RPS_STM32_WINS
} rps_result_t;

typedef enum {
    RPS_STRAT_RANDOM = 0,
    RPS_STRAT_ADAPTIVE
} rps_strategy_t;

typedef struct {
    uint32_t       rng;
    rps_strategy_t strategy;
    uint8_t        exploration_pct;
    rps_move_t     last_player;                /* RPS_NONE avant le 1er coup        */
    rps_move_t     last_prediction;            /* coup du joueur prédit (RPS_NONE si coup aléatoire) */
    uint16_t       markov[4][3];               /* [coup précédent][coup suivant]    */
    uint16_t       score_player, score_stm32, draws;
} rps_game_t;

typedef struct {
    uint8_t count[4];
    uint8_t total;
} rps_vote_t;

const char  *rps_move_name(rps_move_t m);
rps_move_t   rps_beats(rps_move_t m);                       /* coup qui bat m */
rps_result_t rps_judge(rps_move_t player, rps_move_t stm32);

void         rps_game_init(rps_game_t *g, uint32_t seed, rps_strategy_t s, uint8_t exploration_pct);
void         rps_game_reset_score(rps_game_t *g);
uint32_t     rps_rand(rps_game_t *g);
void         rps_rng_mix(rps_game_t *g, uint32_t entropy);   /* ajoute de l'entropie (bruit ToF...) */
rps_move_t   rps_stm32_choose(rps_game_t *g);
rps_result_t rps_play_round(rps_game_t *g, rps_move_t player, rps_move_t stm32); /* score + apprentissage */

/* Engagement : empreinte FNV-1a de (coup, secret). */
uint32_t     rps_commit(rps_move_t m, uint32_t nonce);

void         rps_vote_reset(rps_vote_t *v);
void         rps_vote_add(rps_vote_t *v, rps_move_t m);
rps_move_t   rps_vote_winner(const rps_vote_t *v, uint8_t *count);

#endif /* RPS_GAME_H */
