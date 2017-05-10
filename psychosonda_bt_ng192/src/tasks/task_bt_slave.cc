#include "task_bt_slave.h"

#include "../xlib/stream.h"
#include "../storage/cfg.h"

//Stream control data flow form and to the slave
Stream bt_slave_stream;
//hander for all file operations
FIL slave_file;
//data buffer for file operation
char slave_file_buffer[512];
//time when system start measuring
uint32_t slave_meas_start;
//is system in measurement mode?
bool slave_meas = false;
//do we need to flash all the data from sd_buffer?
bool slave_flush = false;
//end time
uint32_t slave_meas_end = 0;
//bt_enabled after end
uint32_t slave_bt_timeout = 0;

//default values in seconds
#define BT_TIMEOUT		(60)
#define WAKE_UP_PERIOD	(60 * 10)
uint32_t bt_timeout = BT_TIMEOUT * 1000lu;
uint32_t wake_up_period = WAKE_UP_PERIOD;

SleepLock slave_bt_lock;


#define SLAVE_SIGNAL_MAX	5
//markers
uint32_t slave_signals[SLAVE_SIGNAL_MAX];

#define SLAVE_NO_MEAS_ID	0xFFFFFFFF
#define INVALID_TIME		0xFFFFFFFF

EEMEM uint32_t ee_slave_meas_cnt;
EEMEM uint32_t ee_slave_meas_conf_id;

#define CMD_HELLO			0
#define CMD_PUSH_FILE		1
#define CMD_PULL_FILE		2
#define CMD_PUSH_PART		3
#define CMD_PULL_PART		4
#define CMD_CLOSE_FILE		5
#define CMD_MEAS			6
#define CMD_REBOOT			7
#define CMD_OFF				8
#define CMD_SET_TIME		9
#define CMD_LIST_DIR		10
#define CMD_DEL_FILE		11
#define CMD_MV_FILE			12

#define CMD_RET_OK			0
#define CMD_RET_FAIL		1
#define CMD_ID				2
#define CMD_PART			3
#define CMD_DIR_LIST		4

#define FAIL_FILE			0
#define FAIL_MEAS			1

#define START_MEAS_OK		0
#define FAIL_MEAS_BUFFER	1
#define FAIL_MEAS_CFG		2
#define FAIL_MEAS_RAW		3

#define SD_BUFFER_SIZE		512 * 16
#define SD_BUFFER_WRITE		512 * 8

FIL cfg;

//Slave return OK
void bt_slave_ok()
{
	DEBUG(">bt_slave_ok\n");

	bt_slave_stream.StartPacket(1);
	bt_slave_stream.Write(CMD_RET_OK);
}

//Slave return OK + size
void bt_slave_ok(uint32_t size)
{
	DEBUG(">bt_slave_ok %lu\n", size);

	bt_slave_stream.StartPacket(1 + 4);
	bt_slave_stream.Write(CMD_RET_OK);

	byte4_u b4;

	b4.uint32 = size;

	bt_slave_stream.Write(b4.bytes[0]);
	bt_slave_stream.Write(b4.bytes[1]);
	bt_slave_stream.Write(b4.bytes[2]);
	bt_slave_stream.Write(b4.bytes[3]);
}


//Slave indicate error
void bt_slave_fail(uint8_t type, uint8_t code)
{
	DEBUG(">bt_slave_fail %d %d\n", type, code);

	bt_slave_stream.StartPacket(3);
	bt_slave_stream.Write(CMD_RET_FAIL);
	bt_slave_stream.Write(type);
	bt_slave_stream.Write(code);
}

extern struct app_info fw_info;

void bt_slave_hello()
{
	byte2_u tmp;

	DEBUG(">bt_slave_hello\n");

	bt_slave_stream.StartPacket(38);
	bt_slave_stream.Write(CMD_ID);

	for (uint8_t i = 0; i < 32; i++)
		{
			uint8_t c = fw_info.app_name[i];
			if (c < 32 || c > 126)
				c = '_';

			bt_slave_stream.Write(c);
		}

	bt_slave_stream.Write(battery_per);

	tmp.uint16 = bt_pan1322.mtu_size;
	bt_slave_stream.Write(tmp.bytes[0]);
	bt_slave_stream.Write(tmp.bytes[1]);
	
	tmp.int16 = battery_adc_raw;
	bt_slave_stream.Write(tmp.bytes[0]);
	bt_slave_stream.Write(tmp.bytes[1]);
}

uint16_t bt_slave_dir_cnt(char * path)
{
	DEBUG("bt_slave_dir_cnt '%s'\n", path);

	DIR f_dir;
	uint16_t file_cnt = 0;

	if (f_opendir(&f_dir, path) == FR_OK)
	{
		FILINFO f_info;

		while(1)
		{
			uint8_t res = f_readdir(&f_dir, &f_info);
			if (res == FR_OK)
			{
				if (f_info.fname[0] != '\0')
				{
					if (f_info.fname[0] == 0xFF)
						continue;

					file_cnt++;
				}
				else
					break;
			}
			else
				break;
		}
	}

	return file_cnt;
}

