import logging
import urllib2

def getLogger( module ):
    logger = logging.getLogger( module )
    stl = logging.StreamHandler( )
    formatter = logging.Formatter( '%(asctime)s %(name)s %(levelname)s %(message)s' )
    stl.setFormatter( formatter )
    logger.addHandler( stl )
    logger.setLevel( logging.DEBUG )
    return logger

def downloadFile( file, source, target, l ):
    l.info( "Opening %s/%s" % (source,file ) )
    try:
        remote = urllib2.urlopen(  "%s/%s" % (source,file ) )
        l.debug( "Open success, downloading"  )
        local = open( target + "/" + file, "w+" )
        local.write( remote.read() )
    except:
        return 1
    return 0
                                                                                    
