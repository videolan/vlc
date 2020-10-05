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
#include "util/color_scheme_model.hpp"

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

class WindowStateHolder : public QObject
{
public:
    enum Source {
        INTERFACE = 1,
        VIDEO = 2,
    };

    static bool holdFullscreen( QWindow* window, Source source, bool hold )
    {
        QVariant prop = window->property("__windowFullScreen");
        bool ok = false;
        unsigned fullscreenCounter = prop.toUInt(&ok);
        if (!ok)
            fullscreenCounter = 0;

        if (hold)
            fullscreenCounter |= source;
        else
            fullscreenCounter &= ~source;

        Qt::WindowStates oldflags = window->windowStates();
        Qt::WindowStates newflags;

        if( fullscreenCounter != 0 )
            newflags = oldflags | Qt::WindowFullScreen;
        else
            newflags = oldflags & ~Qt::WindowFullScreen;

        if( newflags != oldflags )
        {
            window->setWindowStates( newflags );
        }

        window->setProperty("__windowFullScreen", QVariant::fromValue(fullscreenCounter));

        return fullscreenCounter != 0;
    }


    static bool holdOnTop( QWindow* window, Source source, bool hold )
    {
        QVariant prop = window->property("__windowOnTop");
        bool ok = false;
        unsigned onTopCounter = prop.toUInt(&ok);
        if (!ok)
            onTopCounter = 0;

        if (hold)
            onTopCounter |= source;
        else
            onTopCounter &= ~source;

        Qt::WindowStates oldStates = window->windowStates();
        Qt::WindowFlags oldflags = window->flags();
        Qt::WindowFlags newflags;

        if( onTopCounter != 0 )
            newflags = oldflags | Qt::WindowStaysOnTopHint;
        else
            newflags = oldflags & ~Qt::WindowStaysOnTopHint;
        if( newflags != oldflags )
        {

            window->setFlags( newflags );
            window->show(); /* necessary to apply window flags */
            //workaround: removing onTop state might drop fullscreen state
            window->setWindowStates(oldStates);
        }

        window->setProperty("__windowOnTop", QVariant::fromValue(onTopCounter));

        return onTopCounter != 0;
    }

};

class MainInterface : public QVLCMW
{
    Q_OBJECT

    Q_PROPERTY(bool playlistDocked READ isPlaylistDocked WRITE setPlaylistDocked NOTIFY playlistDockedChanged)
    Q_PROPERTY(bool playlistVisible READ isPlaylistVisible WRITE setPlaylistVisible NOTIFY playlistVisibleChanged)
    Q_PROPERTY(double playlistWidthFactor READ getPlaylistWidthFactor WRITE setPlaylistWidthFactor NOTIFY playlistWidthFactorChanged)
    Q_PROPERTY(bool interfaceAlwaysOnTop READ isInterfaceAlwaysOnTop WRITE setInterfaceAlwaysOnTop NOTIFY interfaceAlwaysOnTopChanged)
    Q_PROPERTY(bool interfaceFullScreen READ isInterfaceFullScreen WRITE setInterfaceFullScreen NOTIFY interfaceFullScreenChanged)
    Q_PROPERTY(bool hasEmbededVideo READ hasEmbededVideo NOTIFY hasEmbededVideoChanged)
    Q_PROPERTY(bool showRemainingTime READ isShowRemainingTime WRITE setShowRemainingTime NOTIFY showRemainingTimeChanged)
    Q_PROPERTY(VLCVarChoiceModel* extraInterfaces READ getExtraInterfaces CONSTANT)
    Q_PROPERTY(float intfScaleFactor READ getIntfScaleFactor NOTIFY intfScaleFactorChanged)
    Q_PROPERTY(bool mediaLibraryAvailable READ hasMediaLibrary CONSTANT)
    Q_PROPERTY(bool gridView READ hasGridView WRITE setGridView NOTIFY gridViewChanged)
    Q_PROPERTY(ColorSchemeModel* colorScheme READ getColorScheme CONSTANT)
    Q_PROPERTY(bool hasVLM READ hasVLM CONSTANT)
    Q_PROPERTY(bool clientSideDecoration READ useClientSideDecoration NOTIFY useClientSideDecorationChanged)

public:
    /* tors */
    MainInterface( intf_thread_t *, QWidget *parent = nullptr, Qt::WindowFlags flags = Qt::WindowFlags());
    virtual ~MainInterface();

