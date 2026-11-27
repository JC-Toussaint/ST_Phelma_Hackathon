/**
 * app.c - Pierre-Feuille-Ciseaux contre le STM32 (VL53L5CX 8x8 + NanoEdge AI)
 *
 * APP_MODE_DATALOG : un signal prétraité (main présente) toutes les PRE_DATALOG_EVERY
 *                    trames est envoyé sur l'UART, une ligne CSV de SIGNAL_LEN entiers 0..100.
 *                    Les lignes commençant par '#' sont des messages.
 * APP_MODE_GAME    : jeu complet, piloté par le bouton bleu B1 ou le terminal série.
 *
 * Terminal série : port COM virtuel ST-LINK (USART2), 115200 bauds, 8N1.
 */
#include "app.h"
#include <stddef.h>
#include "main.h"
#include "app_config.h"

#include "tof_sensor.h"
#include "tof_preproc.h"
#include "uart_io.h"
#include "rps_game.h"
#if APP_MODE == APP_MODE_GAME
#include "rps_ai.h"
#endif

static tof_preproc_t pre;
static tof_frame_t   frame;
static float         nn_signal[SIGNAL_LEN];

/* Erreur bloquante : LED qui clignote vite et message RÉPÉTÉ toutes les 2 s,
 * pour qu'il soit visible même si le terminal est ouvert après le démarrage. */
static void fatal(const char *msg, int code)
{
    for (int n = 0;; n++) {
        if (n % 20 == 0) {
            uart_puts("# ERREUR : ");
            uart_puts(msg);
            uart_puts(" (code ");
            uart_put_int(code);
            uart_puts(") - appuyer sur RESET (bouton noir) apres correction\r\n");
        }
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
        HAL_Delay(100);
    }
}

/* Lit une trame si disponible et la prétraite. Retourne 1 si un signal complet est prêt. */
static int acquire(uint32_t *entropy)
{
    if (!tof_sensor_poll(&frame)) {
        return -1;   /* pas de nouvelle trame */
    }
    if (entropy) {
        uint32_t e = HAL_GetTick();
        for (int i = 0; i < TOF_ZONES; i++) {
            e = (e * 31u) ^ (uint16_t)frame.distance_mm[i];   /* le bruit de mesure est une bonne source d'aléa */
        }
        *entropy = e;
    }
    if (tof_preproc_push(&pre, &frame)) {
        tof_preproc_get_signal(&pre, nn_signal);
        return 1;
    }
    return 0;
}

/* ========================================================================= */
#if APP_MODE == APP_MODE_DATALOG
/* ========================================================================= */

void app_init(void)
{
    uart_io_init();
    uart_puts("\r\n# Pierre-Feuille-Ciseaux : mode COLLECTE\r\n");
    int err = tof_sensor_init();
    if (err) {
        fatal("init VL53L5CX", err);
    }
    tof_preproc_init(&pre);
    uart_puts("# Pret. Une ligne = ");
    uart_put_int(PRE_FRAMES_PER_SIGNAL);
    uart_puts(" trames x 64 zones, un signal toutes les ");
    uart_put_int(PRE_DATALOG_EVERY);
    uart_puts(" trames (115200 bauds)\r\n");
}

void app_loop(void)
{
    static uint32_t t_status, frames, sent, since_sent = PRE_DATALOG_EVERY;
    int r = acquire(NULL);

    if (r >= 0) {
        frames++;
        if (since_sent < PRE_DATALOG_EVERY) {
            since_sent++;
        }
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, pre.hand_present ? GPIO_PIN_SET : GPIO_PIN_RESET);
        /* Un signal sur PRE_DATALOG_EVERY : à 115200 bauds, l'envoi tient entre deux signaux
         * sans faire manquer de trame au capteur. */
        if (r == 1 && since_sent >= PRE_DATALOG_EVERY) {
            uart_put_csv(nn_signal, SIGNAL_LEN);
            sent++;
            since_sent = 0u;
        }
    }

    /* Rien n'est envoyé sans main : on donne des nouvelles chaque seconde. */
    if (HAL_GetTick() - t_status >= 1000u) {
        t_status = HAL_GetTick();
        if (sent == 0u) {
            if (frames == 0u) {
                uart_puts("# Aucune trame recue du capteur depuis 1 s !\r\n");
            } else {
                uart_puts("# En attente d'une main (");
                uart_put_int((int32_t)frames);
                uart_puts(" trames/s, ");
                uart_put_int(pre.hand_zones);
                uart_puts(" zones entre " );
                uart_put_int(PRE_NEAR_MM);
                uart_puts(" et ");
                uart_put_int(PRE_FAR_MM);
                uart_puts(" mm, il en faut ");
                uart_put_int(PRE_MIN_HAND_ZONES);
                uart_puts(")\r\n");
            }
        }
        frames = 0u;
        sent = 0u;
    }
}

