/*
 * devices.h
 *
 *  Created on: 30.7.2014
 *      Author: horinek
 */

#ifndef DEVICES_H_
#define DEVICES_H_

#include "../psychosonda.h"

#include "protocol.h"

#include "ads1292.h"
#include "bmp180.h"
#include "lsm303d.h"
#include "l3gd20.h"
#include "sht21.h"

extern Ads1292 ads1292;

extern I2c mems_i2c;

extern BMP180 bmp180;
extern Lsm303d lsm303d;
extern L3gd20 l3gd20;
extern SHT21 sht21;

extern struct ads1292_settings ads1292_settings;
extern struct l3gd20_settings l3gd20_settings;
extern struct lsm303d_settings lsm303d_settings;
extern struct bmp180_settings bmp180_settings;
extern struct sht21_settings sht21_settings;


int32_t to_dec_3(int64_t c);
int16_t to_dec_2(int32_t c);
int8_t to_dec_1(int8_t c);

#endif /* DEVICES_H_ */
