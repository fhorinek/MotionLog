#include <avr/io.h>
#include <stdlib.h>
#include <avr/eeprom.h>

#include "sp_driver.h"
#include "clock.h"
#include "common.h"
#include "usart.h"
#include "FatFs/ff.h"
#include "FatFs/diskio.h"



void switch_to_app()
{
	void (*reset_vect)( void ) = 0x000000;

	//move int to back to 0x0
	CCPIOWrite(&PMIC.CTRL, 0);

	//USART
	usart_deinit();

	//SD EN
	PORTD.DIRCLR = 0b00000001;
	//SD SPI
	PORTC.DIRCLR = 0b10110000;
	SPIC.CTRL = 0;

	//clock
	osc_deinit();

	//LEDS
	LED_DEINIT;

	//jump to first 128
	EIND = 0x0;
	//jump to 0x0
	reset_vect();
}

#define BUFFER_SIZE 256
uint32_t adr = 0;
uint8_t buffer[BUFFER_SIZE];
uint16_t bytes_read;
uint16_t i;

struct app_info
{
	uint8_t app_name[32];
};

struct app_info ee_fw_info __attribute__ ((section(".fw_info")));
struct app_info fw_info;
struct app_info fw_new;

void mem_erase()
{
	LED_BLUE;
	usart_putstr("Erasing ... ");
//	for (adr = 0; adr < APP_SECTION_SIZE; adr += APP_SECTION_PAGE_SIZE)
//	{
//		// wait for SPM instruction to complete
//		SP_WaitForSPM();
//		// erase page
//		SP_EraseApplicationPage(adr);
//	}
	SP_EraseApplicationSection();
	usart_putstr("done\n");
	LED_OFF;
}

int main()
{
	_delay_ms(4);
	//move int to boot
	CCPIOWrite(&PMIC.CTRL, PMIC_IVSEL_bm);

	osc_init();

	_delay_ms(20);

	usart_init();

	//SD EN
	PORTD.DIRSET = 0b00000001;
	PORTD.OUTCLR = 0b00000001;
	//SD SPI
	PORTC.DIRSET = 0b10110000;

	//LEDS
	LED_INIT;
	LED_OFF;

	//Filesystem
	FATFS FatFs;
	FIL binary;
	uint8_t res;

	//control
	uint8_t upload = 0;

	//usart_putstr("\n\nSD Bootloader v 1.0\n");

	res = f_mount(&FatFs, "", 1);

	if (res != RES_OK)
	{
		usart_putstr(" Unable to mount SD.");
		usart_putchar(res);
		usart_putstr("\n");
	}
	else
	{
		eeprom_busy_wait();
		eeprom_read_block(&fw_info, &ee_fw_info, sizeof(fw_info));

		usart_putstr("App:\n ");
		for (i = 0; i < 32; i++)
		{
			uint8_t c = fw_info.app_name[i];
			if (c < 32 || c > 126)
				c = '_';
			usart_putchar(c);
		}
		usart_putchar('\n');

		res = f_open(&binary, "UPDATE.BIN", FA_READ);
		if (res == RES_OK)
		{
			uint8_t same = 1;

			res = f_read(&binary, &fw_new, 32, &bytes_read);
			usart_putstr("UPDATE.BIN:\n ");
			for (i = 0; i < 32; i++)
			{
				if (fw_new.app_name[i] != fw_info.app_name[i])
					same = 0;
				uint8_t c = fw_new.app_name[i];
				if (c < 32 || c > 126)
					c = '_';
				usart_putchar(c);
			}
			usart_putchar('\n');

			if (same)
				usart_putstr("Up-to-date\n");
			else
				upload = 1;
		}
		else
		{
			usart_putstr(" Unable to open UPDATE.BIN\n");
		}
	}

	if (upload)
	{
		mem_erase();
		adr = 0;

		usart_putstr("Programming ");
		do
		{
			LED_BLUE;
			res = f_read(&binary, buffer, BUFFER_SIZE, &bytes_read);

			if (res == RES_OK)
			{
				LED_GREEN;

				SP_EraseFlashBuffer();

				for (i = 0; i < bytes_read / 2; i++)
				{
					uint8_t datal = buffer[i * 2 + 0];
					uint8_t datah = buffer[i * 2 + 1];

					SP_WaitForSPM();
					SP_LoadFlashWord(adr + i*2, datah << 8 | datal);

				}

				SP_WaitForSPM();
				SP_WriteApplicationPage(adr);

				adr += bytes_read;

				usart_putchar('.');
			}
			LED_OFF;
		} while (bytes_read == BUFFER_SIZE);

		usart_putstr(" done\n");
		f_close(&binary);

		eeprom_busy_wait();
		eeprom_update_block(&fw_new, &ee_fw_info, sizeof(fw_info));
		eeprom_busy_wait();

	}

	usart_putstr("Booting...\n\n\n");

	SP_WaitForSPM();

	switch_to_app();

	return 0;
}

