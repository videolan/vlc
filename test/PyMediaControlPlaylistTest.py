import vlc
import unittest

# FIXME: How to avoid creating / killing vlc for each test method ?

class VariablesTestCase( unittest.TestCase ):
    """[PyMC] Test misc variables interaction"""
    def setUp( self ):
        self.mc = vlc.MediaControl( [ '--quiet'] )

    def tearDown( self ):
        self.mc.exit()
           
    def testSimple( self ):
        """[PyMC] Check simple add"""
        assert len( self.mc.playlist_get_list() ) == 0
        self.mc.playlist_add_item( "test" )
        assert len( self.mc.playlist_get_list() ) == 1