void bt_slave_list(char * path, uint16_t start, uint8_t cnt)
{
	DEBUG("bt_slave_list '%s' %u %u\n", path, start, cnt);

	uint16_t files_in_dir = bt_slave_dir_cnt(path);
	int16_t files_to_list = files_in_dir - start;

//	DEBUG("files_in_dir %u\n", files_in_dir);

	DIR f_dir;
	uint16_t file_cnt = 0;

	byte2_u b2;

	if (files_to_list < 0)
		files_to_list = 0;
	else
	{
		if (files_to_list > cnt)
			files_to_list = cnt;
	}

//	DEBUG("files_to_list %u\n", files_to_list);

	bt_slave_stream.StartPacket(4 + files_to_list * 12);
	bt_slave_stream.Write(CMD_DIR_LIST);

	b2.uint16 = files_in_dir;
	bt_slave_stream.Write(b2.bytes[0]);
	bt_slave_stream.Write(b2.bytes[1]);

	bt_slave_stream.Write(files_to_list);

	if (f_opendir(&f_dir, path) == FR_OK)
	{
		FILINFO f_info;

		while(1)
		{
			if (f_readdir(&f_dir, &f_info) == FR_OK)
			{
				if (f_info.fname[0] != '\0')
				{
//					DEBUG("> %s\n", f_info.fname);
					if (f_info.fname[0] == 0xFF)
						continue;

					file_cnt++;

					if (file_cnt > start && files_to_list > 0)
					{
						uint8_t l = strlen(f_info.fname);
						for (uint8_t i = 0; i < 12; i++)
							if (i < l)
								bt_slave_stream.Write(f_info.fname[i]);
							else
								bt_slave_stream.Write(0);

						files_to_list--;
					}

					if (files_to_list == 0)
						break;
				}
				else
					break;
			}
			else
				break;
		}
	}

}

