/*****************************************************************************
 * main.c: main vlc source
 * Includes the main() function for vlc. Parses command line, start interface
 * and spawn threads.
 *****************************************************************************
 * Copyright (C) 1998, 1999, 2000 VideoLAN
 *
 * Authors:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <signal.h>                               /* SIGHUP, SIGINT, SIGKILL */
#include <stdio.h>                                              /* sprintf() */

#ifdef HAVE_GETOPT_H
#include <getopt.h>                                              /* getopt() */
#endif

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                  /* getenv(), strtol(),  */
#include <string.h>                                            /* strerror() */

#include "config.h"
#include "common.h"
#include "debug.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"                                              /* TestCPU() */
#include "plugins.h"
#include "modules.h"
#include "playlist.h"
#include "stream_control.h"
#include "input_ext-intf.h"

#include "intf_msg.h"
#include "interface.h"

#include "audio_output.h"

#ifdef SYS_BEOS
#include "beos_specific.h"
#endif

#include "main.h"

/*****************************************************************************
 * Command line options constants. If something is changed here, be sure that
 * GetConfiguration and Usage are also changed.
 *****************************************************************************/

/* Long options return values - note that values corresponding to short options
 * chars, and in general any regular char, should be avoided */
#define OPT_NOAUDIO             150
#define OPT_AOUT                151
#define OPT_STEREO              152
#define OPT_MONO                153

#define OPT_NOVIDEO             160
#define OPT_VOUT                161
#define OPT_DISPLAY             162
#define OPT_WIDTH               163
#define OPT_HEIGHT              164
#define OPT_COLOR               165
#define OPT_IDCT                166
#define OPT_YUV                 167

#define OPT_VLANS               170
#define OPT_SERVER              171
#define OPT_PORT                172
#define OPT_BROADCAST           173
#define OPT_DVD                 174

#define OPT_SYNCHRO             180

#define OPT_WARNING             190

/* Usage fashion */
#define USAGE                     0
#define SHORT_HELP                1
#define LONG_HELP                 2

/* Long options */
#ifdef HAVE_GETOPT_H
static const struct option longopts[] =
{
    /*  name,               has_arg,    flag,   val */

    /* General/common options */
    {   "help",             0,          0,      'h' },
    {   "longhelp",         0,          0,      'H' },
    {   "version",          0,          0,      'v' },

    /* Audio options */
    {   "noaudio",          0,          0,      OPT_NOAUDIO },
    {   "aout",             1,          0,      OPT_AOUT },
    {   "stereo",           0,          0,      OPT_STEREO },
    {   "mono",             0,          0,      OPT_MONO },

    /* Video options */
    {   "novideo",          0,          0,      OPT_NOVIDEO },
    {   "vout",             1,          0,      OPT_VOUT },
    {   "display",          1,          0,      OPT_DISPLAY },
    {   "width",            1,          0,      OPT_WIDTH },
    {   "height",           1,          0,      OPT_HEIGHT },
    {   "grayscale",        0,          0,      'g' },
    {   "color",            0,          0,      OPT_COLOR },
    {   "idct",             1,          0,      OPT_IDCT },
    {   "yuv",              1,          0,      OPT_YUV },

    /* DVD options */
    {   "dvdaudio",         1,          0,      'a' },
    {   "dvdchannel",       1,          0,      'c' },
    {   "dvdsubtitle",      1,          0,      's' },
    
    /* Input options */
    {   "vlans",            0,          0,      OPT_VLANS },
    {   "server",           1,          0,      OPT_SERVER },
    {   "port",             1,          0,      OPT_PORT },
    {   "broadcast",        0,          0,      OPT_BROADCAST },
    {   "dvd",              0,          0,      OPT_DVD },

    /* Synchro options */
    {   "synchro",          1,          0,      OPT_SYNCHRO },

    /* Interface messages */
    {   "warning",          1,          0,      OPT_WARNING },
    {   0,                  0,          0,      0 }
};

/* Short options */
static const char *psz_shortopts = "hHvga:s:c:";
#endif


/*****************************************************************************
 * Global variable program_data - this is the one and only, see main.h
 *****************************************************************************/
main_t *p_main;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void SetDefaultConfiguration ( void );
static int  GetConfiguration        ( int i_argc, char *ppsz_argv[],
                                      char *ppsz_env[] );
