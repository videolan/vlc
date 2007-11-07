/*****************************************************************************
 * qt4.cpp : QT4 interface
 ****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
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

#include <QApplication>
#include <QLocale>
#include <QTranslator>

#include "qt4.hpp"
#include <vlc_os_specific.h>
#include "dialogs_provider.hpp"
#include "input_manager.hpp"
#include "main_interface.hpp"

#include "../../../share/vlc32x32.xpm"

/*****************************************************************************
 * Local prototypes.
 *****************************************************************************/
static int  Open         ( vlc_object_t * );
static void Close        ( vlc_object_t * );
static int  OpenDialogs  ( vlc_object_t * );
static void Run          ( intf_thread_t * );
static void Init         ( intf_thread_t * );
static void ShowDialog   ( intf_thread_t *, int, int, intf_dialog_args_t * );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
#define ALWAYS_VIDEO_TEXT N_("Always show video area")
#define ALWAYS_VIDEO_LONGTEXT N_("Start VLC with a cone image, and display it" \
                                   " when there is no video track." )

#define ADVANCED_PREFS_TEXT N_("Show advanced prefs over simple ones")
#define ADVANCED_PREFS_LONGTEXT N_("Show advanced preferences and not simple " \
                                   "preferences when opening the preferences " \
                                   "dialog.")

#define SYSTRAY_TEXT N_("Systray icon")
#define SYSTRAY_LONGTEXT N_("Show an icon in the systray " \
                            "allowing you to control VLC media player " \
                            "for basic actions")

#define MINIMIZED_TEXT N_("Start VLC with only a systray icon")
#define MINIMIZED_LONGTEXT N_("When you launch VLC with that option, " \
                            "VLC will start with just an icon in" \
                            "your taskbar")

#define TITLE_TEXT N_("Show playing item name in window title")
#define TITLE_LONGTEXT N_("Show the name of the song or video in the " \
                          "controler window title")

#define FILEDIALOG_PATH_TEXT N_("Path to use in openfile dialog")

#define NOTIFICATION_TEXT N_("Show notification popup on track change")
#define NOTIFICATION_LONGTEXT N_( \
    "Show a notification popup with the artist and track name when " \
    "the current playlist item changes, when VLC is minimized or hidden." )

#define ADVANCED_OPTIONS_TEXT N_("Advanced options")
#define ADVANCED_OPTIONS_LONGTEXT N_("Show all the advanced options " \
                                    "in the dialogs")

#define OPACITY_TEXT N_("Windows opacity between 0.1 and 1.")
#define OPACITY_LONGTEXT N_("Sets the windows opacity between 0.1 and 1 " \
                            "for main interface, playlist and extended panel." \
                            " This option only works with Windows and " \
                            "X11 with composite extensions.")

#define SHOWFLAGS_TEXT N_("Define what columns to show in playlist window")
#define SHOWFLAGS_LONGTEXT N_("Enter the sum of the options that you want: \n" \
            "Title: 1; Duration: 2; Artist: 4; Genre: 8; " \
            "Copyright: 16; Collection/album: 32; Rating: 256." )

#define ERROR_TEXT N_("Show unimportant error and warnings dialogs" )
#define MINIMAL_TEXT N_("Start in minimal view (menus hidden)." )

#define UPDATER_TEXT N_("Activate the new updates notification")
#define UPDATER_LONGTEXT N_("Activate the automatic notification of new " \
                            "versions of the software. It runs once a week." )

