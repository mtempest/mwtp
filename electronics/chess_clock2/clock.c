/*
 * clock.c - the engine of the chess clock
 */

#include <stdint.h>
#include <stdlib.h>
#include <util/atomic.h>
#include <avr/pgmspace.h>

#include "lcd.h"
#include "timer.h"
#include "input.h"
#include "audio.h"
#include "turnled.h"
#include "eeprom.h"
#include "clock.h"

/* Saves which countdowns were running when paused */
static uint8_t was_running;

enum {
  PLAY_MODE, /* default mode */
  WON_MODE,
  SETUP_MODE,
  NUM_MODES
};
static uint8_t mode;

/* Flag that indicates if the whole display should be updated in PLAY mode */
static uint8_t update_display;

static uint8_t prev_second[NUM_COUNTDOWNS];

/* Current cursor position in SETUP mode */
static uint8_t selected_countdown;
#define FIRST_SECONDS_DIGIT 2
#define MAX_DIGITS 4
static uint8_t selected_digit;

enum {
  CODE_1,
  CODE_2,
  CODE_3,
  CODE_4,
  CODE_5,
  CODE_7,
  CODE_B,
  CODE_W,
  NUM_CODES
};
static prog_char invertmap[10] = { 'O', CODE_1, CODE_2, CODE_3, CODE_4, CODE_5, '9', CODE_7, '8', '6' };
#define BYTES_PER_CHAR 8
#define PATTERN______ 0x00
#define PATTERN_____X 0x01
#define PATTERN____X_ 0x02
#define PATTERN____XX 0x03
#define PATTERN___X__ 0x04
#define PATTERN___X_X 0x05
#define PATTERN___XX_ 0x06
#define PATTERN___XXX 0x07
#define PATTERN__X___ 0x08
#define PATTERN__X__X 0x09
#define PATTERN__X_X_ 0x0a
#define PATTERN__X_XX 0x0b
#define PATTERN__XX__ 0x0c
#define PATTERN__XX_X 0x0d
#define PATTERN__XXX_ 0x0e
#define PATTERN__XXXX 0x0f
#define PATTERN_X____ 0x10
#define PATTERN_X___X 0x11
#define PATTERN_X__X_ 0x12
#define PATTERN_X__XX 0x13
#define PATTERN_X_X__ 0x14
#define PATTERN_X_X_X 0x15
#define PATTERN_X_XX_ 0x16
#define PATTERN_X_XXX 0x17
#define PATTERN_XX___ 0x18
#define PATTERN_XX__X 0x19
#define PATTERN_XX_X_ 0x1a
#define PATTERN_XX_XX 0x1b
#define PATTERN_XXX__ 0x1c
#define PATTERN_XXX_X 0x1d
#define PATTERN_XXXX_ 0x1e
#define PATTERN_XXXXX 0x1f
static const PROGMEM uint8_t charmaps [NUM_CODES*BYTES_PER_CHAR] =
{
  /* CODE_1 */
    PATTERN__XXX_,
    PATTERN___X__,
    PATTERN___X__,
    PATTERN___X__,
    PATTERN___X__,
    PATTERN___XX_,
    PATTERN___X__,
    PATTERN______,
  /* CODE_2 */
    PATTERN_XXXXX,
    PATTERN____X_,
    PATTERN___X__,
    PATTERN__X___,
    PATTERN_X____,
    PATTERN_X___X,
    PATTERN__XXX_,
    PATTERN______,
  /* CODE_3 */
    PATTERN__XXX_,
    PATTERN_X___X,
    PATTERN_X____,
    PATTERN__X___,
    PATTERN___X__,
    PATTERN__X___,
    PATTERN_XXXXX,
    PATTERN______,
  /* CODE_4 */
    PATTERN__X___,
    PATTERN__X___,
    PATTERN_XXXXX,
    PATTERN__X__X,
    PATTERN__X_X_,
    PATTERN__XX__,
    PATTERN__X___,
    PATTERN______,
  /* CODE_5 */
    PATTERN__XXX_,
    PATTERN_X___X,
    PATTERN_X____,
    PATTERN_X____,
    PATTERN__XXXX,
    PATTERN_____X,
    PATTERN_XXXXX,
    PATTERN______,
  /* CODE_7 */
    PATTERN____X_,
    PATTERN____X_,
    PATTERN____X_,
    PATTERN___X__,
    PATTERN__X___,
    PATTERN_X____,
    PATTERN_XXXXX,
    PATTERN______,
  /* CODE_B */
    PATTERN__XXXX,
    PATTERN_X___X,
    PATTERN_X___X,
    PATTERN__XXXX,
    PATTERN_X___X,
    PATTERN_X___X,
    PATTERN__XXXX,
    PATTERN______,
  /* CODE_W */
    PATTERN__X_X_,
    PATTERN_X_X_X,
    PATTERN_X_X_X,
    PATTERN_X_X_X,
    PATTERN_X___X,
    PATTERN_X___X,
    PATTERN_X___X,
    PATTERN______,
};

