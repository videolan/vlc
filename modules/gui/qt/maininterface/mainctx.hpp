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
#include "medialibrary/medialib.hpp"
#include <playlist/playlist_common.hpp>

#include <QSystemTrayIcon>
#include <QtQuick/QQuickView>
#include <QApplication>

#ifdef _WIN32
# include <shobjidl.h>
#endif

#include <atomic>

Q_MOC_INCLUDE( "dialogs/toolbar/controlbar_profile_model.hpp" )
Q_MOC_INCLUDE( "util/csdbuttonmodel.hpp" )
Q_MOC_INCLUDE( "playlist/playlist_controller.hpp" )
Q_MOC_INCLUDE( "maininterface/mainctx_submodels.hpp" )

class CSDButtonModel;
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
struct vlc_window;
class VideoSurfaceProvider;
class ControlbarProfileModel;
class SearchCtx;
class SortCtx;

namespace vlc {
namespace playlist {
}
}

class WindowStateHolder : public QObject
{
public:
    enum Source {
        INTERFACE = 1,
        VIDEO = 2,
    };

    static bool holdFullscreen( QWindow* window, Source source, bool hold );


    static bool holdOnTop( QWindow* window, Source source, bool hold );

};

class MainCtx : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool mediaLibraryVisible READ isMediaLibraryVisible WRITE setMediaLibraryVisible
               NOTIFY mediaLibraryVisibleChanged FINAL)
    Q_PROPERTY(bool playlistDocked READ isPlaylistDocked WRITE setPlaylistDocked NOTIFY playlistDockedChanged FINAL)
    Q_PROPERTY(bool playlistVisible READ isPlaylistVisible WRITE setPlaylistVisible NOTIFY playlistVisibleChanged FINAL)
    Q_PROPERTY(double playlistWidthFactor READ getPlaylistWidthFactor WRITE setPlaylistWidthFactor NOTIFY playlistWidthFactorChanged FINAL)
    Q_PROPERTY(double playerPlaylistWidthFactor READ getPlayerPlaylistWidthFactor WRITE setPlayerPlaylistWidthFactor NOTIFY playerPlaylistFactorChanged FINAL)
    Q_PROPERTY(bool interfaceAlwaysOnTop READ isInterfaceAlwaysOnTop WRITE setInterfaceAlwaysOnTop NOTIFY interfaceAlwaysOnTopChanged FINAL)
    Q_PROPERTY(bool hasEmbededVideo READ hasEmbededVideo NOTIFY hasEmbededVideoChanged FINAL)
    Q_PROPERTY(bool showRemainingTime READ isShowRemainingTime WRITE setShowRemainingTime NOTIFY showRemainingTimeChanged FINAL)
    Q_PROPERTY(VLCVarChoiceModel* extraInterfaces READ getExtraInterfaces CONSTANT FINAL)
    Q_PROPERTY(double intfScaleFactor READ getIntfScaleFactor NOTIFY intfScaleFactorChanged FINAL)
    Q_PROPERTY(bool mediaLibraryAvailable READ hasMediaLibrary CONSTANT FINAL)
    Q_PROPERTY(MediaLib* mediaLibrary READ getMediaLibrary CONSTANT FINAL)
    Q_PROPERTY(bool gridView READ hasGridView WRITE setGridView NOTIFY gridViewChanged FINAL)
    Q_PROPERTY(bool hasGridListMode MEMBER m_hasGridListMode NOTIFY hasGridListModeChanged FINAL)
    Q_PROPERTY(Grouping grouping READ grouping WRITE setGrouping NOTIFY groupingChanged FINAL)
    Q_PROPERTY(ColorSchemeModel* colorScheme READ getColorScheme CONSTANT FINAL)
    Q_PROPERTY(bool hasVLM READ hasVLM CONSTANT FINAL)
    Q_PROPERTY(bool clientSideDecoration READ useClientSideDecoration NOTIFY useClientSideDecorationChanged FINAL)
    Q_PROPERTY(bool hasFirstrun READ hasFirstrun CONSTANT FINAL)
    Q_PROPERTY(int  csdBorderSize READ CSDBorderSize NOTIFY useClientSideDecorationChanged FINAL)
    Q_PROPERTY(bool hasToolbarMenu READ hasToolbarMenu WRITE setHasToolbarMenu NOTIFY hasToolbarMenuChanged FINAL)
    Q_PROPERTY(bool canShowVideoPIP READ canShowVideoPIP CONSTANT FINAL)
    Q_PROPERTY(bool pinVideoControls READ pinVideoControls WRITE setPinVideoControls NOTIFY pinVideoControlsChanged FINAL)
    Q_PROPERTY(float pinOpacity READ pinOpacity WRITE setPinOpacity NOTIFY pinOpacityChanged FINAL)
    Q_PROPERTY(ControlbarProfileModel* controlbarProfileModel READ controlbarProfileModel CONSTANT FINAL)
    Q_PROPERTY(bool hasAcrylicSurface READ hasAcrylicSurface NOTIFY hasAcrylicSurfaceChanged FINAL)
    Q_PROPERTY(bool smoothScroll READ smoothScroll NOTIFY smoothScrollChanged FINAL)
    Q_PROPERTY(QWindow* intfMainWindow READ intfMainWindow CONSTANT FINAL)
    Q_PROPERTY(QScreen* screen READ screen NOTIFY screenChanged)
    Q_PROPERTY(bool useGlobalShortcuts READ getUseGlobalShortcuts WRITE setUseGlobalShortcuts NOTIFY useGlobalShortcutsChanged FINAL)
    Q_PROPERTY(int maxVolume READ maxVolume NOTIFY maxVolumeChanged FINAL)
    Q_PROPERTY(float safeArea READ safeArea NOTIFY safeAreaChanged FINAL)

    Q_PROPERTY(CSDButtonModel *csdButtonModel READ csdButtonModel CONSTANT FINAL)

    //Property to get Operating System info
    Q_PROPERTY(OsType osName READ getOSName CONSTANT)
    Q_PROPERTY(int osVersion READ getOSVersion CONSTANT)

    // This Property only works if hasAcrylicSurface is set
    Q_PROPERTY(bool acrylicActive READ acrylicActive WRITE setAcrylicActive NOTIFY acrylicActiveChanged FINAL)

    // NOTE: This is useful when we want to prioritize player hotkeys over QML keyboard navigation.
    Q_PROPERTY(bool preferHotkeys READ preferHotkeys WRITE setPreferHotkeys NOTIFY preferHotkeysChanged FINAL)

    Q_PROPERTY(bool windowSuportExtendedFrame READ windowSuportExtendedFrame NOTIFY windowSuportExtendedFrameChanged)
    Q_PROPERTY(unsigned windowExtendedMargin READ windowExtendedMargin WRITE setWindowExtendedMargin NOTIFY windowExtendedMarginChanged)
    Q_PROPERTY(SearchCtx* search MEMBER m_search CONSTANT FINAL)
    Q_PROPERTY(SortCtx* sort MEMBER m_sort CONSTANT FINAL)

