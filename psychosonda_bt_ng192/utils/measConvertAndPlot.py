#!/usr/bin/python

import sys
import os
import ntpath
import struct
import argparse
import ConfigParser
import Tkinter
import FileDialog
import matplotlib
import matplotlib.image as mplimage
from bdfExport import BDFexport
import Tkinter, tkFileDialog
from pylab import *

csvDirectoryName = ""
path = ""
defaultPatientIniFile = "patient.ini"
defaultMeasurementFile = "measurement.RAW"
gravitationalAccelerationConstant = None
configurationFile = None
patientFilename = None
measurementFile = None
sharedXaxisLimit = 0
TkinterRoot = None

bio_ch1 = []
bio_ch2 = []
bio_odr = 0
bio_ch1_gain = 0
bio_ch2_gain = 0
bio_max_value = 8388607.0
hasBioData = False
bio_ref_voltage = 2420.0
bio1Avg = None
bio_csv_filename = "bioData.csv"

acc_x = []
acc_y = []
acc_z = []
acc_odr = 0
acc_scale = 0
acc_max_value = 32767.0
hasAccData = False
accxAvg = None
acc_csv_filename = "accData.csv"

gyro_x = []
gyro_y = []
gyro_z = []
gyro_odr = 0
gyro_scale = 0
gyro_bw = 0
gyro_max_value = 32767.0
hasGyroData = False
gyroxAvg = None
gyro_csv_filename = "gyroData.csv"

mag_x = []
mag_y = []
mag_z = []
mag_odr = 0
mag_scale = 0
mag_max_value = 32767.0
hasMagData = False
magxAvg = None
mag_csv_filename = "magData.csv"

baro = []
baro_odr = 0
hasBaroData = False
baroAvg = None
baro_csv_filename = "baroData.csv"

markers = []
markerSymbol = u'\u25CF'

signals = []
signalSymbol = u'\u25CF'


def generate_fake_config():
    global bio_odr
    global bio_ch1_gain
    global bio_ch2_gain
    global hasBio
    global hasBioData
    hasBioData = True
    hasBio = True
    bio_odr = 125
    bio_ch1_gain = 1
    bio_ch2_gain = 1

    global acc_odr
    global acc_scale
    global hasAcc
    global hasAccData
    hasAccData = True
    hasAcc = True
    acc_odr = 25
    acc_scale = 2

    global mag_odr
    global mag_scale
    global hasMag
    global hasMagData
    hasMagData = True
    hasMag = True
    mag_odr = 3
    mag_scale = 2

    global gyro_odr
    global gyro_scale
    global gyro_bw
    global hasGyro
    global hasGyroData
    hasGyroData = True
    hasGyro = True
    gyro_odr = 95
    gyro_scale = 250
    gyro_bw = 12

    global baro_odr
    global hasBaro
    global hasBaroData
    hasBaroData = True
    hasBaro = True
    baro_odr = 10

def generate_fake_data():
    for i in range(125):
        bio_ch1.append(i)
        bio_ch2.append(125-i)

    for i in range(25):
        acc_x.append(i)
        acc_y.append(i+25)
        acc_z.append(i-25)

    for i in range(95):
        gyro_x.append(i)
        gyro_y.append(i+95)
        gyro_z.append(i-95)

    for i in range(3):
        mag_x.append(i)
        mag_y.append(i+10)
        mag_z.append(i+20)

    for i in range(10):
        baro.append(i)

def to_dec_3(c):
    if (c < 0x800000):
        return c
    return c - 0x1000000

def read_ads(data):
    global hasBioData
    global bio1Avg
    global bio_ch1_gain
    global bio_ch2_gain

    if (len(data) != 6):
        print "Error - ADS data malformed", data
        return 

    hasBioData = True

    if data[2] & 0b10000000:
        a = 0xFF
    else:
        a = 0    

    text = chr(data[0]) + chr(data[1]) + chr(data[2]) + chr(a)     
    val_1, = struct.unpack("i", text)
    val_1 = (val_1 * bio_ref_voltage) / (float(bio_max_value)*float(bio_ch1_gain))
    bio_ch1.append(val_1)
    
    if data[5] & 0b10000000:
        a = 0xFF
    else:
        a = 0        

    text = chr(data[3]) + chr(data[4]) + chr(data[5]) + chr(a)     
    val_2, = struct.unpack("i", text)
    val_2 = (val_2 * bio_ref_voltage) / (float(bio_max_value)*float(bio_ch2_gain))
    bio_ch2.append(val_2)
    
    if bio1Avg == None:
        bio1Avg = val_1
    else:
        bio1Avg = (0.5 * val_1) + (1.0 - 0.5) * bio1Avg
    #print "ADS:", val_1, val_2

