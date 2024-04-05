/*****************************************************************************
 * qt.cpp : Qt interface
 ****************************************************************************
 * Copyright © 2006-2009 the VideoLAN team
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

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <qconfig.h>
#include <QtPlugin>
#include <QQuickStyle>

QT_BEGIN_NAMESPACE
#include "plugins.hpp"
QT_END_NAMESPACE

#define VLC_MODULE_LICENSE VLC_LICENSE_GPL_2_PLUS

#include <stdlib.h>
#include <unistd.h>
#ifndef _POSIX_SPAWN
# define _POSIX_SPAWN (-1)
#endif
#if (_POSIX_SPAWN >= 0)
# include <spawn.h>
# include <sys/wait.h>

extern "C" char **environ;
#endif

#include <QApplication>
#include <QDate>
#include <QMutex>
#include <QtQuickControls2/QQuickStyle>
#include <QLoggingCategory>
#include <QQmlError>
#include <QList>
#include <QTranslator>

#include "qt.hpp"

#include "player/player_controller.hpp"    /* THEMIM destruction */
#include "playlist/playlist_controller.hpp" /* THEMPL creation */
#include "dialogs/dialogs_provider.hpp" /* THEDP creation */
#include "dialogs/dialogs/dialogmodel.hpp"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
# include "maininterface/mainctx_win32.hpp"
#else
# include "maininterface/mainctx.hpp"   /* MainCtx creation */
#endif
#include "style/defaultthemeproviders.hpp"
#include "dialogs/extensions/extensions_manager.hpp" /* Extensions manager */
#include "dialogs/plugins/addons_manager.hpp" /* Addons manager */
#include "dialogs/help/help.hpp"     /* Launch Update */
#include "util/dismiss_popup_event_filter.hpp"
#include "maininterface/compositor.hpp"
#include "util/vlctick.hpp"
#include "util/shared_input_item.hpp"
#include "network/networkmediamodel.hpp"
#include "playlist/playlist_common.hpp"
#include "playlist/playlist_item.hpp"
#include "dialogs/dialogs/dialogmodel.hpp"
#include "medialibrary/mlqmltypes.hpp"

#include <QVector>
#include "playlist/playlist_item.hpp"

#include <vlc_interface.h>
#include <vlc_plugin.h>
#include <vlc_window.h>
#include <vlc_player.h>
#include <vlc_threads.h>

#include <QQuickWindow>

#ifndef X_DISPLAY_MISSING
# include <vlc_xlib.h>
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  OpenInternal ( qt_intf_t * );
static void CloseInternal( qt_intf_t * );
static int  OpenIntf     ( vlc_object_t * );
static int  OpenDialogs  ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static int  WindowOpen   ( vlc_window_t * );
static void ShowDialog   ( intf_thread_t *, int, int, intf_dialog_args_t * );


/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define INITIAL_PREFS_VIEW_TEXT N_( "Select the initial preferences view" )
#define INITIAL_PREFS_VIEW_LONGTEXT N_( "Select which preferences view to show upon "\
                                        "opening the preferences dialog." )

#define SYSTRAY_TEXT N_( "Systray icon" )
#define SYSTRAY_LONGTEXT N_( "Show an icon in the systray " \
                             "allowing you to control VLC media player " \
                             "for basic actions." )

#define MINIMIZED_TEXT N_( "Start VLC with only a systray icon" )
#define MINIMIZED_LONGTEXT N_( "VLC will start with just an icon in " \
                               "your taskbar." )

#define KEEPSIZE_TEXT N_( "Resize interface to the native video size" )
#define KEEPSIZE_LONGTEXT N_( "You have two choices:\n" \
            " - The interface will resize to the native video size\n" \
            " - The video will fit to the interface size\n " \
            "By default, interface resize to the native video size." )

#define TITLE_TEXT N_( "Show playing item name in window title" )
#define TITLE_LONGTEXT N_( "Show the name of the song or video in the " \
                           "controller window title." )

#define NOTIFICATION_TEXT N_( "Show notification popup on track change" )
#define NOTIFICATION_LONGTEXT N_( \
    "Show a notification popup with the artist and track name when " \
    "the current playlist item changes, when VLC is minimized or hidden." )

#define OPACITY_TEXT N_( "Windows opacity between 0.1 and 1" )
#define OPACITY_LONGTEXT N_( "Sets the windows opacity between 0.1 and 1 " \
                             "for main interface, playlist and extended panel."\
                             " This option only works with Windows and " \
                             "X11 with composite extensions." )

#define OPACITY_FS_TEXT N_( "Fullscreen controller opacity between 0.1 and 1" )
#define OPACITY_FS_LONGTEXT N_( "Sets the fullscreen controller opacity between 0.1 and 1 " \
                             "for main interface, playlist and extended panel."\
                             " This option only works with Windows and " \
                             "X11 with composite extensions." )

#define INTERFACE_SCALE_TEXT N_( "Initial user scale factor for the interface, between 0.3 and 3.0" )

#define ERROR_TEXT N_( "Show unimportant error and warnings dialogs" )

#define UPDATER_TEXT N_( "Activate the updates availability notification" )
#define UPDATER_LONGTEXT N_( "Activate the automatic notification of new " \
                            "versions of the software. It runs once every " \
                            "two weeks." )

