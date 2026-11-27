/**
 * app.c - Sentinelle : le STM32 apprend SUR PLACE à quoi ressemble une scène
 * "normale" (NanoEdge AI, apprentissage embarqué), puis signale tout changement
 * en expliquant OÙ la scène a changé.
 *
 * APP_MODE_DATALOG  : envoie chaque signal "scène" en CSV (collecte pour le benchmark Studio).
 * APP_MODE_SENTINEL : fonctionne seul, sans PC. Deux cas selon la librairie :
 *   - "Anomaly detection" (apprentissage embarqué) :
 *     démarrage -> 5 s pour sortir du champ -> apprentissage (~20 s) -> surveillance
 *     B1 court  : AJOUTER la situation actuelle comme normale (apprentissage incrémental)
 *     B1 long   : TOUT RÉAPPRENDRE (la connaissance est effacée)
 *     LED       : clignote lentement = sors du champ ; fixe = apprentissage ;
 *                 éteinte = surveillance ; clignote vite = ALARME
 *   - "N-Class classification" (classes normal / anomalie, modèle appris dans le
 *     Studio) : surveillance DÈS le démarrage, rien à apprendre ; la similarité est
 *     la probabilité de "normal". Une image de référence de la scène est prise au
 *     démarrage (1 s) pour la carte des changements ; B1, [l] ou [a] la reprennent
 *     après 5 s (le temps de sortir du champ).
 *
 * Terminal série : port COM virtuel ST-LINK, APP_UART_BAUD (115200) bauds, 8N1.
 */
#include "app.h"
#include <stddef.h>
#include "main.h"
#include "app_config.h"
#include "tof_sensor.h"
#include "scene_preproc.h"
#include "uart_io.h"
#if APP_MODE == APP_MODE_SENTINEL
#include "sentinel_ai.h"
#endif

static scene_preproc_t sp;
static tof_frame_t     frame;
static float           nn_signal[SENT_SIGNAL_LEN];

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

/* ========================================================================= */
#if APP_MODE == APP_MODE_DATALOG
/* ========================================================================= */

void app_init(void)
{
    uart_io_init();
    uart_puts("\r\n# Sentinelle : mode COLLECTE\r\n");
    int err = tof_sensor_init();
    if (err) {
        fatal("init VL53L5CX", err);
    }
    scene_preproc_init(&sp);
    uart_puts("# Pret. Une ligne = ");
    uart_put_int(SENT_FRAMES_PER_SIGNAL);
    uart_puts(" trames x 64 zones\r\n");
}

void app_loop(void)
{
    if (!tof_sensor_poll(&frame)) {
        return;
    }
    if (scene_preproc_push(&sp, &frame)) {
        scene_preproc_take(&sp, nn_signal);
        uart_put_csv(nn_signal, SENT_SIGNAL_LEN);
        HAL_GPIO_TogglePin(LD2_GPIO_Port, LD2_Pin);
    }
}

/* ========================================================================= */
#else /* APP_MODE_SENTINEL */
/* ========================================================================= */

typedef enum { ST_DELAY, ST_LEARN, ST_WATCH } state_t;

static state_t     state;
static uint32_t    t_state;          /* entrée dans l'état courant */
static uint32_t    learned;          /* signaux appris pendant cette session */
static uint32_t    total_learned;    /* depuis le dernier "tout réapprendre" */
static scene_ref_t ref;
static alarm_t     alarm;
static uint8_t     threshold = SENT_THRESHOLD;
static uint32_t    events;
static uint32_t    alarm_since;
static uint8_t     alarm_min_sim;
static int         verbose;
static int         countdown_shown;
static uint32_t    b1_down_at;
static uint8_t     b1_prev = 1;
static int         trained;          /* 1 = librairie de classification : modèle figé */
static uint32_t    ref_left;         /* trames restantes pour l'image de référence */
static uint32_t    ref_at;           /* instant où la prise de l'image commence */

#ifndef SENT_REF_FRAMES
#define SENT_REF_FRAMES  15          /* image de référence : ~1 s à 15 Hz */
#endif

/* Nom de la valeur affichée : similarité (anomalie) ou probabilité de "normal" */
static const char *sim_word(void)
{
    return trained ? "proba normal" : "similarite";
}

static void ai_init_or_die(void)
{
    int rc = sent_ai_init();
    if (rc) {
        /* -2 : librairie de classification sans classe "normal" */
        fatal((rc == -2) ? "init NanoEdge (pas de classe \"normal\" dans la librairie)"
                         : "init NanoEdge", rc);
    }
}

static void put_time(void)
{
    uint32_t s = HAL_GetTick() / 1000u;
    uart_puts("[");
    uart_put_int((int32_t)(s / 60u));
    uart_puts(s % 60u < 10u ? ":0" : ":");
    uart_put_int((int32_t)(s % 60u));
    uart_puts("] ");
}