public:
    /* tors */
    MainCtx(qt_intf_t *);
    virtual ~MainCtx();

    static const QEvent::Type ToolbarsNeedRebuild;
    static constexpr double MIN_INTF_USER_SCALE_FACTOR = 0.3;
    static constexpr double MAX_INTF_USER_SCALE_FACTOR = 3.0;

public:
    /* Getters */
    inline qt_intf_t* getIntf() const { return p_intf; }
    bool smoothScroll() const { return m_smoothScroll; }

    QSystemTrayIcon *getSysTray() { return sysTray; }
    QMenu *getSysTrayMenu() { return systrayMenu.get(); }
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

    enum Grouping
    {
        GROUPING_NONE,
        GROUPING_NAME,
        GROUPING_FOLDER
    };
    Q_ENUM(Grouping)

    enum OsType
    {
        Windows,
        Unknown
    };
    Q_ENUM(OsType)

    inline bool isInterfaceFullScreen() const { return m_windowVisibility == QWindow::FullScreen; }
    inline bool isInterfaceVisible() const { return m_windowVisibility != QWindow::Hidden; }
    bool isMediaLibraryVisible() { return m_mediaLibraryVisible; }
    bool isPlaylistDocked() { return b_playlistDocked; }
    bool isPlaylistVisible() { return m_playlistVisible; }
    inline double getPlaylistWidthFactor() const { return m_playlistWidthFactor; }
    inline double getPlayerPlaylistWidthFactor() const { return m_playerPlaylistWidthFactor; }
    bool isInterfaceAlwaysOnTop() { return b_interfaceOnTop; }
    inline bool isHideAfterCreation() const { return b_hideAfterCreation; }
    inline bool isShowRemainingTime() const  { return m_showRemainingTime; }
    inline double getIntfScaleFactor() const { return m_intfScaleFactor; }
    inline double getIntfUserScaleFactor() const { return m_intfUserScaleFactor; }
    inline int CSDBorderSize() const { return 5 * getIntfScaleFactor(); }
    inline double getMinIntfUserScaleFactor() const { return MIN_INTF_USER_SCALE_FACTOR; }
    inline double getMaxIntfUserScaleFactor() const { return MAX_INTF_USER_SCALE_FACTOR; }
    inline bool hasMediaLibrary() const { return b_hasMedialibrary; }
    inline MediaLib* getMediaLibrary() const { return m_medialib; }
    inline bool hasGridView() const { return m_gridView; }
    inline Grouping grouping() const { return m_grouping; }
    inline ColorSchemeModel* getColorScheme() const { return m_colorScheme; }
    bool hasVLM() const;
    bool useClientSideDecoration() const;
    bool hasFirstrun() const;
    inline bool hasToolbarMenu() const { return m_hasToolbarMenu; }
    inline bool canShowVideoPIP() const { return m_canShowVideoPIP; }
    inline void setCanShowVideoPIP(bool canShowVideoPIP) { m_canShowVideoPIP = canShowVideoPIP; }

    inline bool pinVideoControls() const { return m_pinVideoControls; }
    inline float pinOpacity() const { return m_pinOpacity; }

    inline ControlbarProfileModel* controlbarProfileModel() const { return m_controlbarProfileModel; }
    inline QUrl getDialogFilePath() const { return m_dialogFilepath; }
    inline void setDialogFilePath(const QUrl& filepath ){ m_dialogFilepath = filepath; }
    inline bool hasAcrylicSurface() const { return m_hasAcrylicSurface; }
    inline void reloadFromSettings() { loadFromSettingsImpl(true); }
    inline QScreen* screen() const { return intfMainWindow()->screen(); }
    inline bool getUseGlobalShortcuts() const { return m_useGlobalShortcuts; }
    void setUseGlobalShortcuts(bool useGlobalShortcuts );
    inline int maxVolume() const { return m_maxVolume; }

    inline float safeArea() const { return m_safeArea; }

    inline OsType getOSName() const {return m_osName;}
    inline int getOSVersion() const {return m_osVersion;}

    inline bool windowSuportExtendedFrame() const { return m_windowSuportExtendedFrame; }
    inline unsigned windowExtendedMargin() const { return m_windowExtendedMargin; }
    void setWindowSuportExtendedFrame(bool support);
    void setWindowExtendedMargin(unsigned margin);

    bool hasEmbededVideo() const;
    VideoSurfaceProvider* getVideoSurfaceProvider() const;
    void setVideoSurfaceProvider(VideoSurfaceProvider* videoSurfaceProvider);

    Q_INVOKABLE static inline void setCursor(Qt::CursorShape cursor) { QApplication::setOverrideCursor(QCursor(cursor)); }
    Q_INVOKABLE static inline void restoreCursor(void) { QApplication::restoreOverrideCursor(); }

    Q_INVOKABLE static /*constexpr*/ inline unsigned int qtVersion() { return QT_VERSION; }
    Q_INVOKABLE static /*constexpr*/ inline unsigned int qtVersionCheck(unsigned char major,
                                                                        unsigned char minor,
                                                                        unsigned char patch)
                                                                       { return QT_VERSION_CHECK(major, minor, patch); }

    /**
     * @brief ask for the application to terminate
     * @return true if the application can be close right away, false if it will be delayed
     */
    bool onWindowClose(QWindow* );

    bool acrylicActive() const;
    void setAcrylicActive(bool newAcrylicActive);

    bool preferHotkeys() const;
    void setPreferHotkeys(bool enable);

    QWindow *intfMainWindow() const;

    Q_INVOKABLE QVariant settingValue(const QString &key, const QVariant &defaultValue) const;
    Q_INVOKABLE void setSettingValue(const QString &key, const QVariant &value);

    Q_INVOKABLE static void setAttachedToolTip(QObject* toolTip);

    CSDButtonModel *csdButtonModel() { return m_csdButtonModel.get(); }

    Q_INVOKABLE static double dp(const double px, const double scale);
    Q_INVOKABLE double dp(const double px) const;

    Q_INVOKABLE bool useXmasCone() const;

