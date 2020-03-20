/*****************************************************************************
 * Copyright (C) 2019 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "player_controller.hpp"
#include "player_controller_p.hpp"
#include "util/recents.hpp"

#include <vlc_actions.h>           /* ACTION_ID */
#include <vlc_url.h>            /* vlc_uri_decode */
#include <vlc_strings.h>        /* vlc_strfplayer */
#include <vlc_aout.h>           /* audio_output_t */
#include <vlc_es.h>
#include <vlc_cxx_helpers.hpp>
#include <vlc_vout.h>

#include <QApplication>
#include <QFile>
#include <QDir>
#include <QSignalMapper>
#include <QMessageBox>

#include <assert.h>

#define POSITION_MIN_UPDATE_INTERVAL VLC_TICK_FROM_MS(15)

//PlayerController private implementation

using InputItemPtr = vlc_shared_data_ptr_type(input_item_t,
                                              input_item_Hold,
                                              input_item_Release);

using EsIdPtr = vlc_shared_data_ptr_type(vlc_es_id_t,
                                         vlc_es_id_Hold,
                                         vlc_es_id_Release);

using TitleListPtr = vlc_shared_data_ptr_type(vlc_player_title_list,
                                              vlc_player_title_list_Hold,
                                              vlc_player_title_list_Release);

PlayerControllerPrivate::~PlayerControllerPrivate()
{
    vlc_player_locker locker{m_player}; //this also locks the player
    vlc_player_vout_RemoveListener( m_player, m_player_vout_listener );
    vlc_player_aout_RemoveListener( m_player, m_player_aout_listener );
    vlc_player_RemoveListener( m_player, m_player_listener );
    vlc_player_RemoveTimer( m_player, m_player_timer );
}

bool PlayerController::isCurrentItemSynced()
{
    Q_D(PlayerController);

    /* The media can change before the UI gets updated and thus a player
     * request can be submitted on the wrong media or worse, no media at
     * all. Here d->m_currentItem is read and modified under the player
     * lock. */

    return d->m_currentItem.get() == vlc_player_GetCurrentMedia( d->m_player );
}


void PlayerControllerPrivate::UpdateName(input_item_t* media)
{
    Q_Q(PlayerController);
    /* Update text, name and nowplaying */
    QString name;
    if (! media)
        return;

    /* Try to get the nowplaying */
    char *format = var_InheritString( p_intf, "input-title-format" );
    char *formatted = NULL;
    if (format != NULL)
    {
        vlc_player_Lock( m_player );
        formatted = vlc_strfplayer( m_player, media, format );
        vlc_player_Unlock( m_player );
        free( format );
        if( formatted != NULL )
        {
            name = qfu(formatted);
            free( formatted );
        }
    }

    /* If we have Nothing */
    if( name.simplified().isEmpty() )
    {
        char *uri = input_item_GetURI( media );
        char *file = uri ? strrchr( uri, '/' ) : NULL;
        if( file != NULL )
        {
            vlc_uri_decode( ++file );
            name = qfu(file);
        }
        else
            name = qfu(uri);
        free( uri );
    }

    name = name.trimmed();

    if( m_name != name )
    {
        emit q->nameChanged( name );
        m_name = name;
    }
}


void PlayerControllerPrivate::UpdateArt(input_item_t *p_item)
{
    Q_Q(PlayerController);
    if (! p_item)
        return;

    QString url = PlayerController::decodeArtURL( p_item );

    /* the art hasn't changed, no need to update */
    if(m_artUrl == url)
        return;

    /* Update Art meta */
    m_artUrl = url;
    emit q->artChanged( m_artUrl );
}

void PlayerControllerPrivate::UpdateStats( const input_stats_t& stats )
{
    Q_Q(PlayerController);
    emit q->statisticsUpdated( stats );
}

void PlayerControllerPrivate::UpdateProgram(enum vlc_player_list_action action, const struct vlc_player_program* prgm)
{
    Q_Q(PlayerController);
    m_programList.updatePrograms( action, prgm );
    emit q->isEncryptedChanged( prgm->scrambled );
}

void PlayerControllerPrivate::UpdateTrackSelection(vlc_es_id_t *trackid, bool selected)
{
    if (trackid == NULL)
        return;
    es_format_category_e cat = vlc_es_id_GetCat(trackid);
    TrackListModel* tracklist;
    switch (cat) {
    case VIDEO_ES: tracklist = &m_videoTracks; break;
    case AUDIO_ES: tracklist = &m_audioTracks; break;
    case SPU_ES: tracklist = &m_subtitleTracks; break;
    default: return;
    }
    tracklist->updateTrackSelection(trackid, selected);
}

void PlayerControllerPrivate::UpdateMeta( input_item_t *p_item )
{
    Q_Q(PlayerController);
    emit q->currentMetaChanged( p_item  );
}

void PlayerControllerPrivate::UpdateInfo( input_item_t *p_item )
{
    Q_Q(PlayerController);
    emit q->infoChanged( p_item );
}

void PlayerControllerPrivate::UpdateVouts(vout_thread_t **vouts, size_t i_vouts)
{
    Q_Q(PlayerController);
    bool hadVideo = m_hasVideo;
    m_hasVideo = i_vouts > 0;

    vout_thread_t* main_vout = nullptr;
    if (m_hasVideo)
        main_vout = vouts[0];

    m_zoom.resetObject( main_vout );
    m_aspectRatio.resetObject( main_vout );
    m_crop.resetObject(  main_vout );
    m_deinterlace.resetObject( main_vout );
    m_deinterlaceMode.resetObject( main_vout );
    m_autoscale.resetObject( main_vout );

    emit q->voutListChanged(vouts, i_vouts);
    if( hadVideo != m_hasVideo )
        emit q->hasVideoOutputChanged(m_hasVideo);
}

void PlayerControllerPrivate::UpdateSpuOrder(vlc_es_id_t *es_id, enum vlc_vout_order order)
{
    switch (order)
    {
        case VLC_VOUT_ORDER_NONE:
            if (es_id == m_secondarySpuEsId.get())
                m_secondarySpuEsId.reset(NULL, false);
            break;
        case VLC_VOUT_ORDER_SECONDARY:
            m_secondarySpuEsId.reset(es_id, true);
            if (m_secondarySubtitleDelay != 0)
            {
                vlc_player_locker lock{ m_player };
                vlc_player_SetEsIdDelay(m_player, es_id,
                                        m_secondarySubtitleDelay,
                                        VLC_PLAYER_WHENCE_ABSOLUTE);
            }
            break;
        default:
            break;
    }
}

int PlayerControllerPrivate::interpolateTime(vlc_tick_t system_now)
{
    vlc_tick_t new_time;
    if (vlc_player_timer_point_Interpolate(&m_player_time, system_now,
                                           &new_time, &m_position) == VLC_SUCCESS)
    {
        m_time = new_time != VLC_TICK_INVALID ? new_time - VLC_TICK_0 : 0;
        return VLC_SUCCESS;
    }
    return VLC_EGENERIC;
}

