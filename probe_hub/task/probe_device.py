import net.protocol as pr
import cfg
import configparser
import struct

import urllib2

from time import time

import os

from subprocess import Popen, PIPE


PROBE_TTL = 60.0 * 5
PROBE_WAIT = 0

class probe_device():
    def __init__(self, addr, parent):
        self.addr = addr
        self.parent = parent
        
        self.id = self.get_device_id(addr)

        self.busy = False 
        self.busy_timer = 0
        
        self.available = False
        
        self.connections = {}
        
    def get_device_id(self, addr):
        sql = "SELECT id from devices WHERE mac = %s"
        data = self.parent.db.query(sql, (addr, ))
        if len(data) == 0:
            sql = "INSERT INTO devices (id, mac, alias, conf_version) VALUES (NULL, %s, '', 0)" 
            data = self.parent.db.query(sql, (addr, ))
            return self.get_device_id(addr)
        
        return data[0][0];
        
    def log_activity(self, log_type, msg):
        sql = "INSERT INTO logs VALUES (NULL, %s, %s, %s, %s)"
        self.parent.db.query(sql, (self.id, log_type, time(), msg))

    def release(self):
        self.log_activity("release", "Releasing device")
        self.busy = False
        self.connections = {}
        
    def fail(self):
        self.log_activity("fail", "Fail to connect, Releasing device")
        self.busy = False
        self.connections = {}

    def update_connection(self, connection, rssi):
        if len(self.connections) == 0:
            self.available = time()
        self.connections[connection] = [rssi, time()]
        
    def work(self):
        #clean connections
        now = time()
        
        if self.busy_timer and now > self.busy_timer and self.busy:
            self.busy = False
            self.connections = {}        
            self.log_activity("released", "Busy lock released")
        
        to_remove = [] 
        for connection in self.connections:
            __, update = self.connections[connection]
            if now - update > PROBE_TTL:
                to_remove.append(connection)
                 
        for connection in to_remove:
            del self.connections[connection]
            
        if len(self.connections) == 0:
            self.available = False
            
        if self.available is not False and not self.busy: 
            if time() - self.available >= PROBE_WAIT:
                #scan done, create connection
                best = self.find_best_route()
                self.busy = True
                self.busy_timer = time() + 600.0
                
                self.log_activity("connect", best)
                packet = pr.Packet(pr.DEVICE_CONNECT, {"addr": self.addr})
                self.parent.net.send_packet(packet, best)
            
            
    def find_best_route(self):
        rssi_max = -9999
        best = False
        for connection in self.connections:
            rssi, __ = self.connections[connection]
            if rssi_max < rssi:
                rssi_max = rssi
                best = connection
                
        return best
    
    def mkdir(self, path):
        try:
            os.makedirs(path)
        except:
            pass    
    
    def decompress(self, data):
        cmd = [cfg.heatshrink_bin, "-d", "-w 10"]
        p = Popen(cmd, stdin=PIPE, stdout=PIPE)
        data_out, __ = p.communicate(data)
        
        return data_out
    
    def save_log(self, name, data, meta):
        self.log_activity("store", "%s (%0.2fkB, %0.2fkBps, %0.1f s)" % (name, len(data) / 1024.0, meta["speed"], meta["time"]))

        
        if (name[-3:] == "HTS"):
            try:
                data = self.decompress(data)
            except:
                self.log_activity("fail", "Unable to decompress the data")
                error = True

        try:
            head = data[0:14]
            __, __, conf_id, module_id, timestamp = struct.unpack("=bbIII", head)
            error = False
        except:
            self.log_activity("fail", "Invalid header structure")
            conf_id = -1
            module_id = -1
            timestamp = -1
            error = True

        sql = "INSERT INTO results VALUES (NULL, %s, %s, %s, %s, %s)"
        self.parent.db.query(sql, (self.id, conf_id, module_id, timestamp, data))
        
        sql = "SELECT LAST_INSERT_ID()"
        res = self.parent.db.query(sql)
        
        log_id = res[0][0]
        
        if not error:
            #notify server
            urllib2.urlopen(cfg.api_url + "?event=result&id=" + str(log_id))
        
    def write_file(self, path, data):
        f = open(path, "wb")
        f.write(data)
        f.close()        
        
    def read_file(self, path):
        f = open(path, "rb")
        data = f.read()
        f.close()
        
        return data
        
    def get_conf(self, remote):
        def u(lst):
            return [l.upper() for l in lst]
        
        add = {}
        rem = []
        
        sql = "SELECT name, configuration FROM tasks WHERE device_id = %s AND enabled = 1"
        res = self.parent.db.query(sql, (self.id, ))
        
        local = {}
        for name, data in res:
            name = "%08X.CFG" % name
            local[name] = data
            
        for name in local:
            if name.upper() not in u(remote):
                add[name.upper()] = local[name] 
                
        for conf in u(remote):
            if conf not in u(local):
                rem.append(conf)
                
        if len(add):
            self.log_activity("add_file", str(add.keys()))
        
        if len(rem):
            self.log_activity("rem_file", str(rem))
        
        return {"add": add, "remove": rem}    
    
    def get_fw(self, remote_fw):
        sql = "SELECT data FROM firmware WHERE device_id = %s OR device_id IS NULL ORDER BY device_id DESC, id DESC LIMIT 1"
        data = self.parent.db.query(sql, (self.id, ))       
            
        if not data:            
            return None
        
        data = data[0][0]
        fw = ""
        for c in data[0:32]:
            if c < 32 or c > 126:
                fw += "_"
            else:
                fw += chr(c)
                
        
        if fw <= remote_fw:
            return None
        
        self.log_activity("update", fw)
        return "".join(map(chr, data))
        
    def parse_cfg(self, s):
        try:
            c = configparser.ConfigParser()
            u = unicode(s, "utf-8")
            c.read_string(u)
            return c
        except:
            pass
        
        return None
            
    def cfg_get_default(self, c, s, v, d, t):
        try:
            return t(c.get(s, v))
        except:
            return d
    
    def parse(self, sender, data):
        if data.cmd == pr.DEVICE_CLOSED:
            self.release()

        if data.cmd == pr.DEVICE_FAIL:
            self.fail()
                
        if data.cmd == pr.DEVICE_ACQURED:
            self.connections = {}           
            self.busy_timer = time() + 60.0
            self.available = False
            self.log_activity("connected", "Device connected")
        
                
        if data.cmd == pr.DEVICE_LOG:
            name = data.payload["name"]
            text = data.payload["data"]
            meta = data.payload["meta"]
            
            self.save_log(name, text, meta)
            
        if data.cmd == pr.DEVICE_GET_CONF:
            conf = data.payload["conf"]
            remote_fw = data.payload["fw"]
            scfg = data.payload["cfg"]
            bat = data.payload["bat"]
            
            sql = "SELECT alias, conf_version FROM devices WHERE id = %s"
            data = self.parent.db.query(sql, (self.id, ))
            alias = data[0][0]
            version = data[0][1]
            
            send_server_cfg = True
            
            if scfg:
                device_cfg = self.parse_cfg(scfg)
                device_cfg_version = self.cfg_get_default(device_cfg, "cfg", "version", 0, int)
                
                if device_cfg_version > version:
                    device_cfg_alias = self.cfg_get_default(device_cfg, "cfg", "alias", "", str)
                    
                    sql = "UPDATE devices SET alias = %s, conf_version = %s WHERE id = %s"
                    self.parent.db.query(sql, (device_cfg_alias, device_cfg_version, self.id))                        
                    send_server_cfg = False         

                    #update configuration on server
                    
                    #wipe database config
                    sql = "DELETE FROM configuration WHERE device_id = %s"
                    self.parent.db.query(sql, (self.id, ))
                    #traverse scfg
                    if device_cfg.has_section("cfg"):
                        for name, value in device_cfg.items("cfg"):
                            if name in ["alias", "version"]:
                                continue
                            sql = "INSERT INTO configuration VALUES (NULL, %s, %s, %s)"
                            self.parent.db.query(sql, (self.id, name, value))

                if device_cfg_version == version:
                    send_server_cfg = False  

            if type(bat) == int:
                bat = [bat, 0]
                
            self.log_activity("battery", bat[0])
            self.log_activity("batt_raw", bat[1])
            self.log_activity("firmware", remote_fw)
            
            payload = self.get_conf(conf)
            payload["fw"] = self.get_fw(remote_fw)
            payload["addr"] = self.addr

            
            if send_server_cfg:
                sql = "SELECT name, value FROM configuration WHERE device_id = %s"
                data = self.parent.db.query(sql, (self.id, ))

                cfg = "[cfg]\n"
                data.append(("version", version))
                data.append(("alias", alias))

                for name, value in data:
                    cfg += "%s=%s\n" % (name, value)
                
                payload["add"]["CFG"] = cfg     
           
            packet = pr.Packet(pr.DEVICE_CONF, payload)
            self.parent.net.send_packet(packet, sender)   
            
            