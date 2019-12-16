/*****************************************************************************
 * vlcproc.cpp
 *****************************************************************************
 * Copyright (C) 2003-2019 the VideoLAN team
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *          Erwan Tulou      <erwan10@videolan.org>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <vlc_common.h>
#include <vlc_aout.h>
#include <vlc_vout.h>
#include <vlc_player.h>
#include <vlc_playlist.h>
#include <vlc_url.h>
#include <vlc_strings.h>

#include "vlcproc.hpp"
#include "os_factory.hpp"
#include "os_loop.hpp"
#include "os_timer.hpp"
#include "var_manager.hpp"
#include "vout_manager.hpp"
#include "fsc_window.hpp"
#include "theme.hpp"
#include "window_manager.hpp"
#include "../commands/async_queue.hpp"
#include "../commands/cmd_change_skin.hpp"
#include "../commands/cmd_show_window.hpp"
#include "../commands/cmd_quit.hpp"
#include "../commands/cmd_resize.hpp"
#include "../commands/cmd_vars.hpp"
#include "../commands/cmd_playtree.hpp"
#include "../commands/cmd_dialogs.hpp"
#include "../commands/cmd_audio.hpp"
#include "../commands/cmd_callbacks.hpp"
#include "../utils/var_bool.hpp"
#include "../utils/var_string.hpp"
#include <sstream>

#include <assert.h>

void on_playlist_items_reset( vlc_playlist_t *playlist,
                              vlc_playlist_item_t *const items[],
                              size_t count, void *data )
{
    (void)playlist;(void)items;(void)count;
    VlcProc *pThis = (VlcProc*)data;
    CmdGeneric *pCmdTree = new CmdPlaytreeReset( pThis->getIntf() );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmdTree ), true );
}

void on_playlist_items_added( vlc_playlist_t *playlist, size_t index,
                              vlc_playlist_item_t *const items[],
                              size_t count, void *data )
{
    (void)playlist;(void)items;
    VlcProc *pThis = (VlcProc*)data;
    for( size_t i = 0; i < count; i++ )
    {
        CmdGeneric *pCmdTree =
            new CmdPlaytreeAppend( pThis->getIntf(), index + i );

        // Push the command in the asynchronous command queue
        AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
        pQueue->push( CmdGenericPtr( pCmdTree ), false );
    }
}

void on_playlist_items_removed( vlc_playlist_t *playlist, size_t index,
                                size_t count, void *data )
{
    (void)playlist;
    VlcProc *pThis = (VlcProc*)data;
    for( size_t i = 0; i < count; i++ )
    {
        CmdPlaytreeDelete *pCmdTree =
            new CmdPlaytreeDelete( pThis->getIntf(), index + i );

        // Push the command in the asynchronous command queue
        AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
        pQueue->push( CmdGenericPtr( pCmdTree ), false );
    }
}

void on_playlist_items_updated( vlc_playlist_t *playlist, size_t index,
                                vlc_playlist_item_t *const items[],
                                size_t count, void *data )
{
    (void)playlist;(void)items;
    VlcProc *pThis = (VlcProc*)data;
    for( size_t i = 0; i < count; i++ )
    {
        CmdGeneric *pCmd = new CmdItemUpdate( pThis->getIntf(), index + i );

        // Push the command in the asynchronous command queue
        AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
        pQueue->push( CmdGenericPtr( pCmd ), false );
    }
}

void on_playlist_playback_repeat_changed( vlc_playlist_t *playlist,
    enum vlc_playlist_playback_repeat repeat, void *data)
{
    (void)playlist;
    vlc_value_t val = { .i_int = repeat };
    VlcProc::onGenericCallback( "repeat", val, data );
}

void on_playlist_playback_order_changed( vlc_playlist_t *playlist,
    enum vlc_playlist_playback_order order, void *data)
{
    (void)playlist;
    vlc_value_t val = { .i_int = order };
    VlcProc::onGenericCallback( "order", val, data );
}

void on_playlist_current_index_changed( vlc_playlist_t *playlist,
                                        ssize_t index, void *data )
{
    (void)playlist;
    VlcProc *pThis = (VlcProc*)data;
    msg_Dbg( pThis->getIntf(), "current index changed %i", (int)index );
    CmdGeneric *pCmd = new CmdItemPlaying( pThis->getIntf(), index );

    // Push the command in the asynchronous command queue
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ), false );
}

void on_player_current_media_changed( vlc_player_t *player,
                                      input_item_t *media, void *data )
{
    (void)player;
    vlc_value_t val = { .p_address = media };
    if( media ) input_item_Hold( media );
    VlcProc::onGenericCallback( "current_media", val, data );
}

void on_player_state_changed( vlc_player_t *player,
                              enum vlc_player_state new_state, void *data)
{
    (void)player;
    vlc_value_t val = { .i_int = new_state };
    VlcProc::onGenericCallback( "state", val, data );
}

void on_player_rate_changed( vlc_player_t *player, float new_rate, void *data )
{
    (void)player;
    vlc_value_t val = { .f_float = new_rate };
    VlcProc::onGenericCallback( "rate", val, data );
}

void on_player_capabilities_changed( vlc_player_t *player,
    int old_caps, int new_caps, void *data )
{
    (void)player;(void)old_caps;
    vlc_value_t val = { .i_int = new_caps };
    VlcProc::onGenericCallback( "capabilities", val, data );
}

void on_player_position_changed( vlc_player_t *player, vlc_tick_t time,
                                 float pos, void *data )
{
    (void)player;(void)time;
    vlc_value_t val = { .f_float = pos};
    VlcProc::onGenericCallback( "position", val, data );
}

void on_player_track_selection_changed( vlc_player_t *player,
     vlc_es_id_t *unselected_id, vlc_es_id_t *selected_id, void *data)
{
    (void)player;
    vlc_value_t val;
    const struct vlc_player_track *track;

    if( selected_id )
    {
        track = vlc_player_GetTrack( player, selected_id );
        if( track && track->fmt.i_cat == AUDIO_ES )
        {
            val.b_bool = true;
            VlcProc::onGenericCallback( "audio_es", val, data );

            val.i_int = track->fmt.i_bitrate;
            VlcProc::onGenericCallback( "bit_rate", val, data );

            val.i_int = track->fmt.audio.i_rate;
            VlcProc::onGenericCallback( "sample_rate", val, data );
        }
    }
    else if( unselected_id )
    {
        track = vlc_player_GetSelectedTrack( player, AUDIO_ES );
        if( !track )
        {
            val.b_bool = false;
            VlcProc::onGenericCallback( "audio_es", val, data );

            val.i_int = 0;
            VlcProc::onGenericCallback( "bit_rate", val, data );
            VlcProc::onGenericCallback( "sample_rate", val, data );
        }
    }
}

void on_player_titles_changed( vlc_player_t *player,
    vlc_player_title_list *titles, void *data)
{
    (void)player;
    bool isDvd = vlc_player_title_list_GetCount( titles ) > 0;
    vlc_value_t val = { .b_bool = isDvd };
    VlcProc::onGenericCallback( "isDvd", val, data );
}

void on_player_recording_changed( vlc_player_t *player,
                                  bool recording, void *data )
{
    (void)player;
    vlc_value_t val;
    val.b_bool = recording;
    VlcProc::onGenericCallback( "recording", val, data );
}

void on_player_vout_changed( vlc_player_t *player,
    enum vlc_player_vout_action action, vout_thread_t *vout,
    enum vlc_vout_order order, vlc_es_id_t *es_id, void *data )
{
    (void)player;(void)order;(void)es_id;
    vlc_value_t val = { .p_address = NULL };
    if( vout && action == VLC_PLAYER_VOUT_STARTED )
    {
        val.p_address = vout;
        vout_Hold( vout );
    }
    VlcProc::onGenericCallback( "vout", val, data );
}

void on_player_aout_volume_changed( audio_output *aout,
                                    float volume, void *data )
{
    (void)aout;
    vlc_value_t val = { .f_float = volume };
    VlcProc::onGenericCallback( "volume", val, data );
}

void on_player_aout_mute_changed( audio_output_t *aout, bool mute, void *data )
{
    (void)aout;
    vlc_value_t val = { .b_bool = mute};
    VlcProc::onGenericCallback( "mute", val, data );
}

VlcProc *VlcProc::instance( intf_thread_t *pIntf )
{
    if( pIntf->p_sys->p_vlcProc == NULL )
    {
        pIntf->p_sys->p_vlcProc = new VlcProc( pIntf );
    }

    return pIntf->p_sys->p_vlcProc;
}


void VlcProc::destroy( intf_thread_t *pIntf )
{
    delete pIntf->p_sys->p_vlcProc;
    pIntf->p_sys->p_vlcProc = NULL;
}

#define SET_BOOL(m,v)         ((VarBoolImpl*)(m).get())->set(v)
#define SET_STREAMTIME(m,v,b) ((StreamTime*)(m).get())->set(v,b)
#define SET_TEXT(m,v)         ((VarText*)(m).get())->set(v)
#define SET_STRING(m,v)       ((VarString*)(m).get())->set(v)
#define SET_VOLUME(m,v,b)     ((Volume*)(m).get())->setVolume(v,b)

VlcProc::VlcProc( intf_thread_t *pIntf ): SkinObject( pIntf ),
    m_varEqBands( pIntf ), m_pVout( NULL ), m_pAout( NULL ),
    mPlaylistListenerId( NULL ),
    mPlayerListenerId( NULL ),
    mPlayerAoutListenerId( NULL ),
    mPlayerVoutListenerId( NULL )
{
    // Create and register VLC variables
    VarManager *pVarManager = VarManager::instance( getIntf() );

#define REGISTER_VAR( var, type, name ) \
    var = VariablePtr( new type( getIntf() ) ); \
    pVarManager->registerVar( var, name );
    REGISTER_VAR( m_cVarRandom, VarBoolImpl, "playlist.isRandom" )
    REGISTER_VAR( m_cVarLoop, VarBoolImpl, "playlist.isLoop" )
    REGISTER_VAR( m_cVarRepeat, VarBoolImpl, "playlist.isRepeat" )
    REGISTER_VAR( m_cPlaytree, Playtree, "playtree" )
    pVarManager->registerVar( getPlaytreeVar().getPositionVarPtr(),
                              "playtree.slider" );
    pVarManager->registerVar( m_cVarRandom, "playtree.isRandom" );
    pVarManager->registerVar( m_cVarLoop, "playtree.isLoop" );

    REGISTER_VAR( m_cVarPlaying, VarBoolImpl, "vlc.isPlaying" )
    REGISTER_VAR( m_cVarStopped, VarBoolImpl, "vlc.isStopped" )
    REGISTER_VAR( m_cVarPaused, VarBoolImpl, "vlc.isPaused" )

    /* Input variables */
    pVarManager->registerVar( m_cVarRepeat, "playtree.isRepeat" );
    REGISTER_VAR( m_cVarTime, StreamTime, "time" )
    REGISTER_VAR( m_cVarSeekable, VarBoolImpl, "vlc.isSeekable" )
    REGISTER_VAR( m_cVarDvdActive, VarBoolImpl, "dvd.isActive" )

    REGISTER_VAR( m_cVarRecordable, VarBoolImpl, "vlc.canRecord" )
    REGISTER_VAR( m_cVarRecording, VarBoolImpl, "vlc.isRecording" )

    /* Vout variables */
    REGISTER_VAR( m_cVarFullscreen, VarBoolImpl, "vlc.isFullscreen" )
    REGISTER_VAR( m_cVarHasVout, VarBoolImpl, "vlc.hasVout" )

    /* Aout variables */
    REGISTER_VAR( m_cVarHasAudio, VarBoolImpl, "vlc.hasAudio" )
    REGISTER_VAR( m_cVarVolume, Volume, "volume" )
    REGISTER_VAR( m_cVarMute, VarBoolImpl, "vlc.isMute" )
    REGISTER_VAR( m_cVarEqualizer, VarBoolImpl, "equalizer.isEnabled" )
    REGISTER_VAR( m_cVarEqPreamp, EqualizerPreamp, "equalizer.preamp" )