#define QT_QML_DEBUG_TEXT N_( "set the options for qml debugger" )
#define QT_QML_DEBUG_LONGTEXT N_( "set the options for qml debugger (see http://doc.qt.io/qt-5/qtquick-debugging.html#starting-applications)" )

#define UPDATER_DAYS_TEXT N_("Number of days between two update checks")

#define PRIVACY_TEXT N_( "Ask for network policy at start" )

#define RECENTPLAY_TEXT N_( "Save the recently played items in the menu" )

#define RECENTPLAY_FILTER_TEXT N_( "List of words separated by | to filter" )
#define RECENTPLAY_FILTER_LONGTEXT N_( "Regular expression used to filter " \
        "the recent items played in the player." )

#define QT_MODE_TEXT N_( "Selection of the starting mode and look" )
#define QT_MODE_LONGTEXT N_( "Start VLC with:\n" \
                             " - normal mode\n"  \
                             " - a zone always present to show information " \
                                  "as lyrics, album arts...\n" \
                             " - minimal mode with limited controls" )

#define QT_FULLSCREEN_TEXT N_( "Show a controller in fullscreen mode" )
#define QT_NATIVEOPEN_TEXT N_( "Embed the file browser in open dialog" )

#define FULLSCREEN_NUMBER_TEXT N_( "Define which screen fullscreen goes" )
#define FULLSCREEN_NUMBER_LONGTEXT N_( "Screennumber of fullscreen, instead of " \
                                       "same screen where interface is." )

#define QT_AUTOLOAD_EXTENSIONS_TEXT N_( "Load extensions on startup" )
#define QT_AUTOLOAD_EXTENSIONS_LONGTEXT N_( "Automatically load the "\
                                            "extensions module on startup." )

#define QT_MINIMAL_MODE_TEXT N_("Start in minimal view (without menus)" )

#define QT_BGCONE_TEXT N_( "Display background cone or art" )
#define QT_BGCONE_LONGTEXT N_( "Display background cone or current album art " \
                            "when not playing. " \
                            "Can be disabled to prevent burning screen." )
#define QT_BGCONE_EXPANDS_TEXT N_( "Expanding background cone or art" )
#define QT_BGCONE_EXPANDS_LONGTEXT N_( "Background art fits window's size." )

#define QT_DISABLE_VOLUME_KEYS_TEXT N_( "Ignore keyboard volume buttons." )
#define QT_DISABLE_VOLUME_KEYS_LONGTEXT N_(                                             \
    "With this option checked, the volume up, volume down and mute buttons on your "    \
    "keyboard will always change your system volume. With this option unchecked, the "  \
    "volume buttons will change VLC's volume when VLC is selected and change the "      \
    "system volume when VLC is not selected." )

#define QT_PAUSE_MINIMIZED_TEXT N_( "Pause the video playback when minimized" )
#define QT_PAUSE_MINIMIZED_LONGTEXT N_( \
    "With this option enabled, the playback will be automatically paused when minimizing the window." )

#define ICONCHANGE_TEXT N_( "Allow automatic icon changes")
#define ICONCHANGE_LONGTEXT N_( \
    "This option allows the interface to change its icon on various occasions.")

#define VOLUME_MAX_TEXT N_( "Maximum Volume displayed" )

#define AUTORAISE_ON_PLAYBACK_TEXT N_( "When to raise the interface" )
#define AUTORAISE_ON_PLAYBACK_LONGTEXT N_( "This option allows the interface to be raised automatically " \
    "when a video/audio playback starts, or never." )

#define QT_CLIENT_SIDE_DECORATION_TEXT N_( "Enable window titlebar" )
#define QT_CLIENT_SIDE_DECORATION_LONGTEXT N_( "This option enables the title bar. Disabling it will remove " \
    "the titlebar and move window buttons within the interface (Client Side Decoration)" )


#define QT_MENUBAR_TEXT N_( "Show the menu bar" )
#define QT_MENUBAR_LONGTEXT N_( "This option displays the classic menu bar" )

#define QT_PIN_CONTROLS_TEXT N_("Pin video controls")
#define QT_PIN_CONTROLS_LONGTEXT N_("Place video controls above and below the video instead of above")

#define FULLSCREEN_CONTROL_PIXELS N_( "Fullscreen controller mouse sensitivity" )

#define QT_COMPOSITOR_TEXT N_("Select Qt video integration backend")
#define QT_COMPOSITOR_LONGTEXT N_("Select Qt video integration backend. Use with care, the interface may not start if an incompatible compositor is selected")

#define SMOOTH_SCROLLING_TEXT N_( "Use smooth scrolling in Flickable based views" )
#define SMOOTH_SCROLLING_LONGTEXT N_( "Deactivating this option will disable smooth scrolling in Flickable based views (such as the Playqueue)" )

#define SAFE_AREA_TEXT N_( "Safe area for the user interface" )
#define SAFE_AREA_LONGTEXT N_( "Sets the safe area percentage between 0.0 and 100 when you want " \
                               "to ensure the visibility of the user interface on a constrained " \
                               "viewport" )

static const int initial_prefs_view_list[] = { 0, 1, 2 };
static const char *const initial_prefs_view_list_texts[] =
    { N_("Simple"), N_("Advanced"), N_("Expert") };

static const int i_notification_list[] =
    { NOTIFICATION_NEVER, NOTIFICATION_MINIMIZED, NOTIFICATION_ALWAYS };

