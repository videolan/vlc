#!/usr/bin/python -O
#
# VideoLAN RTSP Server
#
# Author: Cyril Deguet <asmax@via.ecp.fr>


import cfg,socket,struct,time,threading


def ntpTime():
    "Return the current time in NTP decimal format"
    return "%d" % (int(time.time()) + 2208988800L)



class SdpMessage:
    "Build a SDP message"
 
    uri = "http://www.videolan.org/"

    def __init__(self, sessionName, address, uri):
        "Build the message"
        self.sessionName = sessionName
        self.address = address
        self.uri = uri
        
    def getMessage(self):
        "Return the SDP message"
        msg = "v=0\r\n" + \
              "o=asmax " + ntpTime() + " " + ntpTime() + \
                  " IN IP4 sphinx.via.ecp.fr\r\n" + \
              "s=" + self.sessionName + "\r\n" + \
              "u=" + self.uri + "\r\n" + \
              "t=0 0\r\n" + \
              "c=IN IP4 " + self.address + "/1\r\n" + \
              "m=video 1234 RTP/MP2T 33\r\n" + \
              "a=control:" + self.uri + "\r\n"
        return msg



class SapServer(threading.Thread):
    "SAP server class"

    PORT = 9875
    GROUP = "224.2.127.254"
    TTL = 1

    def __init__(self):
        # Open the socket
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.IPPROTO_IP, socket.IP_MULTICAST_TTL, self.TTL)
        self.sock.connect((self.GROUP, self.PORT))

    def sendMessage(self, message):
        "Message must be a SdpMessage"
        # FIXME
        header = " " + struct.pack("!BH", 12, 4212) + socket.inet_aton('138.195.156.214') 
        data = header + message.getMessage()
        self.sock.send(data)

    def announce(self):
        for id, session in cfg.announceList.multicastList.items():
            message = SdpMessage(session.name, session.dest, session.uri)
            self.sendMessage(message)

    def run(self):
        while 1:
            self.announce()
            time.sleep(1)
