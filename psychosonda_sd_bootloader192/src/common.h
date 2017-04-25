#ifndef COMMON_H_
#define COMMON_H_

#include <avr/io.h>
#include <util/delay.h>

#define VERSION		0x0001

#define USART_USART		USARTD0
#define USART_PORT		PORTD
#define WAIT_TIME		100
#define USART_RXC		USARTD0_RXC_vect
#define USART_TXC		USARTD0_TXC_vect
#define UART_TX_PIN		3

#define LED_OFF			PORTE.OUTSET = 0b00000011;\
						PORTE.OUTCLR = 0b00100000;

#define LED_GREEN		PORTE.OUTCLR = 0b00000010;\
						PORTE.OUTSET = 0b00100000;

#define LED_BLUE		PORTE.OUTCLR = 0b00000001;\
						PORTE.OUTSET = 0b00100000;

#define LED_INIT		PORTE.DIRSET = 0b00100011;

#define LED_DEINIT		PORTE.DIRCLR = 0b00100011;

#define SD_SS_HI		PORTC.OUTSET = 0b00010000;
#define SD_SS_LO		PORTC.OUTCLR = 0b00010000;


void CCPIOWrite( volatile uint8_t * address, uint8_t value );

#endif /* COMMON_H_ */