extern "C" {

//player callbacks

static  void on_player_current_media_changed(vlc_player_t *, input_item_t *new_media, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_current_media_changed");

    if (!new_media)
    {
        that->callAsync([that] () {
            emit that->q_func()->inputChanged(false);
            vlc_player_locker lock{ that->m_player };
            that->m_currentItem.reset(nullptr);
        });
        return;
    }

    InputItemPtr newMediaPtr = InputItemPtr( new_media );
    that->callAsync([that,newMediaPtr] () {
        PlayerController* q = that->q_func();
        that->UpdateName( newMediaPtr.get() );
        that->UpdateArt( newMediaPtr.get() );
        that->UpdateMeta( newMediaPtr.get() );

        RecentsMRL::getInstance( that->p_intf )->addRecent( newMediaPtr.get()->psz_uri );

        {
            vlc_player_locker lock{ that->m_player };
            that->m_currentItem = std::move(newMediaPtr);
        }

        that->m_canRestorePlayback = false;
        emit q->playbackRestoreQueried();

        emit q->inputChanged(true);
    });
}

static void on_player_state_changed(vlc_player_t *, enum vlc_player_state state, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_state_changed");

    that->callAsync([that,state] () {
        PlayerController* q = that->q_func();
        that->m_playing_status = static_cast<PlayerController::PlayingState>(state);
        switch ( state ) {
        case VLC_PLAYER_STATE_STARTED:
            msg_Dbg( that->p_intf, "on_player_state_changed VLC_PLAYER_STATE_STARTED");
            break;
        case VLC_PLAYER_STATE_PLAYING:
        {
            msg_Dbg( that->p_intf, "on_player_state_changed VLC_PLAYER_STATE_PLAYING");
            PlayerController::AoutPtr aout = q->getAout();
            that->m_audioStereoMode.resetObject( aout.get() );
            that->m_audioVisualization.resetObject( aout.get() );
            break;
        }
        case VLC_PLAYER_STATE_PAUSED:
            msg_Dbg( that->p_intf, "on_player_state_changed VLC_PLAYER_STATE_PAUSED");
            break;
        case VLC_PLAYER_STATE_STOPPING:
            msg_Dbg( that->p_intf, "on_player_state_changed VLC_PLAYER_STATE_STOPPING");
            break;
        case VLC_PLAYER_STATE_STOPPED:
        {
            msg_Dbg( that->p_intf, "on_player_state_changed VLC_PLAYER_STATE_STOPPED");

            that->m_audioStereoMode.resetObject((audio_output_t*)nullptr);
            that->m_audioVisualization.resetObject((audio_output_t*)nullptr);

            /* reset the state on stop */
            that->m_position = 0;
            that->m_time = 0;
            that->m_length = 0;
            emit q->positionUpdated( -1.0, 0 ,0 );
            that->m_rate = 1.0f;
            emit q->rateChanged( 1.0f );
            that->m_name = "";
            emit q->nameChanged( "" );
            that->m_hasChapters = false;
            emit q->hasChaptersChanged( false );
            that->m_hasTitles = false;
            emit q->hasTitlesChanged( false );
            that->m_hasMenu= false;
            emit q->hasMenuChanged( false );
            that->m_isMenu= false;
            emit q->isMenuChanged( false );
            that->m_isInteractive= false;
            emit q->isInteractiveChanged( false );
            that->m_canRestorePlayback = false;
            emit q->playbackRestoreQueried();

            that->m_teletextAvailable = false;
            emit q->teletextAvailableChanged( false );
            that->m_ABLoopState = PlayerController::ABLOOP_STATE_NONE;
            that->m_ABLoopA = VLC_TICK_INVALID;
            that->m_ABLoopB = VLC_TICK_INVALID;
            emit q->ABLoopStateChanged(PlayerController::ABLOOP_STATE_NONE);
            emit q->ABLoopAChanged(VLC_TICK_INVALID);
            emit q->ABLoopBChanged(VLC_TICK_INVALID);
            that->m_hasVideo = false;
            emit q->hasVideoOutputChanged( false );

            emit q->voutListChanged( NULL, 0 );

            /* Reset all InfoPanels but stats */
            that->m_artUrl = "";
            emit q->artChanged( NULL );
            emit q->artChanged( "" );
            emit q->infoChanged( NULL );
            emit q->currentMetaChanged( (input_item_t *)NULL );

            that->m_encrypted =false;
            emit q->isEncryptedChanged( false );
            that->m_recording =false;
            emit q->recordingChanged( false );

            that->m_buffering = 0.f;
            emit q->bufferingChanged( 0.0 );

            break;
        }
        }
        emit q->playingStateChanged( that->m_playing_status );
    });
}

void on_player_error_changed(vlc_player_t *, enum vlc_player_error , void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_error_changed");
}

static void on_player_buffering(vlc_player_t *, float new_buffering, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_buffering");
    that->callAsync([that,new_buffering](){
        that->m_buffering = new_buffering;
        emit that->q_func()->bufferingChanged( new_buffering );
    });
}

static void on_player_capabilities_changed(vlc_player_t *, int old_caps, int new_caps, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_capabilities_changed");
    that->callAsync([that, old_caps, new_caps]() {
        PlayerController* q = that->q_func();
        that->m_capabilities = new_caps;

        bool oldSeekable = old_caps & VLC_PLAYER_CAP_SEEK;
        bool newSeekable = new_caps & VLC_PLAYER_CAP_SEEK;
        if (newSeekable != oldSeekable)
            emit q->seekableChanged( newSeekable );

        bool oldRewindable = old_caps & VLC_PLAYER_CAP_REWIND;
        bool newRewindable = new_caps & VLC_PLAYER_CAP_REWIND;
        if (newRewindable != oldRewindable)
            emit q->rewindableChanged( newRewindable );

        bool oldPauseable = old_caps & VLC_PLAYER_CAP_PAUSE;
        bool newPauseable = new_caps & VLC_PLAYER_CAP_PAUSE;
        if (newPauseable != oldPauseable)
            emit q->pausableChanged( newPauseable );

        bool oldChangeRate = old_caps & VLC_PLAYER_CAP_CHANGE_RATE;
        bool newChangeRate = new_caps & VLC_PLAYER_CAP_CHANGE_RATE;
        if (newChangeRate != oldChangeRate)
            emit q->rateChangableChanged( newChangeRate );
    });

    //FIXME other events?
}

static void on_player_track_list_changed(vlc_player_t *, enum vlc_player_list_action action, const struct vlc_player_track *track, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    struct vlc_player_track* dup = vlc_player_track_Dup(track);
    if (!dup)
        return;
    std::shared_ptr<struct vlc_player_track> trackPtr(dup, vlc_player_track_Delete);

    msg_Dbg( that->p_intf, "on_player_track_list_changed");
    that->callAsync([that,action,trackPtr] () {
        switch (trackPtr.get()->fmt.i_cat) {
        case VIDEO_ES:
            msg_Dbg( that->p_intf, "on_player_track_list_changed (video)");
            that->m_videoTracks.updateTracks( action, trackPtr.get() );
            break;
        case AUDIO_ES:
            msg_Dbg( that->p_intf, "on_player_track_list_changed (audio)");
            that->m_audioTracks.updateTracks( action, trackPtr.get() );
            break;
        case SPU_ES:
            msg_Dbg( that->p_intf, "on_player_track_list_changed (spu)");
            that->m_subtitleTracks.updateTracks( action, trackPtr.get() );
            break;
        default:
            //we don't handle other kind of tracks
            msg_Dbg( that->p_intf, "on_player_track_list_changed (other)");
            break;
        }
    });
}

static void on_player_track_selection_changed(vlc_player_t *, vlc_es_id_t * unselected, vlc_es_id_t *selected, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_track_selection_changed");

    EsIdPtr unselectedPtr = EsIdPtr(unselected);
    EsIdPtr selectedPtr = EsIdPtr(selected);

    that->callAsync([that,unselectedPtr,selectedPtr] () {
        if (unselectedPtr)
            that->UpdateTrackSelection( unselectedPtr.get(), false );
        if (selectedPtr)
            that->UpdateTrackSelection( selectedPtr.get(), true );
    });
}


static void on_player_program_list_changed(vlc_player_t *, enum vlc_player_list_action action, const struct vlc_player_program *prgm, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_program_list_changed");
    struct vlc_player_program* dup = vlc_player_program_Dup(prgm);
    if (!dup)
        return;
    std::shared_ptr<struct vlc_player_program> prgmPtr(dup, vlc_player_program_Delete);

    that->callAsync([that,action,prgmPtr] (){
        that->UpdateProgram(action, prgmPtr.get());
    });
}

static void on_player_program_selection_changed(vlc_player_t *, int unselected, int selected, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_program_selection_changed");

    that->callAsync([that,unselected,selected] (){
        that->m_programList.updateProgramSelection(unselected, false);
        that->m_programList.updateProgramSelection(selected, true);
    });
}

static void on_player_titles_changed(vlc_player_t *, struct vlc_player_title_list *titles, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_title_array_changed");

    TitleListPtr titleListPtr = TitleListPtr(titles);

    that->callAsync([that,titleListPtr] (){
        struct vlc_player_title_list *titles = titleListPtr.get();
        that->m_chapterList.resetTitle(nullptr);
        that->m_titleList.resetTitles(titles);

        bool hasMenu = false;
        bool hasTitles = false;
        if (titles)
        {
            size_t nbTitles = vlc_player_title_list_GetCount(titles);
            for( size_t i = 0; i < nbTitles; i++)
            {
                const vlc_player_title* title = vlc_player_title_list_GetAt(titles, i);
                if( (title ->flags & INPUT_TITLE_MENU) != 0 )
                {
                    hasMenu = true;
                    break;
                }
            }
            hasTitles = nbTitles != 0;
        }
        if (hasTitles != that->m_hasTitles)
        {
            that->m_hasTitles = hasTitles;
            emit that->q_func()->hasTitlesChanged(hasTitles);
        }
        if (!hasTitles && that->m_hasChapters)
        {
            that->m_hasChapters = false;
            emit that->q_func()->hasChaptersChanged(false);
        }
        if (hasMenu != that->m_hasMenu)
        {
            that->m_hasMenu = hasMenu;
            emit that->q_func()->hasMenuChanged(hasMenu);
        }
    });
}

static void on_player_title_selection_changed(vlc_player_t *,
                                              const struct vlc_player_title *new_title, size_t new_idx, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_title_selection_changed");

    bool hasChapter = (new_title != nullptr && new_title->chapter_count != 0);
    that->callAsync([that,new_title,new_idx,hasChapter] (){
        PlayerController* q = that->q_func();
        that->m_chapterList.resetTitle(new_title);
        that->m_titleList.setCurrent(new_idx);
        that->m_hasChapters  = hasChapter;
        that->m_isInteractive = new_title && (new_title->flags & VLC_PLAYER_TITLE_INTERACTIVE);
        that->m_isMenu = new_title && (new_title->flags & VLC_PLAYER_TITLE_MENU);
        emit q->isMenuChanged( that->m_isMenu );
        emit q->isInteractiveChanged( that->m_isInteractive );
        emit q->hasChaptersChanged( hasChapter );
    });
}

static void on_player_chapter_selection_changed(vlc_player_t *,
                                                const struct vlc_player_title *, size_t,
                                                const struct vlc_player_chapter *, size_t chapter_idx,
                                                void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_chapter_selection_changed");
    that->callAsync([that,chapter_idx] (){
        that->m_chapterList.setCurrent(chapter_idx);
    });
}


static void on_player_teletext_menu_changed(vlc_player_t *, bool has_teletext_menu, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_teletext_menu_changed, %s", has_teletext_menu ? "available" : "unavailable" );
    that->callAsync([that,has_teletext_menu] () {
        that->m_teletextAvailable = has_teletext_menu;
        emit that->q_func()->teletextAvailableChanged(has_teletext_menu);
    });
}

static void on_player_teletext_enabled_changed(vlc_player_t *, bool enabled, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_teletext_enabled_changed %s", enabled ? "enabled" : "disabled");
    that->callAsync([that,enabled] () {
        that->m_teletextEnabled = enabled;
        emit that->q_func()->teletextEnabledChanged(enabled);
    });
}

static void on_player_teletext_page_changed(vlc_player_t *, unsigned new_page, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_teletext_page_changed %u", new_page);
    that->callAsync([that,new_page] () {
        that->m_teletextPage = new_page;
        emit that->q_func()->teletextPageChanged(new_page);
    });
}

static void on_player_teletext_transparency_changed(vlc_player_t *, bool enabled, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_teletext_transparency_changed %s", enabled ? "enabled" : "disabled");
    that->callAsync([that,enabled] () {
        that->m_teletextTransparent = enabled;
        emit that->q_func()->teletextTransparencyChanged(enabled);
    });
}

static void on_player_category_delay_changed(vlc_player_t *,
                               enum es_format_category_e cat, vlc_tick_t new_delay,
                               void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_category_delay_changed: %d", cat );
    that->callAsync([that,cat,new_delay] (){
        switch (cat)
        {
            case AUDIO_ES:
                that->m_audioDelay = new_delay;
                emit that->q_func()->audioDelayChanged( new_delay );
                break;
            case SPU_ES:
                that->m_subtitleDelay = new_delay;
                emit that->q_func()->subtitleDelayChanged( new_delay );
                break;
            default: vlc_assert_unreachable();
        }
    });
}

static void on_player_track_delay_changed(vlc_player_t *,
                                vlc_es_id_t *es_id, vlc_tick_t new_delay,
                                void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    EsIdPtr esIdPtr = EsIdPtr(es_id);

    that->callAsync([that,esIdPtr,new_delay] (){
        if (that->m_secondarySpuEsId == esIdPtr)
        {
            that->m_secondarySubtitleDelay = new_delay;
            emit that->q_func()->secondarySubtitleDelayChanged( new_delay );
        }
    });
}

static void on_player_associated_subs_fps_changed(vlc_player_t *, float subs_fps, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_associated_subs_fps_changed");
    that->callAsync([that,subs_fps] (){
        that->m_subtitleFPS = subs_fps;
        emit that->q_func()->subtitleFPSChanged( subs_fps );
    });
}

void on_player_renderer_changed(vlc_player_t *, vlc_renderer_item_t *new_item, void *data)
{
    VLC_UNUSED(new_item);
    VLC_UNUSED(data);
}


static void on_player_record_changed(vlc_player_t *, bool recording, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_record_changed");
    that->callAsync([that,recording] (){
        that->m_recording = recording;
        emit that->q_func()->recordingChanged( recording );
    });
}

static void on_player_signal_changed(vlc_player_t *, float quality, float strength, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    VLC_UNUSED(quality);
    VLC_UNUSED(strength);
    msg_Dbg( that->p_intf, "on_player_signal_changed");
}

static void on_player_stats_changed(vlc_player_t *, const struct input_stats_t *stats, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    struct input_stats_t stats_tmp = *stats;
    that->callAsync([that,stats_tmp] () {
        that->m_stats = stats_tmp;
        emit that->q_func()->statisticsUpdated( that->m_stats );
    });
}

static void on_player_atobloop_changed(vlc_player_t *, enum vlc_player_abloop state, vlc_tick_t time, float, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_atobloop_changed");
    that->callAsync([that,state,time] (){
        PlayerController* q = that->q_func();
        switch (state) {
        case VLC_PLAYER_ABLOOP_NONE:
            that->m_ABLoopA = VLC_TICK_INVALID;
            that->m_ABLoopB = VLC_TICK_INVALID;
            emit q->ABLoopAChanged(that->m_ABLoopA);
            emit q->ABLoopBChanged(that->m_ABLoopB);
            break;
        case VLC_PLAYER_ABLOOP_A:
            that->m_ABLoopA = time;
            emit q->ABLoopAChanged(that->m_ABLoopA);
            break;
        case VLC_PLAYER_ABLOOP_B:
            that->m_ABLoopB = time;
            emit q->ABLoopBChanged(that->m_ABLoopB);
            break;
        }
        that->m_ABLoopState = static_cast<PlayerController::ABLoopState>(state);
        emit q->ABLoopStateChanged(that->m_ABLoopState);
    });
}

static void on_player_media_stopped_action_changed(vlc_player_t *, enum vlc_player_media_stopped_action new_action, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    that->callAsync([that,new_action] () {
        that->m_mediaStopAction = static_cast<PlayerController::MediaStopAction>(new_action);
        emit that->q_func()->mediaStopActionChanged(that->m_mediaStopAction);
    });
}

static void on_player_media_meta_changed(vlc_player_t *, input_item_t *media, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_item_meta_changed");

    InputItemPtr mediaPtr(media);
    //call on object thread
    that->callAsync([that,mediaPtr] () {
        that->UpdateName(mediaPtr.get());
        that->UpdateArt(mediaPtr.get());
        that->UpdateMeta(mediaPtr.get());
    });
}

static void on_player_media_epg_changed(vlc_player_t *, input_item_t *, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_item_epg_changed");
    that->callAsync([that] () {
        emit that->q_func()->epgChanged();
    });
}

static void on_player_subitems_changed(vlc_player_t *, input_item_t *, input_item_node_t *, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_subitems_changed");
}


static void on_player_vout_changed(vlc_player_t *player, enum vlc_player_vout_action,
    vout_thread_t *, enum vlc_vout_order order, vlc_es_id_t *es_id, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_vout_list_changed");

    switch (vlc_es_id_GetCat(es_id))
    {
        case VIDEO_ES:
        {
            //player is locked within callbacks*
            size_t i_vout = 0;
            vout_thread_t **vouts = vlc_player_vout_HoldAll(player, &i_vout);

            std::shared_ptr<vout_thread_t*> voutsPtr( vouts, [i_vout]( vout_thread_t**vouts ) {
                for (size_t i = 0; i < i_vout; i++)
                    vout_Release( vouts[i] );
                free(vouts);
            });

            //call on object thread
            that->callAsync([that,voutsPtr,i_vout] () {
                that->UpdateVouts(voutsPtr.get(), i_vout);
            });
            break;
        }
        case SPU_ES:
        {
            EsIdPtr esIdPtr = EsIdPtr(es_id);
            that->callAsync([that,esIdPtr,order] () {
                that->UpdateSpuOrder(esIdPtr.get(), order);
            });
            break;
        }
        default:
            break;
    }
}

//player vout callbacks

static void on_player_vout_fullscreen_changed(vout_thread_t* vout, bool is_fullscreen, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_vout_fullscreen_changed %s", is_fullscreen ? "fullscreen" : "windowed");

    PlayerController::VoutPtr voutPtr = PlayerController::VoutPtr(vout);
    that->callAsync([that,voutPtr,is_fullscreen] () {
        PlayerController* q = that->q_func();
        const PlayerController::VoutPtrList voutList = q->getVouts();
        if (voutPtr == nullptr //property sets for all vout
            || (voutList.size() == 1 && voutPtr.get() == voutList[0].get()) ) //on the only vout
        {
            that->m_fullscreen = is_fullscreen;
            emit q->fullscreenChanged(is_fullscreen);
        }
    });
}

static void on_player_vout_wallpaper_mode_changed(vout_thread_t* vout, bool enabled, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_vout_wallpaper_mode_changed %s", enabled ? "enabled" : "disabled");

    PlayerController::VoutPtr voutPtr = PlayerController::VoutPtr(vout);
    that->callAsync([that,voutPtr, enabled] () {
        PlayerController* q = that->q_func();
        const PlayerController::VoutPtrList voutList = q->getVouts();
        if (voutPtr == nullptr  //property sets for all vout
            || (voutList.size() == 1 && voutPtr.get() == voutList[0].get()) ) //on the only vout
        {
            that->m_wallpaperMode = enabled;
            emit q->wallpaperModeChanged(enabled);
        }
    });
}

//player aout callbacks

static void on_player_aout_volume_changed(audio_output_t *, float volume, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_aout_volume_changed");
    that->callAsync([that,volume](){
        that->m_volume = volume;
        emit that->q_func()->volumeChanged( volume );
    });
}

static void on_player_aout_mute_changed(audio_output_t *, bool muted, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_aout_mute_changed");
    that->callAsync([that,muted](){
        that->m_muted = muted;
        emit that->q_func()->soundMuteChanged(muted);
    });
}

static void on_player_corks_changed(vlc_player_t *, unsigned, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_player_corks_changed");
}

static void on_player_playback_restore_queried(vlc_player_t *, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    msg_Dbg( that->p_intf, "on_playback_restore_queried");
    that->callAsync([that](){
        that->m_canRestorePlayback = true;
        emit that->q_func()->playbackRestoreQueried();
    });
}

static void on_player_timer_update(const struct vlc_player_timer_point *point,
                                   void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    that->callAsync([that,point_copy = *point](){
        PlayerController* q = that->q_func();

        that->m_player_time = point_copy;
        bool lengthOrRateChanged = false;

        if (that->m_length != that->m_player_time.length)
        {
            that->m_length = that->m_player_time.length;
            emit q->lengthChanged(that->m_length);

            lengthOrRateChanged = true;
        }
        if (that->m_rate != that->m_player_time.rate)
        {
            that->m_rate = that->m_player_time.rate;
            emit q->rateChanged(that->m_rate);

            lengthOrRateChanged = true;
        }

        vlc_tick_t system_now = vlc_tick_now();
        if (that->interpolateTime(system_now) == VLC_SUCCESS)
        {
            if (lengthOrRateChanged || !that->m_position_timer.isActive())
            {
                q->updatePosition();

                if (that->m_player_time.system_date != INT64_MAX)
                {
                    // Setup the position update interval, depending on media
                    // length and rate.  XXX: VLC_TICK_FROM_MS(1) is an educated
                    // guess, it should be also calculated according to the slider
                    // size.

                    vlc_tick_t interval =
                        that->m_length / that->m_player_time.rate / VLC_TICK_FROM_MS(1);
                    if (interval < POSITION_MIN_UPDATE_INTERVAL)
                        interval = POSITION_MIN_UPDATE_INTERVAL;

                    that->m_position_timer.start(MS_FROM_VLC_TICK(interval));
                }
            }
            q->updateTime(system_now, lengthOrRateChanged);
        }

    });
}

static void on_player_timer_discontinuity(vlc_tick_t system_date, void *data)
{
    PlayerControllerPrivate* that = static_cast<PlayerControllerPrivate*>(data);
    that->callAsync([that,system_date](){
        PlayerController* q = that->q_func();

        if (system_date != VLC_TICK_INVALID
         && that->interpolateTime(system_date) == VLC_SUCCESS)
        {
            // The discontinuity event got a valid system date, update the time
            // properties.
            q->updatePosition();
            q->updateTime(system_date, false);
        }

        // And stop the timers.
        that->m_position_timer.stop();
        that->m_time_timer.stop();
    });
}

} //extern "C"