def read_baro(data):
    global hasBaroData
    global baroAvg

    if (len(data) != 2):
        print "Error - MAG data malformed", data
        return 

    hasBaroData = True

    if data[2] & 0b10000000:
        a = 0xFF
    else:
        a = 0    
    
    text = chr(data[0]) + chr(data[1]) + chr(data[2]) + chr(a)     
    val, = struct.unpack("i", text)
    val = val/1000.0
    baro.append(val)

    if baroAvg == None:
        baroAvg = val
    else:
        baroAvg = (0.5 * val) + (1.0 - 0.5) * baroAvg
    
    #print "BARO:", val

def read_acc(data):
    global hasAccData
    global acc_max_value
    global acc_scale
    global accxAvg

    hasAccData = True 
    
    for i in range(len(data) / 6):
        s = i * 6
        text = "".join(map(chr, data[s:s + 6]))
        
        if (len(text) != 6):
            print "Error - ACC data malformed", map(ord, text)
            return 
           
        x, y, z = struct.unpack("hhh", text)
        x = (x * float(acc_scale)) / acc_max_value
        y = (y * float(acc_scale)) / acc_max_value
        z = (z * float(acc_scale)) / acc_max_value

        xAcc = x * float(gravitationalAccelerationConstant)
        yAcc = y * float(gravitationalAccelerationConstant)
        zAcc = z * float(gravitationalAccelerationConstant)

        acc_x.append(xAcc)
        acc_y.append(yAcc)
        acc_z.append(zAcc)

        if accxAvg == None:
            accxAvg = xAcc
        else:
            accxAvg = (0.5 * xAcc) + (1.0 - 0.5) * accxAvg
        #print "ACC:", x, y, z
        
def read_gyro(data):
    global hasGyroData
    global gyro_scale
    global gyro_max_value
    global gyroxAvg

    hasGyroData = True

    for i in range(len(data) / 6):
        s = i * 6
        text = "".join(map(chr, data[s:s + 6]))   
        x, y, z = struct.unpack("hhh", text)
        x = x * float(gyro_scale) / gyro_max_value
        y = y * float(gyro_scale) / gyro_max_value
        z = z * float(gyro_scale) / gyro_max_value
        gyro_x.append(x)
        gyro_y.append(y)
        gyro_z.append(z)

        if gyroxAvg == None:
            gyroxAvg = x
        else:
            gyroxAvg = (0.5 * x) + (1.0 - 0.5) * gyroxAvg
        #print "GYRO:", x, y, z        
    
def read_mag(data):
    global hasMagData
    global mag_scale
    global mag_max_value
    global magxAvg

    hasMagData = True

    if (len(data) != 6):
        print "Error - MAG data malformed", data
        return 

    text = "".join(map(chr, data))   
    x, y, z = struct.unpack("hhh", text)
    x = (int(x) * float(mag_scale)) / mag_max_value
    y = (int(y) * float(mag_scale)) / mag_max_value
    z = (int(z) * float(mag_scale)) / mag_max_value

    xuTesla = x *  1000
    yuTesla = y *  1000
    zuTesla = z *  1000
    mag_x.append(xuTesla)
    mag_y.append(yuTesla)
    mag_z.append(zuTesla)

    if magxAvg == None:
        magxAvg = xuTesla
    else:
        magxAvg = (0.5 * xuTesla) + (1.0 - 0.5) * magxAvg

    #print "MAG:", x, y, z
    
def read_event(data):    
    if (len(data) != 5):
        print "Error - EVENT data malformed", data
        return     
    
    text = "".join(map(chr, data))   
    type, totalms = struct.unpack("=bL", text)
    t = "???"
    if (type == 0):
        t = "MARK"
    if (type == 1):
        t = "END"
    
    min = totalms / (1000.0 * 60)
    ms = totalms - min * (1000.0 * 60)
    sec = ms / (1000.0)
    ms -= sec * (1000.0)

    if type == 0:
        #print "Mark ",totalms/1000.0
        markers.append(totalms/1000.0)
    
#     TODO: add differner color
    if type == 2:
        print "Signal ",totalms/1000.0
        signals.append(totalms/1000.0)    
    
    #print "EVENT: %s at %02d:%02d.%02d" % (t, min, sec, ms)

def write_bio_settings(file):
    global bio_odr
    global bio_ch1_gain
    global bio_ch2_gain

    settings = "#BIO_DATARATE:"+str(bio_odr)+",CH1_GAIN:"+str(bio_ch1_gain)+",CH2_GAIN:"+str(bio_ch2_gain)+"\n"
    file.write(settings)

def write_bio_csv_data(file):
    for dataIndex in range(len(bio_ch1)):
        ch1data = bio_ch1[dataIndex]
        ch2data = bio_ch2[dataIndex]
        file.write(str(ch1data)+","+str(ch2data)+"\n")

def write_acc_settings(file):
    global acc_odr
    global acc_scale

    settings = "#ACC_DATARATE:"+str(acc_odr)+",ACC_SCALE:"+str(acc_scale)+"\n"
    file.write(settings)