static void Usage                   ( int i_fashion );
static void Version                 ( void );

static void InitSignalHandler       ( void );
static void SimpleSignalHandler     ( int i_signal );
static void FatalSignalHandler      ( int i_signal );

/*****************************************************************************
 * main: parse command line, start interface and spawn threads
 *****************************************************************************
 * Steps during program execution are:
 *      -configuration parsing and messages interface initialization
 *      -opening of audio output device and some global modules
 *      -execution of interface, which exit on error or on user request
 *      -closing of audio output device and some global modules
 * On error, the spawned threads are canceled, and the open devices closed.
 *****************************************************************************/
int main( int i_argc, char *ppsz_argv[], char *ppsz_env[] )
{
    main_t  main_data;                      /* root of all data - see main.h */

    p_main = &main_data;                       /* set up the global variable */

    /*
     * System specific initialization code
     */
#ifdef SYS_BEOS
    beos_Create();
#endif

    /*
     * Test if our code is likely to run on this CPU 
     */
#ifdef HAVE_MMX
    if( !( TestCPU() & CPU_CAPABILITY_MMX ) )
    {
        fprintf( stderr, "Sorry, this program needs an MMX processor. "
                         "Please run the non-MMX version.\n" );
        return( 1 );
    }
#endif

    /*
     * Initialize messages interface
     */
    p_main->p_msg = intf_MsgCreate();
    if( !p_main->p_msg )                         /* start messages interface */
    {
        fprintf( stderr, "error: can't initialize messages interface (%s)\n",
                 strerror(errno) );
        return( errno );
    }

    /*
     * Read configuration
     */
    if( GetConfiguration( i_argc, ppsz_argv, ppsz_env ) )  /* parse cmd line */
    {
        intf_MsgDestroy();
        return( errno );
    }

    /*
     * Initialize playlist and get commandline files
     */
    p_main->p_playlist = playlist_Create( );
    if( !p_main->p_playlist )
    {
        intf_ErrMsg( "playlist error: playlist initialization failed" );
        intf_MsgDestroy();
        return( errno );
    }
    playlist_Init( p_main->p_playlist, optind );

    /*
     * Initialize plugin bank
     */
    p_main->p_bank = bank_Create( );
    if( !p_main->p_bank )
    {
        intf_ErrMsg( "plugin error: plugin bank initialization failed" );
        playlist_Destroy( p_main->p_playlist );
        intf_MsgDestroy();
        return( errno );
    }
    bank_Init( p_main->p_bank );

    /*
     * Initialize module bank
     */
    p_main->p_module_bank = module_CreateBank( );
    if( !p_main->p_module_bank )
    {
        intf_ErrMsg( "module error: module bank initialization failed" );
        bank_Destroy( p_main->p_bank );
        playlist_Destroy( p_main->p_playlist );
        intf_MsgDestroy();
        return( errno );
    }
    module_InitBank( p_main->p_module_bank );

    /*
     * Initialize shared resources and libraries
     */
    /* FIXME: no VLANs */
#if 0
    if( p_main->b_vlans && input_VlanCreate() )
    {
        /* On error during vlans initialization, switch off vlans */
        intf_Msg( "Virtual LANs initialization failed : "
                  "vlans management is deactivated" );
        p_main->b_vlans = 0;
    }
#endif

    /*
     * Run interface
     */
    p_main->p_intf = intf_Create();

    if( p_main->p_intf != NULL )
    {
        /*
         * Set signal handling policy for all threads
         */
        InitSignalHandler();

        /*
         * Open audio device and start aout thread
         */
        if( p_main->b_audio )
        {
            p_main->p_aout = aout_CreateThread( NULL );
            if( p_main->p_aout == NULL )
            {
                /* On error during audio initialization, switch off audio */
                intf_ErrMsg( "aout error: audio initialization failed,"
                             " audio is deactivated" );
                p_main->b_audio = 0;
            }
        }

        /*
         * This is the main loop
         */
        intf_Run( p_main->p_intf );

        intf_Destroy( p_main->p_intf );

        /*
         * Close audio device
         */
        if( p_main->b_audio )
        {
            aout_DestroyThread( p_main->p_aout, NULL );
        }
    }

    /*
     * Free shared resources and libraries
     */
    /* FIXME */
#if 0
    if( p_main->b_vlans )
    {
        input_VlanDestroy();
    }
#endif

    /*
     * Free module bank
     */
    module_DestroyBank( p_main->p_module_bank );

    /*
     * Free plugin bank
     */
    bank_Destroy( p_main->p_bank );

    /*
     * Free playlist
     */
    playlist_Destroy( p_main->p_playlist );

#ifdef SYS_BEOS
    /*
     * System specific cleaning code
     */
    beos_Destroy();
#endif

    /*
     * Terminate messages interface and program
     */
    intf_Msg( "intf: program terminated." );
    intf_MsgDestroy();

    return( 0 );
}

