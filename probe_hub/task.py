import common.log as log

import net.net as net
import net.protocol as pr
import cfg
import common.db

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
        whitelist = ["Bioprobe"]
        return name in whitelist 
        
    def parse_data(self, sender, data):
        self.log("DATA from " + sender + " " + str(data.cmd) + " " + str(data.payload), log.DEBUG)
        
        if data.cmd == pr.SCAN_IRQ:
            for addr in data.payload:
                dev_type, name, rssi = data.payload[addr]
                
                if not self.valid_device(name):
                    continue
                 
                if addr not in self.devices:
                    self.devices[addr] = probe_device(addr, dev_type, self)
                    
                self.devices[addr].update_connection(sender, rssi)
                
#             payload = [0] * 1024 * 1024;
#             packet = pr.Packet(pr.DUMMY, payload)
#             self.net.send_packet(packet, sender)   
           
        if data.cmd & pr.DEVICE_MASK:
            addr = data.payload["addr"]
            if addr in self.devices:
                self.devices[addr].parse(sender, data)

        
    def loop(self):
        self.net.start()
        
        self.net.write(["server", cfg.bind, cfg.port, cfg.hubs])
        
        while self.running:
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
