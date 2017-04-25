import sys
import struct

class BDFsignal(object):
    def __init__(self, label, transducerType, physicalDimension, physicalMininum, physicalMaximum, digitalMinimum, digitalMaximum, prefilteringInfo, data):
        self.label = "{0:16s}".format(label)
        self.transducerType = "{0:80s}".format(transducerType)
        self.physicalDimension = "{0:8s}".format(physicalDimension)
        self.physicalMininum = "{0:8s}".format(str(physicalMininum))
        self.physicalMaximum = "{0:8s}".format(str(physicalMaximum))
        self.digitalMinimum = "{0:8s}".format(str(digitalMinimum))
        self.digitalMaximum = "{0:8s}".format(str(digitalMaximum))
        self.prefilteringInfo = "{0:80s}".format(prefilteringInfo)
        self.data = data

    def getLabel(self):
        return self.label

    def getTransducerType(self):
        return self.transducerType

    def getPhysicalDimension(self):
        return self.physicalDimension

    def getPhysicalMinimum(self):
        return self.physicalMininum

    def getPhysicalMaximum(self):
        return self.physicalMaximum

    def getDigitalMinimum(self):
        return self.digitalMinimum

    def getDigitalMaximum(self):
        return self.digitalMaximum

    def getPrefilteringInfo(self):
        return self.prefilteringInfo

    def getData(self):
        return self.data

class BDFheaderBuilder(object):
    def __init__(self):
        self.fileVersion = '\xFFBIOSEMI'
        self.patientIdentification = ''
        self.recordingIdentification = ''
        self.startDateOfRecording = ''
        self.startTimeOfRecording = ''
        self.headerSize = '0'
        self.bdfType = 'BDF+C                                       '
        self.numberOfDataRecords = '1       '
        self.durationOfDataRecordInSeconds = '0'
        self.numberOfSignalsInDataRecord = '0'

    def setPatientIdentification(self, patientCode, sex, birthdate, name, additionalInfo):
        _patientCode = "MCH-"+ "{0:07d}".format(patientCode)+' '    
        _sex = sex[0]+' '
        _birthdate = birthdate+' '
        _name = name+' '
        _additionalInfo = additionalInfo

        self.patientIdentification = "{0:80s}".format(_patientCode+_sex+_birthdate+_name+_additionalInfo)
        return self

    '''startdate - 27-FEB-1991
       administrationCode = 0000/00
       technicianInitials = MT
       additionalInfo - String'''
    def setRecordingIdentification(self, startdate, administrationCode, technicianInitials, additionalInfo):
        _startdate = "Startdate "+startdate
        _psg = "PSG:"+administrationCode

        self.recordingIdentification = "{0:80s}".format(_startdate+' '+_psg+' '+technicianInitials+' '+additionalInfo)
        return self

    def setStartDateOfRecording(self, startDate):
        self.startDateOfRecording = startDate
        return self

    def setStartTimeOfRecording(self, startTime):
        self.startTimeOfRecording = startTime
        return self

    def setRecordingDurationInSeconds(self, recordingLength):
        self.durationOfDataRecordInSeconds = "{0:8s}".format(str(recordingLength))
        return self

    def setNumberOfSignals(self, numberOfSignals):
        self.numberOfSignals = "{0:4s}".format(str(numberOfSignals))
        return self

    def toString(self):
        headerString = ''+ self.fileVersion
        headerString += self.patientIdentification
        headerString += self.recordingIdentification
        headerString += self.startDateOfRecording
        headerString += self.startTimeOfRecording
        self.headerSize = 256 + (int(self.numberOfSignals))*(16+80+8+8+8+8+8+80+8+32)
        self.headerSize = "{0:8s}".format(str(self.headerSize))
        headerString += self.headerSize
        headerString += self.bdfType
        headerString += self.numberOfDataRecords
        headerString += self.durationOfDataRecordInSeconds
        headerString += self.numberOfSignals
        return headerString





