import unittest

import native_libvlc_test

class NativeStatsTestCase( unittest.TestCase ):
    def testTimers( self ):
        """[Stats] Test timers"""
    	native_libvlc_test.timers_test()