static const struct vlc_player_cbs player_cbs = {
    on_player_current_media_changed,
    on_player_state_changed,
    on_player_error_changed,
    on_player_buffering,
    nullptr, // on_player_rate_changed: handled by on_player_timer_update
    on_player_capabilities_changed,
    nullptr, // on_player_position_changed: handled by on_player_timer_update
    nullptr, // on_player_length_changed: handled by on_player_timer_update
    on_player_track_list_changed,
    on_player_track_selection_changed,
    on_player_track_delay_changed,
    on_player_program_list_changed,
    on_player_program_selection_changed,
    on_player_titles_changed,
    on_player_title_selection_changed,
    on_player_chapter_selection_changed,
    on_player_teletext_menu_changed,
    on_player_teletext_enabled_changed,
    on_player_teletext_page_changed,
    on_player_teletext_transparency_changed,
    on_player_category_delay_changed,
    on_player_associated_subs_fps_changed,
    on_player_renderer_changed,
    on_player_record_changed,
    on_player_signal_changed,
    on_player_stats_changed,
    on_player_atobloop_changed,
    on_player_media_stopped_action_changed,
    on_player_media_meta_changed,
    on_player_media_epg_changed,
    on_player_subitems_changed,
    on_player_vout_changed,
    on_player_corks_changed,
    on_player_playback_restore_queried
};

