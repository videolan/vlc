/*****************************************************************************
 * main.c: main vlc source
 * Includes the main() function for vlc. Parses command line, start interface
 * and spawn threads.
 *****************************************************************************
 * Copyright (C) 1998-2001 VideoLAN
 * $Id: main.c,v 1.135 2001/12/10 13:17:35 sam Exp $
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
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
#include <setjmp.h>                                       /* longjmp, setjmp */

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

#include <errno.h>                                                 /* ENOMEM */
#include <stdlib.h>                                  /* getenv(), strtol(),  */
#include <string.h>                                            /* strerror() */
#include <fcntl.h>                                       /* open(), O_WRONLY */
#include <sys/stat.h>                                             /* S_IREAD */

#include "common.h"
#include "debug.h"
#include "intf_msg.h"
#include "threads.h"
#include "mtime.h"
#include "tests.h"                                              /* TestCPU() */
#include "modules.h"

#include "stream_control.h"
#include "input_ext-intf.h"

#include "intf_playlist.h"
#include "interface.h"

#include "audio_output.h"

#include "video.h"
#include "video_output.h"

#ifdef SYS_BEOS
#   include "beos_specific.h"
#endif

#ifdef SYS_DARWIN
#   include "darwin_specific.h"
#endif

#ifdef WIN32
#   include "win32_specific.h"
#endif

#include "netutils.h"                                 /* network_ChannelJoin */

/*****************************************************************************
 * Command line options constants. If something is changed here, be sure that
 * GetConfiguration and Usage are also changed.
 *****************************************************************************/

/* Long options return values - note that values corresponding to short options
 * chars, and in general any regular char, should be avoided */
#define OPT_NOAUDIO             150
#define OPT_STEREO              151
#define OPT_MONO                152
#define OPT_SPDIF               153
#define OPT_VOLUME              154
#define OPT_DESYNC              155

#define OPT_NOVIDEO             160
#define OPT_DISPLAY             161
#define OPT_WIDTH               162
#define OPT_HEIGHT              163
#define OPT_COLOR               164
#define OPT_FULLSCREEN          165
#define OPT_OVERLAY             166
#define OPT_XVADAPTOR           167
#define OPT_SMP                 168

#define OPT_CHANNELS            170
#define OPT_SERVER              171
#define OPT_PORT                172
#define OPT_BROADCAST           173
#define OPT_CHANNELSERVER       174

#define OPT_INPUT               180
#define OPT_MOTION              181
#define OPT_IDCT                182
#define OPT_YUV                 183
#define OPT_DOWNMIX             184
#define OPT_IMDCT               185
#define OPT_MEMCPY              186
#define OPT_DVDCSS_METHOD       187
#define OPT_DVDCSS_VERBOSE      188

#define OPT_SYNCHRO             190
#define OPT_WARNING             191
#define OPT_VERSION             192
#define OPT_STDOUT              193
#define OPT_STATS               194

#define OPT_MPEG_ADEC           200

/* Usage fashion */
#define USAGE                     0
#define SHORT_HELP                1
#define LONG_HELP                 2

/* Needed for x86 CPU capabilities detection */
#define cpuid( a )                 \
    asm volatile ( "cpuid"         \
                 : "=a" ( i_eax ), \
                   "=b" ( i_ebx ), \
                   "=c" ( i_ecx ), \
                   "=d" ( i_edx )  \
                 : "a"  ( a )      \
                 : "cc" );

