/*****************************************************************************
 * main.c: main vlc source
 * Includes the main() function for vlc. Parses command line, start interface
 * and spawn threads.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: main.c,v 1.182 2002/04/21 11:23:03 gbazin Exp $
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


#define INTF_TEXT N_("interface module")
#define INTF_LONGTEXT N_( \
    "This option allows you to select the interface used by vlc.\nNote that " \
    "the default behaviour is to automatically select the best method " \
    "available.")

#define WARNING_TEXT N_("warning level (or use -v, -vv, etc...)")
#define WARNING_LONGTEXT N_( \
    "Increasing the warning level will allow you to see more debug messages " \
    "and can sometimes help you to troubleshoot a problem.")

#define STATS_TEXT N_("output statistics")
#define STATS_LONGTEXT N_( \
    "Enabling the stats mode will flood your log console with various " \
    "statistics messages.")

#define INTF_PATH_TEXT N_("interface default search path")
#define INTF_PATH_LONGTEXT N_( \
    "This option allows you to set the default path that the interface will " \
    "open when looking for a file.")

#define AOUT_TEXT N_("audio output module")
#define AOUT_LONGTEXT N_( \
    "This option allows you to select the audio audio output method used by " \
    "vlc.\nNote that the default behaviour is to automatically select the " \
    "best method available.")

#define NOAUDIO_TEXT N_("disable audio")
#define NOAUDIO_LONGTEXT N_( \
    "This will completely disable the audio output. The audio decoding " \
    "stage shouldn't even be done, so it can allow you to save some " \
    "processing power.")

#define MONO_TEXT N_("mono audio")
#define MONO_LONGTEXT N_("This will force a mono audio output")

#define VOLUME_TEXT N_("audio output volume")
#define VOLUME_LONGTEXT N_( \
    "You can set the default audio output volume here, in a range from 0 to " \
    "1024.")

#define FORMAT_TEXT N_("audio output format")
#define FORMAT_LONGTEXT N_( \
    "You can force the audio output format here.\n" \
    "0 -> 16 bits signed native endian (default)\n" \
    "1 ->  8 bits unsigned\n"                       \
    "2 -> 16 bits signed little endian\n"           \
    "3 -> 16 bits signed big endian\n"              \
    "4 ->  8 bits signed\n"                         \
    "5 -> 16 bits unsigned little endian\n"         \
    "6 -> 16 bits unsigned big endian\n"            \
    "7 -> mpeg2 audio (unsupported)\n"              \
    "8 -> ac3 pass-through")

#define RATE_TEXT N_("audio output frequency (Hz)")
#define RATE_LONGTEXT N_( \
    "You can force the audio output frequency here.\nCommon values are " \
    "48000, 44100, 32000, 22050, 16000, 11025, 8000.")

#define DESYNC_TEXT N_("compensate desynchronization of audio (in ms)")
#define DESYNC_LONGTEXT N_( \
    "This option allows you to delay the audio output. This can be handy if " \
    "you notice a lag between the video and the audio.")

#define VOUT_TEXT N_("video output module")
#define VOUT_LONGTEXT N_( \
    "This option allows you to select the video output method used by vlc.\n" \
    "Note that the default behaviour is to automatically select the best " \
    "method available.")

#define NOVIDEO_TEXT N_("disable video")
#define NOVIDEO_LONGTEXT N_( \
    "This will completely disable the video output. The video decoding " \
    "stage shouldn't even be done, so it can allow you to save some " \
    "processing power.")

#define DISPLAY_TEXT N_("display identifier")
#define DISPLAY_LONGTEXT ""

#define WIDTH_TEXT N_("video width")
#define WIDTH_LONGTEXT N_( \
    "You can enforce the video width here.\nNote that by default vlc will " \
    "adapt to the video characteristics.")

#define HEIGHT_TEXT N_("video height")
#define HEIGHT_LONGTEXT N_( \
    "You can enforce the video height here.\nNote that by default vlc will " \
    "adapt to the video characteristics.")

#define ZOOM_TEXT N_("zoom video")
#define ZOOM_LONGTEXT N_( \
    "You can zoom the video by the specified factor.")

#define GRAYSCALE_TEXT N_("grayscale video output")
#define GRAYSCALE_LONGTEXT N_( \
    "Using this option, vlc will not decode the color information from the " \
    "video (this can also allow you to save some processing power).")

#define FULLSCREEN_TEXT N_("fullscreen video output")
#define FULLSCREEN_LONGTEXT N_( \
    "If this option is enabled, vlc will always start a video in fullscreen " \
    "mode.")

#define NOOVERLAY_TEXT N_("disable hardware acceleration for the video output")
#define NOOVERLAY_LONGTEXT N_( \
    "By default vlc will try to take advantage of the overlay capabilities " \
    "of you graphics card.")

#define SPUMARGIN_TEXT N_("force SPU position")
#define SPUMARGIN_LONGTEXT ""

#define FILTER_TEXT N_("video filter module")
#define FILTER_LONGTEXT ""

#define SERVER_PORT_TEXT N_("server port")
#define SERVER_PORT_LONGTEXT ""

#define NETCHANNEL_TEXT N_("enable network channel mode")
#define NETCHANNEL_LONGTEXT ""

#define CHAN_SERV_TEXT N_("channel server address")
#define CHAN_SERV_LONGTEXT ""

#define CHAN_PORT_TEXT N_("channel server port")
#define CHAN_PORT_LONGTEXT ""

#define IFACE_TEXT N_("network interface")
#define IFACE_LONGTEXT ""

#define INPUT_PROGRAM_TEXT N_("choose program (SID)")
#define INPUT_PROGRAM_LONGTEXT N_( \
    "Choose the program to select by giving its Service ID.")

#define INPUT_AUDIO_TEXT N_("choose audio")
#define INPUT_AUDIO_LONGTEXT ""

#define INPUT_CHAN_TEXT N_("choose channel")
#define INPUT_CHAN_LONGTEXT ""

#define INPUT_SUBT_TEXT N_("choose subtitles")
#define INPUT_SUBT_LONGTEXT ""

#define DVD_DEV_TEXT N_("DVD device")
#define DVD_DEV_LONGTEXT ""

#define VCD_DEV_TEXT N_("VCD device")
#define VCD_DEV_LONGTEXT ""

#define SAT_FREQ_TEXT N_("satellite transponder frequency")
#define SAT_FREQ_LONGTEXT ""

#define SAT_POL_TEXT N_("satellite transponder polarization")
#define SAT_POL_LONGTEXT ""

#define SAT_FEC_TEXT N_("satellite transponder FEC")
#define SAT_FEC_LONGTEXT ""

#define SAT_SRATE_TEXT N_("satellite transponder symbol rate")
#define SAT_SRATE_LONGTEXT ""

#define SAT_DISEQC_TEXT N_("use diseqc with antenna")
#define SAT_DISEQC_LONGTEXT ""

#define SAT_LNB_LOF1_TEXT N_("antenna lnb_lof1 (kHz)")
#define SAT_LNB_LOF1_LONGTEXT ""

#define SAT_LNB_LOF2_TEXT N_("antenna lnb_lof2 (kHz)")
#define SAT_LNB_LOF2_LONGTEXT ""

#define SAT_LNB_SLOF_TEXT N_("antenna lnb_slof (kHz)")
#define SAT_LNB_SLOF_LONGTEXT ""

#define IPV6_TEXT N_("force IPv6")
#define IPV6_LONGTEXT ""

#define IPV4_TEXT N_("force IPv4")
#define IPV4_LONGTEXT ""

#define ADEC_MPEG_TEXT N_("choose MPEG audio decoder")
#define ADEC_MPEG_LONGTEXT ""

#define ADEC_AC3_TEXT N_("choose AC3 audio decoder")
#define ADEC_AC3_LONGTEXT ""

#define VDEC_SMP_TEXT N_("use additional processors")
#define VDEC_SMP_LONGTEXT ""

#define VPAR_SYNCHRO_TEXT N_("force synchro algorithm {I|I+|IP|IP+|IPB}")
#define VPAR_SYNCHRO_LONGTEXT ""

#define NOMMX_TEXT N_("disable CPU's MMX support")
#define NOMMX_LONGTEXT ""

#define NO3DN_TEXT N_("disable CPU's 3D Now! support")
#define NO3DN_LONGTEXT ""

#define NOMMXEXT_TEXT N_("disable CPU's MMX EXT support")
#define NOMMXEXT_LONGTEXT ""

#define NOSSE_TEXT N_("disable CPU's SSE support")
#define NOSSE_LONGTEXT ""

#define NOALTIVEC_TEXT N_("disable CPU's AltiVec support")
#define NOALTIVEC_LONGTEXT ""

#define PLAYLIST_LAUNCH_TEXT N_("launch playlist on startup")
#define PLAYLIST_LAUNCH_LONGTEXT ""

#define PLAYLIST_ENQUEUE_TEXT N_("enqueue playlist as default")
#define PLAYLIST_ENQUEUE_LONGTEXT ""

#define PLAYLIST_LOOP_TEXT N_("loop playlist on end")
#define PLAYLIST_LOOP_LONGTEXT ""

#define MEMCPY_TEXT N_("memory copy module")
#define MEMCPY_LONGTEXT ""

#define FAST_PTHREAD_TEXT N_("fast pthread on NT/2K/XP (developpers only)")
#define FAST_PTHREAD_LONGTEXT N_( \
    "On Windows NT/2K/XP we use a slow but correct pthread implementation, " \
    "you can also use this faster implementation but you might experience " \
    "problems with it.")

/*
 * Quick usage guide for the configuration options:
 *
 * MODULE_CONFIG_START
 * MODULE_CONFIG_STOP
 * ADD_CATEGORY_HINT( N_(text), longtext )
 * ADD_SUBCATEGORY_HINT( N_(text), longtext )
 * ADD_STRING( option_name, value, p_callback, N_(text), N_(longtext) )
 * ADD_FILE( option_name, psz_value, p_callback, N_(text), N_(longtext) )
 * ADD_PLUGIN( option_name, psz_value, i_capability, p_callback,
 *             N_(text), N_(longtext) )
 * ADD_INTEGER( option_name, i_value, p_callback, N_(text), N_(longtext) )
 * ADD_BOOL( option_name, p_callback, N_(text), N_(longtext) )
 */

