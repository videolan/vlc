/*****************************************************************************
 * libvlc-common.c: libvlc instances creation and deletion, interfaces handling
 *****************************************************************************
 * Copyright (C) 1998-2006 the VideoLAN team
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Rémi Denis-Courmont <rem # videolan : org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

/** \file
 * This file contains functions to create and destroy libvlc instances
 */

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc/vlc.h>
#include "control/libvlc_internal.h"
#include <vlc_input.h>

#include "modules/modules.h"
#include "config/configuration.h"

#include <errno.h>                                                 /* ENOMEM */
#include <stdio.h>                                              /* sprintf() */
#include <string.h>
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

#ifdef HAVE_DBUS_3
/* used for one-instance mode */
#   include <dbus/dbus.h>
#endif

#ifdef HAVE_HAL
#   include <hal/libhal.h>
#endif

#include "vlc_os_specific.h"

#include <vlc_playlist.h>
#include <vlc_interface.h>

#include <vlc_aout.h>
#include "audio_output/aout_internal.h"

#include <vlc_vout.h>

#include <vlc_sout.h>
#include "stream_output/stream_output.h"

#include <vlc_charset.h>

#include "libvlc.h"

#include "playlist/playlist_internal.h"

/*****************************************************************************
 * The evil global variable. We handle it with care, don't worry.
 *****************************************************************************/
static libvlc_global_data_t   libvlc_global;
static libvlc_global_data_t *p_libvlc_global = &libvlc_global;
static libvlc_int_t *    p_static_vlc = NULL;
static volatile unsigned int i_instances = 0;

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#if defined (__APPLE__) || defined (WIN32)
static void SetLanguage   ( char const * );
#endif
static inline int LoadMessages (void);
static int  GetFilenames  ( libvlc_int_t *, int, const char *[] );
static void Help          ( libvlc_int_t *, char const *psz_help_name );
static void Usage         ( libvlc_int_t *, char const *psz_module_name );
static void ListModules   ( libvlc_int_t *, vlc_bool_t );
static void Version       ( void );

#ifdef WIN32
static void ShowConsole   ( vlc_bool_t );
static void PauseConsole  ( void );
#endif
static int  ConsoleWidth  ( void );

static int  VerboseCallback( vlc_object_t *, char const *,
                             vlc_value_t, vlc_value_t, void * );

static void InitDeviceValues( libvlc_int_t * );

libvlc_global_data_t *vlc_global( void )
{
    return &libvlc_global;
}

/*****************************************************************************
 * vlc_current_object: return the current object.
 *****************************************************************************
 * If i_object is non-zero, return the corresponding object. Otherwise,
 * return the statically allocated p_vlc object.
 *****************************************************************************/
libvlc_int_t * vlc_current_object( int i_object )
{
    return i_object ? vlc_object_get( i_object ) : p_static_vlc;
}


/**
 * Allocate a libvlc instance, initialize global data if needed
 * It also initializes the threading system
 */
libvlc_int_t * libvlc_InternalCreate( void )
{
    int i_ret;
    libvlc_int_t * p_libvlc = NULL;
    vlc_value_t lockval;
    char *psz_env = NULL;

#if 0
    /* &libvlc_global never changes,
     * so we can safely call this multiple times. */
    p_libvlc_global = &libvlc_global;
#endif

    /* vlc_threads_init *must* be the first internal call! No other call is
     * allowed before the thread system has been initialized. */
    i_ret = vlc_threads_init( p_libvlc_global );
    if( i_ret < 0 ) return NULL;

    /* Now that the thread system is initialized, we don't have much, but
     * at least we have var_Create */
    var_Create( p_libvlc_global, "libvlc", VLC_VAR_MUTEX );
    var_Get( p_libvlc_global, "libvlc", &lockval );
    vlc_mutex_lock( lockval.p_address );

    i_instances++;

    if( !libvlc_global.b_ready )
    {
        /* Guess what CPU we have */
        cpu_flags = CPUCapabilities();
       /* The module bank will be initialized later */
        libvlc_global.p_module_bank = NULL;

        libvlc_global.b_ready = VLC_TRUE;
    }
    vlc_mutex_unlock( lockval.p_address );
    var_Destroy( p_libvlc_global, "libvlc" );

    /* Allocate a libvlc instance object */
    p_libvlc = vlc_object_create( p_libvlc_global, VLC_OBJECT_LIBVLC );
    if( p_libvlc == NULL )
    {
        i_instances--;
        return NULL;
    }
    p_libvlc->p_playlist = NULL;
    p_libvlc->psz_object_name = "libvlc";

    /* Initialize message queue */
    msg_Create( p_libvlc );

    /* Find verbosity from VLC_VERBOSE environment variable */
    psz_env = getenv( "VLC_VERBOSE" );
    if( psz_env != NULL )
        p_libvlc->i_verbose = atoi( psz_env );
    else
        p_libvlc->i_verbose = 3;
#if defined( HAVE_ISATTY ) && !defined( WIN32 )
    p_libvlc->b_color = isatty( 2 ); /* 2 is for stderr */
#else
    p_libvlc->b_color = VLC_FALSE;
#endif

    /* Announce who we are - Do it only for first instance ? */
    msg_Dbg( p_libvlc, COPYRIGHT_MESSAGE );
    msg_Dbg( p_libvlc, "libvlc was configured with %s", CONFIGURE_LINE );

    /* Initialize mutexes */
    vlc_mutex_init( p_libvlc, &p_libvlc->config_lock );
#ifdef __APPLE__
    vlc_mutex_init( p_libvlc, &p_libvlc->quicktime_lock );
    vlc_thread_set_priority( p_libvlc, VLC_THREAD_PRIORITY_LOW );
#endif
    /* Store data for the non-reentrant API */
    p_static_vlc = p_libvlc;

    return p_libvlc;
}

/**
 * Initialize a libvlc instance
 * This function initializes a previously allocated libvlc instance:
 *  - CPU detection
 *  - gettext initialization
 *  - message queue, module bank and playlist initialization
 *  - configuration and commandline parsing
 */
int libvlc_InternalInit( libvlc_int_t *p_libvlc, int i_argc,
                         const char *ppsz_argv[] )
{
    char         p_capabilities[200];
    char *       p_tmp = NULL;
    char *       psz_modules = NULL;
    char *       psz_parser = NULL;
    char *       psz_control = NULL;
    vlc_bool_t   b_exit = VLC_FALSE;
    int          i_ret = VLC_EEXIT;
    playlist_t  *p_playlist = NULL;
    vlc_value_t  val;
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )
# if defined (WIN32) || defined (__APPLE__)
    char *       psz_language;
