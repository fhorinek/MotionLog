/*
 * tasks.h
 *
 *  Created on: 24.7.2014
 *      Author: horinek
 */

#ifndef TASKS_H_
#define TASKS_H_

#include "../psychosonda.h"

#include "task_usb/task_usb.h"
#include "task_powerdown.h"
#include "task_bt_slave.h"

#define TASK_POWERDOWN			0
#define TASK_USB				1
#define TASK_BT_SLAVE			2

#define NO_TASK					0xFF

#define TASK_IRQ_BT				0
#define TASK_IRQ_ADS			1
#define TASK_IRQ_BUTTON			2
#define TASK_IRQ_ACC			3
#define TASK_IRQ_MAG			4
#define TASK_IRQ_GYRO			5
#define TASK_IRQ_BARO			6
#define TASK_IRQ_USB			7
#define TASK_IRQ_BAT			8
#define TASK_IRQ_TEMPERATURE	9
#define TASK_IRQ_HUMIDITY		10

class SleepLock
{
private:
	volatile bool active;
public:
	SleepLock();
	void Lock();
	void Unlock();
	bool Active();

};

void task_timer_setup(bool full_speed = true);
void task_timer_stop();

void task_init();
void task_set(uint8_t task);
void task_rgui();

uint32_t task_get_ms_tick();

void task_loop();
void task_system_loop();
void task_sleep();
void task_irqh(uint8_t type, uint8_t * buff);

#endif /* TASKS_H_ */
