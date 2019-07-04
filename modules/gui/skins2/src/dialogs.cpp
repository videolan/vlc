/*****************************************************************************
 * dialogs.cpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
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

#include <sstream>
#include "dialogs.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_change_skin.hpp"
#include "../commands/cmd_quit.hpp"
#include "../commands/cmd_playlist.hpp"
#include "../commands/cmd_playtree.hpp"
#include <vlc_modules.h>
#include <vlc_url.h>

/// Callback called when a new skin is chosen
void Dialogs::showChangeSkinCB( intf_dialog_args_t *pArg )
{
    intf_thread_t *pIntf = (intf_thread_t *)pArg->p_arg;

    if( pArg->i_results )
    {
        if( pArg->psz_results[0] )
        {
            char* psz_path = vlc_uri2path( pArg->psz_results[0] );
            if( psz_path )
            {
                // Create a change skin command
                CmdChangeSkin *pCmd =
                    new CmdChangeSkin( pIntf, psz_path );
                free( psz_path );

                // Push the command in the asynchronous command queue
                AsyncQueue *pQueue = AsyncQueue::instance( pIntf );
                pQueue->push( CmdGenericPtr( pCmd ) );
            }
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
        pQueue->push( CmdGenericPtr( pCmd ) );
    }
}


/// Callback called when the popup menu is requested
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    (void)p_this; (void)psz_variable; (void)old_val;

    Dialogs *p_dialogs = (Dialogs *)param;
    p_dialogs->showPopupMenu( new_val.b_bool != 0, INTF_DIALOG_POPUPMENU );

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
        module_unneed( m_pProvider, m_pModule );
        vlc_object_delete(m_pProvider);

        /* Unregister callbacks */
        var_DelCallback( vlc_object_instance(getIntf()), "intf-popupmenu",
                         PopupMenuCB, this );
    }
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
    delete pIntf->p_sys->p_dialogs;
    pIntf->p_sys->p_dialogs = NULL;
}


bool Dialogs::init()
{
    // Allocate descriptor
    m_pProvider = (intf_thread_t *)vlc_object_create( getIntf(),
                                                    sizeof( intf_thread_t ) );
    if( m_pProvider == NULL )
        return false;

    m_pModule = module_need( m_pProvider, "dialogs provider", NULL, false );
    if( m_pModule == NULL )
    {
        vlc_object_delete(m_pProvider);
        m_pProvider = NULL;
        return false;
    }

    /* Register callback for the intf-popupmenu variable */
    var_AddCallback( vlc_object_instance(getIntf()), "intf-popupmenu",
                     PopupMenuCB, this );

    return true;
}


void Dialogs::showFileGeneric( const std::string &rTitle, const std::string &rExtensions,
                               DlgCallback callback, int flags )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        intf_dialog_args_t *p_arg = (intf_dialog_args_t*)
                                    calloc( 1, sizeof( intf_dialog_args_t ) );

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
                     _("Skin files |*.vlt;*.wsz;*.xml"),
                     showChangeSkinCB, kOPEN );
}


void Dialogs::showPlaylistLoad()
{
    std::stringstream fileTypes;
    fileTypes << _("Playlist Files |") << EXTENSIONS_PLAYLIST  << _("|All Files |*");
    showFileGeneric( _("Open playlist"),
                     fileTypes.str(),
                     showPlaylistLoadCB, kOPEN );
}


void Dialogs::showPlaylistSave()
{
    showFileGeneric( _("Save playlist"), _("XSPF playlist |*.xspf|"
                                           "M3U file |*.m3u|"
                                           "HTML playlist |*.html"),
                     showPlaylistSaveCB, kSAVE );
}

void Dialogs::showPlaylist()
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_PLAYLIST,
                                     0, NULL );
    }
}

void Dialogs::showFileSimple( bool play )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_FILE_SIMPLE,
                                     (int)play, NULL );
    }
}


void Dialogs::showFile( bool play )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_FILE,
                                     (int)play, NULL );
    }
}


void Dialogs::showDirectory( bool play )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_DIRECTORY,
                                     (int)play, NULL );
    }
}


void Dialogs::showDisc( bool play )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_DISC,
                                     (int)play, NULL );
    }
}


void Dialogs::showNet( bool play )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_NET,
                                     (int)play, NULL );
    }
}


void Dialogs::showMessages()
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_MESSAGES, 0, NULL );
    }
}


void Dialogs::showPrefs()
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_PREFS, 0, NULL );
    }
}


void Dialogs::showFileInfo()
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_FILEINFO, 0, NULL );
    }
}


void Dialogs::showStreamingWizard()
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_WIZARD, 0, NULL );
    }
}


void Dialogs::showPopupMenu( bool bShow, int popupType = INTF_DIALOG_POPUPMENU )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, popupType,
                                     (int)bShow, NULL );
    }
}

void Dialogs::showInteraction( interaction_dialog_t *p_dialog )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        intf_dialog_args_t *p_arg = (intf_dialog_args_t *)
                                    calloc( 1, sizeof(intf_dialog_args_t) );

        p_arg->p_dialog = p_dialog;
        p_arg->p_intf = getIntf();

        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_INTERACTION,
                                     0, p_arg );
    }
}

void Dialogs::sendKey( int key )
{
    if( m_pProvider && m_pProvider->pf_show_dialog )
    {
        m_pProvider->pf_show_dialog( m_pProvider, INTF_DIALOG_SENDKEY,
                                     key, NULL );
    }
}
