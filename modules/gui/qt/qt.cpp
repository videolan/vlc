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

#include "qt.hpp"

#include "player/player_controller.hpp"    /* THEMIM destruction */
#include "playlist/playlist_controller.hpp" /* THEMPL creation */
#include "dialogs/dialogs_provider.hpp" /* THEDP creation */
#ifdef _WIN32
# include "maininterface/main_interface_win32.hpp"
#else
# include "maininterface/main_interface.hpp"   /* MainInterface creation */
#endif
#include "dialogs/extensions/extensions_manager.hpp" /* Extensions manager */
#include "dialogs/plugins/addons_manager.hpp" /* Addons manager */
#include "dialogs/help/help.hpp"     /* Launch Update */
#include "util/recents.hpp"          /* Recents Item destruction */
#include "util/qvlcapp.hpp"     /* QVLCApplication definition */
#include "maininterface/compositor.hpp"

#include <QVector>
#include "playlist/playlist_item.hpp"

#include <vlc_plugin.h>
#include <vlc_vout_window.h>
#include <vlc_cxx_helpers.hpp>

#ifdef QT_STATIC /* For static builds */
 #include <QtPlugin>
 #include <QQuickWindow>

 #ifdef QT_STATICPLUGIN
  Q_IMPORT_PLUGIN(QSvgIconPlugin)
  Q_IMPORT_PLUGIN(QSvgPlugin)
  Q_IMPORT_PLUGIN(QJpegPlugin)
  Q_IMPORT_PLUGIN(QtQuick2Plugin)
  Q_IMPORT_PLUGIN(QtQuickControls2Plugin)
  Q_IMPORT_PLUGIN(QtQuickLayoutsPlugin)
  Q_IMPORT_PLUGIN(QtQuick2WindowPlugin)
  Q_IMPORT_PLUGIN(QtQuickTemplates2Plugin)
  Q_IMPORT_PLUGIN(QtQmlModelsPlugin)
  Q_IMPORT_PLUGIN(QtGraphicalEffectsPlugin)
  Q_IMPORT_PLUGIN(QtGraphicalEffectsPrivatePlugin)

  #if !HAS_QT56
   Q_IMPORT_PLUGIN(AccessibleFactory)
  #endif
  #ifdef _WIN32
   Q_IMPORT_PLUGIN(QWindowsVistaStylePlugin)
   Q_IMPORT_PLUGIN(QWindowsIntegrationPlugin)
  #endif
 #endif
#endif

#ifndef X_DISPLAY_MISSING
# include <vlc_xlib.h>
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  OpenIntf     ( vlc_object_t * );
static int  OpenDialogs  ( vlc_object_t * );
static int  Open         ( vlc_object_t *, bool );
static void Close        ( vlc_object_t * );
static int  WindowOpen   ( vout_window_t * );
static void ShowDialog   ( intf_thread_t *, int, int, intf_dialog_args_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ADVANCED_PREFS_TEXT N_( "Show advanced preferences over simple ones" )
#define ADVANCED_PREFS_LONGTEXT N_( "Show advanced preferences and not simple "\
                                    "preferences when opening the preferences "\
                                    "dialog." )

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

#define INTERFACE_SCALE_TEXT N_( "User scale factor for the interface, betwwen 0.1 and 10.0" )

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

#define SLIDERCOL_TEXT N_( "Define the colors of the volume slider" )
#define SLIDERCOL_LONGTEXT N_( "Define the colors of the volume slider\n" \
                       "By specifying the 12 numbers separated by a ';'\n" \
            "Default is '255;255;255;20;226;20;255;176;15;235;30;20'\n" \
            "An alternative can be '30;30;50;40;40;100;50;50;160;150;150;255'")

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

#define FULLSCREEN_CONTROL_PIXELS N_( "Fullscreen controller mouse sensitivity" )

#define CONTINUE_PLAYBACK_TEXT N_("Continue playback?")

static const int i_notification_list[] =
    { NOTIFICATION_NEVER, NOTIFICATION_MINIMIZED, NOTIFICATION_ALWAYS };

static const char *const psz_notification_list_text[] =
    { N_("Never"), N_("When minimized"), N_("Always") };

static const int i_continue_list[] =
    { 0, 1, 2 };

static const char *const psz_continue_list_text[] =
    { N_("Never"), N_("Ask"), N_("Always") };

static const int i_raise_list[] =
    { MainInterface::RAISE_NEVER, MainInterface::RAISE_VIDEO, \
      MainInterface::RAISE_AUDIO, MainInterface::RAISE_AUDIOVIDEO,  };