#undef REGISTER_VAR
    m_cVarSpeed = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarSpeed, "speed" );
    SET_TEXT( m_cVarSpeed, UString( getIntf(), "1") );
    m_cVarStreamName = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamName, "streamName" );
    m_cVarStreamURI = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamURI, "streamURI" );
    m_cVarStreamBitRate = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamBitRate, "bitrate" );
    m_cVarStreamSampleRate = VariablePtr( new VarText( getIntf(), false ) );
    pVarManager->registerVar( m_cVarStreamSampleRate, "samplerate" );
    m_cVarStreamArt = VariablePtr( new VarString( getIntf() ) );
    pVarManager->registerVar( m_cVarStreamArt, "streamArt" );

    // Register the equalizer bands
    for( int i = 0; i < EqualizerBands::kNbBands; i++)
    {
        std::stringstream ss;
        ss << "equalizer.band(" << i << ")";
        pVarManager->registerVar( m_varEqBands.getBand( i ), ss.str() );
    }

    static const struct vlc_playlist_callbacks playlist_cbs = {
    on_playlist_items_reset,
    on_playlist_items_added,
    NULL, // on_playlist_items_moved,
    on_playlist_items_removed,
    on_playlist_items_updated,
    on_playlist_playback_repeat_changed,
    on_playlist_playback_order_changed,
    on_playlist_current_index_changed,
    NULL, // on_playlist_has_prev_changed
    NULL, // on_playlist_has_next_changed
    };

    static const struct vlc_player_cbs player_cbs = {
    on_player_current_media_changed,
    on_player_state_changed,
    NULL, //on_player_error_changed,
    NULL, //on_player_buffering,
    on_player_rate_changed,
    on_player_capabilities_changed,
    on_player_position_changed,
    NULL, //on_player_length_changed,
    NULL, //on_player_track_list_changed,
    on_player_track_selection_changed,
    NULL, //on_player_track_delay_changed,
    NULL, //on_player_program_list_changed,
    NULL, //on_player_program_selection_changed,
    on_player_titles_changed,
    NULL, //on_player_title_selection_changed,
    NULL, //on_player_chapter_selection_changed,
    NULL, //on_player_teletext_menu_changed,
    NULL, //on_player_teletext_enabled_changed,
    NULL, //on_player_teletext_page_changed,
    NULL, //on_player_teletext_transparency_changed,
    NULL, //on_player_category_delay_changed,
    NULL, //on_player_associated_subs_fps_changed,
    NULL, //on_player_renderer_changed,
    on_player_recording_changed,
    NULL, //on_player_signal_changed,
    NULL, //on_player_stats_changed,
    NULL, //on_player_atobloop_changed,
    NULL, //on_player_media_stopped_action_changed,
    NULL, //on_player_media_meta_changed,
    NULL, //on_player_media_epg_changed,
    NULL, //on_player_subitems_changed,
    on_player_vout_changed,
    NULL, //on_player_corks_changed
    NULL, //on_playback_restore_queried
    };

    static const struct vlc_player_vout_cbs player_vout_cbs = {
    NULL, // on_player_vout_fullscreen_changed,
    NULL, // on_player_vout_wallpaper_mode_changed
    };

    static const struct vlc_player_aout_cbs player_aout_cbs = {
    on_player_aout_volume_changed,
    on_player_aout_mute_changed,
    NULL, //on_player_device_changed,
    };

    // Add various listeners
    vlc_playlist_Lock( getPL() );

    mPlaylistListenerId =
        vlc_playlist_AddListener( getPL(), &playlist_cbs, this, true);

    vlc_player_t *player = vlc_playlist_GetPlayer( getPL() );

    mPlayerListenerId =
        vlc_player_AddListener( player, &player_cbs, this );
    mPlayerAoutListenerId =
        vlc_player_aout_AddListener( player, &player_aout_cbs, this );
    mPlayerVoutListenerId =
        vlc_player_vout_AddListener( player, &player_vout_cbs, this );

    vlc_playlist_Unlock( getPL() );

    // XXX WARNING XXX
    // The object variable callbacks are called from other VLC threads,
    // so they must put commands in the queue and NOT do anything else
    // (X11 calls are not reentrant)


    var_AddCallback( vlc_object_instance(getIntf()), "intf-toggle-fscontrol",
                     genericCallback, this );

    // initialize variables refering to libvlc and playlist objects
    init_variables();
}