static const char *const psz_notification_list_text[] =
    { N_("Never"), N_("When minimized"), N_("Always") };

static const int i_raise_list[] =
    { MainCtx::RAISE_NEVER, MainCtx::RAISE_VIDEO, \
      MainCtx::RAISE_AUDIO, MainCtx::RAISE_AUDIOVIDEO,  };

static const char *const psz_raise_list_text[] =
    { N_( "Never" ), N_( "Video" ), N_( "Audio" ), _( "Audio/Video" ) };

static const char *const compositor_vlc[] = {
    "auto",
#ifdef _WIN32
#ifdef HAVE_DCOMP_H
    "dcomp",
#endif
    "win7",
#endif
#ifdef QT_HAS_WAYLAND_COMPOSITOR
    "wayland",
#endif
#ifdef QT_HAS_X11_COMPOSITOR
    "x11",
#endif
    "dummy"
};
static const char *const compositor_user[] = {
    N_("Automatic"),
#ifdef _WIN32
#ifdef HAVE_DCOMP_H
    "Direct Composition",
#endif
    "Windows 7",
#endif
#ifdef QT_HAS_WAYLAND_COMPOSITOR
    "Wayland",
#endif
#ifdef QT_HAS_X11_COMPOSITOR
    N_("X11"),
#endif
    N_("Dummy"),
};

/**********************************************************************/
vlc_module_begin ()
    set_shortname( "Qt" )
    set_description( N_("Qt interface") )
    set_subcategory( SUBCAT_INTERFACE_MAIN )
    set_capability( "interface", 151 )
    set_callbacks( OpenIntf, Close )

    add_shortcut("qt")

    add_bool( "qt-minimal-view", false, QT_MINIMAL_MODE_TEXT,
              nullptr )

    add_bool( "qt-system-tray", true, SYSTRAY_TEXT, SYSTRAY_LONGTEXT)

    add_integer( "qt-notification", NOTIFICATION_MINIMIZED,
                 NOTIFICATION_TEXT,
                 NOTIFICATION_LONGTEXT )
            change_integer_list( i_notification_list, psz_notification_list_text )

    add_bool( "qt-start-minimized", false, MINIMIZED_TEXT,
              MINIMIZED_LONGTEXT)
    add_bool( "qt-pause-minimized", false, QT_PAUSE_MINIMIZED_TEXT,
              QT_PAUSE_MINIMIZED_LONGTEXT )

    add_float_with_range( "qt-opacity", 1., 0.1, 1., OPACITY_TEXT,
                          OPACITY_LONGTEXT )
    add_float_with_range( "qt-fs-opacity", 0.8, 0.1, 1., OPACITY_FS_TEXT,
                          OPACITY_FS_LONGTEXT )

    //qt-interface-scale is stored in Qt config file
    //this option is here to force an initial scale factor at startup
    add_float_with_range( "qt-interface-scale", -1.0, 0.3, 3.0, INTERFACE_SCALE_TEXT,
                          nullptr )
        change_volatile()

    add_bool( "qt-video-autoresize", true, KEEPSIZE_TEXT,
              KEEPSIZE_LONGTEXT )
    add_bool( "qt-name-in-title", true, TITLE_TEXT,
              TITLE_LONGTEXT )
    add_bool( "qt-fs-controller", true, QT_FULLSCREEN_TEXT,
              nullptr )

    add_string("qt-compositor", "auto", QT_COMPOSITOR_TEXT, QT_COMPOSITOR_LONGTEXT)
            change_string_list(compositor_vlc, compositor_user)

    add_obsolete_bool( "qt-recentplay" )
    add_obsolete_string( "qt-recentplay-filter" )
    add_obsolete_integer( "qt-continue" )

#ifdef UPDATE_CHECK
    add_bool( "qt-updates-notif", true, UPDATER_TEXT,
              UPDATER_LONGTEXT )
    add_integer_with_range( "qt-updates-days", 3, 0, 180,
              UPDATER_DAYS_TEXT, nullptr )
#endif

#ifdef QT_QML_DEBUG
    add_string( "qt-qmljsdebugger", NULL,
                QT_QML_DEBUG_TEXT, QT_QML_DEBUG_LONGTEXT )
#endif

#ifdef _WIN32
    add_bool( "qt-disable-volume-keys"             /* name */,
              true                                 /* default value */,
              QT_DISABLE_VOLUME_KEYS_TEXT          /* text */,
              QT_DISABLE_VOLUME_KEYS_LONGTEXT      /* longtext */)
#endif

#if QT_CLIENT_SIDE_DECORATION_AVAILABLE
    add_bool( "qt-titlebar",
#ifdef _WIN32
              false                              /* use CSD by default on windows */,
#else
              true                               /* but not on linux */,
#endif
              QT_CLIENT_SIDE_DECORATION_TEXT, QT_CLIENT_SIDE_DECORATION_LONGTEXT )
