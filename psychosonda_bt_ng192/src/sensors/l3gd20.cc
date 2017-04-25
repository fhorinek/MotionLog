/*
 * L3gd20.cc
 *
 *  Created on: 1.8.2014
 *      Author: horinek
 */

#include "l3gd20.h"

bool L3gd20::INT1()
{
	return (GpioRead(GY_INT1) == LOW);
}

bool L3gd20::INT2()
{
	return (GpioRead(GY_INT2) == HIGH);
}

void L3gd20::Init(I2c *i2c, struct l3gd20_settings settings)
{
	if (!settings.enabled)
		return;

	this->i2c = i2c;

	GpioSetDirection(GY_INT1, INPUT);
	GpioSetDirection(GY_INT2, INPUT);

	this->Set(settings);
}

void L3gd20::Deinit()
{
	if (!settings.enabled)
		return;

	this->Stop();

	GpioSetInterrupt(GY_INT2, gpio_clear);

	this->settings.enabled = false;
}

void L3gd20::Write(uint8_t adr, uint8_t data)
{
	this->i2c->Wait();

	this->i2c->Write(adr);
	this->i2c->Write(data);
	this->i2c->StartTransmittion(L3G_ADDRESS, 0);
}

uint8_t L3gd20::Read(uint8_t adr)
{
	this->i2c->Wait();

	this->i2c->Write(adr);
	this->i2c->StartTransmittion(L3G_ADDRESS, 1);
	this->i2c->Wait();

	return this->i2c->Read();
}

uint16_t L3gd20::Read16(uint8_t adr)
{
	this->i2c->Wait();

	this->i2c->Write(adr | 0b10000000); // + auto increment
	this->i2c->StartTransmittion(L3G_ADDRESS, 2);
	this->i2c->Wait();

	uint16_t tmp = this->i2c->Read() << 8;
	tmp |= this->i2c->Read();

	return tmp;
}

void L3gd20::WriteOr(uint8_t adr, uint8_t data)
{
	uint8_t tmp = this->Read(adr);

	this->Write(adr, tmp | data);
}

void L3gd20::WriteAnd(uint8_t adr, uint8_t data)
{
	uint8_t tmp = this->Read(adr);

	this->Write(adr, tmp & data);
}

void L3gd20::Reset()
{
	if (!this->settings.enabled)
		return;

	this->Write(0x24, 0b10000000); //CTRL_REG5 <- BOOT

	_delay_ms(1);

	this->Write(0x24, 0b00000000); //CTRL_REG5 -> BOOT
}

bool L3gd20::SelfTest()
{
	if (!this->settings.enabled)
		return false;

	uint8_t id = this->Read(0x0F);

	return (id == L3G_ID);
}

void L3gd20::EnableGyro(l3g_odr odr, l3g_scale scale, l3g_bw bw)
{
	if (!this->settings.enabled)
		return;

	this->Write(0x20, 0b00001111 | odr | bw); //CTRL_REG1 <- DR | BW
	this->WriteOr(0x23, scale); //CTRL_REG4 -< scale
}

void L3gd20::EnableGyroFIFO(uint8_t thold)
{
	if (!this->settings.enabled)
		return;

	//enable FIFO
	this->WriteOr(0x24, 0b01000000); //CTRL_REG5 <- FIFO_EN
	//FIFO watermark on INT2
	this->WriteOr(0x22, 0b00000100); //CTRL_REG3 <-  I2_WTM
	//FIFO mode to Stream mode
	this->Write(0x2E, 0b01000000 | (thold & 0b00011111)); //FIFO_CTRL_REG
	DEBUG("%02X\n", this->Read(0x2E));

//	this->WriteOr(0x22, 0b00001000); //CTRL_REG3 <-  I2_WTM

	GpioSetInterrupt(GY_INT2, gpio_interrupt1, gpio_rising);
}