//Start measuring
uint8_t bt_slave_meas_start(uint32_t cfg_id)
{
	uint8_t res;

	DEBUG("Starting meas\n");

	MEMS_POWER_ON;
	I2C_POWER_ON;
	MEMS_I2C_PWR_ON;

	_delay_ms(10);

	mems_i2c.InitMaster(MEMS_I2C, 400000ul, 120, 20);
	DUMP_REG(mems_i2c.i2c->MASTER.BAUD);

	DEBUG("Checking for i2c devices\n");
	//i2c need time to spool up
	_delay_ms(1);
	mems_i2c.Scan();

	sd_buffer = new DataBuffer(SD_BUFFER_SIZE); //10K
	if (sd_buffer->size == 0)
	{
		DEBUG("Could not allocate memory for sd buffer!\n");
		return FAIL_MEAS_BUFFER;
	}

	char filename[64];
	bool meas_single_use;
	uint32_t meas_id;

	sprintf(filename, "/CONF/%08lX.CFG", cfg_id);
	DEBUG("\nReading cfg file: %s\n", filename);

	res = f_open(&cfg, filename, FA_READ);

	if (res == FR_OK)
	{
		//Meas section
		DEBUG("\n");
		DEBUG("Measurement\n");

		meas_id = cfg_get_int(&cfg, "meas", "id", 0);
		meas_single_use = cfg_get_int(&cfg, "meas", "single_use", 0);

		//increment file
		eeprom_busy_wait();
		uint32_t ram_meas_cnt = eeprom_read_dword(&ee_slave_meas_cnt);
		ram_meas_cnt++;
		eeprom_update_dword(&ee_slave_meas_cnt, ram_meas_cnt);

		sprintf(filename, "/LOGS/%08lX.RAW", ram_meas_cnt);

		DEBUG(" output file: %s\n", filename);

		slave_meas_end = cfg_get_int(&cfg, "meas", "duration", 0);
		if (slave_meas_end)
			DEBUG(" duration: %lu ms\n", slave_meas_end);
		else
			DEBUG(" duration: user end meas\n");

		for (uint8_t i=0; i < SLAVE_SIGNAL_MAX; i++)
		{
			char tmp[8];
			sprintf(tmp, "signal%d", i + 1);
			slave_signals[i] = cfg_get_int(&cfg, "meas", tmp, 0);
			if (slave_signals[i])
				DEBUG(" %s: %lu ms\n", tmp, slave_signals[i]);
		}

		//Bio section
		DEBUG("\n");
		DEBUG("ADS1292:\n");

		ads1292_settings.enabled = cfg_have_section(&cfg, "bio");
		if (ads1292_settings.enabled)
		{
			ads1292_settings.odr = cfg_get_int(&cfg, "bio", "odr", 125);
			ads1292_settings.ch1_gain = cfg_get_int(&cfg, "bio", "ch1_gain", 1);
			ads1292_settings.ch1_source = cfg_get_int(&cfg, "bio", "ch1_source", BIO_SOURCE);

			ads1292_settings.resp_enabled = cfg_get_int(&cfg, "bio", "resp_enabled", 0);
			ads1292_settings.resp_phase = cfg_get_int(&cfg, "bio", "resp_phase", 125);
			ads1292_settings.resp_freq = cfg_get_int(&cfg, "bio", "resp_freq", 64);

			ads1292_settings.ch2_gain = cfg_get_int(&cfg, "bio", "ch2_gain", 1);
			ads1292_settings.ch2_source = cfg_get_int(&cfg, "bio", "ch2_source", BIO_SOURCE);

			DEBUG(" odr: %d\n", ads1292_settings.odr);
			DEBUG(" ch1_gain: %d\n", ads1292_settings.ch1_gain);
			DEBUG(" ch1_source: %d\n", ads1292_settings.ch1_source);
			DEBUG(" ch2_gain: %d\n", ads1292_settings.ch2_gain);
			DEBUG(" ch2_source: %d\n", ads1292_settings.ch2_source);
		}
		else
			DEBUG(" disabled\n");

		ads1292.Init(ads1292_settings);

		//Gyro section
		DEBUG("\n");
		DEBUG("L3GD20:\n");

		l3gd20_settings.enabled = cfg_have_section(&cfg, "gyro");
		if (l3gd20_settings.enabled)
		{
			l3gd20_settings.odr = cfg_get_int(&cfg, "gyro", "odr", 95);
			l3gd20_settings.scale = cfg_get_int(&cfg, "gyro", "scale", 250);
			l3gd20_settings.bw = cfg_get_int(&cfg, "gyro", "bw", 12);

			DEBUG(" odr: %d\n", l3gd20_settings.odr);
			DEBUG(" scale: %d\n", l3gd20_settings.scale);
			DEBUG(" bw: %d\n", l3gd20_settings.bw);
		}
		else
			DEBUG("disabled\n");

		l3gd20.Init(&mems_i2c, l3gd20_settings);

		//Magnetometer & Accelerometer section
		DEBUG("\n");
		DEBUG("LSM303D:\n");

		lsm303d_settings.enabled = cfg_have_section(&cfg, "acc") || cfg_have_section(&cfg, "mag");
		if (lsm303d_settings.enabled)
		{
			lsm303d_settings.accOdr = cfg_get_int(&cfg, "acc", "odr", 0);
			lsm303d_settings.accScale = cfg_get_int(&cfg, "acc", "scale", 0);

			lsm303d_settings.magOdr = cfg_get_int(&cfg, "mag", "odr", 0);
			lsm303d_settings.magScale = cfg_get_int(&cfg, "mag", "scale", 2);

			DEBUG(" ACC ODR: %d\n", lsm303d_settings.accOdr);
			DEBUG(" ACC scale: %d\n", lsm303d_settings.accScale);
			DEBUG(" MAG ODR: %d\n", lsm303d_settings.magOdr);
			DEBUG(" MAG scale: %d\n", lsm303d_settings.magScale);
		}
		else
			DEBUG("disabled\n");

		lsm303d.Init(&mems_i2c, lsm303d_settings);

		//Barometer section
		DEBUG("\n");
		DEBUG("BMP180:\n");

		bmp180_settings.enabled = cfg_have_section(&cfg, "baro");
		if (bmp180_settings.enabled)
		{
			bmp180_settings.odr = cfg_get_int(&cfg, "baro", "odr", 2);

			DEBUG(" BARO ODR: %d\n", bmp180_settings.odr);
		}
		else
			DEBUG("disabled\n");

		bmp180.Init(&mems_i2c, bmp180_settings);

		//Enviromental
		DEBUG("\n");
		DEBUG("SHT21:\n");

		if (cfg_have_section(&cfg, "enviro"))
		{
			sht21_settings.rh_enabled = cfg_get_int(&cfg, "enviro", "humidity", 0);
			sht21_settings.temp_enabled = cfg_get_int(&cfg, "enviro", "temperature", 0);

			DEBUG(" humidity: %d\n", sht21_settings.rh_enabled);
			DEBUG(" temperature: %d\n", sht21_settings.temp_enabled);
		}
		else
		{
			DEBUG("disabled\n");
		}

		sht21.Init(&mems_i2c, sht21_settings);
	}
	else
	{
		DEBUG(" Error opening cfg file %02X\n", res);
		return FAIL_MEAS_CFG;
	}

	f_close(&cfg);
	DEBUG("\n\n");

	res = f_open(&slave_file, filename, FA_WRITE | FA_CREATE_ALWAYS);

	if (res != FR_OK)
	{
		DEBUG("Could not create raw file. Error %02X\n", res);
		return FAIL_MEAS_RAW;
	}
	else
	{
		if (meas_single_use)
		{
			sprintf(filename, "CONF/%08lX.CFG", cfg_id);
			DEBUG("\nRemoving single use cfg file: %s\n", filename);
			assert(f_unlink(filename) == FR_OK);

			//leave empty file so the task will not be reuploaded
			DEBUG("\nCreating empty file: %s\n", filename);
			assert(f_open(&cfg, filename, FA_WRITE | FA_CREATE_ALWAYS) == FR_OK);
			f_close(&cfg);
		}

		uint8_t head_data[1];
		head_data[0] = make_head(id_head, 12);

		uint32_t time = time_get_actual();

		sd_buffer->Write(1, head_data);
		sd_buffer->Write(4, (uint8_t*) &cfg_id);
		sd_buffer->Write(4, (uint8_t*) &meas_id);
		sd_buffer->Write(4, (uint8_t*) &time);

		slave_meas_start = task_get_ms_tick();
		DEBUG("slave_meas_start %lu\n", slave_meas_start);
		if (slave_meas_end)
			slave_meas_end = slave_meas_start + slave_meas_end;

		DEBUG("slave_meas_end %lu\n", slave_meas_end);

		for (uint8_t i=0; i < SLAVE_SIGNAL_MAX; i++)
		{
			if (slave_signals[i])
				slave_signals[i] += slave_meas_start;
			DEBUG("slave_signals[%d] %lu\n", i, slave_signals[i]);
		}

		slave_meas = true;

		ads1292.Start();
		lsm303d.Start();
		l3gd20.Start();
		bmp180.Start();
		sht21.Start();

		bt_module_deinit();

		DEBUG("Free RAM %d", freeRam());

		buzzer_beep(_100ms * 5, _100ms, _100ms);
		led_anim(LED_BREATHG, 0xFF);

		return START_MEAS_OK;
	}
}