#endif

    add_bool( "qt-menubar", false, QT_MENUBAR_TEXT, QT_MENUBAR_LONGTEXT )

    add_bool( "qt-embedded-open", false, QT_NATIVEOPEN_TEXT,
               nullptr )

    add_bool( "qt-pin-controls", false, QT_PIN_CONTROLS_TEXT, QT_PIN_CONTROLS_LONGTEXT )


    add_obsolete_bool( "qt-advanced-pref" ) /* since 4.0.0 */
    add_integer( "qt-initial-prefs-view", 0, INITIAL_PREFS_VIEW_TEXT, INITIAL_PREFS_VIEW_LONGTEXT )
        change_integer_range( 0, 2 )
        change_integer_list( initial_prefs_view_list, initial_prefs_view_list_texts )
    add_bool( "qt-error-dialogs", true, ERROR_TEXT,
              nullptr )

    add_obsolete_string( "qt-slider-colours")

    add_bool( "qt-privacy-ask", true, PRIVACY_TEXT, nullptr )
        change_private ()

    add_integer( "qt-fullscreen-screennumber", -1, FULLSCREEN_NUMBER_TEXT,
               FULLSCREEN_NUMBER_LONGTEXT )

    add_bool( "qt-autoload-extensions", true,
              QT_AUTOLOAD_EXTENSIONS_TEXT, QT_AUTOLOAD_EXTENSIONS_LONGTEXT )

    add_bool( "qt-bgcone", true, QT_BGCONE_TEXT, QT_BGCONE_LONGTEXT )
    add_bool( "qt-bgcone-expands", false, QT_BGCONE_EXPANDS_TEXT,
              QT_BGCONE_EXPANDS_LONGTEXT )

    add_bool( "qt-icon-change", true, ICONCHANGE_TEXT, ICONCHANGE_LONGTEXT )

    add_integer_with_range( "qt-max-volume", 125, 60, 300, VOLUME_MAX_TEXT, nullptr)

    add_integer_with_range( "qt-fs-sensitivity", 3, 0, 4000, FULLSCREEN_CONTROL_PIXELS,
            nullptr)

    add_integer( "qt-auto-raise", MainCtx::RAISE_VIDEO, AUTORAISE_ON_PLAYBACK_TEXT,
                 AUTORAISE_ON_PLAYBACK_LONGTEXT )
            change_integer_list( i_raise_list, psz_raise_list_text )

    add_bool( "qt-smooth-scrolling", true, SMOOTH_SCROLLING_TEXT, SMOOTH_SCROLLING_LONGTEXT )

    add_float_with_range( "qt-safe-area", 0, 0, 100.0, SAFE_AREA_TEXT, SAFE_AREA_LONGTEXT )

    cannot_unload_broken_library()

    add_submodule ()
        set_description( "Dialogs provider" )
        set_capability( "dialogs provider", 51 )

        set_callbacks( OpenDialogs, Close )

    add_submodule ()
        set_capability( "vout window", 0 )
        set_callback( WindowOpen )

#ifdef _WIN32
    add_submodule ()
        set_capability( "qt theme provider", 10 )
        set_callback( WindowsThemeProviderOpen )
        set_description( "Qt Windows theme" )
        add_shortcut("qt-themeprovider-windows")
#endif
    add_submodule()
        set_capability("qt theme provider", 1)
        set_description( "Qt basic system theme" )
        set_callback( SystemPaletteThemeProviderOpen )
        add_shortcut("qt-themeprovider-systempalette")
vlc_module_end ()

/*****************************************/

Q_DECLARE_METATYPE(QQmlError)

/* Ugly, but the Qt interface assumes single instance anyway */
static qt_intf_t* g_qtIntf = nullptr;
static vlc::threads::condition_variable wait_ready;
static vlc::threads::mutex lock;
static bool busy = false;
static enum {
    OPEN_STATE_INIT,
    OPEN_STATE_OPENED,
    OPEN_STATE_ERROR,
} open_state = OPEN_STATE_INIT;


enum CleanupReason {
    CLEANUP_ERROR,
    CLEANUP_APP_TERMINATED,
    CLEANUP_INTF_CLOSED
};

static inline void triggerQuit()
{
    QMetaObject::invokeMethod(qApp, []() {
            qApp->closeAllWindows();
            qApp->quit();
        }, Qt::QueuedConnection);
}

class Translator : public QTranslator
{
public:
    explicit Translator(QObject* parent) : QTranslator(parent) { }

    bool isEmpty() const override { return false; };

    QString translate(const char *context,
                      const char *sourceText,
                      const char *disambiguation = nullptr,
                      int n = -1) const override
    {
        Q_UNUSED(context);
        Q_UNUSED(disambiguation);
        Q_UNUSED(n);
        const char* const text = vlc_gettext(sourceText);
        assert(text);
        return QString::fromUtf8(text);
    }
};

/*****************************************************************************
 * Module callbacks
 *****************************************************************************/

static void *Thread( void * );
static void *ThreadCleanup( qt_intf_t *p_intf, CleanupReason cleanupReason );

#ifdef Q_OS_MAC
/* Used to abort the app.exec() on OSX after libvlc_Quit is called */
#include "../../../lib/libvlc_internal.h" /* libvlc_SetExitHandler */
static void Abort( void *obj )
{
    triggerQuit();
}
#endif

