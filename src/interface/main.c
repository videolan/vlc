/*****************************************************************************
 * main.c: main vlc source
 * (c)1998 VideoLAN
 *****************************************************************************
 * Includes the main() function for vlc. Parses command line, start interface
 * and spawn threads.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/soundcard.h>                                 /* audio_output.h */

#include "config.h"
#include "common.h"
#include "mtime.h"
#include "vlc_thread.h"
#include "input_vlan.h"
#include "intf_msg.h"
#include "interface.h"
#include "audio_output.h"
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

#define OPT_NOVLANS             170
#define OPT_SERVER              171
#define OPT_PORT                172

/* Usage fashion */
#define SHORT_HELP                0
#define LONG_HELP                 1

/* Long options */
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

    /* Input options */
    {   "novlans",          0,          0,      OPT_NOVLANS },
    {   "server",           1,          0,      OPT_SERVER },
    {   "port",             1,          0,      OPT_PORT },

    {   0,                  0,          0,      0 }
};

/* Short options */
static const char *psz_shortopts = "hHvg";

/*****************************************************************************
 * Global variable program_data - this is the one and only, see main.h
 *****************************************************************************/
main_t *p_main;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void SetDefaultConfiguration ( void );
static int  GetConfiguration        ( int i_argc, char *ppsz_argv[], char *ppsz_env[] );
static void Usage                   ( int i_fashion );
static void Version                 ( void );

static void InitSignalHandler       ( void );
static void SignalHandler           ( int i_signal );
static int  TestMMX                 ( void );

/*****************************************************************************
 * main: parse command line, start interface and spawn threads
 *****************************************************************************
 * Steps during program execution are:
 *      -configuration parsing and messages interface initialization
 *      -openning of audio output device and some global modules
 *      -execution of interface, which exit on error or on user request
 *      -closing of audio output device and some global modules
 * On error, the spawned threads are cancelled, and the open devices closed.
 *****************************************************************************/
