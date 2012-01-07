/*
 * eeprom.h
 */

/* These only give access to the lower 256 bytes of EEPROM */
uint8_t read_eeprom(uint8_t addr);
void write_eeprom(uint8_t addr, uint8_t value);
