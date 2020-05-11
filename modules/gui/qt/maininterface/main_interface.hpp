/*****************************************************************************
 * main_interface.hpp : Main Interface
 ****************************************************************************
 * Copyright (C) 2006-2010 VideoLAN and AUTHORS
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
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

#ifndef QVLC_MAIN_INTERFACE_H_
#define QVLC_MAIN_INTERFACE_H_

#include "qt.hpp"

#include "widgets/native/qvlcframe.hpp"
#include "player/player_controller.hpp"

#include <QSystemTrayIcon>
#include <QStackedWidget>
#include <QtQuick/QQuickView>
#include <QtQuickWidgets/QQuickWidget>
#include <QtQuick/QQuickWindow>

#ifdef _WIN32
# include <shobjidl.h>
#endif

#include <atomic>

class QSettings;
class QCloseEvent;
class QKeyEvent;
class QLabel;
class QEvent;
class VideoWidget;
class VisualSelector;
class QVBoxLayout;
class QStackedLayout;
class QMenu;
class QSize;
class QScreen;
class QTimer;
class StandardPLPanel;
struct vout_window_t;
class VideoSurfaceProvider;

class MainInterface : public QVLCMW
{
    Q_OBJECT

    Q_PROPERTY(bool playlistDocked READ isPlaylistDocked WRITE setPlaylistDocked NOTIFY playlistDockedChanged)
    Q_PROPERTY(bool playlistVisible READ isPlaylistVisible WRITE setPlaylistVisible NOTIFY playlistVisibleChanged)
    Q_PROPERTY(bool interfaceAlwaysOnTop READ isInterfaceAlwaysOnTop WRITE setInterfaceAlwaysOnTop NOTIFY interfaceAlwaysOnTopChanged)
    Q_PROPERTY(bool interfaceFullScreen READ isInterfaceFullScreen WRITE setInterfaceFullScreen NOTIFY interfaceFullScreenChanged)
    Q_PROPERTY(bool hasEmbededVideo READ hasEmbededVideo NOTIFY hasEmbededVideoChanged)
    Q_PROPERTY(bool showRemainingTime READ isShowRemainingTime WRITE setShowRemainingTime NOTIFY showRemainingTimeChanged)
    Q_PROPERTY(VLCVarChoiceModel* extraInterfaces READ getExtraInterfaces CONSTANT)
    Q_PROPERTY(float intfScaleFactor READ getIntfScaleFactor NOTIFY intfScaleFactorChanged)

public:
    /* tors */
    MainInterface( intf_thread_t *);
    virtual ~MainInterface();

    static const QEvent::Type ToolbarsNeedRebuild;

public:
    void requestResizeVideo( unsigned, unsigned );
    void requestVideoState( unsigned );
    void requestVideoWindowed( );
    void requestVideoFullScreen( const char * );

public:
    /* Getters */
    QSystemTrayIcon *getSysTray() { return sysTray; }
    QMenu *getSysTrayMenu() { return systrayMenu; }
    enum
    {
        CONTROLS_VISIBLE  = 0x1,
        CONTROLS_HIDDEN   = 0x2,
        CONTROLS_ADVANCED = 0x4,
    };
    enum
    {
        RAISE_NEVER,
        RAISE_VIDEO,
        RAISE_AUDIO,
        RAISE_AUDIOVIDEO,
    };
    bool isInterfaceFullScreen() { return b_interfaceFullScreen; }
    bool isPlaylistDocked() { return b_playlistDocked; }
    bool isPlaylistVisible() { return playlistVisible; }
    bool isInterfaceAlwaysOnTop() { return b_interfaceOnTop; }
    inline bool isShowRemainingTime() const  { return m_showRemainingTime; }
    inline float getIntfScaleFactor() const { return m_intfScaleFactor; }

    bool hasEmbededVideo() const;
    VideoSurfaceProvider* getVideoSurfaceProvider() const;
    void setVideoSurfaceProvider(VideoSurfaceProvider* videoSurfaceProvider);;

