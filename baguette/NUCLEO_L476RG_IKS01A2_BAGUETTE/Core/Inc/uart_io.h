/**
 * uart_io.h - Sorties texte légères sur USART2 (port COM virtuel du ST-LINK)
 *
 * USART2 est initialisé à 115200 bauds par le BSP de ST (BSP_COM_Init(COM1)).
 * Pas de printf avec des float (pas besoin de -u _printf_float).
 */
#ifndef UART_IO_H
#define UART_IO_H

#include <stdint.h>

void uart_puts(const char *s);
void uart_put_int(int32_t v);
void uart_put_pct(float ratio01);                      /* affiche "87%" */
int  uart_getc_nonblock(void);                         /* -1 si rien reçu */
/* Envoie une ligne CSV "v0,v1,...\r\n" d'entiers 0..65535 ; n <= WAND_SIGNAL_LEN */
void uart_put_csv_u16(const uint16_t *values, int n);

#endif /* UART_IO_H */
