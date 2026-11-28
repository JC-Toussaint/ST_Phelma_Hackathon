/**
 * app.c - La table qui écoute : reconnaître la zone tapée sur une table avec NanoEdge AI
 *
 * APP_MODE_DATALOG : chaque tape détectée est envoyée en CSV (une ligne de TAP_SIGNAL_LEN entiers)
 *                    pour la collecte (outils/collecte_nanoedge.py) ; « # En attente… » sinon, avec la
 *                    cadence réellement mesurée (échantillons par seconde).
 * APP_MODE_TABLE   : chaque tape est classée par la librairie NanoEdge ; une ligne
 *                    « TAPE <zone> <proba>  (probabilité de chaque classe) » est affichée.
 *
 * Le capteur est lu par paquets dans sa FIFO (imu_sensor.c). Après l'envoi d'une ligne (~0,3 s), la FIFO
 * est vidée sans être traitée : aucune tape ne compte pendant l'envoi, et la suivante reste bien alignée.
 *
 * LED LD2 : collecte = change d'état à chaque tape envoyée ;
 *           reconnaissance = allumée quand la table est prête, clignote sur une zone reconnue.
 * Terminal série : port COM virtuel du ST-LINK, 115200 bauds, 8N1.
 */
#include "app.h"
#include <string.h>
#include "main.h"
#include "app_config.h"
#include "stm32l4xx_nucleo.h"
#include "imu_sensor.h"
#include "tap_preproc.h"
#include "uart_io.h"
#if APP_MODE == APP_MODE_TABLE
#include "tap_ai.h"
#endif

static tap_pre_t    pre;
static acc_sample_t batch[IMU_FIFO_BATCH];
static uint16_t     sig[TAP_SIGNAL_LEN];
static uint32_t     read_errors;
static uint32_t     rate_count, rate_t0, rate_hz;   /* cadence mesurée (échantillons par seconde) */

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
    uart_puts("\r\n# La table qui ecoute : mode ");
    uart_puts(mode);
    uart_puts("\r\n");
    int err = imu_init();
    if (err) {
        fatal("init " IMU_NAME " (carte " IMU_BOARD " bien enfichee ?)", err);
    }
    tap_pre_init(&pre);
    rate_t0 = HAL_GetTick();
}

/* Après un long envoi : jeter ce que la FIFO a accumulé, et repartir en attente de calme. */
static void resync(void)
{
    imu_flush();
    imu_overrun();
    tap_pre_rearm(&pre);
}

/* Vide la FIFO ; retourne 1 quand une tape complète est disponible dans sig[]. */
static int next_tap(void)
{
    int n = imu_read_batch(batch, IMU_FIFO_BATCH);
    if (n < 0) {
        if (++read_errors == 100u) {
            uart_puts("# ERREUR : lectures du " IMU_NAME " en echec (bus I2C ?)\r\n");
        }
        return 0;
    }
    if (imu_overrun()) {
        /* Échantillons perdus : une tape en cours serait déformée, on l'abandonne. */
        if (tap_pre_state(&pre) == TAP_RECORDING) {
            tap_pre_rearm(&pre);
        }
    }
    rate_count += (uint32_t)n;
    for (int i = 0; i < n; i++) {
        if (tap_pre_push(&pre, &batch[i])) {
            tap_pre_take(&pre, sig);
            return 1;                    /* le reste du paquet est jeté par resync() après l'envoi */
        }
    }
    return 0;
}

/* Cadence mesurée, mise à jour une fois par seconde (à comparer à IMU_ODR_HZ). */
static void update_rate(void)
{
    uint32_t now = HAL_GetTick();
    if (now - rate_t0 >= 1000u) {
        rate_hz = rate_count * 1000u / (now - rate_t0);
        rate_count = 0;
        rate_t0 = now;
    }
}

/* ========================================================================= */
#if APP_MODE == APP_MODE_DATALOG
/* ========================================================================= */

static uint32_t last_msg;

