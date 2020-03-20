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

#ifndef QVLC_INPUT_MANAGER_H_
#define QVLC_INPUT_MANAGER_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt.hpp"
#include <QObject>
#include <QEvent>
#include <QAbstractListModel>
#include <QScopedPointer>
#include <vlc_cxx_helpers.hpp>
#include "player/input_models.hpp"
#include "util/audio_device_model.hpp"
#include "util/varchoicemodel.hpp"
#include "util/vlctick.hpp"

class QSignalMapper;

class IMEvent : public QEvent
{
public:
    enum event_types {
        FullscreenControlToggle = QEvent::User + IMEventTypeOffset + 1,
        FullscreenControlShow,
        FullscreenControlHide,
        FullscreenControlPlanHide,
    };

    IMEvent( event_types type, input_item_t *p_input = NULL )
        : QEvent( (QEvent::Type)(type) )
    {
        if( (p_item = p_input) != NULL )
            input_item_Hold( p_item );
    }

    virtual ~IMEvent()
    {
        if( p_item )
            input_item_Release( p_item );
    }

    input_item_t *item() const { return p_item; }

private:
    input_item_t *p_item;
};

class PlayerControllerPrivate;
class PlayerController : public QObject
{
    Q_OBJECT
    friend class VLCMenuBar;

public:
    enum ABLoopState {
        ABLOOP_STATE_NONE= VLC_PLAYER_ABLOOP_NONE,
        ABLOOP_STATE_A = VLC_PLAYER_ABLOOP_A,
        ABLOOP_STATE_B = VLC_PLAYER_ABLOOP_B
    };
    Q_ENUM(ABLoopState)

    enum PlayingState
    {
        PLAYING_STATE_STARTED = VLC_PLAYER_STATE_STARTED,
        PLAYING_STATE_PLAYING = VLC_PLAYER_STATE_PLAYING,
        PLAYING_STATE_PAUSED = VLC_PLAYER_STATE_PAUSED,
        PLAYING_STATE_STOPPING = VLC_PLAYER_STATE_STOPPING,
        PLAYING_STATE_STOPPED = VLC_PLAYER_STATE_STOPPED
    };
    Q_ENUM(PlayingState)

    enum MediaStopAction
    {
        MEDIA_STOPPED_CONTINUE = VLC_PLAYER_MEDIA_STOPPED_CONTINUE,
        MEDIA_STOPPED_PAUSE = VLC_PLAYER_MEDIA_STOPPED_PAUSE,
        MEDIA_STOPPED_STOP = VLC_PLAYER_MEDIA_STOPPED_STOP,
        MEDIA_STOPPED_EXIT = VLC_PLAYER_MEDIA_STOPPED_EXIT
    };
    Q_ENUM(MediaStopAction)

    enum Telekeys{
        TELE_RED = VLC_PLAYER_TELETEXT_KEY_RED,
        TELE_GREEN = VLC_PLAYER_TELETEXT_KEY_GREEN,
        TELE_YELLOW = VLC_PLAYER_TELETEXT_KEY_YELLOW,
        TELE_BLUE = VLC_PLAYER_TELETEXT_KEY_BLUE,
        TELE_INDEX = VLC_PLAYER_TELETEXT_KEY_INDEX
    };
    Q_ENUM(Telekeys)

    //playback
    Q_PROPERTY(PlayingState playingState READ getPlayingState NOTIFY playingStateChanged)
    Q_PROPERTY(bool isPlaying READ hasInput NOTIFY inputChanged)
    Q_PROPERTY(QString name READ getName NOTIFY nameChanged)
    Q_PROPERTY(float buffering READ getBuffering  NOTIFY bufferingChanged)
    Q_PROPERTY(float rate READ getRate WRITE setRate NOTIFY rateChanged)
    Q_PROPERTY(MediaStopAction mediaStopAction READ getMediaStopAction WRITE setMediaStopAction NOTIFY mediaStopActionChanged)