/*****************************************************************************
 * main_GetIntVariable: get the int value of an environment variable
 *****************************************************************************
 * This function is used to read some default parameters in modules.
 *****************************************************************************/
int main_GetIntVariable( char *psz_name, int i_default )
{
    char *      psz_env;                                /* environment value */
    char *      psz_end;                             /* end of parsing index */
    long int    i_value;                                            /* value */

    psz_env = getenv( psz_name );
    if( psz_env )
    {
        i_value = strtol( psz_env, &psz_end, 0 );
        if( (*psz_env != '\0') && (*psz_end == '\0') )
        {
            return( i_value );
        }
    }
    return( i_default );
}

/*****************************************************************************
 * main_GetPszVariable: get the string value of an environment variable
 *****************************************************************************
 * This function is used to read some default parameters in modules.
 *****************************************************************************/
char * main_GetPszVariable( char *psz_name, char *psz_default )
{
    char *psz_env;

    psz_env = getenv( psz_name );
    if( psz_env )
    {
        return( psz_env );
    }
    return( psz_default );
}

/*****************************************************************************
 * main_PutPszVariable: set the string value of an environment variable
 *****************************************************************************
 * This function is used to set some default parameters in modules. The use of
 * this function will cause some memory leak: since some systems use the pointer
 * passed to putenv to store the environment string, it can't be freed.
 *****************************************************************************/
void main_PutPszVariable( char *psz_name, char *psz_value )
{
    char *psz_env;

    psz_env = malloc( strlen(psz_name) + strlen(psz_value) + 2 );
    if( psz_env == NULL )
    {
        intf_ErrMsg( "intf error: cannot create psz_env (%s)",
                     strerror(ENOMEM) );
    }
    else
    {
        sprintf( psz_env, "%s=%s", psz_name, psz_value );
        if( putenv( psz_env ) )
        {
            intf_ErrMsg( "intf error: cannot putenv (%s)", strerror(errno) );
        }
    }
}

/*****************************************************************************
 * main_PutIntVariable: set the integer value of an environment variable
 *****************************************************************************
 * This function is used to set some default parameters in modules. The use of
 * this function will cause some memory leak: since some systems use the pointer
 * passed to putenv to store the environment string, it can't be freed.
 *****************************************************************************/
void main_PutIntVariable( char *psz_name, int i_value )
{
    char psz_value[ 256 ];                               /* buffer for value */

    sprintf( psz_value, "%d", i_value );
    main_PutPszVariable( psz_name, psz_value );
}

/* following functions are local */

/*****************************************************************************
 * SetDefaultConfiguration: set default options
 *****************************************************************************
 * This function is called by GetConfiguration before command line is parsed.
 * It sets all the default values required later by the program. At this stage,
 * most structure are not yet allocated, so initialization must be done using
 * environment.
 *****************************************************************************/
static void SetDefaultConfiguration( void )
{
    /*
     * All features are activated by default except vlans
     */
    p_main->b_audio  = 1;
    p_main->b_video  = 1;
    p_main->b_vlans  = 0;
    p_main->b_dvd    = 0;
}

/*****************************************************************************
 * GetConfiguration: parse command line
 *****************************************************************************
 * Parse command line and configuration file for configuration. If the inline
 * help is requested, the function Usage() is called and the function returns
 * -1 (causing main() to exit). The messages interface is initialized at this
 * stage, but most structures are not allocated, so only environment should
 * be used.
 *****************************************************************************/
