/*
 * audio.c
 */

#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/atomic.h>
#include <util/delay.h>

#include "audio.h"
#include "timer.h"

/* f = F_CPU/PRESCALER/2/(TOP+1) */
/* TOP = F_CPU/PRESCALER/2/f - 1 */
#define PRESCALER 1
#define TOP_FOR_HZ(f) (F_CPU/PRESCALER/2/(f) - 1)

enum
{
  TONE_A4,
  TONE_D5,
  TONE_E5,
  TONE_D7,
  TONE_FF,
  TONE_NONE
};
#define FREQ_A4 440.00
#define FREQ_D5 587.33
#define FREQ_E5 659.26
#define FREQ_A6 1174.7
#define FREQ_D7 2349.3

#define LAST 1
#define CONTINUE 0
#define TONE_MASK 0x7
#define PERIOD_MASK 0xF
#define PERIOD_SHIFT 3
#define LAST_SHIFT 7
#define AUDIO_CMD(tone_id, period, last) (((tone_id) & TONE_MASK) | (((period) & PERIOD_MASK)<<PERIOD_SHIFT) | (((last)==LAST)<<LAST_SHIFT))

static const prog_uint8_t* audio_cmd_ptr;
static uint8_t cycle_count;

const prog_uint8_t tick[] =
{
  AUDIO_CMD(TONE_D7,1,LAST)
};

const prog_uint8_t tada[] =
{
  AUDIO_CMD(TONE_FF,1,CONTINUE),
  AUDIO_CMD(TONE_A4,1,CONTINUE),
  AUDIO_CMD(TONE_D5,1,CONTINUE),
  AUDIO_CMD(TONE_E5,1,CONTINUE),
  AUDIO_CMD(TONE_A4,1,CONTINUE),
  AUDIO_CMD(TONE_D5,1,CONTINUE),
  AUDIO_CMD(TONE_E5,1,CONTINUE),
  AUDIO_CMD(TONE_A4,1,CONTINUE),
  AUDIO_CMD(TONE_D5,1,CONTINUE),
  AUDIO_CMD(TONE_E5,1,CONTINUE),
  AUDIO_CMD(TONE_A4,1,CONTINUE),
  AUDIO_CMD(TONE_D5,1,CONTINUE),
  AUDIO_CMD(TONE_E5,1,CONTINUE),
  AUDIO_CMD(TONE_A4,1,CONTINUE),
  AUDIO_CMD(TONE_D5,1,CONTINUE),
  AUDIO_CMD(TONE_E5,1,CONTINUE),
  AUDIO_CMD(TONE_A4,1,CONTINUE),
  AUDIO_CMD(TONE_D5,1,CONTINUE),
  AUDIO_CMD(TONE_E5,1,CONTINUE),
  AUDIO_CMD(TONE_FF,1,LAST)
};

static void process_one_command(void);

void init_audio(void)
{
  TCCR1A = (0<<COM1A1) | (0<<COM1A0)    /* Disconnect OC1A from output */
         | (0<<COM1B1) | (0<<COM1B0)    /* Disconnect OC1B from output */
         | (1<<WGM11) | (1<<WGM10);     /* Together with WGM13 and WGM12: Fast PWM mode, OCR1A sets top */
  TCCR1B = (0<<ICNC1)                   /* Input capture noise canceller is not used */
         | (0<<ICES1)                   /* Don't care which input capture edge is used */
         | (1<<WGM13) | (1<<WGM12)      /* See above */
         | (0<<CS12) | (0<<CS11) | (1<<CS10); /* Prescaler divides by 1 */

  /* Note: it is not necessary to reinitialise the timer when it is turned back on */
  PRR |= (1<<PRTIM1);  /* Turn off Timer1 */

  /* Set the pin to be an output set low */
  OC1A_PORT &= ~(1<<OC1A_BIT);
  OC1A_DDR |= (1<<OC1A_BIT);

}

void play (const prog_uint8_t * cmds)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    audio_cmd_ptr = cmds;
    PRR &= ~(1<<PRTIM1);  /* Turn on Timer1 */
    process_one_command();
    enable_task(AUDIO_TASK);
  }
}

void process_audio(void)
{
  uint8_t cmd;
  if (cycle_count > 0)
  {
    cycle_count --;
  }
  if (cycle_count == 0)
  {
    cmd = pgm_read_byte(audio_cmd_ptr);
    if ((cmd & (1<<LAST_SHIFT)) == CONTINUE)
    {
      audio_cmd_ptr++;
      process_one_command();
    }
    else
    {
      TCCR1A = (0<<COM1A1) | (0<<COM1A0)    /* Disconnect OC1A from output */
             | (0<<COM1B1) | (0<<COM1B0)    /* Disconnect OC1B from output */
             | (1<<WGM11) | (1<<WGM10);     /* Together with WGM13 and WGM12: Fast PWM mode, OCR1A sets top */
      disable_task(AUDIO_TASK);
      PRR |= (1<<PRTIM1);  /* Turn off Timer1 */
    }
  }
}

static void process_one_command(void)
{
  uint8_t cmd;
  cmd = pgm_read_byte(audio_cmd_ptr);
  cycle_count = (cmd >> PERIOD_SHIFT) & PERIOD_MASK;
  cmd &= TONE_MASK;
  if (cmd < TONE_NONE)
  {
    TCCR1A = (0<<COM1A1) | (1<<COM1A0)    /* Toggle OC1A on compare match */
           | (0<<COM1B1) | (0<<COM1B0)    /* Disconnect OC1B from output */
           | (1<<WGM11) | (1<<WGM10);     /* Together with WGM13 and WGM12: Fast PWM mode, OCR1A sets top */
    switch (cmd)
    {
    case TONE_A4:
      OCR1A = TOP_FOR_HZ(FREQ_A4);
      break;
    case TONE_D5:
      OCR1A = TOP_FOR_HZ(FREQ_D5);
      break;
    case TONE_E5:
      OCR1A = TOP_FOR_HZ(FREQ_E5);
      break;
    case TONE_D7:
      OCR1A = TOP_FOR_HZ(FREQ_D7);
      break;
    case TONE_FF:
      OCR1A = 1;
      break;
    }
  }
  else
  {
    /* No tone */
    TCCR1A = (0<<COM1A1) | (0<<COM1A0)    /* Disconnect OC1A from output */
           | (0<<COM1B1) | (0<<COM1B0)    /* Disconnect OC1B from output */
           | (1<<WGM11) | (1<<WGM10);     /* Together with WGM13 and WGM12: Fast PWM mode, OCR1A sets top */
  }
}