static const char *const psz_raise_list_text[] =
    { N_( "Never" ), N_( "Video" ), N_( "Audio" ), _( "Audio/Video" ) };

/**********************************************************************/
vlc_module_begin ()
    set_shortname( "Qt" )
    set_description( N_("Qt interface") )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_MAIN )
    set_capability( "interface", 151 )
    set_callbacks( OpenIntf, Close )

    add_shortcut("qt")

    add_bool( "qt-minimal-view", false, QT_MINIMAL_MODE_TEXT,
              QT_MINIMAL_MODE_TEXT, false );

    add_bool( "qt-system-tray", true, SYSTRAY_TEXT, SYSTRAY_LONGTEXT, false)

    add_integer( "qt-notification", NOTIFICATION_MINIMIZED,
                 NOTIFICATION_TEXT,
                 NOTIFICATION_LONGTEXT, false )
            change_integer_list( i_notification_list, psz_notification_list_text )

    add_bool( "qt-start-minimized", false, MINIMIZED_TEXT,
              MINIMIZED_LONGTEXT, true)
    add_bool( "qt-pause-minimized", false, QT_PAUSE_MINIMIZED_TEXT,
              QT_PAUSE_MINIMIZED_LONGTEXT, false )

    add_float_with_range( "qt-opacity", 1., 0.1, 1., OPACITY_TEXT,
                          OPACITY_LONGTEXT, false )
    add_float_with_range( "qt-fs-opacity", 0.8, 0.1, 1., OPACITY_FS_TEXT,
                          OPACITY_FS_LONGTEXT, false )

    add_float_with_range( "qt-interface-scale", 1.0, 0.1, 10., INTERFACE_SCALE_TEXT,
                          INTERFACE_SCALE_TEXT, false )

    add_bool( "qt-video-autoresize", true, KEEPSIZE_TEXT,
              KEEPSIZE_LONGTEXT, false )
    add_bool( "qt-name-in-title", true, TITLE_TEXT,
              TITLE_LONGTEXT, false )
    add_bool( "qt-fs-controller", true, QT_FULLSCREEN_TEXT,
              QT_FULLSCREEN_TEXT, false )

    add_bool( "qt-recentplay", true, RECENTPLAY_TEXT,
              RECENTPLAY_TEXT, false )
    add_string( "qt-recentplay-filter", "",
                RECENTPLAY_FILTER_TEXT, RECENTPLAY_FILTER_LONGTEXT, false )
    add_integer( "qt-continue", 1, CONTINUE_PLAYBACK_TEXT, CONTINUE_PLAYBACK_TEXT, false )
            change_integer_list(i_continue_list, psz_continue_list_text )

#ifdef UPDATE_CHECK
    add_bool( "qt-updates-notif", true, UPDATER_TEXT,
              UPDATER_LONGTEXT, false )
    add_integer_with_range( "qt-updates-days", 3, 0, 180,
              UPDATER_DAYS_TEXT, UPDATER_DAYS_TEXT, false )
#endif

#ifdef QT_QML_DEBUG
    add_string( "qt-qmljsdebugger", NULL,
                QT_QML_DEBUG_TEXT, QT_QML_DEBUG_LONGTEXT, false )
#endif

#ifdef _WIN32
    add_bool( "qt-disable-volume-keys"             /* name */,
              true                                 /* default value */,
              QT_DISABLE_VOLUME_KEYS_TEXT          /* text */,
              QT_DISABLE_VOLUME_KEYS_LONGTEXT      /* longtext */,
              false                                /* advanced mode only */)
