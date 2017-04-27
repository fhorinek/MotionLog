REQ_ID = 0x00
ANS_ID = 0x10

SCAN_IRQ = 0x20

DEVICE_MASK     = 0x40

DEVICE_CONNECT  = 0x40
DEVICE_CLOSED   = 0x41
DEVICE_FAIL     = 0x42
DEVICE_LOG      = 0x43
DEVICE_GET_CONF = 0x44
DEVICE_CONF     = 0x45
DEVICE_ACQURED  = 0x46

DUMMY           = 0xEF

class Packet():
    def __init__(self, cmd, payload = []):
        self.cmd = cmd        
        self.payload = payload
        