def write_acc_csv_data(file):
    for dataIndex in range(len(acc_x)):
        x = acc_x[dataIndex]
        y = acc_y[dataIndex]
        z = acc_z[dataIndex]
        file.write(str(x)+","+str(y)+","+str(z)+"\n")

def write_gyro_settings(file):
    global gyro_odr
    global gyro_scale
    global gyro_bw

    settings = "#GYRO_DATARATE:"+str(gyro_odr)+",GYRO_SCALE:"+str(gyro_scale)+",GYRO_BANDWIDTH:"+str(gyro_bw)+"\n"
    file.write(settings)

def write_gyro_csv_data(file):
    for dataIndex in range(len(gyro_x)):
        x = gyro_x[dataIndex]
        y = gyro_y[dataIndex]
        z = gyro_z[dataIndex]
        file.write(str(x)+","+str(y)+","+str(z)+"\n")

def write_mag_settings(file):
    global mag_odr
    global mag_scale

    settings = "#MAG_DATARATE:"+str(mag_odr)+",GYRO_SCALE:"+str(mag_scale)+"\n"
    file.write(settings)

def write_mag_csv_data(file):
    for dataIndex in range(len(mag_x)):
        x = mag_x[dataIndex]
        y = mag_y[dataIndex]
        z = mag_z[dataIndex]
        file.write(str(x)+","+str(y)+","+str(z)+"\n")

def write_baro_settings(file):
    global baro_odr

    settings = "#BARO_DATARATE:"+str(baro_odr)+"\n"
    file.write(settings)

def write_baro_csv_data(file):
    for data in baro:
        file.write(str(data)+"\n")


def write_converted_data():
    global bio_csv_filename
    global acc_csv_filename
    global gyro_csv_filename
    global mag_csv_filename
    global baro_csv_filename
    global path
    global csvDirectoryName

    fullPathToCSVdir = os.path.join(path,csvDirectoryName)
    if not os.path.exists(fullPathToCSVdir):
        os.makedirs(fullPathToCSVdir)

    if hasBioData:
        f = open(os.path.join(fullPathToCSVdir,bio_csv_filename), "w+")
        write_bio_settings(f)
        write_bio_csv_data(f)
        f.close()

    if hasAccData:
        f = open(os.path.join(fullPathToCSVdir,acc_csv_filename), "w+")
        write_acc_settings(f)
        write_acc_csv_data(f)
        f.close()

    if hasGyroData:
        f = open(os.path.join(fullPathToCSVdir,gyro_csv_filename), "w+")
        write_gyro_settings(f)
        write_gyro_csv_data(f)
        f.close()

    if hasMagData:
        f = open(os.path.join(fullPathToCSVdir,mag_csv_filename), "w+")
        write_mag_settings(f)
        write_mag_csv_data(f)
        f.close()

    if hasBaroData:
        f = open(os.path.join(fullPathToCSVdir,baro_csv_filename), "w+")
        write_baro_settings(f)
        write_baro_csv_data(f)
        f.close()

id_time = 0
id_ads  = 1
id_acc  = 2
id_gyro = 3
id_mag  = 4
id_bmp  = 5
id_event= 6


def read_file(filename):
    f = open(filename, "rb")
    size = os.path.getsize(filename)
    
    index = 0
    
    wait_for_head = 0
    wait_for_len  = 1
    wait_for_data = 2
    
    
    state = wait_for_head
    
    while (1):
        index += 1
        if index == size:
            break
        c = ord(f.read(1))
        
        if (state == wait_for_head):
            id = (c & 0xF0) >> 4
            data_len = c & 0x0F
            data = []
            if (data_len == 0):
                state = wait_for_len
            else:
                state = wait_for_data
            continue
         
        if (state == wait_for_len):
            data_len = c
            state = wait_for_data
            continue
        
        if (state == wait_for_data):
            data_len -= 1
            data.append(c)
            
            if (data_len == 0):
                state = wait_for_head
                
                if (id == id_ads):
                    read_ads(data)
                    continue
                
                if (id == id_acc):
                    read_acc(data)
                    continue

                if (id == id_mag):
                    read_mag(data)
                    continue
                
                if (id == id_gyro):
                    read_gyro(data)
                    continue                

                if (id == id_bmp):
                    read_baro(data)
                    continue    
                
                if (id == id_event):
                    read_event(data)
                    continue              

                print "block id %d, size %d:" % (id, len(data))
                print "[",
                for v in data:
                    print ("%02X" % v),
                print "]"
                
                
                    
            continue


def getMaximumLength(maximumLengths):
    maxlen = 0
    for m in maximumLengths:
        if m > maxlen:
            maxlen = m
    return maxlen

def getMaximumLengthIndex(sampleLengths):
    index = 0
    maxIndex = 0
    maxlen = 0
    for m in sampleLengths:
        if m > maxlen:
            maxIndex = index
            maxlen = m
        index += 1
    return maxIndex

def getTotalCountOfAvailableGraphs(sampleLengths):
    total = 0
    for sl in sampleLengths:
        if sl > 0:
            total += 1
    return total

