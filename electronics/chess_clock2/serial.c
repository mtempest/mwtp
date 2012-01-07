/*
 * serial.c
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/atomic.h>
#include "serial.h"

#define BUFSIZE 64

static char rx_buffer[BUFSIZE];
static char tx_buffer[BUFSIZE];

static uint8_t rx_errors;
static uint8_t rx_in;
static uint8_t rx_out;
static uint8_t tx_in;
static uint8_t tx_out;

/* Not for 2x mode */
#define NORMAL_ASYNC_BRR_FOR_BAUD(baud) ((F_CPU / (16L * (baud))) - 1)

void init_serial(void)
{
  UCSR0B = (0<<RXCIE0)  /* disable receive interrupt */
         | (0<<TXCIE0)  /* disable transmit complete interrupt */
         | (0<<UDRIE0)  /* disable transmit data register empty interrupt */
         | (1<<TXEN0)   /* enable transmitter */
         | (1<<RXEN0)   /* enable receiver */
         | (0<<UCSZ02)  /* with UCSZ00 and UCSZ01 below, set the character size to 7 bits */
         | (0<<RXB80)   /* Don't care about the 9th RX bit */
         | (0<<TXB80);  /* Don't care about the 9th TX bit */
  UCSR0C = (0<<UMSEL01) | (0<<UMSEL00) /* asynchronous mode */
         | (1<<UPM01) | (1<<UPM00)     /* odd parity */
         | (0<<USBS0)                  /* 1 stop bit */
         | (1<<UCSZ01) | (0<<UCSZ00)   /* see above */
         | (0<<UCPOL0);                /* Must be set to zero in asynchronous mode */
  UBRR0 = NORMAL_ASYNC_BRR_FOR_BAUD(2400);
  DDRD &= 1<<PD0;
  DDRD |= 1<<PD1;
  UDR0 = 0xAA;
  while ( !( UCSR0A & (1<<UDRE0)) ) ;
  UDR0 = 0x55;
}

void poll_serial(void)
{
  if ((tx_in != tx_out) && ((UCSR0A & (1<<UDRE0)) != 0))
  {
    UDR0 = tx_buffer[tx_out];
    tx_out = (tx_out + 1) % BUFSIZE;
  }
  if (((UCSR0A & (1<<RXC0)) != 0) && ((UCSR0A & (1<<FE0 | 1<<UPE0)) == 0))
  {
    uint8_t next_rx_in;
    next_rx_in = (rx_in + 1) % BUFSIZE;
    if (next_rx_in != rx_out)
    {
      rx_buffer[rx_in] = UDR0;
      rx_in = next_rx_in;
    }
  }
}

ISR(USART_RX_vect)
{
  uint8_t status;
  while (((status = UCSR0A) & (1<<RXC0)) != 0)
  {
    char data = UDR0;
    if ((status & (1<<DOR0)) != 0)
    {
      rx_errors |= UART_FIFO_FULL;
    }
    if ((status & (1<<FE0)) != 0)
    {
      rx_errors |= FRAMING_ERROR;
    }
    else if ((status & (1<<UPE0)) != 0)
    {
      rx_errors |= PARITY_ERROR;
    }
    else
    {
      uint8_t next_rx_in;
      next_rx_in = (rx_in + 1) % BUFSIZE;
      if (next_rx_in != rx_out)
      {
        rx_buffer[rx_in] = data;
        rx_in = next_rx_in;
      }
      else
      {
        rx_errors |= SW_FIFO_FULL;
      }
    }
  } /* while receiving characters */
}

uint8_t read_rx_errors(void)
{
  uint8_t errors;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    errors = rx_errors;
    rx_errors = 0;
  }
  return errors;
}

uint8_t serial_getc(void)
{
  uint8_t c;
  if (rx_in == rx_out)
  {
    c = 0;
  }
  else
  {
    c = rx_buffer[rx_out];
    rx_out++;
  }
  return c;
}

void serial_puts(const char * s)
{
  while (*s != 0)
  {
    serial_putc(*s);
    s++;
  }
}

void serial_putc(char c)
{
  uint8_t next_tx_in;
  next_tx_in = (tx_in+1)%BUFSIZE;
  if (next_tx_in != tx_out)
  {
    tx_buffer[tx_in] = c;
    tx_in = next_tx_in;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
      //UCSR0B |= 1<<UDRIE0;
    }
  }
}

ISR(USART_UDRE_vect)
{
  if (tx_in != tx_out)
  {
    UDR0 = tx_buffer[tx_out];
    tx_out++;
  }
  else
  {
    UCSR0B &= 1<<UDRIE0;
  }
}

