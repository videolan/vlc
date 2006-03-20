import unittest

import native_libvlc_test

class NativeLibvlcTestCase( unittest.TestCase ):
    def testException( self ):
        """[LibVLC] Checks libvlc_exception"""
    	native_libvlc_test.exception_test()
    def testStartup( self ):
        """[LibVLC] Checks creation/destroy of libvlc"""
    	native_libvlc_test.create_destroy()
    def testPlaylist( self ):
        """[LibVLC] Checks basic playlist interaction"""
    	native_libvlc_test.playlist_test()
    def testVLM( self ):
        """[LibVLC] Checks VLM wrapper"""
    	native_libvlc_test.vlm_test()
