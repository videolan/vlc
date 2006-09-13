/*****************************************************************************
 * qt4.cpp : QT4 interface
 ****************************************************************************
 * Copyright (C) 2006 the VideoLAN team
 * $Id$
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA. *****************************************************************************/

#include <QApplication>

#include "qt4.hpp"
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
vlc_module_begin();
    set_shortname( (char *)"Qt" );
    set_description( (char*)_("Qt interface") );
    set_category( CAT_INTERFACE) ;
    set_subcategory( SUBCAT_INTERFACE_MAIN );
    set_capability( "interface", 100 );
    set_callbacks( Open, Close );

    set_program( "qvlc" );
    add_shortcut("qt");

    add_submodule();
        set_description( "Dialogs provider" );
        set_capability( "dialogs provider", 51 );
        add_bool( "qt-always-video", VLC_FALSE, NULL, "", "", VLC_TRUE );
        set_callbacks( OpenDialogs, Close );
vlc_module_end();


/*****************************************************************************
 * Module callbacks
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    p_intf->pf_run = Run;
    p_intf->p_sys = (intf_sys_t *)malloc(sizeof( intf_sys_t ) );
    memset( p_intf->p_sys, 0, sizeof( intf_sys_t ) );

    p_intf->p_sys->p_playlist = (playlist_t *)vlc_object_find( p_intf,
                            VLC_OBJECT_PLAYLIST, FIND_ANYWHERE );
    if( !p_intf->p_sys->p_playlist )
    {
        free( p_intf->p_sys );
        return VLC_EGENERIC;
    }

    p_intf->p_sys->p_sub = msg_Subscribe( p_intf, MSG_QUEUE_NORMAL );

    return VLC_SUCCESS;
}

static int OpenDialogs( vlc_object_t *p_this )
{
    intf_thread_t *p_intf = (intf_thread_t *)p_this;
    Open( p_this );
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
    char *argv[] = { "" };
    int argc = 1;
    Q_INIT_RESOURCE( vlc );

    QApplication *app = new QApplication( argc, argv , true );
    app->setWindowIcon( QIcon( QPixmap(vlc_xpm) ) );
    p_intf->p_sys->p_app = app;

    // Initialize timers
    DialogsProvider::getInstance( p_intf );

    // Normal interface
    if( !p_intf->pf_show_dialog )
    {
        MainInterface *p_mi = new MainInterface( p_intf );
        p_intf->p_sys->p_mi = p_mi;
        p_mi->show();
    }

    if( p_intf->pf_show_dialog )
        vlc_thread_ready( p_intf );

    app->setQuitOnLastWindowClosed( false );
    app->exec();
    MainInputManager::killInstance();
    delete p_intf->p_sys->p_mi;
}

/*****************************************************************************
 * Callback to show a dialog
 *****************************************************************************/
static void ShowDialog( intf_thread_t *p_intf, int i_dialog_event, int i_arg,
                        intf_dialog_args_t *p_arg )
{
    DialogEvent *event = new DialogEvent( i_dialog_event, i_arg, p_arg );
    QApplication::postEvent( DialogsProvider::getInstance( p_intf ),
                             static_cast<QEvent*>(event) );
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
