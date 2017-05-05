
import net.net as net
import bt.interface
import common.log as log
import net.protocol as pr
import cfg
import time
import cProfile

class hub(log.Logger):
    def __init__(self, label):
        log.Logger.__init__(self, "hub")
        
        self.bt = bt.interface.Interface(self)
        
        self.label = label
        
        self.running = True 
        
        self.sockets = {}
        
        self.log("Init done", log.INFO)        
        
    def end(self):
        self.running = False
        self.net.write(["end"])
        self.bt.write(["end"])
        
    def parse_data(self, data):
        self.log("parse data " + str(data.cmd) + " " + str(data.payload ), log.DEBUG)
        if data.cmd == pr.REQ_ID:
            packet = pr.Packet(pr.ANS_ID, {"label": self.label})
            self.net.send_packet(packet)
            
        if data.cmd == pr.DEVICE_CONNECT:
            self.bt.write(["connect", data.payload["addr"], data.payload["type"]])
            
        if data.cmd == pr.DEVICE_CONF:
            self.bt.write(["config", data.payload["addr"], data.payload["add"], data.payload["remove"], data.payload["fw"]])
      
    def start_net(self):
        #start networking
        self.net = net.Net()
        self.net.start()
        self.net.write(["client", cfg.server, cfg.port])
        
    def loop(self):
        self.start_net()
        
        self.bt.start()
        self.bt.write(["scan"])
        
        while self.running:
            self.net.wait(0.5)
            for msg in self.net.read():
                self.log("cmd " + str(msg), log.DEBUG)

                cmd = msg[0]
                
                if cmd == "data":
                    self.parse_data(msg[2])
                    
                if cmd == "del":
                    #if net socket dies, restart net
                    self.net.write(["end"]) 
                    del self.net
                    self.start_net()

            self.bt.wait(0.5)
            for msg in self.bt.read():
                self.log("cmd " + str(msg), log.DEBUG)

                cmd = msg[0]
                
                if cmd == "scan":
                    print msg
                    if msg[1] is not False:
                        self.net.send_packet(pr.Packet(pr.SCAN_IRQ, msg[1]))
                        
                    self.bt.write(["scan"])
                    
                if cmd == "crash":
                    del self.bt
                    self.bt = bt.interface.Interface(self)
                    self.bt.start()
                    self.bt.write(["scan"])
                    

     
    def boot(self):
        self.log("Loop start", log.INFO)    
        try:
            self.loop()
        except KeyboardInterrupt:
            self.log("Interrupted!", log.INFO)
            self.end()
        self.log("Loop end", log.INFO)        
    

cProfile.run("hub(\"test hub\").boot()", sort="cumulative")