/* Long options */
static const struct option longopts[] =
{
    /*  name,               has_arg,    flag,   val */

    /* General/common options */
    {   "help",             0,          0,      'h' },
    {   "longhelp",         0,          0,      'H' },
    {   "version",          0,          0,      OPT_VERSION },

    /* Interface options */
    {   "intf",             1,          0,      'I' },
    {   "warning",          1,          0,      OPT_WARNING },
    {   "stdout",           1,          0,      OPT_STDOUT },
    {   "stats",            0,          0,      OPT_STATS },

    /* Audio options */
    {   "noaudio",          0,          0,      OPT_NOAUDIO },
    {   "aout",             1,          0,      'A' },
    {   "stereo",           0,          0,      OPT_STEREO },
    {   "mono",             0,          0,      OPT_MONO },
    {   "spdif",            0,          0,      OPT_SPDIF },
    {   "downmix",          1,          0,      OPT_DOWNMIX },
    {   "imdct",            1,          0,      OPT_IMDCT },
    {   "volume",           1,          0,      OPT_VOLUME },
    {   "desync",           1,          0,      OPT_DESYNC },

    /* Video options */
    {   "novideo",          0,          0,      OPT_NOVIDEO },
    {   "vout",             1,          0,      'V' },
    {   "display",          1,          0,      OPT_DISPLAY },
    {   "width",            1,          0,      OPT_WIDTH },
    {   "height",           1,          0,      OPT_HEIGHT },
    {   "grayscale",        0,          0,      'g' },
    {   "color",            0,          0,      OPT_COLOR },
    {   "motion",           1,          0,      OPT_MOTION },
    {   "idct",             1,          0,      OPT_IDCT },
    {   "yuv",              1,          0,      OPT_YUV },
    {   "fullscreen",       0,          0,      OPT_FULLSCREEN },
    {   "overlay",          0,          0,      OPT_OVERLAY },
    {   "xvadaptor",        1,          0,      OPT_XVADAPTOR },
    {   "smp",              1,          0,      OPT_SMP },

    /* DVD options */
    {   "dvdtitle",         1,          0,      't' },
    {   "dvdchapter",       1,          0,      'T' },
    {   "dvdangle",         1,          0,      'u' },
    {   "dvdaudio",         1,          0,      'a' },
    {   "dvdchannel",       1,          0,      'c' },
    {   "dvdsubtitle",      1,          0,      's' },
    {   "dvdcss-method",    1,          0,      OPT_DVDCSS_METHOD },
    {   "dvdcss-verbose",   1,          0,      OPT_DVDCSS_VERBOSE },
    
    /* Input options */
    {   "input",            1,          0,      OPT_INPUT },
    {   "channels",         0,          0,      OPT_CHANNELS },
    {   "channelserver",    1,          0,      OPT_CHANNELSERVER },

    /* Misc options */
    {   "synchro",          1,          0,      OPT_SYNCHRO },
    {   "memcpy",           1,          0,      OPT_MEMCPY },

    /* Decoder options */
    {   "mpeg_adec",        1,          0,      OPT_MPEG_ADEC },

    {   0,                  0,          0,      0 }
};

/* Short options */
static const char *psz_shortopts = "hHvgt:T:u:a:s:c:I:A:V:";

/*****************************************************************************
 * Global variables - these are the only ones, see main.h and modules.h
 *****************************************************************************/
main_t        *p_main;
module_bank_t *p_module_bank;
aout_bank_t   *p_aout_bank;
vout_bank_t   *p_vout_bank;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static int  GetConfiguration        ( int *pi_argc, char *ppsz_argv[],
                                      char *ppsz_env[] );
static int  GetFilenames            ( int i_argc, char *ppsz_argv[] );
static void Usage                   ( int i_fashion );
static void Version                 ( void );

static void InitSignalHandler       ( void );
static void SimpleSignalHandler     ( int i_signal );
static void FatalSignalHandler      ( int i_signal );
static void InstructionSignalHandler( int i_signal );
static int  CPUCapabilities         ( void );

static int  RedirectSTDOUT          ( void );
static void ShowConsole             ( void );

static jmp_buf env;
static int  i_illegal;

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
    aout_bank_t   aout_bank;
    vout_bank_t   vout_bank;

    p_main        = &main_data;               /* set up the global variables */
    p_module_bank = &module_bank;
    p_aout_bank   = &aout_bank;
    p_vout_bank   = &vout_bank;

#ifdef ENABLE_NLS
    /* 
     * Support for getext
     */
    if( ! setlocale(LC_MESSAGES, "") )
    {
        fprintf( stderr, "warning: unsupported locale.\n" );
    }

    if( ! bindtextdomain(PACKAGE, LOCALEDIR) )
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
    main_PutIntVariable( "MALLOC_CHECK_", 2 );