static void print_menu(void)
{
    uart_puts("\r\n=====================================\r\n");
    if (trained) {
        uart_puts("  SENTINELLE - modele appris dans le Studio\r\n");
        uart_puts("  (classification normal / anomalie)\r\n");
        uart_puts("=====================================\r\n");
        uart_puts("  [B1] ou [l] ou [a] : reprendre l'image de reference (dans 5 s)\r\n");
    } else {
        uart_puts("  SENTINELLE - apprentissage embarque\r\n");
        uart_puts("=====================================\r\n");
        uart_puts("  [B1 long] ou [l] : tout reapprendre\r\n");
        uart_puts("  [B1 court] ou [a] : ajouter la scene actuelle comme normale\r\n");
    }
    uart_puts("  [+] / [-] : seuil d'alarme (actuel : ");
    uart_put_int(threshold);
    uart_puts("%)\r\n");
    uart_puts("  [v] ");
    uart_puts(sim_word());
    uart_puts(" en continu   [c] carte des changements   [h] menu\r\n\r\n");
}

static void print_map(void)
{
    char map[TOF_ZONES];
    int n = scene_ref_diff(&ref, &frame, map);
    uart_puts("    Carte des changements (+ apparu/rapproche, - disparu/eloigne) : ");
    uart_put_int(n);
    uart_puts(" zone(s)\r\n");
    for (int y = 0; y < TOF_GRID; y++) {
        uart_puts("      ");
        for (int x = 0; x < TOF_GRID; x++) {
            char cell[3] = { map[y * TOF_GRID + x], ' ', '\0' };
            uart_puts(cell);
        }
        uart_puts("\r\n");
    }
}

static void print_bar(uint8_t sim)
{
    char bar[23];
    bar[0] = '[';
    for (int i = 0; i < 20; i++) {
        bar[1 + i] = (i < sim / 5) ? '#' : (i == threshold / 5 ? '|' : '-');
    }
    bar[21] = ']';
    bar[22] = '\0';
    uart_puts("  ");
    uart_puts(sim_word());
    uart_puts(" ");
    if (sim < 100) uart_puts(" ");
    if (sim < 10)  uart_puts(" ");
    uart_put_int(sim);
    uart_puts("% ");
    uart_puts(bar);
    uart_puts(alarm.active ? "  ALARME\r\n" : "\r\n");
}

/* full = 1 : tout effacer et réapprendre ; full = 0 : apprentissage incrémental */
static void start_learning(int full)
{
    if (full) {
        ai_init_or_die();
        scene_ref_reset(&ref);
        total_learned = 0;
        events = 0;
        uart_puts("\r\n");
        put_time();
        uart_puts("TOUT REAPPRENDRE : sors du champ du capteur...\r\n");
    } else {
        uart_puts("\r\n");
        put_time();
        uart_puts("AJOUT : la scene actuelle va etre apprise comme normale...\r\n");
    }
    alarm_init(&alarm, threshold, SENT_CONFIRM);
    state = ST_DELAY;
    t_state = HAL_GetTick();
    countdown_shown = -1;
}

/* Librairie de classification : nouvelle image de référence de la scène, prise
 * dans delay_ms ; la surveillance continue pendant ce temps. */
static void start_reference(uint32_t delay_ms)
{
    scene_ref_reset(&ref);
    ref_left = SENT_REF_FRAMES;
    ref_at = HAL_GetTick() + delay_ms;
    if (delay_ms) {
        uart_puts("\r\n");
        put_time();
        uart_puts("Image de reference dans ");
        uart_put_int((int32_t)(delay_ms / 1000u));
        uart_puts(" s : sors du champ du capteur...\r\n");
    }
}

static void handle_inputs(void)
{
    int c = uart_getc_nonblock();
    uint8_t b1 = (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET) ? 1 : 0;  /* actif bas */
    uint32_t now = HAL_GetTick();

    if (b1_prev && !b1) {
        b1_down_at = now;
    } else if (!b1_prev && b1) {
        if (trained) {
            start_reference(SENT_LEARN_DELAY_MS);
        } else {
            start_learning(now - b1_down_at >= SENT_LONG_PRESS_MS);
        }
    }
    b1_prev = b1;

    switch (c) {
    case 'l': case 'L': case 'a': case 'A':
        if (trained) {
            start_reference(SENT_LEARN_DELAY_MS);
        } else {
            start_learning(c == 'l' || c == 'L');
        }
        break;
    case '+':
        threshold = (uint8_t)((threshold <= 94) ? threshold + 5 : 99);
        alarm.threshold = threshold;
        uart_puts("  seuil = "); uart_put_int(threshold); uart_puts("%\r\n");
        break;
    case '-':
        threshold = (uint8_t)((threshold >= 10) ? threshold - 5 : 5);
        alarm.threshold = threshold;
        uart_puts("  seuil = "); uart_put_int(threshold); uart_puts("%\r\n");
        break;
    case 'v': case 'V':
        verbose = !verbose;
        break;
    case 'c': case 'C':
        print_map();
        break;
    case 'h': case 'H': case '?':
        print_menu();
        break;
    default:
        break;
    }
}

