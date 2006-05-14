import unittest

import native_gc_test

class NativeGcTestCase( unittest.TestCase ):
    def testGc( self ):
        """[GC] Test GC"""
        native_gc_test.gc_test()
