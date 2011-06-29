/*****************************************************************************
 * libvlc.c: libvlc instances creation and deletion, interfaces handling
 *****************************************************************************
 * Copyright (C) 1998-2008 the VideoLAN team
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

#include <vlc_common.h>
#include "control/libvlc_internal.h"
#include <vlc_input.h>

#include "modules/modules.h"
#include "config/configuration.h"

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

#include "config/vlc_getopt.h"

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif

#ifdef HAVE_DBUS
/* used for one-instance mode */
#   include <dbus/dbus.h>
#endif


#include <vlc_media_library.h>
#include <vlc_playlist.h>
#include <vlc_interface.h>

#include <vlc_aout.h>
#include "audio_output/aout_internal.h"

#include <vlc_charset.h>
#include <vlc_fs.h>
#include <vlc_cpu.h>
#include <vlc_url.h>
#include <vlc_atomic.h>
#include <vlc_modules.h>

#include "libvlc.h"

#include "playlist/playlist_internal.h"

#include <vlc_vlm.h>

#ifdef __APPLE__
# include <libkern/OSAtomic.h>
#endif

#include <assert.h>

/*****************************************************************************
 * The evil global variables. We handle them with care, don't worry.
 *****************************************************************************/
static unsigned          i_instances = 0;

#ifndef WIN32
static bool b_daemon = false;
#endif

#undef vlc_gc_init
#undef vlc_hold
#undef vlc_release

/**
 * Atomically set the reference count to 1.
 * @param p_gc reference counted object
 * @param pf_destruct destruction calback
 * @return p_gc.
 */
void *vlc_gc_init (gc_object_t *p_gc, void (*pf_destruct) (gc_object_t *))
{
    /* There is no point in using the GC if there is no destructor... */
    assert (pf_destruct);
    p_gc->pf_destructor = pf_destruct;

    vlc_atomic_set (&p_gc->refs, 1);
    return p_gc;
}

/**
 * Atomically increment the reference count.
 * @param p_gc reference counted object
 * @return p_gc.
 */
void *vlc_hold (gc_object_t * p_gc)
{
    uintptr_t refs;

    assert( p_gc );
    refs = vlc_atomic_inc (&p_gc->refs);
    assert (refs != 1); /* there had to be a reference already */
    return p_gc;
}

/**
 * Atomically decrement the reference count and, if it reaches zero, destroy.
 * @param p_gc reference counted object.
 */
void vlc_release (gc_object_t *p_gc)
{
    uintptr_t refs;

    assert( p_gc );
    refs = vlc_atomic_dec (&p_gc->refs);
    assert (refs != (uintptr_t)(-1)); /* reference underflow?! */
    if (refs == 0)
        p_gc->pf_destructor (p_gc);
}

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
#if defined( ENABLE_NLS ) && (defined (__APPLE__) || defined (WIN32)) && \
    ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )
static void SetLanguage   ( char const * );
#endif
static void GetFilenames  ( libvlc_int_t *, unsigned, const char *const [] );
static void Help          ( libvlc_int_t *, char const *psz_help_name );
static void Usage         ( libvlc_int_t *, char const *psz_search );
static void ListModules   ( libvlc_int_t *, bool );
static void Version       ( void );

#ifdef WIN32
static void ShowConsole   ( bool );
static void PauseConsole  ( void );
#endif
static int  ConsoleWidth  ( void );

static vlc_mutex_t global_lock = VLC_STATIC_MUTEX;
extern const char psz_vlc_changeset[];

/**
 * Allocate a libvlc instance, initialize global data if needed
 * It also initializes the threading system
 */
libvlc_int_t * libvlc_InternalCreate( void )
{
    libvlc_int_t *p_libvlc;
    libvlc_priv_t *priv;
    char *psz_env = NULL;

    /* Now that the thread system is initialized, we don't have much, but
     * at least we have variables */
    vlc_mutex_lock( &global_lock );
    if( i_instances == 0 )
    {
        /* Guess what CPU we have */
        cpu_flags = CPUCapabilities();
        /* The module bank will be initialized later */
    }

    /* Allocate a libvlc instance object */
    p_libvlc = vlc_custom_create( (vlc_object_t *)NULL, sizeof (*priv),
                                  VLC_OBJECT_GENERIC, "libvlc" );
    if( p_libvlc != NULL )
        i_instances++;
    vlc_mutex_unlock( &global_lock );

    if( p_libvlc == NULL )
        return NULL;

    priv = libvlc_priv (p_libvlc);
    priv->p_playlist = NULL;
    priv->p_ml = NULL;
    priv->p_dialog_provider = NULL;
    priv->p_vlm = NULL;

    /* Initialize message queue */
    priv->msg_bank = msg_Create ();
    if (unlikely(priv->msg_bank == NULL))
        goto error;

    /* Find verbosity from VLC_VERBOSE environment variable */
    psz_env = getenv( "VLC_VERBOSE" );
    if( psz_env != NULL )
        priv->i_verbose = atoi( psz_env );
    else
        priv->i_verbose = 3;
#if defined( HAVE_ISATTY ) && !defined( WIN32 )
    priv->b_color = isatty( 2 ); /* 2 is for stderr */
#else
    priv->b_color = false;
#endif

    /* Initialize mutexes */
    vlc_mutex_init( &priv->ml_lock );
    vlc_mutex_init( &priv->timer_lock );
    vlc_ExitInit( &priv->exit );

    return p_libvlc;
error:
    vlc_object_release (p_libvlc);
    return NULL;
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
    libvlc_priv_t *priv = libvlc_priv (p_libvlc);
    char *       p_tmp = NULL;
    char *       psz_modules = NULL;
    char *       psz_parser = NULL;
    char *       psz_control = NULL;
    bool   b_exit = false;
    int          i_ret = VLC_EEXIT;
    playlist_t  *p_playlist = NULL;
    char        *psz_val;
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )
# if defined (WIN32) || defined (__APPLE__)
    char *       psz_language;
