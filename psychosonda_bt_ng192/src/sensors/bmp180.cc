/*
 * BMP180.cc
 *
 *  Created on: 29.11.2013
 *      Author: horinek
 */

#include "bmp180.h"

//first period
ISR(BMP_MEAS_TIMER_OVF_IRQ)
{
	//prolong next interrupt if this one is pending
	bmp180.meas_timer.SetValue(0 + 1);

	bmp180.ReadPressure();
	bmp180.StartTemperature();

	bmp180.CompensateTemperature();
	bmp180.CompensatePressure();

	task_irqh(TASK_IRQ_BARO, 0);
}

//second period
ISR(BMP_MEAS_TIMER_CMPA_IRQ)
{
	//prolong next interrupt if this one is pending
	bmp180.meas_timer.SetValue(125 * 5 + 1);

	bmp180.ReadTemperature();
	bmp180.StartPressure();
}

void BMP180::Start()
{
	if (this->settings.enabled)
		this->meas_timer.Start();
}

void BMP180::Stop()
{
	if (this->settings.enabled)
		this->meas_timer.Stop();
}

void BMP180::Init(I2c * i2c, struct bmp180_settings settings)
{
	this->i2c = i2c;
	this->settings = settings;

	if (!this->settings.enabled)
		return;

	this->press_osr = BMP180_OSR_HIGH;

	this->ReadPROM();

	BMP_MEAS_TIMER_PWR_ON;
	this->meas_timer.Init(BMP_MEAS_TIMER, timer_div256);

	if (this->settings.odr > 50)
		this->settings.odr = 50;

	if (this->settings.odr < 2)
		this->settings.odr = 2;

	uint16_t x = 1000 / this->settings.odr;

	this->meas_timer.SetTop(x * 125);
	this->meas_timer.SetCompare(timer_A, 125 * 5); //5ms - temp end min
	this->meas_timer.EnableInterrupts(timer_overflow | timer_compareA);
}

void BMP180::Deinit()
{
	BMP_MEAS_TIMER_PWR_OFF;
}

bool BMP180::SelfTest()
{
	this->i2c->Write(BMP180_CHIP_ID_REG);
	this->i2c->StartTransmittion(BMP180_ADDRESS, 1);
	this->i2c->Wait();

	uint8_t data = this->i2c->Read();

	if (data != BMP180_CHIP_ID)
	{
		return false;
	}

//	DEBUG1("AC1  %d", this->Calibration_AC1);
//	DEBUG1("AC2  %d", this->Calibration_AC2);
//	DEBUG1("AC3  %d", this->Calibration_AC3);
//	DEBUG1("AC4  %u", this->Calibration_AC4);
//	DEBUG1("AC5  %u", this->Calibration_AC5);
//	DEBUG1("AC6  %u", this->Calibration_AC6);
//	DEBUG1("B1   %d", this->Calibration_B1);
//	DEBUG1("B2   %d", this->Calibration_B2);
//	DEBUG1("MB   %d", this->Calibration_MB);
//	DEBUG1("MC   %d", this->Calibration_MC);
//	DEBUG1("MD   %d", this->Calibration_MD);

	return true;
}

uint16_t BMP180::Read16(uint8_t cmd)
{
	this->i2c->Write(cmd);
	this->i2c->StartTransmittion(BMP180_ADDRESS, 2);
	this->i2c->Wait();

	uint8_t a = this->i2c->Read();
	uint8_t b = this->i2c->Read();

	return (a << 8) | b;
}

uint32_t BMP180::Read24(uint8_t cmd)
{
	union {uint32_t val; uint8_t raw[4];} res;

	this->i2c->Write(cmd);
	this->i2c->StartTransmittion(BMP180_ADDRESS, 3);
	this->i2c->Wait();

	res.raw[2] = this->i2c->Read();
	res.raw[1] = this->i2c->Read();
	res.raw[0] = this->i2c->Read();

	return res.val;
}

void BMP180::ReadPROM()
{
//	mark();

    Calibration_AC1 = this->Read16(BMP180_PROM + 0);
    Calibration_AC2 = this->Read16(BMP180_PROM + 2);
    Calibration_AC3 = this->Read16(BMP180_PROM + 4);
    Calibration_AC4 = this->Read16(BMP180_PROM + 6);
    Calibration_AC5 = this->Read16(BMP180_PROM + 8);
    Calibration_AC6 = this->Read16(BMP180_PROM + 10);

    Calibration_B1 = this->Read16(BMP180_PROM + 12);
    Calibration_B2 = this->Read16(BMP180_PROM + 14);
    Calibration_MB = this->Read16(BMP180_PROM + 16);
    Calibration_MC = this->Read16(BMP180_PROM + 18);
    Calibration_MD = this->Read16(BMP180_PROM + 20);
}

void BMP180::Write(uint8_t address, uint8_t data)
{
	this->i2c->Write(address);
	this->i2c->Write(data);
	this->i2c->StartTransmittion(BMP180_ADDRESS, 0);
	this->i2c->Wait();
}

void BMP180::StartPressure()
{
	this->Write(BMP180_CONTROL, BMP180_PRESSURE_MEAS | this->press_osr << 6);
}

void BMP180::StartTemperature()
{
	this->Write(BMP180_CONTROL, BMP180_TEMPERATURE_MEAS | this->press_osr);
}

void BMP180::ReadPressure()
{
    this->raw_pressure = this->Read24(BMP180_READ) >> (8 - this->press_osr);
}

void BMP180::ReadTemperature()
{
	this->raw_temperature = this->Read16(BMP180_READ);
}

void BMP180::CompensateTemperature()
{
	int32_t x1 = ((this->raw_temperature - Calibration_AC6) * Calibration_AC5) >> 15;
	int32_t x2 = ((int64_t)Calibration_MC << 11) / (x1 + Calibration_MD);

    this->param_b5 = x1 + x2;

    this->temperature = (int16_t)((this->param_b5 + 8) >> 4) / 10.0;
}


void BMP180::CompensatePressure()
{
    int32_t b6 = this->param_b5 - 4000;
    int32_t x1 = (this->Calibration_B2 * (b6 * b6 >> 12)) >> 11;
    int32_t x2 = this->Calibration_AC2 * b6 >> 11;
    int32_t x3 = x1 + x2;
    int32_t b3 = ((((int32_t)this->Calibration_AC1 * 4 + x3) << this->press_osr) + 2) / 4;
    x1 = this->Calibration_AC3 * b6 >> 13;
    x2 = (this->Calibration_B1 * (b6 * b6 >> 12)) >> 16;
    x3 = ((x1 + x2) + 2) >> 2;
    int32_t b4 = (int32_t)this->Calibration_AC4 * (x3 + 32768) >> 15;
    uint32_t b7 = (((this->raw_pressure - b3)) * (50000 >> this->press_osr));

    int32_t p;

    if (b7 < 0x80000000)
		p = ((b7 * 2) / b4);
    else
        p = ((b7 / b4) * 2);

    x1 = (p >> 8) * (p >> 8);
    x1 = (x1 * 3038) >> 16;
    x2 = (-7357 * p) >> 16;

    p = p + ((x1 + x2 + 3791) >> 4);

    this->pressure = p;
}

float BMP180::GetAltitude(float currentSeaLevelPressureInPa, float pressure)
{
    // Calculate altitude from sea level.
    float altitude = 44330.0 * (1.0 - pow(pressure / currentSeaLevelPressureInPa, 0.1902949571836346));
    return altitude;
}