MODULE_CONFIG_START

/* Interface options */
ADD_CATEGORY_HINT( N_("Interface"), NULL )
ADD_PLUGIN  ( "intf", MODULE_CAPABILITY_INTF, NULL, NULL, INTF_TEXT, INTF_LONGTEXT )
ADD_INTEGER ( "warning", 0, NULL, WARNING_TEXT, WARNING_LONGTEXT )
ADD_BOOL    ( "stats", NULL, STATS_TEXT, STATS_LONGTEXT )
ADD_STRING  ( "search_path", NULL, NULL, INTF_PATH_TEXT, INTF_PATH_LONGTEXT )

/* Audio options */
ADD_CATEGORY_HINT( N_("Audio"), NULL)
ADD_PLUGIN  ( "aout", MODULE_CAPABILITY_AOUT, NULL, NULL, AOUT_TEXT, AOUT_LONGTEXT )
ADD_BOOL    ( "noaudio", NULL, NOAUDIO_TEXT, NOAUDIO_LONGTEXT )
ADD_BOOL    ( "mono", NULL, MONO_TEXT, MONO_LONGTEXT )
ADD_INTEGER ( "volume", VOLUME_DEFAULT, NULL, VOLUME_TEXT, VOLUME_LONGTEXT )
ADD_INTEGER ( "rate", 44100, NULL, RATE_TEXT, RATE_LONGTEXT )
ADD_INTEGER ( "desync", 0, NULL, DESYNC_TEXT, DESYNC_LONGTEXT )
ADD_INTEGER ( "aout_format", 0, NULL, FORMAT_TEXT,
              FORMAT_LONGTEXT )

