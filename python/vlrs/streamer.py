#!/usr/bin/python -O
#
# VideoLAN RTSP Server
#
# Author: Cyril Deguet <asmax@via.ecp.fr>

import cfg, vlc


class VlcError(Exception):
    "Exception class for libvlc calls"
    pass



class VlcStreamer:
    "Manage a streamer with libvlc"
    
    def __init__(self, file, address):
        "Create the streamer"
        self.file = file
        self.address = address
        self.id = vlc.create()
        if self.id < 0:        
            raise VlcError
        if vlc.init(self.id, self.address) < 0:
            raise VlcError
        if vlc.addTarget(self.id, self.file) < 0:
            raise VlcError
            
    def play(self):
        "Play the stream"
        if vlc.play(self.id) < 0:
            raise VlcError
            
    def stop(self):
        "Stop the stream"
        if vlc.stop(self.id) < 0:
            raise VlcError
            
    def pause(self):
        "Pause the stream"
        if vlc.pause(self.id) < 0:
            raise VlcError


