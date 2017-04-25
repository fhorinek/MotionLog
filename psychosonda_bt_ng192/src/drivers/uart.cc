#include "uart.h"

uint8_t uart_buffer[BUFFER_SIZE];

Usart uart(0, NULL, BUFFER_SIZE, uart_buffer);

uint8_t debug_level = false;

CreateStdIn(uart_in, uart.Read);
CreateStdOut(uart_out, uart.Write);

void uart_send(char * msg)
{
	char * ptr = msg;

	while (*ptr != 0)
	{
		uart.Write(*ptr);
		ptr++;
	}
}

void uart_init()
{
	//enable usart
	UART_PWR_ON;

	//init uart
	uart.Init(UART_UART, 921600ul);
	uart.SetInterruptPriority(HIGH);
//	uart.dbg = true;

	SetStdIO(uart_in, uart_out);
}

void uart_low_speed()
{
	//enable usart
	UART_PWR_ON;

	//init uart
	uart.Init(UART_UART, 9600);
	uart.SetInterruptPriority(HIGH);
//	uart.dbg = true;

	SetStdIO(uart_in, uart_out);
}

void uart_stop()
{
	uart.Stop();

	//disable usart
	UART_PWR_OFF;
}

void DUMP_REG(uint8_t val)
{
	DEBUG("%02X - ", val);
	for (uint8_t q = 8; q > 0; q--)
	{
		DEBUG("%d", (val & (1 << (q - 1))) >> (q - 1));
		if (q == 5)
			DEBUG(" ");
	}
	DEBUG("\n");
}
