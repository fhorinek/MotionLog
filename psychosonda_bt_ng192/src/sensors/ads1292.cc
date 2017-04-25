/*
 * ads1292.cc
 *
 *  Created on: 30.1.2014
 *      Author: horinek
 */

#include "ads1292.h"

ISR(ADS_DRDY_PIN_INT)
{
	task_irqh(TASK_IRQ_ADS, NULL);
}

void Ads1292::Init(struct ads1292_settings settings)
{
	if (!settings.enabled)
		return;

	ADS_SPI_PWR_ON;

	GpioWrite(ADS_EN_PIN, LOW);
	GpioWrite(ADS_RESET_PIN, LOW);
	GpioWrite(ADS_START_PIN, LOW);
	GpioWrite(ADS_CS_PIN, LOW);

	GpioSetDirection(ADS_EN_PIN, OUTPUT);

	GpioSetDirection(ADS_RESET_PIN, OUTPUT);
	GpioSetDirection(ADS_START_PIN, OUTPUT);
	GpioSetDirection(ADS_DRDY_PIN, INPUT);
	GpioSetDirection(ADS_CS_PIN, OUTPUT);

	GpioWrite(ADS_EN_PIN, HIGH);
	GpioWrite(ADS_CS_PIN, HIGH);

	GpioSetInterrupt(ADS_DRDY_PIN, gpio_interrupt1, gpio_falling, MEDIUM);

	_delay_ms(500);

	this->value_ch1 = 0;
	this->value_ch2 = 0;

	this->usart_spi.Init(ADS_SPI_USART, 500000ul);
	this->usart_spi.BecomeSPI(1, MSB, 2000000ul);

	this->Set(settings);
}

void Ads1292::Deinit()
{
	if (!settings.enabled)
		return;

	GpioSetInterrupt(ADS_DRDY_PIN, gpio_clear);

	GpioSetDirection(ADS_EN_PIN, INPUT);

	GpioSetDirection(ADS_RESET_PIN, INPUT);
	GpioSetDirection(ADS_START_PIN, INPUT);
	GpioSetDirection(ADS_CS_PIN, INPUT);

	this->usart_spi.Stop();
	ADS_SPI_PWR_OFF;

	this->settings.enabled = false;
}

uint8_t Ads1292::ReadRegister(uint8_t reg)
{
	uint8_t result;

	GpioWrite(ADS_CS_PIN, LOW);

	_delay_us(10); //>4 tclk (8us = 4x2us @ 512 kHz)

	reg &= 0b00011111;
	this->usart_spi.SendRaw(RREG | reg);
	_delay_us(10);
	this->usart_spi.SendRaw(0x00);
	_delay_us(10);
	result = this->usart_spi.SendRaw(0x00);

	_delay_us(10);

	GpioWrite(ADS_CS_PIN, HIGH);

	return result;
}

void Ads1292::WriteRegister(uint8_t reg, uint8_t data)
{
	if (reg == 0) return;

	GpioWrite(ADS_CS_PIN, LOW);

	_delay_us(10); //>4 tclk (8us = 4x2us @ 512 kHz)

	reg &= 0b00011111;

	this->usart_spi.SendRaw(WREG | reg);
	_delay_us(10);
	this->usart_spi.SendRaw(0x00);
	_delay_us(10);
	this->usart_spi.SendRaw(data);

	_delay_us(10);

	GpioWrite(ADS_CS_PIN, HIGH);
}

void Ads1292::SendCommand(uint8_t cmd)
{
	GpioWrite(ADS_CS_PIN, LOW);

	_delay_us(10); // 5 = 6us at 32

	this->usart_spi.SendRaw(cmd);

	_delay_us(10);

	GpioWrite(ADS_CS_PIN, HIGH);
}

bool Ads1292::SelfTest()
{
	if (!settings.enabled)
		return false;

	this->SendCommand(SDATAC);

	uint8_t id = this->ReadRegister(ID_REG);

//	DEBUG1("ADS ID is (%02X) ", id);

	if (id != 0x73)
	{
		return false;
	}

	return true;
}

void Ads1292::CfgTest()
{
	if (!settings.enabled)
		return;
//	test signal
    this->SendCommand(SDATAC);

    this->WriteRegister(CONFIG1_REG, SAMP_125_SPS);
    this->WriteRegister(CONFIG2_REG, CONFIG2_const | PDB_REFBUF | INT_TEST | TEST_FREQ); // Enable reference buffer - 0xA0
    _delay_ms(100); // internal reference start-up time
    this->WriteRegister(CH1SET_REG, CH1SET_const | GAIN_1 | TEST_SIGNAL);
    this->WriteRegister(CH2SET_REG, CH2SET_const | GAIN_1 | TEST_SIGNAL);
}