#endif
#endif

    /* System specific initialization code */
    system_Init();

    /*
     * Support for gettext
     */
    vlc_bindtextdomain (PACKAGE_NAME);

    /* Initialize the module bank and load the configuration of the
     * main module. We need to do this at this stage to be able to display
     * a short help if required by the user. (short help == main module
     * options) */
    module_InitBank( p_libvlc );

    if( config_LoadCmdLine( p_libvlc, i_argc, ppsz_argv, NULL ) )
    {
        module_EndBank( p_libvlc, false );
        return VLC_EGENERIC;
    }

    priv->i_verbose = var_InheritInteger( p_libvlc, "verbose" );
    /* Announce who we are - Do it only for first instance ? */
    msg_Dbg( p_libvlc, "VLC media player - %s", VERSION_MESSAGE );
    msg_Dbg( p_libvlc, "%s", COPYRIGHT_MESSAGE );
    msg_Dbg( p_libvlc, "revision %s", psz_vlc_changeset );
    msg_Dbg( p_libvlc, "configured with %s", CONFIGURE_LINE );
    /*xgettext: Translate "C" to the language code: "fr", "en_GB", "nl", "ru"... */
    msg_Dbg( p_libvlc, "translation test: code is \"%s\"", _("C") );

    /* Check for short help option */
    if( var_InheritBool( p_libvlc, "help" ) )
    {
        Help( p_libvlc, "help" );
        b_exit = true;
        i_ret = VLC_EEXITSUCCESS;
    }
    /* Check for version option */
    else if( var_InheritBool( p_libvlc, "version" ) )
    {
        Version();
        b_exit = true;
        i_ret = VLC_EEXITSUCCESS;
    }

    /* Check for daemon mode */
#if !defined( WIN32 ) && !defined( __SYMBIAN32__ )
    if( var_InheritBool( p_libvlc, "daemon" ) )
    {
#ifdef HAVE_DAEMON
        char *psz_pidfile = NULL;

        if( daemon( 1, 0) != 0 )
        {
            msg_Err( p_libvlc, "Unable to fork vlc to daemon mode" );
            b_exit = true;
        }
        b_daemon = true;

        /* lets check if we need to write the pidfile */
        psz_pidfile = var_CreateGetNonEmptyString( p_libvlc, "pidfile" );
        if( psz_pidfile != NULL )
        {
            FILE *pidfile;
            pid_t i_pid = getpid ();
            msg_Dbg( p_libvlc, "PID is %d, writing it to %s",
                               i_pid, psz_pidfile );
            pidfile = vlc_fopen( psz_pidfile,"w" );
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
            b_exit = true;
        }
        else if( i_pid )
        {
            /* This is the parent, exit right now */
            msg_Dbg( p_libvlc, "closing parent process" );
            b_exit = true;
            i_ret = VLC_EEXITSUCCESS;
        }
        else
        {
            /* We are the child */
            msg_Dbg( p_libvlc, "daemon spawned" );
            close( STDIN_FILENO );
            close( STDOUT_FILENO );
            close( STDERR_FILENO );

            b_daemon = true;
        }
#endif
    }
#endif

    if( b_exit )
    {
        module_EndBank( p_libvlc, false );
        return i_ret;
    }

    /* Check for translation config option */
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )
# if defined (WIN32) || defined (__APPLE__)
    if( !var_InheritBool( p_libvlc, "ignore-config" ) )
        config_LoadConfigFile( p_libvlc );
    priv->i_verbose = var_InheritInteger( p_libvlc, "verbose" );

    /* Check if the user specified a custom language */
    psz_language = var_CreateGetNonEmptyString( p_libvlc, "language" );
    if( psz_language && strcmp( psz_language, "auto" ) )
    {
        /* Reset the default domain */
        SetLanguage( psz_language );

        /* Translate "C" to the language code: "fr", "en_GB", "nl", "ru"... */
        msg_Dbg( p_libvlc, "translation test: code is \"%s\"", _("C") );
    }
    free( psz_language );
# endif
#endif

    /*
     * Load the builtins and plugins into the module_bank.
     * We have to do it before config_Load*() because this also gets the
     * list of configuration options exported by each module and loads their
     * default values.
     */
    module_LoadPlugins( p_libvlc );
    if( p_libvlc->b_die )
    {
        b_exit = true;
    }

    size_t module_count;
    module_t **list = module_list_get( &module_count );
    module_list_free( list );
    msg_Dbg( p_libvlc, "module bank initialized (%zu modules)", module_count );

    /* Check for help on modules */
    if( (p_tmp = var_InheritString( p_libvlc, "module" )) )
    {
        Help( p_libvlc, p_tmp );
        free( p_tmp );
        b_exit = true;
        i_ret = VLC_EEXITSUCCESS;
    }
    /* Check for full help option */
    else if( var_InheritBool( p_libvlc, "full-help" ) )
    {
        var_Create( p_libvlc, "advanced", VLC_VAR_BOOL );
        var_SetBool( p_libvlc, "advanced", true );
        var_Create( p_libvlc, "help-verbose", VLC_VAR_BOOL );
        var_SetBool( p_libvlc, "help-verbose", true );
        Help( p_libvlc, "full-help" );
        b_exit = true;
        i_ret = VLC_EEXITSUCCESS;
    }
    /* Check for long help option */
    else if( var_InheritBool( p_libvlc, "longhelp" ) )
    {
        Help( p_libvlc, "longhelp" );
        b_exit = true;
        i_ret = VLC_EEXITSUCCESS;
    }
    /* Check for module list option */
    else if( var_InheritBool( p_libvlc, "list" ) )
    {
        ListModules( p_libvlc, false );
        b_exit = true;
        i_ret = VLC_EEXITSUCCESS;
    }
    else if( var_InheritBool( p_libvlc, "list-verbose" ) )
    {
        ListModules( p_libvlc, true );
        b_exit = true;
        i_ret = VLC_EEXITSUCCESS;
    }

    if( module_count <= 1 )
    {
        msg_Err( p_libvlc, "No plugins found! Check your VLC installation.");
        b_exit = true;
        i_ret = VLC_ENOITEM;
    }

    if( b_exit )
    {
        module_EndBank( p_libvlc, true );
        return i_ret;
    }

    /*
     * Override default configuration with config file settings
     */
    if( !var_InheritBool( p_libvlc, "ignore-config" ) )
    {
        if( var_InheritBool( p_libvlc, "reset-config" ) )
        {
            config_ResetAll( p_libvlc );
            config_SaveConfigFile( p_libvlc );
        }
        else
            config_LoadConfigFile( p_libvlc );
    }

    /*
     * Override configuration with command line settings
     */
    int vlc_optind;
    if( config_LoadCmdLine( p_libvlc, i_argc, ppsz_argv, &vlc_optind ) )
    {
#ifdef WIN32
        ShowConsole( false );
        /* Pause the console because it's destroyed when we exit */
        fprintf( stderr, "The command line options couldn't be loaded, check "
                 "that they are valid.\n" );
        PauseConsole();
#endif
        module_EndBank( p_libvlc, true );
        return VLC_EGENERIC;
    }
    priv->i_verbose = var_InheritInteger( p_libvlc, "verbose" );

