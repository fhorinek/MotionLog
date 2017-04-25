import bluepy.btle as ble

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