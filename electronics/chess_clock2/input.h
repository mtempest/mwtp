/*
 * input.h
 */

void init_inputs(void);


enum {
  INPUT_EOT1,
  INPUT_EOT2,
  INPUT_EOT3,
  INPUT_EOT4,
  INPUT_UP,
  INPUT_DOWN,
  INPUT_COPY,
  INPUT_PAUSE,
  INPUT_RESTART
};

void input_asserted(uint8_t id); /* user must provide this */
void input_long_push(uint8_t id); /* user must provide this */
void input_repeat(uint8_t id); /* user must provide this */

uint8_t is_second_control_fitted(void);

uint8_t raw_input(uint8_t n);

/* Calls input_asserted(id) whenever a key is pressed */
void poll_inputs(void);

/* Calls input_long_push(id) whenever a key is held in long enough */
/* Calls input_repeat(id) cyclically for held-in keys after a hold-off period */
void process_inputs(void);