/* FIXME: could be replaced by using Unix sockets */
#ifdef HAVE_DBUS
    dbus_threads_init_default();

    if( var_InheritBool( p_libvlc, "one-instance" )
    || ( var_InheritBool( p_libvlc, "one-instance-when-started-from-file" )
      && var_InheritBool( p_libvlc, "started-from-file" ) ) )
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
            DBusMessage *p_test_msg   = NULL;
            DBusMessage *p_test_reply = NULL;

            p_test_msg =  dbus_message_new_method_call(
                    "org.mpris.MediaPlayer2.vlc", "/org/mpris/MediaPlayer2",
                    "org.freedesktop.DBus.Introspectable", "Introspect" );

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

                for( i_input = vlc_optind; i_input < i_argc;i_input++ )
                {
                    /* Skip input options, we can't pass them through D-Bus */
                    if( ppsz_argv[i_input][0] == ':' )
                    {
                        msg_Warn( p_libvlc, "Ignoring option %s",
                                  ppsz_argv[i_input] );
                        continue;
                    }

                    /* We need to resolve relative paths in this instance */
                    char *psz_mrl = make_URI( ppsz_argv[i_input], NULL );
                    const char *psz_after_track = "/";

                    if( psz_mrl == NULL )
                        continue;
                    msg_Dbg( p_libvlc, "Adds %s to the running Media Player",
                             psz_mrl );

                    p_dbus_msg = dbus_message_new_method_call(
                        "org.mpris.MediaPlayer2.vlc", "/org/mpris/MediaPlayer2",
                        "org.mpris.MediaPlayer2.TrackList", "AddTrack" );

                    if ( NULL == p_dbus_msg )
                    {
                        msg_Err( p_libvlc, "D-Bus problem" );
                        free( psz_mrl );
                        system_End( p_libvlc );
                        exit( 1 );
                    }

                    /* append MRLs */
                    dbus_message_iter_init_append( p_dbus_msg, &dbus_args );
                    if ( !dbus_message_iter_append_basic( &dbus_args,
                                DBUS_TYPE_STRING, &psz_mrl ) )
                    {
                        dbus_message_unref( p_dbus_msg );
                        free( psz_mrl );
                        system_End( p_libvlc );
                        exit( 1 );
                    }
                    free( psz_mrl );

                    if( !dbus_message_iter_append_basic( &dbus_args,
                                DBUS_TYPE_OBJECT_PATH, &psz_after_track ) )
                    {
                        dbus_message_unref( p_dbus_msg );
                        system_End( p_libvlc );
                        exit( 1 );
                    }

                    b_play = TRUE;
                    if( var_InheritBool( p_libvlc, "playlist-enqueue" ) )
                        b_play = FALSE;

                    if ( !dbus_message_iter_append_basic( &dbus_args,
                                DBUS_TYPE_BOOLEAN, &b_play ) )
                    {
                        dbus_message_unref( p_dbus_msg );
                        system_End( p_libvlc );
                        exit( 1 );
                    }

                    /* send message and get a handle for a reply */
                    if ( !dbus_connection_send_with_reply ( p_conn,
                                p_dbus_msg, &p_dbus_pending, -1 ) )
                    {
                        msg_Err( p_libvlc, "D-Bus problem" );
                        dbus_message_unref( p_dbus_msg );
                        system_End( p_libvlc );
                        exit( 1 );
                    }

                    if ( NULL == p_dbus_pending )
                    {
                        msg_Err( p_libvlc, "D-Bus problem" );
                        dbus_message_unref( p_dbus_msg );
                        system_End( p_libvlc );
                        exit( 1 );
                    }
                    dbus_connection_flush( p_conn );
                    dbus_message_unref( p_dbus_msg );
                    /* block until we receive a reply */
                    dbus_pending_call_block( p_dbus_pending );
                    dbus_pending_call_unref( p_dbus_pending );
                } /* processes all command line MRLs */

                /* bye bye */
                system_End( p_libvlc );
                exit( 0 );
            }
        }
        /* we unreference the connection when we've finished with it */
        if( p_conn ) dbus_connection_unref( p_conn );
    }
#endif

    /*
     * Message queue options
     */
    char * psz_verbose_objects = var_CreateGetNonEmptyString( p_libvlc, "verbose-objects" );
    if( psz_verbose_objects )
    {
        char * psz_object, * iter = psz_verbose_objects;
        while( (psz_object = strsep( &iter, "," )) )
        {
            switch( psz_object[0] )
            {
                printf("%s\n", psz_object+1);
                case '+': msg_EnableObjectPrinting(p_libvlc, psz_object+1); break;
                case '-': msg_DisableObjectPrinting(p_libvlc, psz_object+1); break;
                default:
                    msg_Err( p_libvlc, "verbose-objects usage: \n"
                            "--verbose-objects=+printthatobject,"
                            "-dontprintthatone\n"
                            "(keyword 'all' to applies to all objects)");
                    free( psz_verbose_objects );
                    /* FIXME: leaks!!!! */
                    return VLC_EGENERIC;
            }
        }
        free( psz_verbose_objects );
    }

    /* Last chance to set the verbosity. Once we start interfaces and other
     * threads, verbosity becomes read-only. */
    var_Create( p_libvlc, "verbose", VLC_VAR_INTEGER | VLC_VAR_DOINHERIT );
    if( var_InheritBool( p_libvlc, "quiet" ) )
    {
        var_SetInteger( p_libvlc, "verbose", -1 );
        priv->i_verbose = -1;
    }
    vlc_threads_setup( p_libvlc );

    if( priv->b_color )
        priv->b_color = var_InheritBool( p_libvlc, "color" );

    char p_capabilities[200];
#define PRINT_CAPABILITY( capability, string )                              \
    if( vlc_CPU() & capability )                                            \
    {                                                                       \
        strncat( p_capabilities, string " ",                                \
                 sizeof(p_capabilities) - strlen(p_capabilities) );         \
        p_capabilities[sizeof(p_capabilities) - 1] = '\0';                  \
    }
    p_capabilities[0] = '\0';

