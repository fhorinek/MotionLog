import time
import threading
import traceback
import sys

ERROR = 0
WARN = 1
INFO = 2
DEBUG = 3

log_lock = threading.RLock()

class Logger():
    def __init__(self, name, filename = None):
        self.name = name
        self.levels = ["ERROR", "WARN", "INFO", "DEBUG"]
        
        if not filename:
            self.hfile = sys.stdout
        else:    
            self.hfile = open(filename, "a+")
        
        
    def log(self, message, level = ERROR):
        log_lock.acquire(True)
        self.hfile.write("[%s, %16s, %5s] %s\n" % (time.strftime("%H:%M %S"), self.name, self.levels[level], message))
        if level == ERROR:
            self.hfile.write("----- TRACEBACK --------------------------------------------------------------------\n")
            traceback.print_exc(self.hfile)
            self.hfile.write("------------------------------------------------------------------------------------\n")
        
        self.hfile.flush()
        log_lock.release()