#   endif
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

    intf_MsgImm( COPYRIGHT_MESSAGE "\n" );

    /*
     * Read configuration
     */
    if( GetConfiguration( &i_argc, ppsz_argv, ppsz_env ) ) /* parse cmd line */
    {
        intf_MsgDestroy();
        return( errno );
    }

    /*
     * Redirect the standard output if required by the user, and on Win32 we
     * also open a console to display the debug messages.
     */
    RedirectSTDOUT();

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
        intf_StatMsg("info: CPU has capabilities %s", p_capabilities );
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
     * Initialize module, aout and vout banks
     */
    module_InitBank();
    aout_InitBank();
    vout_InitBank();

    /*
     * Choose the best memcpy module
     */
    p_main->p_memcpy_module = module_Need( MODULE_CAPABILITY_MEMCPY, NULL );

    if( p_main->p_memcpy_module != NULL )
    {
#define f p_main->p_memcpy_module->p_functions->memcpy.functions.memcpy
        p_main->fast_memcpy = f.fast_memcpy;
#undef f
    }
    else
    {
        intf_ErrMsg( "intf error: no suitable memcpy module, "
                     "using libc default" );
        p_main->fast_memcpy = memcpy;
    }

    /*
     * Initialize shared resources and libraries
     */
    if( main_GetIntVariable( INPUT_NETWORK_CHANNEL_VAR,
                             INPUT_NETWORK_CHANNEL_DEFAULT ) &&
        network_ChannelCreate() )
    {
        /* On error during Channels initialization, switch off channels */
        intf_Msg( "Channels initialization failed : "
                  "Channel management is deactivated" );
        main_PutIntVariable( INPUT_NETWORK_CHANNEL_VAR, 0 );
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
        if( main_GetIntVariable( INPUT_NETWORK_CHANNEL_VAR,
                                 INPUT_NETWORK_CHANNEL_DEFAULT ) )
        {
            network_ChannelJoin( COMMON_CHANNEL );
        }
    }

    /*
     * Free memcpy module
     */
    if( p_main->p_memcpy_module != NULL )
    {
        module_Unneed( p_main->p_memcpy_module );
    }

    /*
     * Free module, aout and vout banks
     */
    vout_EndBank();
    aout_EndBank();
    module_EndBank();

    /*
     * Free playlist
     */
    intf_PlaylistDestroy( p_main->p_playlist );

    /*
     * System specific cleaning code
     */
#if defined( SYS_BEOS ) || defined( SYS_DARWIN ) || defined( WIN32 )
    system_End();
#endif


    /*
     * Terminate messages interface and program
     */
    intf_Msg( "intf: program terminated" );
    intf_MsgDestroy();

    /*
     * Stop threads system
     */
    vlc_threads_end( );

    return 0;
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
 * GetConfiguration: parse command line
 *****************************************************************************
 * Parse command line and configuration file for configuration. If the inline
 * help is requested, the function Usage() is called and the function returns
 * -1 (causing main() to exit). The messages interface is initialized at this
 * stage, but most structures are not allocated, so only environment should
 * be used.
 *****************************************************************************/
