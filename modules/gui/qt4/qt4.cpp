/*****************************************************************************
 * qt4.cpp : QT4 interface
 ****************************************************************************
 * Copyright © 2006-2008 the VideoLAN team
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
#include <QLocale>
#include <QTranslator>
#include <QDate>
#include <QMutex>
#include <QMutexLocker>
#include <QWaitCondition>

#include "qt4.hpp"
#include "input_manager.hpp"    /* THEMIM creation */
#include "dialogs_provider.hpp" /* THEDP creation */
#include "main_interface.hpp"   /* MainInterface creation */
#include "dialogs/help.hpp"     /* Launch Update */
#include "recents.hpp"          /* Recents Item destruction */
#include "util/qvlcapp.hpp"

#ifdef HAVE_X11_XLIB_H
#include <X11/Xlib.h>
#endif

#include "../../../share/vlc32x32.xpm"
#include "../../../share/vlc32x32-christmas.xpm"
#include <vlc_plugin.h>

#ifdef WIN32
 #include <QtPlugin>
 Q_IMPORT_PLUGIN(qjpeg)
 Q_IMPORT_PLUGIN(qtaccessiblewidgets)
#endif

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static int  OpenDialogs  ( vlc_object_t * );
static int  WindowOpen   ( vlc_object_t * );
static void WindowClose  ( vlc_object_t * );
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
                           "controler window title." )

#define FILEDIALOG_PATH_TEXT N_( "Path to use in openfile dialog" )

#define NOTIFICATION_TEXT N_( "Show notification popup on track change" )
#define NOTIFICATION_LONGTEXT N_( \
    "Show a notification popup with the artist and track name when " \
    "the current playlist item changes, when VLC is minimized or hidden." )

#define ADVANCED_OPTIONS_TEXT N_( "Advanced options" )
#define ADVANCED_OPTIONS_LONGTEXT N_( "Show all the advanced options " \
                                      "in the dialogs." )

#define OPACITY_TEXT N_( "Windows opacity between 0.1 and 1." )
#define OPACITY_LONGTEXT N_( "Sets the windows opacity between 0.1 and 1 " \
                             "for main interface, playlist and extended panel."\
                             " This option only works with Windows and " \
                             "X11 with composite extensions." )

#define ERROR_TEXT N_( "Show unimportant error and warnings dialogs" )

#define UPDATER_TEXT N_( "Activate the updates availability notification" )
#define UPDATER_LONGTEXT N_( "Activate the automatic notification of new " \
                            "versions of the software. It runs once every " \
                            "two weeks." )
#define UPDATER_DAYS_TEXT N_("Number of days between two update checks")

#define COMPLETEVOL_TEXT N_( "Allow the volume to be set to 400%" )
#define COMPLETEVOL_LONGTEXT N_( "Allow the volume to have range from 0% to " \
                                 "400%, instead of 0% to 200%. This option " \
                                 "can distort the audio, since it uses " \
                                 "software amplification." )

#define SAVEVOL_TEXT N_( "Automatically save the volume on exit" )

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

#define QT_NORMAL_MODE_TEXT N_( "Classic look" )
#define QT_ALWAYS_VIDEO_MODE_TEXT N_( "Complete look with information area" )
#define QT_MINIMAL_MODE_TEXT N_( "Minimal look with no menus" )

#define QT_FULLSCREEN_TEXT N_( "Show a controller in fullscreen mode" )

static const int i_mode_list[] =
    { QT_NORMAL_MODE, QT_ALWAYS_VIDEO_MODE, QT_MINIMAL_MODE };
static const char *const psz_mode_list_text[] =
    { QT_NORMAL_MODE_TEXT, QT_ALWAYS_VIDEO_MODE_TEXT, QT_MINIMAL_MODE_TEXT };

