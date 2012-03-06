/*****************************************************************************
 * libvlc.c: libvlc instances creation and deletion, interfaces handling
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 * $Id$
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Rémi Denis-Courmont <rem # videolan : org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
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
#include "../lib/libvlc_internal.h"
#include <vlc_input.h>

#include "modules/modules.h"
#include "config/configuration.h"

#include <stdio.h>                                              /* sprintf() */
#include <string.h>
#include <stdlib.h>                                                /* free() */

#include "config/vlc_getopt.h"

#ifdef HAVE_LOCALE_H
#   include <locale.h>
#endif
#ifdef HAVE_UNISTD_H
#   include <unistd.h> /* isatty() */
#endif

#ifdef HAVE_DBUS
/* used for one-instance mode */
#   include <dbus/dbus.h>
#endif


#include <vlc_media_library.h>
#include <vlc_playlist.h>
#include <vlc_interface.h>

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

#if !defined(WIN32) && !defined(__OS2__)
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
    /* Allocate a libvlc instance object */
    p_libvlc = vlc_custom_create( (vlc_object_t *)NULL, sizeof (*priv),
                                  "libvlc" );
    if( p_libvlc == NULL )
        return NULL;

    priv = libvlc_priv (p_libvlc);
    priv->p_playlist = NULL;
    priv->p_ml = NULL;
    priv->p_dialog_provider = NULL;
    priv->p_vlm = NULL;

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
    vlc_ExitInit( &priv->exit );

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
    libvlc_priv_t *priv = libvlc_priv (p_libvlc);
    char *       psz_modules = NULL;
    char *       psz_parser = NULL;
    char *       psz_control = NULL;
    playlist_t  *p_playlist = NULL;
    char        *psz_val;

    /* System specific initialization code */
    system_Init();

    /* Initialize the module bank and load the configuration of the
     * main module. We need to do this at this stage to be able to display
     * a short help if required by the user. (short help == main module
     * options) */
    module_InitBank ();

    /* Get command line options that affect module loading. */
    if( config_LoadCmdLine( p_libvlc, i_argc, ppsz_argv, NULL ) )
    {
        module_EndBank (false);
        return VLC_EGENERIC;
    }
    priv->i_verbose = var_InheritInteger( p_libvlc, "verbose" );

    /* Announce who we are (TODO: only first instance?) */
    msg_Dbg( p_libvlc, "VLC media player - %s", VERSION_MESSAGE );
    msg_Dbg( p_libvlc, "%s", COPYRIGHT_MESSAGE );
    msg_Dbg( p_libvlc, "revision %s", psz_vlc_changeset );
    msg_Dbg( p_libvlc, "configured with %s", CONFIGURE_LINE );

    /* Load the builtins and plugins into the module_bank.
     * We have to do it before config_Load*() because this also gets the
     * list of configuration options exported by each module and loads their
     * default values. */
    size_t module_count = module_LoadPlugins (p_libvlc);

    /*
     * Override default configuration with config file settings
     */
    if( !var_InheritBool( p_libvlc, "ignore-config" ) )
    {
        if( var_InheritBool( p_libvlc, "reset-config" ) )
            config_SaveConfigFile( p_libvlc ); /* Save default config */
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
        MessageBox (NULL, TEXT("The command line options could not be parsed.\n"
                    "Make sure they are valid."), TEXT("VLC media player"),
                    MB_OK|MB_ICONERROR);
#endif
        module_EndBank (true);
        return VLC_EGENERIC;
    }
    priv->i_verbose = var_InheritInteger( p_libvlc, "verbose" );

    /*
     * Support for gettext
     */
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )
# if defined (WIN32) || defined (__APPLE__)
    /* Check if the user specified a custom language */
    char *lang = var_InheritString (p_libvlc, "language");
    if (lang != NULL && strcmp (lang, "auto"))
        SetLanguage (lang);
    free (lang);
# endif
    vlc_bindtextdomain (PACKAGE_NAME);
#endif
    /*xgettext: Translate "C" to the language code: "fr", "en_GB", "nl", "ru"... */
    msg_Dbg( p_libvlc, "translation test: code is \"%s\"", _("C") );

    if (config_PrintHelp (VLC_OBJECT(p_libvlc)))
    {
        module_EndBank (true);
        return VLC_EEXITSUCCESS;
    }

    if( module_count <= 1 )
    {
        msg_Err( p_libvlc, "No plugins found! Check your VLC installation.");
        module_EndBank (true);
        return VLC_ENOITEM;
    }

