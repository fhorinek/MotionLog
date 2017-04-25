/*
 * common.h
 *
 *  Created on: 12.9.2014
 *      Author: horinek
 */

#ifndef COMMON_H_
#define COMMON_H_

#include <xlib/core/clock.h>
#include <xlib/core/usart.h>
#include <xlib/core/spi.h>
#include <xlib/core/system.h>
#include <xlib/core/rtc.h>
#include <xlib/core/timer.h>
#include <xlib/core/adc.h>
#include <xlib/core/i2c.h>

#include "build_defs.h"
#include "debug.h"


union byte4
{
	int32_t uint32;
	uint8_t uint8[4];
};

union byte2
{
	uint16_t uint16;
	uint8_t uint8[2];
};

struct app_info
{
	uint8_t app_name[32];
};

extern struct app_info fw_info;


//PINOUT
//---------------- GENERAL -------------------
#define USB_PWR_ON				PR.PRGEN &= 0b10111111;
#define USB_PWR_OFF				PR.PRGEN |= 0b01000000;
#define RTC_PWR_ON				PR.PRGEN &= 0b11111011;
#define RTC_PWR_OFF				PR.PRGEN |= 0b00000100;

//---------------- PORTA ---------------------
#define ADS_CS_PIN				porta0
#define ADS_START_PIN			porta1
#define SWITCH					porta2
#define ADS_RESET_PIN			porta3
#define ADS_EN_PIN				porta4
#define GY_INT1					porta5
#define GY_INT2					porta6
#define AM_INT1					porta7

#define SWITCH_INT				porta_interrupt0
#define MEMS0_INT				porta_interrupt1

//---------------- PORTB ---------------------
//GND							portb0
#define AM_INT2					portb1
#define MEMS_EN1				portb2
#define MEMS_EN2				portb3
#define MEMS_EN3				portb4
#define I_SNS					portb5
#define I2C_EN					portb6
#define BAT_SNS					portb7

#define I_SNS_ADC				ext_portb5
#define BAT_SNS_ADC				ext_portb7

#define MEMS1_INT				portb_interrupt0

#define BATTERY_ADC_PWR_ON		PR.PRPB &= 0b11111101;
#define BATTERY_ADC_PWR_OFF		PR.PRPB |= 0b00000010;
#define BATTERY_ADC_DISABLE		ADCB.CTRLA = 0;
#define BATTERY_ADC_ENABLE		ADCB.CTRLA = ADC_ENABLE_bm;


#define MEMS_POWER_ON			GpioWrite(MEMS_EN1, HIGH);\
								GpioWrite(MEMS_EN2, HIGH);\
								GpioWrite(MEMS_EN3, HIGH);

#define MEMS_POWER_OFF			GpioWrite(MEMS_EN1, LOW);\
								GpioWrite(MEMS_EN2, LOW);\
								GpioWrite(MEMS_EN3, LOW);

#define MEMS_POWER_INIT			GpioSetDirection(MEMS_EN1, OUTPUT);\
								GpioSetDirection(MEMS_EN2, OUTPUT);\
								GpioSetDirection(MEMS_EN3, OUTPUT);\
								MEMS_POWER_OFF;

#define I2C_POWER_ON			GpioWrite(I2C_EN, HIGH);
#define I2C_POWER_OFF			GpioWrite(I2C_EN, LOW);
#define I2C_POWER_INIT			GpioSetDirection(I2C_EN, OUTPUT);\
								I2C_POWER_OFF;


//---------------- PORTC ---------------------
//SDA							portc0
//SCL							portc1
#define USB_IN					portc2
#define BUZZER					portc3
#define SD_SS					portc4
//MOSI SD						portc5
//MISO SD						portc6
//SCK  SD						portc7

#define MEMS_I2C				i2cc
#define MEMS_I2C_PWR_OFF		PR.PRPC |= 0b01000000
#define MEMS_I2C_PWR_ON			PR.PRPC &= 0b10111111

#define SD_SPI					spic
#define SD_SPI_PWR_ON			PR.PRPC &= 0b11110111;
#define SD_SPI_PWR_OFF			PR.PRPC |= 0b00001000;

#define BUZZER_TIMER			timerc0
#define BUZZER_TIMER_OVF		timerc0_overflow_interrupt
#define BUZZER_TIMER_PWR_OFF	PR.PRPC |= 0b00000001
#define BUZZER_TIMER_PWR_ON		PR.PRPC &= 0b11111110

#define TASK_TIMER				timerc1
#define TASK_TIMER_OVF			timerc1_overflow_interrupt
#define TASK_TIMER_PWR_OFF		PR.PRPC |= 0b00000010
#define TASK_TIMER_PWR_ON		PR.PRPC &= 0b11111101

#define USB_CONNECTED			(GpioRead(USB_IN) == HIGH)
#define USB_CONNECTED_IRQ		portc_interrupt0
#define USB_CONNECTED_IRQ_ON	GpioSetInterrupt(USB_IN, gpio_interrupt0, gpio_bothedges);
#define USB_CONNECTED_IRQ_OFF	GpioSetInterrupt(USB_IN, gpio_clear);

//---------------- PORTD ---------------------
#define SD_EN					portd0
#define LEDR					portd1
//FT_RXD						portd2
//FT_TXT						portd3
#define T1						portd4
#define T2						portd5
//XUSB_N						portd6
//XUSB_P						portd7

