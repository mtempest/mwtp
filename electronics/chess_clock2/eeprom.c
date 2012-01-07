/*
 * eeprom.c
 */

#include <stdint.h>
#include <avr/io.h>
#include <util/atomic.h>

/* These only give access to the lower 256 bytes of EEPROM */
uint8_t read_eeprom(uint8_t addr)
{
  uint8_t value;
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    /* Wait for completion of previous write */
    while(EECR & (1<<EEPE))
      ;

    /* Set up address register */
    EEAR = addr;

    /* Start eeprom read by writing EERE */
    EECR |= (1<<EERE);

    /* Return data from Data Register */
    value = EEDR;
  }
  return value;
}

void write_eeprom(uint8_t addr, uint8_t value)
{
  ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
  {
    /* Wait for completion of previous write */
    while(EECR & (1<<EEPE))
      ;

    /* Set up address and Data Registers */
    EEAR = addr;
    EEDR = value;

    /* Write logical one to EEMPE */
    EECR |= (1<<EEMPE);

    /* Start eeprom write by setting EEPE */
    EECR |= (1<<EEPE);
  }
}
