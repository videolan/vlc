/*****************************************************************************
 * theme_repository.cpp
 *****************************************************************************
 * Copyright (C) 2004 VideoLAN
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
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

#include "theme_repository.hpp"


ThemeRepository *ThemeRepository::instance( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_repository == NULL )
    {
        pIntf->p_sys->p_repository = new ThemeRepository( pIntf );
    }

    return pIntf->p_sys->p_repository;
}


void ThemeRepository::destroy( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_repository )
    {
        delete pIntf->p_sys->p_repository;
        pIntf->p_sys->p_repository = NULL;
    }
}


ThemeRepository::ThemeRepository( intf_thread_t *pIntf ): SkinObject( pIntf )
{
    vlc_value_t val, text;

/*    // Create a timer to poll the status of the vlc
    OSFactory *pOsFactory = OSFactory::instance( pIntf );
    m_pTimer = pOsFactory->createOSTimer( Callback( this, &doManage ) );
    m_pTimer->start( 100, false );

    // Create and register VLC variables
    VarManager *pVarManager = VarManager::instance( getIntf() );

#define REGISTER_VAR( var, type, name ) \
    var = VariablePtr( new type( getIntf() ) ); \
    pVarManager->registerVar( var, name );
    REGISTER_VAR( m_cPlaylist, Playlist, "playlist" )
    pVarManager->registerVar( getPlaylistVar().getPositionVarPtr(),
                              "playlist.slider" );
    REGISTER_VAR( m_cVarRandom, VarBoolImpl, "playlist.isRandom" )
    REGISTER_VAR( m_cVarLoop, VarBoolImpl, "playlist.isLoop" )
    REGISTER_VAR( m_cVarRepeat, VarBoolImpl, "playlist.isRepeat" )
    REGISTER_VAR( m_cVarTime, StreamTime, "time" )
    REGISTER_VAR( m_cVarVolume, Volume, "volume" )
    REGISTER_VAR( m_cVarStream, Stream, "stream" )
    REGISTER_VAR( m_cVarMute, VarBoolImpl, "vlc.isMute" )
    REGISTER_VAR( m_cVarPlaying, VarBoolImpl, "vlc.isPlaying" )
    REGISTER_VAR( m_cVarStopped, VarBoolImpl, "vlc.isStopped" )
    REGISTER_VAR( m_cVarPaused, VarBoolImpl, "vlc.isPaused" )
    REGISTER_VAR( m_cVarSeekable, VarBoolImpl, "vlc.isSeekable" )
#undef REGISTER_VAR

    // XXX WARNING XXX
    // The object variable callbacks are called from other VLC threads,
    // so they must put commands in the queue and NOT do anything else
    // (X11 calls are not reentrant)

    // Called when the playlist changes
    var_AddCallback( pIntf->p_sys->p_playlist, "intf-change",
                     onIntfChange, this );
    // Called when the current played item changes
    var_AddCallback( pIntf->p_sys->p_playlist, "playlist-current",
                     onPlaylistChange, this );
    // Called when a playlist item changed
    var_AddCallback( pIntf->p_sys->p_playlist, "item-change",
                     onItemChange, this );
    // Called when our skins2 demux wants us to load a new skin
    var_AddCallback( pIntf, "skin-to-load", onSkinToLoad, this );

    // Callbacks for vout requests
    getIntf()->pf_request_window = &getWindow;
    getIntf()->pf_release_window = &releaseWindow;
    getIntf()->pf_control_window = &controlWindow;

    getIntf()->p_sys->p_input = NULL;*/

    var_Create( pIntf, "intf-skins", VLC_VAR_STRING |
                VLC_VAR_HASCHOICE | VLC_VAR_ISCOMMAND );
    text.psz_string = _("Select skin");
    var_Change( pIntf, "intf-skins", VLC_VAR_SETTEXT, &text, NULL );
    
    val.psz_string = "test";
    text.psz_string = "test";
    var_Change( pIntf, "intf-skins", VLC_VAR_ADDCHOICE,
                         &val, &text );

    /* Only fill the list with available modules */
/*    p_list = vlc_list_find( p_intf, VLC_OBJECT_MODULE, FIND_ANYWHERE );
    for( ppsz_parser = ppsz_interfaces; *ppsz_parser; ppsz_parser += 2 )
    {
        for( i = 0; i < p_list->i_count; i++ )
        {
            module_t *p_module = (module_t *)p_list->p_values[i].p_object;
            if( !strcmp( p_module->psz_object_name, ppsz_parser[0] ) )
            {
                val.psz_string = ppsz_parser[0];
                text.psz_string = ppsz_parser[1];
                var_Change( p_intf, "intf-switch", VLC_VAR_ADDCHOICE,
                            &val, &text );
                break;
            }
        }
    }
    vlc_list_release( p_list );
*/
    var_AddCallback( pIntf, "intf-skins", changeSkin, this );

}


ThemeRepository::~ThemeRepository()
{
/*    m_pTimer->stop();
    delete( m_pTimer );
    if( getIntf()->p_sys->p_input )
    {
        vlc_object_release( getIntf()->p_sys->p_input );
    }

    // Callbacks for vout requests
    getIntf()->pf_request_window = NULL;
    getIntf()->pf_release_window = NULL;
    getIntf()->pf_control_window = NULL;

    var_DelCallback( getIntf()->p_sys->p_playlist, "intf-change",
                     onIntfChange, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "playlist-current",
                     onPlaylistChange, this );
    var_DelCallback( getIntf()->p_sys->p_playlist, "item-change",
                     onItemChange, this );*/
}


int ThemeRepository::changeSkin( vlc_object_t *pThis, char const *pCmd,
                                 vlc_value_t oldval, vlc_value_t newval,
                                 void *pData )
{
/*    intf_thread_t *p_intf = (intf_thread_t *)p_this;

    p_intf->psz_switch_intf =
        malloc( strlen(newval.psz_string) + sizeof(",none") );
    sprintf( p_intf->psz_switch_intf, "%s,none", newval.psz_string );
    p_intf->b_die = VLC_TRUE;
*/
    return VLC_SUCCESS;
}

