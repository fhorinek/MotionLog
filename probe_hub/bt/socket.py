from time import time
import common.log as log
import struct
import os

#commands
CMD_HELLO      = 0
CMD_PUSH_FILE  = 1
CMD_PULL_FILE  = 2
CMD_PUSH_PART  = 3
CMD_PULL_PART  = 4
CMD_CLOSE_FILE = 5
CMD_MEAS       = 6
CMD_REBOOT     = 7
CMD_OFF        = 8
CMD_SET_TIME   = 9
CMD_LIST_DIR   = 10
CMD_DEL_FILE   = 11
CMD_MV_FILE    = 12

#responses
CMD_RET_OK     = 0
CMD_RET_FAIL   = 1
CMD_ID         = 2
CMD_PART       = 3
CMD_DIR_LIST   = 4

WAIT_HELLO      = 0
SET_TIME        = 1
PULL_CFG        = 2
PULL_CFG_PART   = 3
CLOSE_CFG       = 4
LIST_CONF       = 5
LIST_LOGS       = 6
PULL_LOG        = 7
PULL_LOG_PART   = 8
CLOSE_LOG       = 9
DEL_LOG         = 10
PUSH_CONF       = 11
CLOSE_CONF      = 12
DEL_CONF        = 13
PUSH_FW         = 14
CLOSE_FW        = 15
RENAME_FW       = 16
WAIT_FOR_SERVER = 17

IDLE        = 0
LENGTH_LOW  = 1
LENGTH_HI   = 2
DATA        = 3
CRC         = 4

CRC_KEY = 0xD5 #CRC-8
START_BYTE = 0xC0

TIMEOUT = 1
STREAM_OVERHEAD = 7 + 4 + 10

PACKET_TX_TIMEOUT = 5
PACKET_MAX_RETRY = 3