    Q_PROPERTY(VLCTick time READ getTime WRITE setTime NOTIFY timeChanged)
    Q_PROPERTY(VLCTick remainingTime READ getRemainingTime NOTIFY remainingTimeChanged)
    Q_PROPERTY(float position READ getPosition WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(VLCTick length READ getLength NOTIFY lengthChanged)

    Q_PROPERTY(bool seekable READ isSeekable NOTIFY seekableChanged)
    Q_PROPERTY(bool rewindable READ isRewindable NOTIFY rewindableChanged)
    Q_PROPERTY(bool pausable READ isPausable NOTIFY pausableChanged)
    Q_PROPERTY(bool ratechangable READ isRateChangable NOTIFY rateChangableChanged)

    Q_PROPERTY(bool canRestorePlayback READ canRestorePlayback NOTIFY playbackRestoreQueried)

    //tracks
    Q_PROPERTY(TrackListModel* videoTracks READ getVideoTracks CONSTANT)
    Q_PROPERTY(TrackListModel* audioTracks READ getAudioTracks CONSTANT)
    Q_PROPERTY(TrackListModel* subtitleTracks READ getSubtitleTracks CONSTANT)

    Q_PROPERTY(VLCTick audioDelay READ getAudioDelay WRITE setAudioDelay NOTIFY audioDelayChanged)
    Q_PROPERTY(VLCTick subtitleDelay READ getSubtitleDelay WRITE setSubtitleDelay NOTIFY subtitleDelayChanged)
    Q_PROPERTY(VLCTick secondarySubtitleDelay READ getSecondarySubtitleDelay WRITE setSecondarySubtitleDelay NOTIFY secondarySubtitleDelayChanged)
    Q_PROPERTY(float subtitleFPS READ getSubtitleFPS WRITE setSubtitleFPS NOTIFY subtitleFPSChanged)

    //title/chapters/menu
    Q_PROPERTY(TitleListModel* titles READ getTitles CONSTANT)
    Q_PROPERTY(ChapterListModel* chapters READ getChapters CONSTANT)

    Q_PROPERTY(bool hasTitles READ hasTitles NOTIFY hasTitlesChanged)
    Q_PROPERTY(bool hasChapters READ hasChapters NOTIFY hasChaptersChanged)
    Q_PROPERTY(bool hasMenu READ hasMenu NOTIFY hasMenuChanged)
    Q_PROPERTY(bool isMenu READ isMenu NOTIFY isMenuChanged)
    Q_PROPERTY(bool isInteractive READ isInteractive NOTIFY isInteractiveChanged)

    //programs
    Q_PROPERTY(ProgramListModel* programs READ getPrograms CONSTANT)
    Q_PROPERTY(bool isEncrypted READ isEncrypted NOTIFY isEncryptedChanged)

    //teletext
    Q_PROPERTY(bool teletextEnabled READ isTeletextEnabled WRITE enableTeletext NOTIFY teletextEnabledChanged)
    Q_PROPERTY(bool isTeletextAvailable READ isTeletextAvailable  NOTIFY teletextAvailableChanged)
    Q_PROPERTY(int teletextPage READ getTeletextPage WRITE setTeletextPage NOTIFY teletextPageChanged)
    Q_PROPERTY(bool teletextTransparency READ getTeletextTransparency WRITE setTeletextTransparency NOTIFY teletextTransparencyChanged)

    //vout properties
    Q_PROPERTY(bool hasVideoOutput READ hasVideoOutput NOTIFY hasVideoOutputChanged)
    Q_PROPERTY(VLCVarChoiceModel* zoom READ getZoom CONSTANT)
    Q_PROPERTY(VLCVarChoiceModel* aspectRatio READ getAspectRatio CONSTANT)
    Q_PROPERTY(VLCVarChoiceModel* crop READ getCrop CONSTANT)
    Q_PROPERTY(VLCVarChoiceModel* deinterlace READ getDeinterlace CONSTANT)
    Q_PROPERTY(VLCVarChoiceModel* deinterlaceMode READ getDeinterlaceMode CONSTANT)
    Q_PROPERTY(bool fullscreen READ isFullscreen WRITE setFullscreen NOTIFY fullscreenChanged)
    Q_PROPERTY(bool wallpaperMode READ getWallpaperMode WRITE setWallpaperMode NOTIFY wallpaperModeChanged)
    Q_PROPERTY(bool autoscale READ getAutoscale WRITE setAutoscale NOTIFY autoscaleChanged)

    //aout properties
    Q_PROPERTY(float volume READ getVolume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ isMuted WRITE setMuted NOTIFY soundMuteChanged)
    Q_PROPERTY(AudioDeviceModel* audioDevices READ getAudioDevices CONSTANT)
    Q_PROPERTY(VLCVarChoiceModel* audioStereoMode READ getAudioStereoMode CONSTANT)
    Q_PROPERTY(VLCVarChoiceModel* audioVisualization READ getAudioVisualizations CONSTANT)
    Q_PROPERTY(bool hasAudioVisualization READ hasAudioVisualization NOTIFY hasAudioVisualizationChanged)

    //misc
    Q_PROPERTY(ABLoopState ABloopState READ getABloopState WRITE setABloopState NOTIFY ABLoopStateChanged)
    Q_PROPERTY(VLCTick ABLoopA READ getABLoopA NOTIFY ABLoopAChanged)
    Q_PROPERTY(VLCTick ABLoopB READ getABLoopB NOTIFY ABLoopBChanged)
    Q_PROPERTY(bool recording READ isRecording WRITE setRecording NOTIFY recordingChanged)

    /* exposed actions */
public slots:
    Q_INVOKABLE void reverse();
    Q_INVOKABLE void slower();
    Q_INVOKABLE void faster();
    Q_INVOKABLE void littlefaster();
    Q_INVOKABLE void littleslower();
    Q_INVOKABLE void normalRate();

    Q_INVOKABLE void jumpFwd();
    Q_INVOKABLE void jumpBwd();
    Q_INVOKABLE void jumpToTime( VLCTick i_time );
    Q_INVOKABLE void jumpToPos( float );
    Q_INVOKABLE void frameNext();

    //title/chapters/menu
    Q_INVOKABLE void sectionNext();
    Q_INVOKABLE void sectionPrev();
    Q_INVOKABLE void sectionMenu();

    Q_INVOKABLE void chapterNext();
    Q_INVOKABLE void chapterPrev();
    Q_INVOKABLE void titleNext();
    Q_INVOKABLE void titlePrev();

    //programs
    Q_INVOKABLE void changeProgram( int );

    //vout properties
    Q_INVOKABLE void toggleFullscreen();

    //aout properties
    Q_INVOKABLE void setVolumeUp();
    Q_INVOKABLE void setVolumeDown();
    Q_INVOKABLE void toggleMuted();

    //misc
    Q_INVOKABLE void toggleABloopState();
    Q_INVOKABLE void snapshot();
    Q_INVOKABLE void toggleRecord();

public:
    PlayerController( intf_thread_t * );
    ~PlayerController();

public:
    static void vout_Hold_fct( vout_thread_t* vout ) { vout_Hold(vout); }
    static void vout_Release_fct( vout_thread_t* vout ) { vout_Release(vout); }
    typedef vlc_shared_data_ptr_type(vout_thread_t, PlayerController::vout_Hold_fct, PlayerController::vout_Release_fct) VoutPtr;

    static void aout_Hold_fct( audio_output_t* aout ) { aout_Hold(aout); }
    static void aout_Release_fct( audio_output_t* aout ) { aout_Release(aout); }
    typedef vlc_shared_data_ptr_type(audio_output_t, PlayerController::aout_Hold_fct, PlayerController::aout_Release_fct) AoutPtr;
    typedef QVector<VoutPtr> VoutPtrList;


public:
    input_item_t *getInput();

    VoutPtr getVout();
    VoutPtrList getVouts() const;
    PlayerController::AoutPtr getAout();
    int AddAssociatedMedia(enum es_format_category_e cat, const QString& uri, bool select, bool notify, bool check_ext);

    void requestArtUpdate( input_item_t *p_item, bool b_forced );
    void setArt( input_item_t *p_item, QString fileUrl );
    static const QString decodeArtURL( input_item_t *p_item );
    void updatePosition();
    void updateTime(vlc_tick_t system_now, bool forceTimer);

    //getter/setters binded to a Q_PROPERTY
public slots:
    //playback
    PlayingState getPlayingState() const;
    bool hasInput() const;
    QString getName() const;
    float getBuffering() const;
    float getRate() const;
    void setRate( float );
    MediaStopAction getMediaStopAction() const;
    void setMediaStopAction(MediaStopAction );
    VLCTick getTime() const;
    void setTime(VLCTick);
    VLCTick getRemainingTime() const;
    float getPosition() const;
    void setPosition(float);
    VLCTick getLength() const;
    bool isSeekable() const;
    bool isRewindable() const;
    bool isPausable() const;
    bool isRateChangable() const;
    void updatePositionFromTimer();
    void updateTimeFromTimer();
    bool canRestorePlayback() const;
    void restorePlaybackPos();

    //tracks
    TrackListModel* getVideoTracks();
    TrackListModel* getAudioTracks();
    TrackListModel* getSubtitleTracks();

    VLCTick getAudioDelay() const;
    void setAudioDelay( VLCTick );
    VLCTick getSubtitleDelay() const;
    VLCTick getSecondarySubtitleDelay() const;
    void setSubtitleDelay( VLCTick );
    void setSecondarySubtitleDelay( VLCTick );
    float getSubtitleFPS( ) const;
    void setSubtitleFPS( float );

    //title/chapters/menu
    TitleListModel* getTitles();
    ChapterListModel* getChapters();
    bool hasTitles() const;
    bool hasChapters() const;
    bool hasMenu()  const;
    bool isMenu()  const;
    bool isInteractive()  const;

    //programs
    ProgramListModel* getPrograms();
    bool isEncrypted() const;

    //teletext
    bool isTeletextEnabled() const;
    void enableTeletext(bool enable);
    bool isTeletextAvailable() const;
    int getTeletextPage() const;
    void setTeletextPage(int page);
    bool getTeletextTransparency() const;
    void setTeletextTransparency( bool transparent );

    //vout properties
    bool hasVideoOutput() const;
    VLCVarChoiceModel* getZoom();
    VLCVarChoiceModel* getAspectRatio();
    VLCVarChoiceModel* getCrop();
    VLCVarChoiceModel* getDeinterlace();
    VLCVarChoiceModel* getDeinterlaceMode();
    bool isFullscreen() const;
    void setFullscreen( bool );
    bool getWallpaperMode() const;
    void setWallpaperMode( bool );
    bool getAutoscale() const;
    void setAutoscale( bool );

    //aout properties
    float getVolume() const;
    void setVolume( float volume );
    bool isMuted() const;
    void setMuted( bool muted );
    AudioDeviceModel* getAudioDevices();
    VLCVarChoiceModel* getAudioStereoMode();
    VLCVarChoiceModel* getAudioVisualizations();
    bool hasAudioVisualization() const;


    //misc
    bool isRecording() const;
    void setRecording(bool record);
    void setABloopState(ABLoopState);
    ABLoopState getABloopState() const;
    VLCTick getABLoopA() const;
    VLCTick getABLoopB() const;

signals:
    //playback
    void playingStateChanged( PlayingState state );
    void inputChanged( bool hasInput );
    void nameChanged( const QString& );
    void bufferingChanged( float );
    void rateChanged( float );
    void mediaStopActionChanged( MediaStopAction );

    void timeChanged( VLCTick );
    void remainingTimeChanged( VLCTick );
    void positionChanged( float );
    void lengthChanged( VLCTick );
    void positionUpdated( float , VLCTick, int );
    void seekRequested( float pos ); //not exposed through Q_PROPERTY

    void seekableChanged( bool );
    void rewindableChanged( bool );
    void pausableChanged( bool );
    void recordableChanged( bool );
    void rateChangableChanged( bool );

    void playbackRestoreQueried();

    //tracks
    void audioDelayChanged(VLCTick);
    void subtitleDelayChanged(VLCTick);
    void secondarySubtitleDelayChanged(VLCTick);
    void subtitleFPSChanged(float);

    //title/chapters/menu
    void hasTitlesChanged( bool );
    void hasChaptersChanged( bool );
    void hasMenuChanged( bool );
    void isMenuChanged( bool );
    void isInteractiveChanged( bool );

    //program
    void isEncryptedChanged( bool );

    //teletext
    void teletextEnabledChanged(bool);
    void teletextAvailableChanged(bool);
    void teletextPageChanged(int);
    void teletextTransparencyChanged(bool);

    //vout properties
    void hasVideoOutputChanged( bool );
    void fullscreenChanged( bool );
    void wallpaperModeChanged( bool );
    void autoscaleChanged( bool );
    void voutListChanged( vout_thread_t **pp_vout, int i_vout );

    //aout properties
    void volumeChanged( float );
    void soundMuteChanged( bool );
    void hasAudioVisualizationChanged( bool );

    //misc
    void recordingChanged( bool );
    void ABLoopStateChanged(ABLoopState);
    void ABLoopAChanged(VLCTick);
    void ABLoopBChanged(VLCTick);

    // Other signals

    // You can resume playback
    void resumePlayback( VLCTick );
    // Statistics are updated
    void statisticsUpdated( const input_stats_t& stats );
    void infoChanged( input_item_t* );
    void currentMetaChanged( input_item_t* );
    void metaChanged( input_item_t *);
    void artChanged( QString ); /* current item art ( same as item == NULL ) */
    void artChanged( input_item_t * );

    void bookmarksChanged();
    // Program Event changes
    void epgChanged();

private slots:
    void menusUpdateAudio( const QString& );

private:
    bool isCurrentItemSynced();

    Q_DECLARE_PRIVATE(PlayerController)
    QScopedPointer<PlayerControllerPrivate> d_ptr;
    QSignalMapper *menusAudioMapper; //used by VLCMenuBar

    /* updateArt gui request */
    input_fetcher_callbacks_t input_fetcher_cbs;
    static void onArtFetchEnded_callback(input_item_t *, bool fetched, void *userdata);
    void onArtFetchEnded(input_item_t *, bool fetched);
};

#endif