#endif

    add_bool( "qt-embedded-open", false, QT_NATIVEOPEN_TEXT,
               QT_NATIVEOPEN_TEXT, false )


    add_bool( "qt-advanced-pref", false, ADVANCED_PREFS_TEXT,
              ADVANCED_PREFS_LONGTEXT, false )
    add_bool( "qt-error-dialogs", true, ERROR_TEXT,
              ERROR_TEXT, false )

    add_string( "qt-slider-colours", "153;210;153;20;210;20;255;199;15;245;39;29",
                SLIDERCOL_TEXT, SLIDERCOL_LONGTEXT, false )

    add_bool( "qt-privacy-ask", true, PRIVACY_TEXT, PRIVACY_TEXT,
              false )
        change_private ()

    add_integer( "qt-fullscreen-screennumber", -1, FULLSCREEN_NUMBER_TEXT,
               FULLSCREEN_NUMBER_LONGTEXT, false );

    add_bool( "qt-autoload-extensions", true,
              QT_AUTOLOAD_EXTENSIONS_TEXT, QT_AUTOLOAD_EXTENSIONS_LONGTEXT,
              false )

    add_bool( "qt-bgcone", true, QT_BGCONE_TEXT, QT_BGCONE_LONGTEXT, true )
    add_bool( "qt-bgcone-expands", false, QT_BGCONE_EXPANDS_TEXT,
              QT_BGCONE_EXPANDS_LONGTEXT, true )

    add_bool( "qt-icon-change", true, ICONCHANGE_TEXT, ICONCHANGE_LONGTEXT, true )

    add_integer_with_range( "qt-max-volume", 125, 60, 300, VOLUME_MAX_TEXT, VOLUME_MAX_TEXT, true)

    add_integer_with_range( "qt-fs-sensitivity", 3, 0, 4000, FULLSCREEN_CONTROL_PIXELS,
            FULLSCREEN_CONTROL_PIXELS, true)

    add_obsolete_bool( "qt-blingbling" )      /* Suppressed since 1.0.0 */
    add_obsolete_integer( "qt-display-mode" ) /* Suppressed since 1.1.0 */

    add_obsolete_bool( "qt-adv-options" )     /* Since 2.0.0 */
    add_obsolete_bool( "qt-volume-complete" ) /* Since 2.0.0 */
    add_obsolete_integer( "qt-startvolume" )  /* Since 2.0.0 */

    add_integer( "qt-auto-raise", MainInterface::RAISE_VIDEO, AUTORAISE_ON_PLAYBACK_TEXT,
                 AUTORAISE_ON_PLAYBACK_LONGTEXT, false )
            change_integer_list( i_raise_list, psz_raise_list_text )

    cannot_unload_broken_library()

    add_submodule ()
        set_description( "Dialogs provider" )
        set_capability( "dialogs provider", 51 )

        set_callbacks( OpenDialogs, Close )

    add_submodule ()
        set_capability( "vout window", 0 )
        set_callback( WindowOpen )

vlc_module_end ()

/*****************************************/

/* Ugly, but the Qt interface assumes single instance anyway */
static vlc::threads::condition_variable wait_ready;
static vlc::threads::mutex lock;
static bool busy = false;
static enum {
    OPEN_STATE_INIT,
    OPEN_STATE_OPENED,
    OPEN_STATE_ERROR,
} open_state = OPEN_STATE_INIT;

/*****************************************************************************
 * Module callbacks
 *****************************************************************************/

static void *Thread( void * );
static void *ThreadCleanup( intf_thread_t *p_intf, bool error );

#ifdef Q_OS_MAC
/* Used to abort the app.exec() on OSX after libvlc_Quit is called */
#include "../../../lib/libvlc_internal.h" /* libvlc_SetExitHandler */
static void Abort( void *obj )
{
    QVLCApp::triggerQuit();
}
#endif

/* Open Interface */
static int Open( vlc_object_t *p_this, bool isDialogProvider )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

#ifndef X_DISPLAY_MISSING
    if (!vlc_xlib_init(p_this))
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
        msg_Dbg(p_this, "Qt check failed (%d). Skipping.", status);
        return VLC_EGENERIC;
    }
#endif

    /* Get the playlist before the lock to avoid a lock-order-inversion */
    vlc_playlist_t *playlist = vlc_intf_GetMainPlaylist(p_intf);

    vlc::threads::mutex_locker locker (lock);
    if (busy || open_state == OPEN_STATE_ERROR)
    {
        if (busy)
            msg_Err (p_this, "cannot start Qt multiple times");
        return VLC_EGENERIC;
    }

    /* Allocations of p_sys */
    intf_sys_t *p_sys = p_intf->p_sys = new intf_sys_t;
    p_sys->b_isDialogProvider = isDialogProvider;
    p_sys->p_mi = NULL;

    /* set up the playlist to work on */
    p_sys->p_playlist = playlist;
    p_sys->p_player = vlc_playlist_GetPlayer( p_sys->p_playlist );

    /* */
#ifdef Q_OS_MAC
    /* Run mainloop on the main thread as Cocoa requires */
    libvlc_SetExitHandler( vlc_object_instance(p_intf), Abort, p_intf );
    Thread( (void *)p_intf );
#else
    if( vlc_clone( &p_sys->thread, Thread, p_intf, VLC_THREAD_PRIORITY_LOW ) )
    {
        delete p_sys;
        return VLC_ENOMEM;
    }
