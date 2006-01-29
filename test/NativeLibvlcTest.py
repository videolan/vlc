import vlc
import unittest

import native_libvlc_test

class NativeLibvlcTestCase( unittest.TestCase ):
    def testException( self ):
        """Checks libvlc_exception"""
    	native_libvlc_test.exception_test()
    def testStartup( self ):
        """Checks creation/destroy of libvlc"""
    	native_libvlc_test.create_destroy()
