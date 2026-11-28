/**
 * uart_io.c - Sorties texte sur USART2 (voir uart_io.h)
 */
#include "uart_io.h"
#include "app_config.h"
#include "stm32l4xx_nucleo.h"   /* hcom_uart[COM1] : USART2 du BSP de ST */
#include <string.h>

#if APP_UART_BAUD != 115200
#error "USART2 reste a 115200 bauds (initialise par BSP_COM_Init) : APP_UART_BAUD doit valoir 115200"
#endif

#define UART  (&hcom_uart[COM1])

void uart_puts(const char *s)
{
    /* Une ligne de 1536 valeurs fait jusqu'à ~7,7 Ko, soit ~0,7 s à 115200 bauds. */
    HAL_UART_Transmit(UART, (uint8_t *)s, (uint16_t)strlen(s), 3000);
}

void uart_put_int(int32_t v)
{
    char buf[12];
    int i = sizeof(buf) - 1;
    uint32_t u = (v < 0) ? (uint32_t)(-v) : (uint32_t)v;
    buf[i] = '\0';
    do {
        buf[--i] = (char)('0' + (u % 10u));
        u /= 10u;
    } while (u && i > 1);
    if (v < 0) {
        buf[--i] = '-';
    }
    uart_puts(&buf[i]);
}

void uart_put_pct(float r)
{
    if (r < 0.0f) r = 0.0f;
    if (r > 1.0f) r = 1.0f;
    uart_put_int((int32_t)(r * 100.0f + 0.5f));
    uart_puts("%");
}

int uart_getc_nonblock(void)
{
    uint8_t c;
    if (__HAL_UART_GET_FLAG(UART, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(UART);
    }
    if (__HAL_UART_GET_FLAG(UART, UART_FLAG_RXNE)) {
        if (HAL_UART_Receive(UART, &c, 1, 0) == HAL_OK) {
            return c;
        }
    }
    return -1;
}

void uart_put_csv_u16(const uint16_t *values, int n)
{
    static char line[WAND_SIGNAL_LEN * 6 + 4];
    char *p = line;
    if (n > WAND_SIGNAL_LEN) {
        n = WAND_SIGNAL_LEN;
    }
    for (int i = 0; i < n; i++) {
        char tmp[6];
        int k = 0;
        unsigned v = values[i];
        do {
            tmp[k++] = (char)('0' + v % 10u);
            v /= 10u;
        } while (v);
        while (k) {
            *p++ = tmp[--k];
        }
        *p++ = (i == n - 1) ? '\r' : ',';
    }
    *p++ = '\n';
    *p = '\0';
    uart_puts(line);
}