#if defined( __i386__ ) || defined( __x86_64__ )
    if( !var_InheritBool( p_libvlc, "mmx" ) )
        cpu_flags &= ~CPU_CAPABILITY_MMX;
    if( !var_InheritBool( p_libvlc, "3dn" ) )
        cpu_flags &= ~CPU_CAPABILITY_3DNOW;
    if( !var_InheritBool( p_libvlc, "mmxext" ) )
        cpu_flags &= ~CPU_CAPABILITY_MMXEXT;
    if( !var_InheritBool( p_libvlc, "sse" ) )
        cpu_flags &= ~CPU_CAPABILITY_SSE;
    if( !var_InheritBool( p_libvlc, "sse2" ) )
        cpu_flags &= ~CPU_CAPABILITY_SSE2;
    if( !var_InheritBool( p_libvlc, "sse3" ) )
        cpu_flags &= ~CPU_CAPABILITY_SSE3;
    if( !var_InheritBool( p_libvlc, "ssse3" ) )
        cpu_flags &= ~CPU_CAPABILITY_SSSE3;
    if( !var_InheritBool( p_libvlc, "sse41" ) )
        cpu_flags &= ~CPU_CAPABILITY_SSE4_1;
    if( !var_InheritBool( p_libvlc, "sse42" ) )
        cpu_flags &= ~CPU_CAPABILITY_SSE4_2;

    PRINT_CAPABILITY( CPU_CAPABILITY_MMX, "MMX" );
    PRINT_CAPABILITY( CPU_CAPABILITY_3DNOW, "3DNow!" );
    PRINT_CAPABILITY( CPU_CAPABILITY_MMXEXT, "MMXEXT" );
    PRINT_CAPABILITY( CPU_CAPABILITY_SSE, "SSE" );
    PRINT_CAPABILITY( CPU_CAPABILITY_SSE2, "SSE2" );
    PRINT_CAPABILITY( CPU_CAPABILITY_SSE3, "SSE3" );
    PRINT_CAPABILITY( CPU_CAPABILITY_SSSE3, "SSSE3" );
    PRINT_CAPABILITY( CPU_CAPABILITY_SSE4_1, "SSE4.1" );
    PRINT_CAPABILITY( CPU_CAPABILITY_SSE4_2, "SSE4.2" );
    PRINT_CAPABILITY( CPU_CAPABILITY_SSE4A,  "SSE4A" );

#elif defined( __powerpc__ ) || defined( __ppc__ ) || defined( __ppc64__ )
    if( !var_InheritBool( p_libvlc, "altivec" ) )
        cpu_flags &= ~CPU_CAPABILITY_ALTIVEC;

    PRINT_CAPABILITY( CPU_CAPABILITY_ALTIVEC, "AltiVec" );

#elif defined( __arm__ )
    PRINT_CAPABILITY( CPU_CAPABILITY_NEON, "NEONv1" );

#endif

#if HAVE_FPU
    strncat( p_capabilities, "FPU ",
             sizeof(p_capabilities) - strlen( p_capabilities) );
    p_capabilities[sizeof(p_capabilities) - 1] = '\0';
#endif

    if (p_capabilities[0])
        msg_Dbg( p_libvlc, "CPU has capabilities %s", p_capabilities );

    /*
     * Choose the best memcpy module
     */
    priv->p_memcpy_module = module_need( p_libvlc, "memcpy", "$memcpy", false );
    /* Avoid being called "memcpy":*/
    vlc_object_set_name( p_libvlc, "main" );

    priv->b_stats = var_InheritBool( p_libvlc, "stats" );
    priv->i_timers = 0;
    priv->pp_timers = NULL;

    priv->i_last_input_id = 0; /* Not very safe, should be removed */

    /*
     * Initialize hotkey handling
     */
    priv->actions = vlc_InitActions( p_libvlc );

    /* Create a variable for showing the fullscreen interface */
    var_Create( p_libvlc, "intf-show", VLC_VAR_BOOL );
    var_SetBool( p_libvlc, "intf-show", true );

    /* Create a variable for showing the right click menu */
    var_Create( p_libvlc, "intf-popupmenu", VLC_VAR_BOOL );

    /* variables for signalling creation of new files */
    var_Create( p_libvlc, "snapshot-file", VLC_VAR_STRING );
    var_Create( p_libvlc, "record-file", VLC_VAR_STRING );

    /* some default internal settings */
    var_Create( p_libvlc, "window", VLC_VAR_STRING );
    var_Create( p_libvlc, "user-agent", VLC_VAR_STRING );
    var_SetString( p_libvlc, "user-agent", "(LibVLC "VERSION")" );

    /* Initialize playlist and get commandline files */
    p_playlist = playlist_Create( VLC_OBJECT(p_libvlc) );
    if( !p_playlist )
    {
        msg_Err( p_libvlc, "playlist initialization failed" );
        if( priv->p_memcpy_module != NULL )
        {
            module_unneed( p_libvlc, priv->p_memcpy_module );
        }
        module_EndBank( p_libvlc, true );
        return VLC_EGENERIC;
    }

    /* System specific configuration */
    system_Configure( p_libvlc, i_argc - vlc_optind, ppsz_argv + vlc_optind );

#if defined(MEDIA_LIBRARY)
    /* Get the ML */
    if( var_GetBool( p_libvlc, "load-media-library-on-startup" ) )
    {
        priv->p_ml = ml_Create( VLC_OBJECT( p_libvlc ), NULL );
        if( !priv->p_ml )
        {
            msg_Err( p_libvlc, "ML initialization failed" );
            return VLC_EGENERIC;
        }
    }
    else
    {
        priv->p_ml = NULL;
    }
#endif

    /* Add service discovery modules */
    psz_modules = var_InheritString( p_libvlc, "services-discovery" );
    if( psz_modules )
    {
        char *p = psz_modules, *m;
        while( ( m = strsep( &p, " :," ) ) != NULL )
            playlist_ServicesDiscoveryAdd( p_playlist, m );
        free( psz_modules );
    }

#ifdef ENABLE_VLM
    /* Initialize VLM if vlm-conf is specified */
    psz_parser = var_CreateGetNonEmptyString( p_libvlc, "vlm-conf" );
    if( psz_parser )
    {
        priv->p_vlm = vlm_New( p_libvlc );
        if( !priv->p_vlm )
            msg_Err( p_libvlc, "VLM initialization failed" );
    }
    free( psz_parser );