#endif
#endif

    /* System specific initialization code */
    system_Init( p_libvlc, &i_argc, ppsz_argv );

    /* Get the executable name (similar to the basename command) */
    if( i_argc > 0 )
    {
        const char *exe = p_libvlc->psz_object_name = ppsz_argv[0];
        while( *exe )
        {
            if( *exe++ == '/' )
                p_libvlc->psz_object_name = exe;
        }
    }
    else
    {
        p_libvlc->psz_object_name = "vlc";
    }

    /*
     * Support for gettext
     */
    LoadMessages ();

    /* Translate "C" to the language code: "fr", "en_GB", "nl", "ru"... */
    msg_Dbg( p_libvlc, "translation test: code is \"%s\"", _("C") );

    /* Initialize the module bank and load the configuration of the
     * main module. We need to do this at this stage to be able to display
     * a short help if required by the user. (short help == main module
     * options) */
    module_InitBank( p_libvlc );

    if( config_LoadCmdLine( p_libvlc, &i_argc, ppsz_argv, VLC_TRUE ) )
    {
        module_EndBank( p_libvlc );
        return VLC_EGENERIC;
    }

    /* Check for short help option */
    if( config_GetInt( p_libvlc, "help" ) > 0 )
    {
        Help( p_libvlc, "help" );
        b_exit = VLC_TRUE;
        i_ret = VLC_EEXITSUCCESS;
    }
    /* Check for version option */
    else if( config_GetInt( p_libvlc, "version" ) > 0 )
    {
        Version();
        b_exit = VLC_TRUE;
        i_ret = VLC_EEXITSUCCESS;
    }

    /* Set the config file stuff */
    p_libvlc->psz_homedir    = config_GetHomeDir();
    p_libvlc->psz_configdir  = config_GetConfigDir( p_libvlc );
    p_libvlc->psz_datadir    = config_GetUserDataDir( p_libvlc );
    p_libvlc->psz_cachedir   = config_GetCacheDir( p_libvlc );
    p_libvlc->psz_configfile = config_GetCustomConfigFile( p_libvlc );

    /* Check for plugins cache options */
    if( config_GetInt( p_libvlc, "reset-plugins-cache" ) > 0 )
    {
        libvlc_global.p_module_bank->b_cache_delete = VLC_TRUE;
    }

    /* Will be re-done properly later on */
    p_libvlc->i_verbose = config_GetInt( p_libvlc, "verbose" );

    /* Check for daemon mode */
#ifndef WIN32
    if( config_GetInt( p_libvlc, "daemon" ) )
    {
#if HAVE_DAEMON
        char *psz_pidfile = NULL;

        if( daemon( 1, 0) != 0 )
        {
            msg_Err( p_libvlc, "Unable to fork vlc to daemon mode" );
            b_exit = VLC_TRUE;
        }
        libvlc_global.b_daemon = VLC_TRUE;

        /* lets check if we need to write the pidfile */
        psz_pidfile = config_GetPsz( p_libvlc, "pidfile" );
        if( psz_pidfile != NULL )
        {
            FILE *pidfile;
            pid_t i_pid = getpid ();
            msg_Dbg( p_libvlc, "PID is %d, writing it to %s",
                               i_pid, psz_pidfile );
            pidfile = utf8_fopen( psz_pidfile,"w" );
            if( pidfile != NULL )
            {
                utf8_fprintf( pidfile, "%d", (int)i_pid );
                fclose( pidfile );
            }
            else
            {
                msg_Err( p_libvlc, "cannot open pid file for writing: %s (%m)",
                         psz_pidfile );
            }
        }
        free( psz_pidfile );

#else
        pid_t i_pid;

        if( ( i_pid = fork() ) < 0 )
        {
            msg_Err( p_libvlc, "unable to fork vlc to daemon mode" );
            b_exit = VLC_TRUE;
        }
        else if( i_pid )
        {
            /* This is the parent, exit right now */
            msg_Dbg( p_libvlc, "closing parent process" );
            b_exit = VLC_TRUE;
            i_ret = VLC_EEXITSUCCESS;
        }
        else
        {
            /* We are the child */
            msg_Dbg( p_libvlc, "daemon spawned" );
            close( STDIN_FILENO );
            close( STDOUT_FILENO );
            close( STDERR_FILENO );

            libvlc_global.b_daemon = VLC_TRUE;
        }
#endif
    }
#endif

    if( b_exit )
    {
        module_EndBank( p_libvlc );
        return i_ret;
    }

    /* Check for translation config option */
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )
# if defined (WIN32) || defined (__APPLE__)
    /* This ain't really nice to have to reload the config here but it seems
     * the only way to do it. */
    config_LoadConfigFile( p_libvlc, "main" );
    config_LoadCmdLine( p_libvlc, &i_argc, ppsz_argv, VLC_TRUE );

    /* Check if the user specified a custom language */
    psz_language = config_GetPsz( p_libvlc, "language" );
    if( psz_language && *psz_language && strcmp( psz_language, "auto" ) )
    {
        vlc_bool_t b_cache_delete = libvlc_global.p_module_bank->b_cache_delete;

        /* Reset the default domain */
        SetLanguage( psz_language );

        /* Translate "C" to the language code: "fr", "en_GB", "nl", "ru"... */
        msg_Dbg( p_libvlc, "translation test: code is \"%s\"", _("C") );

        module_EndBank( p_libvlc );
        module_InitBank( p_libvlc );
        config_LoadConfigFile( p_libvlc, "main" );
        config_LoadCmdLine( p_libvlc, &i_argc, ppsz_argv, VLC_TRUE );
        libvlc_global.p_module_bank->b_cache_delete = b_cache_delete;
    }
    if( psz_language ) free( psz_language );
# endif
#endif

    /*
     * Load the builtins and plugins into the module_bank.
     * We have to do it before config_Load*() because this also gets the
     * list of configuration options exported by each module and loads their
     * default values.
     */
    module_LoadBuiltins( p_libvlc );
    module_LoadPlugins( p_libvlc );
    if( p_libvlc->b_die )
    {
        b_exit = VLC_TRUE;
    }

    msg_Dbg( p_libvlc, "module bank initialized, found %i modules",
                    libvlc_global.p_module_bank->i_children );

    /* Check for help on modules */
    if( (p_tmp = config_GetPsz( p_libvlc, "module" )) )
    {
        Help( p_libvlc, p_tmp );
        free( p_tmp );
        b_exit = VLC_TRUE;
        i_ret = VLC_EEXITSUCCESS;
    }
    /* Check for long help option */
    else if( config_GetInt( p_libvlc, "longhelp" ) > 0 )
    {
        Help( p_libvlc, "longhelp" );
        b_exit = VLC_TRUE;
        i_ret = VLC_EEXITSUCCESS;
    }
    /* Check for module list option */
    else if( config_GetInt( p_libvlc, "list" ) > 0 )
    {
        ListModules( p_libvlc, VLC_FALSE );
        b_exit = VLC_TRUE;
        i_ret = VLC_EEXITSUCCESS;
    }
    else if( config_GetInt( p_libvlc, "list-verbose" ) > 0 )
    {
        ListModules( p_libvlc, VLC_TRUE );
        b_exit = VLC_TRUE;
        i_ret = VLC_EEXITSUCCESS;
    }

    /* Check for config file options */
    if( config_GetInt( p_libvlc, "reset-config" ) > 0 )
    {
        config_ResetAll( p_libvlc );
        config_LoadCmdLine( p_libvlc, &i_argc, ppsz_argv, VLC_TRUE );
        config_SaveConfigFile( p_libvlc, NULL );
    }
    if( config_GetInt( p_libvlc, "save-config" ) > 0 )
    {
        config_LoadConfigFile( p_libvlc, NULL );
        config_LoadCmdLine( p_libvlc, &i_argc, ppsz_argv, VLC_TRUE );
        config_SaveConfigFile( p_libvlc, NULL );
    }

    if( b_exit )
    {
        module_EndBank( p_libvlc );
        return i_ret;
    }

    /*
     * Init device values
     */
    InitDeviceValues( p_libvlc );

    /*
     * Override default configuration with config file settings
     */
    config_LoadConfigFile( p_libvlc, NULL );

    /*
     * Override configuration with command line settings
     */
    if( config_LoadCmdLine( p_libvlc, &i_argc, ppsz_argv, VLC_FALSE ) )
    {
#ifdef WIN32
        ShowConsole( VLC_FALSE );
        /* Pause the console because it's destroyed when we exit */
        fprintf( stderr, "The command line options couldn't be loaded, check "
                 "that they are valid.\n" );
        PauseConsole();
#endif
        module_EndBank( p_libvlc );
        return VLC_EGENERIC;
    }

    /*
     * System specific configuration
     */
    system_Configure( p_libvlc, &i_argc, ppsz_argv );