vlc_module_begin();
    set_shortname( (char *)"Qt" );
    set_description( (char*)_("Qt interface") );
    set_category( CAT_INTERFACE ) ;
    set_subcategory( SUBCAT_INTERFACE_MAIN );
    set_capability( "interface", 151 );
    set_callbacks( Open, Close );

    set_program( "qvlc" );
    add_shortcut("qt");

    add_submodule();
        set_description( "Dialogs provider" );
        set_capability( "dialogs provider", 51 );

        add_bool( "qt-always-video", VLC_FALSE, NULL, ALWAYS_VIDEO_TEXT,
                ALWAYS_VIDEO_LONGTEXT, VLC_TRUE );
        add_bool( "qt-system-tray", VLC_TRUE, NULL, SYSTRAY_TEXT,
                SYSTRAY_LONGTEXT, VLC_FALSE);
        add_bool( "qt-start-minimized", VLC_FALSE, NULL, MINIMIZED_TEXT,
                MINIMIZED_LONGTEXT, VLC_TRUE);
        add_bool( "qt-minimal-view", VLC_FALSE, NULL, MINIMAL_TEXT,
                MINIMAL_TEXT, VLC_TRUE );

        add_bool( "qt-name-in-title", VLC_TRUE, NULL, TITLE_TEXT,
                  TITLE_LONGTEXT, VLC_FALSE );
        add_string( "qt-filedialog-path", NULL, NULL, FILEDIALOG_PATH_TEXT,
                FILEDIALOG_PATH_TEXT, VLC_TRUE);
            change_autosave();
            change_internal();

        add_bool( "qt-notification", VLC_TRUE, NULL, NOTIFICATION_TEXT,
                  NOTIFICATION_LONGTEXT, VLC_FALSE );
        add_float_with_range( "qt-opacity", 1., 0.1, 1., NULL, OPACITY_TEXT,
                  OPACITY_LONGTEXT, VLC_FALSE );

        add_bool( "qt-adv-options", VLC_FALSE, NULL, ADVANCED_OPTIONS_TEXT,
                  ADVANCED_OPTIONS_LONGTEXT, VLC_TRUE );
        add_bool( "qt-advanced-pref", VLC_FALSE, NULL, ADVANCED_PREFS_TEXT,
                ADVANCED_PREFS_LONGTEXT, VLC_FALSE );
        add_bool( "qt-error-dialogs", VLC_TRUE, NULL, ERROR_TEXT,
                ERROR_TEXT, VLC_FALSE );
        add_bool( "qt-updates-notif", VLC_TRUE, NULL, UPDATER_TEXT, 
                UPDATER_LONGTEXT, VLC_FALSE );

        add_integer( "qt-pl-showflags",
                VLC_META_ENGINE_ARTIST|VLC_META_ENGINE_TITLE|
                VLC_META_ENGINE_DURATION|VLC_META_ENGINE_COLLECTION,
                NULL, SHOWFLAGS_TEXT,
                SHOWFLAGS_LONGTEXT, VLC_TRUE );
        set_callbacks( OpenDialogs, Close );
vlc_module_end();

/*****************************************************************************
 * Module callbacks
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    p_intf->pf_run = Run;
#if defined HAVE_GETENV && defined Q_WS_X11
    if( !getenv( "DISPLAY" ) )
    {
        msg_Err(p_intf, "no X server");
        return VLC_EGENERIC;
    }
#endif
    p_intf->p_sys = (intf_sys_t *)malloc(sizeof( intf_sys_t ) );
    if( !p_intf->p_sys )
    {
        msg_Err(p_intf, "Out of memory");
        return VLC_ENOMEM;
    }
    memset( p_intf->p_sys, 0, sizeof( intf_sys_t ) );

    p_intf->p_sys->p_playlist = pl_Yield( p_intf );
    p_intf->p_sys->p_sub = msg_Subscribe( p_intf, MSG_QUEUE_NORMAL );

    p_intf->b_play = VLC_TRUE;

    return VLC_SUCCESS;
}

static int OpenDialogs( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    int val = Open( p_this );
    if( val )
        return val;

    p_intf->pf_show_dialog = ShowDialog;
    return VLC_SUCCESS;
}

static void Close( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    vlc_mutex_lock( &p_intf->object_lock );
    p_intf->b_dead = VLC_TRUE;
    vlc_mutex_unlock( &p_intf->object_lock );

    vlc_object_release( p_intf->p_sys->p_playlist );
    msg_Unsubscribe( p_intf, p_intf->p_sys->p_sub );
    free( p_intf->p_sys );
}


/*****************************************************************************
 * Initialize the interface or the dialogs provider
 *****************************************************************************/
