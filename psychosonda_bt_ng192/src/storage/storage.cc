#include "storage.h"

FATFS FatFs;		/* FatFs work area needed for each volume */
DataBuffer * sd_buffer;

extern Spi sd_spi_usart;

uint32_t storage_space = 0;
uint32_t storage_free_space = 0;

bool sd_avalible = false;

bool storage_init()
{
	uint8_t res;

	//power spi & sdcard
	SD_EN_ON;
	SD_SPI_PWR_ON;

	res = f_mount(&FatFs, "", 1);		/* Give a work area to the default drive */

	DEBUG("Mounting SD card ... ");

	if (res != RES_OK)
	{
		DEBUG("Error %02X\n", res);

		//not needed
		sd_spi_usart.Stop();

		SD_SPI_PWR_OFF;

		return false;
	}

	DEBUG("OK\n");

	uint32_t size;

	FATFS * FatFs1;

	res = f_getfree("", &size, &FatFs1);

//	DEBUG1("f_getfree res = %d, size = %lu MiB", res, size / 256);

	uint32_t sector_count;

	res = disk_ioctl(0, GET_SECTOR_COUNT, &sector_count);

//	DEBUG1("GET_SECTOR_COUNT res = %d, size = %lu", res, sector_count);

	uint16_t sector_size;

	res = disk_ioctl(0, GET_SECTOR_SIZE, &sector_size);

//	DEBUG1("GET_SECTOR_SIZE res = %d, size = %u", res, sector_size);

	storage_space = sector_count * sector_size;
	storage_free_space = size * 4 * 1024;

	DEBUG("Disk info\n");
	DEBUG(" sector size  %12u\n", sector_size);
	DEBUG(" sector count %12lu\n", sector_count);
	DEBUG(" total space  %12lu\n", storage_space);
	DEBUG(" free space   %12lu\n", storage_free_space);

	sd_avalible = true;
	return true;
}

void storage_deinit()
{
	uint8_t res;

	sd_avalible = false;

	res = f_mount(NULL, "", 1); //unmount

	//power spi & sdcard
	SD_EN_OFF;
	SD_SPI_PWR_OFF;
}

void storage_copy_file(char * src, char * dst)
{
	FIL fsrc, fdst;
	FRESULT fr;
	BYTE buffer[512];
	UINT br, bw;

	fr = f_open(&fsrc, src, FA_OPEN_EXISTING | FA_READ);
	if (fr)
	{
		DEBUG("Cannot open source file\n");
		return;
	}
	fr = f_open(&fdst, dst, FA_CREATE_ALWAYS | FA_WRITE);
	if (fr)
	{
		DEBUG("Cannot open destination file\n");
		return;
	}

    /* Copy source to destination */
    for (;;) {
        fr = f_read(&fsrc, buffer, sizeof buffer, &br);  /* Read a chunk of source file */
        if (fr || br == 0) break; /* error or eof */
        fr = f_write(&fdst, buffer, br, &bw);            /* Write it to the destination file */
        if (fr || bw < br) break; /* error or disk full */
    }

    /* Close files */
    f_close(&fsrc);
    f_close(&fdst);
}

bool storage_ready()
{
	return sd_avalible;
}
