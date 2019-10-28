/*****************************************************************************
 * libvlc.c: libvlc instances creation and deletion, interfaces handling
 *****************************************************************************
 * Copyright (C) 1998-2008 VLC authors and VideoLAN
 *
 * Authors: Vincent Seguin <seguin@via.ecp.fr>
 *          Samuel Hocevar <sam@zoy.org>
 *          Gildas Bazin <gbazin@videolan.org>
 *          Derk-Jan Hartman <hartman at videolan dot org>
 *          Rémi Denis-Courmont
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

#include "modules/modules.h"
#include "config/configuration.h"
#include "preparser/preparser.h"
#include "media_source/media_source.h"

#include <stdio.h>                                              /* sprintf() */
#include <string.h>
#include <stdlib.h>                                                /* free() */
#include <errno.h>

#include "config/vlc_getopt.h"

#include <vlc_playlist.h>
#include <vlc_interface.h>

#include <vlc_actions.h>
#include <vlc_charset.h>
#include <vlc_dialog.h>
#include <vlc_keystore.h>
#include <vlc_fs.h>
#include <vlc_cpu.h>
#include <vlc_url.h>
#include <vlc_modules.h>
#include <vlc_media_library.h>
#include <vlc_thumbnailer.h>

#include "libvlc.h"

#include <vlc_vlm.h>

#include <assert.h>

/*****************************************************************************
 * Local prototypes
 *****************************************************************************/
static void GetFilenames  ( libvlc_int_t *, unsigned, const char *const [] );

/**
 * Allocate a blank libvlc instance, also setting the exit handler.
 * Vlc's threading system must have been initialized first
 */
libvlc_int_t * libvlc_InternalCreate( void )
{
    libvlc_int_t *p_libvlc;
    libvlc_priv_t *priv;

    /* Allocate a libvlc instance object */
    p_libvlc = (vlc_custom_create)( NULL, sizeof (*priv), "libvlc" );
    if( p_libvlc == NULL )
        return NULL;

    priv = libvlc_priv (p_libvlc);
    vlc_mutex_init(&priv->lock);
    priv->interfaces = NULL;
    priv->main_playlist = NULL;
    priv->p_vlm = NULL;
    priv->media_source_provider = NULL;

    vlc_ExitInit( &priv->exit );

    return p_libvlc;
}

