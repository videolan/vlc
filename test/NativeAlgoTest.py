import unittest

import native_libvlc_test

class NativeAlgoTestCase( unittest.TestCase ):
    def test_bsearch_direct( self ):
        """[Algo] Check Bsearch with simple types"""
    	native_libvlc_test.bsearch_direct_test()
    def test_bsearch_struct( self ):
        """[Algo] Check Bsearch with structs"""
    	native_libvlc_test.bsearch_member_test()
    def test_dict( self ):
        """[Algo] Check dictionnaries"""
    	native_libvlc_test.dict_test()
