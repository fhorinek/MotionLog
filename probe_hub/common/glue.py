import threading
from multiprocessing import Pipe

class MyThread(threading.Thread):
    def __init__(self):
        threading.Thread.__init__(self)
        self.pipe_ext, self.pipe_int = Pipe()
        self.event = threading.Event()
        self.int_event = threading.Event()

    def wait(self, timeout):
        self.event.wait(timeout)
            
    def read(self):
        msgs = []
        self.event.clear()
        while self.pipe_ext.poll():
            msgs.append(self.pipe_ext.recv())
            
        return msgs

    def write(self, data): #write to internal_read
        self.int_event.set() 
        self.pipe_ext.send(data)

    def internal_wait(self, timeout):
        self.int_event.wait(timeout)
    
    def internal_read(self):
        msgs = []
        self.int_event.clear()
        while self.pipe_int.poll():
            msgs.append(self.pipe_int.recv())
            
        return msgs

    def internal_write(self, data): #write to read
        self.event.set() 
        self.pipe_int.send(data)