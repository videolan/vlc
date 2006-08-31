/*****************************************************************************
 * main_inteface.cpp : Main interface
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

#include "qt4.hpp"
#include <QEvent>
#include "dialogs_provider.hpp"
#include "dialogs/playlist.hpp"
#include "dialogs/prefs_dialog.hpp"
#include "dialogs/streaminfo.hpp"
#include "dialogs/messages.hpp"
#include <QApplication>
#include <QSignalMapper>
#include "menus.hpp"
#include <vlc_intf_strings.h>

DialogsProvider* DialogsProvider::instance = NULL;

DialogsProvider::DialogsProvider( intf_thread_t *_p_intf ) :
                                      QObject( NULL ), p_intf( _p_intf )
{
//    idle_timer = new QTimer( this );
//    idle_timer->start( 0 );

    fixed_timer = new QTimer( this );
    fixed_timer->start( 150 /* milliseconds */ );

    menusMapper = new QSignalMapper();
    connect( menusMapper, SIGNAL( mapped(QObject *) ), this,
            SLOT(menuAction( QObject *)) );

    menusUpdateMapper = new QSignalMapper();
    connect( menusUpdateMapper, SIGNAL( mapped(QObject *) ), this,
            SLOT(menuUpdateAction( QObject *)) );
}

DialogsProvider::~DialogsProvider()
{
}
void DialogsProvider::customEvent( QEvent *event )
{
    if( event->type() == DialogEvent_Type )
    {
        DialogEvent *de = static_cast<DialogEvent*>(event);
        switch( de->i_dialog )
        {
            case INTF_DIALOG_FILE:
            case INTF_DIALOG_DISC:
            case INTF_DIALOG_NET:
            case INTF_DIALOG_CAPTURE:
                openDialog( de->i_dialog ); break;
            case INTF_DIALOG_PLAYLIST:
                playlistDialog(); break;
            case INTF_DIALOG_MESSAGES:
                messagesDialog(); break;
            case INTF_DIALOG_PREFS:
               prefsDialog(); break;
            case INTF_DIALOG_POPUPMENU:
            case INTF_DIALOG_AUDIOPOPUPMENU:
            case INTF_DIALOG_VIDEOPOPUPMENU:
            case INTF_DIALOG_MISCPOPUPMENU:
               popupMenu( de->i_dialog ); break;
            case INTF_DIALOG_FILEINFO:
               streaminfoDialog(); break;
            case INTF_DIALOG_INTERACTION:
               doInteraction( de->p_arg ); break;
            case INTF_DIALOG_VLM:
            case INTF_DIALOG_BOOKMARKS:
               bookmarksDialog(); break;
            case INTF_DIALOG_WIZARD:
            default:
               msg_Warn( p_intf, "unimplemented dialog\n" );
        }
    }
}

void DialogsProvider::playlistDialog()
{
    PlaylistDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::openDialog()
{
    openDialog( 0 );
}
void DialogsProvider::PLAppendDialog()
{
}
void DialogsProvider::MLAppendDialog()
{
}
void DialogsProvider::openDialog( int i_dialog )
{
}

void DialogsProvider::doInteraction( intf_dialog_args_t *p_arg )
{
    InteractionDialog *qdialog;
    interaction_dialog_t *p_dialog = p_arg->p_dialog;
    switch( p_dialog->i_action )
    {
    case INTERACT_NEW:
        qdialog = new InteractionDialog( p_intf, p_dialog );
        p_dialog->p_private = (void*)qdialog;
        if( !(p_dialog->i_status == ANSWERED_DIALOG) )
            qdialog->show();
        break;
    case INTERACT_UPDATE:
        qdialog = (InteractionDialog*)(p_dialog->p_private);
        if( qdialog)
            qdialog->Update();
        break;
    case INTERACT_HIDE:
        qdialog = (InteractionDialog*)(p_dialog->p_private);
        if( qdialog )
            qdialog->hide();
        p_dialog->i_status = HIDDEN_DIALOG;
        break;
    case INTERACT_DESTROY:
        qdialog = (InteractionDialog*)(p_dialog->p_private);
        delete qdialog;
        p_dialog->i_status = DESTROYED_DIALOG;
        break;
    }
}

void DialogsProvider::quit()
{
    p_intf->b_die = VLC_TRUE;
    QApplication::quit();
}

void DialogsProvider::streaminfoDialog()
{
    StreamInfoDialog::getInstance( p_intf, true )->toggleVisible();
}

void DialogsProvider::streamingDialog()
{
}

void DialogsProvider::prefsDialog()
{
    PrefsDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::messagesDialog()
{
    MessagesDialog::getInstance( p_intf, true )->toggleVisible();
}

void DialogsProvider::menuAction( QObject *data )
{
    QVLCMenu::DoAction( p_intf, data );
}

void DialogsProvider::menuUpdateAction( QObject *data )
{
    MenuFunc * f = qobject_cast<MenuFunc *>(data);
    f->doFunc( p_intf );
}

void DialogsProvider::simplePLAppendDialog()
{
    QStringList files = showSimpleOpen();
    QString file;
    foreach( file, files )
    {
        const char * psz_utf8 = file.toUtf8().data();
        playlist_PlaylistAdd( THEPL, psz_utf8, psz_utf8,
                     PLAYLIST_APPEND | PLAYLIST_PREPARSE, PLAYLIST_END );
    }
}

void DialogsProvider::simpleMLAppendDialog()
{
    QStringList files = showSimpleOpen();
    QString file;
    foreach( file, files )
    {
        const char * psz_utf8 = file.toUtf8().data();
        playlist_MLAdd( THEPL, psz_utf8, psz_utf8,
                        PLAYLIST_APPEND | PLAYLIST_PREPARSE, PLAYLIST_END );
    }
}

void DialogsProvider::simpleOpenDialog()
{
    QStringList files = showSimpleOpen();
    QString file;
    for( size_t i = 0 ; i< files.size(); i++ )
    {
        const char * psz_utf8 = files[i].toUtf8().data();
        /* Play the first one, parse and enqueue the other ones */
        playlist_PlaylistAdd( THEPL, psz_utf8, psz_utf8,
                     PLAYLIST_APPEND | (i ? 0 : PLAYLIST_GO) |
                     ( i ? PLAYLIST_PREPARSE : 0 ),
                     PLAYLIST_END );
    }
}

QStringList DialogsProvider::showSimpleOpen()
{
    QString FileTypes;
    FileTypes = "Video Files ( ";
    FileTypes += EXTENSIONS_VIDEO;
    FileTypes += ");; Sound Files ( ";
    FileTypes += EXTENSIONS_AUDIO;
    FileTypes += ");; PlayList Files ( ";
    FileTypes += EXTENSIONS_PLAYLIST;
    FileTypes += ");; All Files (*.*)" ;
    FileTypes.replace(QString(";*"), QString(" *"));
    return QFileDialog::getOpenFileNames( NULL, qfu(I_POP_SEL_FILES ),
                    p_intf->p_vlc->psz_homedir, FileTypes );
}

void DialogsProvider::bookmarksDialog()
{
}

void DialogsProvider::popupMenu( int i_dialog )
{

}