int main( int i_argc, char *ppsz_argv[], char *ppsz_env[] )
{
    main_t  main_data;                      /* root of all data - see main.h */
    p_main = &main_data;                       /* set up the global variable */

    /*
     * Read configuration, initialize messages interface and set up program
     */
#ifdef HAVE_MMX
    if( !TestMMX() )
    {
        fprintf( stderr, "Sorry, this program needs an MMX processor. Please run the non-MMX version.\n" );
        return( 1 );
    }
#endif
    p_main->p_msg = intf_MsgCreate();
    if( !p_main->p_msg )                         /* start messages interface */
    {
        fprintf(stderr, "critical error: can't initialize messages interface (%s)\n",
                strerror(errno));
        return(errno);
    }
    if( GetConfiguration( i_argc, ppsz_argv, ppsz_env ) )  /* parse cmd line */
    {
        intf_MsgDestroy();
        return(errno);
    }
    intf_MsgImm( COPYRIGHT_MESSAGE "\n" );          /* print welcome message */

    /*
     * Initialize shared resources and libraries
     */
    if( main_data.b_vlans && input_VlanCreate() )
    {
        /* On error during vlans initialization, switch of vlans */
        intf_Msg("Virtual LANs initialization failed : vlans management is deactivated\n");
        main_data.b_vlans = 0;
    }

    /*
     * Open audio device and start aout thread
     */
    if( main_data.b_audio )
    {
        main_data.p_aout = aout_CreateThread( NULL );
        if( main_data.p_aout == NULL )
        {
            /* On error during audio initialization, switch of audio */
            intf_Msg("Audio initialization failed : audio is deactivated\n");
            main_data.b_audio = 0;
        }
    }

    /*
     * Run interface
     */
    main_data.p_intf = intf_Create();
    if( main_data.p_intf != NULL )
    {
        InitSignalHandler();               /* prepare signals for interception */
        intf_Run( main_data.p_intf );
        intf_Destroy( main_data.p_intf );
    }

    /*
     * Close audio device
     */
    if( main_data.b_audio )
    {
        aout_DestroyThread( main_data.p_aout, NULL );
    }

    /*
     * Free shared resources and libraries
     */
    if( main_data.b_vlans )
    {
        input_VlanDestroy();
    }

    /*
     * Terminate messages interface and program
     */
    intf_Msg( "Program terminated.\n" );
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
        intf_ErrMsg("error: %s\n", strerror(ENOMEM));
    }
    else
    {
        sprintf( psz_env, "%s=%s", psz_name, psz_value );
        if( putenv( psz_env ) )
        {
            intf_ErrMsg("error: %s\n", strerror(errno));
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

    sprintf(psz_value, "%d", i_value );
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
     * All features are activated by default
     */
    p_main->b_audio  = 1;
    p_main->b_video  = 1;
    p_main->b_vlans  = 1;
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

    /* Set default configuration and copy arguments */
    p_main->i_argc    = i_argc;
    p_main->ppsz_argv = ppsz_argv;
    p_main->ppsz_env  = ppsz_env;
    SetDefaultConfiguration();

    /* Parse command line options */
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

        /* Input options */
        case OPT_NOVLANS:                                       /* --novlans */
            p_main->b_vlans = 0;
            break;
        case OPT_SERVER:                                         /* --server */
            main_PutPszVariable( INPUT_SERVER_VAR, optarg );
            break;
        case OPT_PORT:                                             /* --port */
            main_PutPszVariable( INPUT_PORT_VAR, optarg );
            break;

        /* Internal error: unknown option */
        case '?':
        default:
            intf_ErrMsg("intf error: unknown option '%s'\n", ppsz_argv[optind - 1]);
            return( EINVAL );
            break;
        }
    }

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
    intf_Msg(COPYRIGHT_MESSAGE "\n");

    /* Usage */
    intf_Msg("usage: vlc [options...] [parameters]\n" );

    /* Options */
    intf_Msg("Options:\n" \
             "  -h, --help, -H, --longhelp        \tprint short/long usage\n" \
             "  -v, --version                     \tprint version information\n" \
             "  --noaudio, --novideo              \tdisable audio/video\n" \
             "  --aout {" AUDIO_OPTIONS "}            \taudio output method\n" \
             "  --stereo, --mono                  \tstereo/mono audio\n" \
             "  --vout {" VIDEO_OPTIONS "}            \tvideo output method\n" \
             "  --display <display>               \tdisplay string\n" \
             "  --width <w>, --height <h>         \tdisplay dimensions\n" \
             "  -g, --grayscale, --color          \tgrayscale/color video\n" \
             "  --novlans                         \tdisable vlans\n" \
             "  --server <host>, --port <port>    \tvideo server adress\n" \
             );

    if( i_fashion == SHORT_HELP )
        return;

    /* Interface parameters */
    intf_Msg("Interface parameters:\n" \
             "  " INTF_INIT_SCRIPT_VAR "=<filename>             \tinitialization script\n" \
             "  " INTF_CHANNELS_VAR "=<filename>            \tchannels list\n"\
             );

    /* Audio parameters */
    intf_Msg("Audio parameters:\n" \
             "  " AOUT_METHOD_VAR "=<method name>        \taudio method (" AUDIO_OPTIONS ")\n" \
             "  " AOUT_DSP_VAR "=<filename>              \tdsp device path\n" \
             "  " AOUT_STEREO_VAR "={1|0}                \tstereo or mono output\n" \
             "  " AOUT_RATE_VAR "=<rate>             \toutput rate\n" \
             );

    /* Video parameters */
    intf_Msg("Video parameters:\n" \
             "  " VOUT_METHOD_VAR "=<method name>        \tdisplay method (" VIDEO_OPTIONS ")\n" \
             "  " VOUT_DISPLAY_VAR "=<display name>      \tdisplay used\n" \
             "  " VOUT_WIDTH_VAR "=<width>               \tdisplay width\n" \
             "  " VOUT_HEIGHT_VAR "=<height>             \tdislay height\n" \
             "  " VOUT_FB_DEV_VAR "=<filename>           \tframebuffer device path\n" \
             "  " VOUT_GRAYSCALE_VAR "={1|0}             \tgrayscale or color output\n" \
             );

    /* Input parameters */
    intf_Msg("Input parameters:\n" \
             "  " INPUT_SERVER_VAR "=<hostname>          \tvideo server\n" \
             "  " INPUT_PORT_VAR "=<port>            \tvideo server port\n" \
             "  " INPUT_IFACE_VAR "=<interface>          \tnetwork interface\n" \
             "  " INPUT_VLAN_SERVER_VAR "=<hostname>     \tvlan server\n" \
             "  " INPUT_VLAN_PORT_VAR "=<port>           \tvlan server port\n"\
             );
}

