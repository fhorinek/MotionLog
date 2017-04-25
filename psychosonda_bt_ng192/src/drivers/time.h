
#ifndef TIME_H_
#define TIME_H_

#include "../psychosonda.h"

void datetime_from_epoch(uint32_t epoch, uint8_t * psec, uint8_t * pmin, uint8_t * phour, uint8_t * pday, uint8_t * pwday, uint8_t * pmonth, uint16_t * pyear);
uint32_t datetime_to_epoch(uint8_t sec, uint8_t min, uint8_t hour, uint8_t day, uint8_t month, uint16_t year);
void time_str(char * buff, uint32_t epoch);

uint32_t time_get_actual();
void time_set_actual(uint32_t val);

void time_init();

void time_set_next_wake_up(uint32_t time);
bool time_for_wake_up();


#endif
