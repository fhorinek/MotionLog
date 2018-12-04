import bluepy.btle as ble
import threading
import struct

def perform_scan(iface):
    a = ble.Scanner(iface)
    res = a.scan(0.2)
    
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

class bt_inerface_le(ble.DefaultDelegate):
    
    def __init__(self, addr, rx_mtu, iface):
        ble.DefaultDelegate.__init__(self)

        self.rx_buffer = ""
        self.tx_buffer = []
        
        self.delegate = self
        self.sock = ble.Peripheral(addr, ble.ADDR_TYPE_RANDOM, iface).withDelegate(self)
        #set MTU
        self.sock.setMTU(rx_mtu)
        #enable notification
        self.sock.writeCharacteristic(0x0212, struct.pack('<h', 0x001), withResponse=True)

        self.is_alive = True
        
        threading.Thread(target=self.run).start()
        
    def end(self):
        self.is_alive = False
        
    def run(self):
        while self.is_alive:
            self.sock.waitForNotifications(0.0005)
            while self.tx_buffer:
                data = self.tx_buffer[0]
                self.tx_buffer = self.tx_buffer[1:]
                self.sock.writeCharacteristic(0x0211, data, withResponse=True)
                
        self.sock.disconnect()

        
    def handleNotification(self, cHandle, data):
#         print "le read %03d" % (len(data)), map(ord, data)
        self.rx_buffer += data

        f = open("rx_data.bin", "a")
        f.write(data)
        f.close()
            
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
#         print "le write %03d" % len(data), data
        self.tx_buffer.append("".join(map(chr, data)))
        