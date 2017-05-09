import le
import spp
import common.glue
import common.log as log
import threading
import net.protocol as pr
import time

class Interface(common.glue.MyThread, log.Logger):
    def __init__(self, parent):
        common.glue.MyThread.__init__(self)
        log.Logger.__init__(self, "bt interface")
        self.log("Init done", log.INFO)
        self.sockets = {}
        self.parent = parent
    
    def scan(self):
        try:
            devices = {}
            spp_dev = spp.perform_scan()
            for addr in spp_dev:
                name, rssi = spp_dev[addr]
                devices[addr] = ["spp", name, rssi]
    
            #le_dev = le.perform_scan()
            #for addr in le_dev:
            #    name, rssi = le_dev[addr]
            #    devices[addr] = ["gat", name, rssi]
                       
            self.internal_write(["scan", devices])
        except Exception as e:
            self.log("Scan failed: %s" % str(e), log.ERROR)
            self.internal_write(["scan", False])
        
        
    def create_connection(self, addr, dev_type):
        try:
            if dev_type == "spp":
                self.sockets[addr] = spp.bt_socket_classic(addr, self)
        except Exception as e:
            self.log("Create connection failed: %s" % str(e), log.ERROR)
            
            packet = pr.Packet(pr.DEVICE_FAIL, {"addr": addr})
            self.parent.net.send_packet(packet)
            if addr in self.sockets:
                del self.sockets[addr]
            
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
     
    def push_log(self, addr, name, data):
        packet = pr.Packet(pr.DEVICE_LOG, {"addr": addr, "name": name, "data": data})
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
    
                    if cmd == "scan":
                        sthread = threading.Thread(target=self.scan, args=())
                        sthread.start()
                        
                    if cmd == "connect":
                        self.create_connection(msg[1], msg[2])
                        
                    if cmd == "config":
                        addr = msg[1]
                        add = msg[2]
                        rem = msg[3]
                        fw = msg[4]
                        self.sockets[addr].config(add, rem, fw)
                    
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
            time.sleep(2)
            self.internal_write(["crash"])
             
        self.end()   
        self.log("Thread end", log.INFO)
        