void bt_slave_meas_stop()
{
	slave_meas = false;

	ads1292.Stop();
	lsm303d.Stop();
	l3gd20.Stop();
	bmp180.Stop();
	sht21.Stop();

	buzzer_beep(_100ms, _100ms, _100ms, _100ms, _100ms);
	led_anim(LED_NO_ANIM);

	DEBUG("Writing end time...\n");

	//write end event
	uint8_t event_data[1 + 1];
	event_data[0] = make_head(id_event, 5);
	event_data[1] = event_end;
	uint32_t act_time = task_get_ms_tick() - slave_meas_start;
	sd_buffer->Write(2, event_data);
	sd_buffer->Write(4, (uint8_t*)&act_time);
	sd_buffer->lock = true;

	DEBUG("Flushing SD buffer ...");

	slave_flush = true;
	task_bt_save_buffer();
	slave_flush = false;
	DEBUG("OK\n");

	f_close(&slave_file);

	//XXX: just for the buzzer sound
	_delay_ms(500);

	MEMS_POWER_OFF;
	I2C_POWER_OFF;
	MEMS_I2C_PWR_OFF;
}

void bt_slave_rxpacket()
{
	char file_path[64];
	char second_path[64];
	uint8_t ret;
	uint8_t cmd = bt_slave_stream.Read();
	uint16_t len;
	byte2_u b2;
	byte4_u b4;
	uint16_t bw;
	uint8_t cnt;

	led_anim(LED_FASTB);

	switch (cmd)
	{
	case(CMD_HELLO):
		bt_slave_hello();
	break;

	case(CMD_PUSH_FILE):
		len = bt_slave_stream.Read();
		for(uint8_t i=0; i < len; i++)
			file_path[i] = bt_slave_stream.Read();
		file_path[len] = 0;

		DEBUG("CMD_PUSH_FILE %s, %u\n", file_path, len);

		ret = f_open(&slave_file, file_path, FA_WRITE | FA_CREATE_ALWAYS);
		if (ret != FR_OK)
			bt_slave_fail(FAIL_FILE, ret);
		else
			bt_slave_ok();
	break;

	case(CMD_PULL_FILE):
		len = bt_slave_stream.Read();
		for(uint8_t i=0; i < len; i++)
			file_path[i] = bt_slave_stream.Read();
		file_path[len] = 0;

		DEBUG("CMD_PULL_FILE %s, %u\n", file_path, len);

		ret = f_open(&slave_file, file_path, FA_READ);
		if (ret != FR_OK)
			bt_slave_fail(FAIL_FILE, ret);
		else
			bt_slave_ok(f_size(&slave_file));
	break;

	case(CMD_PUSH_PART):
		b2.bytes[0] = bt_slave_stream.Read();
		b2.bytes[1] = bt_slave_stream.Read();
		len = b2.uint16;
		b4.bytes[0] = bt_slave_stream.Read();
		b4.bytes[1] = bt_slave_stream.Read();
		b4.bytes[2] = bt_slave_stream.Read();
		b4.bytes[3] = bt_slave_stream.Read();

		DEBUG("CMD_PUSH_PART %u, %lu\n", len, b4.uint32);

		if (f_tell(&slave_file) != b4.uint32)
		{
			ret = f_lseek(&slave_file, b4.uint32);
			if (ret != FR_OK)
				bt_slave_fail(FAIL_FILE, ret);
		}

		for (uint16_t i=0; i < len; i++)
			slave_file_buffer[i] = bt_slave_stream.Read();

		ret = f_write(&slave_file, slave_file_buffer, len, &bw);
		if (ret != FR_OK)
		{
			bt_slave_fail(FAIL_FILE, ret);
			break;
		}

		if (bw != len)
		{
			bt_slave_fail(FAIL_FILE, 0xFF);
			break;
		}

		f_sync(&slave_file);

		bt_slave_ok();
	break;

	case(CMD_PULL_PART):
		b2.bytes[0] = bt_slave_stream.Read();
		b2.bytes[1] = bt_slave_stream.Read();
		len = b2.uint16;
		b4.bytes[0] = bt_slave_stream.Read();
		b4.bytes[1] = bt_slave_stream.Read();
		b4.bytes[2] = bt_slave_stream.Read();
		b4.bytes[3] = bt_slave_stream.Read();

		DEBUG("CMD_PULL_PART %u, %lu\n", len, b4.uint32);

		if (f_tell(&slave_file) != b4.uint32)
		{
			ret = f_lseek(&slave_file, b4.uint32);
			if (ret != FR_OK)
				bt_slave_fail(FAIL_FILE, ret);
		}

		uint16_t bw;
		ret = f_read(&slave_file, slave_file_buffer, len, &bw);
		if (ret != FR_OK)
		{
			bt_slave_fail(FAIL_FILE, ret);
			break;
		}

		bt_slave_stream.StartPacket(3 + bw);
		bt_slave_stream.Write(CMD_PART);

		b2.uint16 = bw;
		bt_slave_stream.Write(b2.bytes[0]);
		bt_slave_stream.Write(b2.bytes[1]);

		for (uint16_t i=0; i < bw; i++)
			bt_slave_stream.Write(slave_file_buffer[i]);

	break;

	case(CMD_CLOSE_FILE):
		DEBUG("CMD_CLOSE_FILE\n");

		ret = f_close(&slave_file);
		if (ret != FR_OK)
		{
			bt_slave_fail(FAIL_FILE, ret);
			break;
		}

		bt_slave_ok();
	break;

	case(CMD_MEAS):
		DEBUG("CMD_MEAS\n");
//		ret = bt_slave_meas_start();
		ret = 1;

		if (ret == START_MEAS_OK)
			bt_slave_ok();
		else
			bt_slave_fail(FAIL_MEAS, ret);
	break;

	case(CMD_REBOOT):
		DEBUG("CMD_REBOOT\n");

		bt_slave_ok();
		SystemReset();
	break;

	case(CMD_OFF):
		DEBUG("CMD_OFF\n");

		bt_slave_ok();
		task_set(TASK_POWERDOWN);
	break;

	case(CMD_SET_TIME):
		DEBUG("CMD_SET_TIME\n");
		b4.bytes[0] = bt_slave_stream.Read();
		b4.bytes[1] = bt_slave_stream.Read();
		b4.bytes[2] = bt_slave_stream.Read();
		b4.bytes[3] = bt_slave_stream.Read();

		time_set_actual(b4.uint32);

		//Print actual time
		time_str(slave_file_buffer, time_get_actual());
		DEBUG("Time is ... %s\n", slave_file_buffer);

		bt_slave_ok();
	break;

	case(CMD_LIST_DIR):
		DEBUG("CMD_LIST_DIR\n");
		//from
		b2.bytes[0] = bt_slave_stream.Read();
		b2.bytes[1] = bt_slave_stream.Read();

		cnt = bt_slave_stream.Read();

		len = bt_slave_stream.Read();
		for(uint8_t i=0; i < len; i++)
			file_path[i] = bt_slave_stream.Read();
		file_path[len] = 0;

		bt_slave_list(file_path, b2.uint16, cnt);
	break;

	case(CMD_DEL_FILE):
		len = bt_slave_stream.Read();
		for(uint8_t i=0; i < len; i++)
			file_path[i] = bt_slave_stream.Read();
		file_path[len] = 0;

		DEBUG("CMD_DEL_FILE %s, %u\n", file_path, len);

		ret = f_unlink(file_path);

		bt_slave_ok();
	break;

	case(CMD_MV_FILE):
		len = bt_slave_stream.Read();
		for(uint8_t i=0; i < len; i++)
			file_path[i] = bt_slave_stream.Read();
		file_path[len] = 0;

		len = bt_slave_stream.Read();
		for(uint8_t i=0; i < len; i++)
			second_path[i] = bt_slave_stream.Read();
		second_path[len] = 0;

		DEBUG("CMD_MV_FILE %s, %s\n", file_path, second_path);

		f_unlink(second_path);
		ret = f_rename(file_path, second_path);
		if (ret != FR_OK)
			bt_slave_fail(FAIL_FILE, ret);
		else
			bt_slave_ok();
	break;
	}

	led_anim(LED_NO_ANIM);
	led_set(0, 0, 0xFF);
}