void app_init(void)
{
    common_init("COLLECTE");
    uart_puts("# Pret. Une ligne = ");
    uart_put_int(TAP_SAMPLES);
    uart_puts(" echantillons x 3 axes (ax,ay,az) a ");
    uart_put_int(IMU_ODR_HZ);
    uart_puts(" Hz, repos retire, valeurs 0..");
    uart_put_int(TAP_VALUE_MAX);
    uart_puts(" (");
    uart_put_int(TAP_VALUE_MAX / 2);
    uart_puts(" = zero)\r\n");
}

void app_loop(void)
{
    if (next_tap()) {
        uart_put_csv_u16(sig, TAP_SIGNAL_LEN);
        BSP_LED_Toggle(LED2);
        resync();
        last_msg = HAL_GetTick();
        rate_t0 = last_msg;              /* l'envoi fausserait la mesure de cadence */
        rate_count = 0;
        return;
    }
    update_rate();
    /* Message d'attente une fois par seconde (l'outil de collecte remplace la ligne au lieu d'empiler). */
    uint32_t now = HAL_GetTick();
    if (now - last_msg >= 1000u) {
        last_msg = now;
        if (tap_pre_state(&pre) == TAP_ARMING) {
            uart_puts("# En attente : table encore agitee (");
        } else {
            uart_puts("# En attente d'une tape (");
        }
        uart_put_int((int32_t)rate_hz);
        uart_puts(" ech/s)\r\n");
    }
}

/* ========================================================================= */
#else /* APP_MODE_TABLE */
/* ========================================================================= */

static uint32_t blink_until;

static void print_menu(void)
{
    uart_puts("\r\n=====================================\r\n");
    uart_puts("  LA TABLE QUI ECOUTE - reconnaissance\r\n");
    uart_puts("=====================================\r\n");
    uart_puts("  Classes de la librairie : ");
    for (int k = 0; k < tap_ai_class_count(); k++) {
        uart_puts(k ? ", " : "");
        uart_puts(tap_ai_class_name(k));
    }
    uart_puts("\r\n  Table au calme (LED allumee = prete), puis une tape franche sur une zone.\r\n");
    uart_puts("  [h] menu   [r] cadence mesuree\r\n\r\n");
}

static void print_result(int best, float conf)
{
    const char *name = tap_ai_class_name(best);
    uart_puts("TAPE ");
    if (strcmp(name, TAP_REJECT_CLASS) == 0) {
        uart_puts("- (autre chose)");
    } else if (conf < TAP_MIN_CONFIDENCE) {
        uart_puts("? pas reconnue (");
        uart_puts(name);
        uart_puts(" ");
        uart_put_pct(conf);
        uart_puts(")");
    } else {
        uart_puts(name);
        uart_puts(" ");
        uart_put_pct(conf);
        blink_until = HAL_GetTick() + 400u;
    }
    uart_puts("   (");
    for (int k = 0; k < tap_ai_class_count(); k++) {
        uart_puts(k ? " " : "");
        uart_puts(tap_ai_class_name(k));
        uart_puts(" ");
        uart_put_pct(tap_ai_class_prob(k));
    }
    uart_puts(")\r\n");
}

void app_init(void)
{
    common_init("RECONNAISSANCE");
    int rc = tap_ai_init();
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
    } else if (c == 'r' || c == 'R') {
        uart_puts("# Cadence mesuree : ");
        uart_put_int((int32_t)rate_hz);
        uart_puts(" ech/s (attendu ");
        uart_put_int(IMU_ODR_HZ);
        uart_puts(")\r\n");
    }

    if (next_tap()) {
        float conf = 0.0f;
        int best = tap_ai_classify(sig, &conf);
        if (best < 0) {
            uart_puts("# ERREUR : classification NanoEdge\r\n");
        } else {
            print_result(best, conf);
        }
        resync();
        rate_t0 = HAL_GetTick();
        rate_count = 0;
    }
    update_rate();

    /* LED : clignote après une zone reconnue, sinon allumée quand la table est prête. */
    uint32_t now = HAL_GetTick();
    if ((int32_t)(blink_until - now) > 0) {
        if (((blink_until - now) / 100u) & 1u) BSP_LED_On(LED2); else BSP_LED_Off(LED2);
    } else if (tap_pre_state(&pre) == TAP_ARMED) {
        BSP_LED_On(LED2);
    } else {
        BSP_LED_Off(LED2);
    }
}

#endif /* APP_MODE */
