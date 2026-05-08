#ifndef UART_H
#define UART_H

void uart_init(void);
void uart_send(unsigned int c);
char uart_getc(void);
void uart_puts(const char *s);
void uart_hex(unsigned int val);

#endif
