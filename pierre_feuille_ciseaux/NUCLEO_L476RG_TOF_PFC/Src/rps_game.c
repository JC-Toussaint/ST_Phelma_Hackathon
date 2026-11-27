/**
 * rps_game.c - Logique Pierre-Feuille-Ciseaux (voir rps_game.h)
 */
#include "rps_game.h"
#include <string.h>

const char *rps_move_name(rps_move_t m)
{
    switch (m) {
    case RPS_PIERRE:  return "PIERRE";
    case RPS_FEUILLE: return "FEUILLE";
    case RPS_CISEAUX: return "CISEAUX";
    default:          return "?";
    }
}

rps_move_t rps_beats(rps_move_t m)
{
    switch (m) {
    case RPS_PIERRE:  return RPS_FEUILLE;  /* la feuille enveloppe la pierre */
    case RPS_FEUILLE: return RPS_CISEAUX;  /* les ciseaux coupent la feuille */
    case RPS_CISEAUX: return RPS_PIERRE;   /* la pierre casse les ciseaux    */
    default:          return RPS_NONE;
    }
}

rps_result_t rps_judge(rps_move_t player, rps_move_t stm32)
{
    if (player == stm32) {
        return RPS_DRAW;
    }
    return (rps_beats(stm32) == player) ? RPS_PLAYER_WINS : RPS_STM32_WINS;
}

/* xorshift32 */
uint32_t rps_rand(rps_game_t *g)
{
    uint32_t x = g->rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g->rng = x;
    return x;
}

void rps_rng_mix(rps_game_t *g, uint32_t entropy)
{
    g->rng ^= entropy * 2654435761u;
    if (g->rng == 0u) {
        g->rng = 0x9E3779B9u;
    }
    (void)rps_rand(g);
}

void rps_game_reset_score(rps_game_t *g)
{
    g->score_player = 0;
    g->score_stm32  = 0;
    g->draws        = 0;
}

void rps_game_init(rps_game_t *g, uint32_t seed, rps_strategy_t s, uint8_t exploration_pct)
{
    memset(g, 0, sizeof(*g));
    g->rng = seed ? seed : 0x9E3779B9u;
    g->strategy = s;
    g->exploration_pct = exploration_pct;
    g->last_player = RPS_NONE;
}

static rps_move_t random_move(rps_game_t *g)
{
    return (rps_move_t)(RPS_PIERRE + (rps_rand(g) % 3u));
}

rps_move_t rps_stm32_choose(rps_game_t *g)
{
    g->last_prediction = RPS_NONE;
    if (g->strategy == RPS_STRAT_RANDOM || (rps_rand(g) % 100u) < g->exploration_pct) {
        return random_move(g);
    }

    /* Prédit le prochain coup du joueur à partir de son coup précédent. */
    const uint16_t *row = g->markov[g->last_player];
    uint16_t best = 0;
    int nbest = 0;
    rps_move_t cand[3];
    for (int k = 0; k < 3; k++) {
        if (row[k] > best) {
            best = row[k];
            nbest = 0;
        }
        if (row[k] == best) {
            cand[nbest++] = (rps_move_t)(RPS_PIERRE + k);
        }
    }
    if (best == 0) {
        return random_move(g);     /* pas encore de données */
    }
    rps_move_t predicted = cand[rps_rand(g) % (uint32_t)nbest];
    g->last_prediction = predicted;
    return rps_beats(predicted);
}

rps_result_t rps_play_round(rps_game_t *g, rps_move_t player, rps_move_t stm32)
{
    rps_result_t r = rps_judge(player, stm32);
    if (r == RPS_PLAYER_WINS)      g->score_player++;
    else if (r == RPS_STM32_WINS)  g->score_stm32++;
    else                           g->draws++;

    if (player >= RPS_PIERRE && player <= RPS_CISEAUX) {
        uint16_t *cell = &g->markov[g->last_player][player - RPS_PIERRE];
        if (*cell < 0xFFFFu) {
            (*cell)++;
        }
        g->last_player = player;
    }
    return r;
}

uint32_t rps_commit(rps_move_t m, uint32_t nonce)
{
    uint32_t h = 2166136261u;
    const char *name = rps_move_name(m);
    for (const char *p = name; *p; p++) {
        h = (h ^ (uint8_t)*p) * 16777619u;
    }
    for (int i = 0; i < 4; i++) {
        h = (h ^ (uint8_t)(nonce >> (8 * i))) * 16777619u;
    }
    return h;
}

void rps_vote_reset(rps_vote_t *v)
{
    memset(v, 0, sizeof(*v));
}

void rps_vote_add(rps_vote_t *v, rps_move_t m)
{
    if (m >= RPS_PIERRE && m <= RPS_CISEAUX && v->count[m] < 255u) {
        v->count[m]++;
        v->total++;
    }
}

rps_move_t rps_vote_winner(const rps_vote_t *v, uint8_t *count)
{
    rps_move_t best = RPS_NONE;
    uint8_t bc = 0;
    for (int m = RPS_PIERRE; m <= RPS_CISEAUX; m++) {
        if (v->count[m] > bc) {
            bc = v->count[m];
            best = (rps_move_t)m;
        }
    }
    if (count) {
        *count = bc;
    }
    return best;
}