VlcProc::~VlcProc()
{
    if( m_pVout )
    {
        var_DelCallback( m_pVout, "mouse-moved",
                         genericCallback, this );
        vout_Release( m_pVout );
        m_pVout = NULL;
    }
    if( m_pAout )
    {
        var_DelCallback( m_pAout, "audio-filter", genericCallback, this );
        var_DelCallback( m_pAout, "equalizer-bands",
                         EqBandsCallback, this );
        var_DelCallback( m_pAout, "equalizer-preamp",
                         EqPreampCallback, this );
        aout_Release( m_pAout );
        m_pAout = NULL;
    }

    var_DelCallback( vlc_object_instance(getIntf()), "intf-toggle-fscontrol",
                     genericCallback, this );

    // Remove various listeners
    vlc_playlist_Lock( getPL() );

    vlc_playlist_RemoveListener( getPL(), mPlaylistListenerId );

    vlc_player_t *player = vlc_playlist_GetPlayer( getPL() );
    vlc_player_RemoveListener( player, mPlayerListenerId );
    vlc_player_aout_RemoveListener( player, mPlayerAoutListenerId );
    vlc_player_vout_RemoveListener( player, mPlayerVoutListenerId );

    vlc_playlist_Unlock( getPL() );
}

int VlcProc::genericCallback( vlc_object_t *pObj, const char *pVariable,
                              vlc_value_t oldVal, vlc_value_t newVal,
                              void *pParam )
{
    (void)pObj; (void)oldVal;
    onGenericCallback( pVariable, newVal, pParam );

    return VLC_SUCCESS;
}