#endif

    /* Wait for the interface to be ready. This prevents the main
     * LibVLC thread from starting video playback before we can create
     * an embedded video window. */
    while (open_state == OPEN_STATE_INIT)
        wait_ready.wait(lock);

    if (open_state == OPEN_STATE_ERROR)
    {
#ifndef Q_OS_MAC
        vlc_join (p_sys->thread, NULL);
#endif
        delete p_sys;
        return VLC_EGENERIC;
    }

    busy = true;
    return VLC_SUCCESS;
}

/* Open Qt interface */
static int OpenIntf( vlc_object_t *p_this )
{
    return Open( p_this, false );
}

/* Open Dialog Provider */
static int OpenDialogs( vlc_object_t *p_this )
{
    return Open( p_this, true );
}

static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    /* And quit */
    msg_Dbg( p_this, "requesting exit..." );
    QVLCApp::triggerQuit();

    msg_Dbg( p_this, "waiting for UI thread..." );
#ifndef Q_OS_MAC
    vlc_join (p_sys->thread, NULL);
#endif
    delete p_sys;

    vlc::threads::mutex_locker locker (lock);
    assert (busy);
    assert (open_state == OPEN_STATE_INIT);
    busy = false;
}

static inline void qRegisterMetaTypes()
{
    // register all types used by signal/slots
    qRegisterMetaType<size_t>("size_t");
    qRegisterMetaType<ssize_t>("ssize_t");
    qRegisterMetaType<vlc_tick_t>("vlc_tick_t");
}