vlc_module_begin ()
    set_shortname( "Qt" )
    set_description( N_("Qt interface") )
    set_category( CAT_INTERFACE )
    set_subcategory( SUBCAT_INTERFACE_MAIN )
    set_capability( "interface", 151 )
    set_callbacks( Open, Close )

    add_shortcut("qt")
    add_integer( "qt-display-mode", QT_NORMAL_MODE, NULL,
                 QT_MODE_TEXT, QT_MODE_LONGTEXT, false )
        change_integer_list( i_mode_list, psz_mode_list_text, NULL )

    add_bool( "qt-notification", true, NULL, NOTIFICATION_TEXT,
              NOTIFICATION_LONGTEXT, false )

    add_float_with_range( "qt-opacity", 1., 0.1, 1., NULL, OPACITY_TEXT,
                          OPACITY_LONGTEXT, false )

    add_bool( "qt-system-tray", true, NULL, SYSTRAY_TEXT,
              SYSTRAY_LONGTEXT, false)
    add_bool( "qt-start-minimized", false, NULL, MINIMIZED_TEXT,
              MINIMIZED_LONGTEXT, true)
    add_bool( "qt-video-autoresize", true, NULL, KEEPSIZE_TEXT,
              KEEPSIZE_LONGTEXT, false )
    add_bool( "qt-name-in-title", true, NULL, TITLE_TEXT,
              TITLE_LONGTEXT, false )
    add_bool( "qt-fs-controller", true, NULL, QT_FULLSCREEN_TEXT,
              QT_FULLSCREEN_TEXT, false )

    add_bool( "qt-volume-complete", false, NULL, COMPLETEVOL_TEXT,
              COMPLETEVOL_LONGTEXT, true )
    add_bool( "qt-autosave-volume", false, NULL, SAVEVOL_TEXT,
              SAVEVOL_TEXT, true )
    add_string( "qt-filedialog-path", NULL, NULL, FILEDIALOG_PATH_TEXT,
                FILEDIALOG_PATH_TEXT, true )
        change_autosave ()
        change_internal ()

    add_bool( "qt-recentplay", true, NULL, RECENTPLAY_TEXT,
              RECENTPLAY_TEXT, false )
    add_string( "qt-recentplay-filter", "", NULL,
                RECENTPLAY_FILTER_TEXT, RECENTPLAY_FILTER_LONGTEXT, false )

    add_bool( "qt-adv-options", false, NULL, ADVANCED_OPTIONS_TEXT,
              ADVANCED_OPTIONS_LONGTEXT, true )
    add_bool( "qt-advanced-pref", false, NULL, ADVANCED_PREFS_TEXT,
              ADVANCED_PREFS_LONGTEXT, false )
    add_bool( "qt-error-dialogs", true, NULL, ERROR_TEXT,
              ERROR_TEXT, false )
#ifdef UPDATE_CHECK
    add_bool( "qt-updates-notif", true, NULL, UPDATER_TEXT,
              UPDATER_LONGTEXT, false )
    add_integer( "qt-updates-days", 7, NULL, UPDATER_DAYS_TEXT,
                 UPDATER_DAYS_TEXT, false )
#endif
    add_string( "qt-slider-colours",
                "255;255;255;20;226;20;255;176;15;235;30;20",
                NULL, SLIDERCOL_TEXT, SLIDERCOL_LONGTEXT, false )

    add_bool( "qt-privacy-ask", true, NULL, PRIVACY_TEXT, PRIVACY_TEXT,
              false )
        change_internal ()

    add_obsolete_bool( "qt-blingbling" ) /* Suppressed since 1.0.0 */

    add_submodule ()
        set_description( "Dialogs provider" )
        set_capability( "dialogs provider", 51 )

        set_callbacks( OpenDialogs, Close )

#if defined (Q_WS_X11)
# define WID_CAPABILITY "xwindow"
#elif defined (WIN32)
# define WID_CAPABILITY "hwnd"
#endif
#ifdef WID_CAPABILITY
    add_submodule ()
        set_capability( WID_CAPABILITY, 50 )
        set_callbacks( WindowOpen, WindowClose )
#endif

vlc_module_end ()

/* Ugly, but the Qt4 interface assumes single instance anyway */
static struct
{
    QMutex lock;
    QWaitCondition ready;
} iface;

/*****************************************************************************
 * Module callbacks
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys;

#ifdef Q_WS_X11
    char *psz_display = var_CreateGetNonEmptyString( p_intf, "x11-display" );
    Display *p_display = XOpenDisplay( psz_display );
    free( psz_display );
    if( !p_display )
    {
        msg_Err( p_intf, "Could not connect to X server" );
        return VLC_EGENERIC;
    }
#endif

    /* Allocations */
    p_sys = p_intf->p_sys = new intf_sys_t;
    p_sys->b_isDialogProvider = false;
    p_sys->p_popup_menu = NULL; /* ??? */
    p_sys->p_playlist = pl_Hold( p_intf );
    p_sys->p_mi = NULL;
