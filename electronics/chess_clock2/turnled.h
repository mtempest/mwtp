/*
 * turnled.h
 */

void init_turnled(void);

void process_turnled(void);

enum {
  TURNLED_1,
  TURNLED_2,
  TURNLED_3,
  TURNLED_4,
  NUM_TURNLEDS
};
void turnled_on(uint8_t id);
void turnled_off(uint8_t id);
