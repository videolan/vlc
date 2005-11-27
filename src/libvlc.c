/*****************************************************************************
 * libvlc.c: main libvlc source
 *****************************************************************************
 * Copyright (C) 1998-2004 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
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
 * Pretend we are a builtin module
 *****************************************************************************/
#define MODULE_NAME main
#define MODULE_PATH main
#define __BUILTIN__

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <vlc/vlc.h>
#include <vlc/input.h>

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>                                            /* strerror() */
#include <stdlib.h>                                                /* free() */

#ifndef WIN32
#   include <netinet/in.h>                            /* BSD: struct in_addr */
#endif

#ifdef HAVE_UNISTD_H
#   include <unistd.h>
#elif defined( WIN32 ) && !defined( UNDER_CE )
#   include <io.h>
#endif

#ifdef WIN32                       /* optind, getopt(), included in unistd.h */
#   include "extras/getopt.h"
#endif

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif

#ifdef HAVE_HAL
#   include <hal/libhal.h>
#endif

#include "vlc_cpu.h"                                        /* CPU detection */
#include "os_specific.h"

#include "vlc_error.h"

#include "vlc_playlist.h"
#include "vlc_interface.h"

#include "audio_output.h"

#include "vlc_video.h"
#include "video_output.h"

#include "stream_output.h"
#include "charset.h"

#include "libvlc.h"

/*****************************************************************************
 * The evil global variable. We handle it with care, don't worry.
 *****************************************************************************/
static libvlc_t   libvlc;
static libvlc_t * p_libvlc;
static vlc_t *    p_static_vlc;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void LocaleInit( void );
static void LocaleDeinit( void );
static void SetLanguage   ( char const * );
static int  GetFilenames  ( vlc_t *, int, char *[] );
static void Help          ( vlc_t *, char const *psz_help_name );
static void Usage         ( vlc_t *, char const *psz_module_name );
static void ListModules   ( vlc_t * );
static void Version       ( void );

#ifdef WIN32
static void ShowConsole   ( void );
static void PauseConsole  ( void );
#endif
static int  ConsoleWidth  ( void );

static int  VerboseCallback( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );

static void InitDeviceValues( vlc_t * );

/*****************************************************************************
 * vlc_current_object: return the current object.
 *****************************************************************************
 * If i_object is non-zero, return the corresponding object. Otherwise,
 * return the statically allocated p_vlc object.
 *****************************************************************************/
vlc_t * vlc_current_object( int i_object )
{
    if( i_object )
    {
         return vlc_object_get( p_libvlc, i_object );
    }

    return p_static_vlc;
}

/*****************************************************************************
 * VLC_Version: return the libvlc version.
 *****************************************************************************
 * This function returns full version string (numeric version and codename).
 *****************************************************************************/
char const * VLC_Version( void )
{
    return VERSION_MESSAGE;
}

/*****************************************************************************
 * VLC_CompileBy, VLC_CompileHost, VLC_CompileDomain,
 * VLC_Compiler, VLC_Changeset
 *****************************************************************************/
#define DECLARE_VLC_VERSION( func, var )                                    \
char const * VLC_##func ( void )                                            \
{                                                                           \
    return VLC_##var ;                                                      \
}

DECLARE_VLC_VERSION( CompileBy, COMPILE_BY );
DECLARE_VLC_VERSION( CompileHost, COMPILE_HOST );
DECLARE_VLC_VERSION( CompileDomain, COMPILE_DOMAIN );
DECLARE_VLC_VERSION( Compiler, COMPILER );

extern const char psz_vlc_changeset[];
char const * VLC_Changeset( void )
{
    return psz_vlc_changeset;
}

/*****************************************************************************
 * VLC_Error: strerror() equivalent
 *****************************************************************************
 * This function returns full version string (numeric version and codename).
 *****************************************************************************/
char const * VLC_Error( int i_err )
{
    return vlc_error( i_err );
}

/*****************************************************************************
 * VLC_Create: allocate a vlc_t structure, and initialize libvlc if needed.
 *****************************************************************************
 * This function allocates a vlc_t structure and returns a negative value
 * in case of failure. Also, the thread system is initialized.
 *****************************************************************************/
int VLC_Create( void )
{
    int i_ret;
    vlc_t * p_vlc = NULL;
    vlc_value_t lockval;

    /* &libvlc never changes, so we can safely call this multiple times. */
    p_libvlc = &libvlc;

    /* vlc_threads_init *must* be the first internal call! No other call is
     * allowed before the thread system has been initialized. */
    i_ret = vlc_threads_init( p_libvlc );
    if( i_ret < 0 )
    {
        return i_ret;
    }

    /* Now that the thread system is initialized, we don't have much, but
     * at least we have var_Create */
    var_Create( p_libvlc, "libvlc", VLC_VAR_MUTEX );
    var_Get( p_libvlc, "libvlc", &lockval );
    vlc_mutex_lock( lockval.p_address );
    if( !libvlc.b_ready )
    {
        char *psz_env;

        /* Guess what CPU we have */
        libvlc.i_cpu = CPUCapabilities();

        /* Find verbosity from VLC_VERBOSE environment variable */
        psz_env = getenv( "VLC_VERBOSE" );
        libvlc.i_verbose = psz_env ? atoi( psz_env ) : -1;

#if defined( HAVE_ISATTY ) && !defined( WIN32 )
        libvlc.b_color = isatty( 2 ); /* 2 is for stderr */
#else
        libvlc.b_color = VLC_FALSE;
#endif

        /* Initialize message queue */
        msg_Create( p_libvlc );

        /* Announce who we are */
        msg_Dbg( p_libvlc, COPYRIGHT_MESSAGE );
        msg_Dbg( p_libvlc, "libvlc was configured with %s", CONFIGURE_LINE );

        /* The module bank will be initialized later */
        libvlc.p_module_bank = NULL;

        libvlc.b_ready = VLC_TRUE;

        /* UTF-8 convertor are initialized after the locale */
        libvlc.from_locale = libvlc.to_locale = (vlc_iconv_t)(-1);
    }
    vlc_mutex_unlock( lockval.p_address );
    var_Destroy( p_libvlc, "libvlc" );

    /* Allocate a vlc object */
    p_vlc = vlc_object_create( p_libvlc, VLC_OBJECT_VLC );
    if( p_vlc == NULL )
    {
        return VLC_EGENERIC;
    }
    p_vlc->thread_id = 0;

    p_vlc->psz_object_name = "root";

    /* Initialize mutexes */
    vlc_mutex_init( p_vlc, &p_vlc->config_lock );
#ifdef SYS_DARWIN
    vlc_mutex_init( p_vlc, &p_vlc->quicktime_lock );
    vlc_thread_set_priority( p_vlc, VLC_THREAD_PRIORITY_LOW );
#endif

    /* Store our newly allocated structure in the global list */
    vlc_object_attach( p_vlc, p_libvlc );

    /* Store data for the non-reentrant API */
    p_static_vlc = p_vlc;

    return p_vlc->i_object_id;
}

/*****************************************************************************
 * VLC_Init: initialize a vlc_t structure.
 *****************************************************************************
 * This function initializes a previously allocated vlc_t structure:
 *  - CPU detection
 *  - gettext initialization
 *  - message queue, module bank and playlist initialization
 *  - configuration and commandline parsing
 *****************************************************************************/
int VLC_Init( int i_object, int i_argc, char *ppsz_argv[] )
{
    char         p_capabilities[200];
    char *       p_tmp;
    char *       psz_modules;
    char *       psz_parser;
    char *       psz_control;
    vlc_bool_t   b_exit = VLC_FALSE;
    int          i_ret = VLC_EEXIT;
    vlc_t *      p_vlc = vlc_current_object( i_object );
    module_t    *p_help_module;
    playlist_t  *p_playlist;
    vlc_value_t  val;
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )
    char *       psz_language;