/* FIXME: could be replaced by using Unix sockets */
#ifdef HAVE_DBUS_3
    dbus_threads_init_default();

    if( config_GetInt( p_libvlc, "one-instance" ) > 0 )
    {
        /* Initialise D-Bus interface, check for other instances */
        DBusConnection  *p_conn = NULL;
        DBusError       dbus_error;

        dbus_error_init( &dbus_error );

        /* connect to the session bus */
        p_conn = dbus_bus_get( DBUS_BUS_SESSION, &dbus_error );
        if( !p_conn )
        {
            msg_Err( p_libvlc, "Failed to connect to D-Bus session daemon: %s",
                    dbus_error.message );
            dbus_error_free( &dbus_error );
        }
        else
        {
            /* check if VLC is available on the bus
             * if not: D-Bus control is not enabled on the other
             * instance and we can't pass MRLs to it */
            DBusMessage *p_test_msg = NULL;
            DBusMessage *p_test_reply = NULL;
            p_test_msg =  dbus_message_new_method_call(
                    "org.mpris.vlc", "/",
                    "org.freedesktop.MediaPlayer", "Identity" );
            /* block until a reply arrives */
            p_test_reply = dbus_connection_send_with_reply_and_block(
                    p_conn, p_test_msg, -1, &dbus_error );
            dbus_message_unref( p_test_msg );
            if( p_test_reply == NULL )
            {
                dbus_error_free( &dbus_error );
                msg_Dbg( p_libvlc, "No Media Player is running. "
                        "Continuing normally." );
            }
            else
            {
                int i_input;
                DBusMessage* p_dbus_msg = NULL;
                DBusMessageIter dbus_args;
                DBusPendingCall* p_dbus_pending = NULL;
                dbus_bool_t b_play;

                dbus_message_unref( p_test_reply );
                msg_Warn( p_libvlc, "Another Media Player is running. Exiting");

                for( i_input = optind;i_input < i_argc;i_input++ )
                {
                    msg_Dbg( p_libvlc, "Adds %s to the running Media Player",
                            ppsz_argv[i_input] );

                    p_dbus_msg = dbus_message_new_method_call(
                            "org.mpris.vlc", "/TrackList",
                            "org.freedesktop.MediaPlayer", "AddTrack" );

                    if ( NULL == p_dbus_msg )
                    {
                        msg_Err( p_libvlc, "D-Bus problem" );
                        system_End( p_libvlc );
                        exit( VLC_ETIMEOUT );
                    }

                    /* append MRLs */
                    dbus_message_iter_init_append( p_dbus_msg, &dbus_args );
                    if ( !dbus_message_iter_append_basic( &dbus_args,
                                DBUS_TYPE_STRING, &ppsz_argv[i_input] ) )
                    {
                        msg_Err( p_libvlc, "Out of memory" );
                        dbus_message_unref( p_dbus_msg );
                        system_End( p_libvlc );
                        exit( VLC_ENOMEM );
                    }
                    b_play = TRUE;
                    if( config_GetInt( p_libvlc, "playlist-enqueue" ) > 0 )
                        b_play = FALSE;
                    if ( !dbus_message_iter_append_basic( &dbus_args,
                                DBUS_TYPE_BOOLEAN, &b_play ) )
                    {
                        msg_Err( p_libvlc, "Out of memory" );
                        dbus_message_unref( p_dbus_msg );
                        system_End( p_libvlc );
                        exit( VLC_ENOMEM );
                    }

                    /* send message and get a handle for a reply */
                    if ( !dbus_connection_send_with_reply ( p_conn,
                                p_dbus_msg, &p_dbus_pending, -1 ) )
                    {
                        msg_Err( p_libvlc, "D-Bus problem" );
                        dbus_message_unref( p_dbus_msg );
                        system_End( p_libvlc );
                        exit( VLC_ETIMEOUT );
                    }

                    if ( NULL == p_dbus_pending )
                    {
                        msg_Err( p_libvlc, "D-Bus problem" );
                        dbus_message_unref( p_dbus_msg );
                        system_End( p_libvlc );
                        exit( VLC_ETIMEOUT );
                    }
                    dbus_connection_flush( p_conn );
                    dbus_message_unref( p_dbus_msg );
                    /* block until we receive a reply */
                    dbus_pending_call_block( p_dbus_pending );
                    dbus_pending_call_unref( p_dbus_pending );
                } /* processes all command line MRLs */

                /* bye bye */
                system_End( p_libvlc );
                exit( VLC_SUCCESS );
            }
        }
        /* we unreference the connection when we've finished with it */
        if( p_conn ) dbus_connection_unref( p_conn );
    }