def getPeriod(frequency):
    if (frequency == 0):
        return 0
    return 1.0/frequency

def plotMarkers(avgValue):
    global markers

    markerValues = [avgValue for i in range(len(markers))]
    customMarker = "$%s$" % markerSymbol

    xx = arange(100)
    cut = (xx > 0) & (xx % 2 == 0)
    y1 = sin(xx)
    y2 = (xx**2) % 2.0+cos(xx+0.5)
#    scatter(markers, markerValues, c="red",s=0,zorder=2)
    for x in markers:
        axvline(x=x,ymin=0,ymax=1,c="red",linewidth=2,zorder=0, clip_on=False)
        #axvline(x=x,ymin=0,ymax=1.2,c="red",linewidth=2, zorder=0,clip_on=False)
    #plot(markers, markerValues, 'cD')#, marker=customMarker, markersize=12)

def plotSignals(avgValue):
    global signals

    markerValues = [avgValue for i in range(len(signals))]
    customMarker = "$%s$" % signalSymbol

    xx = arange(100)
    cut = (xx > 0) & (xx % 2 == 0)
    y1 = sin(xx)
    y2 = (xx**2) % 2.0+cos(xx+0.5)
#    scatter(signals, markerValues, c="black",s=0,zorder=2)
    for x in signals:
        axvline(x=x,ymin=0,ymax=1,c="black",linewidth=2,zorder=0, clip_on=False)
        #axvline(x=x,ymin=0,ymax=1.2,c="red",linewidth=2, zorder=0,clip_on=False)
    #plot(markers, markerValues, 'cD')#, marker=customMarker, markersize=12)


def plotGraph(currentAxis, data, frequency,titleText, ylabelText, xlabelText, xAxisLimit, hideXaxisTicks):
    title(titleText)
    ylabel(ylabelText)
    xlabel(xlabelText)

    sampleCount = len(data)
    period = getPeriod(frequency)
    t = arange(0, sampleCount * period, period)

    if len(t) > sampleCount:
        print "t:",len(t)," data:",sampleCount
        t = t[:sampleCount]
    grid(True)
    tight_layout()
    plot(t, data, zorder=1)

    if hideXaxisTicks:
        plt.tick_params(\
            axis='x',          # changes apply to the x-axis
            which='both',      # both major and minor ticks are affected
            bottom='off',      # ticks along the bottom edge are off
            top='off',         # ticks along the top edge are off
            labelbottom='off') # labels along the bottom edge are off

        # Remove first and last yaxis ticks
        yticks = currentAxis.yaxis.get_major_ticks()
        yticks[0].label1.set_visible(False)
        #yticks[-1].label1.set_visible(False) #last tick
    else:
        xlim([0,xAxisLimit])

def getFrequencyFit(data, totalTime):
    return (len(data)-1)/totalTime

# Checks configuration file validity by
# checking whether the file has at least
# one section
def checkConfigurationFileValidity(config):
    hasBio = config.has_section("bio")
    hasAcc = config.has_section("acc")
    hasGyro = config.has_section("gyro")
    hasMag = config.has_section("mag")
    hasBaro = config.has_section("baro")

    return (hasBio or hasAcc or hasGyro or hasMag or hasBaro)

def getSectionODR(config, section):
    return config.getint(section,"odr")

def getBioChannel1Gain(config):
    return config.getint("bio","ch1_gain")

def getBioChannel2Gain(config):
    return config.getint("bio","ch2_gain")

def getSectionScale(config, section):
    return config.getint(section,"scale")

def getGyroBW(config):
    return config.getint("gyro","bw")

def readMeasurementConfig(config):
    if config.has_section("bio"):
        global bio_odr
        global bio_ch1_gain
        global bio_ch2_gain
        print "Found section [bio]"
        bio_odr = getSectionODR(config, "bio")
        bio_ch1_gain = getBioChannel1Gain(config)
        bio_ch2_gain = getBioChannel2Gain(config)
    if config.has_section("gyro"):
        global gyro_odr
        global gyro_scale
        global gyro_bw
        global gyro_max_value
        print "Found section [gyro]"
        gyro_odr = getSectionODR(config, "gyro")
        gyro_scale = getSectionScale(config, "gyro")
        gyro_bw = getGyroBW(config)
    if config.has_section("acc"):
        global acc_odr
        global acc_scale
        global acc_max_value
        print "Found section [acc]"
        acc_odr = getSectionODR(config, "acc")
        acc_scale = getSectionScale(config, "acc")
    if config.has_section("mag"):
        global mag_odr
        global mag_scale
        global mag_max_value
        print "Found section [mag]"
        mag_odr = getSectionODR(config, "mag")
        mag_scale = getSectionScale(config, "mag")
    if config.has_section("baro"):
        global baro_odr
        print "Found section [baro]"
        baro_odr = getSectionODR(config, "baro")

