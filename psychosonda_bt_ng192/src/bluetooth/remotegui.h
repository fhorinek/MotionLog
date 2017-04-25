/*
 * diaglink.h
 *
 *  Created on: 4.2.2014
 *      Author: horinek
 */

#ifndef REMOTEGUI_H_
#define REMOTEGUI_H_

#include "../drivers/uart.h"

#include <string.h>
#include <xlib/ring.h>

struct Line
{
	uint8_t size;
	uint8_t multi;
	uint8_t flags;
	uint16_t spacing; //in 10us
};

//LINE FLAGS
#define LINE_FLOAT					0b10000000
#define LINE_SIGNED					0b01000000

//MSG TYPES
#define RGUI_TYPE_STREAM			0x00
#define RGUI_TYPE_LINE_TYPE			0x01
#define RGUI_TYPE_LOG				0x02
#define RGUI_TYPE_COMPONENT			0x03
#define RGUI_TYPE_ADD_DATA_SOURCE	0x04
#define RGUI_TYPE_REM_DATA_SOURCE	0x05
#define RGUI_TYPE_CLEAR_SCREEN		0x06
#define RGUI_TYPE_ADD_ACTION		0x07
#define RGUI_TYPE_SCREEN_PROPERTIES	0x08
#define RGUI_TYPE_SWITCH_SCREEN		0x09
#define RGUI_TYPE_REM_COMPONENT		0x0A
#define RGUI_TYPE_LINE_PROPERTIES	0x0B
#define RGUI_TYPE_CREATE_FILE		0x0C
#define RGUI_TYPE_FILE_OP			0x0D
#define RGUI_TYPE_SET_IMAGE			0x0E
#define RGUI_TYPE_SET_PROPERTY		0x0F
#define RGUI_TYPE_SPEAK				0x10
#define RGUI_TYPE_GET_HELLO			0x11
#define RGUI_TYPE_SET_HEARTBEAT		0x12

//from client
#define RGUI_HELLO					0xFF
#define RGUI_FILE					0xFE
#define RGUI_ACTION					0xFD
#define RGUI_VALUE					0xFC

//components types
#define RGUI_COMP_TEXT				0x01
#define RGUI_COMP_BUTTON			0x02
#define RGUI_COMP_CHART				0x03
#define RGUI_COMP_GAUGE				0x04
#define RGUI_COMP_PROGRESSBAR		0x05
#define RGUI_COMP_SEEKBAR			0x06
#define RGUI_COMP_IMAGE				0x07
#define RGUI_COMP_COLORPICKER		0x08

//file operation
#define RGUI_FILE_READ				0x00
#define RGUI_FILE_WRITE				0x01
#define RGUI_FILE_SEEK				0x02
#define RGUI_FILE_SIZE				0x03
#define RGUI_FILE_LOAD				0x04
#define RGUI_FILE_STORE				0x05

//parser state machine
#define RGUI_PARSER_START			0
#define RGUI_PARSER_ID				1
#define RGUI_PARSER_LENH			2
#define RGUI_PARSER_LENL			3
#define RGUI_PARSER_TYPE			4
#define RGUI_PARSER_DATA			5
#define RGUI_PARSER_CRC				6

//message
#define LOG_NORMAL					0
#define LOG_WARNING					1
#define LOG_ERROR					2

//properties
#define RGUI_PROPERTY_X				0x00
#define RGUI_PROPERTY_Y				0x01
#define RGUI_PROPERTY_WIDTH			0x02
#define RGUI_PROPERTY_HEIGHT		0x03
#define RGUI_PROPERTY_SCREEN		0x04

//label + button
#define RGUI_PROPERTY_PREFIX		0xA0
#define RGUI_PROPERTY_SUFIX			0xA1
#define RGUI_PROPERTY_TEXT			0xA2

//layout
#define LAYOUT_WRAP					0xFFFF
#define RGUI_POPUP					0xFF

//etc
#define RGUI_CRC_KEY				0xD5


#define AUTO_ID						0xFF
#define LAST_ID						0xFF

#define RGUI_MAX_LINES				16

class RemoteGUI
{
	Usart * usart = NULL;
	bool debug;

	uint8_t crc;
	uint8_t iter_id;

	void Init();

	void AssembleHead(uint16_t len, uint8_t type);
	void Write(uint8_t c);

	uint8_t StartComponent(uint8_t id, uint8_t type, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t len);
	void SendStr(const char * str);
	void SendBuf(const char * str);
	void SendLineCfg(uint8_t id);

	void FileOperation(uint8_t id, uint8_t op, uint16_t len);

	uint8_t packet_id;
	uint8_t packet_type;

	uint8_t parser_state;
	uint8_t parser_crc;

	uint16_t command_len;
	uint16_t command_pos;

	bool action_pending;
	bool fileop_pending;
	bool hello_pending;
	bool value_pending;

public:
	Line lines[RGUI_MAX_LINES];
	uint8_t * command_buffer = NULL;

	//remote
	uint16_t screen_width;
	uint16_t screen_height;

	//componets
	uint8_t last_id;
	uint8_t auto_id;

