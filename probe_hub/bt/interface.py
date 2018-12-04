import cfg

import le
import common.glue
import common.log as log
import net.protocol as pr
import socket
from time import sleep, time

SCAN_PERIOD = 5
MAX_DEVICES = 4

class Interface(common.glue.MyThread, log.Logger):
    def __init__(self, parent):
        common.glue.MyThread.__init__(self)
        log.Logger.__init__(self, "bt interface")
        self.log("Init done", log.INFO)
        self.sockets = {}
        self.parent = parent
        self.work_iface = 0
        self.scan_iface = 0
        self.next_scan = 0
        
        self.to_connect = []
    
    def valid_device(self, name):
        return name in cfg.whitelist 
    
    def scan(self):
        try:
            devices = {}
            if len(self.sockets) > 0:
                sleep(0.5)
    
            le_dev = le.perform_scan(self.scan_iface)
            
            for addr in le_dev:
                name, rssi = le_dev[addr]
                if self.valid_device(name):
                    devices[addr] = [name, rssi]
                    self.log(" %s %s %d" % (addr, name, rssi), log.INFO)
                    
            if len(devices) > 0:
                self.internal_write(["scan", devices])
                
        except Exception as e:
            self.log("Scan failed: %s" % str(e), log.ERROR)

            
    def have_device(self, addr):
        return addr in self.sockets
        
    def can_create_socket(self):
        return len(self.sockets) < MAX_DEVICES
        
    def create_connection(self, addr):
        if self.can_create_socket():
            try:
                self.sockets[addr] = socket.socket(addr, self)
                    
            except Exception as e:
                self.log("Create connection failed: %s" % str(e), log.ERROR)
                
                packet = pr.Packet(pr.DEVICE_FAIL, {"addr": addr})
                self.parent.net.send_packet(packet)
                if addr in self.sockets:
                    del self.sockets[addr]
        else:
            self.log("Too many connections", log.WARN)
            packet = pr.Packet(pr.DEVICE_FAIL, {"addr": addr})
            self.parent.net.send_packet(packet)            
            
    def end(self):
        for k in self.sockets.keys():
            s = self.sockets[k]
            self.release(s.addr) 
            
    def release(self, addr):
        packet = pr.Packet(pr.DEVICE_CLOSED, {"addr": addr})
        self.parent.net.send_packet(packet)
        del self.sockets[addr]
     
    def acquired(self, addr):
        packet = pr.Packet(pr.DEVICE_ACQURED, {"addr": addr})
        self.parent.net.send_packet(packet)
     
    def push_log(self, addr, name, data, meta):
        packet = pr.Packet(pr.DEVICE_LOG, {"addr": addr, "name": name, "data": data, "meta": meta})
        self.parent.net.send_packet(packet)

    def get_conf(self, addr, conf, fw, cfg, bat):
        packet = pr.Packet(pr.DEVICE_GET_CONF, {"addr": addr, "conf": conf, "fw": fw, "bat": bat, "cfg": cfg})
        self.parent.net.send_packet(packet)
        
    def run(self):
        self.log("Thread start", log.INFO)
        self.running = True
        
        try:
            while self.running:
                working = False

                for msg in self.internal_read():
                    working = True
                    
                    self.log("msg " + str(msg), log.DEBUG)
    
                    cmd = msg[0]
                    
                    if cmd == "end":
                        self.running = False
                        
                    if cmd == "connect":
                        addr = msg[1]
                        
                        if addr not in self.to_connect:
                            self.to_connect.append(addr)
                        
                    if cmd == "config":
                        addr = msg[1]
                        add = msg[2]
                        rem = msg[3]
                        fw = msg[4]
                        self.sockets[addr].config(add, rem, fw)
                    
                #create connections before scan
                for addr in self.to_connect:
                    self.create_connection(addr)
                  
                self.to_connect = []

                #scaning during comunication will pause the communication
#                 if len(self.sockets) == 0:
                if self.next_scan < time() or len(self.sockets) == 0:
                    if self.can_create_socket():
                        self.scan()
                        self.next_scan = time() + SCAN_PERIOD
                    
                try:
                    s = None
                    for k in self.sockets.keys():
                        s = self.sockets[k] 
                        if s.work():
                            working = True
                except:
                    self.log("Socket error", log.ERROR)
                    working = True
                    if s:
                        self.release(s.addr)
                    
                        
                if not working:
                    self.internal_wait(0.1)

        except:
            self.log("Interface error", log.ERROR)
            sleep(2)
            self.internal_write(["crash"])
             
        self.end()   
        self.log("Thread end", log.INFO)
        
