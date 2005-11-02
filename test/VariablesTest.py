import vlc
import unittest

# FIXME: How to avoid creating / killing vlc for each test method ?

class VariablesTestCase( unittest.TestCase ):
    """Test misc variables interaction"""
    def setUp( self ):
        self.mc = vlc.MediaControl( [ '--quiet'] )
        # FIXME ! - Get this through children test
        self.libvlc = vlc.Object(1)
        self.playlist = vlc.Object(268)

    def tearDown( self ):
        self.playlist.release() 
        self.libvlc.release() 
        self.mc.exit()
            
    # The Python binding can't create variables, so just get default ones
    def testInt( self ):
        """Get/Set integer variable"""
        assert self.libvlc.get( "width" ) == 0
        self.libvlc.set( "width", 42 ) 
        assert self.libvlc.get( 'width' ) == 42

    # FIXME: Python binding should listen to return value and raise exception 
    def testInvalidInt( self ):
        """Get/Set invalid integer"""
        self.libvlc.set( "width" , 5 )
        self.libvlc.set( "width", "foo" )
        assert self.libvlc.get( "width" ) == -1
    
    def testString( self ):
        """Get/Set string variable"""
        assert self.libvlc.get( "open" ) == ''
        self.libvlc.set( "open", "foo" ) 
        assert self.libvlc.get( "open" ) == "foo"
           
