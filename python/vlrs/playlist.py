#!/usr/bin/python -O
#
# VideoLAN RTSP Server
#
# Author: Cyril Deguet <asmax@via.ecp.fr>


import cfg, string, threading


class PlayList:
    "Contains the media playlist"

    def __init__(self):
        self.lock = threading.Lock()

    def readConfig(self, filename):
        "Read the playlist file"
        f = open(filename)
        newList = {}
        while 1:
            line = string.strip(f.readline())
            if line == "":
                break
            items = string.split(line, '\t')
            newList[items[0]] = {'file':items[1], 'name':items[2], 'addr':items[3]}
        self.lock.acquire()
        self.list = newList
        self.lock.release()
            
    def getMedia(self, uri):
        "Return the description of an item in the playlist"
        self.lock.acquire()
        if self.list.has_key(uri):
            media = self.list[uri]
        else:
            media = None
        self.lock.release()
        return media

