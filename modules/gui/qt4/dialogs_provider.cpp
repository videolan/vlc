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

#include <QEvent>
#include <QApplication>
#include <QSignalMapper>
#include <QFileDialog>

#include "qt4.hpp"
#include "dialogs_provider.hpp"
#include "main_interface.hpp"
#include "menus.hpp"
#include <vlc_intf_strings.h>

/* The dialogs */
#include "dialogs/playlist.hpp"
#include "dialogs/prefs_dialog.hpp"
#include "dialogs/mediainfo.hpp"
#include "dialogs/messages.hpp"
#include "dialogs/extended.hpp"
#include "dialogs/sout.hpp"
#include "dialogs/open.hpp"
#include "dialogs/help.hpp"

DialogsProvider* DialogsProvider::instance = NULL;

DialogsProvider::DialogsProvider( intf_thread_t *_p_intf ) :
                                      QObject( NULL ), p_intf( _p_intf )
{
    fixed_timer = new QTimer( this );
    fixed_timer->start( 150 /* milliseconds */ );

    menusMapper = new QSignalMapper();
    CONNECT( menusMapper, mapped(QObject *), this, menuAction( QObject *) );

    menusUpdateMapper = new QSignalMapper();
    CONNECT( menusUpdateMapper, mapped(QObject *),
             this, menuUpdateAction( QObject *) );

    SDMapper = new QSignalMapper();
    CONNECT( SDMapper, mapped (QString), this, SDMenuAction( QString ) );
}

DialogsProvider::~DialogsProvider()
{
    PlaylistDialog::killInstance();
    MediaInfoDialog::killInstance();
}

