#!/usr/bin/python

import common.log as log

import net.net as net
import net.protocol as pr
import cfg
import common.db

import cProfile

from task.probe_device import probe_device
            
class task(log.Logger):
    def __init__(self):
        log.Logger.__init__(self, "task")
        
        self.net = net.Net()
        self.db = common.db.db_conn()
        
        self.running = True
        
        self.devices = {}
        
        self.log("Init done", log.INFO)    
            
    def end(self):
        self.running = False
        self.net.write(["end"])
        
    def valid_device(self, name):
        whitelist = ["BioprobeBLE"] #"BioprobeSPP", "Bioprobe"
        return name in whitelist 
        
    def parse_data(self, sender, data):
        #self.log("DATA from " + sender + " " + str(data.cmd) + " " + str(data.payload), log.DEBUG)
        
        if data.cmd == pr.SCAN_IRQ:
            for addr in data.payload:
                name, rssi = data.payload[addr]
                
                if not self.valid_device(name):
                    continue
                 
                if addr not in self.devices:
                    self.devices[addr] = probe_device(addr, self)
                    
                self.devices[addr].update_connection(sender, rssi)
          
        if data.cmd & pr.DEVICE_MASK:
            addr = data.payload["addr"]
            if addr in self.devices:
                self.devices[addr].parse(sender, data)

        
    def loop(self):
        self.net.start()
        
        self.net.write(["server", cfg.bind, cfg.port, cfg.hubs])
        
        while self.running:
            self.net.wait(0.1)
            for msg in self.net.read():
                self.log("net cmd " + str(msg), log.DEBUG)

                cmd = msg[0]
                
                if cmd == "data":
                    self.parse_data(msg[1], msg[2])
                
            for dev in self.devices:
                self.devices[dev].work()
                
                
    
    def boot(self):
        self.log("Loop start", log.INFO)        
        try:
            self.loop()
        except KeyboardInterrupt:
            self.log("Interrupted!", log.INFO)
            self.end()
        self.log("Loop end", log.INFO)        

task().boot()    
#cProfile.run("task().boot()", sort="tottime")
