#!/usr/bin/python -O
#
# VideoLAN RTSP Server
#
# Author: Cyril Deguet <asmax@via.ecp.fr>


import cfg, SocketServer, string, sys

from playlist import PlayList
from rtsp import RtspServerHandler
from session import SessionList


PORT = 1554

if len(sys.argv) == 1:
    print "usage: vlrs <playlist>\n"
    sys.exit()

cfg.playlist = PlayList()
cfg.playlist.readConfig(sys.argv[1])
cfg.sessionList = SessionList()

rtspServer = SocketServer.TCPServer(('', PORT), RtspServerHandler)
try:
    rtspServer.serve_forever()
except KeyboardInterrupt:
    rtspServer.server_close()