protected:
    /* Systray */
    void createSystray();
    void initSystray();
    void handleSystray();

    qt_intf_t* p_intf = nullptr;

    bool m_hasEmbededVideo = false;
    VideoSurfaceProvider* m_videoSurfaceProvider = nullptr;
    bool m_showRemainingTime = false;

    /* */
    QSettings           *settings = nullptr;
    QSystemTrayIcon     *sysTray = nullptr;
    std::unique_ptr<QMenu> systrayMenu;

    /* Flags */
    double               m_intfUserScaleFactor = 1.;
    double               m_intfScaleFactor = 1.;
    int                  i_notificationSetting = 0; /// Systray Notifications
    bool                 b_hideAfterCreation = false; /// --qt-start-minimized
    bool                 b_minimalView = false;         ///< Minimal video
    bool                 b_playlistDocked = false;
    QWindow::Visibility  m_windowVisibility = QWindow::Windowed;
    bool                 b_interfaceOnTop = false;      ///keep UI on top
#ifdef QT_HAS_WAYLAND
    bool                 b_hasWayland = false;
#endif
    bool                 b_hasMedialibrary = false;
    MediaLib*            m_medialib = nullptr;
    bool                 m_gridView = false;
    bool                 m_hasGridListMode = false;
    Grouping             m_grouping = GROUPING_NONE;
    ColorSchemeModel*    m_colorScheme = nullptr;
    bool                 m_windowTitlebar = true;
    // NOTE: Ideally this should be a QVLCBool.
    bool                 m_hasToolbarMenu = false;
    bool                 m_canShowVideoPIP = false;

    /* Pinned */
    bool                 m_pinVideoControls = false;
    float                m_pinOpacity       = 1.0f;

    bool                 m_useGlobalShortcuts = true;
    QUrl                 m_dialogFilepath; /* Last path used in dialogs */

    /* States */
    bool                 m_mediaLibraryVisible = true;
    bool                 m_playlistVisible = false;       ///< Is the playlist visible ?
    double               m_playlistWidthFactor = 4.;   ///< playlist size: root.width / playlistScaleFactor
    double               m_playerPlaylistWidthFactor = 4.;

    VLCVarChoiceModel* m_extraInterfaces = nullptr;

    ControlbarProfileModel* m_controlbarProfileModel = nullptr;

    bool m_hasAcrylicSurface = false;
    bool m_acrylicActive = false;

    bool m_smoothScroll = true;

    bool m_preferHotkeys = false;

    int m_maxVolume = 125;

    float m_safeArea = 0.0;

    OsType m_osName;
    int m_osVersion;

    bool m_windowSuportExtendedFrame = false;
    unsigned m_windowExtendedMargin = 0;

    std::unique_ptr<CSDButtonModel> m_csdButtonModel;

    SearchCtx* m_search = nullptr;
    SortCtx* m_sort = nullptr;

