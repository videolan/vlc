import unittest

import native_libvlc_test

class NativeI18NTestCase( unittest.TestCase ):
    def testi18n_atof( self ):
        """[I18N] Test i18n_atof"""
    	native_libvlc_test.i18n_atof_test()