static void *Thread( void *obj )
{
    intf_thread_t *p_intf = (intf_thread_t *)obj;
    intf_sys_t *p_sys = p_intf->p_sys;
    char vlc_name[] = "vlc"; /* for WM_CLASS */
    char *argv[3] = { nullptr };
    int argc = 0;

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

    Q_INIT_RESOURCE( vlc );

    p_intf->p_sys->p_compositor = vlc::Compositor::createCompositor(p_intf);

#if HAS_QT56
    QApplication::setAttribute( Qt::AA_EnableHighDpiScaling );
    QApplication::setAttribute( Qt::AA_UseHighDpiPixmaps );
#endif

    // at the moment, the vout is created in another thread than the rendering thread
    QApplication::setAttribute( Qt::AA_DontCheckOpenGLContextThreadAffinity );
    QQuickWindow::setDefaultAlphaBuffer(true);

    QQuickStyle::setStyle("fusion");

    /* Start the QApplication here */
    QVLCApp app( argc, argv );

    //app.setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);

    /* Set application direction to locale direction,
     * necessary for  RTL locales */
    app.setLayoutDirection(QLocale().textDirection());

    p_sys->p_app = &app;


    /* All the settings are in the .conf/.ini style */
#ifdef _WIN32
    char *cConfigDir = config_GetUserDir( VLC_CONFIG_DIR );
    QString configDir = cConfigDir;
    free( cConfigDir );
    if( configDir.endsWith( "\\vlc" ) )
        configDir.chop( 4 ); /* the "\vlc" dir is added again by QSettings */
    QSettings::setPath( QSettings::IniFormat, QSettings::UserScope, configDir );
#endif

    p_sys->mainSettings = new QSettings(
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

    /* Initialize the Dialog Provider and the Main Input Manager */
    DialogsProvider::getInstance( p_intf );
    p_sys->p_mainPlayerController = new PlayerController(p_intf);
    p_sys->p_mainPlaylistController = new vlc::playlist::PlaylistControllerModel(p_intf->p_sys->p_playlist);

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
    MainInterface *p_mi = NULL;

    if( !p_sys->b_isDialogProvider )
    {
        p_mi = p_intf->p_sys->p_compositor->makeMainInterface();
        p_sys->p_mi = p_mi;

        if (!p_mi)
        {
            msg_Err(p_intf, "unable to create main interface");
            return ThreadCleanup( p_intf, true );
        }

        /* Check window type from the Qt platform back-end */
        bool known_type = true;

        QString platform = app.platformName();
        if( platform == qfu("xcb") )
            p_sys->voutWindowType = VOUT_WINDOW_TYPE_XID;
        else if( platform == qfu("wayland") || platform == qfu("wayland-egl") )
            p_sys->voutWindowType = VOUT_WINDOW_TYPE_WAYLAND;
        else if( platform == qfu("windows") )
            p_sys->voutWindowType = VOUT_WINDOW_TYPE_HWND;
        else if( platform == qfu("cocoa" ) )
            p_sys->voutWindowType = VOUT_WINDOW_TYPE_NSOBJECT;
        else
        {
            msg_Err( p_intf, "unknown Qt platform: %s", qtu(platform) );
            known_type = false;
        }

        /* FIXME: Temporary, while waiting for a proper window provider API */
        libvlc_int_t *libvlc = vlc_object_instance( p_intf );

        var_Create( libvlc, "qt4-iface", VLC_VAR_ADDRESS );

        if( known_type )
        {
            var_SetAddress( libvlc, "qt4-iface", p_intf );
            var_SetString( libvlc, "window", "qt,any" );
        }
    }

    /* Explain how to show a dialog :D */
    p_intf->pf_show_dialog = ShowDialog;

    /* Tell the main LibVLC thread we are ready */
    {
        vlc::threads::mutex_locker locker (lock);
        open_state = OPEN_STATE_OPENED;
        wait_ready.signal();
    }

#ifdef Q_OS_MAC
    /* We took over main thread, register and start here */
    if( !p_sys->b_isDialogProvider )
    {
        vlc_playlist_Lock( p_intf->p_sys->p_playlist );
        vlc_playlist_Start( p_intf->p_sys->p_playlist );
        vlc_playlist_Unlock( p_intf->p_sys->p_playlist );
    }
#endif

    /* Last settings */
    app.setQuitOnLastWindowClosed( false );

    /* Retrieve last known path used in file browsing */
    p_sys->filepath =
         getSettings()->value( "filedialog-path", QVLCUserDir( VLC_HOME_DIR ) ).toString();

    /* Loads and tries to apply the preferred QStyle */
    QString s_style = getSettings()->value( "MainWindow/QtStyle", "" ).toString();
    if( s_style.compare("") != 0 )
        QApplication::setStyle( s_style );

    qRegisterMetaTypes();

    /* Launch */
    app.exec();

    msg_Dbg( p_intf, "QApp exec() finished" );
    if (p_mi != NULL)
    {
        libvlc_int_t *libvlc = vlc_object_instance( p_intf );
        var_Destroy( libvlc, "qt4-iface" );
    }
    return ThreadCleanup( p_intf, false );
}

static void *ThreadCleanup( intf_thread_t *p_intf, bool error )
{
    intf_sys_t *p_sys = p_intf->p_sys;

    {
        vlc::threads::mutex_locker locker (lock);
        if( error )
        {
            open_state = OPEN_STATE_ERROR;
            wait_ready.signal();
        }
        else
            open_state = OPEN_STATE_INIT;
    }

    if (p_sys->p_compositor)
    {
        p_sys->p_compositor->destroyMainInterface();
        p_sys->p_mi = nullptr;

        delete p_sys->p_compositor;
        p_sys->p_compositor = nullptr;
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

    /* Delete the recentsMRL object before the configuration */
    RecentsMRL::killInstance();

    /* Save the path or delete if recent play are disabled */
    if( var_InheritBool( p_intf, "qt-recentplay" ) )
        getSettings()->setValue( "filedialog-path", p_sys->filepath );
    else
        getSettings()->remove( "filedialog-path" );

    /* Destroy the main playlist controller */
    delete p_sys->p_mainPlaylistController;
    /* Destroy the main InputManager */
    delete p_sys->p_mainPlayerController;
    /* Delete the configuration. Application has to be deleted after that. */
    delete p_sys->mainSettings;

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

/**
 * Video output window provider
 */
static int WindowOpen( vout_window_t *p_wnd )
{
    if( !var_InheritBool( p_wnd, "embedded-video" ) )
        return VLC_EGENERIC;

    libvlc_int_t *libvlc = vlc_object_instance( p_wnd );
    intf_thread_t *p_intf =
        (intf_thread_t *)var_InheritAddress( libvlc, "qt4-iface" );
    if( !p_intf )
    {   /* If another interface is used, this plugin cannot work */
        msg_Dbg( p_wnd, "Qt interface not found" );
        return VLC_EGENERIC;
    }

    switch( p_intf->p_sys->voutWindowType )
    {
        case VOUT_WINDOW_TYPE_XID:
        case VOUT_WINDOW_TYPE_HWND:
            if( var_InheritBool( p_wnd, "video-wallpaper" ) )
                return VLC_EGENERIC;
            break;
    }

    vlc::threads::mutex_locker locker (lock);
    if (unlikely(open_state != OPEN_STATE_OPENED))
        return VLC_EGENERIC;

    return p_intf->p_sys->p_compositor->setupVoutWindow( p_wnd ) ? VLC_SUCCESS : VLC_EGENERIC;

}
