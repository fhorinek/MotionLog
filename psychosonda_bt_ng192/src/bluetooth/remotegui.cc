
#include "remotegui.h"
#include "bt.h"

void RemoteGUI::Init(Usart * usart)
{
	this->usart = usart;

	this->Init();
}

void RemoteGUI::Init()
{
	this->debug = false;

	this->iter_id = 0;
	this->parser_state = 0;

	this->last_id = 0;
	this->auto_id = 0;

	this->action_pending = false;
	this->fileop_pending = false;
	this->action_pending = false;
}

void RemoteGUI::Debug()
{
	this->debug = true;
}

void RemoteGUI::Write(uint8_t c)
{
//	this->usart->Write(c);

	putc(c, bt_pan1322_out);

//	DEBUG("%02X ", c);

	this->crc = CalcCRC(this->crc, RGUI_CRC_KEY, c);
}

void RemoteGUI::SetLine(uint8_t id, uint8_t size, uint8_t multi, uint8_t flags, uint16_t spacing)
{
	this->lines[id].size = size;
	this->lines[id].multi = multi;
	this->lines[id].flags = flags;
	this->lines[id].spacing = spacing;

	this->SendLineCfg(id);
}

void RemoteGUI::SetLineFromType(uint8_t id, uint8_t type)
{
	this->SetLine(id, 1, 1, 0, 0);
}

void RemoteGUI::SetLineFromType(uint8_t id, int8_t type)
{
	this->SetLine(id, 1, 1, LINE_SIGNED, 0);
}

void RemoteGUI::SetLineFromType(uint8_t id, uint16_t type)
{
	this->SetLine(id, 2, 1, 0, 0);
}

void RemoteGUI::SetLineFromType(uint8_t id, int16_t type)
{
	this->SetLine(id, 2, 1, LINE_SIGNED, 0);
}

void RemoteGUI::SetLineFromType(uint8_t id, uint32_t type)
{
	this->SetLine(id, 4, 1, 0, 0);
}

void RemoteGUI::SetLineFromType(uint8_t id, int32_t type)
{
	this->SetLine(id, 4, 1, LINE_SIGNED, 0);
}

void RemoteGUI::SetLineFromType(uint8_t id, float type)
{
	this->SetLine(id, 4, 1, LINE_FLOAT, 0);
}

void RemoteGUI::SendLineCfg(uint8_t id)
{
	byte2_u tmp;
	tmp.uint16 = this->lines[id].spacing;

	this->AssembleHead(6, RGUI_TYPE_LINE_TYPE);

	this->Write(id);
	this->Write(this->lines[id].size);
	this->Write(this->lines[id].multi);
	this->Write(this->lines[id].flags);
	this->Write(tmp.bytes[1]);
	this->Write(tmp.bytes[0]);

	this->SendCRC();
}

uint16_t RemoteGUI::GetLineLen(uint8_t id)
{
	return this->lines[id].size * this->lines[id].multi + 1;
}

void RemoteGUI::AssembleHead(uint16_t len, uint8_t type)
{
	byte2_u b2;

	b2.uint16 = len;

	// ** for bt module
	bt_pan1322.StreamHead(len + 6);
	// ** end

	this->crc = 0x00;

	this->Write(0xAA);
	this->Write(this->iter_id);
	this->Write(b2.bytes[1]);//avr is little
	this->Write(b2.bytes[0]);
	this->Write(type);

	this->iter_id++;
}

void RemoteGUI::SendCRC()
{
	this->Write(this->crc);

	// ** for bt module
	bt_pan1322.StreamTail();
	// ** end

	if (this->debug)
		printf(" END\n");
}

void RemoteGUI::StartStream(uint32_t timestamp, uint16_t len)
{
	byte4_u b4;

	b4.uint32 = timestamp;

	uint16_t length = 3 + len; //timestamp + data_len

	this->AssembleHead(length, RGUI_TYPE_STREAM);
	//timestamp
	this->Write(b4.bytes[2]);
	this->Write(b4.bytes[1]);
	this->Write(b4.bytes[0]);
}

void RemoteGUI::SendLine(uint8_t id, uint8_t data)
{
	this->SendLine(id, &data);
}

void RemoteGUI::SendLine(uint8_t id, int8_t data)
{
	this->SendLine(id, (uint8_t *) &data);
}

void RemoteGUI::SendLine(uint8_t id, uint16_t data)
{
	this->SendLine(id, (uint8_t *) &data);
}

