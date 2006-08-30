import unittest

import native_libvlc_test

class NativeURLTestCase( unittest.TestCase ):
    def testurl_decode( self ):
        """[URL] Test url_decode and base64"""
    	native_libvlc_test.url_test()
