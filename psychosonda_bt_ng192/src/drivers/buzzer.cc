#include "buzzer.h"

volatile uint16_t timer_duration_sound[3];
volatile uint16_t timer_duration_delay[2];
volatile uint8_t buzzer_timer_pos = 0;

volatile bool buzzer_mute = 0;

Timer buzzer_timer;

void buzzer_set_mute(bool set)
{
	buzzer_mute = set;
}

bool buzzer_is_silent()
{
	return buzzer_timer_pos == 0;
}

void buzzer_init()
{
	BUZZER_TIMER_PWR_ON;

	//buzzer
	buzzer_timer.Init(BUZZER_TIMER, timer_div1024);
	buzzer_timer.SetMode(timer_pwm);
	buzzer_timer.EnableOutputs(timer_D);
	buzzer_timer.SetTop(24);
	buzzer_timer.SetCompare(timer_D, 12);
	buzzer_timer.EnableInterrupts(timer_overflow);
}

void buzzer_beep(uint16_t s1)
{
	buzzer_beep(s1, 0, 0, 0, 0);
}

void buzzer_beep(uint16_t s1, uint16_t d1, uint16_t s2)
{
	buzzer_beep(s1, d1, s2, 0, 0);
}

void buzzer_beep(uint16_t s1, uint16_t d1, uint16_t s2, uint16_t d2, uint16_t s3)
{
	if (buzzer_mute)
		return;

	buzzer_timer_pos = 1;

	timer_duration_sound[0] = s1;
	timer_duration_sound[1] = s2;
	timer_duration_sound[2] = s3;

	timer_duration_delay[0] = d1;
	timer_duration_delay[1] = d2;

	buzzer_timer.SetValue(0);
	buzzer_timer.EnableOutputs(timer_D);
	buzzer_timer.Start();
}

ISR(BUZZER_TIMER_OVF)
{
	if (buzzer_timer_pos == 6)
	{
		buzzer_timer.Stop();
		buzzer_timer_pos = 0;
		return;
	}

	if (buzzer_timer_pos % 2 == 1)
	{
		if (timer_duration_sound[buzzer_timer_pos / 2] == 0)
		{
			buzzer_timer_pos++;
			buzzer_timer.DisableOutputs(timer_D);
		}
		else
			timer_duration_sound[buzzer_timer_pos / 2] --;
	}
	else
	{
		if (timer_duration_delay[buzzer_timer_pos / 2 - 1] == 0)
		{
			buzzer_timer_pos++;
			buzzer_timer.EnableOutputs(timer_D);
		}
		else
			timer_duration_delay[buzzer_timer_pos / 2 - 1] --;
	}
}