/* Open Interface */
static int OpenInternal( qt_intf_t *p_intf )
{
#ifndef X_DISPLAY_MISSING
    if (!vlc_xlib_init(&p_intf->obj))
        return VLC_EGENERIC;
#endif

#if (_POSIX_SPAWN >= 0)
    /* Check if QApplication works */
    char *path = config_GetSysPath(VLC_PKG_LIBEXEC_DIR, "vlc-qt-check");
    if (unlikely(path == NULL))
        return VLC_ENOMEM;

    char *argv[] = { path, NULL };
    pid_t pid;

    int val = posix_spawn(&pid, path, NULL, NULL, argv, environ);
    free(path);
    if (val)
        return VLC_ENOMEM;

    int status;
    while (waitpid(pid, &status, 0) == -1);

    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
    {
        msg_Dbg(p_intf, "Qt check failed (%d). Skipping.", status);
        return VLC_EGENERIC;
    }
#endif

    /* Get the playlist before the lock to avoid a lock-order-inversion */
    vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(p_intf->intf);

#ifndef Q_OS_MAC
    vlc::threads::mutex_locker locker (lock);
    if (busy || open_state == OPEN_STATE_ERROR)
    {
        if (busy)
            msg_Err (p_intf, "cannot start Qt multiple times");
        return VLC_EGENERIC;
    }
#endif

    p_intf->p_mi = NULL;

    /* set up the playlist to work on */
    p_intf->p_playlist = playlist;
    p_intf->p_player = vlc_playlist_GetPlayer( p_intf->p_playlist );

    /* */
#ifdef Q_OS_MAC
    /* Run mainloop on the main thread as Cocoa requires */
    libvlc_SetExitHandler( vlc_object_instance(p_intf), Abort, p_intf );
    Thread( (void *)p_intf );
#else
    if( vlc_clone( &p_intf->thread, Thread, p_intf ) )
    {
        return VLC_ENOMEM;
    }
#endif

    /* Wait for the interface to be ready. This prevents the main
     * LibVLC thread from starting video playback before we can create
     * an embedded video window. */
#ifndef Q_OS_MAC
    while (open_state == OPEN_STATE_INIT)
        wait_ready.wait(lock);
#endif

    if (open_state == OPEN_STATE_ERROR)
    {
#ifndef Q_OS_MAC
        vlc_join (p_intf->thread, NULL);
#endif
        return VLC_EGENERIC;
    }

    busy = true;
    return VLC_SUCCESS;
}


static void CloseInternal( qt_intf_t *p_intf )
{
    /* And quit */
    msg_Dbg( p_intf, "requesting exit..." );

    triggerQuit();

    msg_Dbg( p_intf, "waiting for UI thread..." );
#ifndef Q_OS_MAC
    vlc_join (p_intf->thread, NULL);
#endif

    //mutex scope
    {
        vlc::threads::mutex_locker locker (lock);
        assert (busy);
        assert (open_state == OPEN_STATE_INIT);
        busy = false;
    }
    vlc_LogDestroy(p_intf->obj.logger);
    vlc_object_delete(p_intf);
}


/* Open Qt interface */
static int OpenIntfCommon( vlc_object_t *p_this, bool dialogProvider )
{
    auto intfThread = reinterpret_cast<intf_thread_t*>(p_this);
    libvlc_int_t *libvlc = vlc_object_instance( p_this );

    /* Ensure initialization of objects in qt_intf_t. */
    auto p_intf = vlc_object_create<qt_intf_t>( libvlc );
    if (!p_intf)
        return VLC_ENOMEM;
    p_intf->obj.logger = vlc_LogHeaderCreate(libvlc->obj.logger, "qt");
    if (!p_intf->obj.logger)
    {
        vlc_object_delete(p_intf);
        return VLC_EGENERIC;
    }
    p_intf->intf = intfThread;
    p_intf->b_isDialogProvider = dialogProvider;
    p_intf->isShuttingDown = false;
    p_intf->refCount = 1;

    int ret = OpenInternal(p_intf);
    if (ret != VLC_SUCCESS)
    {
        vlc_LogDestroy(p_intf->obj.logger);
        vlc_object_delete(p_intf);
        return VLC_EGENERIC;
    }
    intfThread->pf_show_dialog = p_intf->pf_show_dialog;
    intfThread->p_sys = reinterpret_cast<intf_sys_t*>(p_intf);

    //mutex scope
    {
        vlc::threads::mutex_locker locker (lock);
        g_qtIntf = p_intf;
    }
    return VLC_SUCCESS;
}

static int OpenIntf( vlc_object_t *p_this )
{
    return OpenIntfCommon(p_this, false);
}

/* Open Dialog Provider */
static int OpenDialogs( vlc_object_t *p_this )
{
    return OpenIntfCommon(p_this, false);
}

/* close interface */
static void Close( vlc_object_t *p_this )
{
    intf_thread_t* intfThread = (intf_thread_t*)(p_this);
    auto p_intf = reinterpret_cast<qt_intf_t*>(intfThread->p_sys);
    if (p_intf)
    {
        //cleanup the interface
        QMetaObject::invokeMethod( p_intf->p_mi, [p_intf] () {
            ThreadCleanup(p_intf, CLEANUP_INTF_CLOSED);
        }, Qt::BlockingQueuedConnection);

        bool shutdown = false;
        //mutex scope
        {
            vlc::threads::mutex_locker locker(lock);
            assert(g_qtIntf == p_intf);
            p_intf->refCount -= 1;
            if (p_intf->refCount == 0)
            {
                shutdown = true;
                g_qtIntf = nullptr;
            }
            else
                p_intf->isShuttingDown = true;
        }

        if (shutdown)
            CloseInternal(p_intf);
    }
}

