/*****************************************************************************
 * qt4.cpp : Qt interface
 ****************************************************************************
 * Copyright © 2006-2009 the VideoLAN team
 * $Id$
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

#include <QApplication>
#include <QDate>
#include <QMutex>

#include "qt4.hpp"

#include "input_manager.hpp"    /* THEMIM destruction */
#include "dialogs_provider.hpp" /* THEDP creation */
#include "main_interface.hpp"   /* MainInterface creation */
#include "extensions_manager.hpp" /* Extensions manager */
#include "dialogs/help.hpp"     /* Launch Update */
#include "recents.hpp"          /* Recents Item destruction */
#include "util/qvlcapp.hpp"     /* QVLCApplication definition */
#include "components/playlist/playlist_model.hpp" /* for ~PLModel() */

#ifdef Q_WS_X11
 #include <vlc_xlib.h>
#endif

#include <vlc_plugin.h>
#include <vlc_vout_window.h>

#ifdef _WIN32 /* For static builds */
 #include <QtPlugin>
 Q_IMPORT_PLUGIN(qjpeg)
 Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  OpenIntf     ( vlc_object_t * );
static int  OpenDialogs  ( vlc_object_t * );
static int  Open         ( vlc_object_t *, bool );
static void Close        ( vlc_object_t * );
static int  WindowOpen   ( vout_window_t *, const vout_window_cfg_t * );
static void WindowClose  ( vout_window_t * );
static void *Thread      ( void * );
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
                               "your taskbar" )

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


#define ERROR_TEXT N_( "Show unimportant error and warnings dialogs" )

#define UPDATER_TEXT N_( "Activate the updates availability notification" )
#define UPDATER_LONGTEXT N_( "Activate the automatic notification of new " \
                            "versions of the software. It runs once every " \
                            "two weeks." )
#define UPDATER_DAYS_TEXT N_("Number of days between two update checks")

#define PRIVACY_TEXT N_( "Ask for network policy at start" )

#define RECENTPLAY_TEXT N_( "Save the recently played items in the menu" )

#define RECENTPLAY_FILTER_TEXT N_( "List of words separated by | to filter" )
#define RECENTPLAY_FILTER_LONGTEXT N_( "Regular expression used to filter " \
        "the recent items played in the player" )

#define SLIDERCOL_TEXT N_( "Define the colors of the volume slider " )
#define SLIDERCOL_LONGTEXT N_( "Define the colors of the volume slider\n" \
                       "By specifying the 12 numbers separated by a ';'\n" \
            "Default is '255;255;255;20;226;20;255;176;15;235;30;20'\n" \
            "An alternative can be '30;30;50;40;40;100;50;50;160;150;150;255' ")

#define QT_MODE_TEXT N_( "Selection of the starting mode and look " )
#define QT_MODE_LONGTEXT N_( "Start VLC with:\n" \
                             " - normal mode\n"  \
                             " - a zone always present to show information " \
                                  "as lyrics, album arts...\n" \
                             " - minimal mode with limited controls" )

#define QT_FULLSCREEN_TEXT N_( "Show a controller in fullscreen mode" )
#define QT_NATIVEOPEN_TEXT N_( "Embed the file browser in open dialog" )

#define FULLSCREEN_NUMBER_TEXT N_( "Define which screen fullscreen goes" )
#define FULLSCREEN_NUMBER_LONGTEXT N_( "Screennumber of fullscreen, instead of " \
                                       "same screen where interface is" )

#define QT_AUTOLOAD_EXTENSIONS_TEXT N_( "Load extensions on startup" )
#define QT_AUTOLOAD_EXTENSIONS_LONGTEXT N_( "Automatically load the "\
                                            "extensions module on startup" )

#define QT_MINIMAL_MODE_TEXT N_("Start in minimal view (without menus)" )

#define QT_BGCONE_TEXT N_( "Display background cone or art" )
#define QT_BGCONE_LONGTEXT N_( "Display background cone or current album art " \
                            "when not playing. " \
                            "Can be disabled to prevent burning screen." )
#define QT_BGCONE_EXPANDS_TEXT N_( "Expanding background cone or art." )
#define QT_BGCONE_EXPANDS_LONGTEXT N_( "Background art fits window's size" )

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