int VlcProc::EqBandsCallback( vlc_object_t *pObj, const char *pVariable,
                              vlc_value_t oldVal, vlc_value_t newVal,
                              void *pParam )
{
    (void)pObj; (void)pVariable; (void)oldVal;
    VlcProc *pThis = (VlcProc*)pParam;

    // Post a set equalizer bands command
    CmdSetEqBands *pCmd = new CmdSetEqBands( pThis->getIntf(),
                                             pThis->m_varEqBands,
                                             newVal.psz_string );
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );

    return VLC_SUCCESS;
}

int VlcProc::EqPreampCallback( vlc_object_t *pObj, const char *pVariable,
                               vlc_value_t oldVal, vlc_value_t newVal,
                               void *pParam )
{
    (void)pObj; (void)pVariable; (void)oldVal;
    VlcProc *pThis = (VlcProc*)pParam;
    EqualizerPreamp *pVarPreamp = (EqualizerPreamp*)(pThis->m_cVarEqPreamp.get());

    // Post a set preamp command
    CmdSetEqPreamp *pCmd = new CmdSetEqPreamp( pThis->getIntf(), *pVarPreamp,
                                              (newVal.f_float + 20.0) / 40.0 );
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    pQueue->push( CmdGenericPtr( pCmd ) );

    return VLC_SUCCESS;
}