void RemoteGUI::SendLine(uint8_t id, int16_t data)
{
	this->SendLine(id, (uint8_t *) &data);
}

void RemoteGUI::SendLine(uint8_t id, uint32_t data)
{
	this->SendLine(id, (uint8_t *) &data);
}

void RemoteGUI::SendLine(uint8_t id, int32_t data)
{
	this->SendLine(id, (uint8_t *) &data);
}

void RemoteGUI::SendLine(uint8_t id, float data)
{
	this->SendLine(id, (uint8_t *) &data);
}

void RemoteGUI::SendLine(uint8_t id, uint8_t * data)
{
	uint8_t size = this->lines[id].size;
	uint8_t multi = this->lines[id].multi;

	//write id
	this->Write(id);

	for (uint8_t i = 0; i < multi; i++)
		for (uint8_t j = 0; j < size; j++)
		{
			uint16_t adr = i * size + (size - j - 1); //avr is little
			this->Write(data[adr]);
		}
}

void RemoteGUI::AddDataSource(uint8_t component_id, uint8_t id)
{
	this->AssembleHead(2, RGUI_TYPE_ADD_DATA_SOURCE);

	if (component_id == LAST_ID)
		component_id = this->last_id;

	this->Write(component_id);
	this->Write(id);

	this->SendCRC();
}

uint8_t RemoteGUI::StartComponent(uint8_t id, uint8_t type, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t len)
{
	this->AssembleHead(11 + len, RGUI_TYPE_COMPONENT);

	if (id == AUTO_ID)
		id = this->auto_id++;

	this->last_id = id;

	this->Write(id); //component id
	this->Write(screen); //screen

	this->Write(type); //component type

	byte2_u b2;

	b2.uint16 = x;
	this->Write(b2.bytes[1]); //x pos
	this->Write(b2.bytes[0]);

	b2.uint16 = y;
	this->Write(b2.bytes[1]); //y pos
	this->Write(b2.bytes[0]);

	b2.uint16 = w;
	this->Write(b2.bytes[1]); //width
	this->Write(b2.bytes[0]);

	b2.uint16 = h;
	this->Write(b2.bytes[1]); //height
	this->Write(b2.bytes[0]);

	return id;
}

void RemoteGUI::SendStr(const char * str)
{
	uint8_t len = strlen(str);
	this->Write(len);

	for (uint8_t i = 0; i < len; i++)
		this->Write(str[i]);
}

void RemoteGUI::ClearScreen(uint8_t screen_id)
{
	this->AssembleHead(1, RGUI_TYPE_CLEAR_SCREEN);
	this->Write(screen_id);

	this->SendCRC();
}

uint8_t RemoteGUI::AddComponentText(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char * prefix, const char * text, const char * sufix)
{
	uint16_t len = 3;
	len += strlen(prefix);
	len += strlen(sufix);
	len += strlen(text);

	id = this->StartComponent(id, RGUI_COMP_TEXT, screen, x, y, w, h, len);

	this->SendStr(prefix);
	this->SendStr(sufix);
	this->SendStr(text);

	this->SendCRC();

	return id;
}

uint8_t RemoteGUI::AddComponentText(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char * text)
{
	return this->AddComponentText(id, screen, x, y, w, h, "", text, "");
}


uint8_t RemoteGUI::AddComponentButton(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char * prefix, const char * text, const char * sufix)
{
	uint16_t len = 3;
	len += strlen(prefix);
	len += strlen(sufix);
	len += strlen(text);

	id = this->StartComponent(id, RGUI_COMP_BUTTON, screen, x, y, w, h, len);

	this->SendStr(prefix);
	this->SendStr(sufix);
	this->SendStr(text);

	this->SendCRC();

	return id;
}

uint8_t RemoteGUI::AddComponentButton(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const char * text)
{
	return this->AddComponentButton(id, screen, x, y, w, h, "", text, "");
}


uint8_t RemoteGUI::AddComponentGauge(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t min, uint16_t max)
{
	uint16_t len = 4;

	id = this->StartComponent(id, RGUI_COMP_GAUGE, screen, x, y, w, h, len);

	byte2_u b2;

	b2.uint16 = min;
	this->Write(b2.bytes[1]); //min
	this->Write(b2.bytes[0]);

	b2.uint16 = max;
	this->Write(b2.bytes[1]); //max
	this->Write(b2.bytes[0]);

	this->SendCRC();

	return id;
}