static void on_signal_watch(void)
{
    uint8_t sim = 0;
    if (sent_ai_detect(nn_signal, &sim)) {
        return;
    }
    if (verbose) {
        print_bar(sim);
    }
    if (alarm.active && sim < alarm_min_sim) {
        alarm_min_sim = sim;
    }

    switch (alarm_update(&alarm, sim)) {
    case ALARM_RAISED:
        events++;
        alarm_since = HAL_GetTick();
        alarm_min_sim = sim;
        uart_puts("\r\n");
        put_time();
        uart_puts("!!! ALARME #");
        uart_put_int((int32_t)events);
        uart_puts(" : la scene a change (");
        uart_puts(sim_word());
        uart_puts(" ");
        uart_put_int(sim);
        uart_puts("%)\r\n");
        print_map();
        if (trained) {
            uart_puts("    Si c'est normal desormais : recollecter et regenerer la librairie.\r\n");
        } else {
            uart_puts("    [B1 court] si c'est normal desormais.\r\n");
        }
        break;
    case ALARM_CLEARED:
        put_time();
        uart_puts("Retour a la normale apres ");
        uart_put_int((int32_t)((HAL_GetTick() - alarm_since) / 1000u));
        uart_puts(" s (");
        uart_puts(sim_word());
        uart_puts(" mini ");
        uart_put_int(alarm_min_sim);
        uart_puts("%)\r\n");
        break;
    default:
        break;
    }
}

void app_init(void)
{
    uart_io_init();
    uart_puts("\r\n# Sentinelle : init capteur (1-2 s)...\r\n");
    int err = tof_sensor_init();
    if (err) {
        fatal("init VL53L5CX", err);
    }
    scene_preproc_init(&sp);
    trained = sent_ai_is_trained();
    print_menu();
    if (trained) {
        ai_init_or_die();
        alarm_init(&alarm, threshold, SENT_CONFIRM);
        start_reference(0);
        state = ST_WATCH;
        put_time();
        uart_puts("SURVEILLANCE ACTIVE (modele NanoEdge appris dans le Studio, rien a apprendre).\r\n");
    } else {
        start_learning(1);
    }
}

void app_loop(void)
{
    uint32_t now = HAL_GetTick();

    handle_inputs();

    int have_frame = tof_sensor_poll(&frame);
    int have_signal = 0;
    if (have_frame && scene_preproc_push(&sp, &frame)) {
        scene_preproc_take(&sp, nn_signal);
        have_signal = 1;
    }

    switch (state) {
    case ST_DELAY: {
        int left = (int)((SENT_LEARN_DELAY_MS - (now - t_state) + 999u) / 1000u);
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, ((now / 500u) & 1u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        if (now - t_state >= SENT_LEARN_DELAY_MS) {
            uart_puts("\r\n");
            put_time();
            uart_puts("Apprentissage en cours, ne bouge rien...\r\n");
            HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
            scene_preproc_init(&sp);     /* ne pas apprendre une fenêtre commencée avant */
            learned = 0;
            state = ST_LEARN;
        } else if (left != countdown_shown) {
            countdown_shown = left;
            uart_puts("  ");
            uart_put_int(left);
            uart_puts("...");
        }
        break;
    }

    case ST_LEARN:
        if (have_frame) {
            scene_ref_add(&ref, &frame);
        }
        if (have_signal) {
            sent_ai_learn(nn_signal);
            learned++;
            total_learned++;
            if (learned % 10u == 0u) {
                uart_puts("  appris : ");
                uart_put_int((int32_t)learned);
                uart_puts("/");
                uart_put_int(SENT_LEARN_SIGNALS);
                uart_puts("\r\n");
            }
            if (learned >= SENT_LEARN_SIGNALS) {
                put_time();
                uart_puts("Apprentissage termine (");
                uart_put_int((int32_t)total_learned);
                uart_puts(" signaux au total). SURVEILLANCE ACTIVE.\r\n");
                uint32_t min = sent_ai_min_learn();
                if (min && total_learned < min) {
                    uart_puts("  Attention : NanoEdge conseille au moins ");
                    uart_put_int((int32_t)min);
                    uart_puts(" apprentissages ([a] pour completer).\r\n");
                }
                HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
                state = ST_WATCH;
            }
        }
        break;

    case ST_WATCH:
        if (trained && ref_left && have_frame && (int32_t)(now - ref_at) >= 0) {
            scene_ref_add(&ref, &frame);
            if (--ref_left == 0u) {
                put_time();
                uart_puts("Image de reference de la scene prise ([c] pour la carte des changements).\r\n");
            }
        }
        if (have_signal) {
            on_signal_watch();
        }
        HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin,
                          (alarm.active && ((now / 100u) & 1u)) ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;
    }
}

#endif /* APP_MODE */
