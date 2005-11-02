import vlc
import unittest

class BaseTestCase( unittest.TestCase ):
    def testStartup(self):
        """Checks that VLC starts"""
      	mc = vlc.MediaControl( ['--quiet'])
        mc.exit()

#    def testHelp(self):
#        """Check help string"""
#        mc=vlc.MediaControl( [ '--help'] )