uint8_t RemoteGUI::AddComponentChart(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, float min, float max, uint32_t xwindow)
{
	uint16_t len = 11;

	id = this->StartComponent(id, RGUI_COMP_CHART, screen, x, y, w, h, len);

	byte4_u b4;

	b4.flt = min;
	this->Write(b4.bytes[3]);
	this->Write(b4.bytes[2]);
	this->Write(b4.bytes[1]);
	this->Write(b4.bytes[0]);

	b4.flt = max;
	this->Write(b4.bytes[3]);
	this->Write(b4.bytes[2]);
	this->Write(b4.bytes[1]);
	this->Write(b4.bytes[0]);

	b4.uint32 = xwindow;
	this->Write(b4.bytes[2]);
	this->Write(b4.bytes[1]);
	this->Write(b4.bytes[0]);

	this->SendCRC();

	return id;
}


uint8_t RemoteGUI::AddComponentProgressbar(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t max)
{
	uint16_t len = 2;

	id = this->StartComponent(id, RGUI_COMP_PROGRESSBAR, screen, x, y, w, h, len);

	byte2_u b2;

	b2.uint16 = max;
	this->Write(b2.bytes[1]); //max
	this->Write(b2.bytes[0]);

	this->SendCRC();

	return id;
}

uint8_t RemoteGUI::AddComponentImage(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h)
{
	id = this->StartComponent(id, RGUI_COMP_IMAGE, screen, x, y, w, h, 0);

	this->SendCRC();

	return id;
}

uint8_t RemoteGUI::AddComponentSeekbar(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t max)
{
	uint16_t len = 2;

	id = this->StartComponent(id, RGUI_COMP_SEEKBAR, screen, x, y, w, h, len);

	byte2_u b2;

	b2.uint16 = max;
	this->Write(b2.bytes[1]); //max
	this->Write(b2.bytes[0]);

	this->SendCRC();

	return id;
}

uint8_t RemoteGUI::AddComponentColorpicker(uint8_t id, uint8_t screen, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool intensity)
{
	uint16_t len = 1;

	id = this->StartComponent(id, RGUI_COMP_COLORPICKER, screen, x, y, w, h, len);

	this->Write(intensity);

	this->SendCRC();

	return id;
}

void RemoteGUI::AddAction(uint8_t component_id, const char * cmd)
{
	uint16_t len = 2;
	len += strlen(cmd);

	this->AssembleHead(len, RGUI_TYPE_ADD_ACTION);

	if (component_id == LAST_ID)
		component_id = this->last_id;

	this->Write(component_id);
	this->SendStr(cmd);

	this->SendCRC();
}

bool RemoteGUI::ParserStep(uint8_t c)
{
//	DEBUG(">> %02X step: %d\n", c, this->parser_state);

	switch(this->parser_state)
	{

	case(RGUI_PARSER_START):
		if (c == 0xAA)
			this->parser_state++;
			this->parser_crc = 0x00;
	break;

	case(RGUI_PARSER_ID):
		this->packet_id = c;
		this->parser_state++;
	break;

	case(RGUI_PARSER_LENH):
		this->command_len = c << 8;
		this->parser_state++;
	break;

	case(RGUI_PARSER_LENL):
		this->command_len |= c;

		free(this->command_buffer);

		this->command_buffer = (uint8_t *) malloc((1 + this->command_len) * sizeof(uint8_t));
		this->command_pos = 0;

		this->parser_state++;
	break;

	case(RGUI_PARSER_TYPE):
		this->packet_type = c;
		this->parser_state++;
	break;


	case(RGUI_PARSER_DATA):
		this->command_buffer[this->command_pos] = c;

		if (this->command_pos + 1 < this->command_len)
			this->command_pos++;
		else
		{
			this->command_buffer[this->command_len] = 0;
			this->parser_state++;
		}

	break;

	case(RGUI_PARSER_CRC):
		this->parser_state = RGUI_PARSER_START;

		if (this->parser_crc == c)
		{
			//process Hello
			if (this->packet_type == RGUI_HELLO)
				this->ProcessHello();

			//process FileOP
			if (this->packet_type == RGUI_FILE)
				this->ProcessFileOp();

			//process Action
			if (this->packet_type == RGUI_ACTION)
				this->ProcessAction();

			//process Value
			if (this->packet_type == RGUI_VALUE)
				this->ProcessValue();

			return true;
		}
		else
		{
			DEBUG(">> CRC error %02X != %02X\n", this->parser_crc, c);
			return false;
		}

	break;
	}

	this->parser_crc = CalcCRC(this->parser_crc, RGUI_CRC_KEY, c);

	return false;
}

