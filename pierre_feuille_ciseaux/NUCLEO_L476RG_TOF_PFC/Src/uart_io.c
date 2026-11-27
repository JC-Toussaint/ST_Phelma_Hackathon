/**
 * uart_io.c - Sorties texte sur USART2 (voir uart_io.h)
 */
#include "uart_io.h"
#include "main.h"
#include "app_config.h"
#include <string.h>

extern UART_HandleTypeDef huart2;

#if APP_UART_BAUD != 115200
#error "USART2 reste a 115200 bauds (initialise par main.c du projet ST) : APP_UART_BAUD doit valoir 115200"
#endif

/* USART2 est initialisé à 115200 bauds par main.c (projet ST) : rien à reconfigurer. */
void uart_io_init(void)
{
}

void uart_puts(const char *s)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 1000);
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

void uart_put_hex32(uint32_t v)
{
    static const char hex[] = "0123456789abcdef";
    char buf[9];
    for (int i = 0; i < 8; i++) {
        buf[i] = hex[(v >> (28 - 4 * i)) & 0xFu];
    }
    buf[8] = '\0';
    uart_puts(buf);
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
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_ORE)) {
        __HAL_UART_CLEAR_OREFLAG(&huart2);
    }
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        if (HAL_UART_Receive(&huart2, &c, 1, 0) == HAL_OK) {
            return c;
        }
    }
    return -1;
}

void uart_put_csv(const float *values, int n)
{
    static char line[512 * 4 + 4];
    char *p = line;
    if (n > 512) {
        n = 512;
    }
    for (int i = 0; i < n; i++) {
        int v = (int)(values[i] + 0.5f);
        if (v < 0)   v = 0;
        if (v > 100) v = 100;
        if (v == 100)      { *p++ = '1'; *p++ = '0'; *p++ = '0'; }
        else if (v >= 10)  { *p++ = (char)('0' + v / 10); *p++ = (char)('0' + v % 10); }
        else               { *p++ = (char)('0' + v); }
        *p++ = (i == n - 1) ? '\r' : ',';
    }
    *p++ = '\n';
    *p = '\0';
    uart_puts(line);
}