#ifdef HAVE_DAEMON
    /* Check for daemon mode */
    if( var_InheritBool( p_libvlc, "daemon" ) )
    {
        char *psz_pidfile = NULL;

        if( daemon( 1, 0) != 0 )
        {
            msg_Err( p_libvlc, "Unable to fork vlc to daemon mode" );
            module_EndBank (true);
            return VLC_EEXIT;
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
    }
#endif

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
                        system_End( );
                        exit( 1 );
                    }

                    /* append MRLs */
                    dbus_message_iter_init_append( p_dbus_msg, &dbus_args );
                    if ( !dbus_message_iter_append_basic( &dbus_args,
                                DBUS_TYPE_STRING, &psz_mrl ) )
                    {
                        dbus_message_unref( p_dbus_msg );
                        free( psz_mrl );
                        system_End( );
                        exit( 1 );
                    }
                    free( psz_mrl );

                    if( !dbus_message_iter_append_basic( &dbus_args,
                                DBUS_TYPE_OBJECT_PATH, &psz_after_track ) )
                    {
                        dbus_message_unref( p_dbus_msg );
                        system_End( );
                        exit( 1 );
                    }

                    b_play = TRUE;
                    if( var_InheritBool( p_libvlc, "playlist-enqueue" ) )
                        b_play = FALSE;

                    if ( !dbus_message_iter_append_basic( &dbus_args,
                                DBUS_TYPE_BOOLEAN, &b_play ) )
                    {
                        dbus_message_unref( p_dbus_msg );
                        system_End( );
                        exit( 1 );
                    }

                    /* send message and get a handle for a reply */
                    if ( !dbus_connection_send_with_reply ( p_conn,
                                p_dbus_msg, &p_dbus_pending, -1 ) )
                    {
                        msg_Err( p_libvlc, "D-Bus problem" );
                        dbus_message_unref( p_dbus_msg );
                        system_End( );
                        exit( 1 );
                    }

                    if ( NULL == p_dbus_pending )
                    {
                        msg_Err( p_libvlc, "D-Bus problem" );
                        dbus_message_unref( p_dbus_msg );
                        system_End( );
                        exit( 1 );
                    }
                    dbus_connection_flush( p_conn );
                    dbus_message_unref( p_dbus_msg );
                    /* block until we receive a reply */
                    dbus_pending_call_block( p_dbus_pending );
                    dbus_pending_call_unref( p_dbus_pending );
                } /* processes all command line MRLs */

                /* bye bye */
                system_End( );
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

    vlc_CPU_dump( VLC_OBJECT(p_libvlc) );
    /*
     * Choose the best memcpy module
     */
    priv->p_memcpy_module = module_need( p_libvlc, "memcpy", "$memcpy", false );
    /* Avoid being called "memcpy":*/
    vlc_object_set_name( p_libvlc, "main" );

    priv->b_stats = var_InheritBool( p_libvlc, "stats" );

    /*
     * Initialize hotkey handling
     */
    priv->actions = vlc_InitActions( p_libvlc );

    /* Create a variable for showing the fullscreen interface */
    var_Create( p_libvlc, "intf-toggle-fscontrol", VLC_VAR_BOOL );
    var_SetBool( p_libvlc, "intf-toggle-fscontrol", true );

    /* Create a variable for the Boss Key */
    var_Create( p_libvlc, "intf-boss", VLC_VAR_VOID );

    /* Create a variable for showing the main interface */
    var_Create( p_libvlc, "intf-show", VLC_VAR_BOOL );

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
        module_EndBank (true);
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

    if( var_InheritBool( p_libvlc, "file-logging" )
#ifdef HAVE_SYSLOG_H
        && !var_InheritBool( p_libvlc, "syslog" )
#endif
        )
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
#if defined (WIN32) || defined (__OS2__)
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

    msg_Dbg( p_libvlc, "removing stats" );

#if !defined( WIN32 ) && !defined( __OS2__ )
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
    module_EndBank (true);

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

    system_End( );

    /* Destroy mutexes */
    vlc_ExitDestroy( &priv->exit );
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
#if !defined( WIN32 ) && !defined( __OS2__ )
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
