
#ifndef BUZZER_H_
#define BUZZER_H_


#include "../psychosonda.h"

#define _1sec	1302
#define _100ms	130
#define _10ms	13

void buzzer_beep(uint16_t s1);
void buzzer_beep(uint16_t s1, uint16_t d1, uint16_t s2);
void buzzer_beep(uint16_t s1, uint16_t d1, uint16_t s2, uint16_t d2, uint16_t s3);
void buzzer_init();

bool buzzer_is_silent();
void buzzer_set_mute(bool set);

#endif /* BUZZER_H_ */