static const int i_notification_list[] =
    { NOTIFICATION_NEVER, NOTIFICATION_MINIMIZED, NOTIFICATION_ALWAYS };

static const char *const psz_notification_list_text[] =
    { N_("Never"), N_("When minimized"), N_("Always") };

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

#ifdef UPDATE_CHECK
    add_bool( "qt-updates-notif", true, UPDATER_TEXT,
              UPDATER_LONGTEXT, false )
    add_integer_with_range( "qt-updates-days", 3, 0, 180,
              UPDATER_DAYS_TEXT, UPDATER_DAYS_TEXT, false )
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

    add_obsolete_bool( "qt-blingbling" )      /* Suppressed since 1.0.0 */
    add_obsolete_integer( "qt-display-mode" ) /* Suppressed since 1.1.0 */

    add_obsolete_bool( "qt-adv-options" )     /* Since 2.0.0 */
    add_obsolete_bool( "qt-volume-complete" ) /* Since 2.0.0 */
    add_obsolete_integer( "qt-startvolume" )  /* Since 2.0.0 */

    cannot_unload_broken_library()

    add_submodule ()
        set_description( "Dialogs provider" )
        set_capability( "dialogs provider", 51 )

        set_callbacks( OpenDialogs, Close )

#if defined (Q_WS_X11) || (defined (Q_WS_QPA) && defined (__unix__))
    add_submodule ()
        set_capability( "vout window xid", 0 )
        set_callbacks( WindowOpen, WindowClose )
#endif
#if defined (Q_WS_WIN) || (defined (Q_WS_QPA) && defined (_WIN32)) \
 || defined (Q_WS_PM)  || (defined (Q_WS_QPA) && defined (__OS2__))
    add_submodule ()
        set_capability( "vout window hwnd", 0 )
        set_callbacks( WindowOpen, WindowClose )
#endif
#if defined (Q_WS_MAC) || (defined (Q_WS_QPA) && defined (__APPLE__))
    add_submodule ()
        set_capability( "vout window nsobject", 0 )
        set_callbacks( WindowOpen, WindowClose )
#endif

vlc_module_end ()

/*****************************************/

/* Ugly, but the Qt interface assumes single instance anyway */
static vlc_sem_t ready;
static QMutex lock;
static bool busy = false;
static bool active = false;

/*****************************************************************************
 * Module callbacks
 *****************************************************************************/

#ifdef Q_WS_MAC
/* Used to abort the app.exec() on OSX after libvlc_Quit is called */
#include "../../../lib/libvlc_internal.h" /* libvlc_SetExitHandler */
static void Abort( void *obj )
{
    QVLCApp::triggerQuit();
}
#endif

static void RegisterIntf( vlc_object_t *p_this )
{
    playlist_t *pl = pl_Get(p_this);
    var_Create (pl, "qt4-iface", VLC_VAR_ADDRESS);
    var_SetAddress (pl, "qt4-iface", p_this);
    var_Create (pl, "window", VLC_VAR_STRING);
    var_SetString (pl, "window", "qt4,any");
}

/* Open Interface */
static int Open( vlc_object_t *p_this, bool isDialogProvider )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;

#ifdef Q_WS_X11
    if( !vlc_xlib_init( p_this ) )
        return VLC_EGENERIC;

    Display *p_display = XOpenDisplay( NULL );
    if( !p_display )
    {
        msg_Err( p_intf, "Could not connect to X server" );
        return VLC_EGENERIC;
    }
    XCloseDisplay( p_display );
#endif

    QMutexLocker locker (&lock);
    if (busy)
    {
        msg_Err (p_this, "cannot start Qt multiple times");
        return VLC_EGENERIC;
    }

    /* Allocations of p_sys */
    intf_sys_t *p_sys = p_intf->p_sys = new intf_sys_t;
    p_intf->p_sys->b_isDialogProvider = isDialogProvider;
    p_sys->p_mi = NULL;
    p_sys->pl_model = NULL;

    /* */
    vlc_sem_init (&ready, 0);
#ifdef Q_WS_MAC
    /* Run mainloop on the main thread as Cocoa requires */
    libvlc_SetExitHandler( p_intf->p_libvlc, Abort, p_intf );
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
    vlc_sem_wait (&ready);
    vlc_sem_destroy (&ready);
    busy = active = true;

#ifndef Q_WS_MAC
    if( !isDialogProvider )
    {
        RegisterIntf( p_this );
    }