public slots:
    void toggleUpdateSystrayMenu();
    void showUpdateSystrayMenu();
    void hideUpdateSystrayMenu();
    void toggleToolbarMenu();
    void toggleInterfaceFullScreen();
    void setMediaLibraryVisible( bool );
    void setPlaylistDocked( bool );
    void setPlaylistVisible( bool );
    void setPlaylistWidthFactor( double );
    void setPlayerPlaylistWidthFactor( double factor );
    void setInterfaceAlwaysOnTop( bool );
    void setShowRemainingTime( bool );
    void setGridView( bool );
    void setGrouping( Grouping );
    void incrementIntfUserScaleFactor( bool increment);
    void setIntfUserScaleFactor( double );
    void setHasToolbarMenu( bool );

    void setPinVideoControls( bool );
    void setPinOpacity( float );

    void updateIntfScaleFactor();
    void onWindowVisibilityChanged(QWindow::Visibility);
    void setHasAcrylicSurface(bool);

    void sendHotkey(Qt::Key key, Qt::KeyboardModifiers modifiers );

    void emitBoss();
    void emitRaise();
    void emitShow();

    virtual void reloadPrefs();
    VLCVarChoiceModel* getExtraInterfaces();

protected slots:
    void handleSystrayClick( QSystemTrayIcon::ActivationReason );
    void updateSystrayTooltipName( const QString& );
    void updateSystrayTooltipStatus( PlayerController::PlayingState );

    void onInputChanged( bool );

