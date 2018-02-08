/*
 * pan1026.h
 *
 *  Created on: 18.5.2015
 *      Author: horinek
 */

#ifndef PAN1026_H_
#define PAN1026_H_

#include "../../common.h"
#include "../drivers/uart.h"
#include "../../xlib/core/usart.h"

/*! The max. ATT size of TC35661 is limited to 64 bytes */
#define PAN1026_SPP_MTU			512//max 543
#define PAN1026_MTU_RX			64 //max 64
#define PAN1026_MTU_TX			64 //max 64
#define PAN1026_MTU_TX_FALLBACK	20 //min 20

#define PAN1026_BUFFER_SIZE 64

class pan1026
{
public:

	Usart * usart;

	bool connected;

	void Init(Usart * uart);
	void Restart();
	void TxResume();

	void Step();

	uint8_t next_cmd;
	uint8_t last_cmd;
	uint8_t state;
	uint8_t parser_status;
	uint16_t parser_packet_length;
	uint8_t parser_buffer_index;
	uint8_t parser_buffer[PAN1026_BUFFER_SIZE];
	uint32_t parser_timer;

	uint16_t mtu_size;

	uint8_t cmd_iter;

	bool btle_connection;

	uint16_t btle_service_handles[3];
	uint16_t btle_characteristic_handles[7];
	uint16_t btle_characteristic_element_handles[7];
	uint16_t btle_notifications;

	uint16_t btle_connection_handle;

	bool repat_last_cmd;
	bool busy;

	void SetNextStep(uint8_t cmd);

	void Parse(uint8_t c);
	void ParseHCI();
	void ParseMNG();
	void ParseSPP();

	void ParseMNG_LE();
	void ParseGAT_cli();
	void ParseGAT_ser();

	bool Idle();
	void SetBusy(uint16_t timeout = 0);
	void ClearBusy();
	uint32_t busy_timer;

	uint32_t repeat_timer;

	void StreamWrite(uint8_t data);
	void RawSendStatic(const uint8_t * data, uint8_t len);

	void SendString();

	uint16_t last_send_len;

	uint8_t pan_mac_address[6];

	uint8_t client_mac_address[6];

	char client_name[32];

};

#endif /* PAN1026_H_ */
