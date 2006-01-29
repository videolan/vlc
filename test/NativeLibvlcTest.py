import vlc
import unittest

import native_libvlc_test

class NativeLibvlcTestCase( unittest.TestCase ):
    def testMe( self ):
	native_libvlc_test.create_destroy()