	uint8_t component_id;

	//file
	uint8_t file_id;
	uint8_t file_op;
	uint16_t read_size;
	uint32_t file_size;

	//INIT
	void Init(Usart * usart);

	void Log(const char * msg, uint8_t type);
	void Log(const char * msg);
	void Debug();

	//LINE
	void SetLine(uint8_t id, uint8_t size, uint8_t multi, uint8_t flags, uint16_t spacing);
		void SetLineFromType(uint8_t id, uint8_t type);
		void SetLineFromType(uint8_t id, int8_t type);
		void SetLineFromType(uint8_t id, uint16_t type);
		void SetLineFromType(uint8_t id, int16_t type);
		void SetLineFromType(uint8_t id, uint32_t type);
		void SetLineFromType(uint8_t id, int32_t type);
		void SetLineFromType(uint8_t id, float type);

	void SetLineProperties(uint8_t id, uint8_t red, uint8_t green, uint8_t blue, const char * name);

	//STREAM
	void StartStream(uint32_t timestamp, uint16_t len);
	uint16_t GetLineLen(uint8_t id);

	void SendLine(uint8_t id, uint8_t * data);
		void SendLine(uint8_t id, uint8_t data);
		void SendLine(uint8_t id, int8_t data);
		void SendLine(uint8_t id, uint16_t data);
		void SendLine(uint8_t id, int16_t data);
		void SendLine(uint8_t id, uint32_t data);
		void SendLine(uint8_t id, int32_t data);
		void SendLine(uint8_t id, float data);

	void SendCRC();

	//SCREENS
	void ClearScreen(uint8_t screen_id);
	void SetScreenProperties(uint8_t screen_id, const char * name);
	void SwitchScreen(uint8_t screen);

	//COMPONENTS
	uint8_t AddComponentText(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char * prefix, const char * text, const char * sufix);
		uint8_t AddComponentText(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char * text);
	uint8_t AddComponentButton(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char * prefix, const char * text, const char * sufix);
		uint8_t AddComponentButton(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char * text);
	uint8_t AddComponentGauge(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t min, uint16_t max);
	uint8_t AddComponentChart(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float min, float max, uint32_t xwindow);
	uint8_t AddComponentProgressbar(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h,uint16_t max);
	uint8_t AddComponentImage(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h);
	uint8_t AddComponentSeekbar(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t max);
	uint8_t AddComponentColorpicker(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool intensity);
	void RemoveComponent(uint8_t id);

	void AddAction(uint8_t component_id, const char * cmd);
	void AddDataSource(uint8_t component_id, uint8_t id);
	void SetImage(uint8_t id, uint16_t image_id);

	void SetComponentProperty(uint8_t component_id, uint8_t property_id, int val);
	void SetComponentProperty8(uint8_t component_id, uint8_t property_id, uint8_t val);
	void SetComponentProperty16(uint8_t component_id, uint8_t property_id, uint16_t val);
	void SetComponentProperty(uint8_t component_id, uint8_t property_id, const char * val);

	//FILES
	void CreateFile(uint8_t id);
	void FileRead(uint8_t id, uint16_t size);
	void FileWrite(uint8_t id, uint16_t size, uint8_t * buf);
	void FileSeek(uint8_t id, uint32_t pos);
	void FileSize(uint8_t id);
	void FileLoad(uint8_t id);
	void FileStore(uint8_t id);

	//ETC
	void Speak(const char * message);
	void GetHello();
	void SetHeartbeat(uint16_t time, const char * cmd);


	//PARSER
	bool ParserStep(uint8_t c);

	uint8_t * ActionBuffer();
	uint8_t ActionBuffer(uint8_t pos);
	uint8_t * FileBuffer();
	uint8_t * ValueBuffer();

	void ProcessHello();
	void ProcessFileOp();
	void ProcessAction();
	void ProcessValue();

	bool ActionPending();
	bool FileOpPending();
	bool HelloPending();
	bool ValuePending();

	//for value
	uint16_t GetValueProgress();
	uint8_t GetValueColorRed();
	uint8_t GetValueColorGreen();
	uint8_t GetValueColorBlue();
};

#ifdef DEBUG_OUTPUT

#define RGUI_MESSAGE(x...) \
	do { \
		char msg_buff[512];\
		sprintf(msg_buff, x);\
		DEBUG_OUTPUT.Log(msg_buff, LOG_NORMAL);\
	} while(0)

#define RGUI_WARNING(x...) \
	do { \
		char msg_buff[512];\
		sprintf(msg_buff, x);\
		DEBUG_OUTPUT.Log(msg_buff, LOG_WARNING);\
	} while(0)

#define RGUI_ERROR(x...) \
	do { \
		char msg_buff[512];\
		sprintf(msg_buff, x);\
		DEBUG_OUTPUT.Log(msg_buff, LOG_ERROR);\
	} while(0)

#else

#define RGUI_MESSAGE(x...)
#define RGUI_WARNING(x...)
#define RGUI_ERROR(x...)

#endif


extern RemoteGUI rgui;

#endif /* DIAGLINK_H_ */
