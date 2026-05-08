#include "uart.h"

void kernel_main(void) {
    uart_init();

    uart_puts("\n\n");
    uart_puts("================================\n");
    uart_puts("  VitOS v0.1 -- Phase 1\n");
    uart_puts("  Raspberry Pi 4 Bare Metal\n");
    uart_puts("================================\n");
    uart_puts("\n");
    uart_puts("[boot] UART initialized\n");
    uart_puts("[boot] Running on core 0 (EL2)\n");
    uart_puts("[boot] Hello from VitOS!\n");
    uart_puts("\n");
    uart_puts("UART echo mode (type something):\n");
    uart_puts("> ");

    while (1) {
        char c = uart_getc();
        if (c == '\r' || c == '\n') {
            uart_puts("\n> ");
        } else {
            uart_send(c);
        }
    }
}
