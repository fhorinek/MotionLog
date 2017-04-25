/*
 * ads1292.h
 *
 *  Created on: 30.1.2014
 *      Author: horinek
 */

#ifndef ADS1292_H_
#define ADS1292_H_

class Ads1292;

#include "../psychosonda.h"


//common reg


struct ads1292_settings
{
	bool enabled;

	uint16_t  odr;

	uint8_t ch1_gain;
	uint8_t ch1_source;
	uint8_t ch2_gain;
	uint8_t ch2_source;

	bool resp_enabled;
	uint8_t resp_phase;
	uint8_t resp_freq;
};

enum spi_command {
	// system commands
	WAKEUP = 0x02,
	STANDBY = 0x04,
	RESET = 0x06,
	START = 0x08,
	STOP = 0x0a,

	// read commands
	RDATAC = 0x10,
	SDATAC = 0x11,
	RDATA = 0x12,

	// register commands
	RREG = 0x20, // 001x xxxx
	WREG = 0x40  // 010x xxxx
};

enum reg {
	// device settings
	ID_REG = 0x00,

	// global settings
	CONFIG1_REG = 0x01,
	CONFIG2_REG = 0x02,
	LOFF_REG = 0x03,

	// channel specific settings
   	CH1SET_REG = 0x04,
	CH2SET_REG = 0x05,

	// lead off status
	RLD_SENS_REG = 0x06,
	LOFF_SENS_REG = 0x07,
	LOFF_STAT_REG = 0x08,

	// other
	RESP1_REG = 0x09,
	RESP2_REG = 0x0A,
	GPIO_REG = 0x0B
};

enum ID_bits {
	DEV_ID7 = 0x80,
	DEV_ID6 = 0x40,
	DEV_ID5 = 0x20,
	DEV_ID2 = 0x04,
	DEV_ID1 = 0x02,
	DEV_ID0 = 0x01,

	ID_const = 0x10,
	ID_ADS129x = DEV_ID6,
	ID_ADS1292R = (DEV_ID6 | DEV_ID5),

	ID_4CHAN = 0,
	ID_6CHAN = DEV_ID0,
	ID_8CHAN = DEV_ID1,

	ID_ADS1292 = (ID_ADS129x | DEV_ID0)
};

enum CONFIG1_bits {
	SINGLE_SHOT = 0x80,
	DR2 = 0x04,
	DR1 = 0x02,
	DR0 = 0x01,

	CONFIG1_const = 0x00,
	SAMP_125_SPS = CONFIG1_const,
	SAMP_250_SPS = (CONFIG1_const | DR0),
	SAMP_500_SPS = (CONFIG1_const | DR1),
	SAMP_1_KSPS = (CONFIG1_const | DR1 | DR0),
	SAMP_2_KSPS = (CONFIG1_const | DR2),
	SAMP_4_KSPS = (CONFIG1_const | DR2 | DR0),
	SAMP_8_KSPS = (CONFIG1_const | DR2 | DR1)

};

enum CONFIG2_bits {
	PDB_LOFF_COMP = 0x40,
        PDB_REFBUF = 0x20,
        VREF_4V = 0x10,
        CLK_EN = 0x08,
	INT_TEST = 0x02, //amplitude = ±(VREFP – VREFN) / 2400
	TEST_FREQ = 0x01,

	CONFIG2_const = 0x80,
	INT_TEST_1HZ = (CONFIG2_const | INT_TEST | TEST_FREQ),
	INT_TEST_DC = (CONFIG2_const | INT_TEST)
};

enum LOFF_bits {
	COMP_TH2 = 0x80,
	COMP_TH1 = 0x40,
	COMP_TH0 = 0x20,
	VLEAD_OFF_EN = 0x10,
	ILEAD_OFF1 = 0x08,
	ILEAD_OFF0 = 0x04,
	FLEAD_OFF1 = 0x02,
	FLEAD_OFF0 = 0x01,

	LOFF_const = 0x10,

	COMP_TH_95 = 0x00,
	COMP_TH_92 = 0x20,
	COMP_TH_90 = 0x40,
	COMP_TH_87 = 0x60,
	COMP_TH_85 = 0x80,
	COMP_TH_80 = 0xA0,
	COMP_TH_75 = 0xC0,
	COMP_TH_70 = 0xF0,

