#!/usr/bin/python

import sys
import serial
from intelhex import IntelHex
import time
import datetime


def add8(a, b):
    return (a + b & 0xFF)

page_size = 255

class Hex2BinConv():
    def __init__(self, out):
        self.hex = IntelHex()
        self.out = out
 
        
    def load(self, filename):
        print
        print "Loading application from hex"
        self.hex.loadfile(filename, "hex")
        
        size = self.hex.maxaddr() - self.hex.minaddr()
        print " size: %0.2f KiB (%d B)" % (size/1024, size) 

    
    def conv(self, label):
        done = False
        adr = self.hex.minaddr()
        max_adr = self.hex.maxaddr()

        out_file = open(self.out, "wb");
        
        lab_str = '';
        
        for i in range(32):
            if i >= len(label):
                c = chr(0)
            else:
                c = label[i]
            lab_str += c
        
        print " label: %s" % lab_str
        print "Converting HEX 2 BIN ...", 
        
        out_file.write(lab_str)

        while(adr <= max_adr):
            out_file.write(chr(self.hex[adr]))

            adr += 1
        
        out_file.close()
        print "Done"
        
            
    def batch(self, filename, label):
        start = time.clock()
        
        self.load(filename)
        self.conv(label)
        
        end = time.clock()
        print 
        print "That's all folks! (%.2f seconds)" % (end - start)
            

if (len(sys.argv) < 3 or len(sys.argv) > 4):
    print "Usage %s hex_file output_file [label]" % __file__
    sys.exit(-1)
    
hex = sys.argv[1]
out = sys.argv[2]
label = ""  
if (len(sys.argv) == 4):
    label = sys.argv[3]
    
if (label == "" or label == "auto"):
    try:
        f = open("build_number.txt", 'r')
        num = int(f.read())
        f.close()
    except:
        num = 0
    
    num += 1
    
    f = open("build_number.txt", 'w')
    f.write("%u" % num)
    f.close()
    
    label = datetime.datetime.now().strftime("bio-%06u" % num)
    
#a = StaxProg("/dev/ttyACM0", 115200)
#a.batch("/home/horinek/data/workspace/RGBtest/Debug/RGBtest.hex")

a = Hex2BinConv(out)
a.batch(hex, label)