#endif

    /*
     * Load background interfaces
     */
    psz_modules = var_CreateGetNonEmptyString( p_libvlc, "extraintf" );
    psz_control = var_CreateGetNonEmptyString( p_libvlc, "control" );

    if( psz_modules && psz_control )
    {
        char* psz_tmp;
        if( asprintf( &psz_tmp, "%s:%s", psz_modules, psz_control ) != -1 )
        {
            free( psz_modules );
            psz_modules = psz_tmp;
        }
    }
    else if( psz_control )
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
        if( asprintf( &psz_temp, "%s,none", psz_module ) != -1)
        {
            intf_Create( p_libvlc, psz_temp );
            free( psz_temp );
        }
    }
    free( psz_modules );
    free( psz_control );

    /*
     * Always load the hotkeys interface if it exists
     */
    intf_Create( p_libvlc, "hotkeys,none" );

#ifdef HAVE_DBUS
    /* loads dbus control interface if in one-instance mode
     * we do it only when playlist exists, because dbus module needs it */
    if( var_InheritBool( p_libvlc, "one-instance" )
     || ( var_InheritBool( p_libvlc, "one-instance-when-started-from-file" )
       && var_InheritBool( p_libvlc, "started-from-file" ) ) )
        intf_Create( p_libvlc, "dbus,none" );

# if !defined (HAVE_MAEMO)
    /* Prevents the power management daemon from suspending the system
     * when VLC is active */
    if( var_InheritBool( p_libvlc, "inhibit" ) > 0 )
        intf_Create( p_libvlc, "inhibit,none" );
# endif
#endif

    if( var_InheritBool( p_libvlc, "file-logging" ) &&
        !var_InheritBool( p_libvlc, "syslog" ) )
    {
        intf_Create( p_libvlc, "logger,none" );
    }
#ifdef HAVE_SYSLOG_H
    if( var_InheritBool( p_libvlc, "syslog" ) )
    {
        char *logmode = var_CreateGetNonEmptyString( p_libvlc, "logmode" );
        var_SetString( p_libvlc, "logmode", "syslog" );
        intf_Create( p_libvlc, "logger,none" );

        if( logmode )
        {
            var_SetString( p_libvlc, "logmode", logmode );
            free( logmode );
        }
        var_Destroy( p_libvlc, "logmode" );
    }
#endif

    if( var_InheritBool( p_libvlc, "network-synchronisation") )
    {
        intf_Create( p_libvlc, "netsync,none" );
    }

#ifdef __APPLE__
    var_Create( p_libvlc, "drawable-view-top", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-view-left", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-view-bottom", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-view-right", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-clip-top", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-clip-left", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-clip-bottom", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-clip-right", VLC_VAR_INTEGER );
    var_Create( p_libvlc, "drawable-nsobject", VLC_VAR_ADDRESS );
#endif
#ifdef WIN32
    var_Create( p_libvlc, "drawable-hwnd", VLC_VAR_INTEGER );
#endif

    /*
     * Get input filenames given as commandline arguments.
     * We assume that the remaining parameters are filenames
     * and their input options.
     */
    GetFilenames( p_libvlc, i_argc - vlc_optind, ppsz_argv + vlc_optind );

    /*
     * Get --open argument
     */
    psz_val = var_InheritString( p_libvlc, "open" );
    if ( psz_val != NULL )
    {
        playlist_AddExt( p_playlist, psz_val, NULL, PLAYLIST_INSERT, 0,
                         -1, 0, NULL, 0, true, pl_Unlocked );
        free( psz_val );
    }

    return VLC_SUCCESS;
}

/**
 * Cleanup a libvlc instance. The instance is not completely deallocated
 * \param p_libvlc the instance to clean
 */
void libvlc_InternalCleanup( libvlc_int_t *p_libvlc )
{
    libvlc_priv_t *priv = libvlc_priv (p_libvlc);
    playlist_t    *p_playlist = libvlc_priv (p_libvlc)->p_playlist;

    /* Deactivate the playlist */
    msg_Dbg( p_libvlc, "deactivating the playlist" );
    pl_Deactivate( p_libvlc );

    /* Remove all services discovery */
    msg_Dbg( p_libvlc, "removing all services discovery tasks" );
    playlist_ServicesDiscoveryKillAll( p_playlist );

    /* Ask the interfaces to stop and destroy them */
    msg_Dbg( p_libvlc, "removing all interfaces" );
    libvlc_Quit( p_libvlc );
    intf_DestroyAll( p_libvlc );

#ifdef ENABLE_VLM
    /* Destroy VLM if created in libvlc_InternalInit */
    if( priv->p_vlm )
    {
        vlm_Delete( priv->p_vlm );
    }
#endif

#if defined(MEDIA_LIBRARY)
    media_library_t* p_ml = priv->p_ml;
    if( p_ml )
    {
        ml_Destroy( VLC_OBJECT( p_ml ) );
        vlc_object_release( p_ml );
        libvlc_priv(p_playlist->p_libvlc)->p_ml = NULL;
    }
#endif

    /* Free playlist now, all threads are gone */
    playlist_Destroy( p_playlist );
    stats_TimersDumpAll( p_libvlc );
    stats_TimersCleanAll( p_libvlc );

    msg_Dbg( p_libvlc, "removing stats" );

#ifndef WIN32
    char* psz_pidfile = NULL;

    if( b_daemon )
    {
        psz_pidfile = var_CreateGetNonEmptyString( p_libvlc, "pidfile" );
        if( psz_pidfile != NULL )
        {
            msg_Dbg( p_libvlc, "removing pid file %s", psz_pidfile );
            if( unlink( psz_pidfile ) == -1 )
            {
                msg_Dbg( p_libvlc, "removing pid file %s: %m",
                        psz_pidfile );
            }
        }
        free( psz_pidfile );
    }
#endif

    if( priv->p_memcpy_module )
    {
        module_unneed( p_libvlc, priv->p_memcpy_module );
        priv->p_memcpy_module = NULL;
    }

    /* Save the configuration */
    if( !var_InheritBool( p_libvlc, "ignore-config" ) )
        config_AutoSaveConfigFile( VLC_OBJECT(p_libvlc) );

    /* Free module bank. It is refcounted, so we call this each time  */
    module_EndBank( p_libvlc, true );

    vlc_DeinitActions( p_libvlc, priv->actions );
}

/**
 * Destroy everything.
 * This function requests the running threads to finish, waits for their
 * termination, and destroys their structure.
 * It stops the thread systems: no instance can run after this has run
 * \param p_libvlc the instance to destroy
 */
