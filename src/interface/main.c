/*****************************************************************************
 * main.c: main vlc source
 * Includes the main() function for vlc. Parses command line, start interface
 * and spawn threads.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: main.c,v 1.158 2002/03/04 01:53:56 stef Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@netcourrier.com>
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
#include <signal.h>                               /* SIGHUP, SIGINT, SIGKILL */
#include <stdio.h>                                              /* sprintf() */
#include <setjmp.h>                                       /* longjmp, setjmp */

#include <videolan/vlc.h>

#ifdef HAVE_GETOPT_LONG
#   ifdef HAVE_GETOPT_H
#       include <getopt.h>                                       /* getopt() */
#   endif
#else
#   include "GNUgetopt/getopt.h"
#endif

#ifdef SYS_DARWIN
#   include <mach/mach.h>                               /* Altivec detection */
#   include <mach/mach_error.h>       /* some day the header files||compiler *
                                                       will define it for us */
#   include <mach/bootstrap.h>
#endif

#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( _MSC_VER ) && defined( _WIN32 )
#   include <io.h>
#endif

#ifdef HAVE_LOCALE_H
#    include <locale.h>
#endif

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                  /* getenv(), strtol(),  */
#include <string.h>                                            /* strerror() */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <sys/stat.h>                                             /* S_IREAD */

#include "netutils.h"                                 /* network_ChannelJoin */

#include "stream_control.h"
#include "input_ext-intf.h"

#include "intf_playlist.h"
#include "interface.h"

#include "audio_output.h"

#include "video.h"
#include "video_output.h"

#include "debug.h"

/*****************************************************************************
 * Configuration options for the main program. Each plugin will also separatly
 * define its own configuration options.
 * Look into configuration.h if you need to know more about the following
 * macros.
 * 
 *****************************************************************************/
#define BUILTIN
#define MODULE_NAME main
#include "modules_inner.h"                        /* for configuration stuff */

/* Quick usage guide
MODULE_CONFIG_START
MODULE_CONFIG_STOP
ADD_CATEGORY_HINT( text, longtext )
ADD_SUBCATEGORY_HINT( text, longtext )
ADD_STRING( option_name, value, p_callback, text, longtext )
ADD_FILE( option_name, psz_value, p_callback, text, longtext )
ADD_PLUGIN( option_name, psz_value, i_capability, p_callback, text, longtext )
ADD_INTEGER( option_name, i_value, p_callback, text, longtext )
ADD_BOOL( option_name, p_callback, text, longtext )
*/

MODULE_CONFIG_START

/* Help options */
ADD_CATEGORY_HINT( "Help Options", NULL )
ADD_BOOL    ( "help", NULL,"print help and exit (or use -h)", NULL )
ADD_BOOL    ( "longhelp", NULL, "print long help version and exit (or use -H)",
              NULL )
ADD_BOOL    ( "list", NULL, "list available plugins (or use -l)", NULL )
ADD_STRING  ( "pluginhelp", NULL, NULL,"print help on a plugin and exit",NULL )
ADD_BOOL    ( "version", NULL, "output version information and exit", NULL )

/* Interface options */
ADD_CATEGORY_HINT( "Interface Options", NULL)
ADD_PLUGIN  ( INTF_METHOD_VAR, MODULE_CAPABILITY_INTF, NULL, NULL,
              "interface method", NULL )
ADD_INTEGER ( INTF_WARNING_VAR, 0, NULL, "warning level (or use -v)", NULL )
ADD_BOOL    ( INTF_STATS_VAR, NULL, "output statistics", NULL )
ADD_STRING  ( INTF_PATH_VAR, NULL, NULL, "interface default search path", NULL)

/* Audio Options */
ADD_CATEGORY_HINT( "Audio Options", NULL)
ADD_BOOL    ( AOUT_NOAUDIO_VAR, NULL, "disable audio", NULL )
ADD_PLUGIN  ( AOUT_METHOD_VAR, MODULE_CAPABILITY_AOUT, NULL, NULL,
              "audio output method", NULL )
ADD_BOOL    ( AOUT_MONO_VAR, NULL, "mono audio", NULL )
ADD_INTEGER ( AOUT_VOLUME_VAR, VOLUME_DEFAULT, NULL, "VLC output volume", NULL)
ADD_INTEGER ( AOUT_RATE_VAR, 44100, NULL, "VLC output frequency", NULL )
ADD_INTEGER ( AOUT_DESYNC_VAR, 0, NULL, "Compensate desynchronization of the "
                                        "audio (in ms)", NULL )

/* Video options */
ADD_CATEGORY_HINT( "Video Options", NULL )
ADD_BOOL    ( VOUT_NOVIDEO_VAR, NULL, "disable video", NULL )
ADD_PLUGIN  ( VOUT_METHOD_VAR, MODULE_CAPABILITY_VOUT, NULL, NULL,
              "video output method", NULL )
