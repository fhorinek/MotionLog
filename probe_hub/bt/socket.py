import os
import struct
from time import time

import common.log as log
import datetime
from bt.le import bt_inerface_le
from bt.spp import bt_interface_classic
from bluetooth.btcommon import BluetoothError
import threading


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
RENAME_CONF     = 13
DEL_CONF        = 14
PUSH_FW         = 15
CLOSE_FW        = 16
RENAME_FW       = 17
WAIT_FOR_SERVER = 18

IDLE        = 0
LENGTH_LOW  = 1
LENGTH_HI   = 2
DATA        = 3
CRC         = 4

CRC_KEY = 0xD5 #CRC-8
START_BYTE = 0xC0

TIMEOUT = 1

DEVICE_FREEZE = 60

STREAM_OVERHEAD = 7 + 4 + 10

SERVER_CONF_TIMEOUT = 5
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
        
        self.reconnecting = False
        
        self.rx_mtu = 64
        self.tx_mtu = 64
        
        self.no_data_since = time()
        
        self.packet_timeout = 5
        
        self.last_packet = None
        self.last_packet_tx_time = 0
        self.last_packet_retry = 0
        
        self.got_conf_from_server = False
        
        self.retry_cnt = 0
        self.total_rx = 0
        self.total_tx = 0
        self.start_time = time()
        
        self.create_interface_ble()
        self.send_cmd(CMD_HELLO)
        self.acquired()        
       
    def create_interface_ble(self):
        self.rx_mtu = 254
        self.tx_mtu = 64
        self.spp_mode = False
        
        self.interface = bt_inerface_le(self.addr, self.rx_mtu, self.parent.work_iface)
        self.log("Connected", log.INFO)

        self.packet_timeout = 0.5

    def switch_to_interface_spp(self, cb):
        if self.spp_mode:
            cb()
            return

        self.log("Switching to SPP", log.INFO)
        
        self.reconnecting = True
        self.spp_mode = True  
        self.switch_to_interface_spp_worker(cb)
