import unittest

import native_libvlc_test

class NativeProfilesTestCase( unittest.TestCase ):
    def testchains( self ):
        """[Streaming] Test sout chains handling"""
        native_libvlc_test.chains_test()
    def testchains2(self ):
        """[Streaming] Test sout chain interactions handling"""
    	native_libvlc_test.gui_chains_test()
    	native_libvlc_test.psz_chains_test()