#ifdef Q_WS_X11
    p_sys->display = p_display;
#endif

    if( vlc_clone( &p_sys->thread, Thread, p_intf, VLC_THREAD_PRIORITY_LOW ) )
    {
        pl_Release (p_sys->p_playlist);
        delete p_sys;
        return VLC_ENOMEM;
    }

    /* */
    QMutexLocker locker (&iface.lock);
    vlc_value_t val;

    while( p_sys->p_mi == NULL && !p_sys->b_isDialogProvider )
        iface.ready.wait( &iface.lock );
    var_Create (p_this->p_libvlc, "qt4-iface", VLC_VAR_ADDRESS);
    val.p_address = p_this;
    var_Set (p_this->p_libvlc, "qt4-iface", val);
    return VLC_SUCCESS;
}

static int OpenDialogs( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    p_intf->pf_show_dialog = ShowDialog;

    int val = Open( p_this );
    if( val )
        return val;

    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    intf_sys_t *p_sys = p_intf->p_sys;

    var_Destroy (p_this->p_libvlc, "qt4-iface");
    QApplication::quit();

    vlc_join (p_sys->thread, NULL);
#ifdef Q_WS_X11
    XCloseDisplay ((Display *)p_sys->display);
#endif
    pl_Release (p_this);
    delete p_sys;
}



static QMutex windowLock;
static QWaitCondition windowWait;