/* Video options */
ADD_CATEGORY_HINT( N_("Video"), NULL )
ADD_PLUGIN  ( "vout", MODULE_CAPABILITY_VOUT, NULL, NULL, VOUT_TEXT, VOUT_LONGTEXT )
ADD_BOOL    ( "novideo", NULL, NOVIDEO_TEXT, NOVIDEO_LONGTEXT )
ADD_INTEGER ( "width", -1, NULL, WIDTH_TEXT, WIDTH_LONGTEXT )
ADD_INTEGER ( "height", -1, NULL, HEIGHT_TEXT, HEIGHT_LONGTEXT )
ADD_FLOAT   ( "zoom", 1, NULL, ZOOM_TEXT, ZOOM_LONGTEXT )
ADD_BOOL    ( "grayscale", NULL, GRAYSCALE_TEXT, GRAYSCALE_LONGTEXT )
ADD_BOOL    ( "fullscreen", NULL, FULLSCREEN_TEXT, FULLSCREEN_LONGTEXT )
ADD_BOOL    ( "nooverlay", NULL, NOOVERLAY_TEXT, NOOVERLAY_LONGTEXT )
ADD_INTEGER ( "spumargin", -1, NULL, SPUMARGIN_TEXT, SPUMARGIN_LONGTEXT )
ADD_PLUGIN  ( "filter", MODULE_CAPABILITY_VOUT, NULL, NULL, FILTER_TEXT, FILTER_LONGTEXT )