static void restart(void)
{
  uint8_t id;
  uint8_t addr;
  uint8_t minutes;
  uint8_t seconds;
  addr = 0;
  for (id = 0; id < NUM_COUNTDOWNS; id++)
  {
    minutes = read_eeprom(addr);
    addr++;
    seconds = read_eeprom(addr);
    addr++;

    if (minutes > 99)
    {
      minutes = 10; /* a sensible arbitrary default */
    }
    if (seconds > 59)
    {
      seconds = 0; /* a sensible arbitrary default */
    }
    set_countdown(id, minutes, seconds);
    turnled_off(id);
  }
  was_running = 0;
  update_display = 1;
}

void init_clock(void)
{
  uint8_t i;
  lcd_command(_BV(LCD_CGRAM));  /* set CG RAM start address 0 */
  for(i=0; i<NUM_CODES*BYTES_PER_CHAR; i++)
  {
    lcd_data(pgm_read_byte_near(&charmaps[i]));
  }
  restart();
}

static void showturn(uint8_t id)
{
  if (countdown_is_running(id))
  {
    lcd_putc('*');
  }
  else if (was_running & (1<<id))
  {
    lcd_putc('=');
  }
  else
  {
    lcd_putc(' ');
  }
}

static void showplaytime(uint8_t minutes, uint8_t seconds, uint8_t invert)
{
  char buffer[4];
  
  if (invert)
  {
    itoa(seconds, buffer, 10); 
    if (seconds < 10)
    {
      lcd_putc(pgm_read_byte_near(&invertmap[buffer[0]-'0']));
      lcd_putc('O');
    }
    else
    {
      lcd_putc(pgm_read_byte_near(&invertmap[buffer[1]-'0']));
      lcd_putc(pgm_read_byte_near(&invertmap[buffer[0]-'0']));
    }
    
    lcd_putc(':');
    
    itoa(minutes, buffer, 10); 
    if (minutes < 10)
    {
      lcd_putc(pgm_read_byte_near(&invertmap[buffer[0]-'0']));
      lcd_putc('O');
    }
    else
    {
      lcd_putc(pgm_read_byte_near(&invertmap[buffer[1]-'0']));
      lcd_putc(pgm_read_byte_near(&invertmap[buffer[0]-'0']));
    }
  }
  else
  {
    itoa(minutes, buffer, 10); 
    if (minutes < 10)
    {
      lcd_putc('0');
    }
    lcd_puts(buffer);
    
    lcd_putc(':');
    
    itoa(seconds, buffer, 10); 
    if (seconds < 10)
    {
      lcd_putc('0');
    }
    lcd_puts(buffer);
  }
}

static void update_play(uint8_t id)
{
  uint8_t minutes;
  uint8_t seconds;
  uint8_t invert;

  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    minutes = Countdown[id].minutes;
    seconds = Countdown[id].seconds;
  }

  lcd_gotoxy(8 * (id%2), id/2);

  if ((id >= 2) && !is_second_control_fitted())
  {
    lcd_puts("        ");
    return;
  }

  invert = 0;
  if (id == 0)
  {
    lcd_putc('B');
    showturn(0);
  }
  else if (id == 2)
  {
    lcd_putc('W');
    showturn(2);
  }
  else
  {
    lcd_putc(' ');
    invert = 1;
  }

  showplaytime(minutes, seconds, invert);

  if (id == 1)
  {
    showturn(1);
    lcd_putc(CODE_W);
  }
  else if (id == 3)
  {
    showturn(3);
    lcd_putc(CODE_B);
  }
  else
  {
    lcd_putc(' ');
  }
} 

static void setup_cursor(void)
{
  uint8_t x;
  uint8_t y;
  if ((selected_countdown / 2) == 0)
  {
    y = 0;
  }
  else
  {
    y = 1;
  }

  if ((selected_countdown % 2) == 0)
  {
    x = 2 + selected_digit;
    if (selected_digit >= 2)
    {
      x++;
    }

  }
  else
  {
    x = 13 - selected_digit;
    if (selected_digit >= 2)
    {
      x--;
    }
  }
  
  lcd_gotoxy(x,y);
}      