static const struct vlc_player_vout_cbs player_vout_cbs = {
    on_player_vout_fullscreen_changed,
    on_player_vout_wallpaper_mode_changed
};

static const struct vlc_player_aout_cbs player_aout_cbs = {
    on_player_aout_volume_changed,
    on_player_aout_mute_changed,
    nullptr
};

static const struct vlc_player_timer_cbs player_timer_cbs = {
    on_player_timer_update,
    on_player_timer_discontinuity,
};

PlayerControllerPrivate::PlayerControllerPrivate(PlayerController *playercontroller, intf_thread_t *p_intf)
    : q_ptr(playercontroller)
    , p_intf(p_intf)
    , m_player(p_intf->p_sys->p_player)
    , m_videoTracks(m_player)
    , m_audioTracks(m_player)
    , m_subtitleTracks(m_player)
    , m_titleList(m_player)
    , m_chapterList(m_player)
    , m_programList(m_player)
    , m_zoom((vout_thread_t*)nullptr, "zoom")
    , m_aspectRatio((vout_thread_t*)nullptr, "aspect-ratio")
    , m_crop((vout_thread_t*)nullptr, "crop")
    , m_deinterlace((vout_thread_t*)nullptr, "deinterlace")
    , m_deinterlaceMode((vout_thread_t*)nullptr, "deinterlace-mode")
    , m_autoscale((vout_thread_t*)nullptr, "autoscale")
    , m_audioStereoMode((audio_output_t*)nullptr, "stereo-mode")
    , m_audioDeviceList(m_player)
    , m_audioVisualization((audio_output_t*)nullptr, "visual")
{
    {
        vlc_player_locker locker{m_player};
        m_player_listener = vlc_player_AddListener( m_player, &player_cbs, this );
        m_player_aout_listener = vlc_player_aout_AddListener( m_player, &player_aout_cbs, this );
        m_player_vout_listener = vlc_player_vout_AddListener( m_player, &player_vout_cbs, this );
        m_player_timer = vlc_player_AddTimer( m_player, VLC_TICK_FROM_MS(500), &player_timer_cbs, this );
    }

    QObject::connect( &m_autoscale, &QVLCBool::valueChanged, q_ptr, &PlayerController::autoscaleChanged );
    QObject::connect( &m_audioVisualization, &VLCVarChoiceModel::hasCurrentChanged, q_ptr, &PlayerController::hasAudioVisualizationChanged );

    m_time_timer.setSingleShot( true );
    m_time_timer.setTimerType( Qt::PreciseTimer );

    // Initialise fullscreen to match the player state
    m_fullscreen = vlc_player_vout_IsFullscreen( m_player );
}