    static const QEvent::Type ToolbarsNeedRebuild;

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
    inline double getPlaylistWidthFactor() const { return playlistWidthFactor; }
    bool isInterfaceAlwaysOnTop() { return b_interfaceOnTop; }
    inline bool isHideAfterCreation() const { return b_hideAfterCreation; }
    inline bool isShowRemainingTime() const  { return m_showRemainingTime; }
    inline float getIntfScaleFactor() const { return m_intfScaleFactor; }
    inline bool hasMediaLibrary() const { return b_hasMedialibrary; }
    inline bool hasGridView() const { return m_gridView; }
    inline ColorSchemeModel* getColorScheme() const { return m_colorScheme; }
    bool hasVLM() const;
    bool useClientSideDecoration() const;

    bool hasEmbededVideo() const;
    VideoSurfaceProvider* getVideoSurfaceProvider() const;
    void setVideoSurfaceProvider(VideoSurfaceProvider* videoSurfaceProvider);;

protected:
    void dropEventPlay( QDropEvent* event, bool b_play );
    void dropEvent( QDropEvent *) Q_DECL_OVERRIDE;
    void dragEnterEvent( QDragEnterEvent * ) Q_DECL_OVERRIDE;
    void dragMoveEvent( QDragMoveEvent * ) Q_DECL_OVERRIDE;
    void dragLeaveEvent( QDragLeaveEvent * ) Q_DECL_OVERRIDE;
    void closeEvent( QCloseEvent *) Q_DECL_OVERRIDE;

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
    bool                 b_hideAfterCreation;
    bool                 b_minimalView;         ///< Minimal video
    bool                 b_playlistDocked;
    bool                 b_interfaceFullScreen;
    bool                 b_interfaceOnTop;      ///keep UI on top
#ifdef QT5_HAS_WAYLAND
    bool                 b_hasWayland;
#endif
    bool                 b_hasMedialibrary;
    bool                 m_gridView;
    ColorSchemeModel*    m_colorScheme;
    bool                 m_clientSideDecoration = false;

    /* States */
    bool                 playlistVisible;       ///< Is the playlist visible ?
    double               playlistWidthFactor;   ///< playlist size: root.width / playlistScaleFactor


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
    void setPlaylistWidthFactor( double );
    void setInterfaceAlwaysOnTop( bool );
    void setShowRemainingTime( bool );
    void setGridView( bool );
    void incrementIntfUserScaleFactor( bool increment);

    void emitBoss();
    void emitRaise();
    void emitShow();

    virtual void reloadPrefs();
    VLCVarChoiceModel* getExtraInterfaces();

protected slots:
    void handleSystrayClick( QSystemTrayIcon::ActivationReason );
    void updateSystrayTooltipName( const QString& );
    void updateSystrayTooltipStatus( PlayerController::PlayingState );

    void showBuffering( float );

    void onInputChanged( bool );
    void updateIntfScaleFactor();

    void sendHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers );

signals:
    void minimalViewToggled( bool );
    void fullscreenInterfaceToggled( bool );
    void setInterfaceVisibible(bool );
    void toggleWindowVisibility();
    void askToQuit();
    void askShow();
    void askBoss();
    void askRaise();
    void kc_pressed(); /* easter eggs */

    void playlistDockedChanged(bool);
    void playlistVisibleChanged(bool);
    void playlistWidthFactorChanged(double);
    void interfaceAlwaysOnTopChanged(bool);
    void interfaceFullScreenChanged(bool);
    void hasEmbededVideoChanged(bool);
    void toolBarConfUpdated();
    void showRemainingTimeChanged(bool);
    void gridViewChanged( bool );
    void colorSchemeChanged( QString );
    void useClientSideDecorationChanged();

    /// forward window maximise query to the actual window or widget
    void requestInterfaceMaximized();
    /// forward window normal query to the actual window or widget
    void requestInterfaceNormal();

    void intfScaleFactorChanged();
};

#endif
