/*
 * input.c
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>
#include "input.h"

/* Inputs table:
   EOT1     PD1  PCINT17
   EOT2     PD2  PCINT18
   UP       PD4  PCINT20
   EOT3     PD7  PCINT23
   EOT4     PB0  PCINT0
   DOWN     PB2  PCINT2
   RESTART  PB3  PCINT3
   PAUSE    PB4  PCINT4
   COPY     PB5  PCINT5
*/

#define D_MASK_EOT1    (1<<PD1)
#define D_MASK_EOT2    (1<<PD2)
#define D_MASK_UP      (1<<PD4)
#define D_MASK_EOT3    (1<<PD7)

#define B_MASK_EOT4    (1<<PB0)
#define B_MASK_DOWN    (1<<PB2)
#define B_MASK_RESTART (1<<PB3)
#define B_MASK_PAUSE   (1<<PB4)
#define B_MASK_COPY    (1<<PB5)

#define B_INVERTED (B_MASK_RESTART | B_MASK_PAUSE | B_MASK_COPY)
#define D_INVERTED (0)

#define B_MASK (B_MASK_EOT4 | B_MASK_DOWN | B_MASK_RESTART | B_MASK_PAUSE | B_MASK_COPY)
#define D_MASK (D_MASK_EOT1 | D_MASK_EOT2 | D_MASK_UP | D_MASK_EOT3)

#define SECOND_CONTROL_TIMEOUT_CYCLES 80

static uint8_t LastB = B_MASK_EOT4;
static uint8_t LastD = D_MASK_EOT3;

#define LONG_PUSH_CYCLES       10
#define REPEAT_HOLDOFF_CYCLES  6
#define REPEAT_INTERVAL_CYCLES 3
#define MAX_COUNTER            255
static uint8_t UpCounter;
static uint8_t DownCounter;
static uint8_t CopyCounter;
static uint8_t PauseCounter;

static uint8_t SecondControlNotFittedCount = SECOND_CONTROL_TIMEOUT_CYCLES;

void init_inputs(void)
{
  /* Enable pin-change interrupts 0 and 2,
     because some of PCINT0-7 and PCINT16-23 correspond to input pins */
  PCICR = (1<<PCIE0) | (1<<PCIE2);

  /* Set the pin-change interrupt masks according to the pins used as inputs */
  PCMSK2 = (1<<PCINT23) | (1<<PCINT20) | (1<<PCINT18) | (1<<PCINT17);
  PCMSK1 = 0;
  PCMSK0 = (1<<PCINT5) | (1<<PCINT4) | (1<<PCINT3) | (1<<PCINT2) | (1<<PCINT0);

  /* Set the input pins as inputs and enable the pull-up resistors */
  DDRB &= ~B_MASK;
  PORTB |= B_MASK;
  DDRD &= ~D_MASK;
  PORTD |= D_MASK;

}

static void held_input(uint8_t * counter_ptr, uint8_t input)
{
  if (*counter_ptr < 255)
  {
    (*counter_ptr)++;
    if (*counter_ptr == LONG_PUSH_CYCLES)
    {
      input_long_push(input);
    }
    if ((*counter_ptr > REPEAT_HOLDOFF_CYCLES) &&
        (((*counter_ptr - REPEAT_HOLDOFF_CYCLES) % REPEAT_INTERVAL_CYCLES) == 0))
    {
      input_repeat(input);
    }
  }
}

void process_inputs(void)
{
  if (((LastB & B_MASK_EOT4) == 0) || ((LastD & D_MASK_EOT3) == 0))
  {
    /* If either of EOT3 or EOT4 is "not pressed", then the control is fitted. */
    SecondControlNotFittedCount = 0;
  }
  else {
    /* Both EOT3 and EOT4 are "pressed". 
       That is a valid input condition, 
       but it could also mean that the control is not fitted.
       The control is "not fitted" if this condition lasts long enough */
    if (SecondControlNotFittedCount < SECOND_CONTROL_TIMEOUT_CYCLES)
    {
      SecondControlNotFittedCount++;
    }
  }

  if (LastD & D_MASK_UP)
  {
    held_input(&UpCounter, INPUT_UP);
  }
  if (LastB & B_MASK_DOWN)
  {
    held_input(&DownCounter, INPUT_DOWN);
  }
  if (LastB & B_MASK_COPY)
  {
    held_input(&CopyCounter, INPUT_COPY);
  }
  if (LastB & B_MASK_PAUSE)
  {
    held_input(&PauseCounter, INPUT_PAUSE);
  }

}

