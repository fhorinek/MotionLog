#include "bt.h"

RemoteGUI rgui;

volatile bool bt_device_connected = false;

volatile uint8_t bt_module_state = BT_MOD_STATE_OFF;

pan1322 bt_pan1322;

#define BT_UART_RX_SIZE		(BUFFER_SIZE * 2)
uint8_t bt_uart_rx[BT_UART_RX_SIZE];
#define BT_UART_TX_SIZE		(BUFFER_SIZE)
uint8_t bt_uart_tx[BT_UART_TX_SIZE];

Usart bt_uart(BT_UART_RX_SIZE, bt_uart_rx, BT_UART_TX_SIZE, bt_uart_tx);

uint32_t bt_reset_counter = 0;
uint8_t bt_reset_counter_step = 0;

ISR(BT_CTS_PIN_INT)
{
	DEBUG("CTS\n");
	bt_pan1322.TxResume();
}

void bt_init()
{
	DEBUG("bt_init\n");

	//pin init
	GpioSetDirection(BT_EN, OUTPUT);
	GpioSetDirection(BT_RESET, OUTPUT);
	GpioSetDirection(BT_RTS, OUTPUT);

	//power is off
	GpioWrite(BT_RTS, LOW);
	GpioWrite(BT_EN, LOW);
	GpioWrite(BT_RESET, LOW);

	//IRQ init
	GpioSetDirection(BT_CTS, INPUT);
	GpioSetPull(BT_CTS, gpio_pull_down);
	GpioSetInterrupt(BT_CTS, gpio_interrupt1, gpio_rising);
}

void bt_module_init()
{
	DEBUG("bt_pan1322_init\n");
	bt_pan1322.Init(&bt_uart);
}

void bt_module_deinit()
{
	GpioWrite(BT_EN, LOW);
	GpioWrite(BT_RESET, LOW);

	bt_irqh(BT_IRQ_DEINIT, 0);
	bt_uart.Stop();
	BT_UART_PWR_OFF;
}

void bt_module_reset()
{
	GpioWrite(BT_EN, LOW);
	GpioWrite(BT_RESET, LOW);

	bt_uart.Stop();
	BT_UART_PWR_OFF;

	bt_reset_counter = task_get_ms_tick() + 2000;
	bt_reset_counter_step = 0;
}

void bt_irqh(uint8_t type, uint8_t * buf)
{
//	DEBUG("bt_irqh > %u %u\n", type, *buf);

	switch(type)
	{
		case(BT_IRQ_INIT):
			DEBUG("BT_MOD_STATE_INIT\n");
			bt_module_state = BT_MOD_STATE_INIT;
		break;
		case(BT_IRQ_INIT_OK):
			DEBUG("BT_MOD_STATE_OK\n");
			bt_module_state = BT_MOD_STATE_OK;
		break;
		case(BT_IRQ_DEINIT):
			DEBUG("BT_MOD_STATE_OFF\n");
			bt_module_state = BT_MOD_STATE_OFF;
		break;
		case(BT_IRQ_CONNECTED):
			DEBUG("BT_IRQ_CONNECTED\n");
			bt_device_connected = true;
		break;
		case(BT_IRQ_DISCONNECTED):
			DEBUG("BT_IRQ_DISCONNECTED\n");
			bt_device_connected = false;
		break;
		case(BT_IRQ_RESET):
			DEBUG("BT_IRQ_RESET\n");
			bt_device_connected = false;
		break;
		case(BT_IRQ_INIT_FAIL):
			DEBUG("BT_IRQ_INIT_FAIL!\n");
			bt_device_connected = false;
		break;
		case(BT_IRQ_DATA):
//			DEBUG("DATA>%c\n", *buf);
		break;
	}

	uint8_t param[2];
	param[0] = type;
	param[1] = *buf;

	task_irqh(TASK_IRQ_BT, param);
}

bool bt_ready()
{
	return (bt_module_state == BT_MOD_STATE_OK);
}

void bt_step()
{
	if (bt_module_state == BT_MOD_STATE_OFF)
		return;

	if (bt_reset_counter)
	{
		if (bt_reset_counter > task_get_ms_tick())
			return;

		switch(bt_reset_counter_step)
		{
			case(0):
				GpioWrite(BT_EN, HIGH);
				bt_reset_counter = task_get_ms_tick() + 500;
				bt_reset_counter_step = 1;
			break;
			case(1):
				//enable bt uart
				BT_UART_PWR_ON;
				bt_uart.Init(BT_UART, 115200);
				bt_uart.SetInterruptPriority(MEDIUM);
//				bt_uart.SetupRxDMA(BT_UART_DMA_CH, BT_UART_DMA_TRIGGER);

				bt_reset_counter = task_get_ms_tick() + 10;
				bt_reset_counter_step = 2;
			break;
			case(2):
				GpioWrite(BT_RESET, HIGH);
				bt_reset_counter_step = 0;
				bt_reset_counter = 0;
			break;
		}

		return;
	}

	bt_pan1322.Step();
}