void libvlc_InternalDestroy( libvlc_int_t *p_libvlc )
{
    libvlc_priv_t *priv = libvlc_priv( p_libvlc );

    vlc_mutex_lock( &global_lock );
    i_instances--;

    if( i_instances == 0 )
    {
        /* System specific cleaning code */
        system_End( p_libvlc );
    }
    vlc_mutex_unlock( &global_lock );

    msg_Destroy (priv->msg_bank);

    /* Destroy mutexes */
    vlc_ExitDestroy( &priv->exit );
    vlc_mutex_destroy( &priv->timer_lock );
    vlc_mutex_destroy( &priv->ml_lock );

#ifndef NDEBUG /* Hack to dump leaked objects tree */
    if( vlc_internals( p_libvlc )->i_refcount > 1 )
        while( vlc_internals( p_libvlc )->i_refcount > 0 )
            vlc_object_release( p_libvlc );
#endif

    assert( vlc_internals( p_libvlc )->i_refcount == 1 );
    vlc_object_release( p_libvlc );
}

/**
 * Add an interface plugin and run it
 */
int libvlc_InternalAddIntf( libvlc_int_t *p_libvlc, char const *psz_module )
{
    if( !p_libvlc )
        return VLC_EGENERIC;

    if( !psz_module ) /* requesting the default interface */
    {
        char *psz_interface = var_CreateGetNonEmptyString( p_libvlc, "intf" );
        if( !psz_interface ) /* "intf" has not been set */
        {
#ifndef WIN32
            if( b_daemon )
                 /* Daemon mode hack.
                  * We prefer the dummy interface if none is specified. */
                psz_module = "dummy";
            else
#endif
                msg_Info( p_libvlc, "%s",
                          _("Running vlc with the default interface. "
                            "Use 'cvlc' to use vlc without interface.") );
        }
        free( psz_interface );
        var_Destroy( p_libvlc, "intf" );
    }

    /* Try to create the interface */
    int ret = intf_Create( p_libvlc, psz_module ? psz_module : "$intf" );
    if( ret )
        msg_Err( p_libvlc, "interface \"%s\" initialization failed",
                 psz_module ? psz_module : "default" );
    return ret;
}

#if defined( ENABLE_NLS ) && (defined (__APPLE__) || defined (WIN32)) && \
    ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )
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
    setenv( "LC_ALL", psz_lang, 1 );

#endif

    setlocale( LC_ALL, psz_lang );
}
#endif

/*****************************************************************************
 * GetFilenames: parse command line options which are not flags
 *****************************************************************************
 * Parse command line for input files as well as their associated options.
 * An option always follows its associated input and begins with a ":".
 *****************************************************************************/
static void GetFilenames( libvlc_int_t *p_vlc, unsigned n,
                          const char *const args[] )
{
    while( n > 0 )
    {
        /* Count the input options */
        unsigned i_options = 0;

        while( args[--n][0] == ':' )
        {
            i_options++;
            if( n == 0 )
            {
                msg_Warn( p_vlc, "options %s without item", args[n] );
                return; /* syntax!? */
            }
        }

        char *mrl = make_URI( args[n], NULL );
        if( !mrl )
            continue;

        playlist_AddExt( pl_Get( p_vlc ), mrl, NULL, PLAYLIST_INSERT,
                0, -1, i_options, ( i_options ? &args[n + 1] : NULL ),
                VLC_INPUT_OPTION_TRUSTED, true, pl_Unlocked );
        free( mrl );
    }
}

/*****************************************************************************
 * Help: print program help
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
static inline void print_help_on_full_help( void )
{
    utf8_fprintf( stdout, "\n" );
    utf8_fprintf( stdout, "%s\n", _("To get exhaustive help, use '-H'.") );
}

static const char vlc_usage[] = N_(
                            "Usage: %s [options] [stream] ..."
                            "\nYou can specify multiple streams on the commandline. They will be enqueued in the playlist."
                            "\nThe first item specified will be played first."
                            "\n"
                            "\nOptions-styles:"
                            "\n  --option  A global option that is set for the duration of the program."
                            "\n   -option  A single letter version of a global --option."
                            "\n   :option  An option that only applies to the stream directly before it"
                            "\n            and that overrides previous settings."
                            "\n"
                            "\nStream MRL syntax:"
                            "\n  [[access][/demux]://]URL[@[title][:chapter][-[title][:chapter]]] [:option=value ...]"
                            "\n"
                            "\n  Many of the global --options can also be used as MRL specific :options."
                            "\n  Multiple :option=value pairs can be specified."
                            "\n"
                            "\nURL syntax:"
                            "\n  [file://]filename              Plain media file"
                            "\n  http://ip:port/file            HTTP URL"
                            "\n  ftp://ip:port/file             FTP URL"
                            "\n  mms://ip:port/file             MMS URL"
                            "\n  screen://                      Screen capture"
                            "\n  [dvd://][device][@raw_device]  DVD device"
                            "\n  [vcd://][device]               VCD device"
                            "\n  [cdda://][device]              Audio CD device"
                            "\n  udp://[[<source address>]@[<bind address>][:<bind port>]]"
                            "\n                                 UDP stream sent by a streaming server"
                            "\n  vlc://pause:<seconds>          Special item to pause the playlist for a certain time"
                            "\n  vlc://quit                     Special item to quit VLC"
                            "\n");

static void Help( libvlc_int_t *p_this, char const *psz_help_name )
{
#ifdef WIN32
    ShowConsole( true );
#endif

    if( psz_help_name && !strcmp( psz_help_name, "help" ) )
    {
        utf8_fprintf( stdout, vlc_usage, "vlc" );
        Usage( p_this, "=help" );
        Usage( p_this, "=main" );
        print_help_on_full_help();
    }
    else if( psz_help_name && !strcmp( psz_help_name, "longhelp" ) )
    {
        utf8_fprintf( stdout, vlc_usage, "vlc" );
        Usage( p_this, NULL );
        print_help_on_full_help();
    }
    else if( psz_help_name && !strcmp( psz_help_name, "full-help" ) )
    {
        utf8_fprintf( stdout, vlc_usage, "vlc" );
        Usage( p_this, NULL );
    }
    else if( psz_help_name )
    {
        Usage( p_this, psz_help_name );
    }

#ifdef WIN32        /* Pause the console because it's destroyed when we exit */
    PauseConsole();
#endif
    fflush( stdout );
}

/*****************************************************************************
 * Usage: print module usage
 *****************************************************************************
 * Print a short inline help. Message interface is initialized at this stage.
 *****************************************************************************/