static int GetConfiguration( int i_argc, char *ppsz_argv[], char *ppsz_env[] )
{
    int c, i_opt;
    char * p_pointer;

    /* Set default configuration and copy arguments */
    p_main->i_argc    = i_argc;
    p_main->ppsz_argv = ppsz_argv;
    p_main->ppsz_env  = ppsz_env;
    SetDefaultConfiguration();

    intf_MsgImm( COPYRIGHT_MESSAGE );

    /* Get the executable name (similar to the basename command) */
    p_main->psz_arg0 = p_pointer = ppsz_argv[ 0 ];
    while( *p_pointer )
    {
        if( *p_pointer == '/' )
        {
            p_main->psz_arg0 = ++p_pointer;
        }
        else
        {
            ++p_pointer;
        }
    }

    /* Parse command line options */
#ifdef HAVE_GETOPT_H
    opterr = 0;
    while( ( c = getopt_long( i_argc, ppsz_argv, psz_shortopts, longopts, 0 ) ) != EOF )
    {
        switch( c )
        {
        /* General/common options */
        case 'h':                                              /* -h, --help */
            Usage( SHORT_HELP );
            return( -1 );
            break;
        case 'H':                                          /* -H, --longhelp */
            Usage( LONG_HELP );
            return( -1 );
            break;
        case 'v':                                           /* -v, --version */
            Version();
            return( -1 );
            break;

        /* Audio options */
        case OPT_NOAUDIO:                                       /* --noaudio */
            p_main->b_audio = 0;
            break;
        case OPT_AOUT:                                             /* --aout */
            main_PutPszVariable( AOUT_METHOD_VAR, optarg );
            break;
        case OPT_STEREO:                                         /* --stereo */
            main_PutIntVariable( AOUT_STEREO_VAR, 1 );
            break;
        case OPT_MONO:                                             /* --mono */
            main_PutIntVariable( AOUT_STEREO_VAR, 0 );
            break;

        /* Video options */
        case OPT_NOVIDEO:                                       /* --novideo */
            p_main->b_video = 0;
            break;
        case OPT_VOUT:                                             /* --vout */
            main_PutPszVariable( VOUT_METHOD_VAR, optarg );
            break;
        case OPT_DISPLAY:                                       /* --display */
            main_PutPszVariable( VOUT_DISPLAY_VAR, optarg );
            break;
        case OPT_WIDTH:                                           /* --width */
            main_PutPszVariable( VOUT_WIDTH_VAR, optarg );
            break;
        case OPT_HEIGHT:                                         /* --height */
            main_PutPszVariable( VOUT_HEIGHT_VAR, optarg );
            break;

        case 'g':                                         /* -g, --grayscale */
            main_PutIntVariable( VOUT_GRAYSCALE_VAR, 1 );
            break;
        case OPT_COLOR:                                           /* --color */
            main_PutIntVariable( VOUT_GRAYSCALE_VAR, 0 );
            break;
	case OPT_IDCT:                                             /* --idct */
            main_PutPszVariable( IDCT_METHOD_VAR, optarg );
            break;
        case OPT_YUV:                                               /* --yuv */
            main_PutPszVariable( YUV_METHOD_VAR, optarg );
            break;

        /* DVD options */
        case 'a':
            if ( ! strcmp(optarg, "ac3") )
                main_PutIntVariable( INPUT_DVD_AUDIO_VAR, REQUESTED_AC3 );
            else if ( ! strcmp(optarg, "lpcm") )
                main_PutIntVariable( INPUT_DVD_AUDIO_VAR, REQUESTED_LPCM );
            else if ( ! strcmp(optarg, "off") )
                main_PutIntVariable( INPUT_DVD_AUDIO_VAR, REQUESTED_NOAUDIO );
            else
                main_PutIntVariable( INPUT_DVD_AUDIO_VAR, REQUESTED_MPEG );
            break;
        case 'c':
            main_PutIntVariable( INPUT_DVD_CHANNEL_VAR, atoi(optarg) );
            break;
        case 's':
            main_PutIntVariable( INPUT_DVD_SUBTITLE_VAR, atoi(optarg) );
            break;

        /* Input options */
        case OPT_VLANS:                                           /* --vlans */
            p_main->b_vlans = 1;
            break;
        case OPT_SERVER:                                         /* --server */
            main_PutPszVariable( INPUT_SERVER_VAR, optarg );
            break;
        case OPT_PORT:                                             /* --port */
            main_PutPszVariable( INPUT_PORT_VAR, optarg );
            break;
        case OPT_BROADCAST:                                   /* --broadcast */
            main_PutIntVariable( INPUT_BROADCAST_VAR, 1 );
            break;
        case OPT_DVD:                                               /* --dvd */
            p_main->b_dvd = 1;
            break;

        /* Synchro options */
        case OPT_SYNCHRO:                                      
            main_PutPszVariable( VPAR_SYNCHRO_VAR, optarg );
            break;

        /* Interface warning messages level */
        case OPT_WARNING:                                       /* --warning */
            main_PutIntVariable( INTF_WARNING_VAR, atoi(optarg) );
            break;
            
        /* Internal error: unknown option */
        case '?':
        default:
            intf_ErrMsg( "intf error: unknown option `%s'", ppsz_argv[optind - 1] );
            Usage( USAGE );
            return( EINVAL );
            break;
        }
    }
#endif

    /* Parse command line parameters - no check is made for these options */
    for( i_opt = optind; i_opt < i_argc; i_opt++ )
    {
        putenv( ppsz_argv[ i_opt ] );
    }
    return( 0 );
}

