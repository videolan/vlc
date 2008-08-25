/*****************************************************************************
 * dialogs_provider.hpp : Dialogs provider
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

#ifndef _DIALOGS_PROVIDER_H_
#define _DIALOGS_PROVIDER_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>
#include <vlc_common.h>
#include <vlc_interface.h>

#include "qt4.hpp"
#include "dialogs/interaction.hpp"
#include "dialogs/open.hpp"

#include <QObject>
#include <QTimer>
#include <QApplication>

#define ADD_FILTER_MEDIA( string )     \
    string += qtr( "Media Files" );    \
    string += " ( ";                   \
    string += EXTENSIONS_MEDIA;        \
    string += ");;";
#define ADD_FILTER_VIDEO( string )     \
    string += qtr( "Video Files" );    \
    string += " ( ";                   \
    string += EXTENSIONS_VIDEO;        \
    string += ");;";
#define ADD_FILTER_AUDIO( string )     \
    string += qtr( "Audio Files" );    \
    string += " ( ";                   \
    string += EXTENSIONS_AUDIO;        \
    string += ");;";
#define ADD_FILTER_PLAYLIST( string )  \
    string += qtr( "Playlist Files" ); \
    string += " ( ";                   \
    string += EXTENSIONS_PLAYLIST;     \
    string += ");;";
#define ADD_FILTER_SUBTITLE( string )  \
    string += qtr( "Subtitles Files" );\
    string += " ( ";                   \
    string += EXTENSIONS_SUBTITLE;     \
    string += ");;";
#define ADD_FILTER_ALL( string )       \
    string += qtr( "All Files" );      \
    string += " (*.*)";

enum {
    EXT_FILTER_MEDIA     =  0x01,
    EXT_FILTER_VIDEO     =  0x02,
    EXT_FILTER_AUDIO     =  0x04,
    EXT_FILTER_PLAYLIST  =  0x08,
    EXT_FILTER_SUBTITLE  =  0x10,
};

class QEvent;
class QSignalMapper;
class QVLCMenu;

class DialogsProvider : public QObject
{
    Q_OBJECT;
    friend class QVLCMenu;

public:
    static DialogsProvider *getInstance()
    {
        assert( instance );
        return instance;
    }
    static DialogsProvider *getInstance( intf_thread_t *p_intf )
    {
        if( !instance )
            instance = new DialogsProvider( p_intf );
        return instance;
    }
    static void killInstance()
    {
        if( instance ) delete instance;
        instance = NULL;
    }
    static bool isAlive()
    {
        return ( instance != NULL );
    }
    virtual ~DialogsProvider();
    QTimer *fixed_timer;

    QStringList showSimpleOpen( QString help = QString(),
                                int filters = EXT_FILTER_MEDIA |
                                EXT_FILTER_VIDEO | EXT_FILTER_AUDIO |
                                EXT_FILTER_PLAYLIST,
                                QString path = QString() );
    bool isDying() { return b_isDying; }
protected:
    QSignalMapper *menusMapper;
    QSignalMapper *menusUpdateMapper;
    QSignalMapper *SDMapper;
    void customEvent( QEvent *);

private:
    DialogsProvider( intf_thread_t *);
    intf_thread_t *p_intf;
    static DialogsProvider *instance;
    void addFromSimple( bool, bool );
    bool b_isDying;

public slots:
    void doInteraction( intf_dialog_args_t * );
    void menuAction( QObject *);
    void menuUpdateAction( QObject * );
    void SDMenuAction( QString );

    void playlistDialog();
    void bookmarksDialog();
    void mediaInfoDialog();
    void mediaCodecDialog();
    void prefsDialog();
    void extendedDialog();
    void messagesDialog();
#ifdef ENABLE_VLM
    void vlmDialog();
#endif
    void helpDialog();
#ifdef UPDATE_CHECK
    void updateDialog();
#endif
    void aboutDialog();
    void gotoTimeDialog();
    void podcastConfigureDialog();

    void simpleOpenDialog();
    void simplePLAppendDialog();
    void simpleMLAppendDialog();

    void openDialog();
    void openDialog( int );
    void openFileGenericDialog( intf_dialog_args_t * );
    void openDiscDialog();
    void openFileDialog();
    void openNetDialog();
    void openCaptureDialog();

    void PLAppendDialog();
    void MLAppendDialog();
    void PLOpenDir();
    void PLAppendDir();
    void MLAppendDir();

    void streamingDialog( QWidget *parent, QString mrl = "",
            bool b_stream = true );
    void openThenStreamingDialogs();
    void openThenTranscodingDialogs();

    void openAPlaylist();
    void saveAPlaylist();

    void loadSubtitlesFile();

    void quit();
};

#endif