/* Input options */
ADD_CATEGORY_HINT( N_("Input"), NULL )
ADD_INTEGER ( "server_port", 1234, NULL, SERVER_PORT_TEXT, SERVER_PORT_LONGTEXT )
ADD_BOOL    ( "network_channel", NULL, NETCHANNEL_TEXT, NETCHANNEL_LONGTEXT )
ADD_STRING  ( "channel_server", "localhost", NULL, CHAN_SERV_TEXT, CHAN_SERV_LONGTEXT )
ADD_INTEGER ( "channel_port", 6010, NULL, CHAN_PORT_TEXT, CHAN_PORT_LONGTEXT )
ADD_STRING  ( "iface", "eth0", NULL, IFACE_TEXT, IFACE_LONGTEXT )

ADD_INTEGER ( "input_program", 0, NULL, INPUT_PROGRAM_TEXT,
        INPUT_PROGRAM_LONGTEXT )
ADD_INTEGER ( "input_audio", -1, NULL, INPUT_AUDIO_TEXT, INPUT_AUDIO_LONGTEXT )
ADD_INTEGER ( "input_channel", -1, NULL, INPUT_CHAN_TEXT, INPUT_CHAN_LONGTEXT )
ADD_INTEGER ( "input_subtitle", -1, NULL, INPUT_SUBT_TEXT, INPUT_SUBT_LONGTEXT )

ADD_STRING  ( "dvd_device", "/dev/dvd", NULL, DVD_DEV_TEXT, DVD_DEV_LONGTEXT )
ADD_STRING  ( "vcd_device", "/dev/cdrom", NULL, VCD_DEV_TEXT, VCD_DEV_LONGTEXT )
#ifdef HAVE_SATELLITE
ADD_INTEGER ( "sat_frequency", 11954, NULL, SAT_FREQ_TEXT, SAT_FREQ_LONGTEXT )
ADD_INTEGER ( "sat_polarization", 0, NULL, SAT_POL_TEXT, SAT_POL_LONGTEXT )
ADD_INTEGER ( "sat_fec", 3, NULL, SAT_FEC_TEXT, SAT_FEC_LONGTEXT )
ADD_INTEGER ( "sat_symbol_rate", 27500, NULL, SAT_SRATE_TEXT,
            SAT_SRATE_LONGTEXT )