uint32_t bt_slave_task_get_time(uint32_t conf_id, uint32_t today_epoch, uint8_t wday, uint32_t epoch)
{
	char filename[64];

	sprintf(filename, "/CONF/%08lX.CFG", conf_id);
	DEBUG("\nReading cfg file: %s\n", filename);
	uint8_t res = f_open(&cfg, filename, FA_READ);

	if (res == FR_OK)
	{
		uint32_t time;
		char value[64];

		//is in allowed range?
		time = cfg_get_int(&cfg, "freq", "begin", 0);
		if (time != 0 && time < epoch)
			return INVALID_TIME;
		DEBUG(" begin = %lu\n", time);

		//is in allowed range?
		time = cfg_get_int(&cfg, "freq", "end", 0);
		if (time != 0 && time > epoch)
			return INVALID_TIME;
		DEBUG(" end = %lu\n", time);

		//is in alowed day?
		if (cfg_get_str(&cfg, "freq", "wday", value))
		{
			DEBUG(" wday = %s\n", value);
			if (strchr(value, wday + '1') == NULL)
				return INVALID_TIME;
			DEBUG(" wday Ok\n");
		}

		//asap
		if(cfg_get_int(&cfg, "freq", "asap", 0))
			return 0;

		//find lowest start time
		uint32_t time_min = INVALID_TIME;
		uint32_t time_next = INVALID_TIME;
		for (uint8_t i=0; i < 150; i++)
		{
			sprintf(value, "start%u", i);
			time = cfg_get_int(&cfg, "freq", value, INVALID_TIME);
			if (time == INVALID_TIME)
				continue;

			DEBUG(" %s > %lu\n", value, time);

			if (time < time_min)
				time_min = time;

			if (time > today_epoch && time < time_next)
				time_next = time;
		}

		f_close(&cfg);

		//no start info
		if (time_min == INVALID_TIME)
			return INVALID_TIME;

		//no start this day
		if (time_next == INVALID_TIME)
			return time_min + (uint32_t)(24 * 3600ul);

		//next start found
		if (time_next != INVALID_TIME)
			return time_next;
	}
	else
	{
		DEBUG(" Could not open file with id %08lX %u\n", conf_id, res);
	}

	return INVALID_TIME;
}