static inline void registerMetaTypes()
{
    qRegisterMetaType<size_t>();
    qRegisterMetaType<ssize_t>();
    qRegisterMetaType<vlc_tick_t>();

    qRegisterMetaType<VLCTick>();
    qRegisterMetaType<SharedInputItem>();
    qRegisterMetaType<NetworkTreeItem>();
    qRegisterMetaType<Playlist>();
    qRegisterMetaType<PlaylistItem>();
    qRegisterMetaType<DialogId>();
    qRegisterMetaType<MLItemId>();
    qRegisterMetaType<QVector<MLItemId>>();
    qRegisterMetaType<QList<QQmlError>>();
}

static void *Thread( void *obj )
{
    qt_intf_t *p_intf = (qt_intf_t *)obj;
    char vlc_name[] = "vlc"; /* for WM_CLASS */
    char *argv[3] = { nullptr };
    int argc = 0;

    vlc_thread_set_name("vlc-qt");

    auto argvReleaser = vlc::wrap_carray<char*>(argv, [](char* ptr[]) {
        for ( int i = 0; ptr[i] != nullptr; ++i )
            free(ptr[i]);
    });
    argv[argc++] = strdup(vlc_name);

#ifdef QT_QML_DEBUG
    char* qmlJsDebugOpt = var_InheritString(p_intf, "qt-qmljsdebugger");
    if (qmlJsDebugOpt)
    {
        msg_Dbg(p_intf, "option qt-qmljsdebugger is %s", qmlJsDebugOpt);
        char* psz_debug_opt;
        if (asprintf(&psz_debug_opt, "-qmljsdebugger=%s", qmlJsDebugOpt) < 0)
        {
            free(qmlJsDebugOpt);
            return NULL;
        }
        argv[argc++] = psz_debug_opt;
        free(qmlJsDebugOpt);
    }
#endif
    argv[argc] = NULL;

#ifdef QT_STATIC
    Q_INIT_RESOURCE( vlc );
    Q_INIT_RESOURCE( shaders );

    Q_INIT_RESOURCE( qmake_Qt5Compat_GraphicalEffects );
    Q_INIT_RESOURCE( qmake_Qt5Compat_GraphicalEffects_private );
    Q_INIT_RESOURCE( qmake_QtQml );
    Q_INIT_RESOURCE( qmake_QtQml_Base );
    Q_INIT_RESOURCE( qmake_QtQml_Models );
    Q_INIT_RESOURCE( qmake_QtQml_WorkerScript );
    Q_INIT_RESOURCE( qmake_QtQuick );
    Q_INIT_RESOURCE( qmake_QtQuick_Window );
    Q_INIT_RESOURCE( qmake_QtQuick_Controls );
    Q_INIT_RESOURCE( qmake_QtQuick_Controls_impl );
    Q_INIT_RESOURCE( qmake_QtQuick_Controls_Basic );
    Q_INIT_RESOURCE( qmake_QtQuick_Controls_Basic_impl );
    Q_INIT_RESOURCE( qmake_QtQuick_Controls_Fusion );
    Q_INIT_RESOURCE( qmake_QtQuick_Controls_Fusion_impl );
#ifdef _WIN32
    Q_INIT_RESOURCE( qmake_QtQuick_Controls_Windows );
    Q_INIT_RESOURCE( qmake_QtQuick_NativeStyle );
    Q_INIT_RESOURCE( qtquickcontrols2windowsstyleplugin_raw_qml_0 );
    Q_INIT_RESOURCE( qtquickcontrols2nativestyleplugin_raw_qml_0 );
#endif
    Q_INIT_RESOURCE( qmake_QtQuick_Layouts );
    Q_INIT_RESOURCE( qmake_QtQuick_Templates );

    Q_INIT_RESOURCE( qtquickcontrols2fusionstyleimplplugin_raw_qml_0 );
    Q_INIT_RESOURCE( qtquickcontrols2fusionstyleplugin_raw_qml_0 );
    Q_INIT_RESOURCE( qtquickcontrols2fusionstyle );
    Q_INIT_RESOURCE( qtquickcontrols2basicstyleplugin_raw_qml_0 );
    Q_INIT_RESOURCE( qtquickcontrols2basicstyleplugin );

    Q_INIT_RESOURCE( qtgraphicaleffectsplugin_raw_qml_0 );
    Q_INIT_RESOURCE( qtgraphicaleffectsprivate_raw_qml_0 );
    Q_INIT_RESOURCE( qtgraphicaleffectsshaders );
    Q_INIT_RESOURCE( qtquickshapes_shaders );
#endif

#ifdef _WIN32
    // QSysInfo::productVersion() returns "unknown" on Windows 7
    // RHI Fallback does not seem to work.

    DWORD dwVersion = 0;
    DWORD dwMajorVersion = 0;
    DWORD dwMinorVersion = 0;

    dwVersion = GetVersion();

    dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
    dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));

    if (dwMajorVersion <= 6 && dwMinorVersion <= 1)
        QQuickWindow::setGraphicsApi(QSGRendererInterface::OpenGL);