class patientInfo:
    def __init__(self):
        pass

    def setName(self,patientsName):
        self.name = patientsName
        return self
    def getName(self):
        return self.name

    def setBirthdate(self, birthdate):
        self.birthdate = datetime.datetime.strptime(birthdate, '%d.%M.%Y').strftime('%d-%b-%Y').upper()
        return self
    def getBirthdate(self):
        return self.birthdate

    def setPatientCode(self, patientCode):
        self.patientCode = patientCode
        return self
    def getPatientCode(self):
        return self.patientCode

    def setPatientSex(self, sex):
        self.sex = sex
        return self
    def getPatientSex(self):
        return self.sex

    def setRecordingDate(self, recordingDate):
        self.recordingDate = recordingDate
        return self
    def getRecordingDate(self):
        return self.recordingDate

    def setRecordingTime(self, recordingTime):
        self.recordingTime = datetime.datetime.strptime(recordingTime,"%H:%M:%S").strftime("%H.%M.%S")
        return self
    def getRecordingTime(self):
        return self.recordingTime

    def setAdministrationCode(self, administrationCode):
        self.administrationCode = administrationCode
        return self
    def getAdministrationCode(self):
        return self.administrationCode

    def setTechnician(self, technician):
        self.technician = technician
        return self
    def getTechnician(self):
        return self.technician

    def setEquipment(self, equipment):
        self.equipment = equipment
        return self
    def getEquipment(self):
        return self.equipment

    def setTransducerType(self, transducerType):
        self.transducerType = transducerType
        return self
    def getTransducerType(self):
        return self.transducerType

patient = patientInfo()

def readPatientInfo(patientInfoFile):
    global patient

    if patientInfoFile.has_section("patient_info"):
        patientName = patientInfoFile.get("patient_info",'name')
        patientBirthdate = patientInfoFile.get("patient_info",'birthdate')
        patientCode = patientInfoFile.getint("patient_info",'patientcode')
        patientSex = patientInfoFile.get("patient_info",'sex')
        patient.setName(patientName).setBirthdate(patientBirthdate).setPatientCode(patientCode).setPatientSex(patientSex)

    if patientInfoFile.has_section("recording_info"):
        recordingDate = patientInfoFile.get("recording_info", 'recordingdate')
        recordingTime = patientInfoFile.get("recording_info", 'recordingtime')
        administrationCode = patientInfoFile.get("recording_info", 'administrationcode')
        technician = patientInfoFile.get("recording_info",'technician')
        equipment = patientInfoFile.get("recording_info",'equipment',)
        patient.setRecordingDate(recordingDate).setRecordingTime(recordingTime).setAdministrationCode(administrationCode).setTechnician(technician).setEquipment(equipment)

    if patientInfoFile.has_section("ecg_measurement"):
        transducerType = patientInfoFile.get("ecg_measurement",'transducertype')
        patient.setTransducerType(transducerType)

def plotBioGraph(sharedaxis,plotRowsCount, plotRowsColumns, plotPosition, sharedXaxisLimit, scaleToFit):
    global bio_odr
    global bio1Avg

    thisaxis = None
    if sharedaxis != None:
        thisaxis = subplot(plotRowsCount,plotRowsColumns,plotPosition, sharex=sharedaxis)
    else:
        thisaxis = subplot(plotRowsCount,plotRowsColumns,plotPosition)
        sharedaxis = thisaxis

    old_bio_odr = bio_odr
    if scaleToFit:
        bio_odr = getFrequencyFit(bio_ch1, sharedXaxisLimit)
    print "Scaling bio signal from "+str(old_bio_odr)+"Hz to "+str(bio_odr)

    plotGraph(thisaxis,bio_ch1, bio_odr,'', 'BIO Channel 1\n[mV]', '', sharedXaxisLimit, True)
    plotMarkers(bio1Avg)
    plotSignals(bio1Avg)
    #legend(['ECG'], loc='upper left')
    
    #Channel 2 plot
    thisaxis = subplot(plotRowsCount,plotRowsColumns,plotPosition+1, sharex=sharedaxis)  
    plotGraph(thisaxis,bio_ch2, bio_odr,'', 'BIO Channel 2\n[mV]', '', sharedXaxisLimit, True)
    plotMarkers(bio1Avg)
    plotSignals(bio1Avg)
    #legend(['Respiration'], loc='upper left')

    return sharedaxis

