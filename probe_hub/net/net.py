import socket
import cmd
import common.log as log
import errno
import pickle
import common.glue
import threading
from collections import OrderedDict
from time import time
import protocol as pr

SEND_IDLE = 0
SEND_HAVE_DATA = 1
SEND_SENDING = 2
SEND_WAITING_FOR_ACK = 3

SEND_TIMEOUT = 10.0

class Connection(log.Logger, threading.Thread):
    def __init__(self, handle, parent, name):
        threading.Thread.__init__(self)
        log.Logger.__init__(self, "Unknown Connection", level=log.INFO)
        self.parser_state = 0
        self.data = []
        self.len = 0
        self.name = name
        self.handle = handle
        self.parent = parent
        self.log("Added", log.INFO)
        self.alive = True

        self.packets_to_send = OrderedDict()
        self.packet_counter = 0
        self.packet_actual = None
        self.last_recieved = -1
        
        self.send_state = SEND_IDLE
        
        self.ack_bin_data = None
        
        self.event = threading.Event()

    def end(self):
        self.alive = False
        
    def parse(self, data):
        self.ack_bin_data = ""
        
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
                    bin_data = "".join(map(chr, self.data))
                    try:
                        obj_data = pickle.loads(bin_data)
                        
                        if obj_data.cmd == pr.PACKET_ACK:
                            #packet is ack
                            data = obj_data.payload
                            if data["error"] == False:
                                #packet was recieved, remove
                                self.log("ACK received, removing packet from queue: %d" % data["id"], log.DEBUG) 
                                if data["id"] in self.packets_to_send:
                                    del self.packets_to_send[data["id"]]
                                self.send_state = SEND_IDLE
                            else:
                                #error on the other side, resend the packet
                                self.send_state = SEND_WAITING_FOR_ACK
                                #zero the timer
                                self.send_timer = 0
                        else:
                            #packed recived and parsed correctly, tell this to the other side
                            self.log("Packet recieved: %d" % obj_data.id, log.DEBUG) 
                            if (self.last_recieved < obj_data.id):
                                #perform action only if the packet was not recieved before
                                self.parent.internal_write(["data", self.name, obj_data])
                                self.last_recieved = obj_data.id
                                
                            self.send_ack(obj_data.id)
                        
                    except ValueError:
                        #error resend last packet
                        self.log("Malformed packet recieved", log.ERROR) 
                        self.send_ack(obj_data.id, True)  
                        
                continue    
            
            
    def send_ack(self, pid, error = False):
        self.log("Sending ACT to: %d" % pid, log.DEBUG)        
        p = pr.Packet(pr.PACKET_ACK, {"error": error, "id": pid})
        self.ack_bin_data += self.packet_to_bin(p)
            
    def send(self, data):
        data.id = self.packet_counter
        
        self.packets_to_send[self.packet_counter] = data
        
        self.log("TX buff add: %d" % self.packet_counter, log.DEBUG)        
        self.packet_counter += 1
        
        self.event.set()
        
    def packet_to_bin(self, p):
        data = pickle.dumps(p)
        
        packet = []
        packet.append(0xAA)
        packet.append((len(data) & 0x000000FF) >> 0)
        packet.append((len(data) & 0x0000FF00) >> 8)
        packet.append((len(data) & 0x00FF0000) >> 16)
        packet.append((len(data) & 0xFF000000) >> 24)
        
        bin_data = "".join(map(chr, packet))
        bin_data += data
        
        return bin_data

        
    def send_packets(self):
        def send_ack_packet():
            try:
                self.handle.send(self.ack_bin_data)
                self.log("Sending ACK now!", log.DEBUG)
                self.ack_bin_data = None
            except socket.error, e:
                self.log("Send error: %s" % str(e), log.ERROR)                
        
        
        if self.send_state == SEND_IDLE:
            if self.ack_bin_data:
                send_ack_packet()
                return True
            
            if len(self.packets_to_send):
                self.send_state = SEND_HAVE_DATA
            else:
                return False
        
        if self.send_state == SEND_HAVE_DATA:
            self.bin_data = ""
            for i in range(len(self.packets_to_send)):
                self.packet_actual = self.packets_to_send.keys()[i]
                
                packet = self.packets_to_send[self.packet_actual]
                self.log("Sending packet: %d" % self.packet_actual, log.DEBUG)
                self.packet_ack = False
                    
                self.send_state = SEND_SENDING
                self.bin_data += self.packet_to_bin(packet)
            
            return True
            
        if self.send_state == SEND_SENDING:
            chunk = 1024 * 10
            
            try:
                sended = self.handle.send(self.bin_data[:chunk])
                self.log("Sending chunk: (%d, %d)" % (self.packet_actual, len(self.bin_data)), log.DEBUG)
                self.bin_data = self.bin_data[sended:]
            except socket.error, e:
                self.log("Send error: %s" % str(e), log.ERROR)
                
            if len(self.bin_data) == 0:
                if self.packet_ack:
                    self.send_state = SEND_IDLE
                else:
                    self.send_state = SEND_WAITING_FOR_ACK
                    
                self.send_timer = time() + SEND_TIMEOUT
            
            return True
                
        if self.send_state == SEND_WAITING_FOR_ACK:
            if self.ack_bin_data:
                send_ack_packet()
                return True
            
            if time() > self.send_timer:
                #resent last packet
                self.log("Resending packet: %d" % self.packet_actual, log.DEBUG)
                self.send_state = SEND_SENDING
                self.bin_data = self.packet_to_bin(self.packets_to_send[self.packet_actual])     
                
        return False           
    
    def run(self):
        while self.alive:
            sleeping = True

            try:
                data = self.handle.recv(1024 * 10)
                if data:
                    self.parse(data)
                    sleeping = False
                else:
                    self.end()
                
            except socket.error, e:
                pass
            
            if self.send_packets():
                sleeping = False
                
            if sleeping:
                self.event.wait(0.5)
                self.event.clear()
                
            
        self.handle.shutdown(socket.SHUT_RDWR)
        self.handle.close()
        self.log("Removed", log.INFO)

            
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
        self.connections[client].start()
        
        self.internal_write(["add", client])
        return client
    
    def del_connection(self, client):
        if client in self.connections:
            self.connections[client].end()
            self.internal_write(["del", client])
        
    def open(self):
        address = self.parameters[0]
        port = self.parameters[1]
        
        self.log("Connection to %s@%u" % (address, port), log.INFO)
        try:
            self.handle.connect((address, port))