static void libvlc_AddInterfaces(libvlc_int_t *libvlc, const char *varname)
{
    char *str = var_InheritString(libvlc, varname);
    if (str == NULL)
        return;

    char *state;
    char *intf = strtok_r(str, ":", &state);

    while (intf != NULL) {
        libvlc_InternalAddIntf(libvlc, intf);
        intf = strtok_r(NULL, ":", &state);
    }

    free(str);
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
    char        *psz_val;
    int          i_ret = VLC_EGENERIC;

    if (unlikely(vlc_LogPreinit(p_libvlc)))
        return VLC_ENOMEM;

    /* System specific initialization code */
    system_Init();

    /* Initialize the module bank and load the configuration of the
     * core module. We need to do this at this stage to be able to display
     * a short help if required by the user. (short help == core module
     * options) */
    module_InitBank ();

    /* Get command line options that affect module loading. */
    if( config_LoadCmdLine( p_libvlc, i_argc, ppsz_argv, NULL ) )
    {
        module_EndBank (false);
        return VLC_EGENERIC;
    }

    vlc_threads_setup (p_libvlc);

    /* Load the builtins and plugins into the module_bank.
     * We have to do it before config_Load*() because this also gets the
     * list of configuration options exported by each module and loads their
     * default values. */
    module_LoadPlugins (p_libvlc);

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
        goto error;

    vlc_LogInit(p_libvlc);

    /*
     * Support for gettext
     */
#if defined( ENABLE_NLS ) \
     && ( defined( HAVE_GETTEXT ) || defined( HAVE_INCLUDED_GETTEXT ) )
    vlc_bindtextdomain (PACKAGE_NAME);
#endif
    /*xgettext: Translate "C" to the language code: "fr", "en_GB", "nl", "ru"... */
    msg_Dbg( p_libvlc, "translation test: code is \"%s\"", _("C") );

    if (config_PrintHelp (VLC_OBJECT(p_libvlc)))
    {
        libvlc_InternalCleanup (p_libvlc);
        exit(0);
    }

    i_ret = VLC_ENOMEM;

    if( libvlc_InternalDialogInit( p_libvlc ) != VLC_SUCCESS )
        goto error;
    if( libvlc_InternalKeystoreInit( p_libvlc ) != VLC_SUCCESS )
        msg_Warn( p_libvlc, "memory keystore init failed" );

    vlc_CPU_dump( VLC_OBJECT(p_libvlc) );

    if( var_InheritBool( p_libvlc, "media-library") )
    {
        priv->p_media_library = libvlc_MlCreate( p_libvlc );
        if ( priv->p_media_library == NULL )
            msg_Warn( p_libvlc, "Media library initialization failed" );
    }

    priv->p_thumbnailer = vlc_thumbnailer_Create( VLC_OBJECT( p_libvlc ) );
    if ( priv->p_thumbnailer == NULL )
        msg_Warn( p_libvlc, "Failed to instantiate thumbnailer" );

    /*
     * Initialize hotkey handling
     */
    if( libvlc_InternalActionsInit( p_libvlc ) != VLC_SUCCESS )
        goto error;

    /*
     * Meta data handling
     */
    priv->parser = input_preparser_New(VLC_OBJECT(p_libvlc));
    if( !priv->parser )
        goto error;

    priv->media_source_provider = vlc_media_source_provider_New( VLC_OBJECT( p_libvlc ) );
    if( !priv->media_source_provider )
        goto error;

    /* variables for signalling creation of new files */
    var_Create( p_libvlc, "snapshot-file", VLC_VAR_STRING );
    var_Create( p_libvlc, "record-file", VLC_VAR_STRING );

    /* some default internal settings */
    var_Create( p_libvlc, "window", VLC_VAR_STRING );
    var_Create( p_libvlc, "vout-cb-type", VLC_VAR_INTEGER );

    /* NOTE: Because the playlist and interfaces start before this function
     * returns control to the application (DESIGN BUG!), all these variables
     * must be created (in place of libvlc_new()) and set to VLC defaults
     * (in place of VLC main()) *here*. */
    var_Create( p_libvlc, "user-agent", VLC_VAR_STRING );
    var_SetString( p_libvlc, "user-agent",
                   "VLC media player (LibVLC "VERSION")" );
    var_Create( p_libvlc, "http-user-agent", VLC_VAR_STRING );
    var_SetString( p_libvlc, "http-user-agent",
                   "VLC/"PACKAGE_VERSION" LibVLC/"PACKAGE_VERSION );
    var_Create( p_libvlc, "app-icon-name", VLC_VAR_STRING );
    var_SetString( p_libvlc, "app-icon-name", PACKAGE_NAME );
    var_Create( p_libvlc, "app-id", VLC_VAR_STRING );
    var_SetString( p_libvlc, "app-id", "org.VideoLAN.VLC" );
    var_Create( p_libvlc, "app-version", VLC_VAR_STRING );
    var_SetString( p_libvlc, "app-version", PACKAGE_VERSION );

    /* System specific configuration */
    system_Configure( p_libvlc, i_argc - vlc_optind, ppsz_argv + vlc_optind );

#ifdef ENABLE_VLM
    /* Initialize VLM if vlm-conf is specified */
    char *psz_parser = var_InheritString( p_libvlc, "vlm-conf" );
    if( psz_parser )
    {
        priv->p_vlm = vlm_New( p_libvlc, psz_parser );
        if( !priv->p_vlm )
            msg_Err( p_libvlc, "VLM initialization failed" );
        free( psz_parser );
    }
#endif

    /*
     * Load background interfaces
     */
    libvlc_AddInterfaces(p_libvlc, "extraintf");
    libvlc_AddInterfaces(p_libvlc, "control");

    if( var_InheritBool( p_libvlc, "network-synchronisation") )
        libvlc_InternalAddIntf( p_libvlc, "netsync,none" );

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
        intf_InsertItem( p_libvlc, psz_val, 0, NULL, 0 );
        free( psz_val );
    }

    /* Callbacks between interfaces */

    /* Create a variable for showing the right click menu */
    var_Create(p_libvlc, "intf-popupmenu", VLC_VAR_BOOL);

    /* Create a variable for showing the fullscreen interface */
    var_Create(p_libvlc, "intf-toggle-fscontrol", VLC_VAR_VOID);

    /* Create a variable for the Boss Key */
    var_Create(p_libvlc, "intf-boss", VLC_VAR_VOID);

    /* Create a variable for showing the main interface */
    var_Create(p_libvlc, "intf-show", VLC_VAR_VOID);

    return VLC_SUCCESS;

error:
    libvlc_InternalCleanup( p_libvlc );
    return i_ret;
}

/**
 * Cleanup a libvlc instance. The instance is not completely deallocated
 * \param p_libvlc the instance to clean
 */