#endif

    auto compositor = var_InheritString(p_intf, "qt-compositor");
    vlc::CompositorFactory compositorFactory(p_intf, compositor);
    free(compositor);

    compositorFactory.preInit();

    // at the moment, the vout is created in another thread than the rendering thread
    QApplication::setAttribute( Qt::AA_DontCheckOpenGLContextThreadAffinity );
    QQuickWindow::setDefaultAlphaBuffer(true);

#ifdef QT_STATIC
#ifdef _WIN32
    QQuickStyle::setStyle(QLatin1String("Windows"));
#else
    QQuickStyle::setStyle(QLatin1String("Fusion"));
#endif
    QQuickStyle::setFallbackStyle(QLatin1String("Fusion"));
#endif

    /* Start the QApplication here */
    QApplication app( argc, argv );
    app.setProperty("initialStyle", app.style()->objectName());

    {
        // Install custom translator:
        const auto translator = new Translator(&app);
        const auto ret = QCoreApplication::installTranslator(translator);
        assert(ret);
    }

    registerMetaTypes();

    //app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    /* Set application direction to locale direction,
     * necessary for  RTL locales */
    app.setLayoutDirection(QLocale().textDirection());

    /* All the settings are in the .conf/.ini style */
#ifdef _WIN32
    char *cConfigDir = config_GetUserDir( VLC_CONFIG_DIR );
    if (likely(cConfigDir != nullptr))
    {
        QString configDir = cConfigDir;
        free( cConfigDir );
        if( configDir.endsWith( "\\vlc" ) )
            configDir.chop( 4 ); /* the "\vlc" dir is added again by QSettings */
        QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, configDir );
    }
#endif

    p_intf->mainSettings = new QSettings(
#ifdef _WIN32
            QSettings::IniFormat,
#else
            QSettings::NativeFormat,
#endif
            QSettings::UserScope, "vlc", "vlc-qt-interface" );

    app.setApplicationDisplayName( qtr("VLC media player") );

    if( QDate::currentDate().dayOfYear() >= QT_XMAS_JOKE_DAY && var_InheritBool( p_intf, "qt-icon-change" ) )
        app.setWindowIcon( QIcon::fromTheme( "vlc-xmas", QIcon( ":/logo/vlc128-xmas.png" ) ) );
    else
        app.setWindowIcon( QIcon::fromTheme( "vlc", QIcon( ":/logo/vlc256.png" ) ) );

    app.setDesktopFileName( PACKAGE );

    DialogErrorModel::getInstance( p_intf );

    /* Initialize the Dialog Provider and the Main Input Manager */
    DialogsProvider::getInstance( p_intf );
    p_intf->p_mainPlayerController = new PlayerController(p_intf);
    p_intf->p_mainPlaylistController = new vlc::playlist::PlaylistController(p_intf->p_playlist);

#ifdef UPDATE_CHECK
    /* Checking for VLC updates */
    if( var_InheritBool( p_intf, "qt-updates-notif" ) &&
        !var_InheritBool( p_intf, "qt-privacy-ask" ) )
    {
        int interval = var_InheritInteger( p_intf, "qt-updates-days" );
        if( QDate::currentDate() >
             getSettings()->value( "updatedate" ).toDate().addDays( interval ) )
        {
            /* The constructor of the update Dialog will do the 1st request */
            UpdateDialog::getInstance( p_intf );
            getSettings()->setValue( "updatedate", QDate::currentDate() );
        }
    }
#endif

    /* Create the normal interface in non-DP mode */
#ifdef _WIN32
    p_intf->p_mi = new MainCtxWin32(p_intf);
#else
    p_intf->p_mi = new MainCtx(p_intf);
#endif

    if( !p_intf->b_isDialogProvider )
    {
        bool ret = false;
        do {
            p_intf->p_compositor.reset(compositorFactory.createCompositor());
            if (! p_intf->p_compositor)
                break;
            ret = p_intf->p_compositor->makeMainInterface(p_intf->p_mi);
            if (!ret)
            {
                p_intf->p_compositor->destroyMainInterface();
                p_intf->p_compositor.reset();
            }
        } while(!ret);

        if (!ret)
        {
            msg_Err(p_intf, "unable to create main interface");
            delete p_intf->p_mi;
            p_intf->p_mi = nullptr;
            //process deleteLater events as the main loop will never run
            QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
            return ThreadCleanup( p_intf, CLEANUP_ERROR );
        }

        /* Check window type from the Qt platform back-end */
        bool known_type = true;

        const QString& platform = app.platformName();
        if( platform == QLatin1String("xcb") )
            p_intf->voutWindowType = VLC_WINDOW_TYPE_XID;
        else if( platform.startsWith( QLatin1String("wayland") ) ) {
            p_intf->voutWindowType = VLC_WINDOW_TYPE_WAYLAND;

            // Workaround for popup widgets not closing on mouse press on wayland:
            app.installEventFilter(new DismissPopupEventFilter(&app));
        }
        else if( platform == QLatin1String("windows") || platform == QLatin1String("direct2d") )
            p_intf->voutWindowType = VLC_WINDOW_TYPE_HWND;
        else if( platform == QLatin1String("cocoa") )
            p_intf->voutWindowType = VLC_WINDOW_TYPE_NSOBJECT;
        else
        {
            msg_Err( p_intf, "unknown Qt platform: %s", qtu(platform) );
            known_type = false;
        }

        /* FIXME: Temporary, while waiting for a proper window provider API */
        libvlc_int_t *libvlc = vlc_object_instance( p_intf );
        if( known_type )
            var_SetString( libvlc, "window", "qt,any" );
    }

    /* Explain how to show a dialog :D */
    p_intf->pf_show_dialog = ShowDialog;

    /* Tell the main LibVLC thread we are ready */
    {
        vlc::threads::mutex_locker locker (lock);
        assert(g_qtIntf == nullptr);
        g_qtIntf = p_intf;
        open_state = OPEN_STATE_OPENED;
        wait_ready.signal();
    }