#define ADD_CALLBACK_ENTRY( var, func, remove ) \
    if( strcmp( pVariable, var ) == 0 ){\
        cb = &VlcProc::func; \
        bRemove = remove; \
    }\
    else

void VlcProc::onGenericCallback( const char *pVariable,
                                 vlc_value_t newVal, void *data )
{
    VlcProc *pThis = (VlcProc*)data;
    AsyncQueue *pQueue = AsyncQueue::instance( pThis->getIntf() );
    std::string label = pVariable;
    void (VlcProc::*cb)(vlc_value_t);
    bool bRemove;

    ADD_CALLBACK_ENTRY( "current_media", on_current_media_changed, false )
    ADD_CALLBACK_ENTRY( "repeat", on_repeat_changed, false )
    ADD_CALLBACK_ENTRY( "order", on_order_changed, false )
    ADD_CALLBACK_ENTRY( "volume", on_volume_changed, true )
    ADD_CALLBACK_ENTRY( "mute", on_mute_changed, false )
    ADD_CALLBACK_ENTRY( "random", on_random_changed, false )

    ADD_CALLBACK_ENTRY( "state", on_state_changed, false )
    ADD_CALLBACK_ENTRY( "rate", on_rate_changed, true )
    ADD_CALLBACK_ENTRY( "capabilities", on_capabilities_changed, true )
    ADD_CALLBACK_ENTRY( "position", on_position_changed, true )
    ADD_CALLBACK_ENTRY( "audio_es", on_audio_es_changed, false )
    ADD_CALLBACK_ENTRY( "bit_rate", on_bit_rate_changed, false )
    ADD_CALLBACK_ENTRY( "sample_rate", on_sample_rate_changed, false )
    ADD_CALLBACK_ENTRY( "isDvd", on_isDvd_changed, false )
    ADD_CALLBACK_ENTRY( "recording", on_recording_changed, true )
    ADD_CALLBACK_ENTRY( "vout", on_vout_changed, false )

    ADD_CALLBACK_ENTRY( "intf-toggle-fscontrol", on_intf_show_changed, false )

    ADD_CALLBACK_ENTRY( "mouse-moved", on_mouse_moved_changed,false )
    ADD_CALLBACK_ENTRY( "audio-filter", on_audio_filter_changed, false )
        vlc_assert_unreachable();

    CmdGeneric *pCmd = new CmdCallback( pThis->getIntf(), newVal, cb, label );
    if( pCmd )
        pQueue->push( CmdGenericPtr( pCmd ), bRemove );
}

