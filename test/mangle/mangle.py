import VLCUtil
import shutil
import os
from random import randint
import glob

# Todo
# - Correctly handle errors
# - Launch VLC in a separate thread and detect hangs
# - Correct path handling

global conf
global l

def play_mangled( filename, logfile ):
    os.chdir( "../.." )
    vlc_pid = os.spawnvp( os.P_NOWAIT, "./vlc",
        [ "vlc", "-I", "logger", "--quiet",  "--stop-time", "1",
          filename , "vlc:quit", "--logfile", logfile ])
    ( exit_pid, exit_status ) = os.waitpid( vlc_pid, 0 )
    os.chdir( "test/mangle" )
    l.debug( "VLC exited with status %i" % exit_status )
    return exit_status

def mangle_file( filename, header_size, percentage , new_file):
    shutil.copyfile( filename, new_file )
    file = open ( new_file, "r+" )
    for i in range( header_size ):
        file.seek( i)
        if( randint(0, 100/percentage) == 0 ):
            file.write( "%i" % randint (0, 255 ));
    file.flush()
    file.close()

def process_file_once( file, header_size ):
    suffix = randint( 0, 65535 )
    new_file = conf["temp_folder"] + conf["prefix"] + "%i" % suffix
    log_file = conf["temp_folder"] + conf["log_prefix"] + "%i" % suffix
    
    mangle_file( file, header_size, conf["mangle_ratio"], new_file )
    status = play_mangled( new_file, log_file )
    if( status == 0 ):
        os.remove( new_file )
        os.remove( log_file )
    else:
        l.info( "Potential crash detected : %i, saving results" % suffix )
        shutil.move( new_file , conf["crashdir"] )
        shutil.move( log_file , conf["crashdir"] )

def process_file( file, source, header_size ):
     l.info( "Starting work on " + file )

     if( len( glob.glob( conf["inputdir"] + "/" + file ) ) == 0 ):
         l.warn( "%s does not exist in %s" % (file, conf["inputdir"] ) )
         if( VLCUtil.downloadFile( file, source, conf["inputdir"], l ) != 0 ):
             l.error( "Unable to download %s" % file )
             return 
     
     for i in range( conf["loops"] ):
         process_file_once( conf["inputdir"] + "/" + file, header_size )

l =  VLCUtil.getLogger( "Mangle" )

conf = {}
conf["inputdir"] = "input"
conf["crashdir"] = "crashers"
conf["temp_folder"] = "/tmp/"
conf["prefix"] = "mangle."
conf["log_prefix"] = "vlc-log."
conf["mangle_ratio"] = 4 # Change X% of bytes within header
conf["loops"]  = 20

l.debug( "Creating folders" )

try:
    os.makedirs( conf["crashdir"] )
    os.makedirs( conf["inputdir"] )
    os.makedirs( conf["temp_folder"] )
except:
    pass


##########
process_file( "bl.mp4", "ftp://streams.videolan.org/streams-videolan/reference/mp4", 3000 )
process_file( "Win98Crash.mov", "ftp://streams.videolan.org/streams-videolan/reference/mov", 3000 )
process_file( "x264.avi", "ftp://streams.videolan.org/streams-videolan/reference/avi", 3000 )
process_file( "batidadomontoya.wmv", "ftp://streams.videolan.org/streams-videolan/reference/asf", 3000 )
process_file( "tarzan.ogm", "ftp://streams.videolan.org/streams-videolan/reference/ogm", 3000 )

