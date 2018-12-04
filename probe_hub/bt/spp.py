#taken from: https://github.com/karulis/pybluez/blob/master/examples/advanced/inquiry-with-rssi.py

import struct
import bluetooth._bluetooth as bluez
import bluetooth
from bluetooth.btcommon import RFCOMM

WORK_DEVICE = 0
SCAN_DEVICE = 0

def read_inquiry_mode(sock):
    """returns the current mode, or -1 on failure"""
    # save current filter
    old_filter = sock.getsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, 14)

    # Setup socket filter to receive only events related to the
    # read_inquiry_mode command
    flt = bluez.hci_filter_new()
    opcode = bluez.cmd_opcode_pack(bluez.OGF_HOST_CTL, 
            bluez.OCF_READ_INQUIRY_MODE)
    bluez.hci_filter_set_ptype(flt, bluez.HCI_EVENT_PKT)
    bluez.hci_filter_set_event(flt, bluez.EVT_CMD_COMPLETE);
    bluez.hci_filter_set_opcode(flt, opcode)
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, flt )

    # first read the current inquiry mode.
    bluez.hci_send_cmd(sock, bluez.OGF_HOST_CTL, 
            bluez.OCF_READ_INQUIRY_MODE )

    pkt = sock.recv(255)

    status,mode = struct.unpack("xxxxxxBB", pkt)
    if status != 0: mode = -1

    # restore old filter
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, old_filter )
    return mode

def write_inquiry_mode(sock, mode):
    """returns 0 on success, -1 on failure"""
    # save current filter
    old_filter = sock.getsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, 14)

    # Setup socket filter to receive only events related to the
    # write_inquiry_mode command
    flt = bluez.hci_filter_new()
    opcode = bluez.cmd_opcode_pack(bluez.OGF_HOST_CTL, 
            bluez.OCF_WRITE_INQUIRY_MODE)
    bluez.hci_filter_set_ptype(flt, bluez.HCI_EVENT_PKT)
    bluez.hci_filter_set_event(flt, bluez.EVT_CMD_COMPLETE);
    bluez.hci_filter_set_opcode(flt, opcode)
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, flt )

    # send the command!
    bluez.hci_send_cmd(sock, bluez.OGF_HOST_CTL, 
            bluez.OCF_WRITE_INQUIRY_MODE, struct.pack("B", mode) )

    pkt = sock.recv(255)

    status = struct.unpack("xxxxxxB", pkt)[0]

    # restore old filter
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, old_filter )
    if status != 0: return -1
    return 0

def device_inquiry_with_with_rssi(sock):
    # save current filter
    old_filter = sock.getsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, 14)

    # perform a device inquiry on bluetooth device #0
    # The inquiry should last 8 * 1.28 = 10.24 seconds
    # before the inquiry is performed, bluez should flush its cache of
    # previously discovered devices
    flt = bluez.hci_filter_new()
    bluez.hci_filter_all_events(flt)
    bluez.hci_filter_set_ptype(flt, bluez.HCI_EVENT_PKT)
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, flt )

    duration = 4
    max_responses = 255
    cmd_pkt = struct.pack("BBBBB", 0x33, 0x8b, 0x9e, duration, max_responses)
    bluez.hci_send_cmd(sock, bluez.OGF_LINK_CTL, bluez.OCF_INQUIRY, cmd_pkt)

    results = []

    done = False
    while not done:
        pkt = sock.recv(255)
        __, event, __ = struct.unpack("BBB", pkt[:3])
        if event == bluez.EVT_INQUIRY_RESULT_WITH_RSSI:
            pkt = pkt[3:]
            nrsp = bluetooth.get_byte(pkt[0])
            for i in range(nrsp):
                addr = bluez.ba2str( pkt[1+6*i:1+6*i+6] )
                rssi = bluetooth.byte_to_signed_int(
                        bluetooth.get_byte(pkt[1+13*nrsp+i]))
                results.append( ( addr, rssi ) )
        elif event == bluez.EVT_INQUIRY_COMPLETE:
            done = True
        elif event == bluez.EVT_CMD_STATUS:
            status, __, __ = struct.unpack("BBH", pkt[3:7])
            if status != 0:
                done = True
        elif event == bluez.EVT_INQUIRY_RESULT:
            pkt = pkt[3:]
            nrsp = bluetooth.get_byte(pkt[0])
            for i in range(nrsp):
                addr = bluez.ba2str( pkt[1+6*i:1+6*i+6] )
                results.append( ( addr, -1 ) )

    # restore old filter
    sock.setsockopt( bluez.SOL_HCI, bluez.HCI_FILTER, old_filter )

    return results

def perform_scan(dev_id = SCAN_DEVICE):
    sock = bluez.hci_open_dev(dev_id)
    mode = read_inquiry_mode(sock)

    if mode != 1:
        write_inquiry_mode(sock, 1)

    names = {}
    for line in bluetooth.discover_devices(duration=5, lookup_names=True, device_id = dev_id):
        names[line[0].lower()] = line[1]
    
    devices = {}
    for line in device_inquiry_with_with_rssi(sock):
        addr = line[0].lower()
        rssi = line[1]
        if addr in devices:
            devices[addr].append(rssi)
        else:
            devices[addr] = [rssi]
    
    result = {}
    for addr in names:
        name = names[addr]
        if addr not in devices:
            continue
        rssis = devices[addr];
        result[addr] =  [name, sum(rssis) / len(rssis)]
        
    return result

SPP_UUID = "00001101-0000-1000-8000-00805F9B34FB"
def have_spp(addr):
    services = bluetooth.find_service(address=addr)
    if len(services) == 1:
        rfcomm = services[0]["protocol"] == "RFCOMM"
        uuid = SPP_UUID in services[0]["service-classes"]
        if rfcomm and uuid:
            host = services[0]["host"]
            port = services[0]["port"]
            return (host, port)
    else:
        return False
        
def get_spp_port(addr):
    services = bluetooth.find_service(address=addr)
    if len(services) == 1:
        rfcomm = services[0]["protocol"] == "RFCOMM"
        uuid = SPP_UUID in services[0]["service-classes"]
        if rfcomm and uuid:
            return services[0]["port"]
    else:
        return 5
        
        
class bt_interface_classic():
    def __init__(self, addr):
        self.sock = bluetooth.BluetoothSocket(RFCOMM)
        conn = addr, get_spp_port(addr)
        self.sock.connect(conn)
        self.sock.setblocking(0)
    
    def read(self):
        try:
            data = self.sock.recv(1)
            return map(ord, data)
        except:
            #print "no data"
            return []
    
    def write(self, data):
        data = "".join(map(chr, data))
        self.sock.send(data)
    