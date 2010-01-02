/*****************************************************************************
 * skin_main.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_plugin.h>
#include <vlc_input.h>
#include <vlc_demux.h>
#include <vlc_playlist.h>
#include <vlc_threads.h>
#include <vlc_vout_window.h>

#include "dialogs.hpp"
#include "os_factory.hpp"
#include "os_loop.hpp"
#include "var_manager.hpp"
#include "vlcproc.hpp"
#include "theme_loader.hpp"
#include "theme.hpp"
#include "theme_repository.hpp"
#include "vout_window.hpp"
#include "vout_manager.hpp"
#include "../parser/interpreter.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_quit.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../commands/cmd_minimize.hpp"
#include "../commands/cmd_playlist.hpp"

//---------------------------------------------------------------------------
// Exported interface functions.
//---------------------------------------------------------------------------
#ifdef WIN32_SKINS
extern "C" __declspec( dllexport )
    int __VLC_SYMBOL( vlc_entry ) ( module_t *p_module );
#endif


//---------------------------------------------------------------------------
// Local prototypes
//---------------------------------------------------------------------------
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );
static void *Run  ( void * );

static int DemuxOpen( vlc_object_t * );
static int Demux( demux_t * );
static int DemuxControl( demux_t *, int, va_list );

//---------------------------------------------------------------------------
// Prototypes for configuration callbacks
//---------------------------------------------------------------------------
static int onSystrayChange( vlc_object_t *pObj, const char *pVariable,
                            vlc_value_t oldVal, vlc_value_t newVal,
                            void *pParam );
static int onTaskBarChange( vlc_object_t *pObj, const char *pVariable,
                            vlc_value_t oldVal, vlc_value_t newVal,
                            void *pParam );

static struct
{
    intf_thread_t *intf;
    vlc_mutex_t mutex;
} skin_load = { NULL, VLC_STATIC_MUTEX };

//---------------------------------------------------------------------------
// Open: initialize interface
//---------------------------------------------------------------------------
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    // Allocate instance and initialize some members
    p_intf->p_sys = (intf_sys_t *) calloc( 1, sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
        return VLC_ENOMEM;

    // Suscribe to messages bank
#if 0
    p_intf->p_sys->p_sub = msg_Subscribe( p_intf );
#endif

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->p_playlist = pl_Hold( p_intf );
    if( !p_intf->p_sys->p_playlist )
    {
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }

    // Initialize "singleton" objects
    p_intf->p_sys->p_logger = NULL;
    p_intf->p_sys->p_queue = NULL;
    p_intf->p_sys->p_dialogs = NULL;
    p_intf->p_sys->p_interpreter = NULL;
    p_intf->p_sys->p_osFactory = NULL;
    p_intf->p_sys->p_osLoop = NULL;
    p_intf->p_sys->p_varManager = NULL;
    p_intf->p_sys->p_voutManager = NULL;
    p_intf->p_sys->p_vlcProc = NULL;
    p_intf->p_sys->p_repository = NULL;

    // No theme yet
    p_intf->p_sys->p_theme = NULL;

    // Create a variable to be notified of skins to be loaded
    var_Create( p_intf, "skin-to-load", VLC_VAR_STRING );

    vlc_mutex_init( &p_intf->p_sys->vout_lock );
    vlc_cond_init( &p_intf->p_sys->vout_wait );

    vlc_mutex_init( &p_intf->p_sys->init_lock );
    vlc_cond_init( &p_intf->p_sys->init_wait );

    vlc_mutex_lock( &p_intf->p_sys->init_lock );
    p_intf->p_sys->b_ready = false;

    if( vlc_clone( &p_intf->p_sys->thread, Run, p_intf,
                               VLC_THREAD_PRIORITY_LOW ) )
    {
        vlc_mutex_unlock( &p_intf->p_sys->init_lock );

        vlc_cond_destroy( &p_intf->p_sys->init_wait );
        vlc_mutex_destroy( &p_intf->p_sys->init_lock );
        vlc_cond_destroy( &p_intf->p_sys->vout_wait );
        vlc_mutex_destroy( &p_intf->p_sys->vout_lock );
        pl_Release( p_intf->p_sys->p_playlist );
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }

    while( !p_intf->p_sys->b_ready )
        vlc_cond_wait( &p_intf->p_sys->init_wait, &p_intf->p_sys->init_lock );
    vlc_mutex_unlock( &p_intf->p_sys->init_lock );

    vlc_mutex_lock( &skin_load.mutex );
    skin_load.intf = p_intf;
    vlc_mutex_unlock( &skin_load.mutex );

    return VLC_SUCCESS;
}

//---------------------------------------------------------------------------
// Close: destroy interface
//---------------------------------------------------------------------------
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    msg_Dbg( p_intf, "closing skins2 module" );

    vlc_mutex_lock( &skin_load.mutex );
    skin_load.intf = NULL;
    vlc_mutex_unlock( &skin_load.mutex);

    vlc_join( p_intf->p_sys->thread, NULL );

    vlc_mutex_destroy( &p_intf->p_sys->init_lock );
    vlc_cond_destroy( &p_intf->p_sys->init_wait );

    if( p_intf->p_sys->p_playlist )
        pl_Release( p_this );

    // Unsubscribe from messages bank
#if 0
    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );
#endif

    vlc_cond_destroy( &p_intf->p_sys->vout_wait );
    vlc_mutex_destroy( &p_intf->p_sys->vout_lock );

    // Destroy structure
    free( p_intf->p_sys );
}


//---------------------------------------------------------------------------
// Run: main loop
//---------------------------------------------------------------------------
static void *Run( void * p_obj )
{
    int canc = vlc_savecancel();

    intf_thread_t *p_intf = (intf_thread_t *)p_obj;

    bool b_error = false;
    char *skin_last = NULL;
    ThemeLoader *pLoader = NULL;
    OSLoop *loop = NULL;

    vlc_mutex_lock( &p_intf->p_sys->init_lock );

    // Initialize singletons
    if( OSFactory::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "cannot initialize OSFactory" );
        b_error = true;
        goto end;
    }
    if( AsyncQueue::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "cannot initialize AsyncQueue" );
        b_error = true;
        goto end;
    }
    if( Interpreter::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "cannot instanciate Interpreter" );
        b_error = true;
        goto end;
    }
    if( VarManager::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "cannot instanciate VarManager" );
        b_error = true;
        goto end;
    }
    if( VlcProc::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "cannot initialize VLCProc" );
        b_error = true;
        goto end;
    }
    if( VoutManager::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "cannot instanciate VoutManager" );
        b_error = true;
        goto end;
    }
    if( ThemeRepository::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "cannot instanciate ThemeRepository" );
        b_error = true;
        goto end;
    }
    if( Dialogs::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "cannot instanciate qt4 dialogs provider" );
        b_error = true;
        goto end;
    }

    // Load a theme
    skin_last = config_GetPsz( p_intf, "skins2-last" );
    pLoader = new ThemeLoader( p_intf );

    if( !skin_last || !pLoader->load( skin_last ) )
    {
        // No skins (not even the default one). let's quit
        CmdQuit *pCmd = new CmdQuit( p_intf );
        AsyncQueue *pQueue = AsyncQueue::instance( p_intf );
        pQueue->push( CmdGenericPtr( pCmd ) );
        msg_Err( p_intf, "no skins found : exiting");
    }

    delete pLoader;
    free( skin_last );

    // Get the instance of OSLoop
    loop = OSFactory::instance( p_intf )->getOSLoop();

    // Signal the main thread this thread is now ready
    p_intf->p_sys->b_ready = true;
    vlc_cond_signal( &p_intf->p_sys->init_wait );
    vlc_mutex_unlock( &p_intf->p_sys->init_lock );

    // Enter the main event loop
    loop->run();

    // Destroy OSLoop
    OSFactory::instance( p_intf )->destroyOSLoop();

    // save and delete the theme
    if( p_intf->p_sys->p_theme )
    {
        p_intf->p_sys->p_theme->saveConfig();

        delete p_intf->p_sys->p_theme;
        p_intf->p_sys->p_theme = NULL;

        msg_Dbg( p_intf, "current theme deleted" );
    }

    // save config file
    config_SaveConfigFile( p_intf, NULL );

end:
    // Destroy "singleton" objects
    Dialogs::destroy( p_intf );
    ThemeRepository::destroy( p_intf );
    VoutManager::destroy( p_intf );
    VlcProc::destroy( p_intf );
    VarManager::destroy( p_intf );
    Interpreter::destroy( p_intf );
    AsyncQueue::destroy( p_intf );
    OSFactory::destroy( p_intf );

    if( b_error )
    {
        p_intf->p_sys->b_ready = true;
        vlc_cond_signal( &p_intf->p_sys->init_wait );
        vlc_mutex_unlock( &p_intf->p_sys->init_lock );

        libvlc_Quit( p_intf->p_libvlc );
    }

    vlc_restorecancel(canc);
    return NULL;
}

static vlc_mutex_t serializer = VLC_STATIC_MUTEX;

// Callbacks for vout requests
static int WindowOpen( vlc_object_t *p_this )
{
    vout_window_t *pWnd = (vout_window_t *)p_this;

    vlc_mutex_lock( &skin_load.mutex );
    intf_thread_t *pIntf = skin_load.intf;
    if( pIntf )
        vlc_object_hold( pIntf );
    vlc_mutex_unlock( &skin_load.mutex );

    if( pIntf == NULL )
        return VLC_EGENERIC;

    if( !config_GetInt( pIntf, "skinned-video") )
    {
        vlc_object_release( pIntf );
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( &serializer );

    pWnd->hwnd = VoutManager::getWindow( pIntf, pWnd );

    if( pWnd->hwnd )
    {
        pWnd->control = &VoutManager::controlWindow;
        pWnd->sys = (vout_window_sys_t*)pIntf;

        vlc_mutex_unlock( &serializer );
        return VLC_SUCCESS;
    }
    else
    {
        vlc_object_release( pIntf );
        vlc_mutex_unlock( &serializer );
        return VLC_EGENERIC;
    }
}

static void WindowClose( vlc_object_t *p_this )
{
    vout_window_t *pWnd = (vout_window_t *)p_this;
    intf_thread_t *pIntf = (intf_thread_t *)pWnd->sys;

    VoutManager::releaseWindow( pIntf, pWnd );

    vlc_object_release( pIntf );
}

//---------------------------------------------------------------------------
// DemuxOpen: initialize demux
//---------------------------------------------------------------------------
static int DemuxOpen( vlc_object_t *p_this )
{
    demux_t *p_demux = (demux_t*)p_this;
    intf_thread_t *p_intf;
    char *ext;

    // Needed callbacks
    p_demux->pf_demux   = Demux;
    p_demux->pf_control = DemuxControl;

    // Test that we have a valid .vlt or .wsz file, based on the extension
    // TODO: an actual check of the contents would be better...
    if( ( ext = strchr( p_demux->psz_path, '.' ) ) == NULL ||
        ( strcasecmp( ext, ".vlt" ) && strcasecmp( ext, ".wsz" ) ) )
    {
        return VLC_EGENERIC;
    }

    vlc_mutex_lock( &skin_load.mutex );
    p_intf = skin_load.intf;
    if( p_intf )
        vlc_object_hold( p_intf );
    vlc_mutex_unlock( &skin_load.mutex );

    if( p_intf != NULL )
    {
        playlist_t *p_playlist = pl_Hold( p_this );
        // Make sure the item is deleted afterwards
        /// \bug does not always work
        playlist_CurrentPlayingItem( p_playlist )->i_flags |= PLAYLIST_REMOVE_FLAG;
        pl_Release( p_this );

        var_SetString( p_intf, "skin-to-load", p_demux->psz_path );
        vlc_object_release( p_intf );
    }
    else
    {
        msg_Warn( p_this,
                  "skin could not be loaded (not using skins2 intf)" );
    }

    return VLC_SUCCESS;
}


//---------------------------------------------------------------------------
// Demux: return EOF
//---------------------------------------------------------------------------
static int Demux( demux_t *p_demux )
{
    return 0;
}


//---------------------------------------------------------------------------
// DemuxControl
//---------------------------------------------------------------------------
static int DemuxControl( demux_t *p_demux, int i_query, va_list args )
{
    return demux_vaControlHelper( p_demux->s, 0, 0, 0, 1, i_query, args );
}


//---------------------------------------------------------------------------
// Callbacks
//---------------------------------------------------------------------------

/// Callback for the systray configuration option
static int onSystrayChange( vlc_object_t *pObj, const char *pVariable,
                            vlc_value_t oldVal, vlc_value_t newVal,
                            void *pParam )
{
    intf_thread_t *pIntf;

    vlc_mutex_lock( &skin_load.mutex );
    pIntf = skin_load.intf;
    if( pIntf )
        vlc_object_hold( pIntf );
    vlc_mutex_unlock( &skin_load.mutex );

    if( pIntf == NULL )
    {
        return VLC_EGENERIC;
    }

    AsyncQueue *pQueue = AsyncQueue::instance( pIntf );
    if( newVal.b_bool )
    {
        CmdAddInTray *pCmd = new CmdAddInTray( pIntf );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }
    else
    {
        CmdRemoveFromTray *pCmd = new CmdRemoveFromTray( pIntf );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }

    vlc_object_release( pIntf );
    return VLC_SUCCESS;
}


/// Callback for the systray configuration option
static int onTaskBarChange( vlc_object_t *pObj, const char *pVariable,
                            vlc_value_t oldVal, vlc_value_t newVal,
                            void *pParam )
{
    intf_thread_t *pIntf;

    vlc_mutex_lock( &skin_load.mutex );
    pIntf = skin_load.intf;
    if( pIntf )
        vlc_object_hold( pIntf );
    vlc_mutex_unlock( &skin_load.mutex );

    if( pIntf == NULL )
    {
        return VLC_EGENERIC;
    }

    AsyncQueue *pQueue = AsyncQueue::instance( pIntf );
    if( newVal.b_bool )
    {
        CmdAddInTaskBar *pCmd = new CmdAddInTaskBar( pIntf );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }
    else
    {
        CmdRemoveFromTaskBar *pCmd = new CmdRemoveFromTaskBar( pIntf );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }

    vlc_object_release( pIntf );
    return VLC_SUCCESS;
}


//---------------------------------------------------------------------------
// Module descriptor
//---------------------------------------------------------------------------
#define SKINS2_LAST      N_("Skin to use")
#define SKINS2_LAST_LONG N_("Path to the skin to use.")
#define SKINS2_CONFIG      N_("Config of last used skin")
#define SKINS2_CONFIG_LONG N_("Windows configuration of the last skin used. " \
        "This option is updated automatically, do not touch it." )
#define SKINS2_SYSTRAY      N_("Systray icon")
#define SKINS2_SYSTRAY_LONG N_("Show a systray icon for VLC")
#define SKINS2_TASKBAR      N_("Show VLC on the taskbar")
#define SKINS2_TASKBAR_LONG N_("Show VLC on the taskbar")
#define SKINS2_TRANSPARENCY      N_("Enable transparency effects")
#define SKINS2_TRANSPARENCY_LONG N_("You can disable all transparency effects"\
    " if you want. This is mainly useful when moving windows does not behave" \
    " correctly.")
#define SKINS2_PLAYLIST N_("Use a skinned playlist")
#define SKINS2_PLAYLIST_LONG N_("Use a skinned playlist")
#define SKINS2_VIDEO N_("Display video in a skinned window if any")
#define SKINS2_VIDEO_LONG N_( \
    "When set to 'no', this parameter is intended to give old skins a chance" \
    " to play back video even though no video tag is implemented")

vlc_module_begin ()
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_MAIN )
    add_file( "skins2-last", "", NULL, SKINS2_LAST, SKINS2_LAST_LONG,
              true )
        change_autosave ()
    add_string( "skins2-config", "", NULL, SKINS2_CONFIG, SKINS2_CONFIG_LONG,
                true )
        change_autosave ()
        change_internal ()
#ifdef WIN32
    add_bool( "skins2-systray", false, onSystrayChange, SKINS2_SYSTRAY,
              SKINS2_SYSTRAY_LONG, false );
    add_bool( "skins2-taskbar", true, onTaskBarChange, SKINS2_TASKBAR,
              SKINS2_TASKBAR_LONG, false );
#endif
    add_bool( "skins2-transparency", false, NULL, SKINS2_TRANSPARENCY,
              SKINS2_TRANSPARENCY_LONG, false );

    add_bool( "skinned-playlist", true, NULL, SKINS2_PLAYLIST,
              SKINS2_PLAYLIST_LONG, false );
    add_bool( "skinned-video", true, NULL, SKINS2_VIDEO,
              SKINS2_VIDEO_LONG, false );
    set_shortname( N_("Skins"))
    set_description( N_("Skinnable Interface") )
    set_capability( "interface", 30 )
    set_callbacks( Open, Close )
    add_shortcut( "skins" )

    add_submodule ()
#ifdef WIN32
        set_capability( "vout window hwnd", 51 )
#else
        set_capability( "vout window xid", 51 )
#endif
        set_callbacks( WindowOpen, WindowClose )

    add_submodule ()
        set_description( N_("Skins loader demux") )
        set_capability( "demux", 5 )
        set_callbacks( DemuxOpen, NULL )
        add_shortcut( "skins" )

vlc_module_end ()
