/*
 * timer.c
 */

#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>

#include "timer.h"
#include "audio.h"
#include "turnled.h"
#include "input.h"

#define MULTIPLIER   16
#define DIVISOR     125

uint8_t __timer_timestamp;
CountdownType Countdown[NUM_COUNTDOWNS];

static uint8_t tasks;

static void process_countdown(void);

void init_timer(void)
{
  TCCR2A = (0<<COM2A1) | (0<<COM2A0) /* OC2A disconnected */
         | (0<<COM2B1) | (0<<COM2B0) /* OC2B disconnected */
         | (1<<WGM21)  | (1<<WGM20); /* Together with WGM22: Fast PWM, OCR2A sets TOP */
  TCCR2B = (0<<FOC2A)                /* Don't force output compare 2A */
         | (0<<FOC2B)                /* Don't force output compare 2B */
         | (1<<WGM22)                /* See above */
         | (1<<CS22) | (1<<CS21) | (1<<CS20);  /* Prescaler divides by 1024 */

  OCR2A = 124; /* With a 1MHz clock, and 1024 prescaling, in FAST PWM mode - this gives 7.8125 Hz
                  That is 125/16.
                  So to get seconds, we must multiply the number of interrupts by 16 and divide by 125.
                  (249 gives 3.90625 Hz, which is 125/32 Hz) */

  TIMSK2 = (0<<OCIE2B)              /* No interrupt from output compare match 2B */
         | (0<<OCIE2A)              /* No interrupt from output compare match 2A */
         | (1<<TOIE2);              /* Enable interrupt from overflow */
}

uint8_t seconds_since(const uint8_t since_timestamp, uint8_t * new_timestamp_ptr)
{
  uint8_t now;
  now = timestamp();
  if (new_timestamp_ptr)
  {
    *new_timestamp_ptr = now;
  }
  return now - since_timestamp;
}

void enable_task(uint8_t id)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    tasks |= 1<<id;
  }
}

void disable_task(uint8_t id)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    tasks &= ~(1<<id);
  }
}

uint8_t is_any_task_active(void)
{
  return (tasks != 0);
}

/* Interrupt handler for timer2 overflow */
ISR(TIMER2_OVF_vect)
{
  static uint8_t count;

  /* Multiply the timer frequency by adding to a counter in each interrupt */
  count += MULTIPLIER;

  /* Then divide the counter to get seconds by detecting when the counter goes above
     the threshold (i.e. the divisor) and subtracting the divisor from the counter
     and incrementing the number of seconds */
  if (count >= DIVISOR)
  {
    count -= DIVISOR;
    __timer_timestamp++;
  }

  if (tasks & (1<<AUDIO_TASK))
  {
    process_audio();
  }

  if (tasks & (1<<TURNLED_TASK))
  {
    process_turnled();
  }

  if (tasks & (1<<COUNTDOWN_TASK))
  {
    process_countdown();
  }

  if (tasks & (1<<INPUTS_TASK))
  {
    process_inputs();
  }

}

static void process_countdown(void)
{
  uint8_t id;
  for (id = 0; id < NUM_COUNTDOWNS; id++)
  {
    if (Countdown[id]._running)
    {
      if (Countdown[id]._subseconds >= MULTIPLIER)
      {
        Countdown[id]._subseconds -= MULTIPLIER;
      }
      else
      {
        Countdown[id]._subseconds += DIVISOR - MULTIPLIER;
        if (Countdown[id].seconds > 0)
        {
          Countdown[id].seconds--;
        } /* end if seconds was non-zero */
        else
        {
          if (Countdown[id].minutes > 0)
          {
            Countdown[id].minutes--;
            Countdown[id].seconds = 59;
          }
          else
          {
            Countdown[id]._running = 0;
            Countdown[id]._expired = 1;
          }
        } /* end else seconds was zero */
      }
    } /* end if running */
  } /* end for all countdowns */
}