	ILEAD_OFF_6nA = 0x00,
	ILEAD_OFF_22nA = 0x04,
	ILEAD_OFF_6uA = 0x08,
	ILEAD_OFF_22uA = 0x0C,

	FLEAD_OFF_AC = 0x01,
	FLEAD_OFF_DC = 0x00
};

enum CH1SET_bits {
	PD = 0x80,
	GAIN2 = 0x40,
	GAIN1 = 0x20,
	GAIN0 = 0x10,
	MUX3 = 0x08,
	MUX2 = 0x04,
	MUX1 = 0x02,
	MUX0 = 0x01,

	CH1SET_const = 0x00,

	GAIN_1 = 0x10,
	GAIN_2 = 0x20,
	GAIN_3 = 0x30,
	GAIN_4 = 0x40,
	GAIN_6 = 0x00,
	GAIN_8 = 0x50,
	GAIN_12 = 0x60,

	NORMAL = 0x00,
	SHORTED = 0x01,
	RLD_INPUT = 0x02,
	AVDD = 0x03,
	TEMP = 0x04,
	TEST_SIGNAL = 0x05,
	RLD_DRP = 0x06,
	RLD_DRM = 0x07,
	RLD_DRPM = 0x08,
	CH3_IN = 0x09

};

enum CH2SET_bits {
	CH2SET_const = 0x00,
	DVDD = 0x03


};

enum RLD_SENS_bits {
	CHOP1 = 0x80,
	CHOP0 = 0x40,
	PDB_RLD = 0x20,
	RLD_LOFF_SENS = 0x10,
	RLD2N = 0x08,
	RLD2P = 0x04,
	RLD1N = 0x02,
	RLD1P = 0x01,

	RLD_SENS_const = 0x00
};

enum LOFF_SENS_bits {
	FLIP2 = 0x20,
	FLIP1 = 0x10,
	LOFF2N = 0x08,
	LOFF2P = 0x04,
	LOFF1N = 0x02,
	LOFF1P = 0x01,

	LOFF_SENS_const = 0x00
};

enum LOFF_STAT_bits {
	CLK_DIV = 0x40,
	RLD_STAT = 0x10,
	IN2N_OFF = 0x08,
	IN2P_OFF = 0x04,
	IN1N_OFF = 0x02,
	IN1P_OFF = 0x01,

	LOFF_STAT_const = 0x00
};

enum RESP1_bits {
	RESP_DEMOD_EN = 0x80,
	RESP_MOD_EN = 0x40,
	RESP_PH3 = 0x20,
	RESP_PH2 = 0x10,
	RESP_PH1 = 0x08,
	RESP_PH0 = 0x04,
	RESP_CTRL = 0x01,

	RESP1_const = 0x02

};

enum RESP2_bits {

	CALIB_ON = 0x80,
	RESP_FREQ = 0x04,
	RLDREF_INT = 0x02,

	RESP2_const = 0x01,
	RESP_32kHz = 0x00,
	RESP_64kHz = 0x04

};

enum GPIO_bits {
	GPIOC2 = 0x08,
	GPIOC1 = 0x04,
	GPIOD2 = 0x02,
	GPIOD1 = 0x01,

	GPIO_const = 0x00
};

enum SIGNAL_source {
	TEST_SOURCE = 0x00,
	BIO_SOURCE = 0x01
};


class Ads1292
{
public:
	struct ads1292_settings settings;

	Usart usart_spi;

	uint8_t ReadRegister(uint8_t reg);
	void WriteRegister(uint8_t reg, uint8_t data);
	void SendCommand(uint8_t cmd);


	uint16_t status;
	uint16_t real_sample_rate;

	uint32_t value_ch1;
	uint32_t value_ch2;

	bool rld;
	bool n2;
	bool p2;
	bool n1;
	bool p1;


	void ReadData();

	bool SelfTest();
	void Reset();

	void CfgTest();
	void CfgTest2();


	uint16_t SetODR(uint16_t odr);
	uint8_t SetGain(uint8_t ch, uint8_t gain);
	void SetChannelSource(uint8_t ch, enum SIGNAL_source);
	void EnableChannelTestSignal(uint8_t ch);
	void SetResp();

	void Set(struct ads1292_settings settings);

	//common control
	void Init(struct ads1292_settings settings);
	void Deinit();

	void Start();
	void Stop();

	void Store();
};



#endif /* ADS1292_H_ */
