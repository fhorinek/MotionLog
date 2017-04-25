/*
 * protocol.h
 *
 *  Created on: 18.2.2015
 *      Author: horinek
 */

#ifndef PROTOCOL_H_
#define PROTOCOL_H_


// FILE FORMAT
// 7654 3210 |
// id   len
// id 0-15
//	0 - MEAS HEAD cfg_id, timestamp
//	1 - ADS  ch1, ch2
//  2 - ACC  x, y, z (FIFO?)
//  3 - GYRO x, y, z (FIFO?)
//  4 - MAG  x, y, z
//  5 - BMP

// len 0-14
//  0xf - next byte set length

#define make_head(id, len)	(id << 4 | (len & 0x0F))

#define id_head		0x0
#define id_ads		0x1
#define id_acc		0x2
#define id_gyro		0x3
#define id_mag		0x4
#define id_bmp		0x5
#define id_event	0x6
#define id_bat		0x7
#define id_temp		0x8
#define id_humi		0x9

#define event_mark		0
#define event_end		1
#define event_signal	2

#endif /* PROTOCOL_H_ */