ADD_BOOL    ( "sat_diseqc", 0, SAT_DISEQC_TEXT, SAT_DISEQC_LONGTEXT )
ADD_INTEGER ( "sat_lnb_lof1", 10000, NULL, SAT_LNB_LOF1_TEXT, 
            SAT_LNB_LOF1_LONGTEXT )
ADD_INTEGER ( "sat_lnb_lof2", 10000, NULL, SAT_LNB_LOF2_TEXT, 
            SAT_LNB_LOF2_LONGTEXT )
ADD_INTEGER ( "sat_lnb_slof", 11700, NULL, SAT_LNB_SLOF_TEXT, 
            SAT_LNB_SLOF_LONGTEXT )
#endif

ADD_BOOL    ( "ipv6", NULL, IPV6_TEXT, IPV6_LONGTEXT )
ADD_BOOL    ( "ipv4", NULL, IPV4_TEXT, IPV4_LONGTEXT )

/* Decoder options */
ADD_CATEGORY_HINT( N_("Decoders"), NULL )
ADD_PLUGIN  ( "mpeg_adec", MODULE_CAPABILITY_DECODER, NULL, NULL, ADEC_MPEG_TEXT, ADEC_MPEG_LONGTEXT )
ADD_PLUGIN  ( "ac3_adec", MODULE_CAPABILITY_DECODER, NULL, NULL, ADEC_AC3_TEXT, ADEC_AC3_LONGTEXT )
ADD_INTEGER ( "vdec_smp", 0, NULL, VDEC_SMP_TEXT, VDEC_SMP_LONGTEXT )
ADD_STRING  ( "vpar_synchro", NULL, NULL, VPAR_SYNCHRO_TEXT, VPAR_SYNCHRO_LONGTEXT )

/* CPU options */
ADD_CATEGORY_HINT( N_("CPU"), NULL )
ADD_BOOL    ( "nommx", NULL, NOMMX_TEXT, NOMMX_LONGTEXT )
ADD_BOOL    ( "no3dn", NULL, NO3DN_TEXT, NO3DN_LONGTEXT )
ADD_BOOL    ( "nommxext", NULL, NOMMXEXT_TEXT, NOMMXEXT_LONGTEXT )
ADD_BOOL    ( "nosse", NULL, NOSSE_TEXT, NOSSE_LONGTEXT )
ADD_BOOL    ( "noaltivec", NULL, NOALTIVEC_TEXT, NOALTIVEC_LONGTEXT )

/* Playlist options */
ADD_CATEGORY_HINT( N_("Playlist"), NULL )
ADD_BOOL    ( "playlist_launch", NULL, PLAYLIST_LAUNCH_TEXT, PLAYLIST_LAUNCH_LONGTEXT )
ADD_BOOL    ( "playlist_enqueue", NULL, PLAYLIST_ENQUEUE_TEXT, PLAYLIST_ENQUEUE_LONGTEXT )
ADD_BOOL    ( "playlist_loop", NULL, PLAYLIST_LOOP_TEXT, PLAYLIST_LOOP_LONGTEXT )

/* Misc options */
ADD_CATEGORY_HINT( N_("Miscellaneous"), NULL )
ADD_PLUGIN  ( "memcpy", MODULE_CAPABILITY_MEMCPY, NULL, NULL, MEMCPY_TEXT, MEMCPY_LONGTEXT )

#if defined(WIN32)
ADD_BOOL    ( "fast_pthread", NULL, FAST_PTHREAD_TEXT, FAST_PTHREAD_LONGTEXT )
#endif

MODULE_CONFIG_STOP

MODULE_INIT_START
    SET_DESCRIPTION( N_("main program") )
    ADD_CAPABILITY( MAIN, 100/*whatever*/ )
MODULE_INIT_STOP

MODULE_ACTIVATE_START
MODULE_ACTIVATE_STOP

MODULE_DEACTIVATE_START
MODULE_DEACTIVATE_STOP