/*****************************************************************************
 * Usage: print program usage
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
static void Usage( int i_fashion )
{
    /* Usage */
    intf_Msg( "Usage: %s [options] [parameters] [file]...",
              p_main->psz_arg0 );

    if( i_fashion == USAGE )
    {
        intf_Msg( "Try `%s --help' for more information.",
                  p_main->psz_arg0 );
        return;
    }

    /* Options */
    intf_Msg( "\nOptions:"
              "\n      --noaudio                  \tdisable audio"
              "\n      --aout <module>            \taudio output method"
              "\n      --stereo, --mono           \tstereo/mono audio"
              "\n"
              "\n      --novideo                  \tdisable video"
              "\n      --vout <module>            \tvideo output method"
              "\n      --display <display>        \tdisplay string"
              "\n      --width <w>, --height <h>  \tdisplay dimensions"
              "\n  -g, --grayscale                \tgrayscale output"
              "\n      --color                    \tcolor output"
              "\n      --idct <module>            \tIDCT method"
              "\n      --yuv <module>             \tYUV method"
              "\n      --synchro <type>           \tforce synchro algorithm"
              "\n"
              "\n      --dvd                      \tDVD mode"
              "\n  -a, --dvdaudio <type>          \tchoose DVD audio type"
              "\n  -c, --dvdchannel <channel>     \tchoose DVD audio channel"
              "\n  -s, --dvdsubtitle <channel>    \tchoose DVD subtitle channel"
              "\n"
              "\n      --vlans                    \tenable vlans"
              "\n      --server <host>            \tvideo server address"
              "\n      --port <port>              \tvideo server port"
              "\n      --broadcast                \tlisten to a broadcast"
              "\n"
              "\n      --warning <level>          \tdisplay warning messages"
              "\n"
              "\n  -h, --help                     \tprint help and exit"
              "\n  -H, --longhelp                 \tprint long help and exit"
              "\n  -v, --version                  \toutput version information and exit" );

    if( i_fashion == SHORT_HELP )
        return;

    /* Interface parameters */
    intf_Msg( "\nInterface parameters:\n"
              "\n  " INTF_INIT_SCRIPT_VAR "=<filename>               \tinitialization script"
              "\n  " INTF_CHANNELS_VAR "=<filename>            \tchannels list"
              "\n  " INTF_WARNING_VAR "=<level>                \twarning level" );

    /* Audio parameters */
    intf_Msg( "\nAudio parameters:"
              "\n  " AOUT_METHOD_VAR "=<method name>        \taudio method"
              "\n  " AOUT_DSP_VAR "=<filename>              \tdsp device path"
              "\n  " AOUT_STEREO_VAR "={1|0}                \tstereo or mono output"
              "\n  " AOUT_RATE_VAR "=<rate>             \toutput rate" );

    /* Video parameters */
    intf_Msg( "\nVideo parameters:"
              "\n  " VOUT_METHOD_VAR "=<method name>        \tdisplay method"
              "\n  " VOUT_DISPLAY_VAR "=<display name>      \tdisplay used"
              "\n  " VOUT_WIDTH_VAR "=<width>               \tdisplay width"
              "\n  " VOUT_HEIGHT_VAR "=<height>             \tdislay height"
              "\n  " VOUT_FB_DEV_VAR "=<filename>           \tframebuffer device path"
              "\n  " VOUT_GRAYSCALE_VAR "={1|0}             \tgrayscale or color output"
              "\n  " IDCT_METHOD_VAR "=<method name>        \tIDCT method"
              "\n  " YUV_METHOD_VAR "=<method name>         \tYUV method"
              "\n  " VPAR_SYNCHRO_VAR "={I|I+|IP|IP+|IPB}   \tsynchro algorithm" );

    /* DVD parameters */
    intf_Msg( "\nDVD parameters:"
              "\n  " INPUT_DVD_DEVICE_VAR "=<device>           \tDVD device"
              "\n  " INPUT_DVD_AUDIO_VAR "={ac3|lpcm|mpeg|off} \taudio type"
              "\n  " INPUT_DVD_CHANNEL_VAR "=[0-15]            \taudio channel"
              "\n  " INPUT_DVD_SUBTITLE_VAR "=[0-31]           \tsubtitle channel" );

    /* Input parameters */
    intf_Msg( "\nInput parameters:\n"
              "\n  " INPUT_SERVER_VAR "=<hostname>          \tvideo server"
              "\n  " INPUT_PORT_VAR "=<port>            \tvideo server port"
              "\n  " INPUT_IFACE_VAR "=<interface>          \tnetwork interface"
              "\n  " INPUT_BROADCAST_VAR "={1|0}            \tbroadcast mode"
              "\n  " INPUT_VLAN_SERVER_VAR "=<hostname>     \tvlan server"
              "\n  " INPUT_VLAN_PORT_VAR "=<port>           \tvlan server port"
 );

}

