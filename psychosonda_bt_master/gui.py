
import threading
import select
import sys
import os
import struct
import time

from glue import CommandQuee
from serial_stream import SerialStream


S_IDLE = 0
S_PUSH = 1
S_PULL = 2
    
STREAM_OVERHEAD = 7 + 4 + 10
    
MTU = 500  
    
FILE_CHUNK_RX = MTU - STREAM_OVERHEAD
FILE_CHUNK_TX = MTU - STREAM_OVERHEAD
    
class Interface(threading.Thread):
    def __init__(self, cin, cout):
        threading.Thread.__init__(self)
        self.cin = cin
        self.cout = cout
        self.state = S_IDLE
        
    def send_line(self, cmd = None):
        if cmd == None:
            cmd = int(raw_input("raw cmd:"))

        line = []
        line.append(cmd)
        
        self.cout.Write("send", False)
        self.cout.Write(line)
        
    def pull_file(self):
        path = raw_input("pull file:")
        
        try:
            self.file = open(path, "wb")
            self.file_pos = 0

            name = "LOGS/" + os.path.basename(path)
        
            line = [2]
            line += [len(name)]
            line += map(ord, name)
                    
            self.cout.Write("send", False)
            self.cout.Write(line)                
                    
            self.state = S_PULL
        
        
        except Exception as e:
            print "Error writing", path, e
            
    def pull_next(self):
        chunk = FILE_CHUNK_RX
       
        line = [4]
        line += [chunk & 0x00FF]
        line += [(chunk & 0xFF00) >> 8]
        line += map(ord, struct.pack("<L", self.file_pos))
       
        self.cout.Write("send", False)
        self.cout.Write(line)       
        
        self.file_pos += chunk                    
        
    def del_file(self, path = ""):
        if not len(path):
            path = raw_input("del file:")
    
        line = [11]
        line += [len(path)]
        line += map(ord, path)
                
        self.cout.Write("send", False)
        self.cout.Write(line)                
        
    def mv_file(self, path = "", new_path = ""):
        if not len(path):
            path = raw_input("path:")

        if not len(new_path):
            path = raw_input("new_path:")
    
        line = [12]
        line += [len(path)]
        line += map(ord, path)
        line += [len(new_path)]
        line += map(ord, new_path)
                
        self.cout.Write("send", False)
        self.cout.Write(line)            
        
    def push_file(self, path = "", dest=""):
        if not len(path):
            path = raw_input("push file:")

        if not len(dest):
            dest = raw_input("destination [/conf]:")
            if len(dest) == 0:
                dest = "/conf"            
        
        try:
            self.file = open(path, "rb")
            self.file_size = os.path.getsize(path)
            print "file size", self.file_size
            self.file_pos = 0

            name = dest + "/" + os.path.basename(path)
        
            line = [1]
            line += [len(name)]
            line += map(ord, name)
                    
            self.cout.Write("send", False)
            self.cout.Write(line)                
                    
            self.state = S_PUSH
        
        
        except Exception as e:
            print "Error opening", path, e
        
        
    def push_next(self):
        if (self.file_pos == self.file_size):
            self.cout.Write("send", False)
            self.cout.Write([5]) #closefile
            self.state = S_IDLE    
            return          
        
        chunk = FILE_CHUNK_TX
        
        if (chunk + self.file_pos > self.file_size):
            chunk = self.file_size - self.file_pos
        
        line = [3]
        line += [(chunk & 0x00FF) >> 0]
        line += [(chunk & 0xFF00) >> 8]
        line += map(ord, struct.pack("<L", self.file_pos))
        
        data = self.file.read(chunk)
        line += map(ord, data)
        
        self.cout.Write("send", False)
        self.cout.Write(line)            
        
        self.file_pos += chunk
        
    def set_time(self):
        #XXX:quick HACK
        t = int(time.time() + 60 * 60)
        line = [9]
        line += map(ord, struct.pack("<L", t))
        
        self.cout.Write("send", False)
        self.cout.Write(line)             
        

    def list_dir(self, path = "", start = 0):
        if len(path) == 0:
            path = raw_input("list dir:")
            
        self.dir_path = path
        self.dir_start = start
        
        cnt = 35
        
        line = [10]
        line += [(start & 0x00FF) >> 0]
        line += [(start & 0xFF00) >> 8]         
        line += [cnt]
        line += [len(path)]
        line += map(ord, path)
                
        self.cout.Write("send", False)
        self.cout.Write(line)                


    def cmd_help(self):
        print       
        print "HELLO            0"
        print "PUSH_FILE        1"
        print "PULL_FILE        2"
        print "PUSH_PART        3"
        print "PULL_PART        4"
        print "CLOSE_FILE       5"
        print "MEAS             6"
        print "REBOOT           7"
        print "OFF              8"
        print "SET_TIME         9"
        print "LIST_DIR         10"
        print "DEL_FILE         11"
        print       
        
    def gui_help(self):
        print       
        print "s - Send command"       
        print "p - Push file"       
        print "g - Pull file"       
        print "l - List dir"       
        print "d - Delete file"       
        print "r - Reboot"       
        print "u - Upload new firmware"
        print "x - Power down"
        print "h - Help"
        print       
               
    def end(self):
        self.running = False
        self.cout.Write("quit")
               
    def run(self):
        self.gui_help()
        self.running = True
        
        while(self.running):
            while sys.stdin in select.select([sys.stdin], [], [], 0)[0]:
                line = sys.stdin.readline()
                if line:
                    if (line[0] == 's'):
                        self.cmd_help()
                        self.send_line()

                    if (line[0] == 'p'):
                        self.push_file()

                    if (line[0] == 'u'):
                        self.push_file("/home/horinek/git/MotionLog/psychosonda_bt_ng192/Release/UPDATE.BIN", "/")
                    
                    if (line[0] == 'g'):
                        self.pull_file()
                    
                    if (line[0] == 'l'):
                        self.list_dir()                        
                    
                    if (line[0] == 'd'):
                        self.del_file()                        
                
                    if (line[0] == 'h'):
                        self.gui_help()

                    if (line[0] == 'r'):
                        self.send_line(7)
                        self.end()

                    if (line[0] == 'x'):
                        self.send_line(8)
                        self.end()

                        
                else: # an empty line means stdin has been closed
                    print 'Connection closed'
                    self.cout.Write("quit")
                    return
            else:
                if self.cin.HaveEvent():
                    type = self.cin.Read()
                    if type == "msg": 
                        data = self.cin.Read()