void bt_slave_update_tasks()
{
	DEBUG("bt_slave_update_tasks\n");

	DIR f_dir;

	uint8_t hour;
	uint8_t min;
	uint8_t sec;
	uint8_t day;
	uint8_t wday;
	uint8_t mon;
	uint16_t year;

	uint32_t time = time_get_actual();
	DEBUG("time %lu\n", time);

	datetime_from_epoch(time, &sec, &min, &hour, &day, &wday, &mon, &year);
	DEBUG(" sec %u\n", sec);
	DEBUG(" min %u\n", min);
	DEBUG(" hour %u\n", hour);
	DEBUG(" day %u\n", day);
	DEBUG(" wday %u\n", wday);
	DEBUG(" mon %u\n", mon);
	DEBUG(" year %u\n", year);

	uint32_t today_base = datetime_to_epoch(0, 0, 0, day, mon, year);
	DEBUG("today_base %lu\n", today_base);
	uint32_t today_time = sec + min * 60ul + hour * 3600ul;
	DEBUG("today_time %lu\n", today_time);

	uint32_t conf_id;
	uint32_t time_next;

	uint32_t time_min = INVALID_TIME;
	uint32_t conf_id_min;

	if (f_opendir(&f_dir, "/CONF") == FR_OK)
	{
		FILINFO f_info;

		while(1)
		{
			if (f_readdir(&f_dir, &f_info) == FR_OK)
			{
				if (f_info.fname[0] != '\0')
				{
					DEBUG("> %s\n", f_info.fname);
					if (f_info.fattrib & AM_DIR)
						continue;

					if (f_info.fname[0] == 0xFF)
						continue;

					conf_id = hexfn_to_num(f_info.fname);

					time_next = bt_slave_task_get_time(conf_id, today_time, wday, time);
					if (time_next == INVALID_TIME)
						continue;

					DEBUG("  time_next = %lu\n", time_next);

					if (time_next < time_min)
					{
						time_min = time_next;
						conf_id_min = conf_id;
					}
				}
				else
					break;
			}
			else
				break;
		}
	}

	if (time_min != INVALID_TIME && time + wake_up_period > today_base + time_min)
	{
		DEBUG("meas is first\n");
		time_set_next_wake_up(today_base + time_min);
		eeprom_busy_wait();
		eeprom_update_dword(&ee_slave_meas_conf_id, conf_id_min);
		eeprom_busy_wait();
	}
	else
	{
		DEBUG("period is first\n");
		time_set_next_wake_up(time + wake_up_period);
		eeprom_busy_wait();
		eeprom_update_dword(&ee_slave_meas_conf_id, SLAVE_NO_MEAS_ID);
		eeprom_busy_wait();
	}

	DEBUG("bt_slave_update_tasks done\n");
}