static void Run( intf_thread_t *p_intf )
{
    if( p_intf->pf_show_dialog )
    {
        if( vlc_thread_create( p_intf, "Qt dialogs", Init, 0, VLC_TRUE ) )
            msg_Err( p_intf, "failed to create Qt dialogs thread" );
    }
    else
        Init( p_intf );
}

static void Init( intf_thread_t *p_intf )
{
    char dummy[] = "";
    char *argv[] = { dummy };
    int argc = 1;

    Q_INIT_RESOURCE( vlc );

#ifndef WIN32
    /* KLUDGE:
     * disables icon theme use because that makes Cleanlooks style bug
     * because it asks gconf for some settings that timeout because of threads
     * see commits 21610 21622 21654 for reference */
    QApplication::setDesktopSettingsAware(false);
#endif

    /* Start the QApplication here */
    QApplication *app = new QApplication( argc, argv , true );
    app->setWindowIcon( QIcon( QPixmap(vlc_xpm) ) );
    p_intf->p_sys->p_app = app;

    // Initialize timers and the Dialog Provider
    DialogsProvider::getInstance( p_intf );

    // Create the normal interface
    if( !p_intf->pf_show_dialog )
    {
        MainInterface *p_mi = new MainInterface( p_intf );
        p_intf->p_sys->p_mi = p_mi;
        p_mi->show();
    }
    else
    /*if( p_intf->pf_show_dialog )*/
        vlc_thread_ready( p_intf );
    // Translation - get locale
    QLocale ql = QLocale::system ();
    // Translations for qt's own dialogs
    QTranslator qtTranslator( 0 );
    // Let's find the right path for the translation file
#if !defined( WIN32 )
    QString path =  QString(QT4LOCALEDIR);
#else
    QString path = QString( QString(system_VLCPath()) + DIR_SEP +
                            "locale" + DIR_SEP );
#endif
    // files depending on locale
    bool b_loaded = qtTranslator.load( path + "qt_" + ql.name());
    if (!b_loaded)
        msg_Dbg(p_intf, "Error while initializing qt-specific localization");
    app->installTranslator(&qtTranslator);

    /* Start playing if needed */
    if( !p_intf->pf_show_dialog && p_intf->b_play )
    {
        playlist_Control( THEPL, PLAYLIST_AUTOPLAY, VLC_FALSE );
    }

    /* Explain to the core how to show a dialog :D */
    p_intf->pf_show_dialog = ShowDialog;

    /* Last settings */
    app->setQuitOnLastWindowClosed( false );

    /* Launch */
    app->exec();

    /* And quit */
    MainInputManager::killInstance();
    DialogsProvider::killInstance();
    delete p_intf->p_sys->p_mi;
    delete app;
}

/*****************************************************************************
 * Callback to show a dialog
 *****************************************************************************/
static void ShowDialog( intf_thread_t *p_intf, int i_dialog_event, int i_arg,
                        intf_dialog_args_t *p_arg )
{
    DialogEvent *event = new DialogEvent( i_dialog_event, i_arg, p_arg );
    QApplication::postEvent( THEDP, static_cast<QEvent*>(event) );
}

/*****************************************************************************
 * PopupMenuCB: callback to show the popupmenu.
 *  We don't show the menu directly here because we don't want the
 *  caller to block for a too long time.
 *****************************************************************************/
static int PopupMenuCB( vlc_object_t *p_this, const char *psz_variable,
                        vlc_value_t old_val, vlc_value_t new_val, void *param )
{
    intf_thread_t *p_intf = (intf_thread_t *)param;
    ShowDialog( p_intf, INTF_DIALOG_POPUPMENU, new_val.b_bool, 0 );
    return VLC_SUCCESS;
}
