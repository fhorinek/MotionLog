#ifndef XLIB_COMMON_H_
#define XLIB_COMMON_H_

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <util/delay.h>
#include <util/atomic.h>

#ifdef __cplusplus
	#include "oofix.h"
#endif

#define LOW		0
#define HIGH	1
#define MEDIUM	2
#define NONE	4

#define LO		LOW
#define HI		HIGH
#define ME		MEDIUM

#define OUTPUT	HIGH
#define INPUT	LOW

#define ON		HIGH
#define OFF		LOW

#define MSB		HIGH
#define LSB		LOW

#define READ	HIGH
#define WRITE	LOW

#define BUFFER_SIZE	64

#define BAT_EN_ADC	(1 << 0)
#define BAT_EN_LED	(1 << 1)

#ifdef abs
#undef abs
#endif

#define min(a,b) ((a)<(b)?(a):(b))
#define max(a,b) ((a)>(b)?(a):(b))
#define abs(x) ((x)>0?(x):-(x))

#ifdef __cplusplus
extern "C" {
#endif

void EnableInterrupts();
void DisableInterrupts();
void CCPIOWrite(volatile uint8_t * address, uint8_t value);
uint8_t CalcCRC(uint8_t old_crc, uint8_t key, uint8_t data);

void turnoff_subsystems();

void bat_en_high(uint8_t mask);
void bat_en_low(uint8_t mask);


union byte4_u
{
	float flt;
	uint32_t uint32;
	int32_t int32;
	uint8_t bytes[4];
};

union byte2_u
{
	uint16_t uint16;
	int16_t int16;
	uint8_t bytes[2];
};

union byte1_u
{
	int8_t int8;
	uint8_t uint8;
};

#ifdef __cplusplus
}
#endif


#endif /* XLIB_COMMON_H_ */