#endif

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    /*
     * System specific initialization code
     */
    system_Init( p_vlc, &i_argc, ppsz_argv );

    /* Get the executable name (similar to the basename command) */
    if( i_argc > 0 )
    {
        p_vlc->psz_object_name = p_tmp = ppsz_argv[ 0 ];
        while( *p_tmp )
        {
            if( *p_tmp == '/' ) p_vlc->psz_object_name = ++p_tmp;
            else ++p_tmp;
        }
    }
    else
    {
        p_vlc->psz_object_name = "vlc";
    }

    /*
     * Support for gettext
     */
    SetLanguage( "" );

    /*
     * Global iconv, must be done after setlocale()
     * so that vlc_current_charset() works.
     */
    LocaleInit();

    /* Translate "C" to the language code: "fr", "en_GB", "nl", "ru"... */
    msg_Dbg( p_vlc, "translation test: code is \"%s\"", _("C") );

    /* Initialize the module bank and load the configuration of the
     * main module. We need to do this at this stage to be able to display
     * a short help if required by the user. (short help == main module
     * options) */
    module_InitBank( p_vlc );

    /* Hack: insert the help module here */
    p_help_module = vlc_object_create( p_vlc, VLC_OBJECT_MODULE );
    if( p_help_module == NULL )
    {
        module_EndBank( p_vlc );
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_EGENERIC;
    }
    p_help_module->psz_object_name = "help";
    p_help_module->psz_longname = N_("Help options");
    config_Duplicate( p_help_module, p_help_config );
    vlc_object_attach( p_help_module, libvlc.p_module_bank );
    /* End hack */

    if( config_LoadCmdLine( p_vlc, &i_argc, ppsz_argv, VLC_TRUE ) )
    {
        vlc_object_detach( p_help_module );
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        module_EndBank( p_vlc );
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_EGENERIC;
    }

    /* Check for short help option */
    if( config_GetInt( p_vlc, "help" ) )
    {
        Help( p_vlc, "help" );
        b_exit = VLC_TRUE;
        i_ret = VLC_EEXITSUCCESS;
    }
    /* Check for version option */
    else if( config_GetInt( p_vlc, "version" ) )
    {
        Version();
        b_exit = VLC_TRUE;
        i_ret = VLC_EEXITSUCCESS;
    }

    /* Set the config file stuff */
    p_vlc->psz_homedir = config_GetHomeDir();
    p_vlc->psz_userdir = config_GetUserDir();
    if( p_vlc->psz_userdir == NULL )
        p_vlc->psz_userdir = strdup(p_vlc->psz_homedir);
    p_vlc->psz_configfile = config_GetPsz( p_vlc, "config" );
    if( p_vlc->psz_configfile != NULL && p_vlc->psz_configfile[0] == '~'
         && p_vlc->psz_configfile[1] == '/' )
    {
        char *psz = malloc( strlen(p_vlc->psz_userdir)
                             + strlen(p_vlc->psz_configfile) );
        /* This is incomplete : we should also support the ~cmassiot/ syntax. */
        sprintf( psz, "%s/%s", p_vlc->psz_userdir,
                               p_vlc->psz_configfile + 2 );
        free( p_vlc->psz_configfile );
        p_vlc->psz_configfile = psz;
    }

    /* Check for plugins cache options */
    if( config_GetInt( p_vlc, "reset-plugins-cache" ) )
    {
        libvlc.p_module_bank->b_cache_delete = VLC_TRUE;
    }

    /* Hack: remove the help module here */
    vlc_object_detach( p_help_module );
    /* End hack */

    /* Will be re-done properly later on */
    p_vlc->p_libvlc->i_verbose = config_GetInt( p_vlc, "verbose" );

    /* Check for daemon mode */
#ifndef WIN32
    if( config_GetInt( p_vlc, "daemon" ) )
    {
#if HAVE_DAEMON
        if( daemon( 1, 0) != 0 )
        {
            msg_Err( p_vlc, "Unable to fork vlc to daemon mode" );
            b_exit = VLC_TRUE;
        }

        p_vlc->p_libvlc->b_daemon = VLC_TRUE;

#else
        pid_t i_pid;

        if( ( i_pid = fork() ) < 0 )
        {
            msg_Err( p_vlc, "Unable to fork vlc to daemon mode" );
            b_exit = VLC_TRUE;
        }
        else if( i_pid )
        {
            /* This is the parent, exit right now */
            msg_Dbg( p_vlc, "closing parent process" );
            b_exit = VLC_TRUE;
            i_ret = VLC_EEXITSUCCESS;
        }
        else
        {
            /* We are the child */
            msg_Dbg( p_vlc, "daemon spawned" );
            close( STDIN_FILENO );
            close( STDOUT_FILENO );
            close( STDERR_FILENO );

            p_vlc->p_libvlc->b_daemon = VLC_TRUE;
        }
#endif
    }
#endif

    if( b_exit )
    {
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        module_EndBank( p_vlc );
        if( i_object ) vlc_object_release( p_vlc );
        return i_ret;
    }

    /* Check for translation config option */
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )

    /* This ain't really nice to have to reload the config here but it seems
     * the only way to do it. */
    config_LoadConfigFile( p_vlc, "main" );
    config_LoadCmdLine( p_vlc, &i_argc, ppsz_argv, VLC_TRUE );

    /* Check if the user specified a custom language */
    psz_language = config_GetPsz( p_vlc, "language" );
    if( psz_language && *psz_language && strcmp( psz_language, "auto" ) )
    {
        vlc_bool_t b_cache_delete = libvlc.p_module_bank->b_cache_delete;

        /* Reset the default domain */
        SetLanguage( psz_language );
        LocaleDeinit();
        LocaleInit();

        /* Translate "C" to the language code: "fr", "en_GB", "nl", "ru"... */
        msg_Dbg( p_vlc, "translation test: code is \"%s\"", _("C") );

        module_EndBank( p_vlc );
        module_InitBank( p_vlc );
        config_LoadConfigFile( p_vlc, "main" );
        config_LoadCmdLine( p_vlc, &i_argc, ppsz_argv, VLC_TRUE );
        libvlc.p_module_bank->b_cache_delete = b_cache_delete;
    }
    if( psz_language ) free( psz_language );