PlayerController::PlayerController( intf_thread_t *_p_intf )
    : QObject(NULL)
    , d_ptr( new PlayerControllerPrivate(this, _p_intf) )
{
    /* Audio Menu */
    menusAudioMapper = new QSignalMapper(this);
    CONNECT( menusAudioMapper, mapped(const QString&), this, menusUpdateAudio(const QString&) );
    CONNECT( &d_ptr->m_position_timer, timeout(), this, updatePositionFromTimer() );
    CONNECT( &d_ptr->m_time_timer, timeout(), this, updateTimeFromTimer() );

    input_fetcher_cbs.on_art_fetch_ended = onArtFetchEnded_callback;
}

PlayerController::~PlayerController()
{
}

// PLAYBACK

input_item_t *PlayerController::getInput()
{
    Q_D(PlayerController);
    vlc_player_locker locker{ d->m_player };
    return vlc_player_GetCurrentMedia( d->m_player );
}

bool PlayerController::hasInput() const
{
    Q_D(const PlayerController);
    vlc_player_locker locker{ d->m_player };
    return vlc_player_IsStarted( d->m_player );
}

void PlayerController::reverse()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "reverse");
    vlc_player_locker lock{ d->m_player };
    if ( vlc_player_CanChangeRate( d->m_player ) )
    {
        float f_rate_ = vlc_player_GetRate( d->m_player );
        vlc_player_ChangeRate( d->m_player, -f_rate_ );
    }
}