class socket(log.Logger):
    def __init__(self, addr, parent):
        log.Logger.__init__(self, "sock " + addr, "logs/sock_%s.log" % addr.replace(":", "_"))

        self.log("Connecting to " + addr, log.INFO)
        self.addr = addr
        self.parent = parent
        self.reset_parser()
        self.step = WAIT_HELLO
        self.is_alive = True
        
        self.rx_mtu = 64
        self.tx_mtu = 64
        
        self.last_packet = None
        self.last_packet_tx_time = 0
        self.last_packet_retry = 0
        
        self.got_conf_from_server = False
       
    def acquired(self):
        self.parent.acquired(self.addr)
       
    def alive(self):
        return self.is_alive
       
    def end(self, reboot = False):
        if reboot:
            self.send_cmd(CMD_REBOOT)
            self.log("Rebooting", log.INFO)
        else:
            self.send_cmd(CMD_OFF)
            self.log("Powering off", log.INFO)
            
        self.parent.release(self.addr)
        self.is_alive = False
       
    def reset_parser(self):
        self.parser_state = IDLE

    def set_time(self):
        self.log("setting time", log.INFO)
        #XXX this is hack FIXME
        t = int(time() + (2 * 60 * 60))
        line = [CMD_SET_TIME]
        line += map(ord, struct.pack("<L", t))
        
        self.tx_packet(line)
        
        self.step = SET_TIME

        
    def list_dir(self, path, start = 0):
        self.log("listing dir " + path + " from " + str(start), log.INFO)
            
        self.dir_path = path
        self.dir_start = start
        
        cnt = 35
        
        line = [CMD_LIST_DIR]
        line += [(start & 0x00FF) >> 0]
        line += [(start & 0xFF00) >> 8]         
        line += [cnt]
        line += [len(path)]
        line += map(ord, path)    
        
        self.tx_packet(line)    
        
    def send_cmd(self, cmd):
        self.log("sending command %02X" % cmd, log.INFO)
        line = []
        line.append(cmd)
        
        self.tx_packet(line)

    def push_fw(self):
        path = "/UPDATE.TMP"
        self.file_name = path
        self.file_data = self.new_fw
        self.file_pos = 0
        self.push_file(path)      
        
        self.step = PUSH_FW      

    def push_next_conf(self):
        name = self.conf_add.keys().pop()
        data = self.conf_add.pop(name)
        
        if name == "CFG":
            path = "/device.cfg"
        else:  
            path = "/conf/" + name
             
        self.file_name = path
        self.file_data = data
        self.file_pos = 0
        self.push_file(path)      
        
        self.step = PUSH_CONF

    def push_file(self, path):
        self.log("pushing file %s" % path, log.INFO)
        line = []
        line += [CMD_PUSH_FILE]
        line += [len(path)]
        line += map(ord, path)
        
        self.tx_packet(line)
     
    def rename_fw(self):
        line = []
        line += [CMD_MV_FILE]
        path = "UPDATE.TMP"
        line += [len(path)]
        line += map(ord, path)
        path = "UPDATE.BIN"
        line += [len(path)]
        line += map(ord, path)
        
        self.tx_packet(line)  
        
        self.step = RENAME_FW   
     
    def push_next(self):
        done = False
        
        chunk = self.tx_mtu - STREAM_OVERHEAD
        
        if (chunk + self.file_pos > len(self.file_data)):
            chunk = len(self.file_data) - self.file_pos
            done = True
        
        line = [CMD_PUSH_PART]
        line += [(chunk & 0x00FF) >> 0]
        line += [(chunk & 0xFF00) >> 8]
        line += map(ord, struct.pack("<L", self.file_pos))
        
        start = self.file_pos
        end = self.file_pos + chunk
        
        data = self.file_data[start:end]
        line += map(ord, data) 
        
        self.tx_packet(line)
        
        self.file_pos += chunk
        
        return done        
     
    def pull_cfg(self):
        path = "/device.cfg" 
        self.file_name = path
        self.file_data = ""  
        self.pull_file(path)    
        self.step = PULL_CFG
     
    def pull_next_log(self):
        path = "/logs/" + self.logs.pop()
        self.file_name = path
        self.file_data = ""  
        self.pull_file(path)     
        self.step = PULL_LOG
 
        
    def pull_file(self, path):
        self.log("pulling file %s" % path, log.INFO)
        line = []
        line += [CMD_PULL_FILE]
        line += [len(path)]
        line += map(ord, path)
        
        self.tx_packet(line)
        
    def del_file(self, path):
        self.log("deleting file %s" % path, log.INFO)
        line = []
        line += [CMD_DEL_FILE]
        line += [len(path)]
        line += map(ord, path)
        
        self.tx_packet(line)        
        
    def pull_next(self):
        chunk = self.rx_mtu - STREAM_OVERHEAD
        file_pos = len(self.file_data)
       
        line = [CMD_PULL_PART]
        line += [chunk & 0x00FF]
        line += [(chunk & 0xFF00) >> 8]
        line += map(ord, struct.pack("<L", file_pos))
       
        self.tx_packet(line)  
        
    def list_logs(self):
        self.list_dir("/logs")
        self.step = LIST_LOGS
        self.logs = []

    def list_conf(self):
        self.step = LIST_CONF
        self.list_dir("/conf")
        self.conf = []                
        

    def process_logs(self, data):
        in_dir = (data[2] << 8) | data[1]
        listed = data[3]            
        for i in range(listed):
            index = 4 + i * 12
            fname = "".join(map(chr, data[index:index + 12]))
            self.logs.append(fname)    
            
        if in_dir > self.dir_start + listed:
            self.list_dir(self.dir_path, self.dir_start + listed)
        else:
            return True
        
        return False
        
    def process_pull_head(self, data):
        len_s =  "".join(map(chr, data[1:]))
        self.file_size, = struct.unpack("<L", len_s)

    def pull_cfg_part(self):
        self.pull_next()
        self.step = PULL_CFG_PART
        
    def pull_log_part(self):
        self.pull_next()
        self.step = PULL_LOG_PART

    def process_pull_data(self, data):
        length = (data[2] << 8) | data[1]
        for i in range(length):
            self.file_data += chr(data[3 + i])
            
        if len(self.file_data) < self.file_size:
            return False
        else:
            return True

    def close_cfg_file(self):
        self.send_cmd(CMD_CLOSE_FILE)
        self.step = CLOSE_CFG    
        
    def close_log_file(self):
        self.send_cmd(CMD_CLOSE_FILE)
        self.step = CLOSE_LOG      

    def close_conf_file(self):
        self.send_cmd(CMD_CLOSE_FILE)
        self.step = CLOSE_CONF   

    def close_fw(self):
        self.send_cmd(CMD_CLOSE_FILE)
        self.step = CLOSE_FW  

    def del_log_file(self, path):
        self.del_file(path)
        self.step = DEL_LOG        
        
    def process_conf(self, data):
        in_dir = (data[2] << 8) | data[1]
        listed = data[3]            
        for i in range(listed):
            index = 4 + i * 12
            fname = "".join(map(chr, data[index:index + 12]))
            self.conf.append(fname)    
            
        if in_dir > self.dir_start + listed:
            self.list_dir(self.dir_path, self.dir_start + listed)
        else:
            return True
        
        return False        
        
    def del_next_conf_file(self):
        self.del_file("conf/" + self.conf_rem.pop())
        self.step = DEL_CONF
        
        
    def configure_device(self):
        if len(self.conf_add) > 0:
            self.push_next_conf()
            return 
        
        if len(self.conf_rem) > 0:
            self.del_next_conf_file()
            return
        
        if self.new_fw:
            self.push_fw()
            return
            
        self.log("No changes in configuration", log.INFO)
        self.end()        
        
    def rx_packet(self, data):
        self.log("RX: " + str(data), log.DEBUG)

        if self.step == WAIT_HELLO:
            self.fw = "".join(map(chr, data[1:33]))
            self.bat = data[33]
            try:
                self.bat_raw, = struct.unpack("<h","".join(map(chr, data[34:36])))
            except:
                self.bat_raw = 0
                
            self.set_time()
            return
            
        if self.step == SET_TIME:
            #assuming OK
            self.pull_cfg()
            self.cfg = None
            return 
    
        if self.step == PULL_CFG:
            if data[0] == CMD_RET_OK:
                self.process_pull_head(data)
                self.pull_cfg_part()
            else:
                self.list_conf()
            return
        
        if self.step == PULL_CFG_PART:
            if not self.process_pull_data(data):
                #file not complete
                self.pull_cfg_part()
            else:
                #file complete
                self.cfg = self.file_data
                #close file
                self.close_cfg_file()
            return            
        
        if self.step == CLOSE_CFG:
            self.list_conf()
            return
    
        if self.step == LIST_CONF:
            if self.process_conf(data):
                self.log("Listing confs: %s" % str(self.conf), log.INFO)
                #ask server if there is any change in conf, report fw version and battery level
                self.parent.get_conf(self.addr, self.conf, self.fw, self.cfg, [self.bat, self.bat_raw])
                self.list_logs()
            return        
    
        if self.step == LIST_LOGS:
            #if listing is complete
            if self.process_logs(data):
                self.log("Listing logs: %s" % str(self.logs), log.INFO)
                if len(self.logs) > 0:
                    #download logs from device
                    self.pull_next_log()
                    
                else:
                    #configure device
                    if self.got_conf_from_server:
                        self.configure_device()
                    else:
                        self.step = WAIT_FOR_SERVER
            return      
                
        if self.step == PULL_LOG:
            self.process_pull_head(data)
            self.pull_log_part()            
            return
            
        if self.step == PULL_LOG_PART:
            if not self.process_pull_data(data):
                #file not complete
                self.pull_log_part()
            else:
                #file complete 
                #send to server
                name = os.path.basename(self.file_name)
                self.parent.push_log(self.addr, name, self.file_data)
                #close file
                self.close_log_file()
            return
        
        if self.step == CLOSE_LOG:
            #delete log from device
            self.del_log_file(self.file_name)
            return
        
        if self.step == DEL_LOG:
            if len(self.logs) > 0:
                #pull next file
                self.pull_next_log()
            else:
                #continue
                if self.got_conf_from_server:
                    self.configure_device()
                else:
                    self.step = WAIT_FOR_SERVER
            return
        
        if self.step == PUSH_CONF:
            if self.push_next():
                self.close_conf_file()
            return
        
        if self.step == CLOSE_CONF:
            if len(self.conf_add) > 0:
                self.push_next_conf()
            else:
                if len(self.conf_rem) > 0:
                    self.del_next_conf_file()
                else:
                    if self.new_fw:
                        self.push_fw()
                    else:
                        self.end()
            return        
        
        if self.step == DEL_CONF:
            if len(self.conf_rem) > 0:
                self.del_next_conf_file()
            else:
                if self.new_fw:
                    self.push_fw()
                else:
                    self.end()
            return           
        
        if self.step == PUSH_FW:
            if self.push_next():
                self.close_fw()
            return
        
        if self.step == CLOSE_FW:
            self.rename_fw()
            return
        
        if self.step == RENAME_FW:
            self.end(True)
            return            
        
    def config(self, add, rem, fw):
        self.conf_add = add
        self.conf_rem = rem
        self.new_fw = fw
        
        self.got_conf_from_server = True
        if self.step == WAIT_FOR_SERVER:
            self.configure_device()
        
        
    def work(self):
        working = False
        
        data = self.read()
        if data:
            working = True
            
        for c in data: 
            self.parse(c)
            
        if self.last_packet:
            working = True
            
            if time() - self.last_packet_tx_time > PACKET_TX_TIMEOUT:
                self.log("TX packet timeout", log.WARN)
                
                if self.last_packet_retry >= PACKET_MAX_RETRY:
                    self.log("Max retry count reached", log.ERROR)
                    self.parent.release(self.addr)
                    self.is_alive = False
                else:
                    self.write(self.last_packet)
                    self.last_packet_tx_time = time()
                    self.last_packet_retry += 1
                    self.log(" retry %d" % self.last_packet_retry, log.INFO)

        return working

    def read(self):
        return False
    
    def write(self, data):
        pass
    
    def calc_crc(self, csum, data):
        for i in range(0, 8):
            if ((data & 0x01) ^ (csum & 0x01)):
                csum = (csum >> 1) % 0x100 
                csum = (csum ^ CRC_KEY) % 0x100
            else:
                csum = (csum >> 1) % 0x100
            data = (data >> 1) % 0x100
            
        return csum        
    
    def tx_packet(self, data):
        if (len(data) == 0):
            return
        
        to_send = []
        
        to_send.append(0xC0)
        to_send.append((len(data) & 0x00FF) >> 0)
        crc = self.calc_crc(0x00, (len(data) & 0x00FF) >> 0)
        to_send.append((len(data) & 0xFF00) >> 8)
        crc = self.calc_crc(crc, (len(data) & 0xFF00) >> 8)

        for byte in data:
            to_send.append(byte)
            crc = self.calc_crc(crc, byte)
            
        to_send.append(crc)
        
        self.log("TX: " + str(to_send), log.DEBUG)
        
        self.write(to_send)
        
        self.last_packet = to_send
        self.last_packet_tx_time = time()
        self.last_packet_retry = 0

    def parse(self, c):
        if (self.parser_state == IDLE):
            if (c == START_BYTE):
                self.parser_state  = LENGTH_LOW
            return
            
        if (self.parser_state == LENGTH_LOW):
            self.data_len = c
            self.parser_state = LENGTH_HI
            self.crc = self.calc_crc(0x00, c)
            return
        
        if (self.parser_state == LENGTH_HI):
            self.data_len |= (c << 8)
            self.parser_state = DATA
            self.data = []
            self.crc = self.calc_crc(self.crc, c)
            return
        
        if (self.parser_state == DATA):
            self.data.append(c)
            self.crc = self.calc_crc(self.crc, c)
            self.data_len -= 1;
            if (self.data_len == 0):
                self.parser_state = CRC
            return
        
        if (self.parser_state == CRC):
            if (self.crc == c):
                #got answer
                self.last_packet = None
                
                self.rx_packet(self.data)
                #print "CRC ok %02X" % crc
                #print byte
            else:
                self.log("CRC fail %X != %X" % (self.crc, c), log.ERROR)
#                 print bytes
                                                            
            self.parser_state = IDLE        
        