#endif

    /*
     * Message queue options
     */

    var_Create( p_libvlc, "verbose", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    if( config_GetInt( p_libvlc, "quiet" ) > 0 )
    {
        val.i_int = -1;
        var_Set( p_libvlc, "verbose", val );
    }
    var_AddCallback( p_libvlc, "verbose", VerboseCallback, NULL );
    var_Change( p_libvlc, "verbose", VLC_VAR_TRIGGER_CALLBACKS, NULL, NULL );

    if( p_libvlc->b_color )
        p_libvlc->b_color = config_GetInt( p_libvlc, "color" ) > 0;

    /*
     * Output messages that may still be in the queue
     */
    msg_Flush( p_libvlc );

    if( !config_GetInt( p_libvlc, "fpu" ) )
        cpu_flags &= ~CPU_CAPABILITY_FPU;

#if defined( __i386__ ) || defined( __x86_64__ )
    if( !config_GetInt( p_libvlc, "mmx" ) )
        cpu_flags &= ~CPU_CAPABILITY_MMX;
    if( !config_GetInt( p_libvlc, "3dn" ) )
        cpu_flags &= ~CPU_CAPABILITY_3DNOW;
    if( !config_GetInt( p_libvlc, "mmxext" ) )
        cpu_flags &= ~CPU_CAPABILITY_MMXEXT;
    if( !config_GetInt( p_libvlc, "sse" ) )
        cpu_flags &= ~CPU_CAPABILITY_SSE;
    if( !config_GetInt( p_libvlc, "sse2" ) )
        cpu_flags &= ~CPU_CAPABILITY_SSE2;
#endif
#if defined( __powerpc__ ) || defined( __ppc__ ) || defined( __ppc64__ )
    if( !config_GetInt( p_libvlc, "altivec" ) )
        cpu_flags &= ~CPU_CAPABILITY_ALTIVEC;
#endif

#define PRINT_CAPABILITY( capability, string )                              \
    if( vlc_CPU() & capability )                                            \
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
    msg_Dbg( p_libvlc, "CPU has capabilities %s", p_capabilities );

    /*
     * Choose the best memcpy module
     */
    p_libvlc->p_memcpy_module = module_Need( p_libvlc, "memcpy", "$memcpy", 0 );

    if( p_libvlc->pf_memcpy == NULL )
    {
        p_libvlc->pf_memcpy = memcpy;
    }

    if( p_libvlc->pf_memset == NULL )
    {
        p_libvlc->pf_memset = memset;
    }

    p_libvlc->b_stats = config_GetInt( p_libvlc, "stats" ) > 0;
    p_libvlc->i_timers = 0;
    p_libvlc->pp_timers = NULL;
    vlc_mutex_init( p_libvlc, &p_libvlc->timer_lock );

    /*
     * Initialize hotkey handling
     */
    var_Create( p_libvlc, "key-pressed", VLC_VAR_INTEGER );
    p_libvlc->p_hotkeys = malloc( libvlc_hotkeys_size );
    /* Do a copy (we don't need to modify the strings) */
    memcpy( p_libvlc->p_hotkeys, libvlc_hotkeys, libvlc_hotkeys_size );

    /* Initialize playlist and get commandline files */
    playlist_ThreadCreate( p_libvlc );
    if( !p_libvlc->p_playlist )
    {
        msg_Err( p_libvlc, "playlist initialization failed" );
        if( p_libvlc->p_memcpy_module != NULL )
        {
            module_Unneed( p_libvlc, p_libvlc->p_memcpy_module );
        }
        module_EndBank( p_libvlc );
        return VLC_EGENERIC;
    }
    p_playlist = p_libvlc->p_playlist;

    psz_modules = config_GetPsz( p_playlist, "services-discovery" );
    if( psz_modules && *psz_modules )
    {
        /* Add service discovery modules */
        playlist_ServicesDiscoveryAdd( p_playlist, psz_modules );
    }
    if( psz_modules ) free( psz_modules );

    /*
     * Load background interfaces
     */
    psz_modules = config_GetPsz( p_libvlc, "extraintf" );
    psz_control = config_GetPsz( p_libvlc, "control" );

    if( psz_modules && *psz_modules && psz_control && *psz_control )
    {
        psz_modules = (char *)realloc( psz_modules, strlen( psz_modules ) +
                                                    strlen( psz_control ) + 1 );
        sprintf( psz_modules, "%s:%s", psz_modules, psz_control );
    }
    else if( psz_control && *psz_control )
    {
        free( psz_modules );
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
    free( psz_modules );
    free( psz_control );

    /*
     * Always load the hotkeys interface if it exists
     */
    VLC_AddIntf( 0, "hotkeys,none", VLC_FALSE, VLC_FALSE );

#ifdef HAVE_DBUS_3
    /* loads dbus control interface if in one-instance mode
     * we do it only when playlist exists, because dbus module needs it */
    if( config_GetInt( p_libvlc, "one-instance" ) > 0 )
        VLC_AddIntf( 0, "dbus,none", VLC_FALSE, VLC_FALSE );

    /* Prevents the power management daemon to suspend the computer
     * when VLC is active */
    if( config_GetInt( p_libvlc, "inhibit" ) > 0 )
        VLC_AddIntf( 0, "inhibit,none", VLC_FALSE, VLC_FALSE );
#endif

    /*
     * If needed, load the Xscreensaver interface
     * Currently, only for X
     */
#ifdef HAVE_X11_XLIB_H
    if( config_GetInt( p_libvlc, "disable-screensaver" ) )
    {
        VLC_AddIntf( 0, "screensaver,none", VLC_FALSE, VLC_FALSE );
    }
#endif

    if( config_GetInt( p_libvlc, "file-logging" ) > 0 )
    {
        VLC_AddIntf( 0, "logger,none", VLC_FALSE, VLC_FALSE );
    }
#ifdef HAVE_SYSLOG_H
    if( config_GetInt( p_libvlc, "syslog" ) > 0 )
    {
        const char *psz_logmode = "logmode=syslog";
        libvlc_InternalAddIntf( p_libvlc, "logger,none", VLC_FALSE, VLC_FALSE,
                                1, &psz_logmode );
    }
#endif

    if( config_GetInt( p_libvlc, "show-intf" ) > 0 )
    {
        VLC_AddIntf( 0, "showintf,none", VLC_FALSE, VLC_FALSE );
    }

    if( config_GetInt( p_libvlc, "network-synchronisation") > 0 )
    {
        VLC_AddIntf( 0, "netsync,none", VLC_FALSE, VLC_FALSE );
    }

#ifdef WIN32
    if( config_GetInt( p_libvlc, "prefer-system-codecs") > 0 )
    {
        char *psz_codecs = config_GetPsz( p_playlist, "codec" );
        if( psz_codecs )
        {
            char *psz_morecodecs;
            asprintf(&psz_morecodecs, "%s,dmo,quicktime", psz_codecs);
            if( psz_morecodecs )
            {
                config_PutPsz( p_libvlc, "codec", psz_morecodecs);
                free(psz_morecodecs);
            }
        }
        else
            config_PutPsz( p_libvlc, "codec", "dmo,quicktime");
        free(psz_codecs);
    }
#endif

    /*
     * FIXME: kludge to use a p_libvlc-local variable for the Mozilla plugin
     */
    var_Create( p_libvlc, "drawable", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-view-top", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-view-left", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-view-bottom", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-view-right", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-clip-top", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-clip-left", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-clip-bottom", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-clip-right", VLC_VAR_INTEGER );

    /* Create volume callback system. */
    var_Create( p_libvlc, "volume-change", VLC_VAR_BOOL );

    /*
     * Get input filenames given as commandline arguments
     */
    GetFilenames( p_libvlc, i_argc, ppsz_argv );

    /*
     * Get --open argument
     */
    var_Create( p_libvlc, "open", VLC_VAR_STRING | VLC_VAR_DOINHERIT );
    var_Get( p_libvlc, "open", &val );
    if ( val.psz_string != NULL && *val.psz_string )
    {
        VLC_AddTarget( p_libvlc->i_object_id, val.psz_string, NULL, 0,
                       PLAYLIST_INSERT, 0 );
    }
    if ( val.psz_string != NULL ) free( val.psz_string );

    return VLC_SUCCESS;
}

/**
 * Cleanup a libvlc instance. The instance is not completely deallocated
 * \param p_libvlc the instance to clean
 */
int libvlc_InternalCleanup( libvlc_int_t *p_libvlc )
{
    intf_thread_t      * p_intf = NULL;
    vout_thread_t      * p_vout = NULL;
    aout_instance_t    * p_aout = NULL;
    announce_handler_t * p_announce = NULL;

    /* Ask the interfaces to stop and destroy them */
    msg_Dbg( p_libvlc, "removing all interfaces" );
    while( (p_intf = vlc_object_find( p_libvlc, VLC_OBJECT_INTF, FIND_CHILD )) )
    {
        intf_StopThread( p_intf );
        vlc_object_detach( p_intf );
        vlc_object_release( p_intf );
        intf_Destroy( p_intf );
        p_intf = NULL;
    }

    /* Free playlist */
    msg_Dbg( p_libvlc, "removing playlist" );
    playlist_ThreadDestroy( p_libvlc->p_playlist );

    /* Free video outputs */
    msg_Dbg( p_libvlc, "removing all video outputs" );
    while( (p_vout = vlc_object_find( p_libvlc, VLC_OBJECT_VOUT, FIND_CHILD )) )
    {
        vlc_object_detach( p_vout );
        vlc_object_release( p_vout );
        vout_Destroy( p_vout );
    }

    /* Free audio outputs */
    msg_Dbg( p_libvlc, "removing all audio outputs" );
    while( (p_aout = vlc_object_find( p_libvlc, VLC_OBJECT_AOUT, FIND_CHILD )) )
    {
        vlc_object_detach( (vlc_object_t *)p_aout );
        vlc_object_release( (vlc_object_t *)p_aout );
        aout_Delete( p_aout );
    }

    stats_TimersDumpAll( p_libvlc );
    stats_TimersClean( p_libvlc );

    /* Free announce handler(s?) */
    while( (p_announce = vlc_object_find( p_libvlc, VLC_OBJECT_ANNOUNCE,
                                                 FIND_CHILD ) ) )
    {
        msg_Dbg( p_libvlc, "removing announce handler" );
        vlc_object_detach( p_announce );
        vlc_object_release( p_announce );
        announce_HandlerDestroy( p_announce );
    }
    return VLC_SUCCESS;
}

/**
 * Destroy everything.
 * This function requests the running threads to finish, waits for their
 * termination, and destroys their structure.
 * It stops the thread systems: no instance can run after this has run
 * \param p_libvlc the instance to destroy
 * \param b_release whether we should do a release on the instance
 */
int libvlc_InternalDestroy( libvlc_int_t *p_libvlc, vlc_bool_t b_release )
{
    vlc_value_t lockval;

    if( !p_libvlc )
        return VLC_EGENERIC;

#ifndef WIN32
    char* psz_pidfile = NULL;

    if( libvlc_global.p_module_bank )
    if( config_GetInt( p_libvlc, "daemon" ) > 0 )
    {
        psz_pidfile = config_GetPsz( p_libvlc, "pidfile" );
        if( psz_pidfile != NULL )
        {
            msg_Dbg( p_libvlc, "removing pid file %s", psz_pidfile );
            if( unlink( psz_pidfile ) == -1 )
            {
                msg_Dbg( p_libvlc, "removing pid file %s: %m",
                        psz_pidfile );
            }
        }
        free ( psz_pidfile );
    }
#endif

    if( p_libvlc->p_memcpy_module )
    {
        module_Unneed( p_libvlc, p_libvlc->p_memcpy_module );
        p_libvlc->p_memcpy_module = NULL;
    }

    /* Free module bank. It is refcounted, so we call this each time  */
    module_EndBank( p_libvlc );

    FREENULL( p_libvlc->psz_homedir );
    FREENULL( p_libvlc->psz_configdir );
    FREENULL( p_libvlc->psz_datadir );
    FREENULL( p_libvlc->psz_cachedir );
    FREENULL( p_libvlc->psz_configfile );
    FREENULL( p_libvlc->p_hotkeys );

    var_Create( p_libvlc_global, "libvlc", VLC_VAR_MUTEX );
    var_Get( p_libvlc_global, "libvlc", &lockval );
    vlc_mutex_lock( lockval.p_address );
    i_instances--;

    if( i_instances == 0 )
    {
        /* System specific cleaning code */
        system_End( p_libvlc );
    }
    vlc_mutex_unlock( lockval.p_address );
    var_Destroy( p_libvlc_global, "libvlc" );

    msg_Flush( p_libvlc );
    msg_Destroy( p_libvlc );

    /* Destroy mutexes */
    vlc_mutex_destroy( &p_libvlc->config_lock );
    vlc_mutex_destroy( &p_libvlc->timer_lock );

    if( b_release ) vlc_object_release( p_libvlc );
    vlc_object_destroy( p_libvlc );
    p_libvlc = NULL;

    /* Stop thread system: last one out please shut the door!
     * The number of initializations of the thread system is counted, we
     * can call this each time */
    vlc_threads_end( p_libvlc_global );

    return VLC_SUCCESS;
}

/**
 * Add an interface plugin and run it
 */
int libvlc_InternalAddIntf( libvlc_int_t *p_libvlc,
                            char const *psz_module,
                            vlc_bool_t b_block, vlc_bool_t b_play,
                            int i_options, const char *const *ppsz_options )
{
    int i_err;
    intf_thread_t *p_intf = NULL;

    if( !p_libvlc )
        return VLC_EGENERIC;

    if( !psz_module ) /* requesting the default interface */
    {
        char *psz_interface = config_GetPsz( p_libvlc, "intf" );
        if( !psz_interface || !*psz_interface ) /* "intf" has not been set */
            msg_Info( p_libvlc, _("Running vlc with the default interface. Use 'cvlc' to use vlc without interface.") );
        free( psz_interface );
    }

#ifndef WIN32
    if( libvlc_global.b_daemon && b_block && !psz_module )
    {
        /* Daemon mode hack.
         * We prefer the dummy interface if none is specified. */
        char *psz_interface = config_GetPsz( p_libvlc, "intf" );
        if( !psz_interface || !*psz_interface ) psz_module = "dummy";
        if( psz_interface ) free( psz_interface );
    }
#endif

    /* Try to create the interface */
    p_intf = intf_Create( p_libvlc, psz_module ? psz_module : "$intf",
                          i_options, ppsz_options );
    if( p_intf == NULL )
    {
        msg_Err( p_libvlc, "interface \"%s\" initialization failed",
                 psz_module );
        return VLC_EGENERIC;
    }

    /* Interface doesn't handle play on start so do it ourselves */
    if( !p_intf->b_play && b_play )
        playlist_Play( p_libvlc->p_playlist );

    /* Try to run the interface */
    p_intf->b_play = b_play;
    i_err = intf_RunThread( p_intf );
    if( i_err || p_intf->b_should_run_on_first_thread )
    {
        vlc_object_detach( p_intf );
        intf_Destroy( p_intf );
        p_intf = NULL;
        return i_err;
    }

    if( b_block )
    {
        /* FIXME: should be moved to interface/interface.c */
        if( p_intf->pf_run )
            vlc_thread_join( p_intf );
        else
            while( vlc_object_lock_and_wait( p_intf ) == 0 );

        vlc_object_detach( p_intf );
        intf_Destroy( p_intf );
    }

    return VLC_SUCCESS;
};

#if defined (__APPLE__) || defined (WIN32)
/*****************************************************************************
 * SetLanguage: set the interface language.
 *****************************************************************************
 * We set the LC_MESSAGES locale category for interface messages and buttons,
 * as well as the LC_CTYPE category for string sorting and possible wide
 * character support.
 *****************************************************************************/
static void SetLanguage ( const char *psz_lang )
{
#ifdef __APPLE__
    /* I need that under Darwin, please check it doesn't disturb
     * other platforms. --Meuuh */
    setenv( "LANG", psz_lang, 1 );

#else
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
}
#endif


static inline int LoadMessages (void)
{
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )
    /* Specify where to find the locales for current domain */
#if !defined( __APPLE__ ) && !defined( WIN32 ) && !defined( SYS_BEOS )
    static const char psz_path[] = LOCALEDIR;
#else
    char psz_path[1024];
    if (snprintf (psz_path, sizeof (psz_path), "%s/%s",
                  libvlc_global.psz_vlcpath, "locale")
                     >= (int)sizeof (psz_path))
        return -1;

#endif
    if (bindtextdomain (PACKAGE_NAME, psz_path) == NULL)
    {
        fprintf (stderr, "Warning: cannot bind text domain "PACKAGE_NAME
                         " to directory %s\n", psz_path);
        return -1;
    }

    /* LibVLC wants all messages in UTF-8.
     * Unfortunately, we cannot ask UTF-8 for strerror_r(), strsignal_r()
     * and other functions that are not part of our text domain.
     */
    if (bind_textdomain_codeset (PACKAGE_NAME, "UTF-8") == NULL)
    {
        fprintf (stderr, "Error: cannot set Unicode encoding for text domain "
                         PACKAGE_NAME"\n");
        // Unbinds the text domain to avoid broken encoding
        bindtextdomain (PACKAGE_NAME, "DOES_NOT_EXIST");
        return -1;
    }

    /* LibVLC does NOT set the default textdomain, since it is a library.
     * This could otherwise break programs using LibVLC (other than VLC).
     * textdomain (PACKAGE_NAME);
     */
#endif
    return 0;
}

