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

#include <stdlib.h>
#include <vlc/input.h>
#include "dialogs.hpp"
#include "os_factory.hpp"
#include "os_loop.hpp"
#include "var_manager.hpp"
#include "vlcproc.hpp"
#include "theme_loader.hpp"
#include "theme.hpp"
#include "theme_repository.hpp"
#include "../parser/interpreter.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_quit.hpp"
#include "../commands/cmd_dialogs.hpp"


//---------------------------------------------------------------------------
// Exported interface functions.
//---------------------------------------------------------------------------
#ifdef WIN32_SKINS
extern "C" __declspec( dllexport )
    int __VLC_SYMBOL( vlc_entry ) ( module_t *p_module );
#endif


//---------------------------------------------------------------------------
// Local prototypes.
//---------------------------------------------------------------------------
static int  Open  ( vlc_object_t * );
static void Close ( vlc_object_t * );
static void Run   ( intf_thread_t * );

static int DemuxOpen( vlc_object_t * );
static int Demux( demux_t * );
static int DemuxControl( demux_t *, int, va_list );


//---------------------------------------------------------------------------
// Open: initialize interface
//---------------------------------------------------------------------------
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    // Allocate instance and initialize some members
    p_intf->p_sys = (intf_sys_t *) malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        msg_Err( p_intf, "out of memory" );
        return( VLC_ENOMEM );
    };

    p_intf->pf_run = Run;

    // Suscribe to messages bank
    p_intf->p_sys->p_sub = msg_Subscribe( p_intf, MSG_QUEUE_NORMAL );

    p_intf->p_sys->p_input = NULL;
    p_intf->p_sys->p_playlist = (playlist_t *)vlc_object_find( p_intf,
        VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( p_intf->p_sys->p_playlist == NULL )
    {
        msg_Err( p_intf, "No playlist object found" );
        msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );
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
    p_intf->p_sys->p_vlcProc = NULL;
    p_intf->p_sys->p_repository = NULL;

    // No theme yet
    p_intf->p_sys->p_theme = NULL;

    // Create a variable to be notified of skins to be loaded
    var_Create( p_intf, "skin-to-load", VLC_VAR_STRING );

    // Initialize singletons
    if( OSFactory::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "Cannot initialize OSFactory" );
        vlc_object_release( p_intf->p_sys->p_playlist );
        msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );
        return VLC_EGENERIC;
    }
    if( AsyncQueue::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "Cannot initialize AsyncQueue" );
        vlc_object_release( p_intf->p_sys->p_playlist );
        msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );
        return VLC_EGENERIC;
    }
    if( Interpreter::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "Cannot instanciate Interpreter" );
        vlc_object_release( p_intf->p_sys->p_playlist );
        msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );
        return VLC_EGENERIC;
    }
    if( VarManager::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "Cannot instanciate VarManager" );
        vlc_object_release( p_intf->p_sys->p_playlist );
        msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );
        return VLC_EGENERIC;
    }
    if( VlcProc::instance( p_intf ) == NULL )
    {
        msg_Err( p_intf, "Cannot initialize VLCProc" );
        vlc_object_release( p_intf->p_sys->p_playlist );
        msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );
        return VLC_EGENERIC;
    }
    Dialogs::instance( p_intf );
    ThemeRepository::instance( p_intf );

    // We support play on start
    p_intf->b_play = VLC_TRUE;

    return( VLC_SUCCESS );
}

//---------------------------------------------------------------------------
// Close: destroy interface
//---------------------------------------------------------------------------
static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    // Destroy "singleton" objects
    OSFactory::instance( p_intf )->destroyOSLoop();
    ThemeRepository::destroy( p_intf );
    Dialogs::destroy( p_intf );
    Interpreter::destroy( p_intf );
    AsyncQueue::destroy( p_intf );
    VarManager::destroy( p_intf );
    VlcProc::destroy( p_intf );
    OSFactory::destroy( p_intf );

    if( p_intf->p_sys->p_playlist )
    {
        vlc_object_release( p_intf->p_sys->p_playlist );
    }

    // Unsubscribe from messages bank
    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );

    // Destroy structure
    free( p_intf->p_sys );
}