void libvlc_InternalCleanup( libvlc_int_t *p_libvlc )
{
    libvlc_priv_t *priv = libvlc_priv (p_libvlc);

    if (priv->parser != NULL)
        input_preparser_Deactivate(priv->parser);

    /* Ask the interfaces to stop and destroy them */
    msg_Dbg( p_libvlc, "removing all interfaces" );
    intf_DestroyAll( p_libvlc );

    if ( priv->p_thumbnailer )
        vlc_thumbnailer_Release( priv->p_thumbnailer );

    libvlc_InternalDialogClean( p_libvlc );
    libvlc_InternalKeystoreClean( p_libvlc );

#ifdef ENABLE_VLM
    /* Destroy VLM if created in libvlc_InternalInit */
    if( priv->p_vlm )
    {
        vlm_Delete( priv->p_vlm );
    }
#endif

#if !defined( _WIN32 ) && !defined( __OS2__ )
    char *pidfile = var_InheritString( p_libvlc, "pidfile" );
    if( pidfile != NULL )
    {
        msg_Dbg( p_libvlc, "removing PID file %s", pidfile );
        if( unlink( pidfile ) )
            msg_Warn( p_libvlc, "cannot remove PID file %s: %s",
                      pidfile, vlc_strerror_c(errno) );
        free( pidfile );
    }
#endif

    if (priv->parser != NULL)
        input_preparser_Delete(priv->parser);

    if (priv->main_playlist)
        vlc_playlist_Delete(priv->main_playlist);

    if ( priv->p_media_library )
        libvlc_MlRelease( priv->p_media_library );

    if( priv->media_source_provider )
        vlc_media_source_provider_Delete( priv->media_source_provider );

    libvlc_InternalActionsClean( p_libvlc );

    /* Save the configuration */
    if( !var_InheritBool( p_libvlc, "ignore-config" ) )
        config_AutoSaveConfigFile( VLC_OBJECT(p_libvlc) );

    vlc_LogDestroy(p_libvlc->obj.logger);
    /* Free module bank. It is refcounted, so we call this each time  */
    module_EndBank (true);
#if defined(_WIN32) || defined(__OS2__)
    system_End( );
#endif
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
    vlc_object_delete(p_libvlc);
}

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

        char *mrl = NULL;
        if( strstr( args[n], "://" ) == NULL )
        {
            mrl = vlc_path2uri( args[n], NULL );
            if( !mrl )
                continue;
        }

        intf_InsertItem( p_vlc, (mrl != NULL) ? mrl : args[n], i_options,
                         ( i_options ? &args[n + 1] : NULL ),
                         VLC_INPUT_OPTION_TRUSTED );
        free( mrl );
    }
}

int vlc_MetadataRequest(libvlc_int_t *libvlc, input_item_t *item,
                        input_item_meta_request_option_t i_options,
                        const input_preparser_callbacks_t *cbs,
                        void *cbs_userdata,
                        int timeout, void *id)
{
    libvlc_priv_t *priv = libvlc_priv(libvlc);

    if (unlikely(priv->parser == NULL))
        return VLC_ENOMEM;

    input_preparser_Push( priv->parser, item, i_options, cbs, cbs_userdata, timeout, id );
    return VLC_SUCCESS;

}

/**
 * Requests extraction of the meta data for an input item (a.k.a. preparsing).
 * The actual extraction is asynchronous. It can be cancelled with
 * libvlc_MetadataCancel()
 */
int libvlc_MetadataRequest(libvlc_int_t *libvlc, input_item_t *item,
                           input_item_meta_request_option_t i_options,
                           const input_preparser_callbacks_t *cbs,
                           void *cbs_userdata,
                           int timeout, void *id)
{
    libvlc_priv_t *priv = libvlc_priv(libvlc);
    assert(i_options & META_REQUEST_OPTION_SCOPE_ANY);

    if (unlikely(priv->parser == NULL))
        return VLC_ENOMEM;

    vlc_mutex_lock( &item->lock );
    if( item->i_preparse_depth == 0 )
        item->i_preparse_depth = 1;
    vlc_mutex_unlock( &item->lock );

    return vlc_MetadataRequest(libvlc, item, i_options, cbs, cbs_userdata, timeout, id);
}

/**
 * Requests retrieving/downloading art for an input item.
 * The retrieval is performed asynchronously.
 */
int libvlc_ArtRequest(libvlc_int_t *libvlc, input_item_t *item,
                      input_item_meta_request_option_t i_options,
                      const input_fetcher_callbacks_t *cbs,
                      void *cbs_userdata)
{
    libvlc_priv_t *priv = libvlc_priv(libvlc);
    assert(i_options & META_REQUEST_OPTION_FETCH_ANY);

    if (unlikely(priv->parser == NULL))
        return VLC_ENOMEM;

    input_preparser_fetcher_Push(priv->parser, item, i_options,
                                 cbs, cbs_userdata);
    return VLC_SUCCESS;
}

/**
 * Cancels extraction of the meta data for an input item.
 *
 * This does nothing if the input item is already processed or if it was not
 * added with libvlc_MetadataRequest()
 */
void libvlc_MetadataCancel(libvlc_int_t *libvlc, void *id)
{
    libvlc_priv_t *priv = libvlc_priv(libvlc);

    if (unlikely(priv->parser == NULL))
        return;

    input_preparser_Cancel(priv->parser, id);
}
