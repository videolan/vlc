/*****************************************************************************
 * dialogs.cpp
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#include "dialogs.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_change_skin.hpp"
#include "../commands/cmd_quit.hpp"
#include "../commands/cmd_playlist.hpp"
#include "../commands/cmd_playtree.hpp"


/// Callback called when a new skin is chosen
void Dialogs::showChangeSkinCB( intf_dialog_args_t *pArg )
{
    intf_thread_t *pIntf = (intf_thread_t *)pArg->p_arg;

    if( pArg->i_results )
    {
        if( pArg->psz_results[0] )
        {
            // Create a change skin command
            CmdChangeSkin *pCmd =
                new CmdChangeSkin( pIntf, pArg->psz_results[0] );

            // Push the command in the asynchronous command queue
            AsyncQueue *pQueue = AsyncQueue::instance( pIntf );
            pQueue->remove( "change skin" );
            pQueue->push( CmdGenericPtr( pCmd ) );
        }
    }
    else if( !pIntf->p_sys->p_theme )
    {
        // If no theme is already loaded, it's time to quit!
        CmdQuit *pCmd = new CmdQuit( pIntf );
        AsyncQueue *pQueue = AsyncQueue::instance( pIntf );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }
}


void Dialogs::showPlaylistLoadCB( intf_dialog_args_t *pArg )
{
    intf_thread_t *pIntf = (intf_thread_t *)pArg->p_arg;

    if( pArg->i_results && pArg->psz_results[0] )
    {
        // Create a Playlist Load command
        CmdPlaylistLoad *pCmd =
            new CmdPlaylistLoad( pIntf, pArg->psz_results[0] );

        // Push the command in the asynchronous command queue
        AsyncQueue *pQueue = AsyncQueue::instance( pIntf );
        pQueue->remove( "load playlist" );
        pQueue->remove( "load playtree" );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }
}


void Dialogs::showPlaylistSaveCB( intf_dialog_args_t *pArg )
{
    intf_thread_t *pIntf = (intf_thread_t *)pArg->p_arg;

    if( pArg->i_results && pArg->psz_results[0] )
    {
        // Create a Playlist Save command
        CmdPlaylistSave *pCmd =
            new CmdPlaylistSave( pIntf, pArg->psz_results[0] );

        // Push the command in the asynchronous command queue
        AsyncQueue *pQueue = AsyncQueue::instance( pIntf );
        pQueue->remove( "load playlist" );
        pQueue->remove( "load playtree" );
        pQueue->push( CmdGenericPtr( pCmd ) );
    }
}


/// Callback called when the popup menu is requested
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    Dialogs *p_dialogs = (Dialogs *)param;
    p_dialogs->showPopupMenu( new_val.b_bool != 0 );

    return VLC_SUCCESS;
}


Dialogs::Dialogs( intf_thread_t *pIntf ):
    SkinObject( pIntf ), m_pProvider( NULL ), m_pModule( NULL )
{
}


Dialogs::~Dialogs()
{
    if( m_pProvider && m_pModule )
    {
        // Detach the dialogs provider from its parent interface
        vlc_object_detach( m_pProvider );

        module_Unneed( m_pProvider, m_pModule );
        vlc_object_destroy( m_pProvider );
    }

    /* Unregister callbacks */
    var_DelCallback( getIntf()->p_sys->p_playlist, "intf-popupmenu",
                     PopupMenuCB, this );
}


Dialogs *Dialogs::instance( intf_thread_t *pIntf )
{
    if( ! pIntf->p_sys->p_dialogs )
    {
        Dialogs *pDialogs = new Dialogs( pIntf );
        if( pDialogs->init() )
        {
            // Initialization succeeded
            pIntf->p_sys->p_dialogs = pDialogs;
        }
        else
        {
            // Initialization failed
            delete pDialogs;
        }
    }
    return pIntf->p_sys->p_dialogs;
}


void Dialogs::destroy( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_dialogs )
    {
        delete pIntf->p_sys->p_dialogs;
        pIntf->p_sys->p_dialogs = NULL;
    }
}


bool Dialogs::init()
{
    // Allocate descriptor
    m_pProvider = (intf_thread_t *)vlc_object_create( getIntf(),
                                                      VLC_OBJECT_DIALOGS );
    if( m_pProvider == NULL )
    {
        msg_Err( getIntf(), "out of memory" );
        return false;
    }

    m_pModule = module_Need( m_pProvider, "dialogs provider", NULL, 0 );
    if( m_pModule == NULL )
    {
        msg_Err( getIntf(), "No suitable dialogs provider found" );
        vlc_object_destroy( m_pProvider );
        m_pProvider = NULL;
        return false;
    }

    // Attach the dialogs provider to its parent interface
    vlc_object_attach( m_pProvider, getIntf() );

    // Initialize dialogs provider
    // (returns as soon as initialization is done)
    if( m_pProvider->pf_run )
    {
        m_pProvider->pf_run( m_pProvider );
    }

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( getIntf()->p_sys->p_playlist, "intf-popupmenu",
                     PopupMenuCB, this );

    return true;
}


void Dialogs::showFileGeneric( const string &rTitle, const string &rExtensions,
                               DlgCallback callback, int flags )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        intf_dialog_args_t *p_arg =
            (intf_dialog_args_t *)malloc( sizeof(intf_dialog_args_t) );
        memset( p_arg, 0, sizeof(intf_dialog_args_t) );

        p_arg->psz_title = strdup( rTitle.c_str() );
        p_arg->psz_extensions = strdup( rExtensions.c_str() );

        p_arg->b_save = flags & kSAVE;
        p_arg->b_multiple = flags & kMULTIPLE;

        p_arg->p_arg = getIntf();
        p_arg->pf_callback = callback;

        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_FILE_GENERIC,
                                     0, p_arg );
    }
}


void Dialogs::showChangeSkin()
{
    showFileGeneric( _("Open a skin file"),
                     _("Skin files (*.vlt)|*.vlt|Skin files (*.xml)|*.xml"),
                     showChangeSkinCB, kOPEN );
}


void Dialogs::showPlaylistLoad()
{
    showFileGeneric( _("Open playlist"),
                     _("All playlists|*.pls;*.m3u;*.asx;*.b4s|M3U files|*.m3u"),
                     showPlaylistLoadCB, kOPEN );
}


void Dialogs::showPlaylistSave()
{
    showFileGeneric( _("Save playlist"), _("M3U file|*.m3u"),
                     showPlaylistSaveCB, kSAVE );
}


void Dialogs::showFileSimple( bool play )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_FILE_SIMPLE,
                                     (int)play, 0 );
    }
}


void Dialogs::showFile( bool play )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_FILE,
                                     (int)play, 0 );
    }
}


void Dialogs::showDirectory( bool play )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_DIRECTORY,
                                     (int)play, 0 );
    }
}


void Dialogs::showDisc( bool play )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_DISC,
                                     (int)play, 0 );
    }
}


void Dialogs::showNet( bool play )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_NET,
                                     (int)play, 0 );
    }
}


void Dialogs::showMessages()
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_MESSAGES, 0, 0 );
    }
}


void Dialogs::showPrefs()
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_PREFS, 0, 0 );
    }
}


void Dialogs::showFileInfo()
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_FILEINFO, 0, 0 );
    }
}


void Dialogs::showStreamingWizard()
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_WIZARD, 0, 0 );
    }
}


void Dialogs::showPopupMenu( bool bShow )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_POPUPMENU,
                                     (int)bShow, 0 );
    }
}

