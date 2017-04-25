/*
 * devices.cc
 *
 *  Created on: 30.7.2014
 *      Author: horinek
 */

#include "devices.h"


Ads1292 ads1292;

I2c mems_i2c;

BMP180 bmp180;
Lsm303d lsm303d;
L3gd20 l3gd20;
SHT21 sht21;

struct ads1292_settings ads1292_settings;
struct l3gd20_settings l3gd20_settings;
struct lsm303d_settings lsm303d_settings;
struct bmp180_settings bmp180_settings;
struct sht21_settings sht21_settings;

int32_t to_dec_3(int64_t c)
{
	if (c < (int64_t)0x800000)
		return c;
	return (int64_t)c - (int64_t)0x1000000;
}

int16_t to_dec_2(int32_t c)
{
	if (c < (int32_t)0xFF)
		return c;
	return (int32_t)c - (int32_t)0x100;
}

int8_t to_dec_1(int8_t c)
{
	if (c<32) return c;
	return c - 64;
}

ISR(MEMS0_INT)
{
	if (lsm303d.INT1())
	{
		task_irqh(TASK_IRQ_MAG, 0);
	}

	if (l3gd20.INT2())
	{
		task_irqh(TASK_IRQ_GYRO, 0);
	}
}

ISR(MEMS1_INT)
{
	if (lsm303d.INT2())
	{
		task_irqh(TASK_IRQ_ACC, 0);
	}
}