#         therad = threading.Thread(target=self.switch_to_interface_spp_worker, args=(cb,))
#         therad.start()
        
    def switch_to_interface_spp_worker(self, cb):
        start = time()
        #kill ble interface
        self.interface.end()
        self.reset_parser()

        retry_cnt = 3
        for i in range(retry_cnt):
            try:
                new_interface = bt_interface_classic(self.addr)
            except BluetoothError as err:
                if i == retry_cnt - 1:
                    raise err
                else:
                    self.log("Error. Retry %u" % (i + 1), log.WARN)
                    continue
            break
        
        self.interface = new_interface        
        
        self.rx_mtu = 548
        self.tx_mtu = 60


        self.packet_timeout = 30
        self.log("Connected in %0.1fs" % (time() - start), log.INFO)
        self.reconnecting = False
        cb()
        
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
        self.interface.is_alive = False
        
        duration = time() - self.start_time
        
        self.log("Closing socket", log.INFO)
        self.log("RX: %u" % self.total_rx, log.INFO)
        self.log("TX: %u" % self.total_tx, log.INFO)
        self.log("TIME: %u" % duration, log.INFO)
        self.log("-------------------------------------", log.INFO)
       
    def reset_parser(self):
        self.parser_state = IDLE
        self.data_len = 0

    def set_time(self):
        self.log("setting time", log.INFO)
        #Local time in seconds (epoch like)
        t = int((datetime.datetime.now() - datetime.datetime(1970,1,1)).total_seconds())

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
        self.switch_to_interface_spp(self.push_fw_cb)
        
    def push_fw_cb(self):
        self.log("pushing firmware", log.INFO)
        path = "/tmp.bin"
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
             
        self.log("pushing %s" % (path), log.INFO)             
             
        self.file_name = path
        self.file_data = data
        self.file_pos = 0
        self.push_file("/tmp.bin")      
        
        self.step = PUSH_CONF

    def push_file(self, path):
        self.log("pushing file %s" % path, log.INFO)
        line = []
        line += [CMD_PUSH_FILE]
        line += [len(path)]
        line += map(ord, path)
        
        self.tx_packet(line)

    def rename_conf(self):
        self.log("renaming tmp.bin to %s" % self.file_name, log.INFO)
        line = []
        line += [CMD_MV_FILE]
        path = "/tmp.BIN"
        line += [len(path)]
        line += map(ord, path)
        path = self.file_name
        line += [len(path)]
        line += map(ord, path)
        
        self.tx_packet(line)  
        
        self.step = RENAME_CONF

     
    def rename_fw(self):
        self.log("renaming tmp.bin to UPDATE.BIN", log.INFO)
        line = []
        line += [CMD_MV_FILE]
        path = "/tmp.BIN"
        line += [len(path)]
        line += map(ord, path)
        path = "UPDATE.BIN"
        line += [len(path)]
        line += map(ord, path)
        
        self.tx_packet(line)  
        
        self.step = RENAME_FW   
     
    def push_next(self):
        chunk = self.tx_mtu - STREAM_OVERHEAD

        if chunk + self.file_pos > len(self.file_data):
            chunk = len(self.file_data) - self.file_pos
        
        if chunk == 0:
            return True

        pos = min(1, (float(self.file_pos + chunk) / len(self.file_data))) * 100.0
        self.log("pushing next %u %u %u%%" % (chunk, self.file_pos, pos), log.INFO)
        
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
        
        return False        
     
    def pull_cfg(self):
        path = "/device.cfg" 
        self.file_name = path
        self.file_data = ""  
        self.pull_file(path)    
        self.step = PULL_CFG
     
     
    def pull_next_log(self):
        self.switch_to_interface_spp(self.pull_next_log_cb)   
     
    def pull_next_log_cb(self):
        path = "/logs/" + self.logs.pop()
        self.file_name = path
        self.file_data = ""  
        self.pull_file(path)     
        self.step = PULL_LOG
 
        
    def pull_file(self, path):
        self.log("pulling file %s" % path, log.INFO)
        self.pull_start = time()
        self.pull_index = -1;
        
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
        
        if self.file_size == 0:
            pos = 0
        else:
            pos = min(1, (float(file_pos + chunk) / self.file_size)) * 100.0
            
        self.log("pulling next %u %u %u%%" % (chunk, file_pos, pos), log.INFO)
       
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
        assert len(len_s) == 4, "Wrong length %s" % data
        self.file_size, = struct.unpack("<L", len_s)

    def pull_cfg_part(self):
        self.pull_next()
        self.step = PULL_CFG_PART
        
    def pull_log_part(self):
        self.pull_next()
        self.step = PULL_LOG_PART

    def process_pull_data(self, data):
        length = (data[2] << 8) | data[1]
        pull_index = (data[4] << 8) | data[3]
        
        if pull_index == self.pull_index + 1:
            self.last_rx_data = ""
            for i in range(length):
                self.last_rx_data += chr(data[5 + i])
            self.file_data += self.last_rx_data
        else:
            self.log("Unexpected pull index %u, dropping data" % pull_index, log.WARN)

        self.pull_index = pull_index            
            
        if len(self.file_data) < self.file_size:
            return False
        else:
            delta = time() - self.pull_start
            size = len(self.file_data)
            speed = size / delta
            self.log("pull done in %fsec %0.2fKb @ %0.2fbps" % (delta, size / 1024.0, speed), log.INFO)
            self.last_rx_speed = speed
            self.last_rx_time = delta
            return True

    def close_cfg_file(self):
        self.log("Closing cfg file", log.INFO);
        self.send_cmd(CMD_CLOSE_FILE)
        self.step = CLOSE_CFG    
        
    def close_log_file(self):
        self.log("Closing log file", log.INFO);
        self.send_cmd(CMD_CLOSE_FILE)
        self.step = CLOSE_LOG      

    def close_conf_file(self):
        self.log("Closing conf file", log.INFO);
        self.send_cmd(CMD_CLOSE_FILE)
        self.step = CLOSE_CONF   

    def close_fw(self):
        self.log("Closing fw file", log.INFO);
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
#         self.log("RX: " + str(data) , log.DEBUG)
#         self.log("self.step: " + str(self.step) , log.DEBUG)

        if self.step == WAIT_HELLO:
            self.fw = "".join(map(chr, data[1:33]))
            self.bat = data[33]
            try:
                self.bat_raw, = struct.unpack("<h","".join(map(chr, data[36:38])))
            except:
                self.bat_raw = 0
                
            self.set_time()
            return
            
        if self.step == SET_TIME:
            #assuming OK
            assert data[0] == CMD_RET_OK, "Answer is not OK %s" % data
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
                    self.pull_next_log()                    