signals:
    void minimalViewToggled( bool );
    void fullscreenInterfaceToggled( bool );
    void setInterfaceVisibible(bool );
    void setInterfaceFullScreen( bool );
    void toggleWindowVisibility();
    void askToQuit();
    void askShow();
    void askBoss();
    void askRaise();
    void kc_pressed(); /* easter eggs */

    void mediaLibraryVisibleChanged(bool);
    void playlistDockedChanged(bool);
    void playlistVisibleChanged(bool);
    void playlistWidthFactorChanged(double);
    void playerPlaylistFactorChanged(double);
    void interfaceAlwaysOnTopChanged(bool);
    void hasEmbededVideoChanged(bool);
    void showRemainingTimeChanged(bool);
    void gridViewChanged( bool );
    void hasGridListModeChanged( bool );
    void groupingChanged( Grouping );
    void colorSchemeChanged( QString );
    void useClientSideDecorationChanged();
    void hasToolbarMenuChanged();

    /// forward window maximise query to the actual window or widget
    void requestInterfaceMaximized();
    /// forward window normal query to the actual window or widget
    void requestInterfaceNormal();
    /// forward window normal query to the actual window or widget
    void requestInterfaceMinimized();

    void intfScaleFactorChanged();

    void pinVideoControlsChanged();
    void pinOpacityChanged();

    void hasAcrylicSurfaceChanged();

    void acrylicActiveChanged();

    void smoothScrollChanged();

    void preferHotkeysChanged();

    void screenChanged();

    void useGlobalShortcutsChanged( bool );

    void maxVolumeChanged();

    void safeAreaChanged();

    void navBoxToggled();

    void windowSuportExtendedFrameChanged();
    void windowExtendedMarginChanged(unsigned margin);

private:
    void loadPrefs(bool callSignals);
    void loadFromSettingsImpl(bool callSignals);
};

#endif