#undef ADD_CALLBACK_ENTRY

void VlcProc::on_bit_rate_changed( vlc_value_t newVal )
{
    int bitrate = newVal.i_int / 1000;
    if( bitrate != 0 )
        SET_TEXT( m_cVarStreamBitRate, UString::fromInt( getIntf(), bitrate ) );
    else
        SET_TEXT( m_cVarStreamBitRate, UString( getIntf(), "") );
}

void VlcProc::on_sample_rate_changed( vlc_value_t newVal )
{
    int sampleRate = newVal.i_int / 1000;
    if( sampleRate != 0 )
    SET_TEXT( m_cVarStreamSampleRate, UString::fromInt(getIntf(),sampleRate) );
    else
    SET_TEXT( m_cVarStreamSampleRate, UString( getIntf(), "") );
}

void VlcProc::on_random_changed( vlc_value_t newVal )
{
    SET_BOOL( m_cVarRandom, newVal.b_bool );
}

void VlcProc::on_loop_changed( vlc_value_t newVal )
{
    SET_BOOL( m_cVarLoop, newVal.b_bool );
}

void VlcProc::on_current_media_changed( vlc_value_t newVal )
{
    input_item_t* pItem = static_cast<input_item_t*>(newVal.p_address);
    msg_Dbg(getIntf(),"current media changed %p", pItem );
    if( pItem )
    {
        // Update short name (as defined by --input-title-format)
        char *psz_name = NULL;
        char *psz_fmt = var_InheritString( getIntf(), "input-title-format" );
        if( psz_fmt != NULL )
        {
            vlc_playlist_Lock( getPL() );
            vlc_player_t* player = vlc_playlist_GetPlayer( getPL() );
            psz_name = vlc_strfplayer( player, NULL, psz_fmt );
            vlc_playlist_Unlock( getPL() );
            free( psz_fmt );
        }

        SET_TEXT( m_cVarStreamName, UString( getIntf(),
                                             psz_name ? psz_name : "" ) );
        free( psz_name );

        // Update local path (if possible) or full uri
        char *psz_uri = input_item_GetURI( pItem );
        char *psz_path = vlc_uri2path( psz_uri );
        char *psz_save = psz_path ? psz_path : psz_uri;
        SET_TEXT( m_cVarStreamURI, UString( getIntf(), psz_save ) );
        free( psz_path );
        free( psz_uri );

        // Update art uri
        char *psz_art = input_item_GetArtURL( pItem );
        SET_STRING( m_cVarStreamArt, std::string( psz_art ? psz_art : "" ) );
        free( psz_art );

        input_item_Release( pItem );
    }
}

void VlcProc::on_repeat_changed( vlc_value_t newVal )
{
    enum vlc_playlist_playback_repeat repeat =
        (enum vlc_playlist_playback_repeat)newVal.i_int;

    if( repeat == VLC_PLAYLIST_PLAYBACK_REPEAT_ALL )
    {
        SET_BOOL( m_cVarRepeat, false );
        SET_BOOL( m_cVarLoop, true );
    }
    else if( repeat == VLC_PLAYLIST_PLAYBACK_REPEAT_CURRENT )
    {
        SET_BOOL( m_cVarLoop, false );
        SET_BOOL( m_cVarRepeat, true );
    }
    else
    {
        SET_BOOL( m_cVarRepeat, false );
        SET_BOOL( m_cVarLoop, false );
    }
}

void VlcProc::on_order_changed( vlc_value_t newVal )
{
    enum vlc_playlist_playback_order order =
        (enum vlc_playlist_playback_order)newVal.i_int;

    SET_BOOL( m_cVarRandom, (order == VLC_PLAYLIST_PLAYBACK_ORDER_RANDOM) );
}

void VlcProc::on_volume_changed( vlc_value_t newVal )
{
    SET_VOLUME( m_cVarVolume, newVal.f_float, false );
}

