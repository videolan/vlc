import vlc
import unittest

import native_stats_test

class NativeStatsTestCase( unittest.TestCase ):
    def testTimers( self ):
        """[Stats] Test timers"""
    	native_stats_test.timers_test()