#                     #download logs from device
#                     self.pull_next_log()
                else:
                    #configure device
                    if self.got_conf_from_server:
                        self.configure_device()
                    else:
                        self.step = WAIT_FOR_SERVER
            return      
                
        if self.step == PULL_LOG:
            assert data[0] == CMD_RET_OK, "Answer is not OK %s" % data
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
                meta = {"speed": self.last_rx_speed, "time": self.last_rx_time}
                self.parent.push_log(self.addr, name, self.file_data, meta)
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
                    self.server_time_start = time() 
                    self.step = WAIT_FOR_SERVER
            return
        
        if self.step == PUSH_CONF:
            if self.push_next():
                self.close_conf_file()
            return
        
        if self.step == CLOSE_CONF:
            self.rename_conf()
            return
        
        if self.step == RENAME_CONF:
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

        if not self.reconnecting:
            data = self.read()
            if data:
                working = True
            
            for c in data: 
                self.parse(c)
            
        if self.last_packet:
            working = True
            self.no_data_since = time()
            
            if time() - self.last_packet_tx_time > self.packet_timeout:
                self.log("TX packet timeout", log.WARN)
                self.retry_packet()
                
            if self.step == WAIT_FOR_SERVER and time() - self.server_time_start > SERVER_CONF_TIMEOUT:
                self.log("No answer from server", log.WARN)
                self.end()

        if time() - self.no_data_since > DEVICE_FREEZE:
            self.log("No comunication timeout", log.WARN)
            self.end()            

        return working

    def retry_packet(self):
        if self.last_packet_retry >= PACKET_MAX_RETRY:
            self.log("Max retry count reached. Closing.", log.ERROR)
            self.parent.release(self.addr)
            self.is_alive = False
        else:
            self.reset_parser()
            self.write(self.last_packet)
            self.last_packet_tx_time = time()
            self.last_packet_retry += 1
            self.log(" retry %d" % self.last_packet_retry, log.INFO)
        

    def read(self):
        return self.interface.read()
    
    def write(self, data):
        self.interface.write(data)
    
    def calc_crc(self, csum, data):
#         c_char = data
        for __ in range(0, 8):
            if ((data & 0x01) ^ (csum & 0x01)):
                csum = (csum >> 1) % 0x100 
                csum = (csum ^ CRC_KEY) % 0x100
            else:
                csum = (csum >> 1) % 0x100
            data = (data >> 1) % 0x100
            
#         self.log("CRC %02X %02X" % (c_char, csum), log.DEBUG)
            
        return csum        
    
    def tx_packet(self, data):
        if (len(data) == 0):
            return
        
        self.total_tx += len(data)
        
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
        
#         self.log("TX: " + str(to_send), log.DEBUG)
        
        self.write(to_send)
        
        self.last_packet = to_send
        self.last_packet_tx_time = time()
        self.last_packet_retry = 0

    def parse(self, c):
        #self.log(" > %02X %d %d" % (c, self.parser_state, self.data_len), log.DEBUG)
        self.total_rx += 1
        
        if (self.parser_state == IDLE):
            if (c == START_BYTE):
                self.parser_state = LENGTH_LOW
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
                self.retry_packet()
#                 print bytes
                                                            
            self.parser_state = IDLE        
        