ADD_STRING  ( VOUT_DISPLAY_VAR, NULL, NULL, "display string", NULL )
ADD_INTEGER ( VOUT_WIDTH_VAR, 720, NULL, "display width", NULL )
ADD_INTEGER ( VOUT_HEIGHT_VAR, 576, NULL, "display height", NULL )
ADD_BOOL    ( VOUT_GRAYSCALE_VAR, NULL, "grayscale output", NULL )
ADD_BOOL    ( VOUT_FULLSCREEN_VAR, NULL, "fullscreen output", NULL )
ADD_BOOL    ( VOUT_NOOVERLAY_VAR, NULL, "disable accelerated display", NULL )
ADD_PLUGIN  ( VOUT_FILTER_VAR, MODULE_CAPABILITY_VOUT, NULL, NULL,
              "video filter module", NULL )
ADD_INTEGER ( VOUT_SPUMARGIN_VAR, -1, NULL, "force SPU position", NULL )

/* Input options */
ADD_CATEGORY_HINT( "Input Options", NULL )
ADD_STRING  ( INPUT_METHOD_VAR, NULL, NULL, "input method", NULL )
ADD_INTEGER ( INPUT_PORT_VAR, 1234, NULL, "server port", NULL )
ADD_BOOL    ( INPUT_NETWORK_CHANNEL_VAR, NULL, "enable network channel mode",
              NULL )
ADD_STRING  ( INPUT_CHANNEL_SERVER_VAR, "localhost", NULL,
              "channel server address", NULL )
ADD_INTEGER ( INPUT_CHANNEL_PORT_VAR, 6010, NULL, "channel server port", NULL )
ADD_STRING  ( INPUT_IFACE_VAR, "eth0", NULL, "network interface", NULL )

ADD_INTEGER ( INPUT_AUDIO_VAR, -1, NULL, "choose audio", NULL )
ADD_INTEGER ( INPUT_CHANNEL_VAR, -1, NULL, "choose channel", NULL )
ADD_INTEGER ( INPUT_SUBTITLE_VAR, -1, NULL, "choose subtitles", NULL )

ADD_STRING  ( INPUT_DVD_DEVICE_VAR, "/dev/dvd", NULL, "DVD device", NULL )
ADD_STRING  ( INPUT_VCD_DEVICE_VAR, "/dev/cdrom", NULL, "VCD device", NULL )

/* Decoder options */
ADD_CATEGORY_HINT( "Decoders Options", NULL )
ADD_PLUGIN  ( ADEC_MPEG_VAR, MODULE_CAPABILITY_DECODER, NULL, NULL,
              "choose MPEG audio decoder", NULL )
ADD_PLUGIN  ( ADEC_AC3_VAR, MODULE_CAPABILITY_DECODER, NULL, NULL,
              "choose AC3 audio decoder", NULL )
ADD_INTEGER ( VDEC_SMP_VAR, 0, NULL, "use additional processors", NULL )
ADD_STRING  ( VPAR_SYNCHRO_VAR, NULL, NULL, "force synchro algorithm "
                                            "{I|I+|IP|IP+|IPB}", NULL )

/* CPU options */
ADD_CATEGORY_HINT( "CPU Options Options", NULL )
ADD_BOOL    ( NOMMX_VAR, NULL, "disable CPU's MMX support", NULL )
ADD_BOOL    ( NO3DN_VAR, NULL, "disable CPU's 3D Now! support", NULL )
ADD_BOOL    ( NOMMXEXT_VAR, NULL, "disable CPU's MMX EXT support", NULL )
ADD_BOOL    ( NOSSE_VAR, NULL, "disable CPU's SSE support", NULL )
ADD_BOOL    ( NOALTIVEC_VAR, NULL, "disable CPU's AltiVec support", NULL )

/* Playlist options */
ADD_BOOL    ( PLAYLIST_STARTUP_VAR, NULL, "launch playlist on startup", NULL )
ADD_BOOL    ( PLAYLIST_ENQUEUE_VAR, NULL, "enqueue playlist as default", NULL )
ADD_BOOL    ( PLAYLIST_LOOP_VAR, NULL, "loop on playlist end", NULL )

/* Misc options */
ADD_CATEGORY_HINT( "Miscellaneous Options", NULL )
ADD_PLUGIN  ( MEMCPY_METHOD_VAR, MODULE_CAPABILITY_MEMCPY, NULL, NULL,
              "memory copy method", NULL )

MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( "Main program" )
    ADD_CAPABILITY( MAIN, 100/*whatever*/ )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP
/*****************************************************************************
 * End configuration.
 *****************************************************************************/

/*****************************************************************************
 * Global variables - these are the only ones, see main.h and modules.h
 *****************************************************************************/
main_t        *p_main;
module_bank_t *p_module_bank;
input_bank_t  *p_input_bank;
aout_bank_t   *p_aout_bank;
vout_bank_t   *p_vout_bank;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  GetConfigurationFromFile    ( void ){return 0;};
static int  GetConfigurationFromCmdLine ( int *pi_argc, char *ppsz_argv[],
                                          boolean_t b_ignore_errors );
static int  GetFilenames                ( int i_argc, char *ppsz_argv[] );
static void Usage                       ( const char *psz_module_name );
static void ListModules                 ( void );
static void Version                     ( void );

