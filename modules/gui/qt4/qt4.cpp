/*****************************************************************************
 * qt4.cpp : QT4 interface
 ****************************************************************************
 * Copyright (C) 2000-2005 the VideoLAN team
 * $Id: wxwidgets.cpp 15731 2006-05-25 14:43:53Z zorglub $
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
#include "main_interface.hpp"

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
    set_shortname( (char*)"QT" );
    set_description( (char*)_("QT interface") );
    set_category( CAT_INTERFACE) ;
    set_subcategory( SUBCAT_INTERFACE_MAIN );
    set_capability( "interface", 100 );
    set_callbacks( Open, Close );

    add_submodule();
        set_description( "Dialogs provider" );
        set_capability( "dialogs provider", 51 );
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
        if( vlc_thread_create( p_intf, "QT dialogs", Init, 0, VLC_TRUE ) )
        {
            msg_Err( p_intf, "failed to create QT dialogs thread" );
        }
    }
    else
    {
        Init( p_intf );
    }
}

static void Init( intf_thread_t *p_intf )
{
    char *argv[] = { "" };
    int argc = 1;
    QApplication *app = new QApplication( argc, argv , true );
    p_intf->p_sys->p_app = app;

    /* Normal interface */
    if( !p_intf->pf_show_dialog )
    {
        MainInterface *p_mi = new MainInterface( p_intf );
        p_intf->p_sys->p_mi = p_mi;
        p_mi->show();
    }

    if( p_intf->pf_show_dialog )
        vlc_thread_ready( p_intf );

    app->exec();
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