#ifdef Q_OS_MAC
    /* We took over main thread, register and start here */
    if( !p_intf->b_isDialogProvider )
    {
        vlc_playlist_Lock( p_intf->p_playlist );
        vlc_playlist_Start( p_intf->p_playlist );
        vlc_playlist_Unlock( p_intf->p_playlist );
    }
#endif

    /* Last settings */
    app.setQuitOnLastWindowClosed( false );

    /* Loads and tries to apply the preferred QStyle */
    QString s_style = getSettings()->value( "MainWindow/QtStyle", "" ).toString();
    if( s_style.compare("") != 0 )
        QApplication::setStyle( s_style );

    /* Launch */
    app.exec();

    msg_Dbg( p_intf, "QApp exec() finished" );
    return ThreadCleanup( p_intf, CLEANUP_APP_TERMINATED );
}

static void *ThreadCleanup( qt_intf_t *p_intf, CleanupReason cleanupReason )
{
    {
#ifndef Q_OS_MAC
        vlc::threads::mutex_locker locker (lock);
#endif
        if( cleanupReason == CLEANUP_ERROR )
        {
            open_state = OPEN_STATE_ERROR;
#ifndef Q_OS_MAC
            wait_ready.signal();
#endif
        }
        else
            open_state = OPEN_STATE_INIT;
    }

    if ( p_intf->p_compositor )
    {
        if (cleanupReason == CLEANUP_INTF_CLOSED)
        {
            p_intf->p_compositor->unloadGUI();
            delete p_intf->p_mi;
            p_intf->p_mi = nullptr;
        }
        else // CLEANUP_APP_TERMINATED
        {
            p_intf->p_compositor->destroyMainInterface();
            delete p_intf->p_mi;
            p_intf->p_mi = nullptr;

            delete p_intf->mainSettings;
            p_intf->mainSettings = nullptr;

            p_intf->p_compositor.reset();
        }
    }

    /* */
    ExtensionsManager::killInstance();
    AddonsManager::killInstance();

    /* Destroy all remaining windows,
       because some are connected to some slots
       in the MainInputManager
       Settings must be destroyed after that.
     */
    DialogsProvider::killInstance();

    DialogErrorModel::killInstance();

    /* Destroy the main playlist controller */
    if (p_intf->p_mainPlaylistController)
    {
        delete p_intf->p_mainPlaylistController;
        p_intf->p_mainPlaylistController = nullptr;
    }

    /* Destroy the main InputManager */
    if (p_intf->p_mainPlayerController)
    {
        delete p_intf->p_mainPlayerController;
        p_intf->p_mainPlayerController = nullptr;
    }

    /* Delete the application automatically */
    return NULL;
}

/*****************************************************************************
 * Callback to show a dialog
 *****************************************************************************/
static void ShowDialog( intf_thread_t *p_intf, int i_dialog_event, int i_arg,
                        intf_dialog_args_t *p_arg )
{
    VLC_UNUSED( p_intf );
    DialogEvent *event = new DialogEvent( i_dialog_event, i_arg, p_arg );
    QApplication::postEvent( THEDP, event );
}

static void WindowCloseCb( vlc_window_t * )
{
    qt_intf_t *p_intf = nullptr;
    bool shutdown = false;
    //mutex scope
    {
        vlc::threads::mutex_locker locker(lock);
        assert(g_qtIntf != nullptr);
        p_intf = g_qtIntf;

        p_intf->refCount -= 1;
        if (p_intf->refCount == 0)
            shutdown = true;
    }
    if (shutdown)
        CloseInternal(p_intf);
}

/**
 * Video output window provider
 */
static int WindowOpen( vlc_window_t *p_wnd )
{
    if( !var_InheritBool( p_wnd, "embedded-video" ) )
        return VLC_EGENERIC;

    qt_intf_t *p_intf = nullptr;
    //mutex scope
    {
        vlc::threads::mutex_locker locker(lock);

        p_intf = g_qtIntf;
        if( !p_intf )
        {   /* If another interface is used, this plugin cannot work */
            msg_Dbg( p_wnd, "Qt interface not found" );
            return VLC_EGENERIC;
        }

        if (unlikely(open_state != OPEN_STATE_OPENED))
            return VLC_EGENERIC;

        if (p_intf->isShuttingDown)
            return VLC_EGENERIC;

        switch( p_intf->voutWindowType )
        {
            case VLC_WINDOW_TYPE_XID:
            case VLC_WINDOW_TYPE_HWND:
                if( var_InheritBool( p_wnd, "video-wallpaper" ) )
                    return VLC_EGENERIC;
                break;
        }

        bool ret = p_intf->p_compositor->setupVoutWindow( p_wnd, &WindowCloseCb );
        if (ret)
            p_intf->refCount += 1;
        return ret ? VLC_SUCCESS : VLC_EGENERIC;
    }
}
