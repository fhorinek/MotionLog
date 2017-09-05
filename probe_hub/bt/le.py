import bluepy.btle as ble
from socket import socket
import threading
import struct

def perform_scan():
    a = ble.Scanner()
    res = a.scan(2)

    
    devices = {}
    for line in res:
        addr = str(line.addr)
        name = line.getValueText(9)
        rssi = line.rssi
        if addr in devices:
            devices[addr][1].append(rssi)
        else:
            devices[addr] = [name, [rssi]]
    
    result = {}
    for addr in devices:
        name = devices[addr][0]
        rssis = devices[addr][1]
        result[addr] =  [name, sum(rssis) / len(rssis)]
        
    return result

class bt_socket_le(socket, ble.DefaultDelegate):
    def __init__(self, addr, parent):
        socket.__init__(self, addr, parent)
        ble.DefaultDelegate.__init__(self)

        self.rx_mtu = 64
        self.tx_mtu = 64
        
        self.delegate = self
        self.sock = ble.Peripheral(addr, ble.ADDR_TYPE_RANDOM).withDelegate(self)
        #set MTU
        self.sock.setMTU(self.rx_mtu)
        #enable notification
        self.sock.writeCharacteristic(0x0212, struct.pack('<h', 0x001), withResponse=True)

        self.rx_buffer = ""
        self.tx_buffer = []
        
        self.acquired()
        
        threading.Thread(target=self.run).start()
        
    def run(self):
        while self.is_alive:
            self.sock.waitForNotifications(0.0005)
            while self.tx_buffer:
                data = self.tx_buffer[0]
                self.tx_buffer = self.tx_buffer[1:]
                self.sock.writeCharacteristic(0x0211, data, withResponse=True)
        
    def handleNotification(self, cHandle, data):
        print "len %03d <<%s>>" % (len(data), data)
        self.rx_buffer += data
            
        ble.DefaultDelegate.handleNotification(self, cHandle, data)    
    
    def read(self):
        try:
            data = self.rx_buffer
            self.rx_buffer = ""
            return map(ord, data)
        except:
            #print "no data"
            return []
    
    def write(self, data):
        print "le write", data
        self.tx_buffer.append("".join(map(chr, data)))
        