static void InitSignalHandler           ( void );
static void SimpleSignalHandler         ( int i_signal );
static void FatalSignalHandler          ( int i_signal );
static void IllegalSignalHandler        ( int i_signal );
static u32  CPUCapabilities             ( void );

#ifdef WIN32
static void ShowConsole                 ( void );
#endif

static jmp_buf env;
static int     i_illegal;
static char   *psz_capability;

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
    main_t        main_data;                /* root of all data - see main.h */
    module_bank_t module_bank;
    input_bank_t  input_bank;
    aout_bank_t   aout_bank;
    vout_bank_t   vout_bank;
    char *p_tmp;

    p_main        = &main_data;               /* set up the global variables */
    p_module_bank = &module_bank;
    p_input_bank  = &input_bank;
    p_aout_bank   = &aout_bank;
    p_vout_bank   = &vout_bank;

    p_main->i_warning_level = 0;

#if defined( ENABLE_NLS ) && defined ( HAVE_GETTEXT )
    /*
     * Support for getext
     */
#if defined( HAVE_LOCALE_H ) && defined( HAVE_LC_MESSAGES )
    if( !setlocale( LC_MESSAGES, "" ) )
    {
        fprintf( stderr, "warning: unsupported locale.\n" );
    }
#endif

    if( !bindtextdomain( PACKAGE, LOCALEDIR ) )
    {
        fprintf( stderr, "warning: no domain %s in directory %s\n",
                 PACKAGE, LOCALEDIR );
    }

    textdomain( PACKAGE );
#endif

    /*
     * Initialize threads system
     */
    vlc_threads_init( );

    /*
     * Test if our code is likely to run on this CPU
     */
    p_main->i_cpu_capabilities = CPUCapabilities();

    /*
     * System specific initialization code
     */
#if defined( SYS_BEOS ) || defined( SYS_DARWIN ) || defined( WIN32 )
    system_Init( &i_argc, ppsz_argv, ppsz_env );

#elif defined( SYS_LINUX )
#   ifdef DEBUG
    /* Activate malloc checking routines to detect heap corruptions. */
    putenv( "MALLOC_CHECK_=2" );
