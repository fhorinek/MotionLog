import mysql.connector
import cfg
import common.log as log


class db_conn(log.Logger):
    def __init__(self):
        log.Logger.__init__(self, "mysql")
        self.con = None
        
        self.connect()
        
    def connect(self):
        try:
            self.log("Connectiong to database", log.INFO)
            self.con = mysql.connector.connect(host=cfg.db_host, user=cfg.db_user, passwd=cfg.db_pass, db=cfg.db_name)
        except:
            self.con = None
            self.log("could not connect to database", log.ERROR)
            
    def query(self, query, params = None):
        try:
            if params is not None:
                q = query % params
            else:
                q = query
            cur = self.con.cursor()
            self.log("QUERY: %s" % q, log.DEBUG)
            cur.execute(query, params)
            try:
                data = cur.fetchall()
            except:
                data = None
                
            self.con.commit()
        except:
            self.log("Error executing query \n>>%s<<" % q, log.ERROR)
            return None
        
        return data