void RemoteGUI::SwitchScreen(uint8_t screen)
{
	this->AssembleHead(1, RGUI_TYPE_SWITCH_SCREEN);
	this->Write(screen);
	this->SendCRC();
}

void RemoteGUI::RemoveComponent(uint8_t id)
{
	this->AssembleHead(1, RGUI_TYPE_REM_COMPONENT);
	this->Write(id);
	this->SendCRC();
}

void RemoteGUI::ProcessHello()
{
	this->screen_width = this->command_buffer[0] << 8 | this->command_buffer[1];
	this->screen_height = this->command_buffer[2] << 8 | this->command_buffer[3];

	this->hello_pending = true;
}

void RemoteGUI::ProcessFileOp()
{
	this->file_id = this->command_buffer[0];
	this->file_op = this->command_buffer[1];

	if (this->file_op == RGUI_FILE_READ)
		this->read_size = this->command_buffer[2] << 8 | this->command_buffer[3];

	if (this->file_op == RGUI_FILE_SIZE)
		this->file_size = ((uint32_t)this->command_buffer[2] << 24) | ((uint32_t)this->command_buffer[3] << 16) | ((uint32_t)this->command_buffer[4] << 8) | this->command_buffer[5];

	this->fileop_pending = true;
}

void RemoteGUI::ProcessAction()
{
	this->component_id = this->command_buffer[0];

	this->action_pending = true;
}

void RemoteGUI::ProcessValue()
{
	this->component_id = this->command_buffer[0];

	this->value_pending = true;
}

bool RemoteGUI::ActionPending()
{
	if (!this->action_pending)
		return false;
	this->action_pending = false;
	return true;
}

bool RemoteGUI::FileOpPending()
{
	if (!this->fileop_pending)
		return false;
	this->fileop_pending = false;
	return true;
}

bool RemoteGUI::HelloPending()
{
	if (!this->hello_pending)
		return false;
	this->hello_pending = false;
	return true;
}

bool RemoteGUI::ValuePending()
{
	if (!this->value_pending)
		return false;
	this->value_pending = false;
	return true;
}

uint8_t * RemoteGUI::FileBuffer()
{
	return this->command_buffer + 4;
}

uint8_t * RemoteGUI::ActionBuffer()
{
	return this->command_buffer + 1;
}

uint8_t RemoteGUI::ActionBuffer(uint8_t pos)
{
	return *(this->command_buffer + 1 + pos);
}

uint16_t RemoteGUI::GetValueProgress()
{
	return this->command_buffer[1] << 8 | this->command_buffer[2];
}

uint8_t RemoteGUI::GetValueColorRed()
{
	return this->command_buffer[1];
}

uint8_t RemoteGUI::GetValueColorGreen()
{
	return this->command_buffer[2];
}

uint8_t RemoteGUI::GetValueColorBlue()
{
	return this->command_buffer[3];
}

void RemoteGUI::SetLineProperties(uint8_t id, uint8_t red, uint8_t green, uint8_t blue, const char * name)
{
	uint16_t len = 5;
	len += strlen(name);

	this->AssembleHead(len, RGUI_TYPE_LINE_PROPERTIES);

	this->Write(id);

	this->Write(red);
	this->Write(green);
	this->Write(blue);

	this->SendStr(name);

	this->SendCRC();
}

void RemoteGUI::SetScreenProperties(uint8_t screen_id, const char * name)
{
	uint16_t len = 2;
	len += strlen(name);

	this->AssembleHead(len, RGUI_TYPE_SCREEN_PROPERTIES);

	this->Write(screen_id);
	this->SendStr(name);

	this->SendCRC();
}

void RemoteGUI::Log(const char * msg, uint8_t type = LOG_NORMAL)
{
	uint16_t len = 2;
	len += strlen(msg);

	this->AssembleHead(len, RGUI_TYPE_LOG);

	this->Write(type);
	this->SendStr(msg);

	this->SendCRC();
}

void RemoteGUI::SetImage(uint8_t id, uint16_t image_id)
{
	this->AssembleHead(3, RGUI_TYPE_SET_IMAGE);

	this->Write(id);

	byte2_u b2;

	b2.uint16 = image_id;
	this->Write(b2.bytes[1]);
	this->Write(b2.bytes[0]);

	this->SendCRC();
}

void RemoteGUI::CreateFile(uint8_t id)
{
	this->AssembleHead(1, RGUI_TYPE_CREATE_FILE);
	this->Write(id);
	this->SendCRC();
}

