#!/usr/bin/python -O
#
# VideoLAN RTSP Server
#
# Author: Cyril Deguet <asmax@via.ecp.fr>

import cfg

from sap import SapServer
from session import Session


class AnnounceList:
    "List of streams to be announced"

    def __init__(self):
        # Create the SAP server
        self.multicastList = {}
        self.sapServer = SapServer()
        self.sapServer.start()

    def readPlaylist(self):
        pass

    def addMulticastSession(self, session):
        "Add a multicast session in the announce list"
        self.multicastList[session.id] = session
        
    def delMulticastSession(self, session):
        "Delete a multicast session from the announce list"
        del self.multicastList[session.id]