void task_bt_slave_init()
{
	bool init_fail = false;

	DEBUG("Starting BT Slave task\n");

	//protocol object
	bt_slave_stream.Init(bt_pan1322_out, 520);
	bt_slave_stream.RegisterOnPacket(bt_slave_rxpacket);

	//Storage init
	if (!storage_init())
	{
		DEBUG("Could not mount the SD card!\n");
		init_fail = true;
	}

	DEBUG("Reading cfg\n");
	uint8_t ret = f_open(&cfg, "/device.cfg", FA_READ);
	if (ret == FR_OK)
	{
		uint8_t mute = cfg_get_int(&cfg, "cfg", "mute", 0);
		DEBUG(" mute: %u\n", mute);
		buzzer_set_mute(mute);

		//variabile in ms
		bt_timeout = cfg_get_int(&cfg, "cfg", "bt_timeout", BT_TIMEOUT) * 1000lu;
		DEBUG(" bt_timeout: %lu\n", bt_timeout);

		//variabile in seconds
		wake_up_period = cfg_get_int(&cfg, "cfg", "wake_period", WAKE_UP_PERIOD);
		DEBUG(" wake_up_period: %lu\n", wake_up_period);

		f_close(&cfg);
	}

	buzzer_beep(_100ms * 4);

	f_mkdir("/LOGS");
	f_mkdir("/CONF");

	eeprom_busy_wait();
	uint32_t cfg_id = eeprom_read_dword(&ee_slave_meas_conf_id);

	bool start_bt = true;
	if (time_for_wake_up() && cfg_id != SLAVE_NO_MEAS_ID)
	{
		uint8_t ret = bt_slave_meas_start(cfg_id);
		DEBUG("bt_slave_meas_start = %d\n", ret);
		start_bt = ret != START_MEAS_OK;
	}

	if (start_bt)
	{
		//bluetooth radio init
		DEBUG("Bluetooth Init\n");
		bt_init();
		bt_module_init();
		task_bt_slave_reset_timeout();
	}

	if (init_fail)
	{
		DEBUG("HORIBLE error!\n");
		//XXX should warn user
	}
}

void task_bt_slave_reset_timeout()
{
	slave_bt_timeout = task_get_ms_tick() + bt_timeout;
}

void task_bt_slave_stop()
{
	if (slave_meas)
		bt_slave_meas_stop();

	DEBUG("\n\n");

	led_anim(LED_NO_ANIM);
	bt_module_deinit();

	//if file changed
	bt_slave_update_tasks();

	storage_deinit();
}

void task_bt_save_buffer()
{
	while (sd_buffer->Length() > SD_BUFFER_WRITE || (slave_flush && sd_buffer->Length() > 0))
	{
		uint8_t * data_ptr;
		uint16_t wrt;
		uint8_t res;

		uint16_t size = f_size(&slave_file) / 1024;

		DEBUG("BL%d\n", sd_buffer->Length());

		DEBUG("%lu ms\n", task_get_ms_tick());
		DEBUG("WRITING %dkB..", size);

		uint16_t real_write_size = sd_buffer->Read(SD_BUFFER_WRITE, &data_ptr);

		res = f_write(&slave_file, data_ptr, real_write_size, &wrt);

		res = f_sync(&slave_file);

		DEBUG("done\n");
	}
}

void task_bt_slave_loop()
{
	uint32_t ms_time = task_get_ms_tick();

	if (slave_meas)
	{
		if (slave_meas_end)
			if (slave_meas_end < ms_time)
			{
				//do not recurse
				slave_meas_end = 0;

				DEBUG("Time is up!\n");

				bt_slave_meas_stop();

				task_bt_slave_reset_timeout();
				bt_init();
				bt_module_init();
			}

		for (uint8_t i=0; i < SLAVE_SIGNAL_MAX; i++)
		{
			if (slave_signals[i])
				if (slave_signals[i] < ms_time)
				{
					slave_signals[i] = 0;
					buzzer_beep(_100ms * 3);

					//MAKE marker
					uint8_t event_data[1 + 1];
					event_data[0] = make_head(id_event, 5);
					event_data[1] = event_signal;
					uint32_t act_time = task_get_ms_tick() - slave_meas_start;
					sd_buffer->Write(2, event_data);
					sd_buffer->Write(4, (uint8_t*)&act_time);
				}
		}

		//save buffer to sd card
		task_bt_save_buffer();
	}
	else
	{
		//if not connected
		if (!slave_bt_lock.Active())
		{
			if (slave_bt_timeout < ms_time)
				task_set(TASK_POWERDOWN);
		}
	}

}

void bt_slave_button_irqh(uint8_t state)
{
	switch (state)
	{
		case(BUTTON_SHORT):
			if (slave_meas)
			{
				//MAKE marker
				uint8_t event_data[1 + 1];
				event_data[0] = make_head(id_event, 5);
				event_data[1] = event_mark;
				uint32_t act_time = task_get_ms_tick() - slave_meas_start;
				sd_buffer->Write(2, event_data);
				sd_buffer->Write(4, (uint8_t*)&act_time);

				buzzer_beep(_100ms * 10);
			}
		break;

		//power off
		case(BUTTON_HOLD):
			task_set(TASK_POWERDOWN);
		break;
	}
}