#             self.handle.setblocking(0)
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
            self.internal_wait(0.1)
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
    
                    
            if self.role == "none":
                if self.configuration == "server":
                    self.bind()
                if self.configuration == "client":
                    self.open()
                
                    
            if self.role is "server":
                try:
                    s, a = self.handle.accept()
                    self.log("Connection from " + str(a), log.INFO)
                    
                    if a[0] in self.allow_list:
                        name = self.allow_list[a[0]]
                        self.add_connection(s, name)                        
                    else:
                        self.log("Unknown hub, rejecting", log.WARN)
                        s.shutdown(socket.SHUT_RDWR)
                        s.close()
                        

                except socket.error, e:
                    err = e.args[0]
                    if err == errno.EAGAIN or err == errno.EWOULDBLOCK:
                        pass
                    else:
                        self.log(e, log.ERROR)
                
#             if self.role in ["server", "client"]:
#                 #RX
#                 for c in self.connections.keys():
#                     waiting = False
#                     try:
#                         data = ""
#                         while True:
#                             data += self.connections[c].handle.recv(1024)
#                             if not data:
#                                 break
#                             
#                     except socket.error, e:
#                         err = e.args[0]
#                         if err == errno.EAGAIN or err == errno.EWOULDBLOCK:
#                             #waiting for data
#                             waiting = True
#                             pass 
#                         else:
#                             self.log(e, log.ERROR)
# 
#                     if len(data) == 0 and not waiting:
#                         self.del_connection(c)
#                         continue
#                     
#                     if data:
#                         self.connections[c].parse(data)
                         
#             time.sleep(0.1)    

                    
                    
        self.log("Thread end", log.INFO)
                
        