void PlayerController::setRate( float new_rate )
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "setRate %f", new_rate);
    vlc_player_locker lock{ d->m_player };
    if ( vlc_player_CanChangeRate( d->m_player ) )
        vlc_player_ChangeRate( d->m_player, new_rate );
}

void PlayerController::setMediaStopAction(PlayerController::MediaStopAction action)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_SetMediaStoppedAction( d->m_player, static_cast<vlc_player_media_stopped_action>(action) );
}

void PlayerController::slower()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "slower");
    vlc_player_locker lock{ d->m_player };
    if ( vlc_player_CanChangeRate( d->m_player ) )
        vlc_player_DecrementRate( d->m_player );
}

void PlayerController::faster()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "faster");
    vlc_player_locker lock{ d->m_player };
    if ( vlc_player_CanChangeRate( d->m_player ) )
        vlc_player_IncrementRate( d->m_player );
}

void PlayerController::littlefaster()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "littlefaster");
    var_SetInteger( vlc_object_instance(d->p_intf), "key-action", ACTIONID_RATE_FASTER_FINE );
}

void PlayerController::littleslower()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "littleslower");
    var_SetInteger( vlc_object_instance(d->p_intf), "key-action", ACTIONID_RATE_SLOWER_FINE );
}

void PlayerController::normalRate()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "normalRate");
    vlc_player_locker lock{ d->m_player };
    if ( vlc_player_CanChangeRate( d->m_player ) )
        vlc_player_ChangeRate( d->m_player, 1.0f );
}


void PlayerController::setTime(VLCTick new_time)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    vlc_player_SetTime( d->m_player, new_time );
}

void PlayerController::setPosition(float position)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    vlc_player_SetPosition( d->m_player, position );
}

void PlayerController::jumpFwd()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "jumpFwd");
    int i_interval = var_InheritInteger( d->p_intf, "short-jump-size" );
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    vlc_player_JumpTime( d->m_player, vlc_tick_from_sec( i_interval ) );
}

void PlayerController::jumpBwd()
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "jumpBwd");
    int i_interval = var_InheritInteger( d->p_intf, "short-jump-size" );
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    vlc_player_JumpTime( d->m_player, vlc_tick_from_sec( -i_interval ) );
}

void PlayerController::jumpToTime(VLCTick i_time)
{
    Q_D(PlayerController);
    msg_Dbg( d->p_intf, "jumpToTime");
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    vlc_player_JumpTime( d->m_player, vlc_tick_from_sec( i_time ) );
}

void PlayerController::jumpToPos( float new_pos )
{
    Q_D(PlayerController);
    {
        vlc_player_locker lock{ d->m_player };
        if( !isCurrentItemSynced() )
            return;
        if( vlc_player_IsStarted( d->m_player ) )
            vlc_player_SetPosition( d->m_player, new_pos );
    }
    emit seekRequested( new_pos );
}

void PlayerController::frameNext()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    vlc_player_NextVideoFrame( d->m_player );
}

//TRACKS

void PlayerController::setAudioDelay(VLCTick delay)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    vlc_player_SetAudioDelay( d->m_player, delay, VLC_PLAYER_WHENCE_ABSOLUTE );
}

void PlayerController::setSubtitleDelay(VLCTick delay)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if(!isCurrentItemSynced() )
        return;
    vlc_player_SetSubtitleDelay( d->m_player, delay, VLC_PLAYER_WHENCE_ABSOLUTE );
}

void PlayerController::setSecondarySubtitleDelay(VLCTick delay)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if (d->m_secondarySpuEsId.get() != NULL)
        vlc_player_SetEsIdDelay(d->m_player, d->m_secondarySpuEsId.get(),
                                delay, VLC_PLAYER_WHENCE_ABSOLUTE);
}

void PlayerController::setSubtitleFPS(float fps)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    vlc_player_SetAssociatedSubsFPS( d->m_player, fps );
}

//TITLE/CHAPTER/MENU

void PlayerController::sectionPrev()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
    {
        if (vlc_player_GetSelectedChapter( d->m_player ) != NULL)
            vlc_player_SelectPrevChapter( d->m_player );
        else
            vlc_player_SelectPrevTitle( d->m_player );
    }
}

void PlayerController::sectionNext()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
    {
        if (vlc_player_GetSelectedChapter( d->m_player ) != NULL)
            vlc_player_SelectNextChapter( d->m_player );
        else
            vlc_player_SelectNextTitle( d->m_player );
    }
}

void PlayerController::sectionMenu()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
        vlc_player_Navigate( d->m_player, VLC_PLAYER_NAV_MENU );
}

void PlayerController::chapterNext()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if (vlc_player_IsStarted(d->m_player ))
        vlc_player_SelectNextChapter( d->m_player );
}

void PlayerController::chapterPrev()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if (vlc_player_IsStarted(d->m_player ))
        vlc_player_SelectPrevChapter( d->m_player );
}

void PlayerController::titleNext()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if (vlc_player_IsStarted(d->m_player ))
        vlc_player_SelectNextTitle( d->m_player );
}

void PlayerController::titlePrev()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if (vlc_player_IsStarted(d->m_player ))
        vlc_player_SelectPrevTitle( d->m_player );
}

//PROGRAMS

void PlayerController::changeProgram( int program )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if( vlc_player_IsStarted( d->m_player ) )
        vlc_player_SelectProgram( d->m_player, program );
}

//TELETEXT


void PlayerController::enableTeletext( bool enable )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if (vlc_player_IsStarted(d->m_player ))
        vlc_player_SetTeletextEnabled( d->m_player, enable );
}

void PlayerController::setTeletextPage(int page)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if (vlc_player_IsTeletextEnabled( d->m_player ))
        vlc_player_SelectTeletextPage( d->m_player, page );
}

void PlayerController::setTeletextTransparency( bool transparent )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if( !isCurrentItemSynced() )
        return;
    if (vlc_player_IsTeletextEnabled( d->m_player ))
        vlc_player_SetTeletextTransparency( d->m_player, transparent );
}

//VOUT PROPERTIES

PlayerController::VoutPtrList PlayerController::getVouts() const
{
    Q_D(const PlayerController);
    vout_thread_t **pp_vout;
    VoutPtrList VoutList;
    size_t i_vout;
    {
        vlc_player_locker lock{ d->m_player };
        if( !vlc_player_IsStarted( d->m_player ) )
            return VoutPtrList{};
        i_vout = 0;
        pp_vout = vlc_player_vout_HoldAll( d->m_player, &i_vout );
        if ( i_vout <= 0 )
            return VoutPtrList{};
    }
    VoutList.reserve( i_vout );
    for( size_t i = 0; i < i_vout; i++ )
    {
        assert( pp_vout[i] );
        //pass ownership
        VoutList.append(VoutPtr(pp_vout[i], false));
    }
    free( pp_vout );

    return VoutList;
}

