#include "gpio.h"

void GpioWrite(PORT_t * port, uint8_t pin, uint8_t set)
{
	if (set != LOW)
		port->OUTSET = (1 << pin);
	else
		port->OUTCLR = (1 << pin);
}

uint8_t GpioRead(PORT_t * port, uint8_t pin)
{
	return ((port->IN & (1 << pin)) >> pin);
}

void GpioSetPull(PORT_t * port, uint8_t pin, xlib_core_gpio_pull pull)
{
	(*(register8_t *)(&port->PIN0CTRL + sizeof(register8_t) * pin)) |= pull;
}

void GpioSetInvert(PORT_t * port, uint8_t pin, uint8_t inv)
{
	if (inv == ON)
		(*(register8_t *)(&port->PIN0CTRL + sizeof(register8_t) * pin)) |= PORT_INVEN_bm;
	else
		(*(register8_t *)(&port->PIN0CTRL + sizeof(register8_t) * pin)) &= ~PORT_INVEN_bm;
}

void GpioSetDirection(PORT_t * port, uint8_t pin, uint8_t dir)
{
	if (dir != INPUT)
		port->DIRSET = (1 << pin);
	else
		port->DIRCLR = (1 << pin);
}

void GpioSetInterrupt(PORT_t * port, uint8_t pin, xlib_core_gpio_intmask mask)
{
	GpioSetInterrupt(port, pin, mask, gpio_bothedges, LOW);
}

void GpioSetInterrupt(PORT_t * port, uint8_t pin, xlib_core_gpio_intmask mask, xlib_core_gpio_int interrupt)
{
	GpioSetInterrupt(port, pin, mask, interrupt, LOW);
}

void GpioSetInterrupt(PORT_t * port, uint8_t pin, xlib_core_gpio_intmask mask, xlib_core_gpio_int interrupt, uint8_t priority)
{
	(*(register8_t *)(&port->PIN0CTRL + sizeof(register8_t) * pin)) |= interrupt;

	uint8_t p_val;
	switch (priority)
	{
		case(LOW):
			p_val = 0b00000001;
		break;
		case(MEDIUM):
			p_val = 0b00000010;
		break;
		case(HIGH):
			p_val = 0b00000011;
		break;
	}

	switch (mask)
	{
		case(gpio_clear):
			port->INT0MASK &= ~(1 << pin);
			port->INT1MASK &= ~(1 << pin);
			break;
		case(gpio_interrupt0):
			port->INT0MASK |= (1 << pin);
			port->INTCTRL = (port->INTCTRL & 0b00001100) | (p_val << 0);
			break;
		case(gpio_interrupt1):
			port->INT1MASK |= (1 << pin);
			port->INTCTRL = (port->INTCTRL & 0b00000011) | (p_val << 2);
			break;
	}
}