protected:
    void dropEventPlay( QDropEvent* event, bool b_play );
    void changeEvent( QEvent * ) Q_DECL_OVERRIDE;
    void dropEvent( QDropEvent *) Q_DECL_OVERRIDE;
    void dragEnterEvent( QDragEnterEvent * ) Q_DECL_OVERRIDE;
    void dragMoveEvent( QDragMoveEvent * ) Q_DECL_OVERRIDE;
    void dragLeaveEvent( QDragLeaveEvent * ) Q_DECL_OVERRIDE;
    void closeEvent( QCloseEvent *) Q_DECL_OVERRIDE;
    virtual void toggleUpdateSystrayMenuWhenVisible();
    void resizeWindow(int width, int height);

protected:
    /* Systray */
    void createSystray();
    void initSystray();
    void handleSystray();

    /* */
    void setInterfaceFullScreen( bool );
    void computeMinimumSize();

    bool m_hasEmbededVideo = false;
    VideoSurfaceProvider* m_videoSurfaceProvider = nullptr;
    bool m_showRemainingTime = false;

    /* */
    QSettings           *settings;
    QSystemTrayIcon     *sysTray;
    QMenu               *systrayMenu;

    QString              input_name;
    QVBoxLayout         *mainLayout;

    /* Status Bar */
    QLabel              *nameLabel;
    QLabel              *cryptedLabel;

    /* Status and flags */
    QPoint              lastWinPosition;
    QSize               lastWinSize;  /// To restore the same window size when leaving fullscreen
    QScreen             *lastWinScreen;

    QSize               pendingResize; // to be applied when fullscreen is disabled

    QMap<QWidget *, QSize> stackWidgetsSizes;

    /* Flags */
    float                m_intfUserScaleFactor;
    float                m_intfScaleFactor;
    unsigned             i_notificationSetting; /// Systray Notifications
    bool                 b_autoresize;          ///< persistent resizable window
    bool                 b_videoFullScreen;     ///< --fullscreen
    bool                 b_hideAfterCreation;
    bool                 b_minimalView;         ///< Minimal video
    bool                 b_playlistDocked;
    bool                 b_interfaceFullScreen;
    bool                 b_interfaceOnTop;      ///keep UI on top
    bool                 b_pauseOnMinimize;
    bool                 b_maximizedView;
    bool                 b_isWindowTiled;
#ifdef QT5_HAS_WAYLAND
    bool                 b_hasWayland;
#endif
    bool                 b_hasMedialibrary = false;
    /* States */
    bool                 playlistVisible;       ///< Is the playlist visible ?

    bool                 b_hasPausedWhenMinimized;

    static const Qt::Key kc[10]; /* easter eggs */
    int i_kc_offset;

    VLCVarChoiceModel* m_extraInterfaces;

public slots:
    void toggleUpdateSystrayMenu();
    void showUpdateSystrayMenu();
    void hideUpdateSystrayMenu();
    void toggleInterfaceFullScreen();
    void setPlaylistDocked( bool );
    void setPlaylistVisible( bool );
    void setInterfaceAlwaysOnTop( bool );
    void setShowRemainingTime( bool );

    void emitBoss();
    void emitRaise();
    void emitShow();
    void popupMenu( bool show );

    virtual void reloadPrefs();
    VLCVarChoiceModel* getExtraInterfaces();

protected slots:
    void handleSystrayClick( QSystemTrayIcon::ActivationReason );
    void updateSystrayTooltipName( const QString& );
    void updateSystrayTooltipStatus( PlayerController::PlayingState );

    void showBuffering( float );

    /* Manage the Video Functions from the vout threads */
    void setVideoSize(unsigned int w, unsigned int h);
    virtual void setVideoFullScreen( bool );
    void setVideoOnTop( bool );
    void setBoss();
    void setRaise();
    void setFullScreen( bool );
    void onInputChanged( bool );
    void updateIntfScaleFactor();

    void sendHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers );

signals:
    void askVideoToResize( unsigned int, unsigned int );
    void askVideoSetFullScreen( bool );
    void askVideoOnTop( bool );
    void minimalViewToggled( bool );
    void fullscreenInterfaceToggled( bool );
    void askToQuit();
    void askShow();
    void askBoss();
    void askRaise();
    void askPopupMenu( bool show );
    void kc_pressed(); /* easter eggs */

    void playlistDockedChanged(bool);
    void playlistVisibleChanged(bool);
    void interfaceAlwaysOnTopChanged(bool);
    void interfaceFullScreenChanged(bool);
    void hasEmbededVideoChanged(bool);
    void toolBarConfUpdated();
    void showRemainingTimeChanged(bool);

    void intfScaleFactorChanged();
};

#endif