/*****************************************************************************
 * Version: print complete program version
 *****************************************************************************
 * Print complete program version and build number.
 *****************************************************************************/
static void Version( void )
{
    intf_Msg(VERSION_MESSAGE "\n\n");
    intf_Msg("This is free software; see the documentation or contact <videolan@via.ecp.fr>\n" \
             "for use and copying conditions.\n" \
             "\n" \
             "This software is protected by the international copyright laws, and is\n" \
             "provided without any warranty, including the implied warranties of\n" \
             "merchantibility and fitness for a particular purpose.\n" \
            );
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
    signal( SIGHUP,  SignalHandler );
    signal( SIGINT,  SignalHandler );
    signal( SIGQUIT, SignalHandler );
}

/*****************************************************************************
 * SignalHandler: system signal handler
 *****************************************************************************
 * This function is called when a signal is received by the program. It tries to
 * end the program in a clean way.
 *****************************************************************************/
static void SignalHandler( int i_signal )
{
    /* Once a signal has been trapped, the termination sequence will be armed and
     * following signals will be ignored to avoid sending messages to an interface
     * having been destroyed */
    signal( SIGHUP,  SIG_IGN );
    signal( SIGINT,  SIG_IGN );
    signal( SIGQUIT, SIG_IGN );

    /* Acknowledge the signal received */
    intf_ErrMsgImm("intf: signal %d received\n", i_signal );

    /* Try to terminate everything - this is done by requesting the end of the
     * interface thread */
    p_main->p_intf->b_die = 1;
}

#ifdef HAVE_MMX
/*****************************************************************************
 * TestMMX: tests if the processor has MMX support.
 *****************************************************************************
 * This function is called if HAVE_MMX is enabled, to check whether the
 * cpu really supports MMX.
 *****************************************************************************/
static int TestMMX( void )
{
    int i_reg, i_dummy = 0;

    /* test for a 386 cpu */
    asm volatile ( "pushfl
                    popl %%eax
                    movl %%eax, %%ecx
                    xorl $0x40000, %%eax
                    pushl %%eax
                    popfl
                    pushfl
                    popl %%eax
                    xorl %%ecx, %%eax
                    andl $0x40000, %%eax"
                 : "=a" ( i_reg ) );
    
    if( !i_reg )
        return( 0 );

    /* test for a 486 cpu */
    asm volatile ( "movl %%ecx, %%eax
                    xorl $0x200000, %%eax
                    pushl %%eax
                    popfl
                    pushfl
                    popl %%eax
                    xorl %%ecx, %%eax
                    pushl %%ecx 
                    popfl
                    andl $0x200000, %%eax"
                 : "=a" ( i_reg ) );
    
    if( !i_reg )
        return( 0 );

    /* the cpu supports the CPUID instruction - get its level */
    asm volatile ( "cpuid"
                 : "=a" ( i_reg ),
                   "=b" ( i_dummy ),
                   "=c" ( i_dummy ),
                   "=d" ( i_dummy )
                 : "a"  ( 0 ),       /* level 0 */
                   "b"  ( i_dummy ) ); /* buggy compiler shouldn't complain */

    /* this shouldn't happen on a normal cpu */
    if( !i_reg )
        return( 0 );

    /* test for the MMX flag */
    asm volatile ( "cpuid
                    andl $0x00800000, %%edx" /* X86_FEATURE_MMX */
                 : "=a" ( i_dummy ),
                   "=b" ( i_dummy ),
                   "=c" ( i_dummy ),
                   "=d" ( i_reg )
                 : "a"  ( 1 ),       /* level 1 */
                   "b"  ( i_dummy ) ); /* buggy compiler shouldn't complain */

    if( !i_reg )
        return( 0 );

    return( 1 );
}
#endif