void VlcProc::on_mute_changed( vlc_value_t newVal )
{
    SET_BOOL( m_cVarMute, newVal.b_bool );
}

void VlcProc::on_recording_changed( vlc_value_t newVal )
{
    SET_BOOL( m_cVarRecording, newVal.b_bool );
}

void VlcProc::on_state_changed( vlc_value_t newVal )
{
    enum vlc_player_state state = (enum vlc_player_state)newVal.i_int;
    msg_Dbg( getIntf(),"playlist state changed : %i", state );

    bool stopped = ( state == VLC_PLAYER_STATE_STOPPED );
    bool playing = ( state == VLC_PLAYER_STATE_STARTED ||
                     state == VLC_PLAYER_STATE_PLAYING ||
                     state == VLC_PLAYER_STATE_STOPPING );
    bool paused = ( state == VLC_PLAYER_STATE_PAUSED );

    SET_BOOL( m_cVarStopped, stopped );
    SET_BOOL( m_cVarPlaying, playing );
    SET_BOOL( m_cVarPaused, paused );

    // extra cleaning done
    if( state == VLC_PLAYER_STATE_STOPPED )
        reset_input();
}

void VlcProc::on_rate_changed( vlc_value_t newVal )
{
    float rate =  newVal.f_float;
    char* buffer;
    if( asprintf( &buffer, "%.3g", rate ) != -1 )
    {
        SET_TEXT( m_cVarSpeed, UString( getIntf(), buffer ) );
        free( buffer );
    }
}

void VlcProc::on_capabilities_changed( vlc_value_t newVal )
{
    int capabilities = newVal.i_int;
    SET_BOOL( m_cVarSeekable, capabilities & VLC_PLAYER_CAP_SEEK );
}

void VlcProc::on_position_changed( vlc_value_t newVal )
{
    float pos = newVal.f_float;
    SET_STREAMTIME( m_cVarTime, pos, false );
}

void VlcProc::on_audio_es_changed( vlc_value_t newVal )
{
    SET_BOOL( m_cVarHasAudio, newVal.b_bool );
}

void VlcProc::on_isDvd_changed( vlc_value_t newVal )
{
    SET_BOOL( m_cVarDvdActive, newVal.b_bool );
}

void VlcProc::on_vout_changed( vlc_value_t newVal )
{
    vout_thread_t* pVout = (vout_thread_t*)newVal.p_address;
    SET_BOOL( m_cVarHasVout, pVout != NULL );
    if( !pVout || pVout == m_pVout )
    {
        if( pVout )
            vout_Release( pVout );
        return;
    }

    // release previous Vout
    if( m_pVout )
    {
        var_DelCallback( m_pVout, "mouse-moved",
                         genericCallback, this );
        vout_Release( m_pVout );
        m_pVout = NULL;
    }

    // keep new vout held and install callback
    m_pVout = pVout;
    var_AddCallback( m_pVout, "mouse-moved", genericCallback, this );
}

void VlcProc::on_audio_filter_changed( vlc_value_t newVal )
{
    char *pFilters = newVal.psz_string;
    bool b_equalizer = pFilters && strstr( pFilters, "equalizer" );
    SET_BOOL( m_cVarEqualizer, b_equalizer );
}

void VlcProc::on_intf_show_changed( vlc_value_t newVal )
{
    bool b_fullscreen = getFullscreenVar().get();

    if( !b_fullscreen )
    {
        if( newVal.b_bool )
        {
            // Create a raise all command
            CmdRaiseAll *pCmd = new CmdRaiseAll( getIntf(),
                getIntf()->p_sys->p_theme->getWindowManager() );

            // Push the command in the asynchronous command queue
            AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );
            pQueue->push( CmdGenericPtr( pCmd ) );
        }
    }
    else
    {
        VoutManager* pVoutManager =  VoutManager::instance( getIntf() );
        FscWindow *pWin = pVoutManager->getFscWindow();
        if( pWin )
        {
            bool b_visible = pWin->getVisibleVar().get();
            AsyncQueue *pQueue = AsyncQueue::instance( getIntf() );

            if( !b_visible )
            {
               CmdShowWindow* pCmd = new CmdShowWindow( getIntf(),
                             getIntf()->p_sys->p_theme->getWindowManager(),
                             *pWin );
               pQueue->push( CmdGenericPtr( pCmd ) );
            }
            else
            {
               CmdHideWindow* pCmd = new CmdHideWindow( getIntf(),
                              getIntf()->p_sys->p_theme->getWindowManager(),
                              *pWin );
               pQueue->push( CmdGenericPtr( pCmd ) );
            }
        }
    }
}

