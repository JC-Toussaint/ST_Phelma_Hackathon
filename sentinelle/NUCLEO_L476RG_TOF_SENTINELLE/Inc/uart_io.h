/**
 * uart_io.h - Sorties texte légères sur USART2 (port COM virtuel du ST-LINK)
 * Évite printf/float (pas besoin de -u _printf_float).
 */
#ifndef UART_IO_H
#define UART_IO_H

#include <stdint.h>

/* Reconfigure USART2 (initialisé à 115200 par main.c) si APP_UART_BAUD diffère. */
void uart_io_init(void);
void uart_puts(const char *s);
void uart_put_int(int32_t v);
void uart_put_hex32(uint32_t v);
void uart_put_pct(float ratio01);   /* affiche "87%" */
int  uart_getc_nonblock(void);      /* -1 si rien reçu */
/* Envoie une ligne CSV "v0,v1,...\r\n" d'entiers 0..100 (valeurs arrondies, écrêtées). n <= 512 */
void uart_put_csv(const float *values, int n);

#endif /* UART_IO_H */
