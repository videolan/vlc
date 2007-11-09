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

#include <assert.h>
#include <vlc/vlc.h>
#include <vlc_interface.h>

#include "dialogs/interaction.hpp"

#include <QObject>
#include <QTimer>
#include <QApplication>

#define ADD_FILTER_MEDIA( string )   \
    string += qtr("Media Files");      \
    string += " ( ";                 \
    string += EXTENSIONS_MEDIA;      \
    string += ");;";
#define ADD_FILTER_VIDEO( string )   \
    string += qtr("Video Files");      \
    string += " ( ";                 \
    string += EXTENSIONS_VIDEO;      \
    string += ");;";
#define ADD_FILTER_AUDIO( string )   \
    string += qtr("Audio Files");      \
    string += " ( ";                 \
    string += EXTENSIONS_AUDIO;      \
    string += ");;";
#define ADD_FILTER_PLAYLIST( string )\
    string += qtr("Playlist Files");   \
    string += " ( ";                 \
    string += EXTENSIONS_PLAYLIST;   \
    string += ");;";
#define ADD_FILTER_SUBTITLE( string )\
    string += qtr("Subtitles Files");   \
    string += " ( ";                 \
    string += EXTENSIONS_SUBTITLE;   \
    string += ");;";
#define ADD_FILTER_ALL( string )     \
    string += qtr("All Files");        \
    string += " (*.*)";

#define EXT_FILTER_MEDIA        0x01
#define EXT_FILTER_VIDEO        0x02
#define EXT_FILTER_AUDIO        0x04
#define EXT_FILTER_PLAYLIST     0x08
#define EXT_FILTER_SUBTITLE     0x10

enum {
    OPEN_FILE_TAB,
    OPEN_DISC_TAB,
    OPEN_NETWORK_TAB,
    OPEN_CAPTURE_TAB
};

enum {
    OPEN_AND_PLAY,
    OPEN_AND_STREAM,
    OPEN_AND_SAVE,
    OPEN_AND_ENQUEUE
};

class QEvent;
class QSignalMapper;
class QVLCMenu;

class DialogsProvider : public QObject
{
    Q_OBJECT;
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
        instance=NULL;
    }
    virtual ~DialogsProvider();
    QTimer *fixed_timer;

    QStringList showSimpleOpen( QString help = QString(),
                                int filters = EXT_FILTER_MEDIA |
                                EXT_FILTER_VIDEO | EXT_FILTER_AUDIO |
                                EXT_FILTER_PLAYLIST,
                                QString path = QString() );
protected:
    friend class QVLCMenu;
    QSignalMapper *menusMapper;
    QSignalMapper *menusUpdateMapper;
    QSignalMapper *SDMapper;
    void customEvent( QEvent *);
private:
    DialogsProvider( intf_thread_t *);
    intf_thread_t *p_intf;
    static DialogsProvider *instance;
    void addFromSimple( bool, bool );

public slots:
    void doInteraction( intf_dialog_args_t * );
    void menuAction( QObject *);
    void menuUpdateAction( QObject *);
    void SDMenuAction( QString );

    void playlistDialog();
    void bookmarksDialog();
    void mediaInfoDialog();
    void mediaCodecDialog();
    void prefsDialog();
    void extendedDialog();
    void messagesDialog();
    void vlmDialog();
    void helpDialog();
    void aboutDialog();
    void gotoTimeDialog();

    void simpleOpenDialog();
    void simplePLAppendDialog();
    void simpleMLAppendDialog();

    void openDialog();
    void openDialog( int );
    void openDiscDialog();
    void openFileDialog();
    void openNetDialog();
    void openCaptureDialog();

    void PLAppendDialog();
    void MLAppendDialog();
    void PLAppendDir();
    void MLAppendDir();

    void streamingDialog( QString mrl = "", bool b_stream = true );
    void openThenStreamingDialogs();
    void openThenTranscodingDialogs();

    void openPlaylist();
    void savePlaylist();

    void podcastConfigureDialog();

    void switchToSkins();
    void quit();
};

#endif
