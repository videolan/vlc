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

#include <cassert>

#include "qt.hpp"

#include "dialogs/open/open.hpp"

#include "playlist/playlist_item.hpp"

#include "util/singleton.hpp"
#include "util/shared_input_item.hpp"

#include "medialibrary/mlqmltypes.hpp"
#include "medialibrary/mlplaylistlistmodel.hpp"

#include <QObject>
#include <QStringList>

#include <vlc_es.h>

#define TITLE_EXTENSIONS_MEDIA qtr( "Media Files" )
#define TITLE_EXTENSIONS_VIDEO qtr( "Video Files" )
#define TITLE_EXTENSIONS_AUDIO qtr( "Audio Files" )
#define TITLE_EXTENSIONS_IMAGE qtr( "Image Files" )
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

class OpenDialog;
class FirstRunWizard;
class ExtendedDialog;
class MessagesDialog;
class GotoTimeDialog;
class VLMDialog;
class HelpDialog;
class AboutDialog;
class MediaInfoDialog;
class PlaylistsDialog;
class BookmarksDialog;
class PodcastConfigDialog;
class PluginDialog;
class EpgDialog;
class UpdateDialog;

class DialogsProvider : public QObject, public Singleton<DialogsProvider>
{
    Q_OBJECT
    friend class VLCMenuBar;
    friend class Singleton<DialogsProvider>;

public:
    static DialogsProvider *getInstance()
    {
        const auto instance = Singleton<DialogsProvider>::getInstance<false>();
        assert( instance );
        return instance;
    }
    static DialogsProvider *getInstance( qt_intf_t *p_intf )
    {
        return Singleton<DialogsProvider>::getInstance( p_intf );
    }
    QStringList showSimpleOpen( const QString& help = QString(),
                                int filters = EXT_FILTER_MEDIA |
                                EXT_FILTER_VIDEO | EXT_FILTER_AUDIO |
                                EXT_FILTER_PLAYLIST,
                                const QUrl& path = QUrl() );
    bool isDying() { return b_isDying; }
    static QString getDirectoryDialog( qt_intf_t *p_intf);

    static QString getSaveFileName(QWidget *parent = NULL,
                                    const QString &caption = QString(),
                                    const QUrl &dir = QUrl(),
                                    const QString &filter = QString(),
                                    QString *selectedFilter = NULL );

    Q_INVOKABLE static QVariant getTextDialog(QWidget *parent, const QString& title,
                                              const QString& label,
                                              const QString& placeholder,
                                              bool* ok = nullptr);

protected:
    void customEvent( QEvent *);

private:
    DialogsProvider( qt_intf_t * );
    virtual ~DialogsProvider();

    void loadMediaFile( es_format_category_e category, int filter, const QString& dialogTitle );

    qt_intf_t *p_intf;

    std::unique_ptr<QMenu> popupMenu;
    std::unique_ptr<QMenu> videoPopupMenu;
    std::unique_ptr<QMenu> audioPopupMenu;
    std::unique_ptr<QMenu> miscPopupMenu;

    std::unique_ptr<OpenDialog> m_openDialog;
    std::unique_ptr<FirstRunWizard> m_firstRunDialog;
    std::unique_ptr<ExtendedDialog> m_extendedDialog;
    std::unique_ptr<MessagesDialog> m_messagesDialog;
    std::unique_ptr<GotoTimeDialog> m_gotoTimeDialog;
    std::unique_ptr<VLMDialog> m_vlmDialog;
    std::unique_ptr<HelpDialog> m_helpDialog;
    std::unique_ptr<AboutDialog> m_aboutDialog;
    std::unique_ptr<MediaInfoDialog> m_mediaInfoDialog;
    std::unique_ptr<BookmarksDialog> m_bookmarkDialog;
    std::unique_ptr<PodcastConfigDialog> m_podcastDialog;
    std::unique_ptr<PluginDialog> m_pluginDialog;
    std::unique_ptr<EpgDialog> m_egpDialog;
#ifdef UPDATE_CHECK
    std::unique_ptr<UpdateDialog> m_updateDialog;
#endif
    std::unique_ptr<input_item_parser_id_t,
                    decltype(&input_item_parser_id_Release)> m_parser;

    QWidget* root;
    bool b_isDying;

    void openDialog( OpenDialog::OpenTab );

    template<typename T>
    inline void ensureDialog(std::unique_ptr<T>& dialog);
    template<typename T>
    void toggleDialogVisible(std::unique_ptr<T>& dialog);

public slots:
    void playlistsDialog( const QVariantList & listMedia, MLPlaylistListModel::PlaylistType type = MLPlaylistListModel::PLAYLIST_TYPE_ALL);
    void bookmarksDialog();
    void mediaInfoDialog( void );
    void mediaInfoDialog( const SharedInputItem& inputItem );
    void mediaInfoDialog( const PlaylistItem& pItem );
    void mediaInfoDialog( const MLItemId& itemId );
    void mediaCodecDialog();
    bool questionDialog(const QString& text, const QString& title = {}) const;
    void prefsDialog();
    void firstRunDialog();
    void extendedDialog();
    void synchroDialog();
    void messagesDialog( int page = 0 );
    void sendKey( int key );
#ifdef ENABLE_VLM
    void vlmDialog();
#endif
    void helpDialog();
#if defined(UPDATE_CHECK)
    void updateDialog();
#endif
    void aboutDialog();
    void gotoTimeDialog();
    void podcastConfigureDialog();
    void pluginDialog();
    void epgDialog();
    void setPopupMenu();
    void destroyPopupMenu();

    void openFileGenericDialog( intf_dialog_args_t * );

    void simpleOpenDialog( bool start = true );

    void openDialog();
    void openDiscDialog();
    void openFileDialog();
    void openUrlDialog();
    void openNetDialog();
    void openCaptureDialog();

    void PLOpenDir();
    void PLAppendDir();

    void streamingDialog( QWindow *parent, const QStringList& mrls, bool b_stream = true,
                          QStringList options = QStringList("") );
    void streamingDialog( const QList<QUrl>& urls, bool b_stream = true );
    void openAndStreamingDialogs();
    void openAndTranscodingDialogs();

    void savePlayingToPlaylist();

    void loadSubtitlesFile();
    void loadAudioFile();
    void loadVideoFile();

    void quit();

public:
    void PLAppendDialog( OpenDialog::OpenTab tab = OpenDialog::OPEN_FILE_TAB );

signals:
    void releaseMouseEvents();
    void showToolbarEditorDialog();
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
