#include <stdlib.h>
#include <stdint.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include "lcd.h"
#include <util/delay.h>
#include <util/atomic.h>

#include "timer.h"
#include "audio.h"
#include "turnled.h"
#include "input.h"

static void init_other_hw(void);
static void sleep_until_interrupt(void);

#define IN_SIZE 12
static char inputs[IN_SIZE];

void input_asserted(uint8_t id)
{
  static uint8_t inputs_idx;
  if (inputs_idx >= IN_SIZE-1)
  {
    inputs_idx = 0;
  }
  inputs[inputs_idx] = "1234UDCPR"[id];
  inputs_idx++;

  switch(id)
  {
    case INPUT_EOT1:
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
      {
        play(tick);
        turnled_off(0);
        turnled_on(1);
      }
      break;
    case INPUT_EOT2:
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
      {
        play(tick);
        turnled_off(1);
        turnled_on(0);
      }
      break;
    case INPUT_EOT3:
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
      {
        play(tick);
        turnled_off(2);
        turnled_on(3);
      }
      break;
    case INPUT_EOT4:
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
      {
        play(tick);
        turnled_off(3);
        turnled_on(2);
      }
      break;
    case INPUT_PAUSE:
      play(tada);
      break;
    default:
      /* ignore the other keys for now */
      break;
  }
}

int main(void)
{
  char buffer[7];
  uint16_t num=0;
  uint8_t ts;
  uint8_t increment;

  init_other_hw(); /* Must call this first */
  init_timer();
  init_audio();
  init_turnled();
  init_inputs();

  enable_task(TURNLED_TASK);
  enable_task(AUDIO_TASK);
  enable_task(INPUTS_TASK);
   
  /* initialize display, cursor off */
  lcd_init(LCD_DISP_ON);


  /* Enable interrupts */
  sei();

  ts = timestamp();
  for (;;) {                           /* loop forever */
    poll_inputs();
    increment = seconds_since(ts, &ts);
    if (increment)
    {
      num += increment;

      /* convert integer into string */
      itoa( num , buffer, 10);

      /* clear display and home cursor */
      lcd_clrscr();

      /* put converted string to display */
      lcd_puts(buffer);
      lcd_gotoxy(0,1);
      lcd_puts(inputs);

      lcd_gotoxy(8,0);
      itoa( raw_input(0), buffer, 16);
      lcd_puts(buffer);

      lcd_gotoxy(11,0);
      itoa( raw_input(1), buffer, 16);
      lcd_puts(buffer);

      if (is_second_control_fitted())
      {
        lcd_gotoxy(15,0);
        lcd_putc('2');
      }
    } /* end if increment */
    sleep_until_interrupt();
  } /* end loop forever */
}

static void init_other_hw(void)
{
  /* Turn off hardware that isn't used */
  PRR = (1<<PRTWI)    /* Turn off TWI */
      | (0<<PRTIM2)   /* leave Timer2 on */
      | (1<<PRTIM0)   /* Turn off Timer0 */
      | (0<<PRTIM1)   /* leave Timer1 on */
      | (1<<PRSPI)    /* Turn off SPI */
      | (1<<PRUSART0) /* Turn off USART */
      | (1<<PRADC);   /* Turn off ADC */

  /* Set all pins to input and enable pullups */
  DDRB = DDRC = DDRD = 0;
  PORTB = PORTC = PORTD = 0xFF;
}

static void sleep_until_interrupt(void)
{
  if (is_any_task_active() == 0)
  {
    SMCR = (0<<SM2) | (1<<SM1) | (1<<SM0) | (1<<SE); /* Enable sleep in "power save" mode */
    /* Note: timer 2 is still running in power save mode */
 
    asm("sleep");
    
    SMCR &= (1<<SE);  /* Clear the sleep-enable bit to prevent inadvertent sleep */
  }
}
