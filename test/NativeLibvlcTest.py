import unittest

import native_libvlc_test

class NativeLibvlcTestCase( unittest.TestCase ):
    def testTls( self ):
        """[Thread] Set TLS"""
        native_libvlc_test.threadvar_test()
    def test1Exception( self ):
        """[LibVLC] Checks libvlc_exception"""
#    	native_libvlc_test.exception_test()
    def test2Startup( self ):
        """[LibVLC] Checks creation/destroy of libvlc"""
#    	native_libvlc_test.create_destroy()
    def test3Playlist( self ):
        """[LibVLC] Checks basic playlist interaction"""
#    	native_libvlc_test.playlist_test()
    def test4VLM( self ):
        """[LibVLC] Checks VLM wrapper"""
#    	native_libvlc_test.vlm_test()