#endif

    /*
     * Load the builtins and plugins into the module_bank.
     * We have to do it before config_Load*() because this also gets the
     * list of configuration options exported by each module and loads their
     * default values.
     */
    module_LoadBuiltins( p_vlc );
    module_LoadPlugins( p_vlc );
    if( p_vlc->b_die )
    {
        b_exit = VLC_TRUE;
    }

    msg_Dbg( p_vlc, "module bank initialized, found %i modules",
                    libvlc.p_module_bank->i_children );

    /* Hack: insert the help module here */
    vlc_object_attach( p_help_module, libvlc.p_module_bank );
    /* End hack */

    /* Check for help on modules */
    if( (p_tmp = config_GetPsz( p_vlc, "module" )) )
    {
        Help( p_vlc, p_tmp );
        free( p_tmp );
        b_exit = VLC_TRUE;
        i_ret = VLC_EEXITSUCCESS;
    }
    /* Check for long help option */
    else if( config_GetInt( p_vlc, "longhelp" ) )
    {
        Help( p_vlc, "longhelp" );
        b_exit = VLC_TRUE;
        i_ret = VLC_EEXITSUCCESS;
    }
    /* Check for module list option */
    else if( config_GetInt( p_vlc, "list" ) )
    {
        ListModules( p_vlc );
        b_exit = VLC_TRUE;
        i_ret = VLC_EEXITSUCCESS;
    }

    /* Check for config file options */
    if( config_GetInt( p_vlc, "reset-config" ) )
    {
        vlc_object_detach( p_help_module );
        config_ResetAll( p_vlc );
        config_LoadCmdLine( p_vlc, &i_argc, ppsz_argv, VLC_TRUE );
        config_SaveConfigFile( p_vlc, NULL );
        vlc_object_attach( p_help_module, libvlc.p_module_bank );
    }
    if( config_GetInt( p_vlc, "save-config" ) )
    {
        vlc_object_detach( p_help_module );
        config_LoadConfigFile( p_vlc, NULL );
        config_LoadCmdLine( p_vlc, &i_argc, ppsz_argv, VLC_TRUE );
        config_SaveConfigFile( p_vlc, NULL );
        vlc_object_attach( p_help_module, libvlc.p_module_bank );
    }

    /* Hack: remove the help module here */
    vlc_object_detach( p_help_module );
    /* End hack */

    if( b_exit )
    {
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        module_EndBank( p_vlc );
        if( i_object ) vlc_object_release( p_vlc );
        return i_ret;
    }

    /*
     * Init device values
     */
    InitDeviceValues( p_vlc );

    /*
     * Override default configuration with config file settings
     */
    config_LoadConfigFile( p_vlc, NULL );

    /* Hack: insert the help module here */
    vlc_object_attach( p_help_module, libvlc.p_module_bank );
    /* End hack */

    /*
     * Override configuration with command line settings
     */
    if( config_LoadCmdLine( p_vlc, &i_argc, ppsz_argv, VLC_FALSE ) )
    {
#ifdef WIN32
        ShowConsole();
        /* Pause the console because it's destroyed when we exit */
        fprintf( stderr, "The command line options couldn't be loaded, check "
                 "that they are valid.\n" );
        PauseConsole();
#endif
        vlc_object_detach( p_help_module );
        config_Free( p_help_module );
        vlc_object_destroy( p_help_module );
        module_EndBank( p_vlc );
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_EGENERIC;
    }

    /* Hack: remove the help module here */
    vlc_object_detach( p_help_module );
    config_Free( p_help_module );
    vlc_object_destroy( p_help_module );
    /* End hack */

    /*
     * System specific configuration
     */
    system_Configure( p_vlc, &i_argc, ppsz_argv );

    /*
     * Message queue options
     */

    var_Create( p_vlc, "verbose", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    if( config_GetInt( p_vlc, "quiet" ) )
    {
        val.i_int = -1;
        var_Set( p_vlc, "verbose", val );
    }
    var_AddCallback( p_vlc, "verbose", VerboseCallback, NULL );
    var_Change( p_vlc, "verbose", VLC_VAR_TRIGGER_CALLBACKS, NULL, NULL );

    libvlc.b_color = libvlc.b_color && config_GetInt( p_vlc, "color" );

    /*
     * Output messages that may still be in the queue
     */
    msg_Flush( p_vlc );

    /* p_vlc initialization. FIXME ? */

    if( !config_GetInt( p_vlc, "fpu" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_FPU;

#if defined( __i386__ ) || defined( __x86_64__ )
    if( !config_GetInt( p_vlc, "mmx" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_MMX;
    if( !config_GetInt( p_vlc, "3dn" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_3DNOW;
    if( !config_GetInt( p_vlc, "mmxext" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_MMXEXT;
    if( !config_GetInt( p_vlc, "sse" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_SSE;
    if( !config_GetInt( p_vlc, "sse2" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_SSE2;
#endif
#if defined( __powerpc__ ) || defined( SYS_DARWIN )
    if( !config_GetInt( p_vlc, "altivec" ) )
        libvlc.i_cpu &= ~CPU_CAPABILITY_ALTIVEC;
#endif

#define PRINT_CAPABILITY( capability, string )                              \
    if( libvlc.i_cpu & capability )                                         \
    {                                                                       \
        strncat( p_capabilities, string " ",                                \
                 sizeof(p_capabilities) - strlen(p_capabilities) );         \
        p_capabilities[sizeof(p_capabilities) - 1] = '\0';                  \
    }

    p_capabilities[0] = '\0';
    PRINT_CAPABILITY( CPU_CAPABILITY_486, "486" );
    PRINT_CAPABILITY( CPU_CAPABILITY_586, "586" );
    PRINT_CAPABILITY( CPU_CAPABILITY_PPRO, "Pentium Pro" );
    PRINT_CAPABILITY( CPU_CAPABILITY_MMX, "MMX" );
    PRINT_CAPABILITY( CPU_CAPABILITY_3DNOW, "3DNow!" );
    PRINT_CAPABILITY( CPU_CAPABILITY_MMXEXT, "MMXEXT" );
    PRINT_CAPABILITY( CPU_CAPABILITY_SSE, "SSE" );
    PRINT_CAPABILITY( CPU_CAPABILITY_SSE2, "SSE2" );
    PRINT_CAPABILITY( CPU_CAPABILITY_ALTIVEC, "AltiVec" );
    PRINT_CAPABILITY( CPU_CAPABILITY_FPU, "FPU" );
    msg_Dbg( p_vlc, "CPU has capabilities %s", p_capabilities );

    /*
     * Choose the best memcpy module
     */
    p_vlc->p_memcpy_module = module_Need( p_vlc, "memcpy", "$memcpy", 0 );

    if( p_vlc->pf_memcpy == NULL )
    {
        p_vlc->pf_memcpy = memcpy;
    }

    if( p_vlc->pf_memset == NULL )
    {
        p_vlc->pf_memset = memset;
    }

    /*
     * Initialize hotkey handling
     */
    var_Create( p_vlc, "key-pressed", VLC_VAR_INTEGER );
    p_vlc->p_hotkeys = malloc( sizeof(p_hotkeys) );
    /* Do a copy (we don't need to modify the strings) */
    memcpy( p_vlc->p_hotkeys, p_hotkeys, sizeof(p_hotkeys) );

    /*
     * Initialize playlist and get commandline files
     */
    p_playlist = playlist_Create( p_vlc );
    if( !p_playlist )
    {
        msg_Err( p_vlc, "playlist initialization failed" );
        if( p_vlc->p_memcpy_module != NULL )
        {
            module_Unneed( p_vlc, p_vlc->p_memcpy_module );
        }
        module_EndBank( p_vlc );
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_EGENERIC;
    }

    psz_modules = config_GetPsz( p_playlist, "services-discovery" );
    if( psz_modules && *psz_modules )
    {
        /* Add service discovery modules */
        playlist_AddSDModules( p_playlist, psz_modules );
    }
    if( psz_modules ) free( psz_modules );

    /*
     * Load background interfaces
     */
    psz_modules = config_GetPsz( p_vlc, "extraintf" );
    psz_control = config_GetPsz( p_vlc, "control" );

    if( psz_modules && *psz_modules && psz_control && *psz_control )
    {
        psz_modules = (char *)realloc( psz_modules, strlen( psz_modules ) +
                                                    strlen( psz_control ) + 1 );
        sprintf( psz_modules, "%s:%s", psz_modules, psz_control );
    }
    else if( psz_control && *psz_control )
    {
        if( psz_modules ) free( psz_modules );
        psz_modules = strdup( psz_control );
    }

    psz_parser = psz_modules;
    while ( psz_parser && *psz_parser )
    {
        char *psz_module, *psz_temp;
        psz_module = psz_parser;
        psz_parser = strchr( psz_module, ':' );
        if ( psz_parser )
        {
            *psz_parser = '\0';
            psz_parser++;
        }
        psz_temp = (char *)malloc( strlen(psz_module) + sizeof(",none") );
        if( psz_temp )
        {
            sprintf( psz_temp, "%s,none", psz_module );
            VLC_AddIntf( 0, psz_temp, VLC_FALSE, VLC_FALSE );
            free( psz_temp );
        }
    }
    if ( psz_modules )
    {
        free( psz_modules );
    }

    /*
     * Always load the hotkeys interface if it exists
     */
    VLC_AddIntf( 0, "hotkeys,none", VLC_FALSE, VLC_FALSE );

    /*
     * If needed, load the Xscreensaver interface
     * Currently, only for X
     */
#ifdef HAVE_X11_XLIB_H
    if( config_GetInt( p_vlc, "disable-screensaver" ) == 1 )
    {
        VLC_AddIntf( 0, "screensaver", VLC_FALSE, VLC_FALSE );
    }
#endif

    /*
     * FIXME: kludge to use a p_vlc-local variable for the Mozilla plugin
     */
    var_Create( p_vlc, "drawable", VLC_VAR_INTEGER );
    var_Create( p_vlc, "drawableredraw", VLC_VAR_INTEGER );
    var_Create( p_vlc, "drawablet", VLC_VAR_INTEGER );
    var_Create( p_vlc, "drawablel", VLC_VAR_INTEGER );
    var_Create( p_vlc, "drawableb", VLC_VAR_INTEGER );
    var_Create( p_vlc, "drawabler", VLC_VAR_INTEGER );
    var_Create( p_vlc, "drawablex", VLC_VAR_INTEGER );
    var_Create( p_vlc, "drawabley", VLC_VAR_INTEGER );
    var_Create( p_vlc, "drawablew", VLC_VAR_INTEGER );
    var_Create( p_vlc, "drawableh", VLC_VAR_INTEGER );
    var_Create( p_vlc, "drawableportx", VLC_VAR_INTEGER );
    var_Create( p_vlc, "drawableporty", VLC_VAR_INTEGER );

    /* Create volume callback system. */
    var_Create( p_vlc, "volume-change", VLC_VAR_BOOL );

    /*
     * Get input filenames given as commandline arguments
     */
    GetFilenames( p_vlc, i_argc, ppsz_argv );

    /*
     * Get --open argument
     */
    var_Create( p_vlc, "open", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_vlc, "open", &val );
    if ( val.psz_string != NULL && *val.psz_string )
    {
        VLC_AddTarget( p_vlc->i_object_id, val.psz_string, NULL, 0,
                       PLAYLIST_INSERT, 0 );
    }
    if ( val.psz_string != NULL ) free( val.psz_string );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_AddIntf: add an interface
 *****************************************************************************
 * This function opens an interface plugin and runs it. If b_block is set
 * to 0, VLC_AddIntf will return immediately and let the interface run in a
 * separate thread. If b_block is set to 1, VLC_AddIntf will continue until
 * user requests to quit. If b_play is set to 1, VLC_AddIntf will start playing
 * the playlist when it is completely initialised.
 *****************************************************************************/
int VLC_AddIntf( int i_object, char const *psz_module,
                 vlc_bool_t b_block, vlc_bool_t b_play )
{
    int i_err;
    intf_thread_t *p_intf;
    vlc_t *p_vlc = vlc_current_object( i_object );

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

#ifndef WIN32
    if( p_vlc->p_libvlc->b_daemon && b_block && !psz_module )
    {
        /* Daemon mode hack.
         * We prefer the dummy interface if none is specified. */
        char *psz_interface = config_GetPsz( p_vlc, "intf" );
        if( !psz_interface || !*psz_interface ) psz_module = "dummy";
        if( psz_interface ) free( psz_interface );
    }
#endif

    /* Try to create the interface */
    p_intf = intf_Create( p_vlc, psz_module ? psz_module : "$intf" );

    if( p_intf == NULL )
    {
        msg_Err( p_vlc, "interface \"%s\" initialization failed", psz_module );
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_EGENERIC;
    }

    /* Interface doesn't handle play on start so do it ourselves */
    if( !p_intf->b_play && b_play ) VLC_Play( i_object );

    /* Try to run the interface */
    p_intf->b_play = b_play;
    p_intf->b_block = b_block;
    i_err = intf_RunThread( p_intf );
    if( i_err )
    {
        vlc_object_detach( p_intf );
        intf_Destroy( p_intf );
        if( i_object ) vlc_object_release( p_vlc );
        return i_err;
    }

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_Die: ask vlc to die.
 *****************************************************************************
 * This function sets p_vlc->b_die to VLC_TRUE, but does not do any other
 * task. It is your duty to call VLC_CleanUp and VLC_Destroy afterwards.
 *****************************************************************************/
int VLC_Die( int i_object )
{
    vlc_t *p_vlc = vlc_current_object( i_object );

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_vlc->b_die = VLC_TRUE;

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_CleanUp: CleanUp all the intf, playlist, vout, aout
 *****************************************************************************/
int VLC_CleanUp( int i_object )
{
    intf_thread_t      * p_intf;
    playlist_t         * p_playlist;
    vout_thread_t      * p_vout;
    aout_instance_t    * p_aout;
    announce_handler_t * p_announce;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    /*
     * Ask the interfaces to stop and destroy them
     */
    msg_Dbg( p_vlc, "removing all interfaces" );
    while( (p_intf = vlc_object_find( p_vlc, VLC_OBJECT_INTF, FIND_CHILD )) )
    {
        intf_StopThread( p_intf );
        vlc_object_detach( p_intf );
        vlc_object_release( p_intf );
        intf_Destroy( p_intf );
    }

    /*
     * Free playlists
     */
    msg_Dbg( p_vlc, "removing all playlists" );
    while( (p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST,
                                          FIND_CHILD )) )
    {
        vlc_object_detach( p_playlist );
        vlc_object_release( p_playlist );
        playlist_Destroy( p_playlist );
    }

    /*
     * Free video outputs
     */
    msg_Dbg( p_vlc, "removing all video outputs" );
    while( (p_vout = vlc_object_find( p_vlc, VLC_OBJECT_VOUT, FIND_CHILD )) )
    {
        vlc_object_detach( p_vout );
        vlc_object_release( p_vout );
        vout_Destroy( p_vout );
    }

    /*
     * Free audio outputs
     */
    msg_Dbg( p_vlc, "removing all audio outputs" );
    while( (p_aout = vlc_object_find( p_vlc, VLC_OBJECT_AOUT, FIND_CHILD )) )
    {
        vlc_object_detach( (vlc_object_t *)p_aout );
        vlc_object_release( (vlc_object_t *)p_aout );
        aout_Delete( p_aout );
    }

    /*
     * Free announce handler(s?)
     */
    msg_Dbg( p_vlc, "removing announce handler" );
    while( (p_announce = vlc_object_find( p_vlc, VLC_OBJECT_ANNOUNCE,
                                                 FIND_CHILD ) ) )
   {
        vlc_object_detach( p_announce );
        vlc_object_release( p_announce );
        announce_HandlerDestroy( p_announce );
   }

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_Destroy: Destroy everything.
 *****************************************************************************
 * This function requests the running threads to finish, waits for their
 * termination, and destroys their structure.
 *****************************************************************************/
int VLC_Destroy( int i_object )
{
    vlc_t *p_vlc = vlc_current_object( i_object );

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    /*
     * Free allocated memory
     */
    if( p_vlc->p_memcpy_module )
    {
        module_Unneed( p_vlc, p_vlc->p_memcpy_module );
        p_vlc->p_memcpy_module = NULL;
    }

    /*
     * Free module bank !
     */
    module_EndBank( p_vlc );

    if( p_vlc->psz_homedir )
    {
        free( p_vlc->psz_homedir );
        p_vlc->psz_homedir = NULL;
    }

    if( p_vlc->psz_userdir )
    {
        free( p_vlc->psz_userdir );
        p_vlc->psz_userdir = NULL;
    }

    if( p_vlc->psz_configfile )
    {
        free( p_vlc->psz_configfile );
        p_vlc->psz_configfile = NULL;
    }

    if( p_vlc->p_hotkeys )
    {
        free( p_vlc->p_hotkeys );
        p_vlc->p_hotkeys = NULL;
    }

    /*
     * System specific cleaning code
     */
    system_End( p_vlc );

    /*
     * Free message queue.
     * Nobody shall use msg_* afterward.
     */
    msg_Flush( p_vlc );
    msg_Destroy( p_libvlc );

    /* Destroy global iconv */
    LocaleDeinit();

    /* Destroy mutexes */
    vlc_mutex_destroy( &p_vlc->config_lock );

    vlc_object_detach( p_vlc );

    /* Release object before destroying it */
    if( i_object ) vlc_object_release( p_vlc );

    vlc_object_destroy( p_vlc );

    /* Stop thread system: last one out please shut the door! */
    vlc_threads_end( p_libvlc );

    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_VariableSet: set a vlc variable
 *****************************************************************************/
int VLC_VariableSet( int i_object, char const *psz_var, vlc_value_t value )
{
    vlc_t *p_vlc = vlc_current_object( i_object );
    int i_ret;

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    /* FIXME: Temporary hack for Mozilla, if variable starts with conf:: then
     * we handle it as a configuration variable. Don't tell Gildas :) -- sam */
    if( !strncmp( psz_var, "conf::", 6 ) )
    {
        module_config_t *p_item;
        char const *psz_newvar = psz_var + 6;

        p_item = config_FindConfig( VLC_OBJECT(p_vlc), psz_newvar );

        if( p_item )
        {
            switch( p_item->i_type )
            {
                case CONFIG_ITEM_BOOL:
                    config_PutInt( p_vlc, psz_newvar, value.b_bool );
                    break;
                case CONFIG_ITEM_INTEGER:
                    config_PutInt( p_vlc, psz_newvar, value.i_int );
                    break;
                case CONFIG_ITEM_FLOAT:
                    config_PutFloat( p_vlc, psz_newvar, value.f_float );
                    break;
                default:
                    config_PutPsz( p_vlc, psz_newvar, value.psz_string );
                    break;
            }
            if( i_object ) vlc_object_release( p_vlc );
            return VLC_SUCCESS;
        }
    }

    i_ret = var_Set( p_vlc, psz_var, value );

    if( i_object ) vlc_object_release( p_vlc );
    return i_ret;
}

/*****************************************************************************
 * VLC_VariableGet: get a vlc variable
 *****************************************************************************/
int VLC_VariableGet( int i_object, char const *psz_var, vlc_value_t *p_value )
{
    vlc_t *p_vlc = vlc_current_object( i_object );
    int i_ret;

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    i_ret = var_Get( p_vlc , psz_var, p_value );

    if( i_object ) vlc_object_release( p_vlc );
    return i_ret;
}

/*****************************************************************************
 * VLC_VariableType: get a vlc variable type
 *****************************************************************************/
int VLC_VariableType( int i_object, char const *psz_var, int *pi_type )
{
    int i_type;
    vlc_t *p_vlc = vlc_current_object( i_object );

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    /* FIXME: Temporary hack for Mozilla, if variable starts with conf:: then
     * we handle it as a configuration variable. Don't tell Gildas :) -- sam */
    if( !strncmp( psz_var, "conf::", 6 ) )
    {
        module_config_t *p_item;
        char const *psz_newvar = psz_var + 6;

        p_item = config_FindConfig( VLC_OBJECT(p_vlc), psz_newvar );

        if( p_item )
        {
            switch( p_item->i_type )
            {
                case CONFIG_ITEM_BOOL:
                    i_type = VLC_VAR_BOOL;
                    break;
                case CONFIG_ITEM_INTEGER:
                    i_type = VLC_VAR_INTEGER;
                    break;
                case CONFIG_ITEM_FLOAT:
                    i_type = VLC_VAR_FLOAT;
                    break;
                default:
                    i_type = VLC_VAR_STRING;
                    break;
            }
        }
        else
            i_type = 0;
    }
    else
        i_type = VLC_VAR_TYPE & var_Type( p_vlc , psz_var );

    if( i_object ) vlc_object_release( p_vlc );

    if( i_type > 0 )
    {
        *pi_type = i_type;
        return VLC_SUCCESS;
    }
    return VLC_ENOVAR;
}

/*****************************************************************************
 * VLC_AddTarget: adds a target for playing.
 *****************************************************************************
 * This function adds psz_target to the current playlist. If a playlist does
 * not exist, it will create one.
 *****************************************************************************/
int VLC_AddTarget( int i_object, char const *psz_target,
                   char const **ppsz_options, int i_options,
                   int i_mode, int i_pos )
{
    int i_err;
    playlist_t *p_playlist;
    vlc_t *p_vlc = vlc_current_object( i_object );

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );

    if( p_playlist == NULL )
    {
        msg_Dbg( p_vlc, "no playlist present, creating one" );
        p_playlist = playlist_Create( p_vlc );

        if( p_playlist == NULL )
        {
            if( i_object ) vlc_object_release( p_vlc );
            return VLC_EGENERIC;
        }

        vlc_object_yield( p_playlist );
    }

    i_err = playlist_AddExt( p_playlist, psz_target, psz_target,
                             i_mode, i_pos, -1, ppsz_options, i_options);

    vlc_object_release( p_playlist );

    if( i_object ) vlc_object_release( p_vlc );
    return i_err;
}

/*****************************************************************************
 * VLC_Play: play the playlist
 *****************************************************************************/
int VLC_Play( int i_object )
{
    playlist_t * p_playlist;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_CHILD );

    if( !p_playlist )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    playlist_Play( p_playlist );
    vlc_object_release( p_playlist );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_Pause: toggle pause
 *****************************************************************************/
int VLC_Pause( int i_object )
{
    playlist_t * p_playlist;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_CHILD );

    if( !p_playlist )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    playlist_Pause( p_playlist );
    vlc_object_release( p_playlist );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_Pause: toggle pause
 *****************************************************************************/
int VLC_Stop( int i_object )
{
    playlist_t * p_playlist;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_CHILD );

    if( !p_playlist )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    playlist_Stop( p_playlist );
    vlc_object_release( p_playlist );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_IsPlaying: Query for Playlist Status
 *****************************************************************************/
vlc_bool_t VLC_IsPlaying( int i_object )
{
    playlist_t * p_playlist;
    vlc_bool_t   b_playing;

    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_CHILD );

    if( !p_playlist )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    if( p_playlist->p_input )
    {
        vlc_value_t  val;
        var_Get( p_playlist->p_input, "state", &val );
        b_playing = ( val.i_int == PLAYING_S );
    }
    else
    {
        msg_Dbg(p_vlc, "polling playlist_IsPlaying");
        b_playing = playlist_IsPlaying( p_playlist );
    }
    vlc_object_release( p_playlist );

    if( i_object ) vlc_object_release( p_vlc );
    return b_playing;
}

/**
 * Get the current position in a input
 *
 * Return the current position as a float
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \return a float in the range of 0.0 - 1.0
 */
float VLC_PositionGet( int i_object )
{
    input_thread_t *p_input;
    vlc_value_t val;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_vlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    var_Get( p_input, "position", &val );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_vlc );
    return val.f_float;
}

/**
 * Set the current position in a input
 *
 * Set the current position in a input and then return
 * the current position as a float.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \param i_position a float in the range of 0.0 - 1.0
 * \return a float in the range of 0.0 - 1.0
 */
float VLC_PositionSet( int i_object, float i_position )
{
    input_thread_t *p_input;
    vlc_value_t val;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_vlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    val.f_float = i_position;
    var_Set( p_input, "position", val );
    var_Get( p_input, "position", &val );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_vlc );
    return val.f_float;
}

/**
 * Get the current position in a input
 *
 * Return the current position in seconds from the start.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \return the offset from 0:00 in seconds
 */
int VLC_TimeGet( int i_object )
{
    input_thread_t *p_input;
    vlc_value_t val;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_vlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    var_Get( p_input, "time", &val );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_vlc );
    return val.i_time  / 1000000;
}

/**
 * Seek to a position in the current input
 *
 * Seek i_seconds in the current input. If b_relative is set,
 * then the seek will be relative to the current position, otherwise
 * it will seek to i_seconds from the beginning of the input.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \param i_seconds seconds from current position or from beginning of input
 * \param b_relative seek relative from current position
 * \return VLC_SUCCESS on success
 */
int VLC_TimeSet( int i_object, int i_seconds, vlc_bool_t b_relative )
{
    input_thread_t *p_input;
    vlc_value_t val;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_vlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    if( b_relative )
    {
        val.i_time = i_seconds;
        val.i_time = val.i_time * 1000000L;
        var_Set( p_input, "time-offset", val );
    }
    else
    {
        val.i_time = i_seconds;
        val.i_time = val.i_time * 1000000L;
        var_Set( p_input, "time", val );
    }
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/**
 * Get the total length of a input
 *
 * Return the total length in seconds from the current input.
 * \note For some inputs, this will be unknown.
 *
 * \param i_object a vlc object id
 * \return the length in seconds
 */
int VLC_LengthGet( int i_object )
{
    input_thread_t *p_input;
    vlc_value_t val;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_vlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    var_Get( p_input, "length", &val );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_vlc );
    return val.i_time  / 1000000L;
}

/**
 * Play the input faster than realtime
 *
 * 2x, 4x, 8x faster than realtime
 * \note For some inputs, this will be impossible.
 *
 * \param i_object a vlc object id
 * \return the current speedrate
 */
float VLC_SpeedFaster( int i_object )
{
    input_thread_t *p_input;
    vlc_value_t val;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_vlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    val.b_bool = VLC_TRUE;
    var_Set( p_input, "rate-faster", val );
    var_Get( p_input, "rate", &val );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_vlc );
    return val.f_float / INPUT_RATE_DEFAULT;
}

/**
 * Play the input slower than realtime
 *
 * 1/2x, 1/4x, 1/8x slower than realtime
 * \note For some inputs, this will be impossible.
 *
 * \param i_object a vlc object id
 * \return the current speedrate
 */
float VLC_SpeedSlower( int i_object )
{
    input_thread_t *p_input;
    vlc_value_t val;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_input = vlc_object_find( p_vlc, VLC_OBJECT_INPUT, FIND_CHILD );

    if( !p_input )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    val.b_bool = VLC_TRUE;
    var_Set( p_input, "rate-slower", val );
    var_Get( p_input, "rate", &val );
    vlc_object_release( p_input );

    if( i_object ) vlc_object_release( p_vlc );
    return val.f_float / INPUT_RATE_DEFAULT;
}

/**
 * Return the current playlist item
 *
 * Returns the index of the playlistitem that is currently selected for play.
 * This is valid even if nothing is currently playing.
 *
 * \param i_object a vlc object id
 * \return the current index
 */
int VLC_PlaylistIndex( int i_object )
{
    int i_index;
    playlist_t * p_playlist;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_CHILD );

    if( !p_playlist )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    i_index = p_playlist->i_index;
    vlc_object_release( p_playlist );

    if( i_object ) vlc_object_release( p_vlc );
    return i_index;
}

/**
 * Total amount of items in the playlist
 *
 * \param i_object a vlc object id
 * \return amount of playlist items
 */
int VLC_PlaylistNumberOfItems( int i_object )
{
    int i_size;
    playlist_t * p_playlist;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_CHILD );

    if( !p_playlist )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    i_size = p_playlist->i_size;
    vlc_object_release( p_playlist );

    if( i_object ) vlc_object_release( p_vlc );
    return i_size;
}

/**
 * Next playlist item
 *
 * Skip to the next playlistitem and play it.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int VLC_PlaylistNext( int i_object )
{
    playlist_t * p_playlist;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_CHILD );

    if( !p_playlist )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    playlist_Next( p_playlist );
    vlc_object_release( p_playlist );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/**
 * Previous playlist item
 *
 * Skip to the previous playlistitem and play it.
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int VLC_PlaylistPrev( int i_object )
{
    playlist_t * p_playlist;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_CHILD );

    if( !p_playlist )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    playlist_Prev( p_playlist );
    vlc_object_release( p_playlist );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}


/*****************************************************************************
 * VLC_PlaylistClear: Empty the playlist
 *****************************************************************************/
int VLC_PlaylistClear( int i_object )
{
    int i_err;
    playlist_t * p_playlist;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_playlist = vlc_object_find( p_vlc, VLC_OBJECT_PLAYLIST, FIND_CHILD );

    if( !p_playlist )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    i_err = playlist_Clear( p_playlist );

    vlc_object_release( p_playlist );

    if( i_object ) vlc_object_release( p_vlc );
    return i_err;
}

/**
 * Change the volume
 *
 * \param i_object a vlc object id
 * \param i_volume something in a range from 0-200
 * \return the new volume (range 0-200 %)
 */
int VLC_VolumeSet( int i_object, int i_volume )
{
    audio_volume_t i_vol = 0;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    if( i_volume >= 0 && i_volume <= 200 )
    {
        i_vol = i_volume * AOUT_VOLUME_MAX / 200;
        aout_VolumeSet( p_vlc, i_vol );
    }

    if( i_object ) vlc_object_release( p_vlc );
    return i_vol * 200 / AOUT_VOLUME_MAX;
}

/**
 * Get the current volume
 *
 * Retrieve the current volume.
 *
 * \param i_object a vlc object id
 * \return the current volume (range 0-200 %)
 */
int VLC_VolumeGet( int i_object )
{
    audio_volume_t i_volume;
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    aout_VolumeGet( p_vlc, &i_volume );

    if( i_object ) vlc_object_release( p_vlc );
    return i_volume*200/AOUT_VOLUME_MAX;
}

/**
 * Mute/Unmute the volume
 *
 * \param i_object a vlc object id
 * \return VLC_SUCCESS on success
 */
int VLC_VolumeMute( int i_object )
{
    vlc_t *p_vlc = vlc_current_object( i_object );

    /* Check that the handle is valid */
    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    aout_VolumeMute( p_vlc, NULL );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/*****************************************************************************
 * VLC_FullScreen: toggle fullscreen mode
 *****************************************************************************/
int VLC_FullScreen( int i_object )
{
    vout_thread_t *p_vout;
    vlc_t *p_vlc = vlc_current_object( i_object );

    if( !p_vlc )
    {
        return VLC_ENOOBJ;
    }

    p_vout = vlc_object_find( p_vlc, VLC_OBJECT_VOUT, FIND_CHILD );

    if( !p_vout )
    {
        if( i_object ) vlc_object_release( p_vlc );
        return VLC_ENOOBJ;
    }

    p_vout->i_changes |= VOUT_FULLSCREEN_CHANGE;
    vlc_object_release( p_vout );

    if( i_object ) vlc_object_release( p_vlc );
    return VLC_SUCCESS;
}

/* following functions are local */

static void LocaleInit( void )
{
    char *psz_charset;

    if( !vlc_current_charset( &psz_charset ) )
    {
        char *psz_conv = psz_charset;

        /*
         * Still allow non-ASCII characters when the locale is not set.
         * Western Europeans are being favored for historical reasons.
         */
        psz_conv = strcmp( psz_charset, "ASCII" )
            ? psz_charset
            : "ISO-8859-15";

        vlc_mutex_init( p_libvlc, &libvlc.from_locale_lock );
        vlc_mutex_init( p_libvlc, &libvlc.to_locale_lock );
        libvlc.from_locale = vlc_iconv_open( "UTF-8", psz_charset );
        libvlc.to_locale = vlc_iconv_open( psz_charset, "UTF-8" );
        if( !libvlc.to_locale )
        {
            /* Not sure it is the right thing to do, but at least it
             doesn't make vlc crash with msvc ! */
            libvlc.to_locale = (vlc_iconv_t)(-1);
        }
    }
    else
        libvlc.from_locale = libvlc.to_locale = (vlc_iconv_t)(-1);
    free( psz_charset );
}

static void LocaleDeinit( void )
{
    if( libvlc.to_locale != (vlc_iconv_t)(-1) )
    {
        vlc_mutex_destroy( &libvlc.from_locale_lock );
        vlc_mutex_destroy( &libvlc.to_locale_lock );
        vlc_iconv_close( libvlc.from_locale );
        vlc_iconv_close( libvlc.to_locale );
    }
}

/*****************************************************************************
 * SetLanguage: set the interface language.
 *****************************************************************************
 * We set the LC_MESSAGES locale category for interface messages and buttons,
 * as well as the LC_CTYPE category for string sorting and possible wide
 * character support.
 *****************************************************************************/
static void SetLanguage ( char const *psz_lang )
{
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )

    char *          psz_path;
#if defined( SYS_DARWIN ) || defined ( WIN32 ) || defined( SYS_BEOS )
    char            psz_tmp[1024];
#endif

    if( psz_lang && !*psz_lang )
    {
#   if defined( HAVE_LC_MESSAGES )
        setlocale( LC_MESSAGES, psz_lang );
#   endif
        setlocale( LC_CTYPE, psz_lang );
    }
    else if( psz_lang )
    {
#ifdef SYS_DARWIN
        /* I need that under Darwin, please check it doesn't disturb
         * other platforms. --Meuuh */
        setenv( "LANG", psz_lang, 1 );

#elif defined( SYS_BEOS ) || defined( WIN32 )
        /* We set LC_ALL manually because it is the only way to set
         * the language at runtime under eg. Windows. Beware that this
         * makes the environment unconsistent when libvlc is unloaded and
         * should probably be moved to a safer place like vlc.c. */
        static char psz_lcall[20];
        snprintf( psz_lcall, 19, "LC_ALL=%s", psz_lang );
        psz_lcall[19] = '\0';
        putenv( psz_lcall );
#endif

        setlocale( LC_ALL, psz_lang );
        /* many code paths assume that float numbers are formatted according
         * to the US standard (ie. with dot as decimal point), so we keep
         * C for LC_NUMERIC. */
        setlocale(LC_NUMERIC, "C" );
    }

    /* Specify where to find the locales for current domain */
#if !defined( SYS_DARWIN ) && !defined( WIN32 ) && !defined( SYS_BEOS )
    psz_path = LOCALEDIR;
#else
    snprintf( psz_tmp, sizeof(psz_tmp), "%s/%s", libvlc.psz_vlcpath,
              "locale" );
    psz_path = psz_tmp;
#endif
    if( !bindtextdomain( PACKAGE_NAME, psz_path ) )
    {
        fprintf( stderr, "warning: couldn't bind domain %s in directory %s\n",
                 PACKAGE_NAME, psz_path );
    }

    /* Set the default domain */
    textdomain( PACKAGE_NAME );
    bind_textdomain_codeset( PACKAGE_NAME, "UTF-8" );
#endif
}

/*****************************************************************************
 * GetFilenames: parse command line options which are not flags
 *****************************************************************************
 * Parse command line for input files as well as their associated options.
 * An option always follows its associated input and begins with a ":".
 *****************************************************************************/
static int GetFilenames( vlc_t *p_vlc, int i_argc, char *ppsz_argv[] )
{
    int i_opt, i_options;

    /* We assume that the remaining parameters are filenames
     * and their input options */
    for( i_opt = i_argc - 1; i_opt >= optind; i_opt-- )
    {
        const char *psz_target;
        i_options = 0;

        /* Count the input options */
        while( *ppsz_argv[ i_opt ] == ':' && i_opt > optind )
        {
            i_options++;
            i_opt--;
        }

        /* TODO: write an internal function of this one, to avoid
         *       unnecessary lookups. */
        /* FIXME: should we convert options to UTF-8 as well ?? */
        psz_target = FromLocale( ppsz_argv[ i_opt ] );
        VLC_AddTarget( p_vlc->i_object_id, psz_target,
                       (char const **)( i_options ? &ppsz_argv[i_opt + 1] :
                                        NULL ), i_options,
                       PLAYLIST_INSERT, 0 );
        LocaleFree( psz_target );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Help: print program help
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
static void Help( vlc_t *p_this, char const *psz_help_name )
{
#ifdef WIN32
    ShowConsole();
#endif

    if( psz_help_name && !strcmp( psz_help_name, "help" ) )
    {
        fprintf( stdout, VLC_USAGE, p_this->psz_object_name );
        Usage( p_this, "help" );
        Usage( p_this, "main" );
    }
    else if( psz_help_name && !strcmp( psz_help_name, "longhelp" ) )
    {
        fprintf( stdout, VLC_USAGE, p_this->psz_object_name );
        Usage( p_this, NULL );
    }
    else if( psz_help_name )
    {
        Usage( p_this, psz_help_name );
    }

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
    PauseConsole();
#endif
}

/*****************************************************************************
 * Usage: print module usage
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
static void Usage( vlc_t *p_this, char const *psz_module_name )
{
#define FORMAT_STRING "  %s --%s%s%s%s%s%s%s "
    /* short option ------'    |     | | | |  | |
     * option name ------------'     | | | |  | |
     * <bra -------------------------' | | |  | |
     * option type or "" --------------' | |  | |
     * ket> -----------------------------' |  | |
     * padding spaces ---------------------'  | |
     * comment -------------------------------' |
     * comment suffix --------------------------'
     *
     * The purpose of having bra and ket is that we might i18n them as well.
     */
#define LINE_START 8
#define PADDING_SPACES 25
    vlc_list_t *p_list;
    module_t *p_parser;
    module_config_t *p_item;
    char psz_spaces_text[PADDING_SPACES+LINE_START+1];
    char psz_spaces_longtext[LINE_START+3];
    char psz_format[sizeof(FORMAT_STRING)];
    char psz_buffer[10000];
    char psz_short[4];
    int i_index;
    int i_width = ConsoleWidth() - (PADDING_SPACES+LINE_START+1);
    vlc_bool_t b_advanced = config_GetInt( p_this, "advanced" );
    vlc_bool_t b_description;

    memset( psz_spaces_text, ' ', PADDING_SPACES+LINE_START );
    psz_spaces_text[PADDING_SPACES+LINE_START] = '\0';
    memset( psz_spaces_longtext, ' ', LINE_START+2 );
    psz_spaces_longtext[LINE_START+2] = '\0';

    strcpy( psz_format, FORMAT_STRING );

    /* List all modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    /* Enumerate the config for each module */
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        vlc_bool_t b_help_module;

        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        if( psz_module_name && strcmp( psz_module_name,
                                       p_parser->psz_object_name ) )
        {
            continue;
        }

        /* Ignore modules without config options */
        if( !p_parser->i_config_items )
        {
            continue;
        }

        /* Ignore modules with only advanced config options if requested */
        if( !b_advanced )
        {
            for( p_item = p_parser->p_config;
                 p_item->i_type != CONFIG_HINT_END;
                 p_item++ )
            {
                if( (p_item->i_type & CONFIG_ITEM) &&
                    !p_item->b_advanced ) break;
            }
            if( p_item->i_type == CONFIG_HINT_END ) continue;
        }

        /* Print name of module */
        if( strcmp( "main", p_parser->psz_object_name ) )
        fprintf( stdout, "\n %s\n", p_parser->psz_longname );

        b_help_module = !strcmp( "help", p_parser->psz_object_name );

        /* Print module options */
        for( p_item = p_parser->p_config;
             p_item->i_type != CONFIG_HINT_END;
             p_item++ )
        {
            char *psz_text, *psz_spaces = psz_spaces_text;
            char *psz_bra = NULL, *psz_type = NULL, *psz_ket = NULL;
            char *psz_suf = "", *psz_prefix = NULL;
            signed int i;

            /* Skip deprecated options */
            if( p_item->psz_current )
            {
                continue;
            }
            /* Skip advanced options if requested */
            if( p_item->b_advanced && !b_advanced )
            {
                continue;
            }

            switch( p_item->i_type )
            {
            case CONFIG_HINT_CATEGORY:
            case CONFIG_HINT_USAGE:
                if( !strcmp( "main", p_parser->psz_object_name ) )
                fprintf( stdout, "\n %s\n", p_item->psz_text );
                break;

            case CONFIG_ITEM_STRING:
            case CONFIG_ITEM_FILE:
            case CONFIG_ITEM_DIRECTORY:
            case CONFIG_ITEM_MODULE: /* We could also have "=<" here */
            case CONFIG_ITEM_MODULE_CAT:
            case CONFIG_ITEM_MODULE_LIST:
            case CONFIG_ITEM_MODULE_LIST_CAT:
                psz_bra = " <"; psz_type = _("string"); psz_ket = ">";

                if( p_item->ppsz_list )
                {
                    psz_bra = " {";
                    psz_type = psz_buffer;
                    psz_type[0] = '\0';
                    for( i = 0; p_item->ppsz_list[i]; i++ )
                    {
                        if( i ) strcat( psz_type, "," );
                        strcat( psz_type, p_item->ppsz_list[i] );
                    }
                    psz_ket = "}";
                }
                break;
            case CONFIG_ITEM_INTEGER:
            case CONFIG_ITEM_KEY: /* FIXME: do something a bit more clever */
                psz_bra = " <"; psz_type = _("integer"); psz_ket = ">";

                if( p_item->i_list )
                {
                    psz_bra = " {";
                    psz_type = psz_buffer;
                    psz_type[0] = '\0';
                    for( i = 0; p_item->ppsz_list_text[i]; i++ )
                    {
                        if( i ) strcat( psz_type, ", " );
                        sprintf( psz_type + strlen(psz_type), "%i (%s)",
                                 p_item->pi_list[i],
                                 p_item->ppsz_list_text[i] );
                    }
                    psz_ket = "}";
                }
                break;
            case CONFIG_ITEM_FLOAT:
                psz_bra = " <"; psz_type = _("float"); psz_ket = ">";
                break;
            case CONFIG_ITEM_BOOL:
                psz_bra = ""; psz_type = ""; psz_ket = "";
                if( !b_help_module )
                {
                    psz_suf = p_item->i_value ? _(" (default enabled)") :
                                                _(" (default disabled)");
                }
                break;
            }

            if( !psz_type )
            {
                continue;
            }

            /* Add short option if any */
            if( p_item->i_short )
            {
                sprintf( psz_short, "-%c,", p_item->i_short );
            }
            else
            {
                strcpy( psz_short, "   " );
            }

            i = PADDING_SPACES - strlen( p_item->psz_name )
                 - strlen( psz_bra ) - strlen( psz_type )
                 - strlen( psz_ket ) - 1;

            if( p_item->i_type == CONFIG_ITEM_BOOL && !b_help_module )
            {
                psz_prefix =  ", --no-";
                i -= strlen( p_item->psz_name ) + strlen( psz_prefix );
            }

            if( i < 0 )
            {
                psz_spaces[0] = '\n';
                i = 0;
            }
            else
            {
                psz_spaces[i] = '\0';
            }

            if( p_item->i_type == CONFIG_ITEM_BOOL && !b_help_module )
            {
                fprintf( stdout, psz_format, psz_short, p_item->psz_name,
                         psz_prefix, p_item->psz_name, psz_bra, psz_type,
                         psz_ket, psz_spaces );
            }
            else
            {
                fprintf( stdout, psz_format, psz_short, p_item->psz_name,
                         "", "", psz_bra, psz_type, psz_ket, psz_spaces );
            }

            psz_spaces[i] = ' ';

            /* We wrap the rest of the output */
            sprintf( psz_buffer, "%s%s", p_item->psz_text, psz_suf );
            b_description = config_GetInt( p_this, "help-verbose" );

 description:
            psz_text = psz_buffer;
            while( *psz_text )
            {
                char *psz_parser, *psz_word;
                size_t i_end = strlen( psz_text );

                /* If the remaining text fits in a line, print it. */
                if( i_end <= (size_t)i_width )
                {
                    fprintf( stdout, "%s\n", psz_text );
                    break;
                }

                /* Otherwise, eat as many words as possible */
                psz_parser = psz_text;
                do
                {
                    psz_word = psz_parser;
                    psz_parser = strchr( psz_word, ' ' );
                    /* If no space was found, we reached the end of the text
                     * block; otherwise, we skip the space we just found. */
                    psz_parser = psz_parser ? psz_parser + 1
                                            : psz_text + i_end;

                } while( psz_parser - psz_text <= i_width );

                /* We cut a word in one of these cases:
                 *  - it's the only word in the line and it's too long.
                 *  - we used less than 80% of the width and the word we are
                 *    going to wrap is longer than 40% of the width, and even
                 *    if the word would have fit in the next line. */
                if( psz_word == psz_text
                     || ( psz_word - psz_text < 80 * i_width / 100
                           && psz_parser - psz_word > 40 * i_width / 100 ) )
                {
                    char c = psz_text[i_width];
                    psz_text[i_width] = '\0';
                    fprintf( stdout, "%s\n%s", psz_text, psz_spaces );
                    psz_text += i_width;
                    psz_text[0] = c;
                }
                else
                {
                    psz_word[-1] = '\0';
                    fprintf( stdout, "%s\n%s", psz_text, psz_spaces );
                    psz_text = psz_word;
                }
            }

            if( b_description && p_item->psz_longtext )
            {
                sprintf( psz_buffer, "%s%s", p_item->psz_longtext, psz_suf );
                b_description = VLC_FALSE;
                psz_spaces = psz_spaces_longtext;
                fprintf( stdout, "%s", psz_spaces );
                goto description;
            }
        }
    }

    /* Release the module list */
    vlc_list_release( p_list );
}

/*****************************************************************************
 * ListModules: list the available modules with their description
 *****************************************************************************
 * Print a list of all available modules (builtins and plugins) and a short
 * description for each one.
 *****************************************************************************/
static void ListModules( vlc_t *p_this )
{
    vlc_list_t *p_list;
    module_t *p_parser;
    char psz_spaces[22];
    int i_index;

    memset( psz_spaces, ' ', 22 );

#ifdef WIN32
    ShowConsole();
#endif

    /* List all modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    /* Enumerate each module */
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        int i;

        p_parser = (module_t *)p_list->p_values[i_index].p_object ;

        /* Nasty hack, but right now I'm too tired to think about a nice
         * solution */
        i = 22 - strlen( p_parser->psz_object_name ) - 1;
        if( i < 0 ) i = 0;
        psz_spaces[i] = 0;

        fprintf( stdout, "  %s%s %s\n", p_parser->psz_object_name,
                         psz_spaces, p_parser->psz_longname );

        psz_spaces[i] = ' ';
    }

    vlc_list_release( p_list );

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
    PauseConsole();
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

    fprintf( stdout, _("VLC version %s\n"), VLC_Version() );
    fprintf( stdout, _("Compiled by %s@%s.%s\n"),
             VLC_CompileBy(), VLC_CompileHost(), VLC_CompileDomain() );
    fprintf( stdout, _("Compiler: %s\n"), VLC_Compiler() );
    if( strcmp( VLC_Changeset(), "exported" ) )
        fprintf( stdout, _("Based upon svn changeset [%s]\n"),
                 VLC_Changeset() );
    fprintf( stdout, LICENSE_MSG );

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
    PauseConsole();
#endif
}

/*****************************************************************************
 * ShowConsole: On Win32, create an output console for debug messages
 *****************************************************************************
 * This function is useful only on Win32.
 *****************************************************************************/
#ifdef WIN32 /*  */
static void ShowConsole( void )
{
#   ifndef UNDER_CE
    FILE *f_help;

    if( getenv( "PWD" ) && getenv( "PS1" ) ) return; /* cygwin shell */

    AllocConsole();

    freopen( "CONOUT$", "w", stderr );
    freopen( "CONIN$", "r", stdin );

    if( (f_help = fopen( "vlc-help.txt", "wt" )) )
    {
        fclose( f_help );
        freopen( "vlc-help.txt", "wt", stdout );
        fprintf( stderr, _("\nDumped content to vlc-help.txt file.\n") );
    }

    else freopen( "CONOUT$", "w", stdout );

#   endif
}
#endif

/*****************************************************************************
 * PauseConsole: On Win32, wait for a key press before closing the console
 *****************************************************************************
 * This function is useful only on Win32.
 *****************************************************************************/
#ifdef WIN32 /*  */
static void PauseConsole( void )
{
#   ifndef UNDER_CE

    if( getenv( "PWD" ) && getenv( "PS1" ) ) return; /* cygwin shell */

    fprintf( stderr, _("\nPress the RETURN key to continue...\n") );
    getchar();
    fclose( stdout );

#   endif
}
#endif

/*****************************************************************************
 * ConsoleWidth: Return the console width in characters
 *****************************************************************************
 * We use the stty shell command to get the console width; if this fails or
 * if the width is less than 80, we default to 80.
 *****************************************************************************/
static int ConsoleWidth( void )
{
    int i_width = 80;

#ifndef WIN32
    char buf[20], *psz_parser;
    FILE *file;
    int i_ret;

    file = popen( "stty size 2>/dev/null", "r" );
    if( file )
    {
        i_ret = fread( buf, 1, 20, file );
        if( i_ret > 0 )
        {
            buf[19] = '\0';
            psz_parser = strchr( buf, ' ' );
            if( psz_parser )
            {
                i_ret = atoi( psz_parser + 1 );
                if( i_ret >= 80 )
                {
                    i_width = i_ret;
                }
            }
        }

        pclose( file );
    }
#endif

    return i_width;
}

static int VerboseCallback( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param)
{
    vlc_t *p_vlc = (vlc_t *)p_this;

    if( new_val.i_int >= -1 )
    {
        p_vlc->p_libvlc->i_verbose = __MIN( new_val.i_int, 2 );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * InitDeviceValues: initialize device values
 *****************************************************************************
 * This function inits the dvd, vcd and cd-audio values
 *****************************************************************************/
static void InitDeviceValues( vlc_t *p_vlc )
{
#ifdef HAVE_HAL
    LibHalContext * ctx;
    int i, i_devices;
    char **devices;
    char *block_dev;
    dbus_bool_t b_dvd;
    DBusConnection *p_connection;
    DBusError       error;

#ifdef HAVE_HAL_1
    ctx =  libhal_ctx_new();
    if( !ctx ) return;
    dbus_error_init( &error );
    p_connection = dbus_bus_get ( DBUS_BUS_SYSTEM, &error );
    if( dbus_error_is_set( &error ) )
    {
        dbus_error_free( &error );
        return;
    }
    libhal_ctx_set_dbus_connection( ctx, p_connection );
    if( libhal_ctx_init( ctx, &error ) )
#else
    if( ( ctx = hal_initialize( NULL, FALSE ) ) )
#endif
    {
#ifdef HAVE_HAL_1
        if( ( devices = libhal_get_all_devices( ctx, &i_devices, NULL ) ) )
#else
        if( ( devices = hal_get_all_devices( ctx, &i_devices ) ) )
#endif
        {
            for( i = 0; i < i_devices; i++ )
            {
#ifdef HAVE_HAL_1
                if( !libhal_device_property_exists( ctx, devices[i],
                                                "storage.cdrom.dvd", NULL ) )
#else
                if( !hal_device_property_exists( ctx, devices[ i ],
                                                "storage.cdrom.dvd" ) )
#endif
                {
                    continue;
                }
#ifdef HAVE_HAL_1
                b_dvd = libhal_device_get_property_bool( ctx, devices[ i ],
                                                 "storage.cdrom.dvd", NULL  );
                block_dev = libhal_device_get_property_string( ctx,
                                devices[ i ], "block.device" , NULL );
#else
                b_dvd = hal_device_get_property_bool( ctx, devices[ i ],
                                                      "storage.cdrom.dvd" );
                block_dev = hal_device_get_property_string( ctx, devices[ i ],
                                                            "block.device" );
#endif
                if( b_dvd )
                {
                    config_PutPsz( p_vlc, "dvd", block_dev );
                }

                config_PutPsz( p_vlc, "vcd", block_dev );
                config_PutPsz( p_vlc, "cd-audio", block_dev );
#ifdef HAVE_HAL_1
                libhal_free_string( block_dev );
#else
                hal_free_string( block_dev );
#endif
            }
#ifdef HAVE_HAL_1
            libhal_free_string_array( devices );
#else
            hal_free_string_array( devices );
#endif
        }

#ifdef HAVE_HAL_1
        libhal_ctx_shutdown( ctx, NULL );
#else
        hal_shutdown( ctx );
#endif
    }
    else
    {
        msg_Warn( p_vlc, "Unable to get HAL device properties" );
    }
#endif
}

/*****************************************************************************
 * FromLocale: converts a locale string to UTF-8
 *****************************************************************************/
char *FromLocale( const char *locale )
{
    if( locale == NULL )
        return NULL;

    if( libvlc.from_locale != (vlc_iconv_t)(-1) )
    {
        char *iptr = (char *)locale, *output, *optr;
        size_t inb, outb;

        /*
         * We are not allowed to modify the locale pointer, even if we cast it
         * to non-const.
         */
        inb = strlen( locale );
        outb = inb * 6 + 1;

        /* FIXME: I'm not sure about the value for the multiplication
         * (for western people, multiplication by 3 (Latin9) is sufficient) */
        optr = output = calloc( outb , 1);

        vlc_mutex_lock( &libvlc.from_locale_lock );
        vlc_iconv( libvlc.from_locale, NULL, NULL, NULL, NULL );

        while( vlc_iconv( libvlc.from_locale, &iptr, &inb, &optr, &outb )
                                                               == (size_t)-1 )
        {
            *optr = '?';
            optr++;
            iptr++;
            vlc_iconv( libvlc.from_locale, NULL, NULL, NULL, NULL );
        }
        vlc_mutex_unlock( &libvlc.from_locale_lock );

        return realloc( output, strlen( output ) + 1 );
    }
    return (char *)locale;
}

/*****************************************************************************
 * ToLocale: converts an UTF-8 string to locale
 *****************************************************************************/
char *ToLocale( const char *utf8 )
{
    if( utf8 == NULL )
        return NULL;

    if( libvlc.to_locale != (vlc_iconv_t)(-1) )
    {
        char *iptr = (char *)utf8, *output, *optr;
        size_t inb, outb;

        /*
         * We are not allowed to modify the locale pointer, even if we cast it
         * to non-const.
         */
        inb = strlen( utf8 );
        /* FIXME: I'm not sure about the value for the multiplication
         * (for western people, multiplication is not needed) */
        outb = inb * 2 + 1;

        optr = output = calloc( outb, 1 );
        vlc_mutex_lock( &libvlc.to_locale_lock );
        vlc_iconv( libvlc.to_locale, NULL, NULL, NULL, NULL );

        while( vlc_iconv( libvlc.to_locale, &iptr, &inb, &optr, &outb )
                                                               == (size_t)-1 )
        {
            *optr = '?'; /* should not happen, and yes, it sucks */
            optr++;
            iptr++;
            vlc_iconv( libvlc.to_locale, NULL, NULL, NULL, NULL );
        }
        vlc_mutex_unlock( &libvlc.to_locale_lock );

        return realloc( output, strlen( output ) + 1 );
    }
    return (char *)utf8;
}

void LocaleFree( const char *str )
{
    if( ( str != NULL ) && ( libvlc.to_locale != (vlc_iconv_t)(-1) ) )
        free( (char *)str );
}