/* ========================================================================= */
#else /* APP_MODE_GAME */
/* ========================================================================= */

typedef enum { ST_IDLE, ST_COUNTDOWN, ST_CAPTURE } state_t;

static rps_game_t game;
static rps_vote_t vote;
static state_t    state;
static uint32_t   t0;
static int        step;
static rps_move_t stm32_move;
static uint32_t   nonce;
static int        live_debug;
static uint8_t    b1_prev = 1;

static const char *const ART[4][4] = {
    { "        ", "   ??   ", "        ", "        " },
    { "  ____  ", " (____) ", " (____) ", " (____) " },   /* pierre  */
    { "  _____ ", " |     |", " |     |", " |_____|" },   /* feuille */
    { " \\   /  ", "  \\ /   ", "   X    ", "  O O   " },   /* ciseaux */
};

static void print_menu(void)
{
    uart_puts("\r\n=====================================\r\n");
    uart_puts("  PIERRE - FEUILLE - CISEAUX vs STM32\r\n");
    uart_puts("=====================================\r\n");
    uart_puts("  [B1] ou [espace] : jouer une manche\r\n");
    uart_puts("  [m] strategie    : ");
    uart_puts(game.strategy == RPS_STRAT_ADAPTIVE ? "ADAPTATIVE (j'apprends tes habitudes)\r\n"
                                                  : "ALEATOIRE\r\n");
    uart_puts("  [r] remise a zero du score\r\n");
    uart_puts("  [d] affichage en direct de la reconnaissance\r\n");
    uart_puts("  Tiens ta main a 10-");
    uart_put_int(PRE_FAR_MM / 10);           /* au-delà de PRE_FAR_MM, la main est ignorée */
    uart_puts(" cm au-dessus du capteur.\r\n\r\n");
}

static void print_score(void)
{
    uart_puts("  Score >> Toi ");
    uart_put_int(game.score_player);
    uart_puts(" - ");
    uart_put_int(game.score_stm32);
    uart_puts(" STM32   (egalites : ");
    uart_put_int(game.draws);
    uart_puts(")\r\n\r\n");
}

static void print_duel(rps_move_t player, rps_move_t stm32)
{
    uart_puts("\r\n     TOI            STM32\r\n");
    for (int l = 0; l < 4; l++) {
        uart_puts("   ");
        uart_puts(ART[player][l]);
        uart_puts("   vs   ");
        uart_puts(ART[stm32][l]);
        uart_puts("\r\n");
    }
    uart_puts("   ");
    uart_puts(rps_move_name(player));
    uart_puts("          ");
    uart_puts(rps_move_name(stm32));
    uart_puts("\r\n\r\n");
}

static void start_round(void)
{
    stm32_move = rps_stm32_choose(&game);
    nonce = rps_rand(&game);
    uart_puts("\r\n-- Nouvelle manche --\r\n");
    uart_puts("  J'ai choisi mon coup. Empreinte : ");
    uart_put_hex32(rps_commit(stm32_move, nonce));
    uart_puts("  (preuve que je ne triche pas)\r\n  ");
    state = ST_COUNTDOWN;
    step = 0;
    t0 = HAL_GetTick() - GAME_COUNTDOWN_STEP_MS;   /* affiche le 1er mot tout de suite */
}

static void finish_round(rps_move_t player)
{
    rps_move_t predicted = game.last_prediction;
    rps_result_t r = rps_play_round(&game, player, stm32_move);

    print_duel(player, stm32_move);
    if (r == RPS_DRAW) {
        uart_puts("  >>> EGALITE !\r\n");
    } else if (r == RPS_PLAYER_WINS) {
        uart_puts("  >>> TU GAGNES ! ");
        uart_puts(rps_move_name(player));
        uart_puts(" bat ");
        uart_puts(rps_move_name(stm32_move));
        uart_puts("\r\n");
    } else {
        uart_puts("  >>> LE STM32 GAGNE ! ");
        uart_puts(rps_move_name(stm32_move));
        uart_puts(" bat ");
        uart_puts(rps_move_name(player));
        uart_puts("\r\n");
    }
    if (predicted != RPS_NONE) {
        uart_puts("  (je pensais que tu jouerais ");
        uart_puts(rps_move_name(predicted));
        uart_puts(predicted == player ? " ... bien vu !)\r\n" : " ... rate.)\r\n");
    }
    uart_puts("  Verification : coup=");
    uart_puts(rps_move_name(stm32_move));
    uart_puts(" secret=");
    uart_put_hex32(nonce);
    uart_puts(" -> empreinte ");
    uart_put_hex32(rps_commit(stm32_move, nonce));
    uart_puts("\r\n");
    print_score();

    /* LED : allumée fixe si le STM32 gagne, éteinte sinon */
    HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, (r == RPS_STM32_WINS) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    state = ST_IDLE;
}