void bt_slave_bt_irq(uint8_t * param)
{
//	DEBUG("BT IRQ %d\n", param[0]);

	switch (param[0])
	{
	case (BT_IRQ_PAIR):
		DEBUG("pair sucesfull\n");
	break;

	case (BT_IRQ_CONNECTED):
		DEBUG("connected\n");
		buzzer_beep(100);

		//solid blue light
		led_anim(LED_NO_ANIM);
		led_set(0, 0, 0xFF);

		//say hello to your master
		bt_slave_hello();

		//do not sleep when connected to bt
		slave_bt_lock.Lock();
	break;

	case (BT_IRQ_DISCONNECTED):
		//you can sleep now
		slave_bt_lock.Unlock();
		task_bt_slave_reset_timeout();

		DEBUG("disconnected\n");
		led_anim(LED_BREATHB);
	break;

	case (BT_IRQ_ERROR):
		led_anim(LED_NO_ANIM);
		led_set(0xFF, 0, 0);
		DEBUG("BT Error %d\n", param[1]);
	break;

	case (BT_IRQ_INIT_OK):
		led_anim(LED_BREATHB);
		task_bt_slave_reset_timeout();
	break;

	case (BT_IRQ_DATA):
		bt_slave_stream.Decode(param[1]);
	break;

	case (BT_IRQ_DEINIT):
		slave_bt_lock.Unlock();
	break;
	}
}

void task_bt_slave_irqh(uint8_t type, uint8_t * buff)
{
	int16_t fifo_buffer[16 * 3];

	switch (type)
	{
	//SPI - gpio IRQ
	case(TASK_IRQ_ADS):
		ads1292.ReadData();
		uint8_t ads_data[7];
		ads_data[0] = make_head(id_ads, 6);

		//3 bytes are enough
		memcpy(ads_data + 1, &ads1292.value_ch1, 3);
		memcpy(ads_data + 4, &ads1292.value_ch2, 3);

		sd_buffer->Write(7, ads_data);
	break;

	//I2C - gpio IRQ
	case(TASK_IRQ_ACC):
		SIGNAL2_HI;
		lsm303d.ReadAccStream(fifo_buffer, 16);
		SIGNAL1_HI;

		uint8_t lsm_data[2];
		lsm_data[0] = make_head(id_acc, 0); //0 means that length is next
		lsm_data[1] = 16 * 3 * 2;

		sd_buffer->Write(2, lsm_data);
		sd_buffer->Write(16 * 3 * 2, (uint8_t*)fifo_buffer);
		SIGNAL1_LO;
		SIGNAL2_LO;
	break;

	//I2C - gpio IRQ
	case(TASK_IRQ_MAG):
		uint8_t mag_data[1 + 3 * 2];
		mag_data[0] = make_head(id_mag, 3 * 2);

		lsm303d.ReadMag((int16_t*)(mag_data + 1), (int16_t*)(mag_data + 3), (int16_t*)(mag_data + 5));

		sd_buffer->Write(7, mag_data);
	break;

	//I2C - gpio IRQ
	case(TASK_IRQ_GYRO):
		l3gd20.ReadGyroStream(fifo_buffer, 16);

		uint8_t l3g_data[2];
		l3g_data[0] = make_head(id_gyro, 0); //0 means that length is next
		l3g_data[1] = 16 * 3 * 2;

		sd_buffer->Write(2, l3g_data);
		sd_buffer->Write(16 * 3 * 2, (uint8_t*)fifo_buffer);
	break;

	//I2C - Timer IRQ
	case(TASK_IRQ_BARO):
		uint8_t bmp_data[5];
		bmp_data[0] = make_head(id_bmp, 4);

		memcpy(bmp_data + 1, &bmp180.pressure, 4);

		sd_buffer->Write(5, bmp_data);
	break;

	//ADC - Main loop
	case(TASK_IRQ_BAT):
		uint8_t bat_data[2];

		if (slave_meas)
		{
			bat_data[0] = make_head(id_bat, 1);
			bat_data[1] = *buff;

			sd_buffer->Write(2, bat_data);
		}
	break;

	//I2C - Timer IRQ
	case(TASK_IRQ_TEMPERATURE):
		uint8_t temp_data[3];

		temp_data[0] = make_head(id_temp, 2);
//		DEBUG("TASK_IRQ_TEMPERATURE %d\n", sht21.temperature);
		memcpy(temp_data + 1, &sht21.temperature, 2);

		sd_buffer->Write(3, temp_data);
	break;

	//I2C - Timer IRQ
	case(TASK_IRQ_HUMIDITY):
		uint8_t humi_data[3];

		humi_data[0] = make_head(id_humi, 2);
		memcpy(humi_data + 1, &sht21.humidity, 2);
//		DEBUG("TASK_IRQ_HUMIDITY %d\n", sht21.humidity);

		sd_buffer->Write(3, humi_data);
	break;


	//Gpio - mixed IRQ and main loop
	case(TASK_IRQ_BUTTON):
		bt_slave_button_irqh(*buff);
	break;

	//Gpio - gpio IRQ
	case(TASK_IRQ_USB):
		DEBUG("USB IRQ %d\n", *buff);
		if (*buff == 1)
			task_set(TASK_USB);
	break;

	//Uart - uart IRQ
	case(TASK_IRQ_BT):
		bt_slave_bt_irq(buff);
	break;
	}
}
