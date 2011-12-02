/*****************************************************************************
 * dialogs_provider.hpp : Dialogs provider
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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

#ifndef QVLC_DIALOGS_PROVIDER_H_
#define QVLC_DIALOGS_PROVIDER_H_

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <assert.h>

#include "qt4.hpp"

#include "dialogs/open.hpp"
#include <QObject>
#include <QStringList>

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
    string += " (*)";

enum {
    EXT_FILTER_MEDIA     =  0x01,
    EXT_FILTER_VIDEO     =  0x02,
    EXT_FILTER_AUDIO     =  0x04,
    EXT_FILTER_PLAYLIST  =  0x08,
    EXT_FILTER_SUBTITLE  =  0x10,
};

enum {
    DialogEvent_Type = QEvent::User + DialogEventType + 1,
    //PLUndockEvent_Type = QEvent::User + DialogEventType + 2;
    //PLDockEvent_Type = QEvent::User + DialogEventType + 3;
    SetVideoOnTopEvent_Type = QEvent::User + DialogEventType + 4,
};

class QEvent;
class QSignalMapper;
class QVLCMenuManager;

class DialogsProvider : public QObject
{
    Q_OBJECT
    friend class QVLCMenuManager;

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
        delete instance;
        instance = NULL;
    }
    static bool isAlive()
    {
        return ( instance != NULL );
    }

    QStringList showSimpleOpen( const QString& help = QString(),
                                int filters = EXT_FILTER_MEDIA |
                                EXT_FILTER_VIDEO | EXT_FILTER_AUDIO |
                                EXT_FILTER_PLAYLIST,
                                const QString& path = QString() );
    bool isDying() { return b_isDying; }
protected:
    QSignalMapper *menusMapper;
    QSignalMapper *menusUpdateMapper;
    QSignalMapper *SDMapper;
    void customEvent( QEvent *);

private:
    DialogsProvider( intf_thread_t *);
    virtual ~DialogsProvider();
    static DialogsProvider *instance;

    intf_thread_t *p_intf;
    QWidget* root;
    bool b_isDying;

    void openDialog( int );
    void addFromSimple( bool, bool );

public slots:
    void playMRL( const QString & );

    void playlistDialog();
    void bookmarksDialog();
    void mediaInfoDialog();
    void mediaCodecDialog();
    void prefsDialog();
    void extendedDialog();
    void synchroDialog();
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
    void toolbarDialog();
    void pluginDialog();
    void epgDialog();

    void openFileGenericDialog( intf_dialog_args_t * );

    void simpleOpenDialog();
    void simplePLAppendDialog();
    void simpleMLAppendDialog();

    void openDialog();
    void openDiscDialog();
    void openFileDialog();
    void openUrlDialog();
    void openNetDialog();
    void openCaptureDialog();

    void PLAppendDialog( int tab = OPEN_FILE_TAB );
    void MLAppendDialog( int tab = OPEN_FILE_TAB );

    void PLOpenDir();
    void PLAppendDir();
    void MLAppendDir();

    void streamingDialog( QWidget *parent, const QString& mrl, bool b_stream = true,
                          QStringList options = QStringList("") );
    void openAndStreamingDialogs();
    void openAndTranscodingDialogs();

    void openAPlaylist();
    void saveAPlaylist();

    void loadSubtitlesFile();

    void quit();
private slots:
    void menuAction( QObject *);
    void menuUpdateAction( QObject * );
    void SDMenuAction( const QString& );
signals:
    void  toolBarConfUpdated();
};

class DialogEvent : public QEvent
{
public:
    DialogEvent( int _i_dialog, int _i_arg, intf_dialog_args_t *_p_arg ) :
                 QEvent( (QEvent::Type)(DialogEvent_Type) )
    {
        i_dialog = _i_dialog;
        i_arg = _i_arg;
        p_arg = _p_arg;
    }
    virtual ~DialogEvent() { }

    int i_arg, i_dialog;
    intf_dialog_args_t *p_arg;
};


#endif