def plotAccGraph(sharedaxis,plotRowsCount, plotRowsColumns, plotPosition, sharedXaxisLimit, scaleToFit):
    global acc_odr
    global accxAvg

    thisaxis = None
    if sharedaxis != None:
        thisaxis = subplot(plotRowsCount,plotRowsColumns,plotPosition, sharex=sharedaxis)
    else:
        thisaxis = subplot(plotRowsCount,plotRowsColumns,plotPosition)
        sharedaxis = thisaxis

    old_acc_odr = acc_odr
    if scaleToFit:
        acc_odr = getFrequencyFit(acc_x, sharedXaxisLimit)
    print "Scaling acc signal from "+str(old_acc_odr)+"Hz to "+str(acc_odr)

    plotGraph(thisaxis,acc_x, acc_odr,'', 'Accelerometer\n[m.s-2]', '', sharedXaxisLimit, True)
    plotGraph(thisaxis,acc_y, acc_odr,'', 'Accelerometer\n[m.s-2]', '', sharedXaxisLimit, True)
    plotGraph(thisaxis,acc_z, acc_odr,'', 'Accelerometer\n[m.s-2]', '', sharedXaxisLimit, True)
    plotMarkers(accxAvg)
    plotSignals(accxAvg)
    
    legend(['x', 'y', 'z'], loc='upper left')

    return sharedaxis

def plotGyroGraph(sharedaxis,plotRowsCount, plotRowsColumns, plotPosition, sharedXaxisLimit, scaleToFit):
    global gyro_odr
    global gyroxAvg

    thisaxis = None
    if sharedaxis != None:
         thisaxis = subplot(plotRowsCount,plotRowsColumns,plotPosition, sharex=sharedaxis)
    else:
        thisaxis = subplot(plotRowsCount,plotRowsColumns,plotPosition)
        sharedaxis = thisaxis

    old_gyro_odr = gyro_odr
    if scaleToFit:
        gyro_odr = getFrequencyFit(gyro_x, sharedXaxisLimit)
    print "Scaling gyro signal from "+str(old_gyro_odr)+"Hz to "+str(gyro_odr)

    plotGraph(thisaxis,gyro_x, gyro_odr,'', 'Gyroscope\n[dps]', '', sharedXaxisLimit, True)
    plotGraph(thisaxis,gyro_y, gyro_odr,'', 'Gyroscope\n[dps]', '', sharedXaxisLimit, True)
    plotGraph(thisaxis,gyro_z, gyro_odr,'', 'Gyroscope\n[dps]', '', sharedXaxisLimit, True)
    plotMarkers(gyroxAvg)
    plotSignals(gyroxAvg)
        
    legend(['x', 'y', 'z'], loc='upper left')

    return sharedaxis

def plotMagGraph(sharedaxis,plotRowsCount, plotRowsColumns, plotPosition, sharedXaxisLimit, scaleToFit):
    global mag_odr
    global magxAvg

    thisaxis = None
    if sharedaxis != None:
        thisaxis = subplot(plotRowsCount,plotRowsColumns,plotPosition, sharex=sharedaxis)
    else:
        thisaxis = subplot(plotRowsCount,plotRowsColumns,plotPosition)
        sharedaxis = thisaxis

    old_mag_odr = mag_odr
    if scaleToFit:
        old_mag_odr = mag_odr
        mag_odr = getFrequencyFit(mag_x, sharedXaxisLimit)
    print "Scaling mag signal from "+str(old_mag_odr)+"Hz to "+str(mag_odr)

    plotGraph(thisaxis,mag_x, mag_odr,'', 'Magnetometer\n[uT]', '', sharedXaxisLimit, True)
    plotGraph(thisaxis,mag_y, mag_odr,'', 'Magnetometer\n[uT]', '', sharedXaxisLimit, True)
    plotGraph(thisaxis,mag_z, mag_odr,'', 'Magnetometer\n[uT]', '', sharedXaxisLimit, True)
    plotMarkers(magxAvg)
    plotSignals(magxAvg)
    legend(['x', 'y', 'z'], loc='upper left')

    return sharedaxis

def plotBaroGraph(sharedaxis,plotRowsCount, plotRowsColumns, plotPosition, sharedXaxisLimit, scaleToFit):
    global baro_odr
    global baroAvg

    thisaxis = None
    if sharedaxis != None:
        thisaxis = subplot(plotRowsCount,plotRowsColumns,plotPosition, sharex=sharedaxis)
    else:
        thisaxis = subplot(plotRowsCount,plotRowsColumns,plotPosition)
        sharedaxis = thisaxis

    old_baro_odr = baro_odr
    if scaleToFit:
        baro_odr = getFrequencyFit(baro, sharedXaxisLimit)
    print "Scaling baro signal from "+str(old_baro_odr)+"Hz to "+str(baro_odr)

    plotGraph(thisaxis,baro, baro_odr,'', 'Barometer\n[kPa]', 'time (s)', sharedXaxisLimit, False)
    plotMarkers(baroAvg)
    plotSignals(baroAvg)
    
    #legend(['p'], loc='upper left')

    return sharedaxis

def onresize(event):
    global fig

    width = event.width
    leftMargin = ((-13.0*width)/144000) + 0.2333333
    fig.subplots_adjust(left=leftMargin,bottom=0.06,right=0.99,top=0.95,hspace=0.0)

def pathLeaf(path):
    head, tail = ntpath.split(path)
    return tail or ntpath.basename(head)

def getPath(filePath):
    head, tail = ntpath.split(filePath)
    return head or os.path.dirname(os.path.realpath(sys.argv[0]))+"/"