static int GetConfiguration( int *pi_argc, char *ppsz_argv[], char *ppsz_env[] )
{
    int   i_cmd;
    char *p_tmp;

    /* Set default configuration and copy arguments */
    p_main->i_argc    = *pi_argc;
    p_main->ppsz_argv = ppsz_argv;
    p_main->ppsz_env  = ppsz_env;

    p_main->b_audio     = 1;
    p_main->b_video     = 1;

    p_main->i_warning_level = 0;
    p_main->b_stats = 0;
    p_main->i_desync = 0; /* No desynchronization by default */

    p_main->p_channel = NULL;

    /* Get the executable name (similar to the basename command) */
    p_main->psz_arg0 = p_tmp = ppsz_argv[ 0 ];
    while( *p_tmp )
    {
        if( *p_tmp == '/' )
        {
            p_main->psz_arg0 = ++p_tmp;
        }
        else
        {
            ++p_tmp;
        }
    }

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

    /* Parse command line options */
    opterr = 0;
    while( ( i_cmd = getopt_long( *pi_argc, ppsz_argv,
                                   psz_shortopts, longopts, 0 ) ) != EOF )
    {
        switch( i_cmd )
        {
        /* General/common options */
        case 'h':                                              /* -h, --help */
            ShowConsole();
            RedirectSTDOUT();
            Usage( SHORT_HELP );
#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
            if( strcmp( "", main_GetPszVariable( INTF_STDOUT_VAR,
                                                 INTF_STDOUT_DEFAULT ) ) == 0 )
            {
                /* No stdout redirection has been asked for */
                intf_MsgImm( "\nPress the RETURN key to continue..." );
                getchar();
            }
#endif
            return( -1 );
            break;
        case 'H':                                          /* -H, --longhelp */
            ShowConsole();
            RedirectSTDOUT();
            Usage( LONG_HELP );
#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
            if( strcmp( "", main_GetPszVariable( INTF_STDOUT_VAR,
                                                 INTF_STDOUT_DEFAULT ) ) == 0 )
            {
                /* No stdout redirection has been asked for */
                intf_MsgImm( "\nPress the RETURN key to continue..." );
                getchar();
            }
#endif
            return( -1 );
            break;
        case OPT_VERSION:                                       /* --version */
            ShowConsole();
            RedirectSTDOUT();
            Version();
#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
            if( strcmp( "", main_GetPszVariable( INTF_STDOUT_VAR,
                                                 INTF_STDOUT_DEFAULT ) ) == 0 )
            {
                /* No stdout redirection has been asked for */
                intf_MsgImm( "\nPress the RETURN key to continue..." );
                getchar();
            }
#endif
            return( -1 );
            break;
        case 'v':                                           /* -v, --verbose */
            p_main->i_warning_level++;
            break;

        /* Interface warning messages level */
        case 'I':                                              /* -I, --intf */
            main_PutPszVariable( INTF_METHOD_VAR, optarg );
            break;
        case OPT_WARNING:                                       /* --warning */
            intf_ErrMsg( "intf error: `--warning' is deprecated, use `-v'" );
            p_main->i_warning_level = atoi(optarg);
            break;

        case OPT_STDOUT:                                         /* --stdout */
            main_PutPszVariable( INTF_STDOUT_VAR, optarg );
            break;

        case OPT_STATS:
            p_main->b_stats = 1;
            break;

        /* Audio options */
        case OPT_NOAUDIO:                                       /* --noaudio */
            p_main->b_audio = 0;
            break;
        case 'A':                                              /* -A, --aout */
            main_PutPszVariable( AOUT_METHOD_VAR, optarg );
            break;
        case OPT_STEREO:                                         /* --stereo */
            main_PutIntVariable( AOUT_STEREO_VAR, 1 );
            break;
        case OPT_MONO:                                             /* --mono */
            main_PutIntVariable( AOUT_STEREO_VAR, 0 );
            break;
        case OPT_SPDIF:                                           /* --spdif */
            main_PutIntVariable( AOUT_SPDIF_VAR, 1 );
            break;
        case OPT_DOWNMIX:                                       /* --downmix */
            main_PutPszVariable( DOWNMIX_METHOD_VAR, optarg );
            break;
        case OPT_IMDCT:                                           /* --imdct */
            main_PutPszVariable( IMDCT_METHOD_VAR, optarg );
            break;
        case OPT_VOLUME:                                         /* --volume */
            main_PutIntVariable( AOUT_VOLUME_VAR, atoi(optarg) );
            break;
        case OPT_DESYNC:                                         /* --desync */
            p_main->i_desync = atoi(optarg);
            break;

        /* Video options */
        case OPT_NOVIDEO:                                       /* --novideo */
            p_main->b_video = 0;
            break;
        case 'V':                                              /* -V, --vout */
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
        case OPT_FULLSCREEN:                                 /* --fullscreen */
            main_PutIntVariable( VOUT_FULLSCREEN_VAR, 1 );
            break;
        case OPT_OVERLAY:                                       /* --overlay */
            main_PutIntVariable( VOUT_OVERLAY_VAR, 1 );
            break;
        case OPT_XVADAPTOR:                                   /* --xvadaptor */
            main_PutIntVariable( VOUT_XVADAPTOR_VAR, atoi(optarg) );
            break;
        case OPT_MOTION:                                         /* --motion */
            main_PutPszVariable( MOTION_METHOD_VAR, optarg );
            break;
        case OPT_IDCT:                                             /* --idct */
            main_PutPszVariable( IDCT_METHOD_VAR, optarg );
            break;
        case OPT_YUV:                                               /* --yuv */
            main_PutPszVariable( YUV_METHOD_VAR, optarg );
            break;
        case OPT_SMP:                                               /* --smp */
            main_PutIntVariable( VDEC_SMP_VAR, atoi(optarg) );
            break;

        /* DVD options */
        case 't':                                              /* --dvdtitle */
            main_PutIntVariable( INPUT_TITLE_VAR, atoi(optarg) );
            break;
        case 'T':                                            /* --dvdchapter */
            main_PutIntVariable( INPUT_CHAPTER_VAR, atoi(optarg) );
            break;
        case 'u':                                              /* --dvdangle */
            main_PutIntVariable( INPUT_ANGLE_VAR, atoi(optarg) );
            break;
        case 'a':                                              /* --dvdaudio */
            if ( ! strcmp(optarg, "ac3") )
                main_PutIntVariable( INPUT_AUDIO_VAR, REQUESTED_AC3 );
            else if ( ! strcmp(optarg, "lpcm") )
                main_PutIntVariable( INPUT_AUDIO_VAR, REQUESTED_LPCM );
            else if ( ! strcmp(optarg, "mpeg") )
                main_PutIntVariable( INPUT_AUDIO_VAR, REQUESTED_MPEG );
            else
                main_PutIntVariable( INPUT_AUDIO_VAR, REQUESTED_NOAUDIO );
            break;
        case 'c':                                            /* --dvdchannel */
            main_PutIntVariable( INPUT_CHANNEL_VAR, atoi(optarg) );
            break;
        case 's':                                           /* --dvdsubtitle */
            main_PutIntVariable( INPUT_SUBTITLE_VAR, atoi(optarg) );
            break;
        case OPT_DVDCSS_METHOD:                           /* --dvdcss-method */
            main_PutPszVariable( "DVDCSS_METHOD", optarg );
            break;
        case OPT_DVDCSS_VERBOSE:                         /* --dvdcss-verbose */
            main_PutPszVariable( "DVDCSS_VERBOSE", optarg );
            break;

        /* Input options */
        case OPT_INPUT:                                           /* --input */
            main_PutPszVariable( INPUT_METHOD_VAR, optarg );
            break;
        case OPT_CHANNELS:                                     /* --channels */
            main_PutIntVariable( INPUT_NETWORK_CHANNEL_VAR, 1 );
            break;
        case OPT_CHANNELSERVER:                           /* --channelserver */
            main_PutPszVariable( INPUT_CHANNEL_SERVER_VAR, optarg );
            break;

        /* Misc options */
        case OPT_SYNCHRO:                                      
            main_PutPszVariable( VPAR_SYNCHRO_VAR, optarg );
            break;
        case OPT_MEMCPY:                                      
            main_PutPszVariable( MEMCPY_METHOD_VAR, optarg );
            break;
            
        /* Decoder options */
        case OPT_MPEG_ADEC:
            main_PutPszVariable( ADEC_MPEG_VAR, optarg );
            break;

        /* Internal error: unknown option */
        case '?':
        default:
            ShowConsole();
            RedirectSTDOUT();
            intf_ErrMsg( "intf error: unknown option `%s'",
                         ppsz_argv[optind] );
            Usage( USAGE );
#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
            if( strcmp( "", main_GetPszVariable( INTF_STDOUT_VAR,
                                                 INTF_STDOUT_DEFAULT ) ) == 0 )
            {
                /* No stdout redirection has been asked for */
                intf_MsgImm( "\nPress the RETURN key to continue..." );
                getchar();
            }
#endif
            return( EINVAL );
            break;
        }
    }

    if( p_main->i_warning_level < 0 )
    {
        p_main->i_warning_level = 0;
    }

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
static void Usage( int i_fashion )
{
    /* Usage */
    intf_MsgImm( "Usage: %s [options] [parameters] [file]...",
                 p_main->psz_arg0 );

    if( i_fashion == USAGE )
    {
        intf_MsgImm( "Try `%s --help' for more information.",
                     p_main->psz_arg0 );
        return;
    }

    /* Options */
    intf_MsgImm( "\nOptions:"
          "\n  -I, --intf <module>            \tinterface method"
          "\n  -v, --verbose                  \tverbose mode (cumulative)"
          "\n      --stdout <filename>        \tredirect console stdout"
          "\n      --memcpy <module>          \tmemcpy method"
          "\n"
          "\n      --noaudio                  \tdisable audio"
          "\n  -A, --aout <module>            \taudio output method"
          "\n      --stereo, --mono           \tstereo/mono audio"
          "\n      --spdif                    \tAC3 pass-through mode"
          "\n      --downmix <module>         \tAC3 downmix method"
          "\n      --imdct <module>           \tAC3 IMDCT method"
          "\n      --volume [0..1024]         \tVLC output volume"
          "\n      --desync <time in ms>      \tCompensate desynchronization of the audio"
          "\n"
          "\n      --novideo                  \tdisable video"
          "\n  -V, --vout <module>            \tvideo output method"
          "\n      --display <display>        \tdisplay string"
          "\n      --width <w>, --height <h>  \tdisplay dimensions"
          "\n  -g, --grayscale                \tgrayscale output"
          "\n      --fullscreen               \tfullscreen output"
          "\n      --overlay                  \taccelerated display"
          "\n      --xvadaptor <adaptor>      \tXVideo adaptor"
          "\n      --color                    \tcolor output"
          "\n      --motion <module>          \tmotion compensation method"
          "\n      --idct <module>            \tIDCT method"
          "\n      --yuv <module>             \tYUV method"
          "\n      --synchro <type>           \tforce synchro algorithm"
          "\n      --smp <number of threads>  \tuse several processors"
          "\n"
          "\n  -t, --dvdtitle <num>           \tchoose DVD title"
          "\n  -T, --dvdchapter <num>         \tchoose DVD chapter"
          "\n  -u, --dvdangle <num>           \tchoose DVD angle"
          "\n  -a, --dvdaudio <type>          \tchoose DVD audio type"
          "\n  -c, --dvdchannel <channel>     \tchoose DVD audio channel"
          "\n  -s, --dvdsubtitle <channel>    \tchoose DVD subtitle channel"
          "\n      --dvdcss-method <method>   \tselect dvdcss decryption method"
          "\n      --dvdcss-verbose <level>   \tselect dvdcss verbose level"
          "\n"
          "\n      --input                    \tinput method"
          "\n      --channels                 \tenable channels"
          "\n      --channelserver <host>     \tchannel server address"
          "\n"
          "\n      --mpeg_adec <builtin|mad>  \tchoose audio decoder"
          "\n"
          "\n  -h, --help                     \tprint help and exit"
          "\n  -H, --longhelp                 \tprint long help and exit"
          "\n      --version                  \toutput version information and exit"
          "\n\nPlaylist items :"
          "\n  *.mpg, *.vob                   \tPlain MPEG-1/2 files"
          "\n  dvd:<device>[@<raw device>]    \tDVD device"
          "\n  vcd:<device>                   \tVCD device"
          "\n  udpstream:[<server>[:<server port>]][@[<bind address>][:<bind port>]]"
          "\n                                 \tUDP stream sent by VLS"
          "\n  vlc:loop                       \tLoop execution of the playlist"
          "\n  vlc:pause                      \tPause execution of playlist items"
          "\n  vlc:quit                       \tQuit VLC");

    if( i_fashion == SHORT_HELP )
        return;

    /* Interface parameters */
    intf_MsgImm( "\nInterface parameters:"
        "\n  " INTF_METHOD_VAR "=<method name>        \tinterface method"
        "\n  " INTF_INIT_SCRIPT_VAR "=<filename>              \tinitialization script"
        "\n  " INTF_CHANNELS_VAR "=<filename>         \tchannels list"
        "\n  " INTF_STDOUT_VAR "=<filename>           \tredirect console stdout"
        "\n  " MEMCPY_METHOD_VAR "=<method name>      \tmemcpy method" );

    /* Audio parameters */
    intf_MsgImm( "\nAudio parameters:"
        "\n  " AOUT_METHOD_VAR "=<method name>        \taudio method"
        "\n  " AOUT_DSP_VAR "=<filename>              \tdsp device path"
        "\n  " AOUT_STEREO_VAR "={1|0}                \tstereo or mono output"
        "\n  " AOUT_SPDIF_VAR "={1|0}                 \tAC3 pass-through mode"
        "\n  " DOWNMIX_METHOD_VAR "=<method name>     \tAC3 downmix method"
        "\n  " IMDCT_METHOD_VAR "=<method name>       \tAC3 IMDCT method"
        "\n  " AOUT_VOLUME_VAR "=[0..1024]            \tVLC output volume"
        "\n  " AOUT_RATE_VAR "=<rate>                 \toutput rate" );

    /* Video parameters */
    intf_MsgImm( "\nVideo parameters:"
        "\n  " VOUT_METHOD_VAR "=<method name>        \tdisplay method"
        "\n  " VOUT_DISPLAY_VAR "=<display name>      \tdisplay used"
        "\n  " VOUT_WIDTH_VAR "=<width>               \tdisplay width"
        "\n  " VOUT_HEIGHT_VAR "=<height>             \tdislay height"
        "\n  " VOUT_FB_DEV_VAR "=<filename>           \tframebuffer device path"
        "\n  " VOUT_GRAYSCALE_VAR "={1|0}             \tgrayscale or color output"
        "\n  " VOUT_FULLSCREEN_VAR "={1|0}            \tfullscreen"
        "\n  " VOUT_OVERLAY_VAR "={1|0}               \toverlay"
        "\n  " VOUT_XVADAPTOR_VAR "=<adaptor>         \tXVideo adaptor"
        "\n  " MOTION_METHOD_VAR "=<method name>      \tmotion compensation method"
        "\n  " IDCT_METHOD_VAR "=<method name>        \tIDCT method"
        "\n  " YUV_METHOD_VAR "=<method name>         \tYUV method"
        "\n  " VPAR_SYNCHRO_VAR "={I|I+|IP|IP+|IPB}   \tsynchro algorithm"
        "\n  " VDEC_SMP_VAR "=<number of threads>     \tuse several processors" );

    /* DVD parameters */
    intf_MsgImm( "\nDVD parameters:"
        "\n  " INPUT_DVD_DEVICE_VAR "=<device>        \tDVD device"
        "\n  " INPUT_TITLE_VAR "=<title>              \ttitle number"
        "\n  " INPUT_CHAPTER_VAR "=<chapter>          \tchapter number"
        "\n  " INPUT_ANGLE_VAR "=<angle>              \tangle number"
        "\n  " INPUT_AUDIO_VAR "={ac3|lpcm|mpeg|off}  \taudio type"
        "\n  " INPUT_CHANNEL_VAR "=[0-15]             \taudio channel"
        "\n  " INPUT_SUBTITLE_VAR "=[0-31]            \tsubtitle channel" );

    /* Input parameters */
    intf_MsgImm( "\nInput parameters:"
        "\n  " INPUT_IFACE_VAR "=<interface>          \tnetwork interface"
        "\n  " INPUT_CHANNEL_SERVER_VAR "=<hostname>  \tchannel server"
        "\n  " INPUT_CHANNEL_PORT_VAR "=<port>        \tchannel server port" );

    /* Decoder parameters */
    intf_MsgImm( "\nDecoder parameters:"
        "\n  " ADEC_MPEG_VAR "=<builtin|mad>          \taudio decoder" );
}

/*****************************************************************************
 * Version: print complete program version
 *****************************************************************************
 * Print complete program version and build number.
 *****************************************************************************/
static void Version( void )
{
    intf_MsgImm( VERSION_MESSAGE
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
    intf_ErrMsgImm( "intf error: signal %d received, exiting", i_signal );

    /* Try to terminate everything - this is done by requesting the end of the
     * interface thread */
    p_main->p_intf->b_die = 1;
}

/*****************************************************************************
 * InstructionSignalHandler: system signal handler
 *****************************************************************************
 * This function is called when a illegal instruction signal is received by
 * the program.
 * We use this function to test OS and CPU_Capabilities
 *****************************************************************************/
static void InstructionSignalHandler( int i_signal )
{
    /* Once a signal has been trapped, the termination sequence will be
     * armed and following signals will be ignored to avoid sending messages
     * to an interface having been destroyed */

    /* Acknowledge the signal received */
    i_illegal = 1;
    
#ifdef HAVE_SIGRELSE
    sigrelse( i_signal );
#endif
    longjmp( env, 1 );
}

/*****************************************************************************
 * CPUCapabilities: list the processors MMX support and other capabilities
 *****************************************************************************
 * This function is called to list extensions the CPU may have.
 *****************************************************************************/
static int CPUCapabilities( void )
{
    volatile int i_capabilities = CPU_CAPABILITY_NONE;

#if defined( SYS_BEOS )
    i_capabilities |= CPU_CAPABILITY_FPU
                      | CPU_CAPABILITY_486
                      | CPU_CAPABILITY_586
                      | CPU_CAPABILITY_MMX;

    return( i_capabilities );

#elif defined( SYS_DARWIN )
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

    i_capabilities |= CPU_CAPABILITY_FPU;

    signal( SIGILL, InstructionSignalHandler );
    
    /* test for a 486 CPU */
    asm volatile ( "pushfl\n\t"
                   "popl %%eax\n\t"
                   "movl %%eax, %%ebx\n\t"
                   "xorl $0x200000, %%eax\n\t"
                   "pushl %%eax\n\t"
                   "popfl\n\t"
                   "pushfl\n\t"
                   "popl %%eax"
                 : "=a" ( i_eax ),
                   "=b" ( i_ebx )
                 :
                 : "cc" );

    if( i_eax == i_ebx )
    {
        signal( SIGILL, NULL );     
        return( i_capabilities );
    }

    i_capabilities |= CPU_CAPABILITY_486;

    /* the CPU supports the CPUID instruction - get its level */
    cpuid( 0x00000000 );

    if( !i_eax )
    {
        signal( SIGILL, NULL );     
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
        signal( SIGILL, NULL );     
        return( i_capabilities );
    }

    i_capabilities |= CPU_CAPABILITY_MMX;

    if( i_edx & 0x02000000 )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;

#ifdef CAN_COMPILE_SSE
        /* We test if OS support the SSE instructions */
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
        else
        {
            fprintf( stderr, "warning: your OS doesn't have support for "
                             "SSE instructions, "
                             "some optimizations\nwill be disabled\n" );
#ifdef SYS_LINUX
            fprintf( stderr, "(you will need Linux kernel 2.4.x or later)\n" );
#endif
        }
#endif
    }
    
    /* test for additional capabilities */
    cpuid( 0x80000000 );

    if( i_eax < 0x80000001 )
    {
        signal( SIGILL, NULL );     
        return( i_capabilities );
    }

    /* list these additional capabilities */
    cpuid( 0x80000001 );

#ifdef CAN_COMPILE_3DNOW
    if( i_edx & 0x80000000 )
    {
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
#endif

    if( b_amd && ( i_edx & 0x00400000 ) )
    {
        i_capabilities |= CPU_CAPABILITY_MMXEXT;
    }

    signal( SIGILL, NULL );     
    return( i_capabilities );

#elif defined( __powerpc__ )

    i_capabilities |= CPU_CAPABILITY_FPU;

    /* Test for Altivec */
    signal( SIGILL, InstructionSignalHandler );

#ifdef CAN_COMPILE_ALTIVEC
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
#endif

    signal( SIGILL, NULL );     
    return( i_capabilities );

#else
    /* default behaviour */
    return( i_capabilities );

#endif
}

/*****************************************************************************
 * RedirectSTDOUT: redirect stdout and stderr to a file
 *****************************************************************************
 * This function will redirect stdout and stderr to a file if the user has
 * specified so.
 *****************************************************************************/
static int RedirectSTDOUT( void )
{
    int  i_fd;
    char *psz_filename;

    psz_filename = main_GetPszVariable( INTF_STDOUT_VAR, INTF_STDOUT_DEFAULT );

    if( *psz_filename )
    {
        ShowConsole();
        i_fd = open( psz_filename, O_CREAT | O_TRUNC | O_RDWR,
                                   S_IREAD | S_IWRITE );
        if( dup2( i_fd, fileno(stdout) ) == -1 )
        {
            intf_ErrMsg( "warning: unable to redirect stdout" );
        }

        if( dup2( i_fd, fileno(stderr) ) == -1 )
        {
            intf_ErrMsg( "warning: unable to redirect stderr" );
        }

        close( i_fd );
    }
    else
    {
        /* No stdout redirection has been asked so open a console */
        if( p_main->i_warning_level )
        {
            ShowConsole();
        }

    }

    return 0;
}

/*****************************************************************************
 * ShowConsole: On Win32, create an output console for debug messages
 *****************************************************************************
 * This function is usefull only on Win32.
 *****************************************************************************/
static void ShowConsole( void )
{
#ifdef WIN32 /*  */
    AllocConsole();
    freopen( "CONOUT$", "w", stdout );
    freopen( "CONOUT$", "w", stderr );
    freopen( "CONIN$", "r", stdin );
#endif
    return;
}