void Ads1292::CfgTest2()
{
	if (!settings.enabled)
		return;

//	test signal
    this->SendCommand(SDATAC);

    this->WriteRegister(CONFIG1_REG, SAMP_4_KSPS);
    this->WriteRegister(CONFIG2_REG, CONFIG2_const | PDB_REFBUF | INT_TEST | TEST_FREQ); // Enable reference buffer - 0xA0
    _delay_ms(100); // internal reference start-up time
    this->WriteRegister(CH1SET_REG, CH1SET_const | GAIN_1 | NORMAL);
    this->WriteRegister(CH2SET_REG, CH2SET_const | GAIN_1 | NORMAL);

}

void Ads1292::Start()
{
	if (!this->settings.enabled)
		return;

    this->SendCommand(RDATAC);
    GpioWrite(ADS_START_PIN, HIGH);
}

void Ads1292::Stop()
{
	if (!this->settings.enabled)
		return;

    GpioWrite(ADS_START_PIN, LOW);
    this->SendCommand(SDATAC);
}

void Ads1292::Reset()
{
	if (!settings.enabled)
		return;

//	GpioWrite(ADS_VDD_PIN, HIGH);
	_delay_ms(100);
	GpioWrite(ADS_RESET_PIN, HIGH);
	_delay_ms(40); //34 minimum
	GpioWrite(ADS_RESET_PIN, LOW);
	_delay_us(20); //8 minimum
	GpioWrite(ADS_RESET_PIN, HIGH);
	_delay_us(50); //2us * 18 = 36 minimum

	this->SendCommand(SDATAC);
}

void Ads1292::ReadData()
{
	if (!settings.enabled)
		return;

//    this->spi.SetSlave(ADS_CS_PIN);
    GpioWrite(ADS_CS_PIN, LOW);
    _delay_us(10); //datasheet wait value 4tscl

    // Status
    status = this->usart_spi.SendRaw(0x00);

    status <<= 8;
    status = this->usart_spi.SendRaw(0x00);

    this->usart_spi.SendRaw(0x00);




    // Get 3 Bytes of measured data channel 1
    this->value_ch1 = this->usart_spi.SendRaw(0x00);

    this->value_ch1 <<= 8;
    this->value_ch1 += this->usart_spi.SendRaw(0x00);

    this->value_ch1 <<= 8;
    this->value_ch1 += this->usart_spi.SendRaw(0x00);


    // Get 3 Bytes of measured data channel 2
    this->value_ch2 = this->usart_spi.SendRaw(0x00);

	this->value_ch2 <<= 8;
    this->value_ch2 += this->usart_spi.SendRaw(0x00);

	this->value_ch2 <<= 8;
	this->value_ch2 += this->usart_spi.SendRaw(0x00);

//    convert complement 2
//    this->value_ch1 = to_dec_3(this->value_ch1);
//    this->value_ch2 = to_dec_3(this->value_ch2);
//
//    this->value_ch1 = 16777215;
//    this->value_ch2 = -16777216;

//    this->spi.UnsetSlave();
    GpioWrite(ADS_CS_PIN, HIGH);

    //set status
    this->rld = (status & 0b0000100000000000) ? true : false;
    this->n2  = (status & 0b0000010000000000) ? true : false;
    this->p2  = (status & 0b0000001000000000) ? true : false;
    this->n1  = (status & 0b0000000100000000) ? true : false;
    this->p1  = (status & 0b0000000010000000) ? true : false;
}

uint16_t Ads1292::SetODR(uint16_t odr)
{
	switch(odr)
	{
	case(125):
		ads1292.WriteRegister(CONFIG1_REG, SAMP_125_SPS);
	break;
	case(250):
		ads1292.WriteRegister(CONFIG1_REG, SAMP_250_SPS);
	break;
	case(500):
		ads1292.WriteRegister(CONFIG1_REG, SAMP_500_SPS);
	break;
	case(1000):
		ads1292.WriteRegister(CONFIG1_REG, SAMP_1_KSPS);
	break;
	case(2000):
		ads1292.WriteRegister(CONFIG1_REG, SAMP_2_KSPS);
	break;
	case(4000):
		ads1292.WriteRegister(CONFIG1_REG, SAMP_4_KSPS);
	break;
	case(8000):
		ads1292.WriteRegister(CONFIG1_REG, SAMP_8_KSPS);
	break;
	default:
		ads1292.WriteRegister(CONFIG1_REG, SAMP_125_SPS);
		odr = 125;
	}

	return odr;
}