/*****************************************************************************
 * GetFilenames: parse command line options which are not flags
 *****************************************************************************
 * Parse command line for input files as well as their associated options.
 * An option always follows its associated input and begins with a ":".
 *****************************************************************************/
static int GetFilenames( libvlc_int_t *p_vlc, int i_argc, const char *ppsz_argv[] )
{
    int i_opt, i_options;

    /* We assume that the remaining parameters are filenames
     * and their input options */
    for( i_opt = i_argc - 1; i_opt >= optind; i_opt-- )
    {
        i_options = 0;

        /* Count the input options */
        while( *ppsz_argv[ i_opt ] == ':' && i_opt > optind )
        {
            i_options++;
            i_opt--;
        }

        /* TODO: write an internal function of this one, to avoid
         *       unnecessary lookups. */

        VLC_AddTarget( p_vlc->i_object_id, ppsz_argv[i_opt],
                       ( i_options ? &ppsz_argv[i_opt + 1] :
                                        NULL ), i_options,
                       PLAYLIST_INSERT, 0 );
    }

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Help: print program help
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
static void Help( libvlc_int_t *p_this, char const *psz_help_name )
{
#ifdef WIN32
    ShowConsole( VLC_TRUE );
#endif

    if( psz_help_name && !strcmp( psz_help_name, "help" ) )
    {
        utf8_fprintf( stdout, vlc_usage, p_this->psz_object_name );
        Usage( p_this, "help" );
        Usage( p_this, "main" );
    }
    else if( psz_help_name && !strcmp( psz_help_name, "longhelp" ) )
    {
        utf8_fprintf( stdout, vlc_usage, p_this->psz_object_name );
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
static void Usage( libvlc_int_t *p_this, char const *psz_module_name )
{
#define FORMAT_STRING "  %s --%s%s%s%s%s%s%s "
    /* short option ------'    | | | | | | |
     * option name ------------' | | | | | |
     * <bra ---------------------' | | | | |
     * option type or "" ----------' | | | |
     * ket> -------------------------' | | |
     * padding spaces -----------------' | |
     * comment --------------------------' |
     * comment suffix ---------------------'
     *
     * The purpose of having bra and ket is that we might i18n them as well.
     */

#   define COL(x)  "\033[" #x ";1m"
#   define RED     COL(31)
#   define GREEN   COL(32)
#   define YELLOW  COL(33)
#   define BLUE    COL(34)
#   define MAGENTA COL(35)
#   define CYAN    COL(36)
#   define WHITE   COL(0)
#   define GRAY    "\033[0m"
#define COLOR_FORMAT_STRING (WHITE"  %s --%s"YELLOW"%s%s%s%s%s%s "GRAY)
#define COLOR_FORMAT_STRING_BOOL (WHITE"  %s --%s%s%s%s%s%s%s "GRAY)

#define LINE_START 8
#define PADDING_SPACES 25
#ifdef WIN32
#   define OPTION_VALUE_SEP "="
#else
#   define OPTION_VALUE_SEP " "
#endif
    vlc_list_t *p_list = NULL;
    char psz_spaces_text[PADDING_SPACES+LINE_START+1];
    char psz_spaces_longtext[LINE_START+3];
    char psz_format[sizeof(COLOR_FORMAT_STRING)];
    char psz_format_bool[sizeof(COLOR_FORMAT_STRING_BOOL)];
    char psz_buffer[10000];
    char psz_short[4];
    int i_index;
    int i_width = ConsoleWidth() - (PADDING_SPACES+LINE_START+1);
    int i_width_description = i_width + PADDING_SPACES - 1;
    vlc_bool_t b_advanced    = config_GetInt( p_this, "advanced" ) > 0;
    vlc_bool_t b_description = config_GetInt( p_this, "help-verbose" ) > 0;
    vlc_bool_t b_description_hack;
    vlc_bool_t b_color       = config_GetInt( p_this, "color" ) > 0;
    vlc_bool_t b_has_advanced = VLC_FALSE;

    memset( psz_spaces_text, ' ', PADDING_SPACES+LINE_START );
    psz_spaces_text[PADDING_SPACES+LINE_START] = '\0';
    memset( psz_spaces_longtext, ' ', LINE_START+2 );
    psz_spaces_longtext[LINE_START+2] = '\0';

    if( b_color )
    {
        strcpy( psz_format, COLOR_FORMAT_STRING );
        strcpy( psz_format_bool, COLOR_FORMAT_STRING_BOOL );
    }
    else
    {
        strcpy( psz_format, FORMAT_STRING );
        strcpy( psz_format_bool, FORMAT_STRING );
    }

    /* List all modules */
    p_list = vlc_list_find( p_this, VLC_OBJECT_MODULE, FIND_ANYWHERE );

    /* Ugly hack to make sure that the help options always come first
     * (part 1) */
    if( !psz_module_name )
        Usage( p_this, "help" );

    /* Enumerate the config for each module */
    for( i_index = 0; i_index < p_list->i_count; i_index++ )
    {
        vlc_bool_t b_help_module;
        module_t *p_parser = (module_t *)p_list->p_values[i_index].p_object;
        module_config_t *p_item = NULL;
        module_config_t *p_end = p_parser->p_config + p_parser->confsize;

        if( psz_module_name && strcmp( psz_module_name,
                                       p_parser->psz_object_name ) )
        {
            const char *const *pp_shortcut = p_parser->pp_shortcuts;
            while( *pp_shortcut )
            {
                if( !strcmp( psz_module_name, *pp_shortcut ) )
                    break;
                pp_shortcut ++;
            }
            if( !*pp_shortcut )
                continue;
        }

        /* Ignore modules without config options */
        if( !p_parser->i_config_items )
        {
            continue;
        }

        b_help_module = !strcmp( "help", p_parser->psz_object_name );
        /* Ugly hack to make sure that the help options always come first
         * (part 2) */
        if( !psz_module_name && b_help_module )
            continue;

        /* Ignore modules with only advanced config options if requested */
        if( !b_advanced )
        {
            for( p_item = p_parser->p_config;
                 p_item < p_end;
                 p_item++ )
            {
                if( (p_item->i_type & CONFIG_ITEM) &&
                    !p_item->b_advanced ) break;
            }
        }

        /* Print name of module */
        if( strcmp( "main", p_parser->psz_object_name ) )
        {
            if( b_color )
                utf8_fprintf( stdout, "\n " GREEN "%s" GRAY "\n",
                              p_parser->psz_longname );
            else
                utf8_fprintf( stdout, "\n %s\n", p_parser->psz_longname );
        }
        if( p_parser->psz_help )
        {
            if( b_color )
                utf8_fprintf( stdout, CYAN" %s\n"GRAY, p_parser->psz_help );
            else
                utf8_fprintf( stdout, " %s\n", p_parser->psz_help );
        }

        /* Print module options */
        for( p_item = p_parser->p_config;
             p_item < p_end;
             p_item++ )
        {
            char *psz_text, *psz_spaces = psz_spaces_text;
            const char *psz_bra = NULL, *psz_type = NULL, *psz_ket = NULL;
            const char *psz_suf = "", *psz_prefix = NULL;
            signed int i;
            size_t i_cur_width;

            /* Skip removed options */
            if( p_item->b_removed )
            {
                continue;
            }
            /* Skip advanced options if requested */
            if( p_item->b_advanced && !b_advanced )
            {
                b_has_advanced = VLC_TRUE;
                continue;
            }

            switch( p_item->i_type )
            {
            case CONFIG_HINT_CATEGORY:
            case CONFIG_HINT_USAGE:
                if( !strcmp( "main", p_parser->psz_object_name ) )
                {
                    if( b_color )
                        utf8_fprintf( stdout, GREEN "\n %s\n" GRAY,
                                      p_item->psz_text );
                    else
                        utf8_fprintf( stdout, "\n %s\n", p_item->psz_text );
                }
                if( b_description && p_item->psz_longtext )
                {
                    if( b_color )
                        utf8_fprintf( stdout, CYAN " %s\n" GRAY,
                                      p_item->psz_longtext );
                    else
                        utf8_fprintf( stdout, " %s\n", p_item->psz_longtext );
                }
                break;

            case CONFIG_HINT_SUBCATEGORY:
                if( strcmp( "main", p_parser->psz_object_name ) )
                break;
            case CONFIG_SECTION:
                if( b_color )
                {
                    utf8_fprintf( stdout, RED"   %s:\n"GRAY,
                                  p_item->psz_text );
                    if( b_description && p_item->psz_longtext )
                        utf8_fprintf( stdout, MAGENTA"   %s\n"GRAY,
                                      p_item->psz_longtext );
                }
                else
                {
                    utf8_fprintf( stdout, "   %s:\n", p_item->psz_text );
                    if( b_description && p_item->psz_longtext )
                        utf8_fprintf( stdout, "   %s\n", p_item->psz_longtext );
                }
                break;

            case CONFIG_ITEM_STRING:
            case CONFIG_ITEM_FILE:
            case CONFIG_ITEM_DIRECTORY:
            case CONFIG_ITEM_MODULE: /* We could also have "=<" here */
            case CONFIG_ITEM_MODULE_CAT:
            case CONFIG_ITEM_MODULE_LIST:
            case CONFIG_ITEM_MODULE_LIST_CAT:
            case CONFIG_ITEM_FONT:
            case CONFIG_ITEM_PASSWORD:
                psz_bra = OPTION_VALUE_SEP "<";
                psz_type = _("string");
                psz_ket = ">";

                if( p_item->ppsz_list )
                {
                    psz_bra = OPTION_VALUE_SEP "{";
                    psz_type = psz_buffer;
                    psz_buffer[0] = '\0';
                    for( i = 0; p_item->ppsz_list[i]; i++ )
                    {
                        if( i ) strcat( psz_buffer, "," );
                        strcat( psz_buffer, p_item->ppsz_list[i] );
                    }
                    psz_ket = "}";
                }
                break;
            case CONFIG_ITEM_INTEGER:
            case CONFIG_ITEM_KEY: /* FIXME: do something a bit more clever */
                psz_bra = OPTION_VALUE_SEP "<";
                psz_type = _("integer");
                psz_ket = ">";

                if( p_item->min.i || p_item->max.i )
                {
                    sprintf( psz_buffer, "%s [%i .. %i]", psz_type,
                             p_item->min.i, p_item->max.i );
                    psz_type = psz_buffer;
                }

                if( p_item->i_list )
                {
                    psz_bra = OPTION_VALUE_SEP "{";
                    psz_type = psz_buffer;
                    psz_buffer[0] = '\0';
                    for( i = 0; p_item->ppsz_list_text[i]; i++ )
                    {
                        if( i ) strcat( psz_buffer, ", " );
                        sprintf( psz_buffer + strlen(psz_buffer), "%i (%s)",
                                 p_item->pi_list[i],
                                 p_item->ppsz_list_text[i] );
                    }
                    psz_ket = "}";
                }
                break;
            case CONFIG_ITEM_FLOAT:
                psz_bra = OPTION_VALUE_SEP "<";
                psz_type = _("float");
                psz_ket = ">";
                if( p_item->min.f || p_item->max.f )
                {
                    sprintf( psz_buffer, "%s [%f .. %f]", psz_type,
                             p_item->min.f, p_item->max.f );
                    psz_type = psz_buffer;
                }
                break;
            case CONFIG_ITEM_BOOL:
                psz_bra = ""; psz_type = ""; psz_ket = "";
                if( !b_help_module )
                {
                    psz_suf = p_item->value.i ? _(" (default enabled)") :
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
                utf8_fprintf( stdout, psz_format_bool, psz_short, 
                              p_item->psz_name, psz_prefix, p_item->psz_name,
                              psz_bra, psz_type, psz_ket, psz_spaces );
            }
            else
            {
                utf8_fprintf( stdout, psz_format, psz_short, p_item->psz_name,
                         "", "", psz_bra, psz_type, psz_ket, psz_spaces );
            }

            psz_spaces[i] = ' ';

            /* We wrap the rest of the output */
            sprintf( psz_buffer, "%s%s", p_item->psz_text, psz_suf );
            b_description_hack = b_description;

 description:
            psz_text = psz_buffer;
            i_cur_width = b_description && !b_description_hack
                          ? i_width_description
                          : i_width;
            while( *psz_text )
            {
                char *psz_parser, *psz_word;
                size_t i_end = strlen( psz_text );

                /* If the remaining text fits in a line, print it. */
                if( i_end <= i_cur_width )
                {
                    if( b_color )
                    {
                        if( !b_description || b_description_hack )
                            utf8_fprintf( stdout, BLUE"%s\n"GRAY, psz_text );
                        else
                            utf8_fprintf( stdout, "%s\n", psz_text );
                    }
                    else
                    {
                        utf8_fprintf( stdout, "%s\n", psz_text );
                    }
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

                } while( (size_t)(psz_parser - psz_text) <= i_cur_width );

                /* We cut a word in one of these cases:
                 *  - it's the only word in the line and it's too long.
                 *  - we used less than 80% of the width and the word we are
                 *    going to wrap is longer than 40% of the width, and even
                 *    if the word would have fit in the next line. */
                if( psz_word == psz_text
             || ( (size_t)(psz_word - psz_text) < 80 * i_cur_width / 100
             && (size_t)(psz_parser - psz_word) > 40 * i_cur_width / 100 ) )
                {
                    char c = psz_text[i_cur_width];
                    psz_text[i_cur_width] = '\0';
                    if( b_color )
                    {
                        if( !b_description || b_description_hack )
                            utf8_fprintf( stdout, BLUE"%s\n%s"GRAY,
                                          psz_text, psz_spaces );
                        else
                            utf8_fprintf( stdout, "%s\n%s",
                                          psz_text, psz_spaces );
                    }
                    else
                    {
                        utf8_fprintf( stdout, "%s\n%s", psz_text, psz_spaces );
                    }
                    psz_text += i_cur_width;
                    psz_text[0] = c;
                }
                else
                {
                    psz_word[-1] = '\0';
                    if( b_color )
                    {
                        if( !b_description || b_description_hack )
                            utf8_fprintf( stdout, BLUE"%s\n%s"GRAY,
                                          psz_text, psz_spaces );
                        else
                            utf8_fprintf( stdout, "%s\n%s",
                                          psz_text, psz_spaces );
                    }
                    else
                    {
                        utf8_fprintf( stdout, "%s\n%s", psz_text, psz_spaces );
                    }
                    psz_text = psz_word;
                }
            }

            if( b_description_hack && p_item->psz_longtext )
            {
                sprintf( psz_buffer, "%s%s", p_item->psz_longtext, psz_suf );
                b_description_hack = VLC_FALSE;
                psz_spaces = psz_spaces_longtext;
                utf8_fprintf( stdout, "%s", psz_spaces );
                goto description;
            }
        }
    }

    if( b_has_advanced )
        utf8_fprintf( stdout, "\n" WHITE "%s" GRAY " %s\n", _( "Note:" ),
        _( "add --advanced to your command line to see advanced options." ) );

    /* Release the module list */
    vlc_list_release( p_list );
}

/*****************************************************************************
 * ListModules: list the available modules with their description
 *****************************************************************************
 * Print a list of all available modules (builtins and plugins) and a short
 * description for each one.
 *****************************************************************************/
static void ListModules( libvlc_int_t *p_this, vlc_bool_t b_verbose )
{
    vlc_list_t *p_list = NULL;
    module_t *p_parser = NULL;
    char psz_spaces[22];
    int i_index;

    vlc_bool_t b_color = config_GetInt( p_this, "color" ) > 0;

    memset( psz_spaces, ' ', 22 );

#ifdef WIN32
    ShowConsole( VLC_TRUE );
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

        if( b_color )
            utf8_fprintf( stdout, GREEN"  %s%s "WHITE"%s\n"GRAY,
                          p_parser->psz_object_name,
                          psz_spaces,
                          p_parser->psz_longname );
        else
            utf8_fprintf( stdout, "  %s%s %s\n",
                          p_parser->psz_object_name,
                          psz_spaces, p_parser->psz_longname );

        if( b_verbose )
        {
            const char *const *pp_shortcut = p_parser->pp_shortcuts;
            while( *pp_shortcut )
            {
                if( strcmp( *pp_shortcut, p_parser->psz_object_name ) )
                {
                    if( b_color )
                        utf8_fprintf( stdout, CYAN"   s %s\n"GRAY,
                                      *pp_shortcut );
                    else
                        utf8_fprintf( stdout, "   s %s\n",
                                      *pp_shortcut );
                }
                pp_shortcut++;
            }
            if( p_parser->psz_capability )
            {
                if( b_color )
                    utf8_fprintf( stdout, MAGENTA"   c %s (%d)\n"GRAY,
                                  p_parser->psz_capability,
                                  p_parser->i_score );
                else
                    utf8_fprintf( stdout, "   c %s (%d)\n",
                                  p_parser->psz_capability,
                                  p_parser->i_score );
            }
        }

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
    ShowConsole( VLC_TRUE );
#endif

    utf8_fprintf( stdout, _("VLC version %s\n"), VLC_Version() );
    utf8_fprintf( stdout, _("Compiled by %s@%s.%s\n"),
             VLC_CompileBy(), VLC_CompileHost(), VLC_CompileDomain() );
    utf8_fprintf( stdout, _("Compiler: %s\n"), VLC_Compiler() );
    if( strcmp( VLC_Changeset(), "exported" ) )
        utf8_fprintf( stdout, _("Based upon svn changeset [%s]\n"),
                 VLC_Changeset() );
    utf8_fprintf( stdout, LICENSE_MSG );

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
static void ShowConsole( vlc_bool_t b_dofile )
{
#   ifndef UNDER_CE
    FILE *f_help = NULL;

    if( getenv( "PWD" ) && getenv( "PS1" ) ) return; /* cygwin shell */

    AllocConsole();
    /* Use the ANSI code page (e.g. Windows-1252) as expected by the LibVLC
     * Unicode/locale subsystem. By default, we have the obsolecent OEM code
     * page (e.g. CP437 or CP850). */
    SetConsoleOutputCP (GetACP ());

    freopen( "CONOUT$", "w", stderr );
    freopen( "CONIN$", "r", stdin );

    if( b_dofile && (f_help = fopen( "vlc-help.txt", "wt" )) )
    {
        fclose( f_help );
        freopen( "vlc-help.txt", "wt", stdout );
        utf8_fprintf( stderr, _("\nDumped content to vlc-help.txt file.\n") );
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

    utf8_fprintf( stderr, _("\nPress the RETURN key to continue...\n") );
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
    unsigned i_width = 80;

#ifndef WIN32
    FILE *file = popen( "stty size 2>/dev/null", "r" );
    if (file != NULL)
    {
        if (fscanf (file, "%*u %u", &i_width) <= 0)
            i_width = 80;
        pclose( file );
    }
#else
    CONSOLE_SCREEN_BUFFER_INFO buf;

    if (GetConsoleScreenBufferInfo (GetStdHandle (STD_OUTPUT_HANDLE), &buf))
        i_width = buf.dwSize.X;
#endif

    return i_width;
}

static int VerboseCallback( vlc_object_t *p_this, const char *psz_variable,
                     vlc_value_t old_val, vlc_value_t new_val, void *param)
{
    libvlc_int_t *p_libvlc = (libvlc_int_t *)p_this;
    (void)psz_variable;
    (void)old_val;
    (void)param;

    if( new_val.i_int >= -1 )
    {
        p_libvlc->i_verbose = __MIN( new_val.i_int, 2 );
    }
    return VLC_SUCCESS;
}

/*****************************************************************************
 * InitDeviceValues: initialize device values
 *****************************************************************************
 * This function inits the dvd, vcd and cd-audio values
 *****************************************************************************/
static void InitDeviceValues( libvlc_int_t *p_vlc )
{
#ifdef HAVE_HAL
    LibHalContext * ctx = NULL;
    int i, i_devices;
    char **devices = NULL;
    char *block_dev = NULL;
    dbus_bool_t b_dvd;

#ifdef HAVE_HAL_1
    DBusConnection *p_connection = NULL;
    DBusError       error;

    ctx = libhal_ctx_new();
    if( !ctx ) return;
    dbus_error_init( &error );
    p_connection = dbus_bus_get ( DBUS_BUS_SYSTEM, &error );
    if( dbus_error_is_set( &error ) || !p_connection )
    {
        dbus_error_free( &error );
        return;
    }
    libhal_ctx_set_dbus_connection( ctx, p_connection );
    if( libhal_ctx_init( ctx, &error ) )
#else
    ctx = hal_initialize( NULL, FALSE );
    if( ctx )
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
        dbus_connection_unref( p_connection );
#else
        hal_shutdown( ctx );
#endif
    }
    else
    {
        msg_Warn( p_vlc, "Unable to get HAL device properties" );
    }
#else
    (void)p_vlc;
#endif /* HAVE_HAL */
}
