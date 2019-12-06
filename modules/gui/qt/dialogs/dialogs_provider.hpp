/*****************************************************************************
 * dialogs_provider.hpp : Dialogs provider
 ****************************************************************************
 * Copyright (C) 2006-2008 the VideoLAN team
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

#include "qt.hpp"

#include "dialogs/open/open.hpp"
#include <QObject>
#include <QStringList>

#define TITLE_EXTENSIONS_MEDIA qtr( "Media Files" )
#define TITLE_EXTENSIONS_VIDEO qtr( "Video Files" )
#define TITLE_EXTENSIONS_AUDIO qtr( "Audio Files" )
#define TITLE_EXTENSIONS_PLAYLIST qtr( "Playlist Files" )
#define TITLE_EXTENSIONS_SUBTITLE qtr( "Subtitle Files" )
#define TITLE_EXTENSIONS_ALL qtr( "All Files" )
#define EXTENSIONS_ALL "*"
#define ADD_EXT_FILTER( string, type ) \
    string = string + QString("%1 ( %2 );;") \
            .arg( TITLE_##type ) \
            .arg( QString( type ) );

enum {
    EXT_FILTER_MEDIA     =  0x01,
    EXT_FILTER_VIDEO     =  0x02,
    EXT_FILTER_AUDIO     =  0x04,
    EXT_FILTER_PLAYLIST  =  0x08,
    EXT_FILTER_SUBTITLE  =  0x10,
};

class QEvent;
class QSignalMapper;
class VLCMenuBar;

class DialogsProvider : public QObject
{
    Q_OBJECT
    friend class VLCMenuBar;

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

    QStringList showSimpleOpen( const QString& help = QString(),
                                int filters = EXT_FILTER_MEDIA |
                                EXT_FILTER_VIDEO | EXT_FILTER_AUDIO |
                                EXT_FILTER_PLAYLIST,
                                const QUrl& path = QUrl() );
    bool isDying() { return b_isDying; }
    static QString getDirectoryDialog( intf_thread_t *p_intf);

    static QString getSaveFileName(QWidget *parent = NULL,
                                    const QString &caption = QString(),
                                    const QUrl &dir = QUrl(),
                                    const QString &filter = QString(),
                                    QString *selectedFilter = NULL );

protected:
    void customEvent( QEvent *);

private:
    DialogsProvider( intf_thread_t *);
    virtual ~DialogsProvider();
    static DialogsProvider *instance;

    intf_thread_t *p_intf;

    QMenu* popupMenu;
    QMenu* videoPopupMenu;
    QMenu* audioPopupMenu;
    QMenu* miscPopupMenu;

    QWidget* root;
    bool b_isDying;

    void openDialog( int );

public slots:
    void bookmarksDialog();
    void mediaInfoDialog();
    void mediaCodecDialog();
    void prefsDialog();
    void extendedDialog();
    void synchroDialog();
    void messagesDialog();
    void sendKey( int key );
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
    void setPopupMenu();
    void destroyPopupMenu();

    void openFileGenericDialog( intf_dialog_args_t * );

    void simpleOpenDialog();

    void openDialog();
    void openDiscDialog();
    void openFileDialog();
    void openUrlDialog();
    void openNetDialog();
    void openCaptureDialog();

    void PLAppendDialog( int tab = OPEN_FILE_TAB );

    void PLOpenDir();
    void PLAppendDir();

    void streamingDialog( QWidget *parent, const QStringList& mrls, bool b_stream = true,
                          QStringList options = QStringList("") );
    void openAndStreamingDialogs();
    void openAndTranscodingDialogs();

    void savePlayingToPlaylist();

    void loadSubtitlesFile();

    void quit();

signals:
    void  toolBarConfUpdated();
    void releaseMouseEvents();
};

class DialogEvent : public QEvent
{
public:
    static const QEvent::Type DialogEvent_Type;
    DialogEvent( int _i_dialog, int _i_arg, intf_dialog_args_t *_p_arg ) :
                 QEvent( DialogEvent_Type )
    {
        i_dialog = _i_dialog;
        i_arg = _i_arg;
        p_arg = _p_arg;
    }

    int i_arg, i_dialog;
    intf_dialog_args_t *p_arg;
};


#endif
