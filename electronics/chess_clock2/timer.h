/*
 * timer.h
 */

/* PRIVATE */
extern uint8_t __timer_timestamp;
/* END PRIVATE */

#define timestamp() (*(volatile const uint8_t*)&__timer_timestamp)

uint8_t seconds_since(const uint8_t since_timestamp, uint8_t * new_timestamp_ptr);

void init_timer(void);

enum
{
  AUDIO_TASK,
  TURNLED_TASK,
  BACKLIGHT_TASK,
  COUNTDOWN_TASK,
  INPUTS_TASK,
  NUM_TASKS
};
void enable_task(uint8_t id);
void disable_task(uint8_t id);

uint8_t is_any_task_active(void);

typedef struct
{
  /* public fields */
  uint8_t minutes;
  uint8_t seconds;

  /* private fields */
  uint8_t _running;
  uint8_t _expired;
  uint8_t _subseconds;
} CountdownType;

enum {
  COUNTDOWN_1,
  COUNTDOWN_2,
  COUNTDOWN_3,
  COUNTDOWN_4,
  NUM_COUNTDOWNS
};
extern CountdownType Countdown[NUM_COUNTDOWNS];

#define start_countdown(id) do { Countdown[id]._running = 1; } while (0)
#define stop_countdown(id) do { Countdown[id]._running = 0; } while (0)
#define countdown_has_expired(id) (!!Countdown[id]._expired)
#define countdown_is_running(id) (!!Countdown[id]._running)

#define set_countdown(id, min, sec) do { Countdown[id].minutes = min; \
                                         Countdown[id].seconds = sec; \
                                         Countdown[id]._running = 0; \
                                         Countdown[id]._expired = 0; \
                                         Countdown[id]._subseconds = 0; \
                                    } while(0)

