/*
 * turnled.c
 */

#include <stdint.h>
#include <avr/io.h>
#include "turnled.h"

#define MAX_COUNT (NUM_TURNLEDS+8)

static uint8_t ledstate;
static uint8_t count;

static const struct
{
  volatile uint8_t * port_ptr;
  volatile uint8_t * ddr_ptr;
  uint8_t mask;
} TurnLeds[NUM_TURNLEDS] =
{
  { &PORTB, &DDRB, 1<<PB6 },
  { &PORTB, &DDRB, 1<<PB7 },
  { &PORTD, &DDRD, 1<<PD5 },
  { &PORTD, &DDRD, 1<<PD6 }
};

void init_turnled(void)
{
  uint8_t id;
  for (id = 0; id < NUM_TURNLEDS; id++)
  {
    *TurnLeds[id].ddr_ptr |= TurnLeds[id].mask;
    *TurnLeds[id].port_ptr &= ~TurnLeds[id].mask;
  }
}

void process_turnled(void)
{
  if (count < NUM_TURNLEDS)
  {
    /* Turn off the the previous turnLED */
    *TurnLeds[count].port_ptr &= ~TurnLeds[count].mask;
  }

  /* Go to the next step */
  count++;
  if (count > MAX_COUNT)
  {
    count = 0;
  }

  if ((count < NUM_TURNLEDS) && ((ledstate & (1<<count)) != 0))
  {
    /* Turn on the next turnLED */
    *TurnLeds[count].port_ptr |= TurnLeds[count].mask;
  }
}

void turnled_on(uint8_t id)
{
  ledstate |= 1<<id;
}

void turnled_off(uint8_t id)
{
  ledstate &= ~(1<<id);
}