void L3gd20::Start()
{
	if (!this->settings.enabled)
		return;

	uint8_t odrValue = this->GetODRvalue(&this->settings);
	uint8_t scaleValue = this->GetScaleValue(&this->settings);
	uint8_t bwValue = this->GetBWvalue(&this->settings);

	this->EnableGyro((l3g_odr)odrValue, (l3g_scale)scaleValue, (l3g_bw)bwValue);
}

void L3gd20::Stop()
{
	if (!this->settings.enabled)
		return;

	this->Write(0x20, 0b00000000); //CTRL1
}

void L3gd20::ReadGyro(int16_t * x, int16_t * y, int16_t * z)
{
	if (!this->settings.enabled)
		return;

	this->i2c->Wait();
	this->i2c->Write(0x28 | 0b10000000);
	this->i2c->StartTransmittion(L3G_ADDRESS, 6);

	byte2_u tmp;
	//this->i2c->Wait();

	tmp.bytes[0] = this->i2c->Read();
	tmp.bytes[1] = this->i2c->Read();
	*x = tmp.int16;

	tmp.bytes[0] = this->i2c->Read();
	tmp.bytes[1] = this->i2c->Read();
	*y = tmp.int16;

	tmp.bytes[0] = this->i2c->Read();
	tmp.bytes[1] = this->i2c->Read();
	*z = tmp.int16;

//	//transfer test
//	*x = -3000;
//	*y = 3000;
//	*z = 0;
}

uint8_t L3gd20::GyroStreamLen()
{
	return this->Read(0x2F) & 0b00011111;
}

uint8_t L3gd20::ReadGyroStream(int16_t * buff, uint8_t len)
{
	if (!this->settings.enabled)
		return 0;


	this->i2c->Wait();
	this->i2c->Write(0x28 | 0b10000000);
	this->i2c->StartTransmittion(L3G_ADDRESS, 6 * len);

	int16_t x, y, z;
	byte2_u tmp;

	this->i2c->Wait();

	for(uint8_t i = 0; i < len; i++)
	{
		tmp.bytes[0] = this->i2c->Read();
		tmp.bytes[1] = this->i2c->Read();
		x = tmp.int16;

		tmp.bytes[0] = this->i2c->Read();
		tmp.bytes[1] = this->i2c->Read();
		y = tmp.int16;

		tmp.bytes[0] = this->i2c->Read();
		tmp.bytes[1] = this->i2c->Read();
		z = tmp.int16;

		buff[i * 3 + 0] = x;
		buff[i * 3 + 1] = y;
		buff[i * 3 + 2] = z;
	}

	return len;
}

int8_t L3gd20::ReadTemp()
{
	if (!this->settings.enabled)
		return 0xFF;

	return to_dec_1(this->Read(0x26)); // TEMP_OUT
}

uint8_t L3gd20::GetODRvalue(struct l3gd20_settings *settings)
{
	switch(settings->odr)
	{
	case(95):
		return  l3g_95Hz;
	case(190):
		return l3g_190Hz;
	case(380):
		return l3g_380Hz;
	case(760):
		return l3g_760Hz;
	default:
		settings->odr = 95;
		return l3g_95Hz;
	}
}

uint8_t L3gd20::GetScaleValue(struct l3gd20_settings *settings)
{
	switch(settings->scale)
	{
	case(250):
		return l3g_250dps;
	case(500):
		return l3g_500dps;
	case(2000):
		return l3g_2000dps;
	default:
		settings->scale = 250;
		return l3g_250dps;
	}
}

uint8_t L3gd20::GetBWvalue(struct l3gd20_settings *settings)
{
	switch(settings->bw)
	{
	case(12):
		return l3g_12Hz;
	case(25):
		return l3g_25Hz;
	case(50):
		return l3g_50Hz;
	case(70):
		return l3g_70Hz;
	case(100):
		return l3g_100Hz;
	default:
		settings->bw = 12;
		return l3g_12Hz;
	}
}

void L3gd20::Set(struct l3gd20_settings settings)
{
	this->settings = settings;

	if (!this->settings.enabled)
			return;

	this->Reset();

	this->EnableGyroFIFO(L3GD20_FIFO_TRESHOLD);
}