void VlcProc::on_mouse_moved_changed( vlc_value_t newVal )
{
    (void)newVal;
    FscWindow* pFscWindow = VoutManager::instance( getIntf() )->getFscWindow();
    if( pFscWindow )
        pFscWindow->onMouseMoved();
}

void VlcProc::reset_input()
{
    SET_BOOL( m_cVarSeekable, false );
    SET_BOOL( m_cVarRecordable, true );
    SET_BOOL( m_cVarRecording, false );
    SET_BOOL( m_cVarDvdActive, false );
    SET_BOOL( m_cVarHasAudio, false );
    SET_BOOL( m_cVarHasVout, false );

    SET_STREAMTIME( m_cVarTime, 0, false );
    SET_TEXT( m_cVarStreamName, UString( getIntf(), "") );
    SET_TEXT( m_cVarStreamURI, UString( getIntf(), "") );
    SET_STRING( m_cVarStreamArt, std::string( "" ) );
    SET_TEXT( m_cVarStreamBitRate, UString( getIntf(), "") );
    SET_TEXT( m_cVarStreamSampleRate, UString( getIntf(), "") );
}

void VlcProc::init_variables()
{
    vlc_player_t *player = vlc_playlist_GetPlayer( getPL() );

    float volume = vlc_player_aout_GetVolume( player );
    SET_VOLUME( m_cVarVolume, volume, false );

    bool mute = vlc_player_aout_IsMuted( player ) == 1;
    SET_BOOL( m_cVarMute, mute );

    SET_BOOL( m_cVarStopped, true );
    init_equalizer();
}

void VlcProc::init_equalizer()
{
    //audio_output_t* pAout = playlist_GetAout( getPL() );
    vlc_player_t* player = vlc_playlist_GetPlayer( getPL() );
    audio_output_t* pAout = vlc_player_aout_Hold( player );
    if( pAout )
    {
        if( !var_Type( pAout, "equalizer-bands" ) )
            var_Create( pAout, "equalizer-bands",
                        VLC_VAR_STRING | VLC_VAR_DOINHERIT);
        if( !var_Type( pAout, "equalizer-preamp" ) )
            var_Create( pAout, "equalizer-preamp",
                        VLC_VAR_FLOAT | VLC_VAR_DOINHERIT);

        // New Aout (addCallbacks)
        var_AddCallback( pAout, "audio-filter", genericCallback, this );
        var_AddCallback( pAout, "equalizer-bands",
                         EqBandsCallback, this );
        var_AddCallback( pAout, "equalizer-preamp",
                         EqPreampCallback, this );

        assert( !m_pAout );
        m_pAout = pAout;
    }

    // is equalizer enabled ?
    char *pFilters = pAout ?
                   var_GetNonEmptyString( pAout, "audio-filter" ) :
                   var_InheritString( getIntf(), "audio-filter" );
    bool b_equalizer = pFilters && strstr( pFilters, "equalizer" );
    free( pFilters );
    SET_BOOL( m_cVarEqualizer, b_equalizer );

    // retrieve initial bands
    char* bands = pAout ?
                  var_GetString( pAout, "equalizer-bands" ) :
                  var_InheritString( getIntf(), "equalizer-bands" );
    if( bands )
    {
        m_varEqBands.set( bands );
        free( bands );
    }

    // retrieve initial preamp
    float preamp = pAout ?
                   var_GetFloat( pAout, "equalizer-preamp" ) :
                   var_InheritFloat( getIntf(), "equalizer-preamp" );
    EqualizerPreamp *pVarPreamp = (EqualizerPreamp*)m_cVarEqPreamp.get();
    pVarPreamp->set( (preamp + 20.0) / 40.0 );
}

void VlcProc::setFullscreenVar( bool b_fullscreen )
{
    SET_BOOL( m_cVarFullscreen, b_fullscreen );
}

#undef  SET_BOOL
#undef  SET_STREAMTIME
#undef  SET_TEXT
#undef  SET_STRING
#undef  SET_VOLUME
