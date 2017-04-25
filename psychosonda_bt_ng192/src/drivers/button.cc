/*
 * button.cc
 *
 *  Created on: 30.7.2014
 *      Author: horinek
 */

#include "button.h"

volatile uint8_t button_state = BUTTON_IDLE;
volatile uint32_t button_timestamp = 0;

SleepLock button_lock;

ISR(SWITCH_INT)
{
	uint8_t level = GpioRead(SWITCH);

	//pressed
	if (level == LOW)
	{
		button_lock.Lock();

		button_timestamp = task_get_ms_tick();
		button_state = BUTTON_WAIT;

		return;
	}

	//not pressed
	if (level == HIGH && button_state == BUTTON_WAIT)
	{
		button_lock.Unlock();

		uint32_t delta = task_get_ms_tick() - button_timestamp;

//		DEBUG("delta %lu\n", delta);

		button_state = BUTTON_IDLE;
		if (delta > BUTTON_SHORT_PRESS && delta <= BUTTON_LONG_PRESS)
		{
			button_state = BUTTON_SHORT;
//			DEBUG("Button short press");
		} else if (delta > BUTTON_LONG_PRESS)
		{
			button_state = BUTTON_LONG;
//			DEBUG("Button long press");
		}

		if (button_state != BUTTON_IDLE)
		{
			task_irqh(TASK_IRQ_BUTTON, (uint8_t *)&button_state);
			button_state = BUTTON_IDLE;
		}
	}
}

void button_init()
{
	GpioSetDirection(SWITCH, INPUT);
	GpioSetPull(SWITCH, gpio_pull_up);

	GpioSetInterrupt(SWITCH, gpio_interrupt0, gpio_bothedges);
}

void button_step()
{
//	DEBUG("button state %d %d %02X %02X\n", button_state, GpioRead(SWITCH), PORTA.INTCTRL, PORTA.PIN2CTRL);
	if (GpioRead(SWITCH) == LOW && button_state == BUTTON_WAIT)
	{
		uint32_t ts = task_get_ms_tick();
		uint32_t delta = ts - button_timestamp;

		if (delta < BUTTON_LONG_HOLD)
		{
			return;
		}

		button_state = BUTTON_HOLD;

		task_irqh(TASK_IRQ_BUTTON, (uint8_t *)&button_state);

		button_state = BUTTON_IDLE;

//		GpioSetInterrupt(SWITCH, gpio_interrupt0, gpio_falling);

//		DEBUG("button long hold");
	}
}