#   define COL(x)  "\033[" #x ";1m"
#   define RED     COL(31)
#   define GREEN   COL(32)
#   define YELLOW  COL(33)
#   define BLUE    COL(34)
#   define MAGENTA COL(35)
#   define CYAN    COL(36)
#   define WHITE   COL(0)
#   define GRAY    "\033[0m"
static void
print_help_section( const module_t *m, const module_config_t *p_item,
                    bool b_color, bool b_description )
{
    if( !p_item ) return;
    if( b_color )
    {
        utf8_fprintf( stdout, RED"   %s:\n"GRAY,
                      module_gettext( m, p_item->psz_text ) );
        if( b_description && p_item->psz_longtext )
            utf8_fprintf( stdout, MAGENTA"   %s\n"GRAY,
                          module_gettext( m, p_item->psz_longtext ) );
    }
    else
    {
        utf8_fprintf( stdout, "   %s:\n",
                      module_gettext( m, p_item->psz_text ) );
        if( b_description && p_item->psz_longtext )
            utf8_fprintf( stdout, "   %s\n",
                          module_gettext(m, p_item->psz_longtext ) );
    }
}

static void Usage( libvlc_int_t *p_this, char const *psz_search )
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

#define COLOR_FORMAT_STRING (WHITE"  %s --%s"YELLOW"%s%s%s%s%s%s "GRAY)
#define COLOR_FORMAT_STRING_BOOL (WHITE"  %s --%s%s%s%s%s%s%s "GRAY)

#define LINE_START 8
#define PADDING_SPACES 25
#ifdef WIN32
#   define OPTION_VALUE_SEP "="
#else
#   define OPTION_VALUE_SEP " "
#endif
    char psz_spaces_text[PADDING_SPACES+LINE_START+1];
    char psz_spaces_longtext[LINE_START+3];
    char psz_format[sizeof(COLOR_FORMAT_STRING)];
    char psz_format_bool[sizeof(COLOR_FORMAT_STRING_BOOL)];
    char psz_buffer[10000];
    char psz_short[4];
    int i_width = ConsoleWidth() - (PADDING_SPACES+LINE_START+1);
    int i_width_description = i_width + PADDING_SPACES - 1;
    bool b_advanced    = var_InheritBool( p_this, "advanced" );
    bool b_description = var_InheritBool( p_this, "help-verbose" );
    bool b_description_hack;
    bool b_color       = var_InheritBool( p_this, "color" );
    bool b_has_advanced = false;
    bool b_found       = false;
    int  i_only_advanced = 0; /* Number of modules ignored because they
                               * only have advanced options */
    bool b_strict = psz_search && *psz_search == '=';
    if( b_strict ) psz_search++;

    memset( psz_spaces_text, ' ', PADDING_SPACES+LINE_START );
    psz_spaces_text[PADDING_SPACES+LINE_START] = '\0';
    memset( psz_spaces_longtext, ' ', LINE_START+2 );
    psz_spaces_longtext[LINE_START+2] = '\0';
#ifndef WIN32
    if( !isatty( 1 ) )
#endif
        b_color = false; // don't put color control codes in a .txt file

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
    module_t **list = module_list_get (NULL);
    if (!list)
        return;

    /* Ugly hack to make sure that the help options always come first
     * (part 1) */
    if( !psz_search )
        Usage( p_this, "help" );

    /* Enumerate the config for each module */
    for (size_t i = 0; list[i]; i++)
    {
        bool b_help_module;
        module_t *p_parser = list[i];
        module_config_t *p_item = NULL;
        module_config_t *p_section = NULL;
        module_config_t *p_end = p_parser->p_config + p_parser->confsize;

        if( psz_search &&
            ( b_strict ? strcmp( psz_search, p_parser->psz_object_name )
                       : !strstr( p_parser->psz_object_name, psz_search ) ) )
        {
            char *const *pp_shortcuts = p_parser->pp_shortcuts;
            unsigned i;
            for( i = 0; i < p_parser->i_shortcuts; i++ )
            {
                if( b_strict ? !strcmp( psz_search, pp_shortcuts[i] )
                             : !!strstr( pp_shortcuts[i], psz_search ) )
                    break;
            }
            if( i == p_parser->i_shortcuts )
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
        if( !psz_search && b_help_module )
            continue;

        /* Ignore modules with only advanced config options if requested */
        if( !b_advanced )
        {
            for( p_item = p_parser->p_config;
                 p_item < p_end;
                 p_item++ )
            {
                if( CONFIG_ITEM(p_item->i_type) &&
                    !p_item->b_advanced && !p_item->b_removed ) break;
            }

            if( p_item == p_end )
            {
                i_only_advanced++;
                continue;
            }
        }

        b_found = true;

        /* Print name of module */
        if( strcmp( "main", p_parser->psz_object_name ) )
        {
            if( b_color )
                utf8_fprintf( stdout, "\n " GREEN "%s" GRAY " (%s)\n",
                              module_gettext( p_parser, p_parser->psz_longname ),
                              p_parser->psz_object_name );
            else
                utf8_fprintf( stdout, "\n %s\n",
                              module_gettext(p_parser, p_parser->psz_longname ) );
        }
        if( p_parser->psz_help )
        {
            if( b_color )
                utf8_fprintf( stdout, CYAN" %s\n"GRAY,
                              module_gettext( p_parser, p_parser->psz_help ) );
            else
                utf8_fprintf( stdout, " %s\n",
                              module_gettext( p_parser, p_parser->psz_help ) );
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
                b_has_advanced = true;
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
                                      module_gettext( p_parser, p_item->psz_text ) );
                    else
                        utf8_fprintf( stdout, "\n %s\n",
                                      module_gettext( p_parser, p_item->psz_text ) );
                }
                if( b_description && p_item->psz_longtext )
                {
                    if( b_color )
                        utf8_fprintf( stdout, CYAN " %s\n" GRAY,
                                      module_gettext( p_parser, p_item->psz_longtext ) );
                    else
                        utf8_fprintf( stdout, " %s\n",
                                      module_gettext( p_parser, p_item->psz_longtext ) );
                }
                break;

            case CONFIG_HINT_SUBCATEGORY:
                if( strcmp( "main", p_parser->psz_object_name ) )
                    break;
            case CONFIG_SECTION:
                p_section = p_item;
                break;

            case CONFIG_ITEM_STRING:
            case CONFIG_ITEM_LOADFILE:
            case CONFIG_ITEM_SAVEFILE:
            case CONFIG_ITEM_DIRECTORY:
            case CONFIG_ITEM_KEY:
            case CONFIG_ITEM_MODULE: /* We could also have "=<" here */
            case CONFIG_ITEM_MODULE_CAT:
            case CONFIG_ITEM_MODULE_LIST:
            case CONFIG_ITEM_MODULE_LIST_CAT:
            case CONFIG_ITEM_FONT:
            case CONFIG_ITEM_PASSWORD:
                print_help_section( p_parser, p_section, b_color,
                                    b_description );
                p_section = NULL;
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
            case CONFIG_ITEM_RGB:
                print_help_section( p_parser, p_section, b_color,
                                    b_description );
                p_section = NULL;
                psz_bra = OPTION_VALUE_SEP "<";
                psz_type = _("integer");
                psz_ket = ">";

                if( p_item->min.i || p_item->max.i )
                {
                    sprintf( psz_buffer, "%s [%"PRId64" .. %"PRId64"]",
                             psz_type, p_item->min.i, p_item->max.i );
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
                                 module_gettext( p_parser, p_item->ppsz_list_text[i] ) );
                    }
                    psz_ket = "}";
                }
                break;
            case CONFIG_ITEM_FLOAT:
                print_help_section( p_parser, p_section, b_color,
                                    b_description );
                p_section = NULL;
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
                print_help_section( p_parser, p_section, b_color,
                                    b_description );
                p_section = NULL;
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
            sprintf( psz_buffer, "%s%s", module_gettext( p_parser, p_item->psz_text ),
                     psz_suf );
            b_description_hack = b_description;

 description:
            psz_text = psz_buffer;
            i_cur_width = b_description && !b_description_hack
                          ? i_width_description
                          : i_width;
            if( !*psz_text ) strcpy(psz_text, " ");
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
                sprintf( psz_buffer, "%s%s",
                         module_gettext( p_parser, p_item->psz_longtext ),
                         psz_suf );
                b_description_hack = false;
                psz_spaces = psz_spaces_longtext;
                utf8_fprintf( stdout, "%s", psz_spaces );
                goto description;
            }
        }
    }

    if( b_has_advanced )
    {
        if( b_color )
            utf8_fprintf( stdout, "\n" WHITE "%s" GRAY " %s\n", _( "Note:" ),
           _( "add --advanced to your command line to see advanced options."));
        else
            utf8_fprintf( stdout, "\n%s %s\n", _( "Note:" ),
           _( "add --advanced to your command line to see advanced options."));
    }

    if( i_only_advanced > 0 )
    {
        if( b_color )
        {
            utf8_fprintf( stdout, "\n" WHITE "%s" GRAY " ", _( "Note:" ) );
            utf8_fprintf( stdout, _( "%d module(s) were not displayed because they only have advanced options.\n" ), i_only_advanced );
        }
        else
        {
            utf8_fprintf( stdout, "\n%s ", _( "Note:" ) );
            utf8_fprintf( stdout, _( "%d module(s) were not displayed because they only have advanced options.\n" ), i_only_advanced );
        }
    }
    else if( !b_found )
    {
        if( b_color )
            utf8_fprintf( stdout, "\n" WHITE "%s" GRAY "\n",
                       _( "No matching module found. Use --list or " \
                          "--list-verbose to list available modules." ) );
        else
            utf8_fprintf( stdout, "\n%s\n",
                       _( "No matching module found. Use --list or " \
                          "--list-verbose to list available modules." ) );
    }

    /* Release the module list */
    module_list_free (list);
}