//---------------------------------------------------------------------------
// Run: main loop
//---------------------------------------------------------------------------
static void Run( intf_thread_t *p_intf )
{
    // Load a theme
    ThemeLoader *pLoader = new ThemeLoader( p_intf );
    char *skin_last = config_GetPsz( p_intf, "skins2-last" );

    if( !skin_last || !*skin_last || !pLoader->load( skin_last ) )
    {
        // Get the resource path and try to load the default skin
        OSFactory *pOSFactory = OSFactory::instance( p_intf );
        const list<string> &resPath = pOSFactory->getResourcePath();
        const string &sep = pOSFactory->getDirSeparator();

        list<string>::const_iterator it;
        for( it = resPath.begin(); it != resPath.end(); it++ )
        {
            string path = (*it) + sep + "default.vlt";
            if( pLoader->load( path ) )
            {
                // Theme loaded successfully
                break;
            }
        }
        if( it == resPath.end() )
        {
            // Last chance: the user can select a new theme file
            if( Dialogs::instance( p_intf ) )
            {
                CmdDlgChangeSkin *pCmd = new CmdDlgChangeSkin( p_intf );
                AsyncQueue *pQueue = AsyncQueue::instance( p_intf );
                pQueue->push( CmdGenericPtr( pCmd ) );
            }
            else
            {
                // No dialogs provider, just quit...
                CmdQuit *pCmd = new CmdQuit( p_intf );
                AsyncQueue *pQueue = AsyncQueue::instance( p_intf );
                pQueue->push( CmdGenericPtr( pCmd ) );
                msg_Err( p_intf,
                         "Cannot show the \"open skin\" dialog: exiting...");
            }
        }
    }
    delete pLoader;

    if( skin_last )
    {
        free( skin_last );
    }

    // Get the instance of OSLoop
    OSLoop *loop = OSFactory::instance( p_intf )->getOSLoop();

    // Check if we need to start playing
    if( p_intf->b_play )
    {
        playlist_t *p_playlist =
            (playlist_t *)vlc_object_find( p_intf, VLC_OBJECT_PLAYLIST,
                                           FIND_ANYWHERE );
        if( p_playlist )
        {
            playlist_LockControl( p_playlist, PLAYLIST_AUTOPLAY );
            vlc_object_release( p_playlist );
        }
    }

    // Enter the main event loop
    loop->run();

    // Delete the theme and save the configuration of the windows
    if( p_intf->p_sys->p_theme )
    {
        p_intf->p_sys->p_theme->saveConfig();
        delete p_intf->p_sys->p_theme;
        p_intf->p_sys->p_theme = NULL;
    }
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

    p_intf = (intf_thread_t *)vlc_object_find( p_this, VLC_OBJECT_INTF,
                                               FIND_ANYWHERE );
    if( p_intf != NULL )
    {
        // Do nothing is skins2 is not the main interface
        if( var_Type( p_intf, "skin-to-load" ) == VLC_VAR_STRING )
        {
            playlist_t *p_playlist =
                (playlist_t *) vlc_object_find( p_this, VLC_OBJECT_PLAYLIST,
                                                FIND_ANYWHERE );
            if( p_playlist != NULL )
            {
                // Make sure the item is deleted afterwards
                p_playlist->pp_items[p_playlist->i_index]->b_autodeletion =
                    VLC_TRUE;
                vlc_object_release( p_playlist );
            }

            vlc_value_t val;
            val.psz_string = p_demux->psz_path;
            var_Set( p_intf, "skin-to-load", val );
        }
        else
        {
            msg_Warn( p_this,
                      "skin could not be loaded (not using skins2 intf)" );
        }

        vlc_object_release( p_intf );
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
    return demux2_vaControlHelper( p_demux->s, 0, 0, 0, 1, i_query, args );
}


//---------------------------------------------------------------------------
// Module descriptor
//---------------------------------------------------------------------------
#define SKINS2_LAST      N_("Last skin used")
#define SKINS2_LAST_LONG N_("Select the path to the last skin used.")
#define SKINS2_CONFIG      N_("Config of last used skin")
#define SKINS2_CONFIG_LONG N_("Config of last used skin.")
#define SKINS2_TRANSPARENCY      N_("Enable transparency effects")
#define SKINS2_TRANSPARENCY_LONG N_("You can disable all transparency effects"\
    " if you want. This is mainly useful when moving windows does not behave" \
    " correctly.")

vlc_module_begin();
    set_category( CAT_INTERFACE );
    set_subcategory( SUBCAT_INTERFACE_GENERAL );
    add_string( "skins2-last", "", NULL, SKINS2_LAST, SKINS2_LAST_LONG,
                VLC_TRUE );
        change_autosave();
    add_string( "skins2-config", "", NULL, SKINS2_CONFIG, SKINS2_CONFIG_LONG,
                VLC_TRUE );
        change_autosave();
#ifdef WIN32
    add_bool( "skins2-transparency", VLC_FALSE, NULL, SKINS2_TRANSPARENCY,
              SKINS2_TRANSPARENCY_LONG, VLC_FALSE );
#endif
    set_shortname( _("Skins"));
    set_description( _("Skinnable Interface") );
    set_capability( "interface", 30 );
    set_callbacks( Open, Close );
    add_shortcut( "skins" );
    set_program( "svlc" );

    add_submodule();
        set_description( _("Skins loader demux") );
        set_capability( "demux2", 5 );
        set_callbacks( DemuxOpen, NULL );

vlc_module_end();
