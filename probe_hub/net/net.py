import socket
import cmd
import common.log as log
import time
import errno
import pickle
import common.glue
from posix import wait

class Connection(log.Logger):
    def __init__(self, handle, parent, name):
        log.Logger.__init__(self, "Unknown Connection")
        self.parser_state = 0
        self.data = []
        self.len = 0
        self.name = name
        self.handle = handle
        self.parent = parent
        self.log("Added", log.INFO)
        self.tx_data = ""
        self.tx_chunk = 1024

    def work(self):
        if (self.tx_data):
            try:
                self.handle.send(self.tx_data[:self.tx_chunk])
                self.log("TX: %ub" % (len(self.tx_data[:self.tx_chunk])), log.DEBUG)
                self.tx_data = self.tx_data[self.tx_chunk:]          
            except socket.error, e:
                self.log("TX: error %s" % (str(e)), log.DEBUG)
                

    def __del__(self):
        self.handle.shutdown(socket.SHUT_RDWR)
        self.handle.close()
        self.log("Removed", log.INFO)
        
    def parse(self, data):
        self.log("RX: %ub" % len(data), log.DEBUG)
        for c in data:
            n = ord(c)
            if self.parser_state == 0: #wait for start
                if n == 0xAA:
                    self.parser_state += 1
                continue
            
            if self.parser_state == 1: #len 1
                self.len = n
                self.parser_state += 1
                continue

            if self.parser_state == 2: #len 2
                self.len += n * 256
                self.parser_state += 1
                self.data = []
                continue
            
            if self.parser_state == 3: #len 3
                self.len += n * 256 * 256
                self.parser_state += 1
                self.data = []
                continue
            
            if self.parser_state == 4: #len 4
                self.len += n * 256 * 256 * 256
                self.parser_state += 1
                self.data = []
                continue
            
            if self.parser_state == 5: #data
                self.data.append(n)
                self.len -= 1
                if self.len <= 0:
                    self.parser_state = 0
                    #self.log("RX: %ub" % len(self.data), log.DEBUG)
                    bin_data = "".join(map(chr, self.data))
#                     try:
                    self.parent.internal_write(["data", self.name, pickle.loads(bin_data)])
#                     except ValueError:
#                         f = open("/tmp/last_rcv_%s.bin" % self.name, "wb")
#                         f.write(bin_data)
#                         f.close()
#                         raise ValueError
                continue    
            
            
    def send(self, data):
        data = pickle.dumps(data)

#         f = open("/tmp/last_snd_%s.bin" % self.name, "wb")
#         f.write(data)
#         f.close()

        
        packet = []
        packet.append(0xAA)
        packet.append((len(data) & 0x000000FF) >> 0)
        packet.append((len(data) & 0x0000FF00) >> 8)
        packet.append((len(data) & 0x00FF0000) >> 16)
        packet.append((len(data) & 0xFF000000) >> 24)
        
        bin_data = "".join(map(chr, packet))
        bin_data += data

        self.tx_data += bin_data
        
        self.log("To send: %ub" % len(bin_data), log.DEBUG)

            
class Net(common.glue.MyThread, log.Logger):
    
    def __init__(self):
        common.glue.MyThread.__init__(self)
        log.Logger.__init__(self, "net")
        self.log("Init done", log.INFO)
        
        #main socket
        self.handle = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.connections = {}
        self.connection_counter = 0
        self.role = "none"
        
        self.configuration = "none"
        self.parameters = []
        self.allow_list = {}

    def send_packet(self, data, connection = None):
        if len(self.connections) == 0:
            self.log("No connections opened", log.ERROR)
            return
            
        if connection == None:
            connection = self.connections.keys()[0]
        self.send(connection, data)
       
    def end(self):
        for k in self.connections.keys():
            self.del_connection(k)
            
      
    def add_connection(self, handle, client = None):
        self.connection_counter += 1
        if client == None:
            client = "hub_%03u" % self.connection_counter

        handle.setblocking(0)
        self.connections[client] = Connection(handle, self, client)

        self.internal_write(["add", client])
        return client
    
    def del_connection(self, client):
        if client in self.connections:
            del self.connections[client]
            self.internal_write(["del", client])
        
    def open(self):
        address = self.parameters[0]
        port = self.parameters[1]
        
        self.log("Connection to %s@%u" % (address, port), log.INFO)
        try:
            self.handle.connect((address, port))
            self.handle.setblocking(0)
            self.internal_write(["role", "client"])
            self.role = "client"
            self.add_connection(self.handle, "master")
            self.log("Connected", log.INFO)
        except Exception, e:
            self.log(e, log.ERROR)
    
    def bind(self):
        address = self.parameters[0]
        port = self.parameters[1]
        
        self.log("Binding to %s@%u" % (address, port), log.INFO)
        try:
            self.handle.bind((address, port))
            self.handle.listen(1)
            self.handle.setblocking(0)
            self.internal_write(["role", "server"])
            self.role = "server"
            self.log("Binded", log.INFO)
        except Exception, e:
            self.log(e, log.ERROR)
        
        
    def send(self, to, data):
        if to in self.connections:
            self.connections[to].send(data)
        else:
            self.log("Not connected to " + to, log.ERROR)
        
    def run(self):
        self.log("Thread start", log.INFO)
        self.running = True
        
        while self.running:
            for msg in self.internal_read():
                self.log("msg " + str(msg), log.DEBUG)
                
                cmd = msg[0] 
                
                if cmd == "end":
                    self.end()
                    self.running = False
                
                if cmd == "client":
                    self.configuration = "client"
                    self.parameters = [msg[1], msg[2]]
                    
                if cmd == "server":
                    self.configuration = "server"
                    self.parameters = [msg[1], msg[2]]
                    self.allow_list = msg[3]
 
                if cmd == "send":
                    self.send(msg[1], msg[2])
                    
            data = ""        
                    
            if self.role == "none":
                if self.configuration == "server":
                    self.bind()
                if self.configuration == "client":
                    self.open()
                
                    
            if self.role is "server":
                try:
                    s, a = self.handle.accept()
                    s.setblocking(0)
                    self.log("Connection from " + str(a), log.INFO)
                    
                    if a[0] in self.allow_list:
                        name = self.allow_list[a[0]]
                    else:
                        name = "hub_%s" % a[0]        
                        
                    self.add_connection(s, name)
                except socket.error, e:
                    err = e.args[0]
                    if err == errno.EAGAIN or err == errno.EWOULDBLOCK:
                        pass
                    else:
                        self.log(e, log.ERROR)
                
            if self.role in ["server", "client"]:
                #TX
                for c in self.connections.keys():
                    self.connections[c].work()
                #RX
                for c in self.connections.keys():
                    waiting = False
                    try:
                        data = ""
                        while True:
                            data += self.connections[c].handle.recv(1024)
                            if not data:
                                break
                            
                    except socket.error, e:
                        err = e.args[0]
                        if err == errno.EAGAIN or err == errno.EWOULDBLOCK:
                            #waiting for data
                            waiting = True
                            pass 
                        else:
                            self.log(e, log.ERROR)

                    if len(data) == 0 and not waiting:
                        self.del_connection(c)
                        continue
                    
                    if data:
                        self.connections[c].parse(data)
                         
            time.sleep(0.1)    

                    
                    
        self.log("Thread end", log.INFO)
                
        