#!/usr/bin/python -O
#
# VideoLAN RTSP Server
#
# Author: Cyril Deguet <asmax@via.ecp.fr>


import cfg, random, time

from streamer import VlcError, VlcStreamer


class Session:
    "RTSP Session"
    
    def __init__(self, id, uri, dest):
        self.id = id
        self.uri = uri
        self.dest = dest
        self.state = 'ready'
        media = cfg.playlist.getMedia(self.uri)
        self.fileName = media['file']
        self.name = media['name']
        address = "rtp/ts://" + dest
        self.streamer = VlcStreamer(self.fileName, address)
        
    def play(self):
        "Play this session"
        if self.state == 'playing':
            print "Session " + self.id + " (" + self.fileName + "): already playing"
            return 0
        self.state = 'playing'
        print "Session " + self.id + " (" + self.fileName + "): play"
        try:
            self.streamer.play()
        except VlcError:
            print "Streamer: play failed"
            return -1
        cfg.announceList.addMulticastSession(self)
        return 0

    def pause(self):
        "Pause this session"
        print "Session " + self.id + " (" + self.fileName + "): pause"
        self.state = 'ready'
        try:
            self.streamer.pause()
        except VlcError:
            print "Streamer: pause failed"
            return -1
        return 0

    def stop(self):
        "Stop this session"
        print "Session " + self.id + " (" + self.fileName + "): stop"
        try:
            self.streamer.stop()
        except VlcError:
            print "Streamer: stop failed"
            return -1
        return 0



class SessionList:
    "Manages RTSP sessions"

    list = {}
    chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ"

    def __init__(self):
        self.rand = random.Random(time.time())
    
    def newSessionId(self):
        "Build a random session id"
        id = ""
        for x in range(12):
            id += self.chars[self.rand.randrange(0, len(self.chars), 1)]
        return id

    def newSession(self, uri, dest):
        "Create a new RTSP session"
        id = self.newSessionId()
        while self.list.has_key(id):
            id = self.newSessionId()
        try:
            session = Session(id, uri, dest)
        except VlcError:
            print "Streamer: creation failed"
            return None
        self.list[id] = session
        print "New session: " + id
        return id
        
    def getSession(self, id):
        "Get a session from its session id"
        if self.list.has_key(id):
            return self.list[id]
        else:
            return None

    def delSession(self, id):
        "Delete a session"
        if self.list.has_key(id):
            del self.list[id]
            return 0
        else:
            return -1


