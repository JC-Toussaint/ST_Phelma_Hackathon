/**
 * app.c - Baguette magique : reconnaître des gestes dans l'air avec NanoEdge AI
 *
 * APP_MODE_DATALOG : chaque geste détecté est envoyé en CSV (une ligne de WAND_SIGNAL_LEN entiers)
 *                    pour la collecte (outils/collecte_nanoedge.py) ; « # En attente… » sinon.
 * APP_MODE_WAND    : chaque geste est classé par la librairie NanoEdge ; une ligne
 *                    « GESTE <nom> <proba>  (probabilité de chaque classe) » est affichée.
 *
 * LED LD2 : collecte = change d'état à chaque geste envoyé ;
 *           reconnaissance = allumée quand la baguette est prête, clignote 3 fois sur un geste reconnu.
 * Terminal série : port COM virtuel du ST-LINK, 115200 bauds, 8N1.
 */
#include "app.h"
#include <string.h>
#include "main.h"
#include "app_config.h"
#include "stm32l4xx_nucleo.h"
#include "imu_sensor.h"
#include "wand_preproc.h"
#include "uart_io.h"
#if APP_MODE == APP_MODE_WAND
#include "wand_ai.h"
#endif

static wand_pre_t   pre;
static imu_sample_t sample;
static uint16_t     sig[WAND_SIGNAL_LEN];
static uint32_t     read_errors;

/* Erreur bloquante : LED qui clignote vite et message répété toutes les 2 s. */
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
        BSP_LED_Toggle(LED2);
        HAL_Delay(100);
    }
}

static void common_init(const char *mode)
{
    BSP_LED_Init(LED2);
    BSP_COM_Init(COM1);                 /* USART2, 115200 bauds */
    uart_puts("\r\n# Baguette magique : mode ");
    uart_puts(mode);
    uart_puts("\r\n");
    int err = imu_init();
    if (err) {
        fatal("init LSM6DSL (carte X-NUCLEO-IKS01A2 bien enfichee ?)", err);
    }
    wand_pre_init(&pre);
}

/* Lit le capteur ; retourne 1 quand un geste complet est disponible dans sig[]. */
static int next_gesture(void)
{
    int r = imu_read(&sample);
    if (r < 0) {
        if (++read_errors == 100u) {
            uart_puts("# ERREUR : lectures du LSM6DSL en echec (bus I2C ?)\r\n");
        }
        return 0;
    }
    if (r == 0) {
        return 0;
    }
    if (wand_pre_push(&pre, &sample)) {
        wand_pre_take(&pre, sig);
        return 1;
    }
    return 0;
}

/* ========================================================================= */
#if APP_MODE == APP_MODE_DATALOG
/* ========================================================================= */

static uint32_t last_msg;

void app_init(void)
{
    common_init("COLLECTE");
    uart_puts("# Pret. Une ligne = ");
    uart_put_int(WAND_SAMPLES);
    uart_puts(" echantillons x 6 axes (ax,ay,az,gx,gy,gz), valeurs 0..");
    uart_put_int(WAND_VALUE_MAX);
    uart_puts(" (");
    uart_put_int(WAND_VALUE_MAX / 2);
    uart_puts(" = zero)\r\n");
}

void app_loop(void)
{
    if (next_gesture()) {
        uart_put_csv_u16(sig, WAND_SIGNAL_LEN);
        BSP_LED_Toggle(LED2);
        last_msg = HAL_GetTick();
        return;
    }
    /* Message d'attente une fois par seconde (l'outil de collecte remplace la ligne au lieu d'empiler). */
    uint32_t now = HAL_GetTick();
    if (now - last_msg >= 1000u) {
        last_msg = now;
        if (wand_pre_state(&pre) == WAND_ARMING) {
            uart_puts("# En attente : immobilise la baguette un instant\r\n");
        } else if (wand_pre_state(&pre) == WAND_ARMED) {
            uart_puts("# En attente d'un geste (baguette prete)\r\n");
        }
    }
}

/* ========================================================================= */
#else /* APP_MODE_WAND */
/* ========================================================================= */

static uint32_t blink_until;

static void print_menu(void)
{
    uart_puts("\r\n=====================================\r\n");
    uart_puts("  BAGUETTE MAGIQUE - reconnaissance\r\n");
    uart_puts("=====================================\r\n");
    uart_puts("  Classes de la librairie : ");
    for (int k = 0; k < wand_ai_class_count(); k++) {
        uart_puts(k ? ", " : "");
        uart_puts(wand_ai_class_name(k));
    }
    uart_puts("\r\n  Baguette immobile (LED allumee = prete), puis un geste franc.\r\n");
    uart_puts("  [h] menu\r\n\r\n");
}

static void print_result(int best, float conf)
{
    const char *name = wand_ai_class_name(best);
    uart_puts("GESTE ");
    if (strcmp(name, WAND_REJECT_CLASS) == 0) {
        uart_puts("- (autre mouvement)");
    } else if (conf < WAND_MIN_CONFIDENCE) {
        uart_puts("? pas reconnu (");
        uart_puts(name);
        uart_puts(" ");
        uart_put_pct(conf);
        uart_puts(")");
    } else {
        uart_puts(name);
        uart_puts(" ");
        uart_put_pct(conf);
        blink_until = HAL_GetTick() + 600u;
    }
    uart_puts("   (");
    for (int k = 0; k < wand_ai_class_count(); k++) {
        uart_puts(k ? " " : "");
        uart_puts(wand_ai_class_name(k));
        uart_puts(" ");
        uart_put_pct(wand_ai_class_prob(k));
    }
    uart_puts(")\r\n");
}

void app_init(void)
{
    common_init("RECONNAISSANCE");
    int rc = wand_ai_init();
    if (rc) {
        fatal("init NanoEdge (librairie copiee dans Core/Inc et Core/Src ?)", rc);
    }
    print_menu();
}

void app_loop(void)
{
    int c = uart_getc_nonblock();
    if (c == 'h' || c == 'H' || c == '?') {
        print_menu();
    }

    if (next_gesture()) {
        float conf = 0.0f;
        int best = wand_ai_classify(sig, &conf);
        if (best < 0) {
            uart_puts("# ERREUR : classification NanoEdge\r\n");
        } else {
            print_result(best, conf);
        }
    }

    /* LED : clignote après un geste reconnu, sinon allumée quand la baguette est prête. */
    uint32_t now = HAL_GetTick();
    if ((int32_t)(blink_until - now) > 0) {
        if (((blink_until - now) / 100u) & 1u) BSP_LED_On(LED2); else BSP_LED_Off(LED2);
    } else if (wand_pre_state(&pre) == WAND_ARMED) {
        BSP_LED_On(LED2);
    } else {
        BSP_LED_Off(LED2);
    }
}

#endif /* APP_MODE */
