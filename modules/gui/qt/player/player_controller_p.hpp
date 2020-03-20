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

#ifndef QVLC_INPUT_MANAGER_P_H_
#define QVLC_INPUT_MANAGER_P_H_

#include "player_controller.hpp"
#include "util/variables.hpp"
#include "input_models.hpp"
#include "util/varchoicemodel.hpp"

#include <QTimer>

class PlayerControllerPrivate {
    Q_DISABLE_COPY(PlayerControllerPrivate)
public:
    Q_DECLARE_PUBLIC(PlayerController)
    PlayerController * const q_ptr;

public:
    PlayerControllerPrivate(PlayerController* playercontroller, intf_thread_t* p_intf);
    PlayerControllerPrivate() = delete;
    ~PlayerControllerPrivate();

    void UpdateName( input_item_t *p_item );
    void UpdateArt( input_item_t *p_item );
    void UpdateMeta( input_item_t *p_item );
    void UpdateInfo( input_item_t *p_item );
    void UpdateStats( const input_stats_t& stats );
    void UpdateProgram(vlc_player_list_action action, const vlc_player_program *prgm);
    void UpdateVouts(vout_thread_t **vouts, size_t i_vouts);
    void UpdateTrackSelection(vlc_es_id_t *trackid, bool selected);
    void UpdateSpuOrder(vlc_es_id_t *es_id, enum vlc_vout_order spu_order);
    int interpolateTime(vlc_tick_t system_now);

    ///call function @a fun on object thread
    template <typename Fun>
    void callAsync(Fun&& fun)
    {
        Q_Q(PlayerController);
#if (QT_VERSION >= QT_VERSION_CHECK(5, 10, 0))
        QMetaObject::invokeMethod(q, std::forward<Fun>(fun), Qt::QueuedConnection, nullptr);
#else
        QObject src;
        QObject::connect(&src, &QObject::destroyed, q, std::forward<Fun>(fun), Qt::QueuedConnection);
#endif
    }

public:
    intf_thread_t           *p_intf;
    vlc_player_t            *m_player;

    //callbacks
    vlc_player_listener_id* m_player_listener = nullptr;
    vlc_player_aout_listener_id* m_player_aout_listener = nullptr;
    vlc_player_vout_listener_id* m_player_vout_listener = nullptr;

    //playback
    PlayerController::PlayingState m_playing_status = PlayerController::PLAYING_STATE_STOPPED;
    QString         m_name;
    float           m_buffering = 0.f;
    float           m_rate = 1.f;
    PlayerController::MediaStopAction m_mediaStopAction = PlayerController::MEDIA_STOPPED_CONTINUE;

    VLCTick      m_time = 0;
    VLCTick      m_remainingTime = 0;
    float           m_position = 0.f;
    VLCTick      m_length= 0;

    using InputItemPtr = vlc_shared_data_ptr_type(input_item_t,
                                                  input_item_Hold,
                                                  input_item_Release);

    InputItemPtr    m_currentItem;
    bool            m_canRestorePlayback = false;

    int             m_capabilities = 0;

    //tracks
    TrackListModel m_videoTracks;
    TrackListModel m_audioTracks;
    TrackListModel m_subtitleTracks;

    vlc_shared_data_ptr_type(vlc_es_id_t, vlc_es_id_Hold, vlc_es_id_Release) m_secondarySpuEsId;

    VLCTick      m_audioDelay = 0;
    VLCTick      m_subtitleDelay = 0;
    VLCTick      m_secondarySubtitleDelay = 0;
    float        m_subtitleFPS = 1.0;

    //timer
    vlc_player_timer_id* m_player_timer = nullptr;
    vlc_player_timer_id* m_player_timer_smpte = nullptr;
    struct vlc_player_timer_point m_player_time;
    QTimer m_position_timer;
    QTimer m_time_timer;

    //title/chapters/menu
    TitleListModel m_titleList;
    ChapterListModel m_chapterList;
    bool m_hasTitles = false;
    bool m_hasChapters = false;
    bool m_hasMenu = false;
    bool m_isMenu = false;
    bool m_isInteractive = false;

    //programs
    ProgramListModel m_programList;
    bool m_encrypted = false;

    //teletext
    bool m_teletextEnabled = false;
    bool m_teletextAvailable = false;
    int m_teletextPage = false;
    bool m_teletextTransparent = false;

    //vout properties
    VLCVarChoiceModel m_zoom;
    VLCVarChoiceModel m_aspectRatio;
    VLCVarChoiceModel m_crop;
    VLCVarChoiceModel m_deinterlace;
    VLCVarChoiceModel m_deinterlaceMode;
    QVLCBool m_autoscale;
    bool            m_hasVideo = false;
    bool            m_fullscreen = false;
    bool            m_wallpaperMode = false;

    //aout properties
    VLCVarChoiceModel m_audioStereoMode;
    float           m_volume = 0.f;
    bool            m_muted = false;
    AudioDeviceModel m_audioDeviceList;
    VLCVarChoiceModel m_audioVisualization;

    //misc
    bool            m_recording = false;
    PlayerController::ABLoopState m_ABLoopState = PlayerController::ABLOOP_STATE_NONE;
    VLCTick m_ABLoopA = VLC_TICK_INVALID;
    VLCTick m_ABLoopB = VLC_TICK_INVALID;

    //others
    QString         m_artUrl;
    struct input_stats_t m_stats;

};

#endif /* QVLC_INPUT_MANAGER_P_H_ */