void poll_inputs(void)
{
  uint8_t pb;
  uint8_t pd;

  /* Read the input ports */
  pb = (PINB ^ B_INVERTED) & B_MASK;
  pd = (PIND ^ D_INVERTED) & D_MASK;

  /* debounce any inputs */
  {
    uint8_t i;
    i = 20;
    while ((i>0) && ((pb != 0) || (pd != 0)))
    {
      _delay_ms(1);
      pb &= (PINB ^ B_INVERTED) & B_MASK;
      pd &= (PIND ^ D_INVERTED) & D_MASK;
      i--;
    }
  }

  /* Process inputs */
  if ((pb != 0) || (pd != 0))
  {
    uint8_t newb;
    uint8_t newd;

    /* Identify which inputs have just been asserted */
    newb = pb & ~LastB;
    newd = pd & ~LastD;

    /* Mask off the second control inputs if it is not fitted */
    if (SecondControlNotFittedCount >= SECOND_CONTROL_TIMEOUT_CYCLES)
    {
      newb &= ~B_MASK_EOT4;
      newd &= ~D_MASK_EOT3;
    }

    /* Tell the application which inputs were just asserted */
    if (newd & D_MASK_EOT1)
    {
      input_asserted(INPUT_EOT1);
    }
    if (newd & D_MASK_EOT2)
    {
      input_asserted(INPUT_EOT2);
    }
    if (newd & D_MASK_UP)
    {
      input_asserted(INPUT_UP);
    }
    if (newd & D_MASK_EOT3)
    {
      input_asserted(INPUT_EOT3);
    }
    if (newb & B_MASK_EOT4)
    {
      input_asserted(INPUT_EOT4);
    }
    if (newb & B_MASK_DOWN)
    {
      input_asserted(INPUT_DOWN);
    }
    if (newb & B_MASK_RESTART)
    {
      input_asserted(INPUT_RESTART);
    }
    if (newb & B_MASK_PAUSE)
    {
      input_asserted(INPUT_PAUSE);
    }
    if (newb & B_MASK_COPY)
    {
      input_asserted(INPUT_COPY);
    }
    
  } /* end if any keys are pressed */

  if ((LastB != 0) || (LastD != 0))
  {
    uint8_t oldb;
    uint8_t oldd;

    /* Identify which inputs have just been de-asserted */
    oldb = LastB & ~pb;
    oldd = LastD & ~pd;

    /* Clear the long-push counters */
    if (oldd & D_MASK_UP)
    {
      UpCounter = 0;
    }
    if (oldb & B_MASK_DOWN)
    {
      DownCounter = 0;
    }
    if (oldb & B_MASK_PAUSE)
    {
      PauseCounter = 0;
    }

    /* Mask off the second control inputs if it is not fitted */
    if (SecondControlNotFittedCount >= SECOND_CONTROL_TIMEOUT_CYCLES)
    {
      oldb &= ~B_MASK_EOT4;
      oldd &= ~D_MASK_EOT3;
    }

  }

  LastB = pb;
  LastD = pd;
}

uint8_t is_second_control_fitted(void)
{
  if (SecondControlNotFittedCount < SECOND_CONTROL_TIMEOUT_CYCLES)
  {
    return 1;
  }
  else
  {
    return 0;
  }
}
uint8_t raw_input(uint8_t n)
{
  if (n == 0)
  {
    return (PINB ^ B_INVERTED) & B_MASK;
  }
  else
  {
    return (PIND ^ D_INVERTED) & D_MASK;
  }
}

EMPTY_INTERRUPT(PCINT0_vect);
EMPTY_INTERRUPT(PCINT2_vect);
