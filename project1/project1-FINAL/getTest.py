#!/usr/bin/env python

from threading import Thread
from httplib import HTTPConnection
from BaseHTTPServer import BaseHTTPRequestHandler,HTTPServer
from datetime import datetime, timedelta
#from bcolor import bcolors
import re, socket, calendar
import sys
import time

class bcolors:
    PASS   = '\033[92m'
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'

    def disable(self):
        self.HEADER = ''
        self.OKBLUE = ''
        self.OKGREEN = ''
        self.WARNING = ''
        self.FAIL = ''
        self.ENDC = ''

class TestHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/basic":
            cdata = open("./basic", "r").read()
        if self.path == "/basic2":
            cdata = open("./basic2", "r").read()
        if self.path == "/basic3":
            cdata = open("./basic3", "r").read()
            time.sleep(3)
        if self.path == "/cacheTest":
            cdata = str(time.time())
        if self.path == "/basic4":
			#m_ts = calendar.timegm(time.strptime(lms, "%a, %d %b %Y %H:%M:%S GMT"))
			c_ts = calendar.timegm(time.gmtime())
			#lastModify=lms
			expireDate=(datetime.utcnow()+timedelta(seconds=5) - timedelta(days=1)).strftime("%a, %d %b %Y %H:%M:%S GMT")
			self.hit = True
			self.send_response(304)
			self.send_header('Expires',expireDate)
			cdata = open("./basic4", "r").read()

			#size = len(cdata)
            #self.send_header('Last-Modified', lastModify)
        else:
			size = len(cdata)
			expireDate=(datetime.now()+timedelta(days=1)).strftime("%a, %d %b %Y %H:%M:%S GMT")
			lastModify=(datetime.now()+timedelta(days=-1)).strftime("%a, %d %b %Y %H:%M:%S GMT")
			self.send_response(200)
			self.send_header('Content-Type','text/html')
			self.send_header('Content-Length', str(size))
			self.send_header('Expires',expireDate)
			self.send_header('Last-Modified', lastModify)
        if self.close_connection == True:
            self.send_header('Connection', 'close')
        self.end_headers()
        self.wfile.write(cdata)

        return

class ServerThread (Thread):
    def __init__(self, port):
        Thread.__init__(self)
        self.port = port

    def run(self):
        try:
            TestHandler.protocol_version = "HTTP/1.1"
            self.server = HTTPServer(('', self.port), TestHandler)
            self.server.serve_forever()
        except KeyboardInterrupt:
            self.server.socket.close()                                                                                                                                                       
    
class ClientThread (Thread):
    def __init__(self, proxy, url, file):
        Thread.__init__(self)
        self.proxy = proxy
        self.url = url
        self.file = file
        self.result = False
        self.data = ""

    def run(self):
        if self.file:
            dataFile = open(self.file, "r")
            cdata = dataFile.read()
        
            conn = HTTPConnection(self.proxy)
            conn.request("GET", self.url)
            resp = conn.getresponse()
            rdata = resp.read()
            if rdata == cdata:
                self.result = True
            self.data = rdata
            conn.close()
            dataFile.close()
        else:
            conn = HTTPConnection(self.proxy)
            conn.request("GET", self.url)
            resp = conn.getresponse()
            rdata = resp.read()
            print "Response: " , rdata
            if resp.status == httplib.OK:
                self.result = True
            conn.close()


class ClientPersistThread(Thread):
    def __init__(self, proxy, url, file, url2, file2):
        Thread.__init__(self)
        self.proxy = proxy
        self.url = url
        self.file = file
        self.url2 = url2
        self.file2 = file2
        self.result = False

    def run(self):
        conn = HTTPConnection(self.proxy)
        tmpFlag = True

        dataFile = open(self.file, "r")
        cdata = dataFile.read()
        dataFile = open(self.file2, "r")
        cdata2 = dataFile.read()

        conn.request("GET", self.url)
        resp = conn.getresponse()
        rdata = resp.read()
        if rdata != cdata:
            tmpFlag = False
            
        if resp.will_close == True:
            tmpFlag = False

        connHdrs = {"Connection": "close"}
        conn.request("GET", self.url2, headers=connHdrs)

        resp = conn.getresponse()
        rdata2 = resp.read()
        if rdata2 != cdata2:
            tmpFlag = False

        if resp.will_close == False:
            tmpFlag = False

        if tmpFlag == True:
            self.result = True
        conn.close()
        dataFile.close()

try:
    conf = open("./portconf", "r")
    pport  = conf.readline().rstrip().split(':')[1]
    sport1 = conf.readline().rstrip().split(':')[1]
    sport2 = conf.readline().rstrip().split(':')[1]

    b1 = open("./basic", "w")
    b1.write("basic\n")
    b2 = open("./basic2", "w")
    b2.write("aloha\n")
    b3 = open("./basic3", "w")
    b3.write("cat\n")
    b4 = open("./basic4", "w")
    b4.write("YEAH BOY\n")
    b1.close()
    b2.close()
    b3.close()
    b4.close()

    server1 = ServerThread(int(sport1))
    server2 = ServerThread(int(sport2))

    server1.start()
    server2.start()
	
	



    # client1 = ClientThread("127.0.0.1:" + pport, "http://127.0.0.1:" + sport1 + "/basic", "./basic")
    # client1.start()
    # client1.join()
    # if client1.result:
        # print "Basic object fetching: [" + bcolors.PASS + "PASSED" + bcolors.ENDC + "]" 
    # else: 
        # print "Basic object fetching: [" + bcolors.FAIL + "FAILED" + bcolors.ENDC + "]"
		
    client2 = ClientThread("127.0.0.1:" + pport, "http://127.0.0.1:" + sport1 + "/basic4", "./basic4")
    client2.start()
    client2.join()
    if client2.result:
        print "OUR TEST: [" + bcolors.PASS + "PASSED" + bcolors.ENDC + "]" 
    else: 
        print "OUR TEST: [" + bcolors.FAIL + "FAILED" + bcolors.ENDC + "]"


	

except:
    server1.server.shutdown()
server1.server.shutdown()
server2.server.shutdown()