static void handle_inputs(void)
{
    int c = uart_getc_nonblock();
    uint8_t b1 = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET) ? 1 : 0;  /* B1 actif bas */
    int b1_pressed = (b1_prev && !b1);
    b1_prev = b1;

    if (state != ST_IDLE) {
        return;
    }
    if (b1_pressed || c == ' ' || c == '\r' || c == '\n') {
        start_round();
    } else if (c == 'm' || c == 'M') {
        game.strategy = (game.strategy == RPS_STRAT_ADAPTIVE) ? RPS_STRAT_RANDOM : RPS_STRAT_ADAPTIVE;
        print_menu();
    } else if (c == 'r' || c == 'R') {
        rps_game_reset_score(&game);
        uart_puts("  Score remis a zero.\r\n");
        print_score();
    } else if (c == 'd' || c == 'D') {
        live_debug = !live_debug;
        uart_puts(live_debug ? "  Direct : ON\r\n" : "  Direct : OFF\r\n");
    } else if (c == 'h' || c == '?') {
        print_menu();
    }
}

void app_init(void)
{
    uart_io_init();
    uart_puts("\r\n# Pierre-Feuille-Ciseaux : mode JEU\r\n# Init capteur (1-2 s)...\r\n");
    int err = tof_sensor_init();
    if (err) {
        fatal("init VL53L5CX", err);
    }
    err = rps_ai_init();
    if (err) {
        fatal("init NanoEdge (classes pierre/feuille/ciseaux introuvables ?)", err);
    }
    tof_preproc_init(&pre);
    rps_game_init(&game, HAL_GetTick() ^ 0xC0FFEEu, RPS_STRAT_ADAPTIVE, GAME_EXPLORATION_PCT);
    state = ST_IDLE;
    print_menu();
}

void app_loop(void)
{
    static const char *const WORDS[] = { "Pierre... ", "Feuille... ", "Ciseaux... ", "CHIFOUMI !\r\n" };
    static uint32_t last_debug;
    uint32_t entropy = 0;

    handle_inputs();

    int r = acquire(&entropy);
    if (r >= 0) {
        rps_rng_mix(&game, entropy);
    }

    switch (state) {
    case ST_IDLE:
        if (live_debug && r == 1 && HAL_GetTick() - last_debug > 300) {
            float conf;
            rps_move_t m = rps_ai_classify(nn_signal, &conf);
            last_debug = HAL_GetTick();
            uart_puts("  [direct] ");
            uart_puts(rps_move_name(m));
            uart_puts(" ");
            uart_put_pct(conf);
            uart_puts("  (");
            for (int k = 0; k < rps_ai_class_count(); k++) {
                uart_puts(k ? " " : "");
                uart_puts(rps_ai_class_name(k));
                uart_puts(" ");
                uart_put_pct(rps_ai_class_prob(k));
            }
            uart_puts(")  (");
            uart_put_int(pre.hand_zones);
            uart_puts(" zones, ");
            uart_put_int(pre.hand_min_mm);
            uart_puts(" mm)\r\n");
        }
        break;

    case ST_COUNTDOWN:
        if (HAL_GetTick() - t0 >= GAME_COUNTDOWN_STEP_MS) {
            t0 = HAL_GetTick();
            uart_puts(WORDS[step]);
            HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
            if (++step == 4) {
                rps_vote_reset(&vote);
                HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
                state = ST_CAPTURE;
            }
        }
        break;

    case ST_CAPTURE: {
        uint8_t best_count = 0;
        if (r == 1) {
            float conf;
            rps_move_t m = rps_ai_classify(nn_signal, &conf);
            if (m != RPS_NONE && conf >= GAME_MIN_CONFIDENCE) {
                rps_vote_add(&vote, m);
            }
        }
        rps_move_t best = rps_vote_winner(&vote, &best_count);
        if (best_count >= GAME_VOTES_NEEDED) {
            finish_round(best);
        } else if (HAL_GetTick() - t0 > GAME_CAPTURE_MS) {
            if (best_count >= 2) {
                finish_round(best);     /* peu de votes mais un gagnant clair */
            } else {
                uart_puts("  Je n'ai pas bien vu ta main... manche annulee. [B1] pour rejouer.\r\n");
                HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
                state = ST_IDLE;
            }
        }
        break;
    }
    }
}

#endif /* APP_MODE */