def askForFile(extension, label, defaultFileName):
    global TkinterRoot

    if TkinterRoot == None:
        TkinterRoot = Tkinter.Tk()
        TkinterRoot.withdraw()

    file_opt = options = {}
    options['defaultextension'] = extension
    options['filetypes'] = [(label, extension)]
    #options['initialdir'] = 'C:\\'
    options['initialfile'] = defaultFileName+extension
    options['title'] = 'Choose '+label

    file_path = tkFileDialog.askopenfilename(**file_opt)
    TkinterRoot.destroy()
    TkinterRoot = None
    return file_path

def parseCommandLineArguments():
    global gravitationalAccelerationConstant
    global configurationFile
    global measurementFile
    global defaultMeasurementFile
    global patientFilename
    global path
    global csvDirectoryName
    
    parser = argparse.ArgumentParser(description='This program parses and plots measurement data from BIO probe.')
    parser.add_argument('-g', type=float, nargs='?',  const=9.81, default=9.81, help='value of gravitational acceleration')
    parser.add_argument('-p', nargs=1, help="Patient's info file [.ini]")
    parser.add_argument('-m', nargs=1, help="Measurement file [.RAW]")

    args = parser.parse_args()

    gravitationalAccelerationConstant = args.g

    measurementFile = args.m
    if not measurementFile:
        measurementFile = askForFile(".RAW", "MEASUREMENT FILE", "measurement")
    else:
        measurementFile = measurementFile[0]

    configurationFile = measurementFile.replace("RAW","CFG")

    patientFilename = args.p
    if not patientFilename:
        patientFilename = askForFile(".ini", "PATIENT INFO FILE", "patient")

    path = getPath(measurementFile)
    csvDirectoryName = pathLeaf(measurementFile).replace(".RAW","")

    #print "Measurement file:",measurementFile
    #print "Configuration file:",configurationFile
    #print "File directory path:",path
    #print "CSV directory path:",csvDirectoryName
    print "Using gravitational acceleration constant: ",gravitationalAccelerationConstant

def floatTo3Bytes(f):
    bytes = bytearray(4)
    struct.pack_into('f', bytes, 0, f)
    return bytes[1:]

def packData(dataArray):
    dataOut = ''
    for i in range(len(dataArray)):
        dataOut += floatTo3Bytes(dataArray[i])
    return dataOut

def exportDataToBDF(patientInfo):
    global hasBioData
    global hasAccData
    global acc_scale
    global hasGyroData
    global gyro_scale
    global hasMagData
    global mag_scale
    global hasBaroData
    global measurementFile
    global sharedXaxisLimit
    global path
    global csvDirectoryName
    global measurementFile

    bdfFile = BDFexport()
    bdfFile.setPatientIdentification(patientInfo.getPatientCode(), patientInfo.getPatientSex(), patientInfo.getBirthdate(), patientInfo.getName(), ' ')
    bdfFile.setRecordingIdentification(datetime.datetime.strptime(patientInfo.getRecordingDate(), '%d.%m.%Y').strftime('%d-%b-%Y').upper(), patientInfo.getAdministrationCode(), patientInfo.getTechnician(), patientInfo.getEquipment())

    if hasBioData:
        bdfFile.addSignal('ECG Channel 1', patientInfo.getTransducerType(), 'mV', -2420.0, 2420.0, -8388608, 8388607, ' ', packData(bio_ch1))
        bdfFile.addSignal('ECG Channel 2', patientInfo.getTransducerType(), 'mV', -2420.0, 2420.0, -8388608, 8388607, ' ', packData(bio_ch2))

    if hasAccData:
        bdfFile.addSignal('Acceleration X', ' ', 'm/s2', -acc_scale, acc_scale, -32767, 32767, ' ', packData(acc_x))
        bdfFile.addSignal('Acceleration Y', ' ', 'm/s2', -acc_scale, acc_scale, -32767, 32767, ' ', packData(acc_y))
        bdfFile.addSignal('Acceleration Z', ' ', 'm/s2', -acc_scale, acc_scale, -32767, 32767, ' ', packData(acc_z))

    if hasGyroData:
        bdfFile.addSignal('Gyroscope X', ' ', 'dps', -gyro_scale, gyro_scale, -32767, 32767, ' ', packData(gyro_x))
        bdfFile.addSignal('Gyroscope Y', ' ', 'dps', -gyro_scale, gyro_scale, -32767, 32767, ' ', packData(gyro_y))
        bdfFile.addSignal('Gyroscope Z', ' ', 'dps', -gyro_scale, gyro_scale, -32767, 32767, ' ', packData(gyro_z))

    if hasMagData:
        bdfFile.addSignal('Magnetometer X', ' ', 'uT', -mag_scale, mag_scale, -32767, 32767, ' ', packData(mag_x))
        bdfFile.addSignal('Magnetometer Y', ' ', 'uT', -mag_scale, mag_scale, -32767, 32767, ' ', packData(mag_y))
        bdfFile.addSignal('Magnetometer Z', ' ', 'uT', -mag_scale, mag_scale, -32767, 32767, ' ', packData(mag_z))

    if hasBaroData:
        bdfFile.addSignal('Barometer', ' ', 'kPa', -1, 1, -32767, 32767, ' ', packData(baro))

    bdfFile.addAnnotation(0.00,'Measurement Start')
    if len(markers) > 0:
        for marker in markers:
            bdfFile.addAnnotation(marker,' ')

    if len(signals) > 0:
        for signal in signals:
            bdfFile.addAnnotation(signal,'S')
    
    measurementFileBDF = os.path.join(path,csvDirectoryName,measurementFile.replace('.RAW',''))
    bdfFile.writeToFile(measurementFileBDF, datetime.datetime.strptime(patientInfo.getRecordingDate(), '%d.%m.%Y').strftime('%d.%m.%y'), patientInfo.getRecordingTime(), sharedXaxisLimit)

