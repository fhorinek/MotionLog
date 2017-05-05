#include "battery.h"
#include "../tasks/tasks.h"

SleepLock battery_lock;

void battery_init()
{
	BATTERY_ADC_PWR_ON;
	BATTERY_ADC_ENABLE;

	AdcBInit(adc_int1V);
	AdcBSetMode(adc_unsigned);

	AdcPipeSetSource(pipeb0, BAT_SNS_ADC);

	GpioSetDirection(BAT_EN, OUTPUT);
	bat_en_low(BAT_EN_ADC);
}


#define BATT_THOLD	VDC2ADC(2.2)

#define BATTERY_STATE_IDLE		0
#define BATTERY_STATE_PREPARE	1
#define BATTERY_STATE_START		2
#define BATTERY_STATE_RESULT	3

#define BATTERY_MEAS_AVG 		16
#define BATTERY_MEAS_PERIOD 	40
//#define BATTERY_MEAS_PERIOD 	1

uint32_t battery_next_meas = 0;
uint8_t  battery_meas_state = BATTERY_STATE_PREPARE;

uint16_t battery_meas_acc = 0;
uint8_t  battery_meas_cnt = 0;

volatile int16_t battery_adc_raw = 0;
int8_t battery_per = 0;

#define BATT_COEF_A	(0.1865671642)
#define BATT_COEF_B  (-547.3880597015)

extern volatile uint8_t bat_en_mask;

bool battery_step()
{
	if (battery_next_meas > time_get_actual())
		return false;

	switch (battery_meas_state)
	{
	case(BATTERY_STATE_IDLE):
		battery_adc_raw = battery_meas_acc / BATTERY_MEAS_AVG;
		battery_per = round(((float)battery_adc_raw * BATT_COEF_A) + BATT_COEF_B);
		if (battery_per > 100)
			battery_per = 100;
		if (battery_per < 0)
			battery_per = 0;

		task_irqh(TASK_IRQ_BAT, (uint8_t *)&battery_per);

//		DEBUG("adc %u (%3d%%)", battery_adc_raw, battery_per);

		battery_meas_state = BATTERY_STATE_PREPARE;
		battery_next_meas = time_get_actual() + BATTERY_MEAS_PERIOD;
		battery_meas_acc = 0;
		battery_meas_cnt = 0;

		return true;
	break;

	case(BATTERY_STATE_PREPARE):
		battery_meas_state = BATTERY_STATE_START;

		BATTERY_ADC_PWR_ON;
		BATTERY_ADC_ENABLE;
		bat_en_high(BAT_EN_ADC);
	break;

	case(BATTERY_STATE_START):
		AdcPipeStart(pipeb0);

		battery_meas_state = BATTERY_STATE_RESULT;
	break;

	case(BATTERY_STATE_RESULT):
		if (!AdcPipeReady(pipeb0))
		{
			DEBUG("adc not ready\n");
			return false;
		}

		uint16_t tmp = AdcPipeValue(pipeb0);

		battery_meas_acc += tmp;

		battery_meas_cnt++;

		if (battery_meas_cnt >= BATTERY_MEAS_AVG)
		{
			battery_meas_state = BATTERY_STATE_IDLE;

			bat_en_low(BAT_EN_ADC);

			BATTERY_ADC_DISABLE;
			BATTERY_ADC_PWR_OFF;
		}
		else
			battery_meas_state = BATTERY_STATE_START;
	break;
	}

	return false;
}