/* Hack for help options */
static module_t help_module;
static module_config_t p_help_config[] = {
    { MODULE_CONFIG_ITEM_BOOL, "help", N_("print help (or use -h)"),
      NULL, NULL, 0, 0, NULL, NULL, 0 },
    { MODULE_CONFIG_ITEM_BOOL, "longhelp", N_("print detailed help (or use -H)"),
      NULL, NULL, 0, 0, NULL, NULL, 0 },
    { MODULE_CONFIG_ITEM_BOOL, "list", N_("print a list of available plugins "
      "(or use -l)"), NULL, NULL, 0, 0, NULL, NULL, 0 },
    { MODULE_CONFIG_ITEM_STRING, "plugin", N_("print help on plugin "
      "(or use -p)"), NULL, NULL, 0, 0, NULL, &help_module.config_lock, 0 },
    { MODULE_CONFIG_ITEM_BOOL, "version", N_("print version information"),
      NULL, NULL, 0, 0, NULL, NULL, 0 },
    { MODULE_CONFIG_HINT_END, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, 0 } };

/*****************************************************************************
 * End configuration.
 *****************************************************************************/

/*****************************************************************************
 * Global variables - these are the only ones, see main.h and modules.h
 *****************************************************************************/
main_t        *p_main;
p_main_sys_t  p_main_sys;
module_bank_t *p_module_bank;
input_bank_t  *p_input_bank;
aout_bank_t   *p_aout_bank;
vout_bank_t   *p_vout_bank;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
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
    char *psz_plugin;
    char *p_tmp;
    struct module_config_s *p_item;

    p_main        = &main_data;               /* set up the global variables */
    p_module_bank = &module_bank;
    p_input_bank  = &input_bank;
    p_aout_bank   = &aout_bank;
    p_vout_bank   = &vout_bank;

    p_main->i_warning_level = 0;

    /*
     * Support for gettext
     */
#if defined( ENABLE_NLS ) && defined ( HAVE_GETTEXT )
#   if defined( HAVE_LOCALE_H ) && defined( HAVE_LC_MESSAGES )
    if( !setlocale( LC_MESSAGES, "" ) )
    {
        fprintf( stderr, "warning: unsupported locale settings\n" );
    }

    setlocale( LC_CTYPE, "" );
#   endif

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
    vlc_threads_init();

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
    putenv( "GNOME_DISABLE_CRASH_DIALOG=1" );
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

    /* Hack: insert the help module here */
    help_module.psz_name = "help";
    help_module.i_config_items =
                    sizeof(p_help_config) / sizeof(module_config_t) - 1;
    vlc_mutex_init( &help_module.config_lock );
    help_module.p_config = config_Duplicate( p_help_config );
    for( p_item = help_module.p_config;
         p_item->i_type != MODULE_CONFIG_HINT_END;
         p_item++ )
    {
        p_item->p_lock = &help_module.config_lock;
    }
    help_module.next = p_module_bank->first;
    p_module_bank->first = &help_module;
    /* end hack */

    if( config_LoadCmdLine( &i_argc, ppsz_argv, 1 ) )
    {
        intf_MsgDestroy();
        return( errno );
    }

    /* Check for short help option */
    if( config_GetIntVariable( "help" ) )
    {
        intf_Msg( _("Usage: %s [options] [parameters] [file]...\n"),
                    p_main->psz_arg0 );

        Usage( "help" );
        Usage( "main" );
        return( -1 );
    }

    /* Check for version option */
    if( config_GetIntVariable( "version" ) )
    {
        Version();
        return( -1 );
    }

    /* Hack: remove the help module here */
    p_module_bank->first = help_module.next;
    /* end hack */

    /*
     * Load the builtins and plugins into the module_bank.
     * We have to do it before config_Load*() because this also gets the
     * list of configuration options exported by each plugin and loads their
     * default values.
     */
    module_LoadBuiltins();
    module_LoadPlugins();
    intf_WarnMsg( 2, "module: module bank initialized, found %i modules",
                  p_module_bank->i_count );

    /* Hack: insert the help module here */
    help_module.next = p_module_bank->first;
    p_module_bank->first = &help_module;
    /* end hack */

    /* Check for help on plugins */
    if( (p_tmp = config_GetPszVariable( "plugin" )) )
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

    /* Hack: remove the help module here */
    p_module_bank->first = help_module.next;
    /* end hack */


    /*
     * Override default configuration with config file settings
     */
    vlc_mutex_init( &p_main->config_lock );
    p_main->psz_homedir = config_GetHomeDir();
    config_LoadConfigFile( NULL );

    /*
     * Override configuration with command line settings
     */
    if( config_LoadCmdLine( &i_argc, ppsz_argv, 0 ) )
    {
#ifdef WIN32
        ShowConsole();
        /* Pause the console because it's destroyed when we exit */
        intf_Msg( "The command line options couldn't be loaded, check that "
                  "they are valid.\nPress the RETURN key to continue..." );
        getchar();
#endif
        intf_MsgDestroy();
        return( errno );
    }


    /*
     * System specific configuration
     */