# -------------------------- PROGRAM START ----------------------------
parseCommandLineArguments()

MeasFilename = measurementFile
CfgFilename = configurationFile

print MeasFilename, CfgFilename

config = ConfigParser.ConfigParser()
config.read(CfgFilename)
readMeasurementConfig(config)

print "Reading configuration file.....",
if checkConfigurationFileValidity(config):
    print "Done"
else:
    print "Error - could not find configuration file"

print "Reading Data.....",
#try:
sys.stdout.flush()
read_file(MeasFilename)
print "Done"
#except Exception, e:
#    print "Error - could not find measurement file"
#    print e
#    exit()

print "Exporting Data to CSV.....",
sys.stdout.flush()
write_converted_data()
print "Done"

sampleLengths = [len(bio_ch1), len(bio_ch2), len(acc_x), len(gyro_x), len(mag_x), len(baro)]
maxSampleLengthIndex = getMaximumLengthIndex(sampleLengths)
totalSampleTimes = [(len(bio_ch1)-1)*getPeriod(bio_odr),
                    (len(bio_ch2)-1)*getPeriod(bio_odr),
                    (len(acc_x)-1)*getPeriod(acc_odr), 
                    (len(gyro_x)-1)*getPeriod(gyro_odr),
                    (len(mag_x)-1)*getPeriod(mag_odr),
                    (len(baro)-1)*getPeriod(baro_odr)]

print sampleLengths
print maxSampleLengthIndex
print totalSampleTimes

sharedXaxisLimit = totalSampleTimes[maxSampleLengthIndex]

if patientFilename:
    try:
        patientInfoFile = ConfigParser.ConfigParser()
        patientInfoFile.read(patientFilename)
        readPatientInfo(patientInfoFile)

        print "Exporting Data to BDF.....",
        exportDataToBDF(patient)
        print "Done"
    except:
        print "Error - patient.ini not found"
else:
    print "Not exporting to BDF because patient's file was not provided."

plotRowsCount = getTotalCountOfAvailableGraphs(totalSampleTimes)
plotColumnsCount = 1

currentPlotPosition = 1

fig = figure()
xaxis = None

if (len(bio_ch1) > 0) and config.has_section("bio"):
    scaleToFit = (maxSampleLengthIndex != 0)
    xaxis = plotBioGraph(xaxis,plotRowsCount,plotColumnsCount,currentPlotPosition,sharedXaxisLimit, scaleToFit)
    currentPlotPosition += 2 # Bio channels are split into separate graphs

if (len(acc_x) > 0) and config.has_section("acc"):
    scaleToFit = (maxSampleLengthIndex != 1)
    xaxis = plotAccGraph(xaxis,plotRowsCount,plotColumnsCount,currentPlotPosition,sharedXaxisLimit, scaleToFit)
    currentPlotPosition += 1

if (len(gyro_x) > 0) and config.has_section("gyro"):
    scaleToFit = (maxSampleLengthIndex != 2)
    xaxis = plotGyroGraph(xaxis,plotRowsCount,plotColumnsCount,currentPlotPosition,sharedXaxisLimit, scaleToFit)
    currentPlotPosition += 1

if (len(mag_x) > 0) and config.has_section("mag"):
    scaleToFit = (maxSampleLengthIndex != 3)
    xaxis = plotMagGraph(xaxis,plotRowsCount,plotColumnsCount,currentPlotPosition,sharedXaxisLimit, scaleToFit)
    currentPlotPosition += 1

if (len(baro) > 0) and config.has_section("baro"):
    scaleToFit = (maxSampleLengthIndex != 4)
    xaxis = plotBaroGraph(xaxis,plotRowsCount,plotColumnsCount,currentPlotPosition,sharedXaxisLimit, scaleToFit)
    currentPlotPosition += 1

fig.subplots_adjust(left=0.19,bottom=0.06,right=0.99,top=0.95,hspace=0.0)
connect('resize_event', onresize)
show()                       # Show plot and interact with user