#endif

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

    if( !p_sys->b_isDialogProvider )
    {
        playlist_t *pl = pl_Get(p_this);

        var_Destroy (pl, "window");
        var_Destroy (pl, "qt4-iface");
        playlist_Deactivate (pl); /* release window provider if needed */
    }

    /* And quit */
    msg_Dbg( p_this, "requesting exit..." );
    QVLCApp::triggerQuit();

    msg_Dbg( p_this, "waiting for UI thread..." );
#ifndef Q_WS_MAC
    vlc_join (p_sys->thread, NULL);
#endif
    delete p_sys;

    QMutexLocker locker (&lock);
    assert (busy);
    busy = false;
}

static void *Thread( void *obj )
{
    intf_thread_t *p_intf = (intf_thread_t *)obj;
    MainInterface *p_mi;
    char dummy[] = "vlc"; /* for WM_CLASS */
    char *argv[] = { dummy, NULL, };
    int argc = sizeof(argv) / sizeof(argv[0]) - 1;

    Q_INIT_RESOURCE( vlc );

    /* Start the QApplication here */
    QVLCApp app( argc, argv );

    p_intf->p_sys->p_app = &app;


    /* All the settings are in the .conf/.ini style */
    p_intf->p_sys->mainSettings = new QSettings(
#ifdef _WIN32
            QSettings::IniFormat,
#else
            QSettings::NativeFormat,
#endif
            QSettings::UserScope, "vlc", "vlc-qt-interface" );

    /* Icon setting, Mac uses icon from .icns */
#ifndef Q_WS_MAC
    if( QDate::currentDate().dayOfYear() >= QT_XMAS_JOKE_DAY && var_InheritBool( p_intf, "qt-icon-change" ) )
        app.setWindowIcon( QIcon::fromTheme( "vlc-xmas", QIcon( ":/logo/vlc128-xmas.png" ) ) );
    else
        app.setWindowIcon( QIcon::fromTheme( "vlc", QIcon( ":/logo/vlc256.png" ) ) );
#endif

    /* Initialize timers and the Dialog Provider */
    DialogsProvider::getInstance( p_intf );

    /* Detect screensize for small screens like TV or Netbooks */
    p_intf->p_sys->i_screenHeight =
        app.QApplication::desktop()->availableGeometry().height();

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
    if( !p_intf->p_sys->b_isDialogProvider )
    {
        p_mi = new MainInterface( p_intf );
        p_intf->p_sys->p_mi = p_mi;
    }
    else
        p_mi = NULL;

    /* Explain how to show a dialog :D */
    p_intf->pf_show_dialog = ShowDialog;

    /* Check window type from the Qt platform back-end */
    p_intf->p_sys->voutWindowType = VOUT_WINDOW_TYPE_INVALID;
#if defined (Q_WS_QPA)
    QString platform = app.platformName();
    if( platform == qfu("xcb") )
        p_intf->p_sys->voutWindowType = VOUT_WINDOW_TYPE_XID;
    else
        msg_Err( p_intf, "unknown Qt platform: %s", qtu(platform) );
#elif defined (Q_WS_X11)
    p_intf->p_sys->voutWindowType = VOUT_WINDOW_TYPE_XID;
#elif defined (Q_WS_WIN) || defined (Q_WS_PM)
    p_intf->p_sys->voutWindowType = VOUT_WINDOW_TYPE_HWND;
#elif defined (Q_WS_MAC)
    p_intf->p_sys->voutWindowType = VOUT_WINDOW_TYPE_NSOBJECT;
#endif

    /* Tell the main LibVLC thread we are ready */
    vlc_sem_post (&ready);

#ifdef Q_WS_MAC
    /* We took over main thread, register and start here */
    if( !p_intf->p_sys->b_isDialogProvider )
    {
        RegisterIntf( (vlc_object_t *)p_intf );
        playlist_Play( THEPL );
    }
#endif

    /* Last settings */
    app.setQuitOnLastWindowClosed( false );

    /* Retrieve last known path used in file browsing */
    p_intf->p_sys->filepath =
         getSettings()->value( "filedialog-path", QVLCUserDir( VLC_HOME_DIR ) ).toString();

    /* Loads and tries to apply the preferred QStyle */
    QString s_style = getSettings()->value( "MainWindow/QtStyle", "" ).toString();
    if( s_style.compare("") != 0 )
        QApplication::setStyle( s_style );

    /* Launch */
    app.exec();

    msg_Dbg( p_intf, "QApp exec() finished" );
    if (p_mi != NULL)
    {
        QMutexLocker locker (&lock);
        active = false;

        p_intf->p_sys->p_mi = NULL;
        /* Destroy first the main interface because it is connected to some
           slots in the MainInputManager */
        delete p_mi;
    }

    /* */
    ExtensionsManager::killInstance();

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
        getSettings()->setValue( "filedialog-path", p_intf->p_sys->filepath );
    else
        getSettings()->remove( "filedialog-path" );

    /* */
    delete p_intf->p_sys->pl_model;

    /* Delete the configuration. Application has to be deleted after that. */
    delete p_intf->p_sys->mainSettings;

    /* Destroy the MainInputManager */
    MainInputManager::killInstance();

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
 *
 * TODO move it out of here ?
 */
static int WindowControl( vout_window_t *, int i_query, va_list );

static int WindowOpen( vout_window_t *p_wnd, const vout_window_cfg_t *cfg )
{
    if( cfg->is_standalone )
        return VLC_EGENERIC;

    intf_thread_t *p_intf =
        (intf_thread_t *)var_InheritAddress( p_wnd, "qt4-iface" );
    if( !p_intf )
    {   /* If another interface is used, this plugin cannot work */
        msg_Dbg( p_wnd, "Qt interface not found" );
        return VLC_EGENERIC;
    }
    if( p_intf->p_sys->voutWindowType != cfg->type )
        return VLC_EGENERIC;
    switch( cfg->type )
    {
        case VOUT_WINDOW_TYPE_XID:
            if( var_InheritBool( p_wnd, "video-wallpaper" ) )
                return VLC_EGENERIC;
            break;
    }

    QMutexLocker locker (&lock);
    if (unlikely(!active))
        return VLC_EGENERIC;

    MainInterface *p_mi = p_intf->p_sys->p_mi;
    msg_Dbg( p_wnd, "requesting video window..." );

    int i_x = cfg->x;
    int i_y = cfg->y;
    unsigned i_width = cfg->width;
    unsigned i_height = cfg->height;

    WId wid = p_mi->getVideo( &i_x, &i_y, &i_width, &i_height );
    if( !wid )
        return VLC_EGENERIC;

    switch( cfg->type )
    {
        case VOUT_WINDOW_TYPE_XID:
            p_wnd->handle.xid = (uintptr_t)wid;
            p_wnd->display.x11 = NULL;
            break;
        case VOUT_WINDOW_TYPE_HWND:
            p_wnd->handle.hwnd = (void *)wid;
            break;
        case VOUT_WINDOW_TYPE_NSOBJECT:
            p_wnd->handle.nsobject = (void *)wid;
            break;
    }

    p_wnd->control = WindowControl;
    p_wnd->sys = (vout_window_sys_t*)p_mi;
    return VLC_SUCCESS;
}

static int WindowControl( vout_window_t *p_wnd, int i_query, va_list args )
{
    MainInterface *p_mi = (MainInterface *)p_wnd->sys;
    QMutexLocker locker (&lock);

    if (unlikely(!active))
    {
        msg_Warn (p_wnd, "video already released before control");
        return VLC_EGENERIC;
    }
    return p_mi->controlVideo( i_query, args );
}

static void WindowClose( vout_window_t *p_wnd )
{
    MainInterface *p_mi = (MainInterface *)p_wnd->sys;
    QMutexLocker locker (&lock);

    /* Normally, the interface terminates after the video. In the contrary, the
     * Qt4 main loop is gone, so we cannot send any event to the user interface
     * widgets. Ideally, we would keep the Qt4 main loop running until after
     * the video window is released. But it is far simpler to just have the Qt4
     * thread destroy the window early, and to turn this function into a stub.
     *
     * That assumes the video output will behave sanely if it window is
     * destroyed asynchronously.
     * XCB and Xlib-XCB are fine with that. Plain Xlib wouldn't, */
    if (unlikely(!active))
    {
        msg_Warn (p_wnd, "video already released");
        return;
    }
    msg_Dbg (p_wnd, "releasing video...");
    p_mi->releaseVideo();
}