#                         print "RX data", data
                        
                        cmd = data[0]
                        
                        if cmd == 0:
                            print "OK"
                            if (self.state == S_PUSH):
                                print "TX -> %3d%%" % ((float(self.file_pos) / float(self.file_size)) * 100.0)
                                self.push_next()
                            if (self.state == S_PULL):
#                                 print data[1:]
                                len_s =  "".join(map(chr, data[1:]))
                                self.file_size, = struct.unpack("<L", len_s)
                                print "file size", self.file_size
                                self.pull_next()
                                

                        if cmd == 1:
                            print "FAIL", data[1], data[2] 
                        
                        if cmd == 2:
                            txt = ""
                            for i in range(1, 9):
                                txt += chr(data[i])
                            print txt
                            print "Battery is @ %d%%" % data[9]
                            
                            max_mtu = data[10] | (data[11] << 8)
                            print "MTU size is %d b" % max_mtu
                            FILE_CHUNK_RX = max_mtu - STREAM_OVERHEAD
                            FILE_CHUNK_TX = max_mtu - STREAM_OVERHEAD
                            self.set_time()
                            
                        if cmd == 3:
                            if (self.state == S_PULL):
                                length = (data[2] << 8) | data[1]
                                if self.file_size > 0:
                                    print "RX -> %3d%%" % ((float(self.file_pos + length) / float(self.file_size)) * 100.0)
                                buff = ""
                                for i in range(length):
                                    buff += chr(data[3 + i])
                                
                                self.file.write(buff)
                                
                                if (length < FILE_CHUNK_RX):
                                    self.file.close()
                                    self.state = S_IDLE
                                    print "RX -> done"
                                else:
                                    self.pull_next()
        
                        if cmd == 4:
#                             print data
                            in_dir = (data[2] << 8) | data[1]
                            listed = data[3]
                            if self.dir_start == 0:
                                print "listing dir '%s'" % self.dir_path
                                print "total", in_dir 
                            
                            for i in range(listed):
                                index = 4 + i * 12
                                fname = "".join(map(chr, data[index:index + 12]))
                                print "\t", i + self.dir_start, "\t", fname
                                
                            if in_dir > self.dir_start + listed:
                                self.list_dir(self.dir_path, self.dir_start + listed)

if __name__ == "__main__":
    from_ui = CommandQuee()
    to_ui = CommandQuee()

    stream = SerialStream(from_ui, to_ui)
    stream.open("/dev/rfcomm1", 115200)
    stream.start()
    
    ui = Interface(to_ui, from_ui)
    ui.start()

    