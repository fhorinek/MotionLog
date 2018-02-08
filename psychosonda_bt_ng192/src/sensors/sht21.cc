/*
 * sht21.cc
 *
 *  Created on: 29.11.2013
 *      Author: horinek
 */

#include "sht21.h"

//first period
ISR(SHT_MEAS_TIMER_OVF_IRQ)
{
	//prolong next interrupt if this one is pending
	sht21.meas_timer.SetValue(0 + 1);

	if (sht21.settings.temp_enabled)
		sht21.StartTemperature();
	else
		sht21.StartHumidity();
}

//second period
ISR(SHT_MEAS_TIMER_CMPA_IRQ)
{
	//prolong next interrupt if this one is pending
	sht21.meas_timer.SetValue(SHT21_PERIOD / 3 + 1);

	sht21.Read();
	sht21.CompensateTemperature();
	task_irqh(TASK_IRQ_TEMPERATURE, 0);

	if (sht21.settings.rh_enabled)
		sht21.StartHumidity();

}

//thirt period
ISR(SHT_MEAS_TIMER_CMPB_IRQ)
{
	//prolong next interrupt if this one is pending
	sht21.meas_timer.SetValue((SHT21_PERIOD / 3) * 2 + 1);

	sht21.Read();
	sht21.CompensateHumidity();
	task_irqh(TASK_IRQ_HUMIDITY, 0);
}


void SHT21::Start()
{
	if (this->settings.rh_enabled || this->settings.temp_enabled)
		this->meas_timer.Start();
}

void SHT21::Stop()
{
	if (this->settings.rh_enabled || this->settings.temp_enabled)
		this->meas_timer.Stop();
}

void SHT21::Init(I2c * i2c, struct sht21_settings settings)
{
	this->i2c = i2c;
	this->settings = settings;

	if (!(this->settings.temp_enabled || this->settings.rh_enabled))
		return;

	this->Reset();

	SHT_MEAS_TIMER_PWR_ON;
	this->meas_timer.Init(SHT_MEAS_TIMER, timer_div1024);

	this->meas_timer.SetTop(SHT21_PERIOD); // 1 sec there is no point in faster odr
	this->meas_timer.SetValue(SHT21_PERIOD - 1);
	this->meas_timer.SetCompare(timer_A, SHT21_PERIOD / 3);
	this->meas_timer.SetCompare(timer_B, (SHT21_PERIOD / 3) * 2);

	//temperature
	this->meas_timer.EnableInterrupts(timer_overflow);
	//humidity
	if (this->settings.temp_enabled)
		this->meas_timer.EnableInterrupts(timer_compareA);
	if (this->settings.rh_enabled)
		this->meas_timer.EnableInterrupts(timer_compareB);

}

void SHT21::Reset()
{
	this->Write(SHT21_RESET);
}

void SHT21::Deinit()
{
	SHT_MEAS_TIMER_PWR_OFF;
}

bool SHT21::SelfTest()
{
	this->i2c->StartTransmittion(SHT21_ADDRESS, 0);
	this->i2c->Wait();

	if (this->i2c->Error())
	{
		return false;
	}

	return true;
}

void SHT21::Write(uint8_t cmd)
{
	this->i2c->Write(cmd);
	this->i2c->StartTransmittion(SHT21_ADDRESS, 0);
	this->i2c->Wait();
}

void SHT21::StartHumidity()
{
	this->Write(SHT21_MEASURE_RH);
}

void SHT21::StartTemperature()
{
	this->Write(SHT21_MEASURE_T);
}

bool SHT21::Read()
{
	this->i2c->StartTransmittion(SHT21_ADDRESS, 2);
	this->i2c->Wait();
	if (this->i2c->Error())
		return false;

    byte2_u data;

    data.bytes[1] = this->i2c->Read();
    data.bytes[0] = this->i2c->Read();

    DEBUG(">>%02X %02X\n", data.bytes[0], data.bytes[1]);

    bool humidity = (data.bytes[0] & 0b00000010);

    data.bytes[0] &= 0b11111100;

    if (humidity)
    	this->raw_humidity = data.uint16;
    else
    	this->raw_temperature = data.uint16;

    return true;
}

void SHT21::CompensateTemperature()
{
	float tmp = -46.85 + 175.72 * ((float)this->raw_temperature / 65536.0);
	this->temperature = tmp * 10;
}


void SHT21::CompensateHumidity()
{
	this->humidity = -6.0 + 125.0/655.36 * (float)this->raw_humidity;
}