/*****************************************************************************
 * Version: print complete program version
 *****************************************************************************
 * Print complete program version and build number.
 *****************************************************************************/
static void Version( void )
{
    intf_Msg( VERSION_MESSAGE
              "This program comes with NO WARRANTY, to the extent permitted by law.\n"
              "You may redistribute it under the terms of the GNU General Public License;\n"
              "see the file named COPYING for details.\n"
              "Written by the VideoLAN team at Ecole Centrale, Paris." );

}

/*****************************************************************************
 * InitSignalHandler: system signal handler initialization
 *****************************************************************************
 * Set the signal handlers. SIGTERM is not intercepted, because we need at
 * at least a method to kill the program when all other methods failed, and
 * when we don't want to use SIGKILL.
 *****************************************************************************/
static void InitSignalHandler( void )
{
    /* Termination signals */
    signal( SIGHUP,  FatalSignalHandler );
    signal( SIGINT,  FatalSignalHandler );
    signal( SIGQUIT, FatalSignalHandler );

    /* Other signals */
    signal( SIGALRM, SimpleSignalHandler );
    signal( SIGPIPE, SimpleSignalHandler );
}


/*****************************************************************************
 * SimpleSignalHandler: system signal handler
 *****************************************************************************
 * This function is called when a non fatal signal is received by the program.
 *****************************************************************************/
static void SimpleSignalHandler( int i_signal )
{
    /* Acknowledge the signal received */
    intf_WarnMsg(0, "intf: ignoring signal %d", i_signal );
}


/*****************************************************************************
 * FatalSignalHandler: system signal handler
 *****************************************************************************
 * This function is called when a fatal signal is received by the program.
 * It tries to end the program in a clean way.
 *****************************************************************************/
static void FatalSignalHandler( int i_signal )
{
    /* Once a signal has been trapped, the termination sequence will be armed and
     * following signals will be ignored to avoid sending messages to an interface
     * having been destroyed */
    signal( SIGHUP,  SIG_IGN );
    signal( SIGINT,  SIG_IGN );
    signal( SIGQUIT, SIG_IGN );

    /* Acknowledge the signal received */
    intf_ErrMsgImm("intf error: signal %d received, exiting", i_signal );

    /* Try to terminate everything - this is done by requesting the end of the
     * interface thread */
    p_main->p_intf->b_die = 1;
}