#if defined( WIN32 )
    system_Configure();
#endif

    /* p_main inititalization. FIXME ? */
    p_main->i_desync = (mtime_t)config_GetIntVariable( "desync" )
      * (mtime_t)1000;
    p_main->b_stats = config_GetIntVariable( "stats" );
    p_main->b_audio = !config_GetIntVariable( "noaudio" );
    p_main->b_stereo= !config_GetIntVariable( "mono" );
    p_main->b_video = !config_GetIntVariable( "novideo" );
    if( config_GetIntVariable( "nommx" ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_MMX;
    if( config_GetIntVariable( "no3dn" ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_3DNOW;
    if( config_GetIntVariable( "nommxext" ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_MMXEXT;
    if( config_GetIntVariable( "nosse" ) )
        p_main->i_cpu_capabilities &= ~CPU_CAPABILITY_SSE;
    if( config_GetIntVariable( "noaltivec" ) )
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
    psz_plugin = config_GetPszVariable( "memcpy" );
    p_main->p_memcpy_module = module_Need( MODULE_CAPABILITY_MEMCPY,
                                           psz_plugin, NULL );
    if( psz_plugin ) free( psz_plugin );
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
    if( config_GetIntVariable( "network_channel" ) &&
        network_ChannelCreate() )
    {
        /* On error during Channels initialization, switch off channels */
        intf_ErrMsg( "intf error: channels initialization failed, "
                                 "deactivating channels" );
        config_PutIntVariable( "network_channel", 0 );
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
        if( config_GetIntVariable( "network_channel" ) && p_main->p_channel )
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
    module_t *p_module;
    module_config_t *p_item;
    char psz_spaces[30];

    memset( psz_spaces, 32, 30 );

#ifdef WIN32
    ShowConsole();
#endif

    /* Enumerate the config of each module */
    for( p_module = p_module_bank->first ;
         p_module != NULL ;
         p_module = p_module->next )
    {

        if( psz_module_name && strcmp( psz_module_name, p_module->psz_name ) )
            continue;

        /* ignore plugins without config options */
        if( !p_module->i_config_items ) continue;

        /* print module name */
        intf_Msg( _("%s module options:\n"), p_module->psz_name );

        for( p_item = p_module->p_config;
             p_item->i_type != MODULE_CONFIG_HINT_END;
             p_item++ )
        {
            int i;

            switch( p_item->i_type )
            {
            case MODULE_CONFIG_HINT_CATEGORY:
                intf_Msg( " %s", p_item->psz_text );
                break;

            case MODULE_CONFIG_ITEM_STRING:
            case MODULE_CONFIG_ITEM_FILE:
            case MODULE_CONFIG_ITEM_PLUGIN:
                /* Nasty hack, but right now I'm too tired to think about
                 * a nice solution */
                i = 25 - strlen( p_item->psz_name )
                    - strlen(_(" <string>")) - 1;
                if( i < 0 ) i = 0; psz_spaces[i] = 0;

                intf_Msg( "  --%s%s%s %s", p_item->psz_name,
                          _(" <string>"), psz_spaces, p_item->psz_text );
                psz_spaces[i] = 32;
                break;
            case MODULE_CONFIG_ITEM_INTEGER:
                /* Nasty hack, but right now I'm too tired to think about
                 * a nice solution */
                i = 25 - strlen( p_item->psz_name )
                    - strlen(_(" <integer>")) - 1;
                if( i < 0 ) i = 0; psz_spaces[i] = 0;

                intf_Msg( "  --%s%s%s %s", p_item->psz_name,
                          _(" <integer>"), psz_spaces, p_item->psz_text );
                psz_spaces[i] = 32;
                break;
            case MODULE_CONFIG_ITEM_FLOAT:
                /* Nasty hack, but right now I'm too tired to think about
                 * a nice solution */
                i = 25 - strlen( p_item->psz_name )
                    - strlen(_(" <float>")) - 1;
                if( i < 0 ) i = 0; psz_spaces[i] = 0;

                intf_Msg( "  --%s%s%s %s", p_item->psz_name,
                          _(" <float>"), psz_spaces, p_item->psz_text );
                psz_spaces[i] = 32;
                break;
            case MODULE_CONFIG_ITEM_BOOL:
                /* Nasty hack, but right now I'm too tired to think about
                 * a nice solution */
                i = 25 - strlen( p_item->psz_name ) - 1;
                if( i < 0 ) i = 0; psz_spaces[i] = 0;

                intf_Msg( "  --%s%s %s",
                          p_item->psz_name, psz_spaces, p_item->psz_text );
                psz_spaces[i] = 32;
                break;
            }
        }

        /* Yet another nasty hack.
         * Maybe we could use MODULE_CONFIG_ITEM_END to display tail messages
         * for each module?? */
        if( !strcmp( "main", p_module->psz_name ) )
        {
            intf_Msg( _("\nPlaylist items:"
                "\n  *.mpg, *.vob                   \tplain MPEG-1/2 files"
                "\n  [dvd:][device][@raw_device][@[title][,[chapter][,angle]]]"
                "\n                                 \tDVD device"
                "\n  [vcd:][device][@[title][,[chapter]]"
                "\n                                 \tVCD device"
                "\n  udpstream:[@[<bind address>][:<bind port>]]"
                "\n                                 \tUDP stream sent by VLS"
                "\n  vlc:loop                       \tloop execution of the "
                      "playlist"
                "\n  vlc:pause                      \tpause execution of "
                      "playlist items"
                "\n  vlc:quit                       \tquit VLC") );
        }

        intf_Msg( "" );

    }

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
        intf_Msg( _("\nPress the RETURN key to continue...") );
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
    intf_Msg( _("Usage: %s [options] [parameters] [file]...\n"),
              p_main->psz_arg0 );

    intf_Msg( _("[plugin]              [description]") );

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
        intf_Msg( _("\nPress the RETURN key to continue...") );
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

    intf_Msg( VERSION_MESSAGE );
    intf_Msg(
      _("This program comes with NO WARRANTY, to the extent permitted by "
        "law.\nYou may redistribute it under the terms of the GNU General "
        "Public License;\nsee the file named COPYING for details.\n"
        "Written by the VideoLAN team at Ecole Centrale, Paris.") );

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
        intf_Msg( _("\nPress the RETURN key to continue...") );
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

#if defined( __i386__ )
    fprintf( stderr, "warning: your CPU has %s instructions, but not your "
                     "operating system.\n", psz_capability );
    fprintf( stderr, "         some optimizations will be disabled unless "
                     "you upgrade your OS\n" );
#   if defined( SYS_LINUX )
    fprintf( stderr, "         (for instance Linux kernel 2.4.x or later)\n" );
#   endif
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

#elif defined( __sparc__ )

    i_capabilities |= CPU_CAPABILITY_FPU;
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