PlayerController::VoutPtr PlayerController::getVout()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vout_thread_t* vout = vlc_player_vout_Hold( d->m_player );
    if( vout == NULL )
        return VoutPtr{};
    return VoutPtr{vout, false};
}

void PlayerController::setFullscreen( bool new_val )
{
    Q_D(PlayerController);
    msg_Dbg(d->p_intf, "setFullscreen %s", new_val? "fullscreen" : "windowed");
    vlc_player_locker lock{ d->m_player };
    vlc_player_vout_SetFullscreen( d->m_player, new_val );
}

void PlayerController::toggleFullscreen()
{
    Q_D(PlayerController);
    setFullscreen( ! d->m_fullscreen );
}

void PlayerController::setWallpaperMode( bool new_val )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_vout_SetWallpaperModeEnabled( d->m_player, new_val );
}

bool PlayerController::getAutoscale( ) const
{
    Q_D(const PlayerController);
    return d->m_autoscale.getValue();
}

void PlayerController::setAutoscale( bool new_val )
{
    Q_D(PlayerController);
    d->m_autoscale.setValue( new_val );
}

//AOUT PROPERTIES

PlayerController::AoutPtr PlayerController::getAout()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    return AoutPtr( vlc_player_aout_Hold( d->m_player ), false );
}

void PlayerController::setVolume(float volume)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_aout_SetVolume( d->m_player, volume );
}

void PlayerController::setVolumeUp()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_aout_IncrementVolume( d->m_player, 1, NULL );
}

void PlayerController::setVolumeDown()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_aout_DecrementVolume( d->m_player, 1, NULL );
}

void PlayerController::setMuted(bool muted)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_aout_Mute( d->m_player, muted );
}

void PlayerController::toggleMuted()
{
    Q_D(PlayerController);
    setMuted( !d->m_muted );
}

bool PlayerController::hasAudioVisualization() const
{
    Q_D(const PlayerController);
    return d->m_audioVisualization.hasCurrent();
}


void PlayerController::menusUpdateAudio( const QString& data )
{
    AoutPtr aout = getAout();
    if( aout )
        aout_DeviceSet( aout.get(), qtu(data) );
}

void PlayerController::updatePosition()
{
    Q_D(PlayerController);

    // Update position properties
    emit positionChanged(d->m_position);
    emit positionUpdated(d->m_position, d->m_time,
                         SEC_FROM_VLC_TICK(d->m_length));
}

void PlayerController::updatePositionFromTimer()
{
    Q_D(PlayerController);

    vlc_tick_t system_now = vlc_tick_now();
    if (d->interpolateTime(system_now) == VLC_SUCCESS)
        updatePosition();
}

void PlayerController::updateTime(vlc_tick_t system_now, bool forceUpdate)
{
    Q_D(PlayerController);

    // Update time properties
    emit timeChanged(d->m_time);
    if (d->m_time != VLC_TICK_INVALID && d->m_length != VLC_TICK_INVALID)
        d->m_remainingTime = d->m_length - d->m_time;
    else
        d->m_remainingTime = VLC_TICK_INVALID;
    emit remainingTimeChanged(d->m_remainingTime);

    if (d->m_player_time.system_date != INT64_MAX
     && (forceUpdate || !d->m_time_timer.isActive()))
    {
        // Tell the timer to wait until the next second is reached.
        vlc_tick_t next_update_date =
            vlc_player_timer_point_GetNextIntervalDate(&d->m_player_time, system_now,
                                                       d->m_time, VLC_TICK_FROM_SEC(1));

        vlc_tick_t next_update_interval = next_update_date - system_now;

        if (next_update_interval > 0)
        {
            // The timer can be triggered a little before. In that case, it's
            // likely that we didn't reach the next next second. It's better to
            // add a very small delay in order to be triggered after the next
            // seconds.
            static const unsigned imprecision_delay_ms = 30;

            d->m_time_timer.start(MS_FROM_VLC_TICK(next_update_interval)
                                  + imprecision_delay_ms);
        }
    }
}

void PlayerController::updateTimeFromTimer()
{
    Q_D(PlayerController);

    vlc_tick_t system_now = vlc_tick_now();
    if (d->interpolateTime(system_now) == VLC_SUCCESS)
        updateTime(system_now, false);
}

void PlayerController::restorePlaybackPos()
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    if (!isCurrentItemSynced())
        return;
    vlc_player_RestorePlaybackPos( d->m_player );
}

//MISC

void PlayerController::setABloopState(ABLoopState state)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_SetAtoBLoop( d->m_player, static_cast<vlc_player_abloop>(state));
}

void PlayerController::toggleABloopState()
{
    Q_D(PlayerController);
    switch (d->m_ABLoopState) {
    case ABLOOP_STATE_NONE:
        setABloopState(ABLOOP_STATE_A);
        break;
    case ABLOOP_STATE_A:
        setABloopState(ABLOOP_STATE_B);
        break;
    case ABLOOP_STATE_B:
        setABloopState(ABLOOP_STATE_NONE);
        break;
    }
}

void PlayerController::toggleRecord()
{
    Q_D(PlayerController);
    setRecording(!d->m_recording);
}

void PlayerController::setRecording( bool recording )
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    vlc_player_SetRecordingEnabled( d->m_player, recording );
}

void PlayerController::snapshot()
{
    VoutPtr vout = getVout();
    if (vout)
        var_TriggerCallback(vout.get(), "video-snapshot");
}


//OTHER


/* Playlist Control functions */

void PlayerController::requestArtUpdate( input_item_t *p_item, bool b_forced )
{
    Q_D(PlayerController);

    if ( !p_item )
    {
        /* default to current item */
        vlc_player_locker lock{ d->m_player };
        if ( vlc_player_IsStarted( d->m_player ) )
            p_item = vlc_player_GetCurrentMedia( d->m_player );
    }

    if ( p_item )
    {
        /* check if it has already been enqueued */
        if ( p_item->p_meta && !b_forced )
        {
            int status = vlc_meta_GetStatus( p_item->p_meta );
            if ( status & ( ITEM_ART_NOTFOUND|ITEM_ART_FETCHED ) )
                return;
        }
        libvlc_ArtRequest( vlc_object_instance(d->p_intf), p_item,
                           (b_forced) ? META_REQUEST_OPTION_FETCH_ANY
                                      : META_REQUEST_OPTION_FETCH_LOCAL,
                           &input_fetcher_cbs, this );
    }
}

void PlayerController::onArtFetchEnded_callback(input_item_t *p_item, bool fetched,
                                                void *userdata)
{
    PlayerController *me = reinterpret_cast<PlayerController *>(userdata);
    me->onArtFetchEnded(p_item, fetched);
}