/*****************************************************************************
 * ListModules: list the available modules with their description
 *****************************************************************************
 * Print a list of all available modules (builtins and plugins) and a short
 * description for each one.
 *****************************************************************************/
static void ListModules( libvlc_int_t *p_this, bool b_verbose )
{
    module_t *p_parser;

    bool b_color = var_InheritBool( p_this, "color" );

#ifdef WIN32
    ShowConsole( true );
    b_color = false; // don't put color control codes in a .txt file
#else
    if( !isatty( 1 ) )
        b_color = false;
#endif

    /* List all modules */
    module_t **list = module_list_get (NULL);

    /* Enumerate each module */
    for (size_t j = 0; (p_parser = list[j]) != NULL; j++)
    {
        if( b_color )
            utf8_fprintf( stdout, GREEN"  %-22s "WHITE"%s\n"GRAY,
                          p_parser->psz_object_name,
                          module_gettext( p_parser, p_parser->psz_longname ) );
        else
            utf8_fprintf( stdout, "  %-22s %s\n",
                          p_parser->psz_object_name,
                          module_gettext( p_parser, p_parser->psz_longname ) );

        if( b_verbose )
        {
            char *const *pp_shortcuts = p_parser->pp_shortcuts;
            for( unsigned i = 0; i < p_parser->i_shortcuts; i++ )
            {
                if( strcmp( pp_shortcuts[i], p_parser->psz_object_name ) )
                {
                    if( b_color )
                        utf8_fprintf( stdout, CYAN"   s %s\n"GRAY,
                                      pp_shortcuts[i] );
                    else
                        utf8_fprintf( stdout, "   s %s\n",
                                      pp_shortcuts[i] );
                }
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
    }
    module_list_free (list);

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
    ShowConsole( true );
#endif

    utf8_fprintf( stdout, _("VLC version %s (%s)\n"), VERSION_MESSAGE,
                  psz_vlc_changeset );
    utf8_fprintf( stdout, _("Compiled by %s on %s (%s)\n"),
             VLC_CompileBy(), VLC_CompileHost(), __DATE__" "__TIME__ );
    utf8_fprintf( stdout, _("Compiler: %s\n"), VLC_Compiler() );
    utf8_fprintf( stdout, "%s", LICENSE_MSG );

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
static void ShowConsole( bool b_dofile )
{
#   ifndef UNDER_CE
    FILE *f_help = NULL;

    if( getenv( "PWD" ) && getenv( "PS1" ) ) return; /* cygwin shell */

    AllocConsole();
    /* Use the ANSI code page (e.g. Windows-1252) as expected by the LibVLC
     * Unicode/locale subsystem. By default, we have the obsolecent OEM code
     * page (e.g. CP437 or CP850). */
    SetConsoleOutputCP (GetACP ());
    SetConsoleTitle ("VLC media player version "PACKAGE_VERSION);

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
#elif !defined (UNDER_CE)
    CONSOLE_SCREEN_BUFFER_INFO buf;

    if (GetConsoleScreenBufferInfo (GetStdHandle (STD_OUTPUT_HANDLE), &buf))
        i_width = buf.dwSize.X;
#endif

    return i_width;
}