#define SIGNAL1_INIT			GpioSetDirection(portd4, OUTPUT);
#define SIGNAL2_INIT			GpioSetDirection(portd5, OUTPUT);

//STREDNY pin
#define SIGNAL1_HI				GpioWrite(portd4, HIGH);
#define SIGNAL1_LO				GpioWrite(portd4, LOW);

//VONKAJSI
#define SIGNAL2_HI				GpioWrite(portd5, HIGH);
#define SIGNAL2_LO				GpioWrite(portd5, LOW);

#define UART_UART				usartd0
#define UART_PWR_ON				PR.PRPD &= 0b11101111;
#define UART_PWR_OFF			PR.PRPD |= 0b00010000;

#define SD_EN_ON				GpioWrite(SD_EN, LOW);
#define SD_EN_OFF				GpioWrite(SD_EN, HIGH);
#define SD_EN_INIT				GpioSetDirection(SD_EN, OUTPUT); \
								SD_EN_OFF;

#define LED_TIMER2				timerd0
#define LED_TIMER2_PWR_ON		PR.PRPD	&= 0b11111110;
#define LED_TIMER2_PWR_OFF		PR.PRPD	|= 0b00000001;
#define LED_TIMER2_OVF			timerd0_overflow_interrupt

#define BMP_MEAS_TIMER			timerd1
#define BMP_MEAS_TIMER_OVF_IRQ	timerd1_overflow_interrupt
#define BMP_MEAS_TIMER_CMPA_IRQ	timerd1_compareA_interrupt
#define BMP_MEAS_TIMER_PWR_OFF	PR.PRPD |= 0b00000010
#define BMP_MEAS_TIMER_PWR_ON	PR.PRPD &= 0b11111101



//---------------- PORTE ---------------------
#define LEDB					porte0
#define LEDG					porte1
//BT_RXD						porte2
//BT_TXT						porte3
#define CHARGING				porte4
#define BAT_EN					porte5
//RTC TOSC2						porte6
//RTC TOSC1						porte7

#define BT_UART					usarte0
#define BT_UART_PWR_ON			PR.PRPE &= 0b11101111;
#define BT_UART_PWR_OFF			PR.PRPE |= 0b00010000;
#define BT_UART_DMA_CH			&DMA.CH0
#define BT_UART_DMA_TRIGGER		DMA_CH_TRIGSRC_USARTE0_RXC_gc

#define LED_TIMER1				timere0
#define LED_TIMER1_PWR_ON		PR.PRPE	&= 0b11111110;
#define LED_TIMER1_PWR_OFF		PR.PRPE	|= 0b00000001;
#define LED_TIMER1_OVF			timere0_overflow_interrupt

#define SHT_MEAS_TIMER			timere1
#define SHT_MEAS_TIMER_PWR_ON	PR.PRPE	&= 0b11111101;
#define SHT_MEAS_TIMER_PWR_OFF	PR.PRPE	|= 0b00000010;
#define SHT_MEAS_TIMER_OVF_IRQ	timere1_overflow_interrupt
#define SHT_MEAS_TIMER_CMPA_IRQ	timere1_compareA_interrupt
#define SHT_MEAS_TIMER_CMPB_IRQ	timere1_compareB_interrupt

//---------------- PORTF ---------------------
#define BT_EN					portf0
//SCK  ADS						portf1
//MISO ADS						portf2
//MOSI ADS						portf3
#define BT_RESET				portf4
#define BT_RTS					portf5
//GND							portf6
//GND							portf7

#define ADS_SPI_USART			usartf0
#define ADS_SPI_PWR_ON			PR.PRPF	&= 0b11101111;
#define ADS_SPI_PWR_OFF			PR.PRPF	|= 0b00010000;

#define DEBUG_TIMER				timerf0
#define DEBUG_TIMER_OVF			timerf0_overflow_interrupt
#define DEBUG_TIMER_PWR_OFF		PR.PRPF |= 0b00000001
#define DEBUG_TIMER_PWR_ON		PR.PRPF &= 0b11111110

//---------------- PORTR ---------------------
#define BT_CTS					portr0
#define ADS_DRDY_PIN			portr1

#define BT_CTS_PIN_INT			portr_interrupt0
#define ADS_DRDY_PIN_INT		portr_interrupt1

#define BUILD_VER	"%02d%02d%02d-%02d%02d", BUILD_YEAR, BUILD_MONTH, BUILD_DAY, BUILD_HOUR, BUILD_MIN

//--------------------------------------------

class DataBuffer
{
public:
	uint8_t * data;

	uint16_t size;
	uint16_t length;
	uint16_t write_index;
	uint16_t read_index;

	bool lock;

	DataBuffer(uint16_t size);
	~DataBuffer();

	uint16_t Read(uint16_t len, uint8_t * * data);
	bool Write(uint16_t len, uint8_t * data);

	uint16_t Length();
	void Clear();
};

//--------------------------------------------

uint8_t atoi_c(char * str);
char * find_comma(char * str);

void print_cpu_usage();
void test_memory();
bool cmpn(char * s1, const char * s2, uint8_t n);
void print_fw_info();
int freeRam();

uint8_t hex_to_num(uint8_t c);
uint32_t hexfn_to_num(char * fn);

uint8_t SP_ReadCalibrationByte(uint8_t index);

#endif /* COMMON_H_ */