#   endif
#endif

    /*
     * Initialize messages interface
     */
    intf_MsgCreate();

    intf_Msg( COPYRIGHT_MESSAGE "\n" );


    /* Get the executable name (similar to the basename command) */
    p_main->psz_arg0 = p_tmp = ppsz_argv[ 0 ];
    while( *p_tmp )
    {
        if( *p_tmp == '/' ) p_main->psz_arg0 = ++p_tmp;
        else ++p_tmp;
    }

    /*
     * Initialize the module bank and and load the configuration of the main
     * module. We need to do this at this stage to be able to display a short
     * help if required by the user. (short help == main module options)
     */
    module_InitBank();
    module_LoadMain();

    if( GetConfigurationFromCmdLine( &i_argc, ppsz_argv, 1 ) )
    {
        intf_MsgDestroy();
        return( errno );
    }

    /* Check for short help option */
    if( config_GetIntVariable( "help" ) )
    {
        Usage( "main" );
        return( -1 );
    }

    /* Check for version option */
    if( config_GetIntVariable( "version" ) )
    {
        Version();
        return( -1 );
    }

    /*
     * Load the builtins and plugins into the module_bank.
     * We have to do it before GetConfiguration() because this also gets the
     * list of configuration options exported by each plugin and loads their
     * default values.
     */
    module_LoadBuiltins();
    module_LoadPlugins();
    intf_WarnMsg( 2, "module: module bank initialized, found %i modules",
                  p_module_bank->i_count );

    /* Check for help on plugins */
    if( (p_tmp = config_GetPszVariable( "pluginhelp" )) )
    {
        Usage( p_tmp );
        free( p_tmp );
        return( -1 );
    }

    /* Check for long help option */
    if( config_GetIntVariable( "longhelp" ) )
    {
        Usage( NULL );
        return( -1 );
    }

    /* Check for plugin list option */
    if( config_GetIntVariable( "list" ) )
    {
        ListModules();
        return( -1 );
    }

    /*
     * Override default configuration with config file settings
     */
    if( GetConfigurationFromFile() )
    {
        intf_MsgDestroy();
        return( errno );
    }

    /*
     * Override configuration with command line settings
     */
    if( GetConfigurationFromCmdLine( &i_argc, ppsz_argv, 0 ) )
    {
        intf_MsgDestroy();
        return( errno );
    }

    /* p_main inititalization. FIXME ? */
    p_main->i_desync = (mtime_t)config_GetIntVariable( AOUT_DESYNC_VAR )
      * (mtime_t)1000;
    p_main->b_stats = config_GetIntVariable( INTF_STATS_VAR );
    p_main->b_audio = !config_GetIntVariable( AOUT_NOAUDIO_VAR );
    p_main->b_stereo= !config_GetIntVariable( AOUT_MONO_VAR );
    p_main->b_video = !config_GetIntVariable( VOUT_NOVIDEO_VAR );
    if( config_GetIntVariable( NOMMX_VAR ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_MMX;
    if( config_GetIntVariable( NO3DN_VAR ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_3DNOW;
    if( config_GetIntVariable( NOMMXEXT_VAR ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_MMXEXT;
    if( config_GetIntVariable( NOSSE_VAR ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_SSE;
    if( config_GetIntVariable( NOALTIVEC_VAR ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_ALTIVEC;


    if( p_main->b_stats )
    {
        char          p_capabilities[200];
        p_capabilities[0] = '\0';

#define PRINT_CAPABILITY( capability, string )                              \
        if( p_main->i_cpu_capabilities & capability )                       \
        {                                                                   \
            strncat( p_capabilities, string " ",                            \
                     sizeof(p_capabilities) - strlen(p_capabilities) );     \
            p_capabilities[sizeof(p_capabilities) - 1] = '\0';              \
        }

        PRINT_CAPABILITY( CPU_CAPABILITY_486, "486" );
        PRINT_CAPABILITY( CPU_CAPABILITY_586, "586" );
        PRINT_CAPABILITY( CPU_CAPABILITY_PPRO, "Pentium Pro" );
        PRINT_CAPABILITY( CPU_CAPABILITY_MMX, "MMX" );
        PRINT_CAPABILITY( CPU_CAPABILITY_3DNOW, "3DNow!" );
        PRINT_CAPABILITY( CPU_CAPABILITY_MMXEXT, "MMXEXT" );
        PRINT_CAPABILITY( CPU_CAPABILITY_SSE, "SSE" );
        PRINT_CAPABILITY( CPU_CAPABILITY_ALTIVEC, "Altivec" );
        PRINT_CAPABILITY( CPU_CAPABILITY_FPU, "FPU" );
        intf_StatMsg( "info: CPU has capabilities : %s", p_capabilities );
    }

    /*
     * Initialize playlist and get commandline files
     */
    p_main->p_playlist = intf_PlaylistCreate();
    if( !p_main->p_playlist )
    {
        intf_ErrMsg( "playlist error: playlist initialization failed" );
        intf_MsgDestroy();
        return( errno );
    }
    intf_PlaylistInit( p_main->p_playlist );

    /*
     * Get input filenames given as commandline arguments
     */
    GetFilenames( i_argc, ppsz_argv );

    /*
     * Initialize input, aout and vout banks
     */
    input_InitBank();
    aout_InitBank();
    vout_InitBank();

    /*
     * Choose the best memcpy module
     */
    p_main->p_memcpy_module = module_Need( MODULE_CAPABILITY_MEMCPY, NULL,
                                           NULL );
    if( p_main->p_memcpy_module == NULL )
    {
        intf_ErrMsg( "intf error: no suitable memcpy module, "
                     "using libc default" );
        p_main->pf_memcpy = memcpy;
    }
    else
    {
        p_main->pf_memcpy = p_main->p_memcpy_module->p_functions
                                  ->memcpy.functions.memcpy.pf_memcpy;
    }

    /*
     * Initialize shared resources and libraries
     */
    if( config_GetIntVariable( INPUT_NETWORK_CHANNEL_VAR ) &&
        network_ChannelCreate() )
    {
        /* On error during Channels initialization, switch off channels */
        intf_ErrMsg( "intf error: channels initialization failed, " 
                                 "deactivating channels" );
        config_PutIntVariable( INPUT_NETWORK_CHANNEL_VAR, 0 );
    }

    /*
     * Try to run the interface
     */
    p_main->p_intf = intf_Create();
    if( p_main->p_intf == NULL )
    {
        intf_ErrMsg( "intf error: interface initialization failed" );
    }
    else
    {
        /*
         * Set signal handling policy for all threads
         */
        InitSignalHandler();

        /*
         * This is the main loop
         */
        p_main->p_intf->pf_run( p_main->p_intf );

        /*
         * Finished, destroy the interface
         */
        intf_Destroy( p_main->p_intf );

        /*
         * Go back into channel 0 which is the network
         */
        if( config_GetIntVariable( INPUT_NETWORK_CHANNEL_VAR ) )
        {
            network_ChannelJoin( COMMON_CHANNEL );
        }
    }

    /*
     * Free input, aout and vout banks
     */
    input_EndBank();
    vout_EndBank();
    aout_EndBank();

    /*
     * Free playlist
     */
    intf_PlaylistDestroy( p_main->p_playlist );

    /*
     * Free memcpy module if it was allocated
     */
    if( p_main->p_memcpy_module != NULL )
    {
        module_Unneed( p_main->p_memcpy_module );
    }

    /*
     * Free module bank
     */
    module_EndBank();

    /*
     * System specific cleaning code
     */
#if defined( SYS_BEOS ) || defined( SYS_DARWIN ) || defined( WIN32 )
    system_End();
#endif


    /*
     * Terminate messages interface and program
     */
    intf_WarnMsg( 1, "intf: program terminated" );
    intf_MsgDestroy();

    /*
     * Stop threads system
     */
    vlc_threads_end( );

    return 0;
}


/* following functions are local */

/*****************************************************************************
 * GetConfigurationFromCmdLine: parse command line
 *****************************************************************************
 * Parse command line for configuration. If the inline help is requested, the
 * function Usage() is called and the function returns -1 (causing main() to
 * exit).
 * Now that the module_bank has been initialized, we can dynamically
 * generate the longopts structure used by getops. We have to do it this way
 * because we don't know (and don't want to know) in advance the configuration
 * options used (ie. exported) by each module.
 *****************************************************************************/
static int GetConfigurationFromCmdLine( int *pi_argc, char *ppsz_argv[],
                                        boolean_t b_ignore_errors )
{
    int i_cmd, i, i_index, i_longopts_size;
    module_t *p_module;
    struct option *p_longopts;

    /* Short options */
    const char *psz_shortopts = "hHvl";


    /* Set default configuration and copy arguments */
    p_main->i_argc    = *pi_argc;
    p_main->ppsz_argv = ppsz_argv;

    p_main->p_channel = NULL;

#ifdef SYS_DARWIN
    /* When vlc.app is run by double clicking in Mac OS X, the 2nd arg
     * is the PSN - process serial number (a unique PID-ish thingie)
     * still ok for real Darwin & when run from command line */
    if ( (*pi_argc > 1) && (strncmp( ppsz_argv[ 1 ] , "-psn" , 4 ) == 0) )
                                        /* for example -psn_0_9306113 */
    {
        /* GDMF!... I can't do this or else the MacOSX window server will
         * not pick up the PSN and not register the app and we crash...
         * hence the following kludge otherwise we'll get confused w/ argv[1]
         * being an input file name */
#if 0
        ppsz_argv[ 1 ] = NULL;
#endif
        *pi_argc = *pi_argc - 1;
        pi_argc--;
        return( 0 );
    }
#endif


    /*
     * Generate the longopts structure used by getopt_long
     */
    i_longopts_size = 0;
    for( p_module = p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        /* count the number of exported configuration options (to allocate
         * longopts). The i_config_options we use is an approximation of the
         * real number of options (it also includes markers like: category ...)
         * but it is enough for our purpose */
        i_longopts_size += p_module->i_config_options -1;
    }

    p_longopts = (struct option *)malloc( sizeof(struct option)
                                          * (i_longopts_size + 1) );
    if( p_longopts == NULL )
    {
        intf_ErrMsg( "GetConfigurationFromCmdLine error: "
                     "can't allocate p_longopts" );
        return( -1 );
    }

    /* Fill the longopts structure */
    i_index = 0;
    for( p_module = p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        for( i = 1; i < (p_module->i_config_options -1); i++ )
        {
            if( (p_module->p_config[i].i_type == MODULE_CONFIG_ITEM_CATEGORY)||
                (p_module->p_config[i].i_type ==
                     MODULE_CONFIG_ITEM_SUBCATEGORY)||
                (p_module->p_config[i].i_type ==
                     MODULE_CONFIG_ITEM_SUBCATEGORY_END) )
                 continue;
            p_longopts[i_index].name = p_module->p_config[i].psz_name;
            p_longopts[i_index].has_arg =
                (p_module->p_config[i].i_type == MODULE_CONFIG_ITEM_BOOL)?
                                               no_argument : required_argument;
            p_longopts[i_index].flag = 0;
            p_longopts[i_index].val = 0;
            i_index++;
        }
    }
    /* Close the longopts structure */
    memset( &p_longopts[i_index], 0, sizeof(struct option) );


    /*
     * Parse the command line options
     */
    opterr = 0;
    optind = 1;
    while( ( i_cmd = getopt_long( *pi_argc, ppsz_argv, psz_shortopts,
                                  p_longopts, &i_index ) ) != EOF )
    {

        if( i_cmd == 0 )
        {
            /* A long option has been recognized */

            module_config_t *p_conf;

            /* Store the configuration option */
            p_conf = config_FindConfig( p_longopts[i_index].name );

            switch( p_conf->i_type )
            {
            case MODULE_CONFIG_ITEM_STRING:
            case MODULE_CONFIG_ITEM_FILE:
            case MODULE_CONFIG_ITEM_PLUGIN:
                config_PutPszVariable( p_longopts[i_index].name, optarg );
                break;
            case MODULE_CONFIG_ITEM_INTEGER:
                config_PutIntVariable( p_longopts[i_index].name, atoi(optarg));
                break;
            case MODULE_CONFIG_ITEM_BOOL:
                config_PutIntVariable( p_longopts[i_index].name, 1 );
                break;
            }

            continue;
        }

        /* short options handled here for now */
        switch( i_cmd )
        {

        /* General/common options */
        case 'h':                                              /* -h, --help */
            config_PutIntVariable( "help", 1 );
            break;
        case 'H':                                          /* -H, --longhelp */
            config_PutIntVariable( "longhelp", 1 );
            break;
        case 'l':                                              /* -l, --list */
            config_PutIntVariable( "list", 1 );
            break;
        case 'v':                                           /* -v, --verbose */
            p_main->i_warning_level++;
            break;

        /* Internal error: unknown option */
        case '?':
        default:
            if( !b_ignore_errors )
            {
                intf_ErrMsg( "intf error: unknown option `%s'",
                             ppsz_argv[optind] );
                intf_Msg( "Try `%s --help' for more information.\n",
                          p_main->psz_arg0 );

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
                intf_Msg( "\nPress the RETURN key to continue..." );
                getchar();
#endif
                free( p_longopts );
                return( EINVAL );
                break;
            }
        }

    }

    if( p_main->i_warning_level < 0 )
    {
        p_main->i_warning_level = 0;
    }

    free( p_longopts );
    return( 0 );
}

/*****************************************************************************
 * GetFilenames: parse command line options which are not flags
 *****************************************************************************
 * Parse command line for input files.
 *****************************************************************************/
static int GetFilenames( int i_argc, char *ppsz_argv[] )
{
    int i_opt;

    /* We assume that the remaining parameters are filenames */
    for( i_opt = optind; i_opt < i_argc; i_opt++ )
    {
        intf_PlaylistAdd( p_main->p_playlist, PLAYLIST_END,
                          ppsz_argv[ i_opt ] );
    }

    return( 0 );
}

/*****************************************************************************
 * Usage: print program usage
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
static void Usage( const char *psz_module_name )
{
    int i;
    module_t *p_module;
    char psz_spaces[30];

    memset( psz_spaces, 32, 30 );

#ifdef WIN32
    ShowConsole();
#endif

    /* Usage */
    intf_Msg( "Usage: %s [options] [parameters] [file]...\n",
              p_main->psz_arg0 );

    /* Enumerate the config of each module */
    for( p_module = p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {

        if( psz_module_name && strcmp( psz_module_name, p_module->psz_name ) )
            continue;

        /* print module name */
        intf_Msg( "%s configuration:\n", p_module->psz_name );

        for( i = 0; i < (p_module->i_config_options -1); i++ )
        {
            int j;

            switch( p_module->p_config[i].i_type )
            {
            case MODULE_CONFIG_ITEM_CATEGORY:
                intf_Msg( " %s", p_module->p_config[i].psz_text );
                break;

            case MODULE_CONFIG_ITEM_STRING:
            case MODULE_CONFIG_ITEM_FILE:
            case MODULE_CONFIG_ITEM_PLUGIN:
                /* Nasty hack, but right now I'm too tired to think about
                 * a nice solution */
                j = 25 - strlen( p_module->p_config[i].psz_name )
                    - strlen(" <string>") - 1;
                if( j < 0 ) j = 0; psz_spaces[j] = 0;

                intf_Msg( "  --%s <string>%s %s",
                          p_module->p_config[i].psz_name, psz_spaces,
                          p_module->p_config[i].psz_text );
                psz_spaces[j] = 32;
                break;
            case MODULE_CONFIG_ITEM_INTEGER:
                /* Nasty hack, but right now I'm too tired to think about
                 * a nice solution */
                j = 25 - strlen( p_module->p_config[i].psz_name )
                    - strlen(" <integer>") - 1;
                if( j < 0 ) j = 0; psz_spaces[j] = 0;

                intf_Msg( "  --%s <integer>%s %s",
                          p_module->p_config[i].psz_name, psz_spaces,
                          p_module->p_config[i].psz_text );
                psz_spaces[j] = 32;
                break;
            default:
                /* Nasty hack, but right now I'm too tired to think about
                 * a nice solution */
                j = 25 - strlen( p_module->p_config[i].psz_name ) - 1;
                if( j < 0 ) j = 0; psz_spaces[j] = 0;

                intf_Msg( "  --%s%s %s",
                          p_module->p_config[i].psz_name, psz_spaces,
                          p_module->p_config[i].psz_text );
                psz_spaces[j] = 32;
                break;
            }
        }

        /* Yet another nasty hack.
         * Maybe we could use MODULE_CONFIG_ITEM_END to display tail messages
         * for each module?? */
        if( !strcmp( "main", p_module->psz_name ) )
            intf_Msg( "\nPlaylist items:"
                "\n  *.mpg, *.vob                   \tPlain MPEG-1/2 files"
                "\n  dvd:<device>[@<raw device>]    \tDVD device"
                "\n  vcd:<device>                   \tVCD device"
                "\n  udpstream:[<server>[:<server port>]][@[<bind address>]"
                      "[:<bind port>]]"
                "\n                                 \tUDP stream sent by VLS"
                "\n  vlc:loop                       \tLoop execution of the "
                      "playlist"
                "\n  vlc:pause                      \tPause execution of "
                      "playlist items"
                "\n  vlc:quit                       \tQuit VLC" );

        intf_Msg( "" );

    }

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
        intf_Msg( "\nPress the RETURN key to continue..." );
        getchar();
#endif
}

/*****************************************************************************
 * ListModules: list the available modules with their description
 *****************************************************************************
 * Print a list of all available modules (builtins and plugins) and a short
 * description for each one.
 *****************************************************************************/
static void ListModules( void )
{
    module_t *p_module;
    char psz_spaces[20];

    memset( psz_spaces, 32, 20 );

#ifdef WIN32
    ShowConsole();
#endif

    /* Usage */
    intf_Msg( "Usage: %s [options] [parameters] [file]...\n",
              p_main->psz_arg0 );

    intf_Msg( "[plugin]              [description]" );

    /* Enumerate each module */
    for( p_module = p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {
        int i;

        /* Nasty hack, but right now I'm too tired to think about a nice
         * solution */
        i = 20 - strlen( p_module->psz_name ) - 1;
        if( i < 0 ) i = 0;
        psz_spaces[i] = 0;

        intf_Msg( "  %s%s %s", p_module->psz_name, psz_spaces,
                  p_module->psz_longname );

        psz_spaces[i] = 32;

    }

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
        intf_Msg( "\nPress the RETURN key to continue..." );
        getchar();
#endif
}

/*****************************************************************************
 * Version: print complete program version
 *****************************************************************************
 * Print complete program version and build number.
 *****************************************************************************/
static void Version( void )
{
#ifdef WIN32
    ShowConsole();
#endif
    intf_Msg( VERSION_MESSAGE
        "This program comes with NO WARRANTY, to the extent permitted by law.\n"
        "You may redistribute it under the terms of the GNU General Public License;\n"
        "see the file named COPYING for details.\n"
        "Written by the VideoLAN team at Ecole Centrale, Paris." );
#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
        intf_Msg( "\nPress the RETURN key to continue..." );
        getchar();
#endif
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
#ifndef WIN32
    signal( SIGINT,  FatalSignalHandler );
    signal( SIGHUP,  FatalSignalHandler );
    signal( SIGQUIT, FatalSignalHandler );

    /* Other signals */
    signal( SIGALRM, SimpleSignalHandler );
    signal( SIGPIPE, SimpleSignalHandler );
#endif
}

/*****************************************************************************
 * SimpleSignalHandler: system signal handler
 *****************************************************************************
 * This function is called when a non fatal signal is received by the program.
 *****************************************************************************/
static void SimpleSignalHandler( int i_signal )
{
    /* Acknowledge the signal received */
    intf_WarnMsg( 0, "intf: ignoring signal %d", i_signal );
}

/*****************************************************************************
 * FatalSignalHandler: system signal handler
 *****************************************************************************
 * This function is called when a fatal signal is received by the program.
 * It tries to end the program in a clean way.
 *****************************************************************************/
static void FatalSignalHandler( int i_signal )
{
    /* Once a signal has been trapped, the termination sequence will be
     * armed and following signals will be ignored to avoid sending messages
     * to an interface having been destroyed */
#ifndef WIN32
    signal( SIGINT,  SIG_IGN );
    signal( SIGHUP,  SIG_IGN );
    signal( SIGQUIT, SIG_IGN );
#endif

    /* Acknowledge the signal received */
    intf_ErrMsg( "intf error: signal %d received, exiting", i_signal );

    /* Try to terminate everything - this is done by requesting the end of the
     * interface thread */
    p_main->p_intf->b_die = 1;
}

/*****************************************************************************
 * IllegalSignalHandler: system signal handler
 *****************************************************************************
 * This function is called when an illegal instruction signal is received by
 * the program. We use this function to test OS and CPU capabilities
 *****************************************************************************/
static void IllegalSignalHandler( int i_signal )
{
    /* Acknowledge the signal received */
    i_illegal = 1;

#ifdef HAVE_SIGRELSE
    sigrelse( i_signal );
#endif

    fprintf( stderr, "warning: your CPU has %s instructions, but not your "
                     "operating system.\n", psz_capability );
    fprintf( stderr, "         some optimizations will be disabled unless "
                     "you upgrade your OS\n" );
#ifdef SYS_LINUX
    fprintf( stderr, "         (for instance Linux kernel 2.4.x or later)" );
#endif

    longjmp( env, 1 );
}

/*****************************************************************************
 * CPUCapabilities: list the processors MMX support and other capabilities
 *****************************************************************************
 * This function is called to list extensions the CPU may have.
 *****************************************************************************/
static u32 CPUCapabilities( void )
{
    volatile u32 i_capabilities = CPU_CAPABILITY_NONE;

#if defined( SYS_DARWIN )
    struct host_basic_info hi;
    kern_return_t          ret;
    host_name_port_t       host;

    int i_size;
    char *psz_name, *psz_subname;

    i_capabilities |= CPU_CAPABILITY_FPU;

    /* Should 'never' fail? */
    host = mach_host_self();

    i_size = sizeof( hi ) / sizeof( int );
    ret = host_info( host, HOST_BASIC_INFO, ( host_info_t )&hi, &i_size );

    if( ret != KERN_SUCCESS )
    {
        fprintf( stderr, "error: couldn't get CPU information\n" );
        return( i_capabilities );
    }

    slot_name( hi.cpu_type, hi.cpu_subtype, &psz_name, &psz_subname );
    /* FIXME: need better way to detect newer proccessors.
     * could do strncmp(a,b,5), but that's real ugly */
    if( !strcmp(psz_name, "ppc7400") || !strcmp(psz_name, "ppc7450") )
    {
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;
    }

    return( i_capabilities );

#elif defined( __i386__ )
    volatile unsigned int  i_eax, i_ebx, i_ecx, i_edx;
    volatile boolean_t     b_amd;

    /* Needed for x86 CPU capabilities detection */
#   define cpuid( a )                      \
        asm volatile ( "pushl %%ebx\n\t"   \
                       "cpuid\n\t"         \
                       "movl %%ebx,%1\n\t" \
                       "popl %%ebx\n\t"    \
                     : "=a" ( i_eax ),     \
                       "=r" ( i_ebx ),     \
                       "=c" ( i_ecx ),     \
                       "=d" ( i_edx )      \
                     : "a"  ( a )          \
                     : "cc" );

    i_capabilities |= CPU_CAPABILITY_FPU;

#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
    signal( SIGILL, IllegalSignalHandler );
#   endif

    /* test for a 486 CPU */
    asm volatile ( "pushl %%ebx\n\t"
                   "pushfl\n\t"
                   "popl %%eax\n\t"
                   "movl %%eax, %%ebx\n\t"
                   "xorl $0x200000, %%eax\n\t"
                   "pushl %%eax\n\t"
                   "popfl\n\t"
                   "pushfl\n\t"
                   "popl %%eax\n\t"
                   "movl %%ebx,%1\n\t"
                   "popl %%ebx\n\t"
                 : "=a" ( i_eax ),
                   "=r" ( i_ebx )
                 :
                 : "cc" );

    if( i_eax == i_ebx )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
        signal( SIGILL, NULL );
#   endif
        return( i_capabilities );
    }

    i_capabilities |= CPU_CAPABILITY_486;

    /* the CPU supports the CPUID instruction - get its level */
    cpuid( 0x00000000 );

    if( !i_eax )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
        signal( SIGILL, NULL );
#   endif
        return( i_capabilities );
    }

    /* FIXME: this isn't correct, since some 486s have cpuid */
    i_capabilities |= CPU_CAPABILITY_586;

    /* borrowed from mpeg2dec */
    b_amd = ( i_ebx == 0x68747541 ) && ( i_ecx == 0x444d4163 )
                    && ( i_edx == 0x69746e65 );

    /* test for the MMX flag */
    cpuid( 0x00000001 );

    if( ! (i_edx & 0x00800000) )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
        signal( SIGILL, NULL );
#   endif
        return( i_capabilities );
    }

    i_capabilities |= CPU_CAPABILITY_MMX;

    if( i_edx & 0x02000000 )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;

#   ifdef CAN_COMPILE_SSE
        /* We test if OS support the SSE instructions */
        psz_capability = "SSE";
        i_illegal = 0;
        if( setjmp( env ) == 0 )
        {
            /* Test a SSE instruction */
            __asm__ __volatile__ ( "xorps %%xmm0,%%xmm0\n" : : );
        }

        if( i_illegal == 0 )
        {
            i_capabilities |= CPU_CAPABILITY_SSE;
        }
#   endif
    }

    /* test for additional capabilities */
    cpuid( 0x80000000 );

    if( i_eax < 0x80000001 )
    {
#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
        signal( SIGILL, NULL );
#   endif
        return( i_capabilities );
    }

    /* list these additional capabilities */
    cpuid( 0x80000001 );

#   ifdef CAN_COMPILE_3DNOW
    if( i_edx & 0x80000000 )
    {
        psz_capability = "3D Now!";
        i_illegal = 0;
        if( setjmp( env ) == 0 )
        {
            /* Test a 3D Now! instruction */
            __asm__ __volatile__ ( "pfadd %%mm0,%%mm0\n" "femms\n" : : );
        }

        if( i_illegal == 0 )
        {
            i_capabilities |= CPU_CAPABILITY_3DNOW;
        }
    }
#   endif

    if( b_amd && ( i_edx & 0x00400000 ) )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;
    }

#   if defined( CAN_COMPILE_SSE ) || defined ( CAN_COMPILE_3DNOW )
    signal( SIGILL, NULL );
#   endif
    return( i_capabilities );

#elif defined( __powerpc__ )

    i_capabilities |= CPU_CAPABILITY_FPU;

#   ifdef CAN_COMPILE_ALTIVEC
    signal( SIGILL, IllegalSignalHandler );

    psz_capability = "AltiVec";
    i_illegal = 0;
    if( setjmp( env ) == 0 )
    {
        asm volatile ("mtspr 256, %0\n\t"
                      "vand %%v0, %%v0, %%v0"
                      :
                      : "r" (-1));
    }

    if( i_illegal == 0 )
    {
        i_capabilities |= CPU_CAPABILITY_ALTIVEC;
    }

    signal( SIGILL, NULL );
#   endif

    return( i_capabilities );

#else
    /* default behaviour */
    return( i_capabilities );

#endif
}

/*****************************************************************************
 * ShowConsole: On Win32, create an output console for debug messages
 *****************************************************************************
 * This function is usefull only on Win32.
 *****************************************************************************/
#ifdef WIN32 /*  */
static void ShowConsole( void )
{
    AllocConsole();
    freopen( "CONOUT$", "w", stdout );
    freopen( "CONOUT$", "w", stderr );
    freopen( "CONIN$", "r", stdin );
    return;
}
#endif
