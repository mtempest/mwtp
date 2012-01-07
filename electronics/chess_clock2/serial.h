/*
 * serial.h
 */

#define FRAMING_ERROR  1
#define PARITY_ERROR   2
#define UART_FIFO_FULL 4
#define SW_FIFO_FULL   8

void init_serial(void);

void poll_serial(void);

void serial_puts(const char * s);

void serial_putc(char c);

uint8_t serial_getc(void);

uint8_t read_rx_errors(void);