void DialogsProvider::quit()
{
    p_intf->b_die = VLC_TRUE;
    QApplication::quit();
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
               mediaInfoDialog(); break;
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

/****************************************************************************
 * Individual simple dialogs
 ****************************************************************************/
void DialogsProvider::playlistDialog()
{
    PlaylistDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::prefsDialog()
{
    PrefsDialog::getInstance( p_intf )->toggleVisible();
}
void DialogsProvider::extendedDialog()
{
    ExtendedDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::messagesDialog()
{
    MessagesDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::helpDialog()
{
    HelpDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::aboutDialog()
{
    AboutDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::mediaInfoDialog()
{
    MediaInfoDialog::getInstance( p_intf )->toggleVisible();
}

void DialogsProvider::bookmarksDialog()
{
}

/****************************************************************************
 * All the open/add stuff
 ****************************************************************************/

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
void DialogsProvider::openDialog( int i_tab )
{
    OpenDialog::getInstance( p_intf->p_sys->p_mi  , p_intf )->showTab( i_tab );
}

/**** Simple open ****/

QStringList DialogsProvider::showSimpleOpen()
{
    QString FileTypes;
    FileTypes = _("Media Files");
    FileTypes += " ( ";
    FileTypes += EXTENSIONS_MEDIA;
    FileTypes += ");;";
    FileTypes += _("Video Files");
    FileTypes += " ( ";
    FileTypes += EXTENSIONS_VIDEO;
    FileTypes += ");;";
    FileTypes += _("Sound Files");
    FileTypes += " ( ";
    FileTypes += EXTENSIONS_AUDIO;
    FileTypes += ");;";
    FileTypes += _("PlayList Files");
    FileTypes += " ( ";
    FileTypes += EXTENSIONS_PLAYLIST;
    FileTypes += ");;";
    FileTypes += _("All Files");
    FileTypes += " (*.*)";
    FileTypes.replace(QString(";*"), QString(" *"));
    return QFileDialog::getOpenFileNames( NULL, qfu(I_OP_SEL_FILES ),
                    p_intf->p_libvlc->psz_homedir, FileTypes );
}

void DialogsProvider::addFromSimple( bool pl, bool go)
{
    QStringList files = DialogsProvider::showSimpleOpen();
    int i = 0;
    foreach( QString file, files )
    {
        const char * psz_utf8 = qtu( file );
        playlist_Add( THEPL, psz_utf8, NULL,
                      go ? ( PLAYLIST_APPEND | ( i ? 0 : PLAYLIST_GO ) |
                                               ( i ? PLAYLIST_PREPARSE : 0 ) )
                         : ( PLAYLIST_APPEND | PLAYLIST_PREPARSE ),
                      PLAYLIST_END,
                      pl ? VLC_TRUE : VLC_FALSE );
        i++;
    }
}

void DialogsProvider::simplePLAppendDialog()
{
    addFromSimple( true, false );
}

void DialogsProvider::simpleMLAppendDialog()
{
    addFromSimple( false, false );
}

void DialogsProvider::simpleOpenDialog()
{
    addFromSimple( true, true );
}

void DialogsProvider::openPlaylist()
{
    QStringList files = showSimpleOpen();
    foreach( QString file, files )
    {
        playlist_Import( THEPL, qtu(file) );
    }
}

void DialogsProvider::savePlaylist()
{
    QFileDialog *qfd = new QFileDialog( NULL,
                                   qtr("Choose a filename to save playlist"),
                                   p_intf->p_libvlc->psz_homedir,
                                   qfu("XSPF playlist (*.xspf);; ") +
                                   qfu("M3U playlist (*.m3u);; Any (*.*) ") );
    qfd->setFileMode( QFileDialog::AnyFile );
    qfd->setAcceptMode( QFileDialog::AcceptSave );
    qfd->setConfirmOverwrite( true );

    if( qfd->exec() == QDialog::Accepted )
    {
        if( qfd->selectedFiles().count() > 0 )
        {
            char *psz_module, *psz_m3u = "export-m3u",
                 *psz_xspf = "export-xspf";

            QString file = qfd->selectedFiles().first();
            QString filter = qfd->selectedFilter();

            if( file.contains(".xsp") ||
                ( filter.contains(".xspf") && !file.contains(".m3u") ) )
            {
                psz_module = psz_xspf;
                if( !file.contains( ".xsp" ) )
                    file.append( ".xspf" );
            }
            else
            {
                psz_module = psz_m3u;
                if( !file.contains( ".m3u" ) )
                    file.append( ".m3u" );
            }

            playlist_Export( THEPL, qtu(file), THEPL->p_playlist_category,
                             psz_module);
        }
    }
    delete qfd;
}

static void openDirectory( intf_thread_t* p_intf, bool pl, bool go )
{
    QString dir = QFileDialog::getExistingDirectory ( 0,
                                                     _("Open directory") );
    input_item_t *p_input = input_ItemNewExt( THEPL, qtu(dir), NULL,
                                               0, NULL, -1 );
    playlist_AddInput( THEPL, p_input,
                       go ? ( PLAYLIST_APPEND | PLAYLIST_GO ) : PLAYLIST_APPEND,
                       PLAYLIST_END, pl);
    input_Read( THEPL, p_input, VLC_FALSE );
}

void DialogsProvider::PLAppendDir()
{
    openDirectory( p_intf, true, false );
}

void DialogsProvider::MLAppendDir()
{
    openDirectory( p_intf, false , false );
}


/****************************************************************************
 * Sout emulation
 ****************************************************************************/

void DialogsProvider::streamingDialog()
{
    OpenDialog *o = new OpenDialog( p_intf->p_sys->p_mi, p_intf, true );
    if ( o->exec() == QDialog::Accepted )
    {
        SoutDialog *s = new SoutDialog( p_intf->p_sys->p_mi, p_intf );
        if( s->exec() == QDialog::Accepted )
        {
            msg_Err(p_intf, "mrl %s\n", qta(s->mrl));
            /* Just do it */
            int i_len = strlen( qtu(s->mrl) ) + 10;
            char *psz_option = (char*)malloc(i_len);
            snprintf( psz_option, i_len - 1, ":sout=%s", qtu(s->mrl));

            playlist_AddExt( THEPL, qtu( o->mrl ), "Streaming",
                             PLAYLIST_APPEND | PLAYLIST_GO, PLAYLIST_END,
                             -1, &psz_option, 1, VLC_TRUE );
        }
        delete s;
    }
    delete o;
}

/****************************************************************************
 * Menus / Interaction
 ****************************************************************************/

void DialogsProvider::menuAction( QObject *data )
{
    QVLCMenu::DoAction( p_intf, data );
}

void DialogsProvider::menuUpdateAction( QObject *data )
{
    MenuFunc * f = qobject_cast<MenuFunc *>(data);
    f->doFunc( p_intf );
}

void DialogsProvider::SDMenuAction( QString data )
{
    char *psz_sd = data.toUtf8().data();
    if( !playlist_IsServicesDiscoveryLoaded( THEPL, psz_sd ) )
        playlist_ServicesDiscoveryAdd( THEPL, psz_sd );
    else
        playlist_ServicesDiscoveryRemove( THEPL, psz_sd );
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
            qdialog->update();
        break;
    case INTERACT_HIDE:
        qdialog = (InteractionDialog*)(p_dialog->p_private);
        if( qdialog )
            qdialog->hide();
        p_dialog->i_status = HIDDEN_DIALOG;
        break;
    case INTERACT_DESTROY:
        qdialog = (InteractionDialog*)(p_dialog->p_private);
        if( !p_dialog->i_flags & DIALOG_NONBLOCKING_ERROR )
            delete qdialog;
        p_dialog->i_status = DESTROYED_DIALOG;
        break;
    }
}

void DialogsProvider::switchToSkins()
{
    var_SetString( p_intf, "intf-switch", "skins2" );
}

void DialogsProvider::popupMenu( int i_dialog )
{
}