void PlayerController::onArtFetchEnded(input_item_t *p_item, bool)
{
    Q_D(PlayerController);

    vlc_player_locker lock{ d->m_player };
    bool b_current_item = (p_item == vlc_player_GetCurrentMedia( d->m_player ));
    /* No input will signal the cover art to update,
         * let's do it ourself */
    if ( b_current_item )
        d->UpdateArt( p_item );
    else
        emit artChanged( p_item );
}

const QString PlayerController::decodeArtURL( input_item_t *p_item )
{
    assert( p_item );

    char *psz_art = input_item_GetArtURL( p_item );
    if( psz_art )
    {
        char *psz = vlc_uri2path( psz_art );
        free( psz_art );
        psz_art = psz;
    }

#if 0
    /* Taglib seems to define a attachment://, It won't work yet */
    url = url.replace( "attachment://", "" );
#endif

    QString path = qfu( psz_art ? psz_art : "" );
    free( psz_art );
    return path;
}

void PlayerController::setArt( input_item_t *p_item, QString fileUrl )
{
    Q_D(PlayerController);
    if( hasInput() )
    {
        char *psz_cachedir = config_GetUserDir( VLC_CACHE_DIR );
        QString old_url = decodeArtURL( p_item );
        old_url = QDir( old_url ).canonicalPath();

        if( old_url.startsWith( QString::fromUtf8( psz_cachedir ) ) )
            QFile( old_url ).remove(); /* Purge cached artwork */

        free( psz_cachedir );

        input_item_SetArtURL( p_item , fileUrl.toUtf8().constData() );
        d->UpdateArt( p_item );
    }
}

int PlayerController::AddAssociatedMedia(es_format_category_e cat, const QString &uri, bool select, bool notify, bool check_ext)
{
    Q_D(PlayerController);
    vlc_player_locker lock{ d->m_player };
    return vlc_player_AddAssociatedMedia( d->m_player, cat, qtu(uri), select, notify, check_ext );
}

#define QABSTRACTLIST_GETTER( type, fun, var ) \
    type* PlayerController::fun() \
    { \
        Q_D(PlayerController); \
        return &d->var; \
    }


QABSTRACTLIST_GETTER( TrackListModel, getVideoTracks, m_videoTracks)
QABSTRACTLIST_GETTER( TrackListModel, getAudioTracks, m_audioTracks)
QABSTRACTLIST_GETTER( TrackListModel, getSubtitleTracks, m_subtitleTracks)
QABSTRACTLIST_GETTER( TitleListModel, getTitles, m_titleList)
QABSTRACTLIST_GETTER( ChapterListModel,getChapters, m_chapterList)
QABSTRACTLIST_GETTER( AudioDeviceModel, getAudioDevices, m_audioDeviceList)
QABSTRACTLIST_GETTER( ProgramListModel, getPrograms, m_programList)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getZoom, m_zoom)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getAspectRatio, m_aspectRatio)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getCrop, m_crop)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getDeinterlace, m_deinterlace)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getDeinterlaceMode, m_deinterlaceMode)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getAudioStereoMode, m_audioStereoMode)
QABSTRACTLIST_GETTER( VLCVarChoiceModel, getAudioVisualizations, m_audioVisualization)


#undef QABSTRACTLIST_GETTER

#define PRIMITIVETYPE_GETTER( type, fun, var ) \
    type PlayerController::fun() const \
    { \
        Q_D(const PlayerController); \
        return d->var; \
    }


PRIMITIVETYPE_GETTER(PlayerController::PlayingState, getPlayingState, m_playing_status)
PRIMITIVETYPE_GETTER(QString, getName, m_name)
PRIMITIVETYPE_GETTER(VLCTick, getTime, m_time)
PRIMITIVETYPE_GETTER(VLCTick, getRemainingTime, m_remainingTime)
PRIMITIVETYPE_GETTER(float, getPosition, m_position)
PRIMITIVETYPE_GETTER(VLCTick, getLength, m_length)
PRIMITIVETYPE_GETTER(VLCTick, getAudioDelay, m_audioDelay)
PRIMITIVETYPE_GETTER(VLCTick, getSubtitleDelay, m_subtitleDelay)
PRIMITIVETYPE_GETTER(VLCTick, getSecondarySubtitleDelay, m_secondarySubtitleDelay)
PRIMITIVETYPE_GETTER(bool, isSeekable, m_capabilities & VLC_PLAYER_CAP_SEEK)
PRIMITIVETYPE_GETTER(bool, isRewindable, m_capabilities & VLC_PLAYER_CAP_REWIND)
PRIMITIVETYPE_GETTER(bool, isPausable, m_capabilities & VLC_PLAYER_CAP_PAUSE)
PRIMITIVETYPE_GETTER(bool, isRateChangable, m_capabilities & VLC_PLAYER_CAP_CHANGE_RATE)
PRIMITIVETYPE_GETTER(bool, canRestorePlayback, m_canRestorePlayback);
PRIMITIVETYPE_GETTER(float, getSubtitleFPS, m_subtitleFPS)
PRIMITIVETYPE_GETTER(bool, hasVideoOutput, m_hasVideo)
PRIMITIVETYPE_GETTER(float, getBuffering, m_buffering)
PRIMITIVETYPE_GETTER(PlayerController::MediaStopAction, getMediaStopAction, m_mediaStopAction)
PRIMITIVETYPE_GETTER(float, getVolume, m_volume)
PRIMITIVETYPE_GETTER(bool, isMuted, m_muted)
PRIMITIVETYPE_GETTER(bool, isFullscreen, m_fullscreen)
PRIMITIVETYPE_GETTER(bool, getWallpaperMode, m_wallpaperMode)
PRIMITIVETYPE_GETTER(float, getRate, m_rate)
PRIMITIVETYPE_GETTER(bool, hasTitles, m_hasTitles)
PRIMITIVETYPE_GETTER(bool, hasChapters, m_hasChapters)
PRIMITIVETYPE_GETTER(bool, hasMenu, m_hasMenu)
PRIMITIVETYPE_GETTER(bool, isMenu, m_isMenu)
PRIMITIVETYPE_GETTER(bool, isInteractive, m_isInteractive)
PRIMITIVETYPE_GETTER(bool, isEncrypted, m_encrypted)
PRIMITIVETYPE_GETTER(bool, isRecording, m_recording)
PRIMITIVETYPE_GETTER(PlayerController::ABLoopState, getABloopState, m_ABLoopState)
PRIMITIVETYPE_GETTER(VLCTick, getABLoopA, m_ABLoopA)
PRIMITIVETYPE_GETTER(VLCTick, getABLoopB, m_ABLoopB)
PRIMITIVETYPE_GETTER(bool, isTeletextEnabled, m_teletextEnabled)
PRIMITIVETYPE_GETTER(bool, isTeletextAvailable, m_teletextAvailable)
PRIMITIVETYPE_GETTER(int, getTeletextPage, m_teletextPage)
PRIMITIVETYPE_GETTER(bool, getTeletextTransparency, m_teletextTransparent)

#undef PRIMITIVETYPE_GETTER
