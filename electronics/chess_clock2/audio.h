/*
 * audio.h
 */

void init_audio(void);

void process_audio(void);

extern const prog_uint8_t tick[];
extern const prog_uint8_t tada[];
void play (const prog_uint8_t * cmds);