static void *Thread( void *obj )
{
    intf_thread_t *p_intf = (intf_thread_t *)obj;
    MainInterface *p_mi;
    char dummy[] = "";
    char *argv[] = { dummy };
    int argc = 1;

    Q_INIT_RESOURCE( vlc );

#if !defined(WIN32) && !defined(__APPLE__)
    /* KLUDGE:
     * disables icon theme use because that makes Cleanlooks style bug
     * because it asks gconf for some settings that timeout because of threads
     * see commits 21610 21622 21654 for reference */

    /* If you don't have a gconftool-2 binary, you should comment this line */
    if( strcmp( qVersion(), "4.4.0" ) < 0 ) /* fixed in Qt 4.4.0 */
        QApplication::setDesktopSettingsAware( false );
#endif

    /* Start the QApplication here */
#ifdef Q_WS_X11
    QVLCApp app( (Display *)p_intf->p_sys->display, argc, argv );
#else
    QVLCApp app( argc, argv );
#endif
    p_intf->p_sys->p_app = &app;

    p_intf->p_sys->mainSettings = new QSettings(
#ifdef WIN32
            QSettings::IniFormat,
#else
            QSettings::NativeFormat,
#endif
            QSettings::UserScope, "vlc", "vlc-qt-interface" );

    /* Icon setting */
    if( QDate::currentDate().dayOfYear() >= 354 )
        app.setWindowIcon( QIcon( QPixmap(vlc_christmas_xpm) ) );
    else
        app.setWindowIcon( QIcon( QPixmap(vlc_xpm) ) );

    /* Initialize timers and the Dialog Provider */
    DialogsProvider::getInstance( p_intf );

    /* Detect screensize for small screens like TV or EEEpc*/
    p_intf->p_sys->i_screenHeight =
        app.QApplication::desktop()->availableGeometry().height();

#ifdef UPDATE_CHECK
    /* Checking for VLC updates */
    if( config_GetInt( p_intf, "qt-updates-notif" ) &&
        !config_GetInt( p_intf, "qt-privacy-ask" ) )
    {
        int interval = config_GetInt( p_intf, "qt-updates-days" );
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
    if( !p_intf->pf_show_dialog )
        p_mi = new MainInterface( p_intf );
    else
        p_mi = NULL;

    /* */
    iface.lock.lock();
    p_intf->p_sys->p_mi = p_mi;
    p_intf->p_sys->b_isDialogProvider = p_mi == NULL;
    iface.ready.wakeAll();
    iface.lock.unlock();

    /* Explain to the core how to show a dialog :D */
    p_intf->pf_show_dialog = ShowDialog;

#ifdef ENABLE_NLS
    // Translation - get locale
#   if defined (WIN32) || defined (__APPLE__)
    char* psz_tmp = config_GetPsz( p_intf, "language" );
    QString lang = qfu( psz_tmp );
    free( psz_tmp);
    if (lang == "auto")
        lang = QLocale::system().name();
#   else
    QString lang = QLocale::system().name();
#   endif
    // Translations for qt's own dialogs
    QTranslator qtTranslator( 0 );
    // Let's find the right path for the translation file
#if !defined( WIN32 )
    QString path =  QString( QT4LOCALEDIR );
#else
    QString path = QString( QString(config_GetDataDir()) + DIR_SEP +
                            "locale" + DIR_SEP + "qt4" + DIR_SEP );
#endif
    // files depending on locale
    bool b_loaded = qtTranslator.load( path + "qt_" + lang );
    if (!b_loaded)
        msg_Dbg( p_intf, "Error while initializing qt-specific localization" );
    app.installTranslator( &qtTranslator );
#endif  //ENABLE_NLS

    /* Last settings */
    app.setQuitOnLastWindowClosed( false );

    /* Retrieve last known path used in file browsing */
    char *psz_path = config_GetPsz( p_intf, "qt-filedialog-path" );
    p_intf->p_sys->psz_filepath = EMPTY_STR( psz_path ) ? config_GetHomeDir()
                                                        : psz_path;

    /* Launch */
    app.exec();

    /* And quit */
    msg_Dbg( p_intf, "Quitting the Qt4 Interface" );

    QApplication::closeAllWindows();

    if (p_mi != NULL)
    {
        QMutexLocker locker (&iface.lock);

        msg_Dbg (p_intf, "destroying the main Qt4 interface");
        p_intf->p_sys->p_mi = NULL;
        /* Destroy first the main interface because it is connected to some
           slots in the MainInputManager */
        delete p_mi;
    }

    /* Destroy all remaining windows,
       because some are connected to some slots
       in the MainInputManager
       Settings must be destroyed after that.
     */
    DialogsProvider::killInstance();

    /* Delete the recentsMRL object before the configuration */
    RecentsMRL::killInstance();

    /* Delete the configuration. Application has to be deleted after that. */
    delete p_intf->p_sys->mainSettings;

    /* Destroy the MainInputManager */
    MainInputManager::killInstance();


    /* Save the path */
    config_PutPsz( p_intf, "qt-filedialog-path", p_intf->p_sys->psz_filepath );
    free( psz_path );

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
#include <vlc_window.h>

static int WindowControl (vout_window_t *, int, va_list);

static int WindowOpen (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    intf_thread_t *intf = NULL;
    vlc_value_t val;

    if (config_GetInt (obj, "embedded-video") <= 0)
        return VLC_EGENERIC;

    QMutexLocker (&iface.lock);
    if (var_Get (obj->p_libvlc, "qt4-iface", &val) == 0)
        intf = (intf_thread_t *)val.p_address;
    if (intf == NULL)
    {   /* If another interface is used, this plugin cannot work */
        msg_Dbg (obj, "Qt4 interface not found");
        return VLC_EGENERIC;
    }

    MainInterface *p_mi = intf->p_sys->p_mi;
    msg_Dbg (obj, "requesting video...");

#if defined (Q_WS_X11)
    wnd->handle.xid = p_mi->requestVideo (wnd->vout, &wnd->pos_x, &wnd->pos_y,
                                          &wnd->width, &wnd->height);
    if (!wnd->handle.xid)
        return VLC_EGENERIC;

#elif defined (WIN32)
    wnd->handle.hwnd = p_mi->requestVideo (wnd->vout, &wnd->pos_x, &wnd->pos_y,
                                           &wnd->width, &wnd->height);
    if (!wnd->handle.hwnd)
        return VLC_EGENERIC;

#else
    return VLC_EGENERIC;

#endif

    wnd->control = WindowControl;
    wnd->p_private = p_mi;
    return VLC_SUCCESS;
}

static int WindowControl (vout_window_t *wnd, int query, va_list args)
{
    MainInterface *p_mi = (MainInterface *)wnd->p_private;
    QMutexLocker locker (&iface.lock);

    return p_mi->controlVideo (query, args);
}

static void WindowClose (vlc_object_t *obj)
{
    vout_window_t *wnd = (vout_window_t *)obj;
    MainInterface *p_mi = (MainInterface *)wnd->p_private;
    QMutexLocker locker (&iface.lock);

    msg_Dbg (obj, "releasing video...");
    p_mi->releaseVideo ();
}
