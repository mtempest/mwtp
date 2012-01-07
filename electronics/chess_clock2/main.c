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
#include "clock.h"

static void init_other_hw(void);
static void sleep_until_interrupt(void);

int main(void)
{
  init_other_hw(); /* Must call this first */
  init_timer();
  init_audio();
  init_turnled();
  init_inputs();

  enable_task(TURNLED_TASK);
  enable_task(AUDIO_TASK);
  enable_task(INPUTS_TASK);
  enable_task(COUNTDOWN_TASK);
   
  /* initialize display, cursor off */
  lcd_init(LCD_DISP_ON);

  init_clock();

  /* Enable interrupts */
  sei();

  for (;;) {                           /* loop forever */
    poll_inputs();
    poll_clock();
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