uint8_t Ads1292::SetGain(uint8_t ch, uint8_t gain)
{
	uint8_t reg, val;

	reg = (ch == 1) ? CH1SET_REG : CH2SET_REG;
	val = CH1SET_const | NORMAL;

	switch(gain)
	{
	case(1):
		val |= GAIN_1;
		break;
	case(2):
		val |= GAIN_2;
		break;
	case(3):
		val |= GAIN_3;
		break;
	case(4):
		val |= GAIN_4;
		break;
	case(6):
		val |= GAIN_6;
		break;
	case(8):
		val |= GAIN_8;
		break;
	case(12):
		val |= GAIN_12;
		break;
	default:
		val |= GAIN_1;
		gain = 1;
	}

	ads1292.WriteRegister(reg, val);
	return gain;
}

void Ads1292::EnableChannelTestSignal(uint8_t ch)
{
	uint8_t channelRegister = (ch == 1) ? CH1SET_REG : CH2SET_REG;
	uint8_t channelConfig;

	this->SendCommand(SDATAC);
	this->WriteRegister(CONFIG2_REG, INT_TEST_1HZ);

	channelConfig = this->ReadRegister(channelRegister) & 0b11110000; // Clear input selection
	channelConfig |= TEST_SIGNAL;

	_delay_ms(100); // internal reference start-up time
	this->WriteRegister(channelRegister, channelConfig);
}

void Ads1292::SetChannelSource(uint8_t ch, enum SIGNAL_source source)
{
	switch(source)
	{
	case TEST_SOURCE:
		EnableChannelTestSignal(ch);
		break;
	case BIO_SOURCE:
		// Do nothing. We do not need to set up anything else.
		break;
	}
}

void Ads1292::SetResp()
{
	uint8_t resp1 = 0b00000010;
	uint8_t resp2 = 0b00000011;

	if (this->settings.resp_enabled)
		resp1 |= 0b11000000;

	uint8_t resp_ph = this->settings.resp_freq / 11;

	if (resp_ph > 15)
		resp_ph = 15;

	if (this->settings.resp_freq == 64)
		resp2 |= 0b00000100;
		resp_ph /= 2;

	resp1 |= resp_ph << 2;

	this->WriteRegister(RESP1_REG, resp1);
	this->WriteRegister(RESP2_REG, resp2);
}

void Ads1292::Set(struct ads1292_settings settings)
{
//	DEBUG("SET\n");

	this->settings = settings;

	if (!this->settings.enabled)
		return;

//	DEBUG("reset\n");

	this->Reset();

	this->settings.odr = this->SetODR(this->settings.odr);
	this->settings.ch1_gain = this->SetGain(1, this->settings.ch1_gain);
	this->settings.ch2_gain = this->SetGain(2, this->settings.ch2_gain);
	this->SetChannelSource(1, (SIGNAL_source)this->settings.ch1_source);
	this->SetChannelSource(2, (SIGNAL_source)this->settings.ch2_source);

	this->SetResp();

	//add mandatory bits
	uint8_t tmp = this->ReadRegister(CONFIG2_REG);
	this->WriteRegister(CONFIG2_REG, PDB_REFBUF | tmp);

	tmp = this->ReadRegister(LOFF_REG);
	this->WriteRegister(LOFF_REG, LOFF_const | tmp);

	tmp = this->ReadRegister(RESP1_REG);
	this->WriteRegister(RESP1_REG, RESP1_const | tmp);

	tmp = this->ReadRegister(RESP2_REG);
	this->WriteRegister(RESP2_REG, RESP2_const | tmp);


//	for (uint8_t i = 0x00; i <= 0x0B; i++)
//	{
//		uint8_t val = this->ReadRegister(i);
//		DEBUG("%02X %02X  ", i, val);
//		for (uint8_t j = 0; j < 8; j++)
//		{
//			DEBUG("%d", (val & (1 << (7 - j))) >> (7 - j));
//			if (j == 3)
//				DEBUG(" ");
//		}
//		DEBUG("\n");
//	}
}
