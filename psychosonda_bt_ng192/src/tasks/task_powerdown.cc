#include "task_powerdown.h"

extern SleepLock powerdown_lock;
extern Usart uart;

volatile bool powerdown_loop_break = false;

void task_powerdown_init()
{
	buzzer_beep(_100ms * 1, _100ms * 2, _100ms * 1, _100ms * 2, _100ms * 1);
	while(!buzzer_is_silent());

	//Lower F_CPU
	ClockSetSource(x2MHz);
	//disable other oscillators
	OSC.CTRL = 0b00000001;

	uart_stop();
	_delay_ms(10);

	turnoff_subsystems();

	uart_low_speed();
	_delay_ms(10);

	DEBUG(" *** POWER DOWN INIT ***\n");

	//we do not want to enter sleep
	powerdown_loop_break = false;
	powerdown_lock.Lock();

	task_timer_setup(false);

	SD_EN_OFF;

	DEBUG("Using low speed uart\n");
}


void task_powerdown_stop()
{
	//Reinit all devices
	DEBUG("Restarting all devices\n");

	uart_stop();
	Setup();
	task_timer_setup();
	ewdt_init();
	DEBUG("Restoring full speed uart\n");

	powerdown_lock.Unlock();
}


extern bool time_rtc_irq;

void powerdown_sleep()
{
	_delay_ms(31);
	task_timer_stop();
	do
	{
		ewdt_reset();

		//allow rtc irq handler but do not wake up
		time_rtc_irq = false;

		//wake up device on alarm
		if (time_for_wake_up())
		{
			powerdown_loop_break = true;
			task_set(TASK_BT_SLAVE);
			break;
		}

		//rtc irq set time_rtc_irq to true if executed
		SystemPowerSave();
	} while (time_rtc_irq == true);

	task_timer_setup(false);
}

extern uint8_t task_sleep_lock;

void task_powerdown_loop()
{
	//do not go to the Power down when there is another lock pending (button, buzzer)
	if ((task_sleep_lock == 1 && powerdown_lock.Active()) && powerdown_loop_break == false)
	{
		uart_stop();

		powerdown_sleep();

		uart_low_speed();
	}
}

void task_powerdown_irqh(uint8_t type, uint8_t * buff)
{
	switch(type)
	{
	case(TASK_IRQ_BUTTON):
		if (*buff == BUTTON_HOLD)
		{
			powerdown_loop_break = true;
			task_set(TASK_BT_SLAVE);
		}
	break;

	case(TASK_IRQ_USB):
		uint8_t state = *buff;

		DEBUG("USB IRQ %d\n", state);
		if (state == 1)
		{
			powerdown_loop_break = true;
			task_set(TASK_USB);
		}
	break;
	}
}
