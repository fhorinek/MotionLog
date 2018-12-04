import time
import threading
import traceback
import sys

ERROR = 0
WARN = 1
INFO = 2
DEBUG = 3

MAX_LENGTH = 200

log_lock = threading.RLock()

class Logger():
    def __init__(self, name, filename = None, level = DEBUG):
        self.name = name
        
        self.levels = ["ERROR", "WARN", "INFO", "DEBUG"]
        self.level = level
        
        if not filename:
            self.hfile = sys.stdout
        else:    
            self.hfile = open(filename, "a+")
        
        
    def shut_up(self):
        self.silent = True
        
    def log(self, message, msg_level = ERROR):
        if msg_level > self.level:
            return
        
        message = str(message)
        if len(message) > MAX_LENGTH:
            message = message[:MAX_LENGTH] + "..."
    
        log_lock.acquire(True)
        self.hfile.write("[%s, %16s, %5s] %s\n" % (time.strftime("%H:%M %S"), self.name, self.levels[msg_level], message))
        if msg_level == ERROR:
            self.hfile.write("----- TRACEBACK --------------------------------------------------------------------\n")
            traceback.print_exc(self.hfile)
            self.hfile.write("------------------------------------------------------------------------------------\n")
        
        self.hfile.flush()
        log_lock.release()
