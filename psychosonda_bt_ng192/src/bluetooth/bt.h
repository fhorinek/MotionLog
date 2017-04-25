/*
 * bt.h
 *
 *  Created on: 8.7.2014
 *      Author: horinek
 */

#ifndef BT_H_
#define BT_H_

#include "../common.h"
#include "../drivers/uart.h"
#include "../tasks/tasks.h"

//#include "btcomm.h"
#include "pan_lite.h"

#define DEBUG_OUTPUT rgui
#include "remotegui.h"

#define BT_MOD_STATE_OFF	0
#define BT_MOD_STATE_INIT	1
#define BT_MOD_STATE_OK		2

#define BT_IRQ_CONNECTED	0
#define BT_IRQ_DISCONNECTED	1
#define BT_IRQ_ERROR		2
#define BT_IRQ_INIT_OK		3
#define BT_IRQ_INIT			4
#define BT_IRQ_DEINIT		5
#define BT_IRQ_PAIR			6
#define BT_IRQ_INIT_FAIL	7
#define BT_IRQ_RESET		8
#define BT_IRQ_DATA			9

void bt_init();
void bt_module_init();
void bt_module_deinit();
void bt_module_reset();
void bt_step();
bool bt_device_active();
void bt_ms_irq();

void bt_irqh(uint8_t type, uint8_t * buf);

extern pan1322 bt_pan1322;
extern FILE * bt_pan1322_out;

#endif /* BT_H_ */