class BDFexport:

    def __init__(self):
        self.fileName = 'measurement.bdf'
        self.fileHandle = None
        self.header = BDFheaderBuilder()
        self.signals = []
        self.annotations = []

    '''patientCode - Integer
       sex - M or F 
       birtdate - 27-FEB-1991
       name = String
       additionalInfo - String'''
    def setPatientIdentification(self, patientCode, sex, birthdate, name, additionalInfo):
        self.header.setPatientIdentification(patientCode, sex, birthdate, name, additionalInfo)
    
    '''startdate - 27-FEB-1991
       administrationCode = 0000/00
       technicianInitials = MT
       additionalInfo - String'''
    def setRecordingIdentification(self, startdate, administrationCode, technicianInitials, additionalInfo):
        self.header.setRecordingIdentification(startdate, administrationCode, technicianInitials, additionalInfo)

    def addSignal(self, label, transducer, physicalDimension, physicalMininum, physicalMaximum, digitalMinimum, digitalMaximum, prefilteringInfo, data):
        newSignal = BDFsignal(label, transducer, physicalDimension, physicalMininum, physicalMaximum, digitalMinimum, digitalMaximum, prefilteringInfo, data)
        self.signals.append(newSignal)
        self.header.setNumberOfSignals(len(self.signals)+1)

    def _createFirstAnnotation(self, onset, label):
        newAnnotation = '+0\x14\x14\x00'
        newAnnotation +='+'+str(onset)+'\x15'
        newAnnotation +='0.000001\x14' #duration
        newAnnotation +=label+'\x14\x00'
        self.annotations.append(newAnnotation)

    def _addPaddingToAnnotations(self):
        totalAnnotationLength = 0
        for annotationIndex in range(len(self.annotations)):
            totalAnnotationLength += len(self.annotations[annotationIndex])

        lastAnnotation = self.annotations[len(self.annotations)-1]
        paddingLength = 0
        if (totalAnnotationLength % 3) != 0:
            paddingLength = (int(totalAnnotationLength / 3)+1)*3 - totalAnnotationLength

        for paddingIndex in range(paddingLength):
            lastAnnotation += '\x00'

        self.annotations[len(self.annotations)-1] = lastAnnotation


    def addAnnotation(self, onset_sec, label):
        if len(self.annotations) == 0:
            self._createFirstAnnotation(onset_sec, label)
        else:
            newAnnotation = '+'+str(onset_sec)+'\x14'+label+'\x14\x00'
            self.annotations.append(newAnnotation)

    def _generateSignalHeaderData(self):
        signalHeaderData = ''
        includeAnnotationSignal = (len(self.annotations) > 0)

        if includeAnnotationSignal:
            signalHeaderData += 'BDF Annotations '
        for sig in self.signals:
            signalHeaderData += sig.getLabel()

        if includeAnnotationSignal:
            signalHeaderData += '                                                                                '
        for sig in self.signals:
            signalHeaderData += sig.getTransducerType()

        if includeAnnotationSignal:
            signalHeaderData += '        '
        for sig in self.signals:
            signalHeaderData += sig.getPhysicalDimension()

        if includeAnnotationSignal:
            signalHeaderData += '-1      '
        for sig in self.signals:
            signalHeaderData += sig.getPhysicalMinimum()

        if includeAnnotationSignal:
            signalHeaderData += '1       '
        for sig in self.signals:
            signalHeaderData += sig.getPhysicalMaximum()

        if includeAnnotationSignal:
            signalHeaderData += '-8388608'
        for sig in self.signals:
            signalHeaderData += sig.getDigitalMinimum()

        if includeAnnotationSignal:
            signalHeaderData += '8388607 '
        for sig in self.signals:
            signalHeaderData += sig.getDigitalMaximum()

        if includeAnnotationSignal:
            signalHeaderData += '                                                                                '
        for sig in self.signals:
            signalHeaderData += sig.getPrefilteringInfo()

        if includeAnnotationSignal:
            self._addPaddingToAnnotations()
            annotationsLength =  0 #self.longestAnnotationLength * len(self.annotations) / 3
            for annotation in self.annotations:
                annotationsLength += len(annotation)
            annotationSamples = annotationsLength/3
            signalHeaderData += "{0:8s}".format(str(annotationSamples))
        for sig in self.signals:
            signalHeaderData += "{0:8s}".format(str(len(sig.getData()) / 3))

        if includeAnnotationSignal:
            signalHeaderData += '                                '
        for sig in self.signals:
            signalHeaderData += '                                '

        return signalHeaderData

    def writeToFile(self, fileName, startDateOfRecording, startTimeOfRecording, recordingDurationInSeconds):
        self.fileName = fileName
        self.fileHandle = open(fileName+".BDF", "wb+")

        if self.fileHandle:
            headerData = self.header.setStartDateOfRecording(startDateOfRecording).setStartTimeOfRecording(startTimeOfRecording).setRecordingDurationInSeconds(recordingDurationInSeconds).toString()
            self.fileHandle.write(headerData)

            signalHeaderData = self._generateSignalHeaderData()
            self.fileHandle.write(signalHeaderData)

            includeAnnotationSignal = (len(self.annotations) > 0)
            if includeAnnotationSignal:
                for annotation in self.annotations:
                    self.fileHandle.write(annotation)

            for sig in self.signals:
                self.fileHandle.write(sig.getData())

            self.fileHandle.close()