void poll_clock(void)
{
  if (mode == PLAY_MODE)
  {
    uint8_t id;
    uint8_t force_update;
    uint8_t num_ids;
    if (is_second_control_fitted())
    {
      num_ids = 4;
    }
    else
    {
      num_ids = 2;
      if (countdown_is_running(COUNTDOWN_3) || countdown_is_running(COUNTDOWN_4))
      {
        stop_countdown(COUNTDOWN_3);
        stop_countdown(COUNTDOWN_4);
        update_display = 1;
      }
    }

    id = 0;
    while (id < num_ids)
    {
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
      {
        if (countdown_has_expired(id))
        {
          mode = WON_MODE;
          for (id = 0; id < NUM_COUNTDOWNS; id++)
          {
            stop_countdown(id);
            turnled_off(id);
          }
          update_display = 1;
          play(tada);
        }
      }
      id++;
    } /* end for all countdowns */

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
      force_update = update_display;
      update_display = 0;
    }
    for (id = 0; id < NUM_COUNTDOWNS; id++)
    {
      if (force_update || prev_second[id] != Countdown[id].seconds)
      {
        update_play(id);
        prev_second[id] = Countdown[id].seconds;
      }
    } /* end for all countdowns */
  } /* end if play mode */
}

static void play_mode_input_asserted(uint8_t id)
{
  update_display = 1;
  switch(id)
  {
  case INPUT_EOT1:
    if (! countdown_has_expired(COUNTDOWN_1))
    {
      play(tick);
      turnled_off(TURNLED_1);
      turnled_on(TURNLED_2);
      stop_countdown(COUNTDOWN_1);
      start_countdown(COUNTDOWN_2);
    }
    break;
  case INPUT_EOT2:
    if (! countdown_has_expired(COUNTDOWN_2))
    {
      play(tick);
      turnled_off(TURNLED_2);
      turnled_on(TURNLED_1);
      stop_countdown(COUNTDOWN_2);
      start_countdown(COUNTDOWN_1);
    }
    break;
  case INPUT_EOT3:
    if (! countdown_has_expired(COUNTDOWN_3))
    {
      play(tick);
      turnled_off(TURNLED_3);
      turnled_on(TURNLED_4);
      stop_countdown(COUNTDOWN_3);
      start_countdown(COUNTDOWN_4);
    }
    break;
  case INPUT_EOT4:
    if (! countdown_has_expired(COUNTDOWN_4))
    {
      play(tick);
      turnled_off(TURNLED_4);
      turnled_on(TURNLED_3);
      stop_countdown(COUNTDOWN_4);
      start_countdown(COUNTDOWN_3);
    }
    break;
  case INPUT_PAUSE:
    if (was_running == 0)
    {
      uint8_t id;
      was_running = 0;
      for (id = 0; id < NUM_COUNTDOWNS; id++)
      {
        was_running |= countdown_is_running(id)<<id;
        stop_countdown(id);
      }
    }
    else
    {
      uint8_t id;
      for (id = 0; id < NUM_COUNTDOWNS; id++)
      {
        if ((was_running & (1<<id)) != 0)
        {
          start_countdown(id);
        }
      }
    }
    break;
  case INPUT_RESTART:
    restart();
  default:
    /* ignore the other keys in play mode */
    break;
  } /* end switch */
}

static void add_to_digit(uint8_t * digit_ptr, int8_t delta)
{
  *digit_ptr += (uint8_t)delta;
  if (*digit_ptr > 59)
  {
    *digit_ptr = 0;
  }
}

