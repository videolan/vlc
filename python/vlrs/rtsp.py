#!/usr/bin/python
#
# VideoLAN RTSP Server
#
# Author: Cyril Deguet <asmax@via.ecp.fr>
#
# See: RFC 2326 Real Time Streaming Protocol
#      RFC 2327 Session Description Protocol


import cfg, mimetools, re, socket, time, SocketServer, string, sys

from sap import SdpMessage


class RtspServerHandler(SocketServer.StreamRequestHandler):
    "Request handler of the server socket"
    
    version = "RTSP/1.0"
    ok = "200 OK"
    badRequest = "400 Bad request"
    uriNotFound = "404 Not found"
    sessionNotFound = "454 Session not found"
    invalidHeader = "456 Header field not valid for resource"
    internalError = "500 Internal server error"
    notImplemented = "501 Not implemented"
    
    def error(self, message, cseq):
        self.wfile.write(self.version + " " + message + "\r\n" + \
                         "Cseq: " + cseq + "\r\n" + \
                         "\r\n")

    def parseHeader(self, header):
        "Split a RTCP header into a mapping of parameters"
        list = map(string.strip, re.split('[; \n]*', header, re.S))
        result = {}
        for item in list:
            m = re.match('([^=]*)(?:=(.*))?', item)
            if m is None:
                return None
            result[m.group(1)] = m.group(2)
        return result

    def optionsMethod(self):
        "Handle an OPTION request"
        response = "Public: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, PING, TEARDOWN\r\n" + \
                   "\r\n"
        return response

    def pingMethod(self, msg):
        "Handle a PING request"
        cseq = msg.getheader('cseq')
        id = msg.getheader('Session')
        if id is None:
            self.error(self.badRequest, cseq)
            return
        response = "Session: " + id + "\r\n" + \
                   "\r\n"
        return response

    def describeMethod(self, msg, uri):
        "Handle a DESCRIBE request"
        cseq = msg.getheader('cseq')
        
        # Find the URI in the playlist
        media = cfg.playlist.getMedia(uri)
        if media is None:
            self.error(self.uriNotFound, cseq)
            return None

        message = SdpMessage(media['name'], media['addr'], uri)
        description = message.getMessage()
        size = `len(description)`
        response = "Content-Type: application/sdp\r\n" + \
                   "Content-Length: " + size + "\r\n" + \
                   "\r\n" + description
        return response
                         
    def setupMethod(self, msg, uri):
        "Handle a SETUP request" 
        cseq = msg.getheader('cseq')
        
        # Find the URI in the playlist
        media = cfg.playlist.getMedia(uri)
        if media is None:
            self.error(self.uriNotFound, cseq)
            return None

        transportHeader = msg.getheader('transport')
        if transportHeader is None:
            self.error(self.badRequest, cseq)
            return None
        transport = self.parseHeader(transportHeader)

        # Check the multicast/unicast fields in the headers
        if transport.has_key('multicast'):
            type = "multicast"
        elif transport.has_key('unicast'):
            type = "unicast"
        else:
            self.error(self.invalidHeader, cseq)
            return None
            
        # Check the destination field in the headers
        dest= None
        if transport.has_key('destination'):
            dest = transport['destination']
        if dest is None:
            dest = media['addr']       # default destination address
            
        id = cfg.sessionList.newSession(uri, dest)
        if id is None:
            self.error(self.internalError, cseq)
            return None
        response = "Session: " + id + "\r\n" + \
                   "Transport: RTP/MP2T/UDP;" + type + ";destination=" + dest + "\r\n" + \
                   "\r\n"
        return response

    def playMethod(self, msg, uri):
        "Handle a PLAY request"
        cseq = msg.getheader('cseq')
        
        # Find the URI in the playlist
        media = cfg.playlist.getMedia(uri)
        if media is None:
            self.error(self.uriNotFound, cseq)
            return None

        id = msg.getheader('Session')
        session = cfg.sessionList.getSession(id)
        if session is None:
            self.error(self.sessionNotFound, cseq)
            return None
        if session.play() < 0:
            self.error(self.internalError, cseq)
            return None
        response = "Session: " + id + "\r\n" + \
                   "\r\n"
        return response

    def pauseMethod(self, msg, uri):
        "Handle a PAUSE request"
        cseq = msg.getheader('cseq')
        
        # Find the URI in the playlist
        media = cfg.playlist.getMedia(uri)
        if media is None:
            self.error(self.uriNotFound, cseq)
            return None
            
        id = msg.getheader('Session')
        session = cfg.sessionList.getSession(id)
        if session is None:
            self.error(self.sessionNotFound, cseq)
            return None
        if session.pause() < 0:
            self.error(self.internalError, cseq)
            return None
        response = "Session: " + id + "\r\n" + \
                   "\r\n"
        return response

    def teardownMethod(self, msg, uri):
        "Handle a TEARDOWN request"
        cseq = msg.getheader('cseq')
        
        # Find the URI in the playlist
        media = cfg.playlist.getMedia(uri)
        if media is None:
            self.error(self.uriNotFound, cseq)
            return None
            
        id = msg.getheader('Session')
        session = cfg.sessionList.getSession(id)
        if session is None:
            self.error(self.sessionNotFound, cseq)
            return None
        if session.stop() < 0:
            self.error(self.internalError, cseq)
            return None
        if cfg.sessionList.delSession(id) < 0:
            self.error(self.internalError, cseq)
            return None
        response = "\r\n"
        return response

    def parseRequest(self):
        "Parse a RSTP request"
        requestLine = self.rfile.readline()
        m = re.match("(?P<method>[A-Z]+) (?P<uri>(\*|(?:(?P<protocol>rtsp|rtspu)://" + \
                     "(?P<host>[^:/]*)(:(?P<port>\d*))?(?P<path>.*)))) " + \
                     "RTSP/(?P<major>\d)\.(?P<minor>\d)", requestLine)
        if m is None:
            self.error(self.badRequest, "0")
            return
        uri = m.group('uri')
        
        # Get the message headers
        msg = mimetools.Message(self.rfile, "0")
        cseq = msg.getheader('CSeq')
        if cseq is None:
            self.error(self.badRequest, "0")
            return
            
        method = m.group('method')
        if method == 'OPTIONS':
            response = self.optionsMethod()
        elif method == 'DESCRIBE':
            response = self.describeMethod(msg, uri)
        elif method == 'SETUP':
            response = self.setupMethod(msg, uri)
        elif method == 'PLAY':
            response = self.playMethod(msg, uri)
        elif method == 'PAUSE':
            response = self.pauseMethod(msg, uri)
        elif method == 'PING':
            response = self.pingMethod(msg)
        elif method == 'TEARDOWN':
            response = self.teardownMethod(msg, uri)
        else:
            self.error(self.notImplemented, cseq)
            return
 
        # Send the response
        if response is None:
            return
        else:
            self.wfile.write(self.version + " " + self.ok + "\r\n" + \
                             "CSeq: " + cseq + "\r\n" + \
                             response)
            
    def handle(self):
        "Handle an incoming request"
        while 1:
            try:
                self.parseRequest()
            except IOError:
                return
                         