void RemoteGUI::FileOperation(uint8_t id, uint8_t op, uint16_t len)
{
	this->AssembleHead(len + 2, RGUI_TYPE_FILE_OP);

	this->Write(id);
	this->Write(op);
}

void RemoteGUI::FileRead(uint8_t id, uint16_t size)
{
	this->FileOperation(id, RGUI_FILE_READ, 2);

	byte2_u b2;

	b2.uint16 = size;
	this->Write(b2.bytes[1]);
	this->Write(b2.bytes[0]);

	this->SendCRC();
}

void RemoteGUI::FileWrite(uint8_t id, uint16_t size, uint8_t * buf)
{
	this->FileOperation(id, RGUI_FILE_WRITE, 2 + size);

	byte2_u b2;

	b2.uint16 = size;
	this->Write(b2.bytes[1]);
	this->Write(b2.bytes[0]);

	for (uint16_t i = 0; i < size; i++)
		this->Write(buf[i]);

	this->SendCRC();
}

void RemoteGUI::FileSeek(uint8_t id, uint32_t pos)
{
	this->FileOperation(id, RGUI_FILE_SEEK, 4);

	byte4_u b4;

	b4.uint32 = pos;
	this->Write(b4.bytes[3]);
	this->Write(b4.bytes[2]);
	this->Write(b4.bytes[1]);
	this->Write(b4.bytes[0]);

	this->SendCRC();
}

void RemoteGUI::FileSize(uint8_t id)
{
	this->FileOperation(id, RGUI_FILE_SIZE, 0);

	this->SendCRC();
}

void RemoteGUI::FileLoad(uint8_t id)
{
	this->FileOperation(id, RGUI_FILE_LOAD, 0);

	this->SendCRC();
}

void RemoteGUI::FileStore(uint8_t id)
{
	this->FileOperation(id, RGUI_FILE_STORE, 0);

	this->SendCRC();
}

void RemoteGUI::SetComponentProperty(uint8_t component_id, uint8_t property_id, int val)
{
	switch(property_id)
	{
	case(RGUI_PROPERTY_X):
	case(RGUI_PROPERTY_Y):
	case(RGUI_PROPERTY_WIDTH):
	case(RGUI_PROPERTY_HEIGHT):
		this->SetComponentProperty16(component_id, property_id, val);
	break;

	case(RGUI_PROPERTY_SCREEN):
		this->SetComponentProperty16(component_id, property_id, val);
	break;

	}
}

void RemoteGUI::SetComponentProperty8(uint8_t component_id, uint8_t property_id, uint8_t val)
{
	this->AssembleHead(3, RGUI_TYPE_SET_PROPERTY);

	this->Write(component_id);
	this->Write(property_id);
	this->Write(val);

	this->SendCRC();
}

void RemoteGUI::SetComponentProperty16(uint8_t component_id, uint8_t property_id, uint16_t val)
{
	this->AssembleHead(4, RGUI_TYPE_SET_PROPERTY);

	this->Write(component_id);
	this->Write(property_id);

	byte2_u b2;

	b2.uint16 = val;
	this->Write(b2.bytes[1]);
	this->Write(b2.bytes[0]);

	this->SendCRC();
}
void RemoteGUI::SetComponentProperty(uint8_t component_id, uint8_t property_id, const char * val)
{
	uint16_t len = 3;
	len += strlen(val);

	this->AssembleHead(len, RGUI_TYPE_SET_PROPERTY);

	this->Write(component_id);
	this->Write(property_id);

	this->SendStr(val);

	this->SendCRC();

}

void RemoteGUI::Speak(const char * message)
{
	uint16_t len = 1;
	len += strlen(message);

	this->AssembleHead(len, RGUI_TYPE_SPEAK);

	this->SendStr(message);

	this->SendCRC();
}

void RemoteGUI::GetHello()
{
	this->AssembleHead(0, RGUI_TYPE_GET_HELLO);

	this->SendCRC();
}

void RemoteGUI::SetHeartbeat(uint16_t time, const char * cmd)
{
	uint16_t len = 1 + 2;
	len += strlen(cmd);

	this->AssembleHead(len, RGUI_TYPE_SET_HEARTBEAT);

	byte2_u b2;

	b2.uint16 = time;
	this->Write(b2.bytes[1]);
	this->Write(b2.bytes[0]);
	this->SendStr(cmd);

	this->SendCRC();
}