static void setup_mode_input_asserted(uint8_t id)
{
  int8_t delta;
  uint8_t other_countdown;
  switch(id)
  {
  case INPUT_EOT1:
    if (selected_countdown == COUNTDOWN_1)
    {
      selected_digit++;
      if (selected_digit >= MAX_DIGITS)
      {
        selected_digit = 0;
      }
    }
    else
    {
      selected_countdown = COUNTDOWN_1;
      selected_digit = 0;
    }
    setup_cursor();
    break;
  case INPUT_EOT2:
    if (selected_countdown == COUNTDOWN_2)
    {
      selected_digit++;
      if (selected_digit >= MAX_DIGITS)
      {
        selected_digit = 0;
      }
    }
    else
    {
      selected_countdown = COUNTDOWN_2;
      selected_digit = 0;
    }
    setup_cursor();
    break;
  case INPUT_EOT3:
    if (selected_countdown == COUNTDOWN_3)
    {
      selected_digit++;
      if (selected_digit >= MAX_DIGITS)
      {
        selected_digit = 0;
      }
    }
    else
    {
      selected_countdown = COUNTDOWN_3;
      selected_digit = 0;
    }
    setup_cursor();
    break;
  case INPUT_EOT4:
    if (selected_countdown == COUNTDOWN_4)
    {
      selected_digit++;
      if (selected_digit >= MAX_DIGITS)
      {
        selected_digit = 0;
      }
    }
    else
    {
      selected_countdown = COUNTDOWN_4;
      selected_digit = 0;
    }
    setup_cursor();
    break;
  case INPUT_UP:
    if ((selected_digit % 2) == 0)
    {
      if ((selected_countdown % 2) == 0)
      {
        delta = 10;
      }
      else
      {
        delta = -10;
      }
    }
    else
    {
      if ((selected_countdown % 2) == 0)
      {
        delta = 1;
      }
      else
      {
        delta = -1;
      }
    }
    if (selected_digit < FIRST_SECONDS_DIGIT)
    {
      add_to_digit(&Countdown[selected_countdown].minutes, delta);
    }
    else
    {
      add_to_digit(&Countdown[selected_countdown].seconds, delta);
    }
    update_play(selected_countdown);
    setup_cursor();
    break;
  case INPUT_DOWN:
    if ((selected_digit % 2) == 0)
    {
      if ((selected_countdown % 2) == 0)
      {
        delta = -10;
      }
      else
      {
        delta = 10;
      }
    }
    else
    {
      if ((selected_countdown % 2) == 0)
      {
        delta = -1;
      }
      else
      {
        delta = 1;
      }
    }
    if (selected_digit < FIRST_SECONDS_DIGIT)
    {
      add_to_digit(&Countdown[selected_countdown].minutes, delta);
    }
    else
    {
      add_to_digit(&Countdown[selected_countdown].seconds, delta);
    }
    update_play(selected_countdown);
    setup_cursor();
    break;
  case INPUT_COPY:
    other_countdown = selected_countdown ^ 1;
    Countdown[other_countdown].minutes = Countdown[selected_countdown].minutes;
    Countdown[other_countdown].seconds = Countdown[selected_countdown].seconds;
    update_play(other_countdown);
    setup_cursor();
    break;
  case INPUT_RESTART:
    mode = PLAY_MODE;

    /* turn off cursor */
    lcd_command(LCD_DISP_ON);

    restart();
    break;
  default:
    /* ignore other keys in setup mode */
    break;
  }
}

void input_asserted(uint8_t id)
{
  switch (mode)
  {
  case PLAY_MODE:
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
      play_mode_input_asserted(id);
    } /* end of atomic block */
    break;

  case WON_MODE:
    if (id == INPUT_RESTART)
    {
      ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
      {
        mode = PLAY_MODE;
        restart();
      }
    }
    break;

  case SETUP_MODE:
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
      setup_mode_input_asserted(id);
    } /* end of atomic block */
    break;

  default:
    /* recover from illegal mode */
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
      restart();
      mode = PLAY_MODE;
    } /* end of atomic block */
    break;
    
  }
}

void input_long_push(uint8_t id)
{
  uint8_t countdown;
  switch (mode)
  {
  case PLAY_MODE:
  case WON_MODE:
    if (id == INPUT_PAUSE)
    {
      was_running = 0;
      mode = SETUP_MODE;
      selected_countdown = 0;
      selected_digit = 0;
      for (countdown = 0; countdown < NUM_COUNTDOWNS; countdown++)
      {
        stop_countdown(countdown);
        turnled_off(countdown);
      }

      /* turn on cursor */
      lcd_command(LCD_DISP_ON_CURSOR);
      setup_cursor();
    }
    break;

  case SETUP_MODE:
    if (id == INPUT_PAUSE)
    {
      uint8_t addr;
      mode = PLAY_MODE;

      addr = 0;
      for (countdown = 0; countdown < NUM_COUNTDOWNS; countdown++)
      {
        write_eeprom(addr, Countdown[countdown].minutes);
        addr++;
        write_eeprom(addr, Countdown[countdown].seconds);
        addr++;
      }

      /* turn off cursor */
      lcd_command(LCD_DISP_ON);

      restart();
    }
    else if (id == INPUT_COPY)
    {
      uint8_t other_countdown;
      for (other_countdown = 0; other_countdown < NUM_COUNTDOWNS; other_countdown++)
      {
        if (other_countdown != selected_countdown)
        {
          Countdown[other_countdown].minutes = Countdown[selected_countdown].minutes;
          Countdown[other_countdown].seconds = Countdown[selected_countdown].seconds;
          update_play(other_countdown);
        }
      }
      setup_cursor();
    }
    break;
  default:
    break;
  }
}

void input_repeat(uint8_t id)
{
 if ((mode == SETUP_MODE) &&
     ((id == INPUT_UP) || (id == INPUT_DOWN)))
 {
   input_asserted(id);
 }
}

